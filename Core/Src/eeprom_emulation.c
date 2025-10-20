#include "eeprom_emulation.h"
#include "stm32g4xx_hal.h"
#include "input/keymap.h"
#include "usb_app.h"
#include <string.h>
#include <stddef.h>

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t checksum;
    uint16_t keymap[MATRIX_ROWS][MATRIX_COLS];
    uint16_t encoder_map[ENCODER_COUNT][2];
    uint8_t reserved[64];
} __attribute__((packed)) eeprom_data_v1_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t checksum;
    uint16_t keymap[KEYMAP_LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS];
    uint16_t encoder_map[KEYMAP_LAYER_COUNT][ENCODER_COUNT][2];
    uint8_t reserved[32];
} __attribute__((packed)) eeprom_data_v2_t;

#define EEPROM_V3_PAYLOAD_OFFSET offsetof(eeprom_data_t, keymap)
#define EEPROM_V3_PAYLOAD_SIZE   (sizeof(eeprom_data_t) - EEPROM_V3_PAYLOAD_OFFSET)
#define EEPROM_V2_PAYLOAD_OFFSET offsetof(eeprom_data_v2_t, keymap)
#define EEPROM_V2_PAYLOAD_SIZE   (sizeof(eeprom_data_v2_t) - EEPROM_V2_PAYLOAD_OFFSET)
#define EEPROM_V1_PAYLOAD_OFFSET offsetof(eeprom_data_v1_t, keymap)
#define EEPROM_V1_PAYLOAD_SIZE   (sizeof(eeprom_data_v1_t) - EEPROM_V1_PAYLOAD_OFFSET)

// Private variables
static eeprom_data_t eeprom_data;
static bool eeprom_initialized = false;
static bool config_modified = false;

// Private function declarations
static uint32_t calculate_crc32(const uint8_t *data, uint32_t length);
static bool flash_erase_page(uint32_t page_address);
static bool flash_write_data(uint32_t address, const uint8_t *data, uint32_t length);
static void load_default_config(void);

// Initialize EEPROM emulation
bool eeprom_init(void)
{
    if (eeprom_initialized) {
        return true;
    }
    
    // Try to load existing configuration
    if (eeprom_load_config()) {
        eeprom_initialized = true;
        config_modified = false;
        usb_app_cdc_printf("EEPROM: Configuration loaded from flash\r\n");
        return true;
    }
    
    // If no valid config found, load defaults and save them immediately
    // This is normal for first boot or after firmware updates
    usb_app_cdc_printf("EEPROM: First boot detected, initializing with defaults\r\n");
    load_default_config();
    
    // Save the defaults immediately so they become the "valid" configuration
    if (eeprom_save_config()) {
        eeprom_initialized = true;
        config_modified = false;
        
        usb_app_cdc_printf("EEPROM: Default configuration saved and active\r\n");
        return true;
    }
    
    // If we can't save to flash, still initialize with defaults in RAM
    eeprom_initialized = true;
    config_modified = false; // Don't mark as modified since we tried to save
    usb_app_cdc_printf("EEPROM: Flash write failed, using defaults in RAM only\r\n");
    return true; // Always succeed - we have valid data even if not persisted
}

// Save current configuration to flash
bool eeprom_save_config(void)
{
    if (!eeprom_initialized) {
        usb_app_cdc_printf("EEPROM: Cannot save - not initialized\r\n");
        return false;
    }
    
    if (!config_modified) {
        usb_app_cdc_printf("EEPROM: No changes to save\r\n");
        return true; // No changes to save
    }
    
    usb_app_cdc_printf("EEPROM: Saving configuration to flash...\r\n");
    
    // Update metadata
    eeprom_data.magic = EEPROM_MAGIC;
    eeprom_data.version = EEPROM_VERSION;
    
    // Calculate checksum (excluding the checksum field itself)
    eeprom_data.checksum = calculate_crc32(((const uint8_t*)&eeprom_data) + EEPROM_V3_PAYLOAD_OFFSET,
                                           EEPROM_V3_PAYLOAD_SIZE);
    
    // Erase flash page
    if (!flash_erase_page(EEPROM_START_ADDRESS)) {
        usb_app_cdc_printf("EEPROM: Failed to erase flash page\r\n");
        return false;
    }
    
    // Write data to flash
    if (!flash_write_data(EEPROM_START_ADDRESS, (uint8_t*)&eeprom_data, sizeof(eeprom_data_t))) {
        usb_app_cdc_printf("EEPROM: Failed to write data to flash\r\n");
        return false;
    }
    
    config_modified = false;
    usb_app_cdc_printf("EEPROM: Configuration saved successfully to flash\r\n");
    return true;
}

// Force save current configuration to flash (even if no changes)
bool eeprom_force_save_config(void)
{
    if (!eeprom_initialized) {
        usb_app_cdc_printf("EEPROM: Cannot save - not initialized\r\n");
        return false;
    }
    
    usb_app_cdc_printf("EEPROM: Force saving configuration to flash...\r\n");
    
    // Update metadata
    eeprom_data.magic = EEPROM_MAGIC;
    eeprom_data.version = EEPROM_VERSION;
    
    // Calculate checksum (excluding the checksum field itself)
    eeprom_data.checksum = calculate_crc32(((const uint8_t*)&eeprom_data) + EEPROM_V3_PAYLOAD_OFFSET,
                                           EEPROM_V3_PAYLOAD_SIZE);
    
    usb_app_cdc_printf("EEPROM: Calculated checksum: 0x%08lX\r\n", eeprom_data.checksum);
    
    // Erase flash page
    usb_app_cdc_printf("EEPROM: Erasing flash page at 0x%08lX...\r\n", EEPROM_START_ADDRESS);
    if (!flash_erase_page(EEPROM_START_ADDRESS)) {
        usb_app_cdc_printf("EEPROM: Failed to erase flash page\r\n");
        return false;
    }
    usb_app_cdc_printf("EEPROM: Flash page erased successfully\r\n");
    
    // Write data to flash
    usb_app_cdc_printf("EEPROM: Writing %lu bytes to flash...\r\n", sizeof(eeprom_data_t));
    if (!flash_write_data(EEPROM_START_ADDRESS, (uint8_t*)&eeprom_data, sizeof(eeprom_data_t))) {
        usb_app_cdc_printf("EEPROM: Failed to write data to flash\r\n");
        return false;
    }
    
    config_modified = false;
    usb_app_cdc_printf("EEPROM: Configuration force-saved successfully to flash\r\n");
    return true;
}

// Load configuration from flash
bool eeprom_load_config(void)
{
    eeprom_data_t candidate = {0};
    memcpy(&candidate, (void*)EEPROM_START_ADDRESS, sizeof(eeprom_data_t));

    if (candidate.magic != EEPROM_MAGIC) {
        return false;
    }

    if (candidate.version == EEPROM_VERSION) {
        uint32_t calculated_checksum = calculate_crc32(((const uint8_t*)&candidate) + EEPROM_V3_PAYLOAD_OFFSET,
                                                       EEPROM_V3_PAYLOAD_SIZE);
        if (candidate.checksum != calculated_checksum) {
            usb_app_cdc_printf("EEPROM: Checksum mismatch for v3 data (will use defaults)\r\n");
            return false;
        }

        eeprom_data = candidate;
        return true;
    }

    if (candidate.version == 2) {
        eeprom_data_v2_t legacy2 = {0};
        memcpy(&legacy2, (void*)EEPROM_START_ADDRESS, sizeof(eeprom_data_v2_t));

        uint32_t calculated_checksum = calculate_crc32(((const uint8_t*)&legacy2) + EEPROM_V2_PAYLOAD_OFFSET,
                                                       EEPROM_V2_PAYLOAD_SIZE);
        if (legacy2.checksum != calculated_checksum) {
            usb_app_cdc_printf("EEPROM: v2 checksum mismatch (will use defaults)\r\n");
            return false;
        }

        usb_app_cdc_printf("EEPROM: Migrating v2 data to multilayer layout with layer state\r\n");

        memset(&eeprom_data, 0, sizeof(eeprom_data));
        eeprom_data.magic = EEPROM_MAGIC;
        eeprom_data.version = EEPROM_VERSION;

        memcpy(eeprom_data.keymap, legacy2.keymap, sizeof(legacy2.keymap));
        memcpy(eeprom_data.encoder_map, legacy2.encoder_map, sizeof(legacy2.encoder_map));
        eeprom_data.startup_layer_mask = 0x01;
        eeprom_data.default_layer = 0;

        config_modified = true;
        return true;
    }

    if (candidate.version == 1) {
        eeprom_data_v1_t legacy = {0};
        memcpy(&legacy, (void*)EEPROM_START_ADDRESS, sizeof(eeprom_data_v1_t));

        uint32_t calculated_checksum = calculate_crc32(((const uint8_t*)&legacy) + EEPROM_V1_PAYLOAD_OFFSET,
                                                       EEPROM_V1_PAYLOAD_SIZE);
        if (legacy.checksum != calculated_checksum) {
            usb_app_cdc_printf("EEPROM: Legacy checksum mismatch (will use defaults)\r\n");
            return false;
        }

        usb_app_cdc_printf("EEPROM: Migrating legacy v1 data to multilayer layout\r\n");

        memset(&eeprom_data, 0, sizeof(eeprom_data));
        eeprom_data.magic = EEPROM_MAGIC;
        eeprom_data.version = EEPROM_VERSION;

        for (uint8_t layer = 0; layer < KEYMAP_LAYER_COUNT; layer++) {
            for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
                for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                    if (layer == 0) {
                        eeprom_data.keymap[layer][row][col] = legacy.keymap[row][col];
                    } else {
                        eeprom_data.keymap[layer][row][col] = KC_TRANSPARENT;
                    }
                }
            }
        }

        for (uint8_t layer = 0; layer < KEYMAP_LAYER_COUNT; layer++) {
            for (uint8_t idx = 0; idx < ENCODER_COUNT; idx++) {
                if (layer == 0) {
                    eeprom_data.encoder_map[layer][idx][0] = legacy.encoder_map[idx][0];
                    eeprom_data.encoder_map[layer][idx][1] = legacy.encoder_map[idx][1];
                } else {
                    eeprom_data.encoder_map[layer][idx][0] = KC_TRANSPARENT;
                    eeprom_data.encoder_map[layer][idx][1] = KC_TRANSPARENT;
                }
            }
        }

        eeprom_data.startup_layer_mask = 0x01;
        eeprom_data.default_layer = 0;
        config_modified = true; // ensure we rewrite in new format
        return true;
    }

    usb_app_cdc_printf("EEPROM: Unsupported data version %lu\r\n", candidate.version);
    return false;
}

// Reset configuration to defaults
bool eeprom_reset_config(void)
{
    load_default_config();
    config_modified = true;
    return eeprom_save_config();
}

// Check if EEPROM contains valid data
bool eeprom_is_valid(void)
{
    return eeprom_initialized && !config_modified;
}

// Set keycode for specific position
bool eeprom_set_keycode(uint8_t layer, uint8_t row, uint8_t col, uint16_t keycode)
{
    if (layer >= KEYMAP_LAYER_COUNT || row >= MATRIX_ROWS || col >= MATRIX_COLS) {
        return false;
    }
    
    if (!eeprom_initialized) {
        if (!eeprom_init()) {
            return false;
        }
    }
    
    if (eeprom_data.keymap[layer][row][col] != keycode) {
        eeprom_data.keymap[layer][row][col] = keycode;
        config_modified = true;
        usb_app_cdc_printf("EEPROM: Keymap[L%d][%d][%d] = 0x%04X\r\n", layer, row, col, keycode);
    }
    
    return true;
}

// Get keycode for specific position
uint16_t eeprom_get_keycode(uint8_t layer, uint8_t row, uint8_t col)
{
    if (layer >= KEYMAP_LAYER_COUNT || row >= MATRIX_ROWS || col >= MATRIX_COLS) {
        return 0;
    }
    
    if (!eeprom_initialized) {
        if (!eeprom_init()) {
            // EEPROM init failed, return 0 to indicate fallback needed
            return 0;
        }
    }
    
    return eeprom_data.keymap[layer][row][col];
}

// Set encoder mapping
bool eeprom_set_encoder_map(uint8_t layer, uint8_t encoder_id, uint16_t ccw_keycode, uint16_t cw_keycode)
{
    if (layer >= KEYMAP_LAYER_COUNT || encoder_id >= ENCODER_COUNT) {
        return false;
    }

    if (!eeprom_initialized) {
        if (!eeprom_init()) {
            return false;
        }
    }

    bool changed = false;
    if (eeprom_data.encoder_map[layer][encoder_id][0] != ccw_keycode) {
        eeprom_data.encoder_map[layer][encoder_id][0] = ccw_keycode;
        changed = true;
    }

    if (eeprom_data.encoder_map[layer][encoder_id][1] != cw_keycode) {
        eeprom_data.encoder_map[layer][encoder_id][1] = cw_keycode;
        changed = true;
    }

    if (changed) {
        config_modified = true;
        usb_app_cdc_printf("EEPROM: Encoder[L%d][%d] = CCW:0x%04X CW:0x%04X\r\n",
                           layer, encoder_id, ccw_keycode, cw_keycode);
    }

    return true;
}

bool eeprom_get_encoder_map(uint8_t layer, uint8_t encoder_id, uint16_t *ccw_keycode, uint16_t *cw_keycode)
{
    if (layer >= KEYMAP_LAYER_COUNT || encoder_id >= ENCODER_COUNT || !ccw_keycode || !cw_keycode) {
        return false;
    }

    if (!eeprom_initialized) {
        if (!eeprom_init()) {
            return false;
        }
    }

    *ccw_keycode = eeprom_data.encoder_map[layer][encoder_id][0];
    *cw_keycode = eeprom_data.encoder_map[layer][encoder_id][1];
    return true;
}

bool eeprom_set_slider_config(uint8_t layer, uint8_t slider_id, const slider_config_t *config)
{
    if (layer >= KEYMAP_LAYER_COUNT || slider_id >= SLIDER_COUNT || !config) {
        return false;
    }

    if (!eeprom_initialized) {
        if (!eeprom_init()) {
            return false;
        }
    }

    // Check if the configuration actually changed
    bool changed = false;
    slider_config_t *current_config = &eeprom_data.slider_map[layer][slider_id];
    
    if (current_config->midi_cc != config->midi_cc ||
        current_config->midi_channel != config->midi_channel ||
        current_config->min_midi_value != config->min_midi_value ||
        current_config->max_midi_value != config->max_midi_value) {
        changed = true;
    }

    if (changed) {
        *current_config = *config;  // Copy the entire config
        current_config->layer = layer;  // Ensure layer is correct
        current_config->slider_id = slider_id;  // Ensure slider_id is correct
        config_modified = true;
        usb_app_cdc_printf("EEPROM: Slider[L%d][%d] = CC%d Ch%d Range%d-%d\r\n",
                           layer, slider_id, config->midi_cc, config->midi_channel,
                           config->min_midi_value, config->max_midi_value);
    }

    return true;
}

bool eeprom_get_slider_config(uint8_t layer, uint8_t slider_id, slider_config_t *config)
{
    if (layer >= KEYMAP_LAYER_COUNT || slider_id >= SLIDER_COUNT || !config) {
        return false;
    }

    if (!eeprom_initialized) {
        if (!eeprom_init()) {
            return false;
        }
    }

    *config = eeprom_data.slider_map[layer][slider_id];
    return true;
}

bool eeprom_set_magnetic_switch_calibration(uint8_t switch_id, uint16_t unpressed_value, uint16_t pressed_value, uint8_t sensitivity)
{
    if (switch_id >= MAX_MAGNETIC_SWITCHES_EEPROM) {
        return false;
    }

    if (!eeprom_initialized) {
        if (!eeprom_init()) {
            return false;
        }
    }

    // Check if the calibration actually changed (avoid taking address of packed member)
    bool changed = (eeprom_data.magnetic_switches[switch_id].unpressed_value != unpressed_value ||
                   eeprom_data.magnetic_switches[switch_id].pressed_value != pressed_value ||
                   eeprom_data.magnetic_switches[switch_id].sensitivity != sensitivity ||
                   !eeprom_data.magnetic_switches[switch_id].is_calibrated);

    if (changed) {
        eeprom_data.magnetic_switches[switch_id].unpressed_value = unpressed_value;
        eeprom_data.magnetic_switches[switch_id].pressed_value = pressed_value;
        eeprom_data.magnetic_switches[switch_id].sensitivity = sensitivity;
        eeprom_data.magnetic_switches[switch_id].is_calibrated = true;
        config_modified = true;
        usb_app_cdc_printf("EEPROM: MagSwitch[%d] = unpressed:%d pressed:%d sensitivity:%d%%\r\n",
                           switch_id, unpressed_value, pressed_value, sensitivity);
    }

    return true;
}

bool eeprom_get_magnetic_switch_calibration(uint8_t switch_id, uint16_t *unpressed_value, uint16_t *pressed_value, uint8_t *sensitivity, bool *is_calibrated)
{
    if (switch_id >= MAX_MAGNETIC_SWITCHES_EEPROM || 
        !unpressed_value || !pressed_value || !sensitivity || !is_calibrated) {
        return false;
    }

    if (!eeprom_initialized) {
        if (!eeprom_init()) {
            return false;
        }
    }

    // Avoid taking address of packed member
    *unpressed_value = eeprom_data.magnetic_switches[switch_id].unpressed_value;
    *pressed_value = eeprom_data.magnetic_switches[switch_id].pressed_value;
    *sensitivity = eeprom_data.magnetic_switches[switch_id].sensitivity;
    *is_calibrated = eeprom_data.magnetic_switches[switch_id].is_calibrated;
    
    return true;
}

bool eeprom_set_layer_state(uint8_t active_mask, uint8_t default_layer)
{
    if (!eeprom_initialized) {
        if (!eeprom_init()) {
            return false;
        }
    }

    uint8_t sanitized_default = default_layer;
    if (sanitized_default >= KEYMAP_LAYER_COUNT) {
        sanitized_default = 0;
    }

    uint8_t allowed_mask = (KEYMAP_LAYER_COUNT >= 8)
        ? 0xFF
        : (uint8_t)((1u << KEYMAP_LAYER_COUNT) - 1u);

    uint8_t sanitized_mask = active_mask & allowed_mask;
    if (sanitized_mask == 0) {
        uint16_t startup_bit = (uint16_t)1u << sanitized_default;
        sanitized_mask = (uint8_t)(startup_bit & 0xFFu);
        if (sanitized_mask == 0) {
            sanitized_mask = 1u;
        }
    }

    if (eeprom_data.startup_layer_mask != sanitized_mask || eeprom_data.default_layer != sanitized_default) {
        eeprom_data.startup_layer_mask = sanitized_mask;
        eeprom_data.default_layer = sanitized_default;
        config_modified = true;
        usb_app_cdc_printf("EEPROM: Layer state stored mask=0x%02X default=%u\r\n", sanitized_mask, sanitized_default);
    }

    return true;
}

bool eeprom_get_layer_state(uint8_t *active_mask, uint8_t *default_layer)
{
    if (!active_mask || !default_layer) {
        return false;
    }

    if (!eeprom_initialized) {
        if (!eeprom_init()) {
            return false;
        }
    }

    *active_mask = eeprom_data.startup_layer_mask;
    *default_layer = eeprom_data.default_layer;
    return true;
}

// Private functions

// Simple CRC32 implementation
static uint32_t calculate_crc32(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFF;
    
    for (uint32_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return ~crc;
}

// Erase flash page
static bool flash_erase_page(uint32_t page_address)
{
    HAL_StatusTypeDef status;
    FLASH_EraseInitTypeDef erase_init;
    uint32_t page_error;
    
    // STM32G4 uses 2KB pages; compute bank and page index relative to bank base
    uint32_t bank_base = FLASH_BASE;
    uint32_t bank = FLASH_BANK_1;

    // Determine bank based on address and FLASH_BANK_SIZE
    if (page_address >= (FLASH_BASE + FLASH_BANK_SIZE)) {
        bank = FLASH_BANK_2;
        bank_base = FLASH_BASE + FLASH_BANK_SIZE;
    } else {
        bank = FLASH_BANK_1;
        bank_base = FLASH_BASE;
    }

    uint32_t page_number = (page_address - bank_base) / FLASH_PAGE_SIZE;

    // Calculate number of pages to erase covering the EEPROM region (safe: erase entire reserved range)
    uint32_t nb_pages = (EEPROM_END_ADDRESS - EEPROM_START_ADDRESS) / FLASH_PAGE_SIZE;

    usb_app_cdc_printf("EEPROM: Erasing %lu page(s) starting at page %lu (address 0x%08lX) on bank %lu\r\n",
                        nb_pages, page_number, page_address, (uint32_t)bank == FLASH_BANK_1 ? 1U : 2U);

    // Disable interrupts during flash erase to avoid code execution from flash during operation
    __disable_irq();
    HAL_FLASH_Unlock();

    erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
    erase_init.Banks = bank;
    erase_init.Page = page_number;
    erase_init.NbPages = nb_pages;

    status = HAL_FLASHEx_Erase(&erase_init, &page_error);

    if (status != HAL_OK) {
        usb_app_cdc_printf("EEPROM: Erase failed, status=%d, page_error=%lu\r\n", status, page_error);
    } else {
        usb_app_cdc_printf("EEPROM: Pages erased successfully\r\n");
    }

    HAL_FLASH_Lock();
    __enable_irq();

    return (status == HAL_OK);
}

// Write data to flash
static bool flash_write_data(uint32_t address, const uint8_t *data, uint32_t length)
{
    HAL_StatusTypeDef status = HAL_OK;
    
    usb_app_cdc_printf("EEPROM: Unlocking flash...\r\n");
    HAL_FLASH_Unlock();
    
    // STM32G4 requires 64-bit (8-byte) aligned writes
    for (uint32_t i = 0; i < length; i += 8) {
        uint64_t data_to_write = 0;
        
        // Copy up to 8 bytes, padding with 0xFF if necessary
        for (int j = 0; j < 8; j++) {
            if (i + j < length) {
                data_to_write |= ((uint64_t)data[i + j]) << (j * 8);
            } else {
                data_to_write |= ((uint64_t)0xFF) << (j * 8);
            }
        }
        
        usb_app_cdc_printf("EEPROM: Writing 8 bytes at offset %lu\r\n", i);
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, address + i, data_to_write);
        if (status != HAL_OK) {
            usb_app_cdc_printf("EEPROM: Flash write failed at offset %lu, status=%d\r\n", i, status);
            break;
        }
    }
    
    HAL_FLASH_Lock();
    usb_app_cdc_printf("EEPROM: Flash locked, final status=%d\r\n", status);
    
    if (status != HAL_OK) {
        return false;
    }

    // Read-back verification
    usb_app_cdc_printf("EEPROM: Verifying written data...\r\n");
    const uint8_t *written = (const uint8_t*)address;
    for (uint32_t i = 0; i < length; i++) {
        if (written[i] != data[i]) {
            usb_app_cdc_printf("EEPROM: Verification mismatch at offset %lu (flash=0x%02X expected=0x%02X)\r\n",
                                i, written[i], data[i]);
            return false;
        }
    }

    usb_app_cdc_printf("EEPROM: Verification passed\r\n");
    return true;
}

// Load default configuration from const arrays
static void load_default_config(void)
{
    // Clear the structure
    memset(&eeprom_data, 0, sizeof(eeprom_data_t));
    
    // Copy default keymap for all layers
    for (uint8_t layer = 0; layer < KEYMAP_LAYER_COUNT; layer++) {
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                eeprom_data.keymap[layer][row][col] = keycodes[layer][row][col];
            }
        }
    }

    // Copy default encoder map for all layers
    for (uint8_t layer = 0; layer < KEYMAP_LAYER_COUNT; layer++) {
        for (uint8_t idx = 0; idx < ENCODER_COUNT; idx++) {
            eeprom_data.encoder_map[layer][idx][0] = encoder_map[layer][idx][0];
            eeprom_data.encoder_map[layer][idx][1] = encoder_map[layer][idx][1];
        }
    }

    // Copy default slider configuration map for all layers
    for (uint8_t layer = 0; layer < KEYMAP_LAYER_COUNT; layer++) {
        for (uint8_t idx = 0; idx < SLIDER_COUNT; idx++) {
            eeprom_data.slider_map[layer][idx] = slider_config_map[layer][idx];
            // Ensure layer and slider_id are correct
            eeprom_data.slider_map[layer][idx].layer = layer;
            eeprom_data.slider_map[layer][idx].slider_id = idx;
        }
    }

    eeprom_data.startup_layer_mask = 0x01;
    eeprom_data.default_layer = 0;
    
    config_modified = true;
}
