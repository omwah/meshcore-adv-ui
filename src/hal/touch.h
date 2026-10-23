// Touch panel seen by the UI: one finger's down, moves and up, in display
// pixels (the board turns the panel's own axes into the screen's).
#pragma once
#include <stdint.h>

namespace hal {

struct TouchEvent {
  enum Type : uint8_t { DOWN, MOVE, UP };
  int16_t x, y;
  Type type;
};

class Touch {
public:
  virtual ~Touch() {}
  virtual bool begin() = 0;
  // Next pending event, if any. Called from the UI loop.
  virtual bool poll(TouchEvent& ev) = 0;
};

}  // namespace hal
