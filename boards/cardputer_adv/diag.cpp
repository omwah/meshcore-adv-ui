#include "diag.h"
#include <Arduino.h>
#include "ui/app.h"
#include "st7789_display.h"
#include "tca8418_keyboard.h"
#include "platform_cardputer.h"
#include "target.h"

static char s_line[64];
static int s_len = 0;

static void run(char* c, ui::App& app, St7789Display& d, Tca8418Keyboard& k, CardputerPlatform& p) {
  if (!strncmp(c, "stat", 4)) {
    Serial.printf("stat: lora-cap %s, keyboard %s, sd %s, heap %u (min %u), asleep %d\n",
                  board.loraCapPresent() ? "ok" : "MISSING", k.present() ? "ok" : "MISSING", p.sd() ? "yes" : "no",
                  (unsigned)p.heapFree(), (unsigned)p.heapMin(), app.asleep());
  } else if (!strncmp(c, "bl ", 3)) {
    d.backlightRaw(!strcmp(c + 3, "gpio") ? -1 : atoi(c + 3));
    Serial.println("ok");
  } else if (!strncmp(c, "blf ", 4)) {
    unsigned hz = 0, duty = 128;
    sscanf(c + 4, "%u %u", &hz, &duty);
    d.backlightPwm(hz, (uint8_t)duty);
    Serial.println("ok");
  } else if (!strncmp(c, "fill ", 5)) {
    uint32_t rgb = strtoul(c + 5, nullptr, 16);
    uint16_t v = (uint16_t)(((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x001F));
    uint16_t row[240];   // one row at a time: no static buffer for a diagnostic
    for (auto& px : row) px = (uint16_t)((v >> 8) | (v << 8));
    for (int y = 0; y < 135; y++) d.pushRows(y, 1, row);
    Serial.println("ok");
  } else if (!strcmp(c, "init")) {
    d.reinit();
    app.wake();
    app.invalidate();
    Serial.println("ok");
  } else if (!strcmp(c, "shot")) {
    Serial.print("SHOT ");
    for (int i = 0; i < 16; i++) Serial.printf("%04x", app.theme().pal[i]);
    Serial.print(' ');
    const uint8_t* fb = app.canvas().raw();
    for (int i = 0; i < 240 / 2 * 135; i++) Serial.printf("%02x", fb[i]);
    Serial.println();
  } else if (!strncmp(c, "key ", 4)) {
    int r = -1, col = -1;
    sscanf(c + 4, "%d %d", &r, &col);
    k.inject(r, col, true);
    k.inject(r, col, false);
    Serial.println("ok");
  } else if (c[0]) {
    Serial.println("? stat | bl <n>|gpio | fill <rrggbb> | init | shot | key <row> <col>");
  }
}

void diagPoll(ui::App& app, St7789Display& d, Tca8418Keyboard& k, CardputerPlatform& p) {
  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch == '\r') continue;
    if (ch == '\n') {
      s_line[s_len] = 0;
      run(s_line, app, d, k, p);
      s_len = 0;
    } else if (s_len < (int)sizeof s_line - 1) {
      s_line[s_len++] = ch;
    }
  }
}
