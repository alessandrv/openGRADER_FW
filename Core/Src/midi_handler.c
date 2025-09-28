#include "midi_handler.h"
#include "op_keycodes.h"
#include "usb_app.h"
#include "i2c_manager.h"

void midi_handle_keycode(uint16_t keycode, bool pressed)
{
    if (!IS_OP_MIDI_EVENT(keycode))
    {
        return; // Not a MIDI event
    }

    if (IS_OP_MIDI_NOTE(keycode))
    {
        uint8_t channel = op_midi_note_get_channel(keycode);
        uint8_t note = op_midi_note_get_note(keycode);
        uint8_t midi_channel = (uint8_t)((channel > 0U ? channel : 1U) - 1U);
        uint8_t velocity = pressed ? 0x7FU : 0x00U;

        usb_app_cdc_printf("MIDI NOTE %s: ch=%d note=%d velocity=%d\r\n",
                 pressed ? "ON" : "OFF", midi_channel + 1U, note, velocity);

        if (i2c_manager_get_mode() == 1)
        {
            if (pressed)
            {
                midi_send_note_on(midi_channel, note, velocity);
            }
            else
            {
                midi_send_note_off(midi_channel, note);
            }
        }
        else
        {
            i2c_manager_send_midi_note(midi_channel, note, velocity, pressed);
        }
        return;
    }

    if (!IS_OP_MIDI_CC(keycode))
    {
        return;
    }

    uint8_t channel = op_midi_get_channel(keycode);
    uint8_t controller = op_midi_get_controller(keycode);
    uint8_t value = op_midi_get_value(keycode);
    uint8_t midi_channel = (uint8_t)((channel > 0U ? channel : 1U) - 1U);

    usb_app_cdc_printf("MIDI CC: ch=%d controller=%d value=%d\r\n",
             midi_channel + 1U, controller, value);

    if (pressed)
    {
        if (i2c_manager_get_mode() == 1)
        {
            midi_send_cc(midi_channel, controller, value);
        }
        else
        {
            i2c_manager_send_midi_cc(midi_channel, controller, value);
        }
    }
}

void midi_send_cc(uint8_t channel, uint8_t controller, uint8_t value)
{
    usb_app_midi_send_cc(channel, controller, value);
}

void midi_send_note_on(uint8_t channel, uint8_t note, uint8_t velocity)
{
    usb_app_midi_send_note_on(channel, note, velocity);
}

void midi_send_note_off(uint8_t channel, uint8_t note)
{
    usb_app_midi_send_note_off(channel, note);
}