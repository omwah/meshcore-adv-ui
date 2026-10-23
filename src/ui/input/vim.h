// Vim-style keys for moving around without arrow keys (the T-Deck has none).
// Only for screens that aren't taking text; letters there are commands:
//
//   j k h l        down up left right      gg G       first / last ({n}G: row n)
//   Ctrl-d Ctrl-u  half page down / up     Ctrl-f Ctrl-b   page down / up
//   /  n  N        search, next / previous match
//   gt gT {n}gt    next / previous / n-th tab
//   i a o          write a message         ZZ ZQ      send / discard the draft
//   q              back                    Ctrl-o     previous conversation
//   ]u [u          next / previous conversation with unread messages
//
// A number typed first repeats a motion (5j). Other keys pass through
// unchanged. A half-typed command (g, Z, ], [ or a count) is dropped after
// PENDING_MS or by Esc.
#pragma once
#include <stdint.h>
#include "ui/input/keys.h"

namespace ui {

class Vim {
public:
  static constexpr uint32_t PENDING_MS = 1500;

  // Feed one key; true with `out` set when it completes a command or passes
  // through, false while a command is still being typed.
  bool feed(const Key& k, uint32_t now, Key& out);
  // Drop a half-typed command once it has waited too long; true if dropped.
  bool expire(uint32_t now);
  void reset() { _prefix = 0; _count = 0; }
  // What has been typed of a command so far ("5", "g", "2g"), "" if nothing.
  const char* pending();
  bool busy() const { return _prefix || _count; }

private:
  char _prefix = 0;
  uint16_t _count = 0;
  uint32_t _since = 0;
  char _show[8] = {};
};

}  // namespace ui
