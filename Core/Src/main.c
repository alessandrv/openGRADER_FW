/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  *   // Initialize WS2812 LED strip
  usb_app_cdc_printf("Initializing WS2812 on PC10...\r\n");
  ws2812_init();
  
  // Simple test without using the strip structure or brightness scaling
  usb_app_cdc_printf("Running simple WS2812 test (3 LEDs: R,G,B)...\r\n");
  ws2812_simple_test();
  HAL_Delay(3000);
  
  // Now initialize the full strip for normal operation
  ws2812_strip_init(&led_strip, 8); // Initialize with 8 LEDs (adjust as needed)
  ws2812_set_brightness(&led_strip, 255); // Full brightness for now
  ws2812_clear(&led_strip);
  ws2812_update(&led_strip);
  usb_app_cdc_printf("WS2812 initialization complete\r\n");     : Main program body
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
#include "main.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usb_app.h"
#include "input/matrix.h"
#include "input/encoder.h"
#include "input/keymap.h"
#include "key_state.h"
#include "op_keycodes.h"
#include "pin_config.h"
#include "usart.h"
#include "i2c_manager.h"
#include "tusb.h"
#include "ws2812.h"
#ifndef USB_DEMO_ENABLE_MOUSE
#define USB_DEMO_ENABLE_MOUSE 0
#endif
#ifndef USB_DEMO_ENABLE_MIDI
#define USB_DEMO_ENABLE_MIDI 0
#endif
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
static uint8_t is_usb_connected = 0;
static uint32_t last_usb_check = 0;
#define USB_CHECK_INTERVAL_MS 1000

#ifndef FORCE_SLAVE_MODE
#define FORCE_SLAVE_MODE 0

#endif

// WS2812 LED strip
static ws2812_strip_t led_strip;

/* Matrix event callback (file scope) */
static void matrix_cb(uint8_t row, uint8_t col, uint8_t pressed, uint8_t keycode)
{
  // Debug output for key detection
  usb_app_cdc_printf("Key %s: row=%d, col=%d, keycode=0x%02X\r\n", 
               pressed ? "PRESSED" : "RELEASED", row, col, keycode);
  
  // No visual feedback on LED strip to avoid flashing
  // (LEDs will maintain their master/slave status colors)
  
  (void)row; (void)col; // unused in this simple handler
  if (keycode == 0) return;

  i2c_manager_process_local_key_event(row, col, pressed, keycode);
}

static void ws2812_apply_mode(uint8_t master_mode)
{
  if (master_mode) {
    usb_app_cdc_printf("LED: Master mode - single red LED\r\n");
    ws2812_send_red();
  } else {
    usb_app_cdc_printf("LED: Slave mode - single blue LED\r\n");
    ws2812_send_blue();
  }
}





/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  MX_UART4_Init();
  /* USER CODE BEGIN 2 */
  usb_app_init();
  key_state_init();
  encoder_init();
  matrix_init();
  i2c_manager_init();
  
  // Initialize WS2812 LED strip
  ws2812_init();
  ws2812_strip_init(&led_strip, 35); // Initialize with 8 LEDs (adjust as needed)
  ws2812_set_brightness(&led_strip, 50); // Set to 20% brightness to avoid too much current draw
  
  // Initial LED test pattern
  ws2812_fill(&led_strip, 0, 255, 0); // Green startup color
  ws2812_update(&led_strip);
  HAL_Delay(500);
  ws2812_clear(&led_strip);
  ws2812_update(&led_strip);
  
  // Initialize keymap and EEPROM after I2C is set up
  keymap_init();
  
  // Initial USB connection check to set I2C mode
#if FORCE_SLAVE_MODE
  is_usb_connected = 0; // Force slave mode
  usb_app_cdc_printf("FORCED SLAVE MODE - I2C address will be 0x42\r\n");
#else
  is_usb_connected = tud_connected();
#endif
  i2c_manager_set_mode(is_usb_connected);
  ws2812_apply_mode(is_usb_connected);
  
  matrix_register_callback(matrix_cb);
  usb_app_cdc_printf("TinyUSB composite with keyboard initialized\r\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    usb_app_task();
    matrix_scan();
    encoder_task();
    key_state_task();
    
    // Check USB connection status periodically for master/slave switching
    uint32_t now = HAL_GetTick();
    
    if ((now - last_usb_check) >= USB_CHECK_INTERVAL_MS) {
      last_usb_check = now;
#if FORCE_SLAVE_MODE
      uint8_t usb_connected = 0; // Always force slave mode
#else
      uint8_t usb_connected = tud_connected();
#endif
      if (usb_connected != is_usb_connected) {
        is_usb_connected = usb_connected;
        i2c_manager_set_mode(is_usb_connected);
        usb_app_cdc_printf("USB %s, switching to %s mode\r\n", 
                     is_usb_connected ? "connected" : "disconnected",
                     is_usb_connected ? "master" : "slave");
        ws2812_apply_mode(is_usb_connected);
      }
      

    }
    
    // Additional I2C task processing
    i2c_manager_task();

#if USB_DEMO_ENABLE_MOUSE || USB_DEMO_ENABLE_MIDI
    // USB demo code here if enabled
#endif

#if USB_DEMO_ENABLE_MOUSE
    static uint32_t mouse_demo_tick = 0U;

    if ((now - mouse_demo_tick) >= 1000U)
    {
      (void) usb_app_mouse_report(0U, 4, 0, 0, 0);
      mouse_demo_tick = now;
    }
#endif

#if USB_DEMO_ENABLE_MIDI
    typedef enum
    {
      MIDI_DEMO_IDLE = 0,
      MIDI_DEMO_NOTE_ON
    } midi_demo_state_t;

    static midi_demo_state_t midi_demo_state = MIDI_DEMO_IDLE;
    static uint32_t midi_demo_tick = 0U;

    switch (midi_demo_state)
    {
      case MIDI_DEMO_IDLE:
        if ((now - midi_demo_tick) >= 2000U)
        {
          if (usb_app_midi_send_note_on(0U, 60U, 0x7FU))
          {
            midi_demo_state = MIDI_DEMO_NOTE_ON;
            midi_demo_tick = now;
          }
        }
        break;
      case MIDI_DEMO_NOTE_ON:
        if ((now - midi_demo_tick) >= 300U)
        {
          if (usb_app_midi_send_note_off(0U, 60U))
          {
            midi_demo_state = MIDI_DEMO_IDLE;
            midi_demo_tick = now;
          }
        }
        break;
      default:
        midi_demo_state = MIDI_DEMO_IDLE;
        midi_demo_tick = now;
        break;
    }
#endif
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }

  __HAL_RCC_HSI48_ENABLE();
  while (__HAL_RCC_GET_FLAG(RCC_FLAG_HSI48RDY) == RESET)
  {
  }

  __HAL_RCC_CRS_CLK_ENABLE();
  RCC_CRSInitTypeDef CRSInit = {0};
  CRSInit.Prescaler = RCC_CRS_SYNC_DIV1;
  CRSInit.Source = RCC_CRS_SYNC_SOURCE_USB;
  CRSInit.Polarity = RCC_CRS_SYNC_POLARITY_RISING;
  CRSInit.ReloadValue = __HAL_RCC_CRS_RELOADVALUE_CALCULATE(48000000U, 1000U);
  CRSInit.ErrorLimitValue = RCC_CRS_ERRORLIMIT_DEFAULT;
  CRSInit.HSI48CalibrationValue = RCC_CRS_HSI48CALIBRATION_DEFAULT;
  HAL_RCCEx_CRSConfig(&CRSInit);

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  i2c_manager_uart_rx_cplt_callback(huart);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  i2c_manager_uart_error_callback(huart);
}

/* USER CODE END 4 */

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
