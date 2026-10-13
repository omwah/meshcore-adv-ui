// A scripted MeshCore companion node for simulations: optionally echoes
// direct messages back, after a delay. Remembers what it last got.
#pragma once
#include <stdint.h>
#include <string>
#include <helpers/TxtDataHelpers.h>
#include "MyMesh.h"

class BotTask : public AbstractUITask {
public:
  BotTask(mesh::MainBoard* b, MultiSerialInterface* i) : AbstractUITask(b, i) {}
  void attach(MyMesh* m, mesh::RTCClock* rtc) { _mesh = m; _rtc = rtc; }
  void msgRead(int) override {}
  void newMsg(uint8_t, const char*, const char*, int) override {}
  void notify(UIEventType) override {}
  void loop() override {
    if (!_due || millis() < _due) return;
    _due = 0;
    ContactInfo* c = _mesh->lookupContactByPubKey(_replyTo, 6);
    uint32_t ack, est;
    if (c) _mesh->sendMessage(*c, _rtc->getCurrentTimeUnique(), 0, _reply.c_str(), ack, est);
  }
  void onDirectMessage(const ContactInfo& from, uint8_t txt_type, uint8_t, float, uint32_t, const uint8_t*, int,
                       const char* text) override {
    received++;
    lastText = text;
    if (!echo || txt_type != TXT_TYPE_PLAIN) return;
    memcpy(_replyTo, from.id.pub_key, 6);
    _reply = std::string("Got it: ") + text;
    _due = millis() + 1500;
  }
  bool echo = false;
  int received = 0;
  std::string lastText;   // the last direct message it got

private:
  MyMesh* _mesh = nullptr;
  mesh::RTCClock* _rtc = nullptr;
  uint8_t _replyTo[6] = {};
  std::string _reply;
  uint32_t _due = 0;
};
