#include "ws2812_led.h"
#include <Arduino.h>

static constexpr uint8_t PIN_RGB_LED = 21;

void Ws2812Led::begin() { set(0, 0, 0); }

void Ws2812Led::set(uint8_t r, uint8_t g, uint8_t b) {
  bool lit = r || g || b;
  if (lit) _power.ledPower(true);
  neopixelWrite(PIN_RGB_LED, r, g, b);
  if (!lit) _power.ledPower(false);
}
