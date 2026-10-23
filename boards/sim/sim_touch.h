// Simulated touch panel: the script's taps and swipes, as timed finger events
// in screen pixels.
#pragma once
#include <stdint.h>
#include <string.h>
#include "hal/touch.h"

namespace sim {

class SimTouch : public hal::Touch {
public:
  explicit SimTouch(const uint32_t* nowMs) : _now(nowMs) {}
  bool begin() override { return true; }
  bool poll(hal::TouchEvent& ev) override {
    if (!_n || _q[0].ms > *_now) return false;
    ev = _q[0].ev;
    memmove(_q, _q + 1, sizeof(Ev) * (--_n));
    return true;
  }
  // Schedule a finger event at an absolute time (in time order).
  void at(uint32_t ms, int x, int y, hal::TouchEvent::Type t) {
    if (_n == Q) return;
    _q[_n++] = {ms, {(int16_t)x, (int16_t)y, t}};
  }

private:
  struct Ev {
    uint32_t ms;
    hal::TouchEvent ev;
  };
  static constexpr int Q = 128;
  Ev _q[Q];
  int _n = 0;
  const uint32_t* _now;
};

}  // namespace sim
