#pragma once
// Mejoras de casa (v3.20, docs/HOME_AND_PALS.md, tarea HOME): logica pura (test/test_upgrades.cpp).
// Tres lineas (camas, gimnasio, juguete) con UPG_TIERS niveles; los numeros viven en tuning.h.
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "box.h"     // crc16
#include "upg_tiers.h"
#include "tuning.h"

enum UpgLine : uint8_t { UPG_BED = 0, UPG_GYM, UPG_TOY, UPG_LINES };
static_assert(sizeof(UPG_PRICE[0]) / sizeof(UPG_PRICE[0][0]) == UPG_TIERS, "tuning.h: UPG_PRICE, un precio por nivel");
static_assert(sizeof(UPG_PRICE) / sizeof(UPG_PRICE[0]) == UPG_LINES, "tuning.h: UPG_PRICE, una fila por linea");

// precio del siguiente nivel cuando se tiene "tier" (0..UPG_TIERS): 0 si ya esta al maximo o la linea no existe
inline uint16_t upgPriceNext(uint8_t line, uint8_t tier) {
  return line < UPG_LINES && tier < UPG_TIERS ? UPG_PRICE[line][tier] : 0;
}

// comprobar antes de Pet::buyUpgrade: primero lo que no se arregla (linea mala, al maximo), luego las monedas
enum UpgBuy : uint8_t { UB_OK = 0, UB_BAD, UB_MAX, UB_BROKE };
inline UpgBuy upgCheck(uint8_t line, uint8_t tier, uint16_t coins) {
  if (line >= UPG_LINES) return UB_BAD;
  if (tier >= UPG_TIERS) return UB_MAX;
  if (coins < UPG_PRICE[line][tier]) return UB_BROKE;
  return UB_OK;
}

// lo que la casa le cuenta a la UI (Pet::takeHomeEvt), por hueco: el patio lo anima si se ve
enum HomeEvtKind : uint8_t {
  HE_NONE = 0,
  HE_MORNING,  // buenos dias en la cama: a = JOY dado
  HE_GYM,      // punto de gimnasio: a = 0 ATK, 1 DEF, 2 SPD
  HE_TOY       // juego con el juguete: a = JOY dado
};
struct HomeEvt {
  uint8_t kind = HE_NONE, slot = 0, a = 0;
};

// la cola de eventos de casa para la UI (solo RAM): llena, el mas viejo se pierde
#define HOME_EVT_Q 8
struct HomeEvtRing {
  HomeEvt q[HOME_EVT_Q];
  uint8_t head = 0, n = 0;  // head: el mas viejo
  void push(uint8_t kind, uint8_t slot, uint8_t a) {
    if (n == HOME_EVT_Q) {  // llena: fuera el mas viejo
      head = (uint8_t)((head + 1) % HOME_EVT_Q);
      n--;
    }
    HomeEvt &e = q[(head + n) % HOME_EVT_Q];
    e.kind = kind;
    e.slot = slot;
    e.a = a;
    n++;
  }
  bool pop(HomeEvt &e) {
    if (!n) return false;
    e = q[head];
    head = (uint8_t)((head + 1) % HOME_EVT_Q);
    n--;
    return true;
  }
  void clear() { head = n = 0; }
};

// el gimnasio: que entrenamiento sube (0 ATK, 1 DEF, 2 SPD), el mas bajo de los que estan por debajo del techo
// (empates: ATK, luego DEF, luego SPD); -1 si los tres llegan ya al techo
inline int8_t gymPick(uint8_t atk, uint8_t def, uint8_t spe, uint8_t cap) {
  const uint8_t v[3] = { atk, def, spe };
  int8_t best = -1;
  for (int8_t i = 0; i < 3; i++)
    if (v[i] < cap && (best < 0 || v[i] < v[best])) best = i;
  return best;
}

// ---------- "upg": lo que se guarda (FR-HOME.6) ----------
// Sin migraciones (docs/HOME_AND_PALS.md, Rules): otra version o un blob danado cargan como "nada comprado".
// Cada hueco de casa lleva sus contadores con el uid de su bicho: si el uid cambia, empiezan de cero
#define UPG_VER 1
#define UPG_SLOTS 2  // = HOME_SLOTS (pet.h lo comprueba)
struct __attribute__((packed)) UpgSlot {
  uint32_t uid;       // el bicho al que pertenecen (0 = nadie aun)
  uint32_t day;       // dia de gymGains y toyPlays (vuelven a 0 al cambiar)
  uint8_t gymGains;   // puntos del gimnasio ese dia
  uint8_t toyPlays;   // juegos con el juguete ese dia
  uint16_t sleptMin;  // minutos dormidos de noche desde que se durmio (tope BED_MORNING_MIN_SLEEP)
};
struct __attribute__((packed)) UpgSave {
  uint8_t ver;                // UPG_VER
  uint8_t tier[UPG_LINES];    // 0..UPG_TIERS
  UpgSlot slot[UPG_SLOTS];
  uint16_t crc;               // CRC-16 de todo lo anterior
};
static_assert(sizeof(UpgSlot) == 12 && sizeof(UpgSave) == 30, "\"upg\": 30 bytes");

inline uint16_t upgCrc(const UpgSave &u) { return crc16((const uint8_t *)&u, offsetof(UpgSave, crc)); }
inline void upgSeal(UpgSave &u) {
  u.ver = UPG_VER;
  u.crc = upgCrc(u);
}
inline void upgClear(UpgSave &u) {
  memset(&u, 0, sizeof(u));
  upgSeal(u);
}
// se puede cargar: version conocida, CRC bueno y niveles con sentido
inline bool upgValid(const UpgSave &u) {
  if (u.ver != UPG_VER || u.crc != upgCrc(u)) return false;
  for (uint8_t i = 0; i < UPG_LINES; i++)
    if (u.tier[i] > UPG_TIERS) return false;
  return true;
}
