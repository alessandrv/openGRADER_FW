#ifndef MAGNETIC_SWITCH_H
#define MAGNETIC_SWITCH_H

#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_adc.h"
#include <stdint.h>
#include <stdbool.h>

// Include keyboard config to get magnetic switch definitions
#ifdef KEYBOARD_CONFIG_HEADER
    #include KEYBOARD_CONFIG_HEADER
#endif

// Maximum number of magnetic switches supported
#define MAX_MAGNETIC_SWITCHES 8

// Magnetic switch calibration state
typedef enum {
    MAG_SW_CALIBRATION_NONE = 0,     // Not calibrated
    MAG_SW_CALIBRATION_READY = 1,    // Ready for calibration
    MAG_SW_CALIBRATION_UNPRESSED = 2, // Capturing unpressed value
    MAG_SW_CALIBRATION_PRESSED = 3,   // Capturing pressed value
    MAG_SW_CALIBRATION_COMPLETE = 4   // Calibration finished
} mag_switch_calibration_state_t;

// Magnetic switch configuration structure
typedef struct {
    uint32_t channel;           // ADC channel number
    GPIO_TypeDef *gpio_port;    // GPIO port for the pin
    uint32_t gpio_pin;          // GPIO pin number
    uint16_t unpressed_value;   // ADC value when switch is not pressed
    uint16_t pressed_value;     // ADC value when switch is fully pressed
    uint8_t sensitivity;        // Trigger sensitivity percentage (0-100)
    uint16_t trigger_threshold; // Calculated trigger threshold ADC value
    bool is_calibrated;         // Whether this switch has been calibrated
    bool is_pressed;            // Current press state
    uint16_t keycode;           // Keycode to send when pressed
} magnetic_switch_config_t;

// Global magnetic switch configurations
extern magnetic_switch_config_t magnetic_switches[MAX_MAGNETIC_SWITCHES];
extern uint8_t magnetic_switch_count;

// Function prototypes
// Initialization
void magnetic_switch_init(void);
void magnetic_switch_setup_from_config(void);

// Runtime functions
void magnetic_switch_update(void);
bool magnetic_switch_is_pressed(uint8_t switch_id);
uint16_t magnetic_switch_get_raw_value(uint8_t switch_id);
uint8_t magnetic_switch_get_percentage(uint8_t switch_id);
void magnetic_switch_start_calibration(uint8_t switch_id);
void magnetic_switch_set_unpressed_value(uint8_t switch_id);
void magnetic_switch_set_pressed_value(uint8_t switch_id);
void magnetic_switch_complete_calibration(uint8_t switch_id);
void magnetic_switch_set_sensitivity(uint8_t switch_id, uint8_t sensitivity);
void magnetic_switch_calculate_threshold(uint8_t switch_id);
mag_switch_calibration_state_t magnetic_switch_get_calibration_state(uint8_t switch_id);

#endif /* MAGNETIC_SWITCH_H */