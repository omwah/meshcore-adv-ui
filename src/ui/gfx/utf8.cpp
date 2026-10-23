#include "ui/gfx/utf8.h"

namespace ui {

int utf8Decode(const char* sp, const char* end, uint32_t* cp) {
  const uint8_t* s = (const uint8_t*)sp;
  const uint8_t* e = (const uint8_t*)end;
  if (e && s >= e) return 0;

  uint32_t c = s[0];
  if (c < 0x80) { *cp = c; return 1; }

  // Lead byte: continuation count, payload bits, and the range the first
  // continuation byte must fall in (this is what excludes overlong forms,
  // surrogates and anything above U+10FFFF).
  int extra;
  uint8_t lo = 0x80, hi = 0xBF;
  if (c >= 0xC2 && c <= 0xDF) { extra = 1; c &= 0x1F; }
  else if (c >= 0xE0 && c <= 0xEF) {
    extra = 2; c &= 0x0F;
    if (s[0] == 0xE0) lo = 0xA0;        // not overlong
    else if (s[0] == 0xED) hi = 0x9F;   // not a surrogate
  } else if (c >= 0xF0 && c <= 0xF4) {
    extra = 3; c &= 0x07;
    if (s[0] == 0xF0) lo = 0x90;        // not overlong
    else if (s[0] == 0xF4) hi = 0x8F;   // not above U+10FFFF
  } else {                              // stray continuation, C0, C1, F5..FF
    *cp = REPLACEMENT;
    return 1;
  }

  int n = 1;
  for (int i = 0; i < extra; i++) {
    if (e && s + n >= e) break;         // truncated
    uint8_t b = s[n];
    if (b < lo || b > hi) break;        // also stops at NUL
    c = (c << 6) | (b & 0x3F);
    lo = 0x80; hi = 0xBF;
    n++;
  }
  *cp = n == extra + 1 ? c : REPLACEMENT;
  return n;
}

uint32_t utf8Next(const char** sp) {
  uint32_t cp;
  *sp += utf8Decode(*sp, nullptr, &cp);
  return cp;
}

}  // namespace ui
