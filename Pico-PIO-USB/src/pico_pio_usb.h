#ifndef _PICO_PIO_USB_H_
#define _PICO_PIO_USB_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "pico/stdlib.h"
#include "hardware/pio.h"

// Core configuration structure required by main_host.c
typedef struct {
    uint8_t pin_dp;
    uint8_t pin_dm;
    PIO pio_tx;
    PIO pio_rx;
    uint tx_sm;
    uint rx_sm;
} bio_usb_config_t;

#define PIO_USB_HOST_DEFAULT_CONFIG { \
    .pin_dp = 0, \
    .pin_dm = 1, \
    .pio_tx = pio0, \
    .pio_rx = pio0, \
    .tx_sm = 0, \
    .rx_sm = 1 \
}

// Function blueprints used by your keylogger engine
void pio_usb_host_init(const bio_usb_config_t* config);

#ifdef __cplusplus
}
#endif

#endif
