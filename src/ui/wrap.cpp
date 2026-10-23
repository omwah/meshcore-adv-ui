// Word wrapping on glyph advances.
#include "ui/app.h"

namespace ui {

int wrapText(const char* s, const mc_font_t* f, int w, LineFn fn, void* ctx) {
  int line = 0;
  while (*s) {
    // Find the longest prefix that fits in w pixels (at least one element, so
    // a narrow column still advances), preferring a space break. Elements are
    // glyphs and whole emoji sequences, never a piece of either.
    const char* p = s;
    const char* lastSpace = nullptr;
    int x = 0;
    while (*p && *p != '\n') {
      int adv;
      int b = Canvas::elem(p, f, &adv);
      if (x + adv > w && p > s) break;
      if (*p == ' ') lastSpace = p;
      x += adv;
      p += b;
    }
    const char* end = p;
    const char* next = p;
    if (*p == '\n' || *p == ' ') {
      next = p + 1;               // break exactly at a newline or space
    } else if (*p && lastSpace && lastSpace > s) {
      end = lastSpace;
      next = lastSpace + 1;
    }
    if (fn) fn(ctx, s, (int)(end - s), line);
    line++;
    s = next;
  }
  return line ? line : 1;
}

}  // namespace ui
