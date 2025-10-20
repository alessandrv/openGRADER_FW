# Quick Reference: Multi-Keyboard System

## Build Commands

```powershell
# Standard keyboard (default)
cmake --preset Debug
cmake --build --preset Debug

# OneKey keyboard
cmake --preset Debug -DKEYBOARD=onekey
cmake --build --preset Debug

# Using the helper script
.\build_keyboard.ps1 standard
.\build_keyboard.ps1 onekey
.\build_keyboard.ps1 onekey clean  # Clean build
```

## Available Keyboards

| Keyboard | Matrix | Encoders | Description |
|----------|--------|----------|-------------|
| `standard` | 5×7 (35 keys) | 25 | Full OpenGrader keyboard |
| `onekey` | 1×1 (1 key) | 0 | Minimal test keyboard |

## Keyboard Configuration Template

### Minimal `config.h`
```c
#ifndef KEYBOARD_CONFIG_H
#define KEYBOARD_CONFIG_H
#include "stm32g4xx_hal.h"

#define KEYBOARD_NAME "My Board"
#define MATRIX_ROWS 3
#define MATRIX_COLS 4  
#define ENCODER_COUNT 2

typedef struct { GPIO_TypeDef *port; uint16_t pin; } pin_t;
typedef struct { pin_t pin_a; pin_t pin_b; } encoder_pins_t;

#define MATRIX_COL_PINS { {GPIOx, GPIO_PIN_y}, ... }
#define MATRIX_ROW_PINS { {GPIOx, GPIO_PIN_y}, ... }
#define MATRIX_ROW_PULL_CONFIG { GPIO_PULLDOWN, ... }

#if ENCODER_COUNT > 0
#define ENCODER_PIN_CONFIG { {{port,pin},{port,pin}}, ... }
#define ENCODER_PULL_CONFIG { GPIO_PULLUP, ... }
#endif

#endif
```

### Minimal `keymap.c`
```c
#include "input/keymap.h"
#include "op_keycodes.h"

const uint16_t keycodes[KEYMAP_LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = { {KC_A, KC_B, KC_C, KC_D}, ... },
    [1 ... KEYMAP_LAYER_COUNT-1] = { {KC_TRANSPARENT, ...}, ... }
};

#if ENCODER_COUNT > 0
const uint16_t encoder_map[KEYMAP_LAYER_COUNT][ENCODER_COUNT][2] = {
    [0] = { {KC_LEFT, KC_RIGHT}, ... },
    [1 ... KEYMAP_LAYER_COUNT-1] = { {KC_TRANSPARENT, KC_TRANSPARENT}, ... }
};
#else
const uint16_t encoder_map[KEYMAP_LAYER_COUNT][1][2] = {
    [0 ... KEYMAP_LAYER_COUNT-1] = {{KC_TRANSPARENT, KC_TRANSPARENT}}
};
#endif
```

## Directory Structure

```
keyboards/
├── <keyboard_name>/
│   ├── config.h        # Hardware: pins, matrix size, encoder count
│   └── keymap.c        # Keymaps and encoder mappings
```

## Common Keycodes

```c
// Basic keys
KC_A to KC_Z, KC_1 to KC_0
KC_ENTER, KC_ESC, KC_BSPC, KC_TAB
KC_SPACE, KC_MINUS, KC_EQUAL

// Modifiers  
KC_LCTL, KC_LSFT, KC_LALT, KC_LGUI
KC_RCTL, KC_RSFT, KC_RALT, KC_RGUI

// Navigation
KC_UP, KC_DOWN, KC_LEFT, KC_RIGHT
KC_HOME, KC_END, KC_PGUP, KC_PGDN

// Function keys
KC_F1 to KC_F12

// Special
KC_NO           // No key
KC_TRANSPARENT  // Fall through to lower layer
KC_MO(n)        // Momentary layer n
KC_TO(n)        // Switch to layer n

// MIDI
SEND_MIDI_NOTE(channel, note)
SEND_MIDI_CC(channel, controller, value)
```

## Troubleshooting

### Build fails with "No such file or directory"
- Make sure keyboard directory exists in `keyboards/`
- Check that `config.h` and `keymap.c` are present
- Verify keyboard name spelling in cmake command

### Conflicting type errors
- Ensure `pin_t` and `encoder_pins_t` are defined in `config.h`
- Check that types are defined before pin macros
- Both types must always be defined (even if ENCODER_COUNT=0)

### Array size mismatch errors
- Verify `MATRIX_ROWS` and `MATRIX_COLS` match your keymap array sizes
- Check that `ENCODER_COUNT` matches encoder_map size
- Use `[0 ... KEYMAP_LAYER_COUNT-1]` for default values

### Encoder-related errors on no-encoder keyboards
- Set `ENCODER_COUNT 0` in config.h
- Use `#if ENCODER_COUNT > 0` guards in keymap.c
- Provide stub encoder_map as shown in template

## Example: Creating a 2x2 Macropad

1. Create `keyboards/macropad/config.h`
2. Set `MATRIX_ROWS 2`, `MATRIX_COLS 2`, `ENCODER_COUNT 1`
3. Define 2 column pins, 2 row pins, 1 encoder
4. Create `keyboards/macropad/keymap.c` with 2x2 keymaps
5. Build: `cmake --preset Debug -DKEYBOARD=macropad`

Ready to flash!
