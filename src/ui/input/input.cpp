#include "ui/input/input.h"

namespace ui {

// HID usage -> ASCII, US layout: [usage - 0x04] = {plain, shifted}.
static const char kAscii[][2] = {
  {'a','A'},{'b','B'},{'c','C'},{'d','D'},{'e','E'},{'f','F'},{'g','G'},{'h','H'},{'i','I'},
  {'j','J'},{'k','K'},{'l','L'},{'m','M'},{'n','N'},{'o','O'},{'p','P'},{'q','Q'},{'r','R'},
  {'s','S'},{'t','T'},{'u','U'},{'v','V'},{'w','W'},{'x','X'},{'y','Y'},{'z','Z'},
  {'1','!'},{'2','@'},{'3','#'},{'4','$'},{'5','%'},{'6','^'},{'7','&'},{'8','*'},{'9','('},{'0',')'},
  {0,0},{0,0},{0,0},{0,0},{' ',' '},                       // 0x28-0x2c: Enter Esc Bksp Tab Space
  {'-','_'},{'=','+'},{'[','{'},{']','}'},{'\\','|'},{0,0},  // 0x2d-0x32
  {';',':'},{'\'','"'},{'`','~'},{',','<'},{'.','>'},{'/','?'}, // 0x33-0x38
};

static uint8_t uiMods(uint8_t m) {
  return (uint8_t)(((m & 0x11) ? KM_CTRL : 0) | ((m & 0x22) ? KM_SHIFT : 0) | ((m & 0x44) ? KM_ALT : 0));
}

uint16_t Input::translate(uint8_t u, uint8_t mods) {
  switch (u) {
    case 0x28: return K_ENTER;
    case 0x29: return K_ESC;
    case 0x2a: return K_BKSP;
    case 0x2b: return K_TAB;
    case 0x4c: return K_DEL;
    case 0x4b: return K_PGUP;
    case 0x4e: return K_PGDN;
    case 0x65: return K_MENU;
    case 0x4a: return K_HOME;
    case 0x4d: return K_END;
    case 0x4f: return K_RIGHT;
    case 0x50: return K_LEFT;
    case 0x51: return K_DOWN;
    case 0x52: return K_UP;
  }
  if (u >= 0x3a && u <= 0x45) return (uint16_t)(K_F1 + (u - 0x3a));
  if (u >= 0x04 && u <= 0x38) {
    char c = kAscii[u - 0x04][(mods & 0x22) ? 1 : 0];
    if (c) return (uint16_t)c;
  }
  return K_NONE;
}

static bool repeats(uint16_t code) {
  return code == K_UP || code == K_DOWN || code == K_LEFT || code == K_RIGHT ||
         code == K_BKSP || code == K_DEL;
}

void Input::push(uint16_t code, uint8_t mods, bool repeat) {
  if (_n == Q) return;  // full: drop new keys rather than old ones
  Key& k = _q[(_head + _n) % Q];
  k.code = code;
  k.mods = mods;
  k.repeat = repeat;
  _n++;
}

void Input::feed(const hal::KeyEvent& ev, uint32_t now) {
  uint16_t code = translate(ev.usage, ev.mods);
  if (ev.down) {
    if (ev.usage == _lastUsage && now - _lastDown < _lockout) return;  // chatter
    _lastUsage = ev.usage;
    _lastDown = now;
    _heldUsage = ev.usage;
    _heldMods = ev.mods;
    _heldSwallowed = false;
    _heldSince = now;
    _nextRepeat = now + REPEAT_DELAY;
    _longEscSent = false;
    if (code != K_ESC && code != K_NONE) push(code, uiMods(ev.mods), false);
  } else if (ev.usage == _heldUsage) {
    if (code == K_ESC && !_longEscSent && !_heldSwallowed) push(K_ESC, uiMods(_heldMods), false);
    _heldUsage = 0;
    _heldSwallowed = false;
  }
}

void Input::tick(uint32_t now) {
  if (!_heldUsage || _heldSwallowed) return;
  uint16_t code = translate(_heldUsage, _heldMods);
  if (code == K_ESC) {
    if (!_longEscSent && now - _heldSince >= LONG_ESC) {
      _longEscSent = true;
      push(K_LONG_ESC, 0, false);
    }
  } else if (repeats(code) && (int32_t)(now - _nextRepeat) >= 0) {
    push(code, uiMods(_heldMods), true);
    _nextRepeat = now + REPEAT_EVERY;
  }
}

uint32_t Input::wakeIn(uint32_t now) const {
  if (!_heldUsage || _heldSwallowed) return 0;
  uint16_t code = translate(_heldUsage, _heldMods);
  if (code == K_ESC && !_longEscSent) {
    uint32_t at = _heldSince + LONG_ESC;
    return (int32_t)(at - now) > 0 ? at - now : 1;
  }
  if (repeats(code)) return (int32_t)(_nextRepeat - now) > 0 ? _nextRepeat - now : 1;
  return 0;
}

bool Input::next(Key& k) {
  if (!_n) return false;
  k = _q[_head];
  _head = (uint8_t)((_head + 1) % Q);
  _n--;
  return true;
}

void Input::flush() {
  _n = 0;
  if (_heldUsage) _heldSwallowed = true;
}

}  // namespace ui
