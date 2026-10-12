// Device services for the adapter: heap figures, GPS, SD card, reboot.
#pragma once
#include <SD.h>
#include <esp_random.h>
#include "app/platform.h"

class CardputerPlatform : public app::Platform {
public:
  uint32_t heapFree() override;
  uint32_t heapMin() override;
  uint32_t heapMaxBlock() override;
  model::Gps gps() override;
  void setGpsPower(bool on) override;
  fs::FS* sd() override;
  void reboot() override;
  bool blePairing() override { return blePairingFn && blePairingFn(); }
  bool (*blePairingFn)() = nullptr;
  void randomBytes(uint8_t* out, int n) override { esp_fill_random(out, n); }

private:
  bool _sdTried = false, _sdOk = false;
  uint32_t _sdCheckAt = 0;
};
