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

// Bytes in one glyph's bitmap.
inline int emojiGlyphBytes(const mc_emoji_t* e) { return (e->w * e->h + 1) / 2; }

// A pixel (0 transparent, 1..15) of the glyph at (x, y) of its w x h bitmap.
inline uint8_t emojiPixel(const uint8_t* glyph, const mc_emoji_t* e, int x, int y) {
  int i = y * e->w + x;
  return (i & 1) ? (glyph[i >> 1] & 15) : (glyph[i >> 1] >> 4);
}

}  // namespace ui
