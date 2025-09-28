#include "encoder.h"
#include "keymap.h"
#include "usb_app.h"
#include "main.h"
#include <stddef.h>
#include "pin_config.h"
#include "midi_handler.h"
#include "i2c_manager.h"

// Encoder event callback for slave mode
static encoder_event_cb_t encoder_user_cb = NULL;

typedef struct {
	GPIO_TypeDef *port;
	uint16_t pin;
} gpio_pin_t;

static inline uint8_t read_pin(const pin_t *p) {
	return (uint8_t)HAL_GPIO_ReadPin(p->port, p->pin);
}

// Per-encoder state
typedef struct {
	uint8_t state;        // last 2-bit state (B<<1 | A)
	uint8_t rest_state;   // detected idle state (usually 3 with pull-ups)
	uint8_t armed;        // only emit an event when armed; re-arm on rest
	int8_t step;          // accumulates +1/-1 per valid transition
	uint32_t last_push_ms;// last time we enqueued an event (debounce/oneshot)
} enc_state_t;

static enc_state_t enc[ENCODER_COUNT];

// simple ring buffer of events
typedef enum { ENC_NONE=0, ENC_CW=1, ENC_CCW=-1 } enc_dir_t;
typedef struct { uint8_t idx; enc_dir_t dir; } enc_event_t;
#define ENC_EVT_QSIZE 8
static volatile enc_event_t q[ENC_EVT_QSIZE];
static volatile uint8_t q_head = 0, q_tail = 0;

static void q_push(uint8_t idx, enc_dir_t dir) {
	uint8_t n = (uint8_t)((q_head + 1) & (ENC_EVT_QSIZE-1));
	if (n == q_tail) return; // drop if full
	q[q_head].idx = idx;
	q[q_head].dir = dir;
	q_head = n;
}

static int q_pop(enc_event_t *out) {
	if (q_head == q_tail) return 0;
	*out = (enc_event_t){ q[q_tail].idx, q[q_tail].dir };
	q_tail = (uint8_t)((q_tail + 1) & (ENC_EVT_QSIZE-1));
	return 1;
}

static uint8_t get_state(uint8_t i) {
	uint8_t a = read_pin(&encoder_pins[i].pin_a);
	uint8_t b = read_pin(&encoder_pins[i].pin_b);
	return (uint8_t)((b << 1) | a);
}

static const int8_t state_pos[4] = {0, 1, 3, 2};

// Number of valid quarter-steps per detent click. Many encoders produce 4.
#ifndef ENC_STEPS_PER_DETENT
#define ENC_STEPS_PER_DETENT 4
#endif

void encoder_init(void)
{
	// Enable GPIO clocks for used ports (A..E common case)
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();
	__HAL_RCC_GPIOE_CLK_ENABLE();

	// Configure pins as simple inputs with the requested pull resistors.
	// Polling approach scales to many encoders regardless of EXTI line sharing.
	GPIO_InitTypeDef gi = {0};
	gi.Mode = GPIO_MODE_INPUT;
	gi.Speed = GPIO_SPEED_FREQ_LOW;

	for (uint8_t i = 0; i < ENCODER_COUNT; ++i) {
		// Configure A
		gi.Pin = encoder_pins[i].pin_a.pin;
		gi.Pull = encoder_pulls[i];
		HAL_GPIO_Init(encoder_pins[i].pin_a.port, &gi);
		// Configure B
		gi.Pin = encoder_pins[i].pin_b.pin;
		HAL_GPIO_Init(encoder_pins[i].pin_b.port, &gi);

		enc[i].state = get_state(i);
		enc[i].rest_state = enc[i].state; // treat current as idle
		enc[i].armed = 1;
		enc[i].step = 0;
		enc[i].last_push_ms = 0;
	}
}

void encoder_register_callback(encoder_event_cb_t cb)
{
	encoder_user_cb = cb;
}

// Retain EXTI hook for future use; with polling mode, this is unused.
void encoder_handle_exti(uint16_t pin)
{
	(void)pin;
}

// Simplified encoder handling - now delegates to main.c for proper key state management

void encoder_task(void)
{
	// 1) Poll all encoders and decode transitions into the event queue
	for (uint8_t i = 0; i < ENCODER_COUNT; ++i) {





		uint8_t ns = get_state(i);
		uint8_t os = enc[i].state;
		if (ns != os) {
			int8_t delta = state_pos[ns] - state_pos[os];
			if (delta > 2) {
				delta -= 4;
			} else if (delta < -2) {
				delta += 4;
			}

			if (delta != 0) {
				enc[i].step += delta;
			}

			enc[i].state = ns;

			if (enc[i].armed) {
				if (enc[i].step >= ENC_STEPS_PER_DETENT) {
					uint32_t now = HAL_GetTick();
					if (now - enc[i].last_push_ms >= 20U) {
						enc[i].last_push_ms = now;
						usb_app_cdc_printf("Encoder %d: CW event (step=%d, now=%lu)\r\n", i, enc[i].step, now);
						q_push(i, ENC_CW);
						enc[i].armed = 0;
					}
					enc[i].step = 0;
				} else if (enc[i].step <= -ENC_STEPS_PER_DETENT) {
					uint32_t now = HAL_GetTick();
					if (now - enc[i].last_push_ms >= 20U) {
						enc[i].last_push_ms = now;
						usb_app_cdc_printf("Encoder %d: CCW event (step=%d, now=%lu)\r\n", i, enc[i].step, now);
						q_push(i, ENC_CCW);
						enc[i].armed = 0;
					}
					enc[i].step = 0;
				}
			} else {
				// Not armed - keep accumulator bounded
				enc[i].step = 0;
			}

			if (ns == enc[i].rest_state) {
				enc[i].armed = 1;
				enc[i].step = 0;
			}
		}
	}

	// 2) Process encoder events - either via callback (slave) or immediate HID (master)
	enc_event_t ev;
	if (q_pop(&ev)) {
		uint16_t keycode = encoder_map[ev.idx][(ev.dir == ENC_CW) ? 1 : 0];
		usb_app_cdc_printf("Processing encoder event: idx=%d, dir=%s, keycode=0x%04X\r\n", 
		             ev.idx, (ev.dir == ENC_CW) ? "CW" : "CCW", keycode);
		
		// Handle MIDI keycodes first (like matrix processing does)
		midi_handle_keycode(keycode, 1); // Press
		midi_handle_keycode(keycode, 0); // Release (encoder events are momentary)
		
		// Handle HID events via shared I2C manager queue for both master and slave
		uint8_t hid_keycode = op_keycode_to_hid(keycode);
		if (hid_keycode != 0) {
			uint8_t direction_flag = (ev.dir == ENC_CW) ? 1U : 0U;
			usb_app_cdc_printf("Queueing encoder HID event via I2C manager (dir=%s)\r\n",
						 (direction_flag == 1U) ? "CW" : "CCW");
			i2c_manager_process_local_key_event(254, ev.idx, direction_flag, hid_keycode);
		}
	}
}
