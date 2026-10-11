// Keyboard seen by the UI: key down/up events as HID usages (page 7) with
// the modifiers they went down with. Boards translate their matrix into this.
#pragma once
#include <stdint.h>

namespace hal {

struct KeyEvent {
  uint8_t usage;   // HID usage: a-z 0x04.., Enter 0x28, Esc 0x29, arrows 0x4f-0x52
  uint8_t mods;    // KBD_MOD_* style bits (ctrl 0x11, shift 0x22, alt 0x44)
  bool down;
};

class Keyboard {
public:
  virtual ~Keyboard() {}
  virtual bool begin() = 0;
  // Next pending event, if any. Called from the UI loop.
  virtual bool poll(KeyEvent& ev) = 0;
  // Text entry vs. navigation layout (boards with dual-purpose keys).
  virtual void setTextMode(bool on) { (void)on; }
};

}  // namespace hal
