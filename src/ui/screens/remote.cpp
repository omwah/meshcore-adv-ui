// Remote node mode: the link to the node (status, connect, forget), finding
// nodes nearby, and restarting or erasing the node itself.
#include "ui/screens/screens.h"
#if MC_UI_REMOTE   // builds with Bluetooth (the USB-only build has no remote mode)
#include "ui/widgets.h"
#include <stdio.h>
#include <string.h>

namespace ui { namespace screens {

namespace {

void linkStatus(const model::Link& l, const char*& text, uint8_t& col) {
  switch (l.state) {
    case model::LinkState::Ready:
      text = l.synced ? "Connected" : "Connected, reading the node...";
      col = l.synced ? C_OK : C_ACCENT;
      break;
    case model::LinkState::Connecting: text = "Connecting..."; col = C_ACCENT; break;
    case model::LinkState::Pairing: text = "Pairing..."; col = C_ACCENT; break;
    case model::LinkState::Scanning: text = "Searching for nodes..."; col = C_ACCENT; break;
    case model::LinkState::Lost: text = "Connection lost, retrying"; col = C_WARN; break;
    default:
      text = l.saved ? "Not connected" : "No node chosen yet";
      col = C_WARN;
      break;
  }
}

// ------------------------------------------------------------- scan

class ScanScreen : public MenuScreen {
public:
  const char* title(App&) override { return "Nodes nearby"; }
  void onEnter(App& a) override {
    MenuScreen::onEnter(a);
    a.m().scan(true);
  }
  void onKey(App& a, const Key& k) override {
    if (k.code == K_ESC) a.m().scan(false);
    MenuScreen::onKey(a, k);
  }

protected:
  int drawHead(App& a, Canvas& c, const Rect& r) override {
    const mc_font_t* s = a.fSmall();
    const int lh = s->height + 1;
    c.fillRect(r.x, r.y, r.w, lh + 4, C_ZEBRA);
    char l[48];
    int n = a.m().foundCount();
    snprintf(l, sizeof l, n ? "Companion nodes found: %d" : "Searching for companion nodes...", n);
    c.text(r.x + 4, r.y + 2, l, s, n ? C_TEXT : C_DIM, -1, r.w - 8);
    return lh + 6;
  }
  int count(App& a) override { return a.m().foundCount(); }
  void item(App& a, int i, MenuItem& it) override {
    model::FoundNode f;
    if (!a.m().found(i, f)) return;
    snprintf(_names[i % 8], sizeof _names[0], "%s", f.name);
    it.label = _names[i % 8];
    snprintf(it.value, sizeof it.value, "%d dBm", f.rssi);
    it.valueCol = f.rssi > -70 ? C_OK : f.rssi > -90 ? C_ACCENT : C_WARN;
  }
  void activate(App& a, int i) override {
    // Stock companions use 123456 unless their owner set another PIN.
    a.prompt("Node's Bluetooth PIN", "123456", 6, [](App& app, int idx) {
      const char* t = app.promptText();
      uint32_t v = 0;
      int n = 0;
      for (; t[n] >= '0' && t[n] <= '9'; n++) v = v * 10 + (t[n] - '0');
      if (n != 6 || t[n]) { app.toast("PIN must be 6 digits", C_ERR); return; }
      if (!app.m().connectFound(idx, v)) { app.toast("That node is gone: search again", C_ERR); return; }
      app.pop();   // back to the Remote node screen
      app.toast("Connecting...", C_ACCENT);
    }, i);
  }

private:
  char _names[8][model::NAME_LEN];
};

ScanScreen s_scan;

// ------------------------------------------------------------- the node

enum RNItem { RN_SCAN, RN_CONNECT, RN_FORGET, RN_HDR_NODE, RN_REBOOT, RN_ERASE, RN_N };

class RemoteScreen : public MenuScreen {
public:
  const char* title(App&) override { return "Remote node"; }

protected:
  int drawHead(App& a, Canvas& c, const Rect& r) override {
    model::Link l = a.m().link();
    const mc_font_t* s = a.fSmall();
    const int lh = s->height + 1;
    int lines = l.error[0] ? 3 : 2;
    c.fillRect(r.x, r.y, r.w, lines * lh + 4, C_ZEBRA);
    const char* st;
    uint8_t col;
    linkStatus(l, st, col);
    c.text(r.x + 4, r.y + 2, st, s, col, -1, r.w - 8);
    char d[64];
    if (!l.saved) snprintf(d, sizeof d, "Find nodes nearby to choose one");
    else if (l.state == model::LinkState::Ready && l.nodeBattMv)
      snprintf(d, sizeof d, "%s  %d dBm  %u.%02u V", l.node[0] ? l.node : l.peer, l.rssi, l.nodeBattMv / 1000,
               l.nodeBattMv % 1000 / 10);
    else snprintf(d, sizeof d, "%s", l.node[0] ? l.node : l.peer);
    c.text(r.x + 4, r.y + 2 + lh, d, s, C_DIM, -1, r.w - 8);
    if (l.error[0]) c.text(r.x + 4, r.y + 2 + 2 * lh, l.error, s, C_ERR, -1, r.w - 8);
    return lines * lh + 6;
  }
  int count(App&) override { return RN_N; }
  void item(App& a, int i, MenuItem& it) override {
    model::Link l = a.m().link();
    bool up = l.state == model::LinkState::Ready;
    switch (i) {
      case RN_SCAN: it.label = "Find nodes nearby"; break;
      case RN_CONNECT:
        it.label = up || l.state == model::LinkState::Connecting || l.state == model::LinkState::Lost ? "Disconnect"
                                                                                                        : "Connect";
        it.enabled = l.saved;
        break;
      case RN_FORGET: it.label = "Forget this node"; it.enabled = l.saved; break;
      case RN_HDR_NODE: it.label = "THE NODE"; it.header = true; break;
      case RN_REBOOT: it.label = "Restart the node"; it.enabled = l.synced; break;
      case RN_ERASE: it.label = "Erase the node"; it.enabled = l.synced; it.valueCol = C_ERR; break;
    }
  }
  void activate(App& a, int i) override {
    model::Link l = a.m().link();
    switch (i) {
      case RN_SCAN: s_scan.resetNav(); a.push(&s_scan); break;
      case RN_CONNECT:
        if (l.state == model::LinkState::Idle) a.m().reconnect();
        else a.m().disconnect();
        break;
      case RN_FORGET:
        a.confirm("Forget this node? Its messages stay on this device.", [](App& app, int) { app.m().forgetNode(); });
        break;
      case RN_REBOOT:
        a.confirm("Restart the node?", [](App& app, int) {
          if (app.m().rebootNode()) app.toast("Restarting the node", C_OK);
        });
        break;
      case RN_ERASE:
        a.confirm("Erase the node's identity, contacts, channels and settings? Back it up first.", [](App& app, int) {
          if (app.m().resetNode()) app.toast("Erasing the node", C_WARN);
        });
        break;
    }
  }
};

RemoteScreen s_remote;

}  // namespace

void openRemote(App& a) { s_remote.resetNav(); a.push(&s_remote); }

}}  // namespace ui::screens

#endif  // MC_UI_REMOTE
