#ifndef I2C_PROTOCOL_H
#define I2C_PROTOCOL_H

#include <stdint.h>

// I2C communication protocol for keyboard matrix data
#define I2C_MSG_HEADER 0xAB
#define I2C_MSG_KEY_EVENT 0x01
#define I2C_MSG_MIDI_EVENT 0x02
#define I2C_MIDI_EVENT_TYPE_CC 0x00
#define I2C_MIDI_EVENT_TYPE_NOTE_ON 0x01
#define I2C_MIDI_EVENT_TYPE_NOTE_OFF 0x02
#define I2C_MSG_MAX_SIZE 8

// Key event message structure sent from slave to master
typedef struct {
    uint8_t header;     // Always I2C_MSG_HEADER (0xAB)
    uint8_t msg_type;   // I2C_MSG_KEY_EVENT (0x01)
    uint8_t row;        // Matrix row (0-4)
    uint8_t col;        // Matrix column (0-6)
    uint8_t pressed;    // 1 = pressed, 0 = released
    uint8_t keycode;    // HID keycode
    uint8_t checksum;   // Simple checksum for data integrity
    uint8_t padding;    // Pad to 8 bytes for alignment
} __attribute__((packed)) i2c_key_event_t;

// MIDI event message structure sent from slave to master
typedef struct {
    uint8_t header;      // Always I2C_MSG_HEADER (0xAB)
    uint8_t msg_type;    // I2C_MSG_MIDI_EVENT (0x02)
    uint8_t event_type;  // One of I2C_MIDI_EVENT_TYPE_*
    uint8_t channel;     // MIDI channel (0-15)
    uint8_t data1;       // Controller or note number
    uint8_t data2;       // Value or velocity
    uint8_t checksum;    // Simple checksum for data integrity
    uint8_t reserved;    // Pad to 8 bytes for alignment
} __attribute__((packed)) i2c_midi_event_t;

// Union for different message types
typedef union {
    struct {
        uint8_t header;    // Always I2C_MSG_HEADER (0xAB) for message type detection
        uint8_t msg_type;  // Message type identifier
    } common;
    i2c_key_event_t key_event;
    i2c_midi_event_t midi_event;
} __attribute__((packed)) i2c_message_t;

// Calculate checksum for message
static inline uint8_t i2c_calc_checksum(const i2c_key_event_t *msg)
{
    return msg->header + msg->msg_type + msg->row + msg->col + msg->pressed + msg->keycode;
}

// Calculate checksum for MIDI message
static inline uint8_t i2c_calc_midi_checksum(const i2c_midi_event_t *msg)
{
    return msg->header + msg->msg_type + msg->event_type + msg->channel + msg->data1 + msg->data2;
}

// Validate message checksum
static inline uint8_t i2c_validate_message(const i2c_key_event_t *msg)
{
    return (i2c_calc_checksum(msg) == msg->checksum) ? 1 : 0;
}

// Validate MIDI message checksum
static inline uint8_t i2c_validate_midi_message(const i2c_midi_event_t *msg)
{
    return (i2c_calc_midi_checksum(msg) == msg->checksum) ? 1 : 0;
}

#endif // I2C_PROTOCOL_H