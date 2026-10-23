// Turns raw key events into UI keys, with the rules that keep navigation
// predictable on a small keyboard:
//
//  * A key that goes down again within `lockout_ms` of its last press is
//    ignored (contact chatter, a finger bouncing on a key).
//  * Only arrows, Backspace and Delete auto-repeat (after 400 ms, every
//    80 ms), and with vim keys h/j/k/l too. Enter and every other key fire
//    once per physical press.
//  * Esc fires on release; held for 700 ms it becomes K_LONG_ESC instead
//    (the app takes that as "go home").
//  * flush() drops queued keys and swallows the keys still held, so a press
//    meant for one screen never lands on the next (call it on every screen
//    change).
#pragma once
#include <stdint.h>
#include "hal/keyboard.h"
#include "ui/input/keys.h"

namespace ui {

class Input {
public:
  static constexpr uint32_t REPEAT_DELAY = 400, REPEAT_EVERY = 80, LONG_ESC = 700;

  void setLockout(uint16_t ms) { _lockout = ms; }
  uint16_t lockout() const { return _lockout; }
  // Vim keys: h/j/k/l repeat like the arrows they stand for.
  void setVimRepeat(bool on) { _vimRepeat = on; }

  // Feed one raw event at time `now` (ms).
  void feed(const hal::KeyEvent& ev, uint32_t now);
  // Advance timers (repeat, long Esc).
  void tick(uint32_t now);
  bool next(Key& k);
  void flush();
  bool pending() const { return _n > 0; }
  // Milliseconds until tick() may produce something, for sleeping (0 = idle).
  uint32_t wakeIn(uint32_t now) const;

  static uint16_t translate(uint8_t usage, uint8_t mods);

private:
  void push(uint16_t code, uint8_t mods, bool repeat);
  bool repeats(uint16_t code, uint8_t mods) const;

  static constexpr int Q = 8;
  Key _q[Q];
  uint8_t _head = 0, _n = 0;
  uint16_t _lockout = 60;
  bool _vimRepeat = false;

  uint8_t _heldUsage = 0, _heldMods = 0;   // newest key held
  bool _heldSwallowed = false;             // held across a flush(): ignore until released
  uint32_t _heldSince = 0, _nextRepeat = 0;
  bool _longEscSent = false;

  uint8_t _lastUsage = 0;
  uint32_t _lastDown = 0;
};

}  // namespace ui
