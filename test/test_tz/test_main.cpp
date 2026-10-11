// POSIX TZ evaluation against known 2026 transitions.
#include <unity.h>
#include "ui/tz.h"

using namespace ui;

static uint32_t utc(int y, unsigned m, unsigned d, int hh, int mm) {
  return (uint32_t)(daysFromCivil(y, m, d) * 86400 + hh * 3600 + mm * 60);
}

void test_civil_round_trip() {
  int y;
  unsigned m, d;
  civilFromDays(daysFromCivil(2026, 10, 11), y, m, d);
  TEST_ASSERT_EQUAL(2026, y);
  TEST_ASSERT_EQUAL(10, m);
  TEST_ASSERT_EQUAL(11, d);
  TEST_ASSERT_EQUAL(0, daysFromCivil(1970, 1, 1));
}

void test_us_pacific_2026() {
  TEST_ASSERT_TRUE(setTimeZone("PST8PDT,M3.2.0,M11.1.0"));
  // DST starts Sun 8 Mar 2026 02:00 PST = 10:00 UTC.
  TEST_ASSERT_EQUAL(-8 * 3600, utcOffset(utc(2026, 3, 8, 9, 59)));
  TEST_ASSERT_EQUAL(-7 * 3600, utcOffset(utc(2026, 3, 8, 10, 0)));
  // Ends Sun 1 Nov 2026 02:00 PDT = 09:00 UTC.
  TEST_ASSERT_EQUAL(-7 * 3600, utcOffset(utc(2026, 11, 1, 8, 59)));
  TEST_ASSERT_EQUAL(-8 * 3600, utcOffset(utc(2026, 11, 1, 9, 0)));
}

void test_central_europe_2026() {
  TEST_ASSERT_TRUE(setTimeZone("CET-1CEST,M3.5.0,M10.5.0/3"));
  // Last Sunday of March 2026 is the 29th; 02:00 CET = 01:00 UTC.
  TEST_ASSERT_EQUAL(3600, utcOffset(utc(2026, 3, 29, 0, 59)));
  TEST_ASSERT_EQUAL(7200, utcOffset(utc(2026, 3, 29, 1, 0)));
  // Last Sunday of October 2026 is the 25th; 03:00 CEST = 01:00 UTC.
  TEST_ASSERT_EQUAL(7200, utcOffset(utc(2026, 10, 25, 0, 59)));
  TEST_ASSERT_EQUAL(3600, utcOffset(utc(2026, 10, 25, 1, 0)));
}

void test_southern_hemisphere_new_zealand() {
  TEST_ASSERT_TRUE(setTimeZone("NZST-12NZDT,M9.5.0,M4.1.0/3"));
  TEST_ASSERT_EQUAL(13 * 3600, utcOffset(utc(2026, 1, 15, 0, 0)));   // summer
  TEST_ASSERT_EQUAL(12 * 3600, utcOffset(utc(2026, 7, 15, 0, 0)));   // winter
}

void test_fixed_and_half_hour_offsets() {
  TEST_ASSERT_TRUE(setTimeZone("IST-5:30"));
  TEST_ASSERT_EQUAL(5 * 3600 + 1800, utcOffset(utc(2026, 6, 1, 0, 0)));
  TEST_ASSERT_TRUE(setTimeZone("<-03>3"));
  TEST_ASSERT_EQUAL(-3 * 3600, utcOffset(utc(2026, 6, 1, 0, 0)));
}

void test_every_listed_zone_parses() {
  for (int i = 0; i < kZoneCount; i++) TEST_ASSERT_TRUE_MESSAGE(setTimeZone(kZones[i].posix), kZones[i].name);
  TEST_ASSERT_FALSE(setTimeZone("garbage"));
  TEST_ASSERT_EQUAL(0, utcOffset(1000));   // falls back to UTC
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_civil_round_trip);
  RUN_TEST(test_us_pacific_2026);
  RUN_TEST(test_central_europe_2026);
  RUN_TEST(test_southern_hemisphere_new_zealand);
  RUN_TEST(test_fixed_and_half_hour_offsets);
  RUN_TEST(test_every_listed_zone_parses);
  return UNITY_END();
}
