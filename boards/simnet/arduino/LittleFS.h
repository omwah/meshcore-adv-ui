#pragma once
#include "FS.h"

// MeshCore's RP2040 storage path is used on the host (it needs only a plain
// fs::FS). The simulator points this at the running node's directory.
extern fs::FS LittleFS;
