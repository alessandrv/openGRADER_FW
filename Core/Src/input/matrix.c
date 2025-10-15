#include "matrix.h"
#include "keymap.h"
#include "main.h"
#include "midi_handler.h"
#include "op_keycodes.h"

static matrix_event_cb_t user_cb = NULL;
static uint8_t state[MATRIX_ROWS][MATRIX_COLS] = {0};
static uint16_t active_keycode_cache[MATRIX_ROWS][MATRIX_COLS] = {0};

void matrix_register_callback(matrix_event_cb_t cb)
{
    user_cb = cb;
}

void matrix_init(void)
{
    // Ensure GPIO clocks are enabled for ports we may use (safe default A..E)
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    // Configure column pins as outputs (push-pull, low) and row pins as inputs
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // Configure all column pins as outputs with high speed
    for (uint8_t c = 0; c < MATRIX_COLS; ++c)
    {
        GPIO_InitStruct.Pin = matrix_cols[c].pin;
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH; // High speed for faster switching
        HAL_GPIO_Init(matrix_cols[c].port, &GPIO_InitStruct);
        HAL_GPIO_WritePin(matrix_cols[c].port, matrix_cols[c].pin, GPIO_PIN_RESET);
    }

    // Configure row pins with provided pull settings
    for (uint8_t r = 0; r < MATRIX_ROWS; ++r)
    {
        GPIO_InitStruct.Pin = matrix_rows[r].pin;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = matrix_row_pulls[r];
        HAL_GPIO_Init(matrix_rows[r].port, &GPIO_InitStruct);

        for (uint8_t c = 0; c < MATRIX_COLS; ++c) {
            active_keycode_cache[r][c] = KC_NO;
        }
    }
}

void matrix_scan(void)
{
    // Scan all columns in one call for better responsiveness
    for (uint8_t c = 0; c < MATRIX_COLS; ++c) {
        // drive this column high
        HAL_GPIO_WritePin(matrix_cols[c].port, matrix_cols[c].pin, GPIO_PIN_SET);
        // minimal settle time for high-speed scanning
        for (volatile int i = 0; i < 20; ++i) __NOP();

        for (uint8_t r = 0; r < MATRIX_ROWS; ++r)
        {
            GPIO_PinState s = HAL_GPIO_ReadPin(matrix_rows[r].port, matrix_rows[r].pin);
            uint8_t pressed = (s == GPIO_PIN_SET) ? 1 : 0;
            if (pressed != state[r][c])
            {
                state[r][c] = pressed;
                uint16_t kc = pressed ? keymap_get_active_keycode(r, c)
                                      : active_keycode_cache[r][c];

                if (pressed) {
                    active_keycode_cache[r][c] = kc;
                } else {
                    active_keycode_cache[r][c] = KC_NO;
                    if (kc == KC_NO) {
                        kc = keymap_get_active_keycode(r, c);
                    }
                }

                if (kc == KC_NO) {
                    continue;
                }

                // Handle MIDI keycodes
                midi_handle_keycode(kc, pressed);

                uint8_t hid = 0;
                bool should_send = keymap_translate_keycode(kc, pressed, &hid);

                if (should_send && user_cb)
                {
                    user_cb(r, c, pressed, hid);
                }
            }
        }

        // release column quickly
        HAL_GPIO_WritePin(matrix_cols[c].port, matrix_cols[c].pin, GPIO_PIN_RESET);
    }
}
