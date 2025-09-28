#ifndef MIDI_HANDLER_H
#define MIDI_HANDLER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Process a keycode that maps to a MIDI control change definition.
 *
 * The keycode layout is defined by the op_keycodes helper; when the keycode
 * encodes a MIDI CC event, this function routes it either over USB (CDC/MIDI)
 * or via I2C to the master, depending on the current device mode.
 *
 * @param keycode Encoded keycode value (see op_keycodes.h helpers).
 * @param pressed true when the key is pressed, false on release.
 */
void midi_handle_keycode(uint16_t keycode, bool pressed);

/**
 * @brief Send a MIDI control change event over USB.
 *
 * This helper is used by the I2C manager when the master receives an event
 * from a slave device and needs to forward it to the USB host.
 *
 * @param channel    Zero-based MIDI channel number (0-15).
 * @param controller MIDI controller number.
 * @param value      Controller value (0-127).
 */
void midi_send_cc(uint8_t channel, uint8_t controller, uint8_t value);

/**
 * @brief Send a MIDI Note On event over USB.
 *
 * @param channel  Zero-based MIDI channel (0-15).
 * @param note     MIDI note number (0-127).
 * @param velocity Note velocity (0-127).
 */
void midi_send_note_on(uint8_t channel, uint8_t note, uint8_t velocity);

/**
 * @brief Send a MIDI Note Off event over USB.
 *
 * @param channel Zero-based MIDI channel (0-15).
 * @param note    MIDI note number (0-127).
 */
void midi_send_note_off(uint8_t channel, uint8_t note);

#ifdef __cplusplus
}
#endif

#endif /* MIDI_HANDLER_H */
