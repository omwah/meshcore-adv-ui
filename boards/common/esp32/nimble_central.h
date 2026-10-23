// Remote node mode: this device as a Bluetooth client of a companion node
// (stock firmware's Nordic UART service), over NimBLE. Connecting, pairing
// and writes block in NimBLE, so they run in a task of their own; the main
// loop only exchanges frames through queues.
#pragma once
#if defined(BLE_PIN_CODE) && MC_UI_REMOTE
#include <NimBLEDevice.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include "app/remote/frame_link.h"
#include "app/remote/companion_proto.h"

class NimbleCentral : public app::FrameLink,
                      private NimBLEClientCallbacks,
                      private NimBLEAdvertisedDeviceCallbacks {
public:
  // Start NimBLE as a client (instead of the phone-app server).
  void begin(const char* deviceName);

  // ---- FrameLink ----
  State state() const override { return _state; }
  bool send(const uint8_t* frame, int len) override;
  int recv(uint8_t* buf) override;
  void startScan() override;
  void stopScan() override { _scanWanted = false; }
  int scanCount() const override { return _nFound; }
  bool scanResult(int i, app::Peer& out) const override;
  void connect(const app::Peer& p, uint32_t pin) override;
  void disconnect() override { _want = false; }
  void forget(const app::Peer& p) override;
  int8_t rssi() const override { return _rssi; }
  const char* lastError() const override { return _err; }

private:
  struct Frame {
    uint8_t len;
    uint8_t buf[app::cp::MAX_FRAME];
  };
  static constexpr int RX_FRAMES = 16, TX_FRAMES = 4, MAX_FOUND = 8;

  static void taskMain(void* self);
  void run();
  bool attempt();          // connect, pair, find the service, subscribe
  void dropConnection();
  void setError(const char* e);

  // NimBLE callbacks (its host task)
  void onResult(NimBLEAdvertisedDevice* dev) override;
  void onDisconnect(NimBLEClient* c) override;
  uint32_t onPassKeyRequest() override { return _pin; }
  bool onConfirmPIN(uint32_t) override { return true; }
  void onAuthenticationComplete(ble_gap_conn_desc* desc) override;

  volatile State _state = State::Idle;
  volatile bool _want = false, _scanWanted = false, _peerChanged = false, _connected = false, _authOk = false;
  volatile int _nFound = 0;
  volatile int8_t _rssi = 0;
  app::Peer _peer{};
  app::Peer _found[MAX_FOUND];
  uint32_t _pin = 123456;
  char _err[40] = "";
  portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
  QueueHandle_t _rx = nullptr, _tx = nullptr;
  NimBLEClient* _client = nullptr;
  NimBLERemoteCharacteristic* _write = nullptr;   // the node's RX
  uint32_t _retryAt = 0, _backoff = 2000;
  bool _forgetPending = false, _pairFailed = false;
  app::Peer _forgetPeer{};
};

#endif
