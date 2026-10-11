#pragma once
#include <stdint.h>
#include <time.h>

// The part of RTClib's DateTime that MeshCore uses (UTC fields from epoch).
class DateTime {
public:
  explicit DateTime(uint32_t t = 0) {
    time_t tt = (time_t)t;
    gmtime_r(&tt, &_tm);
  }
  int year() const { return _tm.tm_year + 1900; }
  int month() const { return _tm.tm_mon + 1; }
  int day() const { return _tm.tm_mday; }
  int hour() const { return _tm.tm_hour; }
  int minute() const { return _tm.tm_min; }
  int second() const { return _tm.tm_sec; }

private:
  struct tm _tm;
};
