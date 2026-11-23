#pragma once
// Los dos bichos juntos (v3.22, docs/HOME_AND_PALS.md tarea PALS, FR-PALS.1-7): el director de escenas del patio.
//
// Solo corre con el patio en pantalla (yardTick lo llama justo antes de yardStep), asi que todo lo que pasa aqui
// se ve: nada en el bolsillo ni sin conexion. Mira a los dos bichos de casa (PalsCtx, que rellena el .ino cada
// frame desde Pet) y decide cuando empieza una escena: el saludo al cruzarse, una escena de juego cada 12-28 min
// de patio a la vista, animar al triste, sentarse junto al huevo o junto al que duerme por la tarde, dormir juntos
// sin camas, esperar junto a la comida del pequeno y, rara vez, una rina sin consecuencias. Lleva a los bichos con
// la tarea YT_SCENE (yardDecide no los toca) y al acabar los suelta (YT_NONE, quietos un momento).
//
// Nunca toca Pet: lo que paga (la alegria de una escena de juego o del animo) sale en PalsOut y el .ino lo pide a
// pet.palPlayPerk / palCheerPerk (con sus topes diarios). Los iconos de encima (PalsEmote) son solo del patio: no
// son las emociones de PetLife, que retienen al bicho. Las rinas no pagan ni quitan nada: no hay nada que pagar.
// El azar es el del patio (YardWorld.rng) y el tiempo llega de fuera: es determinista (test/test_pals.cpp).
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "yard.h"
#include "pal_fx.h"
#include "dex.h"
#include "tuning.h"

// las escenas. Las de juego (CHASE..ESCORT) pagan PAL_PLAY_JOY a los dos; CHEER, PAL_CHEER_JOY al triste; las
// demas solo se ven. BERRY, BUMP y TUG son las rinas (como mucho una por PAL_SQUABBLE_GAP_S de patio)
enum : uint8_t {
  PS_NONE = 0,
  PS_GREET,    // se cruzan: quietos, mirandose, un icono
  PS_CHASE,    // juego: uno corre, el otro lo sigue; cambian una vez
  PS_HOP,      // juego: saltos a la vez
  PS_NUZZLE,   // juego: se juntan, se tocan la cabeza, corazones
  PS_TOYPASS,  // juego (con juguete): se lo pasan por turnos
  PS_ESCORT,   // juego (grande y pequeno): el grande acompana despacio al pequeno
  PS_CHEER,    // animar: el contento va junto al triste
  PS_EGGSIT,   // con un huevo en el otro hueco: se sienta a su lado
  PS_SIT,      // por la tarde, junto al que duerme
  PS_SLEEP,    // sin camas: el segundo que se duerme se acerca al primero
  PS_WAIT,     // grande y pequeno: el grande espera junto a la comida del pequeno
  PS_BERRY,    // rina: la misma baya
  PS_BUMP,     // rina: despierta un momento al de la siesta (sigue dormido)
  PS_TUG,      // rina (con juguete): tira y afloja, y luego por turnos
  PS_COUNT
};
// los nombres de la serie (PAL <escena>, FR-PALS.10)
static const char *const PALS_NAME[PS_COUNT] = { "none", "greet",  "chase", "hop",  "nuzzle", "toypass", "escort", "cheer",
                                                 "eggsit", "sit",  "sleep", "wait", "berry",  "bump",    "tug" };

inline bool palsIsPlay(uint8_t s) { return s >= PS_CHASE && s <= PS_ESCORT; }
inline bool palsIsSquabble(uint8_t s) { return s == PS_BERRY || s == PS_BUMP || s == PS_TUG; }
inline bool palsIsFood(uint8_t s) { return s == PS_BERRY || s == PS_WAIT; }
inline int palsSceneByName(const char *n) {
  for (int i = 1; i < PS_COUNT; i++)
    if (!strcmp(n, PALS_NAME[i])) return i;
  return -1;
}

#define PAL_ACT_NONE 0xFF  // el sprite no tiene ese gesto

// lo que el director sabe de un hueco de casa (el .ino lo rellena cada frame)
struct PalsSlot {
  bool pet = false;      // un bicho nacido en casa, con su paseante en el patio
  bool egg = false;      // un huevo en el patio (no el hueco vacio)
  float eggX = 0, eggY = 0;  //   sus pies
  bool asleep = false;
  bool nap = false;      // dormido de siesta (no la noche: nightSlept)
  bool busy = false;     // evoluciona, eclosiona, ceremonia o se muere: no empieza nada (y lo que hay se deja)
  uint8_t hold = YH_NONE;  // lo que el juego pide a su paseante este frame (yardTick: dormir, quieto, el menu...)
  bool sadPose = false;  // ese "quieto" es solo la pose triste (al triste se le puede animar)
  uint8_t food = YF_NONE;  // su comida del patio (yardFood)
  int16_t foodX = 0, foodY = 0;
  uint8_t joy = 0;
  uint16_t level = 0;
  uint8_t stage = 0;     // etapa de evolucion (palsStageOf)
  // los gestos de su sprite (PMD_*, o PAL_ACT_NONE): saltar, el toque (cabezazo, patada), sentarse, contento
  uint8_t actHop = PAL_ACT_NONE, actBump = PAL_ACT_NONE, actSit = PAL_ACT_NONE, actJoy = PAL_ACT_NONE;
};

struct PalsCtx {
  PalsSlot s[2];
  bool modal = false;    // un dialogo, una celebracion, el aviso del viajero, un hallazgo... encima del patio
  int8_t menuSlot = -1;  // el menu de ese bicho esta abierto (-1: ninguno)
  int8_t tilt = 0;       // la placa inclinada: los bichos resbalan
  bool beds = false;     // camas de casa (FR-HOME.1)
  bool toy = false;      // juguete de casa: sus pies
  float toyX = 0, toyY = 0;
  uint8_t hour = 12;
};

// un icono sobre un bicho (solo del patio)
struct PalsEmote {
  uint8_t kind = PE_NONE;
  uint32_t t0 = 0;
  uint16_t ms = 0;
};

// lo que el .ino hace tras el paso: pagar, contar para la medalla, soltar al que anda dormido
struct PalsOut {
  bool play[2] = { false, false };  // escena de juego acabada: pet.palPlayPerk(s) a los dos
  int8_t cheer = -1;                // animo acabado: pet.palCheerPerk(cheer) al triste
  bool seen = false;                // pet.palSceneSeen() (una escena de juego o un animo, vistos hasta el final)
  bool holdFree[2] = { false, false };  // dormido, pero lo lleva el director (dormir juntos): hold = YH_NONE
  uint8_t started = PS_NONE, ended = PS_NONE, aborted = PS_NONE;  // para la serie
  uint8_t refused = PS_NONE;        // la forzada (PAL <escena>) no pudo empezar...
  const char *why = nullptr;        //   ...por esto
};

struct PalsState {
  bool init = false;
  uint8_t scene = PS_NONE, phase = 0;
  uint8_t a = 0, b = 1;            // a = el que empieza (el que anda, el contento, el que corre); b = el otro
  bool own[2] = { false, false };  // sus paseantes los lleva la escena (YT_SCENE)
  uint32_t t0 = 0;                 // inicio de la escena (millis)
  uint32_t until = 0;              // fin de la fase de ahora
  uint32_t next = 0;               // el proximo "re-apuntar" al que sigue a otro
  uint8_t leg = 0;
  int16_t tx = 0, ty = 0;          // el punto de la escena (el juguete, la comida, el huevo)
  // relojes en ms de patio en pantalla (watch solo avanza mientras el patio se ve)
  uint32_t watch = 0, playAt = 0, squabbleAt = 0, greetAt = 0, restAt = 0, rollAt = 0;
  bool wasAsleep[2] = { false, false };
  uint8_t wasFood[2] = { YF_NONE, YF_NONE };
  PalsEmote emo[2];
  int8_t roll = -1;                // con camas: el que se gira hacia el otro, desde rollT0
  uint32_t rollT0 = 0;
  uint8_t force = PS_NONE;         // PAL <escena>: se intenta en el proximo paso
  uint8_t armed = PS_NONE;         // PAL berry / PAL wait sin comida: espera a la proxima comida
  uint8_t lastRefused = PS_NONE;
  const char *lastWhy = nullptr;
  uint16_t count[PS_COUNT] = {};   // escenas empezadas desde el arranque (serie PAL)
};

// ---------- ayudas ----------
// la etapa de evolucion de dex: 0 la primera forma de su linea, 1 la segunda, 2 la tercera. (dex.h solo guarda una
// evolucion por especie: las ramas, como las de Eevee, cuentan como segunda etapa)
inline uint8_t palsStageOf(int16_t dex) {
  if (dex < 1 || dex > DEX_COUNT) return 0;
  int k = DEX_TBL[dex].base;
  if (k < 1 || k == dex) return 0;
  for (uint8_t st = 1; st <= 3; st++) {
    k = DEX_TBL[k].evolvesTo;
    if (!k) break;
    if (k == dex) return st;
  }
  return 1;
}

// grande y pequeno (FR-PALS.6): el hueco del grande (una etapa mas, o PAL_BIG_LEVEL_GAP niveles mas), o -1
inline int palsBigOf(const PalsCtx &c) {
  const PalsSlot &x = c.s[0], &y = c.s[1];
  if (!x.pet || !y.pet) return -1;
  if (x.stage != y.stage) return x.stage > y.stage ? 0 : 1;
  if (x.level >= y.level + PAL_BIG_LEVEL_GAP) return 0;
  if (y.level >= x.level + PAL_BIG_LEVEL_GAP) return 1;
  return -1;
}

inline uint32_t palsGap(YardRng &r, uint32_t minS, uint32_t maxS) {
  return (minS + (uint32_t)r.below((int)(maxS - minS + 1))) * 1000u;
}
inline bool palsReached(uint32_t clock, uint32_t at) { return (int32_t)(clock - at) >= 0; }

// despierto y sin nada que lo tenga (ni el juego, ni la comida, ni una visita o la cama)
inline bool palsFreeSlot(const PalsCtx &c, const YardWorld &w, int i) {
  const PalsSlot &s = c.s[i];
  const YardPet &p = w.p[i];
  return s.pet && p.on && !s.asleep && !s.busy && s.hold == YH_NONE && s.food == YF_NONE &&
         (p.task == YT_NONE || p.task == YT_SCENE);
}
// ...y ademas ahora mismo puede empezar algo: no hace otra cosa (un gesto, la inclinacion)
inline bool palsCanStart(const PalsCtx &c, const YardWorld &w, int i) {
  const YardPet &p = w.p[i];
  return palsFreeSlot(c, w, i) && p.task == YT_NONE && (p.mode == YM_IDLE || p.mode == YM_WALK) && !p.tilting;
}

// lo que impide cualquier escena (FR-PALS.1). foodOwner >= 0: es una escena de comida de ese hueco (su menu abierto
// no la para: la comida se elige en el)
inline const char *palsBlocked(const PalsCtx &c, int foodOwner = -1) {
  if (c.modal) return "modal";
  if (c.tilt) return "tilt";
  if (c.s[0].busy || c.s[1].busy) return "busy";
  if (c.menuSlot >= 0 && !(foodOwner >= 0 && c.menuSlot == foodOwner)) return "menu";
  if (foodOwner < 0 && (c.s[0].food != YF_NONE || c.s[1].food != YF_NONE)) return "food";
  return nullptr;
}

inline void palsEmote(PalsState &st, int i, uint8_t kind, uint32_t now, uint16_t ms) {
  if (i < 0 || i > 1) return;
  st.emo[i].kind = kind;
  st.emo[i].t0 = now;
  st.emo[i].ms = ms;
}

inline void palsFace(YardPet &p, float x) {
  if (x > p.x + 2) p.face = 1;
  else if (x < p.x - 2) p.face = -1;
}

// quieto (sin decidir nada) hasta ms despues
inline void palsStill(YardPet &p, uint32_t now, uint32_t ms) {
  if (p.mode != YM_IDLE) p.t0 = now;
  p.mode = YM_IDLE;
  p.pathN = p.pathI = 0;
  p.loopAct = false;
  p.until = now + ms;
}

// un gesto suyo (sin el, quieto)
inline void palsAct(YardPet &p, uint8_t act, bool loop, uint32_t now, uint32_t ms) {
  if (act == PAL_ACT_NONE) {
    palsStill(p, now, ms);
    return;
  }
  p.mode = YM_GESTURE;
  p.act = act;
  p.loopAct = loop;
  p.t0 = now;
  p.until = now + ms;
  p.pathN = p.pathI = 0;
}

inline void palsTake(PalsState &st, YardWorld &w, int i, uint32_t now) {
  YardPet &p = w.p[i];
  p.task = YT_SCENE;
  p.sceneDist = PAL_NEAR_PX;
  p.scenePct = 0;
  p.arriveMs = 0;
  p.tilting = false;
  p.loopAct = false;
  st.own[i] = true;
  palsStill(p, now, 1000);
}

inline void palsRelease(PalsState &st, YardWorld &w, int i, uint32_t now) {
  if (!st.own[i]) return;
  st.own[i] = false;
  YardPet &p = w.p[i];
  p.sceneDist = p.scenePct = 0;
  if (p.task != YT_SCENE) return;  // (otra cosa ya lo tiene)
  p.task = YT_NONE;
  p.loopAct = false;
  p.pathN = p.pathI = 0;
  p.mode = YM_IDLE;
  p.t0 = now;
  p.until = now + 600;
}

// lo mas lejos que cuenta como "al lado" si no puede acercarse mas que minD
inline int palsBesideMax(int minD) { return minD + 38 > 64 ? minD + 38 : 64; }

// una celda alcanzable desde la del bicho i para quedarse al lado de (tx, ty): su centro a minD-palsBesideMax px, mejor
// en la misma fila y cerca del bicho. avoid = otra celda que no vale (la del otro bicho). -1 si no hay
inline int palsBesideCell(const YardWorld &w, const uint8_t *walk, int i, float tx, float ty, int avoid = -1,
                          int minD = 26) {
  const YardPet &p = w.p[i];
  int from = yardCellAt(p.x, p.y);
  if (from < 0 || !yardFree(walk, from)) from = yardNearestFreeCell(walk, p.x, p.y);
  if (from < 0) return -1;
  bool seen[YARD_CELLS];
  yardReachable(walk, from, seen);
  int best = -1;
  float bs = 1e9f;
  for (int c = 0; c < YARD_CELLS; c++) {
    if (!seen[c] || c == avoid) continue;
    float cx = (float)yardCellCenter(yardCellCX(c)), cy = (float)yardCellCenter(yardCellCY(c));
    float d = yardDist(cx, cy, tx, ty);
    if (d < minD || d > palsBesideMax(minD)) continue;
    float sc = fabsf(cy - ty) + 0.2f * yardDist(cx, cy, p.x, p.y);
    if (sc < bs) {
      bs = sc;
      best = c;
    }
  }
  return best;
}

// el bicho i va al lado de (tx, ty). 1 = ya esta (a PAL_BESIDE_PX o menos), 0 = echo a andar, -1 = no hay como.
// minD: no mas cerca que esto (junto a un huevo o al que duerme, sin pisarlo)
inline int palsGoBeside(YardWorld &w, const uint8_t *walk, int i, float tx, float ty, uint32_t now, int avoid = -1,
                        int minD = 26) {
  YardPet &p = w.p[i];
  float d = yardDist(p.x, p.y, tx, ty);
  if (d <= (minD > 26 ? palsBesideMax(minD) : PAL_BESIDE_PX) && d >= minD) return 1;
  if (!p.walks) return -1;
  int to = palsBesideCell(w, walk, i, tx, ty, avoid, minD);
  if (to < 0) return -1;
  if (to == yardCellAt(p.x, p.y)) return 1;
  return yardWalkTo(w, walk, i, to, now) ? 0 : -1;
}

// una celda alcanzable a entre minG y maxG celdas del bicho i (al azar), sin pasar de avoidX y alejandose del bicho
// away (asi no corre hacia el que lo sigue); -1 si no hay
inline int palsFarCell(YardWorld &w, const uint8_t *walk, int i, int minG, int maxG, int away = -1) {
  const YardPet &p = w.p[i];
  int from = yardCellAt(p.x, p.y);
  if (from < 0 || !yardFree(walk, from)) return -1;
  bool seen[YARD_CELLS];
  yardReachable(walk, from, seen);
  int n = 0;
  auto ok = [&](int c) {
    if (!seen[c]) return false;
    int dx = yardCellCX(c) - yardCellCX(from), dy = yardCellCY(c) - yardCellCY(from), d2 = dx * dx + dy * dy;
    if (d2 < minG * minG || d2 > maxG * maxG || yardCellCenter(yardCellCX(c)) > w.avoidX) return false;
    if (away < 0) return true;
    const YardPet &o = w.p[away];
    float cx = (float)yardCellCenter(yardCellCX(c)), cy = (float)yardCellCenter(yardCellCY(c));
    return yardDist(cx, cy, o.x, o.y) >= yardDist(p.x, p.y, o.x, o.y) + 40;
  };
  for (int c = 0; c < YARD_CELLS; c++) n += ok(c);
  if (!n) return -1;
  int k = w.rng.below(n);
  for (int c = 0; c < YARD_CELLS; c++)
    if (ok(c) && k-- == 0) return c;
  return -1;
}

// el que sigue (i) re-apunta a donde esta el que guia cada PAL_FOLLOW_MS
#define PAL_FOLLOW_MS 600
inline void palsFollow(PalsState &st, YardWorld &w, const uint8_t *walk, int i, int lead, uint32_t now) {
  if (!palsReached(now, st.next)) return;
  st.next = now + PAL_FOLLOW_MS;
  YardPet &p = w.p[i];
  const YardPet &l = w.p[lead];
  if (!p.walks) return;
  int to = yardCellAt(l.x, l.y);
  if (to < 0 || !yardFree(walk, to)) to = yardNearestFreeCell(walk, l.x, l.y);
  int from = yardCellAt(p.x, p.y);
  if (to < 0 || to == from) return;
  bool was = p.mode == YM_WALK;
  uint32_t t0 = p.t0;
  if (yardWalkTo(w, walk, i, to, now) && was) p.t0 = t0;  // (el paso sigue su ciclo)
}

// ---------- empezar y acabar ----------
// los dos de casa estan a la vista (paseantes encendidos)
inline bool palsBothOn(const PalsCtx &c, const YardWorld &w) {
  return c.s[0].pet && c.s[1].pet && w.p[0].on && w.p[1].on;
}

// termina la escena: suelta a los dos; ok = acabo bien (paga las de juego y el animo)
inline void palsEnd(PalsState &st, YardWorld &w, uint32_t now, PalsOut &out, bool ok) {
  uint8_t sc = st.scene;
  for (int i = 0; i < 2; i++) palsRelease(st, w, i, now);
  if (ok) {
    out.ended = sc;
    if (palsIsPlay(sc)) {
      out.play[0] = out.play[1] = true;
      out.seen = true;
    } else if (sc == PS_CHEER) {
      out.cheer = (int8_t)st.b;
      out.seen = true;
    }
  } else {
    out.aborted = sc;
    st.emo[0] = st.emo[1] = PalsEmote{};
  }
  st.scene = PS_NONE;
  st.phase = 0;
  st.restAt = st.watch + PAL_REST_S * 1000u;
  if (sc != PS_EGGSIT && sc != PS_SLEEP) {  // juntos un rato: el saludo puede esperar
    uint32_t g = st.watch + PAL_GREET_COOLDOWN_S * 1000u;
    if ((int32_t)(g - st.greetAt) > 0) st.greetAt = g;
  }
}

// deshace una escena que no llego a arrancar (sin respiro ni aviso: como si no hubiera empezado)
inline void palsUndo(PalsState &st, YardWorld &w, uint32_t now) {
  for (int i = 0; i < 2; i++) palsRelease(st, w, i, now);
  if (st.scene < PS_COUNT && st.count[st.scene]) st.count[st.scene]--;
  st.emo[0] = st.emo[1] = PalsEmote{};
  st.scene = PS_NONE;
  st.phase = 0;
}

inline void palsBegin(PalsState &st, uint8_t scene, int a, int b, uint32_t now) {
  st.scene = scene;
  st.phase = 0;
  st.a = (uint8_t)a;
  st.b = (uint8_t)b;
  st.t0 = now;
  st.until = now + PAL_APPROACH_MS;
  st.next = 0;
  st.leg = 0;
  if (scene < PS_COUNT) st.count[scene]++;
}

// el juguete: las dos celdas a sus lados (a y b). false si no hay
inline bool palsToyCells(const PalsState &st, YardWorld &w, const uint8_t *walk, const PalsCtx &c, int *ca, int *cb) {
  bool seen[YARD_CELLS];
  int fa = yardCellAt(w.p[st.a].x, w.p[st.a].y);
  if (fa < 0 || !yardFree(walk, fa)) return false;
  *ca = yardVisitCell(walk, fa, c.toyX, c.toyY);
  if (*ca < 0) return false;
  yardReachable(walk, fa, seen);
  float ax = (float)yardCellCenter(yardCellCX(*ca)), ay = (float)yardCellCenter(yardCellCY(*ca));
  int best = -1;
  float bd = 1e9f;
  for (int k = 0; k < YARD_CELLS; k++) {  // al otro lado del juguete: lejos de la de a
    if (!seen[k] || k == *ca) continue;
    float kx = (float)yardCellCenter(yardCellCX(k)), ky = (float)yardCellCenter(yardCellCY(k));
    if (yardDist(kx, ky, ax, ay) < 60) continue;
    float d = yardDist(kx, ky, c.toyX, c.toyY) + (fabsf(ky - c.toyY) > 4 ? 12.0f : 0.0f);
    if (d < bd) {
      bd = d;
      best = k;
    }
  }
  *cb = best;
  if (best < 0 || bd >= 90) return false;
  // cada uno a la celda de su lado (que no se crucen)
  auto far = [&](int i, int cell) {
    return yardDist(w.p[i].x, w.p[i].y, (float)yardCellCenter(yardCellCX(cell)), (float)yardCellCenter(yardCellCY(cell)));
  };
  if (far(st.a, *ca) + far(st.b, *cb) > far(st.a, *cb) + far(st.b, *ca)) {
    int t = *ca;
    *ca = *cb;
    *cb = t;
  }
  return true;
}

// intenta empezar la escena sc ahora (las condiciones de la escena y las de FR-PALS.1; no mira las esperas entre
// escenas: eso lo hace palsStep). nullptr si empezo; si no, por que no
inline const char *palsStart(PalsState &st, YardWorld &w, const uint8_t *walk, uint32_t now, const PalsCtx &c,
                             uint8_t sc, PalsOut &out) {
  if (sc == PS_NONE || sc >= PS_COUNT) return "unknown";
  if (st.scene != PS_NONE) return "scene running";
  int big = palsBigOf(c);
  if (sc == PS_WAIT || sc == PS_BERRY) {
    int owner = c.s[0].food != YF_NONE ? 0 : c.s[1].food != YF_NONE ? 1 : -1;
    if (owner < 0) return "no food";
    int o = owner ^ 1;
    if (const char *r = palsBlocked(c, owner)) return r;
    if (!c.s[owner].pet || !w.p[owner].on) return "no pet";
    if (!palsCanStart(c, w, o)) return "other busy";
    if (sc == PS_WAIT && big != o) return "not big & small";
    palsBegin(st, sc, o, owner, now);
    st.tx = c.s[owner].foodX;
    st.ty = c.s[owner].foodY;
    palsTake(st, w, o, now);
    YardPet &p = w.p[o];
    if (sc == PS_BERRY) {
      p.scenePct = PAL_CHASE_PCT;  // trota
      palsEmote(st, o, PE_EXCLAIM, now, 1200);
    } else {
      palsEmote(st, o, PE_HEART, now, 1500);
    }
    if (palsGoBeside(w, walk, o, st.tx, st.ty, now, -1, sc == PS_WAIT ? PAL_SIDE_PX : 26) < 0) palsFace(p, st.tx);
    out.started = sc;
    return nullptr;
  }
  if (const char *r = palsBlocked(c)) return r;
  switch (sc) {
    case PS_EGGSIT: {
      int pet = c.s[0].pet && c.s[1].egg ? 0 : c.s[1].pet && c.s[0].egg ? 1 : -1;
      if (pet < 0) return "no egg";
      if (!palsCanStart(c, w, pet)) return "pet busy";
      if (!w.p[pet].walks) return "can't walk";
      int egg = pet ^ 1;
      palsBegin(st, sc, pet, egg, now);
      st.tx = (int16_t)c.s[egg].eggX;
      st.ty = (int16_t)c.s[egg].eggY;
      palsTake(st, w, pet, now);
      if (palsGoBeside(w, walk, pet, st.tx, st.ty, now, -1, PAL_SIDE_PX) < 0) {
        palsUndo(st, w, now);
        return "no room";
      }
      out.started = sc;
      return nullptr;
    }
    case PS_SLEEP: {
      if (c.beds) return "beds";
      if (!palsBothOn(c, w) || !c.s[0].asleep || !c.s[1].asleep) return "not both asleep";
      if (c.s[0].food != YF_NONE || c.s[1].food != YF_NONE) return "food";
      // el que anda: el que se acaba de dormir (palsStep lo pone en st.b); forzado, el hueco 1
      int b = st.b <= 1 ? st.b : 1, a = b ^ 1;
      if (!w.p[b].walks || w.p[b].task != YT_NONE) return "can't walk";
      palsBegin(st, sc, a, b, now);
      palsTake(st, w, b, now);
      out.holdFree[b] = true;
      int r = palsGoBeside(w, walk, b, w.p[a].x, w.p[a].y, now);
      if (r != 0) {  // ya al lado, o no hay camino: se queda donde esta
        palsUndo(st, w, now);
        out.holdFree[b] = false;
        return r > 0 ? "already together" : "no room";
      }
      out.started = sc;
      return nullptr;
    }
    case PS_SIT:
    case PS_BUMP: {
      int a = -1;
      for (int i = 0; i < 2; i++)
        if (c.s[i].pet && c.s[i ^ 1].pet && c.s[i ^ 1].asleep && palsCanStart(c, w, i)) a = i;
      if (a < 0) return "nobody asleep";
      int b = a ^ 1;
      if (!w.p[b].on) return "no pet";
      if (sc == PS_SIT && c.hour < EVENING_HOUR) return "not evening";
      if (sc == PS_BUMP && (!c.s[b].nap || c.hour < WAKE_HOUR || c.hour >= EVENING_HOUR)) return "not a day nap";
      if (!w.p[a].walks) return "can't walk";
      palsBegin(st, sc, a, b, now);
      palsTake(st, w, a, now);
      if (palsGoBeside(w, walk, a, w.p[b].x, w.p[b].y, now, -1, sc == PS_SIT ? PAL_SIDE_PX : 26) < 0) {
        palsUndo(st, w, now);
        return "no room";
      }
      out.started = sc;
      return nullptr;
    }
    default:
      break;
  }
  // de dos despiertos
  if (!palsBothOn(c, w)) return "needs two pets";
  if (sc == PS_CHEER) {
    int sad = -1;
    for (int i = 0; i < 2; i++)
      if (c.s[i].joy < PAL_CHEER_BELOW && c.s[i ^ 1].joy >= PAL_CHEER_FROM) sad = i;
    if (sad < 0) return "nobody sad";
    int a = sad ^ 1;
    const PalsSlot &ss = c.s[sad];
    if (ss.asleep || ss.food != YF_NONE || (ss.hold != YH_NONE && !ss.sadPose)) return "sad one busy";
    if (!palsCanStart(c, w, a)) return "happy one busy";
    palsBegin(st, sc, a, sad, now);
    palsTake(st, w, a, now);
    if (ss.hold == YH_NONE && w.p[sad].task == YT_NONE) palsTake(st, w, sad, now);  // (con la pose triste no se le toca)
    if (palsGoBeside(w, walk, a, w.p[sad].x, w.p[sad].y, now) < 0) {
      palsUndo(st, w, now);
      return "no room";
    }
    out.started = sc;
    return nullptr;
  }
  if (!palsCanStart(c, w, 0) || !palsCanStart(c, w, 1)) return "pets busy";
  if ((sc == PS_TOYPASS || sc == PS_TUG) && !c.toy) return "no toy";
  if (sc == PS_ESCORT && big < 0) return "not big & small";
  int a = w.rng.below(2), b = a ^ 1;
  if (sc == PS_ESCORT) {  // a = el pequeno (guia), b = el grande (acompana)
    b = big;
    a = big ^ 1;
  }
  palsBegin(st, sc, a, b, now);
  palsTake(st, w, a, now);
  palsTake(st, w, b, now);
  YardPet &pa = w.p[a], &pb = w.p[b];
  switch (sc) {
    case PS_GREET: {
      static const uint8_t K[3] = { PE_HEART, PE_NOTE, PE_EXCLAIM };
      palsFace(pa, pb.x);
      palsFace(pb, pa.x);
      palsStill(pa, now, PAL_GREET_MS + 500);
      palsStill(pb, now, PAL_GREET_MS + 500);
      palsEmote(st, a, K[w.rng.below(3)], now, PAL_GREET_MS);
      palsEmote(st, b, K[w.rng.below(3)], now, PAL_GREET_MS);
      st.phase = 1;
      st.until = now + PAL_GREET_MS;
      break;
    }
    case PS_HOP:
      if (yardDist(pa.x, pa.y, pb.x, pb.y) > 2 * PAL_BESIDE_PX && palsGoBeside(w, walk, a, pb.x, pb.y, now) == 0) break;
      st.phase = 1;  // ya cerca (o sin camino): saltan donde estan
      st.until = now;
      break;
    case PS_NUZZLE:
      if (palsGoBeside(w, walk, a, pb.x, pb.y, now) < 0) {
        palsUndo(st, w, now);
        return "no room";
      }
      break;
    case PS_CHASE:
    case PS_ESCORT:
      st.phase = 1;
      st.until = now;  // la primera carrera empieza en el paso
      break;
    case PS_TOYPASS:
    case PS_TUG: {
      int ca, cb;
      if (!palsToyCells(st, w, walk, c, &ca, &cb)) {
        palsUndo(st, w, now);
        return "no room";
      }
      st.tx = (int16_t)c.toyX;
      st.ty = (int16_t)c.toyY;
      if (ca != yardCellAt(pa.x, pa.y)) yardWalkTo(w, walk, a, ca, now);
      if (cb != yardCellAt(pb.x, pb.y)) yardWalkTo(w, walk, b, cb, now);
      break;
    }
    default:
      palsUndo(st, w, now);
      return "unknown";
  }
  out.started = sc;
  return nullptr;
}

// ---------- un paso de la escena en curso ----------
// lo que la para (ademas de lo de palsBlocked): un bicho suyo que otra cosa se llevo, se durmio, comio...
inline bool palsKeepGoing(const PalsState &st, const YardWorld &w, const PalsCtx &c) {
  uint8_t sc = st.scene;
  int owner = palsIsFood(sc) ? st.b : -1;
  if (palsBlocked(c, owner)) return false;
  for (int i = 0; i < 2; i++) {
    if (!st.own[i]) continue;
    const YardPet &p = w.p[i];
    if (!p.on || p.task != YT_SCENE || !c.s[i].pet) return false;
    if (sc == PS_SLEEP) {
      if (!c.s[i].asleep || c.s[i].food != YF_NONE) return false;
    } else if (!palsFreeSlot(c, w, i)) {
      return false;
    }
  }
  const PalsSlot &b = c.s[st.b];
  switch (sc) {
    case PS_EGGSIT: return b.egg;
    case PS_SIT: return b.pet && b.asleep;
    case PS_BUMP: return b.pet && b.asleep && b.nap;
    case PS_SLEEP: return c.s[st.a].pet && c.s[st.a].asleep && !c.beds;
    case PS_CHEER: return b.pet && !b.asleep && b.food == YF_NONE && (b.hold == YH_NONE || b.sadPose);
    case PS_WAIT:
    case PS_BERRY: return b.pet;
    default: return true;
  }
}

// el que estaba andando hacia el otro llego (o se paro): true si esta al lado
inline bool palsArrived(const YardWorld &w, int i, float tx, float ty, int maxD = PAL_BESIDE_PX) {
  return w.p[i].mode != YM_WALK && yardDist(w.p[i].x, w.p[i].y, tx, ty) <= maxD;
}

inline void palsSceneStep(PalsState &st, YardWorld &w, const uint8_t *walk, uint32_t now, const PalsCtx &c, PalsOut &out) {
  int a = st.a, b = st.b;
  YardPet &pa = w.p[a], &pb = w.p[b];
  const PalsSlot &sa = c.s[a], &sb = c.s[b];
  bool due = palsReached(now, st.until);
  bool walking = pa.mode == YM_WALK;
  // el que espera quieto mira al que viene (y no decide nada mientras)
  auto waitFor = [&](int i, float x) {
    if (!st.own[i]) return;
    YardPet &p = w.p[i];
    if (p.mode == YM_IDLE) {
      palsFace(p, x);
      p.until = now + 1000;
    }
  };
  auto approachFailed = [&](float tx, float ty, int maxD = PAL_BESIDE_PX) {  // llego mal o se le acabo el tiempo
    return (!walking && !palsArrived(w, a, tx, ty, maxD)) || due;
  };
  switch (st.scene) {
    case PS_GREET:
      if (due) palsEnd(st, w, now, out, true);
      return;

    case PS_NUZZLE:
    case PS_CHEER:
      if (st.phase == 0) {  // a va junto a b
        waitFor(b, pa.x);
        if (palsArrived(w, a, pb.x, pb.y)) {
          palsFace(pa, pb.x);
          if (st.own[b]) palsFace(pb, pa.x);
          palsAct(pa, sa.actBump, false, now, 1200);
          if (st.own[b]) palsAct(pb, st.scene == PS_NUZZLE ? sb.actBump : PAL_ACT_NONE, false, now, 1200);
          palsEmote(st, a, PE_HEART, now, 2400);
          palsEmote(st, b, PE_HEART, now, 2400);
          st.phase = 1;
          st.until = now + 1200;
        } else if (approachFailed(pb.x, pb.y)) {
          palsEnd(st, w, now, out, false);
        }
        return;
      }
      if (st.phase == 1 && due) {  // los corazones (el animado, chispas de su tipo)
        uint32_t rest = (st.scene == PS_NUZZLE ? PAL_NUZZLE_MS : PAL_CHEER_MS) - 1200;
        palsStill(pa, now, rest + 500);
        if (st.own[b]) palsStill(pb, now, rest + 500);
        if (st.scene == PS_CHEER) {
          palsEmote(st, b, PE_SPARKLE, now, (uint16_t)rest);
          palsEmote(st, a, PE_NOTE, now, (uint16_t)rest);
        }
        st.phase = 2;
        st.until = now + rest;
        return;
      }
      if (st.phase == 2 && due) palsEnd(st, w, now, out, true);
      return;

    case PS_HOP:
      if (st.phase == 0) {
        waitFor(b, pa.x);
        if (!walking || due) {
          st.phase = 1;
          st.until = now;
          due = true;
        } else {
          return;
        }
      }
      if (st.phase == 1 && due) {  // a la vez: el mismo t0
        palsFace(pa, pb.x);
        palsFace(pb, pa.x);
        palsAct(pa, sa.actHop, true, now, PAL_HOP_MS + 500);
        palsAct(pb, sb.actHop, true, now, PAL_HOP_MS + 500);
        palsEmote(st, a, PE_NOTE, now, PAL_HOP_MS);
        palsEmote(st, b, PE_NOTE, now, PAL_HOP_MS);
        st.phase = 2;
        st.until = now + PAL_HOP_MS;
        return;
      }
      if (st.phase == 2 && due) palsEnd(st, w, now, out, true);
      return;

    case PS_CHASE:
    case PS_ESCORT: {
      bool chase = st.scene == PS_CHASE;
      uint32_t total = chase ? PAL_CHASE_MS : PAL_ESCORT_MS;
      int run = chase && st.leg == 1 ? b : a, fol = run ^ 1;
      YardPet &pr = w.p[run], &pf = w.p[fol];
      if (st.phase == 1 && due) {  // una carrera: el que corre a una celda lejos, el otro detras
        int to = palsFarCell(w, walk, run, chase ? 4 : 3, chase ? 8 : 5, fol);
        if (to < 0) to = palsFarCell(w, walk, run, chase ? 4 : 3, chase ? 8 : 5);  // (arrinconado: a donde sea)
        if (to < 0 || !yardWalkTo(w, walk, run, to, now)) {
          palsEnd(st, w, now, out, st.leg > 0);
          return;
        }
        uint8_t pct = chase ? PAL_CHASE_PCT : PAL_ESCORT_PCT;
        pr.scenePct = pf.scenePct = pct;
        pf.sceneDist = chase ? 40 : 48;
        st.next = now + (chase ? 400 : 200);  // (sale un poco despues)
        palsEmote(st, run, chase ? PE_NOTE : PE_NONE, now, 1500);
        palsEmote(st, fol, chase ? PE_EXCLAIM : PE_HEART, now, 1500);
        st.phase = 2;
        st.until = now + (chase ? total / 2 : total);
        return;
      }
      if (st.phase == 2) {
        palsFollow(st, w, walk, fol, run, now);
        if (pr.mode != YM_WALK || due) {
          if (pr.mode == YM_WALK) palsStill(pr, now, 1000);
          if (chase && st.leg == 0) {  // cambian los papeles
            st.leg = 1;
            st.phase = 1;
            st.until = now;
            palsStill(pf, now, 1000);
            return;
          }
          palsStill(pr, now, 1500);
          palsStill(pf, now, 1500);
          palsFace(pr, pf.x);
          palsFace(pf, pr.x);
          palsEmote(st, a, PE_NOTE, now, 1200);
          palsEmote(st, b, PE_NOTE, now, 1200);
          st.phase = 3;
          st.until = now + 1200;
        }
        return;
      }
      if (st.phase == 3 && due) palsEnd(st, w, now, out, true);
      return;
    }

    case PS_TOYPASS:
    case PS_TUG: {
      bool tug = st.scene == PS_TUG;
      if (st.phase == 0) {  // los dos van al juguete
        if (pa.mode == YM_WALK || pb.mode == YM_WALK) {
          if (!due) return;
        }
        if (yardDist(pa.x, pa.y, st.tx, st.ty) > 90 || yardDist(pb.x, pb.y, st.tx, st.ty) > 90) {
          palsEnd(st, w, now, out, false);
          return;
        }
        palsFace(pa, st.tx);
        palsFace(pb, st.tx);
        if (tug) {  // tira y afloja: los dos a la vez, enfurrunados
          palsAct(pa, sa.actBump, true, now, PAL_TUG_MS + 500);
          palsAct(pb, sb.actBump, true, now, PAL_TUG_MS + 500);
          palsEmote(st, a, PE_POUT, now, PAL_TUG_MS);
          palsEmote(st, b, PE_POUT, now, PAL_TUG_MS);
          st.until = now + PAL_TUG_MS;
        } else {
          st.until = now;
        }
        st.phase = 1;
        st.leg = 0;
        st.t0 = now;
        return;
      }
      if (tug && st.phase == 1) {
        if (!due) return;
        // las paces (enseguida, dentro de PAL_MAKEUP_S) y luego por turnos
        palsEmote(st, a, PE_HEART, now, 1500);
        palsEmote(st, b, PE_HEART, now, 1500);
        palsStill(pa, now, 2000);
        palsStill(pb, now, 2000);
        st.phase = 2;
        st.leg = 0;
        st.until = now + 600;
        st.t0 = now;
        return;
      }
      // turnos: uno juega (salta o patea) y el otro mira; con el pase, PAL_TOY_MS en total
      uint32_t turn = tug ? PAL_TURN_MS : 1000;
      uint8_t legs = tug ? 2 : (uint8_t)(PAL_TOY_MS / 1000);
      if (!due) return;
      if (st.leg >= legs) {
        palsEnd(st, w, now, out, true);
        return;
      }
      int who = (st.leg & 1) ? b : a, oth = who ^ 1;
      palsAct(w.p[who], c.s[who].actHop, !tug ? false : true, now, turn + 300);
      palsFace(w.p[who], st.tx);
      palsStill(w.p[oth], now, turn + 500);
      palsFace(w.p[oth], st.tx);
      if (!tug) palsEmote(st, who, PE_NOTE, now, (uint16_t)turn);
      st.leg++;
      st.until = now + turn;
      return;
    }

    case PS_EGGSIT:
    case PS_SIT:
    case PS_BUMP: {
      float tx = st.scene == PS_EGGSIT ? st.tx : pb.x, ty = st.scene == PS_EGGSIT ? st.ty : pb.y;
      int maxD = st.scene == PS_BUMP ? PAL_BESIDE_PX : palsBesideMax(PAL_SIDE_PX) + 8;
      if (st.phase == 0) {
        if (palsArrived(w, a, tx, ty, maxD)) {
          palsFace(pa, tx);
          if (st.scene == PS_BUMP) {  // el toque; el otro, nube gris un momento, y sigue durmiendo
            palsAct(pa, sa.actBump, false, now, 900);
            palsEmote(st, b, PE_GRUMPY, now, PAL_BUMP_MS);
            st.until = now + PAL_BUMP_MS;
          } else {
            uint32_t ms = st.scene == PS_EGGSIT ? PAL_EGGSIT_MS : PAL_SIT_MS;
            palsAct(pa, sa.actSit, true, now, ms + 500);
            palsEmote(st, a, PE_HEART, now, 1500);
            st.until = now + ms;
          }
          st.phase = 1;
        } else if (approachFailed(tx, ty, maxD)) {
          palsEnd(st, w, now, out, false);
        }
        return;
      }
      if (st.scene == PS_BUMP && st.phase == 1 && due) {  // "..." y las paces
        palsStill(pa, now, 2000);
        palsEmote(st, a, PE_DOTS, now, 900);
        st.phase = 2;
        st.until = now + 900;
        return;
      }
      if (st.scene == PS_BUMP && st.phase == 2 && due) {
        palsEmote(st, a, PE_HEART, now, 1400);
        palsEmote(st, b, PE_HEART, now, 1400);
        st.phase = 3;
        st.until = now + 1400;
        return;
      }
      if (due) palsEnd(st, w, now, out, true);
      return;
    }

    case PS_SLEEP:
      out.holdFree[b] = true;  // el que se acaba de dormir aun anda (dormido: solo se ve)
      if (pb.mode != YM_WALK || due) {
        palsFace(pb, pa.x);
        palsEmote(st, b, PE_HEART, now, 1500);
        out.holdFree[b] = false;
        palsEnd(st, w, now, out, true);
      }
      return;

    case PS_WAIT:
    case PS_BERRY: {
      bool berry = st.scene == PS_BERRY;
      bool food = sb.food != YF_NONE;
      if (st.phase == 0) {  // va hacia la comida (o junto al pequeno)
        if (!food && !st.leg) {  // ya se la comio: llega como mucho a tiempo para las paces (PAL_MAKEUP_S)
          st.leg = 1;
          uint32_t cap = now + PAL_MAKEUP_S * 1000u - 1600;
          if ((int32_t)(cap - st.until) < 0) st.until = cap;
          due = palsReached(now, st.until);
        }
        if (walking && !due) return;
        if (walking) palsStill(pa, now, 1000);
        palsFace(pa, berry ? st.tx : pb.x);
        if (berry && food) {  // llego a tiempo: se miran enfurrunados
          palsEmote(st, a, PE_POUT, now, 2000);
          palsEmote(st, b, PE_POUT, now, 2000);
        }
        st.phase = 1;
        st.until = food ? now + PAL_APPROACH_MS : now;
        return;
      }
      if (st.phase == 1) {  // espera a que se la coma
        waitFor(a, berry ? st.tx : pb.x);
        if (food && !due) return;
        if (berry) {
          palsEmote(st, a, PE_DOTS, now, 1000);
          st.emo[b] = PalsEmote{};
        } else {
          palsEmote(st, a, PE_NOTE, now, 1200);
        }
        palsStill(pa, now, 2500);
        st.phase = 2;
        st.until = now + (berry ? 1000 : 1200);
        return;
      }
      if (st.phase == 2 && due) {
        if (berry) {  // las paces
          palsEmote(st, a, PE_HEART, now, 1400);
          palsEmote(st, b, PE_HEART, now, 1400);
          st.phase = 3;
          st.until = now + 1400;
        } else {
          palsEnd(st, w, now, out, true);
        }
        return;
      }
      if (st.phase == 3 && due) palsEnd(st, w, now, out, true);
      return;
    }
    default:
      palsEnd(st, w, now, out, false);
  }
}

// deja todo (un toque, el menu, otra pantalla): suelta a los dos y borra los iconos
inline void palsCancel(PalsState &st, YardWorld &w, uint32_t now, PalsOut *out = nullptr) {
  if (st.scene == PS_NONE) return;
  PalsOut tmp;
  palsEnd(st, w, now, out ? *out : tmp, false);
}

// pide una escena para el proximo paso (serie PAL <escena>): sin esperar los huecos, pero con todas las reglas
inline void palsForce(PalsState &st, uint8_t scene) { st.force = scene; }

// ---------- el paso del director (yardTick, antes de yardStep) ----------
// dt: lo que avanzo el patio (ya acotado). Con w.bedSnap (el patio se acaba de abrir) no queda ninguna escena
inline void palsStep(PalsState &st, YardWorld &w, const uint8_t *walk, uint32_t now, uint32_t dt, const PalsCtx &c,
                     PalsOut &out) {
  out = PalsOut{};
  if (!st.init) {
    st.init = true;
    st.watch = 0;
    st.playAt = palsGap(w.rng, PAL_SCENE_GAP_MIN_S, PAL_SCENE_GAP_MAX_S);
    st.squabbleAt = palsGap(w.rng, PAL_SQUABBLE_GAP_S, PAL_SQUABBLE_GAP_S * 3 / 2);
    st.greetAt = st.restAt = 0;
    st.rollAt = PAL_ROLL_EVERY_S * 1000u;
  }
  if (w.bedSnap) {  // el patio se acaba de abrir (o vuelve de otra pantalla): nada a medias
    palsCancel(st, w, now, &out);
    st.emo[0] = st.emo[1] = PalsEmote{};
    st.roll = -1;
    for (int i = 0; i < 2; i++) {
      st.wasAsleep[i] = c.s[i].asleep;
      st.wasFood[i] = c.s[i].food;
    }
    return;
  }
  st.watch += dt;
  for (PalsEmote &e : st.emo)
    if (e.kind != PE_NONE && now - e.t0 >= e.ms) e = PalsEmote{};
  if (st.roll >= 0 && now - st.rollT0 >= PAL_ROLL_MS) st.roll = -1;
  bool fellAsleep[2], newFood[2];
  for (int i = 0; i < 2; i++) {
    fellAsleep[i] = c.s[i].pet && c.s[i].asleep && !st.wasAsleep[i];
    newFood[i] = c.s[i].food != YF_NONE && st.wasFood[i] == YF_NONE;
    st.wasAsleep[i] = c.s[i].asleep;
    st.wasFood[i] = c.s[i].food;
  }
  // la escena en curso
  if (st.scene != PS_NONE) {
    if (!palsKeepGoing(st, w, c)) palsEnd(st, w, now, out, false);
    else palsSceneStep(st, w, walk, now, c, out);
    if (st.scene != PS_NONE) return;
  }
  // forzada por la serie
  if (st.force != PS_NONE) {
    uint8_t f = st.force;
    st.force = PS_NONE;
    if (f == PS_SLEEP) st.b = (fellAsleep[0] && !fellAsleep[1]) ? 0 : 1;
    const char *why = palsStart(st, w, walk, now, c, f, out);
    if (why && palsIsFood(f) && !strcmp(why, "no food")) {  // espera a la proxima comida (sin mirar la rina)
      st.armed = f;
      why = "armed: feed a pet";
    }
    if (why) {
      out.refused = st.lastRefused = f;
      out.why = st.lastWhy = why;
    }
    return;
  }
  // dormir juntos (sin camas): el segundo que se duerme, con el primero ya dormido
  for (int i = 0; i < 2; i++)
    if (fellAsleep[i] && !c.beds && c.s[i ^ 1].pet && c.s[i ^ 1].asleep && !fellAsleep[i ^ 1]) {
      st.b = (uint8_t)i;
      if (!palsStart(st, w, walk, now, c, PS_SLEEP, out)) return;
    }
  // la comida: grande y pequeno, el grande espera a su lado; si no, la misma baya (rina, si toca)
  for (int i = 0; i < 2; i++) {
    if (!newFood[i]) continue;
    if (st.armed != PS_NONE) {  // la forzada esperaba esta comida
      uint8_t f = st.armed;
      st.armed = PS_NONE;
      const char *why = palsStart(st, w, walk, now, c, f, out);
      if (!why) return;
      out.refused = st.lastRefused = f;
      out.why = st.lastWhy = why;
      continue;
    }
    int big = palsBigOf(c);
    if (big == (i ^ 1)) {
      if (!palsStart(st, w, walk, now, c, PS_WAIT, out)) return;
    } else if (palsReached(st.watch, st.squabbleAt) && palsReached(st.watch, st.restAt)) {
      if (!palsStart(st, w, walk, now, c, PS_BERRY, out)) {
        st.squabbleAt = st.watch + palsGap(w.rng, PAL_SQUABBLE_GAP_S, PAL_SQUABBLE_GAP_S * 3 / 2);
        return;
      }
    }
  }
  if (!palsReached(st.watch, st.restAt)) return;
  // con camas y los dos dormidos en ellas: cada tanto uno se gira hacia el otro (solo se ve)
  if (c.beds && w.p[0].task == YT_BED && w.p[1].task == YT_BED && c.s[0].asleep && c.s[1].asleep) {
    if (palsReached(st.watch, st.rollAt)) {
      st.roll = (int8_t)w.rng.below(2);
      st.rollT0 = now;
      st.rollAt = st.watch + PAL_ROLL_EVERY_S * 1000u;
      palsEmote(st, st.roll, PE_HEART, now, PAL_ROLL_MS);
    }
  } else {
    st.rollAt = st.watch + PAL_ROLL_EVERY_S * 1000u;
  }
  // la escena de la hora: animar (antes que nada), una de juego, el huevo o sentarse junto al que duerme
  if (palsReached(st.watch, st.playAt) && !palsBlocked(c)) {
    bool ok = !palsStart(st, w, walk, now, c, PS_CHEER, out);
    if (!ok && palsCanStart(c, w, 0) && palsCanStart(c, w, 1)) {
      uint8_t cand[5];
      uint8_t wt[5];
      int n = 0, sum = 0;
      static const uint8_t PLAY[5] = { PS_CHASE, PS_HOP, PS_NUZZLE, PS_TOYPASS, PS_ESCORT };
      for (int k = 0; k < 5; k++) {
        if (PLAY[k] == PS_TOYPASS && !c.toy) continue;
        if (PLAY[k] == PS_ESCORT && palsBigOf(c) < 0) continue;
        cand[n] = PLAY[k];
        wt[n] = PAL_PLAY_W[k];
        sum += wt[n++];
      }
      int r = w.rng.below(sum);
      for (int k = 0; k < n; k++) {
        if (r < wt[k]) {
          ok = !palsStart(st, w, walk, now, c, cand[k], out);
          break;
        }
        r -= wt[k];
      }
    }
    if (!ok) ok = !palsStart(st, w, walk, now, c, PS_EGGSIT, out);
    if (!ok) ok = !palsStart(st, w, walk, now, c, PS_SIT, out);
    if (ok) {
      st.playAt = st.watch + palsGap(w.rng, PAL_SCENE_GAP_MIN_S, PAL_SCENE_GAP_MAX_S);
      return;
    }
  }
  // una rina (la de la siesta o la del juguete)
  if (palsReached(st.watch, st.squabbleAt) && !palsBlocked(c)) {
    uint8_t first = w.rng.below(2) ? PS_BUMP : PS_TUG, second = first == PS_BUMP ? PS_TUG : PS_BUMP;
    if (!palsStart(st, w, walk, now, c, first, out) || !palsStart(st, w, walk, now, c, second, out)) {
      st.squabbleAt = st.watch + palsGap(w.rng, PAL_SQUABBLE_GAP_S, PAL_SQUABBLE_GAP_S * 3 / 2);
      return;
    }
  }
  // el saludo: se cruzan andando
  if (palsReached(st.watch, st.greetAt) && palsBothOn(c, w) && palsCanStart(c, w, 0) && palsCanStart(c, w, 1) &&
      (w.p[0].mode == YM_WALK || w.p[1].mode == YM_WALK) &&
      yardDist(w.p[0].x, w.p[0].y, w.p[1].x, w.p[1].y) <= PAL_GREET_DIST && !palsBlocked(c)) {
    if (!palsStart(st, w, walk, now, c, PS_GREET, out)) st.greetAt = st.watch + PAL_GREET_COOLDOWN_S * 1000u;
  }
}

// ---------- para el dibujo ----------
// con camas: cuanto se gira el bicho i hacia el otro ahora (px, con signo)
inline int palsRollDx(const PalsState &st, const YardWorld &w, int i, uint32_t now) {
  if (st.roll != i || i > 1) return 0;
  uint32_t t = now - st.rollT0;
  if (t >= PAL_ROLL_MS) return 0;
  float k = sinf(3.14159265f * (float)t / PAL_ROLL_MS);
  int dir = w.p[i ^ 1].x > w.p[i].x ? 1 : -1;
  return (int)lroundf(dir * PAL_ROLL_PX * k);
}

// los dos duermen juntos (en sus camas o uno al lado del otro): un solo globo "Zzz" para los dos
inline bool palsZzzTogether(const YardWorld &w) {
  const YardPet &a = w.p[0], &b = w.p[1];
  return a.on && b.on && a.mode == YM_SLEEP && b.mode == YM_SLEEP && yardDist(a.x, a.y, b.x, b.y) <= 2 * PAL_BESIDE_PX;
}

// el juguete esta en juego en una escena (bota)
inline bool palsToyBusy(const PalsState &st, const YardWorld &w) {
  if (st.scene != PS_TOYPASS && st.scene != PS_TUG) return false;
  return st.phase >= 1 && (w.p[st.a].mode == YM_GESTURE || w.p[st.b].mode == YM_GESTURE);
}
