// Regalos (v1.55): lo que se encuentra caminando llega envuelto a casa y entra en la mochila al
// abrirlo, con el bicho despierto. Se guardan en la NVS, hasta GIFT_MAX; con la cola llena lo nuevo va
// derecho a la mochila con su aviso. Cuando la casa queda libre (un final, guardar en la caja) se
// desenvuelven solos: no hay quien los abra, y un huevo envuelto cuenta como huevo.
#include "framework.h"
#include "shim/Arduino.h"
#include "shim/Preferences.h"
#include "../pet.h"

static const uint32_t GDAY = 20000;  // un dia cualquiera (numero de dia unix)

static void gHatch(Pet &p) {
  p.eggTap();
  p.eggTap();
  p.eggTap();
}

// bicho recien nacido y despierto, con reloj y la NVS limpia
static void gBorn(Pet &p, int16_t dex = 4) {
  mockClearForcedRandom();
  mockNvsReset();
  mockSetMillis(0);
  p.begin();
  if (p.awaitingStarter()) p.chooseStarter(dex);
  gHatch(p);
  p.setClock(GDAY * 86400 + 10 * 3600);
}

TEST(gifts, abrir_uno_lo_pone_en_la_mochila_y_alegra) {
  Pet p;
  gBorn(p);
  CHECK(!p.canOpenGift());  // sin regalos
  CHECK(p.addGift(IT_CAKE));
  CHECK(p.addGift(IT_IRON));
  CHECK_EQ(p.giftCount(), (uint8_t)2);
  CHECK(p.canOpenGift());
  uint8_t joy = p.joy;
  bool full = true;
  CHECK_EQ(p.openGift(&full), (uint8_t)IT_CAKE);  // el primero que llego
  CHECK(!full);
  CHECK_EQ(p.items[IT_CAKE], (uint8_t)1);
  CHECK_EQ(p.giftCount(), (uint8_t)1);
  CHECK_EQ(p.giftItem(0), (uint8_t)IT_IRON);
  CHECK(p.showHeart());
  CHECK_EQ(p.joy, joy);  // abrir no da premio: no se puede exprimir
  bool gift = false;
  CHECK_EQ(p.peekFound(&full, &gift), (uint8_t)IT_CAKE);  // el aviso "REGALO!"
  CHECK(gift);
  CHECK(!full);
}

TEST(gifts, dormido_huevo_o_ceremonia_no_se_abren) {
  Pet p;
  gBorn(p);
  CHECK(p.addGift(IT_GOLDEN));
  p.toggleLight();
  CHECK(!p.canOpenGift());
  CHECK_EQ(p.openGift(), (uint8_t)IT_COUNT);
  CHECK_EQ(p.giftCount(), (uint8_t)1);
  p.toggleLight();
  p.startRunaway();
  CHECK(!p.canOpenGift());

  Pet e;  // un huevo en casa
  mockNvsReset();
  e.begin();
  e.chooseStarter(1);
  CHECK(e.addGift(IT_GOLDEN));
  CHECK(!e.canOpenGift());
}

TEST(gifts, sobreviven_a_un_reinicio) {
  Pet p;
  gBorn(p);
  CHECK(p.addGift(IT_EGG));
  CHECK(p.addGift(IT_CHARM));
  CHECK(p.savePending());  // como los pasos: no escribe, lo vuelca el loop
  p.flushSave();
  Pet q;
  q.begin();
  CHECK_EQ(q.giftCount(), (uint8_t)2);
  CHECK_EQ(q.giftItem(0), (uint8_t)IT_EGG);
  CHECK_EQ(q.giftItem(1), (uint8_t)IT_CHARM);
  CHECK_EQ(q.giftItem(2), (uint8_t)IT_COUNT);
  CHECK(q.openGift() == IT_EGG);
  Pet r;  // abrir guarda en el acto
  r.begin();
  CHECK_EQ(r.giftCount(), (uint8_t)1);
  CHECK_EQ(r.items[IT_EGG], (uint8_t)1);
}

TEST(gifts, una_partida_anterior_carga_sin_regalos) {
  Pet p;
  gBorn(p);
  p.flushSave();
  {
    Preferences raw;  // como la dejaba la v1.54: sin "gifts"
    raw.begin("tamapoke", false);
    raw.remove("gifts");
    raw.end();
  }
  Pet q;
  q.begin();
  CHECK_EQ(q.giftCount(), (uint8_t)0);
  {
    Preferences raw;  // y un blob con basura: solo entran objetos validos
    raw.begin("tamapoke", false);
    uint8_t g[GIFT_MAX + 1] = { 3, IT_GOLDEN, 200, IT_CAKE };
    raw.putBytes("gifts", g, sizeof(g));
    raw.end();
  }
  Pet r;
  r.begin();
  CHECK_EQ(r.giftCount(), (uint8_t)2);
  CHECK_EQ(r.giftItem(1), (uint8_t)IT_CAKE);
}

TEST(gifts, con_la_cola_llena_va_a_la_mochila_con_aviso) {
  Pet p;
  gBorn(p);
  for (int i = 0; i < GIFT_MAX; i++) CHECK(p.addGift(IT_GOLDEN));
  CHECK(!p.addGift(IT_GOLDEN));
  CHECK_EQ(p.giftCount(), (uint8_t)GIFT_MAX);
  mockForceRandom(0);  // un hallazgo: baya dorada
  p.addSteps(FIND_MIN_STEPS, GDAY);
  CHECK_EQ(p.giftCount(), (uint8_t)GIFT_MAX);
  CHECK_EQ(p.items[IT_GOLDEN], (uint8_t)1);
  CHECK(p.hasFound());
}

TEST(gifts, el_regalo_de_la_meta_tambien_llega_envuelto) {
  Pet p;
  gBorn(p);
  mockForceRandom(0);
  p.addSteps(STEP_GOALS[0], GDAY);  // 3000: 3 al azar y el de la meta
  CHECK_EQ(p.findsToday, (uint8_t)4);
  CHECK_EQ(p.giftCount(), (uint8_t)4);
  CHECK_EQ(p.items[IT_GOLDEN], (uint8_t)0);
}

TEST(gifts, un_huevo_envuelto_cuenta_al_soltar) {
  Pet p;
  gBorn(p);
  CHECK(p.releaseGivesEgg());  // sin huevos ni caja: soltar trae uno de regalo
  CHECK(p.addGift(IT_EGG));
  CHECK(!p.releaseGivesEgg());  // ya tiene uno, aunque este envuelto
  CHECK(p.addGift(IT_CAKE));
  p.release();
  mockAdvanceMillis(CEREMONY_MS + 1);
  p.update(millis());
  CHECK(p.isVacant());
  // se desenvuelven solos: el huevo para el boton de la casa vacia, sin otro de regalo
  CHECK_EQ(p.giftCount(), (uint8_t)0);
  CHECK_EQ(p.items[IT_EGG], (uint8_t)1);
  CHECK_EQ(p.items[IT_CAKE], (uint8_t)1);
  CHECK(!p.hasFound());
}

TEST(gifts, guardar_en_la_caja_usa_el_huevo_envuelto) {
  Pet p;
  gBorn(p);
  CHECK(p.addGift(IT_EGG));
  CHECK(p.store(0));
  CHECK(p.isEgg());
  CHECK(!p.isVacant());  // el huevo envuelto llego a casa
  CHECK_EQ(p.giftCount(), (uint8_t)0);
  CHECK_EQ(p.items[IT_EGG], (uint8_t)0);
}

TEST(gifts, lo_que_no_cabe_sigue_envuelto_al_desenvolver) {
  Pet p;
  gBorn(p);
  p.items[IT_GOLDEN] = ITEM_MAX;
  p.items[IT_CHARM] = 1;
  CHECK(p.addGift(IT_GOLDEN));
  CHECK(p.addGift(IT_CHARM));  // otro amuleto: se vuelve baya... que tampoco cabe
  CHECK(p.addGift(IT_EGG));
  p.boxPut(5, p.toRecord());  // alguien en la caja: soltar no trae huevo de regalo
  p.release();
  mockAdvanceMillis(CEREMONY_MS + 1);
  p.update(millis());
  CHECK_EQ(p.items[IT_EGG], (uint8_t)1);
  CHECK_EQ(p.giftCount(), (uint8_t)2);
  CHECK_EQ(p.giftItem(0), (uint8_t)IT_GOLDEN);
}

// ---------------------------------------------------------------- botin de combate (v1.56)
TEST(battle_drop, solo_al_ganar) {
  Pet p;
  gBorn(p);
  mockForceRandom(0);  // siempre toca (0 < 70), y la tabla da tarta
  CHECK_EQ(p.battleDrop(false), (uint8_t)IT_COUNT);
  CHECK_EQ(p.giftCount(), (uint8_t)0);
  CHECK_EQ(p.battleDrop(true), (uint8_t)IT_CAKE);
  CHECK_EQ(p.giftCount(), (uint8_t)1);  // envuelto, en casa
  CHECK_EQ(p.giftItem(0), (uint8_t)IT_CAKE);
  CHECK_EQ(p.items[IT_CAKE], (uint8_t)0);
}

TEST(battle_drop, setenta_por_ciento) {
  Pet p;
  gBorn(p);
  mockForceRandom(BATTLE_DROP_ODDS);  // 70: ya no toca
  CHECK_EQ(p.battleDrop(true), (uint8_t)IT_COUNT);
  CHECK_EQ(p.battleDropsToday(), (uint8_t)0);  // lo que no trae nada no gasta el tope
  mockForceRandom(BATTLE_DROP_ODDS - 1);
  CHECK(p.battleDrop(true) != IT_COUNT);
}

TEST(battle_drop, cinco_al_dia_y_al_dia_siguiente_otra_vez) {
  Pet p;
  gBorn(p);
  mockForceRandom(0);
  for (int i = 0; i < BATTLE_DROPS_PER_DAY; i++) CHECK_EQ(p.battleDrop(true), (uint8_t)IT_CAKE);
  CHECK_EQ(p.battleDropsToday(), (uint8_t)BATTLE_DROPS_PER_DAY);
  CHECK_EQ(p.battleDrop(true), (uint8_t)IT_COUNT);  // la sexta victoria del dia, nada
  CHECK_EQ(p.giftCount(), (uint8_t)BATTLE_DROPS_PER_DAY);
  CHECK(p.savePending());  // lo guarda quien cierra el combate: una sola escritura
  p.flushSave();
  Pet q;  // el tope sobrevive a un reinicio
  q.begin();
  q.setClock(GDAY * 86400 + 11 * 3600);  // el reloj, como lo pone el arranque con el RTC
  CHECK_EQ(q.battleDropsToday(), (uint8_t)BATTLE_DROPS_PER_DAY);
  CHECK_EQ(q.battleDrop(true), (uint8_t)IT_COUNT);
  q.setClock((GDAY + 1) * 86400 + 10 * 3600);  // otro dia
  CHECK_EQ(q.battleDropsToday(), (uint8_t)0);
  CHECK_EQ(q.battleDrop(true), (uint8_t)IT_CAKE);
}

TEST(battle_drop, la_tabla_y_el_huevo_es_lo_mas_raro) {
  Pet p;
  gBorn(p);
  mockClearForcedRandom();
  randomSeed(7);
  const int N = 100000;
  int n[IT_COUNT] = { 0 };
  for (int i = 0; i < N; i++) n[p.rollBattleItem()]++;
  auto pct = [&](int c) { return 100.0 * c / N; };
  CHECK_MSG(pct(n[IT_CAKE]) > 39 && pct(n[IT_CAKE]) < 41, "tarta " + std::to_string(pct(n[IT_CAKE])));
  CHECK_MSG(pct(n[IT_GOLDEN]) > 24 && pct(n[IT_GOLDEN]) < 26, "baya " + std::to_string(pct(n[IT_GOLDEN])));
  int vit = n[IT_PROTEIN] + n[IT_IRON] + n[IT_CARBOS];
  CHECK_MSG(pct(vit) > 20 && pct(vit) < 22, "vitaminas " + std::to_string(pct(vit)));
  CHECK(n[IT_PROTEIN] > 0 && n[IT_IRON] > 0 && n[IT_CARBOS] > 0);
  CHECK_MSG(pct(n[IT_RARECANDY]) > 6.5 && pct(n[IT_RARECANDY]) < 7.5, "caramelo " + std::to_string(pct(n[IT_RARECANDY])));
  CHECK_MSG(pct(n[IT_CHARM]) > 3.5 && pct(n[IT_CHARM]) < 4.5, "amuleto " + std::to_string(pct(n[IT_CHARM])));
  CHECK_MSG(pct(n[IT_EGG]) > 2.6 && pct(n[IT_EGG]) < 3.4, "huevo " + std::to_string(pct(n[IT_EGG])));
  for (uint8_t it = 0; it < IT_COUNT; it++)  // el huevo es lo que menos sale de todo lo que sale
    if (it != IT_EGG && n[it]) CHECK(n[IT_EGG] < n[it]);
  p.items[IT_CHARM] = 1;  // con un amuleto ya, el que tocaria es una tarta
  mockForceRandom(BDROP_CAKE + BDROP_GOLDEN + BDROP_VITAMIN + BDROP_RARECANDY);
  CHECK_EQ(p.rollBattleItem(), (uint8_t)IT_CAKE);
  p.items[IT_CHARM] = 0;
  CHECK_EQ(p.rollBattleItem(), (uint8_t)IT_CHARM);
  mockForceRandom(99);
  CHECK_EQ(p.rollBattleItem(), (uint8_t)IT_EGG);
}

TEST(gifts, uno_que_no_cabe_no_tapa_a_los_de_detras) {
  Pet p;
  gBorn(p);
  p.items[IT_EGG] = ITEM_MAX;
  CHECK(p.addGift(IT_EGG));   // no cabe: los huevos solo salen de la mochila con la casa libre
  CHECK(p.addGift(IT_CAKE));
  bool full = true;
  CHECK_EQ(p.openGift(&full), (uint8_t)IT_CAKE);
  CHECK(!full);
  CHECK_EQ(p.items[IT_CAKE], (uint8_t)1);
  CHECK_EQ(p.giftCount(), (uint8_t)1);
  CHECK_EQ(p.giftItem(0), (uint8_t)IT_EGG);
  CHECK_EQ(p.openGift(&full), (uint8_t)IT_EGG);  // solo queda el que no cabe: lo dice y sigue ahi
  CHECK(full);
  CHECK_EQ(p.giftCount(), (uint8_t)1);
}
