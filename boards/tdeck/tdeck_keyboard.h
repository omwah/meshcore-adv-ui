// The T-Deck's keyboard and trackball. The keys are scanned by an ESP32-C3
// at I2C 0x55; in its raw mode it answers with five bytes, a bit per key
// (read every 20 ms, as esp-vim's esp_kbd_tdeck_dev.c does, Apache-2.0).
// Firmware older than June 2025 has no raw mode and sends characters only.
// Mapping is tdeck_keymap.c. The trackball's four hall sensors toggle pins as
// it rolls (arrow keys every two toggles); its click, on GPIO0, is Enter and also
// the unlock button (hal::Keyboard::unlockPressed).
#pragma once
#include "hal/keyboard.h"
extern "C" {
#include "tdeck_keymap.h"
}
#include "report_diff.h"

class TDeckKeyboard : public hal::Keyboard {
public:
  bool begin() override;
  bool poll(hal::KeyEvent& ev) override;
  bool present() const { return _ok; }
  bool rawMode() const { return _raw; }
  bool hasUnlockButton() const override { return true; }
  bool unlockPressed() override { bool u = _unlock; _unlock = false; return u; }
  void setLocked(bool locked) override { _locked = locked; }
  // A worn trackball can roll by itself: off, its rolls are ignored (the
  // click still works).
  void setBall(bool on) { _ballOn = on; }
  // Key backlight while keys are being used (0: off), and how long after.
  void setLight(uint8_t level, uint16_t seconds) { _lightLevel = level; _lightSec = seconds; }

private:
  void keys();
  void ball();
  void click();
  void light();
  void typed(uint8_t c);
  void tap(uint8_t usage, uint8_t mods);
  tdeck_keymap_t _st{};
  tdeck_keymap_ball_t _ball{};
  ReportDiff _diff;
  uint8_t _matrix[TDECK_KEYMAP_COLS] = {};
  bool _ok = false, _raw = true, _ballOn = true;
  bool _clickDown = false, _unlock = false, _locked = false;
  uint32_t _clickAt = 0;
  uint32_t _lastPoll = 0, _lastKey = 0;
  uint8_t _lightLevel = 127, _lit = 0xFF;
  uint16_t _lightSec = 10;
};
