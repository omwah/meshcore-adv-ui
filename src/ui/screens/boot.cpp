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
  void onEnter(App& a) override {
    _start = a.nowMs();
    _done = false;
  }
  bool tick(App& a, uint32_t now) override {
    if (_done || now - _start < 1200) return false;
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
    c.fillRect(0, 44, r.w, 34, C_HEAD);
    c.hline(0, 78, r.w, C_ACCENT);
    const char* t = "MeshCore";
    c.text((r.w - Canvas::textWidth(t, a.fList()) * 1) / 2, 47, t, a.fList(), C_HTXT);
    const char* s = "keyboard edition";
    c.text((r.w - Canvas::textWidth(s, a.fMed())) / 2, 63, s, a.fMed(), C_ACCENT);
    char l[48];
    snprintf(l, sizeof l, "%s  " MC_UI_VERSION, a.m().prefs().nodeName);
    c.text((r.w - Canvas::textWidth(l, a.fSmall())) / 2, 92, l, a.fSmall(), C_DIM);
    // Progress dots.
    int n = (int)((a.nowMs() - _start) / 200) % 4;
    for (int i = 0; i < 3; i++) c.fillRect(r.w / 2 - 10 + i * 8, 108, 4, 4, i < n ? C_ACCENT : C_ZEBRA);
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
