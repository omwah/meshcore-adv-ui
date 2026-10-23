// Host micro-benchmark of the text path: drawing, measuring and wrapping text
// in each class of character, and a flush with emoji sprites on screen. Built
// and run by scripts/bench-text.sh. Host numbers show relative cost between
// profiles and character classes; the ESP32-S3 is slower by a roughly constant
// factor, so use them to compare, not as device timings.
#include <chrono>
#include <stdio.h>
#include <string.h>
#include "ui/app.h"
#include "gen/fonts.h"
#include "gen/unicode_cfg.h"

using namespace ui;
using Clock = std::chrono::steady_clock;

class NullDisplay : public hal::Display {
public:
  bool begin() override { return true; }
  int width() const override { return Canvas::W; }
  int height() const override { return Canvas::H; }
  bool bigEndian() const override { return true; }
  void pushRows(int, int, const uint16_t* p) override { sink += p[0]; }
  void setBacklight(uint8_t) override {}
  volatile uint32_t sink = 0;
};

static Canvas c;
static NullDisplay d;
static const uint16_t kPal[16] = {0x0000, 0x1234};

template <class F> static double perOp(int iters, F f) {
  for (int i = 0; i < iters / 10 + 1; i++) f();     // warm up
  auto t0 = Clock::now();
  for (int i = 0; i < iters; i++) f();
  return std::chrono::duration<double, std::nano>(Clock::now() - t0).count() / iters;
}

static void line(const char* what, const char* s, const mc_font_t* f) {
  int adv, elems = 0;
  for (const char* p = s; *p;) { p += Canvas::elem(p, f, &adv); elems++; }
  double draw = perOp(20000, [&] { c.clear(0); c.text(0, 0, s, f, 1); });
  double meas = perOp(200000, [&] { volatile int w = Canvas::textWidth(s, f); (void)w; });
  printf("  %-26s %3d elements  draw %8.0f ns (%6.1f/elem)  measure %7.0f ns (%5.1f/elem)\n", what, elems, draw,
         draw / elems, meas, meas / elems);
}

static int sink_lines;
static void count(void*, const char*, int, int) { sink_lines++; }

int main() {
  const mc_font_t* f = &font_large;
  printf("profile %s, %dx%d, font %dx%d\n", MC_UNICODE_PROFILE, Canvas::W, Canvas::H, f->width, f->height);
  line("ASCII", "The quick brown fox jumps over", f);
  line("Latin-1 (Spleen)", "Caf\xc3\xa9 r\xc3\xa9sum\xc3\xa9 na\xc3\xafve \xc3\xa0 la", f);
  line("Greek (fallback layer)", "\xce\x9a\xce\xb1\xce\xbb\xce\xb7\xce\xbc\xce\xad\xcf\x81\xce\xb1 \xce\xba\xcf\x8c\xcf\x83\xce\xbc\xce\xb5", f);
  line("Unknown (replacement)", "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac", f);
  line("Emoji (single)", "\xf0\x9f\x98\x80\xf0\x9f\x98\x82\xf0\x9f\xa4\xa3\xf0\x9f\x98\x8a\xf0\x9f\x98\x8e\xf0\x9f\x91\x8d\xf0\x9f\x91\x8b", f);
  line("Emoji (selector, tone, ZWJ)", "\xe2\x9d\xa4\xef\xb8\x8f\xf0\x9f\x91\x8d\xf0\x9f\x8f\xbd\xf0\x9f\x91\xa8\xe2\x80\x8d\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x91\xa7", f);
  line("Mixed message", "Battery low \xf0\x9f\x94\x8b\xe2\x9a\xa0\xef\xb8\x8f at the ridge \xf0\x9f\x93\x8d, back by 5", f);

  const char* msg = "Location received \xf0\x9f\x93\x8d then a long line so it wraps: "
                    "\xf0\x9f\x94\xa5\xf0\x9f\x94\xa5\xf0\x9f\x94\xa5\xf0\x9f\x94\xa5\xf0\x9f\x94\xa5 and the signal "
                    "is 78% \xf0\x9f\x93\xb6 on the way home \xf0\x9f\x8f\xa0 see you at nine";
  double wrap = perOp(50000, [&] { wrapText(msg, f, 200, count, nullptr); });
  printf("  %-26s %3d bytes     wrap %9.0f ns\n", "wrapText, 200 px", (int)strlen(msg), wrap);

  // A screen of text with 24 emoji, flushed whole (every row dirty).
  auto screen = [&](bool withEmoji) {
    c.noClip();
    c.clear(0);
    for (int row = 0; row < 8; row++)
      c.text(0, row * 16, withEmoji ? "ab \xf0\x9f\x98\x80\xf0\x9f\x91\x8d\xf0\x9f\x94\xa5 cd \xf0\x9f\x93\xa1\xf0\x9f\x94\x8b\xe2\x9a\xa1 ef"
                                    : "ab ??? cd ??? ef", f, 1);
  };
  screen(false);
  double plain = perOp(2000, [&] { screen(false); c.flush(d, kPal, true); });
  screen(true);
  double rich = perOp(2000, [&] { screen(true); c.flush(d, kPal, true); });
  printf("  %-26s plain screen + flush %8.0f ns   with 48 emoji %8.0f ns   (+%.0f%%)\n", "full flush", plain, rich,
         100 * (rich - plain) / plain);
  return 0;
}
