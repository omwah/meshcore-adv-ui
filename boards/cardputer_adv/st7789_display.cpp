#include "st7789_display.h"
#include <Arduino.h>

static constexpr int X_GAP = 40, Y_GAP = 53;
static constexpr uint8_t BL_CHANNEL = 0;

void St7789Display::cmd(uint8_t c) {
  digitalWrite(PIN_TFT_DC, LOW);
  digitalWrite(PIN_TFT_CS, LOW);
  _spi.write(c);
  digitalWrite(PIN_TFT_CS, HIGH);
}

void St7789Display::data(const uint8_t* d, size_t n) {
  digitalWrite(PIN_TFT_DC, HIGH);
  digitalWrite(PIN_TFT_CS, LOW);
  _spi.writeBytes(d, n);
  digitalWrite(PIN_TFT_CS, HIGH);
}

void St7789Display::window(int x0, int y0, int x1, int y1) {
  uint8_t c[4] = {(uint8_t)((x0 + X_GAP) >> 8), (uint8_t)(x0 + X_GAP), (uint8_t)((x1 + X_GAP) >> 8), (uint8_t)(x1 + X_GAP)};
  cmd(0x2A);   // CASET
  data(c, 4);
  uint8_t r[4] = {(uint8_t)((y0 + Y_GAP) >> 8), (uint8_t)(y0 + Y_GAP), (uint8_t)((y1 + Y_GAP) >> 8), (uint8_t)(y1 + Y_GAP)};
  cmd(0x2B);   // RASET
  data(r, 4);
  cmd(0x2C);   // RAMWR
}

bool St7789Display::begin() {
  if (_ready) return true;
  pinMode(PIN_TFT_CS, OUTPUT);
  pinMode(PIN_TFT_DC, OUTPUT);
  pinMode(PIN_TFT_RST, OUTPUT);
  digitalWrite(PIN_TFT_CS, HIGH);
  _spi.begin(PIN_TFT_SCL, -1, PIN_TFT_SDA, -1);
  _spi.beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));   // the display owns this bus

  digitalWrite(PIN_TFT_RST, LOW);
  delay(10);
  digitalWrite(PIN_TFT_RST, HIGH);
  delay(120);

  cmd(0x01);   // SWRESET
  delay(150);
  cmd(0x11);   // SLPOUT
  delay(120);
  uint8_t colmod = 0x55;   // 16 bits per pixel
  cmd(0x3A);
  data(&colmod, 1);
  uint8_t madctl = 0x60;   // MV (swap X/Y) | MX (mirror X), RGB order
  cmd(0x36);
  data(&madctl, 1);
  cmd(0x21);   // INVON: this panel needs inverted colours
  cmd(0x13);   // NORON
  cmd(0x29);   // DISPON

  // The backlight driver only dims correctly at a low PWM rate: at 5 kHz it
  // stays dark (found on hardware); 256 Hz works.
  ledcSetup(BL_CHANNEL, 256, 8);
  ledcAttachPin(PIN_TFT_BL, BL_CHANNEL);
  _ready = true;
  return true;
}

void St7789Display::pushRows(int y, int h, const uint16_t* px) {
  window(0, y, 239, y + h - 1);
  data((const uint8_t*)px, (size_t)240 * h * 2);   // already big-endian (bigEndian())
}

void St7789Display::setBacklight(uint8_t level) {
  ledcWrite(BL_CHANNEL, level);
}

void St7789Display::backlightRaw(int level) {
  if (level < 0) {
    ledcDetachPin(PIN_TFT_BL);
    pinMode(PIN_TFT_BL, OUTPUT);
    digitalWrite(PIN_TFT_BL, HIGH);
  } else {
    ledcAttachPin(PIN_TFT_BL, BL_CHANNEL);
    ledcWrite(BL_CHANNEL, level);
  }
}

void St7789Display::backlightPwm(uint32_t hz, uint8_t duty) {
  ledcDetachPin(PIN_TFT_BL);
  ledcSetup(BL_CHANNEL, hz, 8);
  ledcAttachPin(PIN_TFT_BL, BL_CHANNEL);
  ledcWrite(BL_CHANNEL, duty);
}

void St7789Display::sleep(bool on) {
  cmd(on ? 0x10 : 0x11);   // SLPIN / SLPOUT
  if (!on) delay(5);
}
