#ifndef SLIDER_H
#define SLIDER_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32g4xx_hal.h"

// Include keyboard config to get slider definitions
#ifdef KEYBOARD_CONFIG_HEADER
    #include KEYBOARD_CONFIG_HEADER
#endif

#ifndef SLIDER_COUNT
#define SLIDER_COUNT 0
#endif

// Moving average buffer size for noise reduction
#ifndef SLIDER_SMOOTHING_BUFFER_SIZE
#define SLIDER_SMOOTHING_BUFFER_SIZE 8
#endif

// Slider state tracking
typedef struct {
    float smoothed_pct;         // Smoothed percentage value
    uint8_t last_displayed;     // Last displayed percentage
    uint8_t last_midi_value;    // Last MIDI value sent
    bool initialized;           // Whether this slider has been initialized
    uint16_t readings[SLIDER_SMOOTHING_BUFFER_SIZE];  // Moving average buffer for ADC readings
    uint8_t read_index;         // Current position in the buffer
    uint32_t total;             // Running total for moving average
    uint16_t average;           // Current average ADC value
} slider_state_t;

// Initialize slider subsystem
void slider_init(void);

// Scan all sliders and send MIDI CC messages on change
void slider_scan(void);

// Get current percentage value for a slider (0-100)
uint8_t slider_get_percent(uint8_t slider_id);

// Get current MIDI value for a slider
uint8_t slider_get_midi_value(uint8_t slider_id);

// Get current raw slider value for polling (bypasses filtering)
uint8_t slider_get_current_raw_value(uint8_t slider_id);

#endif // SLIDER_H
