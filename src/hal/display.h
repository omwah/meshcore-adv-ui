// Display panel seen by the UI: a fixed-size RGB565 surface that accepts
// horizontal bands of pixels. Boards implement it (ST7789, SDL, headless...).
#pragma once
#include <stdint.h>

namespace hal {

class Display {
public:
  virtual ~Display() {}
  virtual bool begin() = 0;
  virtual int width() const = 0;
  virtual int height() const = 0;
  // Pixels in the order the panel wants them on the wire: true when each
  // RGB565 value must be byte-swapped (big-endian SPI panels).
  virtual bool bigEndian() const { return false; }
  // Write rows [y, y+h) of full width from `px` (width*h values).
  virtual void pushRows(int y, int h, const uint16_t* px) = 0;
  virtual void setBacklight(uint8_t level) = 0;   // 0 = off, 255 = max
  // Panel low-power mode while the screen is off (contents are kept).
  virtual void sleep(bool on) { (void)on; }
};

}  // namespace hal
