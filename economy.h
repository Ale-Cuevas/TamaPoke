#pragma once
// La economia de monedas del patio (v2.0, fase Y4): lo que se gana, lo que cuesta cada cosa y el tope.
// Logica pura (sin Arduino; test/test_economy.cpp). Los numeros estan todos aqui, para afinarlos.
// Los ids de objeto son los de pet.h (IT_*), pero aqui van como numeros para no depender de pet.h
// (pet.h incluye esto); una static_assert en pet.cpp comprueba que coinciden.
#include <stdint.h>
#include "tuning.h"  // los numeros (lo que paga cada cosa, los precios, el tope): se afinan alli

// ---- lo que se gana ----
// Minijuegos: coinsForGame(juego, metrica). La metrica es lo que ya mide cada juego (la misma que
// reparte los puntos de entrenamiento), asi que "jugar bien" paga mas:
//   ECO_BALL  la puntuacion de la pelota (gameScore; ~80 es mucho)
//   ECO_SACK  los golpes al saco en 10 s (sackHits; 30 normal, 40 dificil)
//   ECO_SLED  las filas esquivadas de la carrera (dash.dodged; 50 es una buena)
//   ECO_ZEN   los puntos de DEF que daria la sesion (zenTotalGain: 0..11)
// Metrica 0 = no se gano nada (no se jugo): 0 monedas. Con algo: ECO_GAME_MIN + metrica / div, tope ECO_GAME_MAX.
enum : uint8_t { ECO_BALL = 0, ECO_SACK, ECO_SLED, ECO_ZEN, ECO_GAMES };
static const uint8_t ECO_GAME_DIV[ECO_GAMES] = { ECO_DIV_BALL, ECO_DIV_SACK, ECO_DIV_SLED, ECO_DIV_ZEN };
inline uint8_t coinsForGame(uint8_t game, uint16_t metric) {
  if (game >= ECO_GAMES || !metric) return 0;
  uint32_t c = ECO_GAME_MIN + (uint32_t)metric / ECO_GAME_DIV[game];
  return c > ECO_GAME_MAX ? ECO_GAME_MAX : (uint8_t)c;
}

// Estadios: la primera vez que se vence uno, ECO_STAGE_FIRST (fuera del tope diario); las veces
// siguientes, ECO_STAGE_REPEAT, una vez por estadio y dia (la misma marca del premio del jefe), dentro del tope.

// Metas de pasos (STEP_GOALS 3000 / 6000 / 10000): fuera del tope diario (ya estan limitadas
// por caminar de verdad y solo se cobran una vez al dia cada una). ECO_GOAL_COINS, en tuning.h.
inline uint8_t coinsForGoal(uint8_t tier) { return tier >= 1 && tier <= ECO_GOAL_TIERS ? ECO_GOAL_COINS[tier - 1] : 0; }

// Tope diario de lo que cuenta (minijuegos y estadios repetidos): ECO_DAILY_CAP, ~19 partidas buenas al dia.

// ---- lo que cuesta ----
// Objetos de la mochila, por su id de pet.h (IT_GOLDEN=0, PROTEIN, IRON, CARBOS, CHARM, RARECANDY, EGG, CAKE, REVIVE,
// BALL). 0 = no se vende (el huevo y el amuleto shiny).
#define ECO_ITEMS 10
static const uint16_t ECO_ITEM_PRICE[ECO_ITEMS] = {
  PRICE_GOLDEN,     // IT_GOLDEN  baya dorada
  PRICE_PROTEIN,    // IT_PROTEIN
  PRICE_IRON,       // IT_IRON
  PRICE_CARBOS,     // IT_CARBOS
  0,                // IT_CHARM   no se vende
  PRICE_RARECANDY,  // IT_RARECANDY
  0,                // IT_EGG     no se vende
  PRICE_CAKE,       // IT_CAKE
  PRICE_REVIVE,     // IT_REVIVE
  PRICE_BALL,       // IT_BALL    Poke Ball (v2.2): captura a un salvaje vencido
};
inline uint16_t priceOfItem(uint8_t itemId) { return itemId < ECO_ITEMS ? ECO_ITEM_PRICE[itemId] : 0; }

// Mapas del patio 1..5 (los .bin de la SD: 01 prado, 02 cantera, 03 bosque, 04 playa, 05 ruinas).
// Son del jugador, no del bicho: se empieza con el prado y lo comprado vale para todos los bichos.
// YARD_MAPS y ECO_MAP_PRICE, en tuning.h.
#define YARD_MAP_START 1  // el prado: de todo el mundo desde el principio, no se vende
inline uint16_t priceOfMap(uint8_t n) { return n >= 1 && n <= YARD_MAPS ? ECO_MAP_PRICE[n - 1] : 0; }  // 0 = no se vende

