#pragma once
// TamaPoke uses U8g2 only for the data of two CJK fonts (Arduino_GFX draws
// them). The Makefile extracts those two arrays from U8g2's u8g2_fonts.c.
#include <cstdint>
// the real u8g2.h defines this; without it Arduino_GFX skips every glyph above 0xFF
#define U8G2_WITH_UNICODE
extern "C" const uint8_t u8g2_font_unifont_t_japanese3[];
extern "C" const uint8_t u8g2_font_unifont_t_korean2[];
