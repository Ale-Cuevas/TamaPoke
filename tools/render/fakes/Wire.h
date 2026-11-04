#pragma once
#include "Arduino.h"
// Empty I2C bus: nothing answers (the renderer stubs every chip driver).
class TwoWire {
 public:
  bool begin(int = -1, int = -1, uint32_t = 0) { return true; }
  void setTimeOut(uint16_t) {}
  bool setClock(uint32_t) { return true; }
  void beginTransmission(uint8_t) {}
  uint8_t endTransmission(bool = true) { return 0; }
  size_t write(uint8_t) { return 1; }
  uint8_t requestFrom(uint8_t, uint8_t) { return 0; }
  int available() { return 0; }
  int read() { return -1; }
};
extern TwoWire Wire;
