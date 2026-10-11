// Script runner shared by the simulators: key presses through the Cardputer
// keymap, waits on a virtual clock, screenshots. Each simulator adds its own
// commands for the world it simulates (mock data, or real MeshCore nodes).
#pragma once
#include <stdint.h>
#include <stdio.h>
#include "ui/app.h"
#include "sim_keyboard.h"
#include "sim_display.h"

namespace sim {

class World {
public:
  virtual ~World() {}
  // Commands allowed before the UI starts (e.g. "fresh"); true if handled.
  virtual bool setup(const char* cmd, const char* arg) { (void)cmd; (void)arg; return false; }
  // Called once, just before the UI's first frame.
  virtual void start() {}
  // Advance everything except the UI to `now` (ms).
  virtual void step(uint32_t now) = 0;
  // World-specific commands after start; true if handled. Set `err` on failure.
  virtual bool command(const char* cmd, const char* arg, char* err, size_t errLen) {
    (void)cmd; (void)arg; (void)err; (void)errLen;
    return false;
  }
};

// Runs a script; returns a process exit code. Errors go to stderr with the
// line number. Every `shot` writes <outdir>/<name>.ppm.
int runScript(FILE* in, const char* outdir, ui::App& app, ui::Screen* first, SimKeyboard& kb, SimDisplay& disp,
              uint32_t& now, World& world);

}  // namespace sim
