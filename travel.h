#pragma once
// Los viajes y la coleccion del patio (v2.0, fase Y5 parte A): un bicho de la caja sale de viaje a un mapa
// del jugador 1, 3 u 8 h de tiempo real (la hora del RTC, como el huevo del patio: corre con la placa
// apagada) y vuelve con monedas, crecimiento, quiza un objeto y quiza un recuerdo. Logica pura, sin
// Arduino (test/test_travel.cpp): las tablas, el sorteo de lo que trae, los sucesos del camino y el
// tiempo que falta. Todo el azar sale de UNA semilla guardada al salir, asi que el resultado es el
// mismo se calcule cuando se calcule (y se puede probar).
//
// Los ids de objeto son los de pet.h (IT_*), pero aqui van como numeros para no depender de pet.h
// (pet.h incluye esto); una static_assert en pet.cpp comprueba que coinciden.
#include <stdint.h>
#include "eggtimer.h"  // eggClockKnown / EGG_EPOCH_MIN: lo que cuenta como "hora de verdad"
#include "tuning.h"    // las tablas de cada duracion (monedas, crecimiento, probabilidades): se afinan alli

// ---- los viajes ----
// TRV_MIN, TRV_COINS, TRV_GROW, TRV_ITEM_PCT, TRV_SOUV_PCT y TRV_DUP_COINS: tuning.h
#define TRV_NONE 0xFF  // "ningun hueco" / "sin objeto" / "sin recuerdo"
static const uint8_t TRV_EVENTS[TRV_TIERS] = { 1, 2, 3 };            // sucesos del camino
#define TRV_EVENTS_MAX 3

// objetos que pueden traer (su peso, TRV_POOL_W, en tuning.h, en este mismo orden): baya dorada, tarta, 3
// vitaminas, caramelo raro, revivir, (v2.2) la Poke Ball, (v3.2) una baya de color: el id de la lista es la
// primera (10, IT_BERRY_R) y travelRoll le suma el color (0..5), sacado de la semilla; y (v3.14) el Cebo
static constexpr uint8_t TRV_POOL_ID[TRV_POOL_N] = { 0 /*GOLDEN*/, 7 /*CAKE*/, 1 /*PROTEIN*/, 2 /*IRON*/, 3 /*CARBOS*/,
                                                 5 /*RARECANDY*/, 8 /*REVIVE*/, 9 /*BALL*/, 10 /*BERRY*/, 16 /*LURE*/ };
#define TRV_POOL_BERRY 10  // IT_BERRY_R: el primero de los seis colores
#define TRV_BERRY_COLORS 6

// ---- recuerdos: 5 mapas x 5. id = (mapa - 1) * 5 + i, i = 0..4 ----
#define TRV_DESTS 5  // = YARD_MAPS (economy.h); pet.cpp lo comprueba
#define SOUV_PER_DEST 5
#define SOUV_COUNT (TRV_DESTS * SOUV_PER_DEST)
enum : uint8_t { SOUV_COMMON = 0, SOUV_UNCOMMON, SOUV_RARE };
inline uint8_t souvenirRarity(uint8_t id) {  // 0-1 comun, 2-3 poco comun, 4 raro
  uint8_t i = id % SOUV_PER_DEST;
  return i <= 1 ? SOUV_COMMON : (i <= 3 ? SOUV_UNCOMMON : SOUV_RARE);
}
inline uint8_t souvenirDest(uint8_t id) { return id / SOUV_PER_DEST + 1; }  // mapa 1..5
// el peso de comun / poco comun / raro por duracion (TRV_RARITY_W) esta en tuning.h

// ---- el azar: una semilla -> una secuencia (splitmix32) ----
inline uint32_t trvNext(uint32_t *s) {
  *s += 0x9E3779B9u;
  uint32_t z = *s;
  z = (z ^ (z >> 16)) * 0x85EBCA6Bu;
  z = (z ^ (z >> 13)) * 0xC2B2AE35u;
  return z ^ (z >> 16);
}

// lo que trae un viaje y a quien le toco (Pet rellena lo segundo; se guarda tal cual en la NVS: "trvR")
struct __attribute__((packed)) TravelResult {
  uint8_t valid;       // 1 = hay un resultado sin ver
  uint8_t dest;        // mapa 1..5
  uint8_t tier;        // 0..2 (1 / 3 / 8 h)
  uint8_t item;        // id de objeto (pet.h IT_*) o TRV_NONE
  uint8_t itemLost;    // 1 = la mochila estaba llena: se perdio
  uint8_t souvenir;    // id 0..24 o TRV_NONE
  uint8_t dup;         // 1 = ya lo tenia: se cambio por monedas
  uint8_t shiny;       // el bicho era shiny
  uint16_t coins;      // monedas ganadas EN TOTAL (con las del recuerdo repetido)
  uint16_t dupCoins;   // de ellas, las del recuerdo repetido
  uint16_t growMin;    // minutos de crecimiento del bicho
  uint16_t lvBefore, lvAfter;
  int16_t speciesId;   // quien viajo
  char nick[12];
  uint32_t seed;       // la semilla del viaje (los sucesos salen de ella)
  uint8_t rollItem, rollSouv;  // las tiradas 0..99 (para el registro)
};

// El sorteo (puro): semilla, mapa 1..5, duracion 0..2 y los recuerdos que ya tiene (bit id).
// Rellena dest, tier, coins, dupCoins, growMin, item, souvenir, dup y las tiradas.
// Un recuerdo ya tenido se vuelve a sortear UNA vez; si sigue repetido, TRV_DUP_COINS monedas.
inline TravelResult travelRoll(uint32_t seed, uint8_t dest, uint8_t tier, uint32_t ownedMask) {
  TravelResult r;
  for (unsigned i = 0; i < sizeof(r); i++) ((uint8_t *)&r)[i] = 0;
  if (tier >= TRV_TIERS) tier = TRV_TIERS - 1;
  if (dest < 1) dest = 1;
  if (dest > TRV_DESTS) dest = TRV_DESTS;
  r.dest = dest;
  r.tier = tier;
  r.seed = seed;
  r.item = TRV_NONE;
  r.souvenir = TRV_NONE;
  r.coins = TRV_COINS[tier];
  r.growMin = TRV_GROW[tier];
  uint32_t s = seed ^ 0xA5A5C0DEu;
  // objeto
  r.rollItem = (uint8_t)(trvNext(&s) % 100);
  uint8_t pick = (uint8_t)(trvNext(&s) % 100);  // siempre se consume: el resto no depende de si hubo objeto
  if (r.rollItem < TRV_ITEM_PCT[tier]) {
    uint8_t acc = 0;
    for (uint8_t i = 0; i < TRV_POOL_N; i++) {
      acc += TRV_POOL_W[i];
      if (pick < acc) {
        r.item = TRV_POOL_ID[i];
        if (r.item == TRV_POOL_BERRY) r.item += (uint8_t)(((seed * 2654435761u) >> 20) % TRV_BERRY_COLORS);  // sin tocar la secuencia
        break;
      }
    }
  }
  // recuerdo
  r.rollSouv = (uint8_t)(trvNext(&s) % 100);
  uint32_t a = trvNext(&s), b = trvNext(&s);  // dos intentos, se usen o no
  if (r.rollSouv < TRV_SOUV_PCT[tier]) {
    uint8_t id = 0;
    for (int attempt = 0; attempt < 2; attempt++) {
      uint32_t x = attempt ? b : a;
      uint8_t w = (uint8_t)((x >> 8) % 100), rar = SOUV_COMMON, acc = TRV_RARITY_W[tier][0];
      if (w >= acc) {
        rar = SOUV_UNCOMMON;
        acc += TRV_RARITY_W[tier][1];
        if (w >= acc) rar = SOUV_RARE;
      }
      uint8_t i = rar == SOUV_COMMON ? (uint8_t)((x >> 20) % 2) : (rar == SOUV_UNCOMMON ? (uint8_t)(2 + (x >> 20) % 2) : 4);
      id = (uint8_t)((dest - 1) * SOUV_PER_DEST + i);
      if (!(ownedMask & (1u << id))) break;
    }
    r.souvenir = id;
    if (ownedMask & (1u << id)) {
      r.dup = 1;
      r.dupCoins = TRV_DUP_COINS;
      r.coins += TRV_DUP_COINS;
    }
  }
  return r;
}

// ---- el tiempo ----
inline uint32_t travelSeconds(uint8_t tier) { return (uint32_t)TRV_MIN[tier < TRV_TIERS ? tier : TRV_TIERS - 1] * 60; }

// segundos que faltan. Sin hora (de salida o de ahora) o con el reloj hacia atras: todo el viaje,
// nunca termina de golpe; 0 = ya volvio
inline uint32_t travelTimeLeft(uint32_t startEpoch, uint8_t tier, uint32_t nowEpoch) {
  uint32_t dur = travelSeconds(tier);
  if (!eggClockKnown(startEpoch) || !eggClockKnown(nowEpoch) || nowEpoch <= startEpoch) return dur;
  uint32_t el = nowEpoch - startEpoch;
  return el >= dur ? 0 : dur - el;
}

// minutos de viaje cumplidos (0 sin hora), tope la duracion
inline uint16_t travelElapsedMin(uint32_t startEpoch, uint8_t tier, uint32_t nowEpoch) {
  uint32_t left = travelTimeLeft(startEpoch, tier, nowEpoch);
  return (uint16_t)((travelSeconds(tier) - left) / 60);
}

// ---- los sucesos del camino ----
// Cada plantilla es un texto (S_TRV_EV_0 + plantilla en i18n.h), valido para algunos mapas (bit mapa-1;
// TRV_ANY = todos) y, si TRV_ARG, con un %s: el nombre de una especie del bioma (travelEvent la da).
#define TRV_ANY 0x1F
#define TRV_ARG 0x80  // la plantilla lleva %s (la especie)
#define TRV_TEMPLATES 13
#define TRV_B(n) (1u << ((n) - 1))
static const uint8_t TRV_TPL_MASK[TRV_TEMPLATES] = {
  TRV_ANY | TRV_ARG,                 // 0 Met a wild %s
  TRV_B(1) | TRV_B(3),               // 1 Took a nap under a tree
  TRV_ANY,                           // 2 Found something shiny
  TRV_B(4),                          // 3 Splashed in the water
  TRV_B(2),                          // 4 Climbed a big rock
  TRV_B(3) | TRV_B(5),               // 5 Heard a strange noise
  TRV_B(1),                          // 6 Chased a butterfly
  TRV_B(2) | TRV_B(4) | TRV_B(5),    // 7 Dug a small hole
  TRV_ANY,                           // 8 Watched the stars
  TRV_ANY | TRV_ARG,                 // 9 Shared berries with a wild %s
  TRV_B(4),                          // 10 Built a sand castle
  TRV_B(2) | TRV_B(5),               // 11 Explored a dark corridor
  TRV_ANY,                           // 12 Stopped for a snack
};
// especies (dex) de cada bioma para "Met a wild %s"
static const uint8_t TRV_SPECIES[TRV_DESTS][5] = {
  { 19, 16, 10, 43, 69 },    // prado: Rattata, Pidgey, Caterpie, Oddish, Bellsprout
  { 74, 95, 66, 27, 111 },   // cantera: Geodude, Onix, Machop, Sandshrew, Rhyhorn
  { 13, 25, 46, 48, 102 },   // bosque: Weedle, Pikachu, Paras, Venonat, Exeggcute
  { 98, 90, 120, 54, 72 },   // playa: Krabby, Shellder, Staryu, Psyduck, Tentacool
  { 92, 41, 63, 96, 93 },    // ruinas: Gastly, Zubat, Abra, Drowzee, Haunter
};
inline bool travelTplFits(uint8_t tpl, uint8_t dest) {
  return tpl < TRV_TEMPLATES && dest >= 1 && dest <= TRV_DESTS && (TRV_TPL_MASK[tpl] & TRV_B(dest) & TRV_ANY);
}
inline bool travelTplHasArg(uint8_t tpl) { return tpl < TRV_TEMPLATES && (TRV_TPL_MASK[tpl] & TRV_ARG); }

inline uint8_t travelEventCount(uint8_t tier) { return TRV_EVENTS[tier < TRV_TIERS ? tier : TRV_TIERS - 1]; }

// el suceso i (0..count-1): minuto del viaje en que pasa (dentro de el, creciente), plantilla y la
// especie para el %s (dex; 0 si la plantilla no lo lleva). Sale solo de la semilla: sin repetir plantilla.
inline void travelEvent(uint32_t seed, uint8_t dest, uint8_t tier, uint8_t i, uint16_t *minuteOffset, uint8_t *tpl,
                        uint8_t *speciesArg) {
  if (tier >= TRV_TIERS) tier = TRV_TIERS - 1;
  if (dest < 1) dest = 1;
  if (dest > TRV_DESTS) dest = TRV_DESTS;
  uint8_t n = TRV_EVENTS[tier];
  if (i >= n) i = n - 1;
  uint32_t s = seed ^ 0x5EED1E55u;
  uint8_t used[TRV_EVENTS_MAX];
  uint8_t chosen = 0, t = 0, arg = 0;
  uint16_t off = 0;
  uint16_t seg = TRV_MIN[tier] / n;
  for (uint8_t e = 0; e <= i; e++) {
    uint32_t x = trvNext(&s), y = trvNext(&s), z = trvNext(&s);
    // plantilla: la x-esima de las que valen y no se han usado
    uint8_t fit = 0;
    for (uint8_t k = 0; k < TRV_TEMPLATES; k++) {
      bool u = false;
      for (uint8_t j = 0; j < chosen; j++) u |= used[j] == k;
      if (!u && travelTplFits(k, dest)) fit++;
    }
    uint8_t want = (uint8_t)(x % fit);
    t = 0;
    for (uint8_t k = 0; k < TRV_TEMPLATES; k++) {
      bool u = false;
      for (uint8_t j = 0; j < chosen; j++) u |= used[j] == k;
      if (u || !travelTplFits(k, dest)) continue;
      if (want-- == 0) {
        t = k;
        break;
      }
    }
    used[chosen++] = t;
    // minuto: el tramo e-esimo del viaje, sin tocar el primer quinto ni el ultimo quinto
    off = (uint16_t)(e * seg + seg / 5 + y % (uint32_t)(seg * 3 / 5));
    arg = travelTplHasArg(t) ? TRV_SPECIES[dest - 1][z % 5] : 0;
  }
  if (minuteOffset) *minuteOffset = off;
  if (tpl) *tpl = t;
  if (speciesArg) *speciesArg = arg;
}
