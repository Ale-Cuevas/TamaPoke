#pragma once
// Host stand-in for the Arduino-ESP32 core, scoped to what TamaPoke's sketch,
// pet.cpp, i18n.cpp, sdmon.cpp and Arduino_GFX's canvas need to compile and
// draw on a PC. Time is a fake clock the renderer moves by hand.

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "pgmspace.h"
#include "Print.h"
#include "Printable.h"
#include "WString.h"

using std::max;
using std::min;

typedef uint8_t byte;
typedef bool boolean;

#define LOW 0x0
#define HIGH 0x1
#define INPUT 0x01
#define OUTPUT 0x03
#define INPUT_PULLUP 0x05
#define FALLING 0x02
#define RISING 0x01
#define CHANGE 0x03
#define IRAM_ATTR
#define PI 3.1415926535897932384626433832795
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#define highByte(w) ((uint8_t)((w) >> 8))
#define lowByte(w) ((uint8_t)((w) & 0xff))

// ---- fake clock (render.cpp sets it) ---------------------------------------
extern uint32_t gHostMillis;
inline uint32_t millis() { return gHostMillis; }
inline unsigned long micros() { return (unsigned long)gHostMillis * 1000UL; }
inline void delay(uint32_t ms) { gHostMillis += ms; }
inline void yield() {}

// ---- deterministic random() ------------------------------------------------
extern uint32_t gHostRandom;
inline long random(long howbig) {
  if (howbig <= 0) return 0;
  gHostRandom = gHostRandom * 1664525u + 1013904223u;
  return (long)((gHostRandom >> 8) % (uint32_t)howbig);
}
inline long random(long lo, long hi) { return lo >= hi ? lo : lo + random(hi - lo); }
inline void randomSeed(unsigned long s) { gHostRandom = (uint32_t)s; }
inline uint32_t esp_random() { return (uint32_t)random(0x7fffffff); }

// ---- GPIO: nothing attached ------------------------------------------------
inline void pinMode(uint8_t, uint8_t) {}
inline void digitalWrite(uint8_t, uint8_t) {}
inline int digitalRead(uint8_t) { return HIGH; }  // lines idle high (pull-ups)
inline uint8_t digitalPinToInterrupt(uint8_t p) { return p; }
inline void attachInterrupt(uint8_t, void (*)(), int) {}
inline void detachInterrupt(uint8_t) {}

// ---- memory ------------------------------------------------------------------
inline void *ps_malloc(size_t n) { return malloc(n); }
inline bool psramFound() { return true; }

// ---- USB serial: logs go to stderr when TAMAPOKE_RENDER_VERBOSE is set --------
class HardwareSerial : public Print {
 public:
  void begin(unsigned long) {}
  void setRxBufferSize(size_t) {}
  void setTxTimeoutMs(uint32_t) {}
  void setTimeout(unsigned long) {}
  int available() { return 0; }
  size_t readBytes(uint8_t *, size_t) { return 0; }
  size_t readBytes(char *, size_t) { return 0; }
  String readStringUntil(char) { return String(""); }
  size_t write(uint8_t c) override {
    if (getenv("TAMAPOKE_RENDER_VERBOSE")) fputc(c, stderr);
    return 1;
  }
  using Print::write;
  operator bool() const { return true; }
};
extern HardwareSerial Serial;

class EspClass {
 public:
  uint32_t getFreeHeap() { return 200000; }
  uint32_t getMinFreeHeap() { return 180000; }
  [[noreturn]] void restart() { exit(0); }
};
extern EspClass ESP;
