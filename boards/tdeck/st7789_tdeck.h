// The T-Deck's 2.8" ST7789 (320x240) on the SPI bus it shares with LoRa and
// the SD card. Panel settings follow esp-vim's T-Deck configuration: colour
// inversion, rows/columns swapped and mirrored in X, no offset, no reset line.
#pragma once
#include <SPI.h>
#include "hal/display.h"

class St7789TDeck : public hal::Display {
public:
  explicit St7789TDeck(SPIClass& spi) : _spi(spi) {}
  bool begin() override;
  int width() const override { return 320; }
  int height() const override { return 240; }
  bool bigEndian() const override { return true; }
  void pushRows(int y, int h, const uint16_t* px) override;
  void setBacklight(uint8_t level) override;
  void sleep(bool on) override;

private:
  void cmd(uint8_t c);
  void data(const uint8_t* d, size_t n);
  SPIClass& _spi;
  bool _ready = false;
  uint8_t _step = 0;   // backlight driver's step, 0 (off) to 16
};
