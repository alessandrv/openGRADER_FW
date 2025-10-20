#include "slider.h"
#include "midi_handler.h"
#include "pin_config.h"
#include "usb_app.h"
#include "i2c_manager.h"
#include "keymap.h"  // For layer-aware slider configuration

// Simple round function (avoids pulling in math.h)
static inline float roundf_simple(float x)
{
    return (x >= 0.0f) ? (float)((int)(x + 0.5f)) : (float)((int)(x - 0.5f));
}

// Simple fabsf function
static inline float fabsf_simple(float x)
{
    return (x >= 0.0f) ? x : -x;
}

// Simple abs function (avoids pulling in stdlib.h)
static inline int abs_simple(int x)
{
    return (x >= 0) ? x : -x;
}

#if SLIDER_COUNT > 0

// Slider state for each configured slider
static slider_state_t slider_states[SLIDER_COUNT] = {0};

// ADC handle
static ADC_HandleTypeDef hadc1;

// Last scan time for throttling
static uint32_t last_scan_time = 0;

// Local function prototypes
static uint16_t slider_read_adc(uint8_t slider_id);
static uint8_t slider_adc_to_percent(uint16_t adc_value, const slider_hw_config_t *config);
static uint8_t slider_percent_to_midi(uint8_t percent, uint8_t min_midi, uint8_t max_midi);

/**
 * @brief Update moving average buffer and return smoothed ADC value
 */
static uint16_t slider_moving_average(slider_state_t *state, uint16_t new_reading)
{
    // Remove the oldest reading from the total
    state->total = state->total - state->readings[state->read_index];
    
    // Store the new reading
    state->readings[state->read_index] = new_reading;
    
    // Add the new reading to the total
    state->total = state->total + new_reading;
    
    // Advance to the next position in the array
    state->read_index = (state->read_index + 1) % SLIDER_SMOOTHING_BUFFER_SIZE;
    
    // Calculate the average
    state->average = state->total / SLIDER_SMOOTHING_BUFFER_SIZE;
    
    return state->average;
}

/**
 * @brief Initialize ADC for slider inputs
 */
void slider_init(void)
{
    // Enable GPIO clock for analog pins
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    // Configure GPIO pins for analog input FIRST
    for (uint8_t i = 0; i < SLIDER_COUNT; i++) {
        const slider_hw_config_t *cfg = &slider_configs[i];
        
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Pin = cfg->gpio_pin;
        GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(cfg->gpio_port, &GPIO_InitStruct);
        
        // Initialize state
        slider_states[i].initialized = false;
        slider_states[i].smoothed_pct = 0.0f;
        slider_states[i].last_displayed = 0;
        slider_states[i].last_midi_value = 0;
        
        // Initialize moving average buffer
        slider_states[i].read_index = 0;
        slider_states[i].total = 0;
        slider_states[i].average = 0;
        for (int j = 0; j < SLIDER_SMOOTHING_BUFFER_SIZE; j++) {
            slider_states[i].readings[j] = 0;
        }
    }
    
    // Enable ADC clock
    __HAL_RCC_ADC12_CLK_ENABLE();
    
    // Configure ADC
    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    hadc1.Init.LowPowerAutoWait = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
    hadc1.Init.OversamplingMode = DISABLE;
    
    if (HAL_ADC_Init(&hadc1) != HAL_OK) {
        // Initialization Error
        return;
    }
    
    // Calibrate ADC
    HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
}

/**
 * @brief Read ADC value for a specific slider
 */
static uint16_t slider_read_adc(uint8_t slider_id)
{
    if (slider_id >= SLIDER_COUNT) {
        return 0;
    }
    
    const slider_hw_config_t *cfg = &slider_configs[slider_id];
    
    // Configure ADC channel
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = cfg->channel;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_640CYCLES_5;
    sConfig.SingleDiff = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset = 0;
    
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        return 0;
    }
    
    // Start conversion
    HAL_ADC_Start(&hadc1);
    
    // Wait for conversion to complete (timeout 10ms)
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
        uint16_t value = HAL_ADC_GetValue(&hadc1);
        HAL_ADC_Stop(&hadc1);
        return value;
    }
    
    HAL_ADC_Stop(&hadc1);
    return 0;
}

/**
 * @brief Convert ADC value to percentage (0-100)
 */
static uint8_t slider_adc_to_percent(uint16_t adc_value, const slider_hw_config_t *config)
{
    if (config->value_at_full <= config->value_at_zero) {
        return 0;
    }
    
    // Clamp to range
    if (adc_value < config->value_at_zero) {
        return 0;
    }
    if (adc_value > config->value_at_full) {
        return 100;
    }
    
    // Map to percentage
    float percent = ((float)(adc_value - config->value_at_zero) / 
                     (float)(config->value_at_full - config->value_at_zero)) * 100.0f;
    
    return (uint8_t)roundf_simple(percent);
}

/**
 * @brief Convert percentage to MIDI value based on configured range
 */
static uint8_t slider_percent_to_midi(uint8_t percent, uint8_t min_midi, uint8_t max_midi)
{
    if (percent > 100) {
        percent = 100;
    }
    
    // Map percentage (0-100) to MIDI range
    // Note: This gives us 101 discrete values, so some MIDI values may be unreachable
    // For 0-127 range: 0->0, 50->63.5->64, 100->127
    float midi_range = (float)(max_midi - min_midi);
    float midi_float = min_midi + (percent / 100.0f) * midi_range;
    uint8_t midi_value = (uint8_t)roundf_simple(midi_float);
    
    // Clamp to configured range
    if (midi_value < min_midi) {
        midi_value = min_midi;
    }
    if (midi_value > max_midi) {
        midi_value = max_midi;
    }
    
    return midi_value;
}

/**
 * @brief Scan all sliders and send MIDI CC messages on value changes
 */
void slider_scan(void)
{
    #ifndef SLIDER_SAMPLE_INTERVAL_MS
    #define SLIDER_SAMPLE_INTERVAL_MS 5  // Sample every 5ms (200Hz) for very responsive feedback
    #endif
    
    #ifndef SLIDER_EMA_ALPHA
    #define SLIDER_EMA_ALPHA 0.4f  // More responsive smoothing (was 0.2f)
    #endif
    
    #ifndef SLIDER_FAST_CHANGE_THRESHOLD
    #define SLIDER_FAST_CHANGE_THRESHOLD 0.5f  // Lower threshold for faster response (was 1.0f)
    #endif
    
    #ifndef SLIDER_MIDI_HYSTERESIS
    #define SLIDER_MIDI_HYSTERESIS 2  // MIDI values must change by at least this much to send
    #endif
    
    // Throttle scanning to configured interval
    uint32_t now = HAL_GetTick();
    if ((now - last_scan_time) < SLIDER_SAMPLE_INTERVAL_MS) {
        return;
    }
    last_scan_time = now;
    
    for (uint8_t i = 0; i < SLIDER_COUNT; i++) {
        const slider_hw_config_t *cfg = &slider_configs[i];
        slider_state_t *state = &slider_states[i];
        
        // Read ADC value
        uint16_t adc_value = slider_read_adc(i);
        
        // Apply moving average smoothing to raw ADC value
        uint16_t smoothed_adc = slider_moving_average(state, adc_value);
        
        // Convert smoothed ADC to percentage
        uint8_t raw_pct = slider_adc_to_percent(smoothed_adc, cfg);
        
        // Initialize smoothing on first read
        if (!state->initialized) {
            state->smoothed_pct = (float)raw_pct;
            state->last_displayed = raw_pct;
            state->initialized = true;
            // Don't send MIDI on first read
            uint8_t initial_midi = slider_percent_to_midi(state->last_displayed, cfg->min_midi_value, cfg->max_midi_value);
            state->last_midi_value = initial_midi;
            usb_app_cdc_printf("Slider[%d] init: ADC=%u->%u, Pct=%u, MIDI=%u\r\n", 
                               i, adc_value, smoothed_adc, state->last_displayed, initial_midi);
            continue;
        }
        
        // Use the smoothed percentage directly (moving average already applied)
        state->smoothed_pct = (float)raw_pct;
        
        // Convert smoothed percentage to integer for display
        uint8_t rounded = (uint8_t)roundf_simple(state->smoothed_pct);
        if (rounded > 100) rounded = 100;
        
        // Update displayed percentage
        state->last_displayed = rounded;
        
        // Get layer-aware MIDI configuration and calculate MIDI value
        slider_config_t layer_config;
        if (keymap_get_active_slider_config(i, &layer_config) && layer_config.midi_cc != 0) {
            uint8_t midi_value = slider_percent_to_midi(rounded, 
                                                       layer_config.min_midi_value, 
                                                       layer_config.max_midi_value);
            
            // Use hysteresis to prevent MIDI oscillation on boundaries
            // Only send MIDI if the value changed by at least SLIDER_MIDI_HYSTERESIS
            int midi_diff = abs_simple((int)midi_value - (int)state->last_midi_value);
            
            if (midi_diff >= SLIDER_MIDI_HYSTERESIS) {
                // Send MIDI CC directly via USB
                usb_app_midi_send_cc(layer_config.midi_channel, layer_config.midi_cc, midi_value);
                
                // Also send via I2C only if we're in slave mode
                if (i2c_manager_get_mode() == 0) {  // 0 = slave mode
                    i2c_manager_send_midi_cc(layer_config.midi_channel, layer_config.midi_cc, midi_value);
                }
                
                state->last_midi_value = midi_value;
                usb_app_cdc_printf("Slider[%d] MIDI: ADC=%u->%u Pct=%u CC%d Ch%d Value=%u (diff=%d)\r\n", 
                                   i, adc_value, smoothed_adc, rounded, layer_config.midi_cc, layer_config.midi_channel, midi_value, midi_diff);
            }
        }
    }
}

/**
 * @brief Get current percentage value for a slider
 */
uint8_t slider_get_percent(uint8_t slider_id)
{
    if (slider_id >= SLIDER_COUNT) {
        return 0;
    }
    
    return slider_states[slider_id].last_displayed;
}

/**
 * @brief Get current MIDI value for a slider
 */
uint8_t slider_get_midi_value(uint8_t slider_id)
{
    if (slider_id >= SLIDER_COUNT) {
        return 0;
    }
    
    return slider_states[slider_id].last_midi_value;
}

/**
 * @brief Get current raw slider value for polling (bypasses filtering)
 * This reads the ADC immediately and converts to MIDI value using current layer config.
 * Unlike slider_get_midi_value(), this bypasses all the smoothing, hysteresis, and
 * change detection filters - it gives you the exact current position right now.
 * Perfect for responsive configurator polling where we want real-time feedback.
 */
uint8_t slider_get_current_raw_value(uint8_t slider_id)
{
    if (slider_id >= SLIDER_COUNT) {
        return 0;
    }
    
    const slider_hw_config_t *cfg = &slider_configs[slider_id];
    
    // Read ADC value immediately (no caching, no filtering)
    uint16_t adc_value = slider_read_adc(slider_id);
    
    // Convert to percentage
    uint8_t raw_pct = slider_adc_to_percent(adc_value, cfg);
    
    // Get current layer configuration for MIDI mapping
    slider_config_t layer_config;
    if (keymap_get_active_slider_config(slider_id, &layer_config) && layer_config.midi_cc != 0) {
        // Convert to MIDI value using current layer config
        return slider_percent_to_midi(raw_pct, layer_config.min_midi_value, layer_config.max_midi_value);
    }
    
    // Fall back to hardware config if no layer config
    return slider_percent_to_midi(raw_pct, cfg->min_midi_value, cfg->max_midi_value);
}

#else // SLIDER_COUNT == 0

// Stub implementations when no sliders are configured
void slider_init(void) {}
void slider_scan(void) {}
uint8_t slider_get_percent(uint8_t slider_id) { return 0; }
uint8_t slider_get_midi_value(uint8_t slider_id) { return 0; }

#endif // SLIDER_COUNT > 0
