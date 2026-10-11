#pragma once
#include <stdint.h>
class TwoWire {
public:
  bool begin(int = -1, int = -1, uint32_t = 0) { return true; }
};
extern TwoWire Wire;
