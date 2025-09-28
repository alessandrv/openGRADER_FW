/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : key_state.h
  * @brief          : Header for key_state.c file.
  *                   This file contains function prototypes for key state management.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __KEY_STATE_H
#define __KEY_STATE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g4xx_hal.h"

/* Exported types ------------------------------------------------------------*/
typedef struct {
	uint8_t MODIFIER;
	uint8_t RESERVED;
	uint8_t KEYCODE1;
	uint8_t KEYCODE2;
	uint8_t KEYCODE3;
	uint8_t KEYCODE4;
	uint8_t KEYCODE5;
	uint8_t KEYCODE6;
} KeyboardHIDReport;

/* Exported constants --------------------------------------------------------*/
#define MAX_PRESSED_KEYS 6  // USB HID supports up to 6 simultaneous keys

/* Exported functions prototypes ---------------------------------------------*/
void key_state_init(void);
void key_state_add_key(uint8_t keycode);
void key_state_remove_key(uint8_t keycode);
void key_state_update_hid_report(void);
void key_state_send_encoder_event(uint8_t keycode);
uint8_t key_state_get_pressed_count(void);
void key_state_task(void);

#ifdef __cplusplus
}
#endif

#endif /* __KEY_STATE_H */