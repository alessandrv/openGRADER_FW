#ifndef EEPROM_EMULATION_H
#define EEPROM_EMULATION_H

#include <stdint.h>
#include <stdbool.h>
#include "input/keymap.h"

#ifdef __cplusplus
extern "C" {
#endif

// EEPROM emulation configuration
#define EEPROM_PAGE_SIZE        2048    // STM32G4 flash page size
#define EEPROM_START_ADDRESS    0x0807F000  // Last 4KB of flash (2 pages)
#define EEPROM_END_ADDRESS      0x08080000

// Data structure versions for migration
#define EEPROM_VERSION          1
#define EEPROM_MAGIC            0x4F47454D  // "OGEM" - OpenGrader EEPROM Magic

// EEPROM data structure
typedef struct {
    uint32_t magic;                                     // Magic number for validation
    uint32_t version;                                   // Data structure version
    uint32_t checksum;                                  // CRC32 checksum
    uint16_t keymap[MATRIX_ROWS][MATRIX_COLS];         // Keymap data
    uint16_t encoder_map[ENCODER_COUNT][2];            // Encoder map data
    uint8_t reserved[64];                              // Reserved for future use
} __attribute__((packed)) eeprom_data_t;

// Public API
bool eeprom_init(void);
bool eeprom_save_config(void);
bool eeprom_load_config(void);
bool eeprom_reset_config(void);
bool eeprom_is_valid(void);

// Keymap and encoder map access
bool eeprom_set_keycode(uint8_t row, uint8_t col, uint16_t keycode);
uint16_t eeprom_get_keycode(uint8_t row, uint8_t col);
bool eeprom_set_encoder_map(uint8_t encoder_id, uint16_t ccw_keycode, uint16_t cw_keycode);
bool eeprom_get_encoder_map(uint8_t encoder_id, uint16_t *ccw_keycode, uint16_t *cw_keycode);

#ifdef __cplusplus
}
#endif

#endif /* EEPROM_EMULATION_H */
