#include "input/board_layout.h"
#include "input/board_layout_types.h"
#include "pin_config.h"

// Include the keyboard-specific layout
#ifdef KEYBOARD_CONFIG_HEADER
    #include KEYBOARD_CONFIG_HEADER
#endif

// Define the keyboard layout array
#include "board_layout.h"
#include "matrix.h"
#include "board_layout_types.h"

// Static keyboard layout - initialized dynamically
static layout_cell_t keyboard_layout[MATRIX_ROWS][MATRIX_COLS];
static bool layout_initialized = false;

// Forward declaration
static void init_keyboard_layout(void);

// Function to generate encoder bitmap dynamically
static uint8_t generate_encoder_bitmap_byte(uint8_t start_bit) {
    uint8_t byte_value = 0;
    
    for (uint8_t bit = 0; bit < 8; bit++) {
        uint8_t cell_index = start_bit + bit;
        if (cell_index >= (MATRIX_ROWS * MATRIX_COLS)) {
            break;
        }
        
        uint8_t row = cell_index / MATRIX_COLS;
        uint8_t col = cell_index % MATRIX_COLS;
        
        if (keyboard_layout[row][col].type == LAYOUT_ENCODER) {
            byte_value |= (1 << bit);
        }
    }
    
    return byte_value;
}

// Generate encoder bitmap array
static uint8_t encoder_bitmap[BOARD_LAYOUT_BITMAP_BYTES];
static void init_encoder_bitmap(void) {
    init_keyboard_layout();  // Ensure layout is initialized first
    for (uint8_t byte_idx = 0; byte_idx < BOARD_LAYOUT_BITMAP_BYTES; byte_idx++) {
        encoder_bitmap[byte_idx] = generate_encoder_bitmap_byte(byte_idx * 8);
    }
}

const board_layout_info_t board_layout_info = {
    .version = BOARD_LAYOUT_VERSION,
    .matrix_rows = MATRIX_ROWS,
    .matrix_cols = MATRIX_COLS,
    .encoder_count = ENCODER_COUNT,
    .first_encoder_column = 0,  // Will be calculated dynamically if needed
    .encoders_per_row = 0,      // Will be calculated dynamically if needed
    .bitmap_length = BOARD_LAYOUT_BITMAP_BYTES,
    .reserved = 0,
    .encoder_bitmap = {0}       // Will be filled by init function
};

// Function to get the dynamic board layout info
const board_layout_info_t* get_board_layout_info(void) {
    static bool initialized = false;
    static board_layout_info_t dynamic_info;
    
    if (!initialized) {
        // Copy base info
        dynamic_info = board_layout_info;
        
        // Initialize encoder bitmap
        init_encoder_bitmap();
        
        // Copy the generated bitmap
        for (uint8_t i = 0; i < BOARD_LAYOUT_BITMAP_BYTES && i < sizeof(encoder_bitmap); i++) {
            dynamic_info.encoder_bitmap[i] = encoder_bitmap[i];
        }
        
        initialized = true;
    }
    
    return &dynamic_info;
}

// Function to get layout cell type for configurator
// Initialize layout data dynamically
static void init_keyboard_layout(void) {
    if (layout_initialized) return;
    
    // Initialize layout based on keyboard configuration
    // This approach avoids complex macro expansion issues
    const layout_cell_t layout_data[MATRIX_ROWS][MATRIX_COLS] = KEYBOARD_LAYOUT;
    
    for (int row = 0; row < MATRIX_ROWS; row++) {
        for (int col = 0; col < MATRIX_COLS; col++) {
            keyboard_layout[row][col] = layout_data[row][col];
        }
    }
    
    layout_initialized = true;
}

layout_cell_type_t get_layout_cell_type(uint8_t row, uint8_t col) {
    init_keyboard_layout();
    if (row >= MATRIX_ROWS || col >= MATRIX_COLS) {
        return LAYOUT_EMPTY;
    }
    return keyboard_layout[row][col].type;
}

// Function to get layout cell component ID for configurator  
uint8_t get_layout_cell_component_id(uint8_t row, uint8_t col) {
    init_keyboard_layout();
    if (row >= MATRIX_ROWS || col >= MATRIX_COLS) {
        return 0;
    }
    return keyboard_layout[row][col].component_id;
}
