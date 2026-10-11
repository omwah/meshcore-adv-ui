#include "runner.h"
#include <stdlib.h>
#include <string.h>

namespace sim {

namespace {

struct Ctx {
  ui::App& app;
  World& world;
  uint32_t& now;
  void advance(uint32_t until) {
    while (now < until) {
      now += 10;
      world.step(now);
      app.loop(now);
    }
  }
};

}  // namespace

int runScript(FILE* in, const char* out, ui::App& app, ui::Screen* first, SimKeyboard& kb, SimDisplay& disp,
              uint32_t& now, World& world) {
  Ctx c{app, world, now};
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
    const char* arg = p + used;

    if (!started) {
      if (world.setup(cmd, arg)) continue;
      world.start();
      app.begin(first);
      c.advance(now + 1500);   // past the splash
      started = true;
    }

    if (!strcmp(cmd, "key") || !strcmp(cmd, "hold")) {
      char legend[16] = {};
      int n = 1;
      sscanf(arg, "%15s %d", legend, &n);
      int r, col;
      if (!SimKeyboard::find(legend, r, col)) {
        fprintf(stderr, "line %d: unknown key '%s'\n", lineNo, legend);
        return 2;
      }
      uint32_t t = now + 10;
      if (!strcmp(cmd, "hold")) {
        kb.at(t, r, col, true);
        kb.at(t + n, r, col, false);
        c.advance(t + n + 100);
      } else {
        for (int i = 0; i < n; i++, t += 160) {
          kb.at(t, r, col, true);
          kb.at(t + 40, r, col, false);
        }
        c.advance(t + 60);
      }
    } else if (!strcmp(cmd, "type")) {
      uint32_t t = now + 10;
      int sr, sc;
      SimKeyboard::find("shift", sr, sc);
      for (const char* s = arg; *s; s++) {
        int r, col;
        bool shift;
        if (!SimKeyboard::findChar(*s, r, col, shift)) continue;
        if (shift) kb.at(t, sr, sc, true);
        kb.at(t + 5, r, col, true);
        kb.at(t + 35, r, col, false);
        if (shift) kb.at(t + 40, sr, sc, false);
        t += 90;
        c.advance(t);   // so text mode applies from the second key on
      }
      c.advance(t + 50);
    } else if (!strcmp(cmd, "wait")) {
      c.advance(now + (uint32_t)atoi(arg));
    } else if (!strcmp(cmd, "shot")) {
      // Land on a whole second plus a little, so the caret is always drawn.
      c.advance(((now / 1000) + 1) * 1000 + 20);
      char path[400];
      snprintf(path, sizeof path, "%s/%s.ppm", out, arg);
      if (!disp.writePpm(path)) { perror(path); return 1; }
    } else if (!strcmp(cmd, "theme")) {
      model::Prefs pr = app.m().prefs();
      pr.theme = (uint8_t)atoi(arg);
      app.m().setPrefs(pr);
      app.applyPrefs();
      c.advance(now + 20);
    } else {
      char err[160] = {};
      if (!world.command(cmd, arg, err, sizeof err)) {
        fprintf(stderr, "line %d: unknown command '%s'\n", lineNo, cmd);
        return 2;
      }
      if (err[0]) {
        fprintf(stderr, "line %d: %s\n", lineNo, err);
        return 3;
      }
      c.advance(now + 20);
    }
  }
  return 0;
}

}  // namespace sim
