// Strict UTF-8 decoding for text that arrives over the mesh.
#pragma once
#include <stdint.h>

namespace ui {

constexpr uint32_t REPLACEMENT = 0xFFFD;

// Decode the code point at `s`, reading nothing at or past `end` (pass nullptr
// for a NUL-terminated string; NUL is never part of a multibyte sequence, so
// it stops a truncated one). Returns the bytes consumed, 0 only when s == end.
//
// Accepts U+0000..U+10FFFF except surrogates and rejects overlong forms. Any
// malformed input yields U+FFFD for the longest valid prefix of the bad
// sequence (at least one byte), so decoding resumes at the next byte that can
// start a character and valid text after the damage is kept.
int utf8Decode(const char* s, const char* end, uint32_t* cp);

// Decode one code point from a NUL-terminated string and advance *s. At the
// terminator it returns 0 and advances past it, so callers test *p first.
uint32_t utf8Next(const char** s);

}  // namespace ui
