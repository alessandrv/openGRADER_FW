#ifndef KEY_STATE_H
#define KEY_STATE_H

#include <stdint.h>

#define MAX_PRESSED_KEYS 6

void key_state_init(void);
void key_state_add_key(uint8_t keycode);
void key_state_remove_key(uint8_t keycode);
void key_state_update_hid_report(void);
void key_state_send_encoder_event(uint8_t keycode);
uint8_t key_state_get_pressed_count(void);
void key_state_task(void);

#endif /* KEY_STATE_H */