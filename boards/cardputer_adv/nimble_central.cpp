#include "nimble_central.h"
#if defined(BLE_PIN_CODE) && MC_UI_REMOTE
#include <Arduino.h>
#include <string.h>

// The phone apps' service on stock companion firmware: Nordic UART.
static const char* const SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
static const char* const RX_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";   // we write
static const char* const TX_UUID = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";   // it notifies

void NimbleCentral::begin(const char* deviceName) {
  _rx = xQueueCreate(RX_FRAMES, sizeof(Frame));
  _tx = xQueueCreate(TX_FRAMES, sizeof(Frame));
  NimBLEDevice::init(deviceName);
  NimBLEDevice::setMTU(app::cp::MAX_FRAME + 3);
  // The node shows (or has) a static PIN; we type it: passkey entry, bonded.
  NimBLEDevice::setSecurityAuth(true, true, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_KEYBOARD_ONLY);
  _client = NimBLEDevice::createClient();
  _client->setClientCallbacks(this, false);
  _client->setConnectTimeout(8);
  xTaskCreatePinnedToCore(taskMain, "ble-central", 6144, this, 2, nullptr, 0);
}

// ---- main loop side

bool NimbleCentral::send(const uint8_t* frame, int len) {
  if (_state != State::Ready || len <= 0 || len > app::cp::MAX_FRAME) return false;
  Frame f;
  f.len = (uint8_t)len;
  memcpy(f.buf, frame, len);
  return xQueueSend(_tx, &f, 0) == pdTRUE;
}

int NimbleCentral::recv(uint8_t* buf) {
  Frame f;
  if (!_rx || xQueueReceive(_rx, &f, 0) != pdTRUE) return 0;
  memcpy(buf, f.buf, f.len);
  return f.len;
}

void NimbleCentral::startScan() {
  portENTER_CRITICAL(&_mux);
  _nFound = 0;
  portEXIT_CRITICAL(&_mux);
  _scanWanted = true;
}

bool NimbleCentral::scanResult(int i, app::Peer& out) const {
  if (i < 0 || i >= _nFound) return false;
  portENTER_CRITICAL(const_cast<portMUX_TYPE*>(&_mux));
  out = _found[i];
  portEXIT_CRITICAL(const_cast<portMUX_TYPE*>(&_mux));
  return true;
}

void NimbleCentral::connect(const app::Peer& p, uint32_t pin) {
  _peer = p;
  _pin = pin;
  _err[0] = 0;
  _backoff = 2000;
  _retryAt = 0;
  _peerChanged = true;
  _want = true;
}

void NimbleCentral::forget(const app::Peer& p) {
  _want = false;
  _forgetPeer = p;
  _forgetPending = true;
}

void NimbleCentral::setError(const char* e) { snprintf(_err, sizeof _err, "%s", e); }

// ---- NimBLE callbacks

void NimbleCentral::onResult(NimBLEAdvertisedDevice* dev) {
  if (!dev->isAdvertisingService(NimBLEUUID(SERVICE_UUID))) return;
  app::Peer p{};
  snprintf(p.name, sizeof p.name, "%s", dev->getName().c_str());
  memcpy(p.addr, dev->getAddress().getNative(), 6);
  p.addrType = dev->getAddress().getType();
  p.rssi = (int8_t)dev->getRSSI();
  portENTER_CRITICAL(&_mux);
  int i = 0;
  while (i < _nFound && memcmp(_found[i].addr, p.addr, 6)) i++;
  if (i < MAX_FOUND) {
    if (!p.name[0] && i < _nFound) memcpy(p.name, _found[i].name, sizeof p.name);   // keep a name seen before
    _found[i] = p;
    if (i == _nFound) _nFound++;
  }
  portEXIT_CRITICAL(&_mux);
}

void NimbleCentral::onDisconnect(NimBLEClient*) {
  _connected = false;
  _write = nullptr;
}

void NimbleCentral::onAuthenticationComplete(ble_gap_conn_desc* desc) {
  _authOk = desc->sec_state.encrypted && desc->sec_state.authenticated;
}

// ---- the task

void NimbleCentral::taskMain(void* self) { ((NimbleCentral*)self)->run(); }

void NimbleCentral::dropConnection() {
  if (_client->isConnected()) _client->disconnect();
  for (int i = 0; i < 50 && _client->isConnected(); i++) vTaskDelay(pdMS_TO_TICKS(20));
  _connected = false;
  _write = nullptr;
  xQueueReset(_tx);
}

// One go at the saved node: connect, pair (or use the bond), find the
// service, subscribe to its notifications.
bool NimbleCentral::attempt() {
  NimBLEAddress addr(_peer.addr, _peer.addrType);
  _state = State::Connecting;
  if (!_client->connect(addr, true)) {
    setError("Node not found or out of range");
    return false;
  }
  _connected = true;
  _state = State::Pairing;
  _authOk = false;
  if (!_client->secureConnection() || !_authOk) {
    // A node that was erased forgot our bond: pair afresh next time.
    if (NimBLEDevice::isBonded(addr)) NimBLEDevice::deleteBond(addr);
    setError("Pairing failed: wrong PIN?");
    dropConnection();
    _pairFailed = true;   // no retrying with the same PIN: the user enters it again
    return false;
  }
  NimBLERemoteService* svc = _client->getService(SERVICE_UUID);
  NimBLERemoteCharacteristic* tx = svc ? svc->getCharacteristic(TX_UUID) : nullptr;
  _write = svc ? svc->getCharacteristic(RX_UUID) : nullptr;
  QueueHandle_t rxq = _rx;
  bool ok = tx && _write && tx->subscribe(true, [rxq](NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
    Frame f;
    if (len == 0 || len > sizeof f.buf) return;
    f.len = (uint8_t)len;
    memcpy(f.buf, data, len);
    xQueueSend(rxq, &f, 0);   // dropped if the main loop is far behind
  });
  if (!ok) {
    setError("Not a MeshCore companion node");
    dropConnection();
    return false;
  }
  _rssi = (int8_t)_client->getRssi();
  _err[0] = 0;
  _state = State::Ready;
  return true;
}

void NimbleCentral::run() {
  bool scanning = false;
  uint32_t rssiAt = 0;
  for (;;) {
    uint32_t now = millis();
    if (_forgetPending) {
      _forgetPending = false;
      dropConnection();
      NimBLEAddress a(_forgetPeer.addr, _forgetPeer.addrType);
      if (NimBLEDevice::isBonded(a)) NimBLEDevice::deleteBond(a);
    }
    // Scanning: while asked for (the Nodes nearby screen).
    if (_scanWanted != scanning) {
      NimBLEScan* s = NimBLEDevice::getScan();
      if (_scanWanted) {
        s->setAdvertisedDeviceCallbacks(this, true);
        s->setActiveScan(true);   // names come in scan responses
        s->setInterval(100);
        s->setWindow(80);
        s->clearResults();
        s->start(0, nullptr, false);
      } else {
        s->stop();
      }
      scanning = _scanWanted;
    }
    if (_peerChanged || (!_want && _connected)) {
      _peerChanged = false;
      dropConnection();
    }
    if (_connected && _state == State::Ready) {
      // Frames to the node, in order; a write waits for the node's answer.
      Frame f;
      while (_write && xQueueReceive(_tx, &f, 0) == pdTRUE)
        if (!_write->writeValue(f.buf, f.len, true)) break;
      if (now - rssiAt > 5000) {
        rssiAt = now;
        _rssi = (int8_t)_client->getRssi();
      }
    } else if (_want && !scanning && (int32_t)(now - _retryAt) >= 0) {
      if (!attempt()) {
        if (_pairFailed) {
          _pairFailed = false;
          _want = false;
        }
        _state = _want ? State::Lost : State::Idle;
        _retryAt = millis() + _backoff;
        _backoff = _backoff < 30000 ? _backoff * 2 : 30000;
      } else {
        _backoff = 2000;
      }
    }
    if (!_connected && _state == State::Ready) _state = State::Lost;   // dropped: retry
    if (!_connected && !(_state == State::Connecting || _state == State::Pairing))
      _state = scanning ? State::Scanning : _want ? State::Lost : State::Idle;
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

#endif
