// A stock MeshCore room server (examples/simple_room_server) in the
// simulation; built in room_unit.cpp for the same reason as SimRepeater.
#pragma once
#include <memory>
#include <string>
#include "sim_net.h"

class SimRoom {
public:
  virtual ~SimRoom() {}
  static std::unique_ptr<SimRoom> create(SimMedium& m, int id, const std::string& name, const std::string& dir,
                                         uint32_t epoch, const std::string& adminPassword);
  virtual void loop() = 0;
  virtual void select() = 0;
  virtual void advert(bool flood) = 0;
};
