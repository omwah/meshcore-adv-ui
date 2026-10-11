#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

class Print {
public:
  virtual ~Print() {}
  virtual size_t write(uint8_t c) = 0;
  virtual size_t write(const uint8_t* buf, size_t n) {
    size_t i = 0;
    while (i < n && write(buf[i])) i++;
    return i;
  }
  size_t write(const char* s) { return write((const uint8_t*)s, strlen(s)); }
  size_t print(const char* s) { return write(s); }
  size_t print(char c) { return write((uint8_t)c); }
  size_t print(long v) { char b[24]; snprintf(b, sizeof b, "%ld", v); return write(b); }
  size_t print(int v) { return print((long)v); }
  size_t print(unsigned long v) { char b[24]; snprintf(b, sizeof b, "%lu", v); return write(b); }
  size_t print(unsigned v) { return print((unsigned long)v); }
  size_t print(double v, int d = 2) { char b[32]; snprintf(b, sizeof b, "%.*f", d, v); return write(b); }
  template <typename T> size_t println(T v) { size_t n = print(v); return n + write("\r\n"); }
  size_t println() { return write("\r\n"); }
  size_t printf(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
    char b[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    return write(b);
  }
  virtual void flush() {}
};

class Stream : public Print {
public:
  virtual int available() = 0;
  virtual int read() = 0;
  virtual int peek() = 0;
  size_t readBytes(uint8_t* buf, size_t n) {
    size_t i = 0;
    for (; i < n; i++) {
      int c = read();
      if (c < 0) break;
      buf[i] = (uint8_t)c;
    }
    return i;
  }
  size_t readBytes(char* buf, size_t n) { return readBytes((uint8_t*)buf, n); }
  void setTimeout(unsigned long) {}
};
