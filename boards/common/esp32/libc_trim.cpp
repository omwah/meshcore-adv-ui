// Flash savings in the C library.
//
// NimBLE parses addresses from text with sscanf (NimBLEAddress(std::string),
// reached even from its default constructor), which links newlib's whole
// scanf, about 20 KB. Nothing else in this firmware calls sscanf at run time,
// so -Wl,--wrap=sscanf (platformio.ini) sends every call here: the one format
// NimBLE uses, "%x:%x:%x:%x:%x:%x". Any other format reads nothing.
#include <stdarg.h>
#include <string.h>

extern "C" int __wrap_sscanf(const char* s, const char* fmt, ...) {
  if (strcmp(fmt, "%x:%x:%x:%x:%x:%x") != 0) return 0;
  va_list ap;
  va_start(ap, fmt);
  int n = 0;
  while (n < 6) {
    unsigned v = 0;
    int digits = 0;
    for (;; s++, digits++) {
      char c = *s;
      int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
      if (d < 0) break;
      v = v * 16 + d;
    }
    if (!digits) break;
    *va_arg(ap, int*) = (int)v;
    n++;
    if (n < 6) {
      if (*s != ':') break;
      s++;
    }
  }
  va_end(ap);
  return n;
}

// newlib's tzset parses the TZ environment variable with siscanf (the
// integer scanf, about 8 KB), reached through mktime in MeshCore's RX8130CE
// RTC driver. This firmware never sets TZ (time zones are the UI's own,
// src/ui/tz.cpp), so tzset never parses anything: -Wl,--wrap=siscanf.
extern "C" int __wrap_siscanf(const char*, const char*, ...) { return 0; }
