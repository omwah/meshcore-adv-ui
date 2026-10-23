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
              uint32_t& now, World& world, SimTouch* touch) {
  Ctx c{app, world, now};
  char line[512];
  bool started = false, preStart = false;
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
      // Theme and text size before boot apply to the splash too.
      if (!strcmp(cmd, "theme") || !strcmp(cmd, "textsize") || !strcmp(cmd, "vim")) {
        if (!preStart) { world.attach(app, first); world.start(); preStart = true; }
        model::Prefs pr = app.m().prefs();
        if (cmd[0] == 'v') pr.vimKeys = !strcmp(arg, "on");
        else if (cmd[0] == 't' && cmd[1] == 'h') pr.theme = (uint8_t)atoi(arg);
        else pr.textSize = (uint8_t)atoi(arg);
        app.m().setPrefs(pr);
        continue;
      }
      if (!preStart) { world.attach(app, first); world.start(); }
      app.begin(first);
      if (!strcmp(cmd, "splashshot")) {   // capture the boot splash itself
        c.advance(now + 700);
        char path[400];
        snprintf(path, sizeof path, "%s/%s.ppm", out, arg);
        if (!disp.writePpm(path)) { perror(path); return 1; }
      }
      c.advance(2300);   // past the splash
      started = true;
      if (!strcmp(cmd, "splashshot")) continue;
    }

    if (!strcmp(cmd, "key") || !strcmp(cmd, "hold")) {
      char legend[32] = {};
      int n = 1;
      sscanf(arg, "%31s %d", legend, &n);
      Chord ch;
      if (!SimKeyboard::find(legend, ch)) {
        fprintf(stderr, "line %d: unknown key '%s'\n", lineNo, legend);
        return 2;
      }
      // Modifiers ("ctrl+o") go down just before the key and up just after.
      const int k = ch.n - 1;
      uint32_t t = now + 10;
      if (!strcmp(cmd, "hold")) {
        for (int i = 0; i < k; i++) kb.at(t - 5, ch.row[i], ch.col[i], true);
        kb.at(t, ch.row[k], ch.col[k], true);
        kb.at(t + n, ch.row[k], ch.col[k], false);
        for (int i = 0; i < k; i++) kb.at(t + n + 5, ch.row[i], ch.col[i], false);
        c.advance(t + n + 100);
      } else {
        // One press at a time: the keyboard's event queue is small.
        for (int j = 0; j < n; j++, t += 160) {
          for (int i = 0; i < k; i++) kb.at(t - 5, ch.row[i], ch.col[i], true);
          kb.at(t, ch.row[k], ch.col[k], true);
          kb.at(t + 40, ch.row[k], ch.col[k], false);
          for (int i = 0; i < k; i++) kb.at(t + 45, ch.row[i], ch.col[i], false);
          c.advance(t + 60);
        }
      }
    } else if (!strcmp(cmd, "type")) {
      uint32_t t = now + 10;
      for (const char* s = arg; *s; s++) {
        Chord ch;
        if (!SimKeyboard::findChar(*s, ch)) continue;
        const int k = ch.n - 1;
        for (int i = 0; i < k; i++) kb.at(t, ch.row[i], ch.col[i], true);
        kb.at(t + 5, ch.row[k], ch.col[k], true);
        kb.at(t + 35, ch.row[k], ch.col[k], false);
        for (int i = 0; i < k; i++) kb.at(t + 40, ch.row[i], ch.col[i], false);
        t += 90;
        c.advance(t);   // so text mode applies from the second key on
      }
      c.advance(t + 50);
    } else if (!strcmp(cmd, "repeat")) {
      // repeat <count> <gap ms> <command...>: '#' in the command becomes
      // 001, 002...
      int count = 0, gap = 0, u = 0;
      sscanf(arg, "%d %d %n", &count, &gap, &u);
      for (int i = 1; i <= count; i++) {
        char sub[512], num[8], rcmd[16] = {};
        snprintf(num, sizeof num, "%03d", i);
        int o = 0;
        for (const char* s = arg + u; *s && o < (int)sizeof sub - 4; s++) {
          if (*s == '#') { memcpy(sub + o, num, 3); o += 3; }
          else sub[o++] = *s;
        }
        sub[o] = 0;
        int ru = 0;
        sscanf(sub, "%15s %n", rcmd, &ru);
        char err[160] = {};
        if (!world.command(rcmd, sub + ru, err, sizeof err) || err[0]) {
          fprintf(stderr, "line %d: %s\n", lineNo, err[0] ? err : "unknown command");
          return 3;
        }
        c.advance(now + (uint32_t)gap);
      }
    } else if (!strcmp(cmd, "wait")) {
      c.advance(now + (uint32_t)atoi(arg));
    } else if (!strcmp(cmd, "shot")) {
      // Land on a whole second plus a little, so the caret is always drawn.
      c.advance(((now / 1000) + 1) * 1000 + 20);
      char path[400];
      snprintf(path, sizeof path, "%s/%s.ppm", out, arg);
      if (!disp.writePpm(path)) { perror(path); return 1; }
    } else if (!strcmp(cmd, "textsize")) {
      model::Prefs pr = app.m().prefs();
      pr.textSize = (uint8_t)atoi(arg);
      app.m().setPrefs(pr);
      app.applyPrefs();
      c.advance(now + 20);
    } else if (touch && (!strcmp(cmd, "tap") || !strcmp(cmd, "long"))) {
      // tap x y / long x y: a finger down and up in place.
      int x = 0, y = 0;
      sscanf(arg, "%d %d", &x, &y);
      uint32_t t = now + 10, held = cmd[0] == 'l' ? 700 : 80;
      touch->at(t, x, y, hal::TouchEvent::DOWN);
      touch->at(t + held, x, y, hal::TouchEvent::UP);
      c.advance(t + held + 100);
    } else if (touch && !strcmp(cmd, "swipe")) {
      // swipe x1 y1 x2 y2 [ms]: down, moves every 16 ms, up.
      int x1 = 0, y1 = 0, x2 = 0, y2 = 0, ms = 300;
      sscanf(arg, "%d %d %d %d %d", &x1, &y1, &x2, &y2, &ms);
      uint32_t t = now + 10;
      touch->at(t, x1, y1, hal::TouchEvent::DOWN);
      int steps = ms / 16 > 1 ? ms / 16 : 1;
      for (int i = 1; i <= steps; i++)
        touch->at(t + i * 16, x1 + (x2 - x1) * i / steps, y1 + (y2 - y1) * i / steps, hal::TouchEvent::MOVE);
      touch->at(t + steps * 16 + 10, x2, y2, hal::TouchEvent::UP);
      c.advance(t + steps * 16 + 100);
    } else if (!strcmp(cmd, "vim")) {
      model::Prefs pr = app.m().prefs();
      pr.vimKeys = !strcmp(arg, "on");
      app.m().setPrefs(pr);
      c.advance(now + 20);
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
