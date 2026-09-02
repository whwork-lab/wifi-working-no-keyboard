#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/cyw43_arch.h"
#include "pico/util/queue.h"
#include "pico/mutex.h"
#include "hardware/irq.h"
#include "lwip/tcp.h"
#include "lwip/udp.h"
#include "pio_usb.h"
#include "tusb.h"

// ===== CONFIGURATION =====
#define AP_SSID "PicoW_Keyboard_AP"
#define AP_PASSWORD "123456789"
#define BUFFER_SIZE 4096

// WiFi Configuration for Station mode (connect to existing network)
#define WIFI_SSID "YOUR_SSID"           // Change to your WiFi SSID
#define WIFI_PASSWORD "YOUR_PASSWORD"   // Change to your WiFi password
#define SERVER_IP "192.168.1.100"        // Change to your PC's IP
#define UDP_PORT 4444

// ===== GLOBAL STATE =====
queue_t report_queue;
char key_log_buffer[BUFFER_SIZE] = {0};
size_t log_index = 0;
auto_init_mutex(log_mutex);

typedef struct {
    hid_keyboard_report_t report;
} queue_item_t;

// WiFi networking
static struct udp_pcb *udp_socket = NULL;
static ip_addr_t server_addr;
static bool wifi_connected = false;
static int wifi_mode = 0; // 0 = AP mode, 1 = Station mode

// ===== HTML WEB SERVER =====
const char HTTP_HEADER[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n";
const char HTML_BODY_START[] = "<!DOCTYPE html><html><head><title>Pico W Keyboard WiFi Bridge</title>"
                               "<meta http-equiv='refresh' content='2'>"
                               "<style>body{font-family:monospace; background:#121212; color:#00ff00; padding:20px;}"
                               "h1{color:#ffffff;} pre{background:#1e1e1e; padding:15px; border-radius:5px; border:1px solid #333; overflow-x:auto; white-space:pre-wrap;}</style></head>"
                               "<body><h1>Live Keyboard Inputs via WiFi</h1><pre>";
const char HTML_BODY_END[] = "</pre></body></html>";

// ===== DHCP SERVER =====
#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68

static void dhcp_server_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
    (void)arg; (void)addr; (void)port;
    if (!p) return;

    uint8_t *data = (uint8_t *)p->payload;
    if (p->len >= 240 && data[0] == 1) {
        uint8_t msg_type = 1;
        for (int i = 240; i < p->len - 2; i++) {
            if (data[i] == 0x35 && data[i+1] == 1) {
                msg_type = data[i+2];
                break;
            }
        }

        struct pbuf *p_out = pbuf_alloc(PBUF_TRANSPORT, 300, PBUF_RAM);
        if (p_out) {
            uint8_t *r = (uint8_t *)p_out->payload;
            memset(r, 0, 300);

            r[0] = 2; r[1] = 1; r[2] = 6; r[3] = 0;
            memcpy(&r[4], &data[4], 4);
            r[16] = 192; r[17] = 168; r[18] = 4; r[19] = 20;
            memcpy(&r[28], &data[28], 6);
            r[236] = 99; r[237] = 130; r[238] = 83; r[239] = 99;

            uint8_t *opt_ptr = &r[240];
            *opt_ptr++ = 0x35; *opt_ptr++ = 1; *opt_ptr++ = (msg_type == 1) ? 2 : 5;
            *opt_ptr++ = 1;    *opt_ptr++ = 4; *opt_ptr++ = 255; *opt_ptr++ = 255; *opt_ptr++ = 255; *opt_ptr++ = 0;
            *opt_ptr++ = 3;    *opt_ptr++ = 4; *opt_ptr++ = 192; *opt_ptr++ = 168; *opt_ptr++ = 4;   *opt_ptr++ = 1;
            *opt_ptr++ = 54;   *opt_ptr++ = 4; *opt_ptr++ = 192; *opt_ptr++ = 168; *opt_ptr++ = 4;   *opt_ptr++ = 1;
            *opt_ptr++ = 51;   *opt_ptr++ = 4; *opt_ptr++ = 0;   *opt_ptr++ = 0;   *opt_ptr++ = 25;  *opt_ptr++ = 128;
            *opt_ptr++ = 0xFF;

            udp_sendto(pcb, p_out, IP_ADDR_BROADCAST, DHCP_CLIENT_PORT);
            pbuf_free(p_out);
        }
    }
    pbuf_free(p);
}

void init_dhcp_server() {
    struct udp_pcb *pcb = udp_new();
    if (!pcb) return;
    udp_bind(pcb, IP_ADDR_ANY, DHCP_SERVER_PORT);
    udp_recv(pcb, dhcp_server_recv, NULL);
}

// ===== HTTP WEB SERVER =====
static err_t http_server_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    (void)arg; (void)err;
    if (p != NULL) {
        tcp_recved(pcb, p->tot_len);
        pbuf_free(p);

        tcp_write(pcb, HTTP_HEADER, strlen(HTTP_HEADER), TCP_WRITE_FLAG_COPY);
        tcp_write(pcb, HTML_BODY_START, strlen(HTML_BODY_START), TCP_WRITE_FLAG_COPY);

        mutex_enter_blocking(&log_mutex);
        if (log_index > 0) {
            tcp_write(pcb, key_log_buffer, log_index, TCP_WRITE_FLAG_COPY);
        } else {
            tcp_write(pcb, "[Waiting for keyboard input...]", 31, TCP_WRITE_FLAG_COPY);
        }
        mutex_exit(&log_mutex);

        tcp_write(pcb, HTML_BODY_END, strlen(HTML_BODY_END), TCP_WRITE_FLAG_COPY);
        tcp_output(pcb);
        tcp_close(pcb);
    } else {
        tcp_close(pcb);
    }
    return ERR_OK;
}

static err_t http_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
    (void)arg; (void)err;
    tcp_recv(newpcb, http_server_recv);
    return ERR_OK;
}

void init_web_server() {
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) return;
    if (tcp_bind(pcb, IP_ADDR_ANY, 80) == ERR_OK) {
        pcb = tcp_listen(pcb);
        tcp_accept(pcb, http_server_accept);
    }
}

// ===== KEYCODE TO ASCII CONVERSION =====
char hid_keycode_to_ascii(uint8_t modifier, uint8_t keycode) {
    if (keycode >= 4 && keycode <= 29) {
        char base = (modifier & 0x22) ? 'A' : 'a';
        return base + (keycode - 4);
    }
    if (keycode >= 30 && keycode <= 38) return '1' + (keycode - 30);
    if (keycode == 39) return '0';
    if (keycode == 40) return '\n';
    if (keycode == 44) return ' ';
    return 0;
}

// ===== WIFI CONNECTION (STATION MODE) =====
void wifi_link_status_callback(void) {
    if (wifi_mode != 1) return; // Only for station mode

    int link_status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);

    if (link_status == CYW43_LINK_UP) {
        if (!wifi_connected) {
            wifi_connected = true;
            printf("WiFi connected (Station mode)!\n");
            printf("IP: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_list)));

            // Create UDP socket
            if (udp_socket == NULL) {
                udp_socket = udp_new();
                inet_aton(SERVER_IP, &server_addr);
                printf("UDP socket created, sending to %s:%d\n", SERVER_IP, UDP_PORT);
            }
        }
    } else if (link_status < CYW43_LINK_UP) {
        if (wifi_connected) {
            wifi_connected = false;
            printf("WiFi disconnected\n");
        }
    }
}

void send_keyboard_data_udp(hid_keyboard_report_t *report) {
    if (wifi_mode != 1 || !wifi_connected || udp_socket == NULL) {
        return;
    }

    // Send keyboard report via UDP
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, sizeof(hid_keyboard_report_t), PBUF_RAM);
    if (p != NULL) {
        memcpy(p->payload, report, sizeof(hid_keyboard_report_t));
        udp_sendto(udp_socket, p, &server_addr, UDP_PORT);
        pbuf_free(p);
    }
}

// ===== CORE 1: USB HOST HANDLING =====
void core1_main() {
    pio_usb_configuration_t pio_usb_config = PIO_USB_DEFAULT_CONFIG;
    pio_usb_config.pin_dp = 0;      // GPIO 0 (D+)
    pio_usb_config.pin_dm = 1;      // GPIO 1 (D-)
    pio_usb_config.pio_rx_num = 0;
    pio_usb_config.pio_tx_num = 0;
    pio_usb_config.sm_tx = 0;
    pio_usb_config.sm_rx = 1;
    pio_usb_config.sm_eop = 2;

    pio_usb_host_init(&pio_usb_config);
    tuh_configure(1, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_usb_config);
    tuh_init(1);

    printf("Core 1: USB Host initialized (GPIO0=D+, GPIO1=D-)\n");

    while (1) {
        tuh_task();
    }
}

// ===== CORE 0: MAIN APPLICATION =====
int main() {
    set_sys_clock_khz(120000, true);
    stdio_init_all();
    sleep_ms(2000);

    printf("\n=== Pico W Integrated Keyboard WiFi Bridge ===\n");
    printf("GPIO Pins: D+ = GPIO0, D- = GPIO1, Power = VBUS, GND = Pin 38\n\n");

    // Initialize queue for HID reports
    queue_init(&report_queue, sizeof(queue_item_t), 8);

    // Initialize WiFi
    if (cyw43_arch_init()) {
        printf("WiFi init failed\n");
        return -1;
    }

    printf("Choose WiFi mode:\n");
    printf("0 = AP Mode (broadcast its own network)\n");
    printf("1 = Station Mode (connect to existing network)\n");
    printf("Using AP mode by default...\n\n");

    wifi_mode = 0; // Default to AP mode

    if (wifi_mode == 0) {
        // AP MODE - Create access point for easy viewing
        printf("Starting WiFi AP Mode...\n");
        cyw43_arch_enable_ap_mode(AP_SSID, AP_PASSWORD, CYW43_AUTH_WPA2_AES_PSK);

        cyw43_arch_lwip_begin();
        struct netif *nf = &cyw43_state.netif[CYW43_ITF_AP];
        ip4_addr_t ipaddr, netmask;
        IP4_ADDR(&ipaddr, 192, 168, 4, 1);
        IP4_ADDR(&netmask, 255, 255, 255, 0);
        netif_set_ipaddr(nf, &ipaddr);
        netif_set_netmask(nf, &netmask);

        init_dhcp_server();
        init_web_server();
        cyw43_arch_lwip_end();

        printf("AP SSID: %s\n", AP_SSID);
        printf("AP Password: %s\n", AP_PASSWORD);
        printf("Connect to this network and visit http://192.168.4.1 to see keyboard inputs\n\n");
    } else {
        // STATION MODE - Connect to existing network
        printf("Starting WiFi Station Mode...\n");
        cyw43_arch_enable_sta_mode();
        cyw43_arch_wifi_connect_async(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK);
        printf("Connecting to SSID: %s\n", WIFI_SSID);
        printf("Will send UDP data to: %s:%d\n\n", SERVER_IP, UDP_PORT);
    }

    // Launch Core 1 for USB Host handling
    multicore_launch_core1(core1_main);

    // Initialize USB Device (for future use)
    tud_init(0);

    printf("Main loop starting...\n");
    printf("Waiting for keyboard to be connected...\n\n");

    // ===== MAIN LOOP (CORE 0) =====
    while (1) {
        // Handle WiFi operations
        cyw43_arch_poll();
        if (wifi_mode == 1) {
            wifi_link_status_callback();
        }
        tud_task();

        // Process keyboard reports from queue
        queue_item_t item;
        if (queue_try_remove(&report_queue, &item)) {
            // Send via UDP if in station mode
            send_keyboard_data_udp(&item.report);

            // Log to web server buffer
            mutex_enter_blocking(&log_mutex);
            for (uint8_t i = 0; i < 6; i++) {
                uint8_t keycode = item.report.keycode[i];
                if (keycode > 0) {
                    char c = hid_keycode_to_ascii(item.report.modifier, keycode);
                    if (c > 0 && log_index < (BUFFER_SIZE - 2)) {
                        key_log_buffer[log_index++] = c;
                        key_log_buffer[log_index] = '\0';
                    }
                }
            }
            mutex_exit(&log_mutex);

            // Debug output
            printf("Key: ");
            for (uint8_t i = 0; i < 6; i++) {
                if (item.report.keycode[i] > 0) {
                    printf("%02x ", item.report.keycode[i]);
                }
            }
            printf("\n");
        }

        sleep_ms(10);
    }

    return 0;
}

// ===== TINYUSB HOST CALLBACKS =====
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
    (void)desc_report; (void)desc_len;
    printf("HID device mounted at addr %d, instance %d\n", dev_addr, instance);
    tuh_hid_receive_report(dev_addr, instance);
}

void tuh_hid_unmount_cb(uint8_t dev_addr, uint8_t instance) {
    printf("HID device unmounted at addr %d, instance %d\n", dev_addr, instance);
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
    (void)len;

    hid_keyboard_report_t const* kbd_report = (hid_keyboard_report_t const*) report;

    queue_item_t item;
    memcpy(&item.report, kbd_report, sizeof(hid_keyboard_report_t));

    queue_try_add(&report_queue, &item);

    tuh_hid_receive_report(dev_addr, instance);
}
