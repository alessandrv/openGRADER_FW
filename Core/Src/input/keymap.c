#include "keymap.h"
#include "op_keycodes.h"
#include "pin_config.h"
#include "eeprom_emulation.h"
#include "i2c_manager.h"
#include "usb_app.h"
#include "config_protocol.h"  // For slider_config_t definition

#include <stddef.h>

// EEPROM initialization flag
static bool keymap_initialized = false;
static uint8_t active_layer_mask = 0x01;
static uint8_t default_layer_index = 0;
static uint8_t persistent_layer_index = 0;

typedef struct {
    uint8_t layer;
    uint8_t refcount;
} momentary_layer_entry_t;

static momentary_layer_entry_t momentary_layers[KEYMAP_LAYER_COUNT];
static uint8_t momentary_layer_count = 0;

static void keymap_broadcast_layer_state(void);
static void keymap_recompute_active_mask(bool propagate, bool force_broadcast);
static uint8_t keymap_first_active_layer(uint8_t mask);
static void keymap_clear_momentary_layers(void);
static momentary_layer_entry_t *keymap_find_momentary_entry(uint8_t layer);

// Matrix pin configuration from pin_config.h (which includes keyboard config)
const pin_t matrix_cols[MATRIX_COLS] = MATRIX_COL_PINS;
const pin_t matrix_rows[MATRIX_ROWS] = MATRIX_ROW_PINS;

// Matrix pull configuration from pin_config.h
const uint32_t matrix_row_pulls[MATRIX_ROWS] = MATRIX_ROW_PULL_CONFIG;

// Encoder pin configuration from pin_config.h
#if ENCODER_COUNT > 0
const encoder_pins_t encoder_pins[ENCODER_COUNT] = ENCODER_PIN_CONFIG;
const uint32_t encoder_pulls[ENCODER_COUNT] = ENCODER_PULL_CONFIG;
#else
// Provide empty arrays when no encoders are defined
const encoder_pins_t encoder_pins[1] = {{{NULL, 0}, {NULL, 0}}};
const uint32_t encoder_pulls[1] = {GPIO_NOPULL};
#endif

// The keycodes and encoder_map arrays are defined in the keyboard-specific keymap.c file
// They are declared as extern here and will be linked from keyboards/<name>/keymap.c

// Initialize keymap system
void keymap_init(void)
{
    if (keymap_initialized) {
        return;
    }

    if (!eeprom_init()) {
        // EEPROM init failure already logged; proceed with defaults
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
        stored_mask = (uint8_t)(1u << stored_default);
        if (stored_mask == 0) {
            stored_mask = 1u;
        }
    }

    default_layer_index = stored_default;
    persistent_layer_index = keymap_first_active_layer(stored_mask);
    if (persistent_layer_index >= KEYMAP_LAYER_COUNT) {
        persistent_layer_index = default_layer_index;
    }

    keymap_clear_momentary_layers();
    active_layer_mask = 0;
    keymap_recompute_active_mask(false, false);

    keymap_initialized = true;
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
    uint16_t compiled = keycodes[layer][row][col];
    
    // Detailed debug logging
    usb_app_cdc_printf("KEYMAP_GET: [L%d,%d,%d] EEPROM=0x%04X Compiled=0x%04X\r\n", 
                       layer, row, col, stored, compiled);
    
    if (stored != 0) {
        return stored;
    }

    return compiled;
}

uint16_t keymap_get_active_keycode(uint8_t row, uint8_t col)
{
    if (row >= MATRIX_ROWS || col >= MATRIX_COLS) {
        return KC_NO;
    }

    if (!keymap_initialized) {
        keymap_init();
    }

    for (int8_t idx = (int8_t)momentary_layer_count - 1; idx >= 0; --idx) {
        uint8_t layer = momentary_layers[(uint8_t)idx].layer;
        if (layer >= KEYMAP_LAYER_COUNT) {
            continue;
        }

        uint16_t code = keymap_get_keycode(layer, row, col);
        if (code == KC_TRANSPARENT || code == KC_NO) {
            continue;
        }

        return code;
    }

    if (persistent_layer_index < KEYMAP_LAYER_COUNT) {
        uint16_t base_code = keymap_get_keycode(persistent_layer_index, row, col);
        if (base_code != KC_TRANSPARENT && base_code != KC_NO) {
            return base_code;
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
#if ENCODER_COUNT > 0
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
#else
    // No encoders on this keyboard
    if (ccw_keycode) *ccw_keycode = KC_NO;
    if (cw_keycode) *cw_keycode = KC_NO;
    return false;
#endif
}

bool keymap_get_active_encoder_map(uint8_t encoder_id, uint16_t *ccw_keycode, uint16_t *cw_keycode)
{
#if ENCODER_COUNT > 0
    if (encoder_id >= ENCODER_COUNT || !ccw_keycode || !cw_keycode) {
        return false;
    }

    if (!keymap_initialized) {
        keymap_init();
    }

    for (int8_t idx = (int8_t)momentary_layer_count - 1; idx >= 0; --idx) {
        uint8_t layer = momentary_layers[(uint8_t)idx].layer;
        if (layer >= KEYMAP_LAYER_COUNT) {
            continue;
        }

        uint16_t temp_ccw = 0;
        uint16_t temp_cw = 0;
        keymap_get_encoder_map(layer, encoder_id, &temp_ccw, &temp_cw);

        if (temp_ccw == KC_TRANSPARENT && temp_cw == KC_TRANSPARENT) {
            continue;
        }

        if (temp_ccw != KC_NO || temp_cw != KC_NO) {
            *ccw_keycode = temp_ccw;
            *cw_keycode = temp_cw;
            return true;
        }
    }

    if (persistent_layer_index < KEYMAP_LAYER_COUNT) {
        uint16_t temp_ccw = 0;
        uint16_t temp_cw = 0;
        keymap_get_encoder_map(persistent_layer_index, encoder_id, &temp_ccw, &temp_cw);

        if (!(temp_ccw == KC_TRANSPARENT && temp_cw == KC_TRANSPARENT)) {
            if (temp_ccw != KC_NO || temp_cw != KC_NO) {
                *ccw_keycode = temp_ccw;
                *cw_keycode = temp_cw;
                return true;
            }
        }
    }

    *ccw_keycode = KC_NO;
    *cw_keycode = KC_NO;
    return true;
#else
    // No encoders on this keyboard
    if (ccw_keycode) *ccw_keycode = KC_NO;
    if (cw_keycode) *cw_keycode = KC_NO;
    return false;
#endif
}

bool keymap_set_encoder_map(uint8_t layer, uint8_t encoder_id, uint16_t ccw_keycode, uint16_t cw_keycode)
{
#if ENCODER_COUNT > 0
    if (layer >= KEYMAP_LAYER_COUNT || encoder_id >= ENCODER_COUNT) {
        return false;
    }

    if (!keymap_initialized) {
        keymap_init();
    }

    return eeprom_set_encoder_map(layer, encoder_id, ccw_keycode, cw_keycode);
#else
    // No encoders on this keyboard
    return false;
#endif
}

bool keymap_get_slider_config(uint8_t layer, uint8_t slider_id, slider_config_t *config)
{
#if SLIDER_COUNT > 0
    if (layer >= KEYMAP_LAYER_COUNT || slider_id >= SLIDER_COUNT || !config) {
        return false;
    }

    if (!keymap_initialized) {
        keymap_init();
    }

    // Try to get from EEPROM first
    if (eeprom_get_slider_config(layer, slider_id, config)) {
        return true;
    }

    // Fall back to default configuration from slider_config_map
    const slider_config_t *default_config = &slider_config_map[layer][slider_id];
    *config = *default_config;
    config->layer = layer;
    config->slider_id = slider_id;
    return true;
#else
    // No sliders on this keyboard
    return false;
#endif
}

bool keymap_get_active_slider_config(uint8_t slider_id, slider_config_t *config)
{
#if SLIDER_COUNT > 0
    if (slider_id >= SLIDER_COUNT || !config) {
        return false;
    }

    if (!keymap_initialized) {
        keymap_init();
    }

    // Check momentary layers first (top to bottom)
    for (int8_t idx = (int8_t)momentary_layer_count - 1; idx >= 0; --idx) {
        uint8_t layer = momentary_layers[(uint8_t)idx].layer;
        if (layer >= KEYMAP_LAYER_COUNT) {
            continue;
        }

        slider_config_t temp_config;
        if (keymap_get_slider_config(layer, slider_id, &temp_config)) {
            // Check if this layer has a non-transparent configuration
            // For sliders, we consider a config transparent if MIDI CC is 0
            if (temp_config.midi_cc != 0) {
                *config = temp_config;
                return true;
            }
        }
    }

    // Fall back to persistent layer
    return keymap_get_slider_config(persistent_layer_index, slider_id, config);
#else
    // No sliders on this keyboard
    return false;
#endif
}

bool keymap_set_slider_config(uint8_t layer, uint8_t slider_id, const slider_config_t *config)
{
#if SLIDER_COUNT > 0
    if (layer >= KEYMAP_LAYER_COUNT || slider_id >= SLIDER_COUNT || !config) {
        return false;
    }

    if (!keymap_initialized) {
        keymap_init();
    }

    return eeprom_set_slider_config(layer, slider_id, config);
#else
    // No sliders on this keyboard
    return false;
#endif
}



static uint8_t keymap_first_active_layer(uint8_t mask)
{
    for (uint8_t idx = 0; idx < KEYMAP_LAYER_COUNT; ++idx) {
        if ((mask & (uint8_t)(1u << idx)) != 0u) {
            return idx;
        }
    }

    return KEYMAP_LAYER_COUNT;
}

static void keymap_clear_momentary_layers(void)
{
    for (uint8_t i = 0; i < KEYMAP_LAYER_COUNT; ++i) {
        momentary_layers[i].layer = 0;
        momentary_layers[i].refcount = 0;
    }
    momentary_layer_count = 0;
}

static momentary_layer_entry_t *keymap_find_momentary_entry(uint8_t layer)
{
    for (uint8_t i = 0; i < momentary_layer_count; ++i) {
        if (momentary_layers[i].layer == layer) {
            return &momentary_layers[i];
        }
    }

    return NULL;
}

static void keymap_recompute_active_mask(bool propagate, bool force_broadcast)
{
    if (persistent_layer_index >= KEYMAP_LAYER_COUNT) {
        persistent_layer_index = default_layer_index;
        if (persistent_layer_index >= KEYMAP_LAYER_COUNT) {
            persistent_layer_index = 0;
        }
    }

    uint8_t allowed_mask = (KEYMAP_LAYER_COUNT >= 8)
        ? 0xFF
        : (uint8_t)((1u << KEYMAP_LAYER_COUNT) - 1u);

    uint8_t mask = (uint8_t)(1u << persistent_layer_index);
    if (mask == 0) {
        mask = 1u;
    }

    for (uint8_t i = 0; i < momentary_layer_count; ++i) {
        uint8_t layer = momentary_layers[i].layer;
        if (layer >= KEYMAP_LAYER_COUNT) {
            continue;
        }
        mask |= (uint8_t)(1u << layer);
    }

    mask &= allowed_mask;

    if (mask == 0) {
        persistent_layer_index = default_layer_index;
        if (persistent_layer_index >= KEYMAP_LAYER_COUNT) {
            persistent_layer_index = 0;
        }
        mask = (uint8_t)(1u << persistent_layer_index);
        if (mask == 0) {
            mask = 1u;
        }
    }

    bool changed = (mask != active_layer_mask);
    if (changed) {
        active_layer_mask = mask;
    }

    if (propagate && (changed || force_broadcast)) {
        keymap_broadcast_layer_state();
    }
}

static void keymap_broadcast_layer_state(void)
{
    i2c_manager_broadcast_layer_state(active_layer_mask, default_layer_index);
}

uint8_t keymap_get_layer_mask(void)
{
    if (!keymap_initialized) {
        keymap_init();
    }
    return active_layer_mask;
}

uint8_t keymap_get_default_layer(void)
{
    if (!keymap_initialized) {
        keymap_init();
    }
    return default_layer_index;
}

void keymap_layer_on(uint8_t layer)
{
    if (layer >= KEYMAP_LAYER_COUNT) {
        return;
    }

    if (!keymap_initialized) {
        keymap_init();
    }

    momentary_layer_entry_t *entry = keymap_find_momentary_entry(layer);
    if (entry) {
        if (entry->refcount < UINT8_MAX) {
            entry->refcount++;
        }
    } else if (momentary_layer_count < KEYMAP_LAYER_COUNT) {
        momentary_layers[momentary_layer_count].layer = layer;
        momentary_layers[momentary_layer_count].refcount = 1;
        momentary_layer_count++;
    }

    keymap_recompute_active_mask(true, false);
}

void keymap_layer_off(uint8_t layer)
{
    if (layer >= KEYMAP_LAYER_COUNT) {
        return;
    }

    if (!keymap_initialized) {
        keymap_init();
    }

    for (uint8_t i = 0; i < momentary_layer_count; ++i) {
        if (momentary_layers[i].layer != layer) {
            continue;
        }

        if (momentary_layers[i].refcount > 0) {
            momentary_layers[i].refcount--;
        }

        if (momentary_layers[i].refcount == 0) {
            for (uint8_t j = i; j + 1u < momentary_layer_count; ++j) {
                momentary_layers[j] = momentary_layers[j + 1u];
            }
            if (momentary_layer_count > 0) {
                momentary_layer_count--;
                momentary_layers[momentary_layer_count].layer = 0;
                momentary_layers[momentary_layer_count].refcount = 0;
            }
        }

        keymap_recompute_active_mask(true, false);
        return;
    }
}

void keymap_layer_move(uint8_t layer)
{
    if (layer >= KEYMAP_LAYER_COUNT) {
        return;
    }

    if (!keymap_initialized) {
        keymap_init();
    }

    if (persistent_layer_index == layer) {
        return;
    }

    persistent_layer_index = layer;
    keymap_recompute_active_mask(true, false);
}

void keymap_apply_layer_mask(uint8_t mask, uint8_t default_layer, bool propagate, bool update_default)
{
    if (!keymap_initialized) {
        keymap_init();
    }

    uint8_t allowed_mask = (KEYMAP_LAYER_COUNT >= 8)
        ? 0xFF
        : (uint8_t)((1u << KEYMAP_LAYER_COUNT) - 1u);

    uint8_t previous_default = default_layer_index;
    uint8_t sanitized_default = previous_default;
    if (update_default) {
        if (default_layer < KEYMAP_LAYER_COUNT) {
            sanitized_default = default_layer;
        } else {
            sanitized_default = keymap_first_active_layer(mask & allowed_mask);
            if (sanitized_default >= KEYMAP_LAYER_COUNT) {
                sanitized_default = previous_default;
            }
        }

        if (sanitized_default >= KEYMAP_LAYER_COUNT) {
            sanitized_default = 0;
        }
    }

    uint8_t sanitized_mask = mask & allowed_mask;
    if (sanitized_mask == 0) {
        uint8_t fallback = persistent_layer_index;
        if (update_default) {
            fallback = sanitized_default;
        }
        if (fallback >= KEYMAP_LAYER_COUNT) {
            fallback = previous_default;
        }
        if (fallback >= KEYMAP_LAYER_COUNT) {
            fallback = 0;
        }

        sanitized_mask = (uint8_t)(1u << fallback);
        if (sanitized_mask == 0) {
            sanitized_mask = 1u;
        }
    }

    if (update_default) {
        uint8_t ensure_bit = (uint8_t)(1u << sanitized_default);
        if (ensure_bit != 0) {
            sanitized_mask |= ensure_bit;
        }
    }

    bool default_changed = false;
    if (update_default && sanitized_default != previous_default) {
        default_layer_index = sanitized_default;
        default_changed = true;
        uint8_t persist_mask = (uint8_t)(1u << default_layer_index);
        if (persist_mask == 0) {
            persist_mask = 1u;
        }
        eeprom_set_layer_state(persist_mask, default_layer_index);
    }

    uint8_t candidate_persistent = persistent_layer_index;
    uint8_t current_bit = (candidate_persistent < KEYMAP_LAYER_COUNT)
        ? (uint8_t)(1u << candidate_persistent)
        : 0u;

    if (update_default) {
        candidate_persistent = sanitized_default;
    } else {
        if (current_bit == 0u || (sanitized_mask & current_bit) == 0u) {
            uint8_t default_bit = (uint8_t)(1u << default_layer_index);
            if (default_bit != 0u && (sanitized_mask & default_bit) != 0u) {
                candidate_persistent = default_layer_index;
            } else {
                candidate_persistent = keymap_first_active_layer(sanitized_mask);
            }
        }
    }

    if (candidate_persistent >= KEYMAP_LAYER_COUNT) {
        candidate_persistent = keymap_first_active_layer(sanitized_mask);
    }
    if (candidate_persistent >= KEYMAP_LAYER_COUNT) {
        candidate_persistent = default_layer_index;
    }
    if (candidate_persistent >= KEYMAP_LAYER_COUNT) {
        candidate_persistent = 0;
    }

    persistent_layer_index = candidate_persistent;

    keymap_clear_momentary_layers();
    for (uint8_t layer = 0; layer < KEYMAP_LAYER_COUNT; ++layer) {
        uint8_t bit = (uint8_t)(1u << layer);
        if ((sanitized_mask & bit) == 0) {
            continue;
        }
        if (layer == persistent_layer_index) {
            continue;
        }
        if (momentary_layer_count >= KEYMAP_LAYER_COUNT) {
            break;
        }
        momentary_layers[momentary_layer_count].layer = layer;
        momentary_layers[momentary_layer_count].refcount = 1;
        momentary_layer_count++;
    }

    keymap_recompute_active_mask(propagate, default_changed);
}

void keymap_persist_default_layer_state(void)
{
    if (!keymap_initialized) {
        keymap_init();
    }

    uint8_t persist_bit = (uint8_t)(1u << default_layer_index);
    if (persist_bit == 0) {
        persist_bit = 1u;
    }

    eeprom_set_layer_state(persist_bit, default_layer_index);
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
