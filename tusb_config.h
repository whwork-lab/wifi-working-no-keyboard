#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
 extern "C" {
#endif

#define CFG_TUSB_OS               OPT_OS_PICO

// Enable Dual Stack Operation
#define CFG_TUD_ENABLED           1
#define CFG_TUH_ENABLED           1
#define CFG_TUH_RPI_PIO_USB       1

#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN        __attribute__ ((aligned(4)))
#endif

// DEVICE CONFIGURATION
#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE    64
#endif

#define CFG_TUD_CDC               1
#define CFG_TUD_HID               1
#define CFG_TUD_CDC_RX_BUFSIZE    256
#define CFG_TUD_CDC_TX_BUFSIZE    256
#define CFG_TUD_CDC_EP_BUFSIZE    64
#define CFG_TUD_HID_EP_BUFSIZE    16

// HOST CONFIGURATION
#define CFG_TUH_ENUMERATION_BUFSIZE 256
#define CFG_TUH_HUB               0  // Set to 0 to bypass port tree overhead
#define CFG_TUH_DEVICE_MAX        1  // Direct 1-to-1 keyboard tracking 
#define CFG_TUH_HID               1
#define CFG_TUH_HID_EPIN_BUFSIZE  64
#define CFG_TUH_HID_EPOUT_BUFSIZE 64

// Hardware Port Direct Routing
#define CFG_TUSB_RHPORT0_MODE     OPT_MODE_DEVICE  // Native Port = PC
#define CFG_TUSB_RHPORT1_MODE     OPT_MODE_HOST    // PIO Port = Keyboard

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CONFIG_H_ */
