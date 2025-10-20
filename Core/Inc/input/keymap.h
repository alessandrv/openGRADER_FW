#ifndef KEYMAP_H
#define KEYMAP_H

#include <stdint.h>
#include <stdbool.h>
#include "input/matrix.h"  // This includes the keyboard config which defines pin_t, MATRIX_ROWS, MATRIX_COLS
#include "op_keycodes.h"
#include "config_protocol.h"  // For slider_config_t

#ifndef KEYMAP_LAYER_COUNT
#define KEYMAP_LAYER_COUNT 8
#endif

// Define pins using the board GPIO naming used in CubeMX (port + pin)
// For simplicity we'll expose arrays of pin_t used by matrix.c
// pin_t is defined in the keyboard config (via matrix.h)
extern const pin_t matrix_cols[MATRIX_COLS];
extern const pin_t matrix_rows[MATRIX_ROWS];
// per-row pull configuration (GPIO_NOPULL, GPIO_PULLUP, GPIO_PULLDOWN)
extern const uint32_t matrix_row_pulls[MATRIX_ROWS];

// Simple keycode map [layer][row][col]. Use OpenGrader keycodes; KC_NO = no key.
extern const uint16_t keycodes[KEYMAP_LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS];

// Initialize keymap system
void keymap_init(void);

// helper functions
uint16_t keymap_get_keycode(uint8_t layer, uint8_t row, uint8_t col);
uint16_t keymap_get_active_keycode(uint8_t row, uint8_t col);
bool keymap_set_keycode(uint8_t layer, uint8_t row, uint8_t col, uint16_t keycode);
bool keymap_translate_keycode(uint16_t keycode, bool pressed, uint8_t *hid_code);

// Encoder helper functions
bool keymap_get_encoder_map(uint8_t layer, uint8_t encoder_id, uint16_t *ccw_keycode, uint16_t *cw_keycode);
bool keymap_get_active_encoder_map(uint8_t encoder_id, uint16_t *ccw_keycode, uint16_t *cw_keycode);
bool keymap_set_encoder_map(uint8_t layer, uint8_t encoder_id, uint16_t ccw_keycode, uint16_t cw_keycode);

// Slider helper functions (slider_config_t is defined in config_protocol.h)
bool keymap_get_slider_config(uint8_t layer, uint8_t slider_id, slider_config_t *config);
bool keymap_get_active_slider_config(uint8_t slider_id, slider_config_t *config);
bool keymap_set_slider_config(uint8_t layer, uint8_t slider_id, const slider_config_t *config);

// Encoders configuration
// ENCODER_COUNT and encoder_pins_t are defined in the keyboard config (via matrix.h)
// If not defined, provide defaults
#ifndef ENCODER_COUNT
#define ENCODER_COUNT 25  // Default encoder count
#endif

// encoder_pins_t should be defined in keyboard config
// Encoder pins structure with PINA and PINB
#if ENCODER_COUNT > 0
extern const encoder_pins_t encoder_pins[ENCODER_COUNT];
// Per-encoder pull configuration applied to both A and B (typically GPIO_PULLUP)
extern const uint32_t encoder_pulls[ENCODER_COUNT];
// Per-encoder key mapping: [layer][encoder][dir] -> OpenGrader keycode
// dir index: 0 = CCW (left), 1 = CW (right)
extern const uint16_t encoder_map[KEYMAP_LAYER_COUNT][ENCODER_COUNT][2];
#else
// For keyboards without encoders, provide minimal arrays to avoid compilation errors
extern const encoder_pins_t encoder_pins[1];
extern const uint32_t encoder_pulls[1];
extern const uint16_t encoder_map[KEYMAP_LAYER_COUNT][1][2];
#endif

// Sliders configuration
// SLIDER_COUNT is defined in the keyboard config (via matrix.h)
#ifndef SLIDER_COUNT
#define SLIDER_COUNT 0
#endif

#if SLIDER_COUNT > 0
// Per-slider configuration mapping: [layer][slider] -> MIDI config
// Stores MIDI CC, channel, and value ranges for each slider on each layer
extern const slider_config_t slider_config_map[KEYMAP_LAYER_COUNT][SLIDER_COUNT];
#else
// For keyboards without sliders, provide minimal array to avoid compilation errors
extern const slider_config_t slider_config_map[KEYMAP_LAYER_COUNT][1];
#endif

// Layer state management helpers
void keymap_layer_on(uint8_t layer);
void keymap_layer_off(uint8_t layer);
void keymap_layer_move(uint8_t layer);
uint8_t keymap_get_layer_mask(void);
uint8_t keymap_get_default_layer(void);
void keymap_apply_layer_mask(uint8_t mask, uint8_t default_layer, bool propagate, bool update_default);
void keymap_persist_default_layer_state(void);


#endif // KEYMAP_H
