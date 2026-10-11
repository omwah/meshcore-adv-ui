// Host simulator: runs the real UI against MockModel with a virtual clock.
//
//   sim [-s script] [-o outdir]          (script from stdin by default)
//
// Script commands, one per line ('#' starts a comment):
//   key <legend> [n]       tap a Cardputer key n times (legend as printed, or
//                          up/down/left/right/esc/enter aliases)
//   hold <legend> <ms>     hold a key down
//   type <text>            type text (shift chosen automatically)
//   wait <ms>              let time pass
//   shot <name>            write <outdir>/<name>.ppm (240x135 RGB)
//   theme <n>              switch theme
//   rx dm <contact> <text>, rx chan <slot> <sender>|<text>
//   ble on|off, gps on|off|fix|nofix, fresh, empty
#ifndef PIO_UNIT_TESTING
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui/app.h"
#include "ui/screens/screens.h"
#include "mock_model.h"
#include "sim_keyboard.h"
#include "sim_display.h"

static uint32_t g_now = 0;

static void advance(ui::App& app, sim::MockModel& m, uint32_t until) {
  while (g_now < until) {
    g_now += 10;
    m.step();
    app.loop(g_now);
  }
}

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

  char line[512];
  bool started = false;
  int lineNo = 0;
  while (fgets(line, sizeof line, in)) {
    lineNo++;
    line[strcspn(line, "\r\n")] = 0;
    char* p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (!*p || *p == '#') continue;
    char cmd[16] = {};
    int used = 0;
    sscanf(p, "%15s %n", cmd, &used);
    char* arg = p + used;

    // Commands that shape the model run before the UI starts.
    if (!strcmp(cmd, "fresh")) { model.setFresh(true); continue; }
    if (!strcmp(cmd, "empty")) { model.setEmpty(); continue; }
    if (!started) {
      app.begin(ui::screens::boot());
      advance(app, model, g_now + 1500);   // past the splash
      started = true;
    }

    if (!strcmp(cmd, "key") || !strcmp(cmd, "hold")) {
      char legend[16] = {};
      int n = 1;
      sscanf(arg, "%15s %d", legend, &n);
      int r, c;
      if (!sim::SimKeyboard::find(legend, r, c)) { fprintf(stderr, "line %d: unknown key '%s'\n", lineNo, legend); return 2; }
      uint32_t t = g_now + 10;
      if (!strcmp(cmd, "hold")) {
        kb.at(t, r, c, true);
        kb.at(t + n, r, c, false);
        advance(app, model, t + n + 100);
      } else {
        for (int i = 0; i < n; i++, t += 160) {
          kb.at(t, r, c, true);
          kb.at(t + 40, r, c, false);
        }
        advance(app, model, t + 60);
      }
    } else if (!strcmp(cmd, "type")) {
      uint32_t t = g_now + 10;
      int shiftR, shiftC;
      sim::SimKeyboard::find("shift", shiftR, shiftC);
      for (const char* s = arg; *s; s++) {
        int r, c;
        bool shift;
        if (!sim::SimKeyboard::findChar(*s, r, c, shift)) continue;
        if (shift) kb.at(t, shiftR, shiftC, true);
        kb.at(t + 5, r, c, true);
        kb.at(t + 35, r, c, false);
        if (shift) kb.at(t + 40, shiftR, shiftC, false);
        t += 90;
        // Feed in chunks so text mode switches (after the first key) apply.
        advance(app, model, t);
      }
      advance(app, model, t + 50);
    } else if (!strcmp(cmd, "wait")) {
      advance(app, model, g_now + (uint32_t)atoi(arg));
    } else if (!strcmp(cmd, "shot")) {
      // Hold the caret on so shots don't depend on blink phase.
      uint32_t t = ((g_now / 1000) + 1) * 1000 + 20;
      advance(app, model, t);
      char path[400];
      snprintf(path, sizeof path, "%s/%s.ppm", out, arg);
      if (!display.writePpm(path)) { perror(path); return 1; }
    } else if (!strcmp(cmd, "theme")) {
      model::Prefs pr = model.prefs();
      pr.theme = (uint8_t)atoi(arg);
      model.setPrefs(pr);
      app.applyPrefs();
      advance(app, model, g_now + 20);
    } else if (!strcmp(cmd, "rx")) {
      char kind[8] = {};
      int idx = 0, u = 0;
      sscanf(arg, "%7s %d %n", kind, &idx, &u);
      const char* rest = arg + u;
      if (!strcmp(kind, "dm")) {
        model.rxDirect(idx, rest);
      } else {
        char sender[32] = {};
        const char* bar = strchr(rest, '|');
        if (!bar) { fprintf(stderr, "line %d: rx chan needs sender|text\n", lineNo); return 2; }
        snprintf(sender, sizeof sender, "%.*s", (int)(bar - rest), rest);
        model.rxChannel(idx, sender, bar + 1);
      }
      advance(app, model, g_now + 20);
    } else if (!strcmp(cmd, "ble")) {
      model.setBle(!strcmp(arg, "on"));
    } else if (!strcmp(cmd, "gps")) {
      if (!strcmp(arg, "on") || !strcmp(arg, "off")) {
        model::Prefs pr = model.prefs();
        pr.gpsEnabled = !strcmp(arg, "on");
        model.setPrefs(pr);
      } else {
        model.setGpsFix(!strcmp(arg, "fix"));
      }
    } else {
      fprintf(stderr, "line %d: unknown command '%s'\n", lineNo, cmd);
      return 2;
    }
  }
  return 0;
}
#endif
