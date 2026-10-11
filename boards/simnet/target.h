// MeshCore "variant" for the host simulator: the globals MeshCore expects
// (board, radio_driver, rtc_clock, sensors) forward to the node that is
// currently running, so many nodes can share one process and one clock.
#pragma once
#include <Arduino.h>
#include <Mesh.h>
#include <helpers/SensorManager.h>
#include "sim_net.h"

extern SimBoard board;
extern SimRadioProxy radio_driver;
extern SensorManager sensors;

mesh::LocalIdentity radio_new_identity();
