// LilyGo T-Deck Plus: peripheral power, the shared SPI bus, battery. Pin
// facts are in the board's platformio section.
#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <helpers/ESP32Board.h>

class TDeckBoard : public ESP32Board {
public:
  void begin();
  uint16_t getBattMilliVolts() override;
  const char* getManufacturerName() const override { return "LilyGo T-Deck"; }
};
