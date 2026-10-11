// Time zones as POSIX TZ rules ("PST8PDT,M3.2.0,M11.1.0"), evaluated by a
// small portable parser so the device and the simulator agree without
// depending on libc's TZ support.
#pragma once
#include <stdint.h>

namespace ui {

struct Zone {
  const char* name;    // shown in Settings
  const char* posix;   // POSIX TZ rule
};

extern const Zone kZones[];
extern const int kZoneCount;

// Parse a POSIX TZ rule; false (and UTC) if it can't be parsed.
bool setTimeZone(const char* posix);
// Seconds to add to UTC for local time at `utc`, under the current zone.
int32_t utcOffset(uint32_t utc);
inline uint32_t toLocal(uint32_t utc) { return (uint32_t)((int64_t)utc + utcOffset(utc)); }

// Days since 1970-01-01 for a civil date (proleptic Gregorian).
int32_t daysFromCivil(int y, unsigned m, unsigned d);
void civilFromDays(int32_t z, int& y, unsigned& m, unsigned& d);

}  // namespace ui
