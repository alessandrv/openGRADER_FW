#include "config_protocol.h"
#include "usb_app.h"
#include "input/keymap.h"
#include "i2c_manager.h"
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
            usb_app_cdc_printf("Config: Response sent\r\n");
        }
    }
}

bool config_protocol_process_packet(const config_packet_t *packet)
{
    // Read header from raw bytes to ensure correct interpretation
    uint8_t *raw_bytes = (uint8_t*)packet;
    
    usb_app_cdc_printf("Config: Raw bytes: [0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X]\r\n", 
                 raw_bytes[0], raw_bytes[1], raw_bytes[2], raw_bytes[3], 
                 raw_bytes[4], raw_bytes[5], raw_bytes[6], raw_bytes[7]);
    
    // Read header as little-endian from raw bytes
    uint16_t received_header = raw_bytes[0] | (raw_bytes[1] << 8);
    
    usb_app_cdc_printf("Config: Header=0x%04X Cmd=0x%02X Status=0x%02X Seq=0x%02X Len=%d\r\n",
                       received_header, raw_bytes[2], raw_bytes[3], raw_bytes[4], raw_bytes[5]);
    
    if (received_header != CONFIG_PACKET_HEADER) {
        usb_app_cdc_printf("Config: Invalid header 0x%04X (expected 0x%04X)\r\n", received_header, CONFIG_PACKET_HEADER);
        return false;
    }
    
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
    
    usb_app_cdc_printf("Config: Processing command 0x%02X\r\n", packet->command);
    
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
    
    usb_app_cdc_printf("Config: Packet received, cmd=0x%02X\r\n", rx_packet.command);
}

bool config_protocol_hid_ready(void)
{
    return tud_hid_n_ready(2);
}

// Private command handlers
static void handle_get_info(config_packet_t *response)
{
    device_info_t *info = (device_info_t*)response->payload;
    
    info->protocol_version = CONFIG_PROTOCOL_VERSION;
    info->firmware_version_major = 1;
    info->firmware_version_minor = 0;
    info->firmware_version_patch = 0;
    info->device_type = i2c_manager_get_mode(); // 0=Slave, 1=Master
    info->matrix_rows = MATRIX_ROWS;
    info->matrix_cols = MATRIX_COLS;
    info->encoder_count = ENCODER_COUNT;
    info->i2c_devices = 0; // TODO: Get actual count
    // Copy device name manually
    const char name[] = "OpenGrader Modular";
    int name_len = 0;
    while (name[name_len] != '\0' && name_len < 15) name_len++; // Calculate length
    for (int i = 0; i < name_len && i < 15; i++) {
        info->device_name[i] = name[i];
    }
    info->device_name[name_len] = '\0'; // Null terminate
    
    response->payload_length = sizeof(device_info_t);
    
    usb_app_cdc_printf("Config: Device info - Type:%d, Matrix:%dx%d, Encoders:%d\r\n",
                 info->device_type, info->matrix_rows, info->matrix_cols, info->encoder_count);
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
    
    usb_app_cdc_printf("Config: Get keymap [%d,%d] = 0x%04X\r\n", row, col, entry->keycode);
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
    
    usb_app_cdc_printf("Config: Get encoder %d = CCW:0x%04X CW:0x%04X\r\n", 
                 encoder_id, entry->ccw_keycode, entry->cw_keycode);
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
    // TODO: Implement I2C device scanning
    response->payload[0] = 0; // Device count
    response->payload_length = 1;
    
    usb_app_cdc_printf("Config: Get I2C devices (not implemented)\r\n");
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