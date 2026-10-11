#include <Arduino.h>
#include "target.h"

CardputerBoard board;

// LoRa and the SD card share one SPI bus (SCK 40, MISO 39, MOSI 14).
SPIClass loraSpi(HSPI);
RADIO_CLASS radio = new Module(P_LORA_NSS, P_LORA_DIO_1, P_LORA_RESET, P_LORA_BUSY, loraSpi);
WRAPPER_CLASS radio_driver(radio, board);

ESP32RTCClock fallback_clock;
AutoDiscoverRTCClock rtc_clock(fallback_clock);
MicroNMEALocationProvider gps(Serial1, &rtc_clock);
EnvironmentSensorManager sensors(gps);

bool radio_init() {
  fallback_clock.begin();
  rtc_clock.begin(Wire);
  // Keep the SD card deselected while the radio initialises the bus.
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);
  return radio.std_init(&loraSpi);
}

mesh::LocalIdentity radio_new_identity() {
  RadioNoiseListener rng(radio);
  return mesh::LocalIdentity(&rng);
}
