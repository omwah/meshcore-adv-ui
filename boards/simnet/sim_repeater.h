// A stock MeshCore repeater (examples/simple_repeater) in the simulation.
// Built in its own translation unit (repeater_unit.cpp) because its MyMesh
// and NodePrefs classes share names with the companion's.
#pragma once
#include <memory>
#include <string>
#include "sim_net.h"

class SimRepeater {
public:
  virtual ~SimRepeater() {}
  static std::unique_ptr<SimRepeater> create(SimMedium& m, int id, const std::string& name, const std::string& dir,
                                             uint32_t epoch, const std::string& adminPassword);
  virtual void loop() = 0;
  virtual void select() = 0;
  virtual void advert(bool flood) = 0;
};
