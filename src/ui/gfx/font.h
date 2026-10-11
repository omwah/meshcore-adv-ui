// Fixed-cell bitmap font, as scripts/bdf2c.py emits it: one glyph per code
// point, sorted, `height` rows of `bytes_per_row` bytes each, MSB leftmost.
#pragma once
#include <stdint.h>

typedef struct {
    const char *name;
    uint8_t width, height, bytes_per_row, max_cols;
    uint16_t count;
    const uint32_t *codepoints;
    const uint8_t *bitmaps;
} mc_font_t;
