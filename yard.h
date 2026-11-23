#pragma once
// El patio (v2.0, fase Y2): los bichos que pasean por el mapa (logica pura, test/test_yard.cpp).
//
// Cada bicho tiene sus pies en (x, y) del lienzo de 466x466 y pasea por la mascara de paso del
// mapa (yardbg.h: 15x15 celdas de 32 px, 1 = no se pisa). El planificador sustituye al de la
// escena de lado (beh/behNext): quieto unos segundos, un gesto (Pose/Nod/Breath) o un paseo a
// una celda libre al azar, por un camino de celdas (BFS en la rejilla) que se acorta en linea
// recta cuando no cruza nada bloqueado. Nunca pisa una celda bloqueada (se comprueba en cada
// paso), dos bichos guardan una distancia minima y se dibujan de arriba abajo (el de mas abajo
// encima). Inclinar la placa los lleva hacia el lado bajo, sin salirse de la mascara.
//
// El azar es inyectado (YardRng) para que los tests sean deterministas.
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "yardbg.h"
#include "tuning.h"  // v3.20: BED_WALK_MAX_MS (el paseo hasta la cama)

#define YARD_PETS_MAX 3          // el bicho de casa + 1 de la caja (y sitio para otro de casa)
#define YARD_SPEED 30.0f         // px/s, como el paseo de la escena de lado
#define YARD_TILT_SPEED 50.0f    // px/s hacia el lado inclinado
#define YARD_MIN_DIST 64         // px entre los pies de dos bichos
#define YARD_IDLE_MS 2000        // quieto entre paseos: 2-5 s
#define YARD_IDLE_RAND_MS 3000
#define YARD_WALK_MAX_MS 25000   // un paseo que no llega (otro bicho en medio) se deja
#define YARD_TILT_REST_MS 800    // al nivelar la placa, quieto un momento
#define YARD_PATH_MAX 64
#define YARD_THING_GAP 36      // px entre dos cosas del suelo (cacas de 32 px)
#define YARD_CELLS (YBG_GRID * YBG_GRID)
// escala de los bichos en decimas sobre su escala entera (v2.1: 11; v3.5: 12, asi casi todos, a escala
// entera 2, quedan a 2,4 como el mapa, YBG_ZOOM10; v3.8: 13, a 2,6, un poco mas grandes que el mapa)
#define YARD_PET_SCALE10 13

// modo de un bicho
enum : uint8_t { YM_IDLE = 0, YM_WALK, YM_GESTURE, YM_SLEEP, YM_STAY };
// v3.20 (mejoras de casa): lo que hace por su cuenta, encima del modo. Ir a la cama dormido, tumbado en ella,
// saliendo de ella al despertar, o de visita al gimnasio o al juguete (al llegar, su gesto)
// v3.22 (PALS, pals.h): YT_SCENE = lo lleva el director de escenas de los dos bichos (yardDecide no lo toca)
enum : uint8_t { YT_NONE = 0, YT_BED_WALK, YT_BED, YT_BED_LEAVE, YT_VISIT, YT_SCENE };
#define YARD_BEDS 2  // una cama por hueco de casa (p[0] y p[1])
// lo que el juego le pide en este paso: nada (el planificador decide), dormir, o quedarse quieto
// (comiendo, triste, banandose, mareado...: el dibujo elige la accion)
enum : uint8_t { YH_NONE = 0, YH_SLEEP, YH_STAY };

struct YardRng {
  uint32_t s = 1;
  uint32_t next() {
    s = s * 1664525u + 1013904223u;
    return s >> 8;
  }
  int below(int n) { return n > 0 ? (int)(next() % (uint32_t)n) : 0; }
};

struct YardPet {
  bool on = false;
  bool walks = true;      // tiene sprite de andar (sin el, solo quieto y gestos)
  float x = 233, y = 233; // los pies, en el lienzo
  uint8_t mode = YM_IDLE;
  int8_t face = 1;        // -1 mira a la izquierda, 1 a la derecha
  uint8_t act = 0;        // gesto en curso (lo pone pickGesture o el juego, YM_GESTURE)
  bool tilting = false;   // el paseo de ahora es por la inclinacion
  int8_t tiltSide = 0;
  uint32_t t0 = 0;        // inicio de la animacion de ahora
  uint32_t until = 0;     // fin del estado de ahora
  uint8_t path[YARD_PATH_MAX];
  uint8_t pathN = 0, pathI = 0;
  // v3.20: la cama y las visitas (YT_*)
  uint8_t task = YT_NONE;
  bool loopAct = false;    // el gesto de ahora se repite (el ejercicio, el juego) en vez de quedarse en su final
  uint8_t arriveAct = 0;   // el gesto al acabar el paseo de la visita (o al salir de la cama: el estiron)
  uint32_t arriveMs = 0;   // 0 = ninguno
  bool arriveLoop = false;
  int16_t lookX = -1;      // hacia donde mira al llegar (el mueble); -1 = da igual
  // v3.22 (PALS): solo con task == YT_SCENE. sceneDist = cuanto puede acercarse a los otros (0: YARD_MIN_DIST);
  // scenePct = su velocidad al andar, en % del paseo (0: la normal)
  uint8_t sceneDist = 0;
  uint8_t scenePct = 0;
};

// v3.20: la cama de un hueco de casa en el patio: los pies del bicho tumbado (su celda, bloqueada en la mascara
// para los demas, se le deja libre a el para entrar y salir)
struct YardBed {
  bool on = false;
  float x = 0, y = 0;
};

// gesto al azar del bicho i: true y la accion y su duracion, o false si no tiene ninguno
typedef bool (*YardGestureFn)(uint8_t i, uint8_t *act, uint32_t *ms);

struct YardWorld {
  YardPet p[YARD_PETS_MAX];
  uint8_t n = 0;
  YardRng rng;
  YardGestureFn pickGesture = nullptr;
  int16_t avoidX = 32767;  // v3.3: ni los paseos al azar ni la inclinacion llevan mas a la derecha (el menu)
  YardBed bed[YARD_BEDS];  // v3.20: la cama de p[0] y la de p[1] (on = false: sin cama, como siempre)
  bool bedSnap = false;    // v3.20: el patio acaba de verse: el que ya dormia aparece en su cama, sin paseo
};

inline bool yardBedTask(uint8_t t) { return t == YT_BED_WALK || t == YT_BED || t == YT_BED_LEAVE; }

// ---------- celdas ----------
inline int yardCellIdx(int cx, int cy) { return cy * YBG_GRID + cx; }
inline int yardCellCX(int idx) { return idx % YBG_GRID; }
inline int yardCellCY(int idx) { return idx / YBG_GRID; }
inline bool yardFree(const uint8_t *walk, int idx) {
  return idx >= 0 && idx < YARD_CELLS && !yardCellBlocked(walk, yardCellCX(idx), yardCellCY(idx));
}
inline int yardCellAt(float x, float y) {
  int cx = yardCellOf((int)x), cy = yardCellOf((int)y);
  if (x < 0 || y < 0 || cx >= YBG_GRID || cy >= YBG_GRID) return -1;
  return yardCellIdx(cx, cy);
}
inline bool yardSpotOk(const uint8_t *walk, float x, float y) {
  return x >= 0 && y >= 0 && !yardPointBlocked(walk, (int)x, (int)y);
}

// bloquea las celdas fuera del circulo y bajo el HUD, con las mismas reglas que tools/pack_yard.py
// (outside_or_hud: el centro de cada celda). v3.5: tambien sobre la mascara de un mapa, por si el .bin
// se hizo con otra escala del mapa (YBG_ZOOM10)
inline void yardMaskFrame(uint8_t *walk) {
  for (int cy = 0; cy < YBG_GRID; cy++)
    for (int cx = 0; cx < YBG_GRID; cx++) {
      int px = yardCellCenter(cx), py = yardCellCenter(cy);
      int dx = px - YBG_CANVAS / 2, dy = py - YBG_CANVAS / 2, r = YBG_CANVAS / 2 - 12;
      bool out = dx * dx + dy * dy > r * r;
      bool clock = px >= 140 && px <= 326 && py >= 20 && py <= 110;
      bool dock = px >= 130 && px <= 336 && py >= 376 && py <= 440;  // = DOCK_ZONE de tools/pack_yard.py
      if (out || clock || dock) {
        int k = yardCellIdx(cx, cy);
        walk[k >> 3] |= (uint8_t)(1 << (k & 7));
      }
    }
}

// mascara de respaldo (sin mapa): todo se pisa salvo fuera del circulo y bajo el HUD
inline void yardFallbackWalk(uint8_t *walk) {
  memset(walk, 0, YBG_WALK_BYTES);
  yardMaskFrame(walk);
}

inline int yardFreeCount(const uint8_t *walk) {
  int n = 0;
  for (int k = 0; k < YARD_CELLS; k++) n += yardFree(walk, k);
  return n;
}

// deja libre solo la zona conectada (4 vecinos) mas grande: las celdas sueltas o los rincones cerrados
// del mapa se marcan como bloqueados, asi nada aparece ni pasea donde no se puede llegar. Devuelve
// cuantas celdas quedan libres
inline int yardKeepLargestRegion(uint8_t *walk) {
  uint8_t comp[YARD_CELLS];
  uint8_t q[YARD_CELLS];
  memset(comp, 0, sizeof(comp));
  int best = 0, bestN = 0, id = 0;
  static const int8_t D[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
  for (int c0 = 0; c0 < YARD_CELLS; c0++) {
    if (comp[c0] || !yardFree(walk, c0) || id == 255) continue;
    id++;
    int qh = 0, qt = 0;
    q[qt++] = (uint8_t)c0;
    comp[c0] = (uint8_t)id;
    while (qh < qt) {
      int c = q[qh++];
      for (auto &d : D) {
        int nx = yardCellCX(c) + d[0], ny = yardCellCY(c) + d[1];
        if (yardCellBlocked(walk, nx, ny)) continue;
        int k = yardCellIdx(nx, ny);
        if (comp[k]) continue;
        comp[k] = (uint8_t)id;
        q[qt++] = (uint8_t)k;
      }
    }
    if (qt > bestN) {
      bestN = qt;
      best = id;
    }
  }
  for (int c = 0; c < YARD_CELLS; c++)
    if (comp[c] && comp[c] != best) walk[c >> 3] |= (uint8_t)(1 << (c & 7));
  return bestN;
}

// camino de celdas (4 vecinos) de `from` a `to`, sin incluir `from`; devuelve su largo (0 si no
// hay, o si from == to). Si es mas largo que max se guarda el principio (sigue acercandose).
inline int yardPath(const uint8_t *walk, int from, int to, uint8_t *out, int max) {
  if (!yardFree(walk, from) || !yardFree(walk, to) || from == to) return 0;
  int16_t prev[YARD_CELLS];
  uint8_t q[YARD_CELLS];
  for (int i = 0; i < YARD_CELLS; i++) prev[i] = -1;
  int qh = 0, qt = 0;
  q[qt++] = (uint8_t)from;
  prev[from] = (int16_t)from;
  static const int8_t D[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
  while (qh < qt && prev[to] < 0) {
    int c = q[qh++], cx = yardCellCX(c), cy = yardCellCY(c);
    for (auto &d : D) {
      int nx = cx + d[0], ny = cy + d[1];
      if (yardCellBlocked(walk, nx, ny)) continue;
      int k = yardCellIdx(nx, ny);
      if (prev[k] >= 0) continue;
      prev[k] = (int16_t)c;
      q[qt++] = (uint8_t)k;
    }
  }
  if (prev[to] < 0) return 0;
  int len = 0;
  for (int c = to; c != from; c = prev[c]) len++;
  int keep = len < max ? len : max;
  // del destino hacia atras: se salta lo que no cabe al final
  int i = len;
  for (int c = to; c != from; c = prev[c]) {
    i--;
    if (i < keep) out[i] = (uint8_t)c;
  }
  return keep;
}

// la recta de a a b no pisa nada bloqueado (muestras cada px y un margen de 6 px a los lados)
inline bool yardLineClear(const uint8_t *walk, float ax, float ay, float bx, float by) {
  float dx = bx - ax, dy = by - ay;
  float len = sqrtf(dx * dx + dy * dy);
  int n = (int)len + 1;
  for (int i = 0; i <= n; i++) {
    float t = (float)i / n;
    float x = ax + dx * t, y = ay + dy * t;
    if (!yardSpotOk(walk, x, y) || !yardSpotOk(walk, x - 6, y) || !yardSpotOk(walk, x + 6, y) ||
        !yardSpotOk(walk, x, y - 6) || !yardSpotOk(walk, x, y + 6))
      return false;
  }
  return true;
}

inline float yardDist(float ax, float ay, float bx, float by) {
  float dx = ax - bx, dy = ay - by;
  return sqrtf(dx * dx + dy * dy);
}

// lejos (YARD_MIN_DIST) de los otros bichos encendidos y de donde van
inline bool yardSpotAway(const YardWorld &w, int self, float x, float y) {
  for (int j = 0; j < w.n; j++) {
    if (j == self || !w.p[j].on) continue;
    const YardPet &o = w.p[j];
    if (yardDist(x, y, o.x, o.y) < YARD_MIN_DIST) return false;
    if (o.mode == YM_WALK && o.pathN) {
      int last = o.path[o.pathN - 1];
      if (yardDist(x, y, yardCellCenter(yardCellCX(last)), yardCellCenter(yardCellCY(last))) < YARD_MIN_DIST) return false;
    }
  }
  return true;
}

// pone al bicho i en una celda libre al azar, lejos de los demas. false si el mapa no tiene sitio
inline bool yardPlace(YardWorld &w, const uint8_t *walk, int i) {
  int free = yardFreeCount(walk);
  if (!free) return false;
  YardPet &p = w.p[i];
  for (int tries = 0; tries < 64; tries++) {
    int k = w.rng.below(free);
    for (int c = 0; c < YARD_CELLS; c++)
      if (yardFree(walk, c) && k-- == 0) {
        float x = (float)yardCellCenter(yardCellCX(c)), y = (float)yardCellCenter(yardCellCY(c));
        if (tries < 48 && !yardSpotAway(w, i, x, y)) break;
        p.x = x;
        p.y = y;
        p.pathN = p.pathI = 0;
        p.mode = YM_IDLE;
        p.until = 0;
        return true;
      }
  }
  return false;
}

// la celda libre cuyo centro esta mas cerca de (x, y), o -1
inline int yardNearestFreeCell(const uint8_t *walk, float x, float y) {
  int best = -1;
  float bd = 1e9f;
  for (int c = 0; c < YARD_CELLS; c++) {
    if (!yardFree(walk, c)) continue;
    float d = yardDist((float)yardCellCenter(yardCellCX(c)), (float)yardCellCenter(yardCellCY(c)), x, y);
    if (d < bd) {
      bd = d;
      best = c;
    }
  }
  return best;
}

// a donde va ahora (el centro de la celda del camino que toca)
inline void yardWaypoint(const YardPet &p, float *x, float *y) {
  int c = p.path[p.pathI];
  *x = (float)yardCellCenter(yardCellCX(c));
  *y = (float)yardCellCenter(yardCellCY(c));
}

// empieza un paseo de i a la celda `to`. false si no hay camino
inline bool yardWalkTo(YardWorld &w, const uint8_t *walk, int i, int to, uint32_t now) {
  YardPet &p = w.p[i];
  int from = yardCellAt(p.x, p.y);
  int n = yardPath(walk, from, to, p.path, YARD_PATH_MAX);
  if (!n) return false;
  p.pathN = (uint8_t)n;
  p.pathI = 0;
  p.mode = YM_WALK;
  p.t0 = now;
  p.until = now + YARD_WALK_MAX_MS;
  return true;
}

// paseo a una celda libre al azar (a 2 celdas o mas), alcanzable y lejos de los demas
inline bool yardWander(YardWorld &w, const uint8_t *walk, int i, uint32_t now) {
  YardPet &p = w.p[i];
  int from = yardCellAt(p.x, p.y);
  int free = yardFreeCount(walk);
  if (from < 0 || free < 2) return false;
  for (int tries = 0; tries < 24; tries++) {
    int k = w.rng.below(free), to = -1;
    for (int c = 0; c < YARD_CELLS && to < 0; c++)
      if (yardFree(walk, c) && k-- == 0) to = c;
    if (to < 0) continue;
    int ddx = yardCellCX(to) - yardCellCX(from), ddy = yardCellCY(to) - yardCellCY(from);
    if (ddx * ddx + ddy * ddy < 4) continue;
    if (yardCellCenter(yardCellCX(to)) > w.avoidX) continue;
    if (!yardSpotAway(w, i, (float)yardCellCenter(yardCellCX(to)), (float)yardCellCenter(yardCellCY(to)))) continue;
    if (yardWalkTo(w, walk, i, to, now)) return true;
  }
  return false;
}

// celda alcanzable mas hacia `side` (-1 izquierda, 1 derecha); a igual x, la de y mas cercana. v3.3: sin
// pasar de maxX (el centro de la celda; la columna del menu del bicho), salvo que no haya otra
inline int yardTiltTargetSide(const uint8_t *walk, int from, int side, int maxX = 32767) {
  if (!yardFree(walk, from)) return -1;
  bool seen[YARD_CELLS] = {};
  uint8_t q[YARD_CELLS];
  int qh = 0, qt = 0, best = -1;
  q[qt++] = (uint8_t)from;
  seen[from] = true;
  static const int8_t D[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
  int fy = yardCellCY(from);
  while (qh < qt) {
    int c = q[qh++];
    if (yardCellCenter(yardCellCX(c)) <= maxX) {
      if (best < 0) {
        best = c;
      } else {
        int bx = yardCellCX(best) * side, cxs = yardCellCX(c) * side;
        int dyb = abs(yardCellCY(best) - fy), dyc = abs(yardCellCY(c) - fy);
        if (cxs > bx || (cxs == bx && dyc < dyb)) best = c;
      }
    }
    for (auto &d : D) {
      int nx = yardCellCX(c) + d[0], ny = yardCellCY(c) + d[1];
      if (yardCellBlocked(walk, nx, ny)) continue;
      int k = yardCellIdx(nx, ny);
      if (seen[k]) continue;
      seen[k] = true;
      q[qt++] = (uint8_t)k;
    }
  }
  return best < 0 ? from : best;
}

// orden de dibujo: los encendidos de y menor a mayor (el de mas abajo se pinta el ultimo).
// Devuelve cuantos hay en order.
inline int yardDrawOrder(const YardWorld &w, uint8_t *order) {
  int n = 0;
  for (int i = 0; i < w.n; i++) {
    if (!w.p[i].on) continue;
    int j = n++;
    while (j > 0 && w.p[order[j - 1]].y > w.p[i].y) {
      order[j] = order[j - 1];
      j--;
    }
    order[j] = (uint8_t)i;
  }
  return n;
}

// el siguiente capricho de un bicho quieto: paseo, gesto o seguir quieto
inline void yardDecide(YardWorld &w, const uint8_t *walk, int i, uint32_t now) {
  YardPet &p = w.p[i];
  p.t0 = now;
  p.tilting = false;
  p.loopAct = false;
  int r = w.rng.below(100);
  if (r < 45 && p.walks && yardWander(w, walk, i, now)) return;
  if (r >= 45 && r < 70 && w.pickGesture) {
    uint8_t act;
    uint32_t ms;
    if (w.pickGesture((uint8_t)i, &act, &ms)) {
      p.mode = YM_GESTURE;
      p.act = act;
      p.until = now + ms;
      return;
    }
  }
  p.mode = YM_IDLE;
  p.until = now + YARD_IDLE_MS + (uint32_t)w.rng.below(YARD_IDLE_RAND_MS);
}

// un gesto pedido por el juego (contento tras el bano, subida de nivel...): quieto y la accion
inline void yardGesture(YardWorld &w, int i, uint8_t act, uint32_t ms, uint32_t now) {
  YardPet &p = w.p[i];
  p.mode = YM_GESTURE;
  p.act = act;
  p.t0 = now;
  p.until = now + ms;
  p.pathN = p.pathI = 0;
  p.tilting = false;
  p.loopAct = false;
  p.arriveMs = 0;
  if (p.task == YT_VISIT || p.task == YT_SCENE) p.task = YT_NONE;  // (la cama la sigue llevando yardStep; v3.22: un
                                                                   //  gesto del juego corta la escena)
}

// avanza a i por su camino dt ms a `speed` px/s. false si se paro (llego, bloqueado o pegado a otro)
inline bool yardAdvance(YardWorld &w, const uint8_t *walk, int i, uint32_t dt, float speed) {
  YardPet &p = w.p[i];
  if (p.pathI >= p.pathN) return false;
  // atajo: si se ve en linea recta un punto mas adelantado del camino, se va directo
  for (int look = 0; look < 4 && p.pathI + 1 < p.pathN; look++) {
    int c = p.path[p.pathI + 1];
    if (!yardLineClear(walk, p.x, p.y, (float)yardCellCenter(yardCellCX(c)), (float)yardCellCenter(yardCellCY(c)))) break;
    p.pathI++;
  }
  float step = speed * dt / 1000.0f;
  while (step > 0 && p.pathI < p.pathN) {
    float tx, ty;
    yardWaypoint(p, &tx, &ty);
    float dx = tx - p.x, dy = ty - p.y, d = sqrtf(dx * dx + dy * dy);
    float s = step < d ? step : d;
    float nx = d > 0 ? p.x + dx * s / d : tx, ny = d > 0 ? p.y + dy * s / d : ty;
    if (!yardSpotOk(walk, nx, ny)) return false;  // (no deberia pasar: el camino va por celdas libres)
    for (int j = 0; j < w.n; j++) {
      if (j == i || !w.p[j].on) continue;
      if (yardBedTask(p.task) && yardBedTask(w.p[j].task)) continue;  // v3.20: las camas van juntas
      float now = yardDist(nx, ny, w.p[j].x, w.p[j].y), before = yardDist(p.x, p.y, w.p[j].x, w.p[j].y);
      float minD = (p.task == YT_SCENE && p.sceneDist) ? (float)p.sceneDist : (float)YARD_MIN_DIST;  // v3.22: escenas
      if (now < minD && now < before) return false;  // se acercaria demasiado: se para
    }
    if (dx > 0.5f) p.face = 1;
    else if (dx < -0.5f) p.face = -1;
    p.x = nx;
    p.y = ny;
    step -= s;
    if (s >= d) p.pathI++;
  }
  return p.pathI < p.pathN;
}

// ---------- la cama y las visitas (v3.20, mejoras de casa) ----------
// Con cama (w.bed[i].on), el bicho que se duerme con el patio a la vista anda hasta ella (si llega en
// BED_WALK_MAX_MS; si no, o si el patio se acaba de abrir, aparece ya tumbado) y duerme alli. Al despertar sale a
// la celda libre mas cercana. Solo se ve: el sueno de la logica empieza y acaba cuando toca. Sin cama, nada cambia.

inline void yardReachable(const uint8_t *walk, int from, bool *seen);  // (mas abajo, con la comida)

// la mascara con la celda de la cama del bicho i libre (para entrar y salir de ella); buf es del que llama
inline const uint8_t *yardBedWalk(const YardWorld &w, const uint8_t *walk, int i, uint8_t *buf) {
  if (i >= YARD_BEDS || !w.bed[i].on) return walk;
  int c = yardCellAt(w.bed[i].x, w.bed[i].y);
  if (c < 0) return walk;
  memcpy(buf, walk, YBG_WALK_BYTES);
  buf[c >> 3] &= (uint8_t)~(1 << (c & 7));
  return buf;
}

// tumbado en su cama ya
inline void yardIntoBed(YardWorld &w, int i, uint32_t now) {
  YardPet &p = w.p[i];
  if (p.task != YT_BED) p.t0 = now;
  p.x = w.bed[i].x;
  p.y = w.bed[i].y;
  p.mode = YM_SLEEP;
  p.task = YT_BED;
  p.pathN = p.pathI = 0;
  p.tilting = false;
  p.loopAct = false;
  p.until = now;
}

// el gesto que esperaba al final del paseo (la visita, o el estiron al salir de la cama), mirando al mueble
inline void yardArrive(YardWorld &w, int i, uint32_t now) {
  YardPet &p = w.p[i];
  if (!p.arriveMs) return;
  if (p.lookX >= 0) p.face = p.lookX > p.x ? 1 : -1;
  p.mode = YM_GESTURE;
  p.act = p.arriveAct;
  p.loopAct = p.arriveLoop;
  p.t0 = now;
  p.until = now + p.arriveMs;
  p.arriveMs = 0;
  p.pathN = p.pathI = 0;
}

// un paso del bicho i dormido y con cama: andando hacia ella, o tumbado
inline void yardSleepStep(YardWorld &w, const uint8_t *walk, int i, uint32_t now, uint32_t dt) {
  YardPet &p = w.p[i];
  if (p.task == YT_BED) {  // pegado a su cama (tambien si cambio el mapa)
    yardIntoBed(w, i, now);
    return;
  }
  uint8_t buf[YBG_WALK_BYTES];
  const uint8_t *bw = yardBedWalk(w, walk, i, buf);
  if (p.task == YT_BED_WALK) {
    // llego (o algo se puso en medio, o se acabo el tiempo): los ultimos px hasta la cama, de golpe
    if (!yardAdvance(w, bw, i, dt, YARD_SPEED) || (int32_t)(now - p.until) >= 0) yardIntoBed(w, i, now);
    return;
  }
  // se acaba de dormir: andando solo si se ve venir y llega a tiempo
  p.arriveMs = 0;
  int from = yardCellAt(p.x, p.y), to = yardCellAt(w.bed[i].x, w.bed[i].y);
  if (w.bedSnap || !p.walks || from < 0 || to < 0 || from == to) {
    yardIntoBed(w, i, now);
    return;
  }
  uint8_t path[YARD_PATH_MAX];
  int n = yardPath(bw, from, to, path, YARD_PATH_MAX);
  if (!n || n >= YARD_PATH_MAX || (uint32_t)(n * YBG_CELL * 1000.0f / YARD_SPEED) > BED_WALK_MAX_MS) {
    yardIntoBed(w, i, now);
    return;
  }
  memcpy(p.path, path, (size_t)n);
  p.pathN = (uint8_t)n;
  p.pathI = 0;
  p.mode = YM_WALK;
  p.task = YT_BED_WALK;
  p.tilting = false;
  p.loopAct = false;
  p.t0 = now;
  p.until = now + BED_WALK_MAX_MS;
}

// se desperto en la cama (o yendo a ella): sale andando a la celda libre mas cercana. stay = algo lo quiere
// quieto (una emocion, la inclinacion...): entonces aparece ya fuera
inline void yardLeaveBed(YardWorld &w, const uint8_t *walk, int i, bool stay, uint32_t now) {
  YardPet &p = w.p[i];
  p.loopAct = false;
  p.tilting = false;
  p.pathN = p.pathI = 0;
  p.mode = YM_IDLE;
  p.t0 = now;
  p.until = now + 600;
  p.task = YT_NONE;
  if (yardSpotOk(walk, p.x, p.y)) {  // aun no habia llegado a la cama: ya esta fuera
    yardArrive(w, i, now);
    return;
  }
  int to = yardNearestFreeCell(walk, p.x, p.y);
  if (to < 0) return;  // (yardStep lo recoloca)
  uint8_t buf[YBG_WALK_BYTES];
  if (!stay && p.walks && yardWalkTo(w, yardBedWalk(w, walk, i, buf), i, to, now)) {
    p.task = YT_BED_LEAVE;
    return;
  }
  p.x = (float)yardCellCenter(yardCellCX(to));
  p.y = (float)yardCellCenter(yardCellCY(to));
  yardArrive(w, i, now);  // (si algo lo quiere quieto, yardStep lo deja en nada)
}

// la celda a la que ir para usar el mueble con los pies en (ox, oy): la alcanzable mas cercana, mejor a un lado
// que delante o detras (asi no lo tapa ni queda tapado). -1 si no hay
inline int yardVisitCell(const uint8_t *walk, int from, float ox, float oy) {
  if (from < 0 || !yardFree(walk, from)) return -1;
  bool seen[YARD_CELLS];
  yardReachable(walk, from, seen);
  int best = -1;
  float bd = 1e9f;
  for (int k = -1; k < YARD_CELLS; k++) {  // (la suya primero y con 2 px de ventaja: las celdas miden 38 o 39 px)
    int c = k < 0 ? from : k;
    if (!seen[c]) continue;
    float cx = (float)yardCellCenter(yardCellCX(c)), cy = (float)yardCellCenter(yardCellCY(c));
    float d = yardDist(cx, cy, ox, oy) + (fabsf(cy - oy) > 4 ? 12.0f : 0.0f) - (c == from ? 2.0f : 0.0f);
    if (d < bd) {
      bd = d;
      best = c;
    }
  }
  return best;
}

// el bicho i va al mueble con los pies en (ox, oy) y al llegar hace act durante ms (en bucle si loop). Si ya
// esta al lado, no puede andar o no hay camino, lo hace donde esta. true si echo a andar
inline bool yardVisit(YardWorld &w, const uint8_t *walk, int i, float ox, float oy, uint8_t act, uint32_t ms, bool loop,
                      uint32_t now) {
  YardPet &p = w.p[i];
  p.arriveAct = act;
  p.arriveMs = ms ? ms : 1;
  p.arriveLoop = loop;
  p.lookX = (int16_t)ox;
  p.tilting = false;
  p.loopAct = false;
  int from = yardCellAt(p.x, p.y), to = yardVisitCell(walk, from, ox, oy);
  if (p.walks && to >= 0 && to != from && yardWalkTo(w, walk, i, to, now)) {
    p.task = YT_VISIT;
    return true;
  }
  p.task = YT_NONE;
  yardArrive(w, i, now);
  return false;
}

// el gesto a hacer en cuanto salga de la cama (el estiron de buenos dias); si no esta en ella, ya
inline void yardAfterBed(YardWorld &w, int i, uint8_t act, uint32_t ms, uint32_t now) {
  YardPet &p = w.p[i];
  p.arriveAct = act;
  p.arriveMs = ms ? ms : 1;
  p.arriveLoop = false;
  p.lookX = -1;
  if (!yardBedTask(p.task)) {
    p.task = YT_NONE;
    p.tilting = false;
    yardArrive(w, i, now);
  }
}

// un paso de dt ms para todos. hold[i] = YH_* que pide el juego; tilt = lado inclinado (-1, 0, 1)
inline void yardStep(YardWorld &w, const uint8_t *walk, uint32_t now, uint32_t dt, const uint8_t *hold, int8_t tilt) {
  for (int i = 0; i < w.n; i++) {
    YardPet &p = w.p[i];
    if (!p.on) continue;
    uint8_t h = hold ? hold[i] : YH_NONE;
    // v3.20: la cama (sin ella, todo como antes)
    bool bedOn = i < YARD_BEDS && w.bed[i].on;
    if (yardBedTask(p.task) && !bedOn) p.task = YT_NONE;  // se quito la cama (HTIER 0): yardPlace si hace falta
    if (bedOn && h == YH_SLEEP) {
      yardSleepStep(w, walk, i, now, dt);
      continue;
    }
    if (p.task == YT_BED || p.task == YT_BED_WALK) yardLeaveBed(w, walk, i, h != YH_NONE || tilt, now);  // se desperto
    if (p.task == YT_BED_LEAVE && h != YH_NONE) yardLeaveBed(w, walk, i, true, now);  // otra cosa lo quiere quieto
    uint8_t bedBuf[YBG_WALK_BYTES];
    const uint8_t *wk = p.task == YT_BED_LEAVE ? yardBedWalk(w, walk, i, bedBuf) : walk;
    if (!yardSpotOk(wk, p.x, p.y)) {  // el mapa cambio bajo sus pies
      yardPlace(w, walk, i);
      p.task = YT_NONE;
      p.arriveMs = 0;
    }
    if (h == YH_SLEEP || h == YH_STAY) {
      uint8_t m = h == YH_SLEEP ? YM_SLEEP : YM_STAY;
      if (p.mode != m) p.t0 = now;
      p.mode = m;
      p.pathN = p.pathI = 0;
      p.tilting = false;
      p.loopAct = false;
      p.arriveMs = 0;  // (una visita que esperaba se deja)
      if (p.task == YT_VISIT) p.task = YT_NONE;
      p.until = now;  // al soltarlo, decide enseguida
      continue;
    }
    if (p.mode == YM_SLEEP || p.mode == YM_STAY) {  // le acaban de soltar
      p.mode = YM_IDLE;
      p.until = now + 600;
      p.t0 = now;
    }
    if (tilt && p.walks && p.mode != YM_GESTURE && p.task != YT_BED_LEAVE) {
      // inclinada: hacia el lado bajo, por la mascara (v3.20: la visita se deja)
      if (p.task == YT_VISIT) {
        p.task = YT_NONE;
        p.arriveMs = 0;
      }
      if (!p.tilting || p.tiltSide != tilt) {
        int from = yardCellAt(p.x, p.y), to = yardTiltTargetSide(walk, from, tilt, w.avoidX);  // (v3.3: no bajo el menu)
        p.tilting = true;
        p.tiltSide = tilt;
        p.pathN = p.pathI = 0;
        if (to < 0 || to == from || !yardWalkTo(w, walk, i, to, now)) p.mode = YM_IDLE;
      }
      if (p.mode == YM_WALK && !yardAdvance(w, walk, i, dt, YARD_TILT_SPEED)) {
        p.mode = YM_IDLE;  // en el borde (o con otro delante): espera mientras siga inclinada
        p.pathN = p.pathI = 0;
      }
      p.until = now + YARD_TILT_REST_MS;
      continue;
    }
    if (p.tilting) {  // nivelada: quieto un momento y sigue con lo suyo
      p.tilting = false;
      p.tiltSide = 0;
      p.mode = YM_IDLE;
      p.pathN = p.pathI = 0;
      p.until = now + YARD_TILT_REST_MS;
    }
    if (p.mode == YM_WALK) {
      float sp = (p.task == YT_SCENE && p.scenePct) ? YARD_SPEED * p.scenePct / 100.0f : YARD_SPEED;  // v3.22: escenas
      if (!yardAdvance(w, wk, i, dt, sp) || (int32_t)(now - p.until) >= 0) {
        p.mode = YM_IDLE;
        p.pathN = p.pathI = 0;
        p.t0 = now;
        p.until = now + YARD_IDLE_MS / 2 + (uint32_t)w.rng.below(YARD_IDLE_RAND_MS);
        // v3.20: al final de la visita (o de salir de la cama), su gesto; si se quedo en la cama, fuera ya
        if (p.task == YT_BED_LEAVE && !yardSpotOk(walk, p.x, p.y)) {
          int c = yardNearestFreeCell(walk, p.x, p.y);
          if (c >= 0) {
            p.x = (float)yardCellCenter(yardCellCX(c));
            p.y = (float)yardCellCenter(yardCellCY(c));
          }
        }
        if (p.task == YT_VISIT || p.task == YT_BED_LEAVE) {
          p.task = YT_NONE;
          yardArrive(w, i, now);
        }
      }
      continue;
    }
    if ((int32_t)(now - p.until) >= 0 && p.task != YT_SCENE) yardDecide(w, walk, i, now);  // (la escena decide)
  }
}

// un punto del suelo cerca de (x, y) donde dejar algo (una caca): libre y a YARD_THING_GAP o mas de las
// otras cosas (ox, oy, n). Prueba a los lados de los pies y si no, el centro de la celda libre mas
// cercana. false si no hay ninguno.
inline bool yardDropSpot(const uint8_t *walk, float x, float y, const int16_t *ox, const int16_t *oy, int n, int *rx,
                         int *ry) {
  auto ok = [&](float px, float py) {
    if (!yardSpotOk(walk, px, py) || !yardSpotOk(walk, px - 12, py) || !yardSpotOk(walk, px + 12, py)) return false;
    for (int k = 0; k < n; k++)
      if (yardDist(px, py, ox[k], oy[k]) < YARD_THING_GAP) return false;
    return true;
  };
  static const int8_t TRY[6][2] = { { -38, 6 }, { 38, 6 }, { -38, -12 }, { 38, -12 }, { 0, 24 }, { -60, 14 } };
  for (auto &t : TRY)
    if (ok(x + t[0], y + t[1])) {
      *rx = (int)(x + t[0]);
      *ry = (int)(y + t[1]);
      return true;
    }
  int best = -1;
  float bd = 1e9f;
  for (int c = 0; c < YARD_CELLS; c++) {
    if (!yardFree(walk, c)) continue;
    float px = (float)yardCellCenter(yardCellCX(c)), py = (float)yardCellCenter(yardCellCY(c));
    float d = yardDist(px, py, x, y);
    if (d < bd && ok(px, py)) {
      bd = d;
      best = c;
    }
  }
  if (best < 0) return false;
  *rx = yardCellCenter(yardCellCX(best));
  *ry = yardCellCenter(yardCellCY(best));
  return true;
}

// una celda libre al azar lejos (40 px) de las cosas (ox, oy, n) y de los bichos: su centro
inline bool yardFreeSpot(const YardWorld &w, const uint8_t *walk, YardRng &rng, const int16_t *ox, const int16_t *oy,
                         int n, int *rx, int *ry) {
  int free = yardFreeCount(walk);
  if (!free) return false;
  for (int tries = 0; tries < 64; tries++) {
    int k = rng.below(free), c = 0;
    for (; c < YARD_CELLS; c++)
      if (yardFree(walk, c) && k-- == 0) break;
    float px = (float)yardCellCenter(yardCellCX(c)), py = (float)yardCellCenter(yardCellCY(c));
    bool far = true;
    for (int j = 0; j < n && far; j++) far = yardDist(px, py, ox[j], oy[j]) >= 40;
    for (int j = 0; j < w.n && far && tries < 48; j++)
      if (w.p[j].on) far = yardDist(px, py, w.p[j].x, w.p[j].y) >= 40;
    if (!far) continue;
    *rx = (int)px;
    *ry = (int)py;
    return true;
  }
  return false;
}

// el bocadillo con varias necesidades: cual toca ahora (una cada cycleMs)
inline int yardCycleIndex(int n, uint32_t now, uint32_t cycleMs) {
  return n > 0 ? (int)((now / cycleMs) % (uint32_t)n) : -1;
}

// toque sobre un bicho dibujado con los pies en (px, py), ancho w y alto h (con un margen)
inline bool yardPetHit(int x, int y, float px, float py, int w, int h) {
  int half = w / 2 + 8;
  if (half < 28) half = 28;
  if (h < 48) h = 48;
  return x >= px - half && x <= px + half && y >= py - h - 8 && y <= py + 12;
}

// ---------- hacia donde anda (sprite del paseo, Y3) ----------
// el patio se ve desde arriba: andando mas en vertical que en horizontal (|dy| > |dx|) se usa el
// sprite de espaldas (sube) o de frente (baja); si no, el de lado. Sin sprite de arriba/abajo (sprites
// viejos de la SD), el de lado segun hacia donde miraba (face)
enum : uint8_t { YW_LEFT = 0, YW_RIGHT, YW_UP, YW_DOWN };
inline uint8_t yardWalkDir(float dx, float dy, int8_t face, bool hasUp, bool hasDown) {
  if (fabsf(dy) > fabsf(dx)) {
    if (dy < 0 && hasUp) return YW_UP;
    if (dy > 0 && hasDown) return YW_DOWN;
  }
  return face < 0 ? YW_LEFT : YW_RIGHT;
}

// hacia donde va ahora: del bicho a su punto del camino (0, 0 si no tiene camino)
inline void yardHeading(const YardPet &p, float *dx, float *dy) {
  *dx = *dy = 0;
  if (p.pathI >= p.pathN) return;
  float tx, ty;
  yardWaypoint(p, &tx, &ty);
  *dx = tx - p.x;
  *dy = ty - p.y;
}

// ---------- la comida del menu del bicho (fase Y3; v3.3) ----------
// La comida que se elige en la lista del menu de un bicho aparece DELANTE de el y se la come ya, con su accion
// Eat (YF_EAT); solo al acabar el bocado el juego aplica la comida (yardFoodStep devuelve true). Si el bicho
// aun se esta apartando de debajo del menu, la eleccion espera (YF_QUEUED, sin nada en el suelo) y se sirve
// en cuanto llega. Una por bicho; la logica de Pet decide lo demas. (Hasta la v3.2 la comida caia, el bicho
// la veia y andaba hasta ella: ese camino ya no existe.)
#define YARD_FOOD_EAT_MS 900          // el bocado: la comida se encoge y desaparece
#define YARD_FOOD_FRONT_DX 28         // px delante de sus pies
#define YARD_FOOD_QUEUE_MAX_MS 6000   // si no acaba de apartarse (algo en medio), se le sirve donde este

enum : uint8_t { YF_NONE = 0, YF_QUEUED, YF_EAT };

struct YardFood {
  uint8_t phase = YF_NONE;
  uint8_t kind = 0;     // lo que es (el juego: FOOD_* de food.h)
  int16_t x = 0, y = 0; // donde esta en el suelo
  uint32_t t0 = 0;      // inicio de la fase de ahora (en YF_QUEUED: cuando se eligio)
  uint32_t tag = 0;     // de quien es (el juego: el uid del bicho; si cambia, se cancela)
};

// las celdas a las que se llega andando desde `from` (4 vecinos)
inline void yardReachable(const uint8_t *walk, int from, bool *seen) {
  memset(seen, 0, YARD_CELLS);
  if (!yardFree(walk, from)) return;
  uint8_t q[YARD_CELLS];
  int qh = 0, qt = 0;
  q[qt++] = (uint8_t)from;
  seen[from] = true;
  static const int8_t D[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
  while (qh < qt) {
    int c = q[qh++];
    for (auto &d : D) {
      int nx = yardCellCX(c) + d[0], ny = yardCellCY(c) + d[1];
      if (yardCellBlocked(walk, nx, ny)) continue;
      int k = yardCellIdx(nx, ny);
      if (seen[k]) continue;
      seen[k] = true;
      q[qt++] = (uint8_t)k;
    }
  }
}

// v3.3: la celda libre y alcanzable desde (px, py) con el centro en x <= maxX mas cercana al bicho (para que
// se aparte de debajo del menu de la derecha), o -1
inline int yardCellLeftOf(const uint8_t *walk, float px, float py, int maxX) {
  int from = yardCellAt(px, py);
  if (from < 0 || !yardFree(walk, from)) from = yardNearestFreeCell(walk, px, py);
  if (from < 0) return -1;
  bool seen[YARD_CELLS];
  yardReachable(walk, from, seen);
  int best = -1;
  float bd = 1e9f;
  for (int c = 0; c < YARD_CELLS; c++) {
    if (!seen[c]) continue;
    float cx = (float)yardCellCenter(yardCellCX(c)), cy = (float)yardCellCenter(yardCellCY(c));
    if (cx > maxX) continue;
    float d = yardDist(cx, cy, px, py);
    if (d < bd) {
      bd = d;
      best = c;
    }
  }
  return best;
}

// lo que el planificador le pide al bicho mientras hay comida: quieto en el bocado; esperando, nada (se aparta)
inline uint8_t yardFoodHold(const YardFood &f) { return f.phase == YF_EAT ? YH_STAY : YH_NONE; }

inline void yardFoodFace(YardPet &p, const YardFood &f) {
  if (f.x > p.x + 2) p.face = 1;
  else if (f.x < p.x - 2) p.face = -1;
}

inline void yardFoodStartEat(YardFood &f, YardPet &p, uint32_t now) {
  f.phase = YF_EAT;
  f.t0 = now;
  p.mode = YM_STAY;
  p.pathN = p.pathI = 0;
  p.t0 = now;
  yardFoodFace(p, f);
}

// la comida delante del bicho, y se la come ya: a YARD_FOOD_FRONT_DX px hacia donde mira; si ahi no se pisa,
// al otro lado, y si tampoco, a sus pies. El bicho se queda quieto mirandola
inline void yardFoodFront(YardFood &f, YardPet &p, const uint8_t *walk, uint8_t kind, uint32_t now, uint32_t tag) {
  int8_t face = p.face < 0 ? -1 : 1;
  float fx = p.x + face * YARD_FOOD_FRONT_DX;
  if (!yardSpotOk(walk, fx, p.y)) {
    float bx = p.x - face * YARD_FOOD_FRONT_DX;
    fx = yardSpotOk(walk, bx, p.y) ? bx : p.x;
  }
  f.kind = kind;
  f.x = (int16_t)fx;
  f.y = (int16_t)(p.y + 2);  // un pelo por delante de sus pies: se dibuja encima de el
  f.tag = tag;
  yardFoodStartEat(f, p, now);
}

// la eleccion espera a que el bicho deje de apartarse del menu (cuenta como su comida: una a la vez)
inline void yardFoodQueue(YardFood &f, uint8_t kind, uint32_t now, uint32_t tag) {
  f = YardFood{};
  f.phase = YF_QUEUED;
  f.kind = kind;
  f.t0 = now;
  f.tag = tag;
}

// ya se le puede servir lo que espera: no anda y no esta debajo del menu (o no puede apartarse); o lleva
// YARD_FOOD_QUEUE_MAX_MS esperando
inline bool yardFoodReady(const YardFood &f, const YardPet &p, bool underMenu, uint32_t now) {
  if (now - f.t0 >= YARD_FOOD_QUEUE_MAX_MS) return true;
  return p.mode != YM_WALK && (!underMenu || !p.walks);
}

// lo que espera, delante del bicho si ya esta listo. true si se sirvio
inline bool yardFoodServe(YardFood &f, YardPet &p, const uint8_t *walk, bool ready, uint32_t now) {
  if (f.phase != YF_QUEUED || !ready) return false;
  yardFoodFront(f, p, walk, f.kind, now, f.tag);
  return true;
}

// un paso de la comida para el bicho i (despues de yardStep). true = se la acaba de comer: el juego
// aplica ahora la comida (y f vuelve a YF_NONE)
inline bool yardFoodStep(YardFood &f, YardWorld &w, int i, uint32_t now) {
  if (f.phase != YF_EAT) return false;
  YardPet &p = w.p[i];
  yardFoodFace(p, f);
  if (now - f.t0 >= YARD_FOOD_EAT_MS) {
    f.phase = YF_NONE;
    return true;
  }
  return false;
}

// ---------- apartarse del menu del bicho (v3.3) ----------
// El menu de un bicho tapa la derecha del patio (su columna empieza en colX). Mientras esta abierto, el bicho
// que este debajo anda a la celda libre mas cercana a su izquierda (yardCellLeftOf); si algo lo para, lo
// vuelve a intentar cada YARD_ASIDE_RETRY_MS. Apartarse gana a la pose triste; despues, el del menu se queda
// quieto y el otro sigue paseando (yardWander ya no va hasta alli: avoidX)
#define YARD_ASIDE_RETRY_MS 1500
struct YardAside {
  uint32_t retryAt = 0;  // el proximo intento (0: ya)
};

// el bicho con los pies en x y el cuerpo de bw px de ancho queda debajo de la columna que empieza en colX
inline bool yardUnderMenu(float x, int bw, int colX) { return x + bw / 2 > colX; }

// lo que el menu pide al bicho i en este paso, cuando el juego no lo tiene ya dormido, comiendo o quieto por
// otra cosa: base = lo que pediria sin el menu (YH_NONE, o YH_STAY por la pose triste); mine = el menu es suyo
inline uint8_t yardMenuHold(YardWorld &w, const uint8_t *walk, int i, uint8_t base, bool mine, int bw, int colX,
                            YardAside &a, uint32_t now) {
  YardPet &p = w.p[i];
  if (p.mode == YM_GESTURE && (int32_t)(now - p.until) < 0) return mine ? YH_NONE : base;  // la pose acaba
  if (p.walks && yardUnderMenu(p.x, bw, colX)) {
    if (p.mode == YM_WALK) return YH_NONE;  // ya se aparta (o pasea): que siga
    if ((int32_t)(now - a.retryAt) >= 0) {
      a.retryAt = now + YARD_ASIDE_RETRY_MS;
      int c = yardCellLeftOf(walk, p.x, p.y, colX - bw / 2 - 16);
      if (c >= 0 && yardWalkTo(w, walk, i, c, now)) return YH_NONE;
    }
  }
  return mine ? YH_STAY : base;
}
