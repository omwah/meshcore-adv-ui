// A link to a companion node that carries whole protocol frames: Bluetooth
// LE on the device (boards/cardputer_adv/nimble_central.*), an in-process
// pipe in the host simulator. Plain C++, no Arduino.
#pragma once
#include <stdint.h>

namespace app {

struct Peer {                 // a node found by a scan, or the saved one
  char name[32];
  uint8_t addr[6];
  uint8_t addrType;
  int8_t rssi;
};

class FrameLink {
public:
  enum class State : uint8_t { Idle, Scanning, Connecting, Pairing, Ready, Lost };
  virtual ~FrameLink() {}

  virtual State state() const = 0;
  bool ready() const { return state() == State::Ready; }
  // One frame each; recv returns its length (0: none waiting).
  virtual bool send(const uint8_t* frame, int len) = 0;
  virtual int recv(uint8_t* buf) = 0;
  virtual void loop(uint32_t ms) { (void)ms; }

  // Finding and choosing a node (a no-op where there is only one).
  virtual void startScan() {}
  virtual void stopScan() {}
  virtual int scanCount() const { return 0; }
  virtual bool scanResult(int i, Peer& out) const { (void)i; (void)out; return false; }
  // Connect, pairing with `pin` if the node asks; keeps reconnecting after a
  // loss until disconnect().
  virtual void connect(const Peer& p, uint32_t pin) { (void)p; (void)pin; }
  virtual void disconnect() {}
  // Disconnect and drop the bond with this node.
  virtual void forget(const Peer& p) { (void)p; }
  virtual int8_t rssi() const { return 0; }
  // Why the last connection attempt failed ("" if it didn't).
  virtual const char* lastError() const { return ""; }
};

}  // namespace app
