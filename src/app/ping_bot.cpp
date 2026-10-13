#include "app/ping_bot.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace app {

namespace {

// snprintf that keeps count within the buffer.
void add(char* out, int n, int& o, const char* fmt, ...) {
  if (o >= n - 1) return;
  va_list ap;
  va_start(ap, fmt);
  int w = vsnprintf(out + o, n - o, fmt, ap);
  va_end(ap);
  o = w < 0 ? o : o + w < n - 1 ? o + w : n - 1;
}

// SNR with one decimal, without printf's float support.
void addSnr(char* out, int n, int& o, float snr) {
  int t = (int)(snr * 10 + (snr < 0 ? -0.5f : 0.5f));
  add(out, n, o, "SNR %s%d.%d", t < 0 ? "-" : "", (t < 0 ? -t : t) / 10, (t < 0 ? -t : t) % 10);
}

}  // namespace

bool isPing(const char* text) {
  while (*text == ' ') text++;
  const char* const words[] = {"ping", "test"};
  for (const char* w : words) {
    int i = 0;
    while (w[i] && tolower((unsigned char)text[i]) == w[i]) i++;
    if (w[i]) continue;
    const char* r = text + i;
    while (*r && (*r == ' ' || ispunct((unsigned char)*r))) r++;
    if (!*r) return true;
  }
  return false;
}

int formatPong(char* out, int n, uint8_t pathLen, const uint8_t* path, bool stored, float snr, int rssi,
               RelayNamer namer, void* ctx) {
  int o = 0;
  out[0] = 0;
  if (pathLen == 0xFF) {
    add(out, n, o, "pong: by stored route, ");
    addSnr(out, n, o, snr);
    return o;
  }
  int hops = pathLen & 63, size = (pathLen >> 6) + 1;
  if (hops == 0) {   // a neighbour, however it was routed
    add(out, n, o, "pong: direct, ");
    addSnr(out, n, o, snr);
    add(out, n, o, ", RSSI %d", rssi);
    return o;
  }
  add(out, n, o, "pong: %d hop%s", hops, hops == 1 ? "" : "s");
  if (path) {
    for (int h = 0; h < hops; h++) {
      const uint8_t* hash = path + h * size;
      char name[11];
      add(out, n, o, h ? ">" : " ");
      if (namer && namer(ctx, hash, size, name, sizeof name)) {
        add(out, n, o, "%s", name);
      } else {
        for (int b = 0; b < size; b++) add(out, n, o, "%02x", hash[b]);
      }
    }
  }
  if (stored) add(out, n, o, " (stored route)");
  add(out, n, o, ", ");
  addSnr(out, n, o, snr);
  return o;
}

}  // namespace app
