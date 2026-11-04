#pragma once
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Round frame for a square RGB565 framebuffer: everything outside a centred
// disc is painted black, and the pixels the circle's edge cuts through are
// darkened by how much of them lies inside it (a 1 px anti-aliased edge).
//
// TamaPoke was drawn for a round 466x466 AMOLED. On the square 480x480 panel
// of the AMOLED-2.16 the corners that the round glass used to hide would be
// visible; masking them keeps the original round look, and on an AMOLED the
// black pixels are simply off (no light, no burn-in, no power).
//
// No hardware here: test/test_roundmask.cpp checks it on the PC.

// Darkens an RGB565 colour by a coverage of a/255 (0 = black, 255 = unchanged).
inline uint16_t scale565(uint16_t c, uint8_t a) {
  uint32_t r = ((c >> 11) & 0x1F) * a / 255;
  uint32_t g = ((c >> 5) & 0x3F) * a / 255;
  uint32_t b = (c & 0x1F) * a / 255;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

// Fraction (0..1) of pixel (x, y) inside the disc of radius r centred at
// (cx, cy), in continuous coordinates where pixel x spans [x, x + 1): one
// pixel of linear ramp across the edge, measured from the pixel centre.
inline float discCoverage(int x, int y, float cx, float cy, float r) {
  float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
  float c = r - sqrtf(dx * dx + dy * dy) + 0.5f;
  return c <= 0.0f ? 0.0f : (c >= 1.0f ? 1.0f : c);
}

class RoundMask {
 public:
  ~RoundMask() { release(); }

  // Prepares the mask for an n x n frame and a disc of radius r centred at
  // (cx, cy). Returns false if out of memory (apply() then does nothing).
  bool build(int n, float cx, float cy, float r) {
    release();
    n_ = n;
    rowL_ = (int16_t *)malloc(sizeof(int16_t) * n);
    rowR_ = (int16_t *)malloc(sizeof(int16_t) * n);
    if (!rowL_ || !rowR_) { release(); return false; }
    // Pass 1: the covered span of each row, and how many pixels it only
    // partly covers. Coverage along a row rises then falls, so the covered
    // pixels are always one contiguous run.
    uint32_t edges = 0;
    for (int y = 0; y < n; y++) {
      int l = n, rgt = n;
      for (int x = 0; x < n; x++) {
        float c = discCoverage(x, y, cx, cy, r);
        if (c <= 0.0f) continue;
        if (l == n) l = x;
        rgt = x + 1;
        if (alphaOf(c) < 255) edges++;
      }
      rowL_[y] = (int16_t)l;
      rowR_[y] = (int16_t)rgt;
    }
    // Pass 2: the partly covered pixels, as (index << 8 | alpha).
    edge_ = (uint32_t *)malloc(sizeof(uint32_t) * (edges ? edges : 1));
    if (!edge_) { release(); return false; }
    edgeCount_ = 0;
    for (int y = 0; y < n; y++)
      for (int x = rowL_[y]; x < rowR_[y]; x++) {
        uint8_t a = alphaOf(discCoverage(x, y, cx, cy, r));
        if (a < 255) edge_[edgeCount_++] = ((uint32_t)(y * n + x) << 8) | a;
      }
    return true;
  }

  // Blacks out the corners of an n x n frame and softens the circle's edge.
  void apply(uint16_t *fb) const {
    if (!fb || !edge_) return;
    for (int y = 0; y < n_; y++) {
      uint16_t *row = fb + (uint32_t)y * n_;
      if (rowL_[y] > 0) memset(row, 0, sizeof(uint16_t) * rowL_[y]);
      if (rowR_[y] < n_) memset(row + rowR_[y], 0, sizeof(uint16_t) * (n_ - rowR_[y]));
    }
    for (uint32_t i = 0; i < edgeCount_; i++) {
      uint32_t idx = edge_[i] >> 8;
      fb[idx] = scale565(fb[idx], (uint8_t)(edge_[i] & 0xFF));
    }
  }

  bool ready() const { return edge_ != nullptr; }
  int size() const { return n_; }
  uint32_t edgeCount() const { return edgeCount_; }
  // first covered pixel of row y, and one past the last (== size() if none)
  int rowStart(int y) const { return rowL_[y]; }
  int rowEnd(int y) const { return rowR_[y]; }

 private:
  static uint8_t alphaOf(float c) { return (uint8_t)(c * 255.0f + 0.5f); }
  void release() {
    free(rowL_);
    free(rowR_);
    free(edge_);
    rowL_ = rowR_ = nullptr;
    edge_ = nullptr;
    edgeCount_ = 0;
  }

  int n_ = 0;
  int16_t *rowL_ = nullptr;
  int16_t *rowR_ = nullptr;
  uint32_t *edge_ = nullptr;
  uint32_t edgeCount_ = 0;
};
