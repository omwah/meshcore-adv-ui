#include "st7789_tdeck.h"
#include <Arduino.h>

static const SPISettings kSpi(40000000, MSBFIRST, SPI_MODE0);

// The bus is shared: each command and its data are one transaction.
void St7789TDeck::cmd(uint8_t c) {
  _spi.beginTransaction(kSpi);
  digitalWrite(PIN_TFT_DC, LOW);
  digitalWrite(PIN_TFT_CS, LOW);
  _spi.write(c);
  digitalWrite(PIN_TFT_CS, HIGH);
  _spi.endTransaction();
}

void St7789TDeck::data(const uint8_t* d, size_t n) {
  _spi.beginTransaction(kSpi);
  digitalWrite(PIN_TFT_DC, HIGH);
  digitalWrite(PIN_TFT_CS, LOW);
  _spi.writeBytes(d, n);
  digitalWrite(PIN_TFT_CS, HIGH);
  _spi.endTransaction();
}

bool St7789TDeck::begin() {
  if (_ready) return true;
  pinMode(PIN_TFT_CS, OUTPUT);
  pinMode(PIN_TFT_DC, OUTPUT);
  digitalWrite(PIN_TFT_CS, HIGH);
  pinMode(PIN_TFT_BL, OUTPUT);
  digitalWrite(PIN_TFT_BL, LOW);

  cmd(0x01);   // SWRESET (no reset line)
  delay(150);
  cmd(0x11);   // SLPOUT
  delay(120);
  uint8_t colmod = 0x55;   // 16 bits per pixel
  cmd(0x3A);
  data(&colmod, 1);
  uint8_t madctl = 0x60;   // MV (swap X/Y) | MX (mirror X), RGB order
  cmd(0x36);
  data(&madctl, 1);
  cmd(0x21);   // INVON
  cmd(0x13);   // NORON
  cmd(0x29);   // DISPON
  _ready = true;
  return true;
}

void St7789TDeck::pushRows(int y, int h, const uint16_t* px) {
  uint8_t c[4] = {0, 0, (uint8_t)(319 >> 8), (uint8_t)(319 & 0xFF)};
  uint8_t r[4] = {(uint8_t)(y >> 8), (uint8_t)y, (uint8_t)((y + h - 1) >> 8), (uint8_t)(y + h - 1)};
  // Window and pixels in one transaction: the radio never cuts in between.
  _spi.beginTransaction(kSpi);
  digitalWrite(PIN_TFT_CS, LOW);
  digitalWrite(PIN_TFT_DC, LOW);
  _spi.write(0x2A);   // CASET
  digitalWrite(PIN_TFT_DC, HIGH);
  _spi.writeBytes(c, 4);
  digitalWrite(PIN_TFT_DC, LOW);
  _spi.write(0x2B);   // RASET
  digitalWrite(PIN_TFT_DC, HIGH);
  _spi.writeBytes(r, 4);
  digitalWrite(PIN_TFT_DC, LOW);
  _spi.write(0x2C);   // RAMWR
  digitalWrite(PIN_TFT_DC, HIGH);
  _spi.writeBytes((const uint8_t*)px, (size_t)320 * h * 2);   // already big-endian (bigEndian())
  digitalWrite(PIN_TFT_CS, HIGH);
  _spi.endTransaction();
}

// The backlight driver dims in 16 steps, counted in pulses on its pin: each
// low-high pulse steps it down one, wrapping from the dimmest to the
// brightest; low for 3 ms turns it off. As LilyGo's T-Deck example does it.
void St7789TDeck::setBacklight(uint8_t level) {
  uint8_t to = level ? (uint8_t)(1 + (level * 15 + 127) / 255) : 0;   // 1..16
  if (to == _step) return;
  if (!to) {
    digitalWrite(PIN_TFT_BL, LOW);
    delay(3);
    _step = 0;
    return;
  }
  if (!_step) {
    digitalWrite(PIN_TFT_BL, HIGH);   // on at the brightest
    _step = 16;
    delayMicroseconds(30);
  }
  int pulses = ((16 - to) - (16 - _step) + 16) % 16;
  for (int i = 0; i < pulses; i++) {
    digitalWrite(PIN_TFT_BL, LOW);
    digitalWrite(PIN_TFT_BL, HIGH);
  }
  _step = to;
}

void St7789TDeck::sleep(bool on) {
  cmd(on ? 0x10 : 0x11);   // SLPIN / SLPOUT
  if (!on) delay(5);
}
