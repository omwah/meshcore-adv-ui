// Serial bring-up commands (Bluetooth build only: in the USB build the port
// carries the companion protocol). One command per line:
//   stat                 status line (no keys, names or addresses)
//   bl <0-255>|gpio      backlight PWM duty, or the pin driven high directly
//   fill <rrggbb>        fill the panel with a colour, bypassing the UI
//   init                 re-run the panel initialisation
//   shot                 dump the UI frame: "SHOT <palette hex> <framebuffer hex>"
//   key <row> <col>      tap a key on the 4x14 matrix (as printed)
#pragma once
namespace ui { class App; }
class St7789Display;
class Tca8418Keyboard;
class Esp32Platform;

// Set by main: plays the message chime (diag 'beep').
extern void (*g_diagBeep)();

void diagPoll(ui::App& app, St7789Display& d, Tca8418Keyboard& k, Esp32Platform& p);
