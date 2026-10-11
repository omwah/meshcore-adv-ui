// MeshCore "variant" for the Cardputer ADV + Cap LoRa-1262, outside the
// MeshCore tree (pins come from the board's platformio section).
#pragma once
#define RADIOLIB_STATIC_ONLY 1
#include <RadioLib.h>
#include <SPI.h>
#include <helpers/radiolib/RadioLibWrappers.h>
#include <helpers/radiolib/CustomSX1262Wrapper.h>
#include <helpers/AutoDiscoverRTCClock.h>
#include <helpers/sensors/EnvironmentSensorManager.h>
#include <helpers/sensors/MicroNMEALocationProvider.h>
#include "cardputer_board.h"

extern CardputerBoard board;
extern RADIO_CLASS radio;
extern WRAPPER_CLASS radio_driver;
extern AutoDiscoverRTCClock rtc_clock;
extern EnvironmentSensorManager sensors;
extern MicroNMEALocationProvider gps;
extern SPIClass loraSpi;   // shared with the SD card slot

bool radio_init();
mesh::LocalIdentity radio_new_identity();
