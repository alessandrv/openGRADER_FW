# Multi-Keyboard Layout System Implementation

## Summary

Successfully implemented a QMK-style multi-keyboard layout system for the OpenGrader firmware. The system allows you to define multiple keyboard configurations (layouts, pins, encoders) that share the same core firmware logic.

## What Was Done

### 1. Created Keyboard Directory Structure

```
openGRADER_FW/keyboards/
├── README.md                     # Documentation for keyboard system
├── standard/                     # Full 5x7 keyboard with 25 encoders
│   ├── config.h                 # Hardware configuration
│   └── keymap.c                 # Default keymap
└── onekey/                      # Minimal 1x1 keyboard for testing  
    ├── config.h                 # Hardware configuration
    └── keymap.c                 # Default keymap
```

### 2. Modified Core Files

#### CMakeLists.txt
- Added `KEYBOARD` variable (defaults to "standard")
- Automatically includes keyboard-specific keymap.c
- Defines `KEYBOARD_CONFIG_HEADER` macro for compile-time configuration
- Added keyboards directory to include paths

#### Core/Inc/input/matrix.h
- Made MATRIX_ROWS and MATRIX_COLS configurable via keyboard config
- Includes keyboard config header dynamically
- Provides defaults if no keyboard selected

#### Core/Inc/pin_config.h
- Simplified to just include the keyboard-specific config
- No longer hardcodes pin definitions

#### Core/Src/input/keymap.c
- Removed hardcoded keycode and encoder arrays
- Arrays now defined in keyboard-specific keymap.c files
- Added conditional compilation for keyboards without encoders

#### Core/Inc/input/keymap.h
- Added support for variable encoder counts (including 0)
- Made encoder-related functions handle ENCODER_COUNT=0 gracefully

### 3. Keyboard Configurations

#### Standard Keyboard (`keyboards/standard/`)
- **Matrix**: 5 rows × 7 columns (35 keys)
- **Encoders**: 25 rotary encoders
- **Pins**: Full GPIO configuration as before
- **Layers**: 8 layers with comprehensive keymap

#### OneKey Keyboard (`keyboards/onekey/`)
- **Matrix**: 1 row × 1 column (1 key)
- **Encoders**: 0 encoders
- **Pins**: Minimal - A5 for column, A6 for row
- **Layers**: 8 layers, each with a different single key (A, 1, Space, Enter, Esc, MIDI Note, Layer Toggle, Empty)

## How to Use

### Building for a Specific Keyboard

```powershell
# Build for standard keyboard (default)
cmake --preset Debug
cmake --build --preset Debug

# Build for onekey keyboard
cmake --preset Debug -DKEYBOARD=onekey
cmake --build --preset Debug

# Or use the provided script
.\build_keyboard.ps1 onekey
.\build_keyboard.ps1 standard clean
```

### Creating a New Keyboard

1. Create directory: `keyboards/myboard/`

2. Create `config.h`:
```c
#ifndef KEYBOARD_CONFIG_H
#define KEYBOARD_CONFIG_H

#include "stm32g4xx_hal.h"

#define KEYBOARD_NAME "My Custom Board"
#define MATRIX_ROWS 3
#define MATRIX_COLS 4
#define ENCODER_COUNT 2

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} pin_t;

typedef struct {
    pin_t pin_a;
    pin_t pin_b;
} encoder_pins_t;

#define MATRIX_COL_PINS { ... }
#define MATRIX_ROW_PINS { ... }
#define MATRIX_ROW_PULL_CONFIG { ... }

#if ENCODER_COUNT > 0
#define ENCODER_PIN_CONFIG { ... }
#define ENCODER_PULL_CONFIG { ... }
#endif

#endif
```

3. Create `keymap.c`:
```c
#include "input/keymap.h"
#include "op_keycodes.h"

const uint16_t keycodes[KEYMAP_LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS] = {
    // Define keymaps
};

#if ENCODER_COUNT > 0
const uint16_t encoder_map[KEYMAP_LAYER_COUNT][ENCODER_COUNT][2] = {
    // Define encoder mappings
};
#else
const uint16_t encoder_map[KEYMAP_LAYER_COUNT][1][2] = {
    [0 ... KEYMAP_LAYER_COUNT-1] = {{ KC_TRANSPARENT, KC_TRANSPARENT }}
};
#endif
```

4. Build:
```powershell
cmake --preset Debug -DKEYBOARD=myboard
cmake --build --preset Debug
```

## Technical Details

### Compile-Time Configuration

The system uses C preprocessor macros to select the keyboard configuration at compile time:

1. CMake defines `KEYBOARD_CONFIG_HEADER=<keyboard/config.h>`
2. `matrix.h` includes this header, which defines:
   - `MATRIX_ROWS` and `MATRIX_COLS`
   - `ENCODER_COUNT`
   - Pin type definitions (`pin_t`, `encoder_pins_t`)
   - Pin mapping macros
3. All other files use these definitions

### Zero-Encoder Support

The system gracefully handles keyboards without encoders:

- `#if ENCODER_COUNT > 0` guards encoder-specific code
- Minimal stub arrays provided when `ENCODER_COUNT == 0`
- Encoder functions return early/safely when no encoders exist

### Memory Impact

Compile-time configuration means:
- No runtime overhead
- Only the active keyboard's data is included in the binary
- Smaller binaries for simpler keyboards:
  - **Standard**: 89,316 bytes FLASH (17.04%)
  - **OneKey**: 83,880 bytes FLASH (16.00%) - 5.4 KB saved!

## Benefits

1. **Modularity**: Easy to create new keyboard variants
2. **Code Reuse**: All logic shared, only configs differ
3. **Type Safety**: Compile-time checks prevent size mismatches
4. **No Runtime Cost**: Zero overhead vs. hardcoded configs
5. **QMK-like**: Familiar structure for keyboard firmware developers

## Files Modified

### Core Firmware
- `CMakeLists.txt` - Build system updates
- `Core/Inc/input/matrix.h` - Dynamic matrix dimensions
- `Core/Inc/input/keymap.h` - Variable encoder support
- `Core/Inc/pin_config.h` - Keyboard config inclusion
- `Core/Src/input/keymap.c` - Externalized keymap arrays

### New Files
- `keyboards/README.md` - Documentation
- `keyboards/standard/config.h` - Standard keyboard config
- `keyboards/standard/keymap.c` - Standard keyboard keymap
- `keyboards/onekey/config.h` - OneKey keyboard config  
- `keyboards/onekey/keymap.c` - OneKey keyboard keymap
- `build_keyboard.ps1` - Build helper script

## Testing

Both keyboards build successfully:
- ✅ Standard keyboard: 89,316 bytes FLASH
- ✅ OneKey keyboard: 83,880 bytes FLASH  
- ✅ No compilation errors or warnings
- ✅ Proper conditional compilation for encoders

## Next Steps

Potential enhancements:
1. Create more keyboard variants (numpad, macro pad, etc.)
2. Add keyboard-specific features (RGB patterns, display configs)
3. Create CMake presets for each keyboard
4. Add validation scripts to check keyboard configs
5. Support runtime keyboard detection (if needed)

