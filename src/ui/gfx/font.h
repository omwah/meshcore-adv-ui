// Fixed-cell bitmap font, as scripts/bdf2c.py and scripts/gen_unicode.py emit
// it. Glyphs are `height` rows of `bytes_per_row` bytes, MSB leftmost, 1bpp.
//
// Code points are listed as runs of consecutive values: run r covers
// [first, first + count) and its glyphs start at glyph number `index`, so the
// glyph for cp is number runs[r].index + (cp - runs[r].first). Runs are sorted
// by `first`. Sparse sets (Greek, a few symbols) cost a few bytes, not four
// per glyph.
//
// A font may name a fallback layer in `next`: another font with its own cell
// size (a double-width layer has width = 2 * the primary's), tried in order
// when a code point is not in this one. The advance for a glyph is the width
// of the layer it came from.
#pragma once
#include <stdint.h>

typedef struct {
    uint32_t first;
    uint16_t count;
    uint16_t index;
} mc_run_t;

typedef struct mc_font {
    const char *name;
    uint8_t width, height, bytes_per_row, max_cols;
    uint16_t nruns;
    const mc_run_t *runs;
    const uint8_t *bitmaps;
    const struct mc_font *next;
} mc_font_t;
