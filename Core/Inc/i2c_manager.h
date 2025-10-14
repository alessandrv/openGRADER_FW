#ifndef I2C_MANAGER_H
#define I2C_MANAGER_H

#include "stm32g4xx_hal.h"
#include "i2c_protocol.h"
#include <stdbool.h>

/* I2C Communication Manager
 * Handles master/slave mode switching and communication
 */

/* Public function declarations */
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
void i2c_manager_process_local_key_event(uint8_t row, uint8_t col, uint8_t pressed, uint8_t keycode);
void i2c_manager_handle_slave_key_event(const i2c_key_event_t *event);
void i2c_manager_handle_slave_midi_event(const i2c_midi_event_t *event);
void i2c_manager_handle_slave_layer_state(const i2c_layer_state_t *event);
void i2c_manager_broadcast_layer_state(uint8_t layer_mask, uint8_t default_layer);

/* Encoder callback for slave mode */
void i2c_manager_encoder_callback(uint8_t encoder_idx, uint8_t direction, uint8_t keycode);

/* I2C slave callbacks (to be called from HAL interrupt handlers) */
void i2c_manager_addr_callback(I2C_HandleTypeDef *hi2c, uint8_t TransferDirection, uint16_t AddrMatchCode);
void i2c_manager_slave_rx_complete_callback(I2C_HandleTypeDef *hi2c);
void i2c_manager_slave_tx_complete_callback(I2C_HandleTypeDef *hi2c);
void i2c_manager_listen_complete_callback(I2C_HandleTypeDef *hi2c);
void i2c_manager_error_callback(I2C_HandleTypeDef *hi2c);

#endif /* I2C_MANAGER_H */