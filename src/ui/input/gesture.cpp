#include "ui/input/gesture.h"
#include <stdlib.h>

namespace ui {

void GestureReader::push(Gesture::Type t, int16_t dx, int16_t dy) {
  if (_n == Q) {
    // Full: merge drags rather than lose distance, drop anything else.
    if (t == Gesture::DRAG && _q[Q - 1].type == Gesture::DRAG) _q[Q - 1].dy += dy;
    return;
  }
  _q[_n++] = {t, _x0, _y0, dx, dy};
}

void GestureReader::feed(const hal::TouchEvent& ev, uint32_t now) {
  switch (ev.type) {
    case hal::TouchEvent::DOWN:
      _down = true;
      _state = PRESS;
      _x0 = _x = ev.x;
      _y0 = _y = _lastY = ev.y;
      _t0 = now;
      break;
    case hal::TouchEvent::MOVE: {
      if (!_down) break;
      _x = ev.x;
      _y = ev.y;
      int dx = _x - _x0, dy = _y - _y0;
      if (_state == PRESS && (abs(dx) > SLOP || abs(dy) > SLOP)) {
        _state = abs(dy) >= abs(dx) ? DRAGGING : SIDEWAYS;
        _lastY = _y0;
      }
      if (_state == DRAGGING && _y != _lastY) {
        push(Gesture::DRAG, 0, (int16_t)(_y - _lastY));
        _lastY = _y;
      }
      break;
    }
    case hal::TouchEvent::UP:
      if (!_down) break;
      _down = false;
      if (_state == PRESS && now - _t0 < LONG_MS) push(Gesture::TAP, 0, 0);
      else if (_state == SIDEWAYS && abs(ev.x - _x0) >= SWIPE) push(Gesture::SWIPE, (int16_t)(ev.x - _x0), (int16_t)(ev.y - _y0));
      _state = IDLE;
      break;
  }
}

void GestureReader::tick(uint32_t now) {
  if (_down && _state == PRESS && now - _t0 >= LONG_MS) {
    push(Gesture::LONG, 0, 0);
    _state = DONE;
  }
}

uint32_t GestureReader::wakeIn(uint32_t now) const {
  if (!_down || _state != PRESS) return 0;
  uint32_t at = _t0 + LONG_MS;
  return (int32_t)(at - now) > 0 ? at - now : 1;
}

bool GestureReader::next(Gesture& g) {
  if (!_n) return false;
  g = _q[0];
  for (int i = 1; i < _n; i++) _q[i - 1] = _q[i];
  _n--;
  return true;
}

}  // namespace ui
