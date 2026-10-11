// A conversation (DM, room or channel): messages bottom-anchored like a chat
// app, a meta line per message (sender, time, delivery or signal), and a
// compose box. Up/Down scroll; Enter or typing starts composing.
#include "ui/screens/screens.h"
#include "ui/widgets.h"
#include "ui/icons.h"
#include <stdio.h>
#include <string.h>

namespace ui { namespace screens {

namespace {

constexpr int META_H = 9;     // small font line
constexpr int GAP = 3;
constexpr int COMPOSE_H = 30;

struct DrawCtx {
  Canvas* c;
  const mc_font_t* f;
  int x, y;
  uint8_t col;
};

void drawLine(void* p, const char* s, int len, int line) {
  DrawCtx* d = (DrawCtx*)p;
  char tmp[model::TEXT_LEN];
  if (len >= (int)sizeof tmp) len = sizeof tmp - 1;
  memcpy(tmp, s, len);
  tmp[len] = 0;
  d->c->text(d->x, d->y + line * d->f->height, tmp, d->f, d->col);
}

class ConversationScreen : public Screen {
public:
  model::ThreadId id;

  const char* title(App& a) override {
    if (id.kind == model::ThreadKind::Channel) {
      model::Channel ch;
      for (int i = 0, n = a.m().channelCount(); i < n; i++)
        if (a.m().channel(i, ch) && ch.idx == id.idx) { snprintf(_title, sizeof _title, "%s", ch.name); return _title; }
    } else {
      model::Contact ct;
      if (a.m().contact(id.idx, ct)) { snprintf(_title, sizeof _title, "%s", ct.name); return _title; }
    }
    return "Chat";
  }

  void onEnter(App& a) override {
    a.m().markRead(id);
    _readRev = a.m().revision();
  }

  bool textMode(App&) override { return _composing; }

  bool tick(App& a, uint32_t now) override {
    // New messages arriving while open are read; keep the view at the bottom
    // unless the user scrolled up.
    if (a.m().revision() != _readRev) {
      a.m().markRead(id);
      _readRev = a.m().revision();
      return true;
    }
    // Blink the caret.
    if (_composing && (now / 500) != _blink) { _blink = now / 500; return true; }
    return false;
  }

  void draw(App& a, Canvas& c, const Rect& r) override {
    Rect mr = r;
    if (_composing) mr.h -= COMPOSE_H;
    int n = a.m().messageCount(id);
    const mc_font_t* f = a.fBody();
    int textW = mr.w - 14;

    if (n == 0) {
      const char* msg = "No messages yet. Press Enter to write.";
      DrawCtx d{&c, a.fMed(), r.x + 10, mr.y + 30, C_DIM};
      wrapText(msg, a.fMed(), r.w - 20, drawLine, &d);
    }

    // Walk from the newest message up. _scroll counts pixels hidden below.
    int bottom = mr.y + mr.h + _scroll;
    int total = 0;
    for (int i = n - 1; i >= 0; i--) {
      model::Message m;
      if (!a.m().message(id, i, m)) continue;
      int lines = wrapText(m.text, f, textW, nullptr, nullptr);
      int h = META_H + lines * f->height + GAP;
      total += h;
      int top = bottom - h;
      if (top < mr.y + mr.h && bottom > mr.y) drawMessage(a, c, m, mr, top, f, textW);
      bottom = top;
    }
    _maxScroll = total > mr.h ? total - mr.h : 0;
    if (_scroll > _maxScroll) _scroll = _maxScroll;
    if (_scroll > 0) {
      // "More below" marker.
      c.fillRect(mr.x + mr.w / 2 - 12, mr.y + mr.h - 4, 24, 3, C_ACCENT);
    }
    drawScrollbar(c, mr, total, total - mr.h - _scroll, mr.h);

    if (_composing) drawCompose(a, c, r);
  }

  void onKey(App& a, const Key& k) override {
    if (_composing) { composeKey(a, k); return; }
    Rect r = a.content();
    switch (k.code) {
      case K_ESC: a.pop(); return;
      case K_UP: _scroll += 12; if (_scroll > _maxScroll) _scroll = _maxScroll; return;
      case K_DOWN: _scroll -= 12; if (_scroll < 0) _scroll = 0; return;
      case K_PGUP: _scroll += r.h - 20; if (_scroll > _maxScroll) _scroll = _maxScroll; return;
      case K_PGDN: _scroll -= r.h - 20; if (_scroll < 0) _scroll = 0; return;
      case K_END: _scroll = 0; return;
      case K_HOME: _scroll = _maxScroll; return;
      case K_TAB:
        if (id.kind == model::ThreadKind::Channel) openChannel(a, id.idx);
        else openContact(a, id.idx);
        return;
      case K_ENTER: startCompose(a); return;
    }
    if (k.isChar()) {
      startCompose(a);
      composeKey(a, k);
    }
  }

  const Hint* hints(App&) override {
    static const Hint view[] = {{"ent", "write"}, {"^v", "scroll"}, {"tab", "info"}, {"esc", "back"}, {nullptr, nullptr}};
    static const Hint comp[] = {{"ent", "send"}, {"esc", "stop"}, {"fn+,/", "cursor"}, {nullptr, nullptr}};
    return _composing ? comp : view;
  }

  void reset() {
    _scroll = 0;
    _composing = false;
    _draft[0] = 0;
  }

private:
  void startCompose(App& a) {
    (void)a;
    _composing = true;
    _scroll = 0;
    _ed.attach(_draft, maxLen(a) + 1);
  }

  int maxLen(App& a) {
    // Channel messages carry "name: " inside the payload.
    int lim = model::TEXT_LEN - 1;
    if (id.kind == model::ThreadKind::Channel) lim -= (int)strlen(a.m().prefs().nodeName) + 2;
    return lim;
  }

  void composeKey(App& a, const Key& k) {
    if (k.code == K_ESC) { _composing = false; return; }
    if (k.code == K_ENTER) {
      if (!_draft[0]) { _composing = false; return; }
      if (a.m().send(id, _draft)) {
        _draft[0] = 0;
        _ed.attach(_draft, maxLen(a) + 1);
        _composing = false;
      } else {
        a.toast("Send failed: no path or queue full", C_ERR);
      }
      return;
    }
    if (k.code == K_UP || k.code == K_DOWN) return;
    _ed.key(k);
  }

  void drawMessage(App& a, Canvas& c, const model::Message& m, const Rect& r, int top,
                   const mc_font_t* f, int textW) {
    bool chan = id.kind == model::ThreadKind::Channel;
    uint8_t bar = m.outgoing ? C_ACCENT : chan ? C_CHAN : C_CHAT;
    int lines = wrapText(m.text, f, textW, nullptr, nullptr);
    int h = META_H + lines * f->height;
    if (m.outgoing) c.fillRect(r.x + 4, top, r.w - 7, h + 1, C_ZEBRA);
    c.fillRect(r.x + 4, top, 2, h + 1, bar);

    // Meta line: who, when, then delivery state or signal.
    const mc_font_t* sf = a.fSmall();
    char t[8];
    fmtTime(t, sizeof t, m.ts);
    int x = r.x + 9;
    if (m.outgoing) {
      x = c.text(x, top + 1, "me", sf, C_ACCENT) + 5;
    } else if (chan && m.sender[0]) {
      x = c.text(x, top + 1, m.sender, sf, C_CHAT, -1, 100) + 5;
    }
    x = c.text(x, top + 1, t, sf, C_DIM) + 5;
    if (m.outgoing) {
      const uint8_t* ic = icon::clock;
      uint8_t col = C_WARN;
      const char* label = "sending";
      switch (m.status) {
        case model::Delivery::Pending: break;
        case model::Delivery::Sent: ic = icon::check; col = C_DIM; label = chan ? "sent" : "sent, no ack"; break;
        case model::Delivery::Delivered: ic = icon::check; col = C_OK; label = "delivered"; break;
        case model::Delivery::Failed: ic = icon::cross; col = C_ERR; label = "failed"; break;
        default: ic = nullptr; label = ""; break;
      }
      if (ic) c.bitmap(x, top, ic, 8, 8, col);
      x = c.text(x + 10, top + 1, label, sf, col) + 4;
      if (chan && m.repeats) {
        char rp[16];
        snprintf(rp, sizeof rp, "heard %ux", m.repeats);
        c.text(x, top + 1, rp, sf, C_OK);
      } else if (m.retries) {
        char rt[12];
        snprintf(rt, sizeof rt, "retry %u", m.retries);
        c.text(x, top + 1, rt, sf, C_WARN);
      }
    } else {
      char s[24];
      if (m.hops >= 0) {
        snprintf(s, sizeof s, m.hops == 0 ? "direct" : "%d hop%s", m.hops, m.hops == 1 ? "" : "s");
        x = c.text(x, top + 1, s, sf, m.hops == 0 ? C_OK : C_DIM) + 5;
      }
      if (m.snr_x4 != INT8_MIN) {
        char snr[8];
        fmtSnr(snr, sizeof snr, m.snr_x4);
        snprintf(s, sizeof s, "%sdB", snr);
        c.text(x, top + 1, s, sf, snrColor(m.snr_x4));
      }
    }

    DrawCtx d{&c, f, r.x + 9, top + META_H, C_TEXT};
    wrapText(m.text, f, textW, drawLine, &d);
  }

  void drawCompose(App& a, Canvas& c, const Rect& r) {
    int y = r.y + r.h - COMPOSE_H;
    c.fillRect(r.x, y, r.w, COMPOSE_H, C_ZEBRA);
    c.hline(r.x, y, r.w, C_ACCENT);
    Rect er{r.x + 4, y + 3, r.w - 8, COMPOSE_H - 4};
    _ed.drawWrapped(a, c, er, a.fMed(), C_TEXT);
    char cnt[12];
    int len = _ed.length(), lim = maxLen(a);
    snprintf(cnt, sizeof cnt, "%d", lim - len);
    int w = Canvas::textWidth(cnt, a.fSmall());
    c.text(r.x + r.w - w - 3, y + COMPOSE_H - 9, cnt, a.fSmall(), lim - len < 20 ? C_WARN : C_DIM);
  }

  char _title[model::NAME_LEN];
  char _draft[model::TEXT_LEN] = {};
  LineEditor _ed;
  bool _composing = false;
  int _scroll = 0, _maxScroll = 0;
  uint32_t _readRev = 0, _blink = 0;
};

ConversationScreen s_conv;

}  // namespace

void openThread(App& a, model::ThreadId t) {
  if (!(s_conv.id == t)) s_conv.reset();
  s_conv.id = t;
  a.push(&s_conv);
}

}}  // namespace ui::screens
