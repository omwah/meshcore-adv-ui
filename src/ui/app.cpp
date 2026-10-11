#include "ui/app.h"
#include "ui/icons.h"
#include "ui/widgets.h"
#include "ui/screens/screens.h"
#include "ui/tz.h"
#include <stdio.h>
#include <string.h>

namespace ui {

// ---------------------------------------------------------------- dialogs

namespace {

class ConfirmScreen : public Screen {
public:
  char question[160];
  Action yes = nullptr;
  int arg = 0;
  const char* title(App&) override { return "Confirm"; }
  void draw(App& a, Canvas& c, const Rect& r) override {
    int bx = r.x + 8, bw = r.w - 16;
    int lines = wrapText(question, a.fMed(), bw - 16, nullptr, nullptr);
    int bh = lines * 13 + 32;
    if (bh > r.h - 4) bh = r.h - 4;
    int by = r.y + (r.h - bh) / 2;
    c.fillRect(bx, by, bw, bh, C_ZEBRA);
    c.rect(bx, by, bw, bh, C_WARN);
    struct Ctx { Canvas* c; int x, y; const mc_font_t* f; } ctx{&c, bx + 8, by + 8, a.fMed()};
    wrapText(question, a.fMed(), bw - 16, [](void* p, const char* s, int len, int line) {
      Ctx* x = (Ctx*)p;
      char tmp[64];
      if (len > 63) len = 63;
      memcpy(tmp, s, len);
      tmp[len] = 0;
      x->c->text(x->x, x->y + line * 13, tmp, x->f, C_TEXT);
    }, &ctx);
    c.pill(bx + 20, by + bh - 20, 70, 14, C_OK);
    c.text(bx + 30, by + bh - 19, "Y: Yes", a.fMed(), C_BG);
    c.pill(bx + bw - 90, by + bh - 20, 70, 14, C_DIM);
    c.text(bx + bw - 80, by + bh - 19, "N: No", a.fMed(), C_BG);
  }
  void onKey(App& a, const Key& k) override {
    if (k.code == 'y' || k.code == 'Y' || k.code == K_ENTER) {
      Action act = yes;
      int v = arg;
      a.pop();
      if (act) act(a, v);
    } else if (k.code == 'n' || k.code == 'N' || k.code == K_ESC) {
      a.pop();
    }
  }
  const Hint* hints(App&) override {
    static const Hint h[] = {{"y", "yes"}, {"n", "no"}, {nullptr, nullptr}};
    return h;
  }
};

class PromptScreen : public Screen {
public:
  char label[32];
  char text[model::TEXT_LEN];
  int maxLen = 31;
  bool secret = false;
  Action done = nullptr;
  int arg = 0;
  LineEditor ed;
  const char* title(App&) override { return label; }
  bool textMode(App&) override { return true; }
  void draw(App& a, Canvas& c, const Rect& r) override {
    c.fillRect(r.x + 4, r.y + 14, r.w - 8, 22, C_ZEBRA);
    c.rect(r.x + 4, r.y + 14, r.w - 8, 22, C_ACCENT);
    ed.draw(a, c, r.x + 8, r.y + 17, r.w - 16, a.fList(), C_TEXT, secret);
    char n[16];
    snprintf(n, sizeof n, "%d/%d", ed.length(), maxLen);
    c.text(r.x + r.w - 6 - Canvas::textWidth(n, a.fSmall()), r.y + 40, n, a.fSmall(), C_DIM);
    c.text(r.x + 6, r.y + 40, secret ? "hidden as you type" : "Fn + , / move the cursor", a.fSmall(), C_DIM);
  }
  void onKey(App& a, const Key& k) override {
    if (k.code == K_ESC) { a.pop(); return; }
    if (k.code == K_ENTER) {
      Action act = done;
      int v = arg;
      a.pop();
      if (act) act(a, v);
      return;
    }
    if (k.isChar() && ed.length() >= maxLen) return;
    if (ed.key(k)) a.invalidate();
  }
  const Hint* hints(App&) override {
    static const Hint h[] = {{"ent", "ok"}, {"esc", "cancel"}, {"fn", "arrows"}, {nullptr, nullptr}};
    return h;
  }
};

ConfirmScreen s_confirm;
PromptScreen s_prompt;

}  // namespace

void App::confirm(const char* q, Action yes, int arg) {
  snprintf(s_confirm.question, sizeof s_confirm.question, "%s", q);
  s_confirm.yes = yes;
  s_confirm.arg = arg;
  push(&s_confirm);
}

void App::prompt(const char* label, const char* initial, int maxLen, Action done, int arg, bool secret) {
  snprintf(s_prompt.label, sizeof s_prompt.label, "%s", label);
  snprintf(s_prompt.text, sizeof s_prompt.text, "%s", initial ? initial : "");
  s_prompt.maxLen = maxLen < (int)sizeof s_prompt.text - 1 ? maxLen : (int)sizeof s_prompt.text - 1;
  s_prompt.secret = secret;
  s_prompt.done = done;
  s_prompt.arg = arg;
  s_prompt.ed.attach(s_prompt.text, s_prompt.maxLen + 1);
  push(&s_prompt);
}

const char* App::promptText() const { return s_prompt.text; }

// ---------------------------------------------------------------- app

void App::begin(Screen* root) {
  _d.begin();
  _k.begin();
  applyPrefs();
  _stack[0] = root;
  _depth = 1;
  entered();
}

void App::applyPrefs() {
  const model::Prefs& p = _m.prefs();
  _theme = p.theme < kThemeCount ? p.theme : 0;
  setTimeZone(kZones[p.timeZone < kZoneCount ? p.timeZone : 0].posix);
  _in.setLockout(p.inputLockoutMs);
  _d.setBacklight(p.brightness);
  _forceFlush = true;
  _dirty = true;
}

void App::previewTheme(uint8_t idx) {
  _theme = idx < kThemeCount ? idx : 0;
  _forceFlush = true;
  _dirty = true;
}

void App::entered() {
  _in.flush();
  _k.setTextMode(top()->textMode(*this));
  top()->onEnter(*this);
  _dirty = true;
}

void App::push(Screen* s) {
  if (_depth < MAX_DEPTH) _stack[_depth++] = s;
  else _stack[_depth - 1] = s;
  entered();
}

void App::pop() {
  if (_depth > 1) _depth--;
  entered();
}

void App::replace(Screen* s) {
  _stack[_depth - 1] = s;
  entered();
}

void App::popToRoot() {
  _depth = 1;
  entered();
}

void App::toast(const char* text, uint8_t col) {
  snprintf(_toast, sizeof _toast, "%s", text);
  _toastCol = col;
  _toastUntil = _now + 2500;
  _dirty = true;
}

Rect App::content() const {
  if (_stack[_depth - 1] && _stack[_depth - 1]->fullscreen()) return {0, 0, Canvas::W, Canvas::H};
  return {0, HEAD_H, Canvas::W, Canvas::H - HEAD_H - FOOT_H};
}

uint32_t App::loop(uint32_t now) {
  _now = now;
  hal::KeyEvent ev;
  while (_k.poll(ev)) _in.feed(ev, now);
  _in.tick(now);

  // Text-mode can change within a screen (e.g. starting to compose).
  Key k;
  while (_in.next(k)) {
    Screen* before = top();
    if (k.code == K_LONG_ESC) {
      popToRoot();
    } else if (k.code == K_MENU && !top()->fullscreen()) {
      ui::screens::openMainMenu(*this);
    } else {
      top()->onKey(*this, k);
    }
    if (top() == before) _k.setTextMode(top()->textMode(*this));
    _dirty = true;
  }

  if (top()->tick(*this, now)) _dirty = true;
  uint32_t rev = _m.revision();
  if (rev != _lastRev) { _lastRev = rev; _dirty = true; }
  uint32_t minute = _m.now() / 60;
  if (minute != _lastMinute) { _lastMinute = minute; _dirty = true; }
  if (_toastUntil && (int32_t)(now - _toastUntil) >= 0) { _toastUntil = 0; _dirty = true; }

  if (_dirty) render();

  uint32_t wait = _in.wakeIn(now);
  return wait ? wait : 50;
}

void App::render() {
  _dirty = false;
  _c.noClip();
  _c.clear(C_BG);
  Screen* s = top();
  Rect r = content();
  if (!s->fullscreen()) drawHeader();
  _c.clip(r.x, r.y, r.w, r.h);
  s->draw(*this, _c, r);
  _c.noClip();
  if (!s->fullscreen()) drawFooter();
  _c.flush(_d, kThemes[_theme].pal, _forceFlush);
  _forceFlush = false;
}

void App::drawHeader() {
  _c.fillRect(0, 0, Canvas::W, HEAD_H - 1, C_HEAD);
  _c.hline(0, HEAD_H - 1, Canvas::W, C_ACCENT);
  const mc_font_t* f = fMed();

  // Right to left: clock, battery, GPS, BLE, unread.
  int x = Canvas::W - 2;
  char hm[8];
  fmtTime(hm, sizeof hm, _m.now());
  x -= Canvas::textWidth(hm, f);
  _c.text(x, 0, hm, f, C_HTXT);

  model::Diag d = _m.diag();
  x -= 17;
  drawBattery(_c, x, 3, d.battPct, d.charging, C_HTXT);

  model::Gps g = _m.gps();
  if (g.enabled) {
    x -= 10;
    _c.bitmap(x, 2, icon::gps, 8, 8, g.fix ? C_OK : C_DIM);
  }
  if (_m.bleAvailable() && _m.prefs().bleEnabled) {
    x -= 10;
    _c.bitmap(x, 2, icon::ble, 8, 8, _m.bleConnected() ? C_OK : C_DIM);
  }
  unsigned unread = _m.unreadTotal();
  if (unread) x = drawBadge(*this, x - 3, 1, unread);

  _c.text(3, 0, top()->title(*this), f, C_HTXT, -1, x - 8);
}

void App::drawFooter() {
  int y = Canvas::H - FOOT_H;
  _c.fillRect(0, y, Canvas::W, FOOT_H, C_ZEBRA);
  const mc_font_t* f = fSmall();
  if (_toastUntil) {
    _c.fillRect(0, y, Canvas::W, FOOT_H, _toastCol);
    _c.text(4, y + 1, _toast, f, C_BG, -1, Canvas::W - 8);
    return;
  }
  const Hint* h = top()->hints(*this);
  int x = 3;
  for (; h && h->key; h++) {
    int kw = Canvas::textWidth(h->key, f) + 4;
    int lw = Canvas::textWidth(h->label, f);
    if (x + kw + lw + 2 > Canvas::W) break;
    _c.pill(x, y + 1, kw, FOOT_H - 2, C_DIM);
    _c.text(x + 2, y + 1, h->key, f, C_BG);
    x += kw + 2;
    _c.text(x, y + 1, h->label, f, C_TEXT);
    x += lw + 7;
  }
}

// ---------------------------------------------------------------- helpers

void drawSignal(Canvas& c, int x, int y, int8_t snr_x4) {
  int bars = 0;
  uint8_t col = C_DIM;
  if (snr_x4 != INT8_MIN) {
    float snr = snr_x4 / 4.0f;
    bars = snr >= 5 ? 4 : snr >= 0 ? 3 : snr >= -7 ? 2 : snr >= -12 ? 1 : 0;
    col = snrColor(snr_x4);
  }
  for (int i = 0; i < 4; i++) {
    int h = 2 + i * 2;
    if (i < bars) c.fillRect(x + i * 3, y + 8 - h, 2, h, col);
    else c.fillRect(x + i * 3, y + 7, 2, 1, C_DIM);
  }
}

uint8_t snrColor(int8_t snr_x4) {
  if (snr_x4 == INT8_MIN) return C_DIM;
  float snr = snr_x4 / 4.0f;
  return snr >= 0 ? C_OK : snr >= -10 ? C_WARN : C_ERR;
}

void drawBattery(Canvas& c, int x, int y, uint8_t pct, bool charging, uint8_t fg) {
  c.rect(x, y, 13, 7, fg);
  c.vline(x + 13, y + 2, 3, fg);
  int w = (pct * 11 + 50) / 100;
  uint8_t col = charging ? C_OK : pct <= 15 ? C_ERR : pct <= 35 ? C_WARN : C_OK;
  c.fillRect(x + 1, y + 1, w, 5, col);
}

void drawScrollbar(Canvas& c, const Rect& r, int total, int first, int visible) {
  if (total <= visible) return;
  int x = r.x + r.w - 2;
  c.fillRect(x, r.y, 2, r.h, C_ZEBRA);
  int th = r.h * visible / total;
  if (th < 4) th = 4;
  int ty = r.y + (r.h - th) * first / (total - visible);
  c.fillRect(x, ty, 2, th, C_ACCENT);
}

int drawBadge(App& a, int xRight, int y, unsigned n) {
  char b[8];
  snprintf(b, sizeof b, n > 99 ? "99+" : "%u", n);
  int w = Canvas::textWidth(b, a.fSmall()) + 6;
  int x = xRight - w;
  a.canvas().pill(x, y, w, 10, C_BADGE);
  a.canvas().text(x + 3, y + 1, b, a.fSmall(), C_HTXT);
  return x;
}

uint8_t typeColor(model::NodeType t) {
  switch (t) {
    case model::NodeType::Repeater: return C_RPT;
    case model::NodeType::Room:
    case model::NodeType::Sensor: return C_ROOM;
    default: return C_CHAT;
  }
}

const uint8_t* typeIcon(model::NodeType t) {
  switch (t) {
    case model::NodeType::Repeater: return icon::repeater;
    case model::NodeType::Room: return icon::room;
    case model::NodeType::Sensor: return icon::sensor;
    default: return icon::person;
  }
}

void fmtAgo(char* out, size_t n, uint32_t now, uint32_t ts) {
  if (!ts || ts > now + 60) { snprintf(out, n, "-"); return; }
  fmtAge(out, n, now > ts ? now - ts : 0);
}

void fmtAge(char* out, size_t n, uint32_t d) {
  if (d < 60) snprintf(out, n, "%us", (unsigned)d);
  else if (d < 3600) snprintf(out, n, "%um", (unsigned)(d / 60));
  else if (d < 86400) snprintf(out, n, "%uh", (unsigned)(d / 3600));
  else snprintf(out, n, "%ud", (unsigned)(d / 86400));
}

void fmtTime(char* out, size_t n, uint32_t ts) {
  uint32_t s = toLocal(ts) % 86400;
  snprintf(out, n, "%02u:%02u", (unsigned)(s / 3600), (unsigned)(s / 60 % 60));
}

void fmtStamp(char* out, size_t n, uint32_t now, uint32_t ts) {
  uint32_t lt = toLocal(ts);
  if (lt / 86400 == toLocal(now) / 86400) { fmtTime(out, n, ts); return; }
  static const char* const kMon[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  int y;
  unsigned m, d;
  civilFromDays((int32_t)(lt / 86400), y, m, d);
  char hm[8];
  fmtTime(hm, sizeof hm, ts);
  snprintf(out, n, "%s %u %s", kMon[m - 1], d, hm);
}

void fmtDur(char* out, size_t n, uint32_t s) {
  if (s < 60) snprintf(out, n, "%us", (unsigned)s);
  else if (s < 3600) snprintf(out, n, "%um%02us", (unsigned)(s / 60), (unsigned)(s % 60));
  else if (s < 86400) snprintf(out, n, "%uh%02um", (unsigned)(s / 3600), (unsigned)(s / 60 % 60));
  else snprintf(out, n, "%ud%02uh", (unsigned)(s / 86400), (unsigned)(s / 3600 % 24));
}

void fmtSnr(char* out, size_t n, int8_t snr_x4) {
  if (snr_x4 == INT8_MIN) { snprintf(out, n, "--"); return; }
  int v = snr_x4 * 10 / 4;   // tenths
  snprintf(out, n, "%s%d.%d", v < 0 ? "-" : "", (v < 0 ? -v : v) / 10, (v < 0 ? -v : v) % 10);
}

int wrapText(const char* s, const mc_font_t* f, int w, LineFn fn, void* ctx) {
  int cols = w / f->width;
  if (cols < 1) cols = 1;
  int line = 0;
  while (*s) {
    // Find the longest prefix of <= cols code points, preferring a space break.
    const char* p = s;
    const char* lastSpace = nullptr;
    int n = 0;
    while (*p && *p != '\n' && n < cols) {
      if (*p == ' ') lastSpace = p;
      utf8Next(&p);
      n++;
    }
    const char* end = p;
    const char* next = p;
    if (*p == '\n' || *p == ' ') {
      next = p + 1;               // break exactly at a newline or space
    } else if (*p && lastSpace && lastSpace > s) {
      end = lastSpace;
      next = lastSpace + 1;
    }
    if (fn) fn(ctx, s, (int)(end - s), line);
    line++;
    s = next;
  }
  return line ? line : 1;
}

}  // namespace ui
