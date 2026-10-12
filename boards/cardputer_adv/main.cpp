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
#include "es8311_sound.h"
#if !defined(ENABLE_USB_INTERFACE)
  #include "diag.h"
#endif

#if defined(BLE_PIN_CODE)
  #include <helpers/esp32/SerialBLEInterface.h>
  // MeshCore's Bluetooth companion, plus knowing when a phone is pairing so
  // the UI can show the PIN: connected but not yet authenticated.
  class PairingBLEInterface : public SerialBLEInterface {
  public:
    volatile bool linked = false, authed = false;
    void onConnect(BLEServer* s, esp_ble_gatts_cb_param_t* p) override {
      linked = true;
      authed = false;
      SerialBLEInterface::onConnect(s, p);
    }
    void onAuthenticationComplete(esp_ble_auth_cmpl_t c) override {
      authed = c.success;
      if (!c.success) linked = false;
      SerialBLEInterface::onAuthenticationComplete(c);
    }
    void onDisconnect(BLEServer* s) override {
      linked = authed = false;
      SerialBLEInterface::onDisconnect(s);
    }
    bool pairing() const { return linked && !authed; }
  };
  static PairingBLEInterface ble_interface;
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
static Es8311Sound sound;
static ui::App ui_app(adapter, display, keyboard, &sound);

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

  adapter.begin(the_mesh, store, rtc_clock, SPIFFS, kBleBuilt, fresh);
  // Show the splash now: Bluetooth and sensors take a while to start.
  keyboard.begin();
  sound.begin();
  ui_app.begin(ui::screens::boot());
  ui_app.loop(millis());
#if defined(BLE_PIN_CODE)
  platform.blePairingFn = [] { return ble_interface.pairing(); };
  ble_interface.begin(BLE_NAME_PREFIX, the_mesh.getNodePrefs()->node_name, adapter.choosePinForBoot());
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

  if (!adapter.prefs().bleEnabled) adapter.disableBluetooth();
  board.onBootComplete();
#if !defined(ENABLE_USB_INTERFACE)
  g_diagBeep = [] {
    static const hal::Tone t[] = {{1760, 70}, {0, 40}, {2349, 110}};
    sound.play(t, 3);
  };
#endif
  // One status line for bring-up (no keys, names or addresses).
  Serial.printf("meshcore-adv-ui %s: lora-cap %s, keyboard %s, speaker %s, sd %s, contacts %d, heap %u (min %u)\n",
                MC_UI_VERSION, board.loraCapPresent() ? "ok" : "MISSING", keyboard.present() ? "ok" : "MISSING",
                sound.present() ? "ok" : "MISSING",
                platform.sd() ? "yes" : "no", the_mesh.getNumContacts(), (unsigned)platform.heapFree(),
                (unsigned)platform.heapMin());
}

void loop() {
  the_mesh.loop();
  interfaces.loop();
  sensors.loop();
  adapter.loop();
  uint32_t uiWait = ui_app.loop(millis());
  rtc_clock.tick();
#if !defined(ENABLE_USB_INTERFACE)
  diagPoll(ui_app, display, keyboard, platform);
#endif
  // Don't spin: sleeping lets the idle task halt the CPU until the next
  // interrupt. At most 10 ms, so the radio (one packet buffer, ~40 ms per
  // packet at SF7) is always read in time; not at all while MeshCore has
  // packets to send or retry.
  if (!the_mesh.hasPendingWork()) delay(uiWait < 10 ? uiWait : 10);
}
