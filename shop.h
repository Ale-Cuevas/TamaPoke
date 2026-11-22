#pragma once
// La tienda de la mochila y el selector de fondos (v2.0, fase Y4b): logica pura (test/test_shop.cpp).
// Que se vende y en que orden, si una compra sale o por que no, y que pastilla lleva cada mapa.
#include <stdint.h>
#include "economy.h"

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
