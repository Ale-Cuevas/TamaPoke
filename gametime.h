#pragma once
#include <math.h>
#include <stdint.h>
#include "tuning.h"  // GAME_PHYS_REF_MS

// Fisica por tiempo real (v3.23). Hasta la v3.22 las cosas "por frame" de los juegos (el saco, el bicho de
// la pelota) estaban afinadas a frames de 85 ms. Con el ritmo de GAME_FRAME_MS el mismo paso por frame iria mas
// rapido, asi que se escala: un paso de referencia son GAME_PHYS_REF_MS ms. Logica pura, tests en
// test/test_gametime.cpp. La pelota, el trineo y la cascada ya iban por tiempo (ballStepK, dashAdvance,
// zenAdvance) y el combate va por millis(): no pasan por aqui.

// pasos de referencia que caben en dtMs, con un tope (un frame tardio o un cambio de pantalla no teletransporta)
inline float gameDtScale(uint32_t dtMs, float maxSteps = 2.0f) {
  float r = (float)dtMs / (float)GAME_PHYS_REF_MS;
  return r > maxSteps ? maxSteps : r;
}

// factor por frame (p. ej. 0,84 por paso de referencia) pasado a dtMs de reloj: factor^(dt/ref)
inline float gameDecay(float perStep, uint32_t dtMs, float maxSteps = 2.0f) {
  return powf(perStep, gameDtScale(dtMs, maxSteps));
}
