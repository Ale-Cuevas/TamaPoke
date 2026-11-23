#pragma once
// La tienda de la mochila y el selector de fondos (v2.0, fase Y4b): logica pura (test/test_shop.cpp).
// Que se vende y en que orden, si una compra sale o por que no, y que pastilla lleva cada mapa.
#include <stdint.h>
#include "economy.h"
#include "upgrade.h"

// los objetos a la venta, en el orden en que aparecen en la tienda (ids de pet.h: IT_GOLDEN=0, PROTEIN,
// IRON, CARBOS, CHARM, RARECANDY, EGG, CAKE, REVIVE, BALL, BERRY_R..O = 10..15, LURE = 16). Solo entran los que
// cuestan algo (el Cebo, no).
static const uint8_t SHOP_ORDER[] = { 0 /*GOLDEN*/, 7 /*CAKE*/, 5 /*RARECANDY*/, 1 /*PROTEIN*/, 2 /*IRON*/, 3 /*CARBOS*/,
                                      8 /*REVIVE*/, 9 /*BALL*/, 10 /*BERRY_R*/, 11 /*BERRY_B*/, 12 /*BERRY_G*/,
                                      13 /*BERRY_Y*/, 14 /*BERRY_P*/, 15 /*BERRY_O*/ };
#define SHOP_ITEMS ((int)(sizeof(SHOP_ORDER) / sizeof(SHOP_ORDER[0])))
#define SHOP_PAGE0 8  // los de la 1.a pagina (con la tarjeta Fondos); el resto, las 6 bayas de colores, van en la 2.a (v3.2)
static_assert(SHOP_ITEMS == SHOP_PAGE0 + 6, "la tienda: 8 objetos y 6 bayas de colores");

// cuantos objetos se venden de verdad (precio > 0): el test comprueba que SHOP_ORDER los cubre todos
inline int shopSellableCount() {
  int n = 0;
  for (uint8_t i = 0; i < ECO_ITEMS; i++)
    if (priceOfItem(i) > 0) n++;
  return n;
}

enum ShopBuy : uint8_t { SB_OK = 0, SB_NOT_SOLD, SB_BROKE, SB_FULL };
// comprobar antes de llamar a Pet::buyItem; "cap" es itemCap(id). Primero lo que no se puede arreglar
// (no se vende, no cabe el paquete entero: have + shopPackOf(id) > cap) y luego las monedas.
inline ShopBuy shopCheckItem(uint8_t itemId, uint16_t coins, uint8_t have, uint8_t cap) {
  uint16_t p = priceOfItem(itemId);
  if (!p) return SB_NOT_SOLD;
  if ((uint16_t)have + shopPackOf(itemId) > cap) return SB_FULL;
  if (coins < p) return SB_BROKE;
  return SB_OK;
}

// la pastilla de abajo del selector de fondos
enum PickerPill : uint8_t {
  PK_USING = 0,  // es el que se ve ahora: sin accion
  PK_USE,        // suyo: pastilla Usar
  PK_BUY,        // se puede comprar
  PK_BROKE,      // no hay monedas: precio en gris
  PK_NONE        // el mapa no existe
};
// wanted = Pet::yardMapWanted(); owned = Pet::ownsMap(n); price = priceOfMap(n)
inline PickerPill pickerPill(uint8_t n, uint8_t wanted, bool owned, uint16_t coins) {
  if (n < 1 || n > YARD_MAPS) return PK_NONE;
  if (n == wanted) return PK_USING;
  if (owned) return PK_USE;
  return coins >= priceOfMap(n) ? PK_BUY : PK_BROKE;
}

// el siguiente mapa al navegar (sin dar la vuelta): 0 si no hay mas
inline uint8_t pickerStep(uint8_t n, int dir) {
  int m = (int)n + dir;
  return m < 1 || m > YARD_MAPS ? 0 : (uint8_t)m;
}

// ---------- la pagina Casa de la tienda (v3.20, HOME): 3 tarjetas, una por linea de mejoras ----------
#define SHOP_ITEM_PAGES 2                       // paginas de objetos (la 2.a, las bayas de colores)
#define SHOP_PAGES (SHOP_ITEM_PAGES + 1)        // + la pagina Casa, la ultima (se llega deslizando)
inline bool shopPageIsHome(uint8_t page) { return page == SHOP_ITEM_PAGES; }

// lo que ensena la tarjeta de una linea con "tier" comprado (0..UPG_TIERS)
enum UpgCard : uint8_t {
  UC_BUY = 0,  // siguiente nivel a la venta y alcanza el dinero
  UC_BROKE,    // a la venta, pero precio en gris
  UC_MAX       // ya esta al maximo: pone MAX, con el icono del nivel 3, y no pregunta
};
inline UpgCard upgCardKind(uint8_t line, uint8_t tier, uint16_t coins) {
  UpgBuy r = upgCheck(line, tier, coins);
  return r == UB_OK ? UC_BUY : r == UB_BROKE ? UC_BROKE : UC_MAX;
}
// nivel que se dibuja (0..UPG_TIERS-1: el siguiente a comprar; al maximo, el ultimo)
inline uint8_t upgShownTier(uint8_t tier) { return tier >= UPG_TIERS ? UPG_TIERS - 1 : tier; }
// el numero de la linea de efecto de ese nivel dibujado (BED_ENE_GAIN, GYM_CAP o TOY_JOY de tuning.h)
inline uint8_t upgEffectNum(uint8_t line, uint8_t shown) {
  uint8_t t = shown + 1;
  return line == UPG_BED ? BED_ENE_GAIN[t] : line == UPG_GYM ? GYM_CAP[t] : TOY_JOY[t];
}
// una tarjeta abre la pregunta de compra solo si se puede comprar o falta dinero (la de MAX no)
inline bool upgCardAsks(uint8_t line, uint8_t tier) { return upgCheck(line, tier, 0xFFFF) == UB_OK; }
