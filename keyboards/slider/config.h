#ifndef KEYBOARD_CONFIG_H
#define KEYBOARD_CONFIG_H

#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_adc.h"

/* Keyboard name */
#define KEYBOARD_NAME "OpenGrader Slider"

/* Matrix configuration - switches + dual sliders */
#define MATRIX_ROWS 5
#define MATRIX_COLS 3

/* Encoder configuration */
#define ENCODER_COUNT 0

/* Slider configuration */
#define SLIDER_COUNT 2

/* Layout definition */
#include "input/board_layout_types.h"

// Slider keyboard layout: 5x3 matrix for switches + dual sliders
// Layout pattern: SWITCH SLIDER SLIDER
//                 SWITCH   -     -    (- means slider continues from above)
//                 SWITCH   -     -
//                 SWITCH   -     -  
//                 SWITCH   -     -
// Note: Sliders are defined only in row 0, empty cells below indicate continuation
//
#define KEYBOARD_LAYOUT { \
    { LAYOUT_CELL(LAYOUT_SWITCH, 0), LAYOUT_CELL(LAYOUT_SLIDER, 0), LAYOUT_CELL(LAYOUT_SLIDER, 1) }, \
    { LAYOUT_CELL(LAYOUT_SWITCH, 1), LAYOUT_CELL(LAYOUT_EMPTY, 0),  LAYOUT_CELL(LAYOUT_EMPTY, 0) }, \
    { LAYOUT_CELL(LAYOUT_SWITCH, 2), LAYOUT_CELL(LAYOUT_EMPTY, 0),  LAYOUT_CELL(LAYOUT_EMPTY, 0) }, \
    { LAYOUT_CELL(LAYOUT_SWITCH, 3), LAYOUT_CELL(LAYOUT_EMPTY, 0),  LAYOUT_CELL(LAYOUT_EMPTY, 0) }, \
    { LAYOUT_CELL(LAYOUT_SWITCH, 4), LAYOUT_CELL(LAYOUT_EMPTY, 0),  LAYOUT_CELL(LAYOUT_EMPTY, 0) } \
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

/* Slider pin definitions */
// Slider 0: ADC1_IN4 on PA3
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

/* Smoothing configuration - optimized for responsive MIDI output */
#define SLIDER_EMA_ALPHA 0.4f           // More responsive smoothing (higher = more responsive)
#define SLIDER_FAST_CHANGE_THRESHOLD 0.5f // Lower threshold for faster response to movement
#define SLIDER_SAMPLE_INTERVAL_MS 5     // Sample every 5ms (200Hz) for very responsive feedback
#define SLIDER_MIDI_HYSTERESIS 1         // MIDI values must change by at least this much to prevent oscillation

/* Matrix pin definitions - switches + dual sliders */
#define MATRIX_COL_PINS { \
    {GPIOD, GPIO_PIN_1}, /* D1 - Column 0 (switches) */ \
    {GPIOD, GPIO_PIN_2}, /* D2 - Column 1 (slider 0) */ \
    {GPIOD, GPIO_PIN_3}  /* D3 - Column 2 (slider 1) */ \
}

#define MATRIX_ROW_PINS { \
    {GPIOD, GPIO_PIN_4}, /* D4 - Row 0 */ \
    {GPIOD, GPIO_PIN_5}, /* D5 - Row 1 */ \
    {GPIOD, GPIO_PIN_6}, /* D6 - Row 2 */ \
    {GPIOD, GPIO_PIN_7}, /* D7 - Row 3 */ \
    {GPIOD, GPIO_PIN_8}  /* D8 - Row 4 */ \
}

#define MATRIX_ROW_PULL_CONFIG { \
    GPIO_PULLDOWN, \
    GPIO_PULLDOWN, \
    GPIO_PULLDOWN, \
    GPIO_PULLDOWN, \
    GPIO_PULLDOWN \
}

#endif /* KEYBOARD_CONFIG_H */
