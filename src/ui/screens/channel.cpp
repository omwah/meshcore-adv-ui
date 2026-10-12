// Channel management (open, mute, key, remove), joining channels, and the
// generic scrollable text page.
#include "ui/screens/screens.h"
#include "ui/widgets.h"
#include "ui/icons.h"
#include <stdio.h>
#include <string.h>

namespace ui { namespace screens {

namespace {

// ------------------------------------------------------------- text page

class TextScreen : public Screen {
public:
  char title_[32];
  const char* body = "";
  int scroll = 0;
  const char* title(App&) override { return title_; }
  void draw(App& a, Canvas& c, const Rect& r) override {
    const int lh = a.fMenu()->height;
    struct Ctx { Canvas* c; const mc_font_t* f; int x, y, first, rows, lh; } ctx{&c, a.fMenu(), r.x + 4, r.y + 2, scroll, (r.h - 2) / lh, lh};
    _lines = wrapText(body, a.fMenu(), r.w - 10, [](void* p, const char* s, int len, int line) {
      Ctx* x = (Ctx*)p;
      int row = line - x->first;
      if (row < 0 || row >= x->rows) return;
      char tmp[64];
      if (len > 63) len = 63;
      memcpy(tmp, s, len);
      tmp[len] = 0;
      x->c->text(x->x, x->y + row * x->lh, tmp, x->f, C_TEXT);
    }, &ctx);
    _rows = ctx.rows;
    drawScrollbar(c, r, _lines, scroll, _rows);
  }
  void onKey(App& a, const Key& k) override {
    int maxs = _lines > _rows ? _lines - _rows : 0;
    if (k.code == K_ESC || k.code == K_ENTER) a.pop();
    else if (k.code == K_UP && scroll > 0) scroll--;
    else if (k.code == K_DOWN && scroll < maxs) scroll++;
    else if (k.code == K_PGUP) scroll = scroll > _rows ? scroll - _rows : 0;
    else if (k.code == K_PGDN) scroll = scroll + _rows < maxs ? scroll + _rows : maxs;
  }
  const Hint* hints(App&) override {
    static const Hint h[] = {{"^v", "scroll"}, {"esc", "back"}, {nullptr, nullptr}};
    return h;
  }

private:
  int _lines = 0, _rows = 1;
};

TextScreen s_text;

// ------------------------------------------------------------- channel

enum ChItem { CH_OPEN, CH_MUTE, CH_KEY, CH_CLEAR, CH_REMOVE, CH_N };

int findChannel(App& a, int idx, model::Channel& out) {
  for (int i = 0, n = a.m().channelCount(); i < n; i++)
    if (a.m().channel(i, out) && out.idx == idx) return i;
  return -1;
}

class ChannelScreen : public MenuScreen {
public:
  int chIdx = 0;   // MeshCore channel slot

  const char* title(App& a) override {
    model::Channel ch;
    if (findChannel(a, chIdx, ch) >= 0) { snprintf(_title, sizeof _title, "%s", ch.name); return _title; }
    return "Channel";
  }

protected:
  int drawHead(App& a, Canvas& c, const Rect& r) override {
    model::Channel ch;
    if (findChannel(a, chIdx, ch) < 0) return 0;
    const mc_font_t* s = a.fSmall();
    const int lh = s->height + 1, h = 2 * lh + 4;
    c.fillRect(r.x, r.y, r.w, h, C_ZEBRA);
    c.fillRect(r.x, r.y, 3, h, C_CHAN);
    c.bitmap(r.x + 6, r.y + 2 + (s->height - 8) / 2, ch.hashtag ? icon::hash : icon::lock, 8, 8, C_CHAN);
    char l[48];
    snprintf(l, sizeof l, "slot %u  %s", ch.idx, ch.hashtag ? "public hashtag" : "private key");
    c.text(r.x + 18, r.y + 2, l, s, C_DIM, -1, r.w - 22);
    c.text(r.x + 6, r.y + 2 + lh, ch.hashtag ? "anyone who joins the same #name can read"
                                             : "only holders of the key can read",
           s, C_DIM, -1, r.w - 10);
    return h + 2;
  }

  int count(App&) override { return CH_N; }

  void item(App& a, int i, MenuItem& it) override {
    model::Channel ch;
    findChannel(a, chIdx, ch);
    switch (i) {
      case CH_OPEN: it.label = "Open chat"; break;
      case CH_MUTE: it.label = "Muted"; it.choice = true; snprintf(it.value, sizeof it.value, ch.muted ? "yes" : "no"); it.valueCol = ch.muted ? C_WARN : C_DIM; break;
      case CH_KEY: it.label = "Show key"; break;
      case CH_CLEAR: it.label = "Clear history"; break;
      case CH_REMOVE: it.label = "Leave channel"; it.enabled = chIdx != 0; break;
    }
  }

  void activate(App& a, int i) override {
    model::Channel ch;
    int li = findChannel(a, chIdx, ch);
    if (li < 0) return;
    switch (i) {
      case CH_OPEN: openThread(a, {model::ThreadKind::Channel, (uint16_t)chIdx}); break;
      case CH_MUTE: adjust(a, i, 1); break;
      case CH_KEY: {
        static char body[96];
        uint8_t k[16];
        if (!a.m().channelKey(li, k)) return;
        int o = snprintf(body, sizeof body, "Key (hex):\n");
        for (int b = 0; b < 16; b++) o += snprintf(body + o, sizeof body - o, "%02x%s", k[b], b == 7 ? "\n" : "");
        o += snprintf(body + o, sizeof body - o, "\n\nShare it only with people who should read %s.", ch.name);
        openText(a, "Channel key", body);
        break;
      }
      case CH_CLEAR:
        a.confirm("Delete all messages in this channel, including the SD card archive?", [](App& app, int slot) {
          app.m().clearHistory({model::ThreadKind::Channel, (uint16_t)slot});
          app.toast("History cleared");
        }, chIdx);
        break;
      case CH_REMOVE:
        a.confirm("Leave this channel? Its key is deleted from this device.", [](App& app, int arg) {
          app.m().removeChannel(arg);
          app.popToRoot();
          app.toast("Left channel");
        }, li);
        break;
    }
  }

  void adjust(App& a, int i, int) override {
    if (i != CH_MUTE) return;
    model::Channel ch;
    int li = findChannel(a, chIdx, ch);
    if (li >= 0) a.m().setMuted(li, !ch.muted);
  }

private:
  char _title[model::NAME_LEN];
};

ChannelScreen s_channel;

// ------------------------------------------------------------- join

enum JItem { J_HASHTAG, J_PRIVATE, J_CREATE, J_N };

bool parseHexKey(const char* s, uint8_t out[16]) {
  int n = 0;
  for (; *s && n < 32; s++) {
    char c = *s;
    int v = c >= '0' && c <= '9' ? c - '0' : (c | 0x20) >= 'a' && (c | 0x20) <= 'f' ? (c | 0x20) - 'a' + 10 : -1;
    if (v < 0) { if (c == ' ' || c == ':') continue; return false; }
    if (n & 1) out[n / 2] |= v; else out[n / 2] = (uint8_t)(v << 4);
    n++;
  }
  return n == 32;
}

char s_joinName[model::NAME_LEN];

class JoinScreen : public MenuScreen {
public:
  const char* title(App&) override { return "Join channel"; }

protected:
  int count(App&) override { return J_N; }
  void item(App&, int i, MenuItem& it) override {
    static const char* const kL[J_N] = {"Public #hashtag", "Private (enter key)", "Create private"};
    it.label = kL[i];
  }
  void activate(App& a, int i) override {
    if (i == J_HASHTAG) {
      a.prompt("Hashtag (without #)", "", 30, [](App& app, int) {
        char n[model::NAME_LEN];
        snprintf(n, sizeof n, "#%s", app.promptText());
        if (app.promptText()[0] && app.m().joinHashtag(n)) { app.popToRoot(); app.toast("Joined channel", C_OK); }
        else app.toast("Could not join (no free slot?)", C_ERR);
      });
    } else if (i == J_PRIVATE) {
      a.prompt("Channel name", "", 30, [](App& app, int) {
        snprintf(s_joinName, sizeof s_joinName, "%s", app.promptText());
        if (!s_joinName[0]) return;
        app.prompt("Key: 32 hex digits", "", 47, [](App& app2, int) {
          uint8_t k[16];
          if (!parseHexKey(app2.promptText(), k)) { app2.toast("Key must be 32 hex digits", C_ERR); return; }
          if (app2.m().joinPrivate(s_joinName, k)) { app2.popToRoot(); app2.toast("Joined channel", C_OK); }
          else app2.toast("Could not join (no free slot?)", C_ERR);
        });
      });
    } else {
      a.prompt("New channel name", "", 30, [](App& app, int) {
        if (app.promptText()[0] && app.m().createPrivate(app.promptText())) { app.popToRoot(); app.toast("Channel created: share its key", C_OK); }
        else app.toast("Could not create (no free slot?)", C_ERR);
      });
    }
  }
};

JoinScreen s_join;

}  // namespace

void openChannel(App& a, int chIdx) {
  s_channel.chIdx = chIdx;
  s_channel.resetNav();
  a.push(&s_channel);
}

void openJoinChannel(App& a) {
  s_join.resetNav();
  a.push(&s_join);
}

void openText(App& a, const char* title, const char* body) {
  snprintf(s_text.title_, sizeof s_text.title_, "%s", title);
  s_text.body = body;
  s_text.scroll = 0;
  a.push(&s_text);
}

}}  // namespace ui::screens
