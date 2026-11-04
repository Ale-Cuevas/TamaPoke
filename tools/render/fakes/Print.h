#pragma once
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "Printable.h"
#include "WString.h"

#define DEC 10
#define HEX 16

// Arduino Print: everything goes through write(uint8_t).
class Print {
 public:
  virtual ~Print() {}
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t *buf, size_t n) {
    size_t k = 0;
    while (n--) k += write(*buf++);
    return k;
  }
  virtual void flush() {}
  size_t write(const char *s) { return s ? write((const uint8_t *)s, strlen(s)) : 0; }
  size_t print(const char *s) { return write(s); }
  size_t print(const __FlashStringHelper *s) { return write(reinterpret_cast<const char *>(s)); }
  size_t print(const String &s) { return write(s.c_str()); }
  size_t print(char c) { return write((uint8_t)c); }
  size_t print(int n, int base = DEC) { return printNum((long)n, base); }
  size_t print(unsigned n, int base = DEC) { return printNum((unsigned long)n, base); }
  size_t print(long n, int base = DEC) { return printNum(n, base); }
  size_t print(unsigned long n, int base = DEC) { return printNum(n, base); }
  size_t print(double d, int digits = 2) {
    char b[48];
    snprintf(b, sizeof b, "%.*f", digits, d);
    return write(b);
  }
  size_t print(const Printable &p) { return p.printTo(*this); }
  size_t println() { return write("\r\n"); }
  template <class T> size_t println(const T &v) { return print(v) + println(); }
  template <class T> size_t println(const T &v, int f) { return print(v, f) + println(); }
  size_t printf(const char *fmt, ...) __attribute__((format(printf, 2, 3))) {
    char b[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    return write(b);
  }

 private:
  size_t printNum(long n, int base) {
    char b[40];
    snprintf(b, sizeof b, base == HEX ? "%lx" : "%ld", n);
    return write(b);
  }
  size_t printNum(unsigned long n, int base) {
    char b[40];
    snprintf(b, sizeof b, base == HEX ? "%lx" : "%lu", n);
    return write(b);
  }
};
