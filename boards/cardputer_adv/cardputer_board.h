// M5Stack Cardputer ADV with the Cap LoRa-1262: power, battery and the
// LoRa cap's IO expander. Pin facts are in the board's platformio section.
#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <helpers/ESP32Board.h>

class CardputerBoard : public ESP32Board {
public:
  void begin();
  uint16_t getBattMilliVolts() override;
  const char* getManufacturerName() const override { return "M5Stack Cardputer ADV"; }
  bool loraCapPresent() const { return _capFound; }

private:
  bool _capFound = false;
};
