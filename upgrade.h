#pragma once
// Mejoras de casa (v3.20, docs/HOME_AND_PALS.md, tarea HOME): logica pura (test/test_upgrades.cpp).
// Tres lineas (camas, gimnasio, juguete) con UPG_TIERS niveles; los numeros viven en tuning.h.
#include <stdint.h>
#include "tuning.h"

enum UpgLine : uint8_t { UPG_BED = 0, UPG_GYM, UPG_TOY, UPG_LINES };
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
