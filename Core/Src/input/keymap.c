#include "keymap.h"
#include "op_keycodes.h"
#include "pin_config.h"
#include "eeprom_emulation.h"
#include "i2c_manager.h"

// EEPROM initialization flag
static bool keymap_initialized = false;
static uint8_t active_layer_mask = 0x01;
static uint8_t default_layer_index = 0;

static void keymap_broadcast_layer_state(void);
static void keymap_set_layer_mask_internal(uint8_t mask, uint8_t default_index, bool propagate);

// Matrix pin configuration from pin_config.h
const pin_t matrix_cols[MATRIX_COLS] = MATRIX_COL_PINS;
const pin_t matrix_rows[MATRIX_ROWS] = MATRIX_ROW_PINS;

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
    // Layer 1: Function and navigation keysihhhii
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

// Matrix pull configuration from pin_config.h
const uint32_t matrix_row_pulls[MATRIX_ROWS] = MATRIX_ROW_PULL_CONFIG;

// Encoder pin configuration from pin_config.h
const encoder_pins_t encoder_pins[ENCODER_COUNT] = ENCODER_PIN_CONFIG;
const uint32_t encoder_pulls[ENCODER_COUNT] = ENCODER_PULL_CONFIG;

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

// Initialize keymap system
void keymap_init(void)
{
    if (!keymap_initialized) {
        // Initialize EEPROM emulation
        if (!eeprom_init()) {
            // EEPROM init failed, but we can still use defaults
            // This will be logged in eeprom_init()
        }
        uint8_t stored_mask = 0;
        uint8_t stored_default = 0;

        if (!eeprom_get_layer_state(&stored_mask, &stored_default)) {
            stored_mask = 0;
            stored_default = 0;
        }

        if (stored_default >= KEYMAP_LAYER_COUNT) {
            stored_default = 0;
        }

        uint8_t allowed_mask = (KEYMAP_LAYER_COUNT >= 8)
            ? 0xFF
            : (uint8_t)((1u << KEYMAP_LAYER_COUNT) - 1u);
        stored_mask &= allowed_mask;

        if (stored_mask == 0) {
            uint16_t startup_bit = (uint16_t)1u << stored_default;
            stored_mask = (uint8_t)(startup_bit & 0xFFu);
            if (stored_mask == 0) {
                stored_mask = 1u;
            }
        }

        active_layer_mask = stored_mask;
        default_layer_index = stored_default;
        keymap_initialized = true;
    }
}

uint16_t keymap_get_keycode(uint8_t layer, uint8_t row, uint8_t col)
{
    if (layer >= KEYMAP_LAYER_COUNT || row >= MATRIX_ROWS || col >= MATRIX_COLS) {
        return KC_NO;
    }

    if (!keymap_initialized) {
        keymap_init();
    }

    uint16_t stored = eeprom_get_keycode(layer, row, col);
    if (stored != 0) {
        return stored;
    }

    return keycodes[layer][row][col];
}

uint16_t keymap_get_active_keycode(uint8_t row, uint8_t col)
{
    if (row >= MATRIX_ROWS || col >= MATRIX_COLS) {
        return KC_NO;
    }

    if (!keymap_initialized) {
        keymap_init();
    }

    for (int8_t layer = KEYMAP_LAYER_COUNT - 1; layer >= 0; --layer) {
        if ((active_layer_mask & (uint8_t)(1u << layer)) == 0) {
            continue;
        }

        uint16_t code = keymap_get_keycode((uint8_t)layer, row, col);
        if (code == KC_TRANSPARENT) {
            continue;
        }

        if (code != 0) {
            return code;
        }
    }

    return KC_NO;
}

bool keymap_set_keycode(uint8_t layer, uint8_t row, uint8_t col, uint16_t keycode)
{
    if (layer >= KEYMAP_LAYER_COUNT || row >= MATRIX_ROWS || col >= MATRIX_COLS) {
        return false;
    }

    if (!keymap_initialized) {
        keymap_init();
    }

    return eeprom_set_keycode(layer, row, col, keycode);
}

bool keymap_get_encoder_map(uint8_t layer, uint8_t encoder_id, uint16_t *ccw_keycode, uint16_t *cw_keycode)
{
    if (layer >= KEYMAP_LAYER_COUNT || encoder_id >= ENCODER_COUNT || !ccw_keycode || !cw_keycode) {
        return false;
    }

    if (!keymap_initialized) {
        keymap_init();
    }

    if (eeprom_get_encoder_map(layer, encoder_id, ccw_keycode, cw_keycode)) {
        if (*ccw_keycode != 0 || *cw_keycode != 0) {
            return true;
        }
    }

    *ccw_keycode = encoder_map[layer][encoder_id][0];
    *cw_keycode = encoder_map[layer][encoder_id][1];
    return true;
}

bool keymap_get_active_encoder_map(uint8_t encoder_id, uint16_t *ccw_keycode, uint16_t *cw_keycode)
{
    if (encoder_id >= ENCODER_COUNT || !ccw_keycode || !cw_keycode) {
        return false;
    }

    if (!keymap_initialized) {
        keymap_init();
    }

    for (int8_t layer = KEYMAP_LAYER_COUNT - 1; layer >= 0; --layer) {
        if ((active_layer_mask & (uint8_t)(1u << layer)) == 0) {
            continue;
        }

        uint16_t temp_ccw = 0;
        uint16_t temp_cw = 0;
        keymap_get_encoder_map((uint8_t)layer, encoder_id, &temp_ccw, &temp_cw);

        if (temp_ccw == KC_TRANSPARENT && temp_cw == KC_TRANSPARENT) {
            continue;
        }

        if (temp_ccw != 0 || temp_cw != 0) {
            *ccw_keycode = temp_ccw;
            *cw_keycode = temp_cw;
            return true;
        }
    }

    *ccw_keycode = KC_NO;
    *cw_keycode = KC_NO;
    return true;
}

bool keymap_set_encoder_map(uint8_t layer, uint8_t encoder_id, uint16_t ccw_keycode, uint16_t cw_keycode)
{
    if (layer >= KEYMAP_LAYER_COUNT || encoder_id >= ENCODER_COUNT) {
        return false;
    }

    if (!keymap_initialized) {
        keymap_init();
    }

    return eeprom_set_encoder_map(layer, encoder_id, ccw_keycode, cw_keycode);
}

static void keymap_set_layer_mask_internal(uint8_t mask, uint8_t default_index, bool propagate)
{
    if (default_index >= KEYMAP_LAYER_COUNT) {
        default_index = 0;
    }

    uint8_t allowed_mask = (KEYMAP_LAYER_COUNT >= 8)
        ? 0xFF
        : (uint8_t)((1u << KEYMAP_LAYER_COUNT) - 1u);
    mask &= allowed_mask;

    if (mask == 0) {
        uint16_t startup_bit = (uint16_t)1u << default_index;
        mask = (uint8_t)(startup_bit & 0xFFu);
        if (mask == 0) {
            mask = 1u;
        }
    }

    uint8_t previous_mask = active_layer_mask;
    uint8_t previous_default = default_layer_index;

    if (mask == previous_mask && default_index == previous_default) {
        return;
    }

    active_layer_mask = mask;
    default_layer_index = default_index;

    if (propagate && (previous_mask != active_layer_mask || previous_default != default_layer_index)) {
        keymap_broadcast_layer_state();
    }

    if (previous_default != default_layer_index) {
        uint16_t persist_bit = (uint16_t)1u << default_layer_index;
        uint8_t persist_mask = (uint8_t)(persist_bit & 0xFFu);
        if (persist_mask == 0) {
            persist_mask = 1u;
        }
        eeprom_set_layer_state(persist_mask, default_layer_index);
    }
}

static void keymap_broadcast_layer_state(void)
{
    i2c_manager_broadcast_layer_state(active_layer_mask, default_layer_index);
}

uint8_t keymap_get_layer_mask(void)
{
    return active_layer_mask;
}

uint8_t keymap_get_default_layer(void)
{
    return default_layer_index;
}

void keymap_layer_on(uint8_t layer)
{
    if (layer >= KEYMAP_LAYER_COUNT) {
        return;
    }

    uint8_t mask = active_layer_mask | (uint8_t)(1u << layer);
    keymap_set_layer_mask_internal(mask, default_layer_index, true);
}

void keymap_layer_off(uint8_t layer)
{
    if (layer >= KEYMAP_LAYER_COUNT) {
        return;
    }

    uint8_t mask = (uint8_t)(active_layer_mask & ~(uint8_t)(1u << layer));
    keymap_set_layer_mask_internal(mask, default_layer_index, true);
}

void keymap_layer_move(uint8_t layer)
{
    if (layer >= KEYMAP_LAYER_COUNT) {
        return;
    }

    keymap_set_layer_mask_internal((uint8_t)(1u << layer), layer, true);
}

void keymap_apply_layer_mask(uint8_t mask, uint8_t default_layer, bool propagate)
{
    keymap_set_layer_mask_internal(mask, default_layer, propagate);
}

bool keymap_translate_keycode(uint16_t keycode, bool pressed, uint8_t *hid_code)
{
    if (!hid_code) {
        return false;
    }

    *hid_code = 0;

    if (IS_OP_MO_LAYER(keycode)) {
        uint8_t target = OP_LAYER_TARGET(keycode);
        if (target < KEYMAP_LAYER_COUNT) {
            if (pressed) {
                keymap_layer_on(target);
            } else {
                keymap_layer_off(target);
            }
        }
        return false;
    }

    if (IS_OP_TO_LAYER(keycode)) {
        uint8_t target = OP_LAYER_TARGET(keycode);
        if (target < KEYMAP_LAYER_COUNT && pressed) {
            keymap_layer_move(target);
        }
        return false;
    }

    *hid_code = op_keycode_to_hid(keycode);
    return (*hid_code != 0);
}
