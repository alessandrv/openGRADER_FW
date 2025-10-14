/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : ws2812.c
  * @brief          : WS2812B LED driver implementation
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

/* Includes ------------------------------------------------------------------*/
#include "ws2812.h"
#include <string.h>

/* USER CODE BEGIN Includes */
// Define GPIO macros directly to avoid compilation issues
#define WS2812_GPIO_PORT    GPIOC
#define WS2812_GPIO_PIN     GPIO_PIN_10
#define WS2812_GPIO_BSRR    GPIOC->BSRR
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
static void ws2812_send_bit(uint8_t bit);
static void ws2812_send_byte(uint8_t byte);
static void ws2812_reset(void);
static uint8_t ws2812_apply_brightness(uint8_t color, uint8_t brightness);
static ws2812_color_t ws2812_hsv_to_rgb(uint16_t hue, uint8_t saturation, uint8_t value);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
 * @brief Initialize WS2812 driver and configure GPIO
 * @retval None
 */
void ws2812_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // Enable GPIOC clock
    __HAL_RCC_GPIOC_CLK_ENABLE();
    
    // Configure PC10 as output
    GPIO_InitStruct.Pin = WS2812_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH; // Use highest speed for precise timing
    HAL_GPIO_Init(WS2812_GPIO_PORT, &GPIO_InitStruct);
    
    // Set initial state to low using direct register access
    WS2812_GPIO_BSRR = (uint32_t)WS2812_GPIO_PIN << 16;
    
    // Send reset signal
    ws2812_reset();
}

/**
 * @brief Initialize a WS2812 strip structure
 * @param strip Pointer to strip structure
 * @param num_leds Number of LEDs in the strip
 * @retval None
 */
void ws2812_strip_init(ws2812_strip_t *strip, uint16_t num_leds)
{
    if (strip == NULL || num_leds > WS2812_MAX_LEDS) {
        return;
    }
    
    strip->num_leds = num_leds;
    strip->brightness = 255; // Full brightness by default
    
    // Clear all LEDs
    memset(strip->leds, 0, sizeof(strip->leds));
}

/**
 * @brief Set color of a specific LED
 * @param strip Pointer to strip structure
 * @param led_index LED index (0-based)
 * @param red Red component (0-255)
 * @param green Green component (0-255)
 * @param blue Blue component (0-255)
 * @retval None
 */
void ws2812_set_led(ws2812_strip_t *strip, uint16_t led_index, uint8_t red, uint8_t green, uint8_t blue)
{
    if (strip == NULL || led_index >= strip->num_leds) {
        return;
    }
    
    strip->leds[led_index].red = red;
    strip->leds[led_index].green = green;
    strip->leds[led_index].blue = blue;
}

/**
 * @brief Set color of a specific LED using color structure
 * @param strip Pointer to strip structure
 * @param led_index LED index (0-based)
 * @param color Color structure
 * @retval None
 */
void ws2812_set_led_color(ws2812_strip_t *strip, uint16_t led_index, ws2812_color_t color)
{
    if (strip == NULL || led_index >= strip->num_leds) {
        return;
    }
    
    strip->leds[led_index] = color;
}

/**
 * @brief Set brightness for the entire strip
 * @param strip Pointer to strip structure
 * @param brightness Brightness level (0-255)
 * @retval None
 */
void ws2812_set_brightness(ws2812_strip_t *strip, uint8_t brightness)
{
    if (strip == NULL) {
        return;
    }
    
    strip->brightness = brightness;
}

/**
 * @brief Clear all LEDs (set to black)
 * @param strip Pointer to strip structure
 * @retval None
 */
void ws2812_clear(ws2812_strip_t *strip)
{
    if (strip == NULL) {
        return;
    }
    
    memset(strip->leds, 0, strip->num_leds * sizeof(ws2812_color_t));
}

/**
 * @brief Update the LED strip with current data
 * @param strip Pointer to strip structure
 * @retval None
 */
void ws2812_update(ws2812_strip_t *strip)
{
    if (strip == NULL) {
        return;
    }
    
    // Disable interrupts for precise timing
    __disable_irq();
    
    // Send data for each LED (GRB format)
    for (uint16_t i = 0; i < strip->num_leds; i++) {
        // Apply brightness and send in GRB order
        uint8_t green = ws2812_apply_brightness(strip->leds[i].green, strip->brightness);
        uint8_t red = ws2812_apply_brightness(strip->leds[i].red, strip->brightness);
        uint8_t blue = ws2812_apply_brightness(strip->leds[i].blue, strip->brightness);
        
        // Debug output for first LED only to avoid spam
        if (i == 0) {
            // Note: This debug print inside IRQ disabled section is not ideal, 
            // but needed for debugging. Remove in production.
            // printf would require re-enabling interrupts briefly
        }
        
        ws2812_send_byte(green);
        ws2812_send_byte(red);
        ws2812_send_byte(blue);
    }
    
    // Send reset signal
    ws2812_reset();
    
    // Re-enable interrupts
    __enable_irq();
}

/**
 * @brief Generate rainbow pattern on the strip
 * @param strip Pointer to strip structure
 * @param offset Color offset for animation
 * @retval None
 */
void ws2812_rainbow(ws2812_strip_t *strip, uint8_t offset)
{
    if (strip == NULL) {
        return;
    }
    
    for (uint16_t i = 0; i < strip->num_leds; i++) {
        uint16_t hue = (uint16_t)((i * 360 / strip->num_leds) + offset) % 360;
        ws2812_color_t color = ws2812_hsv_to_rgb(hue, 255, 255);
        ws2812_set_led_color(strip, i, color);
    }
}

/**
 * @brief Set a solid color for all LEDs
 * @param strip Pointer to strip structure
 * @param red Red component (0-255)
 * @param green Green component (0-255)
 * @param blue Blue component (0-255)
 * @retval None
 */
void ws2812_fill(ws2812_strip_t *strip, uint8_t red, uint8_t green, uint8_t blue)
{
    if (strip == NULL) {
        return;
    }
    
    for (uint16_t i = 0; i < strip->num_leds; i++) {
        ws2812_set_led(strip, i, red, green, blue);
    }
}

/**
 * @brief Test function to set specific pattern for debugging
 * @param strip Pointer to strip structure
 * @retval None
 */
void ws2812_test_pattern(ws2812_strip_t *strip)
{
    if (strip == NULL) {
        return;
    }
    
    // Clear all first
    ws2812_clear(strip);
    
    // Set first LED to red only
    if (strip->num_leds > 0) {
        ws2812_set_led(strip, 0, 255, 0, 0);  // Red
    }
    
    // Set second LED to green only  
    if (strip->num_leds > 1) {
        ws2812_set_led(strip, 1, 0, 255, 0);  // Green
    }
    
    // Set third LED to blue only
    if (strip->num_leds > 2) {
        ws2812_set_led(strip, 2, 0, 0, 255);  // Blue
    }
    
    // Set fourth LED to low white
    if (strip->num_leds > 3) {
        ws2812_set_led(strip, 3, 32, 32, 32);  // Low white
    }
}

/**
 * @brief Simple test function that sends raw data without brightness scaling
 * @retval None
 */
void ws2812_simple_test(void)
{
    // Disable interrupts for precise timing
    __disable_irq();
    
    // Send data for first LED: pure red (G=0, R=255, B=0)
    // Green byte (0x00)
    ws2812_send_byte(0x00);
    // Red byte (0xFF) 
    ws2812_send_byte(0xFF);
    // Blue byte (0x00)
    ws2812_send_byte(0x00);
    
    // Send data for second LED: pure green (G=255, R=0, B=0)
    // Green byte (0xFF)
    ws2812_send_byte(0xFF);
    // Red byte (0x00)
    ws2812_send_byte(0x00);
    // Blue byte (0x00)
    ws2812_send_byte(0x00);
    
    // Send data for third LED: pure blue (G=0, R=0, B=255)
    // Green byte (0x00)
    ws2812_send_byte(0x00);
    // Red byte (0x00)
    ws2812_send_byte(0x00);
    // Blue byte (0xFF)
    ws2812_send_byte(0xFF);
    
    // Send reset signal
    ws2812_reset();
    
    // Re-enable interrupts
    __enable_irq();
}

/**
 * @brief Color verification test - sends single LED with known color
 * @param color_byte Single byte to test (helps identify timing issues)
 * @retval None
 */
void ws2812_byte_test(uint8_t color_byte)
{
    // Disable interrupts for precise timing
    __disable_irq();
    
    // Send data for one LED with the test byte in all positions
    ws2812_send_byte(color_byte); // Green
    ws2812_send_byte(color_byte); // Red  
    ws2812_send_byte(color_byte); // Blue
    
    // Send reset signal
    ws2812_reset();
    
    // Re-enable interrupts
    __enable_irq();
}

/**
 * @brief Very simple test - send one LED with specific colors to debug timing
 * @retval None
 */
void ws2812_debug_test(void)
{
    // Disable interrupts for precise timing
    __disable_irq();
    
    // Try sending just one LED with alternating pattern to see if timing works
    // Send 0xAA (10101010) pattern for all three channels
    ws2812_send_byte(0xAA); // Green
    ws2812_send_byte(0xAA); // Red  
    ws2812_send_byte(0xAA); // Blue
    
    // Send reset signal
    ws2812_reset();
    
    // Re-enable interrupts
    __enable_irq();
}

/**
 * @brief Send pure red to one LED (for master mode)
 * @retval None
 */
void ws2812_send_red(void)
{
    __disable_irq();
    ws2812_send_byte(0x00); // Green = 0
    ws2812_send_byte(0xFF); // Red = 255  
    ws2812_send_byte(0x00); // Blue = 0
    ws2812_reset();
    __enable_irq();
}

/**
 * @brief Send pure blue to one LED (for slave mode)
 * @retval None
 */
void ws2812_send_blue(void)
{
    __disable_irq();
    ws2812_send_byte(0x00); // Green = 0
    ws2812_send_byte(0x00); // Red = 0
    ws2812_send_byte(0xFF); // Blue = 255
    ws2812_reset();
    __enable_irq();
}

/**
 * @brief Send pure red to all LEDs (for master mode) - robust version
 * @param num_leds Number of LEDs in the strip
 * @retval None
 */
void ws2812_send_all_red(uint16_t num_leds)
{
    // Limit to reasonable number to avoid timing issues
    if (num_leds > WS2812_MAX_LEDS) {
        num_leds = WS2812_MAX_LEDS;
    }
    
    __disable_irq();
    
    // Send red to all LEDs - send continuously without breaks
    for (uint16_t i = 0; i < num_leds; i++) {
        // Send GRB: Green=0, Red=255, Blue=0 for red color
        ws2812_send_byte(0x00); // Green = 0
        ws2812_send_byte(0xFF); // Red = 255  
        ws2812_send_byte(0x00); // Blue = 0
    }
    
    // Send reset signal to latch the data
    ws2812_reset();
    __enable_irq();
}

/**
 * @brief Test function - send exactly 2 LEDs manually
 * @retval None
 */
void ws2812_two_led_test(void)
{
    __disable_irq();
    
    // First LED: Red (GRB = 0, 255, 0)
    ws2812_send_byte(0x00); // Green = 0
    ws2812_send_byte(0xFF); // Red = 255  
    ws2812_send_byte(0x00); // Blue = 0
    
    // Second LED: Red (GRB = 0, 255, 0)  
    ws2812_send_byte(0x00); // Green = 0
    ws2812_send_byte(0xFF); // Red = 255  
    ws2812_send_byte(0x00); // Blue = 0
    
    // Send reset signal to latch the data
    ws2812_reset();
    __enable_irq();
}

/**
 * @brief Send pure blue to all LEDs (for slave mode) - robust version
 * @param num_leds Number of LEDs in the strip
 * @retval None
 */
void ws2812_send_all_blue(uint16_t num_leds)
{
    // Limit to reasonable number to avoid timing issues
    if (num_leds > WS2812_MAX_LEDS) {
        num_leds = WS2812_MAX_LEDS;
    }
    
    __disable_irq();
    
    // Send blue to all LEDs - send continuously without breaks
    for (uint16_t i = 0; i < num_leds; i++) {
        // Send GRB: Green=0, Red=0, Blue=255 for blue color
        ws2812_send_byte(0x00); // Green = 0
        ws2812_send_byte(0x00); // Red = 0
        ws2812_send_byte(0xFF); // Blue = 255
    }
    
    // Send reset signal to latch the data
    ws2812_reset();
    __enable_irq();
}

/**
 * @brief Send a single bit to the WS2812 using conservative timing
 * @param bit Bit value (0 or 1)
 * @retval None
 */
static void ws2812_send_bit(uint8_t bit)
{
    if (bit) {
        // Send '1' bit: T1H = 0.8us, T1L = 0.45us
        // Set pin high
        WS2812_GPIO_BSRR = WS2812_GPIO_PIN;
        
        // High time for '1' - conservative timing for 0.8us
        __asm volatile (
            "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
            "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
            "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
            "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
            "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
            "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
            "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
            "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
            "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
            "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
        );
        
        // Set pin low
        WS2812_GPIO_BSRR = (uint32_t)WS2812_GPIO_PIN << 16;
        
        // Low time for '1' - conservative timing for 0.45us
        __asm volatile (
            "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
            "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
            "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
            "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
            "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
        );
    } else {
        // Send '0' bit: T0H = 0.4us, T0L = 0.85us
        // Set pin high
        WS2812_GPIO_BSRR = WS2812_GPIO_PIN;
        
        // High time for '0' - conservative timing for 0.4us
        __asm volatile (
            "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
            "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
            "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
            "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
        );
        
        // Set pin low
        WS2812_GPIO_BSRR = (uint32_t)WS2812_GPIO_PIN << 16;
        
        // Low time for '0' - conservative timing for 0.85us
        __asm volatile (
            "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
            "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
            "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
            "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
            "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
            "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
            "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
            "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
            "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
        );
    }
}

/**
 * @brief Send a byte to the WS2812 (MSB first)
 * @param byte Byte to send
 * @retval None
 */
static void ws2812_send_byte(uint8_t byte)
{
    for (int i = 7; i >= 0; i--) {
        ws2812_send_bit((byte >> i) & 0x01);
    }
}

/**
 * @brief Send reset signal to the WS2812 strip
 * @retval None
 */
static void ws2812_reset(void)
{
    // Set pin low using direct register access
    WS2812_GPIO_BSRR = (uint32_t)WS2812_GPIO_PIN << 16;
    
    // Wait for more than 50us (8500 cycles at 170MHz)
    for (volatile uint32_t i = 0; i < 8500; i++) {
        __asm volatile ("nop");
    }
}

/**
 * @brief Apply brightness scaling to a color component
 * @param color Original color value (0-255)
 * @param brightness Brightness level (0-255)
 * @retval Scaled color value
 */
static uint8_t ws2812_apply_brightness(uint8_t color, uint8_t brightness)
{
    return (uint8_t)(((uint16_t)color * brightness) / 255);
}

/**
 * @brief Convert HSV color to RGB
 * @param hue Hue (0-359 degrees)
 * @param saturation Saturation (0-255)
 * @param value Value/Brightness (0-255)
 * @retval RGB color structure
 */
static ws2812_color_t ws2812_hsv_to_rgb(uint16_t hue, uint8_t saturation, uint8_t value)
{
    ws2812_color_t rgb = {0, 0, 0};
    
    if (saturation == 0) {
        // Grayscale
        rgb.red = rgb.green = rgb.blue = value;
        return rgb;
    }
    
    uint8_t region = hue / 60;
    uint8_t remainder = (hue - (region * 60)) * 255 / 60;
    
    uint8_t p = (value * (255 - saturation)) / 255;
    uint8_t q = (value * (255 - ((saturation * remainder) / 255))) / 255;
    uint8_t t = (value * (255 - ((saturation * (255 - remainder)) / 255))) / 255;
    
    switch (region) {
        case 0:
            rgb.red = value; rgb.green = t; rgb.blue = p;
            break;
        case 1:
            rgb.red = q; rgb.green = value; rgb.blue = p;
            break;
        case 2:
            rgb.red = p; rgb.green = value; rgb.blue = t;
            break;
        case 3:
            rgb.red = p; rgb.green = q; rgb.blue = value;
            break;
        case 4:
            rgb.red = t; rgb.green = p; rgb.blue = value;
            break;
        default:
            rgb.red = value; rgb.green = p; rgb.blue = q;
            break;
    }
    
    return rgb;
}

/* USER CODE END 0 */