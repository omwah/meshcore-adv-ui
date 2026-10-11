// Simulated Cardputer keyboard: keys are pressed by their printed legend at
// a (row, col) of the real 4x14 matrix and go through the same keymap code
// as the device, so the simulator types and navigates exactly like it.
#pragma once
#include <stdint.h>
#include "hal/keyboard.h"
extern "C" {
#include "cardputer_keymap.h"
}
#include "report_diff.h"

namespace sim {

class SimKeyboard : public hal::Keyboard {
public:
  explicit SimKeyboard(const uint32_t* nowMs) : _now(nowMs) { cardputer_keymap_reset(&_st); }

  bool begin() override { return true; }
  void setTextMode(bool on) override { cardputer_keymap_text_mode(&_st, on); }
  bool poll(hal::KeyEvent& ev) override;

  // Schedule a matrix change at an absolute time.
  void at(uint32_t ms, int row, int col, bool down);
  // Find a printed legend ("a", ";", "enter", "fn", "esc"...) on the matrix.
  static bool find(const char* legend, int& row, int& col);
  // Find the key and shift state that types character `c` on the US layout.
  static bool findChar(char c, int& row, int& col, bool& shift);
  uint32_t lastScheduled() const { return _last; }

private:
  struct Ev {
    uint32_t ms;
    int8_t row, col;
    bool down;
  };
  static constexpr int Q = 256;
  Ev _q[Q];
  int _n = 0;
  uint32_t _last = 0;
  const uint32_t* _now;
  cardputer_keymap_t _st{};
  ReportDiff _diff;
};

}  // namespace sim
