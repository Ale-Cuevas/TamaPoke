#pragma once
// El huevo del patio (v2.0, fase Y3): eclosiona solo a las 4 h de tiempo real desde que se pone en el
// patio, y cada sacudida de la placa (motion.h; una cada 30 s como mucho) le quita 5 min, hasta 2 h en
// total: lo mas rapido son 2 h. El tiempo es el del RTC (epoch en segundos, como lastSeenEpoch), asi
// que corre tambien con la placa apagada. Logica pura (test/test_eggtimer.cpp).
//
// placed: epoch en que se puso. 0 = no es un huevo del patio (el inicial y los del sistema de toques
// de antes: se abren con 3 toques). Por debajo de EGG_EPOCH_MIN (sin reloj al ponerlo) cuenta como
// puesto pero aun sin hora: no avanza hasta que se sepa la hora (eggStamp).
#include <stdint.h>
#include <stdio.h>

#define EGG_HATCH_MIN 240           // 4 h de tiempo real
#define EGG_SHAKE_MIN 5             // lo que quita una sacudida
#define EGG_SHAKE_CAP 120           // como mucho 2 h en total
#define EGG_SHAKE_COOLDOWN_MS 30000 // entre dos sacudidas que cuentan
#define EGG_EPOCH_MIN 1000000000UL  // 2001: menos que esto no es una hora de verdad
#define EGG_NO_CLOCK 1UL            // puesto sin hora (se fecha al saberla)

inline bool eggClockKnown(uint32_t placed) { return placed >= EGG_EPOCH_MIN; }

// minutos de tiempo real desde que se puso (0 sin hora, o si el reloj fue hacia atras)
inline uint32_t eggElapsedMin(uint32_t placed, uint32_t nowEpoch) {
  if (!eggClockKnown(placed) || nowEpoch <= placed) return 0;
  return (nowEpoch - placed) / 60;
}

// minutos que faltan para que eclosione (0 = ya)
inline uint16_t eggMinutesLeft(uint32_t placed, uint32_t nowEpoch, uint8_t saved) {
  uint32_t done = eggElapsedMin(placed, nowEpoch) + (saved > EGG_SHAKE_CAP ? EGG_SHAKE_CAP : saved);
  return done >= EGG_HATCH_MIN ? 0 : (uint16_t)(EGG_HATCH_MIN - done);
}

// una sacudida: true si cuenta (pasaron EGG_SHAKE_COOLDOWN_MS desde la ultima que conto y aun queda
// tope); *saved sube EGG_SHAKE_MIN sin pasar del tope, y *lastMs se apunta. haveLast = false: ninguna
// conto aun desde el arranque
inline bool eggShake(uint8_t *saved, uint32_t *lastMs, bool *haveLast, uint32_t nowMs) {
  if (*saved >= EGG_SHAKE_CAP) return false;
  if (*haveLast && nowMs - *lastMs < EGG_SHAKE_COOLDOWN_MS) return false;
  uint16_t s = (uint16_t)*saved + EGG_SHAKE_MIN;
  *saved = (uint8_t)(s > EGG_SHAKE_CAP ? EGG_SHAKE_CAP : s);
  *lastMs = nowMs;
  *haveLast = true;
  return true;
}

// la hora de ponerlo para que ya hayan pasado `elapsed` minutos a nowEpoch (serie EGGT)
inline uint32_t eggPlacedFor(uint32_t nowEpoch, uint32_t elapsedMin) {
  uint32_t back = elapsedMin * 60;
  return nowEpoch > back + EGG_EPOCH_MIN ? nowEpoch - back : EGG_EPOCH_MIN;
}

// grietas que se ven (0-2) segun lo que falta: una a mitad, dos en la ultima media hora
inline uint8_t eggCracksFor(uint16_t left) { return left <= 30 ? 2 : (left <= EGG_HATCH_MIN / 2 ? 1 : 0); }

// "3h 05m", "45m" (el globo del huevo; sin palabras, igual en todos los idiomas)
inline void eggLeftText(char *out, size_t n, uint16_t left) {
  if (left >= 60) snprintf(out, n, "%uh %02um", (unsigned)(left / 60), (unsigned)(left % 60));
  else snprintf(out, n, "%um", (unsigned)(left ? left : 1));
}
