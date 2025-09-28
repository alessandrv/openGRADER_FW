#include "usb_app.h"

#include "tusb.h"
#include "class/hid/hid.h"
#include "class/hid/hid_device.h"

#include "stm32g4xx_hal.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stddef.h>

#define HID_QUEUE_SIZE     64U
#define HID_QUEUE_MASK     (HID_QUEUE_SIZE - 1U)

static uint8_t hid_queue[HID_QUEUE_SIZE];
static uint16_t hid_queue_head;
static uint16_t hid_queue_tail;
static bool keyboard_pending_release;
static bool cdc_line_active;
static bool cdc_last_char_cr;
static void cdc_task(void);
static void hid_task(void);
static void midi_task(void);
static bool hid_queue_push(uint8_t value);
static bool hid_queue_pop(uint8_t *value);
static bool ascii_to_hid(uint8_t ascii, uint8_t *modifier, uint8_t *keycode);
static bool midi_write_packet(uint8_t const packet[4]);

void usb_app_init(void)
{
	__HAL_RCC_USB_CLK_ENABLE();

	HAL_NVIC_SetPriority(USB_LP_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(USB_LP_IRQn);
#ifdef USB_HP_IRQn
	HAL_NVIC_SetPriority(USB_HP_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(USB_HP_IRQn);
#endif
#ifdef USBWakeUp_IRQn
	HAL_NVIC_SetPriority(USBWakeUp_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(USBWakeUp_IRQn);
#endif

	tusb_init();
}

void usb_app_task(void)
{
	tud_task();
	cdc_task();
	hid_task();
	midi_task();
}

//--------------------------------------------------------------------+
// CDC task
//--------------------------------------------------------------------+

static void cdc_task(void)
{
	if (!tud_cdc_connected())
	{
		cdc_line_active = false;
		return;
	}

	if (!cdc_line_active)
	{
		cdc_line_active = true;
		tud_cdc_write_str("TinyUSB composite CDC + Keyboard + Mouse + MIDI ready.\r\n");
		tud_cdc_write_flush();
	}

	uint8_t buf[64];
	uint32_t count = tud_cdc_read(buf, sizeof(buf));

	if (!count)
	{
		return;
	}

	tud_cdc_write(buf, count);
	tud_cdc_write_flush();

	for (uint32_t i = 0; i < count; i++)
	{
		uint8_t ch = buf[i];

		if (ch == '\r')
		{
			hid_queue_push('\n');
			cdc_last_char_cr = true;
			continue;
		}

		if ((ch == '\n') && cdc_last_char_cr)
		{
			cdc_last_char_cr = false;
			continue;
		}

		cdc_last_char_cr = false;
		hid_queue_push(ch);
	}
}

//--------------------------------------------------------------------+
// HID task
//--------------------------------------------------------------------+

static void hid_task(void)
{
	if (keyboard_pending_release && tud_hid_n_ready(0))
	{
		tud_hid_n_keyboard_report(0, 0, 0, NULL);
		keyboard_pending_release = false;
	}

	if (tud_hid_n_ready(0) && !keyboard_pending_release)
	{
		uint8_t ascii;
		while (hid_queue_pop(&ascii))
		{
			uint8_t modifier = 0;
			uint8_t keycode = 0;

			if (!ascii_to_hid(ascii, &modifier, &keycode))
			{
				continue;
			}

			uint8_t keycodes[6] = { keycode, 0, 0, 0, 0, 0 };
			if (tud_hid_n_keyboard_report(0, 0, modifier, keycodes))
			{
				keyboard_pending_release = true;
				break;
			}
		}
	}
}

//--------------------------------------------------------------------+
// MIDI task
//--------------------------------------------------------------------+

static void midi_task(void)
{
	if (!tud_midi_mounted())
	{
		return;
	}

	uint8_t packet[4];
	while (tud_midi_packet_read(packet))
	{
		tud_midi_packet_write(packet);
	}
}

//--------------------------------------------------------------------+
// Queue helpers
//--------------------------------------------------------------------+

static bool hid_queue_push(uint8_t value)
{
	uint16_t next = (uint16_t) ((hid_queue_head + 1U) & HID_QUEUE_MASK);

	if (next == hid_queue_tail)
	{
		return false;
	}

	hid_queue[hid_queue_head] = value;
	hid_queue_head = next;
	return true;
}

static bool hid_queue_pop(uint8_t *value)
{
	if (hid_queue_head == hid_queue_tail)
	{
		return false;
	}

	*value = hid_queue[hid_queue_tail];
	hid_queue_tail = (uint16_t) ((hid_queue_tail + 1U) & HID_QUEUE_MASK);
	return true;
}

//--------------------------------------------------------------------+
// ASCII to HID conversion
//--------------------------------------------------------------------+

static bool ascii_to_hid(uint8_t ascii, uint8_t *modifier, uint8_t *keycode)
{
	*modifier = 0;
	*keycode = 0;

	if (ascii >= 'a' && ascii <= 'z')
	{
		*keycode = (uint8_t) (HID_KEY_A + (ascii - 'a'));
		return true;
	}

	if (ascii >= 'A' && ascii <= 'Z')
	{
		*modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
		*keycode = (uint8_t) (HID_KEY_A + (ascii - 'A'));
		return true;
	}

	if (ascii >= '1' && ascii <= '9')
	{
		*keycode = (uint8_t) (HID_KEY_1 + (ascii - '1'));
		return true;
	}

	switch (ascii)
	{
		case '0':
			*keycode = HID_KEY_0;
			return true;
		case '\n':
			*keycode = HID_KEY_ENTER;
			return true;
		case '\t':
			*keycode = HID_KEY_TAB;
			return true;
		case ' ':
			*keycode = HID_KEY_SPACE;
			return true;
		case '-':
			*keycode = HID_KEY_MINUS;
			return true;
		case '_':
			*modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
			*keycode = HID_KEY_MINUS;
			return true;
		case '=':
			*keycode = HID_KEY_EQUAL;
			return true;
		case '+':
			*modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
			*keycode = HID_KEY_EQUAL;
			return true;
		case '[':
			*keycode = HID_KEY_BRACKET_LEFT;
			return true;
		case '{':
			*modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
			*keycode = HID_KEY_BRACKET_LEFT;
			return true;
		case ']':
			*keycode = HID_KEY_BRACKET_RIGHT;
			return true;
		case '}':
			*modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
			*keycode = HID_KEY_BRACKET_RIGHT;
			return true;
		case ';':
			*keycode = HID_KEY_SEMICOLON;
			return true;
		case ':':
			*modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
			*keycode = HID_KEY_SEMICOLON;
			return true;
		case '\'':
			*keycode = HID_KEY_APOSTROPHE;
			return true;
		case '"':
			*modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
			*keycode = HID_KEY_APOSTROPHE;
			return true;
		case '`':
			*keycode = HID_KEY_GRAVE;
			return true;
		case '~':
			*modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
			*keycode = HID_KEY_GRAVE;
			return true;
		case ',':
			*keycode = HID_KEY_COMMA;
			return true;
		case '<':
			*modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
			*keycode = HID_KEY_COMMA;
			return true;
		case '.':
			*keycode = HID_KEY_PERIOD;
			return true;
		case '>':
			*modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
			*keycode = HID_KEY_PERIOD;
			return true;
		case '/':
			*keycode = HID_KEY_SLASH;
			return true;
		case '?':
			*modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
			*keycode = HID_KEY_SLASH;
			return true;
		case '!':
			*modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
			*keycode = HID_KEY_1;
			return true;
		case '@':
			*modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
			*keycode = HID_KEY_2;
			return true;
		case '#':
			*modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
			*keycode = HID_KEY_3;
			return true;
		case '$':
			*modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
			*keycode = HID_KEY_4;
			return true;
		case '%':
			*modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
			*keycode = HID_KEY_5;
			return true;
		case '^':
			*modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
			*keycode = HID_KEY_6;
			return true;
		case '&':
			*modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
			*keycode = HID_KEY_7;
			return true;
		case '*':
			*modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
			*keycode = HID_KEY_8;
			return true;
		case '(':
			*modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
			*keycode = HID_KEY_9;
			return true;
		case ')':
			*modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
			*keycode = HID_KEY_0;
			return true;
		default:
			return false;
	}
}

//--------------------------------------------------------------------+
// Public helpers
//--------------------------------------------------------------------+

bool usb_app_mouse_report(uint8_t buttons, int8_t delta_x, int8_t delta_y, int8_t wheel, int8_t pan)
{
	if (!tud_hid_n_ready(1))
	{
		return false;
	}

	return tud_hid_n_mouse_report(1, 0, buttons, delta_x, delta_y, wheel, pan);
}

bool usb_app_midi_send_packet(uint8_t const packet[4])
{
	if (packet == NULL)
	{
		return false;
	}

	return midi_write_packet(packet);
}

bool usb_app_midi_send_note_on(uint8_t channel, uint8_t note, uint8_t velocity)
{
	if (channel > 15U)
	{
		return false;
	}

	uint8_t const packet[4] = { 0x09, (uint8_t) (0x90U | (channel & 0x0FU)), note, velocity };
	return midi_write_packet(packet);
}

bool usb_app_midi_send_note_off(uint8_t channel, uint8_t note)
{
	if (channel > 15U)
	{
		return false;
	}

	uint8_t const packet[4] = { 0x08, (uint8_t) (0x80U | (channel & 0x0FU)), note, 0x00 };
	return midi_write_packet(packet);
}

bool usb_app_midi_send_cc(uint8_t channel, uint8_t controller, uint8_t value)
{
	if (channel > 15U)
	{
		return false;
	}

	uint8_t const packet[4] = { 0x0B, (uint8_t) (0xB0U | (channel & 0x0FU)), controller, value };
	return midi_write_packet(packet);
}

//--------------------------------------------------------------------+
// TinyUSB callbacks
//--------------------------------------------------------------------+

void tud_mount_cb(void)
{
	hid_queue_head = 0;
	hid_queue_tail = 0;
	keyboard_pending_release = false;
}

void tud_umount_cb(void)
{
	hid_queue_head = 0;
	hid_queue_tail = 0;
	keyboard_pending_release = false;
	cdc_line_active = false;
}

void tud_suspend_cb(bool remote_wakeup_en)
{
	(void) remote_wakeup_en;
}

//--------------------------------------------------------------------+
// Internal helpers
//--------------------------------------------------------------------+

static bool midi_write_packet(uint8_t const packet[4])
{
	if (!tud_midi_mounted())
	{
		return false;
	}

	return tud_midi_packet_write(packet);
}

//--------------------------------------------------------------------+
// Public helpers
//--------------------------------------------------------------------+

void usb_app_cdc_printf(const char *format, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (len > 0)
    {
        tud_cdc_write(buffer, (uint32_t)len);
        tud_cdc_write_flush();
    }
}

void tud_resume_cb(void)
{
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
	(void) instance;
	(void) report_id;
	(void) report_type;
	(void) buffer;
	(void) reqlen;
	return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
	(void) instance;
	(void) report_id;
	(void) report_type;
	(void) buffer;
	(void) bufsize;
}
