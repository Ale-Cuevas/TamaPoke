#pragma once
// Huevos con el color del tipo (v2.4): el huevo dice de que tipo es el bicho que lleva dentro.
// Un tipo: todo el huevo de su color. Dos tipos: el primero arriba a la izquierda, el segundo abajo a la
// derecha, y se funden en una banda en diagonal ( / ) por el centro. Las motas son una version palida
// del color que tienen debajo. dex 0 = el huevo clasico (crema con motas verdes, el de SPR_EGG).
// Logica pura (sin Arduino): el .ino la usa para pintar SPR_EGG celda a celda y los tests la comprueban.
#include <stdint.h>
#include "dex.h"

static constexpr uint16_t eggRgb(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// cuerpo ('y') y sombra ('Y') de cada tipo, en el orden de TY_* (dex.h)
static const uint16_t TYPE_EGG_BASE[TY_COUNT] = {
  eggRgb(0xf8, 0xd9, 0x90),  // normal: el crema de siempre (spriteColor 'y')
  eggRgb(0xf5, 0x86, 0x3d),  // fuego
  eggRgb(0x5a, 0xa0, 0xe8),  // agua
  eggRgb(0xff, 0xd8, 0x4a),  // electrico
  eggRgb(0x6c, 0xc0, 0x58),  // planta
  eggRgb(0xa8, 0xe4, 0xec),  // hielo
  eggRgb(0xd0, 0x5a, 0x40),  // lucha
  eggRgb(0xc0, 0x5c, 0xc8),  // veneno
  eggRgb(0xc8, 0x94, 0x5a),  // tierra
  eggRgb(0xa4, 0xd0, 0xf4),  // volador: azul cielo
  eggRgb(0xf0, 0x8a, 0xa4),  // psiquico
  eggRgb(0xa8, 0xc0, 0x30),  // bicho
  eggRgb(0xb0, 0xa0, 0x78),  // roca
  eggRgb(0x5c, 0x54, 0x8c),  // fantasma
  eggRgb(0x6a, 0x5a, 0xe0),  // dragon
};
static const uint16_t TYPE_EGG_SHADE[TY_COUNT] = {
  eggRgb(0xe0, 0xb8, 0x60),  // normal (spriteColor 'Y')
  eggRgb(0xd6, 0x5f, 0x28),
  eggRgb(0x3a, 0x74, 0xc0),
  eggRgb(0xd8, 0xa8, 0x20),
  eggRgb(0x46, 0x92, 0x3c),
  eggRgb(0x70, 0xb8, 0xcc),
  eggRgb(0x9a, 0x38, 0x26),
  eggRgb(0x86, 0x36, 0x90),
  eggRgb(0x94, 0x6a, 0x3a),
  eggRgb(0x6e, 0x9e, 0xd8),
  eggRgb(0xc7, 0x5f, 0x80),
  eggRgb(0x7a, 0x90, 0x20),
  eggRgb(0x7e, 0x70, 0x50),
  eggRgb(0x3a, 0x32, 0x60),
  eggRgb(0x44, 0x36, 0xa8),
};

// la diagonal ( / ) son las celdas con la misma col + fila; EGG_DIAG_MID pasa por el centro del huevo
// de SPR_EGG (columnas 8-24, filas 6-28) y la mezcla ocupa EGG_BLEND_HALF celdas a cada lado
#define EGG_DIAG_MID 33
#define EGG_BLEND_HALF 6

// a + (b - a) * num / den, canal a canal (como lerp565 del .ino)
static inline uint16_t eggMix(uint16_t a, uint16_t b, int num, int den) {
  int ar = a >> 11, ag = (a >> 5) & 63, ab = a & 31;
  int br = b >> 11, bg = (b >> 5) & 63, bb = b & 31;
  int r = ar + (br - ar) * num / den, g = ag + (bg - ag) * num / den, bl = ab + (bb - ab) * num / den;
  return (uint16_t)((r << 11) | (g << 5) | bl);
}

// los tipos que pintan el huevo de dex; false = huevo clasico (sin especie o fuera de la tabla)
static inline bool eggTypesOf(int16_t dex, uint8_t &t1, uint8_t &t2) {
  t1 = TY_NORMAL;
  t2 = TY_NONE;
  if (dex <= 0 || dex > DEX_COUNT) return false;
  t1 = DEX_TBL[dex].type1;
  t2 = DEX_TBL[dex].type2;
  if (t1 >= TY_COUNT) return false;
  if (t2 >= TY_COUNT || t2 == t1) t2 = TY_NONE;
  return true;
}

// color de la celda (col, fila) de SPR_EGG con el caracter ch. false = no es del cuerpo ('y' 'Y' 'g'):
// se pinta con spriteColor como siempre (el contorno, el brillo)
static inline bool eggCellColor(char ch, uint8_t t1, uint8_t t2, int col, int row, uint16_t &out) {
  if (ch != 'y' && ch != 'Y' && ch != 'g') return false;
  if (t1 >= TY_COUNT) t1 = TY_NORMAL;
  bool shade = ch == 'Y';
  uint16_t c = shade ? TYPE_EGG_SHADE[t1] : TYPE_EGG_BASE[t1];
  if (t2 < TY_COUNT && t2 != t1) {
    int k = col + row - EGG_DIAG_MID + EGG_BLEND_HALF;  // 0 = empieza la mezcla, 2 * HALF = acaba
    if (k < 0) k = 0;
    if (k > 2 * EGG_BLEND_HALF) k = 2 * EGG_BLEND_HALF;
    c = eggMix(c, shade ? TYPE_EGG_SHADE[t2] : TYPE_EGG_BASE[t2], k, 2 * EGG_BLEND_HALF);
  }
  if (ch == 'g') c = eggMix(c, 0xFFFF, 1, 2);  // las motas: el color de debajo, palido
  out = c;
  return true;
}
