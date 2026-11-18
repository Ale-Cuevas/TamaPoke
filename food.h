#pragma once
// La comida (v3.2): la baya normal (gratis, sin limite), las seis bayas de colores, la tarta y la baya dorada.
// Cada linea evolutiva tiene UNA baya favorita oculta, calculada con su forma BASE (asi no cambia al
// evolucionar). Logica pura (sin Arduino; test/test_food.cpp). Los numeros estan en tuning.h.
#include <stdint.h>
#include "dex.h"
#include "tuning.h"

// ids de comida (lo que pide Pet::feedFood y lo que lleva la comida del patio). Las bayas de colores van seguidas
enum : uint8_t { FOOD_PLAIN = 0, FOOD_BERRY0, FOOD_BERRY1, FOOD_BERRY2, FOOD_BERRY3, FOOD_BERRY4, FOOD_BERRY5,
                 FOOD_CAKE, FOOD_GOLDEN, FOOD_COUNT };
#define BERRY_COLORS 6  // roja, azul, verde, amarilla, rosa, naranja (FOOD_BERRY0 + color)
#define FOOD_NO_ITEM 0xFF

// el objeto de la mochila (ids de pet.h, IT_*, aqui como numeros para no depender de pet.h; una static_assert en
// pet.cpp comprueba que coinciden) que gasta cada comida; la baya normal no gasta nada
#define FOOD_ITEM_BERRY0 10  // IT_BERRY_R
#define FOOD_ITEM_CAKE 7     // IT_CAKE
#define FOOD_ITEM_GOLDEN 0   // IT_GOLDEN
inline uint8_t foodItem(uint8_t f) {
  if (f >= FOOD_BERRY0 && f < FOOD_BERRY0 + BERRY_COLORS) return (uint8_t)(FOOD_ITEM_BERRY0 + (f - FOOD_BERRY0));
  if (f == FOOD_CAKE) return FOOD_ITEM_CAKE;
  if (f == FOOD_GOLDEN) return FOOD_ITEM_GOLDEN;
  return FOOD_NO_ITEM;
}
// la comida de un objeto de la mochila (FOOD_COUNT si no es comida)
inline uint8_t foodOfItem(uint8_t item) {
  for (uint8_t f = 0; f < FOOD_COUNT; f++)
    if (foodItem(f) == item) return f;
  return FOOD_COUNT;
}
inline bool foodIsBerry(uint8_t f) { return f >= FOOD_BERRY0 && f < FOOD_BERRY0 + BERRY_COLORS; }

// la baya favorita (0..5) de la linea de la especie dex: sale de su forma base. Algo mezclado para que lineas
// seguidas no den colores seguidos
inline uint8_t favBerryOf(uint16_t dex) {
  uint16_t b = dex <= DEX_COUNT ? DEX_TBL[dex].base : 0;
  uint32_t h = (uint32_t)b * 2654435761u;
  return (uint8_t)((h >> 16) % BERRY_COLORS);
}

// lo que da una comida. fav = la baya de colores que es la favorita del bicho
struct FoodEffect {
  uint8_t fullness;  // comida que sube (la tarta la llena del todo)
  uint8_t joy;
  uint8_t bond;
  bool heart;        // "le encanta!": corazon
};
inline FoodEffect foodEffect(uint8_t f, bool fav) {
  if (f == FOOD_PLAIN) return { PLAIN_BERRY_FOOD, 0, 0, false };
  if (foodIsBerry(f)) return fav ? FoodEffect{ FAV_BERRY_FOOD, FAV_BERRY_JOY, FAV_BERRY_BOND, true }
                                 : FoodEffect{ BERRY_FOOD, BERRY_JOY, 0, false };
  if (f == FOOD_CAKE) return { 100, CAKE_JOY, 0, true };
  if (f == FOOD_GOLDEN) return { GOLDEN_FOOD, GOLDEN_JOY, 0, true };
  return { 0, 0, 0, false };
}
