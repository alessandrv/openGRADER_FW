#include "input/board_layout.h"
#include "pin_config.h"

const board_layout_info_t board_layout_info = {
    .version = BOARD_LAYOUT_VERSION,
    .matrix_rows = MATRIX_ROWS,
    .matrix_cols = MATRIX_COLS,
    .encoder_count = ENCODER_COUNT,
    .first_encoder_column = BOARD_LAYOUT_FIRST_ENCODER_COLUMN,
    .encoders_per_row = (MATRIX_COLS > BOARD_LAYOUT_FIRST_ENCODER_COLUMN) ? (MATRIX_COLS - BOARD_LAYOUT_FIRST_ENCODER_COLUMN) : 0,
    .bitmap_length = BOARD_LAYOUT_BITMAP_BYTES,
    .reserved = 0,
    .encoder_bitmap = {
        0x7C, // Indices 0-7: row 0 columns 2-6
        0x3E, // Indices 8-15: row 1 columns 2-6
        0x9F, // Indices 16-23: row 2 columns 2-6 + row 3 column 2
        0xCF, // Indices 24-31: row 3 columns 3-6 + row 4 columns 2-3
        0x07  // Indices 32-39: row 4 columns 4-6
    }
};
