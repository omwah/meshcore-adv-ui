// The Cardputer ADV's RGB LED: one WS2812 on GPIO21 (as in Meshtastic's
// m5stack_cardputer_adv variant), driven by the Arduino core's RMT helper.
//
// Its power comes through the backlight switch (the same note as
// Meshtastic's): the display keeps that on while the LED is lit.
#pragma once
#include "hal/led.h"
#include "st7789_display.h"

class Ws2812Led : public hal::Led {
public:
  explicit Ws2812Led(St7789Display& power) : _power(power) {}
  void begin();   // dark at boot
  void set(uint8_t r, uint8_t g, uint8_t b) override;

private:
  St7789Display& _power;
};
