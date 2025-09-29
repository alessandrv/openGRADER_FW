# OpenGrader Modular Keyboard - Real-time Configuration

This project implements a real-time configuration system for the OpenGrader modular keyboard using STM32G474 and a Tauri-based desktop application.

## Features

### STM32 Firmware Features
- **Real-time Keymap Configuration**: Change key assignments on-the-fly via USB HID
- **Encoder Configuration**: Configure rotary encoder mappings in real-time
- **EEPROM Emulation**: Persistent storage using internal flash memory
- **USB HID Protocol**: Custom configuration protocol over HID interface
- **Multiple USB Interfaces**: Keyboard, Mouse, MIDI, CDC, and Configuration HID

### Desktop Application Features
- **Device Discovery**: Automatic detection of OpenGrader devices
- **Visual Keymap Editor**: Interactive matrix layout for key assignment
- **Encoder Configuration**: Easy setup for rotary encoder mappings
- **Configuration Management**: Save, load, and reset configurations
- **Real-time Updates**: Changes are applied immediately to the keyboard

## Architecture

### STM32 Implementation
- **EEPROM Emulation** (`eeprom_emulation.c/h`): Flash-based persistent storage
- **Configuration Protocol** (`config_protocol.c/h`): USB HID communication protocol
- **Keymap Management** (`keymap.c/h`): Runtime keymap modification support
- **USB Descriptors** (`usb_descriptors.c`): Multi-interface USB device

### Tauri Application
- **Rust Backend**: HID communication and protocol handling
- **Svelte Frontend**: Modern web-based user interface
- **Real-time Communication**: Direct USB HID communication with keyboard

## Protocol

The configuration protocol uses a 64-byte HID packet structure:

```c
typedef struct {
    uint16_t header;           // 0x4F47 ("OG")
    uint8_t command;          // Command type
    uint8_t status;           // Response status
    uint8_t sequence;         // Sequence number
    uint8_t payload_length;   // Payload size
    uint8_t reserved[2];      // Reserved
    uint8_t payload[60];      // Command data
} config_packet_t;
```

### Supported Commands
- `CMD_GET_INFO`: Get device information
- `CMD_GET_KEYMAP`/`CMD_SET_KEYMAP`: Read/write keymap entries
- `CMD_GET_ENCODER_MAP`/`CMD_SET_ENCODER_MAP`: Read/write encoder mappings
- `CMD_SAVE_CONFIG`: Save configuration to EEPROM
- `CMD_LOAD_CONFIG`: Load configuration from EEPROM
- `CMD_RESET_CONFIG`: Reset to factory defaults

## EEPROM Storage

Configuration data is stored in the last 4KB of flash memory with:
- **Magic Number**: 0x4F47454D ("OGEM")
- **Version Control**: For future migration support
- **CRC32 Checksum**: Data integrity verification
- **Keymap Data**: Full matrix configuration
- **Encoder Data**: All encoder mappings

## Usage

### Building STM32 Firmware
1. Open the project in STM32CubeIDE or use CMake
2. Build and flash to STM32G474VET6
3. The device will appear as multiple USB interfaces

### Running Desktop Application
1. Navigate to the configurator directory:
   ```bash
   cd OpenGRADER_config/opengrader-configurator
   ```
2. Install dependencies:
   ```bash
   npm install
   ```
3. Run in development mode:
   ```bash
   npm run tauri dev
   ```
4. Or build for production:
   ```bash
   npm run tauri build
   ```

### Configuration Workflow
1. **Connect Device**: Scan and connect to your OpenGrader keyboard
2. **Edit Keymap**: Click keys in the matrix to assign new functions
3. **Configure Encoders**: Set clockwise/counter-clockwise actions
4. **Save Configuration**: Store settings to EEPROM for persistence
5. **Test Changes**: All changes are applied immediately

## Key Features

### Real-time Updates
- Changes are applied instantly without requiring a reboot
- Visual feedback in the desktop application
- Persistent storage ensures settings survive power cycles

### Modular Design
- Support for variable matrix sizes (configured per device)
- Up to 25 encoders supported
- Extensible protocol for future features

### Safety Features
- CRC32 checksums prevent corruption
- Fallback to defaults if EEPROM is invalid
- Non-blocking configuration updates

## Development Notes

### Adding New Keycodes
1. Update `op_keycodes.h` in STM32 project
2. Update `keycodes.rs` in Tauri application
3. Rebuild both firmware and application

### Extending the Protocol
1. Add new command types to `config_protocol.h`
2. Implement handlers in `config_protocol.c`
3. Add corresponding Rust functions in `hid_manager.rs`
4. Update UI in Svelte components

## Troubleshooting

### Device Not Detected
- Ensure STM32 firmware is properly flashed
- Check USB cable and connection
- Verify device appears in system device manager

### Configuration Not Saving
- Check EEPROM initialization in debug output
- Verify flash memory regions are not write-protected
- Ensure sufficient flash space for configuration data

### Communication Errors
- Check HID interface is properly enumerated
- Verify packet structure matches between firmware and application
- Monitor CDC output for debug messages