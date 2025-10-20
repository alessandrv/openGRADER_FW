#include "input/keymap.h"
#include "op_keycodes.h"
#include "config_protocol.h"  // For slider_config_t

// Mixed controls keyboard keymap - 6x2 matrix with column 1: potentiometer/switches/encoder/magnetic switch, column 2: slider
const uint16_t keycodes[KEYMAP_LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS] = {
    // Layer 0: Mixed controls
    {
        {KC_NO, KC_NO},      // Row 0: Potentiometer 0, Slider 1
        {KC_A, KC_NO},       // Row 1: Switch 0 (A), Empty
        {KC_B, KC_NO},       // Row 2: Switch 1 (B), Empty
        {KC_C, KC_NO},       // Row 3: Switch 2 (C), Empty
        {KC_O, KC_NO},       // Row 4: Encoder 0, Empty
        {KC_M, KC_NO}        // Row 5: Magnetic Switch 0 (M), Empty
    }
};

// Encoder keymap for rotary encoders
const uint16_t encoder_map[KEYMAP_LAYER_COUNT][ENCODER_COUNT][2] = {
    // Layer 0: Default encoder mapping
    {
        {KC_UP, KC_DOWN}     // Encoder 0: [Clockwise, Counter-clockwise]
    }
};

// Potentiometer configuration map (per layer) - reuses slider infrastructure
const slider_config_t slider_config_map[KEYMAP_LAYER_COUNT][SLIDER_COUNT] = {
    // Layer 0: Default configuration
    {
        {
            .layer = 0,
            .slider_id = 0,
            .midi_cc = 1,
            .midi_channel = 0,
            .min_midi_value = 0,
            .max_midi_value = 127,
            .reserved = {0}
        },
        {
            .layer = 0,
            .slider_id = 1,
            .midi_cc = 2,
            .midi_channel = 0,
            .min_midi_value = 0,
            .max_midi_value = 127,
            .reserved = {0}
        }
    }
};