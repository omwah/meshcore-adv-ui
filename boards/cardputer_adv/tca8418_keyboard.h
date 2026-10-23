// The Cardputer ADV keyboard: a TCA8418 key-scan controller (I2C 0x34,
// interrupt on GPIO11) wired as 7 rows x 8 columns. Register sequence ported
// from esp-vim's esp_kbd_cardputer_dev.c (Apache-2.0) to Arduino Wire; key
// mapping is cardputer_keymap.c.
#pragma once
#include "hal/keyboard.h"
extern "C" {
#include "keymap/cardputer_keymap.h"
}
#include "report_diff.h"

class Tca8418Keyboard : public hal::Keyboard {
public:
  bool begin() override;
  bool poll(hal::KeyEvent& ev) override;
  void setTextMode(bool on) override { cardputer_keymap_text_mode(&_st, on); }
  bool present() const { return _ok; }
  // Feed a matrix event as if the key were pressed (bring-up diagnostics).
  void inject(int row, int col, bool down);

private:
  void drain();
  cardputer_keymap_t _st{};
  ReportDiff _diff;
  bool _ok = false;
  uint32_t _lastPoll = 0;
};
