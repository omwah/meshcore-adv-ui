// The UI: a stack of screens drawn into one 4-bpp canvas under a status
// header and above a key-hint footer. Owns input filtering and theme.
//
// Everything is statically allocated; screens are singletons that keep only
// their own cursor state and read the model when they draw.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "ui/gfx/canvas.h"
#include "ui/input/input.h"
#include "ui/theme.h"
#include "model/model.h"
#include "hal/display.h"
#include "hal/keyboard.h"
#include "gen/fonts.h"

namespace ui {

class App;

struct Rect {
  int x, y, w, h;
};

struct Hint {
  const char* key;    // short key name ("ent", "tab", "<>", "^v", "esc")
  const char* label;
};

class Screen {
public:
  virtual ~Screen() {}
  virtual const char* title(App& app) = 0;
  virtual void draw(App& app, Canvas& c, const Rect& r) = 0;
  virtual void onKey(App& app, const Key& k) = 0;
  // Key hints for the footer, terminated by {nullptr, nullptr}.
  virtual const Hint* hints(App& app) { (void)app; return nullptr; }
  virtual bool textMode(App& app) { (void)app; return false; }
  virtual void onEnter(App& app) { (void)app; }
  // Called every loop; return true to request a redraw.
  virtual bool tick(App& app, uint32_t now) { (void)app; (void)now; return false; }
  // Screens that draw the full display (boot, lock) hide header and footer.
  virtual bool fullscreen() { return false; }
};

using Action = void (*)(App& app, int arg);

class App {
public:
  static constexpr int HEAD_H = 13;
  static constexpr int FOOT_H = 10;
  static constexpr int MAX_DEPTH = 8;

  App(model::MeshModel& m, hal::Display& d, hal::Keyboard& k) : _m(m), _d(d), _k(k) {}

  void begin(Screen* root);
  // Run one iteration: read keys, update, redraw if needed. Returns the
  // milliseconds until the next iteration needs to run (for idle sleep).
  uint32_t loop(uint32_t nowMs);

  // Navigation. Every change flushes pending input.
  void push(Screen* s);
  void pop();
  void replace(Screen* s);
  void popToRoot();
  Screen* top() { return _stack[_depth - 1]; }

  // Transient message over the footer.
  void toast(const char* text, uint8_t col = C_ACCENT);
  // Modal yes/no; `yes` runs with `arg` when confirmed.
  void confirm(const char* question, Action yes, int arg = 0);
  // Modal single-line text entry; `done` runs with the text in promptText().
  void prompt(const char* label, const char* initial, int maxLen, Action done, int arg = 0, bool secret = false);
  const char* promptText() const;

  void invalidate() { _dirty = true; }
  void applyPrefs();               // theme, lockout, brightness from model prefs
  void previewTheme(uint8_t idx);  // apply a theme without saving it

  model::MeshModel& m() { return _m; }
  Canvas& canvas() { return _c; }
  uint32_t nowMs() const { return _now; }
  const Theme& theme() const { return kThemes[_theme]; }

  const mc_font_t* fList() const { return &font_large; }
  const mc_font_t* fBody() const { return _m.prefs().largeText ? &font_large : &font_medium; }
  const mc_font_t* fSmall() const { return &font_small; }
  const mc_font_t* fMed() const { return &font_medium; }

  Rect content() const;

private:
  void render();
  void drawHeader();
  void drawFooter();
  void entered();

  model::MeshModel& _m;
  hal::Display& _d;
  hal::Keyboard& _k;
  Canvas _c;
  Input _in;
  Screen* _stack[MAX_DEPTH] = {};
  int _depth = 0;
  uint8_t _theme = 0;
  bool _dirty = true, _forceFlush = true;
  uint32_t _now = 0, _lastRev = 0, _lastMinute = 0;
  char _toast[48] = {};
  uint8_t _toastCol = C_ACCENT;
  uint32_t _toastUntil = 0;
};

// Shared drawing helpers.
void drawSignal(Canvas& c, int x, int y, int8_t snr_x4);       // 4 bars, 9x8
void drawBattery(Canvas& c, int x, int y, uint8_t pct, bool charging, uint8_t fg);  // 14x7
void drawScrollbar(Canvas& c, const Rect& r, int total, int first, int visible);
// Unread-count pill ending at xRight; returns its left edge.
int drawBadge(App& a, int xRight, int y, unsigned n);
uint8_t typeColor(model::NodeType t);
const uint8_t* typeIcon(model::NodeType t);
void fmtAgo(char* out, size_t n, uint32_t now, uint32_t ts);    // "3m", "2h", "5d"
void fmtAge(char* out, size_t n, uint32_t secs);                // same, from seconds
void fmtTime(char* out, size_t n, uint32_t ts);                 // "HH:MM"
void fmtDur(char* out, size_t n, uint32_t secs);                // "3m05s", "2h10m", "4d03h"
void fmtSnr(char* out, size_t n, int8_t snr_x4);                 // "-7.5"
uint8_t snrColor(int8_t snr_x4);
// Word-wrap UTF-8 text to `w` pixels. Calls back per line; returns line count.
using LineFn = void (*)(void* ctx, const char* s, int len, int line);
int wrapText(const char* s, const mc_font_t* f, int w, LineFn fn, void* ctx);

}  // namespace ui
