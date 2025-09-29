#ifndef KEYMAP_H
#define KEYMAP_H

#include <stdint.h>
#include <stdbool.h>
#include "input/matrix.h"
#include "op_keycodes.h"

// Define pins using the board GPIO naming used in CubeMX (port + pin)
// For simplicity we'll expose arrays of pin_t used by matrix.c
extern const pin_t matrix_cols[MATRIX_COLS];
extern const pin_t matrix_rows[MATRIX_ROWS];
// per-row pull configuration (GPIO_NOPULL, GPIO_PULLUP, GPIO_PULLDOWN)
extern const uint32_t matrix_row_pulls[MATRIX_ROWS];

// Simple keycode map (rows x cols). Use OpenGrader keycodes; KC_NO = no key.
extern const uint16_t keycodes[MATRIX_ROWS][MATRIX_COLS];

// helper functions
uint16_t keymap_get_keycode(uint8_t row, uint8_t col);
bool keymap_set_keycode(uint8_t row, uint8_t col, uint16_t keycode);

// Encoder helper functions
bool keymap_get_encoder_map(uint8_t encoder_id, uint16_t *ccw_keycode, uint16_t *cw_keycode);
bool keymap_set_encoder_map(uint8_t encoder_id, uint16_t ccw_keycode, uint16_t cw_keycode);

// Encoders configuration
#ifndef ENCODER_COUNT
#define ENCODER_COUNT 25  // Restored full encoder count
#endif

// Define encoder pins structure
typedef struct {
    pin_t pin_a;
    pin_t pin_b;
} encoder_pins_t;

// Encoder pins structure with PINA and PINB
extern const encoder_pins_t encoder_pins[ENCODER_COUNT];

// Per-encoder pull configuration applied to both A and B (typically GPIO_PULLUP)
extern const uint32_t encoder_pulls[ENCODER_COUNT];

// Per-encoder key mapping: [encoder][dir] -> OpenGrader keycode
// dir index: 0 = CCW (left), 1 = CW (right)
extern const uint16_t encoder_map[ENCODER_COUNT][2];


#endif // KEYMAP_H
