#ifndef BOARD_LAYOUT_TYPES_H
#define BOARD_LAYOUT_TYPES_H

#include <stdint.h>

// Layout cell types
typedef enum {
    LAYOUT_EMPTY = 0,    // No component
    LAYOUT_SWITCH = 1,   // Regular switch/key
    LAYOUT_ENCODER = 2,  // Rotary encoder
    LAYOUT_SLIDER = 3,   // Analog slider (spans multiple rows)
    LAYOUT_POTENTIOMETER = 4,  // Analog potentiometer (circular control)
    LAYOUT_MAGNETIC_SWITCH = 5  // Magnetic switch with analog sensing
} layout_cell_type_t;

// Layout cell definition
typedef struct {
    layout_cell_type_t type;
    uint8_t component_id;  // Index into switches/encoders/sliders array
} layout_cell_t;

// Convenient macros for layout definition
#define EMPTY       LAYOUT_EMPTY
#define SW          LAYOUT_SWITCH
#define ENC         LAYOUT_ENCODER
#define SLIDER      LAYOUT_SLIDER
#define POT         LAYOUT_POTENTIOMETER
#define MAG_SW      LAYOUT_MAGNETIC_SWITCH

// Helper macro to create layout cell with auto-incrementing IDs
#define LAYOUT_CELL(type, id) {type, id}

#endif /* BOARD_LAYOUT_TYPES_H */