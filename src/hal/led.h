// A status light (an RGB LED). Boards without one simply don't provide one.
#pragma once
#include <stdint.h>

namespace hal {

class Led {
public:
  virtual ~Led() {}
  // Show a colour; 0, 0, 0 is off. Must not block.
  virtual void set(uint8_t r, uint8_t g, uint8_t b) = 0;
};

}  // namespace hal
