// Headless panel: keeps the pushed RGB565 frame in memory for screenshots.
#pragma once
#include <stdio.h>
#include <string.h>
#include "hal/display.h"

namespace sim {

class SimDisplay : public hal::Display {
public:
  static constexpr int W = 240, H = 135;
  bool begin() override { return true; }
  int width() const override { return W; }
  int height() const override { return H; }
  void pushRows(int y, int h, const uint16_t* px) override {
    memcpy(&_fb[y * W], px, sizeof(uint16_t) * W * h);
    _pushedRows += h;
  }
  void setBacklight(uint8_t level) override { _bl = level; }

  bool writePpm(const char* path) const {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (int i = 0; i < W * H; i++) {
      uint16_t c = _fb[i];
      uint8_t rgb[3] = {(uint8_t)((c >> 11) * 255 / 31), (uint8_t)(((c >> 5) & 63) * 255 / 63), (uint8_t)((c & 31) * 255 / 31)};
      fwrite(rgb, 1, 3, f);
    }
    return fclose(f) == 0;
  }
  const uint16_t* frame() const { return _fb; }
  unsigned long pushedRows() const { return _pushedRows; }

private:
  uint16_t _fb[W * H] = {};
  uint8_t _bl = 255;
  unsigned long _pushedRows = 0;
};

}  // namespace sim
