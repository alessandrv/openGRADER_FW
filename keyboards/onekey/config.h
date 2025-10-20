#ifndef KEYBOARD_CONFIG_H
#define KEYBOARD_CONFIG_H

#include "stm32g4xx_hal.h"

/* Keyboard name */
#define KEYBOARD_NAME "OpenGrader OneKey"

/* Matrix configuration */
#define MATRIX_ROWS 1
#define MATRIX_COLS 1

/* Encoder configuration */
#define ENCODER_COUNT 0

/* Layout definition */
#include "input/board_layout_types.h"

// OneKey keyboard layout: 1x1 matrix
// Layout pattern: SW
//
#define KEYBOARD_LAYOUT { \
    { LAYOUT_CELL(SW, 0) } \
}

/* Pin type definition */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} pin_t;

/* Encoder pin type */
typedef struct {
    pin_t pin_a;
    pin_t pin_b;
} encoder_pins_t;

/* Matrix pin definitions */
// Matrix columns: Single column on D1
#define MATRIX_COL_PINS { \
    {GPIOA, GPIO_PIN_5}  /* D1 */ \
}

// Matrix rows: Single row on D4
#define MATRIX_ROW_PINS { \
    {GPIOA, GPIO_PIN_6}  /* D4 */ \
}

// Matrix row pull configuration: default pulldown on row pins (diode col->row)
#define MATRIX_ROW_PULL_CONFIG { \
    GPIO_PULLDOWN \
}

/* Encoder pin definitions - empty for onekey */
#define ENCODER_PIN_CONFIG {}

#define ENCODER_PULL_CONFIG {}

#endif /* KEYBOARD_CONFIG_H */
