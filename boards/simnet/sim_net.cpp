#include <Arduino.h>
#include <math.h>
#include <algorithm>
#include "target.h"

SimRadio* SimRadio::current = nullptr;
SimRTC* SimRTC::current = nullptr;
SimRTCProxy rtc_clock;
SimBoard board;
SimRadioProxy radio_driver;
SensorManager sensors;

uint32_t loraAirtimeMs(int len, uint8_t sf, float bwKHz, uint8_t cr, int preamble) {
  double ts = pow(2.0, sf) / (bwKHz * 1000.0);   // symbol time, s
  int de = ts > 0.016 ? 1 : 0;                     // low data rate optimisation
  double num = 8.0 * len - 4.0 * sf + 28 + 16;     // explicit header, CRC on
  double payloadSym = 8 + std::max(ceil(num / (4.0 * (sf - 2 * de))) * cr, 0.0);
  double t = (preamble + 4.25) * ts + payloadSym * ts;
  return (uint32_t)ceil(t * 1000.0);
}

int SimRadio::recvRaw(uint8_t* bytes, int sz) {
  uint32_t now = millis();
  for (size_t i = 0; i < _rx.size(); i++) {
    if (_rx[i].at > now) continue;
    Rx rx = std::move(_rx[i]);
    _rx.erase(_rx.begin() + i);
    int n = (int)std::min(rx.data.size(), (size_t)sz);
    memcpy(bytes, rx.data.data(), n);
    _lastSnr = rx.snr;
    _lastRssi = rx.rssi;
    _nRecv++;
    return n;
  }
  return 0;
}

uint32_t SimRadio::getEstAirtimeFor(int len) {
  return loraAirtimeMs(len, _sf, _bw, _cr, _sf <= 8 ? 32 : 16);
}

float SimRadio::packetScore(float snr, int len) {
  // Same shape as RadioLibWrapper::packetScoreInt for SF10.
  static const float kLimit = -15.0f;
  if (snr < kLimit) return 0.0f;
  float s = (snr - kLimit) / 10.0f;
  float lenPenalty = 1.0f - (len / 256.0f);
  return std::max(0.0f, std::min(1.0f, s * lenPenalty));
}

bool SimRadio::startSendRaw(const uint8_t* bytes, int len) {
  uint32_t air = getEstAirtimeFor(len);
  _txEnd = millis() + air;
  _sending = true;
  _nSent++;
  _m.transmit(*this, bytes, len, air);
  return true;
}

bool SimRadio::isSendComplete() {
  if (_sending && millis() >= _txEnd) { _sending = false; return true; }
  return false;
}

bool SimRadio::isInRecvMode() const { return !_sending; }

void SimMedium::link(int a, int b, float snr, bool both) {
  unlink(a, b);
  _links.push_back({a, b, snr});
  if (both) _links.push_back({b, a, snr});
}

void SimMedium::unlink(int a, int b) {
  _links.erase(std::remove_if(_links.begin(), _links.end(),
                              [&](const Link& l) { return (l.a == a && l.b == b) || (l.a == b && l.b == a); }),
               _links.end());
}

void SimMedium::transmit(SimRadio& from, const uint8_t* data, int len, uint32_t airtime) {
  _packets++;
  for (const Link& l : _links) {
    if (l.a != from.id()) continue;
    for (SimRadio* r : _radios) {
      if (r->id() != l.b) continue;
      // Different radio settings hear nothing.
      if (fabsf(r->freq() - from.freq()) > 0.01f || r->sf() != from.sf() || fabsf(r->bw() - from.bw()) > 0.01f) continue;
      SimRadio::Rx rx;
      rx.at = millis() + airtime;
      rx.snr = l.snr;
      rx.rssi = -120.0f + l.snr + 6.0f;
      rx.data.assign(data, data + len);
      r->deliver(std::move(rx));
    }
  }
}
