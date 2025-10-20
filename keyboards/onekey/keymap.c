#include "input/keymap.h"
#include "op_keycodes.h"

// keycodes [layer][rows][cols]. Layer 0 populated, higher layers default to transparent
const uint16_t keycodes[KEYMAP_LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS] = {
    // Layer 0: Single key - A
    {
        {KC_A}
    },
    // Layer 1: Single key - 1
    {
        {KC_1}
    },
    // Layer 2: Single key - Space
    {
        {KC_SPACE}
    },
    // Layer 3: Single key - Enter
    {
        {KC_ENTER}
    },
    // Layer 4: Single key - Escape
    {
        {KC_ESC}
    },
    // Layer 5: MIDI Note Middle C
    {
        {SEND_MIDI_NOTE(1, 60)}
    },
    // Layer 6: Layer toggle
    {
        {KC_MO(1)}
    },
    // Layer 7: Reserved
    {
        {KC_NO}
    }
};

// Default encoder map - empty for onekey
#if ENCODER_COUNT > 0
const uint16_t encoder_map[KEYMAP_LAYER_COUNT][ENCODER_COUNT][2] = {
    [0 ... KEYMAP_LAYER_COUNT-1] = {
        [0 ... ENCODER_COUNT-1] = { KC_TRANSPARENT, KC_TRANSPARENT }
    }
};
#else
// When there are no encoders, provide an empty array to avoid compilation errors
const uint16_t encoder_map[KEYMAP_LAYER_COUNT][1][2] = {
    [0 ... KEYMAP_LAYER_COUNT-1] = {
        { KC_TRANSPARENT, KC_TRANSPARENT }
    }
};
#endif
