#pragma once
#include <math.h>
#include <stdint.h>

// Minijuego "carrera" (DASH, docs/NEW_GAMES.md): el bicho baja una pendiente en trineo. Se queda
// en el centro de la pantalla y el mundo pasa por debajo; inclinar la placa lo mueve de lado para
// esquivar las filas de obstaculos, que llegan cada vez mas deprisa. Tres golpes y se acaba.
// Logica pura, con tests en test/test_dashgame.cpp: el sketch solo dibuja, lee la inclinacion
// (imuRollFast) y da el premio.
//
// Coordenadas: x lateral en px de pista (0 = centro, +-DASH_HALF_W los bordes); y = distancia
// recorrida en px (crece hacia abajo, en el sentido de la marcha). El trineo esta en y = dist
// (su borde de abajo) y en pantalla siempre en (CX, DASH_PET_Y): un objeto en (x, y) se dibuja en
// (CX + x - px, DASH_PET_Y + y - dist).

#define DASH_STEP_MS 10u          // paso fijo de la fisica
#define DASH_LATE_MAX_MS 250u     // un frame tardio cuenta como mucho esto
#define DASH_CAL_MS 2000u         // cuenta atras de la calibracion
#define DASH_CAL_WINDOW_MS 1000u  // la linea base es la media de este ultimo tramo
#define DASH_CAL_MAX_SD 4.0f      // grados: mas temblor y se repite ("Quieta")
#define DASH_CAL_RETRIES 2        // tras 2 repeticiones se acepta igual
#define DASH_DEAD_DEG 3.0f        // zona muerta de la direccion
#define DASH_FULL_DEG 22.0f       // direccion a tope
#define DASH_LAT_MAX 300.0f       // px/s de lado a tope de direccion, a 1x
#define DASH_HALF_W 170.0f        // media anchura de la pista
#define DASH_PET_Y 225            // borde de abajo del trineo en pantalla (CY = 233): con 200 el
                                  // marcador tapaba al bicho (visto en el renderizador)
#define DASH_HIT_W 60.0f          // caja de choque del trineo: la misma para todas las especies
#define DASH_HIT_H 34.0f
#define DASH_HIT_SHRINK 0.15f     // los obstaculos chocan un 15 % mas pequenos de lo que se ven
#define DASH_V0 160.0f            // px/s al empezar
#define DASH_DOUBLE_S 40.0f       // a los 40 s, el doble de rapido
#define DASH_SPEED_MAX 3.0f       // tope (a los 80 s)
#define DASH_ROW_GAP_START 260.0f // separacion entre filas a 1x...
#define DASH_ROW_GAP_END 170.0f   // ...y a 3x
#define DASH_GAP_MIN 110.0f       // hueco libre minimo en cada fila
#define DASH_OBS_W_MIN 50.0f      // ancho de un obstaculo
#define DASH_OBS_W_MAX 140.0f
#define DASH_OBS_H 36.0f          // alto (en el sentido de la marcha)
#define DASH_DOUBLE_P0 0.15f      // probabilidad de fila con 2 obstaculos a 1x...
#define DASH_DOUBLE_P1 0.60f      // ...y a 3x
#define DASH_REACH 0.7f           // el hueco siguiente cae a 0,7 x lo que se puede recorrer de lado
#define DASH_SPAWN_AHEAD 340.0f   // las filas nacen asi de lejos (por debajo del borde de la pantalla)
#define DASH_INVULN_MS 1200u      // tras un golpe
#define DASH_LIVES 3
#define DASH_PER_POINT 4          // VEL: 1 punto cada 4 filas esquivadas...
#define DASH_GAIN_MAX 18          // ...con el tope del saco
#define DASH_WEIGHT_DIV 8         // peso: -filas/8
#define DASH_SLED_W_MIN 70        // ancho dibujado del trineo: sigue al sprite entre estos topes
#define DASH_SLED_W_MAX 150
#define DASH_SLED_MARGIN 24
#define DASH_SLED_LEAN_DEG 12.0f  // el trineo se inclina esto con la direccion a tope
#define DASH_ROWS 8               // filas vivas a la vez (caben de sobra: se ven 2-4)
#define DASH_KINDS 3              // aspectos de obstaculo por bioma (solo dibujo)

struct DashObs {
  float x0, x1;   // de lado, en px de pista
  uint8_t kind;   // 0..DASH_KINDS-1, solo para el dibujo
};

struct DashRow {
  bool live = false;
  float y = 0;          // distancia de su borde de arriba
  float gapX = 0;       // centro del hueco garantizado
  uint8_t n = 0;        // obstaculos (1 o 2)
  DashObs o[2];
  bool hit = false;     // esta fila costo un golpe
  bool counted = false; // ya paso por detras del trineo
};

struct DashState {
  uint32_t t = 0;         // ms de carrera (sin la calibracion)
  float dist = 0;         // px recorridos
  float px = 0;           // posicion lateral del trineo
  float steer = 0;        // ultima direccion (-1..1), para inclinar el dibujo
  bool bump = false;      // tocando un borde de la pista
  float nextRowY = DASH_SPAWN_AHEAD;
  float lastGapX = 0;
  DashRow rows[DASH_ROWS];
  uint16_t dodged = 0;
  uint8_t hits = 0;
  uint32_t invulnUntil = 0;  // t hasta el que no le pasa nada
  uint32_t lastHitT = 0;
  uint32_t rng = 1;
  uint32_t acc = 0;
};

inline uint32_t dashRand(uint32_t &s) {  // xorshift32
  s ^= s << 13;
  s ^= s >> 17;
  s ^= s << 5;
  return s;
}
inline float dashRand01(uint32_t &s) { return (float)(dashRand(s) % 10001) / 10000.0f; }

// direccion -1..1 a partir del angulo relativo a la linea base (+ = lado derecho abajo = derecha)
inline float dashSteer(float relDeg) {
  float a = fabsf(relDeg);
  if (a <= DASH_DEAD_DEG) return 0;
  float s = (a - DASH_DEAD_DEG) / (DASH_FULL_DEG - DASH_DEAD_DEG);
  if (s > 1) s = 1;
  return relDeg > 0 ? s : -s;
}

// factor de velocidad a los tMs de carrera: 1x, 2x a los 40 s, tope 3x a los 80 s
inline float dashSpeed(uint32_t tMs) {
  float f = 1.0f + (float)tMs / (DASH_DOUBLE_S * 1000.0f);
  return f > DASH_SPEED_MAX ? DASH_SPEED_MAX : f;
}

inline float dashProgress(float f) { return (f - 1.0f) / (DASH_SPEED_MAX - 1.0f); }  // 0 a 1x, 1 a 3x
inline float dashRowGap(float f) { return DASH_ROW_GAP_START + (DASH_ROW_GAP_END - DASH_ROW_GAP_START) * dashProgress(f); }
inline float dashDoubleP(float f) { return DASH_DOUBLE_P0 + (DASH_DOUBLE_P1 - DASH_DOUBLE_P0) * dashProgress(f); }
// px/s de lado a tope: crece con la raiz de la velocidad, para que a 3x se pueda seguir esquivando
inline float dashLatMax(float f) { return DASH_LAT_MAX * sqrtf(f); }
// hasta donde puede moverse el hueco de una fila a la siguiente sin dejar de ser alcanzable
inline float dashReach(float f) { return DASH_REACH * dashLatMax(f) * (dashRowGap(f) / (DASH_V0 * f)); }

// una fila nueva a partir del hueco de la anterior (FR-DASH.10): el hueco, de DASH_GAP_MIN como
// minimo, cae al alcance del anterior y dentro de la pista; 1 o 2 obstaculos pegados a el
inline DashRow dashNextRow(uint32_t &rng, float prevGapX, float f) {
  DashRow r;
  r.live = true;
  float reach = dashReach(f);
  // una fila doble necesita sitio para un obstaculo a cada lado: su hueco va mas al centro
  bool two = dashRand01(rng) < dashDoubleP(f);
  float lim1 = DASH_HALF_W - DASH_GAP_MIN / 2, lim2 = lim1 - DASH_OBS_W_MIN;
  float lo = prevGapX - reach, hi = prevGapX + reach;
  if (two && (lo > lim2 || hi < -lim2)) two = false;  // desde aqui no se llega a un hueco central
  float lim = two ? lim2 : lim1;
  if (lo < -lim) lo = -lim;
  if (hi > lim) hi = lim;
  r.gapX = lo + (hi - lo) * dashRand01(rng);
  float gl = r.gapX - DASH_GAP_MIN / 2, gr = r.gapX + DASH_GAP_MIN / 2;
  float roomL = gl + DASH_HALF_W, roomR = DASH_HALF_W - gr;  // sitio a cada lado del hueco
  auto width = [&](float room) {
    float w = DASH_OBS_W_MIN + (DASH_OBS_W_MAX - DASH_OBS_W_MIN) * dashRand01(rng);
    return w > room ? room : w;
  };
  auto left = [&](DashObs &o) { float w = width(roomL); o = { gl - w, gl, (uint8_t)(dashRand(rng) % DASH_KINDS) }; };
  auto right = [&](DashObs &o) { float w = width(roomR); o = { gr, gr + w, (uint8_t)(dashRand(rng) % DASH_KINDS) }; };
  if (two) {
    left(r.o[0]);
    right(r.o[1]);
    r.n = 2;
  } else {
    // uno solo, del lado con sitio (si caben los dos lados, al azar)
    bool useLeft = roomL >= DASH_OBS_W_MIN && (roomR < DASH_OBS_W_MIN || (dashRand(rng) & 1));
    if (useLeft) left(r.o[0]);
    else right(r.o[0]);
    r.n = 1;
  }
  return r;
}

// la caja del trineo (en px de pista y distancia) toca el obstaculo, ya encogido?
inline bool dashHitsObs(float px, float dist, float rowY, const DashObs &o) {
  float sx = (o.x1 - o.x0) * DASH_HIT_SHRINK / 2, sy = DASH_OBS_H * DASH_HIT_SHRINK / 2;
  float ox0 = o.x0 + sx, ox1 = o.x1 - sx, oy0 = rowY + sy, oy1 = rowY + DASH_OBS_H - sy;
  float tx0 = px - DASH_HIT_W / 2, tx1 = px + DASH_HIT_W / 2, ty0 = dist - DASH_HIT_H, ty1 = dist;
  return tx0 < ox1 && tx1 > ox0 && ty0 < oy1 && ty1 > oy0;
}

inline void dashStart(DashState &d, uint32_t seed) {
  d = DashState();
  d.rng = seed ? seed : 1;
}

inline bool dashOver(const DashState &d) { return d.hits >= DASH_LIVES; }
inline bool dashInvuln(const DashState &d) { return d.t < d.invulnUntil; }

// un paso fijo de DASH_STEP_MS con la direccion `steer` (-1..1)
inline void dashStep(DashState &d, float steer) {
  const float dt = DASH_STEP_MS / 1000.0f;
  float f = dashSpeed(d.t);
  d.steer = steer;
  d.dist += DASH_V0 * f * dt;
  d.px += steer * dashLatMax(f) * dt;
  float lim = DASH_HALF_W - DASH_HIT_W / 2;
  d.bump = false;
  if (d.px > lim) { d.px = lim; d.bump = steer > 0; }
  if (d.px < -lim) { d.px = -lim; d.bump = steer < 0; }
  // filas nuevas por delante
  while (d.nextRowY < d.dist + DASH_SPAWN_AHEAD) {
    for (DashRow &r : d.rows) {
      if (r.live) continue;
      r = dashNextRow(d.rng, d.lastGapX, f);
      r.y = d.nextRowY;
      d.lastGapX = r.gapX;
      break;
    }
    d.nextRowY += dashRowGap(f);
  }
  for (DashRow &r : d.rows) {
    if (!r.live) continue;
    if (!r.hit && !dashInvuln(d)) {
      for (uint8_t i = 0; i < r.n; i++)
        if (dashHitsObs(d.px, d.dist, r.y, r.o[i])) {
          r.hit = true;
          d.hits++;
          d.lastHitT = d.t;
          d.invulnUntil = d.t + DASH_INVULN_MS;
          break;
        }
    }
    // la fila ya quedo detras del trineo: esquivada (si no le costo un golpe)
    if (!r.counted && r.y + DASH_OBS_H < d.dist - DASH_HIT_H) {
      r.counted = true;
      if (!r.hit) d.dodged++;
    }
    if (r.y + DASH_OBS_H < d.dist - 400) r.live = false;  // fuera de la pantalla por arriba
  }
  d.t += DASH_STEP_MS;
}

// avanza dtMs de reloj con la direccion de ahora (pasos fijos: igual a cualquier ritmo de frame)
inline void dashAdvance(DashState &d, float steer, uint32_t dtMs) {
  if (dtMs > DASH_LATE_MAX_MS) dtMs = DASH_LATE_MAX_MS;
  d.acc += dtMs;
  while (d.acc >= DASH_STEP_MS && !dashOver(d)) {
    dashStep(d, steer);
    d.acc -= DASH_STEP_MS;
  }
}

// puntos de VEL que da una carrera con `dodged` filas esquivadas
inline uint8_t dashGain(uint16_t dodged) {
  uint16_t g = dodged / DASH_PER_POINT;
  return g > DASH_GAIN_MAX ? DASH_GAIN_MAX : (uint8_t)g;
}

// ancho dibujado del trineo para un sprite de `spriteW` px visibles en pantalla
inline int dashSledW(int spriteW) {
  int w = spriteW + DASH_SLED_MARGIN;
  return w < DASH_SLED_W_MIN ? DASH_SLED_W_MIN : (w > DASH_SLED_W_MAX ? DASH_SLED_W_MAX : w);
}

// Calibracion (FR-DASH.2): media y desviacion de la inclinacion en el ultimo tramo de la cuenta
struct DashCal {
  double sum = 0, sumSq = 0;
  uint32_t n = 0;
  void reset() { *this = DashCal(); }
  void add(float deg) { sum += deg; sumSq += (double)deg * deg; n++; }
  float mean() const { return n ? (float)(sum / n) : 0.0f; }
  float sd() const {
    if (n < 2) return 0;
    double m = sum / n, v = sumSq / n - m * m;
    return v > 0 ? (float)sqrt(v) : 0.0f;
  }
  // con este temblor, se repite la cuenta? (tras DASH_CAL_RETRIES repeticiones, ya no)
  bool shaky(uint8_t retries) const { return sd() > DASH_CAL_MAX_SD && retries < DASH_CAL_RETRIES; }
};
