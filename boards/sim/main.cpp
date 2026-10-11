// Host simulator: runs the real UI against MockModel with a virtual clock.
//
//   sim [-s script] [-o outdir]          (script from stdin by default)
//
// Commands (see runner.cpp for key/hold/type/wait/shot/theme):
//   rx dm <contact> <text>, rx chan <slot> <sender>|<text>
//   ble on|off, gps on|off|fix|nofix
//   fresh, empty                (before anything else)
#ifndef PIO_UNIT_TESTING
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui/app.h"
#include "ui/screens/screens.h"
#include "mock_model.h"
#include "runner.h"

namespace {

class MockWorld : public sim::World {
public:
  explicit MockWorld(sim::MockModel& m) : _m(m) {}
  bool setup(const char* cmd, const char*) override {
    if (!strcmp(cmd, "fresh")) { _m.setFresh(true); return true; }
    if (!strcmp(cmd, "empty")) { _m.setEmpty(); return true; }
    return false;
  }
  void step(uint32_t) override { _m.step(); }
  bool command(const char* cmd, const char* arg, char* err, size_t n) override {
    if (!strcmp(cmd, "rx")) {
      char kind[8] = {};
      int idx = 0, u = 0;
      sscanf(arg, "%7s %d %n", kind, &idx, &u);
      const char* rest = arg + u;
      if (!strcmp(kind, "dm")) {
        _m.rxDirect(idx, rest);
      } else {
        const char* bar = strchr(rest, '|');
        if (!bar) { snprintf(err, n, "rx chan needs sender|text"); return true; }
        char sender[32];
        snprintf(sender, sizeof sender, "%.*s", (int)(bar - rest), rest);
        _m.rxChannel(idx, sender, bar + 1);
      }
      return true;
    }
    if (!strcmp(cmd, "ble")) { _m.setBle(!strcmp(arg, "on")); return true; }
    if (!strcmp(cmd, "gps")) {
      if (!strcmp(arg, "on") || !strcmp(arg, "off")) {
        model::Prefs pr = _m.prefs();
        pr.gpsEnabled = !strcmp(arg, "on");
        _m.setPrefs(pr);
      } else {
        _m.setGpsFix(!strcmp(arg, "fix"));
      }
      return true;
    }
    return false;
  }

private:
  sim::MockModel& _m;
};

uint32_t g_now = 0;

}  // namespace

int main(int argc, char** argv) {
  const char* script = nullptr;
  const char* out = ".";
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-s") && i + 1 < argc) script = argv[++i];
    else if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
  }
  FILE* in = script ? fopen(script, "r") : stdin;
  if (!in) { perror(script); return 1; }

  static sim::MockModel model;
  model.setClock(&g_now);
  static sim::SimDisplay display;
  static sim::SimKeyboard kb(&g_now);
  static ui::App app(model, display, kb);
  static MockWorld world(model);
  return sim::runScript(in, out, app, ui::screens::boot(), kb, display, g_now, world);
}
#endif
