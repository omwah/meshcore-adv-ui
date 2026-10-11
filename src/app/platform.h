// Device services the MeshCore adapter needs beyond what MeshCore provides.
// Each board implements this (ESP32 heap, GPS module, SD card...).
#pragma once
#include <stdint.h>
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
};

// Battery percentage from a Li-ion cell voltage (open-circuit curve).
uint8_t batteryPercent(uint16_t mv);

}  // namespace app
