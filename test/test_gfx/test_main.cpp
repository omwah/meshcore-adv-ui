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
  RUN_TEST(test_wrap_breaks_at_spaces);
  RUN_TEST(test_wrap_hard_breaks_long_words);
  return UNITY_END();
}
