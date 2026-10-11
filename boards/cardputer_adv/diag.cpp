#include "diag.h"
#include <Arduino.h>
#include "ui/app.h"
#include "st7789_display.h"
#include "tca8418_keyboard.h"
#include "platform_cardputer.h"
#include "target.h"
#include "MyMesh.h"
#include <SPIFFS.h>

static char s_line[64];
static int s_len = 0;

static void run(char* c, ui::App& app, St7789Display& d, Tca8418Keyboard& k, CardputerPlatform& p) {
  if (!strncmp(c, "stat", 4)) {
    Serial.printf("stat: lora-cap %s, keyboard %s, sd %s, heap %u (min %u), asleep %d, batt %u mV (arduino analogRead %u mV at pin)\n",
                  board.loraCapPresent() ? "ok" : "MISSING", k.present() ? "ok" : "MISSING", p.sd() ? "yes" : "no",
                  (unsigned)p.heapFree(), (unsigned)p.heapMin(), app.asleep(), board.getBattMilliVolts(),
                  (unsigned)analogReadMilliVolts(PIN_VBAT_READ));
  } else if (!strcmp(c, "radio")) {
    NodePrefs* np = the_mesh.getNodePrefs();
    Serial.printf("radio: %.3f MHz bw %.1f sf %u cr %u tx %d dBm boost %u | rx %lu tx %lu err %lu | noise %d last rssi %.0f snr %.1f\n",
                  np->freq, np->bw, np->sf, np->cr, np->tx_power_dbm, np->rx_boosted_gain,
                  (unsigned long)radio_driver.getPacketsRecv(), (unsigned long)radio_driver.getPacketsSent(),
                  (unsigned long)radio_driver.getPacketsRecvErrors(), radio_driver.getNoiseFloor(),
                  radio_driver.getLastRSSI(), radio_driver.getLastSNR());
  } else if (!strcmp(c, "files")) {
    // Flash files and sizes, and the start of the node's public key.
    File root = SPIFFS.open("/");
    for (File f = root.openNextFile(); f; f = root.openNextFile()) Serial.printf("file %s %u\n", f.path(), (unsigned)f.size());
    const uint8_t* k = the_mesh.self_id.pub_key;
    Serial.printf("node key starts %02X%02X%02X%02X\n", k[0], k[1], k[2], k[3]);
  } else if (!strcmp(c, "txtest")) {
    // One 4-byte packet that is not valid MeshCore (nodes ignore it): shows
    // whether the transmitter works and RadioLib's error code if not.
    uint8_t junk[4] = {0xFF, 0x00, 0x00, 0x00};
    uint32_t t0 = millis();
    int16_t err = radio.transmit(junk, sizeof junk);
    uint32_t dt = millis() - t0;
    radio_driver.begin();   // back to MeshCore's receive state
    Serial.printf("txtest: err %d after %lu ms (0 = sent)\n", err, (unsigned long)dt);
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
    Serial.println("? stat | radio | files | txtest | bl <n>|gpio | blf <hz> <duty> | fill <rrggbb> | init | shot | key <row> <col>");
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
