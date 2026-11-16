// La tienda de la mochila y el selector de fondos (v2.0, Y4b; shop.h).
#include "framework.h"
#include "shim/Arduino.h"
#include "../pet.h"
#include "../shop.h"

TEST(shop, la_lista_cubre_todo_lo_que_se_vende) {
  CHECK_EQ(SHOP_ITEMS, shopSellableCount());
  for (int k = 0; k < SHOP_ITEMS; k++) {
    CHECK(priceOfItem(SHOP_ORDER[k]) > 0);
    for (int j = k + 1; j < SHOP_ITEMS; j++) CHECK(SHOP_ORDER[k] != SHOP_ORDER[j]);
  }
  CHECK_EQ(SHOP_ORDER[0], (uint8_t)IT_GOLDEN);  // los ids de aqui son los de pet.h
  CHECK_EQ(SHOP_ORDER[1], (uint8_t)IT_CAKE);
  CHECK_EQ(SHOP_ORDER[2], (uint8_t)IT_RARECANDY);
  CHECK_EQ(SHOP_ORDER[6], (uint8_t)IT_REVIVE);
  CHECK_EQ(priceOfItem(IT_EGG), (uint16_t)0);
  CHECK_EQ(priceOfItem(IT_CHARM), (uint16_t)0);
}

TEST(shop, comprobar_una_compra) {
  CHECK_EQ(shopCheckItem(IT_GOLDEN, 30, 0, ITEM_MAX), SB_OK);
  CHECK_EQ(shopCheckItem(IT_GOLDEN, 29, 0, ITEM_MAX), SB_BROKE);
  CHECK_EQ(shopCheckItem(IT_GOLDEN, 999, ITEM_MAX, ITEM_MAX), SB_FULL);
  CHECK_EQ(shopCheckItem(IT_GOLDEN, 5, ITEM_MAX, ITEM_MAX), SB_FULL);  // lo lleno gana a lo pobre
  CHECK_EQ(shopCheckItem(IT_EGG, 999, 0, ITEM_MAX), SB_NOT_SOLD);
  CHECK_EQ(shopCheckItem(IT_CHARM, 999, 0, ITEM_MAX), SB_NOT_SOLD);
}

TEST(shop, la_pastilla_del_selector) {
  // mapa en uso, suyo, comprable, sin monedas, inexistente
  CHECK_EQ(pickerPill(2, 2, true, 0), PK_USING);
  CHECK_EQ(pickerPill(1, 2, true, 0), PK_USE);
  CHECK_EQ(pickerPill(3, 2, false, priceOfMap(3)), PK_BUY);
  CHECK_EQ(pickerPill(3, 2, false, priceOfMap(3) - 1), PK_BROKE);
  CHECK_EQ(pickerPill(0, 2, false, 999), PK_NONE);
  CHECK_EQ(pickerPill(YARD_MAPS + 1, 2, false, 999), PK_NONE);
}

TEST(shop, elegir_el_del_bioma_es_automatico) {
  CHECK_EQ(pickerChoice(4, 4), (uint8_t)0);
  CHECK_EQ(pickerChoice(3, 4), (uint8_t)3);
  CHECK_EQ(pickerStep(1, -1), (uint8_t)0);
  CHECK_EQ(pickerStep(1, 1), (uint8_t)2);
  CHECK_EQ(pickerStep(YARD_MAPS, 1), (uint8_t)0);
  CHECK_EQ(pickerStep(YARD_MAPS, -1), (uint8_t)(YARD_MAPS - 1));
}

TEST(shop, comprar_y_usar_un_mapa_con_el_pet) {
  mockClearForcedRandom();
  mockNvsReset();
  mockSetMillis(1000);
  Pet p;
  p.begin();
  if (p.awaitingStarter()) p.chooseStarter(4);
  p.eggTap(); p.eggTap(); p.eggTap();
  p.setCoins(500);
  uint8_t biome = p.biomeMap();
  uint8_t other = biome == 3 ? 4 : 3;
  CHECK_EQ(pickerPill(biome, p.yardMapWanted(), p.ownsMap(biome), p.coins), PK_USING);
  CHECK_EQ(pickerPill(other, p.yardMapWanted(), p.ownsMap(other), p.coins), PK_BUY);
  CHECK(p.buyMap(other));
  p.chooseMap(pickerChoice(other, biome));
  CHECK_EQ(p.yardMapWanted(), other);
  CHECK_EQ(pickerPill(other, p.yardMapWanted(), p.ownsMap(other), p.coins), PK_USING);
  CHECK_EQ(pickerPill(biome, p.yardMapWanted(), p.ownsMap(biome), p.coins), PK_USE);
  p.chooseMap(pickerChoice(biome, biome));  // de vuelta al del bioma: automatico
  CHECK_EQ(p.yardChosen, (uint8_t)0);
  CHECK_EQ(p.yardMapWanted(), biome);
}
