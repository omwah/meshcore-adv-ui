// Splash shown while the mesh starts, then Home. On a fresh install with a
// backup on the SD card, offers to restore it first.
#include "ui/screens/screens.h"
#include <stdio.h>

namespace ui { namespace screens {

namespace {

class BootScreen : public Screen {
public:
  const char* title(App&) override { return ""; }
  bool fullscreen() override { return true; }
  void onEnter(App&) override {
    _start = 0;      // set on the first frame: setup may take seconds before it
    _done = false;
  }
  bool tick(App& a, uint32_t now) override {
    if (!_start) _start = now ? now : 1;
    if (_done || now - _start < 2000) return true;   // keep animating the dots
    _done = true;
    a.replace(home());
    if (a.m().restoreOffered()) {
      a.confirm("Fresh install. Restore settings, contacts and channels from the SD card backup?",
                [](App& app, int) {
                  if (!app.m().restore(false)) app.toast("Restore failed: backup unreadable", C_ERR);
                });
    }
    return true;
  }
  void draw(App& a, Canvas& c, const Rect& r) override {
    c.fillRect(r.x, r.y, r.w, r.h, C_BG);
    const mc_font_t* big = &font_xlarge;
    const mc_font_t* med = &font_large;
    const mc_font_t* sm = &font_medium;
    // "MeshCore" on top, the node's name large in a band, then its key and
    // the firmware version.
    const char* t = "MeshCore";
    c.text((r.w - Canvas::textWidth(t, med)) / 2, 10, t, med, C_ACCENT);
    int bandY = 34, bandH = big->height + 12;
    c.fillRect(0, bandY, r.w, bandH, C_HEAD);
    c.hline(0, bandY + bandH, r.w, C_ACCENT);
    const char* name = a.m().prefs().nodeName;
    int nw = Canvas::textWidth(name, big);
    if (nw <= r.w - 12) c.text((r.w - nw) / 2, bandY + 6, name, big, C_HTXT);
    else a.fitText(c, 6, bandY + 6, name, big, C_HTXT, r.w - 12, true);
    uint8_t k[6];
    a.m().selfKey(k);
    char l[40];
    snprintf(l, sizeof l, "%02X%02X%02X%02X  " MC_UI_VERSION, k[0], k[1], k[2], k[3]);
    c.text((r.w - Canvas::textWidth(l, sm)) / 2, bandY + bandH + 10, l, sm, C_DIM);
    // Progress dots.
    int n = (int)((a.nowMs() - _start) / 200) % 4;
    for (int i = 0; i < 3; i++) c.fillRect(r.w / 2 - 10 + i * 8, r.h - 14, 4, 4, i < n ? C_ACCENT : C_ZEBRA);
  }
  void onKey(App&, const Key&) override {}

private:
  uint32_t _start = 0;
  bool _done = false;
};

BootScreen s_boot;

}  // namespace

Screen* boot() { return &s_boot; }

}}  // namespace ui::screens
