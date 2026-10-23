// Turns a finger's touch events into what the UI acts on:
//
//  * TAP: down and up within SLOP pixels and before LONG_MS.
//  * LONG: held LONG_MS within SLOP; sent while the finger is still down,
//    and its up is then ignored.
//  * DRAG: moved past SLOP mostly up or down; sent on every move after,
//    with the vertical distance since the last one (dy), for scrolling.
//  * SWIPE: moved past SWIPE pixels mostly sideways; sent on up, dx < 0
//    for a swipe to the left.
//
// x and y are where the finger went down.
#pragma once
#include <stdint.h>
#include "hal/touch.h"

namespace ui {

struct Gesture {
  enum Type : uint8_t { NONE, TAP, LONG, DRAG, SWIPE };
  Type type;
  int16_t x, y;
  int16_t dx, dy;
};

class GestureReader {
public:
  static constexpr int SLOP = 12, SWIPE = 40;
  static constexpr uint32_t LONG_MS = 450;

  void feed(const hal::TouchEvent& ev, uint32_t now);
  // A finger held still turns into LONG here.
  void tick(uint32_t now);
  bool next(Gesture& g);
  // Forget the finger now down (the screen changed under it).
  void cancel() { _state = _down ? DONE : IDLE; _n = 0; }
  bool down() const { return _down; }
  // Milliseconds until tick() may produce something (0 = nothing pending).
  uint32_t wakeIn(uint32_t now) const;

private:
  enum State : uint8_t { IDLE, PRESS, DRAGGING, SIDEWAYS, DONE };
  void push(Gesture::Type t, int16_t dx, int16_t dy);

  State _state = IDLE;
  bool _down = false;
  int16_t _x0 = 0, _y0 = 0, _lastY = 0, _x = 0, _y = 0;
  uint32_t _t0 = 0;
  static constexpr int Q = 8;
  Gesture _q[Q];
  int _n = 0;
};

}  // namespace ui
