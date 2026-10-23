// 4-bit palettized framebuffer (two pixels per byte, high nibble left).
//
// The whole UI draws into this every frame; flush() compares each row with
// the previous frame by hash and pushes only rows that changed, expanding
// them through the active palette into a small line buffer. 240x135 costs
// 16.2 KB here plus 3.8 KB of line buffer, instead of 64.8 KB for RGB565.
// The board sets the size with MC_SCREEN_W/H (the Cardputer's by default).
#pragma once
#include <stdint.h>
#include "ui/gfx/font.h"
#include "hal/display.h"

#ifndef MC_SCREEN_W
#define MC_SCREEN_W 240
#endif
#ifndef MC_SCREEN_H
#define MC_SCREEN_H 135
#endif

namespace ui {

class Canvas {
public:
  static constexpr int W = MC_SCREEN_W;
  static constexpr int H = MC_SCREEN_H;
  static constexpr int BAND = 8;              // rows per pushRows call

  Canvas();

  void clear(uint8_t c);
  void pixel(int x, int y, uint8_t c);
  uint8_t get(int x, int y) const;
  void fillRect(int x, int y, int w, int h, uint8_t c);
  void hline(int x, int y, int w, uint8_t c) { fillRect(x, y, w, 1, c); }
  void vline(int x, int y, int h, uint8_t c) { fillRect(x, y, 1, h, c); }
  void rect(int x, int y, int w, int h, uint8_t c);
  // Rectangle with its four corner pixels cut, for pills and badges.
  void pill(int x, int y, int w, int h, uint8_t c);
  // 1bpp bitmap, `w` <= 8 per row byte, MSB left; transparent background.
  void bitmap(int x, int y, const uint8_t* rows, int w, int h, uint8_t c);

  // UTF-8 text. Returns the x just past the last glyph drawn. bg < 0 is
  // transparent. maxw > 0 clips with an ellipsis when the text is wider.
  int text(int x, int y, const char* s, const mc_font_t* f, uint8_t fg, int bg = -1, int maxw = 0);
  static int textWidth(const char* s, const mc_font_t* f);
  // Bytes of `s` that fit in `maxw` pixels without breaking a UTF-8 sequence.
  static int fitBytes(const char* s, const mc_font_t* f, int maxw);

  // Text shifted left by `offset` pixels and cut to [x, x+maxw): one frame of
  // a marquee.
  void textScrolled(int x, int y, const char* s, const mc_font_t* f, uint8_t fg, int maxw, int offset);

  // Limit drawing to a rectangle (for scrolling regions).
  void clip(int x, int y, int w, int h);
  void noClip() { clip(0, 0, W, H); }

  // Push changed rows (or every row when `force`) through `palette`.
  void flush(hal::Display& d, const uint16_t palette[16], bool force);

  const uint8_t* raw() const { return _fb; }

private:
  uint8_t _fb[W / 2 * H];
  uint32_t _rowHash[H];
  int _cx0, _cy0, _cx1, _cy1;                 // clip, exclusive max
};

// Decode one UTF-8 code point; advances *s. Invalid bytes become U+FFFD.
uint32_t utf8Next(const char** s);

}  // namespace ui
