// The T-Deck's speaker: an I2S amplifier on BCK 7, WS 5, DOUT 6 (LilyGo's
// pin list). Tones as on the Cardputer: square waves straight into the DMA
// ring, so play() returns at once.
#pragma once
#include "hal/sound.h"

class I2sSound : public hal::Sound {
public:
  bool begin();
  void play(const hal::Tone* tones, int n) override;
  bool present() const { return _ok; }

private:
  bool _ok = false;
};
