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
/* Private constants */
#define I2C_SLAVE_ADDRESS 0x42  // 7-bit address for slave mode
#define I2C_EVENT_FIFO_SIZE 16
#define I2C_TAP_TIMEOUT_MS 100

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
static uint8_t detected_slaves[16]; // Track detected slave addresses
static uint8_t detected_slave_count = 0;

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

/* Initialize I2C tx buffer with valid checksum */
static void init_i2c_tx_buffer(void)
{
    i2c_tx_buffer.key_event.header = I2C_MSG_HEADER;
    i2c_tx_buffer.key_event.msg_type = 0xFF;
    i2c_tx_buffer.key_event.row = 0;
    i2c_tx_buffer.key_event.col = 0;
    i2c_tx_buffer.key_event.pressed = 0;
    i2c_tx_buffer.key_event.keycode = 0;
    i2c_tx_buffer.key_event.padding = 0;
    i2c_tx_buffer.key_event.checksum = i2c_calc_checksum(&i2c_tx_buffer.key_event);
}

/* Configure I2C2 as master (for when connected to USB host) */
static void configure_i2c_master(void)
{
    if (current_i2c_mode == 1) return; // Already master
    
    // Deinit current I2C configuration
    HAL_I2C_DeInit(&hi2c2);
    
    // Configure as master
    hi2c2.Instance = I2C2;
    hi2c2.Init.Timing = 0x60715075;
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
    hi2c2.Init.Timing = 0x60715075;
    hi2c2.Init.OwnAddress1 = (I2C_SLAVE_ADDRESS << 1); // Shift for HAL format
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
                 I2C_SLAVE_ADDRESS, (I2C_SLAVE_ADDRESS << 1));
    
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
void i2c_manager_scan_slaves(void)
{
    uint32_t now = HAL_GetTick();
    if ((now - last_slave_scan) < SLAVE_SCAN_INTERVAL_MS) {
        return; // Not time to scan yet
    }
    
    last_slave_scan = now;
    detected_slave_count = 0;
    
    usb_app_cdc_printf("I2C: Starting slave scan...\r\n");
    
    // Scan I2C address range (0x08 to 0x77 are valid 7-bit addresses)
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        // Quick ping to see if device responds
        HAL_StatusTypeDef status = HAL_I2C_IsDeviceReady(&hi2c2, addr << 1, 1, 10);
        
        if (status == HAL_OK) {
            // Device found
            if (detected_slave_count < 16) {
                detected_slaves[detected_slave_count] = addr;
                detected_slave_count++;
            }
            usb_app_cdc_printf("I2C: Found slave at address 0x%02X\r\n", addr);
        }
    }
    
    if (detected_slave_count == 0) {
        usb_app_cdc_printf("I2C: No slaves detected\r\n");
    } else {
        usb_app_cdc_printf("I2C: Total %d slaves detected\r\n", detected_slave_count);
    }
}

void i2c_manager_task(void)
{
    if (current_i2c_mode == 0) { 
        // Slave mode - process encoder state machine
        process_i2c_encoder_state_machine();
    } else if (current_i2c_mode == 1) {
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
    message.key_event.padding = 0;
    message.key_event.checksum = i2c_calc_checksum(&message.key_event);

    // Add to FIFO queue for transmission
    if (!i2c_fifo_push(&message)) {
        usb_app_cdc_printf("Failed to queue I2C event: row=%d, col=%d, pressed=%d\r\n", row, col, pressed);
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
    uint16_t slave_addr = 0x42;
    HAL_StatusTypeDef status;
    uint8_t events_processed = 0;
    const uint8_t max_events_per_tick = 16; // Limit to prevent blocking for too long

    for (events_processed = 0; events_processed < max_events_per_tick; events_processed++)
    {
    // Clear receive buffer
    i2c_rx_buffer = (i2c_message_t){0};
        
        // Request data from slave
        status = HAL_I2C_Master_Receive(&hi2c2, slave_addr << 1, (uint8_t*)&i2c_rx_buffer, sizeof(i2c_message_t), 10);
        
        if (status == HAL_OK) {
            // Check for a valid message header
            if (i2c_rx_buffer.common.header == I2C_MSG_HEADER) {
                if (i2c_rx_buffer.common.msg_type == I2C_MSG_KEY_EVENT) {
                    if (!i2c_master_fifo_push(&i2c_rx_buffer.key_event)) {
                        usb_app_cdc_printf("Master FIFO full, dropping key event\r\n");
                    }
                } else if (i2c_rx_buffer.common.msg_type == I2C_MSG_MIDI_EVENT) {
                    process_slave_midi_event(&i2c_rx_buffer.midi_event);
                } else {
                    // Invalid message type, assume FIFO is empty
                    break;
                }
            } else {
                // Invalid header, assume FIFO is empty
                break;
            }
        } else {
            // HAL error, stop polling
            break;
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
        .checksum = 0,
        .padding = 0
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
            // Master is writing to us - prepare to receive
            HAL_I2C_Slave_Seq_Receive_IT(hi2c, i2c_slave_rx_buffer, sizeof(i2c_slave_rx_buffer), I2C_FIRST_AND_LAST_FRAME);
        } else {
            // Master is reading from us
            if (i2c_slave_state == I2C_SLAVE_STATE_READY) {
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
                    i2c_tx_buffer.key_event.padding = 0;
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
                i2c_tx_buffer.key_event.padding = 0;
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
        // Re-enable listening for the next transaction
        HAL_I2C_EnableListen_IT(hi2c);
    }
}

void i2c_manager_slave_tx_complete_callback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C2) {
        // Transmission is complete, we are ready for a new request
        i2c_slave_state = I2C_SLAVE_STATE_READY;
        // Re-enable listening for the next transaction
        HAL_I2C_EnableListen_IT(hi2c);
    }
}

void i2c_manager_listen_complete_callback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C2) {
        // This indicates the end of a slave transaction (e.g., after a STOP condition)
        // Re-enable listening to be ready for the next master request
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