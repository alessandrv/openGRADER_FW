#ifndef USB_APP_H
#define USB_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

void usb_app_init(void);
void usb_app_task(void);
bool usb_app_mouse_report(uint8_t buttons, int8_t delta_x, int8_t delta_y, int8_t wheel, int8_t pan);
bool usb_app_midi_send_packet(uint8_t const packet[4]);
bool usb_app_midi_send_note_on(uint8_t channel, uint8_t note, uint8_t velocity);
bool usb_app_midi_send_note_off(uint8_t channel, uint8_t note);
bool usb_app_midi_send_cc(uint8_t channel, uint8_t controller, uint8_t value);
void usb_app_cdc_printf(const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif /* USB_APP_H */
