/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : key_state.c
  * @brief          : Key state management implementation
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
#include "key_state.h"
#include "main.h"
#include "usb_app.h"
#include "tusb.h"

#include <stdbool.h>

/* Private variables ---------------------------------------------------------*/
static uint8_t pressed_keys[MAX_PRESSED_KEYS] = {0};
static uint8_t pressed_key_count = 0;
static bool hid_report_dirty = false;

typedef enum {
  ENCODER_TAP_IDLE = 0,
  ENCODER_TAP_WAIT_PRESS_FLUSH,
  ENCODER_TAP_WAIT_RELEASE_FLUSH
} encoder_tap_state_t;

#define ENCODER_TAP_DELAY_MS 5U

static encoder_tap_state_t encoder_tap_state = ENCODER_TAP_IDLE;
static uint8_t encoder_tap_keycode = 0;
static uint32_t encoder_tap_release_time = 0;
static void key_state_force_encoder_release(void);

/* Public functions ----------------------------------------------------------*/

/**
  * @brief Initialize key state management
  * @param None
  * @retval None
  */
void key_state_init(void)
{
  pressed_key_count = 0;
  for (uint8_t i = 0; i < MAX_PRESSED_KEYS; i++) {
    pressed_keys[i] = 0;
  }
  hid_report_dirty = false;
  encoder_tap_state = ENCODER_TAP_IDLE;
  encoder_tap_keycode = 0;
  encoder_tap_release_time = 0;
  usb_app_cdc_printf("Key state management initialized\r\n");
}

/**
  * @brief Add a key to the pressed keys array
  * @param keycode: HID keycode to add
  * @retval None
  */
void key_state_add_key(uint8_t keycode)
{
  // Check if key is already in the array
  for (uint8_t i = 0; i < pressed_key_count; i++) {
    if (pressed_keys[i] == keycode) {
      usb_app_cdc_printf("Key 0x%02X already pressed, ignoring\r\n", keycode);
      return; // Already pressed
    }
  }
  
  // Add key if there's space
  if (pressed_key_count < MAX_PRESSED_KEYS) {
    pressed_keys[pressed_key_count] = keycode;
    pressed_key_count++;
    usb_app_cdc_printf("Added key 0x%02X, total pressed: %d\r\n", keycode, pressed_key_count);
  } else {
    usb_app_cdc_printf("Key buffer full, dropping key 0x%02X\r\n", keycode);
  }
}

/**
  * @brief Remove a key from the pressed keys array
  * @param keycode: HID keycode to remove
  * @retval None
  */
void key_state_remove_key(uint8_t keycode)
{
  // Find and remove the key
  for (uint8_t i = 0; i < pressed_key_count; i++) {
    if (pressed_keys[i] == keycode) {
      // Shift remaining keys down
      for (uint8_t j = i; j < pressed_key_count - 1; j++) {
        pressed_keys[j] = pressed_keys[j + 1];
      }
      pressed_key_count--;
      pressed_keys[pressed_key_count] = 0; // Clear the last slot
      usb_app_cdc_printf("Removed key 0x%02X, total pressed: %d\r\n", keycode, pressed_key_count);
      return;
    }
  }
  usb_app_cdc_printf("Key 0x%02X not found in pressed array\r\n", keycode);
}

/**
  * @brief Update USB HID report with current pressed keys and send it
  * @param None
  * @retval None
  */
void key_state_update_hid_report(void)
{
  hid_report_dirty = true;
  key_state_task();
}

/**
  * @brief Send momentary encoder event without affecting held matrix keys
  * @param keycode: HID keycode for encoder event
  * @retval None
  */
void key_state_send_encoder_event(uint8_t keycode)
{
  usb_app_cdc_printf("Encoder event: keycode=0x%02X with %d held keys\r\n", keycode, pressed_key_count);
  
  if (encoder_tap_state != ENCODER_TAP_IDLE) {
    usb_app_cdc_printf("Encoder tap: previous event pending, forcing release\r\n");
    key_state_force_encoder_release();
    key_state_task();
  }

  encoder_tap_keycode = keycode;
  encoder_tap_release_time = HAL_GetTick() + ENCODER_TAP_DELAY_MS;
  encoder_tap_state = ENCODER_TAP_WAIT_PRESS_FLUSH;

  key_state_add_key(keycode);
  key_state_update_hid_report();
}

/**
  * @brief Get number of currently pressed keys
  * @param None
  * @retval Number of pressed keys
  */
uint8_t key_state_get_pressed_count(void)
{
  return pressed_key_count;
}

static void key_state_try_flush(void)
{
  if (!hid_report_dirty) {
    return;
  }

  if (!tud_hid_n_ready(0)) {
    return;
  }

  uint8_t report[8] = {0};
  for (uint8_t i = 0; i < pressed_key_count && i < 6; i++) {
    report[2 + i] = pressed_keys[i];
  }

  bool sent = tud_hid_n_keyboard_report(0, 0, report[0], &report[2]);
  usb_app_cdc_printf("HID report updated: [%02X %02X %02X %02X %02X %02X], %s\r\n",
               report[2], report[3], report[4], report[5], report[6], report[7],
               sent ? "sent" : "queued");

  if (sent) {
    hid_report_dirty = false;
  }
}

void key_state_task(void)
{
  key_state_try_flush();

  switch (encoder_tap_state) {
    case ENCODER_TAP_WAIT_PRESS_FLUSH:
      if (!hid_report_dirty && (int32_t)(HAL_GetTick() - encoder_tap_release_time) >= 0) {
        usb_app_cdc_printf("Encoder tap: triggering release for keycode=0x%02X\r\n", encoder_tap_keycode);
        key_state_remove_key(encoder_tap_keycode);
        hid_report_dirty = true;
        encoder_tap_state = ENCODER_TAP_WAIT_RELEASE_FLUSH;
      }
      break;
    case ENCODER_TAP_WAIT_RELEASE_FLUSH:
      if (!hid_report_dirty) {
        usb_app_cdc_printf("Encoder tap: completed for keycode=0x%02X\r\n", encoder_tap_keycode);
        encoder_tap_state = ENCODER_TAP_IDLE;
        encoder_tap_keycode = 0;
      }
      break;
    default:
      break;
  }

  key_state_try_flush();
}

static void key_state_force_encoder_release(void)
{
  if (encoder_tap_state == ENCODER_TAP_IDLE) {
    return;
  }

  key_state_remove_key(encoder_tap_keycode);
  hid_report_dirty = true;
  encoder_tap_state = ENCODER_TAP_IDLE;
  encoder_tap_keycode = 0;
}