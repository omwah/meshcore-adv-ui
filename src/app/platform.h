// Device services the MeshCore adapter needs beyond what MeshCore provides.
// Each board implements this (ESP32 heap, GPS module, SD card...).
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include "model/model.h"

namespace fs { class FS; }

namespace app {

class Platform {
public:
  virtual ~Platform() {}
  virtual uint32_t heapFree() = 0;
  virtual uint32_t heapMin() = 0;
  virtual uint32_t heapMaxBlock() = 0;
  virtual bool charging() { return false; }
  virtual model::Gps gps() { return model::Gps(); }
  virtual void setGpsPower(bool on) { (void)on; }
  // Removable storage for backups; null when no card is present.
  virtual fs::FS* sd() { return nullptr; }
  virtual void reboot() = 0;
  // A phone has connected over Bluetooth and is not yet authenticated.
  virtual bool blePairing() { return false; }
  // Random bytes for keys and tags (the device overrides with its hardware RNG).
  virtual void randomBytes(uint8_t* out, int n) { for (int i = 0; i < n; i++) out[i] = (uint8_t)rand(); }
};

// Battery percentage from a Li-ion cell voltage: M5Unified's straight line,
// so it matches Launcher and other M5Stack firmware.
uint8_t batteryPercent(uint16_t mv);

}  // namespace app
