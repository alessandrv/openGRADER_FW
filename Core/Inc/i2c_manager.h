#ifndef I2C_MANAGER_H
#define I2C_MANAGER_H

#include "stm32g4xx_hal.h"
#include "i2c_protocol.h"
#include <stdbool.h>

/* I2C Communication Manager
 * Handles master/slave mode switching and communication
 */

/* Public function declarations */
#define I2C_SLAVE_ADDRESS_BASE 0x42u
#define I2C_MAX_SLAVE_COUNT 8u

void i2c_manager_init(void);
void i2c_manager_set_mode(uint8_t is_master);
uint8_t i2c_manager_get_mode(void);
void i2c_manager_task(void);

/* Slave mode functions */
void i2c_manager_send_key_event(uint8_t row, uint8_t col, uint8_t pressed, uint8_t keycode);
void i2c_manager_send_layer_state(uint8_t layer_mask, uint8_t default_layer);
void i2c_manager_send_midi_cc(uint8_t channel, uint8_t controller, uint8_t value);
void i2c_manager_send_midi_note(uint8_t channel, uint8_t note, uint8_t velocity, bool pressed);

/* Master mode functions */
void i2c_manager_poll_slaves(void);
void i2c_manager_scan_slaves(void);
void i2c_manager_scan_slaves_force(void);
void i2c_manager_process_local_key_event(uint8_t row, uint8_t col, uint8_t pressed, uint8_t keycode);
void i2c_manager_handle_slave_key_event(const i2c_key_event_t *event);
void i2c_manager_handle_slave_midi_event(const i2c_midi_event_t *event);
void i2c_manager_handle_slave_layer_state(const i2c_layer_state_t *event);
void i2c_manager_broadcast_layer_state(uint8_t layer_mask, uint8_t default_layer);

bool i2c_manager_config_roundtrip(uint8_t slave_address,
								  const uint8_t *request,
								  uint8_t request_len,
								  uint8_t *response,
								  uint8_t *response_len,
								  uint32_t timeout_ms);

/* Encoder callback for slave mode */
void i2c_manager_encoder_callback(uint8_t encoder_idx, uint8_t direction, uint8_t keycode);

void i2c_manager_uart_rx_cplt_callback(UART_HandleTypeDef *huart);
void i2c_manager_uart_error_callback(UART_HandleTypeDef *huart);

#endif /* I2C_MANAGER_H */