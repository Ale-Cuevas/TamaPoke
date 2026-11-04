#pragma once
// The real Arduino_GFX core and canvas, plus host versions of the two
// ESP32-only classes the sketch constructs. The "panel" is itself a 480x480
// canvas: it keeps whatever is drawn on it, like the AMOLED's own RAM, and
// render.cpp saves it as a PNG.
#include <Arduino_GFX.h>
#include <Arduino_DataBus.h>
#include <canvas/Arduino_Canvas.h>

class Arduino_ESP32QSPI : public Arduino_DataBus {
 public:
  Arduino_ESP32QSPI(int8_t, int8_t, int8_t, int8_t, int8_t, int8_t) {}
  bool begin(int32_t = 0, int8_t = 0) override { return true; }
  void beginWrite() override {}
  void endWrite() override {}
  void writeCommand(uint8_t) override {}
  void writeCommand16(uint16_t) override {}
  void writeCommandBytes(uint8_t *, uint32_t) override {}
  void write(uint8_t) override {}
  void write16(uint16_t) override {}
  void writeC8D8(uint8_t, uint8_t) override {}
  void writeRepeat(uint16_t, uint32_t) override {}
  void writeBytes(uint8_t *, uint32_t) override {}
  void writePixels(uint16_t *, uint32_t) override {}
};

class Arduino_CO5300 : public Arduino_Canvas {
 public:
  Arduino_CO5300(Arduino_DataBus *, int8_t, uint8_t, int16_t w, int16_t h, uint8_t, uint8_t, uint8_t,
                 uint8_t)
      : Arduino_Canvas(w, h, nullptr) {}
  bool begin(int32_t = GFX_NOT_DEFINED) override {
    if (!Arduino_Canvas::begin(GFX_SKIP_OUTPUT_BEGIN)) return false;
    // power-on RAM content is undefined on the real panel: start with noise so
    // a missing clear shows up in the renders
    uint16_t *fb = getFramebuffer();
    for (int32_t i = 0; i < (int32_t)_width * _height; i++) fb[i] = (uint16_t)(i * 2654435761u >> 16);
    return true;
  }
  void flush(bool = false) override {}
  void setBrightness(uint8_t b) { brightness = b; }
  void displayOn() override {}
  void displayOff() override {}
  uint8_t brightness = 255;
};
