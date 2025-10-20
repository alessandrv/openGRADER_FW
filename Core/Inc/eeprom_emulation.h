#ifndef EEPROM_EMULATION_H
#define EEPROM_EMULATION_H

#include <stdint.h>
#include <stdbool.h>
#include "input/keymap.h"
#include "config_protocol.h"  // For slider_config_t

// Forward declare magnetic switch config to avoid circular dependency
typedef struct {
    uint16_t unpressed_value;
    uint16_t pressed_value;
    uint8_t sensitivity;
    bool is_calibrated;
} magnetic_switch_eeprom_t;

#ifdef __cplusplus
extern "C" {
#endif

// EEPROM emulation configuration
#define EEPROM_PAGE_SIZE        2048    // STM32G4 flash page size
#define EEPROM_START_ADDRESS    0x0807F000  // Last 4KB of flash (2 pages)
#define EEPROM_END_ADDRESS      0x08080000

// Data structure versions for migration
#define EEPROM_VERSION          4  // Incremented for magnetic switch support
#define EEPROM_MAGIC            0x4F47454D  // "OGEM" - OpenGrader EEPROM Magic
#define MAX_MAGNETIC_SWITCHES_EEPROM 8  // Maximum magnetic switches to store

// EEPROM data structure
typedef struct {
    uint32_t magic;                                     // Magic number for validation
    uint32_t version;                                   // Data structure version
    uint32_t checksum;                                  // CRC32 checksum
    uint16_t keymap[KEYMAP_LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS];
    uint16_t encoder_map[KEYMAP_LAYER_COUNT][ENCODER_COUNT][2];
    slider_config_t slider_map[KEYMAP_LAYER_COUNT][SLIDER_COUNT];  // Slider configurations per layer
    magnetic_switch_eeprom_t magnetic_switches[MAX_MAGNETIC_SWITCHES_EEPROM];  // Magnetic switch calibration data
    uint8_t startup_layer_mask;                         // Layer mask restored on boot
    uint8_t default_layer;                              // Default layer index
    uint8_t reserved[18];                               // Reserved for future use (reduced for magnetic switches)
} __attribute__((packed)) eeprom_data_t;

// Public API
bool eeprom_init(void);
bool eeprom_save_config(void);
bool eeprom_force_save_config(void);  // Force save even if no changes
bool eeprom_load_config(void);
bool eeprom_reset_config(void);
bool eeprom_is_valid(void);

// Layer state persistence
bool eeprom_set_layer_state(uint8_t active_mask, uint8_t default_layer);
bool eeprom_get_layer_state(uint8_t *active_mask, uint8_t *default_layer);

// Keymap and encoder map access
bool eeprom_set_keycode(uint8_t layer, uint8_t row, uint8_t col, uint16_t keycode);
uint16_t eeprom_get_keycode(uint8_t layer, uint8_t row, uint8_t col);
bool eeprom_set_encoder_map(uint8_t layer, uint8_t encoder_id, uint16_t ccw_keycode, uint16_t cw_keycode);
bool eeprom_get_encoder_map(uint8_t layer, uint8_t encoder_id, uint16_t *ccw_keycode, uint16_t *cw_keycode);

// Slider configuration access
bool eeprom_set_slider_config(uint8_t layer, uint8_t slider_id, const slider_config_t *config);
bool eeprom_get_slider_config(uint8_t layer, uint8_t slider_id, slider_config_t *config);

// Magnetic switch calibration access
bool eeprom_set_magnetic_switch_calibration(uint8_t switch_id, uint16_t unpressed_value, uint16_t pressed_value, uint8_t sensitivity);
bool eeprom_get_magnetic_switch_calibration(uint8_t switch_id, uint16_t *unpressed_value, uint16_t *pressed_value, uint8_t *sensitivity, bool *is_calibrated);

#ifdef __cplusplus
}
#endif

#endif /* EEPROM_EMULATION_H */
