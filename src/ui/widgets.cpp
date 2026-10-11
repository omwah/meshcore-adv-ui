#include "ui/widgets.h"
#include "ui/icons.h"
#include <stdio.h>
#include <string.h>

namespace ui {

// ---------------------------------------------------------------- ListNav

void ListNav::clamp(int count, int visible) {
  if (sel >= count) sel = count - 1;
  if (sel < 0) sel = 0;
  if (sel < top) top = sel;
  if (sel >= top + visible) top = sel - visible + 1;
  if (top > count - visible) top = count - visible;
  if (top < 0) top = 0;
}

bool ListNav::key(const Key& k, int count, int visible) {
  if (count <= 0) return false;
  int old = sel;
  switch (k.code) {
    // Lists stop at their ends instead of wrapping: one press too many must
    // never throw the cursor to the opposite end of a menu.
    case K_UP:
      if (sel > 0) sel--;
      break;
    case K_DOWN:
      if (sel < count - 1) sel++;
      break;
    case K_PGUP: sel -= visible; break;
    case K_PGDN: sel += visible; break;
    case K_HOME: sel = 0; break;
    case K_END: sel = count - 1; break;
    default: return false;
  }
  (void)old;
  clamp(count, visible);
  return true;
}

// ---------------------------------------------------------------- MenuScreen

static const int ROW_H = 16;

void MenuScreen::onEnter(App& app) {
  if (!_fresh) return;   // returning from a child screen: keep the cursor
  _fresh = false;
  // Land on the first row that can be used.
  int n = count(app);
  for (int i = 0; i < n; i++) {
    MenuItem it;
    memset(&it, 0, sizeof it);
    it.enabled = true;
    item(app, i, it);
    if (!it.header && it.enabled) { _nav.sel = i; return; }
  }
}

void MenuScreen::draw(App& app, Canvas& c, const Rect& r0) {
  Rect r = r0;
  int hh = drawHead(app, c, r);
  r.y += hh;
  r.h -= hh;
  int n = count(app);
  int vis = r.h / ROW_H;
  if (vis < 1) vis = 1;
  _vis = vis;
  _nav.clamp(n, vis);
  const mc_font_t* f = app.fMed();
  for (int row = 0; row < vis && _nav.top + row < n; row++) {
    int i = _nav.top + row;
    MenuItem it;
    memset(&it, 0, sizeof it);
    it.enabled = true;
    it.valueCol = C_ACCENT;
    item(app, i, it);
    int y = r.y + row * ROW_H;
    if (it.header) {
      c.text(r.x + 4, y + 3, it.label, app.fSmall(), C_ACCENT);
      c.hline(r.x + 6 + Canvas::textWidth(it.label, app.fSmall()), y + 7, r.w - 12 - Canvas::textWidth(it.label, app.fSmall()), C_DIM);
      continue;
    }
    bool sel = i == _nav.sel;
    if (sel) {
      c.fillRect(r.x, y, r.w - 3, ROW_H, C_SEL);
      c.fillRect(r.x, y, 2, ROW_H, C_ACCENT);
    } else if (row & 1) {
      c.fillRect(r.x, y, r.w - 3, ROW_H, C_ZEBRA);
    }
    uint8_t fg = it.enabled ? C_TEXT : C_DIM;
    int vw = it.value[0] ? Canvas::textWidth(it.value, f) : 0;
    c.text(r.x + 6, y + 2, it.label, f, fg, -1, r.w - 16 - vw);
    if (vw) {
      int vx = r.x + r.w - 8 - vw;
      if (sel && it.enabled && it.choice) {
        // Show that Left/Right change this value.
        c.bitmap(vx - 9, y + 4, icon::arrowL, 8, 8, C_DIM);
      }
      c.text(vx, y + 2, it.value, f, it.enabled ? it.valueCol : C_DIM);
    }
  }
  drawScrollbar(c, r, n, _nav.top, vis);
}

void MenuScreen::onKey(App& app, const Key& k) {
  int n = count(app);
  int vis = _vis;
  if (k.code == K_ESC) { app.pop(); return; }
  if (_nav.key(k, n, vis)) {
    // Never rest on a section header: keep going the way the cursor moved,
    // and turn back at the ends.
    int dir = (k.code == K_UP || k.code == K_PGUP || k.code == K_END) ? -1 : 1;
    for (int guard = 0; guard < 2 * n; guard++) {
      MenuItem it;
      memset(&it, 0, sizeof it);
      item(app, _nav.sel, it);
      if (!it.header) break;
      if (_nav.sel + dir < 0 || _nav.sel + dir >= n) dir = -dir;
      _nav.sel += dir;
    }
    _nav.clamp(n, vis);
    return;
  }
  MenuItem it;
  memset(&it, 0, sizeof it);
  it.enabled = true;
  item(app, _nav.sel, it);
  if (!it.enabled || it.header) return;
  if (k.code == K_ENTER) activate(app, _nav.sel);
  else if (k.code == K_LEFT) adjust(app, _nav.sel, -1);
  else if (k.code == K_RIGHT) adjust(app, _nav.sel, +1);
}

const Hint* MenuScreen::hints(App&) {
  static const Hint h[] = {{"^v", "move"}, {"ent", "select"}, {"<>", "change"}, {"esc", "back"}, {nullptr, nullptr}};
  return h;
}

// ---------------------------------------------------------------- LineEditor

static bool isCont(char c) { return (c & 0xC0) == 0x80; }

bool LineEditor::key(const Key& k) {
  int len = length();
  if (k.code == K_LEFT) {
    if (_cur > 0) { do _cur--; while (_cur > 0 && isCont(_buf[_cur])); }
    return true;
  }
  if (k.code == K_RIGHT) {
    if (_cur < len) { do _cur++; while (_cur < len && isCont(_buf[_cur])); }
    return true;
  }
  if (k.code == K_HOME) { _cur = 0; return true; }
  if (k.code == K_END) { _cur = len; return true; }
  if (k.code == K_BKSP) {
    if (_cur == 0) return true;
    int s = _cur;
    do s--; while (s > 0 && isCont(_buf[s]));
    memmove(_buf + s, _buf + _cur, len - _cur + 1);
    _cur = s;
    return true;
  }
  if (k.code == K_DEL) {
    if (_cur >= len) return true;
    int e = _cur;
    do e++; while (e < len && isCont(_buf[e]));
    memmove(_buf + _cur, _buf + e, len - e + 1);
    return true;
  }
  if (k.isChar()) {
    if (len + 1 >= _cap) return true;
    memmove(_buf + _cur + 1, _buf + _cur, len - _cur + 1);
    _buf[_cur++] = (char)k.code;
    return true;
  }
  return false;
}

void LineEditor::draw(App& app, Canvas& c, int x, int y, int w, const mc_font_t* f, uint8_t fg, bool secret) {
  // Count code points before the cursor and in total.
  int before = 0, total = 0;
  for (const char* p = _buf; *p;) {
    if (p - _buf < _cur) before++;
    utf8Next(&p);
    total++;
  }
  int cols = w / f->width - 1;
  int first = before > cols ? before - cols : 0;
  const char* p = _buf;
  for (int i = 0; i < first; i++) utf8Next(&p);
  int cx = x;
  for (int i = first; i < total && cx + f->width <= x + w; i++) {
    const char* q = p;
    utf8Next(&q);
    char g[5] = {0};
    if (secret) g[0] = '*';
    else memcpy(g, p, q - p);
    c.text(cx, y, g, f, fg);
    cx += f->width;
    p = q;
  }
  // Blinking caret.
  if ((app.nowMs() / 500) & 1) return;
  int caretX = x + (before - first) * f->width;
  c.fillRect(caretX, y, 1, f->height, C_ACCENT);
}

void LineEditor::drawWrapped(App& app, Canvas& c, const Rect& r, const mc_font_t* f, uint8_t fg) {
  int cols = r.w / f->width;
  int rows = r.h / f->height;
  // Hard-wrap by columns (no word wrap) so the caret position is exact.
  int before = 0, total = 0;
  for (const char* p = _buf; *p;) {
    if (p - _buf < _cur) before++;
    utf8Next(&p);
    total++;
  }
  int caretLine = before / cols;
  int lines = total / cols + 1;
  int firstLine = lines > rows ? lines - rows : 0;
  if (caretLine < firstLine) firstLine = caretLine;
  const char* p = _buf;
  for (int i = 0; i < firstLine * cols && *p; i++) utf8Next(&p);
  for (int i = firstLine * cols; i < total; i++) {
    int line = i / cols - firstLine;
    if (line >= rows) break;
    const char* q = p;
    utf8Next(&q);
    char g[5] = {0};
    memcpy(g, p, q - p);
    c.text(r.x + (i % cols) * f->width, r.y + line * f->height, g, f, fg);
    p = q;
  }
  if ((app.nowMs() / 500) & 1) return;
  int cl = caretLine - firstLine;
  if (cl >= 0 && cl < rows)
    c.fillRect(r.x + (before % cols) * f->width, r.y + cl * f->height, 1, f->height, C_ACCENT);
}

}  // namespace ui
