# Slider Keyboard

An ADC-based MIDI controller keyboard with potentiometer/fader support.

## Hardware Configuration

- **Matrix**: 1×1 (minimal, for testing)
- **Encoders**: 0
- **Sliders**: 1 (PA3 / ADC1 Channel 4)

## Slider Configuration

The slider uses the STM32's ADC to read analog potentiometer values and convert them to MIDI Control Change (CC) messages.

### Current Slider Settings

| Parameter | Value | Description |
|-----------|-------|-------------|
| GPIO Pin | PA3 | Physical pin for analog input |
| ADC Instance | ADC1 | ADC peripheral |
| ADC Channel | Channel 4 | ADC input channel |
| MIDI CC Number | 1 | Modulation Wheel |
| MIDI Channel | 0 | Channel 1 (0-indexed) |
| Value at Zero | 70 | ADC value when slider is at minimum |
| Value at Full | 4095 | ADC value when slider is at maximum |
| MIDI Range | 0-127 | Full MIDI CC range |

### Smoothing Parameters

The slider includes sophisticated signal processing to eliminate jitter and provide smooth, responsive control:

- **EMA Alpha** (`0.2`): Exponential Moving Average smoothing coefficient. Lower values = more smoothing.
- **Hysteresis Margin** (`0.3`): Dead zone around integer percentage thresholds to prevent flickering.
- **Fast Change Threshold** (`1.0`): If the slider moves more than 1% in one sample, snap immediately for responsive feel.
- **Sample Interval** (`10ms`): Slider is sampled at 100Hz for smooth updates.

### Calibration

To calibrate a slider for your specific potentiometer:

1. Flash the slider keyboard firmware
2. Move the slider to its minimum position
3. Read the ADC value (can be printed via USB CDC)
4. Set `value_at_zero` in `config.h`
5. Move the slider to its maximum position
6. Read the ADC value
7. Set `value_at_full` in `config.h`
8. Rebuild and reflash

### Customizing MIDI Output

You can customize the MIDI output by editing `slider_configs` in `config.h`:

```c
static const slider_config_t slider_configs[SLIDER_COUNT] = {
    {
        .adc = ADC1,                  // ADC instance
        .channel = ADC_CHANNEL_4,     // ADC channel
        .gpio_port = GPIOA,           // GPIO port
        .gpio_pin = GPIO_PIN_3,       // GPIO pin
        .midi_cc = 1,                 // MIDI CC number (1 = Modulation)
        .midi_channel = 0,            // MIDI channel (0 = Channel 1)
        .value_at_zero = 70,          // Calibration: min ADC value
        .value_at_full = 4095,        // Calibration: max ADC value
        .min_midi_value = 0,          // Output range: minimum
        .max_midi_value = 127         // Output range: maximum
    }
};
```

**Example**: To create a slider that only outputs MIDI values 5-40 on CC#74 (Brightness):
```c
.midi_cc = 74,
.min_midi_value = 5,
.max_midi_value = 40
```

## Building

```bash
cmake --preset Debug -DKEYBOARD=slider
cmake --build --preset Debug
```

Or use VS Code tasks:
- **Configure: Slider Keyboard**
- **Build: Slider Keyboard**
- **Build + Flash: Slider Keyboard (USB)**

## Technical Details

### Signal Processing Flow

1. **ADC Reading**: 12-bit ADC value (0-4095) read at 100Hz
2. **Calibration**: Map raw ADC to 0-100% using `value_at_zero` and `value_at_full`
3. **Fast-Change Detection**: If change > 1%, snap immediately to new value
4. **EMA Smoothing**: Otherwise, apply exponential moving average
5. **Hysteresis**: Only update integer percentage when crossing threshold + margin
6. **MIDI Mapping**: Map percentage to custom MIDI range (e.g., 0-100% → 0-127)
7. **Transmission**: Send MIDI CC only when value changes

### Memory Usage

The slider keyboard uses approximately:
- **FLASH**: 92,164 bytes (17.58% of 512 KB)
- **RAM**: 5,160 bytes (3.94% of 128 KB)

### Adding More Sliders

1. Increase `SLIDER_COUNT` in `config.h`
2. Add additional entries to `slider_configs` array
3. Ensure GPIO pins are configured as analog input
4. Update ADC clock enable if using multiple ADC instances

## Future Improvements

- [ ] Automatic calibration mode (store min/max in EEPROM)
- [ ] Per-slider sensitivity adjustment
- [ ] Logarithmic/exponential curve options
- [ ] MIDI Program Change support
- [ ] Multiple sliders with independent configurations
