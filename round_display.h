#pragma once
#include <Arduino_GFX_Library.h>
#include "board_amoled_216.h"
#include "roundmask.h"
#include "ui_frame.h"

// Shows TamaPoke's 466x466 round UI on the 480x480 AMOLED-2.16 panel: the
// canvas is blitted at UI_ORIGIN and masked to the round frame. The geometry
// and why it is what it is: ui_frame.h. Since v2.5 the yard fills the whole
// panel around that round frame (showUnder, TP_FULL_YARD).

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

  // Call once after begin(): builds the mask (about 8 KB of RAM) and, with
  // TP_FULL_YARD, the scratch rows the border is painted from (11.5 KB).
  bool beginFrame() {
#if TP_FULL_YARD
    scratch_ = (uint16_t *)malloc(sizeof(uint16_t) * SCRATCH_PX);
#endif
#if TP_ROUND_FRAME
    return mask_.build(UI_SIZE, UI_FRAME_CENTER, UI_FRAME_CENTER, UI_FRAME_RADIUS);
#else
    return true;
#endif
  }

  // v2.5 (TP_FULL_YARD): the next flush shows `full` around the round UI
  // instead of black: the canvas corners and the panel border. `full` is a
  // side x side image with the canvas at (crop, crop) inside it (the 480x480
  // yard map, crop 7). `key` names its content: the border, which the panel
  // keeps in its own RAM, is only sent again when the key or the canvas
  // position changes. Only that one flush: a screen that doesn't call it
  // again gets the black frame back.
  // v3.6: stride = the row length of `full` when it is a window of a bigger image (the yard world); 0 = side
  void showUnder(const uint16_t *full, int16_t side, int16_t crop, uint32_t key, int16_t stride = 0) {
#if TP_FULL_YARD
    under_ = full;
    underSide_ = side;
    underCrop_ = crop;
    underKey_ = key;
    underStride_ = stride ? stride : side;
#else
    (void)full; (void)side; (void)crop; (void)key; (void)stride;
#endif
  }

  // Drawn on top of every frame right before it goes out (the brightness
  // toast): every screen ends in its own flush(), so this is the one place
  // they all pass through.
  void (*overlay)() = nullptr;

  void flush(bool force_flush = false) override {
    if (wantX_ != _output_x || wantY_ != _output_y) moveTo(wantX_, wantY_);
    if (overlay) overlay();
#if TP_FULL_YARD
    const uint16_t *u = under_;
    under_ = nullptr;  // one frame only
#else
    const uint16_t *u = nullptr;
#endif
#if TP_ROUND_FRAME
    // about 93 KB of corners zeroed (or copied from the map) + 1.4 K edge pixels
    if (u) mask_.apply(getFramebuffer(), u + (uint32_t)underCrop_ * underStride_ + underCrop_, underStride_);
    else mask_.apply(getFramebuffer());
#endif
    uint32_t want = u ? underKey_ : BORDER_BLACK;
    if (want != borderKey_) {
      if (u && scratch_) paintBorderFrom(u);
      else {
        paintBorderBlack();
        want = BORDER_BLACK;
      }
      borderKey_ = want;
    }
    Arduino_Canvas::flush(force_flush);
  }

 private:
  // The panel keeps whatever it was last sent, so the strip the canvas no longer
  // covers after a move would show the old edge: the border is marked stale and
  // the same flush paints everything around the new position again (black, or
  // the yard map), then sends the frame there.
  void moveTo(int16_t x, int16_t y) {
    _output_x = x;
    _output_y = y;
    borderKey_ = BORDER_STALE;
  }

  // The border as four bands, all starting and spanning an even number of
  // pixels (the CO5300 rule; the canvas position and size are even): above,
  // below, then left and right of the canvas.
  template <class Paint>
  void forBands(Paint paint) const {
    const int16_t W = BOARD_LCD_WIDTH, H = BOARD_LCD_HEIGHT, x = _output_x, y = _output_y;
    if (y > 0) paint(0, 0, W, y);
    if (y + UI_SIZE < H) paint(0, y + UI_SIZE, W, H - y - UI_SIZE);
    if (x > 0) paint(0, y, x, UI_SIZE);
    if (x + UI_SIZE < W) paint(x + UI_SIZE, y, W - x - UI_SIZE, UI_SIZE);
  }

  void paintBorderBlack() {
    forBands([this](int16_t x, int16_t y, int16_t w, int16_t h) { panel_->fillRect(x, y, w, h, RGB565_BLACK); });
  }

  // Panel pixel (X, Y) shows map pixel (X - ox + crop, Y - oy + crop), so the
  // map runs on from the canvas without a seam. The burn-in orbit can ask for a
  // few pixels past the map's edge: those repeat the edge pixel.
  void paintBorderFrom(const uint16_t *full) {
    const int16_t side = underSide_, offX = underCrop_ - _output_x, offY = underCrop_ - _output_y;
    forBands([&](int16_t x, int16_t y, int16_t w, int16_t h) {
      int16_t rows = (int16_t)((SCRATCH_PX / w) & ~1);  // even chunks
      if (rows < 2) return;
      for (int16_t y0 = y; y0 < y + h; y0 += rows) {
        int16_t n = (y + h - y0) < rows ? (y + h - y0) : rows;
        for (int16_t j = 0; j < n; j++) {
          int my = clampTo(y0 + j + offY, side);
          const uint16_t *src = full + (uint32_t)my * underStride_;
          uint16_t *dst = scratch_ + (uint32_t)j * w;
          for (int16_t i = 0; i < w; i++) dst[i] = src[clampTo(x + i + offX, side)];
        }
        panel_->draw16bitRGBBitmap(x, y0, scratch_, w, n);
      }
    });
  }
  static int clampTo(int v, int side) { return v < 0 ? 0 : (v >= side ? side - 1 : v); }

  // what the border on the panel holds: a map key, black, or stale (to be repainted)
  static constexpr uint32_t BORDER_BLACK = 0xFFFFFFFEu, BORDER_STALE = 0xFFFFFFFFu;
  // the widest band chunk: the full panel width by 12 rows (the bottom band at
  // the orbit's top position); narrower or taller bands go in chunks
  static constexpr uint32_t SCRATCH_PX = (uint32_t)BOARD_LCD_WIDTH * 12;

  RoundMask mask_;
  Arduino_GFX *panel_;
  int16_t wantX_ = UI_ORIGIN, wantY_ = UI_ORIGIN;
  uint32_t borderKey_ = BORDER_BLACK;  // roundDisplayBegin clears the panel
  uint16_t *scratch_ = nullptr;
#if TP_FULL_YARD
  const uint16_t *under_ = nullptr;
  int16_t underSide_ = 0, underCrop_ = 0, underStride_ = 0;
  uint32_t underKey_ = 0;
#else
  static constexpr int16_t underSide_ = 0, underCrop_ = 0, underStride_ = 0;
  static constexpr uint32_t underKey_ = 0;
#endif
};

// Panel set-up that has to follow gfx->begin(): orientation as in Waveshare's
// demo, then the whole panel cleared to black once. The border around the
// canvas is never drawn again, and the panel keeps it in its own RAM.
inline void roundDisplayBegin(Arduino_DataBus *bus, Arduino_GFX *panel, RoundCanvas *canvas) {
  bus->writeC8D8(BOARD_LCD_MADCTL_REG, BOARD_LCD_MADCTL_VALUE);
  panel->fillScreen(RGB565_BLACK);
  if (!canvas->beginFrame()) Serial.println("round frame: out of memory, showing the square canvas");
}
