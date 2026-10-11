// Radio presets offered under Settings > Radio > Preset.
//
// Snapshot of the community-suggested list that the official MeshCore apps
// and config.meshcore.io load at runtime from
//   https://api.meshcore.nz/api/v1/config  (config.suggested_radio_settings.entries)
// (see meshcore-dev/config.meshcore.io src/gui.js getPresets()), taken
// 2026-10-11. The list can change upstream without a MeshCore commit:
// re-check it when bumping the MeshCore submodule.
//
// The first entry is the default for a new device.
#pragma once
#include "model/model.h"

namespace app {

static const model::Preset kRadioPresets[] = {
  // name                         MHz       BW     SF CR  path hash bytes
  {"USA",                         910.525f, 62.5f, 7, 5, 0},
  {"USA - Southern California",   927.875f, 62.5f, 7, 5, 3},
  {"Canada",                      910.525f, 62.5f, 7, 5, 3},
  {"Costa Rica",                  910.525f, 125.f, 11, 5, 0},
  {"Brazil",                      923.125f, 62.5f, 8, 8, 0},
  {"EU/UK (Narrow)",              869.618f, 62.5f, 8, 8, 0},
  {"EU/UK (Deprecated)",          869.525f, 250.f, 11, 5, 0},
  {"EU 433MHz (Long Range)",      433.650f, 250.f, 11, 5, 0},
  {"EU 433MHz (Narrow)",          433.650f, 62.5f, 8, 8, 0},
  {"Czech Republic (Narrow)",     869.432f, 62.5f, 7, 5, 2},
  {"Hungary",                     869.618f, 62.5f, 7, 5, 2},
  {"Netherlands",                 869.618f, 62.5f, 7, 5, 0},
  {"Netherlands (Limburg)",       869.618f, 62.5f, 8, 8, 2},
  {"Portugal 433",                433.375f, 62.5f, 9, 6, 0},
  {"Portugal 868",                869.618f, 62.5f, 7, 6, 0},
  {"Slovakia",                    869.618f, 62.5f, 7, 5, 2},
  {"Switzerland",                 869.618f, 62.5f, 8, 8, 0},
  {"Australia",                   915.800f, 250.f, 10, 5, 0},
  {"Australia (Narrow)",          916.575f, 62.5f, 7, 7, 0},
  {"Australia (Mid)",             915.075f, 125.f, 9, 5, 0},
  {"Australia: SA, WA",           923.125f, 62.5f, 8, 8, 0},
  {"Australia: QLD",              923.125f, 62.5f, 8, 5, 0},
  {"New Zealand (Narrow)",        917.375f, 62.5f, 7, 5, 2},
  {"New Zealand (Gisborne)",      917.375f, 250.f, 11, 5, 1},
  {"Vietnam (Narrow)",            920.250f, 62.5f, 8, 5, 0},
  {"Vietnam (Deprecated)",        920.250f, 250.f, 11, 5, 0},
};
static const int kRadioPresetCount = sizeof(kRadioPresets) / sizeof(kRadioPresets[0]);

}  // namespace app
