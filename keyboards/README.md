# Keyboard Layouts

This directory contains keyboard-specific configurations similar to QMK's keyboard structure. Each keyboard variant shares the same core firmware logic but has different pin configurations and layouts.

## Directory Structure

Each keyboard has its own subdirectory with the following files:

- `config.h` - Hardware configuration (pin mappings, matrix dimensions, encoder count)
- `keymap.c` - Default keymap and encoder mappings

## Available Keyboards

### standard
The full OpenGrader keyboard with:
- 5x7 key matrix (35 keys)
- 25 encoders
- Complete pin configuration for all GPIOs

### onekey
A minimal 1-key keyboard for testing:
- 1x1 key matrix (1 key)
- 0 encoders
- Single GPIO configuration (D1 for column, D4 for row)

## Building for a Specific Keyboard

### Using CMake Command Line

To build for a specific keyboard, use the `-DKEYBOARD=<name>` option:

```bash
# Build for standard keyboard (default)
cmake -B build -DKEYBOARD=standard
cmake --build build

# Build for onekey keyboard
cmake -B build -DKEYBOARD=onekey
cmake --build build
```

### Using CMake Presets

You can also add keyboard-specific presets to `CMakePresets.json`:

```json
{
  "configurePresets": [
    {
      "name": "onekey-debug",
      "inherits": "debug",
      "cacheVariables": {
        "KEYBOARD": "onekey"
      }
    }
  ]
}
```

## Creating a New Keyboard

To create a new keyboard layout:

1. Create a new directory under `keyboards/` with your keyboard name (e.g., `keyboards/myboard/`)

2. Create `config.h` with your hardware configuration:
   ```c
   #ifndef KEYBOARD_CONFIG_H
   #define KEYBOARD_CONFIG_H
   
   #include "stm32g4xx_hal.h"
   #include "input/matrix.h"
   #include "input/encoder.h"
   
   #define KEYBOARD_NAME "My Custom Board"
   #define MATRIX_ROWS <number>
   #define MATRIX_COLS <number>
   #define ENCODER_COUNT <number>
   
   #define MATRIX_COL_PINS { ... }
   #define MATRIX_ROW_PINS { ... }
   #define MATRIX_ROW_PULL_CONFIG { ... }
   #define ENCODER_PIN_CONFIG { ... }
   #define ENCODER_PULL_CONFIG { ... }
   
   #endif
   ```

3. Create `keymap.c` with your default keymap:
   ```c
   #include "input/keymap.h"
   #include "op_keycodes.h"
   
   const uint16_t keycodes[KEYMAP_LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS] = {
       // Define your layers here
   };
   
   #if ENCODER_COUNT > 0
   const uint16_t encoder_map[KEYMAP_LAYER_COUNT][ENCODER_COUNT][2] = {
       // Define encoder mappings
   };
   #else
   const uint16_t encoder_map[KEYMAP_LAYER_COUNT][1][2] = {
       [0 ... KEYMAP_LAYER_COUNT-1] = {
           { KC_TRANSPARENT, KC_TRANSPARENT }
       }
   };
   #endif
   ```

4. Build with your keyboard:
   ```bash
   cmake -B build -DKEYBOARD=myboard
   cmake --build build
   ```

## Notes

- The default keyboard is `standard` if no keyboard is specified
- Matrix dimensions and encoder count are defined per-keyboard
- All keyboards share the same layer count (8 layers by default)
- Pin definitions use the STM32 HAL GPIO format: `{GPIOx, GPIO_PIN_x}`
- For keyboards without encoders, set `ENCODER_COUNT 0` and provide empty configs
