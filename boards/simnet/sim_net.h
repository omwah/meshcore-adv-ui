// Simulated LoRa medium and MeshCore nodes for host tests.
//
// Every node has a SimRadio. A transmission takes the real LoRa airtime for
// the node's SF/BW/CR, then reaches every other node that has a link to the
// sender, with that link's SNR. Collisions are not modelled.
#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#include <Mesh.h>

class SimMedium;

class SimRadio : public mesh::Radio {
public:
  SimRadio(SimMedium& m, int id) : _m(m), _id(id) {}
  static SimRadio* current;   // the node being run

  int recvRaw(uint8_t* bytes, int sz) override;
  uint32_t getEstAirtimeFor(int len) override;
  float packetScore(float snr, int len) override;
  bool startSendRaw(const uint8_t* bytes, int len) override;
  bool isSendComplete() override;
  void onSendFinished() override {}
  bool isInRecvMode() const override;
  int getNoiseFloor() const override { return _noiseFloor; }
  float getLastRSSI() const override { return _lastRssi; }
  float getLastSNR() const override { return _lastSnr; }

  void setParams(float freq, float bw, uint8_t sf, uint8_t cr) { _freq = freq; _bw = bw; _sf = sf; _cr = cr; }
  void setTxPower(int8_t dbm) { _tx = dbm; }
  bool setRxBoostedGainMode(bool on) { _boost = on; return true; }
  bool getRxBoostedGainMode() const { return _boost; }
  uint32_t getPacketsRecv() const { return _nRecv; }
  uint32_t getPacketsSent() const { return _nSent; }
  uint32_t getPacketsRecvErrors() const { return 0; }
  uint32_t getRngSeed() const { return 1000 + _id; }
  void resetStats() { _nRecv = _nSent = 0; }
  bool configSideDetectors(const uint8_t*, int, float) { return false; }

  int id() const { return _id; }
  float freq() const { return _freq; }
  float bw() const { return _bw; }
  uint8_t sf() const { return _sf; }
  uint8_t cr() const { return _cr; }

  struct Rx {
    uint32_t at;          // ms when reception completes
    float snr, rssi;
    std::vector<uint8_t> data;
  };
  void deliver(Rx rx) { _rx.push_back(std::move(rx)); }

private:
  SimMedium& _m;
  int _id;
  float _freq = 910.525f, _bw = 62.5f;
  uint8_t _sf = 7, _cr = 5;
  int8_t _tx = 22;
  bool _boost = true;
  uint32_t _txEnd = 0;
  bool _sending = false;
  uint32_t _nRecv = 0, _nSent = 0;
  float _lastSnr = 0, _lastRssi = 0;
  int _noiseFloor = -118;
  std::vector<Rx> _rx;
};

// Forwards the radio_driver calls MeshCore makes on the global object.
class SimRadioProxy {
public:
  void setParams(float f, float bw, uint8_t sf, uint8_t cr) { SimRadio::current->setParams(f, bw, sf, cr); }
  void setTxPower(int8_t dbm) { SimRadio::current->setTxPower(dbm); }
  bool setRxBoostedGainMode(bool on) { return SimRadio::current->setRxBoostedGainMode(on); }
  bool getRxBoostedGainMode() const { return SimRadio::current->getRxBoostedGainMode(); }
  float getLastRSSI() const { return SimRadio::current->getLastRSSI(); }
  float getLastSNR() const { return SimRadio::current->getLastSNR(); }
  uint32_t getPacketsRecv() const { return SimRadio::current->getPacketsRecv(); }
  uint32_t getPacketsSent() const { return SimRadio::current->getPacketsSent(); }
  uint32_t getPacketsRecvErrors() const { return SimRadio::current->getPacketsRecvErrors(); }
  uint32_t getRngSeed() const { return SimRadio::current->getRngSeed(); }
  uint32_t getEstAirtimeFor(int len) { return SimRadio::current->getEstAirtimeFor(len); }
  void resetStats() { SimRadio::current->resetStats(); }
  bool configSideDetectors(const uint8_t* sfs, int n, float bw) { return SimRadio::current->configSideDetectors(sfs, n, bw); }
  int getNoiseFloor() const { return SimRadio::current->getNoiseFloor(); }
};

class SimBoard : public mesh::MainBoard {
public:
  template <typename T> void attachDynamicPrefs(T*) {} // host has no board-specific persistent prefs
  uint16_t battMv = 3950;
  uint16_t getBattMilliVolts() override { return battMv; }
  const char* getManufacturerName() const override { return "Simulator"; }
  void reboot() override { rebootRequested = true; }
  uint8_t getStartupReason() const override { return BD_STARTUP_NORMAL; }
  bool rebootRequested = false;
};

class SimRTC : public mesh::RTCClock {
public:
  explicit SimRTC(uint32_t base) : _base(base) {}
  static SimRTC* current;
  uint32_t getCurrentTime() override { return _base + (uint32_t)(millis() / 1000); }
  void setCurrentTime(uint32_t t) override { _base = t - (uint32_t)(millis() / 1000); }

private:
  uint32_t _base;
};

// The global rtc_clock some MeshCore examples use, for the current node.
class SimRTCProxy : public mesh::RTCClock {
public:
  uint32_t getCurrentTime() override { return SimRTC::current->getCurrentTime(); }
  void setCurrentTime(uint32_t t) override { SimRTC::current->setCurrentTime(t); }
};

class SimMedium {
public:
  // Link quality from a to b (dB SNR); absent = out of range.
  void link(int a, int b, float snr, bool both = true);
  void unlink(int a, int b);
  void transmit(SimRadio& from, const uint8_t* data, int len, uint32_t airtime);
  void add(SimRadio* r) { _radios.push_back(r); }
  uint32_t packets() const { return _packets; }

private:
  struct Link { int a, b; float snr; };
  std::vector<Link> _links;
  std::vector<SimRadio*> _radios;
  uint32_t _packets = 0;
};

// LoRa time on air (Semtech formula), ms.
uint32_t loraAirtimeMs(int len, uint8_t sf, float bwKHz, uint8_t cr, int preamble);
