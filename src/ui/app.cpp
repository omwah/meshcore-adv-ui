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
    const mc_font_t* bf = a.fMed();   // buttons
    const int bh_btn = bf->height + 2;
    int bx = r.x + 6, bw = r.w - 12;
    // The question in the medium font, or a smaller one if it doesn't fit;
    // lines that still don't fit stop above the buttons.
    const int pt = 5, gap = 3, pb = 4;   // padding: top, above the buttons, bottom
    const mc_font_t* f = bf;
    int lines = wrapText(question, f, bw - 12, nullptr, nullptr);
    const mc_font_t* smaller[] = {a.fSmall(), &font_small};   // at the large text size, small isn't
    for (const mc_font_t* g : smaller) {
      if (pt + lines * (f->height + 1) + gap + bh_btn + pb <= r.h || g == f) continue;
      f = g;
      lines = wrapText(question, f, bw - 12, nullptr, nullptr);
    }
    const int lh = f->height + 1;
    int bh = pt + lines * lh + gap + bh_btn + pb;
    if (bh > r.h) bh = r.h;
    int by = r.y + (r.h - bh) / 2;
    int btnY = by + bh - bh_btn - pb;
    c.fillRect(bx, by, bw, bh, C_ZEBRA);
    c.rect(bx, by, bw, bh, C_WARN);
    struct Ctx { Canvas* c; int x, y, lh, maxY; const mc_font_t* f; } ctx{&c, bx + 6, by + pt, lh, btnY - 1, f};
    wrapText(question, f, bw - 12, [](void* p, const char* s, int len, int line) {
      Ctx* x = (Ctx*)p;
      int y = x->y + line * x->lh;
      if (y + x->f->height > x->maxY) return;
      char tmp[64];
      if (len > 63) len = 63;
      memcpy(tmp, s, len);
      tmp[len] = 0;
      x->c->text(x->x, y, tmp, x->f, C_TEXT);
    }, &ctx);
    int btnW = Canvas::textWidth("Y: Yes", bf) + 16;
    c.pill(bx + bw / 4 - btnW / 2, btnY, btnW, bh_btn, C_OK);
    c.text(bx + bw / 4 - btnW / 2 + 8, btnY + 1, "Y: Yes", bf, C_BG);
    c.pill(bx + 3 * bw / 4 - btnW / 2, btnY, btnW, bh_btn, C_DIM);
    c.text(bx + 3 * bw / 4 - btnW / 2 + 8, btnY + 1, "N: No", bf, C_BG);
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
    const mc_font_t* f = a.fList();
    const int boxH = f->height + 6, top = r.y + 10;
    c.fillRect(r.x + 4, top, r.w - 8, boxH, C_ZEBRA);
    c.rect(r.x + 4, top, r.w - 8, boxH, C_ACCENT);
    ed.draw(a, c, r.x + 8, top + 3, r.w - 16, f, C_TEXT, secret);
    char n[16];
    snprintf(n, sizeof n, "%d/%d", ed.length(), maxLen);
    int ny = top + boxH + 4;
    c.text(r.x + r.w - 6 - Canvas::textWidth(n, a.fSmall()), ny, n, a.fSmall(), C_DIM);
    c.text(r.x + 6, ny, secret ? "hidden as you type" : "Fn + , / move cursor", a.fSmall(), C_DIM, -1,
           r.w - 20 - Canvas::textWidth(n, a.fSmall()));
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

//                    small         med           list          body          menu          head foot tab row chat menu meta
static const Metrics kMetrics[3] = {
  /* small  */ {&font_small,  &font_medium, &font_large,  &font_medium, &font_medium, 13, 10, 14, 16, 24, 16, 9},
  /* medium */ {&font_medium, &font_large,  &font_large,  &font_large,  &font_large,  17, 14, 18, 18, 32, 20, 13},
  /* large  */ {&font_large,  &font_large,  &font_xlarge, &font_xlarge, &font_xlarge, 17, 18, 18, 26, 34, 27, 17},
};

App::App(model::MeshModel& m, hal::Display& d, hal::Keyboard& k, hal::Sound* sound)
    : _m(m), _d(d), _k(k), _sound(sound), _mx(&kMetrics[1]) {}

void App::begin(Screen* root) {
  _d.begin();
  _k.begin();
  applyPrefs();
  _lastUnread = _m.unreadTotal();   // messages already waiting don't chime at boot
  _stack[0] = root;
  _depth = 1;
  entered();
}

void App::applyPrefs() {
  const model::Prefs& p = _m.prefs();
  _theme = p.theme < kThemeCount ? p.theme : 0;
  _mx = &kMetrics[p.textSize < 3 ? p.textSize : 1];
  _lastInput = _now;
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
  _marqueeStart = _now;
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
  _stack[0]->onHome(*this);
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
  return {0, _mx->head, Canvas::W, Canvas::H - _mx->head - _mx->foot};
}

int App::fitText(Canvas& c, int x, int y, const char* s, const mc_font_t* f, uint8_t fg, int maxw, bool scroll) {
  int tw = Canvas::textWidth(s, f);
  if (tw <= maxw || maxw <= 0) return c.text(x, y, s, f, fg);
  if (!scroll) return c.text(x, y, s, f, fg, -1, maxw);
  // Pause 1.2 s, slide at 40 px/s, pause 1.2 s at the end, repeat.
  const uint32_t PAUSE = 1200, MS_PER_PX = 25;
  uint32_t over = (uint32_t)(tw - maxw);
  uint32_t cycle = 2 * PAUSE + over * MS_PER_PX;
  uint32_t t = (_now - _marqueeStart) % cycle;
  int off = t < PAUSE ? 0 : t < PAUSE + over * MS_PER_PX ? (int)((t - PAUSE) / MS_PER_PX) : (int)over;
  c.textScrolled(x, y, s, f, fg, maxw, off);
  _animating = true;
  return x + maxw;
}

void App::wake() {
  if (!_asleep) return;
  _asleep = false;
  _d.sleep(false);
  _d.setBacklight(_m.prefs().brightness);
  _forceFlush = true;
  _dirty = true;
}

uint32_t App::loop(uint32_t now) {
  _now = now;
  hal::KeyEvent ev;
  while (_k.poll(ev)) {
    _lastInput = now;
    if (_asleep) {
      // The key that wakes the screen does nothing else.
      if (ev.down) wake();
      _in.flush();
      continue;
    }
    _in.feed(ev, now);
  }
  _in.tick(now);

  // A phone pairing over Bluetooth: show the PIN on top of everything.
  bool pairing = _m.blePairing();
  if (pairing != _pairing) {
    _pairing = pairing;
    _pairingHidden = false;
    _dirty = true;
    if (pairing) { _lastInput = now; wake(); }
  }

  // A new unread message: the screen, the light, both or neither, as set.
  uint16_t unread = _m.unreadTotal();
  if (unread > _lastUnread) {
    model::NotifyMode nm = _m.prefs().notify;
    if (nm == model::NotifyMode::Screen || nm == model::NotifyMode::ScreenLed) {
      _lastInput = now;
      wake();
    }
    if (nm == model::NotifyMode::Led || nm == model::NotifyMode::ScreenLed) flashLed();
    if (_sound && _m.prefs().sound) {
      static const hal::Tone kChime[] = {{1760, 70}, {0, 40}, {2349, 110}};
      _sound->play(kChime, 3);
    }
  }
  _lastUnread = unread;
  // Screen timeout.
  uint16_t timeout = _m.prefs().screenTimeoutS;
  if (!_asleep && timeout && now - _lastInput > timeout * 1000u) {
    _asleep = true;
    _d.setBacklight(0);
    _d.sleep(true);
  }

  // Text-mode can change within a screen (e.g. starting to compose).
  Key k;
  while (_in.next(k)) {
    Screen* before = top();
    if (_pairing && !_pairingHidden) {
      // The overlay takes keys: Esc hides it, anything else is ignored.
      if (k.code == K_ESC || k.code == K_LONG_ESC) _pairingHidden = true;
      _dirty = true;
      continue;
    }
    if (k.code == K_LONG_ESC) {
      popToRoot();
    } else if (k.code == K_MENU && !top()->fullscreen()) {
      ui::screens::openMainMenu(*this);
    } else {
      top()->onKey(*this, k);
    }
    _marqueeStart = now;
    if (top() == before) _k.setTextMode(top()->textMode(*this));
    _dirty = true;
  }

  if (top()->tick(*this, now)) _dirty = true;
  uint32_t rev = _m.revision();
  if (rev != _lastRev) { _lastRev = rev; _dirty = true; }
  uint32_t minute = _m.now() / 60;
  if (minute != _lastMinute) { _lastMinute = minute; _dirty = true; }
  if (_toastUntil && (int32_t)(now - _toastUntil) >= 0) { _toastUntil = 0; _dirty = true; }

  if (_dirty && !_asleep) render();
  // A moving marquee needs the next frame; only changed rows are sent.
  if (_animating && !_asleep) {
    _dirty = true;
    return 40;
  }

  uint32_t wait = _in.wakeIn(now);
  uint32_t led = ledTick(now);
  if (led && (!wait || led < wait)) wait = led;
  return wait ? wait : 50;
}

// ---- the status light

namespace {

// One pass through these, dimmed, then dark.
const uint8_t kLedColours[][3] = {{255, 0, 0},   {255, 110, 0}, {255, 220, 0}, {0, 255, 0},
                                  {0, 220, 255}, {0, 40, 255},  {170, 0, 255}};
constexpr int kLedSteps = sizeof kLedColours / sizeof kLedColours[0];
constexpr uint32_t kLedStepMs = 280;
constexpr uint8_t kLedLevel = 128;  // of 255: 32 was too dim to notice on the device

}  // namespace

void App::flashLed() {
  if (!_led) return;
  _ledStart = _now;
  _ledOn = true;
  _ledStep = -1;
  ledTick(_now);
}

uint32_t App::ledTick(uint32_t now) {
  if (!_led || !_ledOn) return 0;
  uint32_t t = now - _ledStart;
  int step = (int)(t / kLedStepMs);
  if (step >= kLedSteps) {
    _led->set(0, 0, 0);
    _ledOn = false;
    _ledStep = -1;
    return 0;
  }
  if (step != _ledStep) {
    _ledStep = step;
    const uint8_t* c = kLedColours[step];
    _led->set(c[0] * kLedLevel / 255, c[1] * kLedLevel / 255, c[2] * kLedLevel / 255);
  }
  return kLedStepMs - t % kLedStepMs;
}

void App::render() {
  _dirty = false;
  _animating = false;
  _c.noClip();
  _c.clear(C_BG);
  Screen* s = top();
  Rect r = content();
  if (!s->fullscreen()) drawHeader();
  _c.clip(r.x, r.y, r.w, r.h);
  s->draw(*this, _c, r);
  _c.noClip();
  if (!s->fullscreen()) drawFooter();
  if (_pairing && !_pairingHidden) drawPairing();
  _c.flush(_d, kThemes[_theme].pal, _forceFlush);
  _forceFlush = false;
}

void App::drawHeader() {
  const int H = _mx->head;
  _c.fillRect(0, 0, Canvas::W, H - 1, C_HEAD);
  _c.hline(0, H - 1, Canvas::W, C_ACCENT);
  const mc_font_t* f = fMed();
  const int ty = (H - 1 - f->height) / 2;   // text
  const int iy = (H - 1 - 8) / 2;           // 8 px icons

  // Right to left: clock, battery, GPS, BLE, unread.
  int x = Canvas::W - 2;
  char hm[8];
  fmtTime(hm, sizeof hm, _m.now());
  x -= Canvas::textWidth(hm, f);
  _c.text(x, ty, hm, f, C_HTXT);

  model::Diag d = _m.diag();
  x -= 17;
  drawBattery(_c, x, (H - 1 - 7) / 2, d.battPct, d.charging, C_HTXT);

  model::Gps g = _m.gps();
  if (g.enabled) {
    x -= 10;
    _c.bitmap(x, iy, icon::gps, 8, 8, g.fix ? C_OK : C_DIM);
  }
  if (_m.mode() == model::Mode::Remote) {
    // The link to the node: green when connected and read, amber on the way.
    model::Link l = _m.link();
    x -= 10;
    _c.bitmap(x, iy, icon::ble, 8, 8,
              l.state == model::LinkState::Ready ? (l.synced ? C_OK : C_ACCENT)
              : l.state == model::LinkState::Idle ? C_DIM : C_WARN);
  } else if (_m.bleAvailable() && _m.prefs().bleEnabled) {
    x -= 10;
    _c.bitmap(x, iy, icon::ble, 8, 8, _m.bleConnected() ? C_OK : C_DIM);
  }
  unsigned unread = _m.unreadTotal();
  if (unread) x = drawBadge(*this, x - 3, (H - 1 - (fSmall()->height + 2)) / 2, unread);

  fitText(_c, 3, ty, top()->title(*this), f, C_HTXT, x - 8, true);
}

void App::drawPairing() {
  const mc_font_t* f = fMed();
  const mc_font_t* big = &font_xlarge;
  char pin[12];
  snprintf(pin, sizeof pin, "%06u", (unsigned)_m.bleActivePin());
  int w = Canvas::W - 24, h = f->height * 2 + big->height + 18;
  int x = 12, y = (Canvas::H - h) / 2;
  _c.fillRect(x, y, w, h, C_ZEBRA);
  _c.rect(x, y, w, h, C_ACCENT);
  _c.bitmap(x + 6, y + 6 + (f->height - 8) / 2, icon::ble, 8, 8, C_ACCENT);
  _c.text(x + 18, y + 6, "Bluetooth pairing", f, C_TEXT, -1, w - 24);
  _c.text(x + 6, y + 8 + f->height, "Enter this PIN on the phone:", fSmall(), C_DIM, -1, w - 12);
  int pw = Canvas::textWidth(pin, big);
  _c.text(x + (w - pw) / 2, y + 10 + f->height + fSmall()->height, pin, big, C_ACCENT);
  _c.text(x + 6, y + h - fSmall()->height - 4, "esc: hide", fSmall(), C_DIM);
}

void App::drawFooter() {
  const int FH = _mx->foot;
  int y = Canvas::H - FH;
  _c.fillRect(0, y, Canvas::W, FH, C_ZEBRA);
  const mc_font_t* f = fSmall();
  const int ty = y + (FH - f->height) / 2;
  if (_toastUntil) {
    _c.fillRect(0, y, Canvas::W, FH, _toastCol);
    _c.text(4, ty, _toast, f, C_BG, -1, Canvas::W - 8);
    return;
  }
  const Hint* h = top()->hints(*this);
  int x = 3;
  for (; h && h->key; h++) {
    int kw = Canvas::textWidth(h->key, f) + 4;
    int lw = Canvas::textWidth(h->label, f);
    if (x + kw + lw + 2 > Canvas::W) break;
    _c.pill(x, y + 1, kw, FH - 2, C_DIM);
    _c.text(x + 2, ty, h->key, f, C_BG);
    x += kw + 2;
    _c.text(x, ty, h->label, f, C_TEXT);
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

int drawBadge(App& a, int xRight, int y, unsigned n, uint8_t col) {
  char b[8];
  snprintf(b, sizeof b, n > 99 ? "99+" : "%u", n);
  int w = Canvas::textWidth(b, a.fSmall()) + 6;
  int x = xRight - w;
  a.canvas().pill(x, y, w, a.fSmall()->height + 2, col);
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
