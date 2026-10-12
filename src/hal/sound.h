// Simple notification sounds: a few tones in sequence. Boards without a
// speaker simply don't provide one.
#pragma once
#include <stdint.h>

namespace hal {

struct Tone {
  uint16_t hz;     // 0 = silence
  uint16_t ms;
};

class Sound {
public:
  virtual ~Sound() {}
  // Start playing; must not block for the length of the sound.
  virtual void play(const Tone* tones, int n) = 0;
};

}  // namespace hal
