#include "eeprom_emulation.h"
#include "stm32g4xx_hal.h"
#include "input/keymap.h"
#include "usb_app.h"
#include <string.h>

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
    uint32_t data_size = sizeof(eeprom_data_t) - sizeof(uint32_t);
    
    eeprom_data.checksum = calculate_crc32((uint8_t*)&eeprom_data + sizeof(uint32_t) * 3, 
                                          data_size - sizeof(uint32_t) * 2);
    
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
    uint32_t data_size = sizeof(eeprom_data_t) - sizeof(uint32_t);
    
    eeprom_data.checksum = calculate_crc32((uint8_t*)&eeprom_data + sizeof(uint32_t) * 3, 
                                          data_size - sizeof(uint32_t) * 2);
    
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
    // Read data from flash
    memcpy(&eeprom_data, (void*)EEPROM_START_ADDRESS, sizeof(eeprom_data_t));
    
    // Validate magic number
    if (eeprom_data.magic != EEPROM_MAGIC) {
        // This is normal for first boot - don't spam with error messages
        return false;
    }
    
    // Check version compatibility
    if (eeprom_data.version != EEPROM_VERSION) {
        usb_app_cdc_printf("EEPROM: Version mismatch: %lu != %d (will use defaults)\r\n", eeprom_data.version, EEPROM_VERSION);
        return false;
    }
    
    // Verify checksum
    uint32_t data_size = sizeof(eeprom_data_t) - sizeof(uint32_t);
    uint32_t calculated_checksum = calculate_crc32((uint8_t*)&eeprom_data + sizeof(uint32_t) * 3, 
                                                  data_size - sizeof(uint32_t) * 2);
    
    if (eeprom_data.checksum != calculated_checksum) {
        usb_app_cdc_printf("EEPROM: Checksum mismatch (will use defaults)\r\n");
        return false;
    }
    
    return true;
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
bool eeprom_set_keycode(uint8_t row, uint8_t col, uint16_t keycode)
{
    if (row >= MATRIX_ROWS || col >= MATRIX_COLS) {
        return false;
    }
    
    if (!eeprom_initialized) {
        if (!eeprom_init()) {
            return false;
        }
    }
    
    if (eeprom_data.keymap[row][col] != keycode) {
        eeprom_data.keymap[row][col] = keycode;
        config_modified = true;
        usb_app_cdc_printf("EEPROM: Keymap[%d][%d] = 0x%04X\r\n", row, col, keycode);
    }
    
    return true;
}

// Get keycode for specific position
uint16_t eeprom_get_keycode(uint8_t row, uint8_t col)
{
    if (row >= MATRIX_ROWS || col >= MATRIX_COLS) {
        return 0;
    }
    
    if (!eeprom_initialized) {
        if (!eeprom_init()) {
            // EEPROM init failed, return 0 to indicate fallback needed
            return 0;
        }
    }
    
    return eeprom_data.keymap[row][col];
}

// Set encoder mapping
bool eeprom_set_encoder_map(uint8_t encoder_id, uint16_t ccw_keycode, uint16_t cw_keycode)
{
    if (encoder_id >= ENCODER_COUNT) {
        return false;
    }
    
    if (!eeprom_initialized) {
        if (!eeprom_init()) {
            return false;
        }
    }
    
    bool changed = false;
    if (eeprom_data.encoder_map[encoder_id][0] != ccw_keycode) {
        eeprom_data.encoder_map[encoder_id][0] = ccw_keycode;
        changed = true;
    }
    
    if (eeprom_data.encoder_map[encoder_id][1] != cw_keycode) {
        eeprom_data.encoder_map[encoder_id][1] = cw_keycode;
        changed = true;
    }
    
    if (changed) {
        config_modified = true;
        usb_app_cdc_printf("EEPROM: Encoder[%d] = CCW:0x%04X CW:0x%04X\r\n", 
                     encoder_id, ccw_keycode, cw_keycode);
    }
    
    return true;
}

// Get encoder mapping
bool eeprom_get_encoder_map(uint8_t encoder_id, uint16_t *ccw_keycode, uint16_t *cw_keycode)
{
    if (encoder_id >= ENCODER_COUNT || !ccw_keycode || !cw_keycode) {
        return false;
    }
    
    if (!eeprom_initialized) {
        if (!eeprom_init()) {
            // EEPROM init failed, return false to indicate fallback needed
            return false;
        }
    }
    
    *ccw_keycode = eeprom_data.encoder_map[encoder_id][0];
    *cw_keycode = eeprom_data.encoder_map[encoder_id][1];
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
    
    // Copy default keymap
    for (int row = 0; row < MATRIX_ROWS; row++) {
        for (int col = 0; col < MATRIX_COLS; col++) {
            eeprom_data.keymap[row][col] = keycodes[row][col];
        }
    }
    
    // Copy default encoder map
    for (int i = 0; i < ENCODER_COUNT; i++) {
        eeprom_data.encoder_map[i][0] = encoder_map[i][0];
        eeprom_data.encoder_map[i][1] = encoder_map[i][1];
    }
    
    config_modified = true;
}
