#include "sim_keyboard.h"
#include <string.h>

namespace sim {

// The printed layout, as in cardputer_keymap.h.
static const char* const kLegends[4][14] = {
    {"`", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "bksp"},
    {"tab", "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "[", "]", "\\"},
    {"fn", "shift", "a", "s", "d", "f", "g", "h", "j", "k", "l", ";", "'", "enter"},
    {"ctrl", "opt", "alt", "z", "x", "c", "v", "b", "n", "m", ",", ".", "/", "space"},
};
static const char kShifted[4][14] = {
    {'~', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '{', '}', '|'},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ':', '"', 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '<', '>', '?', 0},
};

bool SimKeyboard::find(const char* legend, int& row, int& col) {
  // Aliases for the navigation meaning of the arrow keys.
  static const char* const alias[][2] = {{"up", ";"}, {"down", "."}, {"left", ","}, {"right", "/"},
                                         {"esc", "`"}, {"menu", "opt"}, {"backspace", "bksp"}, {"ret", "enter"}};
  for (auto& a : alias)
    if (!strcmp(legend, a[0])) legend = a[1];
  for (int r = 0; r < 4; r++)
    for (int c = 0; c < 14; c++)
      if (!strcmp(kLegends[r][c], legend)) { row = r; col = c; return true; }
  return false;
}

bool SimKeyboard::findChar(char ch, int& row, int& col, bool& shift) {
  shift = false;
  if (ch == ' ') return find("space", row, col);
  if (ch >= 'A' && ch <= 'Z') {
    char l[2] = {(char)(ch - 'A' + 'a'), 0};
    shift = true;
    return find(l, row, col);
  }
  for (int r = 0; r < 4; r++)
    for (int c = 0; c < 14; c++) {
      if (kShifted[r][c] == ch) { row = r; col = c; shift = true; return true; }
      if (strlen(kLegends[r][c]) == 1 && kLegends[r][c][0] == ch) { row = r; col = c; shift = false; return true; }
    }
  return false;
}

void SimKeyboard::at(uint32_t ms, int row, int col, bool down) {
  if (_n == Q) return;
  // Keep the queue sorted by time (stable).
  int i = _n++;
  while (i > 0 && _q[i - 1].ms > ms) { _q[i] = _q[i - 1]; i--; }
  _q[i] = {ms, (int8_t)row, (int8_t)col, down};
  if (ms > _last) _last = ms;
}

bool SimKeyboard::poll(hal::KeyEvent& ev) {
  while (_n && _q[0].ms <= *_now) {
    Ev e = _q[0];
    memmove(_q, _q + 1, sizeof(Ev) * (--_n));
    kbd_report_t r;
    if (cardputer_keymap_event(&_st, e.row, e.col, e.down, &r)) _diff.update(r);
  }
  return _diff.next(ev);
}

}  // namespace sim
