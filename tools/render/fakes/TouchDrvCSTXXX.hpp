#pragma once
#include "Wire.h"
// No finger ever touches the rendered screens.
class TouchDrvCST92xx {
 public:
  void setPins(int, int) {}
  bool begin(TwoWire &, uint8_t, int, int) { return true; }
  void reset() {}
  void setMaxCoordinates(uint16_t, uint16_t) {}
  void setSwapXY(bool) {}
  void setMirrorXY(bool, bool) {}
  uint8_t getPoint(int16_t *, int16_t *, uint8_t) { return 0; }
};
