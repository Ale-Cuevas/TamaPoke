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
  explicit RoundCanvas(Arduino_GFX *panel)
      : Arduino_Canvas(UI_SIZE, UI_SIZE, panel, UI_ORIGIN, UI_ORIGIN), panel_(panel) {}

  // Burn-in orbit (burnin.h): from the next flush the canvas goes to the panel
  // at UI_ORIGIN + (dx, dy). Both must be even (the CO5300 only takes windows
  // that start on an even pixel) and keep the canvas on the panel.
  void setShift(int8_t dx, int8_t dy) {
    wantX_ = UI_ORIGIN + dx;
    wantY_ = UI_ORIGIN + dy;
  }
  // where the canvas is on the panel right now (touch maps through this)
  int16_t originX() const { return _output_x; }
  int16_t originY() const { return _output_y; }

  // Call once after begin(): builds the mask (about 8 KB of RAM).
  bool beginFrame() {
#if TP_ROUND_FRAME
    return mask_.build(UI_SIZE, UI_FRAME_CENTER, UI_FRAME_CENTER, UI_FRAME_RADIUS);
#else
    return true;
#endif
  }

  // Drawn on top of every frame right before it goes out (the brightness
  // toast): every screen ends in its own flush(), so this is the one place
  // they all pass through.
  void (*overlay)() = nullptr;

  void flush(bool force_flush = false) override {
    if (wantX_ != _output_x || wantY_ != _output_y) moveTo(wantX_, wantY_);
    if (overlay) overlay();
#if TP_ROUND_FRAME
    mask_.apply(getFramebuffer());  // about 93 KB of corners zeroed + 1.4 K edge pixels
#endif
    Arduino_Canvas::flush(force_flush);
  }

 private:
  // The panel keeps whatever it was last sent, so the strip the canvas no longer
  // covers after a move would show the old edge: paint everything around the new
  // position black (four even-aligned bands), then send frames there.
  void moveTo(int16_t x, int16_t y) {
    _output_x = x;
    _output_y = y;
    const int16_t W = BOARD_LCD_WIDTH, H = BOARD_LCD_HEIGHT;
    if (y > 0) panel_->fillRect(0, 0, W, y, RGB565_BLACK);
    if (y + UI_SIZE < H) panel_->fillRect(0, y + UI_SIZE, W, H - y - UI_SIZE, RGB565_BLACK);
    if (x > 0) panel_->fillRect(0, y, x, UI_SIZE, RGB565_BLACK);
    if (x + UI_SIZE < W) panel_->fillRect(x + UI_SIZE, y, W - x - UI_SIZE, UI_SIZE, RGB565_BLACK);
  }

  RoundMask mask_;
  Arduino_GFX *panel_;
  int16_t wantX_ = UI_ORIGIN, wantY_ = UI_ORIGIN;
};

// Panel set-up that has to follow gfx->begin(): orientation as in Waveshare's
// demo, then the whole panel cleared to black once. The border around the
// canvas is never drawn again, and the panel keeps it in its own RAM.
inline void roundDisplayBegin(Arduino_DataBus *bus, Arduino_GFX *panel, RoundCanvas *canvas) {
  bus->writeC8D8(BOARD_LCD_MADCTL_REG, BOARD_LCD_MADCTL_VALUE);
  panel->fillScreen(RGB565_BLACK);
  if (!canvas->beginFrame()) Serial.println("round frame: out of memory, showing the square canvas");
}
