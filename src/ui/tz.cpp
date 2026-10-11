#include "ui/tz.h"

namespace ui {

const Zone kZones[] = {
  {"UTC",                       "UTC0"},
  {"US Pacific",                "PST8PDT,M3.2.0,M11.1.0"},
  {"US Mountain",               "MST7MDT,M3.2.0,M11.1.0"},
  {"US Arizona",                "MST7"},
  {"US Central",                "CST6CDT,M3.2.0,M11.1.0"},
  {"US Eastern",                "EST5EDT,M3.2.0,M11.1.0"},
  {"US Alaska",                 "AKST9AKDT,M3.2.0,M11.1.0"},
  {"US Hawaii",                 "HST10"},
  {"Canada Atlantic",           "AST4ADT,M3.2.0,M11.1.0"},
  {"Newfoundland",              "NST3:30NDT,M3.2.0,M11.1.0"},
  {"Mexico City",               "CST6"},
  {"UTC-3 Brazil, Argentina",   "<-03>3"},
  {"UK, Ireland, Portugal",     "GMT0BST,M3.5.0/1,M10.5.0"},
  {"Central Europe",            "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Eastern Europe",            "EET-2EEST,M3.5.0/3,M10.5.0/4"},
  {"South Africa",              "SAST-2"},
  {"UTC+3 Moscow, Istanbul",    "<+03>-3"},
  {"UTC+4 Gulf",                "<+04>-4"},
  {"Pakistan",                  "PKT-5"},
  {"India",                     "IST-5:30"},
  {"UTC+6 Bangladesh",          "<+06>-6"},
  {"UTC+7 Thailand, Vietnam",   "<+07>-7"},
  {"UTC+8 China, Singapore",    "CST-8"},
  {"Japan, Korea",              "JST-9"},
  {"Australia Central",         "ACST-9:30ACDT,M10.1.0,M4.1.0/3"},
  {"Australia Queensland",      "AEST-10"},
  {"Australia Eastern",         "AEST-10AEDT,M10.1.0,M4.1.0/3"},
  {"New Zealand",               "NZST-12NZDT,M9.5.0,M4.1.0/3"},
};
const int kZoneCount = sizeof(kZones) / sizeof(kZones[0]);

namespace {

struct Rule {
  uint8_t month, week, wday;   // Mm.w.d
  int32_t time;                // seconds after local midnight
};

struct Tz {
  int32_t stdOff = 0, dstOff = 0;   // seconds east of UTC
  bool hasDst = false;
  Rule start{}, end{};
} g_tz;

const char* skipName(const char* p) {
  if (*p == '<') {
    while (*p && *p != '>') p++;
    return *p ? p + 1 : nullptr;
  }
  const char* s = p;
  while ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z')) p++;
  return p - s >= 3 ? p : nullptr;
}

// [+-]hh[:mm[:ss]] -> seconds. Returns null on error.
const char* parseTime(const char* p, int32_t& out) {
  int sign = 1;
  if (*p == '+' || *p == '-') { if (*p == '-') sign = -1; p++; }
  if (*p < '0' || *p > '9') return nullptr;
  int32_t parts[3] = {0, 0, 0};
  for (int i = 0; i < 3; i++) {
    int v = 0, n = 0;
    while (*p >= '0' && *p <= '9') { v = v * 10 + (*p++ - '0'); n++; }
    if (!n) return nullptr;
    parts[i] = v;
    if (*p != ':' || i == 2) break;
    p++;
  }
  out = sign * (parts[0] * 3600 + parts[1] * 60 + parts[2]);
  return p;
}

const char* parseRule(const char* p, Rule& r) {
  if (*p != 'M') return nullptr;   // only the Mm.w.d form is used in practice
  p++;
  int v[3] = {0, 0, 0};
  for (int i = 0; i < 3; i++) {
    int n = 0;
    while (*p >= '0' && *p <= '9') { v[i] = v[i] * 10 + (*p++ - '0'); n++; }
    if (!n) return nullptr;
    if (i < 2) { if (*p != '.') return nullptr; p++; }
  }
  if (v[0] < 1 || v[0] > 12 || v[1] < 1 || v[1] > 5 || v[2] > 6) return nullptr;
  r.month = (uint8_t)v[0];
  r.week = (uint8_t)v[1];
  r.wday = (uint8_t)v[2];
  r.time = 7200;
  if (*p == '/') {
    p = parseTime(p + 1, r.time);
    if (!p) return nullptr;
  }
  return p;
}

unsigned daysInMonth(int y, unsigned m) {
  static const uint8_t d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
  return m == 2 && leap ? 29 : d[m - 1];
}

// Local midnight (as days since epoch) of the rule's date in year y.
int32_t ruleDay(int y, const Rule& r) {
  int32_t first = daysFromCivil(y, r.month, 1);
  int wd1 = (int)((first % 7 + 7 + 4) % 7);   // 1970-01-01 was a Thursday
  unsigned dom = 1 + (unsigned)((r.wday - wd1 + 7) % 7) + (r.week - 1) * 7u;
  while (dom > daysInMonth(y, r.month)) dom -= 7;
  return daysFromCivil(y, r.month, dom);
}

}  // namespace

int32_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int32_t)doe - 719468;
}

void civilFromDays(int32_t z, int& y, unsigned& m, unsigned& d) {
  z += 719468;
  const int era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = (unsigned)(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  d = doy - (153 * mp + 2) / 5 + 1;
  m = mp < 10 ? mp + 3 : mp - 9;
  y = (int)yoe + era * 400 + (m <= 2);
}

bool setTimeZone(const char* posix) {
  Tz tz;
  const char* p = posix ? skipName(posix) : nullptr;
  int32_t off;
  if (!p || !(p = parseTime(p, off))) { g_tz = Tz(); return false; }
  tz.stdOff = tz.dstOff = -off;   // POSIX offsets are hours *west*
  if (*p) {
    p = skipName(p);
    if (!p) { g_tz = Tz(); return false; }
    tz.dstOff = tz.stdOff + 3600;
    if (*p && *p != ',') {
      if (!(p = parseTime(p, off))) { g_tz = Tz(); return false; }
      tz.dstOff = -off;
    }
    if (*p != ',' || !(p = parseRule(p + 1, tz.start)) || *p != ',' || !(p = parseRule(p + 1, tz.end))) {
      g_tz = Tz();
      return false;
    }
    tz.hasDst = true;
  }
  g_tz = tz;
  return true;
}

int32_t utcOffset(uint32_t utc) {
  if (!g_tz.hasDst) return g_tz.stdOff;
  int y;
  unsigned m, d;
  civilFromDays((int32_t)((utc + (int64_t)g_tz.stdOff) / 86400), y, m, d);
  // Transitions in UTC: DST starts at a local standard time and ends at a
  // local daylight time.
  int64_t start = (int64_t)ruleDay(y, g_tz.start) * 86400 + g_tz.start.time - g_tz.stdOff;
  int64_t end = (int64_t)ruleDay(y, g_tz.end) * 86400 + g_tz.end.time - g_tz.dstOff;
  bool dst = start < end ? (utc >= start && utc < end)       // northern hemisphere
                         : !(utc >= end && utc < start);     // southern: DST spans New Year
  return dst ? g_tz.dstOff : g_tz.stdOff;
}

}  // namespace ui
