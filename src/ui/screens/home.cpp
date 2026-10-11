// Home: tabs for Chats (conversations, most recent first), Contacts (people),
// Nodes (repeaters, rooms, sensors) and Channels. Typing filters the list.
#include "ui/screens/screens.h"
#include "ui/widgets.h"
#include "ui/icons.h"
#include <stdio.h>
#include <string.h>

namespace ui { namespace screens {

namespace {

enum Tab { T_CHATS, T_CONTACTS, T_NODES, T_CHANNELS, T_COUNT };
const char* const kTabNames[T_COUNT] = {"Chats", "Contacts", "Nodes", "Channels"};

constexpr int TAB_H = 14;
constexpr int CHAT_ROW_H = 24;
constexpr int ROW_H = 16;
constexpr int MAX_ORDER = 256;

bool matches(const char* name, const char* q) {
  if (!q[0]) return true;
  for (const char* p = name; *p; p++) {
    int i = 0;
    while (q[i] && p[i] && ((p[i] | 0x20) == (q[i] | 0x20))) i++;
    if (!q[i]) return true;
  }
  return false;
}

class HomeScreen : public Screen {
public:
  const char* title(App&) override { return "MeshCore"; }

  void onEnter(App& a) override {
    _builtRev = ~0u;
    rebuild(a);
  }

  bool tick(App& a, uint32_t) override {
    if (a.m().revision() != _builtRev) { rebuild(a); return true; }
    return false;
  }

  void draw(App& a, Canvas& c, const Rect& r) override {
    drawTabs(a, c, r);
    Rect lr{r.x, r.y + TAB_H, r.w, r.h - TAB_H};
    ListNav& nav = _nav[_tab];
    int rowH = _tab == T_CHATS ? CHAT_ROW_H : ROW_H;
    int vis = lr.h / rowH;
    nav.clamp(_n, vis);
    if (_n == 0) {
      const char* msg = _query[0] ? "No matches" :
                        _tab == T_CHATS ? "No conversations yet" :
                        _tab == T_CHANNELS ? "No channels: tab to join" : "Nobody heard yet";
      c.text(r.x + (r.w - Canvas::textWidth(msg, a.fMed())) / 2, lr.y + 36, msg, a.fMed(), C_DIM);
      return;
    }
    for (int row = 0; row < vis && nav.top + row < _n; row++) {
      int i = nav.top + row;
      int y = lr.y + row * rowH;
      bool sel = i == nav.sel;
      if (sel) {
        c.fillRect(lr.x, y, lr.w - 3, rowH, C_SEL);
      } else if (row & 1) {
        c.fillRect(lr.x, y, lr.w - 3, rowH, C_ZEBRA);
      }
      if (_tab == T_CHATS) drawChat(a, c, y, _order[i], sel);
      else if (_tab == T_CHANNELS) drawChannel(a, c, y, _order[i]);
      else drawContact(a, c, y, _order[i]);
    }
    drawScrollbar(c, lr, _n, nav.top, vis);
  }

  void onKey(App& a, const Key& k) override {
    ListNav& nav = _nav[_tab];
    Rect r = a.content();
    int vis = (r.h - TAB_H) / (_tab == T_CHATS ? CHAT_ROW_H : ROW_H);
    if (k.code == K_LEFT || k.code == K_RIGHT) {
      if (k.repeat) return;   // one tab per press
      _tab = (_tab + (k.code == K_RIGHT ? 1 : T_COUNT - 1)) % T_COUNT;
      _query[0] = 0;
      rebuild(a);
      return;
    }
    if (nav.key(k, _n, vis)) return;
    if (k.code == K_ESC) {
      // Esc never opens anything here, so pressing it repeatedly to get
      // "back home" can't overshoot into a menu.
      if (_query[0]) { _query[0] = 0; rebuild(a); }
      return;
    }
    if (k.code == K_BKSP) {
      int l = (int)strlen(_query);
      if (l) { _query[l - 1] = 0; rebuild(a); }
      return;
    }
    if (k.isChar() && k.code != ' ') {
      int l = (int)strlen(_query);
      if (l < (int)sizeof(_query) - 1) { _query[l] = (char)k.code; _query[l + 1] = 0; nav.sel = 0; rebuild(a); }
      return;
    }
    if (k.code == K_TAB && _tab == T_CHANNELS && _n == 0) { openJoinChannel(a); return; }
    if (_n == 0 || nav.sel >= _n) return;
    int idx = _order[nav.sel];
    if (k.code == K_ENTER) open(a, idx);
    else if (k.code == K_TAB) details(a, idx);
  }

  const Hint* hints(App&) override {
    static const Hint chats[] = {{"ent", "open"}, {"tab", "info"}, {"<>", "tabs"}, {"a-z", "find"}, {"opt", "menu"}, {nullptr, nullptr}};
    static const Hint search[] = {{"ent", "open"}, {"bksp", "edit"}, {"esc", "clear"}, {nullptr, nullptr}};
    static const Hint chan[] = {{"ent", "open"}, {"tab", "manage"}, {"<>", "tabs"}, {"opt", "menu"}, {nullptr, nullptr}};
    if (_query[0]) return search;
    return _tab == T_CHANNELS ? chan : chats;
  }

private:
  void drawTabs(App& a, Canvas& c, const Rect& r) {
    c.fillRect(r.x, r.y, r.w, TAB_H, C_ZEBRA);
    if (_query[0]) {
      c.text(r.x + 4, r.y + 1, "Find:", a.fMed(), C_DIM);
      int x = c.text(r.x + 40, r.y + 1, _query, a.fMed(), C_ACCENT);
      if (!((a.nowMs() / 500) & 1)) c.fillRect(x, r.y + 2, 1, 11, C_ACCENT);
      char n[12];
      snprintf(n, sizeof n, "%d", _n);
      c.text(r.x + r.w - 4 - Canvas::textWidth(n, a.fMed()), r.y + 1, n, a.fMed(), C_DIM);
      return;
    }
    int x = r.x + 1;
    for (int t = 0; t < T_COUNT; t++) {
      int w = Canvas::textWidth(kTabNames[t], a.fMed()) + 10;
      bool on = t == _tab;
      if (on) c.pill(x, r.y + 1, w, TAB_H - 2, C_ACCENT);
      c.text(x + 5, r.y + 1, kTabNames[t], a.fMed(), on ? C_BG : C_DIM);
      // Unread dot on Chats when looking elsewhere.
      if (t == T_CHATS && !on && a.m().unreadTotal()) c.fillRect(x + w - 4, r.y + 3, 3, 3, C_BADGE);
      x += w + 2;
    }
  }

  void drawChat(App& a, Canvas& c, int y, int i, bool sel) {
    model::Thread t;
    if (!a.m().thread(i, t)) return;
    bool chan = t.id.kind == model::ThreadKind::Channel;
    uint8_t col = chan ? C_CHAN : typeColor(t.type);
    c.fillRect(0, y, 3, CHAT_ROW_H, col);
    c.bitmap(6, y + 3, chan ? icon::hash : typeIcon(t.type), 8, 8, col);
    // Right side: time, unread badge.
    char ts[12];
    fmtAgo(ts, sizeof ts, a.m().now(), t.lastTs);
    int rx = Canvas::W - 6 - Canvas::textWidth(ts, a.fSmall());
    c.text(rx, y + 3, ts, a.fSmall(), t.unread ? C_BADGE : C_DIM);
    int tx = rx - 4;
    if (t.unread) tx = drawBadge(a, rx - 3, y + 12, t.unread) - 2;
    if (t.muted) c.bitmap(rx - 10, y + 1, icon::mute, 8, 8, C_DIM);
    if (t.favourite) c.bitmap(rx - (t.muted ? 20 : 10), y + 1, icon::star, 8, 8, C_WARN);
    c.text(17, y + 1, t.title, a.fMed(), t.unread ? C_TEXT : C_TEXT, -1, rx - 40);
    c.text(17, y + 12, t.preview, a.fMed(), sel ? C_TEXT : C_DIM, -1, tx - 20);
  }

  void drawContact(App& a, Canvas& c, int y, int i) {
    model::Contact ct;
    if (!a.m().contact(i, ct)) return;
    uint8_t col = typeColor(ct.type);
    c.bitmap(4, y + 4, typeIcon(ct.type), 8, 8, col);
    // Right: age, hops pill, signal.
    char ago[8];
    fmtAgo(ago, sizeof ago, a.m().now(), ct.lastHeard);
    int x = Canvas::W - 6 - Canvas::textWidth(ago, a.fSmall());
    c.text(x, y + 5, ago, a.fSmall(), C_DIM);
    x -= 13;
    drawSignal(c, x, y + 4, ct.snr_x4);
    char hp[6];
    if (ct.hops < 0) snprintf(hp, sizeof hp, "fl");
    else snprintf(hp, sizeof hp, "%d", ct.hops);
    int hw = Canvas::textWidth(hp, a.fSmall()) + 4;
    x -= hw + 3;
    c.pill(x, y + 4, hw, 9, ct.hops == 0 ? C_OK : ct.hops < 0 ? C_DIM : C_ACCENT);
    c.text(x + 2, y + 4, hp, a.fSmall(), C_BG);
    if (ct.favourite) { x -= 10; c.bitmap(x, y + 4, icon::star, 8, 8, C_WARN); }
    c.text(16, y, ct.name, a.fList(), C_TEXT, -1, x - 20);
  }

  void drawChannel(App& a, Canvas& c, int y, int i) {
    model::Channel ch;
    if (!a.m().channel(i, ch)) return;
    c.bitmap(4, y + 4, ch.hashtag ? icon::hash : icon::lock, 8, 8, C_CHAN);
    int x = Canvas::W - 6;
    // Unread from the matching thread.
    model::ThreadId tid{model::ThreadKind::Channel, ch.idx};
    for (int t = 0, n = a.m().threadCount(); t < n; t++) {
      model::Thread th;
      if (a.m().thread(t, th) && th.id == tid && th.unread) { x = drawBadge(a, x, y + 3, th.unread) - 3; break; }
    }
    if (ch.muted) { x -= 10; c.bitmap(x, y + 4, icon::mute, 8, 8, C_DIM); }
    c.text(16, y, ch.name, a.fList(), C_TEXT, -1, x - 20);
  }

  void rebuild(App& a) {
    _builtRev = a.m().revision();
    _n = 0;
    model::MeshModel& m = a.m();
    if (_tab == T_CHATS) {
      for (int i = 0, n = m.threadCount(); i < n && _n < MAX_ORDER; i++) {
        model::Thread t;
        if (m.thread(i, t) && matches(t.title, _query)) _order[_n++] = (uint16_t)i;
      }
    } else if (_tab == T_CHANNELS) {
      for (int i = 0, n = m.channelCount(); i < n && _n < MAX_ORDER; i++) {
        model::Channel ch;
        if (m.channel(i, ch) && matches(ch.name, _query)) _order[_n++] = (uint16_t)i;
      }
    } else {
      bool people = _tab == T_CONTACTS;
      for (int i = 0, n = m.contactCount(); i < n && _n < MAX_ORDER; i++) {
        model::Contact ct;
        if (!m.contact(i, ct)) continue;
        bool isPerson = ct.type == model::NodeType::Chat || ct.type == model::NodeType::None;
        if (isPerson == people && matches(ct.name, _query)) _order[_n++] = (uint16_t)i;
      }
      // Favourites first, then most recently heard (insertion sort: n is small).
      for (int i = 1; i < _n; i++) {
        uint16_t v = _order[i];
        model::Contact cv;
        m.contact(v, cv);
        int j = i - 1;
        while (j >= 0) {
          model::Contact cj;
          m.contact(_order[j], cj);
          bool before = cv.favourite != cj.favourite ? cv.favourite : cv.lastHeard > cj.lastHeard;
          if (!before) break;
          _order[j + 1] = _order[j];
          j--;
        }
        _order[j + 1] = v;
      }
    }
  }

  void open(App& a, int idx) {
    if (_tab == T_CHATS) {
      model::Thread t;
      if (a.m().thread(idx, t)) openThread(a, t.id);
    } else if (_tab == T_CHANNELS) {
      model::Channel ch;
      if (a.m().channel(idx, ch)) openThread(a, {model::ThreadKind::Channel, ch.idx});
    } else if (_tab == T_CONTACTS) {
      openThread(a, {model::ThreadKind::Direct, (uint16_t)idx});
    } else {
      model::Contact ct;
      if (a.m().contact(idx, ct) && ct.type == model::NodeType::Room) openThread(a, {model::ThreadKind::Direct, (uint16_t)idx});
      else openContact(a, idx);
    }
  }

  void details(App& a, int idx) {
    if (_tab == T_CHATS) {
      model::Thread t;
      if (!a.m().thread(idx, t)) return;
      if (t.id.kind == model::ThreadKind::Channel) openChannel(a, t.id.idx);
      else openContact(a, t.id.idx);
    } else if (_tab == T_CHANNELS) {
      openChannel(a, idx);
    } else {
      openContact(a, idx);
    }
  }

  int _tab = T_CHATS;
  ListNav _nav[T_COUNT];
  char _query[16] = {};
  uint16_t _order[MAX_ORDER];
  int _n = 0;
  uint32_t _builtRev = ~0u;
};

HomeScreen s_home;

}  // namespace

Screen* home() { return &s_home; }

}}  // namespace ui::screens
