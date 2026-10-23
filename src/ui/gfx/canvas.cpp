#include "ui/gfx/canvas.h"
#include <string.h>

namespace ui {

static uint16_t s_line[Canvas::W * Canvas::BAND];

Canvas::Canvas() {
  memset(_fb, 0, sizeof(_fb));
  memset(_rowHash, 0, sizeof(_rowHash));
  noClip();
}

void Canvas::clip(int x, int y, int w, int h) {
  _cx0 = x < 0 ? 0 : x;
  _cy0 = y < 0 ? 0 : y;
  _cx1 = x + w > W ? W : x + w;
  _cy1 = y + h > H ? H : y + h;
}

void Canvas::clear(uint8_t c) {
  memset(_fb, (c & 15) * 0x11, sizeof(_fb));
}

void Canvas::pixel(int x, int y, uint8_t c) {
  if (x < _cx0 || y < _cy0 || x >= _cx1 || y >= _cy1) return;
  uint8_t* p = &_fb[y * (W / 2) + (x >> 1)];
  *p = (x & 1) ? ((*p & 0xF0) | (c & 15)) : ((*p & 0x0F) | (c << 4));
}

uint8_t Canvas::get(int x, int y) const {
  if (x < 0 || y < 0 || x >= W || y >= H) return 0;
  uint8_t b = _fb[y * (W / 2) + (x >> 1)];
  return (x & 1) ? (b & 15) : (b >> 4);
}

void Canvas::fillRect(int x, int y, int w, int h, uint8_t c) {
  int x0 = x < _cx0 ? _cx0 : x, y0 = y < _cy0 ? _cy0 : y;
  int x1 = x + w > _cx1 ? _cx1 : x + w, y1 = y + h > _cy1 ? _cy1 : y + h;
  if (x0 >= x1 || y0 >= y1) return;
  c &= 15;
  for (int yy = y0; yy < y1; yy++) {
    int xx = x0;
    if (xx & 1) pixel(xx++, yy, c);
    int pairs = (x1 - xx) >> 1;
    if (pairs > 0) {
      memset(&_fb[yy * (W / 2) + (xx >> 1)], c * 0x11, pairs);
      xx += pairs * 2;
    }
    if (xx < x1) pixel(xx, yy, c);
  }
}

void Canvas::rect(int x, int y, int w, int h, uint8_t c) {
  hline(x, y, w, c);
  hline(x, y + h - 1, w, c);
  vline(x, y, h, c);
  vline(x + w - 1, y, h, c);
}

void Canvas::pill(int x, int y, int w, int h, uint8_t c) {
  fillRect(x + 1, y, w - 2, h, c);
  vline(x, y + 1, h - 2, c);
  vline(x + w - 1, y + 1, h - 2, c);
}

void Canvas::bitmap(int x, int y, const uint8_t* rows, int w, int h, uint8_t c, int scale) {
  for (int r = 0; r < h; r++)
    for (int i = 0; i < w; i++)
      if (rows[r] & (0x80 >> i)) {
        if (scale == 1) pixel(x + i, y + r, c);
        else fillRect(x + i * scale, y + r * scale, scale, scale, c);
      }
}

void Canvas::bitmapWide(int x, int y, const uint8_t* rows, int w, int h, uint8_t c) {
  const int stride = (w + 7) / 8;
  for (int r = 0; r < h; r++)
    for (int i = 0; i < w; i++)
      if (rows[r * stride + i / 8] & (0x80 >> (i & 7))) pixel(x + i, y + r, c);
}

// Binary search, as esp-vim's glyph_in(); unknown code points fall back to '?'.
static const uint8_t* glyph(const mc_font_t* f, uint32_t cp) {
  int lo = 0, hi = f->count - 1;
  int stride = f->height * f->bytes_per_row;
  while (lo <= hi) {
    int mid = (lo + hi) >> 1;
    uint32_t v = f->codepoints[mid];
    if (v == cp) return f->bitmaps + mid * stride;
    if (v < cp) lo = mid + 1; else hi = mid - 1;
  }
  return cp == '?' ? nullptr : glyph(f, '?');
}

static void drawGlyph(Canvas& c, int x, int y, const mc_font_t* f, uint32_t cp, uint8_t fg) {
  const uint8_t* g = glyph(f, cp);
  if (!g) return;
  for (int r = 0; r < f->height; r++) {
    uint16_t bits = f->bytes_per_row == 1 ? (uint16_t)(g[r] << 8) : (uint16_t)((g[2 * r] << 8) | g[2 * r + 1]);
    for (int i = 0; bits; i++, bits <<= 1)
      if (bits & 0x8000) c.pixel(x + i, y + r, fg);
  }
}

int Canvas::textWidth(const char* s, const mc_font_t* f) {
  int n = 0;
  while (*s) { utf8Next(&s); n++; }
  return n * f->width;
}

int Canvas::fitBytes(const char* s, const mc_font_t* f, int maxw) {
  const char* p = s;
  int w = 0;
  while (*p) {
    const char* q = p;
    utf8Next(&q);
    if (w + f->width > maxw) break;
    w += f->width;
    p = q;
  }
  return (int)(p - s);
}

int Canvas::text(int x, int y, const char* s, const mc_font_t* f, uint8_t fg, int bg, int maxw) {
  int limit = 1 << 30;
  bool ell = false;
  if (maxw > 0 && textWidth(s, f) > maxw) {
    limit = maxw - f->width;               // room for the ellipsis
    ell = true;
  }
  if (bg >= 0) {
    int w = ell ? maxw : textWidth(s, f);
    fillRect(x, y, w, f->height, (uint8_t)bg);
  }
  int cx = x;
  while (*s) {
    uint32_t cp = utf8Next(&s);
    if (cx + f->width - x > limit) break;
    drawGlyph(*this, cx, y, f, cp, fg);
    cx += f->width;
  }
  if (ell) {
    // Three dots on the baseline (Spleen's small fonts have no U+2026).
    int by = y + f->height - 3;
    for (int i = 0; i < 3; i++) pixel(cx + i * 2 + (f->width >= 8 ? 1 : 0), by, fg);
    cx += f->width;
  }
  return cx;
}

void Canvas::textScrolled(int x, int y, const char* s, const mc_font_t* f, uint8_t fg, int maxw, int offset) {
  int c0 = _cx0, c1 = _cx1, r0 = _cy0, r1 = _cy1;
  // Intersect with the current clip, draw, restore.
  _cx0 = x > c0 ? x : c0;
  _cx1 = x + maxw < c1 ? x + maxw : c1;
  int cx = x - offset;
  while (*s) {
    uint32_t cp = utf8Next(&s);
    if (cx + f->width > _cx0 && cx < _cx1) drawGlyph(*this, cx, y, f, cp, fg);
    cx += f->width;
    if (cx >= _cx1) break;
  }
  _cx0 = c0; _cx1 = c1; _cy0 = r0; _cy1 = r1;
}

void Canvas::flush(hal::Display& d, const uint16_t palette[16], bool force) {
  uint16_t pal[16];
  for (int i = 0; i < 16; i++)
    pal[i] = d.bigEndian() ? (uint16_t)((palette[i] >> 8) | (palette[i] << 8)) : palette[i];

  int bandStart = -1, n = 0;
  for (int y = 0; y <= H; y++) {
    bool dirty = false;
    if (y < H) {
      // FNV-1a over the packed row.
      uint32_t h = 2166136261u;
      const uint8_t* row = &_fb[y * (W / 2)];
      for (int i = 0; i < W / 2; i++) h = (h ^ row[i]) * 16777619u;
      dirty = force || h != _rowHash[y];
      _rowHash[y] = h;
    }
    if (dirty) {
      if (bandStart < 0) bandStart = y;
      const uint8_t* row = &_fb[y * (W / 2)];
      uint16_t* out = &s_line[n * W];
      for (int i = 0; i < W / 2; i++) {
        out[2 * i] = pal[row[i] >> 4];
        out[2 * i + 1] = pal[row[i] & 15];
      }
      n++;
    }
    if (bandStart >= 0 && (!dirty || n == BAND || y == H - 1)) {
      d.pushRows(bandStart, n, s_line);
      bandStart = -1;
      n = 0;
    }
  }
}

}  // namespace ui
