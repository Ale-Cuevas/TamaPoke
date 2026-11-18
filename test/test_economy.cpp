// Las monedas del patio (v2.0, Y4; economy.h + Pet): lo que se gana, el tope diario, los precios, comprar,
// los mapas (el del bioma es gratis) y el guardado en la NVS (incluido un guardado de antes sin las claves).
#include "framework.h"
#include "shim/Arduino.h"
#include "shim/Preferences.h"
#include "../pet.h"

static const uint32_t EDAY = 20000;
static const uint32_t ET = EDAY * 86400 + 10 * 3600;

static void eBorn(Pet &p, int16_t dex = 4) {
  mockClearForcedRandom();
  mockNvsReset();
  mockSetMillis(1000);
  p.begin();
  if (p.awaitingStarter()) p.chooseStarter(dex);
  p.eggTap();
  p.eggTap();
  p.eggTap();
  p.setClock(ET);
}

// ---------------------------------------------------------------- logica pura
TEST(economy, lo_que_paga_cada_juego) {
  for (uint8_t g = 0; g < ECO_GAMES; g++) {
    CHECK_EQ(coinsForGame(g, 0), (uint8_t)0);  // sin resultado, nada
    CHECK_EQ(coinsForGame(g, 1), (uint8_t)ECO_GAME_MIN);
    CHECK_EQ(coinsForGame(g, 60000), (uint8_t)ECO_GAME_MAX);
    uint8_t prev = 0;
    for (uint16_t m = 0; m < 300; m++) {  // nunca baja al jugar mejor
      uint8_t c = coinsForGame(g, m);
      CHECK(c >= prev);
      CHECK(c <= ECO_GAME_MAX);
      prev = c;
    }
  }
  CHECK_EQ(coinsForGame(ECO_BALL, 20), (uint8_t)5);
  CHECK_EQ(coinsForGame(ECO_SACK, 30), (uint8_t)6);
  CHECK_EQ(coinsForGame(ECO_SLED, 50), (uint8_t)8);
  CHECK_EQ(coinsForGame(ECO_ZEN, 11), (uint8_t)8);
  CHECK_EQ(coinsForGame(ECO_GAMES, 50), (uint8_t)0);
  CHECK_EQ(coinsForGoal(1), (uint8_t)10);
  CHECK_EQ(coinsForGoal(2), (uint8_t)20);
  CHECK_EQ(coinsForGoal(3), (uint8_t)40);
  CHECK_EQ(coinsForGoal(0), (uint8_t)0);
  CHECK_EQ(coinsForGoal(4), (uint8_t)0);
}

TEST(economy, precios) {
  CHECK_EQ(priceOfItem(IT_GOLDEN), (uint16_t)30);
  CHECK_EQ(priceOfItem(IT_PROTEIN), (uint16_t)60);
  CHECK_EQ(priceOfItem(IT_IRON), (uint16_t)60);
  CHECK_EQ(priceOfItem(IT_CARBOS), (uint16_t)60);
  CHECK_EQ(priceOfItem(IT_RARECANDY), (uint16_t)150);
  CHECK_EQ(priceOfItem(IT_CAKE), (uint16_t)40);
  CHECK_EQ(priceOfItem(IT_REVIVE), (uint16_t)80);
  CHECK_EQ(priceOfItem(IT_EGG), (uint16_t)0);
  CHECK_EQ(priceOfItem(IT_CHARM), (uint16_t)0);
  CHECK_EQ(priceOfItem(IT_COUNT), (uint16_t)0);
  CHECK_EQ(priceOfItem(200), (uint16_t)0);
  CHECK_EQ(priceOfMap(0), (uint16_t)0);
  CHECK_EQ(priceOfMap(1), (uint16_t)0);  // el prado es el inicial: no se vende
  CHECK_EQ(priceOfMap(2), (uint16_t)150);
  CHECK_EQ(priceOfMap(3), (uint16_t)150);
  CHECK_EQ(priceOfMap(4), (uint16_t)200);
  CHECK_EQ(priceOfMap(5), (uint16_t)250);
  CHECK_EQ(priceOfMap(6), (uint16_t)0);
  CHECK_EQ(YARD_MAP_START, 1);
}

// ---------------------------------------------------------------- ganar y el tope
TEST(economy, tope_diario_solo_para_lo_que_cuenta) {
  Pet p;
  eBorn(p);
  CHECK_EQ(p.coins, (uint16_t)0);
  CHECK_EQ(p.addCoins(100, true), (uint16_t)100);
  CHECK_EQ(p.addCoins(100, true), (uint16_t)50);  // solo cabian 50 hasta 150
  CHECK_EQ(p.addCoins(5, true), (uint16_t)0);
  CHECK_EQ(p.coins, (uint16_t)150);
  CHECK_EQ(p.coinsEarnedToday(), (uint16_t)ECO_DAILY_CAP);
  CHECK_EQ(p.addCoins(50, false), (uint16_t)50);  // primeras veces y metas: fuera del tope
  CHECK_EQ(p.coins, (uint16_t)200);
  CHECK_EQ(p.coinsEarnedToday(), (uint16_t)ECO_DAILY_CAP);  // no cuentan
  p.setClock(ET + 86400);  // dia nuevo
  CHECK_EQ(p.coinsEarnedToday(), (uint16_t)0);
  CHECK_EQ(p.addCoins(10, true), (uint16_t)10);
  CHECK_EQ(p.coinsEarnedToday(), (uint16_t)10);
}

TEST(economy, cartera_con_tope) {
  Pet p;
  eBorn(p);
  CHECK_EQ(p.addCoins(COIN_MAX, false), (uint16_t)COIN_MAX);
  CHECK_EQ(p.addCoins(40, false), (uint16_t)0);
  CHECK_EQ(p.coins, (uint16_t)COIN_MAX);
  CHECK(p.spendCoins(9000));
  CHECK_EQ(p.addCoins(9500, false), (uint16_t)9000);  // solo caben 9000 hasta el tope
  CHECK_EQ(p.coins, (uint16_t)COIN_MAX);
}

TEST(economy, los_juegos_pagan_al_acabar) {
  Pet p;
  eBorn(p);
  p.playResult(50);
  CHECK_EQ(p.lastCoins, (uint16_t)8);
  p.trainStrength(0);
  CHECK_EQ(p.lastCoins, (uint16_t)0);
  p.trainStrength(30);
  CHECK_EQ(p.lastCoins, (uint16_t)6);
  p.dashResult(25);
  CHECK_EQ(p.lastCoins, (uint16_t)5);
  p.zenResult(300, 0);
  CHECK(p.lastCoins >= ECO_GAME_MIN && p.lastCoins <= ECO_GAME_MAX);
  uint16_t sum = 8 + 6 + 5 + p.lastCoins;
  CHECK_EQ(p.coins, sum);
  CHECK_EQ(p.coinsEarnedToday(), sum);
}

TEST(economy, un_huevo_no_gana_con_los_juegos) {
  Pet p;
  mockClearForcedRandom();
  mockNvsReset();
  p.begin();
  p.playResult(50);
  CHECK_EQ(p.coins, (uint16_t)0);
}

TEST(economy, las_metas_de_pasos_pagan_fuera_del_tope) {
  Pet p;
  eBorn(p);
  p.addCoins(ECO_DAILY_CAP, true);  // el tope ya lleno
  uint16_t c0 = p.coins;
  p.addSteps(STEP_GOALS[0], EDAY);
  CHECK_EQ(p.coins, (uint16_t)(c0 + 10));
  uint16_t got = 0;
  CHECK_EQ(p.takeGoalBanner(nullptr, &got), (uint8_t)1);
  CHECK_EQ(got, (uint16_t)10);
  p.addSteps(STEP_GOALS[2] - STEP_GOALS[0], EDAY);  // cruza la 2 y la 3 de golpe
  // v3.4: el primer dia de 10000 pasos es tambien la medalla Maraton (del jugador, fuera del tope)
  uint16_t med = medalPayout(0, medalTierOf(MD_MARATHON, 1));
  CHECK_EQ(p.coins, (uint16_t)(c0 + 10 + 20 + 40 + med));
  CHECK_EQ(p.takeGoalBanner(nullptr, &got), (uint8_t)3);
  CHECK_EQ(got, (uint16_t)60);
  p.addSteps(500, EDAY);  // ya cobradas hoy
  CHECK_EQ(p.coins, (uint16_t)(c0 + 70 + med));
}

TEST(economy, estadio_primera_vez_y_repeticion_diaria) {
  Pet p;
  eBorn(p);
  uint32_t t[4] = { p.uid, 0, 0, 0 };
  StageReward r = p.stageDone(0, true, t, 1);
  CHECK(r.newBadge);
  CHECK_EQ(r.coins, (uint16_t)ECO_STAGE_FIRST);
  CHECK_EQ(p.coinsEarnedToday(), (uint16_t)0);  // la primera no cuenta para el tope
  r = p.stageDone(0, true, t, 1);  // mismo estadio, mismo dia: ya cobrado
  CHECK_EQ(r.coins, (uint16_t)0);
  CHECK_EQ(p.coins, (uint16_t)ECO_STAGE_FIRST);
  p.setClock(ET + 86400);
  r = p.stageDone(0, true, t, 1);  // otro dia: repeticion
  CHECK(!r.newBadge);
  CHECK_EQ(r.coins, (uint16_t)ECO_STAGE_REPEAT);
  CHECK_EQ(p.coinsEarnedToday(), (uint16_t)ECO_STAGE_REPEAT);
  r = p.stageDone(1, false, t, 1);  // perder no paga
  CHECK_EQ(r.coins, (uint16_t)0);
}

// ---------------------------------------------------------------- comprar
TEST(economy, comprar_objetos) {
  Pet p;
  eBorn(p);
  CHECK(!p.buyItem(IT_CAKE));  // sin monedas
  CHECK_EQ(p.items[IT_CAKE], (uint8_t)0);
  p.addCoins(100, false);
  uint8_t cakes = p.items[IT_CAKE];
  CHECK(p.buyItem(IT_CAKE));
  CHECK_EQ(p.coins, (uint16_t)60);
  CHECK_EQ(p.items[IT_CAKE], (uint8_t)(cakes + 1));
  CHECK(!p.buyItem(IT_REVIVE));  // 80 > 60
  CHECK(!p.buyItem(IT_EGG));     // no se vende
  CHECK(!p.buyItem(IT_CHARM));
  CHECK(!p.buyItem(IT_COUNT));
  CHECK_EQ(p.coins, (uint16_t)60);
  CHECK(!p.spendCoins(61));
  CHECK(p.spendCoins(60));
  CHECK_EQ(p.coins, (uint16_t)0);
}

// v3.2: cada baya de color cuesta PRICE_BERRY (tuning.h) y la baya normal no se vende (es gratis)
TEST(economy, las_bayas_de_colores_se_compran) {
  Pet p;
  eBorn(p);
  for (uint8_t i = IT_BERRY_R; i <= IT_BERRY_O; i++) CHECK_EQ(priceOfItem(i), (uint16_t)PRICE_BERRY);
  p.addCoins(PRICE_BERRY * 2 + 3, false);
  CHECK(p.buyItem(IT_BERRY_Y));
  CHECK(p.buyItem(IT_BERRY_Y));
  CHECK(!p.buyItem(IT_BERRY_Y));  // sin monedas
  CHECK_EQ(p.items[IT_BERRY_Y], (uint8_t)2);
  CHECK_EQ(p.coins, (uint16_t)3);
  p.addCoins(500, false);
  p.items[IT_BERRY_O] = ITEM_MAX;
  CHECK(!p.buyItem(IT_BERRY_O));  // tope de la mochila
}

TEST(economy, mochila_llena_no_compra) {
  Pet p;
  eBorn(p);
  p.addCoins(500, false);
  p.items[IT_GOLDEN] = ITEM_MAX;
  CHECK(!p.buyItem(IT_GOLDEN));
  CHECK_EQ(p.coins, (uint16_t)500);  // no cobra
  p.items[IT_GOLDEN] = ITEM_MAX - 1;
  CHECK(p.buyItem(IT_GOLDEN));
  CHECK_EQ(p.items[IT_GOLDEN], (uint8_t)ITEM_MAX);
  CHECK_EQ(p.coins, (uint16_t)470);
}

TEST(economy, mapas) {
  Pet p;
  eBorn(p);
  uint8_t own = YARD_MAP_START;
  CHECK(p.ownsMap(own));  // el inicial es de todo el mundo
  CHECK(!p.buyMap(own));  // y no se compra
  for (uint8_t n = 2; n <= YARD_MAPS; n++) CHECK(!p.ownsMap(n));  // nada mas: ni el del bioma
  uint8_t other = 4;
  CHECK(!p.ownsMap(other));
  CHECK(!p.ownsMap(0));
  CHECK(!p.ownsMap(6));
  CHECK(!p.buyMap(other));  // sin monedas
  p.chooseMap(other);       // no es suyo: se ignora
  CHECK_EQ(p.yardChosen, (uint8_t)0);
  CHECK_EQ(p.yardMapWanted(), own);
  p.addCoins(400, false);
  CHECK(p.buyMap(other));
  CHECK_EQ(p.coins, (uint16_t)(400 - priceOfMap(other)));
  CHECK(p.ownsMap(other));
  CHECK(!p.buyMap(other));  // ya es suyo
  CHECK(!p.buyMap(0));
  CHECK(!p.buyMap(9));
  p.chooseMap(other);
  CHECK_EQ(p.yardChosen, other);
  CHECK_EQ(p.yardMapWanted(), other);
  p.chooseMap(0);  // 0: el inicial
  CHECK_EQ(p.yardChosen, (uint8_t)0);
  CHECK_EQ(p.yardMapWanted(), own);
}

// los mapas son del jugador: otro bicho en casa (otra especie, otro bioma) sigue teniendo lo comprado
// y ve el mismo patio
TEST(economy, los_mapas_valen_para_todos_los_bichos) {
  Pet p;
  eBorn(p);
  p.addCoins(500, false);
  CHECK(p.buyMap(3));
  p.chooseMap(3);
  int16_t first = p.speciesId;
  for (int16_t s = 0; s < 40; s++) {
    if (s == first) continue;
    p.speciesId = s;  // como si otro bicho ocupara la casa
    CHECK(p.ownsMap(3));
    CHECK(p.ownsMap(YARD_MAP_START));
    CHECK(!p.ownsMap(5));
    CHECK_EQ(p.yardMapWanted(), (uint8_t)3);
  }
}

// ---------------------------------------------------------------- guardado
TEST(economy, la_nvs_guarda_y_un_guardado_viejo_carga_a_cero) {
  Pet p;
  eBorn(p);
  uint8_t other = 3;
  p.addCoins(120, true);
  p.addCoins(500, false);
  CHECK(p.buyMap(other));
  p.chooseMap(other);
  p.flushSave();
  uint16_t coins = p.coins;
  Pet q;
  q.begin();
  CHECK_EQ(q.coins, coins);
  CHECK(q.ownsMap(other));
  CHECK_EQ(q.yardChosen, other);
  CHECK_EQ(q.yardOwned, (uint16_t)(1u << other));
  q.lastSeenEpoch = ET;
  CHECK_EQ(q.coinsEarnedToday(), (uint16_t)120);  // el contador del dia tambien
  {  // un guardado de antes: sin ninguna clave de monedas
    Preferences nv;
    nv.begin("tamapoke", false);
    CHECK(nv.remove("cn"));
    nv.remove("cnD");
    nv.remove("cnT");
    nv.remove("ybgO");
    nv.remove("ybg");
    nv.end();
  }
  Pet r;
  r.begin();
  CHECK_EQ(r.coins, (uint16_t)0);
  CHECK_EQ(r.yardOwned, (uint16_t)0);
  CHECK_EQ(r.yardChosen, (uint8_t)0);
  r.lastSeenEpoch = ET;
  CHECK_EQ(r.coinsEarnedToday(), (uint16_t)0);
  CHECK_EQ(r.yardMapWanted(), (uint8_t)YARD_MAP_START);  // un guardado viejo empieza en el prado
}

TEST(economy, una_eleccion_de_un_mapa_no_suyo_guardada_se_ignora) {
  Pet p;
  eBorn(p);
  {
    Preferences nv;
    nv.begin("tamapoke", false);
    nv.putUChar("ybg", 5);  // a mano, sin comprar
    nv.end();
  }
  Pet q;
  q.begin();
  CHECK(q.yardChosen != 0);
  CHECK_EQ(q.yardMapWanted(), (uint8_t)YARD_MAP_START);  // no es suyo: el inicial
}
