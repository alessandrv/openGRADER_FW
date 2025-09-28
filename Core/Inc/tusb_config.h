#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#define CFG_TUSB_MCU                  OPT_MCU_STM32G4
#define CFG_TUSB_OS                   OPT_OS_NONE

#define BOARD_DEVICE_RHPORT_NUM       0
#define BOARD_TUD_RHPORT_SPEED        OPT_MODE_FULL_SPEED

#define CFG_TUSB_RHPORT0_MODE         (OPT_MODE_DEVICE | BOARD_TUD_RHPORT_SPEED)
#define CFG_TUSB_RHPORT1_MODE         OPT_MODE_NONE

#define CFG_TUD_ENDPOINT0_SIZE        64

#define CFG_TUSB_DEBUG                0

// Device class support -----------------------------------------------------
#define CFG_TUD_CDC                   1
#define CFG_TUD_HID                   2
#define CFG_TUD_MSC                   0
#define CFG_TUD_MIDI                  1
#define CFG_TUD_VENDOR                0

// CDC buffer sizes ---------------------------------------------------------
#define CFG_TUD_CDC_RX_BUFSIZE        256
#define CFG_TUD_CDC_TX_BUFSIZE        256
#define CFG_TUD_CDC_EP_BUFSIZE        64

// HID ----------------------------------------------------------------------
#define CFG_TUD_HID_EP_BUFSIZE        16
#define CFG_TUD_HID_POLL_INTERVAL     10

// MIDI --------------------------------------------------------------------
#define CFG_TUD_MIDI_RX_BUFSIZE       128
#define CFG_TUD_MIDI_TX_BUFSIZE       128
#define CFG_TUD_MIDI_EP_BUFSIZE       64

#endif /* _TUSB_CONFIG_H_ */
