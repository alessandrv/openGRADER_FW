#include "config_protocol.h"
#include "usb_app.h"
#include "input/keymap.h"
#include "input/board_layout.h"
#include "input/slider.h"
#include "i2c_manager.h"
#include "i2c.h"  // Added to include hi2c2 declaration
#include "pin_config.h"
#include "eeprom_emulation.h"
#include "device_info_util.h"
#include "tusb.h"
#include "class/hid/hid_device.h"
#include <string.h>  // For memset, memcpy, strncpy, snprintf
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

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
static void handle_get_layout_info(config_packet_t *response);
static void handle_get_layout_cell_type(const config_packet_t *request, config_packet_t *response);
static void handle_get_layout_cell_component_id(const config_packet_t *request, config_packet_t *response);
static void handle_set_layer_state(const config_packet_t *request, config_packet_t *response);
static void handle_get_layer_state(config_packet_t *response);
static void handle_midi_send_raw(const config_packet_t *request, config_packet_t *response);
static void handle_midi_note_on(const config_packet_t *request, config_packet_t *response);
static void handle_midi_note_off(const config_packet_t *request, config_packet_t *response);
static void handle_midi_cc(const config_packet_t *request, config_packet_t *response);

// Slider protocol handlers
static void handle_get_slider_value(const config_packet_t *request, config_packet_t *response);
static void handle_get_slider_config(const config_packet_t *request, config_packet_t *response);
static void handle_set_slider_config(const config_packet_t *request, config_packet_t *response);
static bool request_keymap_from_slave(uint8_t slave_addr, uint8_t layer, uint8_t row, uint8_t col, uint16_t *keycode);
static bool send_keymap_to_slave(uint8_t slave_addr, uint8_t layer, uint8_t row, uint8_t col, uint16_t keycode);
static bool request_encoder_from_slave(uint8_t slave_addr, uint8_t layer, uint8_t encoder_id, uint16_t *ccw_keycode, uint16_t *cw_keycode);
static bool send_encoder_to_slave(uint8_t slave_addr, uint8_t layer, uint8_t encoder_id, uint16_t ccw_keycode, uint16_t cw_keycode);
static bool send_save_to_slave(uint8_t slave_addr);
static HAL_StatusTypeDef i2c_master_receive_with_retry(uint8_t slave_addr, uint8_t *buffer, uint16_t length, uint32_t timeout);
static bool request_device_info_from_slave(uint8_t slave_addr, device_info_t *info);
static void handle_get_slave_keymap(const config_packet_t *request, config_packet_t *response)
{
    if (request->payload_length < 4) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }
    
    uint8_t slave_addr = request->payload[0];
    uint8_t layer = request->payload[1];
    uint8_t row = request->payload[2];
    uint8_t col = request->payload[3];
    
    usb_app_cdc_printf("GET_SLAVE[0x%02X,L%d,%d,%d]\r\n", slave_addr, layer, row, col);
    
    // Check if we're in master mode
    if (i2c_manager_get_mode() != 1) {
        response->status = STATUS_ERROR;
        return;
    }
    
    // Create keymap entry structure in response
    keymap_entry_t *entry = (keymap_entry_t*)response->payload;
    entry->layer = layer;
    entry->row = row;
    entry->col = col;
    entry->keycode = 0; // Default value if request fails
    
    // Request keymap from slave
    uint16_t keycode = 0;
    if (request_keymap_from_slave(slave_addr, layer, row, col, &keycode)) {
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

    if (entry->layer >= KEYMAP_LAYER_COUNT) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }
    
    // Send keymap to slave
    if (send_keymap_to_slave(slave_addr, entry->layer, entry->row, entry->col, entry->keycode)) {
        response->status = STATUS_OK;
    } else {
        response->status = STATUS_ERROR;
    }
    
    usb_app_cdc_printf("Config: Set slave keymap [%02X,L%d,%d,%d] = 0x%04X\r\n", 
                 slave_addr, entry->layer, entry->row, entry->col, entry->keycode);
}

static void handle_get_slave_encoder(const config_packet_t *request, config_packet_t *response)
{
    if (request->payload_length < 3) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }

    uint8_t slave_addr = request->payload[0];
    uint8_t layer = request->payload[1];
    uint8_t encoder_id = request->payload[2];

    if (i2c_manager_get_mode() != 1) {
        response->status = STATUS_ERROR;
        return;
    }

    if (layer >= KEYMAP_LAYER_COUNT) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }

    if (encoder_id >= ENCODER_COUNT) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }

    encoder_entry_t *entry = (encoder_entry_t*)response->payload;
    entry->layer = layer;
    entry->encoder_id = encoder_id;
    entry->ccw_keycode = 0;
    entry->cw_keycode = 0;
    entry->reserved = 0;

    uint16_t ccw_code = 0;
    uint16_t cw_code = 0;
    if (request_encoder_from_slave(slave_addr, layer, encoder_id, &ccw_code, &cw_code)) {
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

    if (entry->layer >= KEYMAP_LAYER_COUNT) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }

    if (entry->encoder_id >= ENCODER_COUNT) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }

    if (send_encoder_to_slave(slave_addr, entry->layer, entry->encoder_id, entry->ccw_keycode, entry->cw_keycode)) {
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
    
    device_info_t *info = (device_info_t*)response->payload;
    memset(info, 0, sizeof(device_info_t));

    if (!request_device_info_from_slave(slave_addr, info)) {
        response->status = STATUS_ERROR;
        return;
    }

    response->payload_length = sizeof(device_info_t);
    response->status = STATUS_OK;

    usb_app_cdc_printf("Config: Get slave info for device 0x%02X (%s)\r\n", slave_addr, info->device_name);
}

// Helper functions for I2C communication with slaves
static bool request_keymap_from_slave(uint8_t slave_addr, uint8_t layer, uint8_t row, uint8_t col, uint16_t *keycode)
{
    uint8_t tx_data[I2C_SLAVE_CONFIG_CMD_SIZE] = {
        CMD_GET_KEYMAP,
        layer,
        row,
        col,
        0,
        0,
        0
    };
    uint8_t rx_data[7] = {0};

    if (HAL_I2C_Master_Transmit(&hi2c2, slave_addr << 1, tx_data, sizeof(tx_data), 100) != HAL_OK) {
        usb_app_cdc_printf("GET_KEYMAP: TX failed to 0x%02X\r\n", slave_addr);
        return false;
    }

    if (i2c_master_receive_with_retry(slave_addr, rx_data, sizeof(rx_data), 100) != HAL_OK) {
        usb_app_cdc_printf("GET_KEYMAP: RX failed from 0x%02X\r\n", slave_addr);
        return false;
    }

    if (rx_data[0] != CMD_GET_KEYMAP || rx_data[1] != layer || rx_data[2] != row || rx_data[3] != col) {
        usb_app_cdc_printf("GET_KEYMAP: Bad response [%02X %02X %02X %02X ...]\r\n",
                           rx_data[0], rx_data[1], rx_data[2], rx_data[3]);
        return false;
    }

    if (rx_data[6] != STATUS_OK) {
        usb_app_cdc_printf("GET_KEYMAP: Status %02X from 0x%02X\r\n", rx_data[6], slave_addr);
        return false;
    }

    *keycode = rx_data[4] | (rx_data[5] << 8);

    usb_app_cdc_printf("GET_KEYMAP: [L%d,%d,%d] = 0x%04X\r\n", layer, row, col, *keycode);
    return true;
}

static bool send_keymap_to_slave(uint8_t slave_addr, uint8_t layer, uint8_t row, uint8_t col, uint16_t keycode)
{
    uint8_t tx_data[I2C_SLAVE_CONFIG_CMD_SIZE] = {
        CMD_SET_KEYMAP,
        layer,
        row,
        col,
        keycode & 0xFF,
        (keycode >> 8) & 0xFF,
        0
    };
    uint8_t rx_data[2] = {0};

    if (HAL_I2C_Master_Transmit(&hi2c2, slave_addr << 1, tx_data, sizeof(tx_data), 100) != HAL_OK) {
        usb_app_cdc_printf("SET_KEYMAP: TX failed to 0x%02X\r\n", slave_addr);
        return false;
    }

    if (i2c_master_receive_with_retry(slave_addr, rx_data, sizeof(rx_data), 100) != HAL_OK) {
        usb_app_cdc_printf("SET_KEYMAP: RX failed from 0x%02X\r\n", slave_addr);
        return false;
    }

    if (rx_data[0] != CMD_SET_KEYMAP || rx_data[1] != STATUS_OK) {
        usb_app_cdc_printf("SET_KEYMAP: Bad response [%02X %02X]\r\n", rx_data[0], rx_data[1]);
        return false;
    }

    usb_app_cdc_printf("SET_KEYMAP: [L%d,%d,%d] = 0x%04X OK\r\n", layer, row, col, keycode);
    return true;
}

static bool request_encoder_from_slave(uint8_t slave_addr, uint8_t layer, uint8_t encoder_id, uint16_t *ccw_keycode, uint16_t *cw_keycode)
{
    uint8_t tx_data[I2C_SLAVE_CONFIG_CMD_SIZE] = {
        CMD_GET_ENCODER_MAP,
        layer,
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

    if (i2c_master_receive_with_retry(slave_addr, rx_data, 8, 100) != HAL_OK) {
        usb_app_cdc_printf("GET_ENCODER: RX failed from 0x%02X\r\n", slave_addr);
        return false;
    }

    if (rx_data[0] != CMD_GET_ENCODER_MAP || rx_data[1] != layer || rx_data[2] != encoder_id) {
        usb_app_cdc_printf("GET_ENCODER: Bad response header [%02X %02X %02X]\r\n",
                           rx_data[0], rx_data[1], rx_data[2]);
        return false;
    }

    if (rx_data[7] != STATUS_OK) {
        usb_app_cdc_printf("GET_ENCODER: Status error %02X\r\n", rx_data[7]);
        return false;
    }

    *ccw_keycode = rx_data[3] | (rx_data[4] << 8);
    *cw_keycode = rx_data[5] | (rx_data[6] << 8);

    usb_app_cdc_printf("GET_ENCODER: [L%d,%d] CCW=0x%04X CW=0x%04X\r\n", layer, encoder_id, *ccw_keycode, *cw_keycode);
    return true;
}

static bool send_encoder_to_slave(uint8_t slave_addr, uint8_t layer, uint8_t encoder_id, uint16_t ccw_keycode, uint16_t cw_keycode)
{
    uint8_t tx_data[I2C_SLAVE_CONFIG_CMD_SIZE] = {
        CMD_SET_ENCODER_MAP,
        layer,
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

    if (i2c_master_receive_with_retry(slave_addr, rx_data, sizeof(rx_data), 100) != HAL_OK) {
        usb_app_cdc_printf("SET_ENCODER: RX failed from 0x%02X\r\n", slave_addr);
        return false;
    }

    if (rx_data[0] != CMD_SET_ENCODER_MAP || rx_data[1] != STATUS_OK) {
        usb_app_cdc_printf("SET_ENCODER: Bad response [%02X %02X]\r\n", rx_data[0], rx_data[1]);
        return false;
    }

    usb_app_cdc_printf("SET_ENCODER: [L%d,%d] CCW=0x%04X CW=0x%04X OK\r\n", layer, encoder_id, ccw_keycode, cw_keycode);
    return true;
}

static bool send_save_to_slave(uint8_t slave_addr)
{
    uint8_t tx_data[I2C_SLAVE_CONFIG_CMD_SIZE] = {
        CMD_SAVE_CONFIG,
        0,
        0,
        0,
        0,
        0
    };
    uint8_t rx_data[2] = {0};

    if (HAL_I2C_Master_Transmit(&hi2c2, slave_addr << 1, tx_data, sizeof(tx_data), 200) != HAL_OK) {
        usb_app_cdc_printf("SAVE_CONFIG: TX failed to 0x%02X\r\n", slave_addr);
        return false;
    }

    if (i2c_master_receive_with_retry(slave_addr, rx_data, sizeof(rx_data), 200) != HAL_OK) {
        usb_app_cdc_printf("SAVE_CONFIG: RX failed from 0x%02X\r\n", slave_addr);
        return false;
    }

    if (rx_data[0] != CMD_SAVE_CONFIG || rx_data[1] != STATUS_OK) {
        usb_app_cdc_printf("SAVE_CONFIG: Bad response [%02X %02X]\r\n", rx_data[0], rx_data[1]);
        return false;
    }

    usb_app_cdc_printf("SAVE_CONFIG: Slave 0x%02X saved successfully\r\n", slave_addr);
    return true;
}

static bool request_device_info_from_slave(uint8_t slave_addr, device_info_t *info)
{
    if (!info) {
        return false;
    }

    uint8_t tx_data[I2C_SLAVE_CONFIG_CMD_SIZE] = {
        CMD_GET_INFO,
        0,
        0,
        0,
        0,
        0,
        0
    };

    const uint16_t expected_len = (uint16_t)(1u + sizeof(device_info_t) + 1u);
    uint8_t rx_data[I2C_SLAVE_CONFIG_MAX_RESPONSE] = {0};

    if (expected_len > I2C_SLAVE_CONFIG_MAX_RESPONSE) {
        usb_app_cdc_printf("GET_INFO: Expected length %u exceeds buffer\r\n", expected_len);
        return false;
    }

    if (HAL_I2C_Master_Transmit(&hi2c2, slave_addr << 1, tx_data, sizeof(tx_data), 200) != HAL_OK) {
        usb_app_cdc_printf("GET_INFO: TX failed to 0x%02X\r\n", slave_addr);
        return false;
    }

    if (i2c_master_receive_with_retry(slave_addr, rx_data, expected_len, 200) != HAL_OK) {
        usb_app_cdc_printf("GET_INFO: RX failed from 0x%02X\r\n", slave_addr);
        return false;
    }

    if (rx_data[0] != CMD_GET_INFO) {
        usb_app_cdc_printf("GET_INFO: Bad response header %02X\r\n", rx_data[0]);
        return false;
    }

    uint8_t status = rx_data[expected_len - 1u];
    if (status != STATUS_OK) {
        usb_app_cdc_printf("GET_INFO: Status %02X from 0x%02X\r\n", status, slave_addr);
        return false;
    }

    memcpy(info, &rx_data[1], sizeof(device_info_t));
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

        case CMD_GET_LAYOUT_INFO:
            handle_get_layout_info(&tx_packet);
            break;

        case CMD_SET_LAYER_STATE:
            handle_set_layer_state(packet, &tx_packet);
            break;

        case CMD_GET_LAYER_STATE:
            handle_get_layer_state(&tx_packet);
            break;

        case CMD_GET_LAYOUT_CELL_TYPE:
            handle_get_layout_cell_type(&rx_packet, &tx_packet);
            break;

        case CMD_GET_LAYOUT_CELL_COMPONENT_ID:
            handle_get_layout_cell_component_id(&rx_packet, &tx_packet);
            break;

        case CMD_GET_SLIDER_VALUE:
            handle_get_slider_value(&rx_packet, &tx_packet);
            break;

        case CMD_GET_SLIDER_CONFIG:
            handle_get_slider_config(&rx_packet, &tx_packet);
            break;

        case CMD_SET_SLIDER_CONFIG:
            handle_set_slider_config(&rx_packet, &tx_packet);
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
                bool all_slaves_ok = true;

                if (i2c_manager_get_mode() == 1) {
                    extern uint8_t detected_slaves[I2C_MAX_SLAVE_COUNT];
                    extern uint8_t detected_slave_count;

                    i2c_manager_scan_slaves();

                    for (uint8_t i = 0; i < detected_slave_count; i++) {
                        uint8_t slave_addr = detected_slaves[i];
                        if (!send_save_to_slave(slave_addr)) {
                            all_slaves_ok = false;
                            usb_app_cdc_printf("Config: Failed to save slave 0x%02X\r\n", slave_addr);
                        } else {
                            usb_app_cdc_printf("Config: Saved slave 0x%02X to EEPROM\r\n", slave_addr);
                        }
                    }
                }

                tx_packet.status = all_slaves_ok ? STATUS_OK : STATUS_ERROR;
                usb_app_cdc_printf("Config: Configuration force-saved to EEPROM%s\r\n",
                                    all_slaves_ok ? " (including slaves)" : " (slave save failures)");
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

    uint8_t device_type = i2c_manager_get_mode();
    uint8_t i2c_devices = (device_type == 1u) ? detected_slave_count : 0u;

    device_info_populate(info, device_type, i2c_devices);

    response->payload_length = sizeof(device_info_t);

    usb_app_cdc_printf("Config: Device info - Type:%d, Matrix:%dx%d, Encoders:%d, I2C:%d (%s)\r\n",
                 info->device_type, info->matrix_rows, info->matrix_cols, info->encoder_count, info->i2c_devices, info->device_name);
}

static void handle_get_keymap(const config_packet_t *request, config_packet_t *response)
{
    if (request->payload_length < 3) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }

    uint8_t layer = request->payload[0];
    uint8_t row = request->payload[1];
    uint8_t col = request->payload[2];

    if (layer >= KEYMAP_LAYER_COUNT || row >= MATRIX_ROWS || col >= MATRIX_COLS) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }

    keymap_entry_t *entry = (keymap_entry_t*)response->payload;
    entry->layer = layer;
    entry->row = row;
    entry->col = col;
    entry->keycode = keymap_get_keycode(layer, row, col);

    response->payload_length = sizeof(keymap_entry_t);
}

static void handle_set_keymap(const config_packet_t *request, config_packet_t *response)
{
    if (request->payload_length < sizeof(keymap_entry_t)) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }
    
    const keymap_entry_t *entry = (const keymap_entry_t*)request->payload;

    if (entry->layer >= KEYMAP_LAYER_COUNT || entry->row >= MATRIX_ROWS || entry->col >= MATRIX_COLS) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }
    
    // Set keycode in EEPROM
    if (keymap_set_keycode(entry->layer, entry->row, entry->col, entry->keycode)) {
        // Save configuration to EEPROM immediately for consistency
        if (eeprom_save_config()) {
            response->status = STATUS_OK;
            usb_app_cdc_printf("Config: Set keycode R%dC%d L%d = 0x%04X (saved to EEPROM)\r\n", 
                         entry->row, entry->col, entry->layer, entry->keycode);
        } else {
            response->status = STATUS_ERROR;
            usb_app_cdc_printf("Config: Failed to save keycode to EEPROM\r\n");
        }
    } else {
        response->status = STATUS_ERROR;
    }
    
    usb_app_cdc_printf("Config: Set keymap [%d,%d] = 0x%04X\r\n", 
                 entry->layer, entry->row, entry->col, entry->keycode);
}

static void handle_get_encoder_map(const config_packet_t *request, config_packet_t *response)
{
    if (request->payload_length < 2) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }

    uint8_t layer = request->payload[0];
    uint8_t encoder_id = request->payload[1];

    if (layer >= KEYMAP_LAYER_COUNT || encoder_id >= ENCODER_COUNT) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }

    encoder_entry_t *entry = (encoder_entry_t*)response->payload;
    entry->layer = layer;
    entry->encoder_id = encoder_id;

    // Get encoder mapping from EEPROM or default (use local variables to avoid packed member warnings)
    uint16_t ccw_keycode, cw_keycode;
    if (!keymap_get_encoder_map(layer, encoder_id, &ccw_keycode, &cw_keycode)) {
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
    
    if (entry->layer >= KEYMAP_LAYER_COUNT || entry->encoder_id >= ENCODER_COUNT) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }
    
    // Set encoder mapping in EEPROM
    if (keymap_set_encoder_map(entry->layer, entry->encoder_id, entry->ccw_keycode, entry->cw_keycode)) {
        // Save configuration to EEPROM immediately for consistency
        if (eeprom_save_config()) {
            response->status = STATUS_OK;
            usb_app_cdc_printf("Config: Set encoder E%d L%d = CCW:0x%04X CW:0x%04X (saved to EEPROM)\r\n", 
                         entry->encoder_id, entry->layer, entry->ccw_keycode, entry->cw_keycode);
        } else {
            response->status = STATUS_ERROR;
            usb_app_cdc_printf("Config: Failed to save encoder to EEPROM\r\n");
        }
    } else {
        response->status = STATUS_ERROR;
    }
    
    usb_app_cdc_printf("Config: Set encoder %d = CCW:0x%04X CW:0x%04X\r\n", 
                 entry->encoder_id, entry->ccw_keycode, entry->cw_keycode);
}

static void handle_get_i2c_devices(config_packet_t *response)
{
    extern uint8_t detected_slaves[I2C_MAX_SLAVE_COUNT];
    extern uint8_t detected_slave_count;
    
    // Force a scan of I2C devices to pick up newly attached modules
    i2c_manager_scan_slaves_force();

    // First byte of payload is the count of detected slaves
    response->payload[0] = detected_slave_count;

    // Copy detected slave info into the payload
    for (int i = 0; i < detected_slave_count; i++) {
        i2c_device_info_t device_info;
        memset(&device_info, 0, sizeof(device_info));

        uint8_t address = detected_slaves[i];
        device_info.address = address;
        device_info.status = 1; // Online

        device_info_t slave_info;
        memset(&slave_info, 0, sizeof(slave_info));

        if (request_device_info_from_slave(address, &slave_info)) {
            device_info.device_type = slave_info.device_type;
            device_info.firmware_version_major = slave_info.firmware_version_major;
            device_info.firmware_version_minor = slave_info.firmware_version_minor;
            device_info.firmware_version_patch = slave_info.firmware_version_patch;

            strncpy(device_info.name, slave_info.device_name, sizeof(device_info.name) - 1u);
            device_info.name[sizeof(device_info.name) - 1u] = '\0';
        } else {
            device_info.device_type = 0;
            snprintf(device_info.name, sizeof(device_info.name), "Slave %02X", address);
        }

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

static void handle_get_layout_info(config_packet_t *response)
{
    const size_t layout_size = sizeof(board_layout_info_t);
    if (layout_size > CONFIG_MAX_PAYLOAD_SIZE) {
        response->status = STATUS_ERROR;
        response->payload_length = 0;
        usb_app_cdc_printf("Config: Layout info too large (%u bytes)\r\n", (unsigned int)layout_size);
        return;
    }

    for (size_t i = 0; i < layout_size; i++) {
        response->payload[i] = ((const uint8_t*)&board_layout_info)[i];
    }

    response->payload_length = layout_size;
    usb_app_cdc_printf("Config: Provided layout info (v%u)\r\n", board_layout_info.version);
}

static void handle_get_layout_cell_type(const config_packet_t *request, config_packet_t *response)
{
    if (request->payload_length < 2) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }

    uint8_t row = request->payload[0];
    uint8_t col = request->payload[1];

    layout_cell_type_t cell_type = get_layout_cell_type(row, col);
    
    response->payload[0] = (uint8_t)cell_type;
    response->payload_length = 1;
    response->status = STATUS_OK;
    
    usb_app_cdc_printf("Config: Layout cell type at (%u,%u) = %u\r\n", row, col, (uint8_t)cell_type);
}

static void handle_get_layout_cell_component_id(const config_packet_t *request, config_packet_t *response)
{
    if (request->payload_length < 2) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }

    uint8_t row = request->payload[0];
    uint8_t col = request->payload[1];

    uint8_t component_id = get_layout_cell_component_id(row, col);
    
    response->payload[0] = component_id;
    response->payload_length = 1;
    response->status = STATUS_OK;
    
    usb_app_cdc_printf("Config: Layout component ID at (%u,%u) = %u\r\n", row, col, component_id);
}

static void handle_set_layer_state(const config_packet_t *request, config_packet_t *response)
{
    if (request->payload_length < 2) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }

    uint8_t mask = request->payload[0];
    uint8_t default_layer = request->payload[1];
    uint8_t current_default = keymap_get_default_layer();
    bool update_default = false;
    bool persist_default = false;

    if (request->payload_length >= 3) {
        uint8_t options = request->payload[2];
        update_default = (options & 0x01u) != 0u;
        persist_default = (options & 0x02u) != 0u;
    } else {
        if (default_layer < KEYMAP_LAYER_COUNT && default_layer != current_default) {
            update_default = true;
        }
    }

    keymap_apply_layer_mask(mask, default_layer, true, update_default);

    if (update_default && persist_default) {
        keymap_persist_default_layer_state();
    }

    response->payload[0] = keymap_get_layer_mask();
    response->payload[1] = keymap_get_default_layer();
    response->payload_length = 2;
    response->status = STATUS_OK;

    usb_app_cdc_printf("Config: Set layer state mask=0x%02X default=%d\r\n",
                       response->payload[0], response->payload[1]);
}

static void handle_get_layer_state(config_packet_t *response)
{
    response->payload[0] = keymap_get_layer_mask();
    response->payload[1] = keymap_get_default_layer();
    response->payload_length = 2;
    response->status = STATUS_OK;
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

static HAL_StatusTypeDef i2c_master_receive_with_retry(uint8_t slave_addr, uint8_t *buffer, uint16_t length, uint32_t timeout)
{
    const uint8_t max_attempts = 5;
    HAL_StatusTypeDef status = HAL_ERROR;

    for (uint8_t attempt = 0; attempt < max_attempts; attempt++) {
        status = HAL_I2C_Master_Receive(&hi2c2, slave_addr << 1, buffer, length, timeout);
        if (status == HAL_OK) {
            return HAL_OK;
        }

        if (attempt + 1u < max_attempts) {
            HAL_Delay(1);
        }
    }

    return status;
}

// Slider protocol handlers
static void handle_get_slider_value(const config_packet_t *request, config_packet_t *response)
{
    if (request->payload_length < 1) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }

    uint8_t slider_id = request->payload[0];

#if SLIDER_COUNT > 0
    if (slider_id >= SLIDER_COUNT) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }

    // Use raw value for polling - gives immediate ADC reading with current layer config
    response->payload[0] = slider_get_current_raw_value(slider_id);
    response->payload_length = 1;
    response->status = STATUS_OK;
#else
    response->status = STATUS_NOT_SUPPORTED;
#endif
}

static void handle_get_slider_config(const config_packet_t *request, config_packet_t *response)
{
    if (request->payload_length < 2) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }

    uint8_t layer = request->payload[0];
    uint8_t slider_id = request->payload[1];

#if SLIDER_COUNT > 0
    if (layer >= KEYMAP_LAYER_COUNT) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }
    
    if (slider_id >= SLIDER_COUNT) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }

    // Get slider configuration for the specified layer
    slider_config_t *response_config = (slider_config_t*)response->payload;
    
    if (keymap_get_slider_config(layer, slider_id, response_config)) {
        response_config->layer = layer;
        response_config->slider_id = slider_id;
        response->payload_length = sizeof(slider_config_t);
        response->status = STATUS_OK;
    } else {
        response->status = STATUS_ERROR;
    }
#else
    response->status = STATUS_NOT_SUPPORTED;
#endif
}

static void handle_set_slider_config(const config_packet_t *request, config_packet_t *response)
{
    if (request->payload_length < sizeof(slider_config_t)) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }

#if SLIDER_COUNT > 0
    slider_config_t *config = (slider_config_t*)request->payload;
    
    if (config->layer >= KEYMAP_LAYER_COUNT) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }
    
    if (config->slider_id >= SLIDER_COUNT) {
        response->status = STATUS_INVALID_PARAM;
        return;
    }

    // Set slider configuration for the specified layer
    if (keymap_set_slider_config(config->layer, config->slider_id, config)) {
        // Save configuration to EEPROM immediately for simplicity
        if (eeprom_save_config()) {
            response->status = STATUS_OK;
            usb_app_cdc_printf("Config: Set slider %d layer %d config - CC%d Ch%d Range%d-%d (saved to EEPROM)\r\n", 
                         config->slider_id, config->layer, config->midi_cc, config->midi_channel,
                         config->min_midi_value, config->max_midi_value);
        } else {
            response->status = STATUS_ERROR;
            usb_app_cdc_printf("Config: Failed to save slider config to EEPROM\r\n");
        }
    } else {
        response->status = STATUS_ERROR;
    }
#else
    response->status = STATUS_NOT_SUPPORTED;
#endif
}