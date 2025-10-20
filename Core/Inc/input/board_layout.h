#ifndef BOARD_LAYOUT_H
#define BOARD_LAYOUT_H

#include <stdint.h>
#include "input/matrix.h"

#define BOARD_LAYOUT_VERSION 1
#define BOARD_LAYOUT_FIRST_ENCODER_COLUMN 2

#define BOARD_LAYOUT_CELL_COUNT (MATRIX_ROWS * MATRIX_COLS)
#define BOARD_LAYOUT_BITMAP_BYTES ((BOARD_LAYOUT_CELL_COUNT + 7u) / 8u)

typedef struct {
    uint8_t version;
    uint8_t matrix_rows;
    uint8_t matrix_cols;
    uint8_t encoder_count;
    uint8_t first_encoder_column;
    uint8_t encoders_per_row;
    uint8_t bitmap_length;
    uint8_t reserved;
    uint8_t encoder_bitmap[BOARD_LAYOUT_BITMAP_BYTES];
} __attribute__((packed)) board_layout_info_t;

extern const board_layout_info_t board_layout_info;

// Function to get dynamically generated board layout info
const board_layout_info_t* get_board_layout_info(void);

// Function to get layout cell info for configurator
#include "input/board_layout_types.h"
layout_cell_type_t get_layout_cell_type(uint8_t row, uint8_t col);
uint8_t get_layout_cell_component_id(uint8_t row, uint8_t col);

#endif /* BOARD_LAYOUT_H */
