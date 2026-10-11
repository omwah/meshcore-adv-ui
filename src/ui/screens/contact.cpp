// Contact details and actions, and repeater/room administration.
#include "ui/screens/screens.h"
#include "ui/widgets.h"
#include "ui/icons.h"
#include <stdio.h>
#include <string.h>

namespace ui { namespace screens {

namespace {

void fmtKey(char* out, size_t n, const uint8_t* k, int len) {
  size_t o = 0;
  for (int i = 0; i < len && o + 2 < n; i++) o += snprintf(out + o, n - o, "%02X", k[i]);
}

void fmtPos(char* out, size_t n, int32_t lat, int32_t lon) {
  snprintf(out, n, "%.5f, %.5f", lat / 1e6, lon / 1e6);
}

// ------------------------------------------------------------- contact

enum CItem { CI_MESSAGE, CI_ADMIN, CI_FAV, CI_PATH, CI_TRACE, CI_PING, CI_REMOVE, CI_N };

class ContactScreen : public MenuScreen {
public:
  int idx = 0;

  const char* title(App& a) override {
    model::Contact c;
    if (a.m().contact(idx, c)) { snprintf(_title, sizeof _title, "%s", c.name); return _title; }
    return "Contact";
  }

  bool tick(App& a, uint32_t) override {
    model::Req s = a.m().requestState();
    if (s != _lastReq) {
      bool finished = _lastReq == model::Req::Busy;
      _lastReq = s;
      if (finished && _waiting) {
        _waiting = false;
        if (s == model::Req::Ok) {
          char t[48];
          if (_pending == CI_PING) snprintf(t, sizeof t, "Ping reply in %u ms", (unsigned)a.m().lastRttMs());
          else snprintf(t, sizeof t, "%s", a.m().requestText());
          if (_pending == CI_TRACE) openText(a, "Trace", a.m().requestText());
          else a.toast(t, C_OK);
        } else {
          a.toast(s == model::Req::Timeout ? "No reply (timeout)" : a.m().requestText(), C_ERR);
        }
      }
      return true;
    }
    return false;
  }

protected:
  int drawHead(App& a, Canvas& c, const Rect& r) override {
    model::Contact ct;
    if (!a.m().contact(idx, ct)) return 0;
    const mc_font_t* s = a.fSmall();
    uint8_t col = typeColor(ct.type);
    int y = r.y + 2;
    c.fillRect(r.x, r.y, r.w, 41, C_ZEBRA);
    c.fillRect(r.x, r.y, 3, 41, col);
    c.bitmap(r.x + 6, y, typeIcon(ct.type), 8, 8, col);
    static const char* const kTypes[] = {"node", "chat", "repeater", "room", "sensor"};
    char line[64], key[16], ago[8];
    fmtKey(key, sizeof key, ct.key, 4);
    snprintf(line, sizeof line, "%s  %s", kTypes[(int)ct.type <= 4 ? (int)ct.type : 0], key);
    c.text(r.x + 18, y + 1, line, s, C_DIM);
    // Path / hops.
    if (ct.hops < 0) snprintf(line, sizeof line, "path: unknown (flood)");
    else if (ct.hops == 0) snprintf(line, sizeof line, "path: direct");
    else {
      int o = snprintf(line, sizeof line, "path: %d hop%s ", ct.hops, ct.hops == 1 ? "" : "s");
      for (int i = 0; i < ct.pathLen && i < 6 && o < (int)sizeof line - 4; i++)
        o += snprintf(line + o, sizeof line - o, "%s%02X", i ? ">" : "", ct.path[i]);
    }
    c.text(r.x + 6, y + 10, line, s, ct.hops < 0 ? C_WARN : C_TEXT);
    fmtAgo(ago, sizeof ago, a.m().now(), ct.lastHeard);
    char snr[8];
    fmtSnr(snr, sizeof snr, ct.snr_x4);
    snprintf(line, sizeof line, "heard %s ago  snr %s  rssi %d", ago, snr, ct.rssi);
    c.text(r.x + 6, y + 19, line, s, C_TEXT);
    drawSignal(c, r.x + r.w - 14, y, ct.snr_x4);
    if (ct.hasPos) {
      fmtPos(line, sizeof line, ct.lat_e6, ct.lon_e6);
      c.text(r.x + 6, y + 28, line, s, C_DIM);
    } else {
      c.text(r.x + 6, y + 28, "no position", s, C_DIM);
    }
    return 43;
  }

  int count(App&) override { return CI_N; }

  void item(App& a, int i, MenuItem& it) override {
    model::Contact ct;
    a.m().contact(idx, ct);
    bool admin = ct.type == model::NodeType::Repeater || ct.type == model::NodeType::Room || ct.type == model::NodeType::Sensor;
    switch (i) {
      case CI_MESSAGE: it.label = ct.type == model::NodeType::Room ? "Open room" : "Message"; it.enabled = ct.type != model::NodeType::Repeater && ct.type != model::NodeType::Sensor; break;
      case CI_ADMIN: it.label = "Admin / status"; it.enabled = admin; break;
      case CI_FAV: it.label = "Favourite"; it.choice = true; snprintf(it.value, sizeof it.value, ct.favourite ? "yes" : "no"); it.valueCol = ct.favourite ? C_WARN : C_DIM; break;
      case CI_PATH: it.label = "Reset path"; it.enabled = ct.hops >= 0; break;
      case CI_TRACE: it.label = "Trace route"; break;
      case CI_PING: it.label = "Ping"; break;
      case CI_REMOVE: it.label = "Remove contact"; it.valueCol = C_ERR; break;
    }
    if (_waiting && (i == CI_TRACE || i == CI_PING) && i == _pending) snprintf(it.value, sizeof it.value, "waiting...");
  }

  void activate(App& a, int i) override {
    switch (i) {
      case CI_MESSAGE: openThread(a, {model::ThreadKind::Direct, (uint16_t)idx}); break;
      case CI_ADMIN: openAdmin(a, idx); break;
      case CI_FAV: adjust(a, i, 1); break;
      case CI_PATH:
        a.m().resetPath(idx);
        a.toast("Path reset: next message floods");
        break;
      case CI_TRACE:
      case CI_PING:
        if ((i == CI_TRACE ? a.m().tracePath(idx) : a.m().ping(idx))) { _waiting = true; _pending = i; }
        else a.toast("Busy: another request is running", C_WARN);
        break;
      case CI_REMOVE:
        a.confirm("Remove this contact?", [](App& app, int arg) {
          app.m().removeContact(arg);
          app.pop();
          app.toast("Contact removed");
        }, idx);
        break;
    }
  }

  void adjust(App& a, int i, int) override {
    if (i != CI_FAV) return;
    model::Contact ct;
    if (a.m().contact(idx, ct)) a.m().setFavourite(idx, !ct.favourite);
  }

private:
  char _title[model::NAME_LEN];
  bool _waiting = false;
  int _pending = -1;
  model::Req _lastReq = model::Req::Idle;
};

ContactScreen s_contact;

// ------------------------------------------------------------- admin

enum AItem { AI_LOGIN, AI_STATS, AI_NEIGH, AI_TELEM, AI_CLOCK, AI_ADVERT, AI_CLI, AI_N };

class AdminScreen : public MenuScreen {
public:
  int idx = 0;

  const char* title(App& a) override {
    model::Contact c;
    if (a.m().contact(idx, c)) { snprintf(_title, sizeof _title, "Admin: %s", c.name); return _title; }
    return "Admin";
  }

  bool tick(App& a, uint32_t) override {
    model::Req s = a.m().requestState();
    if (s == _last) return false;
    bool finished = _last == model::Req::Busy;
    _last = s;
    if (finished && _pending >= 0) {
      int p = _pending;
      _pending = -1;
      if (s != model::Req::Ok) {
        a.toast(s == model::Req::Timeout ? "No reply (timeout)" : a.m().requestText(), C_ERR);
      } else if (p == AI_LOGIN) {
        _loggedIn = true;
        a.toast(a.m().requestText(), C_OK);
      } else if (p == AI_STATS) {
        _showStats = true;
      } else if (p == AI_NEIGH) {
        showNeighbours(a);
      } else {
        openText(a, p == AI_TELEM ? "Telemetry" : "Reply", a.m().requestText());
      }
    }
    return true;
  }

  void reset() { _loggedIn = false; _showStats = false; _pending = -1; }

protected:
  int drawHead(App& a, Canvas& c, const Rect& r) override {
    if (!_showStats) return 0;
    const model::RemoteStats& s = a.m().remoteStats();
    if (!s.valid) return 0;
    const mc_font_t* f = a.fSmall();
    c.fillRect(r.x, r.y, r.w, 46, C_ZEBRA);
    char l[48], up[12];
    fmtDur(up, sizeof up, s.uptimeS);
    snprintf(l, sizeof l, "batt %u.%02uV  up %s  queue %u", s.battMv / 1000, (s.battMv % 1000) / 10, up, s.queueLen);
    c.text(r.x + 4, r.y + 2, l, f, C_TEXT);
    char snr[8];
    fmtSnr(snr, sizeof snr, s.lastSnr_x4);
    snprintf(l, sizeof l, "noise %d  rssi %d  snr %s", s.noiseFloor, s.lastRssi, snr);
    c.text(r.x + 4, r.y + 11, l, f, C_TEXT);
    snprintf(l, sizeof l, "rx %lu  tx %lu  air tx %lus rx %lus", (unsigned long)s.rxPackets, (unsigned long)s.txPackets,
             (unsigned long)s.airTxS, (unsigned long)s.airRxS);
    c.text(r.x + 4, r.y + 20, l, f, C_TEXT);
    snprintf(l, sizeof l, "sent flood %lu direct %lu", (unsigned long)s.floodsSent, (unsigned long)s.directSent);
    c.text(r.x + 4, r.y + 29, l, f, C_DIM);
    snprintf(l, sizeof l, "dups flood %lu direct %lu", (unsigned long)s.dupsFlood, (unsigned long)s.dupsDirect);
    c.text(r.x + 4, r.y + 38, l, f, C_DIM);
    return 47;
  }

  int count(App&) override { return AI_N; }

  void item(App& a, int i, MenuItem& it) override {
    (void)a;
    static const char* const kLabels[AI_N] = {"Login", "Status", "Neighbours", "Telemetry", "Sync clock", "Send advert", "CLI command"};
    it.label = kLabels[i];
    if (i == AI_LOGIN) {
      snprintf(it.value, sizeof it.value, _loggedIn ? "logged in" : "required");
      it.valueCol = _loggedIn ? C_OK : C_WARN;
    } else {
      it.enabled = i == AI_TELEM || _loggedIn;
    }
    if (_pending == i) snprintf(it.value, sizeof it.value, "waiting...");
  }

  void activate(App& a, int i) override {
    if (a.m().requestState() == model::Req::Busy) { a.toast("Busy: waiting for a reply", C_WARN); return; }
    bool ok = false;
    switch (i) {
      case AI_LOGIN:
        a.prompt("Password (blank: guest)", "", 15, [](App& app, int arg) {
          if (app.m().login(arg, app.promptText())) s_admin_pending(AI_LOGIN);
        }, idx, true);
        return;
      case AI_STATS: ok = a.m().requestStats(idx); break;
      case AI_NEIGH: ok = a.m().requestNeighbours(idx); break;
      case AI_TELEM: ok = a.m().requestTelemetry(idx); break;
      case AI_CLOCK: ok = a.m().sendCommand(idx, "clock sync"); break;
      case AI_ADVERT: ok = a.m().sendCommand(idx, "advert"); break;
      case AI_CLI:
        a.prompt("Command", "", 60, [](App& app, int arg) {
          if (app.m().sendCommand(arg, app.promptText())) s_admin_pending(AI_CLI);
        }, idx);
        return;
    }
    if (ok) _pending = i;
    else a.toast("Could not send request", C_ERR);
  }

public:
  static void s_admin_pending(int which);
  void setPending(int p) { _pending = p; }

private:
  void showNeighbours(App& a) {
    static char body[600];
    int o = 0, n = a.m().neighbourCount();
    o += snprintf(body + o, sizeof body - o, "%d neighbour%s\n", n, n == 1 ? "" : "s");
    for (int i = 0; i < n && o < (int)sizeof body - 64; i++) {
      model::Neighbour nb;
      if (!a.m().neighbour(i, nb)) continue;
      char key[12], snr[8], ago[12];
      fmtKey(key, sizeof key, nb.key, 4);
      fmtSnr(snr, sizeof snr, nb.snr_x4);
      fmtDur(ago, sizeof ago, nb.heardAgoS);
      o += snprintf(body + o, sizeof body - o, "%s\n  %s  snr %s  %s ago\n", nb.name[0] ? nb.name : "(unknown)", key, snr, ago);
    }
    openText(a, "Neighbours", body);
  }

  char _title[model::NAME_LEN + 8];
  bool _loggedIn = false, _showStats = false;
  int _pending = -1;
  model::Req _last = model::Req::Idle;
};

AdminScreen s_admin;
void AdminScreen::s_admin_pending(int which) { s_admin.setPending(which); }

}  // namespace

void openContact(App& a, int i) {
  s_contact.idx = i;
  s_contact.resetNav();
  a.push(&s_contact);
}

void openAdmin(App& a, int i) {
  if (s_admin.idx != i) s_admin.reset();
  s_admin.idx = i;
  s_admin.resetNav();
  a.push(&s_admin);
}

}}  // namespace ui::screens
