#include "keymap.h"
#include "op_keycodes.h"
#include "pin_config.h"

// Matrix pin configuration from pin_config.h
const pin_t matrix_cols[MATRIX_COLS] = MATRIX_COL_PINS;
const pin_t matrix_rows[MATRIX_ROWS] = MATRIX_ROW_PINS;

// keycodes (rows x cols). Using OpenGrader keycodes
const uint16_t keycodes[MATRIX_ROWS][MATRIX_COLS] = {
    {SEND_MIDI(1,1,43), SEND_MIDI_NOTE(1, 2) , KC_C, KC_D, KC_E, KC_F, KC_G}, // MIDI CC1 Ch1 Val43, B, C, D, E, F, G
    {KC_H, KC_I, KC_J, KC_K, KC_L, KC_M, KC_N}, // H, I, J, K, L, M, N
    {KC_O, KC_P, KC_Q, KC_R, KC_S, KC_T, KC_U}, // O, P, Q, R, S, T, U
    {KC_V, KC_W, KC_X, KC_Y, KC_Z, KC_1, KC_2}, // V, W, X, Y, Z, 1, 2
    {KC_3, KC_4, KC_5, KC_6, KC_7, KC_8, KC_9}  // 3, 4, 5, 6, 7, 8, 9
};

// Matrix pull configuration from pin_config.h
const uint32_t matrix_row_pulls[MATRIX_ROWS] = MATRIX_ROW_PULL_CONFIG;

// Encoder pin configuration from pin_config.h
const encoder_pins_t encoder_pins[ENCODER_COUNT] = ENCODER_PIN_CONFIG;
const uint32_t encoder_pulls[ENCODER_COUNT] = ENCODER_PULL_CONFIG;

// Default encoder map: [i][0]=LEFT(CCW), [i][1]=RIGHT(CW)
// Using OpenGrader keycodes
const uint16_t encoder_map[ENCODER_COUNT][2] = {
    { SEND_MIDI(1,1,45), SEND_MIDI_NOTE(1, 2) },       // A, B
    { KC_C, KC_D },       // C, D
    { KC_E, KC_F },       // E, F
    { KC_G, KC_H },       // G, H
    { KC_I, KC_J },       // I, J
    { KC_K, KC_L },       // K, L
    { KC_M, KC_N },       // M, N
    { KC_O, KC_P },       // O, P
    { KC_Q, KC_R },       // Q, R
    { KC_S, KC_T },       // S, T
    { KC_U, KC_V },       // U, V
    { KC_W, KC_X },       // W, X
    { KC_Y, KC_Z },       // Y, Z
    { KC_1, KC_2 },       // 1, 2
    { KC_3, KC_4 },       // 3, 4
    { KC_5, KC_6 },       // 5, 6
    { KC_7, KC_8 },       // 7, 8
    { KC_9, KC_0 },       // 9, 0
    { KC_ENTER, KC_ESC }, // Enter, Esc
    { KC_BSPC, KC_TAB },  // Backspace, Tab
    { KC_SPC, KC_MINS },  // Space, minus
    { KC_EQL, KC_LBRC },  // equal, left bracket
    { KC_RBRC, KC_BSLS }, // right bracket, backslash
    { KC_SCLN, KC_QUOT }, // semicolon, quote
    { KC_GRV, KC_COMM }   // grave, comma
};

uint16_t keymap_get_keycode(uint8_t row, uint8_t col)
{
    if (row >= MATRIX_ROWS || col >= MATRIX_COLS) return KC_NO;
    return keycodes[row][col];
}
