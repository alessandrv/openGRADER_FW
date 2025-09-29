#include "tusb.h"
#include "class/hid/hid.h"
#include "device/usbd.h"
#include <string.h>

#define USB_VID  0xCafe
#define USB_PID  0x4011
#define USB_BCD  0x0200

enum
{
	ITF_NUM_CDC = 0,
	ITF_NUM_CDC_DATA,
	ITF_NUM_HID_KEYBOARD,
	ITF_NUM_HID_MOUSE,
	ITF_NUM_HID_CONFIG,     // Custom configuration HID interface
	ITF_NUM_MIDI_CONTROL,
	ITF_NUM_MIDI_STREAMING,
	ITF_NUM_TOTAL
};

#define EPNUM_CDC_NOTIF   0x81
#define EPNUM_CDC_OUT     0x02
#define EPNUM_CDC_IN      0x82
#define EPNUM_HID_KEYBOARD_IN 0x83
#define EPNUM_HID_MOUSE_IN    0x84
#define EPNUM_HID_CONFIG_OUT  0x06
#define EPNUM_HID_CONFIG_IN   0x86
#define EPNUM_MIDI_OUT        0x05
#define EPNUM_MIDI_IN         0x85

enum
{
	STRID_LANGID = 0,
	STRID_MANUFACTURER,
	STRID_PRODUCT,
	STRID_SERIAL,
	STRID_CDC,
	STRID_KEYBOARD,
	STRID_MOUSE,
	STRID_CONFIG,
	STRID_MIDI,
};

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_HID_DESC_LEN + TUD_HID_DESC_LEN + TUD_HID_INOUT_DESC_LEN + TUD_MIDI_DESC_LEN)

//--------------------------------------------------------------------+
// HID Report Descriptor
//--------------------------------------------------------------------+

static uint8_t const desc_hid_report_keyboard[] =
{
	TUD_HID_REPORT_DESC_KEYBOARD()
};

static uint8_t const desc_hid_report_mouse[] =
{
	TUD_HID_REPORT_DESC_MOUSE()
};

// Custom HID report descriptor for configuration interface
static uint8_t const desc_hid_report_config[] =
{
	TUD_HID_REPORT_DESC_GENERIC_INOUT(64)
};

//--------------------------------------------------------------------+
// Device Descriptor
//--------------------------------------------------------------------+

tusb_desc_device_t const desc_device =
{
		.bLength            = sizeof(tusb_desc_device_t),
		.bDescriptorType    = TUSB_DESC_DEVICE,
		.bcdUSB             = USB_BCD,
		.bDeviceClass       = TUSB_CLASS_MISC,
		.bDeviceSubClass    = MISC_SUBCLASS_COMMON,
		.bDeviceProtocol    = MISC_PROTOCOL_IAD,
		.bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

		.idVendor           = USB_VID,
		.idProduct          = USB_PID,
		.bcdDevice          = 0x0100,

		.iManufacturer      = 0x01,
		.iProduct           = 0x02,
		.iSerialNumber      = 0x03,

		.bNumConfigurations = 0x01
};

uint8_t const *tud_descriptor_device_cb(void)
{
	return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+

uint8_t const desc_configuration[] =
{
	TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),
	TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, STRID_CDC, EPNUM_CDC_NOTIF, 16, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
	TUD_HID_DESCRIPTOR(ITF_NUM_HID_KEYBOARD, STRID_KEYBOARD, HID_ITF_PROTOCOL_KEYBOARD, sizeof(desc_hid_report_keyboard), EPNUM_HID_KEYBOARD_IN, CFG_TUD_HID_EP_BUFSIZE, CFG_TUD_HID_POLL_INTERVAL),
	TUD_HID_DESCRIPTOR(ITF_NUM_HID_MOUSE, STRID_MOUSE, HID_ITF_PROTOCOL_MOUSE, sizeof(desc_hid_report_mouse), EPNUM_HID_MOUSE_IN, CFG_TUD_HID_EP_BUFSIZE, CFG_TUD_HID_POLL_INTERVAL),
	TUD_HID_INOUT_DESCRIPTOR(ITF_NUM_HID_CONFIG, STRID_CONFIG, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report_config), EPNUM_HID_CONFIG_OUT, EPNUM_HID_CONFIG_IN, 64, CFG_TUD_HID_POLL_INTERVAL),
	TUD_MIDI_DESCRIPTOR(ITF_NUM_MIDI_CONTROL, STRID_MIDI, EPNUM_MIDI_OUT, EPNUM_MIDI_IN, CFG_TUD_MIDI_EP_BUFSIZE)
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
	(void) index;
	return desc_configuration;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

static char const *string_desc_arr[] =
{
	[STRID_LANGID]      = "\x09\x04",                   // 0: English (0x0409)
	[STRID_MANUFACTURER] = "OpenGrader Labs",            // 1: Manufacturer
	[STRID_PRODUCT]      = "OpenGrader Modular Keyboard", // 2: Product
	[STRID_SERIAL]       = "0001",                      // 3: Serial
	[STRID_CDC]          = "Virtual COM",               // 4: CDC Interface
	[STRID_KEYBOARD]     = "Keyboard",                  // 5: Keyboard HID Interface
	[STRID_MOUSE]        = "Mouse",                     // 6: Mouse HID Interface
	[STRID_CONFIG]       = "Configuration Interface",  // 7: Config HID Interface
	[STRID_MIDI]         = "MIDI Port"                  // 8: MIDI Interface
};

static uint16_t _desc_str[32];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
	(void) langid;

	if (index == 0)
	{
		_desc_str[1] = 0x0409;
		_desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8) | (2 + 2));
		return _desc_str;
	}

	if (index >= TU_ARRAY_SIZE(string_desc_arr))
	{
		return NULL;
	}

	uint8_t len = (uint8_t) strlen(string_desc_arr[index]);
	if (len > 31)
	{
		len = 31;
	}

	for (size_t i = 0; i < len; i++)
	{
		_desc_str[1 + i] = string_desc_arr[index][i];
	}

	_desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8) | ((uint8_t) ((len * 2) + 2)));
	return _desc_str;
}

//--------------------------------------------------------------------+
// HID report descriptor callback
//--------------------------------------------------------------------+

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
	switch (instance)
	{
	case 0:
		return desc_hid_report_keyboard;
	case 1:
		return desc_hid_report_mouse;
	case 2:
		return desc_hid_report_config;
	default:
		return NULL;
	}
}
