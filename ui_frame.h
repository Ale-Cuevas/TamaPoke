#pragma once
#include <stdint.h>
#include "board_amoled_216.h"

// Where TamaPoke's round UI sits on the square AMOLED-2.16 panel. Pure
// geometry, shared by the display code (round_display.h) and the PC tests
// (test/test_roundmask.cpp).
//
// The game draws into a 466x466 canvas, the size of the round 1.75" panel it
// was designed for: every coordinate in TamaPoke.ino lives in that space, with
// the centre at (233, 233). The canvas is blitted onto the 480x480 panel near
// its centre, and a round mask blacks out what the round glass used to hide,
// so the device shows the same round screen framed by black.
//
//   panel 480 x 480
//   +-----------------------------------------+
//   | UI_ORIGIN                               |
//   |   +---------------------------------+   |
//   |   |       .-~~~~~~~~~~~~~~~~~-.     |   |
//   |   |     /   466 x 466 canvas   \    |   |
//   |   |    |    round frame inside  |   |   |
//   |   |     \                      /    |   |
//   |   |       `-~~~~~~~~~~~~~~~~~-'     |   |
//   |   +---------------------------------+   |
//   +-----------------------------------------+

#define UI_SIZE 466  // side of the game canvas (the 1.75" round panel)

// Where the canvas starts on the panel, in both axes. The centred value would
// be 7, but the CO5300 only accepts address windows that start on an even
// column/row (and span an even number of pixels), so the canvas sits at 6:
// one pixel left of and above centre on a 2.16" screen, which is invisible.
#define UI_ORIGIN 6
static_assert(UI_ORIGIN % 2 == 0, "CO5300 address windows must start on an even pixel");
static_assert(UI_SIZE % 2 == 0, "CO5300 address windows must span an even number of pixels");
static_assert(UI_ORIGIN + UI_SIZE <= BOARD_LCD_WIDTH && UI_ORIGIN + UI_SIZE <= BOARD_LCD_HEIGHT,
              "the game canvas must fit on the panel");

// The round frame, in canvas coordinates where pixel x spans [x, x + 1).
// Centred on the UI's own centre: the background discs are drawn with
// fillCircle(233, 233, 231), which covers pixels 2..464, i.e. [2, 465],
// centre 233.5. The radius is as large as fits in the canvas, [1, 466], so a
// one-pixel black ring separates those discs from the frame's soft edge.
#define UI_FRAME_CENTER 233.5f
#define UI_FRAME_RADIUS 232.5f

// 1 = round frame (the original look, black corners); 0 = the whole square
// canvas, corners included. Override with -D TP_ROUND_FRAME=0.
#ifndef TP_ROUND_FRAME
#define TP_ROUND_FRAME 1
#endif

// Touch arrives in panel pixels; the game works in canvas pixels. Points in
// the black border come out negative or past 465 and miss every button, but
// keep their distances, so a swipe that starts there still counts.
inline int16_t panelToUi(int16_t v) { return (int16_t)(v - UI_ORIGIN); }
