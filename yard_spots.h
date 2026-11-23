#pragma once
// Los muebles del patio (v3.20, docs/HOME_AND_PALS.md FR-HOME.5): donde va cada mejora de casa en cada mapa
// (logica pura, test/test_yard_spots.cpp). Una tabla en celdas de la rejilla de 15x15 (yardbg.h) en vez de un
// cambio del formato TPY1, asi los .bin de la SD no se regeneran.
//
// Por mapa: la pareja de camas (2 celdas una al lado de la otra: la izquierda es la del hueco 0, la derecha la
// del 1), el gimnasio y el juguete. El indice 0 es el suelo liso de respaldo (yardFallbackWalk). Las celdas se
// eligieron sobre las imagenes de tools/pack_yard.py --spots (tools/yard/out/NN-name_spots.png): fuera del
// reloj, del dock y del borde del circulo, y sin partir la zona por la que se pasea (el test lo comprueba con
// los mapas de verdad y cada combinacion de muebles).
//
// Los muebles que se tienen bloquean sus celdas (yardBlockObjects): nadie pasea por encima y nada cae ahi
// (cacas, regalos, el huevo, el encuentro). Un bicho dormido se tumba en su cama: el juego le deja su celda.
#include <stdint.h>
#include "tuning.h"
#include "yardbg.h"

struct YardSpots {
  uint8_t bedX, bedY;  // la cama del hueco 0; la del hueco 1 es (bedX + 1, bedY)
  uint8_t gymX, gymY;
  uint8_t toyX, toyY;
};

static const YardSpots YARD_SPOTS[YARD_MAPS + 1] = {
  { 9, 6, 4, 8, 8, 10 },   // 0: suelo liso (sin mapa)
  { 10, 8, 5, 5, 8, 10 },  // 1: pradera (las camas en la arena)
  { 11, 7, 5, 9, 9, 9 },   // 2: cantera
  { 3, 7, 11, 7, 8, 5 },   // 3: bosque
  { 9, 5, 4, 8, 10, 9 },   // 4: playa
  { 4, 6, 10, 7, 8, 10 },  // 5: ruinas
  { 7, 4, 3, 7, 9, 9 },    // 6: volcan
  { 4, 7, 11, 7, 8, 10 },  // 7: central electrica
  { 4, 9, 9, 7, 5, 7 },    // 8: torre del cielo
  { 3, 8, 8, 5, 7, 9 },    // 9: distrito toxico
};

// px que cada cama se aparta de su celda hacia fuera de la pareja: dos camas de 64 px en celdas de 38 px
// quedan con 2 px entre ellas (y los dos dormidos, a 66 px: mas que YARD_MIN_DIST)
#define YARD_BED_DX 14

inline const YardSpots &yardSpotsOf(uint8_t map) { return YARD_SPOTS[map <= YARD_MAPS ? map : 0]; }

// las celdas de un mueble (k = 0 cama del hueco 0, 1 cama del hueco 1, 2 gimnasio, 3 juguete)
enum : uint8_t { YOBJ_BED0 = 0, YOBJ_BED1, YOBJ_GYM, YOBJ_TOY, YOBJ_N };
inline void yardObjCell(uint8_t map, uint8_t k, int *cx, int *cy) {
  const YardSpots &s = yardSpotsOf(map);
  switch (k) {
    case YOBJ_BED0: *cx = s.bedX; *cy = s.bedY; break;
    case YOBJ_BED1: *cx = s.bedX + 1; *cy = s.bedY; break;
    case YOBJ_GYM: *cx = s.gymX; *cy = s.gymY; break;
    default: *cx = s.toyX; *cy = s.toyY; break;
  }
}

// los pies del mueble k en el lienzo (el centro de su celda; las camas, apartadas YARD_BED_DX hacia fuera)
inline void yardObjFoot(uint8_t map, uint8_t k, int *x, int *y) {
  int cx, cy;
  yardObjCell(map, k, &cx, &cy);
  *x = yardCellCenter(cx) + (k == YOBJ_BED0 ? -YARD_BED_DX : k == YOBJ_BED1 ? YARD_BED_DX : 0);
  *y = yardCellCenter(cy);
}

// bloquea en la mascara las celdas de los muebles que se tienen
inline void yardBlockObjects(uint8_t *walk, uint8_t map, bool bed, bool gym, bool toy) {
  for (uint8_t k = 0; k < YOBJ_N; k++) {
    bool on = k <= YOBJ_BED1 ? bed : k == YOBJ_GYM ? gym : toy;
    if (!on) continue;
    int cx, cy;
    yardObjCell(map, k, &cx, &cy);
    int c = cy * YBG_GRID + cx;
    walk[c >> 3] |= (uint8_t)(1 << (c & 7));
  }
}

// v3.20 (revision): los sprites de los muebles miden 64x64 (32 a 2x) y las celdas 38 px, asi que una caca o un
// regalo en la celda de encima (o a los lados de la pareja de camas) se vería medio tapado. Para las COSAS (no
// para el paseo de los bichos) se bloquean tambien: la celda de encima del gimnasio y del juguete, las dos de
// encima de las camas y los vecinos de izquierda y derecha de la pareja (solo los que caen dentro de la rejilla)
#define YARD_THINGS_MIN_FREE 12  // si con eso quedan menos celdas libres, las cosas usan la mascara de los muebles sola
inline void yardBlockCell(uint8_t *walk, int cx, int cy) {
  if (cx < 0 || cy < 0 || cx >= YBG_GRID || cy >= YBG_GRID) return;
  int c = cy * YBG_GRID + cx;
  walk[c >> 3] |= (uint8_t)(1 << (c & 7));
}
inline void yardBlockAboveObjects(uint8_t *walk, uint8_t map, bool bed, bool gym, bool toy) {
  const YardSpots &s = yardSpotsOf(map);
  if (bed) {
    yardBlockCell(walk, s.bedX, s.bedY - 1);
    yardBlockCell(walk, s.bedX + 1, s.bedY - 1);
    yardBlockCell(walk, s.bedX - 1, s.bedY);
    yardBlockCell(walk, s.bedX + 2, s.bedY);
  }
  if (gym) yardBlockCell(walk, s.gymX, s.gymY - 1);
  if (toy) yardBlockCell(walk, s.toyX, s.toyY - 1);
}
