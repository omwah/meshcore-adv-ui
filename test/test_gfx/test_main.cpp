// Canvas, dirty-row flushing and text layout.
#include <unity.h>
#include <string.h>
#include "ui/app.h"
#include "gen/fonts.h"

using namespace ui;

class CountingDisplay : public hal::Display {
public:
  int rows = 0, calls = 0;
  uint16_t first = 0;
  bool begin() override { return true; }
  int width() const override { return Canvas::W; }
  int height() const override { return Canvas::H; }
  bool bigEndian() const override { return true; }
  void pushRows(int, int h, const uint16_t* px) override { rows += h; calls++; first = px[0]; }
  void setBacklight(uint8_t) override {}
};

static Canvas s_c;
static const uint16_t kPal[16] = {0x0000, 0x1234};

void test_flush_pushes_only_changed_rows() {
  CountingDisplay d;
  s_c.clear(0);
  s_c.flush(d, kPal, true);
  TEST_ASSERT_EQUAL(Canvas::H, d.rows);
  d.rows = d.calls = 0;
  s_c.flush(d, kPal, false);
  TEST_ASSERT_EQUAL(0, d.rows);
  s_c.fillRect(10, 20, 5, 3, 1);
  s_c.flush(d, kPal, false);
  TEST_ASSERT_EQUAL(3, d.rows);
  TEST_ASSERT_EQUAL(1, d.calls);
}

void test_flush_byte_swaps_for_big_endian_panels() {
  CountingDisplay d;
  s_c.clear(1);
  s_c.flush(d, kPal, true);
  TEST_ASSERT_EQUAL_HEX16(0x3412, d.first);
}

void test_fill_rect_odd_edges_and_clip() {
  s_c.clear(0);
  s_c.fillRect(3, 0, 4, 1, 5);   // x 3..6
  TEST_ASSERT_EQUAL(0, s_c.get(2, 0));
  TEST_ASSERT_EQUAL(5, s_c.get(3, 0));
  TEST_ASSERT_EQUAL(5, s_c.get(6, 0));
  TEST_ASSERT_EQUAL(0, s_c.get(7, 0));
  s_c.clip(0, 0, 4, 4);
  s_c.fillRect(0, 1, 100, 1, 7);
  TEST_ASSERT_EQUAL(7, s_c.get(3, 1));
  TEST_ASSERT_EQUAL(0, s_c.get(4, 1));
  s_c.noClip();
}

void test_utf8_width_counts_code_points() {
  TEST_ASSERT_EQUAL(4 * 8, Canvas::textWidth("caf\xc3\xa9", &font_large));
  TEST_ASSERT_EQUAL(3, Canvas::fitBytes("caf\xc3\xa9", &font_large, 31));   // 'é' doesn't fit
  TEST_ASSERT_EQUAL(5, Canvas::fitBytes("caf\xc3\xa9", &font_large, 32));
}


// --- UTF-8 decoding ------------------------------------------------------

// Decode all of `s` (n bytes, bounded) into code points; returns the count.
static int decodeAll(const char* s, int n, uint32_t* out, int cap) {
  int k = 0;
  for (const char* p = s; p < s + n && k < cap;) {
    int used = utf8Decode(p, s + n, &out[k++]);
    TEST_ASSERT_TRUE(used >= 1 && used <= s + n - p);
    p += used;
  }
  return k;
}

#define DECODES_TO(bytes, expect)                                     \
  do {                                                                \
    uint32_t cp = 0;                                                  \
    int n = utf8Decode(bytes, bytes + sizeof(bytes) - 1, &cp);        \
    TEST_ASSERT_EQUAL_INT((int)sizeof(bytes) - 1, n);                 \
    TEST_ASSERT_EQUAL_HEX32(expect, cp);                              \
  } while (0)

void test_utf8_valid_sequences() {
  DECODES_TO("A", 0x41);
  DECODES_TO("\xc3\xa9", 0xE9);               // 2 bytes
  DECODES_TO("\xc2\x80", 0x80);               // smallest 2-byte
  DECODES_TO("\xdf\xbf", 0x7FF);
  DECODES_TO("\xe2\x82\xac", 0x20AC);         // 3 bytes
  DECODES_TO("\xe0\xa0\x80", 0x800);
  DECODES_TO("\xed\x9f\xbf", 0xD7FF);         // just below the surrogates
  DECODES_TO("\xee\x80\x80", 0xE000);         // just above them
  DECODES_TO("\xef\xbf\xbd", 0xFFFD);
  DECODES_TO("\xf0\x9f\x98\x80", 0x1F600);    // 4 bytes
  DECODES_TO("\xf0\x90\x80\x80", 0x10000);
  DECODES_TO("\xf4\x8f\xbf\xbf", 0x10FFFF);   // the last code point
}

// Each of these must give U+FFFD for exactly `used` bytes, never more.
#define REJECTS(bytes, used)                                                \
  do {                                                                      \
    uint32_t cp = 0;                                                        \
    int n = utf8Decode(bytes, bytes + sizeof(bytes) - 1, &cp);              \
    TEST_ASSERT_EQUAL_HEX32(0xFFFD, cp);                                    \
    TEST_ASSERT_EQUAL_INT(used, n);                                         \
  } while (0)

void test_utf8_rejects_malformed() {
  REJECTS("\x80", 1);                // stray continuation
  REJECTS("\xbf", 1);
  REJECTS("\xff", 1);
  REJECTS("\xfe", 1);
  REJECTS("\xc0\x80", 1);            // overlong NUL
  REJECTS("\xc1\xbf", 1);            // overlong 0x7F
  REJECTS("\xe0\x80\x80", 1);        // overlong 3-byte
  REJECTS("\xe0\x9f\xbf", 1);        // overlong, top of range
  REJECTS("\xf0\x80\x80\x80", 1);    // overlong 4-byte
  REJECTS("\xf0\x8f\xbf\xbf", 1);
  REJECTS("\xed\xa0\x80", 1);        // U+D800, surrogate
  REJECTS("\xed\xbf\xbf", 1);        // U+DFFF
  REJECTS("\xf4\x90\x80\x80", 1);    // U+110000, out of range
  REJECTS("\xf5\x80\x80\x80", 1);    // lead byte never valid
  REJECTS("\xf8\x88\x80\x80\x80", 1);// 5-byte form
  REJECTS("\xe2\x82", 2);            // truncated 3-byte, at the end
  REJECTS("\xf0\x9f\x98", 3);        // truncated 4-byte
  REJECTS("\xc3", 1);                // lead byte alone
}

void test_utf8_resynchronizes_after_damage() {
  uint32_t cp[8];
  // 'a', a 3-byte cut short before 'b', a 4-byte cut short before 'c', then é.
  const char s[] = "a\xe2\x82" "b" "\xf0\x9f" "c" "\xc3\xa9";
  int n = decodeAll(s, sizeof s - 1, cp, 8);
  TEST_ASSERT_EQUAL(6, n);
  TEST_ASSERT_EQUAL_HEX32('a', cp[0]);
  TEST_ASSERT_EQUAL_HEX32(0xFFFD, cp[1]);
  TEST_ASSERT_EQUAL_HEX32('b', cp[2]);
  TEST_ASSERT_EQUAL_HEX32(0xFFFD, cp[3]);
  TEST_ASSERT_EQUAL_HEX32('c', cp[4]);
  TEST_ASSERT_EQUAL_HEX32(0xE9, cp[5]);
  // A run of continuation bytes is one replacement each, not a stall.
  const char run[] = "\x80\x80\x80x";
  n = decodeAll(run, sizeof run - 1, cp, 8);
  TEST_ASSERT_EQUAL(4, n);
  TEST_ASSERT_EQUAL_HEX32('x', cp[3]);
}

void test_utf8_never_reads_past_end() {
  // Heap-sized exactly, so ASan flags any read beyond `n` bytes.
  const char* full = "\xf0\x9f\x98\x80";
  for (int n = 0; n <= 4; n++) {
    char* buf = new char[n ? n : 1];
    memcpy(buf, full, n);
    uint32_t cp = 0;
    int used = utf8Decode(buf, buf + n, &cp);
    TEST_ASSERT_EQUAL_INT(n, used);                      // all of it, or nothing
    if (n == 0) TEST_ASSERT_EQUAL_HEX32(0, cp);          // untouched
    else if (n < 4) TEST_ASSERT_EQUAL_HEX32(0xFFFD, cp);
    else TEST_ASSERT_EQUAL_HEX32(0x1F600, cp);
    delete[] buf;
  }
  // An embedded NUL is a character in a bounded buffer, and cuts a sequence.
  uint32_t cp;
  TEST_ASSERT_EQUAL_INT(1, utf8Decode("\0z", "\0z" + 2, &cp));
  TEST_ASSERT_EQUAL_HEX32(0, cp);
  TEST_ASSERT_EQUAL_INT(1, utf8Decode("\xe2\0\0", "\xe2\0\0" + 3, &cp));
  TEST_ASSERT_EQUAL_HEX32(0xFFFD, cp);
}

void test_utf8_next_walks_a_string() {
  const char* s = "h\xc3\xa9\xf0\x9f\x98\x80\xff!";
  const char* p = s;
  TEST_ASSERT_EQUAL_HEX32('h', utf8Next(&p));
  TEST_ASSERT_EQUAL_HEX32(0xE9, utf8Next(&p));
  TEST_ASSERT_EQUAL_HEX32(0x1F600, utf8Next(&p));
  TEST_ASSERT_EQUAL_HEX32(0xFFFD, utf8Next(&p));
  TEST_ASSERT_EQUAL_HEX32('!', utf8Next(&p));
  TEST_ASSERT_EQUAL_INT(0, *p);
  // A sequence cut short by the terminator stops there instead of eating it.
  const char* t = "\xe2\x82";
  TEST_ASSERT_EQUAL_HEX32(0xFFFD, utf8Next(&t));
  TEST_ASSERT_EQUAL_INT(0, *t);
}

void test_utf8_width_with_invalid_input() {
  // One cell per replacement: the truncated 3-byte sequence is one, the
  // rejected surrogate is three (the lead byte and each stray continuation).
  TEST_ASSERT_EQUAL(3 * 8, Canvas::textWidth("a\xe2\x82" "b", &font_large));
  TEST_ASSERT_EQUAL(4 * 8, Canvas::textWidth("\xed\xa0\x80z", &font_large));
}

static int s_lines;
static char s_line[8][64];
static void collect(void*, const char* s, int len, int line) {
  if (line < 8) { memcpy(s_line[line], s, len); s_line[line][len] = 0; }
  s_lines = line + 1;
}

void test_wrap_breaks_at_spaces() {
  memset(s_line, 0, sizeof s_line);
  int n = wrapText("hello world again", &font_large, 8 * 11, collect, nullptr);  // 11 cols
  TEST_ASSERT_EQUAL(2, n);
  TEST_ASSERT_EQUAL_STRING("hello world", s_line[0]);
  TEST_ASSERT_EQUAL_STRING("again", s_line[1]);
}

void test_wrap_hard_breaks_long_words() {
  int n = wrapText("abcdefghijkl", &font_large, 8 * 5, collect, nullptr);
  TEST_ASSERT_EQUAL(3, n);
  TEST_ASSERT_EQUAL_STRING("abcde", s_line[0]);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_flush_pushes_only_changed_rows);
  RUN_TEST(test_flush_byte_swaps_for_big_endian_panels);
  RUN_TEST(test_fill_rect_odd_edges_and_clip);
  RUN_TEST(test_utf8_width_counts_code_points);
  RUN_TEST(test_utf8_valid_sequences);
  RUN_TEST(test_utf8_rejects_malformed);
  RUN_TEST(test_utf8_resynchronizes_after_damage);
  RUN_TEST(test_utf8_never_reads_past_end);
  RUN_TEST(test_utf8_next_walks_a_string);
  RUN_TEST(test_utf8_width_with_invalid_input);
  RUN_TEST(test_wrap_breaks_at_spaces);
  RUN_TEST(test_wrap_hard_breaks_long_words);
  return UNITY_END();
}
