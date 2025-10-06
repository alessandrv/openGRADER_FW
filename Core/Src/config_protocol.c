#include "config_protocol.h"
#include "usb_app.h"
#include "input/keymap.h"
#include "i2c_manager.h"
#include "i2c.h"  // Added to include hi2c2 declaration
#include "pin_config.h"
#include "eeprom_emulation.h"
#include "tusb.h"
#include "class/hid/hid_device.h"
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

// Private variables
static config_packet_t rx_packet;
static config_packet_t tx_packet;
static bool packet_received = false;
static bool packet_pending = false;

// Forward declarations
static void handle_get_info(config_packet_t *response);
static void handle_get_keymap(const config_packet_t *request, config_packet_t *response);
static void handle_set_keymap(const config_packet_t *request, config_packet_t *response);
static void handle_get_encoder_map(const config_packet_t *request, config_packet_t *response);
static void handle_set_encoder_map(const config_packet_t *request, config_packet_t *response);
static void handle_get_i2c_devices(config_packet_t *response);
static void handle_get_device_status(config_packet_t *response);
static void handle_midi_send_raw(const config_packet_t *request, config_packet_t *response);
static void handle_midi_note_on(const config_packet_t *request, config_packet_t *response);
static void handle_midi_note_off(const config_packet_t *request, config_packet_t *response);
static void handle_midi_cc(const config_packet_t *request, config_packet_t *response);
static bool request_keymap_from_slave(uint8_t slave_addr, uint8_t row, uint8_t col, uint16_t *keycode);
static bool send_keymap_to_slave(uint8_t slave_addr, uint8_t row, uint8_t col, uint16_t keycode);
static bool request_encoder_from_slave(uint8_t slave_addr, uint8_t encoder_id, uint16_t *ccw_keycode, uint16_t *cw_keycode);
static bool send_encoder_to_slave(uint8_t slave_addr, uint8_t encoder_id, uint16_t ccw_keycode, uint16_t cw_keycode);
static void handle_get_slave_keymap(const config_packet_t *request, config_packet_t *response)
{
    if (request->payload_length < 3) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }
    
    uint8_t slave_addr = request->payload[0];
    uint8_t row = request->payload[1];
    uint8_t col = request->payload[2];
    
    usb_app_cdc_printf("GET_SLAVE[0x%02X,%d,%d]\r\n", slave_addr, row, col);
    
    // Check if we're in master mode
    if (i2c_manager_get_mode() != 1) {
        response->status = STATUS_ERROR;
        return;
    }
    
    // Create keymap entry structure in response
    keymap_entry_t *entry = (keymap_entry_t*)response->payload;
    entry->row = row;
    entry->col = col;
    entry->keycode = 0; // Default value if request fails
    
    // Request keymap from slave
    uint16_t keycode = 0;
    if (request_keymap_from_slave(slave_addr, row, col, &keycode)) {
        entry->keycode = keycode;
        response->status = STATUS_OK;
    } else {
        response->status = STATUS_ERROR;
    }
    
    response->payload_length = sizeof(keymap_entry_t);
}

static void handle_set_slave_keymap(const config_packet_t *request, config_packet_t *response)
{
    if (request->payload_length < sizeof(keymap_entry_t) + 1) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }
    
    uint8_t slave_addr = request->payload[0];
    const keymap_entry_t *entry = (const keymap_entry_t*)(request->payload + 1);
    
    // Check if we're in master mode
    if (i2c_manager_get_mode() != 1) {
        response->status = STATUS_ERROR;
        usb_app_cdc_printf("Config: Cannot set slave keymap - not in master mode\r\n");
        return;
    }
    
    // Send keymap to slave
    if (send_keymap_to_slave(slave_addr, entry->row, entry->col, entry->keycode)) {
        response->status = STATUS_OK;
    } else {
        response->status = STATUS_ERROR;
    }
    
    usb_app_cdc_printf("Config: Set slave keymap [%02X,%d,%d] = 0x%04X\r\n", 
                 slave_addr, entry->row, entry->col, entry->keycode);
}

static void handle_get_slave_encoder(const config_packet_t *request, config_packet_t *response)
{
    if (request->payload_length < 2) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }

    uint8_t slave_addr = request->payload[0];
    uint8_t encoder_id = request->payload[1];

    if (i2c_manager_get_mode() != 1) {
        response->status = STATUS_ERROR;
        return;
    }

    if (encoder_id >= ENCODER_COUNT) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }

    encoder_entry_t *entry = (encoder_entry_t*)response->payload;
    entry->encoder_id = encoder_id;
    entry->ccw_keycode = 0;
    entry->cw_keycode = 0;
    entry->reserved = 0;

    uint16_t ccw_code = 0;
    uint16_t cw_code = 0;
    if (request_encoder_from_slave(slave_addr, encoder_id, &ccw_code, &cw_code)) {
        entry->ccw_keycode = ccw_code;
        entry->cw_keycode = cw_code;
        response->status = STATUS_OK;
    } else {
        response->status = STATUS_ERROR;
    }

    response->payload_length = sizeof(encoder_entry_t);
}

static void handle_set_slave_encoder(const config_packet_t *request, config_packet_t *response)
{
    if (request->payload_length < sizeof(encoder_entry_t) + 1) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }

    uint8_t slave_addr = request->payload[0];
    const encoder_entry_t *entry = (const encoder_entry_t*)(request->payload + 1);

    if (i2c_manager_get_mode() != 1) {
        response->status = STATUS_ERROR;
        return;
    }

    if (entry->encoder_id >= ENCODER_COUNT) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }

    if (send_encoder_to_slave(slave_addr, entry->encoder_id, entry->ccw_keycode, entry->cw_keycode)) {
        response->status = STATUS_OK;
    } else {
        response->status = STATUS_ERROR;
    }
}

static void handle_get_slave_info(const config_packet_t *request, config_packet_t *response)
{
    if (request->payload_length < 1) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }
    
    uint8_t slave_addr = request->payload[0];
    
    // Check if we're in master mode
    if (i2c_manager_get_mode() != 1) {
        response->status = STATUS_ERROR;
        usb_app_cdc_printf("Config: Cannot get slave info - not in master mode\r\n");
        return;
    }
    
    // For now, just return basic info about the slave
    // In a more complete implementation, we would query the slave for its info
    device_info_t *info = (device_info_t*)response->payload;
    
    info->protocol_version = CONFIG_PROTOCOL_VERSION;
    info->firmware_version_major = 1;
    info->firmware_version_minor = 0;
    info->firmware_version_patch = 0;
    info->device_type = 0; // 0=Slave
    info->matrix_rows = MATRIX_ROWS; // Assuming same matrix size as master
    info->matrix_cols = MATRIX_COLS;
    info->encoder_count = ENCODER_COUNT;
    info->i2c_devices = 0; // Slaves don't have I2C devices
    
    // Set default name
    char name[16];
    sprintf(name, "Slave %02X", slave_addr);
    memcpy(info->device_name, name, 16);
    
    response->payload_length = sizeof(device_info_t);
    response->status = STATUS_OK;
    
    usb_app_cdc_printf("Config: Get slave info for device 0x%02X\r\n", slave_addr);
}

// Helper functions for I2C communication with slaves
static bool request_keymap_from_slave(uint8_t slave_addr, uint8_t row, uint8_t col, uint16_t *keycode)
{
    uint8_t tx_data[I2C_SLAVE_CONFIG_CMD_SIZE] = {
        CMD_GET_KEYMAP,  // Command to get keymap
        row,             // Row
        col,             // Column
        0,               // Padding / reserved
        0,               // Reserved for alignment with SET command length
        0                // Reserved for future use
    };
    uint8_t rx_data[4] = {0};
    
    // Send request to slave and get response
    if (HAL_I2C_Master_Transmit(&hi2c2, slave_addr << 1, tx_data, sizeof(tx_data), 100) != HAL_OK) {
        usb_app_cdc_printf("GET_KEYMAP: TX failed to 0x%02X\r\n", slave_addr);
        return false;
    }
    
    // Wait for slave to process command and prepare response
    // This delay must be long enough for the slave's RX complete callback to finish
    HAL_Delay(20);
    
    // Receive response
    if (HAL_I2C_Master_Receive(&hi2c2, slave_addr << 1, rx_data, sizeof(rx_data), 100) != HAL_OK) {
        usb_app_cdc_printf("GET_KEYMAP: RX failed from 0x%02X\r\n", slave_addr);
        return false;
    }
    
    // Check if response is valid
    if (rx_data[0] != CMD_GET_KEYMAP) {
        usb_app_cdc_printf("GET_KEYMAP: Bad response [%02X %02X %02X %02X]\r\n", 
                     rx_data[0], rx_data[1], rx_data[2], rx_data[3]);
        return false;
    }
    
    // Extract keycode (little endian)
    *keycode = rx_data[1] | (rx_data[2] << 8);
    
    usb_app_cdc_printf("GET_KEYMAP: [%d,%d] = 0x%04X\r\n", row, col, *keycode);
    return true;
}

static bool send_keymap_to_slave(uint8_t slave_addr, uint8_t row, uint8_t col, uint16_t keycode)
{
    uint8_t tx_data[I2C_SLAVE_CONFIG_CMD_SIZE] = {
        CMD_SET_KEYMAP,      // Command to set keymap
        row,                 // Row
        col,                 // Column
        keycode & 0xFF,      // Keycode (low byte)
        (keycode >> 8) & 0xFF, // Keycode (high byte)
        0                    // Padding
    };
    uint8_t rx_data[2] = {0};
    
    // Send request to slave
    if (HAL_I2C_Master_Transmit(&hi2c2, slave_addr << 1, tx_data, sizeof(tx_data), 100) != HAL_OK) {
        usb_app_cdc_printf("SET_KEYMAP: TX failed to 0x%02X\r\n", slave_addr);
        return false;
    }
    
    // Wait for slave to process command and prepare response
    // This delay must be long enough for the slave's RX complete callback to finish
    HAL_Delay(20);
    
    // Receive response (status)
    if (HAL_I2C_Master_Receive(&hi2c2, slave_addr << 1, rx_data, sizeof(rx_data), 100) != HAL_OK) {
        usb_app_cdc_printf("SET_KEYMAP: RX failed from 0x%02X\r\n", slave_addr);
        return false;
    }
    
    // Check if response is valid
    if (rx_data[0] != CMD_SET_KEYMAP || rx_data[1] != STATUS_OK) {
        usb_app_cdc_printf("SET_KEYMAP: Bad response [%02X %02X]\r\n", rx_data[0], rx_data[1]);
        return false;
    }
    
    usb_app_cdc_printf("SET_KEYMAP: [%d,%d] = 0x%04X OK\r\n", row, col, keycode);
    return true;
}

static bool request_encoder_from_slave(uint8_t slave_addr, uint8_t encoder_id, uint16_t *ccw_keycode, uint16_t *cw_keycode)
{
    uint8_t tx_data[I2C_SLAVE_CONFIG_CMD_SIZE] = {
        CMD_GET_ENCODER_MAP,
        encoder_id,
        0,
        0,
        0,
        0
    };
    uint8_t rx_data[I2C_SLAVE_CONFIG_MAX_RESPONSE] = {0};

    if (HAL_I2C_Master_Transmit(&hi2c2, slave_addr << 1, tx_data, sizeof(tx_data), 100) != HAL_OK) {
        usb_app_cdc_printf("GET_ENCODER: TX failed to 0x%02X\r\n", slave_addr);
        return false;
    }

    HAL_Delay(20);

    if (HAL_I2C_Master_Receive(&hi2c2, slave_addr << 1, rx_data, 7, 100) != HAL_OK) {
        usb_app_cdc_printf("GET_ENCODER: RX failed from 0x%02X\r\n", slave_addr);
        return false;
    }

    if (rx_data[0] != CMD_GET_ENCODER_MAP || rx_data[1] != encoder_id) {
        usb_app_cdc_printf("GET_ENCODER: Bad response header [%02X %02X]\r\n", rx_data[0], rx_data[1]);
        return false;
    }

    if (rx_data[6] != STATUS_OK) {
        usb_app_cdc_printf("GET_ENCODER: Status error %02X\r\n", rx_data[6]);
        return false;
    }

    *ccw_keycode = rx_data[2] | (rx_data[3] << 8);
    *cw_keycode = rx_data[4] | (rx_data[5] << 8);

    usb_app_cdc_printf("GET_ENCODER: [%d] CCW=0x%04X CW=0x%04X\r\n", encoder_id, *ccw_keycode, *cw_keycode);
    return true;
}

static bool send_encoder_to_slave(uint8_t slave_addr, uint8_t encoder_id, uint16_t ccw_keycode, uint16_t cw_keycode)
{
    uint8_t tx_data[I2C_SLAVE_CONFIG_CMD_SIZE] = {
        CMD_SET_ENCODER_MAP,
        encoder_id,
        ccw_keycode & 0xFF,
        (ccw_keycode >> 8) & 0xFF,
        cw_keycode & 0xFF,
        (cw_keycode >> 8) & 0xFF
    };
    uint8_t rx_data[2] = {0};

    if (HAL_I2C_Master_Transmit(&hi2c2, slave_addr << 1, tx_data, sizeof(tx_data), 100) != HAL_OK) {
        usb_app_cdc_printf("SET_ENCODER: TX failed to 0x%02X\r\n", slave_addr);
        return false;
    }

    HAL_Delay(20);

    if (HAL_I2C_Master_Receive(&hi2c2, slave_addr << 1, rx_data, sizeof(rx_data), 100) != HAL_OK) {
        usb_app_cdc_printf("SET_ENCODER: RX failed from 0x%02X\r\n", slave_addr);
        return false;
    }

    if (rx_data[0] != CMD_SET_ENCODER_MAP || rx_data[1] != STATUS_OK) {
        usb_app_cdc_printf("SET_ENCODER: Bad response [%02X %02X]\r\n", rx_data[0], rx_data[1]);
        return false;
    }

    usb_app_cdc_printf("SET_ENCODER: [%d] CCW=0x%04X CW=0x%04X OK\r\n", encoder_id, ccw_keycode, cw_keycode);
    return true;
}

// Public functions
void config_protocol_init(void)
{
    packet_received = false;
    packet_pending = false;
    
    // Clear packets manually instead of using memset
    rx_packet.header = 0;
    rx_packet.command = 0;
    rx_packet.sequence = 0;
    rx_packet.status = 0;
    rx_packet.payload_length = 0;
    for (int i = 0; i < CONFIG_MAX_PAYLOAD_SIZE; i++) {
        rx_packet.payload[i] = 0;
    }
    
    tx_packet.header = 0;
    tx_packet.command = 0;
    tx_packet.sequence = 0;
    tx_packet.status = 0;
    tx_packet.payload_length = 0;
    for (int i = 0; i < CONFIG_MAX_PAYLOAD_SIZE; i++) {
        tx_packet.payload[i] = 0;
    }
    
    // Initialize EEPROM emulation and keymap system
    keymap_init();
    if (!eeprom_init()) {
        usb_app_cdc_printf("Config: EEPROM initialization failed\r\n");
    }
    
    usb_app_cdc_printf("Config protocol initialized\r\n");
}

void config_protocol_task(void)
{
    // Process received packets
    if (packet_received) {
        packet_received = false;
        
        if (config_protocol_process_packet(&rx_packet)) {
            packet_pending = true;
        }
    }
    
    // Send pending response if HID is ready
    if (packet_pending && tud_hid_n_ready(2)) {
        if (tud_hid_n_report(2, 0, &tx_packet, sizeof(tx_packet))) {
            packet_pending = false;
        }
    }
}

bool config_protocol_process_packet(const config_packet_t *packet)
{
    // Prepare response packet
    tx_packet.header = CONFIG_PACKET_HEADER;
    tx_packet.command = packet->command;
    tx_packet.sequence = packet->sequence;
    tx_packet.status = STATUS_OK;
    tx_packet.payload_length = 0;
    // Clear reserved and payload fields manually
    for (int i = 0; i < 2; i++) {
        tx_packet.reserved[i] = 0;
    }
    for (int i = 0; i < CONFIG_MAX_PAYLOAD_SIZE; i++) {
        tx_packet.payload[i] = 0;
    }
    
    
    switch (packet->command) {
        case CMD_GET_INFO:
            handle_get_info(&tx_packet);
            break;
            
        case CMD_GET_KEYMAP:
            handle_get_keymap(packet, &tx_packet);
            break;
            
        case CMD_SET_KEYMAP:
            handle_set_keymap(packet, &tx_packet);
            break;
            
        case CMD_GET_ENCODER_MAP:
            handle_get_encoder_map(packet, &tx_packet);
            break;
            
        case CMD_SET_ENCODER_MAP:
            handle_set_encoder_map(packet, &tx_packet);
            break;
            
        case CMD_GET_I2C_DEVICES:
            handle_get_i2c_devices(&tx_packet);
            break;
            
        case CMD_GET_DEVICE_STATUS:
            handle_get_device_status(&tx_packet);
            break;

        case CMD_MIDI_SEND_RAW:
            handle_midi_send_raw(packet, &tx_packet);
            break;

        case CMD_MIDI_NOTE_ON:
            handle_midi_note_on(packet, &tx_packet);
            break;

        case CMD_MIDI_NOTE_OFF:
            handle_midi_note_off(packet, &tx_packet);
            break;

        case CMD_MIDI_CC:
            handle_midi_cc(packet, &tx_packet);
            break;
            
        case CMD_GET_SLAVE_KEYMAP:
            handle_get_slave_keymap(packet, &tx_packet);
            break;
            
        case CMD_SET_SLAVE_KEYMAP:
            handle_set_slave_keymap(packet, &tx_packet);
            break;
            
        case CMD_GET_SLAVE_INFO:
            handle_get_slave_info(packet, &tx_packet);
            break;

        case CMD_GET_SLAVE_ENCODER:
            handle_get_slave_encoder(packet, &tx_packet);
            break;

        case CMD_SET_SLAVE_ENCODER:
            handle_set_slave_encoder(packet, &tx_packet);
            break;
            
        case CMD_SAVE_CONFIG:
            // Use force save for explicit user save requests
            if (eeprom_force_save_config()) {
                tx_packet.status = STATUS_OK;
                usb_app_cdc_printf("Config: Configuration force-saved to EEPROM\r\n");
            } else {
                tx_packet.status = STATUS_ERROR;
                usb_app_cdc_printf("Config: Failed to save configuration\r\n");
            }
            break;
            
        case CMD_LOAD_CONFIG:
            if (eeprom_load_config()) {
                tx_packet.status = STATUS_OK;
                usb_app_cdc_printf("Config: Configuration loaded from EEPROM\r\n");
            } else {
                tx_packet.status = STATUS_ERROR;
                usb_app_cdc_printf("Config: Failed to load configuration\r\n");
            }
            break;
            
        case CMD_RESET_CONFIG:
            if (eeprom_reset_config()) {
                tx_packet.status = STATUS_OK;
                usb_app_cdc_printf("Config: Configuration reset to defaults\r\n");
            } else {
                tx_packet.status = STATUS_ERROR;
                usb_app_cdc_printf("Config: Failed to reset configuration\r\n");
            }
            break;
            
        case CMD_REBOOT:
            tx_packet.status = STATUS_OK;
            // TODO: Implement system reset after sending response
            break;
            
        default:
            tx_packet.status = STATUS_INVALID_CMD;
            usb_app_cdc_printf("Config: Unknown command 0x%02X\r\n", packet->command);
            break;
    }
    
    return true;
}

void config_protocol_hid_receive(uint8_t const* buffer, uint16_t bufsize)
{
    if (bufsize != sizeof(config_packet_t)) {
        usb_app_cdc_printf("Config: Invalid packet size %d\r\n", bufsize);
        return;
    }
    
    // Copy buffer to rx_packet manually
    uint8_t *dest = (uint8_t*)&rx_packet;
    for (int i = 0; i < sizeof(config_packet_t); i++) {
        dest[i] = buffer[i];
    }
    packet_received = true;
    
}

bool config_protocol_hid_ready(void)
{
    return tud_hid_n_ready(2);
}

// Private command handlers
static void handle_get_info(config_packet_t *response)
{
    extern uint8_t detected_slave_count;
    device_info_t *info = (device_info_t*)response->payload;
    
    info->protocol_version = CONFIG_PROTOCOL_VERSION;
    info->firmware_version_major = 1;
    info->firmware_version_minor = 0;
    info->firmware_version_patch = 0;
    info->device_type = i2c_manager_get_mode(); // 0=Slave, 1=Master
    info->matrix_rows = MATRIX_ROWS;
    info->matrix_cols = MATRIX_COLS;
    info->encoder_count = ENCODER_COUNT;
    info->i2c_devices = detected_slave_count; // Return actual count of detected slaves
    // Copy device name manually
    const char name[] = "OpenGrader Modular";
    int name_len = 0;
    while (name[name_len] != '\0' && name_len < 15) name_len++; // Calculate length
    for (int i = 0; i < name_len && i < 15; i++) {
        info->device_name[i] = name[i];
    }
    info->device_name[name_len] = '\0'; // Null terminate
    
    response->payload_length = sizeof(device_info_t);
    
    usb_app_cdc_printf("Config: Device info - Type:%d, Matrix:%dx%d, Encoders:%d, I2C:%d\r\n",
                 info->device_type, info->matrix_rows, info->matrix_cols, info->encoder_count, info->i2c_devices);
}

static void handle_get_keymap(const config_packet_t *request, config_packet_t *response)
{
    if (request->payload_length < 2) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }
    
    uint8_t row = request->payload[0];
    uint8_t col = request->payload[1];
    
    if (row >= MATRIX_ROWS || col >= MATRIX_COLS) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }
    
    keymap_entry_t *entry = (keymap_entry_t*)response->payload;
    entry->row = row;
    entry->col = col;
    entry->keycode = keymap_get_keycode(row, col);
    
    response->payload_length = sizeof(keymap_entry_t);
    
}

static void handle_set_keymap(const config_packet_t *request, config_packet_t *response)
{
    if (request->payload_length < sizeof(keymap_entry_t)) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }
    
    const keymap_entry_t *entry = (const keymap_entry_t*)request->payload;
    
    if (entry->row >= MATRIX_ROWS || entry->col >= MATRIX_COLS) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }
    
    // Set keycode in EEPROM
    if (keymap_set_keycode(entry->row, entry->col, entry->keycode)) {
        response->status = STATUS_OK;
    } else {
        response->status = STATUS_ERROR;
    }
    
    usb_app_cdc_printf("Config: Set keymap [%d,%d] = 0x%04X\r\n", 
                 entry->row, entry->col, entry->keycode);
}

static void handle_get_encoder_map(const config_packet_t *request, config_packet_t *response)
{
    if (request->payload_length < 1) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }
    
    uint8_t encoder_id = request->payload[0];
    
    if (encoder_id >= ENCODER_COUNT) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }
    
    encoder_entry_t *entry = (encoder_entry_t*)response->payload;
    entry->encoder_id = encoder_id;
    
    // Get encoder mapping from EEPROM or default (use local variables to avoid packed member warnings)
    uint16_t ccw_keycode, cw_keycode;
    if (!keymap_get_encoder_map(encoder_id, &ccw_keycode, &cw_keycode)) {
        response->status = STATUS_ERROR;
        return;
    }
    entry->ccw_keycode = ccw_keycode;
    entry->cw_keycode = cw_keycode;
    entry->reserved = 0;
    
    response->payload_length = sizeof(encoder_entry_t);
   
}

static void handle_set_encoder_map(const config_packet_t *request, config_packet_t *response)
{
    if (request->payload_length < sizeof(encoder_entry_t)) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }
    
    const encoder_entry_t *entry = (const encoder_entry_t*)request->payload;
    
    if (entry->encoder_id >= ENCODER_COUNT) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }
    
    // Set encoder mapping in EEPROM
    if (keymap_set_encoder_map(entry->encoder_id, entry->ccw_keycode, entry->cw_keycode)) {
        response->status = STATUS_OK;
    } else {
        response->status = STATUS_ERROR;
    }
    
    usb_app_cdc_printf("Config: Set encoder %d = CCW:0x%04X CW:0x%04X\r\n", 
                 entry->encoder_id, entry->ccw_keycode, entry->cw_keycode);
}

static void handle_get_i2c_devices(config_packet_t *response)
{
    extern uint8_t detected_slaves[16];
    extern uint8_t detected_slave_count;
    
    // Force a scan of I2C devices
    i2c_manager_scan_slaves();

    // First byte of payload is the count of detected slaves
    response->payload[0] = detected_slave_count;

    // Copy detected slave info into the payload
    for (int i = 0; i < detected_slave_count; i++) {
        i2c_device_info_t device_info;
        // Clear the struct
        for (int j = 0; j < sizeof(i2c_device_info_t); j++) {
            ((uint8_t*)&device_info)[j] = 0;
        }
        
        device_info.address = detected_slaves[i];
        device_info.device_type = 0; // Unknown for now, can be updated later
        device_info.status = 1;      // Online
        device_info.firmware_version_major = 0;
        device_info.firmware_version_minor = 0;
        device_info.firmware_version_patch = 0;
        sprintf(device_info.name, "Slave %02X", detected_slaves[i]);
        // Ensure name is null-terminated
        device_info.name[sizeof(device_info.name) - 1] = '\0';

        memcpy(&response->payload[1 + (i * sizeof(i2c_device_info_t))], &device_info, sizeof(i2c_device_info_t));
    }

    response->payload_length = 1 + (detected_slave_count * sizeof(i2c_device_info_t));
}

static void handle_get_device_status(config_packet_t *response)
{
    // TODO: Implement device status reporting
    response->payload[0] = 1; // Status: Online
    response->payload_length = 1;
    
    usb_app_cdc_printf("Config: Get device status\r\n");
}

static void handle_midi_send_raw(const config_packet_t *request, config_packet_t *response)
{
    if (request->payload_length < 4) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }

    uint8_t packet[4];
    for (int i = 0; i < 4; i++) packet[i] = request->payload[i];

    if (usb_app_midi_send_packet(packet)) {
        response->status = STATUS_OK;
    } else {
        response->status = STATUS_ERROR;
    }
}

static void handle_midi_note_on(const config_packet_t *request, config_packet_t *response)
{
    if (request->payload_length < 3) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }

    uint8_t channel = request->payload[0];
    uint8_t note = request->payload[1];
    uint8_t velocity = request->payload[2];

    if (usb_app_midi_send_note_on(channel, note, velocity)) {
        response->status = STATUS_OK;
    } else {
        response->status = STATUS_ERROR;
    }
}

static void handle_midi_note_off(const config_packet_t *request, config_packet_t *response)
{
    if (request->payload_length < 2) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }

    uint8_t channel = request->payload[0];
    uint8_t note = request->payload[1];

    if (usb_app_midi_send_note_off(channel, note)) {
        response->status = STATUS_OK;
    } else {
        response->status = STATUS_ERROR;
    }
}

static void handle_midi_cc(const config_packet_t *request, config_packet_t *response)
{
    if (request->payload_length < 3) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }

    uint8_t channel = request->payload[0];
    uint8_t controller = request->payload[1];
    uint8_t value = request->payload[2];

    if (usb_app_midi_send_cc(channel, controller, value)) {
        response->status = STATUS_OK;
    } else {
        response->status = STATUS_ERROR;
    }
}