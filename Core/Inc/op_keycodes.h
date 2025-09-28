

#pragma once
#include <stdint.h>

// clang-format off

#define OP_KEYCODES_VERSION "0.0.1"
#define OP_KEYCODES_VERSION_BCD 0x00000001
#define OP_KEYCODES_VERSION_MAJOR 0
#define OP_KEYCODES_VERSION_MINOR 0
#define OP_KEYCODES_VERSION_PATCH 1

enum op_keycode_ranges {
// Ranges
    OP_BASIC                       = 0x0000,
    OP_BASIC_MAX                   = 0x00FF,
    OP_MODS                        = 0x0100,
    OP_MODS_MAX                    = 0x1FFF,
    OP_MOD_TAP                     = 0x2000,
    OP_MOD_TAP_MAX                 = 0x3FFF,
    OP_LAYER_TAP                   = 0x4000,
    OP_LAYER_TAP_MAX               = 0x4FFF,
    OP_LAYER_MOD                   = 0x5000,
    OP_LAYER_MOD_MAX               = 0x51FF,
    OP_TO                          = 0x5200,
    OP_TO_MAX                      = 0x521F,
    OP_MOMENTARY                   = 0x5220,
    OP_MOMENTARY_MAX               = 0x523F,
    OP_DEF_LAYER                   = 0x5240,
    OP_DEF_LAYER_MAX               = 0x525F,
    OP_TOGGLE_LAYER                = 0x5260,
    OP_TOGGLE_LAYER_MAX            = 0x527F,
    OP_ONE_SHOT_LAYER              = 0x5280,
    OP_ONE_SHOT_LAYER_MAX          = 0x529F,
    OP_ONE_SHOT_MOD                = 0x52A0,
    OP_ONE_SHOT_MOD_MAX            = 0x52BF,
    OP_LAYER_TAP_TOGGLE            = 0x52C0,
    OP_LAYER_TAP_TOGGLE_MAX        = 0x52DF,
    OP_PERSISTENT_DEF_LAYER        = 0x52E0,
    OP_PERSISTENT_DEF_LAYER_MAX    = 0x52FF,
    OP_SWAP_HANDS                  = 0x5600,
    OP_SWAP_HANDS_MAX              = 0x56FF,
    OP_TAP_DANCE                   = 0x5700,
    OP_TAP_DANCE_MAX               = 0x57FF,
    OP_MAGIC                       = 0x7000,
    OP_MAGIC_MAX                   = 0x70FF,
    OP_MIDI                        = 0x7100,
    OP_MIDI_MAX                    = 0x71FF,
    OP_SEQUENCER                   = 0x7200,
    OP_SEQUENCER_MAX               = 0x73FF,
    OP_JOYSTICK                    = 0x7400,
    OP_JOYSTICK_MAX                = 0x743F,
    OP_PROGRAMMABLE_BUTTON         = 0x7440,
    OP_PROGRAMMABLE_BUTTON_MAX     = 0x747F,
    OP_AUDIO                       = 0x7480,
    OP_AUDIO_MAX                   = 0x74BF,
    OP_STENO                       = 0x74C0,
    OP_STENO_MAX                   = 0x74FF,
    OP_MACRO                       = 0x7700,
    OP_MACRO_MAX                   = 0x777F,
    OP_CONNECTION                  = 0x7780,
    OP_CONNECTION_MAX              = 0x77BF,
    OP_COMMUNITY_MODULE            = 0x77C0,
    OP_COMMUNITY_MODULE_MAX        = 0x77FF,
    OP_LIGHTING                    = 0x7800,
    OP_LIGHTING_MAX                = 0x78FF,
    OP_QUANTUM                     = 0x7C00,
    OP_QUANTUM_MAX                 = 0x7DFF,
    OP_KB                          = 0x7E00,
    OP_KB_MAX                      = 0x7E3F,
    OP_USER                        = 0x7E40,
    OP_USER_MAX                    = 0x7FFF,
    OP_UNICODEMAP                  = 0x8000,
    OP_UNICODEMAP_MAX              = 0xBFFF,
    OP_UNICODE                     = 0x8000,
    OP_UNICODE_MAX                 = 0xFFFF,
    OP_UNICODEMAP_PAIR             = 0xC000,
    OP_UNICODEMAP_PAIR_MAX         = 0xFFFF,
};

enum op_keycode_defines {
// Basic Keycodes
    KC_NO = 0x0000,
    KC_TRANSPARENT = 0x0001,
    KC_A = 0x0004,
    KC_B = 0x0005,
    KC_C = 0x0006,
    KC_D = 0x0007,
    KC_E = 0x0008,
    KC_F = 0x0009,
    KC_G = 0x000A,
    KC_H = 0x000B,
    KC_I = 0x000C,
    KC_J = 0x000D,
    KC_K = 0x000E,
    KC_L = 0x000F,
    KC_M = 0x0010,
    KC_N = 0x0011,
    KC_O = 0x0012,
    KC_P = 0x0013,
    KC_Q = 0x0014,
    KC_R = 0x0015,
    KC_S = 0x0016,
    KC_T = 0x0017,
    KC_U = 0x0018,
    KC_V = 0x0019,
    KC_W = 0x001A,
    KC_X = 0x001B,
    KC_Y = 0x001C,
    KC_Z = 0x001D,
    KC_1 = 0x001E,
    KC_2 = 0x001F,
    KC_3 = 0x0020,
    KC_4 = 0x0021,
    KC_5 = 0x0022,
    KC_6 = 0x0023,
    KC_7 = 0x0024,
    KC_8 = 0x0025,
    KC_9 = 0x0026,
    KC_0 = 0x0027,
    KC_ENTER = 0x0028,
    KC_ESCAPE = 0x0029,
    KC_BACKSPACE = 0x002A,
    KC_TAB = 0x002B,
    KC_SPACE = 0x002C,
    KC_MINUS = 0x002D,
    KC_EQUAL = 0x002E,
    KC_LEFT_BRACKET = 0x002F,
    KC_RIGHT_BRACKET = 0x0030,
    KC_BACKSLASH = 0x0031,
    KC_NONUS_HASH = 0x0032,
    KC_SEMICOLON = 0x0033,
    KC_QUOTE = 0x0034,
    KC_GRAVE = 0x0035,
    KC_COMMA = 0x0036,
    KC_DOT = 0x0037,
    KC_SLASH = 0x0038,
    KC_CAPS_LOCK = 0x0039,
    KC_F1 = 0x003A,
    KC_F2 = 0x003B,
    KC_F3 = 0x003C,
    KC_F4 = 0x003D,
    KC_F5 = 0x003E,
    KC_F6 = 0x003F,
    KC_F7 = 0x0040,
    KC_F8 = 0x0041,
    KC_F9 = 0x0042,
    KC_F10 = 0x0043,
    KC_F11 = 0x0044,
    KC_F12 = 0x0045,
    KC_PRINT_SCREEN = 0x0046,
    KC_SCROLL_LOCK = 0x0047,
    KC_PAUSE = 0x0048,
    KC_INSERT = 0x0049,
    KC_HOME = 0x004A,
    KC_PAGE_UP = 0x004B,
    KC_DELETE = 0x004C,
    KC_END = 0x004D,
    KC_PAGE_DOWN = 0x004E,
    KC_RIGHT = 0x004F,
    KC_LEFT = 0x0050,
    KC_DOWN = 0x0051,
    KC_UP = 0x0052,

// Modifiers
    KC_LEFT_CTRL = 0x00E0,
    KC_LEFT_SHIFT = 0x00E1,
    KC_LEFT_ALT = 0x00E2,
    KC_LEFT_GUI = 0x00E3,
    KC_RIGHT_CTRL = 0x00E4,
    KC_RIGHT_SHIFT = 0x00E5,
    KC_RIGHT_ALT = 0x00E6,
    KC_RIGHT_GUI = 0x00E7,

// OpenGrader specific keycodes
    OP_ENCODER_CW = 0x7E00,
    OP_ENCODER_CCW = 0x7E01,
    OP_MATRIX_SCAN = 0x7E02,
    OP_DEBUG_TOGGLE = 0x7E03,
    OP_BOOTLOADER = 0x7E04,
    OP_RESET = 0x7E05,

// MIDI keycodes - Control Change (CC) messages
    OP_MIDI_CC_BASE = 0x7E10,
    OP_MIDI_CC_MAX = 0xFE0F,  // Max: 0x7E10 + (15<<11) + (127<<4) + 15 = 0x7E10 + 0x7800 + 0x7F0 + 0xF = 0xFE0F

// MIDI keycode helpers - for Control Change messages  
// New encoding: base + (channel-1)<<11 + controller<<4 + value_index
// Channel 1-16, Controller 0-127, Value from lookup table (16 values)
#define SEND_MIDI_CC(channel, controller, value) \
    ((uint16_t)(OP_MIDI_CC_BASE + (((channel-1) & 0x0F) << 11) + ((controller & 0x7F) << 4) + \
    ((value == 0) ? 0 : (value == 1) ? 1 : (value == 7) ? 2 : (value == 15) ? 3 : (value == 31) ? 4 : \
     (value == 43) ? 5 : (value == 45) ? 6 : (value == 63) ? 7 : (value == 64) ? 8 : (value == 79) ? 9 : \
     (value == 95) ? 10 : (value == 111) ? 11 : (value == 120) ? 12 : (value == 127) ? 13 : 14))) // Added 45->6, fixed indices

#define OP_MIDI_NOTE_FLAG 0x0F

#define SEND_MIDI_NOTE(channel, note) \
    ((uint16_t)(OP_MIDI_CC_BASE + (((channel - 1) & 0x0F) << 11) + ((note & 0x7F) << 4) + OP_MIDI_NOTE_FLAG))

#define SEND_MIDI(channel, controller, value) SEND_MIDI_CC(channel, controller, value)

// Common aliases
    XXXXXXX = KC_NO,
    _______ = KC_TRANSPARENT,
    KC_TRNS = KC_TRANSPARENT,
    KC_ENT = KC_ENTER,
    KC_ESC = KC_ESCAPE,
    KC_BSPC = KC_BACKSPACE,
    KC_SPC = KC_SPACE,
    KC_MINS = KC_MINUS,
    KC_EQL = KC_EQUAL,
    KC_LBRC = KC_LEFT_BRACKET,
    KC_RBRC = KC_RIGHT_BRACKET,
    KC_BSLS = KC_BACKSLASH,
    KC_SCLN = KC_SEMICOLON,
    KC_QUOT = KC_QUOTE,
    KC_GRV = KC_GRAVE,
    KC_COMM = KC_COMMA,
    KC_SLSH = KC_SLASH,
    KC_CAPS = KC_CAPS_LOCK,
    KC_PSCR = KC_PRINT_SCREEN,
    KC_SCRL = KC_SCROLL_LOCK,
    KC_PAUS = KC_PAUSE,
    KC_INS = KC_INSERT,
    KC_PGUP = KC_PAGE_UP,
    KC_DEL = KC_DELETE,
    KC_PGDN = KC_PAGE_DOWN,
    KC_RGHT = KC_RIGHT,
    KC_LCTL = KC_LEFT_CTRL,
    KC_LSFT = KC_LEFT_SHIFT,
    KC_LALT = KC_LEFT_ALT,
    KC_LGUI = KC_LEFT_GUI,
    KC_RCTL = KC_RIGHT_CTRL,
    KC_RSFT = KC_RIGHT_SHIFT,
    KC_RALT = KC_RIGHT_ALT,
    KC_RGUI = KC_RIGHT_GUI,
};

// Range and type checking helpers
#define IS_OP_BASIC(code) ((code) >= OP_BASIC && (code) <= OP_BASIC_MAX)
#define IS_OP_MODIFIER(code) ((code) >= KC_LEFT_CTRL && (code) <= KC_RIGHT_GUI)
#define IS_OP_KB(code) ((code) >= OP_KB && (code) <= OP_KB_MAX)
#define IS_OP_MIDI_EVENT(code) ((code) >= OP_MIDI_CC_BASE && (code) <= OP_MIDI_CC_MAX)
#define IS_OP_MIDI_NOTE(code) (IS_OP_MIDI_EVENT(code) && (((code) - OP_MIDI_CC_BASE) & 0x0F) == OP_MIDI_NOTE_FLAG)
#define IS_OP_MIDI_CC(code) (IS_OP_MIDI_EVENT(code) && (((code) - OP_MIDI_CC_BASE) & 0x0F) != OP_MIDI_NOTE_FLAG)

// MIDI helper functions
// Helper function to get value index from MIDI value
static inline uint8_t op_midi_get_value_index(uint8_t value) {
    // MIDI value lookup table (16 common values) - find index for value
    const uint8_t values[16] = {0, 1, 7, 15, 31, 43, 63, 64, 79, 95, 111, 120, 127, 64, 32, 96};
    for (uint8_t i = 0; i < 16; i++) {
        if (values[i] == value) return i;
    }
    return 12; // Default to index 12 (value 127) if not found
}

static inline uint8_t op_midi_get_channel(uint16_t midi_code) {
    if (!IS_OP_MIDI_CC(midi_code)) return 1; // Default to channel 1
    return (uint8_t)((((midi_code - OP_MIDI_CC_BASE) >> 11) & 0x0F) + 1); // Extract 4-bit channel, convert to 1-indexed
}

static inline uint8_t op_midi_get_controller(uint16_t midi_code) {
    if (!IS_OP_MIDI_CC(midi_code)) return 0;
    return (uint8_t)(((midi_code - OP_MIDI_CC_BASE) >> 4) & 0x7F);
}

static inline uint8_t op_midi_get_value(uint16_t midi_code) {
    if (!IS_OP_MIDI_CC(midi_code)) return 127;
    uint8_t index = (midi_code - OP_MIDI_CC_BASE) & 0x0F;
    // MIDI value lookup table matching the macro indices:
    // 0->0, 1->1, 7->2, 15->3, 31->4, 43->5, 45->6, 63->7, 64->8, 79->9, 95->10, 111->11, 120->12, 127->13, custom->14,15
    const uint8_t values[16] = {0, 1, 7, 15, 31, 43, 45, 63, 64, 79, 95, 111, 120, 127, 50, 100};
    return values[index];
}

static inline uint8_t op_midi_note_get_channel(uint16_t midi_code) {
    if (!IS_OP_MIDI_NOTE(midi_code)) return 1;
    return (uint8_t)((((midi_code - OP_MIDI_CC_BASE) >> 11) & 0x0F) + 1);
}

static inline uint8_t op_midi_note_get_note(uint16_t midi_code) {
    if (!IS_OP_MIDI_NOTE(midi_code)) return 60;
    return (uint8_t)(((midi_code - OP_MIDI_CC_BASE) >> 4) & 0x7F);
}

// Convert OP keycode to HID usage code (for USB HID reports)
static inline uint8_t op_keycode_to_hid(uint16_t keycode) {
    if (keycode >= KC_A && keycode <= KC_RIGHT_GUI) {
        return (uint8_t)keycode;
    }
    return 0; // No key
}