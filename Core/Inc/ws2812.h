/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : ws2812.h
  * @brief          : WS2812B LED driver header
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __WS2812_H__
#define __WS2812_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "gpio.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* USER CODE BEGIN Private defines */

// WS2812 Pin Configuration
#define WS2812_PORT                 GPIOC
#define WS2812_PIN                  GPIO_PIN_10

// WS2812 Timing Configuration (for 170MHz system clock)
// Based on WS2812B datasheet:
// T0H: 0.4us ±0.15us -> 0.25-0.55us (68 cycles @ 170MHz)
// T0L: 0.85us ±0.15us -> 0.7-1.0us (145 cycles @ 170MHz)
// T1H: 0.8us ±0.15us -> 0.65-0.95us (136 cycles @ 170MHz)  
// T1L: 0.45us ±0.15us -> 0.3-0.6us (77 cycles @ 170MHz)
// RES: >50us (8500 cycles @ 170MHz)

#define WS2812_T0H_CYCLES           68    // 0.4us
#define WS2812_T0L_CYCLES           145   // 0.85us  
#define WS2812_T1H_CYCLES           136   // 0.8us
#define WS2812_T1L_CYCLES           77    // 0.45us
#define WS2812_RESET_CYCLES         8500  // 50us

// Maximum number of LEDs supported
#define WS2812_MAX_LEDS             64

/* USER CODE END Private defines */

/* USER CODE BEGIN Prototypes */

typedef struct {
    uint8_t green;
    uint8_t red;  
    uint8_t blue;
} ws2812_color_t;

typedef struct {
    ws2812_color_t leds[WS2812_MAX_LEDS];
    uint16_t num_leds;
    uint8_t brightness;  // Global brightness 0-255
} ws2812_strip_t;

/* Function prototypes -------------------------------------------------------*/

/**
 * @brief Initialize WS2812 driver and configure GPIO
 * @retval None
 */
void ws2812_init(void);

/**
 * @brief Initialize a WS2812 strip structure
 * @param strip Pointer to strip structure
 * @param num_leds Number of LEDs in the strip
 * @retval None
 */
void ws2812_strip_init(ws2812_strip_t *strip, uint16_t num_leds);

/**
 * @brief Set color of a specific LED
 * @param strip Pointer to strip structure
 * @param led_index LED index (0-based)
 * @param red Red component (0-255)
 * @param green Green component (0-255)
 * @param blue Blue component (0-255)
 * @retval None
 */
void ws2812_set_led(ws2812_strip_t *strip, uint16_t led_index, uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief Set color of a specific LED using color structure
 * @param strip Pointer to strip structure
 * @param led_index LED index (0-based)
 * @param color Color structure
 * @retval None
 */
void ws2812_set_led_color(ws2812_strip_t *strip, uint16_t led_index, ws2812_color_t color);

/**
 * @brief Set brightness for the entire strip
 * @param strip Pointer to strip structure
 * @param brightness Brightness level (0-255)
 * @retval None
 */
void ws2812_set_brightness(ws2812_strip_t *strip, uint8_t brightness);

/**
 * @brief Clear all LEDs (set to black)
 * @param strip Pointer to strip structure
 * @retval None
 */
void ws2812_clear(ws2812_strip_t *strip);

/**
 * @brief Update the LED strip with current data
 * @param strip Pointer to strip structure
 * @retval None
 */
void ws2812_update(ws2812_strip_t *strip);

/**
 * @brief Generate rainbow pattern on the strip
 * @param strip Pointer to strip structure
 * @param offset Color offset for animation
 * @retval None
 */
void ws2812_rainbow(ws2812_strip_t *strip, uint8_t offset);

/**
 * @brief Set a solid color for all LEDs
 * @param strip Pointer to strip structure
 * @param red Red component (0-255)
 * @param green Green component (0-255)
 * @param blue Blue component (0-255)
 * @retval None
 */
void ws2812_fill(ws2812_strip_t *strip, uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief Test function to set specific pattern for debugging
 * @param strip Pointer to strip structure
 * @retval None
 */
void ws2812_test_pattern(ws2812_strip_t *strip);

/**
 * @brief Simple test function that sends raw data without brightness scaling
 * @retval None
 */
void ws2812_simple_test(void);

/**
 * @brief Color verification test - sends single LED with known color
 * @param color_byte Single byte to test (helps identify timing issues)
 * @retval None
 */
void ws2812_byte_test(uint8_t color_byte);

/**
 * @brief Very simple test - send one LED with specific colors to debug timing
 * @retval None
 */
void ws2812_debug_test(void);

/**
 * @brief Send pure red to one LED (for master mode)
 * @retval None
 */
void ws2812_send_red(void);

/**
 * @brief Send pure blue to one LED (for slave mode)
 * @retval None
 */
void ws2812_send_blue(void);

/**
 * @brief Send pure red to all LEDs (for master mode)
 * @param num_leds Number of LEDs in the strip
 * @retval None
 */
void ws2812_send_all_red(uint16_t num_leds);

/**
 * @brief Send pure blue to all LEDs (for slave mode)
 * @param num_leds Number of LEDs in the strip
 * @retval None
 */
void ws2812_send_all_blue(uint16_t num_leds);

/**
 * @brief Test function - send exactly 2 LEDs manually
 * @retval None
 */
void ws2812_two_led_test(void);

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __WS2812_H__ */