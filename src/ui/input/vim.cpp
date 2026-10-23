#include "ui/input/vim.h"
#include <stdio.h>

namespace ui {

static Key make(uint16_t code, const Key& from, uint16_t count = 0) {
  Key k = from;
  k.code = code;
  k.mods = 0;
  k.count = count;
  return k;
}

bool Vim::expire(uint32_t now) {
  if (!busy() || now - _since < PENDING_MS) return false;
  reset();
  return true;
}

const char* Vim::pending() {
  _show[0] = 0;
  if (_count && _prefix) snprintf(_show, sizeof _show, "%u%c", _count, _prefix);
  else if (_count) snprintf(_show, sizeof _show, "%u", _count);
  else if (_prefix) snprintf(_show, sizeof _show, "%c", _prefix);
  return _show;
}

bool Vim::feed(const Key& k, uint32_t now, Key& out) {
  expire(now);
  const uint16_t count = _count;
  if (k.code == K_ESC && busy()) { reset(); return false; }   // Esc cancels the command only
  if (k.mods & KM_CTRL) {
    reset();
    switch (k.code) {
      case 'd': out = make(K_HALF_DN, k); return true;
      case 'u': out = make(K_HALF_UP, k); return true;
      case 'f': out = make(K_PGDN, k); return true;
      case 'b': out = make(K_PGUP, k); return true;
      case 'o': out = make(K_JUMP_BACK, k); return true;
    }
    out = k;
    return true;
  }
  if (!k.isChar()) { reset(); out = k; return true; }
  const char c = (char)k.code;

  if (_prefix) {
    char p = _prefix;
    reset();
    uint16_t code = K_NONE;
    if (p == 'g' && c == 'g') code = count ? K_GOTO : K_HOME;
    else if (p == 'g' && c == 't') code = count ? K_TAB_N : K_TAB_NEXT;
    else if (p == 'g' && c == 'T') code = K_TAB_PREV;
    else if (p == 'Z' && c == 'Z') code = K_SEND;
    else if (p == 'Z' && c == 'Q') code = K_DISCARD;
    else if (p == ']' && c == 'u') code = K_NEXT_UNREAD;
    else if (p == '[' && c == 'u') code = K_PREV_UNREAD;
    if (code == K_NONE) return false;   // not a command: dropped, as vim does
    out = make(code, k, count);
    return true;
  }
  if ((c >= '1' && c <= '9') || (c == '0' && _count)) {
    uint32_t n = _count * 10u + (uint32_t)(c - '0');
    _count = (uint16_t)(n > 999 ? 999 : n);
    _since = now;
    return false;
  }
  switch (c) {
    case 'g': case 'Z': case ']': case '[':
      _prefix = c;
      _since = now;
      return false;
  }
  reset();
  switch (c) {
    case 'j': out = make(K_DOWN, k, count); return true;
    case 'k': out = make(K_UP, k, count); return true;
    case 'h': out = make(K_LEFT, k, count); return true;
    case 'l': out = make(K_RIGHT, k, count); return true;
    case 'G': out = make(count ? K_GOTO : K_END, k, count); return true;
    case '/': out = make(K_SEARCH, k); return true;
    case 'n': out = make(K_NEXT_MATCH, k, count); return true;
    case 'N': out = make(K_PREV_MATCH, k, count); return true;
    case 'i': case 'a': case 'o': out = make(K_COMPOSE, k); return true;
    case 'q': out = make(K_ESC, k); return true;
  }
  out = k;
  return true;
}

}  // namespace ui
