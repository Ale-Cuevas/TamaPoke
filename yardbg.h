#pragma once
// Fondos del patio (v2.0, fase Y1b): lector de ficheros TPY1 (logica pura).
//
// tools/pack_yard.py compone cada mapa (15x15 baldosas de 16 px = 240x240) y lo
// guarda indexado: paleta RGB565 + mascara de paso + un byte por pixel. Aqui se
// valida el fichero ya leido en memoria y se expande al lienzo de 466x466: cada
// pixel de la fuente es un bloque de 2x2 (480x480) y se recorta 7 px por cada lado.
//
//   "TPY1" | u8 version=1 | u8 w=240 | u8 h=240 | u8 palCount (0 = 256)
//   | u16 pal[palCount] RGB565 little-endian | u8 walk[29] | u8 px[w*h]
//
// La mascara: 15x15 bits por filas, bit k en el byte k/8, bit k%8 (LSB primero);
// 1 = no se pisa (un objeto, agua, fuera del circulo o bajo el HUD).
//
// El dia y la noche se tintan en la PALETA (256 colores como mucho), no en el
// lienzo: tintar es gratis y luego basta con volver a expandir.
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define YBG_SRC 240         // lado de la imagen fuente
#define YBG_GRID 15         // celdas por lado
#define YBG_TILE 16         // px de la fuente por celda
#define YBG_WALK_BYTES 29   // (15*15 + 7) / 8
#define YBG_CANVAS 466      // lado del lienzo
#define YBG_CROP 7          // (480 - 466) / 2
#define YBG_CELL 32         // px del lienzo por celda (16 a escala 2)
#define YBG_HEADER 8
#define YBG_FILE_MAX (YBG_HEADER + 2 * 256 + YBG_WALK_BYTES + YBG_SRC * YBG_SRC)

enum YbgErr : uint8_t { YBG_OK = 0, YBG_SHORT, YBG_MAGIC, YBG_VERSION, YBG_SIZE, YBG_LENGTH, YBG_INDEX };

struct YardBgView {
  uint16_t pal[256];
  uint16_t palCount;
  uint8_t walk[YBG_WALK_BYTES];
  const uint8_t *px;  // YBG_SRC * YBG_SRC indices; apunta DENTRO del buffer leido (no lo copia)
};

// Valida el fichero completo (cabecera, longitud exacta e indices dentro de la paleta)
// y rellena v. Si no es YBG_OK, v no se debe usar.
inline YbgErr yardBgParse(const uint8_t *d, size_t n, YardBgView *v) {
  if (!d || n < YBG_HEADER) return YBG_SHORT;
  if (memcmp(d, "TPY1", 4) != 0) return YBG_MAGIC;
  if (d[4] != 1) return YBG_VERSION;
  if (d[5] != YBG_SRC || d[6] != YBG_SRC) return YBG_SIZE;
  uint16_t pc = d[7] ? d[7] : 256;
  size_t need = YBG_HEADER + 2u * pc + YBG_WALK_BYTES + (size_t)YBG_SRC * YBG_SRC;
  if (n != need) return n < need ? YBG_SHORT : YBG_LENGTH;
  const uint8_t *p = d + YBG_HEADER;
  for (uint16_t i = 0; i < pc; i++) v->pal[i] = (uint16_t)(p[2 * i] | (p[2 * i + 1] << 8));
  for (uint16_t i = pc; i < 256; i++) v->pal[i] = 0;
  p += 2u * pc;
  memcpy(v->walk, p, YBG_WALK_BYTES);
  p += YBG_WALK_BYTES;
  if (pc < 256)
    for (size_t i = 0; i < (size_t)YBG_SRC * YBG_SRC; i++)
      if (p[i] >= pc) return YBG_INDEX;
  v->palCount = pc;
  v->px = p;
  return YBG_OK;
}

// Expande al lienzo (YBG_CANVAS x YBG_CANVAS, RGB565 nativo) con la paleta dada
// (la del fichero o una tintada). El pixel (x, y) del lienzo es el (x+7)/2, (y+7)/2 de la fuente.
inline void yardBgExpand(const uint8_t *px, const uint16_t *pal, uint16_t *dst) {
  for (int y = 0; y < YBG_CANVAS; y++) {
    const uint8_t *row = px + ((y + YBG_CROP) >> 1) * YBG_SRC;
    uint16_t *out = dst + y * YBG_CANVAS;
    // x = 0 cae en la segunda mitad del pixel 3 de la fuente (7 / 2); de ahi en adelante, de 2 en 2
    int x = 0, sx = YBG_CROP >> 1;
    out[x++] = pal[row[sx++]];
    while (x + 1 < YBG_CANVAS) {
      uint16_t c = pal[row[sx++]];
      out[x++] = c;
      out[x++] = c;
    }
    if (x < YBG_CANVAS) out[x] = pal[row[sx]];
  }
}

// ---------- mascara de paso ----------
// celda bloqueada; fuera de la rejilla cuenta como bloqueada
inline bool yardCellBlocked(const uint8_t *walk, int cx, int cy) {
  if (cx < 0 || cy < 0 || cx >= YBG_GRID || cy >= YBG_GRID) return true;
  int k = cy * YBG_GRID + cx;
  return (walk[k >> 3] >> (k & 7)) & 1;
}
// celda bajo un punto del lienzo
inline int yardCellOf(int canvas) { return (canvas + YBG_CROP) / YBG_CELL; }
// centro de una celda en el lienzo
inline int yardCellCenter(int cell) { return cell * YBG_CELL + YBG_CELL / 2 - YBG_CROP; }
inline bool yardPointBlocked(const uint8_t *walk, int x, int y) {
  if (x < 0 || y < 0) return true;
  return yardCellBlocked(walk, yardCellOf(x), yardCellOf(y));
}

// ---------- tinte del dia ----------
enum YardTint : uint8_t { YTINT_DAY = 0, YTINT_DAWN, YTINT_DUSK, YTINT_NIGHT };

// Mezcla un color RGB565 hacia (tr, tg, tb) de 8 bits con fuerza k/256 y lo
// oscurece a dim/256. Noche: hacia azul y mas oscuro; atardecer: calido.
inline uint16_t yardTint565(uint16_t c, YardTint t) {
  if (t == YTINT_DAY) return c;
  int r = ((c >> 11) & 31) << 3, g = ((c >> 5) & 63) << 2, b = (c & 31) << 3;
  int tr, tg, tb, k, dim;
  switch (t) {
    case YTINT_DAWN:  tr = 255; tg = 200; tb = 170; k = 50;  dim = 236; break;
    case YTINT_DUSK:  tr = 255; tg = 140; tb = 80;  k = 70;  dim = 220; break;
    default:          tr = 40;  tg = 60;  tb = 140; k = 120; dim = 130; break;  // noche
  }
  r = (r * (256 - k) + tr * k) >> 8;
  g = (g * (256 - k) + tg * k) >> 8;
  b = (b * (256 - k) + tb * k) >> 8;
  r = r * dim >> 8;
  g = g * dim >> 8;
  b = b * dim >> 8;
  return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

inline void yardTintPalette(const uint16_t *src, uint16_t *dst, uint16_t n, YardTint t) {
  for (uint16_t i = 0; i < n; i++) dst[i] = yardTint565(src[i], t);
}

// tinte segun la hora (igual que el cielo de la escena de lado: 6-8 amanecer, 18-20 atardecer)
inline YardTint yardTintAt(uint8_t hour, bool sleeping) {
  if (sleeping || hour < 6 || hour >= 20) return YTINT_NIGHT;
  if (hour < 8) return YTINT_DAWN;
  if (hour >= 18) return YTINT_DUSK;
  return YTINT_DAY;
}
