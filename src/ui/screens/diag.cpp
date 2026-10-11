// Diagnostics pages, the received-packet log, GPS status and heard adverts.
#include "ui/screens/screens.h"
#include "ui/widgets.h"
#include "ui/icons.h"
#include <stdio.h>
#include <string.h>

namespace ui { namespace screens {

namespace {

// Horizontal meter: label left, bar, value right.
void meter(App& a, Canvas& c, int x, int y, int w, const char* label, const char* value,
           int num, int lo, int hi, uint8_t col) {
  const mc_font_t* f = a.fSmall();
  c.text(x, y, label, f, C_DIM);
  int bx = x + 42, bw = w - 42 - 44;
  c.fillRect(bx, y + 1, bw, 6, C_ZEBRA);
  int v = num < lo ? lo : num > hi ? hi : num;
  int fw = hi > lo ? (v - lo) * bw / (hi - lo) : 0;
  c.fillRect(bx, y + 1, fw, 6, col);
  c.text(x + w - Canvas::textWidth(value, f), y, value, f, C_TEXT);
}

void kv(App& a, Canvas& c, int x, int y, const char* k, const char* v, uint8_t col = C_TEXT) {
  c.text(x, y, k, a.fSmall(), C_DIM);
  c.text(x + 8 * 5 + 2, y, v, a.fSmall(), col);
}

// ------------------------------------------------------------- diagnostics

class DiagScreen : public Screen {
public:
  const char* title(App&) override { return _page == 0 ? "Diagnostics: radio" : "Diagnostics: system"; }
  bool tick(App& a, uint32_t now) override {
    (void)a;
    if (now - _last >= 1000) { _last = now; return true; }
    return false;
  }
  void draw(App& a, Canvas& c, const Rect& r) override {
    model::Diag d = a.m().diag();
    int x = r.x + 4, w = r.w - 8, y = r.y + 3;
    char v[24], v2[24];
    // Page dots.
    for (int p = 0; p < 2; p++) c.fillRect(r.x + r.w - 14 + p * 6, r.y + r.h - 5, 4, 3, p == _page ? C_ACCENT : C_ZEBRA);

    if (_page == 0) {
      snprintf(v, sizeof v, "%d dBm", d.noiseFloor);
      meter(a, c, x, y, w, "noise", v, d.noiseFloor, -130, -80, d.noiseFloor > -95 ? C_ERR : d.noiseFloor > -110 ? C_WARN : C_OK);
      y += 11;
      snprintf(v, sizeof v, "%d dBm", d.lastRssi);
      meter(a, c, x, y, w, "rssi", v, d.lastRssi, -130, -40, d.lastRssi > -100 ? C_OK : d.lastRssi > -115 ? C_WARN : C_ERR);
      y += 11;
      fmtSnr(v2, sizeof v2, d.lastSnr_x4);
      snprintf(v, sizeof v, "%s dB", v2);
      meter(a, c, x, y, w, "snr", v, d.lastSnr_x4, -20 * 4, 12 * 4, snrColor(d.lastSnr_x4));
      y += 11;
      snprintf(v, sizeof v, "%u%%", d.dutyPct);
      meter(a, c, x, y, w, "duty", v, d.dutyPct, 0, 10, d.dutyPct >= 10 ? C_ERR : d.dutyPct >= 5 ? C_WARN : C_OK);
      y += 13;
      c.hline(x, y - 2, w, C_ZEBRA);
      int cx = x + w / 2;
      snprintf(v, sizeof v, "%lu", (unsigned long)d.rxPackets); kv(a, c, x, y, "rx", v);
      snprintf(v, sizeof v, "%lu", (unsigned long)d.txPackets); kv(a, c, cx, y, "tx", v);
      y += 9;
      snprintf(v, sizeof v, "%lu/%lu", (unsigned long)d.rxFlood, (unsigned long)d.rxDirect); kv(a, c, x, y, "rx f/d", v);
      snprintf(v, sizeof v, "%lu/%lu", (unsigned long)d.txFlood, (unsigned long)d.txDirect); kv(a, c, cx, y, "tx f/d", v);
      y += 9;
      snprintf(v, sizeof v, "%lu", (unsigned long)d.dups); kv(a, c, x, y, "dups", v, d.dups ? C_WARN : C_TEXT);
      snprintf(v, sizeof v, "%lu", (unsigned long)d.rxErrors); kv(a, c, cx, y, "errors", v, d.rxErrors ? C_ERR : C_TEXT);
      y += 9;
      fmtDur(v, sizeof v, d.airTxMs / 1000); kv(a, c, x, y, "air tx", v);
      fmtDur(v, sizeof v, d.airRxMs / 1000); kv(a, c, cx, y, "air rx", v);
      y += 9;
      snprintf(v, sizeof v, "%u", d.queueLen); kv(a, c, x, y, "queue", v, d.queueLen > 4 ? C_WARN : C_TEXT);
      snprintf(v, sizeof v, "%u", d.offlineQueue); kv(a, c, cx, y, "app q", v);
      y += 9;
      model::Radio rd = a.m().radio();
      snprintf(v, sizeof v, "%.3f/%g/SF%u/4:%u %ddBm", rd.freqMHz, rd.bwKHz, rd.sf, rd.cr, rd.txDbm);
      c.text(x, y + 1, v, a.fSmall(), C_ACCENT);
    } else {
      snprintf(v, sizeof v, "%luK", (unsigned long)(d.heapFree / 1024));
      meter(a, c, x, y, w, "heap", v, (int)(d.heapFree / 1024), 0, 160, d.heapFree < 20000 ? C_ERR : d.heapFree < 40000 ? C_WARN : C_OK);
      y += 11;
      snprintf(v, sizeof v, "%luK", (unsigned long)(d.heapMin / 1024));
      meter(a, c, x, y, w, "min", v, (int)(d.heapMin / 1024), 0, 160, d.heapMin < 15000 ? C_ERR : d.heapMin < 30000 ? C_WARN : C_OK);
      y += 11;
      snprintf(v, sizeof v, "%luK", (unsigned long)(d.heapMaxBlock / 1024));
      meter(a, c, x, y, w, "block", v, (int)(d.heapMaxBlock / 1024), 0, 128, d.heapMaxBlock < 8000 ? C_ERR : C_OK);
      y += 11;
      snprintf(v, sizeof v, "%u.%02uV %u%%", d.battMv / 1000, d.battMv % 1000 / 10, d.battPct);
      meter(a, c, x, y, w, "batt", v, d.battPct, 0, 100, d.charging ? C_OK : d.battPct <= 15 ? C_ERR : d.battPct <= 35 ? C_WARN : C_OK);
      y += 13;
      c.hline(x, y - 2, w, C_ZEBRA);
      int cx = x + w / 2;
      fmtDur(v, sizeof v, d.uptimeS); kv(a, c, x, y, "uptime", v);
      kv(a, c, cx, y, "power", d.charging ? "charging" : "battery", d.charging ? C_OK : C_TEXT);
      y += 9;
      snprintf(v, sizeof v, "%u/%u", d.contacts, d.contactsMax); kv(a, c, x, y, "contacts", v, d.contacts * 10 >= d.contactsMax * 9 ? C_WARN : C_TEXT);
      snprintf(v, sizeof v, "%u/%u", d.channels, d.channelsMax); kv(a, c, cx, y, "channels", v);
      y += 9;
      snprintf(v, sizeof v, "%s", a.m().bleAvailable() ? (a.m().bleConnected() ? "connected" : a.m().prefs().bleEnabled ? "advertising" : "off") : "not built");
      kv(a, c, x, y, "ble", v, a.m().bleConnected() ? C_OK : C_TEXT);
      y += 9;
      uint8_t k[6];
      a.m().selfKey(k);
      snprintf(v, sizeof v, "%02X%02X%02X%02X", k[0], k[1], k[2], k[3]);
      kv(a, c, x, y, "node id", v, C_ACCENT);
      y += 9;
      kv(a, c, x, y, "fw", MC_UI_VERSION, C_DIM);
    }
  }
  void onKey(App& a, const Key& k) override {
    if (k.code == K_ESC) a.pop();
    else if ((k.code == K_LEFT || k.code == K_RIGHT || k.code == K_TAB) && !k.repeat) _page ^= 1;
  }
  const Hint* hints(App&) override {
    static const Hint h[] = {{"<>", "page"}, {"esc", "back"}, {nullptr, nullptr}};
    return h;
  }

private:
  int _page = 0;
  uint32_t _last = 0;
};

DiagScreen s_diag;

// ------------------------------------------------------------- packet log

const char* payloadName(uint8_t t) {
  static const char* const kNames[] = {"REQ", "RESP", "TXT", "ACK", "ADV", "GRP", "GDAT", "ANON",
                                       "PATH", "TRACE", "MULTI", "CTRL", "?", "?", "?", "RAW"};
  return kNames[t & 15];
}

uint8_t payloadColor(uint8_t t) {
  switch (t) {
    case 2: return C_CHAT;    // text
    case 3: return C_OK;      // ack
    case 4: return C_RPT;     // advert
    case 5: case 6: return C_CHAN;
    case 8: case 9: return C_ACCENT;
    default: return C_DIM;
  }
}

class LogScreen : public Screen {
public:
  const char* title(App& a) override {
    snprintf(_title, sizeof _title, "Packet log (%d)", a.m().rxLogCount());
    return _title;
  }
  bool tick(App& a, uint32_t) override {
    if (a.m().revision() != _rev) { _rev = a.m().revision(); return true; }
    return false;
  }
  void draw(App& a, Canvas& c, const Rect& r) override {
    const mc_font_t* f = a.fSmall();
    // Column header.
    c.fillRect(r.x, r.y, r.w, 10, C_ZEBRA);
    c.text(r.x + 3, r.y + 1, "age  type  rt hop  snr  rssi len src", f, C_ACCENT);
    int n = a.m().rxLogCount();
    int vis = (r.h - 11) / 10;
    _nav.clamp(n, vis);
    if (!n) c.text(r.x + 40, r.y + 40, "Nothing received yet", a.fMed(), C_DIM);
    for (int row = 0; row < vis && _nav.top + row < n; row++) {
      int i = _nav.top + row;
      model::RxLogEntry e;
      if (!a.m().rxLog(i, e)) continue;
      int y = r.y + 11 + row * 10;
      if (i == _nav.sel) c.fillRect(r.x, y - 1, r.w - 3, 10, C_SEL);
      char ago[8], snr[8], l[16];
      fmtAge(ago, sizeof ago, (a.m().uptimeMs() - e.ms) / 1000);
      c.text(r.x + 3, y, ago, f, C_DIM);
      c.text(r.x + 28, y, payloadName(e.payloadType), f, payloadColor(e.payloadType));
      c.text(r.x + 58, y, e.routeType == 1 || e.routeType == 0 ? "F" : "D", f, C_TEXT);
      snprintf(l, sizeof l, "%u", e.pathLen);
      c.text(r.x + 73, y, l, f, C_TEXT);
      fmtSnr(snr, sizeof snr, e.snr_x4);
      c.text(r.x + 88, y, snr, f, snrColor(e.snr_x4));
      snprintf(l, sizeof l, "%d", e.rssi);
      c.text(r.x + 118, y, l, f, C_TEXT);
      snprintf(l, sizeof l, "%u", e.len);
      c.text(r.x + 148, y, l, f, C_TEXT);
      snprintf(l, sizeof l, "%02X", e.srcHash);
      c.text(r.x + 168, y, l, f, C_DIM);
      if (e.dup) c.text(r.x + 186, y, "dup", f, C_WARN);
    }
    Rect sr{r.x, r.y + 11, r.w, r.h - 11};
    drawScrollbar(c, sr, n, _nav.top, vis);
  }
  void onKey(App& a, const Key& k) override {
    Rect r = a.content();
    if (k.code == K_ESC) a.pop();
    else _nav.key(k, a.m().rxLogCount(), (r.h - 11) / 10);
  }
  const Hint* hints(App&) override {
    static const Hint h[] = {{"^v", "scroll"}, {"esc", "back"}, {nullptr, nullptr}};
    return h;
  }

private:
  ListNav _nav;
  uint32_t _rev = 0;
  char _title[24];
};

LogScreen s_log;

// ------------------------------------------------------------- GPS

class GpsScreen : public Screen {
public:
  const char* title(App&) override { return "GPS"; }
  bool tick(App&, uint32_t now) override {
    if (now - _last >= 1000) { _last = now; return true; }
    return false;
  }
  void draw(App& a, Canvas& c, const Rect& r) override {
    model::Gps g = a.m().gps();
    const mc_font_t* f = a.fMed();
    int x = r.x + 6, y = r.y + 4;
    c.bitmap(x, y + 2, icon::gps, 8, 8, !g.enabled ? C_DIM : g.fix ? C_OK : C_WARN);
    c.text(x + 12, y, !g.enabled ? "Receiver off" : g.fix ? "3D fix" : "Searching for satellites", f,
           !g.enabled ? C_DIM : g.fix ? C_OK : C_WARN);
    y += 16;
    char l[48];
    snprintf(l, sizeof l, "Satellites  %u", g.sats);
    c.text(x, y, l, f, C_TEXT);
    // Satellite count as blocks.
    for (int i = 0; i < 12; i++) c.fillRect(x + 110 + i * 9, y + 2, 7, 8, i < g.sats ? (g.sats >= 6 ? C_OK : C_WARN) : C_ZEBRA);
    y += 14;
    if (g.fix) {
      snprintf(l, sizeof l, "Lat  %.5f", g.lat_e6 / 1e6);  c.text(x, y, l, f, C_TEXT); y += 13;
      snprintf(l, sizeof l, "Lon  %.5f", g.lon_e6 / 1e6);  c.text(x, y, l, f, C_TEXT); y += 13;
      snprintf(l, sizeof l, "Alt  %ld m   HDOP %u.%u", (long)(g.altCm / 100), g.hdop_x10 / 10, g.hdop_x10 % 10);
      c.text(x, y, l, f, C_TEXT); y += 13;
    } else {
      c.text(x, y, "Position unknown", f, C_DIM); y += 39;
    }
    const model::Prefs& p = a.m().prefs();
    snprintf(l, sizeof l, "share in adverts: %s   time sync: %s", p.gpsShareInAdvert ? "on" : "off", p.gpsTimeSyncAtBoot ? "on" : "off");
    c.text(x, y + 2, l, a.fSmall(), C_DIM);
  }
  void onKey(App& a, const Key& k) override {
    if (k.code == K_ESC) a.pop();
    else if (k.code == K_ENTER) {
      if (a.m().gpsSyncTime()) a.toast("Clock set from GPS", C_OK);
      else a.toast("No GPS fix yet", C_WARN);
    } else if (k.code == K_TAB) openSettings(a);
  }
  const Hint* hints(App&) override {
    static const Hint h[] = {{"ent", "set clock"}, {"tab", "settings"}, {"esc", "back"}, {nullptr, nullptr}};
    return h;
  }

private:
  uint32_t _last = 0;
};

GpsScreen s_gps;

// ------------------------------------------------------------- heard

class HeardScreen : public Screen {
public:
  bool repeatersOnly = false;
  const char* title(App& a) override {
    if (repeatersOnly) return a.m().discoverState() == model::Req::Busy ? "Discovering..." : "Repeaters nearby";
    return "Heard adverts";
  }
  bool tick(App& a, uint32_t) override {
    if (a.m().revision() != _rev) { _rev = a.m().revision(); rebuild(a); return true; }
    return false;
  }
  void onEnter(App& a) override { rebuild(a); }
  void draw(App& a, Canvas& c, const Rect& r) override {
    const int RH = 16;
    int vis = r.h / RH;
    _nav.clamp(_n, vis);
    if (!_n) {
      c.text(r.x + 20, r.y + 40, repeatersOnly ? "Listening for repeaters..." : "No adverts heard yet", a.fMed(), C_DIM);
      return;
    }
    for (int row = 0; row < vis && _nav.top + row < _n; row++) {
      int i = _idx[_nav.top + row];
      model::Contact ct;
      if (!a.m().heard(i, ct)) continue;
      int y = r.y + row * RH;
      if (_nav.top + row == _nav.sel) c.fillRect(r.x, y, r.w - 3, RH, C_SEL);
      else if (row & 1) c.fillRect(r.x, y, r.w - 3, RH, C_ZEBRA);
      uint8_t col = typeColor(ct.type);
      c.bitmap(r.x + 4, y + 4, typeIcon(ct.type), 8, 8, col);
      char snr[8], ago[8], l[24];
      fmtAgo(ago, sizeof ago, a.m().now(), ct.lastHeard);
      int x = r.x + r.w - 6 - Canvas::textWidth(ago, a.fSmall());
      c.text(x, y + 5, ago, a.fSmall(), C_DIM);
      fmtSnr(snr, sizeof snr, ct.snr_x4);
      snprintf(l, sizeof l, "%s", snr);
      x -= Canvas::textWidth(l, a.fSmall()) + 6;
      c.text(x, y + 5, l, a.fSmall(), snrColor(ct.snr_x4));
      x -= 13;
      drawSignal(c, x, y + 4, ct.snr_x4);
      if (ct.hops >= 0) {
        snprintf(l, sizeof l, "%d", ct.hops);
        x -= 10;
        c.text(x, y + 5, l, a.fSmall(), ct.hops == 0 ? C_OK : C_DIM);
      }
      c.text(r.x + 16, y, ct.name, a.fList(), C_TEXT, -1, x - r.x - 22);
    }
    drawScrollbar(c, r, _n, _nav.top, vis);
  }
  void onKey(App& a, const Key& k) override {
    Rect r = a.content();
    if (k.code == K_ESC) { a.pop(); return; }
    if (_nav.key(k, _n, r.h / 16)) return;
    if (k.code == K_ENTER && _n) {
      if (a.m().addHeard(_idx[_nav.sel])) a.toast("Added to contacts", C_OK);
      else a.toast("Already a contact, or contacts full", C_WARN);
    }
  }
  const Hint* hints(App&) override {
    static const Hint h[] = {{"ent", "add contact"}, {"^v", "move"}, {"esc", "back"}, {nullptr, nullptr}};
    return h;
  }

private:
  void rebuild(App& a) {
    _n = 0;
    for (int i = 0, n = a.m().heardCount(); i < n && _n < 64; i++) {
      model::Contact ct;
      if (!a.m().heard(i, ct)) continue;
      if (repeatersOnly && (ct.type != model::NodeType::Repeater || ct.hops != 0)) continue;
      _idx[_n++] = (uint8_t)i;
    }
  }
  ListNav _nav;
  uint8_t _idx[64];
  int _n = 0;
  uint32_t _rev = 0;
};

HeardScreen s_heard;

}  // namespace

void openDiag(App& a) { a.push(&s_diag); }
void openPacketLog(App& a) { a.push(&s_log); }
void openGps(App& a) { a.push(&s_gps); }
void openHeard(App& a, bool rptOnly) {
  s_heard.repeatersOnly = rptOnly;
  a.push(&s_heard);
}

}}  // namespace ui::screens
