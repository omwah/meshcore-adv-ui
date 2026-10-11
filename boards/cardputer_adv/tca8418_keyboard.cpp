#include "tca8418_keyboard.h"
#include <Arduino.h>
#include <Wire.h>

static constexpr uint8_t ADDR = 0x34;
static constexpr uint8_t REG_CFG = 0x01, REG_INT_STAT = 0x02, REG_KEY_LCK_EC = 0x03, REG_KEY_EVENT_A = 0x04;
static constexpr uint8_t REG_KP_GPIO1 = 0x1D, REG_KP_GPIO2 = 0x1E, REG_KP_GPIO3 = 0x1F;

static bool rd(uint8_t reg, uint8_t& v) {
  Wire.beginTransmission(ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom(ADDR, (uint8_t)1) != 1) return false;
  v = Wire.read();
  return true;
}

static bool wr(uint8_t reg, uint8_t v) {
  Wire.beginTransmission(ADDR);
  Wire.write(reg);
  Wire.write(v);
  return Wire.endTransmission() == 0;
}

bool Tca8418Keyboard::begin() {
  pinMode(PIN_KBD_INT, INPUT_PULLUP);
  // A launcher can leave the chip mid-transfer: try the first access 3 times.
  bool ok = false;
  for (int i = 0; i < 3 && !ok; i++) {
    Wire.beginTransmission(ADDR);
    ok = Wire.endTransmission() == 0;
    if (!ok) delay(20);
  }
  ok = ok && wr(REG_KP_GPIO1, 0x7F)    // rows 0-6
          && wr(REG_KP_GPIO2, 0xFF)    // columns 0-7
          && wr(REG_KP_GPIO3, 0x00)
          && wr(REG_CFG, 0x01);        // key event interrupts
  uint8_t ev;
  for (int i = 0; ok && i < 16 && rd(REG_KEY_EVENT_A, ev) && ev; i++) {
  }
  ok = ok && wr(REG_INT_STAT, 0x1F);
  cardputer_keymap_reset(&_st);
  _ok = ok;
  return ok;
}

void Tca8418Keyboard::drain() {
  uint8_t n = 0;
  if (!rd(REG_KEY_LCK_EC, n)) return;
  n &= 0x0F;
  for (int i = 0; i < n; i++) {
    uint8_t ev;
    if (!rd(REG_KEY_EVENT_A, ev) || ev == 0) break;
    int row, col;
    kbd_report_t r;
    if (cardputer_keymap_key(ev & 0x7F, &row, &col) && cardputer_keymap_event(&_st, row, col, ev & 0x80, &r))
      _diff.update(r);
  }
  wr(REG_INT_STAT, 0x01);
}

void Tca8418Keyboard::inject(int row, int col, bool down) {
  kbd_report_t r;
  if (cardputer_keymap_event(&_st, row, col, down, &r)) _diff.update(r);
}

bool Tca8418Keyboard::poll(hal::KeyEvent& ev) {
  if (_diff.next(ev)) return true;
  if (!_ok) return false;
  // The interrupt line pulls low when events wait; also look every 20 ms in
  // case it isn't wired on some units.
  uint32_t now = millis();
  if (digitalRead(PIN_KBD_INT) == LOW || now - _lastPoll >= 20) {
    _lastPoll = now;
    drain();
  }
  return _diff.next(ev);
}
