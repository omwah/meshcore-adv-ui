// Minimal Arduino API for compiling MeshCore on a host. Time is virtual:
// the simulator advances it (sim_setMillis), so runs are deterministic.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "Stream.h"

typedef bool boolean;
typedef uint8_t byte;
#define PROGMEM
#define F(x) (x)
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1

unsigned long millis();
unsigned long micros();
void delay(unsigned long ms);
void yield();
long random(long max);
long random(long min, long max);
void randomSeed(unsigned long seed);
char* ltoa(long v, char* buf, int base);
char* itoa(int v, char* buf, int base);
inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline int digitalRead(int) { return 0; }

// Host-side control of the virtual clock.
void sim_setMillis(unsigned long ms);

class HardwareSerial : public Stream {
public:
  void begin(unsigned long) {}
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  size_t write(uint8_t c) override;
  using Print::write;
  operator bool() const { return true; }
};
extern HardwareSerial Serial;

template <typename A, typename B> inline auto min(A a, B b) -> decltype(a < b ? a : b) { return a < b ? a : b; }
template <typename A, typename B> inline auto max(A a, B b) -> decltype(a > b ? a : b) { return a > b ? a : b; }
template <typename T, typename L, typename H> inline T constrain(T v, L lo, H hi) { return v < lo ? lo : v > hi ? hi : v; }
