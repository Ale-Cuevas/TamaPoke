#pragma once
// Encuentros salvajes (v2.2): de vez en cuando algo se mueve en el patio (un arbusto, una roca o un
// monton de arena, segun el mapa). Tocarlo saca al bicho salvaje; se le planta cara con uno de los
// tuyos y, si gana, una Poke Ball lo captura como un huevo que nace a nivel 1 (su forma base).
// Logica pura, sin Arduino (test/test_encounter.cpp): las probabilidades del dia, la hora de la tirada
// base, el nivel, la especie y lo que dura en el patio. Pet (pet.cpp) guarda el estado y llama aqui.
//
// El dia: como mucho UN encuentro por dia natural, con tres tiradas que se suman:
//   - la base: un 10 % a una hora al azar entre las 9:00 y las 20:00 (la misma para ese dia, sale del
//     numero de dia: un reinicio no la cambia);
//   - pasear: al llegar a ENC_WALK_STEPS pasos, otra tirada que lleva el total del dia al 25 %
//     (1 - 0,9 * (1 - p) = 0,25  ->  p = 1 - 0,75 / 0,9 = 16,7 %);
//   - viajar: cada viaje que vuelve tira un 3 % por hora de viaje (1 h: 3 %, 3 h: 9 %, 8 h: 24 %).
// Una vez sale uno, ya no se tira mas ese dia. Se queda ENC_LIFETIME_MIN minutos en el patio.
#include <stdint.h>
#include "dex.h"
#include "battle.h"  // battleRand, evolveForLevel
#include "stage.h"   // STAGES: el nivel sale del ultimo estadio vencido
#include "tuning.h"  // las probabilidades, la hora, lo que dura y el nivel (ENC_*): se afinan alli

// lo que se mueve en el patio, segun el mapa
enum : uint8_t { ENC_OBJ_BUSH = 0, ENC_OBJ_ROCK, ENC_OBJ_SAND };

// las tiradas del dia (la mascara que guarda Pet)
enum : uint8_t { ENC_ROLL_BASE = 1, ENC_ROLL_WALK = 2, ENC_ROLL_SPAWNED = 0x80 };

// las especies que solo salen aqui (dex.h: R_ENCUENTRO) y su peso dentro del 5 %
struct EncExclusive {
  int16_t dex;
  uint8_t w;
};
static const EncExclusive ENC_EXCL[] = { { 138, 24 }, { 140, 24 }, { 142, 24 }, { 147, 24 }, { 151, 4 } };
#define ENC_EXCL_N (sizeof(ENC_EXCL) / sizeof(ENC_EXCL[0]))

// el bicho salvaje tal cual sale de la tirada
struct EncWild {
  int16_t dex = 0;
  uint8_t level = 1;
  uint8_t tAtk = 0, tDef = 0, tSpe = 0;
  bool exclusive = false;
};

static inline bool encHit(uint16_t permil, uint32_t &rng) { return (battleRand(rng) % 1000) < permil; }

// la tirada de un viaje de `hours` horas (por mil)
static inline uint16_t encTravelPermil(uint16_t hours) {
  uint32_t p = (uint32_t)hours * ENC_TRAVEL_PERMIL_H;
  return p > 1000 ? 1000 : (uint16_t)p;
}

// el minuto del dia (ENC_FIRST_MIN..ENC_LAST_MIN) de la tirada base de ese dia: sale del numero de dia
static inline uint16_t encBaseMinute(uint32_t day) {
  uint32_t z = day * 0x9E3779B9u + 0x7F4A7C15u;
  z = (z ^ (z >> 16)) * 0x85EBCA6Bu;
  z = (z ^ (z >> 13)) * 0xC2B2AE35u;
  z ^= z >> 16;
  return (uint16_t)(ENC_FIRST_MIN + z % (ENC_LAST_MIN - ENC_FIRST_MIN + 1));
}

// sigue en el patio: aun no pasaron ENC_LIFETIME_MIN minutos. Un reloj que vuelve atras mas de eso
// tambien lo da por acabado (no se queda dias)
static inline bool encAlive(uint32_t spawn, uint32_t now) {
  if (!spawn) return false;
  const uint32_t life = (uint32_t)ENC_LIFETIME_MIN * 60;
  if (now >= spawn) return now - spawn < life;
  return spawn - now < life;
}
static inline uint16_t encMinutesLeft(uint32_t spawn, uint32_t now) {
  if (!encAlive(spawn, now)) return 0;
  uint32_t gone = now >= spawn ? (now - spawn) / 60 : 0;
  return (uint16_t)(ENC_LIFETIME_MIN - gone);
}

static inline uint8_t encMapObject(uint8_t map) {
  switch (map) {
    case 2:
    case 5: return ENC_OBJ_ROCK;  // cantera y ruinas
    case 4: return ENC_OBJ_SAND;  // playa
    default: return ENC_OBJ_BUSH; // prado, bosque y el suelo liso
  }
}

// los tipos de cada mapa (TY_NONE: solo uno o ninguno). 0 = el suelo liso: todos igual
static inline void encMapTypes(uint8_t map, uint8_t &t1, uint8_t &t2) {
  t1 = t2 = TY_NONE;
  switch (map) {
    case 1: t1 = TY_NORMAL; t2 = TY_PLANTA; break;  // prado
    case 2: t1 = TY_ROCA; t2 = TY_TIERRA; break;    // cantera
    case 3: t1 = TY_BICHO; t2 = TY_PLANTA; break;   // bosque
    case 4: t1 = TY_AGUA; break;                    // playa
    case 5: t1 = TY_ROCA; t2 = TY_TIERRA; break;    // ruinas
  }
}

// peso de una forma base en el sorteo normal: 0 = no sale (exclusivas, legendarias, evoluciones)
static inline uint8_t encLineWeight(int16_t base, uint8_t map) {
  if (base < 1 || base > DEX_COUNT) return 0;
  const DexEntry &d = DEX_TBL[base];
  if (d.rarity != R_COMUN && d.rarity != R_RARO) return 0;
  uint8_t t1, t2;
  encMapTypes(map, t1, t2);
  bool match = (t1 != TY_NONE && (d.type1 == t1 || d.type2 == t1)) || (t2 != TY_NONE && (d.type1 == t2 || d.type2 == t2));
  return match ? ENC_MAP_WEIGHT : 1;
}

// la forma base del encuentro: un 5 % exclusiva (con sus pesos), si no una linea comun o rara
static inline int16_t encPickBase(uint8_t map, uint32_t &rng, bool &exclusive) {
  exclusive = (battleRand(rng) % 100) < ENC_EXCL_PCT;
  if (exclusive) {
    uint32_t tot = 0;
    for (const auto &e : ENC_EXCL) tot += e.w;
    uint32_t r = battleRand(rng) % tot;
    for (const auto &e : ENC_EXCL) {
      if (r < e.w) return e.dex;
      r -= e.w;
    }
    return ENC_EXCL[0].dex;
  }
  uint32_t tot = 0;
  for (int16_t d = 1; d <= DEX_COUNT; d++) tot += encLineWeight(d, map);
  uint32_t r = battleRand(rng) % (tot ? tot : 1);
  for (int16_t d = 1; d <= DEX_COUNT; d++) {
    uint8_t w = encLineWeight(d, map);
    if (r < w) return d;
    r -= w;
  }
  return 19;  // (inalcanzable) Rattata
}

// el ultimo estadio vencido (el mas alto de la mascara de medallas), o -1
static inline int8_t encTopStage(uint8_t badges) {
  for (int8_t s = STAGE_COUNT - 1; s >= 0; s--)
    if (badges & (1u << s)) return s;
  return -1;
}

// el nivel alrededor del que salen: el del jefe del ultimo estadio vencido
static inline uint8_t encLevelCenter(uint8_t badges) {
  int8_t s = encTopStage(badges);
  return s < 0 ? ENC_LV_CENTER_NONE : STAGES[s].lvMax;
}

// centro +-15: la suma de dos +-7 y un +-1 (casi siempre cerca del centro, alguna vez a 15 de el)
static inline uint8_t encRollLevel(uint8_t center, uint32_t &rng) {
  int v = center + (int)(battleRand(rng) % (2 * ENC_LV_SPREAD + 1)) - ENC_LV_SPREAD +
          (int)(battleRand(rng) % (2 * ENC_LV_SPREAD + 1)) - ENC_LV_SPREAD + (int)(battleRand(rng) % 3) - 1;
  if (v < ENC_LV_MIN) v = ENC_LV_MIN;
  if (v > ENC_LV_MAX) v = ENC_LV_MAX;
  return (uint8_t)v;
}

// el bicho salvaje entero: especie (en la forma que toca a su nivel), nivel y entrenamiento (el de los
// esbirros del ultimo estadio vencido)
static inline EncWild encRollWild(uint8_t map, uint8_t badges, uint32_t &rng) {
  EncWild w;
  int16_t base = encPickBase(map, rng, w.exclusive);
  w.level = encRollLevel(encLevelCenter(badges), rng);
  w.dex = evolveForLevel(base, w.level, rng);
  int8_t s = encTopStage(badges);
  uint8_t lo = s < 0 ? 0 : STAGES[s].trMin, hi = s < 0 ? ENC_TR_NONE_MAX : STAGES[s].trMax;
  w.tAtk = stageRange(lo, hi, rng);
  w.tDef = stageRange(lo, hi, rng);
  w.tSpe = stageRange(lo, hi, rng);
  return w;
}
