// The Cardputer ADV's 1.14" ST7789V2 (240x135) on its own SPI bus.
// Panel settings follow esp-vim's Cardputer configuration: colour inversion,
// rows/columns swapped and mirrored in X, window offset (40, 53).
#pragma once
#include <SPI.h>
#include "hal/display.h"

class St7789Display : public hal::Display {
public:
  bool begin() override;
  int width() const override { return 240; }
  int height() const override { return 135; }
  bool bigEndian() const override { return true; }
  void pushRows(int y, int h, const uint16_t* px) override;
  void setBacklight(uint8_t level) override;
  void sleep(bool on) override;
  void backlightRaw(int level);   // diagnostics: <0 = plain GPIO high, else PWM duty
  void backlightPwm(uint32_t hz, uint8_t duty);   // diagnostics: try a PWM frequency
  void reinit() { _ready = false; begin(); }
  // The backlight pin also switches the power the RGB LED runs on. Dimming
  // chops that power, and the LED forgets its colour each time it drops:
  // hold it fully on while the LED shows something (the backlight goes to
  // full meanwhile; a sleeping panel lets the least of it through).
  void ledPower(bool on);

private:
  void cmd(uint8_t c);
  void data(const uint8_t* d, size_t n);
  void window(int x0, int y0, int x1, int y1);
  SPIClass _spi{FSPI};
  bool _ready = false;
  uint8_t _level = 0;        // the backlight as last set
  bool _railHeld = false;    // the pin is held high for it
};
