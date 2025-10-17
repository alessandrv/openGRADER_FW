#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

#include "stm32g4xx_hal.h"
#include "input/matrix.h"
#include "input/encoder.h"

#define ENCODER_COUNT 25

/* Matrix pin definitions */
// Matrix columns: [D1, D0, D2, D3, C11, B9, A3]
#define MATRIX_COL_PINS { \
    {GPIOD, GPIO_PIN_1}, /* D1 */ \
    {GPIOD, GPIO_PIN_0}, /* D0 */ \
    {GPIOD, GPIO_PIN_2}, /* D2 */ \
    {GPIOD, GPIO_PIN_3}, /* D3 */ \
    {GPIOC, GPIO_PIN_11},/* C11 */ \
    {GPIOB, GPIO_PIN_9}, /* B9 */ \
    {GPIOA, GPIO_PIN_3}  /* A3 */ \
}

// Matrix rows: [D4, C12, D14, E11, E10]
#define MATRIX_ROW_PINS { \
    {GPIOD, GPIO_PIN_4}, /* D4 */ \
    {GPIOC, GPIO_PIN_12},/* C12 */ \
    {GPIOD, GPIO_PIN_14},/* D14 */ \
    {GPIOE, GPIO_PIN_11},/* E11 */ \
    {GPIOE, GPIO_PIN_10} /* E10 */ \
}

// Matrix row pull configuration: default pulldown on row pins (diode col->row)
#define MATRIX_ROW_PULL_CONFIG { \
    GPIO_PULLDOWN, \
    GPIO_PULLDOWN, \
    GPIO_PULLDOWN, \
    GPIO_PULLDOWN, \
    GPIO_PULLDOWN \
}

/* Encoder pin definitions */
// Encoders: structured with PINA and PINB in a single data structure
#define ENCODER_PIN_CONFIG { \
    {{GPIOD, GPIO_PIN_7}, {GPIOD, GPIO_PIN_7}},   /* 1: D5, D6 */ \
    {{GPIOD, GPIO_PIN_7}, {GPIOB, GPIO_PIN_3}},   /* 2: D7, B3 */ \
    {{GPIOB, GPIO_PIN_4}, {GPIOB, GPIO_PIN_5}},   /* 3: B4, B5 */ \
    {{GPIOB, GPIO_PIN_6}, {GPIOB, GPIO_PIN_6}},   /* 4: B6, B7 */ \
    {{GPIOE, GPIO_PIN_0}, {GPIOE, GPIO_PIN_1}},   /* 5: E0, E1 */ \
    {{GPIOD, GPIO_PIN_13}, {GPIOD, GPIO_PIN_12}}, /* 6: D13, D12 */ \
    {{GPIOB, GPIO_PIN_12}, {GPIOB, GPIO_PIN_13}}, /* 7: B12, B13 */ \
    {{GPIOC, GPIO_PIN_0}, {GPIOE, GPIO_PIN_6}},   /* 8: C0, E6 */ \
    {{GPIOE, GPIO_PIN_5}, {GPIOE, GPIO_PIN_4}},   /* 9: E5, E4 */ \
    {{GPIOE, GPIO_PIN_3}, {GPIOE, GPIO_PIN_2}},   /* 10: E3, E2 */ \
    {{GPIOD, GPIO_PIN_11}, {GPIOD, GPIO_PIN_10}}, /* 11: D11, D10 */ \
    {{GPIOD, GPIO_PIN_9}, {GPIOD, GPIO_PIN_8}},   /* 12: D9, D8 */ \
    {{GPIOA, GPIO_PIN_2}, {GPIOA, GPIO_PIN_1}},   /* 13: A2, A1 */ \
    {{GPIOA, GPIO_PIN_0}, {GPIOC, GPIO_PIN_3}},   /* 14: A0, C3 */ \
    {{GPIOC, GPIO_PIN_2}, {GPIOC, GPIO_PIN_1}},   /* 15: C2, C1 */ \
    {{GPIOB, GPIO_PIN_15}, {GPIOB, GPIO_PIN_14}}, /* 16: B15, B14 */ \
    {{GPIOB, GPIO_PIN_11}, {GPIOB, GPIO_PIN_10}}, /* 17: B11, B10 */ \
    {{GPIOC, GPIO_PIN_5}, {GPIOC, GPIO_PIN_4}},   /* 18: C5, C4 */ \
    {{GPIOA, GPIO_PIN_7}, {GPIOA, GPIO_PIN_6}},   /* 19: A7, A6 */ \
    {{GPIOA, GPIO_PIN_5}, {GPIOA, GPIO_PIN_4}},   /* 20: A5, A4 */ \
    {{GPIOE, GPIO_PIN_15}, {GPIOE, GPIO_PIN_14}}, /* 21: E15, E14 */ \
    {{GPIOE, GPIO_PIN_13}, {GPIOE, GPIO_PIN_12}}, /* 22: E13, E12 */ \
    {{GPIOE, GPIO_PIN_9}, {GPIOE, GPIO_PIN_8}},   /* 23: E9, E8 */ \
    {{GPIOE, GPIO_PIN_7}, {GPIOB, GPIO_PIN_2}},   /* 24: E7, B2 */ \
    {{GPIOB, GPIO_PIN_1}, {GPIOB, GPIO_PIN_0}},   /* 25: B1, B0 */ \
}

#define ENCODER_PULL_CONFIG { \
    GPIO_PULLUP, GPIO_PULLUP, GPIO_PULLUP, GPIO_PULLUP, GPIO_PULLUP, \
    GPIO_PULLUP, GPIO_PULLUP, GPIO_PULLUP, GPIO_PULLUP, GPIO_PULLUP, \
    GPIO_PULLUP, GPIO_PULLUP, GPIO_PULLUP, GPIO_PULLUP, GPIO_PULLUP, \
    GPIO_PULLUP, GPIO_PULLUP, GPIO_PULLUP, GPIO_PULLUP, GPIO_PULLUP, \
    GPIO_PULLUP, GPIO_PULLUP, GPIO_PULLUP, GPIO_PULLUP, GPIO_PULLUP \
}

#endif /* PIN_CONFIG_H */