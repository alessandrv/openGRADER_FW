#include "input/keymap.h"
#include "op_keycodes.h"
#include "config_protocol.h"  // For slider_config_t

// Slider keyboard keymap - 5x3 matrix with switches and sliders
const uint16_t keycodes[KEYMAP_LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS] = {
    // Layer 0: Basic keys
    {
        {KC_1, KC_NO, KC_NO},           // Row 0: Switch 1, Slider 0, Slider 1
        {KC_2, KC_NO, KC_NO},           // Row 1: Switch 2, Empty, Empty
        {KC_3, KC_NO, KC_NO},           // Row 2: Switch 3, Empty, Empty
        {KC_4, KC_NO, KC_NO},           // Row 3: Switch 4, Empty, Empty
        {KC_5, KC_NO, KC_NO}            // Row 4: Switch 5, Empty, Empty
    },
    // Layer 1: Function keys
    [1] = {
        {KC_F1, KC_NO, KC_NO},          // Row 0: F1, Slider 0, Slider 1
        {KC_F2, KC_NO, KC_NO},          // Row 1: F2, Empty, Empty
        {KC_F3, KC_NO, KC_NO},          // Row 2: F3, Empty, Empty
        {KC_F4, KC_NO, KC_NO},          // Row 3: F4, Empty, Empty
        {KC_F5, KC_NO, KC_NO}           // Row 4: F5, Empty, Empty
    },
    // Layer 2: Letters
    [2] = {
        {KC_A, KC_NO, KC_NO},           // Row 0: A, Slider 0, Slider 1
        {KC_B, KC_NO, KC_NO},           // Row 1: B, Empty, Empty
        {KC_C, KC_NO, KC_NO},           // Row 2: C, Empty, Empty
        {KC_D, KC_NO, KC_NO},           // Row 3: D, Empty, Empty
        {KC_E, KC_NO, KC_NO}            // Row 4: E, Empty, Empty
    },
    // Other layers: Default
    [3 ... KEYMAP_LAYER_COUNT-1] = {
        {KC_TRANSPARENT, KC_NO, KC_NO}, // Row 0: Transparent, Slider 0, Slider 1
        {KC_TRANSPARENT, KC_NO, KC_NO}, // Row 1: Transparent, Empty, Empty
        {KC_TRANSPARENT, KC_NO, KC_NO}, // Row 2: Transparent, Empty, Empty
        {KC_TRANSPARENT, KC_NO, KC_NO}, // Row 3: Transparent, Empty, Empty
        {KC_TRANSPARENT, KC_NO, KC_NO}  // Row 4: Transparent, Empty, Empty
    }
};

// No encoders on this keyboard
const uint16_t encoder_map[KEYMAP_LAYER_COUNT][1][2] = {
    [0 ... KEYMAP_LAYER_COUNT-1] = {
        { KC_TRANSPARENT, KC_TRANSPARENT }
    }
};

// Slider configuration map (per layer)
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
    },
    // Layer 1: Different CC values
    [1] = {
        {
            .layer = 1,
            .slider_id = 0,
            .midi_cc = 3,
            .midi_channel = 0,
            .min_midi_value = 0,
            .max_midi_value = 127,
            .reserved = {0}
        },
        {
            .layer = 1,
            .slider_id = 1,
            .midi_cc = 4,
            .midi_channel = 0,
            .min_midi_value = 0,
            .max_midi_value = 127,
            .reserved = {0}
        }
    },
    // Layer 2: Different CC values
    [2] = {
        {
            .layer = 2,
            .slider_id = 0,
            .midi_cc = 5,
            .midi_channel = 0,
            .min_midi_value = 0,
            .max_midi_value = 127,
            .reserved = {0}
        },
        {
            .layer = 2,
            .slider_id = 1,
            .midi_cc = 6,
            .midi_channel = 0,
            .min_midi_value = 0,
            .max_midi_value = 127,
            .reserved = {0}
        }
    },
    // Remaining layers: use transparent (CC = 0)
    [3 ... KEYMAP_LAYER_COUNT-1] = {
        {
            .layer = 0,  // Will be set correctly at runtime
            .slider_id = 0,
            .midi_cc = 0,  // 0 = transparent/disabled
            .midi_channel = 0,
            .min_midi_value = 0,
            .max_midi_value = 127,
            .reserved = {0}
        },
        {
            .layer = 0,  // Will be set correctly at runtime
            .slider_id = 1,
            .midi_cc = 0,  // 0 = transparent/disabled
            .midi_channel = 0,
            .min_midi_value = 0,
            .max_midi_value = 127,
            .reserved = {0}
        }
    }
};
