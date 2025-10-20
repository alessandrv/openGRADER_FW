#include "input/keymap.h"
#include "op_keycodes.h"

// keycodes [layer][rows][cols]. Layer 0 populated, higher layers default to transparent
const uint16_t keycodes[KEYMAP_LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS] = {
    // Layer 0: Base typing layer with layer toggles
    {
        {KC_Q, KC_W, KC_E, KC_R, KC_T, KC_MO(1), KC_TO(2)},
        {KC_A, KC_S, KC_D, KC_F, KC_G, KC_H, KC_J},
        {KC_Z, KC_X, KC_C, KC_V, KC_B, KC_N, KC_M},
        {KC_1, KC_2, KC_3, KC_4, KC_5, KC_6, KC_7},
        {KC_8, KC_9, KC_0, KC_MINUS, KC_EQUAL, KC_BSPC, KC_ENTER}
    },
    // Layer 1: Function and navigation keys
    {
        {KC_F1, KC_F2, KC_F3, KC_F4, KC_F5, KC_F6, KC_F7},
        {KC_F8, KC_F9, KC_F10, KC_F11, KC_F12, KC_PSCR, KC_PAUS},
        {KC_NO, KC_HOME, KC_UP, KC_END, KC_PGUP, KC_NO, KC_NO},
        {KC_NO, KC_LEFT, KC_DOWN, KC_RIGHT, KC_PGDN, KC_NO, KC_NO},
        {KC_NO, KC_NO, KC_DEL, KC_INS, KC_NO, KC_NO, KC_NO}
    },
    // Layer 2: Number row and symbol helpers
    {
        {KC_1, KC_2, KC_3, KC_4, KC_5, KC_6, KC_7},
        {KC_8, KC_9, KC_0, KC_MINUS, KC_EQUAL, KC_NO, KC_NO},
        {KC_Q, KC_W, KC_E, KC_R, KC_T, KC_Y, KC_U},
        {KC_I, KC_O, KC_P, KC_LBRC, KC_RBRC, KC_BSLS, KC_NO},
        {KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO}
    },
    // Layer 3: Alternating letter pattern for testing
    {
        {KC_Q, KC_NO, KC_W, KC_NO, KC_E, KC_NO, KC_R},
        {KC_NO, KC_T, KC_NO, KC_Y, KC_NO, KC_U, KC_NO},
        {KC_I, KC_NO, KC_O, KC_NO, KC_P, KC_NO, KC_NO},
        {KC_NO, KC_A, KC_NO, KC_S, KC_NO, KC_D, KC_NO},
        {KC_F, KC_NO, KC_G, KC_NO, KC_H, KC_NO, KC_J}
    },
    // Layer 4: Function-key stripes
    {
        {KC_NO, KC_F1, KC_NO, KC_F2, KC_NO, KC_F3, KC_NO},
        {KC_F4, KC_NO, KC_F5, KC_NO, KC_F6, KC_NO, KC_F7},
        {KC_NO, KC_F8, KC_NO, KC_F9, KC_NO, KC_F10, KC_NO},
        {KC_F11, KC_NO, KC_F12, KC_NO, KC_PSCR, KC_NO, KC_PAUS},
        {KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO}
    },
    // Layer 5: MIDI and media testing
    {
        {KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO},
        {KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO},
        {SEND_MIDI_CC(1, 1, 31), SEND_MIDI_CC(1, 2, 63), SEND_MIDI_CC(1, 3, 95), KC_NO, KC_NO, KC_NO, KC_NO},
        {SEND_MIDI_NOTE(1, 60), SEND_MIDI_NOTE(1, 62), SEND_MIDI_NOTE(1, 64), KC_NO, KC_NO, KC_NO, KC_NO},
        {KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO}
    },
    // Layer 6: Modifier cluster
    {
        {KC_LCTL, KC_LSFT, KC_LALT, KC_LGUI, KC_NO, KC_NO, KC_NO},
        {KC_RCTL, KC_RSFT, KC_RALT, KC_RGUI, KC_NO, KC_NO, KC_NO},
        {KC_NO, KC_NO, KC_TAB, KC_SPACE, KC_ENTER, KC_NO, KC_NO},
        {KC_NO, KC_NO, KC_ESC, KC_CAPS, KC_NO, KC_NO, KC_NO},
        {KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO}
    },
    // Layer 7: Reserved for future use (mostly empty)
    {
        {KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO},
        {KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO},
        {KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO},
        {KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO},
        {KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO}
    }
};

// Default encoder map: [i][0]=LEFT(CCW), [i][1]=RIGHT(CW)
// Using OpenGrader keycodes
const uint16_t encoder_map[KEYMAP_LAYER_COUNT][ENCODER_COUNT][2] = {
    {
        { SEND_MIDI(1,1,45), SEND_MIDI_NOTE(1, 2) },
        { KC_C, KC_D },
        { KC_E, KC_F },
        { KC_G, KC_H },
        { KC_I, KC_J },
        { KC_K, KC_L },
        { KC_M, KC_N },
        { KC_O, KC_P },
        { KC_Q, KC_R },
        { KC_S, KC_T },
        { KC_U, KC_V },
        { KC_W, KC_X },
        { KC_Y, KC_Z },
        { KC_1, KC_2 },
        { KC_3, KC_4 },
        { KC_5, KC_6 },
        { KC_7, KC_8 },
        { KC_9, KC_0 },
        { KC_ENTER, KC_ESC },
        { KC_BSPC, KC_TAB },
        { KC_SPC, KC_MINS },
        { KC_EQL, KC_LBRC },
        { KC_RBRC, KC_BSLS },
        { KC_SCLN, KC_QUOT },
        { KC_GRV, KC_COMM }
    },
    [1 ... KEYMAP_LAYER_COUNT-1] = {
        [0 ... ENCODER_COUNT-1] = { KC_TRANSPARENT, KC_TRANSPARENT }
    }
};
