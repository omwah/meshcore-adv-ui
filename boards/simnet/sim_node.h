// One simulated MeshCore companion node: radio, clock, storage directory and
// the stock companion MyMesh, optionally with a UI task attached.
#pragma once
#include <string>
#include <LittleFS.h>
#include <helpers/SimpleMeshTables.h>
#include "MyMesh.h"
#include "sim_net.h"

class NullSerial : public BaseSerialInterface {
public:
  void enable() override { _en = true; }
  void disable() override { _en = false; }
  bool isEnabled() const override { return _en; }
  bool isConnected() const override { return false; }
  bool isWriteBusy() const override { return false; }
  size_t writeFrame(const uint8_t[], size_t len) override { return len; }
  size_t checkRecvFrame(uint8_t[]) override { return 0; }

private:
  bool _en = false;
};

// Deterministic RNG per node (MeshCore keys, tags and jitter come from it).
class SimRNG : public mesh::RNG {
public:
  explicit SimRNG(uint32_t seed) : _s(seed ? seed : 1) {}
  void random(uint8_t* dest, size_t sz) override {
    for (size_t i = 0; i < sz; i++) {
      _s ^= _s << 13; _s ^= _s >> 17; _s ^= _s << 5;
      dest[i] = (uint8_t)_s;
    }
  }

private:
  uint32_t _s;
};

class SimNode {
public:
  // `dir` holds this node's flash (wiped first when `fresh`).
  // `serial`: the app link (none: a node with no app connected).
  SimNode(SimMedium& medium, int id, const std::string& name, const std::string& dir, uint32_t epoch,
          AbstractUITask* ui = nullptr, bool fresh = true, BaseSerialInterface* serial = nullptr);
  ~SimNode();
  void begin();
  void loop();                 // run one iteration as the current node
  void select();               // make MeshCore globals point at this node

  MyMesh& mesh() { return *_mesh; }
  SimRadio& radio() { return _radio; }
  DataStore& store() { return *_store; }
  fs::FS& fs() { return _fs; }
  const std::string& name() const { return _name; }
  int id() const { return _id; }

private:
  int _id;
  std::string _name, _dir;
  SimRadio _radio;
  SimRTC _rtc;
  SimRNG _rng;
  SimpleMeshTables _tables;
  fs::FS _fs;
  DataStore* _store;
  MyMesh* _mesh;
  NullSerial _null;
  BaseSerialInterface* _serial;
};
