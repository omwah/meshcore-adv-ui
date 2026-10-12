// A Bluetooth link between a companion node and our remote model, in one
// process: the node's end is a MeshCore BaseSerialInterface, ours a
// FrameLink. It behaves like stock's BLE interface: the node's frames wait
// in a 4-frame queue (more are dropped) and go out at most one per 60 ms.
#pragma once
#include <deque>
#include <vector>
#include <string.h>
#include <Arduino.h>
#include <helpers/BaseSerialInterface.h>
#include "app/remote/frame_link.h"

class PipeLink : public BaseSerialInterface, public app::FrameLink {
public:
  // ---- the node's side (BaseSerialInterface) ----
  void enable() override { _enabled = true; }
  void disable() override { _enabled = false; _up = false; }
  bool isEnabled() const override { return _enabled; }
  bool isConnected() const override { return _up; }
  bool isWriteBusy() const override { return millis() < _lastOut + 60; }
  size_t writeFrame(const uint8_t src[], size_t len) override {
    if (!_up || len == 0 || len > MAX_FRAME_SIZE || _toApp.size() >= 4) {
      if (_up && len) dropped++;
      return 0;
    }
    _toApp.emplace_back(src, src + len);
    return len;
  }
  size_t checkRecvFrame(uint8_t dest[]) override {
    // Pace what the node sends, as stock's BLE notify loop does.
    if (!_toApp.empty() && millis() >= _lastOut + 60) {
      _lastOut = millis();
      _arrived.push_back(_toApp.front());
      _toApp.pop_front();
    }
    if (_toNode.empty()) return 0;
    std::vector<uint8_t> f = _toNode.front();
    _toNode.pop_front();
    memcpy(dest, f.data(), f.size());
    return f.size();
  }

  // ---- our side (FrameLink) ----
  State state() const override { return _up ? State::Ready : State::Idle; }
  bool send(const uint8_t* frame, int len) override {
    if (!_up) return false;
    _toNode.emplace_back(frame, frame + len);
    return true;
  }
  int recv(uint8_t* buf) override {
    if (_arrived.empty()) return 0;
    std::vector<uint8_t> f = _arrived.front();
    _arrived.pop_front();
    memcpy(buf, f.data(), f.size());
    return (int)f.size();
  }
  int scanCount() const override { return _enabled ? 1 : 0; }
  bool scanResult(int i, app::Peer& out) const override {
    if (i != 0 || !_enabled) return false;
    memset(&out, 0, sizeof out);
    snprintf(out.name, sizeof out.name, "MeshCore-%s", peerName);
    out.addr[5] = 0xA0;
    out.rssi = -60;
    return true;
  }
  void connect(const app::Peer&, uint32_t pin) override {
    _up = _enabled && pin == this->pin;
    snprintf(_err, sizeof _err, "%s", _up || !_enabled ? "" : "Pairing failed: wrong PIN?");
  }
  const char* lastError() const override { return _err; }
  void disconnect() override {
    _up = false;
    _toApp.clear();
    _arrived.clear();
    _toNode.clear();
  }
  int8_t rssi() const override { return _up ? -60 : 0; }

  // Scenario controls.
  void linkUp(bool up) { if (up) _up = _enabled; else disconnect(); }
  char peerName[32] = "Base";
  uint32_t pin = 123456;
  uint32_t dropped = 0;    // node frames lost to a full queue

private:
  bool _enabled = false, _up = false;
  char _err[40] = "";
  unsigned long _lastOut = 0;
  std::deque<std::vector<uint8_t>> _toApp, _arrived, _toNode;
};
