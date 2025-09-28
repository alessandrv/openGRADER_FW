#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>
#include "stm32g4xx_hal.h"
#include "keymap.h"

// HID keycodes for left/right rotation (Z/X)
#define ENC_KEY_LEFT  0x1D // HID Usage ID for 'Z'
#define ENC_KEY_RIGHT 0x1B // HID Usage ID for 'X'

// Encoder event callback type
typedef void (*encoder_event_cb_t)(uint8_t encoder_idx, uint8_t direction, uint8_t keycode);

void encoder_init(void);
void encoder_task(void); // call periodically to emit HID taps
void encoder_register_callback(encoder_event_cb_t cb); // register callback for slave mode

// HAL EXTI callback hook
void encoder_handle_exti(uint16_t pin);

#endif // ENCODER_H
