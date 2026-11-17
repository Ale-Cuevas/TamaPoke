#pragma once
// La economia de monedas del patio (v2.0, fase Y4): lo que se gana, lo que cuesta cada cosa y el tope.
// Logica pura (sin Arduino; test/test_economy.cpp). Los numeros estan todos aqui, para afinarlos.
// Los ids de objeto son los de pet.h (IT_*), pero aqui van como numeros para no depender de pet.h
// (pet.h incluye esto); una static_assert en pet.cpp comprueba que coinciden.
#include <stdint.h>

#define COIN_MAX 9999  // tope de la cartera

// ---- lo que se gana ----
// Minijuegos: coinsForGame(juego, metrica). La metrica es lo que ya mide cada juego (la misma que
// reparte los puntos de entrenamiento), asi que "jugar bien" paga mas:
//   ECO_BALL  la puntuacion de la pelota (gameScore; ~80 es mucho)
//   ECO_SACK  los golpes al saco en 10 s (sackHits; 30 normal, 40 dificil)
//   ECO_SLED  las filas esquivadas de la carrera (dash.dodged; 50 es una buena)
//   ECO_ZEN   los puntos de DEF que daria la sesion (zenTotalGain: 0..11)
// Metrica 0 = no se gano nada (no se jugo): 0 monedas. Con algo: ECO_GAME_MIN + metrica / div, tope ECO_GAME_MAX.
enum : uint8_t { ECO_BALL = 0, ECO_SACK, ECO_SLED, ECO_ZEN, ECO_GAMES };
#define ECO_GAME_MIN 3  // lo menos que paga un juego con algo de resultado
#define ECO_GAME_MAX 8  // lo mas
static const uint8_t ECO_GAME_DIV[ECO_GAMES] = {
  10,  // pelota: +1 cada 10 puntos (50 puntos -> 8)
  8,   // saco: +1 cada 8 golpes (30 -> 6, 40 -> 8)
  10,  // trineo: +1 cada 10 filas (50 -> 8)
  2,   // cascada: +1 cada 2 puntos de DEF (11 -> 8)
};
inline uint8_t coinsForGame(uint8_t game, uint16_t metric) {
  if (game >= ECO_GAMES || !metric) return 0;
  uint32_t c = ECO_GAME_MIN + (uint32_t)metric / ECO_GAME_DIV[game];
  return c > ECO_GAME_MAX ? ECO_GAME_MAX : (uint8_t)c;
}

// Estadios: la primera vez que se vence uno, ECO_STAGE_FIRST (fuera del tope diario); las veces
// siguientes, ECO_STAGE_REPEAT, una vez por estadio y dia (la misma marca del premio del jefe), dentro del tope.
#define ECO_STAGE_FIRST 50
#define ECO_STAGE_REPEAT 10

// Metas de pasos (STEP_GOALS 3000 / 6000 / 10000): fuera del tope diario (ya estan limitadas
// por caminar de verdad y solo se cobran una vez al dia cada una).
#define ECO_GOAL_TIERS 3
static const uint8_t ECO_GOAL_COINS[ECO_GOAL_TIERS] = { 10, 20, 40 };
inline uint8_t coinsForGoal(uint8_t tier) { return tier >= 1 && tier <= ECO_GOAL_TIERS ? ECO_GOAL_COINS[tier - 1] : 0; }

// Tope diario de lo que cuenta (minijuegos y estadios repetidos): ~19 partidas buenas al dia.
#define ECO_DAILY_CAP 150

// ---- lo que cuesta ----
// Objetos de la mochila, por su id de pet.h (IT_GOLDEN=0, PROTEIN, IRON, CARBOS, CHARM, RARECANDY, EGG, CAKE, REVIVE,
// BALL). 0 = no se vende (el huevo y el amuleto shiny).
#define ECO_ITEMS 10
static const uint16_t ECO_ITEM_PRICE[ECO_ITEMS] = {
  30,   // IT_GOLDEN  baya dorada
  60,   // IT_PROTEIN
  60,   // IT_IRON
  60,   // IT_CARBOS
  0,    // IT_CHARM   no se vende
  150,  // IT_RARECANDY
  0,    // IT_EGG     no se vende
  40,   // IT_CAKE
  80,   // IT_REVIVE
  100,  // IT_BALL    Poke Ball (v2.2): captura a un salvaje vencido
};
inline uint16_t priceOfItem(uint8_t itemId) { return itemId < ECO_ITEMS ? ECO_ITEM_PRICE[itemId] : 0; }

// Mapas del patio 1..5 (los .bin de la SD: 01 prado, 02 cantera, 03 bosque, 04 playa, 05 ruinas).
// Son del jugador, no del bicho: se empieza con el prado y lo comprado vale para todos los bichos.
#define YARD_MAPS 5
#define YARD_MAP_START 1  // el prado: de todo el mundo desde el principio, no se vende
static const uint16_t ECO_MAP_PRICE[YARD_MAPS] = { 0, 150, 150, 200, 250 };
inline uint16_t priceOfMap(uint8_t n) { return n >= 1 && n <= YARD_MAPS ? ECO_MAP_PRICE[n - 1] : 0; }  // 0 = no se vende

