#include "input/magnetic_switch.h"
#include "input/slider.h"
#include "usb_app.h"
#include "main.h"
#include "i2c_manager.h"
#include "eeprom_emulation.h"

// Global magnetic switch configurations
magnetic_switch_config_t magnetic_switches[MAX_MAGNETIC_SWITCHES];
uint8_t magnetic_switch_count = 0;

// Static variables for calibration
static mag_switch_calibration_state_t calibration_states[MAX_MAGNETIC_SWITCHES];

void magnetic_switch_init(void) {
    // Initialize ADC for magnetic switch readings
    adc_init();
    
    // Initialize all magnetic switches to default state
    for (uint8_t i = 0; i < MAX_MAGNETIC_SWITCHES; i++) {
        magnetic_switches[i].channel = 0;
        magnetic_switches[i].gpio_port = NULL;
        magnetic_switches[i].gpio_pin = 0;
        magnetic_switches[i].unpressed_value = 0;
        magnetic_switches[i].pressed_value = 4095;
        magnetic_switches[i].sensitivity = 50;
        magnetic_switches[i].trigger_threshold = 2047;
        magnetic_switches[i].is_calibrated = false;
        magnetic_switches[i].is_pressed = false;
        magnetic_switches[i].keycode = 0;
        calibration_states[i] = MAG_SW_CALIBRATION_NONE;
    }
    
    // Set up magnetic switches from hardware configuration
    magnetic_switch_setup_from_config();
    
    // Load calibration data from EEPROM
    for (uint8_t i = 0; i < magnetic_switch_count; i++) {
        uint16_t unpressed, pressed;
        uint8_t sensitivity;
        bool is_calibrated;
        
        if (eeprom_get_magnetic_switch_calibration(i, &unpressed, &pressed, &sensitivity, &is_calibrated)) {
            if (is_calibrated) {
                magnetic_switches[i].unpressed_value = unpressed;
                magnetic_switches[i].pressed_value = pressed;
                magnetic_switches[i].sensitivity = sensitivity;
                magnetic_switches[i].is_calibrated = true;
                magnetic_switch_calculate_threshold(i);
                usb_app_cdc_printf("Loaded calibration for switch %d from EEPROM: unpressed=%d pressed=%d sensitivity=%d%%\r\n",
                                 i, unpressed, pressed, sensitivity);
            }
        }
    }
    
    usb_app_cdc_printf("Magnetic switches initialized (count: %d)\r\n", magnetic_switch_count);
}

void magnetic_switch_update(void) {
    for (uint8_t i = 0; i < magnetic_switch_count; i++) {
        // Use percentage-based detection (0-100) with hysteresis
        // Press at 100%, release at 98% to avoid bouncing
        bool was_pressed = magnetic_switches[i].is_pressed;
        uint8_t pct = magnetic_switch_get_percentage(i);

        bool is_pressed;
        if (was_pressed) {
            // If already pressed, need to drop below 98% to release (hysteresis)
            is_pressed = (pct >= 98);
        } else {
            // If not pressed, need to reach 100% to press
            is_pressed = (pct >= 100);
        }

        // Only send events if state actually changed
        if (is_pressed != was_pressed) {
            magnetic_switches[i].is_pressed = is_pressed;
            
            if (is_pressed) {
                usb_app_cdc_printf("MAG_SW[%d]: PRESS at %d%% (keycode=0x%04X) - Key will be held\r\n", 
                                 i, pct, magnetic_switches[i].keycode);
                // Use row 0 for magnetic switches (row 254 is reserved for encoders which send tap events)
                i2c_manager_process_local_key_event(0, i, 1, magnetic_switches[i].keycode);
            } else {
                usb_app_cdc_printf("MAG_SW[%d]: RELEASE at %d%% (keycode=0x%04X) - Key released\r\n", 
                                 i, pct, magnetic_switches[i].keycode);
                // Use row 0 for magnetic switches (row 254 is reserved for encoders which send tap events)
                i2c_manager_process_local_key_event(0, i, 0, magnetic_switches[i].keycode);
            }
        }
        // While at 100%, the key stays pressed in the HID report (no events sent)
    }
}

bool magnetic_switch_is_pressed(uint8_t switch_id) {
    if (switch_id >= magnetic_switch_count) {
        return false;
    }
    return magnetic_switches[switch_id].is_pressed;
}

uint16_t magnetic_switch_get_raw_value(uint8_t switch_id) {
    if (switch_id >= magnetic_switch_count) {
        return 0;
    }
    
    magnetic_switch_config_t *config = &magnetic_switches[switch_id];
    
    // Configure ADC channel
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = config->channel;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_47CYCLES_5;
    sConfig.SingleDiff = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset = 0;
    
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        return 0;
    }
    
    // Start ADC conversion
    if (HAL_ADC_Start(&hadc1) != HAL_OK) {
        return 0;
    }
    
    // Wait for conversion to complete
    if (HAL_ADC_PollForConversion(&hadc1, 100) != HAL_OK) {
        HAL_ADC_Stop(&hadc1);
        return 0;
    }
    
    // Read ADC value
    uint16_t adc_value = (uint16_t)HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    
    return adc_value;
}

uint8_t magnetic_switch_get_percentage(uint8_t switch_id) {
    if (switch_id >= magnetic_switch_count) {
        return 0;
    }
    
    magnetic_switch_config_t *config = &magnetic_switches[switch_id];
    uint16_t raw_value = magnetic_switch_get_raw_value(switch_id);
    
    uint16_t min_val, max_val;
    
    if (config->is_calibrated) {
        // Use calibrated values
        min_val = config->unpressed_value;
        max_val = config->pressed_value;
    } else {
        // Use default ADC range (0-4095 for 12-bit ADC)
        min_val = 0;
        max_val = 4095;
    }
    
    // Ensure min < max
    if (min_val > max_val) {
        uint16_t temp = min_val;
        min_val = max_val;
        max_val = temp;
    }
    
    // Clamp value to range
    if (raw_value <= min_val) {
        return 0;
    }
    if (raw_value >= max_val) {
        return 100;
    }
    
    // Calculate percentage
    uint32_t percentage = ((uint32_t)(raw_value - min_val) * 100) / (max_val - min_val);
    return (uint8_t)percentage;
}

void magnetic_switch_start_calibration(uint8_t switch_id) {
    if (switch_id >= magnetic_switch_count) {
        return;
    }
    
    calibration_states[switch_id] = MAG_SW_CALIBRATION_READY;
    magnetic_switches[switch_id].is_calibrated = false;
    
    usb_app_cdc_printf("Started calibration for magnetic switch %d\r\n", switch_id);
}

void magnetic_switch_set_unpressed_value(uint8_t switch_id) {
    if (switch_id >= magnetic_switch_count || 
        calibration_states[switch_id] != MAG_SW_CALIBRATION_READY) {
        return;
    }
    
    uint16_t raw_value = magnetic_switch_get_raw_value(switch_id);
    magnetic_switches[switch_id].unpressed_value = raw_value;
    calibration_states[switch_id] = MAG_SW_CALIBRATION_UNPRESSED;
    
    usb_app_cdc_printf("Set unpressed value for switch %d: %d\r\n", switch_id, raw_value);
}

void magnetic_switch_set_pressed_value(uint8_t switch_id) {
    if (switch_id >= magnetic_switch_count || 
        calibration_states[switch_id] != MAG_SW_CALIBRATION_UNPRESSED) {
        return;
    }
    
    uint16_t raw_value = magnetic_switch_get_raw_value(switch_id);
    magnetic_switches[switch_id].pressed_value = raw_value;
    calibration_states[switch_id] = MAG_SW_CALIBRATION_PRESSED;
    
    usb_app_cdc_printf("Set pressed value for switch %d: %d\r\n", switch_id, raw_value);
}

void magnetic_switch_complete_calibration(uint8_t switch_id) {
    if (switch_id >= magnetic_switch_count || 
        calibration_states[switch_id] != MAG_SW_CALIBRATION_PRESSED) {
        return;
    }
    
    magnetic_switches[switch_id].is_calibrated = true;
    calibration_states[switch_id] = MAG_SW_CALIBRATION_COMPLETE;
    
    // Set default sensitivity if not already set
    if (magnetic_switches[switch_id].sensitivity == 0) {
        magnetic_switches[switch_id].sensitivity = 50; // 50% default
    }
    
    // Calculate initial threshold
    magnetic_switch_calculate_threshold(switch_id);
    
    // Save calibration to EEPROM
    eeprom_set_magnetic_switch_calibration(switch_id, 
                                          magnetic_switches[switch_id].unpressed_value,
                                          magnetic_switches[switch_id].pressed_value,
                                          magnetic_switches[switch_id].sensitivity);
    eeprom_save_config();
    
    usb_app_cdc_printf("Completed calibration for switch %d (unpressed: %d, pressed: %d, threshold: %d) - Saved to EEPROM\r\n", 
                switch_id, 
                magnetic_switches[switch_id].unpressed_value,
                magnetic_switches[switch_id].pressed_value,
                magnetic_switches[switch_id].trigger_threshold);
}

void magnetic_switch_set_sensitivity(uint8_t switch_id, uint8_t sensitivity) {
    if (switch_id >= magnetic_switch_count || sensitivity > 100) {
        return;
    }
    
    magnetic_switches[switch_id].sensitivity = sensitivity;
    magnetic_switch_calculate_threshold(switch_id);
    
    // Save to EEPROM if calibrated
    if (magnetic_switches[switch_id].is_calibrated) {
        eeprom_set_magnetic_switch_calibration(switch_id, 
                                              magnetic_switches[switch_id].unpressed_value,
                                              magnetic_switches[switch_id].pressed_value,
                                              magnetic_switches[switch_id].sensitivity);
        eeprom_save_config();
    }
    
    usb_app_cdc_printf("Set sensitivity for switch %d: %d%% (threshold: %d)\r\n", 
                switch_id, sensitivity, magnetic_switches[switch_id].trigger_threshold);
}

void magnetic_switch_calculate_threshold(uint8_t switch_id) {
    if (switch_id >= magnetic_switch_count || !magnetic_switches[switch_id].is_calibrated) {
        return;
    }
    
    magnetic_switch_config_t *config = &magnetic_switches[switch_id];
    
    uint16_t unpressed = config->unpressed_value;
    uint16_t pressed = config->pressed_value;
    
    // Calculate threshold based on sensitivity percentage
    if (pressed > unpressed) {
        // Increasing values when pressed
        uint32_t range = pressed - unpressed;
        uint32_t offset = (range * config->sensitivity) / 100;
        config->trigger_threshold = unpressed + offset;
    } else {
        // Decreasing values when pressed
        uint32_t range = unpressed - pressed;
        uint32_t offset = (range * config->sensitivity) / 100;
        config->trigger_threshold = unpressed - offset;
    }
}

mag_switch_calibration_state_t magnetic_switch_get_calibration_state(uint8_t switch_id) {
    if (switch_id >= magnetic_switch_count) {
        return MAG_SW_CALIBRATION_NONE;
    }
    return calibration_states[switch_id];
}

void magnetic_switch_setup_from_config(void) {
#ifdef MAGNETIC_SWITCH_COUNT
    magnetic_switch_count = MAGNETIC_SWITCH_COUNT;
    
    // Set up each magnetic switch from hardware configuration
    for (uint8_t i = 0; i < MAGNETIC_SWITCH_COUNT && i < MAX_MAGNETIC_SWITCHES; i++) {
        magnetic_switches[i].channel = magnetic_switch_configs[i].channel;
        magnetic_switches[i].gpio_port = magnetic_switch_configs[i].gpio_port;
        magnetic_switches[i].gpio_pin = magnetic_switch_configs[i].gpio_pin;
        magnetic_switches[i].keycode = magnetic_switch_configs[i].keycode;
        
        // Configure GPIO pin for analog input
        __HAL_RCC_GPIOA_CLK_ENABLE(); // Enable GPIOA clock (assuming PA0)
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Pin = magnetic_switches[i].gpio_pin;
        GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(magnetic_switches[i].gpio_port, &GPIO_InitStruct);
        
        // Set up default calibration values for immediate use
        magnetic_switches[i].unpressed_value = 100;      // Typical low value
        magnetic_switches[i].pressed_value = 3000;       // Typical high value when pressed
        magnetic_switches[i].sensitivity = 20;           // 20% trigger point
        magnetic_switches[i].is_calibrated = true;       // Enable by default with reasonable values
        
        // Calculate threshold
        magnetic_switch_calculate_threshold(i);
        
        usb_app_cdc_printf("Magnetic switch %d: CH%d, PA%d, KC=0x%04X, default range %d-%d\r\n",
                           i, 
                           (magnetic_switches[i].channel == ADC_CHANNEL_1) ? 1 : 
                           (magnetic_switches[i].channel == ADC_CHANNEL_2) ? 2 : 0,
                           (magnetic_switches[i].gpio_pin == GPIO_PIN_0) ? 0 :
                           (magnetic_switches[i].gpio_pin == GPIO_PIN_1) ? 1 : 99,
                           magnetic_switches[i].keycode,
                           magnetic_switches[i].unpressed_value,
                           magnetic_switches[i].pressed_value);
    }
#else
    magnetic_switch_count = 0;
    usb_app_cdc_printf("No magnetic switches configured\r\n");
#endif
}