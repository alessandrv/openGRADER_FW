#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

#include "stm32g4xx_hal.h"
#include "input/matrix.h"
#include "input/encoder.h"

// Include the keyboard-specific configuration
// This should be set via compile-time define: -DKEYBOARD_CONFIG_HEADER=<standard/config.h>
#ifdef KEYBOARD_CONFIG_HEADER
    #include KEYBOARD_CONFIG_HEADER
#else
    // Default to standard keyboard if not specified
    #include <standard/config.h>
#endif

#endif /* PIN_CONFIG_H */