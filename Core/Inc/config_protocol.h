#ifndef CONFIG_PROTOCOL_H
#define CONFIG_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// I2C Configuration
#define I2C_SLAVE_ADDRESS_BASE 0x42
#define I2C_BOOTSTRAP_ADDRESS 0x7F
#define I2C_MAX_SLAVE_COUNT 4

// Slave address configuration - uncomment one of these to set fixed address
#define FIXED_SLAVE_ADDRESS 0x43   // First slave
//#define FIXED_SLAVE_ADDRESS 0x43   // Second slave  
//#define FIXED_SLAVE_ADDRESS 0x44   // Third slave
//#define FIXED_SLAVE_ADDRESS 0x45   // Fourth slave

// Configuration Protocol Version
#define CONFIG_PROTOCOL_VERSION 1

// Packet header and sizes
#define CONFIG_PACKET_HEADER 0x4F47    // "OG" - matches Tauri app
#define CONFIG_MAX_PAYLOAD_SIZE 56
#define CONFIG_PACKET_SIZE 64
#define I2C_SLAVE_CONFIG_CMD_SIZE 7
#define I2C_SLAVE_CONFIG_MAX_RESPONSE 64

// Command types
typedef enum {
    CMD_GET_INFO = 0x01,           // Get device information
    CMD_GET_KEYMAP = 0x02,         // Get current keymap
    CMD_SET_KEYMAP = 0x03,         // Set keymap entry
    CMD_GET_ENCODER_MAP = 0x04,    // Get encoder mapping
    CMD_SET_ENCODER_MAP = 0x05,    // Set encoder mapping
    CMD_SAVE_CONFIG = 0x06,        // Save to EEPROM
    CMD_LOAD_CONFIG = 0x07,        // Load from EEPROM  
    CMD_RESET_CONFIG = 0x08,       // Reset to defaults
    CMD_GET_I2C_DEVICES = 0x09,    // Get connected I2C devices
    CMD_SET_I2C_CONFIG = 0x0A,     // Configure I2C device
    CMD_GET_DEVICE_STATUS = 0x0B,  // Get device status
    CMD_REBOOT = 0x0C,             // Reboot device (fixed to match Tauri)
    
    // MIDI commands
    CMD_MIDI_SEND_RAW = 0x0D,      // Send raw 4-byte USB-MIDI packet (payload: 4 bytes)
    CMD_MIDI_NOTE_ON = 0x0E,       // Send MIDI Note On (payload: channel(1), note(1), velocity(1))
    CMD_MIDI_NOTE_OFF = 0x0F,      // Send MIDI Note Off (payload: channel(1), note(1))
    CMD_MIDI_CC = 0x10,            // Send MIDI Control Change (payload: channel(1), controller(1), value(1))
    
    // Slave device commands
    CMD_GET_SLAVE_KEYMAP = 0x11,   // Get keymap from slave device (payload: address(1), row(1), col(1))
    CMD_SET_SLAVE_KEYMAP = 0x12,   // Set keymap on slave device (payload: address(1), row(1), col(1), keycode(2))
    CMD_GET_SLAVE_INFO = 0x13,     // Get detailed info from slave device (payload: address(1))
    CMD_GET_SLAVE_ENCODER = 0x14,  // Get encoder mapping from slave (payload: address(1), encoder_id(1))
    CMD_SET_SLAVE_ENCODER = 0x15,  // Set encoder mapping on slave (payload: address(1), encoder_id(1), keycodes(4))
    CMD_GET_LAYOUT_INFO = 0x16,    // Retrieve static board layout metadata
    CMD_SET_LAYER_STATE = 0x17,    // Update active layer mask/default
    CMD_GET_LAYER_STATE = 0x18,    // Query active layer mask/default
    CMD_ASSIGN_I2C_ADDRESS = 0x19, // Assign new I2C address to slave (payload: new_address(1))
    CMD_GOTO_BOOTSTRAP = 0x1A      // Tell slave to switch to bootstrap address (payload: none)
} config_command_t;

// Response status codes
typedef enum {
    STATUS_OK = 0x00,
    STATUS_ERROR = 0x01,
    STATUS_INVALID_CMD = 0x02,
    STATUS_INVALID_PARAM = 0x03,
    STATUS_BUSY = 0x04,
    STATUS_NOT_SUPPORTED = 0x05
} config_status_t;

// Packet structure (64 bytes total)
typedef struct {
    uint16_t header;           // CONFIG_PACKET_HEADER
    uint8_t command;          // config_command_t
    uint8_t status;           // config_status_t (in response)
    uint8_t sequence;         // Sequence number for multi-packet transfers
    uint8_t payload_length;   // Length of payload data
    uint8_t reserved[2];      // Reserved for future use
    uint8_t payload[CONFIG_MAX_PAYLOAD_SIZE]; // Command-specific data
} __attribute__((packed)) config_packet_t;

// Device information response payload
typedef struct {
    uint8_t protocol_version;
    uint8_t firmware_version_major;
    uint8_t firmware_version_minor;
    uint8_t firmware_version_patch;
    uint8_t device_type;      // 0=Master, 1=Slave
    uint8_t matrix_rows;
    uint8_t matrix_cols;
    uint8_t encoder_count;
    uint8_t layer_count;
    uint8_t i2c_devices;      // Number of connected I2C devices
    char device_name[32];     // Device name string
    uint8_t reserved[14];     // Padding to 56 bytes
} __attribute__((packed)) device_info_t;

// Keymap entry
typedef struct {
    uint8_t layer;
    uint8_t row;
    uint8_t col;
    uint16_t keycode;
} __attribute__((packed)) keymap_entry_t;

// Encoder mapping entry  
typedef struct {
    uint8_t layer;
    uint8_t encoder_id;
    uint16_t ccw_keycode;
    uint16_t cw_keycode;
    uint8_t reserved;
} __attribute__((packed)) encoder_entry_t;

// Public API functions
void config_protocol_init(void);
void config_protocol_task(void);
bool config_protocol_send_response(const config_packet_t *packet);
bool config_protocol_process_packet(const config_packet_t *packet);

// HID interface callbacks  
void config_protocol_hid_receive(uint8_t const* buffer, uint16_t bufsize);
bool config_protocol_hid_ready(void);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_PROTOCOL_H */