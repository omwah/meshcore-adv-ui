// Firmware entry point for the Cardputer ADV. Mirrors the stock companion
// example's setup()/loop() (examples/companion_radio/main.cpp) so phone apps
// keep working, with our UI as the display task. When MeshCore changes that
// file, review this one (scripts/check-upstream-main.sh warns).
#include <Arduino.h>
#include <SPIFFS.h>
#include <Mesh.h>
#include <helpers/ArduinoHelpers.h>
#include <helpers/SimpleMeshTables.h>
#include <helpers/MultiSerialInterface.h>
#include "MyMesh.h"
#include "app/mesh_adapter.h"
#include "ui/app.h"
#include "ui/screens/screens.h"
#include "st7789_display.h"
#include "tca8418_keyboard.h"
#include "platform_cardputer.h"
#if !defined(ENABLE_USB_INTERFACE)
  #include "diag.h"
#endif

#if defined(BLE_PIN_CODE)
  #include <helpers/esp32/SerialBLEInterface.h>
  static SerialBLEInterface ble_interface;
  static constexpr bool kBleBuilt = true;
#else
  static constexpr bool kBleBuilt = false;
#endif
#if defined(ENABLE_USB_INTERFACE)
  #include <helpers/ArduinoSerialInterface.h>
  static ArduinoSerialInterface usb_interface;
#endif

static MultiSerialInterface interfaces;
static DataStore store(SPIFFS, rtc_clock);
static CardputerPlatform platform;
static app::MeshAdapter adapter(&board, &interfaces, platform);
static StdRNG fast_rng;
static SimpleMeshTables tables;
// MyMesh.h declares this extern.
MyMesh the_mesh(radio_driver, fast_rng, rtc_clock, tables, store, &adapter);
static St7789Display display;
static Tca8418Keyboard keyboard;
static ui::App ui_app(adapter, display, keyboard);

static void halt(const char* why) {
  Serial.printf("halt: %s\n", why);
  for (;;) delay(1000);
}

void setup() {
  Serial.begin(115200);
  board.begin();
  display.begin();

  if (!radio_init()) halt("radio init failed (is the Cap LoRa-1262 attached?)");
  fast_rng.begin(radio_driver.getRngSeed());

  SPIFFS.begin(true);
  bool fresh = !SPIFFS.exists("/identity/_main.id");
  store.begin();
  the_mesh.begin(true);

#if defined(BLE_PIN_CODE)
  ble_interface.begin(BLE_NAME_PREFIX, the_mesh.getNodePrefs()->node_name, the_mesh.getBLEPin());
  interfaces.addInterface(InterfaceType::Bluetooth, &ble_interface);
#endif
#if defined(ENABLE_USB_INTERFACE)
  usb_interface.begin(Serial);
  interfaces.addInterface(InterfaceType::USB, &usb_interface);
#endif
  the_mesh.startInterface(interfaces);
  sensors.begin();
#if ENV_INCLUDE_GPS == 1
  the_mesh.applyGpsPrefs();
#endif

  adapter.begin(the_mesh, store, rtc_clock, SPIFFS, kBleBuilt, fresh);
  if (!adapter.prefs().bleEnabled) adapter.disableBluetooth();
  keyboard.begin();
  ui_app.begin(ui::screens::boot());
  board.onBootComplete();
  // One status line for bring-up (no keys, names or addresses).
  Serial.printf("meshcore-adv-ui %s: lora-cap %s, keyboard %s, sd %s, contacts %d, heap %u (min %u)\n",
                MC_UI_VERSION, board.loraCapPresent() ? "ok" : "MISSING", keyboard.present() ? "ok" : "MISSING",
                platform.sd() ? "yes" : "no", the_mesh.getNumContacts(), (unsigned)platform.heapFree(),
                (unsigned)platform.heapMin());
}

void loop() {
  the_mesh.loop();
  interfaces.loop();
  sensors.loop();
  adapter.loop();
  ui_app.loop(millis());
  rtc_clock.tick();
#if !defined(ENABLE_USB_INTERFACE)
  diagPoll(ui_app, display, keyboard, platform);
#endif
}
