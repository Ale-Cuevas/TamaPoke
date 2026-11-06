#pragma once
#include <stdint.h>
#include "wind.h"  // WIND_SIN: la tabla de seno

// Proteccion del AMOLED contra el quemado (v1.28). Logica pura, con tests en
// test/test_burnin.cpp.
//
// 1. Orbita: cada ORBIT_STEP_MS toda la imagen se mueve ORBIT_STEP_PX por una rejilla de
//    3x3 posiciones alrededor de la de siempre, asi ningun borde fijo (la cabecera, las
//    barras, los botones, el horizonte) cae siempre en los mismos pixeles. Los pasos son
//    pares porque el CO5300 solo acepta ventanas que empiezan en pixel par (ui_frame.h).
// 2. Salvapantallas: a los 5 min sin tocar (DIM_OFF, power.h) en vez de la escena entera a
//    brillo 8 se pinta un fondo negro con el bicho pequeno y la hora, que van recorriendo
//    la pantalla despacio (saverPos).

#define ORBIT_STEP_PX 4
#define ORBIT_POSITIONS 9
#define ORBIT_MAX_PX ORBIT_STEP_PX
#define ORBIT_STEP_MS (3UL * 60 * 1000)  // una posicion nueva cada 3 min
static_assert(ORBIT_STEP_PX % 2 == 0, "el CO5300 solo acepta ventanas que empiezan en pixel par");

struct OrbitShift {
  int8_t dx, dy;
};

// posicion `step` de la orbita: el centro y luego las 8 de alrededor, en el sentido de las
// agujas del reloj (cada paso se mueve ORBIT_STEP_PX en un eje, salvo la vuelta al centro)
inline OrbitShift orbitShift(uint32_t step) {
  static const int8_t P[ORBIT_POSITIONS][2] = {
    { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 }, { -1, 0 }, { -1, -1 }, { 0, -1 }, { 1, -1 },
  };
  const int8_t *p = P[step % ORBIT_POSITIONS];
  return { (int8_t)(p[0] * ORBIT_STEP_PX), (int8_t)(p[1] * ORBIT_STEP_PX) };
}

inline uint32_t orbitStepAt(uint32_t ms) { return ms / ORBIT_STEP_MS; }

// --- salvapantallas
#define SAVER_CX 233
#define SAVER_CY 233
#define SAVER_RX 112          // recorrido horizontal a cada lado del centro
#define SAVER_RY 96           // y vertical
#define SAVER_PERIOD_X_MS 251000UL  // periodos distintos y sin divisor comun: una figura de
#define SAVER_PERIOD_Y_MS 337000UL  // Lissajous que tarda horas en repetirse
#define SAVER_BLOCK_R 64      // radio de lo que se dibuja alrededor de la posicion (bicho + hora)

// seno * 127 con 1024 pasos por vuelta, interpolando la tabla de 64
inline int saverSin(uint32_t phase1024) {
  uint32_t i = (phase1024 >> 4) & 63, f = phase1024 & 15;
  int a = WIND_SIN[i], b = WIND_SIN[(i + 1) & 63];
  return a + (b - a) * (int)f / 16;
}

// centro del bloque del salvapantallas en el instante ms
inline void saverPos(uint32_t ms, int *x, int *y) {
  uint32_t px = (uint32_t)((uint64_t)(ms % SAVER_PERIOD_X_MS) * 1024 / SAVER_PERIOD_X_MS);
  uint32_t py = (uint32_t)((uint64_t)(ms % SAVER_PERIOD_Y_MS) * 1024 / SAVER_PERIOD_Y_MS);
  *x = SAVER_CX + saverSin(px) * SAVER_RX / 127;
  *y = SAVER_CY + saverSin(py) * SAVER_RY / 127;
}
