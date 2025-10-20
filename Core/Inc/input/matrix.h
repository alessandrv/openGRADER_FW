#ifndef MATRIX_H
#define MATRIX_H

#include "main.h"
#include <stdint.h>

// Matrix dimensions and pin types are defined by the keyboard config
// Include the keyboard config to get MATRIX_ROWS, MATRIX_COLS, and pin_t
#ifdef KEYBOARD_CONFIG_HEADER
    #include KEYBOARD_CONFIG_HEADER
#else
    // Default values and types if no keyboard is selected
    typedef struct {
        GPIO_TypeDef *port;
        uint16_t pin;
    } pin_t;
    
    #ifndef MATRIX_ROWS
        #define MATRIX_ROWS 5
    #endif
    #ifndef MATRIX_COLS
        #define MATRIX_COLS 7
    #endif
#endif

typedef void (*matrix_event_cb_t)(uint8_t row, uint8_t col, uint8_t pressed, uint8_t keycode);

void matrix_init(void);
void matrix_scan(void);
void matrix_register_callback(matrix_event_cb_t cb);

#endif // MATRIX_H
