#if 0
#include "i2c_manager.h"
#include "i2c.h"
#include "i2c_protocol.h"
#include "uart_debug.h"
#include "key_state.h"
#include "midi_handler.h"
#include "ws2812.h"
#include "main.h"
#include <string.h>
#include "usb_app.h"
#include "input/keymap.h"
#include "config_protocol.h"
#include "eeprom_emulation.h"
#include "device_info_util.h"
/* Private constants */
#define I2C_SLAVE_ADDRESS 0x42  // 7-bit address for slave mode
#define I2C_BOOTSTRAP_ADDRESS 0x7F  // Temporary address for unassigned slaves
#define I2C_EVENT_FIFO_SIZE 16
#define I2C_TAP_TIMEOUT_MS 100
#define LAYER_SYNC_SUPPRESS_MS 50u

/* Private types */
typedef enum { 
    I2C_TAP_IDLE = 0, 
    I2C_TAP_SEND_PRESS, 
    I2C_TAP_WAIT_GAP, 
    I2C_TAP_SEND_RELEASE 
} i2c_tap_state_t;

typedef enum {
    I2C_SLAVE_STATE_READY,
    I2C_SLAVE_STATE_BUSY
} i2c_slave_state_t;

/* Private variables */
extern I2C_HandleTypeDef hi2c2;

static uint8_t current_i2c_mode = 0xFF; // 0xFF = uninitialized, 0 = slave, 1 = master
static uint32_t last_slave_scan = 0;
#define SLAVE_SCAN_INTERVAL_MS 500
uint8_t detected_slaves[I2C_MAX_SLAVE_COUNT]; // Track detected slave addresses
uint8_t detected_slave_count = 0;

static const uint8_t i2c_fixed_addresses[I2C_MAX_SLAVE_COUNT] = {
    I2C_SLAVE_ADDRESS_BASE + 0,
    I2C_SLAVE_ADDRESS_BASE + 1,
    I2C_SLAVE_ADDRESS_BASE + 2,
    I2C_SLAVE_ADDRESS_BASE + 3
};

// Determine slave address - use fixed address if defined, otherwise use base address
#ifdef FIXED_SLAVE_ADDRESS
static uint8_t my_slave_address = FIXED_SLAVE_ADDRESS;
#else
static uint8_t my_slave_address = I2C_SLAVE_ADDRESS_BASE;  // Default to base address
#endif

/* I2C Event FIFO Queue for handling multiple simultaneous key events */
static i2c_message_t i2c_event_fifo[I2C_EVENT_FIFO_SIZE];
static volatile uint8_t i2c_fifo_head = 0;
static volatile uint8_t i2c_fifo_tail = 0;
static volatile uint8_t i2c_fifo_count = 0;

/* Master-side event FIFO to preserve ordering of slave messages */
static i2c_key_event_t i2c_master_event_fifo[I2C_EVENT_FIFO_SIZE];
static volatile uint8_t i2c_master_fifo_head = 0;
static volatile uint8_t i2c_master_fifo_tail = 0;
static volatile uint8_t i2c_master_fifo_count = 0;

/* Track when a fresh HID report needs to be sent to the host */
static volatile i2c_slave_state_t i2c_slave_state = I2C_SLAVE_STATE_READY;

/* I2C encoder state machine (mirrors USB HID encoder logic for slave mode) */
static i2c_tap_state_t i2c_tap_state = I2C_TAP_IDLE;
static uint8_t i2c_tap_encoder_idx = 0;
static uint8_t i2c_tap_keycode = 0;
static uint32_t i2c_tap_t_release_earliest = 0;
static uint32_t i2c_tap_timeout = 0;

/* I2C communication buffers */
static i2c_message_t i2c_tx_buffer = { .key_event = { I2C_MSG_HEADER, 0xFF, 0, 0, 0, 0, 0, 0 } };
static i2c_message_t i2c_rx_buffer = {0};

/* I2C slave receive buffer and callback handling */
static uint8_t i2c_slave_rx_buffer[32];
static volatile uint8_t i2c_slave_data_received = 0;

/* Simple response buffer for configuration commands (separate from event FIFO) */
static uint8_t i2c_slave_config_response[I2C_SLAVE_CONFIG_MAX_RESPONSE] = {0};
static volatile uint8_t i2c_slave_config_response_length = 0;
static volatile uint8_t i2c_slave_has_config_response = 0;
static uint32_t last_layer_broadcast_tick = 0;

/* Private function prototypes */
static uint8_t i2c_fifo_is_full(void);
static uint8_t i2c_fifo_is_empty(void);
static uint8_t i2c_fifo_push(const i2c_message_t *message);
static uint8_t i2c_fifo_pop(i2c_message_t *message);
static uint8_t i2c_master_fifo_is_full(void);
static uint8_t i2c_master_fifo_is_empty(void);
static uint8_t i2c_master_fifo_push(const i2c_key_event_t *event);
static uint8_t i2c_master_fifo_pop(i2c_key_event_t *event);
static void init_i2c_tx_buffer(void);
static void configure_i2c_master(void);
static void configure_i2c_slave(void);
static void process_slave_key_event(const i2c_key_event_t *event);
static void process_slave_midi_event(const i2c_midi_event_t *event);
static void process_slave_layer_state(const i2c_layer_state_t *event);
static uint8_t first_active_layer(uint8_t mask);
static void process_i2c_encoder_state_machine(void);
static void process_master_event_queue(void);
static void queue_midi_event(uint8_t event_type, uint8_t channel, uint8_t data1, uint8_t data2);

/* I2C Event FIFO Management Functions */
static uint8_t i2c_fifo_is_full(void)
{
    return i2c_fifo_count >= I2C_EVENT_FIFO_SIZE;
}

static uint8_t i2c_fifo_is_empty(void)
{
    return i2c_fifo_count == 0;
}

static uint8_t i2c_fifo_push(const i2c_message_t *message)
{
    if (i2c_fifo_is_full()) {
        usb_app_cdc_printf("I2C FIFO: OVERFLOW! Dropping event\r\n");
        return 0; // FIFO full
    }
    
    // Copy message to FIFO
    i2c_event_fifo[i2c_fifo_head] = *message;
    i2c_fifo_head = (i2c_fifo_head + 1) % I2C_EVENT_FIFO_SIZE;
    i2c_fifo_count++;
    
    usb_app_cdc_printf("I2C FIFO: Pushed msg_type=0x%02X (count=%d)\r\n",
                 message->common.msg_type, i2c_fifo_count);
    return 1; // Success
}

static uint8_t i2c_fifo_pop(i2c_message_t *message)
{
    if (i2c_fifo_is_empty()) {
        return 0; // FIFO empty
    }
    
    // Copy message from FIFO
    *message = i2c_event_fifo[i2c_fifo_tail];
    i2c_fifo_tail = (i2c_fifo_tail + 1) % I2C_EVENT_FIFO_SIZE;
    i2c_fifo_count--;
    
    usb_app_cdc_printf("I2C FIFO: Popped msg_type=0x%02X (count=%d)\r\n",
                 message->common.msg_type, i2c_fifo_count);
    return 1; // Success
}

static uint8_t i2c_master_fifo_is_full(void)
{
    return i2c_master_fifo_count >= I2C_EVENT_FIFO_SIZE;
}

static uint8_t i2c_master_fifo_is_empty(void)
{
    return i2c_master_fifo_count == 0;
}

static uint8_t i2c_master_fifo_push(const i2c_key_event_t *event)
{
    if (i2c_master_fifo_is_full()) {
        usb_app_cdc_printf("I2C Master FIFO: OVERFLOW! Dropping event\r\n");
        return 0;
    }

    i2c_master_event_fifo[i2c_master_fifo_head] = *event;
    i2c_master_fifo_head = (i2c_master_fifo_head + 1) % I2C_EVENT_FIFO_SIZE;
    i2c_master_fifo_count++;

    return 1;
}

static uint8_t i2c_master_fifo_pop(i2c_key_event_t *event)
{
    if (i2c_master_fifo_is_empty()) {
        return 0;
    }

    *event = i2c_master_event_fifo[i2c_master_fifo_tail];
    i2c_master_fifo_tail = (i2c_master_fifo_tail + 1) % I2C_EVENT_FIFO_SIZE;
    i2c_master_fifo_count--;
    return 1;
}

/* Identify which HAL error codes indicate the bus needs a full reset */
/* Force all devices at an address to switch to bootstrap mode (currently unused) */
/*
static void force_address_to_bootstrap(uint8_t address)
{
    if (current_i2c_mode != 1) {
        return;  // Only master can do this
    }

    usb_app_cdc_printf("I2C: Forcing all devices at 0x%02X to bootstrap mode\r\n", address);
    
    // Send bootstrap command multiple times to catch all devices
    for (uint8_t attempt = 0; attempt < 3; attempt++) {
        uint8_t tx_data[I2C_SLAVE_CONFIG_CMD_SIZE] = {CMD_GOTO_BOOTSTRAP, 0, 0, 0, 0, 0, 0};
        HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(&hi2c2, address << 1, tx_data, sizeof(tx_data), 100);
        
        if (status == HAL_OK) {
            HAL_Delay(5);
            // Try to receive response (may fail if multiple devices respond)
            uint8_t rx_data[2] = {0};
            HAL_I2C_Master_Receive(&hi2c2, address << 1, rx_data, sizeof(rx_data), 100);
        }
        
        HAL_Delay(50); // Wait between attempts
    }
    
    HAL_Delay(200); // Give devices time to reconfigure
}
*/

/* Initialize I2C tx buffer with valid checksum */
static void init_i2c_tx_buffer(void)
{
    i2c_tx_buffer.key_event.header = I2C_MSG_HEADER;
    i2c_tx_buffer.key_event.msg_type = 0xFF;
    i2c_tx_buffer.key_event.row = 0;
    i2c_tx_buffer.key_event.col = 0;
    i2c_tx_buffer.key_event.pressed = 0;
    i2c_tx_buffer.key_event.keycode = 0;
    i2c_tx_buffer.key_event.layer_mask = keymap_get_layer_mask();
    i2c_tx_buffer.key_event.checksum = i2c_calc_checksum(&i2c_tx_buffer.key_event);
}

/* Configure I2C2 as master (for when connected to USB host) */
static void configure_i2c_master(void)
{
    /* Always re-initialise to guarantee a clean slate */
    if (HAL_I2C_GetState(&hi2c2) != HAL_I2C_STATE_RESET) {
        HAL_I2C_DeInit(&hi2c2);
    }

    hi2c2.Instance = I2C2;
    hi2c2.Init.Timing = 0x00200409;  // 1 MHz Fast Mode Plus
    hi2c2.Init.OwnAddress1 = 0; // Master doesn't need own address
    hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c2.Init.OwnAddress2 = 0;
    hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    if (HAL_I2C_Init(&hi2c2) != HAL_OK) {
        Error_Handler();
    }

    usb_app_cdc_printf("I2C2 configured as MASTER\r\n");

    current_i2c_mode = 1; // Master mode
}

/* Configure I2C2 as slave (for when not connected to USB host) */
static void configure_i2c_slave(void)
{
    if (current_i2c_mode == 0) return; // Already slave
    
    // Deinit current I2C configuration
    HAL_I2C_DeInit(&hi2c2);
    
    // Configure as slave
    hi2c2.Instance = I2C2;
    hi2c2.Init.Timing = 0x00200409;  // 1 MHz Fast Mode Plus
    hi2c2.Init.OwnAddress1 = (my_slave_address << 1); // Shift for HAL format
    hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c2.Init.OwnAddress2 = 0;
    hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    
    if (HAL_I2C_Init(&hi2c2) != HAL_OK) {
        Error_Handler();
    }

    /**I2C Analogue filter 
    */
    if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
    {
        Error_Handler();
    }

    /** Configure Digital filter 
    */
    if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK)
    {
        Error_Handler();
    }
    
    usb_app_cdc_printf("I2C2 configured as SLAVE - Address: 0x%02X (HAL format: 0x%02X)\r\n", 
                 my_slave_address, (my_slave_address << 1));
    
    // Start listening for slave requests
    if (HAL_I2C_EnableListen_IT(&hi2c2) != HAL_OK) {
        // Listening Error
        Error_Handler();
    }
    
    current_i2c_mode = 0; // Slave mode
}

/* I2C master: process received key event from slave */
static void process_slave_key_event(const i2c_key_event_t *event)
{
    if (!i2c_validate_message(event)) {
        return; // Invalid message, ignore
    }

    uint8_t mask = event->layer_mask;
    uint8_t default_layer = keymap_get_default_layer();
    if (mask == 0u) {
        uint8_t default_bit = (uint8_t)(1u << default_layer);
        if (default_bit == 0u) {
            default_bit = 1u;
        }
        mask = default_bit;
    }

    keymap_apply_layer_mask(mask, default_layer, false, false);


    // Check if this is an encoder event (row 254) or regular matrix key
    if (event->row == 254) {
        const char *dir = event->pressed ? "CW" : "CCW";
        // Encoder event: col=encoder_idx, pressed encodes direction, keycode=HID key
        usb_app_cdc_printf("Master: Processing encoder event (%s), keycode=0x%02X\r\n", dir, event->keycode);
        key_state_send_encoder_event(event->keycode);
        return;
    }

    // Regular matrix key event from slave or locally queued master event
    usb_app_cdc_printf("Master: Processing matrix key: %s keycode=0x%02X\r\n",
                 event->pressed ? "press" : "release", event->keycode);

    if (event->pressed) {
        key_state_add_key(event->keycode);
    } else {
        key_state_remove_key(event->keycode);
    }
    key_state_update_hid_report();
}

/* I2C master: process received MIDI event from slave */
static void process_slave_midi_event(const i2c_midi_event_t *event)
{
    if (!i2c_validate_midi_message(event)) {
        return; // Invalid message, ignore
    }

    uint8_t display_channel = (uint8_t)(event->channel + 1U);

    switch (event->event_type) {
        case I2C_MIDI_EVENT_TYPE_CC:
            usb_app_cdc_printf("Master: MIDI CC ch%d ctrl=%d val=%d\r\n",
                     display_channel, event->data1, event->data2);
            midi_send_cc(event->channel, event->data1, event->data2);
            break;
        case I2C_MIDI_EVENT_TYPE_NOTE_ON:
            usb_app_cdc_printf("Master: MIDI Note ON ch%d note=%d vel=%d\r\n",
                     display_channel, event->data1, event->data2);
            midi_send_note_on(event->channel, event->data1, event->data2);
            break;
        case I2C_MIDI_EVENT_TYPE_NOTE_OFF:
            usb_app_cdc_printf("Master: MIDI Note OFF ch%d note=%d\r\n",
                     display_channel, event->data1);
            midi_send_note_off(event->channel, event->data1);
            break;
        default:
            usb_app_cdc_printf("Master: Unknown MIDI event type %d (ch=%d)\r\n",
                     event->event_type, display_channel);
            break;
    }
}

static uint8_t first_active_layer(uint8_t mask)
{
    if (mask == 0) {
        return 0;
    }

    for (uint8_t idx = 0; idx < KEYMAP_LAYER_COUNT; idx++) {
        if ((mask & (uint8_t)(1u << idx)) != 0) {
            return idx;
        }
    }

    return 0;
}

static void process_slave_layer_state(const i2c_layer_state_t *event)
{
    if (!i2c_validate_layer_message(event)) {
        return;
    }

    uint8_t mask = event->layer_mask;
    uint8_t default_layer = event->default_layer;

    if (current_i2c_mode == 1u) {
        uint32_t now = HAL_GetTick();
        if ((int32_t)(now - last_layer_broadcast_tick) < (int32_t)LAYER_SYNC_SUPPRESS_MS) {
            return;
        }

        if (default_layer >= KEYMAP_LAYER_COUNT) {
            default_layer = first_active_layer(mask);
        }

        bool update_default = (default_layer < KEYMAP_LAYER_COUNT) && (default_layer != keymap_get_default_layer());
        keymap_apply_layer_mask(mask, default_layer, true, update_default);
        return;
    }

    if (default_layer >= KEYMAP_LAYER_COUNT) {
        default_layer = first_active_layer(mask);
    }

    bool update_default = (default_layer < KEYMAP_LAYER_COUNT) && (default_layer != keymap_get_default_layer());
    keymap_apply_layer_mask(mask, default_layer, false, update_default);
}

/* I2C master: burst poll to drain slave FIFO quickly */
/* Process I2C encoder state machine for slave mode */
static void process_i2c_encoder_state_machine(void)
{
    if (i2c_tap_state == I2C_TAP_IDLE) return;
    
    // Check for timeout to prevent getting stuck
    if ((int32_t)(HAL_GetTick() - i2c_tap_timeout) >= 0) {
        usb_app_cdc_printf("I2C: Timeout! Resetting state machine (was in state %d)\r\n", i2c_tap_state);
        i2c_tap_state = I2C_TAP_IDLE;
        i2c_tap_keycode = 0;
        return;
    }
    
    switch (i2c_tap_state) {
        case I2C_TAP_SEND_PRESS:
            // Send press event via I2C (row 254 = encoder events)
            usb_app_cdc_printf("I2C: Sending press event (enc=%d, key=0x%02X)\r\n", i2c_tap_encoder_idx, i2c_tap_keycode);
            i2c_manager_send_key_event(254, i2c_tap_encoder_idx, 1, i2c_tap_keycode);
            // For encoder events, minimal gap (1ms) since they're instantaneous rotations
            i2c_tap_t_release_earliest = HAL_GetTick() + 1; 
            i2c_tap_state = I2C_TAP_WAIT_GAP;
            usb_app_cdc_printf("I2C: Moved to WAIT_GAP, release_time=%lu\r\n", i2c_tap_t_release_earliest);
            break;
        case I2C_TAP_WAIT_GAP:
            if ((int32_t)(HAL_GetTick() - i2c_tap_t_release_earliest) >= 0) {
                usb_app_cdc_printf("I2C: Gap complete, moving to release\r\n");
                i2c_tap_state = I2C_TAP_SEND_RELEASE;
            }
            break;
        case I2C_TAP_SEND_RELEASE:
            // Send release event via I2C
            usb_app_cdc_printf("I2C: Sending release event\r\n");
            i2c_manager_send_key_event(254, i2c_tap_encoder_idx, 0, i2c_tap_keycode);
            i2c_tap_state = I2C_TAP_IDLE;
            i2c_tap_keycode = 0;
            usb_app_cdc_printf("I2C: Complete, returning to IDLE\r\n");
            break;
        default:
            usb_app_cdc_printf("I2C: Invalid state %d, resetting\r\n", i2c_tap_state);
            i2c_tap_state = I2C_TAP_IDLE;
            i2c_tap_keycode = 0;
            break;
    }
}

/* Public functions */

void i2c_manager_init(void)
{
    // Initialize I2C tx buffer with valid checksum
    init_i2c_tx_buffer();
    
    // Initialize FIFO
    i2c_fifo_head = 0;
    i2c_fifo_tail = 0;
    i2c_fifo_count = 0;
    i2c_master_fifo_head = 0;
    i2c_master_fifo_tail = 0;
    i2c_master_fifo_count = 0;
    
    // Initialize state machine
    i2c_tap_state = I2C_TAP_IDLE;
    i2c_tap_keycode = 0;
    i2c_slave_state = I2C_SLAVE_STATE_READY;

    // Initialize slave tracking
    memset(detected_slaves, 0, sizeof(detected_slaves));
    detected_slave_count = 0;
    
    usb_app_cdc_printf("I2C Manager initialized\r\n");
}

void i2c_manager_set_mode(uint8_t is_master)
{
    if (is_master) {
        configure_i2c_master();
        usb_app_cdc_printf("I2C configured as master\r\n");
    } else {
        configure_i2c_slave();
        usb_app_cdc_printf("I2C configured as slave\r\n");
    }
}

uint8_t i2c_manager_get_mode(void)
{
    return current_i2c_mode;
}

/* I2C master: scan for available slaves */
static void i2c_manager_scan_slaves_internal(bool force)
{
    if (current_i2c_mode != 1) {
        return;
    }

    uint32_t now = HAL_GetTick();
    if (!force && (now - last_slave_scan) < SLAVE_SCAN_INTERVAL_MS) {
        return;
    }

    last_slave_scan = now;

    uint8_t new_detected[I2C_MAX_SLAVE_COUNT] = {0};
    uint8_t new_count = 0;

    for (uint8_t idx = 0; idx < I2C_MAX_SLAVE_COUNT; idx++) {
        uint8_t addr = i2c_fixed_addresses[idx];
        
        // Check if peripheral is in a bad state and reset if needed
        HAL_I2C_StateTypeDef i2c_state = HAL_I2C_GetState(&hi2c2);
        if (i2c_state == HAL_I2C_STATE_BUSY || i2c_state == HAL_I2C_STATE_BUSY_TX || i2c_state == HAL_I2C_STATE_BUSY_RX) {
            usb_app_cdc_printf("I2C: Peripheral stuck in BUSY state 0x%02X, reinitializing\r\n", i2c_state);
            HAL_I2C_DeInit(&hi2c2);
            configure_i2c_master();
        }
        
        HAL_StatusTypeDef status = HAL_I2C_IsDeviceReady(&hi2c2, (uint16_t)(addr << 1), 3, 15);
        uint32_t error_code = HAL_I2C_GetError(&hi2c2);

        if (status == HAL_OK) {
            new_detected[new_count++] = addr;
        } else if (status != HAL_ERROR || error_code != HAL_I2C_ERROR_AF) {
            // Log non-NACK failures for debugging
            usb_app_cdc_printf("I2C: Probe 0x%02X status=%d error=0x%08lX\r\n", addr, status, error_code);
        }
    }

    bool changed = (new_count != detected_slave_count) ||
                   (memcmp(detected_slaves, new_detected, sizeof(new_detected)) != 0);

    if (changed) {
        memcpy(detected_slaves, new_detected, sizeof(new_detected));
        detected_slave_count = new_count;

        if (new_count == 0) {
            usb_app_cdc_printf("I2C: No slaves detected\r\n");
        } else {
            usb_app_cdc_printf("I2C: Found %d slave(s)\r\n", detected_slave_count);
        }
    }
}

void i2c_manager_scan_slaves(void)
{
    i2c_manager_scan_slaves_internal(false);
}

void i2c_manager_scan_slaves_force(void)
{
    i2c_manager_scan_slaves_internal(true);
}

void i2c_manager_task(void)
{
    if (current_i2c_mode == 0) { 
        // Slave mode - process encoder state machine
        process_i2c_encoder_state_machine();
    } else if (current_i2c_mode == 1) {
        i2c_manager_scan_slaves();
        // Master mode - poll slave, drain queue, and update HID state
        i2c_manager_poll_slaves();
        process_master_event_queue();
        key_state_task();
    }
}

/* I2C slave: send key event to master */
void i2c_manager_send_key_event(uint8_t row, uint8_t col, uint8_t pressed, uint8_t keycode)
{
    i2c_message_t message = {0};
    message.key_event.header = I2C_MSG_HEADER;
    message.key_event.msg_type = I2C_MSG_KEY_EVENT;
    message.key_event.row = row;
    message.key_event.col = col;
    message.key_event.pressed = pressed;
    message.key_event.keycode = keycode;
    message.key_event.layer_mask = keymap_get_layer_mask();
    message.key_event.checksum = i2c_calc_checksum(&message.key_event);

    // Add to FIFO queue for transmission
    if (!i2c_fifo_push(&message)) {
        usb_app_cdc_printf("Failed to queue I2C event: row=%d, col=%d, pressed=%d\r\n", row, col, pressed);
    }
}

void i2c_manager_send_layer_state(uint8_t layer_mask, uint8_t default_layer)
{
    i2c_message_t message = {0};
    message.layer_state.header = I2C_MSG_HEADER;
    message.layer_state.msg_type = I2C_MSG_LAYER_STATE;
    message.layer_state.layer_mask = layer_mask;
    message.layer_state.default_layer = default_layer;
    message.layer_state.reserved0 = 0;
    message.layer_state.reserved1 = 0;
    message.layer_state.reserved2 = 0;
    message.layer_state.checksum = i2c_calc_layer_checksum(&message.layer_state);

    if (!i2c_fifo_push(&message)) {
        usb_app_cdc_printf("Failed to queue I2C layer state: mask=0x%02X default=%d\r\n", layer_mask, default_layer);
    }
}

void i2c_manager_broadcast_layer_state(uint8_t layer_mask, uint8_t default_layer)
{
    if (current_i2c_mode == 0xFF) {
        return;
    }

    if (current_i2c_mode == 0) {
        // In slave mode, queue update for the master
        i2c_manager_send_layer_state(layer_mask, default_layer);
        return;
    }

    if (detected_slave_count == 0) {
        i2c_manager_scan_slaves();
    }

    if (detected_slave_count == 0) {
        return;
    }

    uint8_t tx_data[I2C_SLAVE_CONFIG_CMD_SIZE] = {
        CMD_SET_LAYER_STATE,
        layer_mask,
        default_layer,
        0,
        0,
        0,
        0
    };
    uint8_t rx_data[2] = {0};
    bool any_sent = false;

    for (uint8_t idx = 0; idx < detected_slave_count; idx++) {
        uint8_t address = detected_slaves[idx];
        if (address == 0) {
            continue;
        }

        if (HAL_I2C_Master_Transmit(&hi2c2, address << 1, tx_data, sizeof(tx_data), 100) != HAL_OK) {
            usb_app_cdc_printf("LAYER_STATE: TX failed to 0x%02X\r\n", address);
            continue;
        }

        HAL_Delay(5);

        if (HAL_I2C_Master_Receive(&hi2c2, address << 1, rx_data, sizeof(rx_data), 100) != HAL_OK) {
            usb_app_cdc_printf("LAYER_STATE: RX failed from 0x%02X\r\n", address);
            continue;
        }

        if (rx_data[0] != CMD_SET_LAYER_STATE || rx_data[1] != STATUS_OK) {
            usb_app_cdc_printf("LAYER_STATE: Bad response from 0x%02X [%02X %02X]\r\n",
                               address, rx_data[0], rx_data[1]);
            continue;
        }

        any_sent = true;
    }

    if (any_sent) {
        last_layer_broadcast_tick = HAL_GetTick();
    }
}

static void queue_midi_event(uint8_t event_type, uint8_t channel, uint8_t data1, uint8_t data2)
{
    i2c_message_t message = {0};
    message.midi_event.header = I2C_MSG_HEADER;
    message.midi_event.msg_type = I2C_MSG_MIDI_EVENT;
    message.midi_event.event_type = event_type;
    message.midi_event.channel = channel;
    message.midi_event.data1 = data1;
    message.midi_event.data2 = data2;
    message.midi_event.reserved = 0;
    message.midi_event.checksum = i2c_calc_midi_checksum(&message.midi_event);

    if (!i2c_fifo_push(&message)) {
        usb_app_cdc_printf("Failed to queue I2C MIDI event: type=%d, ch=%d, data1=%d, data2=%d\r\n",
                 event_type, channel + 1U, data1, data2);
    }
}

/* Send MIDI CC event to master via I2C (for slave mode only) */
void i2c_manager_send_midi_cc(uint8_t channel, uint8_t controller, uint8_t value)
{
    queue_midi_event(I2C_MIDI_EVENT_TYPE_CC, channel, controller, value);
}

/* Send MIDI note event to master via I2C (for slave mode only) */
void i2c_manager_send_midi_note(uint8_t channel, uint8_t note, uint8_t velocity, bool pressed)
{
    uint8_t event_type = pressed ? I2C_MIDI_EVENT_TYPE_NOTE_ON : I2C_MIDI_EVENT_TYPE_NOTE_OFF;
    queue_midi_event(event_type, channel, note, velocity);
}

/* I2C master: poll slaves for key events */
void i2c_manager_poll_slaves(void)
{
    if (current_i2c_mode != 1) {
        return;
    }

    if (detected_slave_count == 0) {
        return;
    }

    const uint8_t max_events_per_slave = 4;
    const uint8_t overall_max_events = 16;
    uint8_t total_events = 0;

    for (uint8_t idx = 0; idx < detected_slave_count && total_events < overall_max_events; idx++) {
        uint8_t slave_addr = detected_slaves[idx];
        uint8_t events_for_slave = 0;

        while (events_for_slave < max_events_per_slave && total_events < overall_max_events) {
            i2c_rx_buffer = (i2c_message_t){0};

            HAL_StatusTypeDef status = HAL_I2C_Master_Receive(&hi2c2, (uint16_t)(slave_addr << 1), (uint8_t*)&i2c_rx_buffer, sizeof(i2c_message_t), 10);

            if (status != HAL_OK) {
                // Device is offline or busy; simply move on and try next cycle
                break;
            }

            if (i2c_rx_buffer.common.header != I2C_MSG_HEADER) {
                break;
            }

            if (i2c_rx_buffer.common.msg_type == I2C_MSG_KEY_EVENT) {
                if (!i2c_master_fifo_push(&i2c_rx_buffer.key_event)) {
                    usb_app_cdc_printf("Master FIFO full, dropping key event\r\n");
                }
            } else if (i2c_rx_buffer.common.msg_type == I2C_MSG_MIDI_EVENT) {
                process_slave_midi_event(&i2c_rx_buffer.midi_event);
            } else if (i2c_rx_buffer.common.msg_type == I2C_MSG_LAYER_STATE) {
                process_slave_layer_state(&i2c_rx_buffer.layer_state);
            } else {
                break;
            }

            events_for_slave++;
            total_events++;
        }
    }
}

void i2c_manager_process_local_key_event(uint8_t row, uint8_t col, uint8_t pressed, uint8_t keycode)
{
    if (keycode == 0) {
        return;
    }

    if (current_i2c_mode != 1U) {
        usb_app_cdc_printf("Slave mode: queueing key event for master via I2C (row=%d, col=%d, pressed=%d, keycode=0x%02X)\r\n",
                 row, col, pressed, keycode);
        i2c_manager_send_key_event(row, col, pressed, keycode);
        return;
    }

    i2c_key_event_t event = {
        .header = I2C_MSG_HEADER,
        .msg_type = I2C_MSG_KEY_EVENT,
        .row = row,
        .col = col,
        .pressed = pressed,
        .keycode = keycode,
        .layer_mask = keymap_get_layer_mask(),
        .checksum = 0
    };

    event.checksum = i2c_calc_checksum(&event);

    if (!i2c_master_fifo_push(&event)) {
        usb_app_cdc_printf("Master FIFO full, dropping local key event (row=%d, col=%d, pressed=%d)\r\n", row, col, pressed);
        return;
    }

    usb_app_cdc_printf("Master mode: enqueued local key event (row=%d, col=%d, pressed=%d, keycode=0x%02X)\r\n",
                 row, col, pressed, keycode);
}

static void process_master_event_queue(void)
{
    i2c_key_event_t event;
    uint8_t events_this_cycle = 0;

    while (i2c_master_fifo_pop(&event)) {
        process_slave_key_event(&event);
        events_this_cycle++;
    }

    if (events_this_cycle) {
        usb_app_cdc_printf("Master: processed %d queued events\r\n", events_this_cycle);
    }
}

/* Encoder callback for slave mode */
void i2c_manager_encoder_callback(uint8_t encoder_idx, uint8_t direction, uint8_t keycode)
{
    usb_app_cdc_printf("Encoder callback: idx=%d, dir=%d, keycode=0x%02X, i2c_state=%d\r\n", 
                 encoder_idx, direction, keycode, i2c_tap_state);
    
    // Debug: Flash white when encoder callback is triggered
    
    // Only start new sequence if not already processing one
    if (i2c_tap_state == I2C_TAP_IDLE) {
        i2c_tap_encoder_idx = encoder_idx;
        i2c_tap_keycode = keycode;
        i2c_tap_state = I2C_TAP_SEND_PRESS;
        i2c_tap_timeout = HAL_GetTick() + I2C_TAP_TIMEOUT_MS; // Set timeout
        usb_app_cdc_printf("Slave encoder: Starting I2C sequence\r\n");
    } else {
        usb_app_cdc_printf("Slave encoder: Dropping event, I2C busy (state=%d)\r\n", i2c_tap_state);
    }
}

/* I2C slave callbacks */
void i2c_manager_addr_callback(I2C_HandleTypeDef *hi2c, uint8_t TransferDirection, uint16_t AddrMatchCode)
{
    if (hi2c->Instance == I2C2) {
        if (TransferDirection == I2C_DIRECTION_TRANSMIT) {
            // Master is writing to us - prepare to receive configuration command bytes
            HAL_I2C_Slave_Seq_Receive_IT(hi2c, i2c_slave_rx_buffer, I2C_SLAVE_CONFIG_CMD_SIZE, I2C_FIRST_AND_LAST_FRAME);
        } else {
            // Master is reading from us
            // Check if we have a config response ready (takes priority over event FIFO)
            if (i2c_slave_has_config_response) {
                // Send config response
                uint8_t length = i2c_slave_config_response_length;
                if (length == 0 || length > I2C_SLAVE_CONFIG_MAX_RESPONSE) {
                    length = 1; // fail-safe to avoid zero-length transfers
                }

                usb_app_cdc_printf("SLAVE TX (%d bytes):", length);
                for (uint8_t idx = 0; idx < length; idx++) {
                    usb_app_cdc_printf(" %02X", i2c_slave_config_response[idx]);
                }
                usb_app_cdc_printf("\r\n");

                HAL_I2C_Slave_Seq_Transmit_IT(hi2c, i2c_slave_config_response, length, I2C_FIRST_AND_LAST_FRAME);
                i2c_slave_has_config_response = 0; // Clear flag
                i2c_slave_config_response_length = 0;
            }
            else if (i2c_slave_state == I2C_SLAVE_STATE_READY) {
                // We are ready, try to send an event from the FIFO
                if (i2c_fifo_pop(&i2c_tx_buffer)) {
                    // Event found, prepare for transmission
                    usb_app_cdc_printf("I2C: Sending event from FIFO\r\n");
                    i2c_slave_state = I2C_SLAVE_STATE_BUSY;
                    HAL_I2C_Slave_Seq_Transmit_IT(hi2c, (uint8_t*)&i2c_tx_buffer, sizeof(i2c_message_t), I2C_FIRST_AND_LAST_FRAME);
                } else {
                    // No events queued, send empty/invalid message
                    usb_app_cdc_printf("I2C: No events in FIFO, sending empty\r\n");
                    i2c_tx_buffer.key_event.header = I2C_MSG_HEADER;
                    i2c_tx_buffer.key_event.msg_type = 0xFF; // Invalid message type
                    i2c_tx_buffer.key_event.row = 0;
                    i2c_tx_buffer.key_event.col = 0;
                    i2c_tx_buffer.key_event.pressed = 0;
                    i2c_tx_buffer.key_event.keycode = 0;
                    i2c_tx_buffer.key_event.layer_mask = keymap_get_layer_mask();
                    i2c_tx_buffer.key_event.checksum = i2c_calc_checksum(&i2c_tx_buffer.key_event);
                    i2c_slave_state = I2C_SLAVE_STATE_BUSY; // Still need to wait for this TX to finish
                    HAL_I2C_Slave_Seq_Transmit_IT(hi2c, (uint8_t*)&i2c_tx_buffer, sizeof(i2c_message_t), I2C_FIRST_AND_LAST_FRAME);
                }
            } else {
                // We are busy with a previous transmission, send an empty message immediately
                // This tells the master we're not ready without consuming a real event
                usb_app_cdc_printf("I2C: Slave busy, sending empty\r\n");
                i2c_tx_buffer.key_event.header = I2C_MSG_HEADER;
                i2c_tx_buffer.key_event.msg_type = 0xFF;
                i2c_tx_buffer.key_event.row = 0;
                i2c_tx_buffer.key_event.col = 0;
                i2c_tx_buffer.key_event.pressed = 0;
                i2c_tx_buffer.key_event.keycode = 0;
                i2c_tx_buffer.key_event.layer_mask = keymap_get_layer_mask();
                i2c_tx_buffer.key_event.checksum = i2c_calc_checksum(&i2c_tx_buffer.key_event);
                // We don't set state to busy here because we want the ongoing transmission to complete normally
                HAL_I2C_Slave_Seq_Transmit_IT(hi2c, (uint8_t*)&i2c_tx_buffer, sizeof(i2c_message_t), I2C_FIRST_AND_LAST_FRAME);
            }
        }
    }
}

void i2c_manager_slave_rx_complete_callback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C2) {
        i2c_slave_data_received = 1;
        
        // Process received command
        if (i2c_slave_rx_buffer[0] == CMD_GET_INFO) {
            device_info_t info;
            memset(&info, 0, sizeof(info));
            uint8_t device_type = (current_i2c_mode == 1u) ? 1u : 0u;
            uint8_t i2c_devices = (device_type == 1u) ? detected_slave_count : 0u;
            device_info_populate(&info, device_type, i2c_devices);

            memset(i2c_slave_config_response, 0, sizeof(i2c_slave_config_response));
            i2c_slave_config_response[0] = CMD_GET_INFO;
            memcpy(&i2c_slave_config_response[1], &info, sizeof(info));
            i2c_slave_config_response[1 + sizeof(info)] = STATUS_OK;
            i2c_slave_config_response_length = (uint8_t)(1u + sizeof(info) + 1u);
            i2c_slave_has_config_response = 1;

            usb_app_cdc_printf("SLAVE RX: GET_INFO -> %s\r\n", info.device_name);
        }
        else if (i2c_slave_rx_buffer[0] == CMD_GET_KEYMAP) {
            uint8_t layer = i2c_slave_rx_buffer[1];
            uint8_t row = i2c_slave_rx_buffer[2];
            uint8_t col = i2c_slave_rx_buffer[3];
            uint16_t keycode = keymap_get_keycode(layer, row, col);

            memset(i2c_slave_config_response, 0, sizeof(i2c_slave_config_response));
            i2c_slave_config_response[0] = CMD_GET_KEYMAP;
            i2c_slave_config_response[1] = layer;
            i2c_slave_config_response[2] = row;
            i2c_slave_config_response[3] = col;
            i2c_slave_config_response[4] = keycode & 0xFF;
            i2c_slave_config_response[5] = (keycode >> 8) & 0xFF;
            i2c_slave_config_response[6] = STATUS_OK;
            i2c_slave_config_response_length = 7;
            i2c_slave_has_config_response = 1;

            usb_app_cdc_printf("SLAVE RX: GET[L%d,%d,%d] = 0x%04X\r\n", layer, row, col, keycode);
        }
        else if (i2c_slave_rx_buffer[0] == CMD_SET_KEYMAP) {
            uint8_t layer = i2c_slave_rx_buffer[1];
            uint8_t row = i2c_slave_rx_buffer[2];
            uint8_t col = i2c_slave_rx_buffer[3];
            uint16_t keycode = i2c_slave_rx_buffer[4] | (i2c_slave_rx_buffer[5] << 8);

            bool success = keymap_set_keycode(layer, row, col, keycode);

            memset(i2c_slave_config_response, 0, sizeof(i2c_slave_config_response));
            i2c_slave_config_response[0] = CMD_SET_KEYMAP;
            i2c_slave_config_response[1] = success ? STATUS_OK : STATUS_ERROR;
            i2c_slave_config_response_length = 2;
            i2c_slave_has_config_response = 1;

            usb_app_cdc_printf("SLAVE RX: SET[L%d,%d,%d] = 0x%04X %s\r\n",
                         layer, row, col, keycode, success ? "OK" : "ERR");
        }
        else if (i2c_slave_rx_buffer[0] == CMD_GET_ENCODER_MAP) {
            uint8_t layer = i2c_slave_rx_buffer[1];
            uint8_t encoder_id = i2c_slave_rx_buffer[2];
            uint16_t ccw_keycode = 0;
            uint16_t cw_keycode = 0;
            bool success = keymap_get_encoder_map(layer, encoder_id, &ccw_keycode, &cw_keycode);

            memset(i2c_slave_config_response, 0, sizeof(i2c_slave_config_response));
            i2c_slave_config_response[0] = CMD_GET_ENCODER_MAP;
            i2c_slave_config_response[1] = layer;
            i2c_slave_config_response[2] = encoder_id;
            if (success) {
                i2c_slave_config_response[3] = ccw_keycode & 0xFF;
                i2c_slave_config_response[4] = (ccw_keycode >> 8) & 0xFF;
                i2c_slave_config_response[5] = cw_keycode & 0xFF;
                i2c_slave_config_response[6] = (cw_keycode >> 8) & 0xFF;
                i2c_slave_config_response[7] = STATUS_OK;
            } else {
                i2c_slave_config_response[7] = STATUS_INVALID_PARAM;
            }
            i2c_slave_config_response_length = 8;
            i2c_slave_has_config_response = 1;
        }
        else if (i2c_slave_rx_buffer[0] == CMD_SET_ENCODER_MAP) {
            uint8_t layer = i2c_slave_rx_buffer[1];
            uint8_t encoder_id = i2c_slave_rx_buffer[2];
            uint16_t ccw_keycode = i2c_slave_rx_buffer[3] | (i2c_slave_rx_buffer[4] << 8);
            uint16_t cw_keycode = i2c_slave_rx_buffer[5] | (i2c_slave_rx_buffer[6] << 8);
            bool success = keymap_set_encoder_map(layer, encoder_id, ccw_keycode, cw_keycode);

            memset(i2c_slave_config_response, 0, sizeof(i2c_slave_config_response));
            i2c_slave_config_response[0] = CMD_SET_ENCODER_MAP;
            i2c_slave_config_response[1] = success ? STATUS_OK : STATUS_ERROR;
            i2c_slave_config_response_length = 2;
            i2c_slave_has_config_response = 1;
        }
        else if (i2c_slave_rx_buffer[0] == CMD_SET_LAYER_STATE) {
            uint8_t mask = i2c_slave_rx_buffer[1];
            uint8_t def_layer = i2c_slave_rx_buffer[2];
            bool update_default = (def_layer < KEYMAP_LAYER_COUNT) && (def_layer != keymap_get_default_layer());
            keymap_apply_layer_mask(mask, def_layer, false, update_default);

            memset(i2c_slave_config_response, 0, sizeof(i2c_slave_config_response));
            i2c_slave_config_response[0] = CMD_SET_LAYER_STATE;
            i2c_slave_config_response[1] = STATUS_OK;
            i2c_slave_config_response_length = 2;
            i2c_slave_has_config_response = 1;
            usb_app_cdc_printf("SLAVE RX: layer state updated mask=0x%02X default=%d\r\n", mask, def_layer);
        }
        else if (i2c_slave_rx_buffer[0] == CMD_GET_LAYER_STATE) {
            uint8_t mask = keymap_get_layer_mask();
            uint8_t def_layer = keymap_get_default_layer();

            memset(i2c_slave_config_response, 0, sizeof(i2c_slave_config_response));
            i2c_slave_config_response[0] = CMD_GET_LAYER_STATE;
            i2c_slave_config_response[1] = mask;
            i2c_slave_config_response[2] = def_layer;
            i2c_slave_config_response[6] = STATUS_OK;
            i2c_slave_config_response_length = 7;
            i2c_slave_has_config_response = 1;
        }
        else if (i2c_slave_rx_buffer[0] == CMD_SAVE_CONFIG) {
            bool success = eeprom_save_config();

            memset(i2c_slave_config_response, 0, sizeof(i2c_slave_config_response));
            i2c_slave_config_response[0] = CMD_SAVE_CONFIG;
            i2c_slave_config_response[1] = success ? STATUS_OK : STATUS_ERROR;
            i2c_slave_config_response_length = 2;
            i2c_slave_has_config_response = 1;
            usb_app_cdc_printf("SLAVE RX: SAVE_CONFIG %s\r\n", success ? "OK" : "ERR");
        }
        else if (i2c_slave_rx_buffer[0] == CMD_ASSIGN_I2C_ADDRESS) {
            uint8_t new_address = i2c_slave_rx_buffer[1];
            
            memset(i2c_slave_config_response, 0, sizeof(i2c_slave_config_response));
            i2c_slave_config_response[0] = CMD_ASSIGN_I2C_ADDRESS;
            
            if (new_address >= I2C_SLAVE_ADDRESS_BASE && new_address < (I2C_SLAVE_ADDRESS_BASE + I2C_MAX_SLAVE_COUNT)) {
                my_slave_address = new_address;
                i2c_slave_config_response[1] = STATUS_OK;
                usb_app_cdc_printf("SLAVE RX: Address assigned to 0x%02X, will reconfigure\r\n", new_address);
                
                // Schedule reconfiguration after this transaction completes
                // Note: We'll reconfigure in the tx complete callback
            } else {
                i2c_slave_config_response[1] = STATUS_INVALID_PARAM;
                usb_app_cdc_printf("SLAVE RX: Invalid address 0x%02X\r\n", new_address);
            }
            
            i2c_slave_config_response_length = 2;
            i2c_slave_has_config_response = 1;
        }
        else if (i2c_slave_rx_buffer[0] == CMD_GOTO_BOOTSTRAP) {
            memset(i2c_slave_config_response, 0, sizeof(i2c_slave_config_response));
            i2c_slave_config_response[0] = CMD_GOTO_BOOTSTRAP;
            
            // Switch to bootstrap address
            my_slave_address = I2C_BOOTSTRAP_ADDRESS;
            i2c_slave_config_response[1] = STATUS_OK;
            usb_app_cdc_printf("SLAVE RX: Switching to bootstrap address 0x%02X\r\n", I2C_BOOTSTRAP_ADDRESS);
            
            // Schedule reconfiguration after this transaction completes
            i2c_slave_config_response_length = 2;
            i2c_slave_has_config_response = 1;
        }

        // Re-enable listening so the next transaction can be accepted
        HAL_I2C_EnableListen_IT(hi2c);
    }
}

void i2c_manager_slave_tx_complete_callback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C2) {
        // Check if we need to reconfigure with a new address
        if (i2c_slave_config_response_length > 0 && 
            ((i2c_slave_config_response[0] == CMD_ASSIGN_I2C_ADDRESS || 
              i2c_slave_config_response[0] == CMD_GOTO_BOOTSTRAP) && 
             i2c_slave_config_response[1] == STATUS_OK)) {
            
            // Reconfigure I2C with new address
            usb_app_cdc_printf("I2C: Reconfiguring slave with new address 0x%02X\r\n", my_slave_address);
            configure_i2c_slave();
            return;
        }
        
        // Transmission is complete, we are ready for a new request
        i2c_slave_state = I2C_SLAVE_STATE_READY;
        HAL_I2C_EnableListen_IT(hi2c);
    }
}

void i2c_manager_listen_complete_callback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C2) {
        // This indicates the end of a slave transaction (STOP condition)
        HAL_I2C_EnableListen_IT(hi2c);
    }
}

void i2c_manager_error_callback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C2) {
        uint32_t error_code = HAL_I2C_GetError(hi2c);
        usb_app_cdc_printf("I2C Error: 0x%08lX\r\n", error_code);

        // If a bus error or arbitration lost occurs, reset the peripheral
        if (error_code & (HAL_I2C_ERROR_BERR | HAL_I2C_ERROR_ARLO)) {
            HAL_I2C_DeInit(hi2c);
            configure_i2c_slave(); // Re-initialize slave mode
        }
        
        // Reset state to ready to avoid getting stuck
        i2c_slave_state = I2C_SLAVE_STATE_READY;
        
        // Always re-enable listening after an error
        HAL_I2C_EnableListen_IT(hi2c);
    }
}

void i2c_manager_handle_slave_key_event(const i2c_key_event_t *event)
{
    process_slave_key_event(event);
}

void i2c_manager_handle_slave_midi_event(const i2c_midi_event_t *event)
{
    process_slave_midi_event(event);
}

void i2c_manager_handle_slave_layer_state(const i2c_layer_state_t *event)
{
    process_slave_layer_state(event);
}
#endif /* Legacy I2C manager implementation */

#include "i2c_manager.h"
#include "usart.h"
#include "i2c_protocol.h"
#include "uart_debug.h"
#include "key_state.h"
#include "midi_handler.h"
#include "ws2812.h"
#include "main.h"
#include "usb_app.h"
#include "input/keymap.h"
#include "input/matrix.h"
#include "input/encoder.h"
#include "config_protocol.h"
#include "eeprom_emulation.h"
#include "device_info_util.h"
#include "module_info.h"

#include <string.h>
#include <stdbool.h>

#ifndef I2C_SLAVE_ADDRESS_BASE
#define I2C_SLAVE_ADDRESS_BASE 0x42u
#endif

#ifndef I2C_MAX_SLAVE_COUNT
#define I2C_MAX_SLAVE_COUNT 4u
#endif

#define UART_CHAIN_HEADER         0xD5u
#define UART_CHAIN_BROADCAST      0xFFu
#define UART_MASTER_ADDRESS       0x00u

#define UART_FRAME_TYPE_HELLO             0x01u
#define UART_FRAME_TYPE_KEY_EVENT         0x10u
#define UART_FRAME_TYPE_MIDI_EVENT        0x11u
#define UART_FRAME_TYPE_LAYER_STATE       0x12u
#define UART_FRAME_TYPE_CONFIG_REQUEST    0x20u
#define UART_FRAME_TYPE_CONFIG_RESPONSE   0x21u

#define UART_MAX_PAYLOAD          I2C_SLAVE_CONFIG_MAX_RESPONSE
#define UART_RX_BUFFER_SIZE       128u
#define UART_TX_TIMEOUT_MS        10u
#define UART_CONFIG_TIMEOUT_MS    200u
#define UART_HELLO_RETRY_MS       1000u
#define UART_LAYER_SUPPRESS_MS    50u
#define UART_MASTER_SCAN_INTERVAL_IDLE_MS     1000u
#define UART_MASTER_SCAN_INTERVAL_ACTIVE_MS   5000u

#define I2C_EVENT_FIFO_SIZE       16u
#define I2C_TAP_TIMEOUT_MS        100u

typedef enum {
    I2C_TAP_IDLE = 0,
    I2C_TAP_SEND_PRESS,
    I2C_TAP_WAIT_GAP,
    I2C_TAP_SEND_RELEASE
} i2c_tap_state_t;

typedef struct {
    uint8_t header;
    uint8_t src;
    uint8_t dst;
    uint8_t type;
    uint8_t length;
    uint8_t payload[UART_MAX_PAYLOAD];
    uint8_t checksum;
} __attribute__((packed)) uart_frame_t;

typedef enum {
    UART_RX_WAIT_HEADER = 0,
    UART_RX_READ_SRC,
    UART_RX_READ_DST,
    UART_RX_READ_TYPE,
    UART_RX_READ_LENGTH,
    UART_RX_READ_PAYLOAD,
    UART_RX_READ_CHECKSUM
} uart_rx_state_t;

typedef struct {
    uart_rx_state_t state;
    uart_frame_t frame;
    uint8_t payload_index;
} uart_parser_t;

typedef enum {
    UART_LINK_TOP = 0,
    UART_LINK_LEFT,
    UART_LINK_RIGHT,
    UART_LINK_BOTTOM,
    UART_LINK_COUNT
} uart_link_index_t;

typedef struct {
    UART_HandleTypeDef *huart;
    uart_parser_t parser;
    uint8_t rx_byte;
} uart_link_t;

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;
extern UART_HandleTypeDef huart4;

static uint8_t current_i2c_mode = 0xFFu;
static uint8_t my_address = I2C_SLAVE_ADDRESS_BASE;
uint8_t detected_slaves[I2C_MAX_SLAVE_COUNT] = {0};
uint8_t detected_slave_count = 0;
static uart_link_t *detected_slave_links[I2C_MAX_SLAVE_COUNT] = {0};

static uart_link_t uart_links[UART_LINK_COUNT] = {0};
static uart_link_t *parent_link = NULL;
static uint8_t parent_address = UART_MASTER_ADDRESS;

static uint32_t next_hello_tick = 0;
static uint32_t last_layer_broadcast_tick = 0;
static uint32_t next_master_scan_tick = 0;
static uint32_t master_scan_interval_ms = UART_MASTER_SCAN_INTERVAL_IDLE_MS;
static uint8_t last_reported_slave_count = 0u;

typedef struct {
    volatile bool pending;
    uint8_t dst;
    uint8_t type;
    uint8_t length;
    uint8_t payload[UART_MAX_PAYLOAD];
} slave_config_response_t;

static slave_config_response_t slave_config_response = {0};
static void try_send_slave_config_response(void);

static i2c_message_t i2c_event_fifo[I2C_EVENT_FIFO_SIZE];
static volatile uint8_t i2c_fifo_head = 0;
static volatile uint8_t i2c_fifo_tail = 0;
static volatile uint8_t i2c_fifo_count = 0;

static i2c_key_event_t i2c_master_event_fifo[I2C_EVENT_FIFO_SIZE];
static volatile uint8_t i2c_master_fifo_head = 0;
static volatile uint8_t i2c_master_fifo_tail = 0;
static volatile uint8_t i2c_master_fifo_count = 0;

static i2c_tap_state_t i2c_tap_state = I2C_TAP_IDLE;
static uint8_t i2c_tap_encoder_idx = 0;
static uint8_t i2c_tap_keycode = 0;
static uint32_t i2c_tap_t_release_earliest = 0;
static uint32_t i2c_tap_timeout = 0;

static struct {
    volatile bool pending;
    volatile bool completed;
    uint8_t expected_src;
    uint8_t expected_cmd;
    uint8_t response[UART_MAX_PAYLOAD];
    uint8_t response_len;
} config_exchange = {0};

static const uint8_t uart_fixed_addresses[I2C_MAX_SLAVE_COUNT] = {
    I2C_SLAVE_ADDRESS_BASE + 0u,
    I2C_SLAVE_ADDRESS_BASE + 1u,
    I2C_SLAVE_ADDRESS_BASE + 2u,
    I2C_SLAVE_ADDRESS_BASE + 3u
};

static uint8_t uart_calc_checksum(const uart_frame_t *frame)
{
    uint16_t sum = frame->header + frame->src + frame->dst + frame->type + frame->length;
    for (uint8_t idx = 0; idx < frame->length; ++idx) {
        sum = (uint16_t)(sum + frame->payload[idx]);
    }
    return (uint8_t)sum;
}

static void uart_parser_reset(uart_link_t *link)
{
    if (link == NULL) {
        return;
    }

    link->parser.state = UART_RX_WAIT_HEADER;
    link->parser.payload_index = 0u;
    memset(&link->parser.frame, 0, sizeof(link->parser.frame));
}

static void register_slave_address(uart_link_t *link, uint8_t address)
{
    if (address < I2C_SLAVE_ADDRESS_BASE || address >= (I2C_SLAVE_ADDRESS_BASE + I2C_MAX_SLAVE_COUNT)) {
        return;
    }

    for (uint8_t idx = 0; idx < detected_slave_count; ++idx) {
        if (detected_slaves[idx] == address) {
            detected_slave_links[idx] = link;
            return;
        }
    }

    if (detected_slave_count < I2C_MAX_SLAVE_COUNT) {
        detected_slaves[detected_slave_count] = address;
        detected_slave_links[detected_slave_count] = link;
        detected_slave_count++;
        usb_app_cdc_printf("UART: Registered slave 0x%02X (total=%d)\r\n", address, detected_slave_count);
    }
}

static uart_link_t *uart_link_from_handle(UART_HandleTypeDef *huart)
{
    if (huart == NULL) {
        return NULL;
    }

    for (uint8_t idx = 0u; idx < UART_LINK_COUNT; ++idx) {
        if (uart_links[idx].huart == huart) {
            return &uart_links[idx];
        }
    }

    return NULL;
}

static HAL_StatusTypeDef uart_link_restart_rx(uart_link_t *link)
{
    if (link == NULL || link->huart == NULL) {
        return HAL_ERROR;
    }

    return HAL_UART_Receive_IT(link->huart, &link->rx_byte, 1u);
}

static HAL_StatusTypeDef uart_send_frame(uint8_t dst, uint8_t type, const uint8_t *payload, uint8_t length)
{
    if (length > UART_MAX_PAYLOAD) {
        return HAL_ERROR;
    }

    uart_frame_t frame = {0};
    frame.header = UART_CHAIN_HEADER;
    frame.src = my_address;
    frame.dst = dst;
    frame.type = type;
    frame.length = length;
    if (length > 0u && payload != NULL) {
        memcpy(frame.payload, payload, length);
    }
    frame.checksum = uart_calc_checksum(&frame);

    uint8_t buffer[6u + UART_MAX_PAYLOAD] = {0};
    buffer[0] = frame.header;
    buffer[1] = frame.src;
    buffer[2] = frame.dst;
    buffer[3] = frame.type;
    buffer[4] = frame.length;
    if (length > 0u) {
        memcpy(&buffer[5], frame.payload, length);
    }
    buffer[5u + length] = frame.checksum;

    uint16_t total_len = (uint16_t)(6u + length);
    HAL_StatusTypeDef status = HAL_ERROR;

    if (current_i2c_mode == 0u) {
        if (parent_link != NULL && parent_link->huart != NULL) {
            status = HAL_UART_Transmit(parent_link->huart, buffer, total_len, UART_TX_TIMEOUT_MS);
            usb_app_cdc_printf("UART: slave TX via parent (dst=0x%02X, len=%u, status=%d)\r\n",
                               dst, total_len, status);
        }

        if (status != HAL_OK) {
            for (uint8_t idx = 0u; idx < UART_LINK_COUNT; ++idx) {
                if (uart_links[idx].huart == NULL) {
                    continue;
                }
                HAL_StatusTypeDef attempt = HAL_UART_Transmit(uart_links[idx].huart, buffer, total_len, UART_TX_TIMEOUT_MS);
                if (attempt == HAL_OK) {
                    status = HAL_OK;
                    usb_app_cdc_printf("UART: slave TX fallback link %u success (dst=0x%02X)\r\n", idx, dst);
                    break;
                } else {
                    usb_app_cdc_printf("UART: slave TX fallback link %u failed status=%d\r\n", idx, attempt);
                }
            }
        }

        return status;
    }

    if (dst == UART_CHAIN_BROADCAST) {
        for (uint8_t idx = 0u; idx < UART_LINK_COUNT; ++idx) {
            if (uart_links[idx].huart == NULL) {
                continue;
            }
            HAL_StatusTypeDef attempt = HAL_UART_Transmit(uart_links[idx].huart, buffer, total_len, UART_TX_TIMEOUT_MS);
            if (attempt == HAL_OK) {
                status = HAL_OK;
            }
        }
        return status;
    }

    uart_link_t *target_link = NULL;
    for (uint8_t idx = 0u; idx < detected_slave_count; ++idx) {
        if (detected_slaves[idx] == dst) {
            target_link = detected_slave_links[idx];
            break;
        }
    }

    if (target_link != NULL && target_link->huart != NULL) {
        HAL_StatusTypeDef direct = HAL_UART_Transmit(target_link->huart, buffer, total_len, UART_TX_TIMEOUT_MS);
        if (direct == HAL_OK) {
            return HAL_OK;
        }
        usb_app_cdc_printf("UART: direct TX to 0x%02X via link %p failed (%d)\r\n", dst, (void*)target_link->huart, direct);
    }

    for (uint8_t idx = 0u; idx < UART_LINK_COUNT; ++idx) {
        if (uart_links[idx].huart == NULL) {
            continue;
        }
        if (target_link != NULL && uart_links[idx].huart == target_link->huart) {
            continue;
        }
        HAL_StatusTypeDef attempt = HAL_UART_Transmit(uart_links[idx].huart, buffer, total_len, UART_TX_TIMEOUT_MS);
        if (attempt == HAL_OK) {
            status = HAL_OK;
        } else {
            usb_app_cdc_printf("UART: fallback TX via link %u failed (%d)\r\n", idx, attempt);
        }
    }

    return status;
}

static void uart_send_hello(void)
{
    if (current_i2c_mode != 0u) {
        return;
    }

    uint8_t payload[8] = {0};
    payload[0] = MODULE_FIRMWARE_VERSION_MAJOR;
    payload[1] = MODULE_FIRMWARE_VERSION_MINOR;
    payload[2] = MODULE_FIRMWARE_VERSION_PATCH;
    payload[3] = KEYMAP_LAYER_COUNT;

    if (uart_send_frame(UART_MASTER_ADDRESS, UART_FRAME_TYPE_HELLO, payload, 4u) == HAL_OK) {
        usb_app_cdc_printf("UART: Sent HELLO as 0x%02X\r\n", my_address);
        next_hello_tick = HAL_GetTick() + UART_HELLO_RETRY_MS;
    }
}

static uint8_t i2c_fifo_is_full(void)
{
    return (uint8_t)(i2c_fifo_count >= I2C_EVENT_FIFO_SIZE);
}

static uint8_t i2c_fifo_is_empty(void)
{
    return (uint8_t)(i2c_fifo_count == 0u);
}

static uint8_t i2c_fifo_push(const i2c_message_t *message)
{
    if (i2c_fifo_is_full()) {
        usb_app_cdc_printf("UART TX FIFO overflow, dropping msg_type=0x%02X\r\n", message->common.msg_type);
        return 0u;
    }

    i2c_event_fifo[i2c_fifo_head] = *message;
    i2c_fifo_head = (uint8_t)((i2c_fifo_head + 1u) % I2C_EVENT_FIFO_SIZE);
    ++i2c_fifo_count;
    return 1u;
}

static uint8_t i2c_fifo_pop(i2c_message_t *message)
{
    if (i2c_fifo_is_empty()) {
        return 0u;
    }

    *message = i2c_event_fifo[i2c_fifo_tail];
    i2c_fifo_tail = (uint8_t)((i2c_fifo_tail + 1u) % I2C_EVENT_FIFO_SIZE);
    --i2c_fifo_count;
    return 1u;
}

static uint8_t i2c_master_fifo_is_full(void)
{
    return (uint8_t)(i2c_master_fifo_count >= I2C_EVENT_FIFO_SIZE);
}

static uint8_t i2c_master_fifo_is_empty(void)
{
    return (uint8_t)(i2c_master_fifo_count == 0u);
}

static uint8_t i2c_master_fifo_push(const i2c_key_event_t *event)
{
    if (i2c_master_fifo_is_full()) {
        usb_app_cdc_printf("UART Master FIFO overflow, dropping key event\r\n");
        return 0u;
    }

    i2c_master_event_fifo[i2c_master_fifo_head] = *event;
    i2c_master_fifo_head = (uint8_t)((i2c_master_fifo_head + 1u) % I2C_EVENT_FIFO_SIZE);
    ++i2c_master_fifo_count;
    return 1u;
}

static uint8_t i2c_master_fifo_pop(i2c_key_event_t *event)
{
    if (i2c_master_fifo_is_empty()) {
        return 0u;
    }

    *event = i2c_master_event_fifo[i2c_master_fifo_tail];
    i2c_master_fifo_tail = (uint8_t)((i2c_master_fifo_tail + 1u) % I2C_EVENT_FIFO_SIZE);
    --i2c_master_fifo_count;
    return 1u;
}

static void process_slave_key_event(const i2c_key_event_t *event)
{
    if (!i2c_validate_message(event)) {
        return;
    }

    uint8_t mask = event->layer_mask;
    uint8_t default_layer = keymap_get_default_layer();
    if (mask == 0u) {
        uint8_t default_bit = (uint8_t)(1u << default_layer);
        if (default_bit == 0u) {
            default_bit = 1u;
        }
        mask = default_bit;
    }

    keymap_apply_layer_mask(mask, default_layer, false, false);

    if (event->row == 254u) {
        key_state_send_encoder_event(event->keycode);
        return;
    }

    if (event->pressed != 0u) {
        key_state_add_key(event->keycode);
    } else {
        key_state_remove_key(event->keycode);
    }
    key_state_update_hid_report();
}

static void process_slave_midi_event(const i2c_midi_event_t *event)
{
    if (!i2c_validate_midi_message(event)) {
        return;
    }

    switch (event->event_type) {
        case I2C_MIDI_EVENT_TYPE_CC:
            midi_send_cc(event->channel, event->data1, event->data2);
            break;
        case I2C_MIDI_EVENT_TYPE_NOTE_ON:
            midi_send_note_on(event->channel, event->data1, event->data2);
            break;
        case I2C_MIDI_EVENT_TYPE_NOTE_OFF:
            midi_send_note_off(event->channel, event->data1);
            break;
        default:
            break;
    }
}

static uint8_t first_active_layer(uint8_t mask)
{
    if (mask == 0u) {
        return 0u;
    }

    for (uint8_t idx = 0; idx < KEYMAP_LAYER_COUNT; ++idx) {
        if ((mask & (uint8_t)(1u << idx)) != 0u) {
            return idx;
        }
    }

    return 0u;
}

static void process_slave_layer_state(const i2c_layer_state_t *event)
{
    if (!i2c_validate_layer_message(event)) {
        return;
    }

    uint8_t mask = event->layer_mask;
    uint8_t default_layer = event->default_layer;

    if (default_layer >= KEYMAP_LAYER_COUNT) {
        default_layer = first_active_layer(mask);
    }

    bool update_default = (default_layer < KEYMAP_LAYER_COUNT) && (default_layer != keymap_get_default_layer());
    keymap_apply_layer_mask(mask, default_layer, true, update_default);
}

static void process_master_event_queue(void)
{
    i2c_key_event_t event;
    while (i2c_master_fifo_pop(&event)) {
        process_slave_key_event(&event);
    }
}

static void queue_midi_event(uint8_t event_type, uint8_t channel, uint8_t data1, uint8_t data2)
{
    i2c_message_t message = {0};
    message.midi_event.header = I2C_MSG_HEADER;
    message.midi_event.msg_type = I2C_MSG_MIDI_EVENT;
    message.midi_event.event_type = event_type;
    message.midi_event.channel = channel;
    message.midi_event.data1 = data1;
    message.midi_event.data2 = data2;
    message.midi_event.reserved = 0;
    message.midi_event.checksum = i2c_calc_midi_checksum(&message.midi_event);

    if (!i2c_fifo_push(&message)) {
        usb_app_cdc_printf("UART: Failed to queue MIDI event (type=%u)\r\n", event_type);
    }
}

static void try_send_slave_config_response(void)
{
    if (!slave_config_response.pending) {
        return;
    }

    usb_app_cdc_printf("UART: Pending config response dst=0x%02X len=%u\r\n",
                       slave_config_response.dst, slave_config_response.length);

    HAL_StatusTypeDef status = uart_send_frame(slave_config_response.dst,
                                              slave_config_response.type,
                                              slave_config_response.payload,
                                              slave_config_response.length);
    if (status == HAL_OK) {
        usb_app_cdc_printf("UART: Sent config response to 0x%02X len=%u\r\n",
                           slave_config_response.dst, slave_config_response.length);
        slave_config_response.pending = false;
    } else {
        usb_app_cdc_printf("UART: Config response TX retry (status=%d)\r\n", status);
    }
}

static void process_slave_tx_queue(void)
{
    if (current_i2c_mode != 0u) {
        return;
    }

    try_send_slave_config_response();

    if (slave_config_response.pending) {
        return;
    }

    if (i2c_fifo_is_empty()) {
        return;
    }

    i2c_message_t message;
    if (!i2c_fifo_pop(&message)) {
        return;
    }

    uint8_t frame_type;
    uint8_t length = 0u;
    const uint8_t *payload = NULL;

    switch (message.common.msg_type) {
        case I2C_MSG_KEY_EVENT:
            frame_type = UART_FRAME_TYPE_KEY_EVENT;
            payload = (const uint8_t *)&message.key_event;
            length = sizeof(i2c_key_event_t);
            break;
        case I2C_MSG_MIDI_EVENT:
            frame_type = UART_FRAME_TYPE_MIDI_EVENT;
            payload = (const uint8_t *)&message.midi_event;
            length = sizeof(i2c_midi_event_t);
            break;
        case I2C_MSG_LAYER_STATE:
            frame_type = UART_FRAME_TYPE_LAYER_STATE;
            payload = (const uint8_t *)&message.layer_state;
            length = sizeof(i2c_layer_state_t);
            break;
        default:
            return;
    }

    if (uart_send_frame(UART_MASTER_ADDRESS, frame_type, payload, length) != HAL_OK) {
        usb_app_cdc_printf("UART: TX failed for msg_type=0x%02X\r\n", message.common.msg_type);
    }
}

static uint8_t handle_slave_config_request(const uint8_t *request, uint8_t request_len, uint8_t *response)
{
    if (request_len == 0u || response == NULL) {
        return 0u;
    }

    uint8_t cmd = request[0];
    memset(response, 0, UART_MAX_PAYLOAD);
    response[0] = cmd;

    switch (cmd) {
        case CMD_GET_INFO: {
            device_info_t info;
            memset(&info, 0, sizeof(info));
            device_info_populate(&info, 0u, detected_slave_count);
            memcpy(&response[1], &info, sizeof(info));
            response[1 + sizeof(info)] = STATUS_OK;
            usb_app_cdc_printf("UART: handle CMD_GET_INFO from master len=%u\r\n",
                               (unsigned int)(1u + sizeof(info) + 1u));
            return (uint8_t)(1u + sizeof(info) + 1u);
        }
        case CMD_GET_KEYMAP: {
            if (request_len < 4u) {
                response[1] = STATUS_INVALID_PARAM;
                return 2u;
            }
            uint8_t layer = request[1];
            uint8_t row = request[2];
            uint8_t col = request[3];
            uint16_t keycode = keymap_get_keycode(layer, row, col);
            response[1] = layer;
            response[2] = row;
            response[3] = col;
            response[4] = (uint8_t)(keycode & 0xFFu);
            response[5] = (uint8_t)((keycode >> 8) & 0xFFu);
            response[6] = STATUS_OK;
            return 7u;
        }
        case CMD_SET_KEYMAP: {
            if (request_len < 6u) {
                response[1] = STATUS_INVALID_PARAM;
                return 2u;
            }
            uint8_t layer = request[1];
            uint8_t row = request[2];
            uint8_t col = request[3];
            uint16_t keycode = (uint16_t)(request[4] | (request[5] << 8));
            bool ok = keymap_set_keycode(layer, row, col, keycode);
            response[1] = ok ? STATUS_OK : STATUS_ERROR;
            return 2u;
        }
        case CMD_GET_ENCODER_MAP: {
            if (request_len < 3u) {
                response[1] = STATUS_INVALID_PARAM;
                return 2u;
            }
            uint8_t layer = request[1];
            uint8_t encoder_id = request[2];
            uint16_t ccw = 0;
            uint16_t cw = 0;
            bool ok = keymap_get_encoder_map(layer, encoder_id, &ccw, &cw);
            response[1] = layer;
            response[2] = encoder_id;
            response[3] = (uint8_t)(ccw & 0xFFu);
            response[4] = (uint8_t)((ccw >> 8) & 0xFFu);
            response[5] = (uint8_t)(cw & 0xFFu);
            response[6] = (uint8_t)((cw >> 8) & 0xFFu);
            response[7] = ok ? STATUS_OK : STATUS_INVALID_PARAM;
            return 8u;
        }
        case CMD_SET_ENCODER_MAP: {
            if (request_len < 7u) {
                response[1] = STATUS_INVALID_PARAM;
                return 2u;
            }
            uint8_t layer = request[1];
            uint8_t encoder_id = request[2];
            uint16_t ccw = (uint16_t)(request[3] | (request[4] << 8));
            uint16_t cw = (uint16_t)(request[5] | (request[6] << 8));
            bool ok = keymap_set_encoder_map(layer, encoder_id, ccw, cw);
            response[1] = ok ? STATUS_OK : STATUS_ERROR;
            return 2u;
        }
        case CMD_SET_LAYER_STATE: {
            if (request_len < 3u) {
                response[1] = STATUS_INVALID_PARAM;
                return 2u;
            }
            uint8_t mask = request[1];
            uint8_t def_layer = request[2];
            bool update_default = (def_layer < KEYMAP_LAYER_COUNT) && (def_layer != keymap_get_default_layer());
            keymap_apply_layer_mask(mask, def_layer, false, update_default);
            response[1] = STATUS_OK;
            return 2u;
        }
        case CMD_GET_LAYER_STATE: {
            uint8_t mask = keymap_get_layer_mask();
            uint8_t def_layer = keymap_get_default_layer();
            response[1] = mask;
            response[2] = def_layer;
            response[6] = STATUS_OK;
            return 7u;
        }
        case CMD_SAVE_CONFIG: {
            bool ok = eeprom_save_config();
            response[1] = ok ? STATUS_OK : STATUS_ERROR;
            return 2u;
        }
        default:
            response[1] = STATUS_NOT_SUPPORTED;
            return 2u;
    }
}

static void handle_uart_frame(uart_link_t *link, const uart_frame_t *frame)
{
    if (frame->dst != my_address && frame->dst != UART_CHAIN_BROADCAST) {
        return;
    }

    uint8_t expected_checksum = uart_calc_checksum(frame);
    if (expected_checksum != frame->checksum) {
        usb_app_cdc_printf("UART: Bad checksum from 0x%02X\r\n", frame->src);
        return;
    }

    if (current_i2c_mode == 1u && link != NULL) {
        register_slave_address(link, frame->src);
    } else if (current_i2c_mode == 0u && link != NULL) {
        parent_link = link;
        parent_address = frame->src;
    }

    switch (frame->type) {
        case UART_FRAME_TYPE_HELLO:
            if (current_i2c_mode == 1u && link != NULL) {
                register_slave_address(link, frame->src);
                usb_app_cdc_printf("UART: HELLO from 0x%02X len=%u\r\n", frame->src, frame->length);
            } else if (current_i2c_mode == 0u) {
                parent_link = link;
                parent_address = frame->src;
            }
            break;
        case UART_FRAME_TYPE_KEY_EVENT:
            if (frame->length == sizeof(i2c_key_event_t) && current_i2c_mode == 1u) {
                i2c_key_event_t event;
                memcpy(&event, frame->payload, sizeof(event));
                i2c_master_fifo_push(&event);
            }
            break;
        case UART_FRAME_TYPE_MIDI_EVENT:
            if (frame->length == sizeof(i2c_midi_event_t) && current_i2c_mode == 1u) {
                i2c_midi_event_t event;
                memcpy(&event, frame->payload, sizeof(event));
                process_slave_midi_event(&event);
            }
            break;
        case UART_FRAME_TYPE_LAYER_STATE:
            if (frame->length == sizeof(i2c_layer_state_t)) {
                i2c_layer_state_t event;
                memcpy(&event, frame->payload, sizeof(event));
                process_slave_layer_state(&event);
            }
            break;
        case UART_FRAME_TYPE_CONFIG_REQUEST:
            if (current_i2c_mode == 0u) {
                uint8_t response[UART_MAX_PAYLOAD];
                uint8_t response_len = handle_slave_config_request(frame->payload, frame->length, response);
                if (response_len > 0u) {
                    if (!slave_config_response.pending) {
                        if (response_len > UART_MAX_PAYLOAD) {
                            response_len = UART_MAX_PAYLOAD;
                        }
                        memcpy((void *)slave_config_response.payload, response, response_len);
                        slave_config_response.dst = frame->src;
                        slave_config_response.type = UART_FRAME_TYPE_CONFIG_RESPONSE;
                        slave_config_response.length = response_len;
                        slave_config_response.pending = true;
                        usb_app_cdc_printf("UART: Queued config response to 0x%02X len=%u\r\n",
                                           slave_config_response.dst, response_len);
                        try_send_slave_config_response();
                    } else {
                        usb_app_cdc_printf("UART: Config response dropped (pending)\r\n");
                    }
                }
            }
            break;
        case UART_FRAME_TYPE_CONFIG_RESPONSE:
            if (current_i2c_mode == 1u && config_exchange.pending && frame->src == config_exchange.expected_src) {
                uint8_t copy_len = (frame->length <= UART_MAX_PAYLOAD) ? frame->length : UART_MAX_PAYLOAD;
                memcpy((void *)config_exchange.response, frame->payload, copy_len);
                config_exchange.response_len = copy_len;
                config_exchange.completed = true;
                config_exchange.pending = false;
                usb_app_cdc_printf("UART: RX config response from 0x%02X len=%u\r\n", frame->src, copy_len);
            }
            break;
        default:
            break;
    }
}

static void uart_parser_feed(uart_link_t *link, uint8_t byte)
{
    if (link == NULL) {
        return;
    }

    switch (link->parser.state) {
        case UART_RX_WAIT_HEADER:
            if (byte == UART_CHAIN_HEADER) {
                link->parser.frame.header = byte;
                link->parser.state = UART_RX_READ_SRC;
            }
            break;
        case UART_RX_READ_SRC:
            link->parser.frame.src = byte;
            link->parser.state = UART_RX_READ_DST;
            break;
        case UART_RX_READ_DST:
            link->parser.frame.dst = byte;
            link->parser.state = UART_RX_READ_TYPE;
            break;
        case UART_RX_READ_TYPE:
            link->parser.frame.type = byte;
            link->parser.state = UART_RX_READ_LENGTH;
            break;
        case UART_RX_READ_LENGTH:
            if (byte > UART_MAX_PAYLOAD) {
                uart_parser_reset(link);
            } else {
                link->parser.frame.length = byte;
                link->parser.payload_index = 0u;
                link->parser.state = (byte == 0u) ? UART_RX_READ_CHECKSUM : UART_RX_READ_PAYLOAD;
            }
            break;
        case UART_RX_READ_PAYLOAD:
            if (link->parser.payload_index < UART_MAX_PAYLOAD) {
                link->parser.frame.payload[link->parser.payload_index++] = byte;
            }
            if (link->parser.payload_index >= link->parser.frame.length) {
                link->parser.state = UART_RX_READ_CHECKSUM;
            }
            break;
        case UART_RX_READ_CHECKSUM:
            link->parser.frame.checksum = byte;
            handle_uart_frame(link, &link->parser.frame);
            uart_parser_reset(link);
            break;
        default:
            uart_parser_reset(link);
            break;
    }
}

static void process_uart_housekeeping(void)
{
    if (current_i2c_mode == 0u) {
        uint32_t now = HAL_GetTick();
        if ((int32_t)(now - (int32_t)next_hello_tick) >= 0) {
            uart_send_hello();
        }
    }
}

static void process_i2c_encoder_state_machine(void)
{
    if (i2c_tap_state == I2C_TAP_IDLE) {
        return;
    }

    uint32_t now = HAL_GetTick();
    if ((int32_t)(now - (int32_t)i2c_tap_timeout) >= 0) {
        i2c_tap_state = I2C_TAP_IDLE;
        i2c_tap_keycode = 0;
        return;
    }

    switch (i2c_tap_state) {
        case I2C_TAP_SEND_PRESS:
            i2c_manager_send_key_event(254u, i2c_tap_encoder_idx, 1u, i2c_tap_keycode);
            i2c_tap_t_release_earliest = now + 1u;
            i2c_tap_state = I2C_TAP_WAIT_GAP;
            break;
        case I2C_TAP_WAIT_GAP:
            if ((int32_t)(now - (int32_t)i2c_tap_t_release_earliest) >= 0) {
                i2c_tap_state = I2C_TAP_SEND_RELEASE;
            }
            break;
        case I2C_TAP_SEND_RELEASE:
            i2c_manager_send_key_event(254u, i2c_tap_encoder_idx, 0u, i2c_tap_keycode);
            i2c_tap_state = I2C_TAP_IDLE;
            i2c_tap_keycode = 0u;
            break;
        default:
            i2c_tap_state = I2C_TAP_IDLE;
            i2c_tap_keycode = 0u;
            break;
    }
}

static void pump_main_tasks_during_wait(void)
{
    usb_app_task();
    matrix_scan();
    encoder_task();
    key_state_task();

    if (current_i2c_mode == 0u) {
        process_i2c_encoder_state_machine();
        process_slave_tx_queue();
    } else {
        process_master_event_queue();
    }
}

void i2c_manager_init(void)
{
    memset(i2c_event_fifo, 0, sizeof(i2c_event_fifo));
    memset(i2c_master_event_fifo, 0, sizeof(i2c_master_event_fifo));
    i2c_fifo_head = i2c_fifo_tail = i2c_fifo_count = 0u;
    i2c_master_fifo_head = i2c_master_fifo_tail = i2c_master_fifo_count = 0u;
    i2c_tap_state = I2C_TAP_IDLE;
    i2c_tap_keycode = 0u;
    detected_slave_count = 0u;
    memset(detected_slaves, 0, sizeof(detected_slaves));
    memset(detected_slave_links, 0, sizeof(detected_slave_links));
    config_exchange.pending = false;
    config_exchange.completed = false;
    memset(uart_links, 0, sizeof(uart_links));
    uart_links[UART_LINK_TOP].huart = &huart1;
    uart_links[UART_LINK_LEFT].huart = &huart2;
    uart_links[UART_LINK_RIGHT].huart = &huart3;
    uart_links[UART_LINK_BOTTOM].huart = &huart4;

    for (uint8_t idx = 0u; idx < UART_LINK_COUNT; ++idx) {
        uart_parser_reset(&uart_links[idx]);
        if (uart_links[idx].huart != NULL) {
            if (uart_link_restart_rx(&uart_links[idx]) != HAL_OK) {
                Error_Handler();
            }
        }
    }

    parent_link = NULL;
    parent_address = UART_MASTER_ADDRESS;
    master_scan_interval_ms = UART_MASTER_SCAN_INTERVAL_IDLE_MS;
    last_reported_slave_count = 0u;
    next_master_scan_tick = HAL_GetTick();
}

void i2c_manager_set_mode(uint8_t is_master)
{
    if (is_master) {
        current_i2c_mode = 1u;
        my_address = UART_MASTER_ADDRESS;
        detected_slave_count = 0u;
        memset(detected_slaves, 0, sizeof(detected_slaves));
        memset(detected_slave_links, 0, sizeof(detected_slave_links));
        master_scan_interval_ms = UART_MASTER_SCAN_INTERVAL_IDLE_MS;
        last_reported_slave_count = 0u;
        next_master_scan_tick = HAL_GetTick();
        i2c_manager_scan_slaves();
    } else {
        current_i2c_mode = 0u;
#ifdef FIXED_SLAVE_ADDRESS
        my_address = FIXED_SLAVE_ADDRESS;
#else
        my_address = I2C_SLAVE_ADDRESS_BASE;
#endif
        master_scan_interval_ms = UART_MASTER_SCAN_INTERVAL_IDLE_MS;
        last_reported_slave_count = 0u;
        next_master_scan_tick = 0u;
        memset(detected_slave_links, 0, sizeof(detected_slave_links));
        uart_send_hello();
    }
}

uint8_t i2c_manager_get_mode(void)
{
    return current_i2c_mode;
}

void i2c_manager_scan_slaves(void)
{
    if (current_i2c_mode != 1u || config_exchange.pending) {
        return;
    }

    if (detected_slave_count == 0u) {
        usb_app_cdc_printf("UART: Scanning for slaves\r\n");
    }

    uint8_t new_detected[I2C_MAX_SLAVE_COUNT] = {0};
    uint8_t new_count = 0u;
    const uint8_t request[1] = { CMD_GET_INFO };
    uint8_t response[1u + sizeof(device_info_t) + 1u] = {0};

    for (uint8_t idx = 0u; idx < I2C_MAX_SLAVE_COUNT; ++idx) {
        uint8_t address = uart_fixed_addresses[idx];
        uint8_t response_len = (uint8_t)sizeof(response);

        if (!i2c_manager_config_roundtrip(address,
                                          request,
                                          (uint8_t)sizeof(request),
                                          response,
                                          &response_len,
                                          UART_CONFIG_TIMEOUT_MS)) {
            continue;
        }

        if (response_len < 2u || response[0] != CMD_GET_INFO) {
            usb_app_cdc_printf("UART: Unexpected response header %u len=%u\r\n", response[0], response_len);
            continue;
        }

        if (response[response_len - 1u] != STATUS_OK) {
            usb_app_cdc_printf("UART: Slave 0x%02X status=%u\r\n", address, response[response_len - 1u]);
            continue;
        }

        new_detected[new_count++] = address;

        if (new_count >= I2C_MAX_SLAVE_COUNT) {
            break;
        }
    }

    bool changed = (new_count != detected_slave_count) ||
                   (memcmp(detected_slaves, new_detected, sizeof(new_detected)) != 0);
    bool first_detection = (new_count > 0u && last_reported_slave_count == 0u);
    bool lost_slaves = (new_count == 0u && last_reported_slave_count != 0u);

    memset(detected_slaves, 0, sizeof(detected_slaves));
    memset(detected_slave_links, 0, sizeof(detected_slave_links));
    memcpy(detected_slaves, new_detected, sizeof(new_detected));
    detected_slave_count = new_count;

    if (changed || first_detection || lost_slaves) {
        usb_app_cdc_printf("UART: Found %u slave(s)\r\n", detected_slave_count);
    }

    last_reported_slave_count = detected_slave_count;
    master_scan_interval_ms = (detected_slave_count > 0u) ?
                              UART_MASTER_SCAN_INTERVAL_ACTIVE_MS :
                              UART_MASTER_SCAN_INTERVAL_IDLE_MS;
    next_master_scan_tick = HAL_GetTick() + master_scan_interval_ms;
}

void i2c_manager_scan_slaves_force(void)
{
    i2c_manager_scan_slaves();
}

void i2c_manager_poll_slaves(void)
{
    /* No polling required on UART architecture */
}

void i2c_manager_task(void)
{
    process_uart_housekeeping();

    if (current_i2c_mode == 0u) {
        process_i2c_encoder_state_machine();
        process_slave_tx_queue();
    } else if (current_i2c_mode == 1u) {
        process_master_event_queue();

        uint32_t now = HAL_GetTick();
        if ((int32_t)(now - (int32_t)next_master_scan_tick) >= 0) {
            i2c_manager_scan_slaves();
        }
    }
}

void i2c_manager_send_key_event(uint8_t row, uint8_t col, uint8_t pressed, uint8_t keycode)
{
    i2c_message_t message = {0};
    message.key_event.header = I2C_MSG_HEADER;
    message.key_event.msg_type = I2C_MSG_KEY_EVENT;
    message.key_event.row = row;
    message.key_event.col = col;
    message.key_event.pressed = pressed;
    message.key_event.keycode = keycode;
    message.key_event.layer_mask = keymap_get_layer_mask();
    message.key_event.checksum = i2c_calc_checksum(&message.key_event);

    (void)i2c_fifo_push(&message);
}

void i2c_manager_send_layer_state(uint8_t layer_mask, uint8_t default_layer)
{
    i2c_message_t message = {0};
    message.layer_state.header = I2C_MSG_HEADER;
    message.layer_state.msg_type = I2C_MSG_LAYER_STATE;
    message.layer_state.layer_mask = layer_mask;
    message.layer_state.default_layer = default_layer;
    message.layer_state.checksum = i2c_calc_layer_checksum(&message.layer_state);
    (void)i2c_fifo_push(&message);
}

void i2c_manager_broadcast_layer_state(uint8_t layer_mask, uint8_t default_layer)
{
    if (current_i2c_mode != 1u) {
        i2c_manager_send_layer_state(layer_mask, default_layer);
        return;
    }

    uint8_t payload[sizeof(i2c_layer_state_t)];
    i2c_layer_state_t layer_state = {
        .header = I2C_MSG_HEADER,
        .msg_type = I2C_MSG_LAYER_STATE,
        .layer_mask = layer_mask,
        .default_layer = default_layer,
        .checksum = 0
    };
    layer_state.checksum = i2c_calc_layer_checksum(&layer_state);
    memcpy(payload, &layer_state, sizeof(layer_state));

    for (uint8_t idx = 0; idx < detected_slave_count; ++idx) {
        uart_send_frame(detected_slaves[idx], UART_FRAME_TYPE_LAYER_STATE, payload, sizeof(layer_state));
    }

    last_layer_broadcast_tick = HAL_GetTick();
}

void i2c_manager_send_midi_cc(uint8_t channel, uint8_t controller, uint8_t value)
{
    queue_midi_event(I2C_MIDI_EVENT_TYPE_CC, channel, controller, value);
}

void i2c_manager_send_midi_note(uint8_t channel, uint8_t note, uint8_t velocity, bool pressed)
{
    uint8_t event_type = pressed ? I2C_MIDI_EVENT_TYPE_NOTE_ON : I2C_MIDI_EVENT_TYPE_NOTE_OFF;
    queue_midi_event(event_type, channel, note, velocity);
}

void i2c_manager_process_local_key_event(uint8_t row, uint8_t col, uint8_t pressed, uint8_t keycode)
{
    if (keycode == 0u) {
        return;
    }

    if (current_i2c_mode != 1u) {
        i2c_manager_send_key_event(row, col, pressed, keycode);
        return;
    }

    i2c_key_event_t event = {
        .header = I2C_MSG_HEADER,
        .msg_type = I2C_MSG_KEY_EVENT,
        .row = row,
        .col = col,
        .pressed = pressed,
        .keycode = keycode,
        .layer_mask = keymap_get_layer_mask(),
        .checksum = 0
    };
    event.checksum = i2c_calc_checksum(&event);
    (void)i2c_master_fifo_push(&event);
}

bool i2c_manager_config_roundtrip(uint8_t slave_address,
                                  const uint8_t *request,
                                  uint8_t request_len,
                                  uint8_t *response,
                                  uint8_t *response_len,
                                  uint32_t timeout_ms)
{
    if (current_i2c_mode != 1u || request == NULL || response == NULL || response_len == NULL) {
        return false;
    }

    if (request_len == 0u || request_len > UART_MAX_PAYLOAD) {
        return false;
    }

    config_exchange.pending = true;
    config_exchange.completed = false;
    config_exchange.expected_src = slave_address;
    config_exchange.expected_cmd = request[0];
    config_exchange.response_len = 0u;

    usb_app_cdc_printf("UART: Roundtrip start cmd=0x%02X dst=0x%02X\r\n", request[0], slave_address);

    if (uart_send_frame(slave_address, UART_FRAME_TYPE_CONFIG_REQUEST, request, request_len) != HAL_OK) {
        config_exchange.pending = false;
        usb_app_cdc_printf("UART: Roundtrip TX failed to 0x%02X\r\n", slave_address);
        return false;
    }

    uint32_t start = HAL_GetTick();
    while (!config_exchange.completed) {
        if ((int32_t)(HAL_GetTick() - (int32_t)start) >= (int32_t)timeout_ms) {
            config_exchange.pending = false;
            usb_app_cdc_printf("UART: Roundtrip timeout from 0x%02X\r\n", slave_address);
            return false;
        }

        pump_main_tasks_during_wait();
    }

    uint8_t copy_len = config_exchange.response_len;
    if (copy_len > *response_len) {
        copy_len = *response_len;
    }
    memcpy(response, config_exchange.response, copy_len);
    *response_len = copy_len;
    config_exchange.completed = false;
    usb_app_cdc_printf("UART: Roundtrip OK from 0x%02X (%u bytes)\r\n", slave_address, copy_len);
    return true;
}

void i2c_manager_handle_slave_key_event(const i2c_key_event_t *event)
{
    process_slave_key_event(event);
}

void i2c_manager_handle_slave_midi_event(const i2c_midi_event_t *event)
{
    process_slave_midi_event(event);
}

void i2c_manager_handle_slave_layer_state(const i2c_layer_state_t *event)
{
    process_slave_layer_state(event);
}

void i2c_manager_encoder_callback(uint8_t encoder_idx, uint8_t direction, uint8_t keycode)
{
    (void)direction;
    if (i2c_tap_state == I2C_TAP_IDLE) {
        i2c_tap_encoder_idx = encoder_idx;
        i2c_tap_keycode = keycode;
        i2c_tap_state = I2C_TAP_SEND_PRESS;
        i2c_tap_timeout = HAL_GetTick() + I2C_TAP_TIMEOUT_MS;
    }
}

void i2c_manager_uart_rx_cplt_callback(UART_HandleTypeDef *huart)
{
    uart_link_t *link = uart_link_from_handle(huart);
    if (link == NULL) {
        return;
    }

    uart_parser_feed(link, link->rx_byte);
    if (uart_link_restart_rx(link) != HAL_OK) {
        Error_Handler();
    }
}

void i2c_manager_uart_error_callback(UART_HandleTypeDef *huart)
{
    uart_link_t *link = uart_link_from_handle(huart);
    if (link == NULL) {
        return;
    }

    uart_parser_reset(link);
    HAL_UART_AbortReceive(huart);
    if (uart_link_restart_rx(link) != HAL_OK) {
        Error_Handler();
    }
}