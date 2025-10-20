#ifndef KEYBOARD_CONFIG_H
#define KEYBOARD_CONFIG_H

#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_adc.h"

/* Keyboard name */
#define KEYBOARD_NAME "OpenGrader Mixed Controls"

/* Matrix configuration - 1 potentiometer + 1 slider + 3 switches + 1 encoder */
#define MATRIX_ROWS 5
#define MATRIX_COLS 2

/* Pin definitions */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} pin_t;

/* Encoder pin type */
typedef struct {
    pin_t pin_a;
    pin_t pin_b;
} encoder_pins_t;

/* Matrix pin definitions - for switches */
#define MATRIX_COL_PINS { {GPIOB, GPIO_PIN_0}, {GPIOB, GPIO_PIN_1} }
#define MATRIX_ROW_PINS { {GPIOB, GPIO_PIN_2}, {GPIOB, GPIO_PIN_3}, {GPIOB, GPIO_PIN_4}, {GPIOB, GPIO_PIN_5}, {GPIOB, GPIO_PIN_8} }
#define MATRIX_ROW_PULL_CONFIG { GPIO_PULLDOWN, GPIO_PULLDOWN, GPIO_PULLDOWN, GPIO_PULLDOWN, GPIO_PULLDOWN }

/* Encoder configuration - 1 encoder */
#define ENCODER_COUNT 1
#define ENCODER_PIN_CONFIG { \
    { {GPIOB, GPIO_PIN_6}, {GPIOB, GPIO_PIN_7} } \
}
#define ENCODER_PULL_CONFIG { GPIO_PULLUP }

/* Slider configuration (potentiometers use same backend as sliders) */
#define SLIDER_COUNT 2
#define SLIDER_ADC_PINS ADC_PINS

/* Slider hardware configuration structure */
typedef struct {
    ADC_TypeDef *adc;           // ADC instance (e.g., ADC1, ADC2)
    uint32_t channel;           // ADC channel number
    GPIO_TypeDef *gpio_port;    // GPIO port for the pin
    uint32_t gpio_pin;          // GPIO pin number
    uint8_t midi_cc;            // MIDI CC number to send
    uint8_t midi_channel;       // MIDI channel (0-15)
    uint16_t value_at_zero;     // ADC value that maps to 0%
    uint16_t value_at_full;     // ADC value that maps to 100%
    uint8_t min_midi_value;     // Minimum MIDI value to send (0-127)
    uint8_t max_midi_value;     // Maximum MIDI value to send (0-127)
} slider_hw_config_t;

/* Potentiometer and Slider pin definitions (reuse slider infrastructure) */
// Potentiometer 0: ADC1_IN4 on PA3  
// Slider 1: ADC1_IN3 on PA2
static const slider_hw_config_t slider_configs[SLIDER_COUNT] = {
    {
        .adc = ADC1,
        .channel = ADC_CHANNEL_4,
        .gpio_port = GPIOA,
        .gpio_pin = GPIO_PIN_3,
        .midi_cc = 1,
        .midi_channel = 0,
        .value_at_zero = 70,
        .value_at_full = 4095,
        .min_midi_value = 0,
        .max_midi_value = 127
    },
    {
        .adc = ADC1,
        .channel = ADC_CHANNEL_3,
        .gpio_port = GPIOA,
        .gpio_pin = GPIO_PIN_2,
        .midi_cc = 2,
        .midi_channel = 0,
        .value_at_zero = 70,
        .value_at_full = 4095,
        .min_midi_value = 0,
        .max_midi_value = 127
    }
};

// Physical matrix layout - column 1: potentiometer, switches, encoder; column 2: slider
#define KEYBOARD_LAYOUT { \
    { LAYOUT_CELL(LAYOUT_POTENTIOMETER, 0), LAYOUT_CELL(LAYOUT_SLIDER, 1) }, \
    { LAYOUT_CELL(LAYOUT_SWITCH, 2), LAYOUT_CELL(LAYOUT_EMPTY, 0) }, \
    { LAYOUT_CELL(LAYOUT_SWITCH, 3), LAYOUT_CELL(LAYOUT_EMPTY, 0) }, \
    { LAYOUT_CELL(LAYOUT_SWITCH, 4), LAYOUT_CELL(LAYOUT_EMPTY, 0) }, \
    { LAYOUT_CELL(LAYOUT_ENCODER, 5), LAYOUT_CELL(LAYOUT_EMPTY, 0) } \
}

// Pin definitions for ADC (same pins, different controls)
#define ADC_PIN_COUNT 2
#define ADC_PINS { \
    ADC_CHANNEL_4, /* A3 - Potentiometer 0 */ \
    ADC_CHANNEL_3  /* A2 - Slider 1 */ \
}

// Slider configuration (potentiometers use same backend as sliders)
#define SLIDER_COUNT 2
#define SLIDER_ADC_PINS ADC_PINS

#endif // KEYBOARD_CONFIG_H