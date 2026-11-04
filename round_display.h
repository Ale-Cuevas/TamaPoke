#pragma once
#include <Arduino_GFX_Library.h>
#include "board_amoled_216.h"
#include "roundmask.h"
#include "ui_frame.h"

// Shows TamaPoke's 466x466 round UI on the 480x480 AMOLED-2.16 panel: the
// canvas is blitted at UI_ORIGIN and masked to the round frame. The geometry
// and why it is what it is: ui_frame.h.

// Arduino_Canvas that applies the round frame right before each frame goes to
// the panel. Drawing code is unchanged: it still calls gfx->flush().
class RoundCanvas : public Arduino_Canvas {
 public:
  explicit RoundCanvas(Arduino_G *panel)
      : Arduino_Canvas(UI_SIZE, UI_SIZE, panel, UI_ORIGIN, UI_ORIGIN) {}

  // Call once after begin(): builds the mask (about 8 KB of RAM).
  bool beginFrame() {
#if TP_ROUND_FRAME
    return mask_.build(UI_SIZE, UI_FRAME_CENTER, UI_FRAME_CENTER, UI_FRAME_RADIUS);
#else
    return true;
#endif
  }

  void flush(bool force_flush = false) override {
#if TP_ROUND_FRAME
    mask_.apply(getFramebuffer());  // about 93 KB of corners zeroed + 1.4 K edge pixels
#endif
    Arduino_Canvas::flush(force_flush);
  }

 private:
  RoundMask mask_;
};

// Panel set-up that has to follow gfx->begin(): orientation as in Waveshare's
// demo, then the whole panel cleared to black once. The border around the
// canvas is never drawn again, and the panel keeps it in its own RAM.
inline void roundDisplayBegin(Arduino_DataBus *bus, Arduino_GFX *panel, RoundCanvas *canvas) {
  bus->writeC8D8(BOARD_LCD_MADCTL_REG, BOARD_LCD_MADCTL_VALUE);
  panel->fillScreen(RGB565_BLACK);
  if (!canvas->beginFrame()) Serial.println("round frame: out of memory, showing the square canvas");
}
