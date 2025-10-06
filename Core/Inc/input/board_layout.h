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

#endif /* BOARD_LAYOUT_H */
