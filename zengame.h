#pragma once
#include <math.h>
#include <stdint.h>

// Minijuego "cascada" (ZEN, docs/NEW_GAMES.md): el bicho medita bajo una cascada. Mantener el
// dedo en la pantalla sube la concentracion y soltarlo la baja; hay que tenerla dentro de una
// franja en el centro. La barra tiene inercia (FR-ZEN.4): con un ritmo fijo de "sube / baja"
// bastarian toques rapidos; asi hay que frenar a tiempo y dosificar el dedo. La franja se
// estrecha durante la sesion y la cascada da golpes de agua (avisados antes) que tiran la barra
// hacia abajo. Logica pura, con tests en test/test_zengame.cpp: el sketch solo dibuja y lee el
// dedo.

#define ZEN_SESSION_MS 45000u     // lo que dura la sesion (sin la intro): 30 s hasta la v1.46, 45 desde la
                                  // v1.47 (60 s en la v1.45)
#define ZEN_DRIFT_FROM_MS 30000u  // desde aqui la franja sube y baja despacio, y su concentracion da DEF de
                                  // bonificacion (v1.47)
#define ZEN_DRIFT_AMP 22.0f       // cuanto se aleja el centro de la franja del 50 (en % de la barra)
#define ZEN_DRIFT_PERIOD_MS 12000u  // un vaiven entero (arriba y abajo) tarda esto: maximo ~11,5 %/s
#define ZEN_RAMP_MS 60000u        // la dificultad (franja, golpes) sube al ritmo de 60 s (v1.46): en una
                                  // sesion de 30 s la franja solo llega a la mitad de su estrechamiento
#define ZEN_INTRO_MS 1500u        // "Manten la barra en el verde"
#define ZEN_STEP_MS 10u           // paso fijo de la fisica
#define ZEN_LATE_MAX_MS 250u      // un frame tardio cuenta como mucho esto
// v1.45: la barra se mueve un 15 % mas despacio (empuje, caida, tope y golpes de agua, x0,85)
#define ZEN_PUSH 187.0f           // %/s^2 hacia arriba con el dedo puesto (era 220)
#define ZEN_PULL 153.0f           // %/s^2 hacia abajo sin el dedo (era 180)
#define ZEN_DAMP 1.5f             // amortiguacion: vc *= exp(-ZEN_DAMP * dt)
#define ZEN_VMAX 51.0f            // %/s (era 60)
#define ZEN_BAND_START 31.2f      // ancho de la franja al empezar (34-66; v1.45: era 24, +30 %)
#define ZEN_BAND_END 12.0f        // ...y al acabar (44-56)
#define ZEN_SURGE_FROM_MS 5000u   // primer golpe de agua
#define ZEN_SURGE_GAP_MAX 6000u   // separacion entre golpes al principio...
#define ZEN_SURGE_GAP_MIN 3000u   // ...y a los 60 s de rampa (+-25 % al azar)
#define ZEN_SURGE_KICK_START 21.25f  // %/s que quita un golpe al principio... (era 25)
#define ZEN_SURGE_KICK_END 34.0f     // ...y al final (era 40)
#define ZEN_SURGE_WARN_MS 400u    // la cascada se hincha antes de cada golpe
#define ZEN_TENTHS_PER_POINT 20   // DEF: 1 punto cada 2 s dentro de la franja
#define ZEN_GAIN_MAX 15           // tope de la parte normal (los primeros 30 s)
#define ZEN_BONUS_TENTHS_PER_POINT 15  // bonificacion: 1 DEF cada 1,5 s en la franja mientras se mueve...
#define ZEN_BONUS_MAX 10          // ...hasta 10 (los 15 s enteros)
#define ZEN_WASH 10               // limpieza tras una sesion completa (la cascada lava)

struct ZenState {
  float c = 50, vc = 0;        // concentracion (0-100) y su velocidad (%/s)
  uint32_t t = 0;              // ms de juego
  uint32_t inBandMs = 0;       // tiempo dentro de la franja (toda la sesion)
  uint32_t driftInBandMs = 0;  // ...de ello, con la franja moviendose (bonificacion)
  int8_t driftDir = 1;         // la franja empieza subiendo (+1) o bajando (-1)
  uint32_t nextSurge = ZEN_SURGE_FROM_MS;
  uint32_t rng = 1;
  uint32_t acc = 0;            // ms pendientes de pasos fijos
  uint16_t surges = 0;         // golpes que ya cayeron
  uint32_t lastSurge = 0;      // t del ultimo golpe (para la salpicadura)
};

inline uint32_t zenRand(uint32_t &s) {  // xorshift32
  s ^= s << 13;
  s ^= s >> 17;
  s ^= s << 5;
  return s;
}

inline float zenProgress(uint32_t t) { return t >= ZEN_RAMP_MS ? 1.0f : (float)t / ZEN_RAMP_MS; }

// media anchura de la franja (centrada en 50) a los t ms de juego: se estrecha despacio (ZEN_RAMP_MS)
inline float zenBandHalf(uint32_t t) {
  return 0.5f * (ZEN_BAND_START + (ZEN_BAND_END - ZEN_BAND_START) * zenProgress(t));
}

inline bool zenDrifting(uint32_t t) { return t >= ZEN_DRIFT_FROM_MS; }

// centro de la franja: 50 hasta ZEN_DRIFT_FROM_MS; despues sube y baja despacio (empieza quieto y
// hacia `dir`), sin salirse de la barra
inline float zenCenter(uint32_t t, int8_t dir) {
  if (!zenDrifting(t)) return 50.0f;
  float ph = (float)(t - ZEN_DRIFT_FROM_MS) / ZEN_DRIFT_PERIOD_MS * 6.2832f;
  return 50.0f + (dir < 0 ? -1.0f : 1.0f) * ZEN_DRIFT_AMP * sinf(ph);
}

inline bool zenInBand(float c, uint32_t t, int8_t dir = 1) { return fabsf(c - zenCenter(t, dir)) <= zenBandHalf(t); }

// -1 por debajo de la franja, 0 dentro, +1 por encima
inline int8_t zenZone(float c, uint32_t t, int8_t dir = 1) {
  float h = zenBandHalf(t), m = zenCenter(t, dir);
  return c < m - h ? -1 : (c > m + h ? 1 : 0);
}

inline float zenSurgeKick(uint32_t t) {
  return ZEN_SURGE_KICK_START + (ZEN_SURGE_KICK_END - ZEN_SURGE_KICK_START) * zenProgress(t);
}

// separacion hasta el golpe siguiente: de ZEN_SURGE_GAP_MAX a ZEN_SURGE_GAP_MIN, +-25 %
inline uint32_t zenSurgeGap(uint32_t t, uint32_t &rng) {
  float base = ZEN_SURGE_GAP_MAX + ((float)ZEN_SURGE_GAP_MIN - (float)ZEN_SURGE_GAP_MAX) * zenProgress(t);
  float r = (float)(zenRand(rng) % 1001) / 1000.0f;  // 0..1
  return (uint32_t)(base * (0.75f + 0.5f * r));
}

inline void zenStart(ZenState &z, uint32_t seed) {
  z = ZenState();
  z.rng = seed ? seed : 1;
  z.driftDir = (zenRand(z.rng) & 1) ? 1 : -1;
}

// la cascada se esta hinchando: el golpe llega en menos de ZEN_SURGE_WARN_MS
inline bool zenSurgeWarning(const ZenState &z) {
  return z.nextSurge < ZEN_SESSION_MS && z.nextSurge > z.t && z.nextSurge - z.t <= ZEN_SURGE_WARN_MS;
}

inline bool zenDone(const ZenState &z) { return z.t >= ZEN_SESSION_MS; }

// un paso fijo de ZEN_STEP_MS
inline void zenStep(ZenState &z, bool hold) {
  const float dt = ZEN_STEP_MS / 1000.0f;
  if (z.t >= z.nextSurge && z.nextSurge < ZEN_SESSION_MS) {  // golpe de agua
    z.vc -= zenSurgeKick(z.t);
    z.surges++;
    z.lastSurge = z.t;
    z.nextSurge += zenSurgeGap(z.t, z.rng);
  }
  z.vc += (hold ? ZEN_PUSH : -ZEN_PULL) * dt;
  z.vc *= expf(-ZEN_DAMP * dt);
  if (z.vc > ZEN_VMAX) z.vc = ZEN_VMAX;
  if (z.vc < -ZEN_VMAX) z.vc = -ZEN_VMAX;
  z.c += z.vc * dt;
  if (z.c <= 0) { z.c = 0; if (z.vc < 0) z.vc = 0; }
  if (z.c >= 100) { z.c = 100; if (z.vc > 0) z.vc = 0; }
  if (zenInBand(z.c, z.t, z.driftDir)) {
    z.inBandMs += ZEN_STEP_MS;
    if (zenDrifting(z.t)) z.driftInBandMs += ZEN_STEP_MS;
  }
  z.t += ZEN_STEP_MS;
}

// avanza dtMs de reloj con el dedo como esta ahora (pasos fijos; un frame tardio cuenta como
// mucho ZEN_LATE_MAX_MS). Asi el resultado no depende de cada cuanto se llame.
inline void zenAdvance(ZenState &z, bool hold, uint32_t dtMs) {
  if (dtMs > ZEN_LATE_MAX_MS) dtMs = ZEN_LATE_MAX_MS;
  z.acc += dtMs;
  while (z.acc >= ZEN_STEP_MS && !zenDone(z)) {
    zenStep(z, hold);
    z.acc -= ZEN_STEP_MS;
  }
}

inline uint16_t zenFocusTenths(const ZenState &z) { return (uint16_t)(z.inBandMs / 100); }
inline uint16_t zenDriftTenths(const ZenState &z) { return (uint16_t)(z.driftInBandMs / 100); }

// puntos de DEF de la parte normal: `tenths` decimas de segundo en la franja quieta
inline uint8_t zenGain(uint16_t tenths) {
  uint16_t g = tenths / ZEN_TENTHS_PER_POINT;
  return g > ZEN_GAIN_MAX ? ZEN_GAIN_MAX : (uint8_t)g;
}

// bonificacion: `driftTenths` decimas de segundo en la franja mientras se mueve
inline uint8_t zenBonus(uint16_t driftTenths) {
  uint16_t g = driftTenths / ZEN_BONUS_TENTHS_PER_POINT;
  return g > ZEN_BONUS_MAX ? ZEN_BONUS_MAX : (uint8_t)g;
}

// DEF de la sesion entera: `focusTenths` en total, de ellas `driftTenths` con la franja moviendose
inline uint8_t zenTotalGain(uint16_t focusTenths, uint16_t driftTenths) {
  if (driftTenths > focusTenths) driftTenths = focusTenths;
  return zenGain(focusTenths - driftTenths) + zenBonus(driftTenths);
}
