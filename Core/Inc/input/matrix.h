#ifndef MATRIX_H
#define MATRIX_H

#include "main.h"
#include <stdint.h>

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} pin_t;

#define MATRIX_ROWS 5
#define MATRIX_COLS 7

typedef void (*matrix_event_cb_t)(uint8_t row, uint8_t col, uint8_t pressed, uint8_t keycode);

void matrix_init(void);
void matrix_scan(void);
void matrix_register_callback(matrix_event_cb_t cb);

#endif // MATRIX_H
