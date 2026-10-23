// Emoji sequence matching over a font's emoji table (see font.h).
#pragma once
#include "ui/gfx/font.h"

namespace ui {

// The longest sequence in `e`'s table that starts at the NUL-terminated UTF-8
// string `s`, with U+FE0F/U+FE0E ignored wherever they appear (a variation
// selector after any element belongs to it). Returns the bytes it spans,
// including trailing selectors, or 0 when nothing matches; *glyph is the
// index of its bitmap.
int emojiMatch(const mc_emoji_t* e, const char* s, int* glyph);

// Decode the first `rows` rows of a glyph into `out`, one byte per pixel
// (0 transparent, 1..15 palette), w pixels a row. `out` holds rows * e->w.
void emojiDecode(const mc_emoji_t* e, int glyph, int rows, uint8_t* out);

}  // namespace ui
