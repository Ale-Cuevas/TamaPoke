// Tests de las medallas del jugador (v3.4, medals.h): los tramos y lo que pagan (de tuning.h), contar a cada
// bicho una sola vez (casa, caja, cambios de hueco), los contadores de evento, la migracion de una partida de
// antes y los cortes de luz (nunca se paga dos veces).
#include "framework.h"
#include "shim/Arduino.h"
#include "shim/Preferences.h"
#include "../pet.h"
#include "../medals.h"
#include <string.h>
#include <string>

static const uint32_t M_DAY = 20000u * 86400;  // un dia cualquiera, a medianoche
static const uint32_t M_T = M_DAY + 10 * 3600;

static void mAdvance(Pet &p, uint32_t minutes) {
  for (uint32_t i = 0; i < minutes; i++) {
    mockAdvanceMillis(PET_TICK_MS);
    p.update(millis());
  }
}

static void mTap3(Pet &p) {
  p.eggTap();
  p.eggTap();
  p.eggTap();
}

// partida nueva con el bicho dex nacido en el hueco 0, el reloj puesto, huevos en la mochila y 0 monedas
static void mBorn(Pet &p, int16_t dex = 4) {
  mockClearForcedRandom();
  mockNvsReset();
  mockSetMillis(1000);
  mockSerialSilence(true);
  p.begin();
  if (p.awaitingStarter()) p.chooseStarter(dex);
  p.items[IT_EGG] = ITEM_MAX;
  mTap3(p);
  p.speciesId = dex;
  p.setClock(M_T);
  p.setCoins(0);
}

// un bicho inventado en el hueco k de la caja. legacy: como los de antes de la v3.4 (sin REC_V3, con las
// medallas de antes en `medals`, que no valen nada)
static uint32_t mBox(Pet &p, uint8_t k, int16_t dex, uint16_t lv, uint8_t bond, uint8_t weight, uint8_t mistakes,
                     bool fav, bool legacy) {
  PetRecord r;
  memset(&r, 0, sizeof(r));
  r.speciesId = dex;
  r.growMin = levelStart(lv);
  r.ageMinutes = r.growMin + 10;
  r.bond = bond;
  r.weight = weight;
  r.careMistakes = mistakes;
  r.fullness = r.joy = r.energy = 80;
  r.geneAtk = r.geneDef = r.geneSpe = 100;
  r.flags = (fav ? REC_FAV6 : 0) | (legacy ? 0 : REC_V3);
  r.medals = legacy ? 0x1FF : 0;  // (las MED_* de antes: se ignoran)
  CHECK(p.boxPut(k, r));
  return p.boxRecord(k).uid;
}

// guarda (rename guarda el estado entero; con una copia del nombre: strncpy no admite el mismo buffer)
static void mSave(Pet &p) {
  char nm[sizeof(p.nick)];
  memcpy(nm, p.nick, sizeof(nm));
  p.rename(nm);
}

// la NVS como la dejaba la v3.3: sin "pm" ni "pmc"
static void mMakeLegacy() {
  Preferences x;
  x.begin("tamapoke", false);
  x.remove("pm");
  x.remove("pmc");
  x.putUShort("medal", 0x1FF);  // las medallas de cada bicho de antes
  x.putUShort("tmedal", 12);
  x.end();
}

// lo que se paga de una vez por llegar a los valores de ahora desde nada
static uint32_t fullPayout(Pet &p) {
  uint32_t c = 0;
  for (uint8_t i = 0; i < MEDAL_COUNT; i++) c += medalPayout(0, medalTierOf(i, p.medalValue(i)));
  return c;
}

// ---------------------------------------------------------------- logica pura
TEST(medals, los_tramos_salen_de_tuning) {
  for (uint8_t i = 0; i < MEDAL_COUNT; i++) {
    const uint32_t *t = MEDAL_T[i];
    CHECK(t[0] > 0 && t[0] < t[1] && t[1] < t[2]);
    CHECK_EQ(medalTierOf(i, 0), (uint8_t)0);
    CHECK_EQ(medalTierOf(i, t[0] - 1), (uint8_t)0);
    CHECK_EQ(medalTierOf(i, t[0]), (uint8_t)MTIER_BRONZE);
    CHECK_EQ(medalTierOf(i, t[1] - 1), (uint8_t)MTIER_BRONZE);
    CHECK_EQ(medalTierOf(i, t[1]), (uint8_t)MTIER_SILVER);
    CHECK_EQ(medalTierOf(i, t[2]), (uint8_t)MTIER_GOLD);
    CHECK_EQ(medalTierOf(i, 0xFFFFFFFFu), (uint8_t)MTIER_GOLD);
    CHECK_EQ(medalNext(i, 0), t[0]);
    CHECK_EQ(medalNext(i, t[0]), t[1]);
    CHECK_EQ(medalNext(i, t[2]), (uint32_t)0);
  }
  CHECK_EQ(medalPayout(0, 3), (uint16_t)(MEDAL_COINS[0] + MEDAL_COINS[1] + MEDAL_COINS[2]));
  CHECK_EQ(medalPayout(1, 2), (uint16_t)MEDAL_COINS[1]);
  CHECK_EQ(medalPayout(2, 2), (uint16_t)0);
  CHECK_EQ(medalCoins(0), (uint16_t)0);
}

// la tabla del plan: 19 medallas, 7 de bichos, 6 de evento y 6 derivadas (pinta la promesa a proposito)
TEST(medals, las_fuentes_de_cada_medalla) {
  CHECK_EQ((int)MEDAL_COUNT, 19);
  int n[3] = { 0, 0, 0 };
  for (uint8_t i = 0; i < MEDAL_COUNT; i++) n[medalSource(i)]++;
  CHECK_EQ(n[MSRC_PET], 7);
  CHECK_EQ(n[MSRC_EVENT], 6);
  CHECK_EQ(n[MSRC_DERIVED], 6);
  for (uint8_t i = 0; i < PMB_COUNT; i++) CHECK_EQ(medalSource(i), (uint8_t)MSRC_PET);
}

TEST(medals, lo_que_alcanza_un_bicho) {
  // (los niveles, el vinculo y "en forma" salen de tuning.h; aqui se da por hecho que MEDAL_FIT_LV es el
  // primer nivel, como en la tabla del plan)
  const uint16_t L10 = MEDAL_PET_LV[0];
  const uint8_t W = MEDAL_FIT_WEIGHT, M = MEDAL_FIT_MISTAKES;
  CHECK_EQ(medalPetBits(L10 - 1, MEDAL_BOND - 1, false, W, M, false), (uint8_t)0);
  CHECK_EQ(medalPetBits(L10, 0, false, W, M, false), (uint8_t)(PMB_LV10 | (MEDAL_FIT_LV <= L10 ? PMB_FIT : 0)));
  CHECK_EQ(medalPetBits(L10, 0, false, W + 1, M, false), (uint8_t)PMB_LV10);   // con peso no esta en forma
  CHECK_EQ(medalPetBits(L10, 0, false, W, M + 1, false), (uint8_t)PMB_LV10);   // ni con un descuido de mas
  CHECK_EQ(medalPetBits(MEDAL_FIT_LV - 1, 0, false, W, M, false) & PMB_FIT, 0);  // ni antes de su nivel
  CHECK_EQ(medalPetBits(MEDAL_PET_LV[2], MEDAL_BOND, true, W + 3, M + 2, true),
           (uint8_t)(PMB_LV10 | PMB_LV25 | PMB_LV50 | PMB_BOND | PMB_FINAL | PMB_FAV));
}

TEST(medals, el_blob_pm_con_crc_y_version) {
  PlayerMedals m;
  pmClear(m);
  CHECK(pmValid(m));
  m.tier[3] = 2;
  m.cnt[7] = 5;
  CHECK(!pmValid(m));  // sin sellar: el CRC no cuadra
  pmSeal(m);
  CHECK(pmValid(m));
  PlayerMedals bad = m;
  bad.ver = PM_VER + 1;
  bad.crc = pmCrc(bad);
  CHECK(!pmValid(bad));  // otra version
  bad = m;
  bad.tier[0] = 4;
  bad.crc = pmCrc(bad);
  CHECK(!pmValid(bad));  // un tramo que no existe
  bad = m;
  bad.legN = PM_LEGACY_MAX + 1;
  bad.crc = pmCrc(bad);
  CHECK(!pmValid(bad));
}

TEST(medals, un_bicho_cuenta_una_vez) {
  PlayerMedals m;
  pmClear(m);
  uint8_t counted = 0;
  CHECK_EQ(pmCountPet(m, counted, PMB_LV10 | PMB_FIT), (uint8_t)(PMB_LV10 | PMB_FIT));
  CHECK_EQ(pmCountPet(m, counted, PMB_LV10 | PMB_FIT | PMB_LV25), (uint8_t)PMB_LV25);
  CHECK_EQ(pmCountPet(m, counted, PMB_LV10), (uint8_t)0);  // perder el "en forma" no descuenta
  CHECK_EQ(m.cnt[MD_LV10], (uint16_t)1);
  CHECK_EQ(m.cnt[MD_LV25], (uint16_t)1);
  CHECK_EQ(m.cnt[MD_FIT], (uint16_t)1);
  // la lista de los registros de antes
  CHECK(pmLegacyAdd(m, 77, PMB_LV10));
  CHECK(pmLegacyAdd(m, 77, PMB_FAV));
  CHECK_EQ(pmLegacyBits(m, 77), (uint8_t)(PMB_LV10 | PMB_FAV));
  CHECK_EQ(pmLegacyBits(m, 78), (uint8_t)0);
  CHECK_EQ(pmLegacyBits(m, 0), (uint8_t)0);
  for (uint32_t u = 1; u < PM_LEGACY_MAX; u++) CHECK(pmLegacyAdd(m, 100 + u, 1));
  CHECK(!pmLegacyAdd(m, 999, 1));  // llena
}

TEST(medals, los_banners_se_funden_por_medalla) {
  MedalNews q[MEDAL_COUNT];
  uint8_t n = 0;
  medalNewsPush(q, n, MEDAL_COUNT, MD_DEX, 1, 25);
  medalNewsPush(q, n, MEDAL_COUNT, MD_WILD, 1, 25);
  medalNewsPush(q, n, MEDAL_COUNT, MD_DEX, 2, 75);
  CHECK_EQ(n, (uint8_t)2);
  CHECK_EQ(q[0].medal, (uint8_t)MD_DEX);
  CHECK_EQ(q[0].tier, (uint8_t)2);
  CHECK_EQ(q[0].coins, (uint16_t)100);
  for (uint8_t i = 0; i < MEDAL_COUNT + 3; i++) medalNewsPush(q, n, MEDAL_COUNT, i % MEDAL_COUNT, 1, 1);
  CHECK_EQ(n, (uint8_t)MEDAL_COUNT);  // una por medalla: nunca se pasa
}

TEST(medals, el_numero_corto) {
  char b[12];
  medalShortNum(b, sizeof(b), 999);
  CHECK_STREQ(b, "999");
  medalShortNum(b, sizeof(b), 52310);
  CHECK_STREQ(b, "52k");
  medalShortNum(b, sizeof(b), 250000);
  CHECK_STREQ(b, "250k");
  medalShortNum(b, sizeof(b), 1000000);
  CHECK_STREQ(b, "1M");
  medalShortNum(b, sizeof(b), 1200000);
  CHECK_STREQ(b, "1.2M");
}

// ---------------------------------------------------------------- pagos
TEST(medals, cada_tramo_se_paga_una_vez_y_fuera_del_tope) {
  Pet p;
  mBorn(p);
  p.addCoins(ECO_DAILY_CAP, true);  // el tope del dia, lleno
  uint16_t c0 = p.coins, today = p.coinsEarnedToday();
  const uint32_t *t = MEDAL_T[MD_EVOLVE];
  p.dbgMedalCount(MD_EVOLVE, (uint16_t)t[0]);
  CHECK_EQ(p.medalTierPaid(MD_EVOLVE), (uint8_t)MTIER_BRONZE);
  CHECK_EQ(p.coins, (uint16_t)(c0 + MEDAL_COINS[0]));
  CHECK_EQ(p.coinsEarnedToday(), today);  // no cuenta para el tope
  MedalNews n;
  CHECK(p.takeMedalNews(&n));
  CHECK_EQ(n.medal, (uint8_t)MD_EVOLVE);
  CHECK_EQ(n.tier, (uint8_t)MTIER_BRONZE);
  CHECK_EQ(n.coins, (uint16_t)MEDAL_COINS[0]);
  // mirar otra vez, guardar o reiniciar no paga de nuevo
  p.medalsUpdate();
  mSave(p);
  CHECK_EQ(p.coins, (uint16_t)(c0 + MEDAL_COINS[0]));
  CHECK(!p.takeMedalNews(&n));
  Pet q;
  q.begin();
  CHECK(!q.medalsMigratedNow());
  CHECK_EQ(q.medalTierPaid(MD_EVOLVE), (uint8_t)MTIER_BRONZE);
  CHECK_EQ(q.medalValue(MD_EVOLVE), t[0]);
  q.medalsUpdate();
  CHECK_EQ(q.coins, (uint16_t)(c0 + MEDAL_COINS[0]));
  CHECK_EQ(q.medalNewsPending(), (uint8_t)0);
  // de golpe hasta el Oro: Plata y Oro enteros, un solo banner
  q.dbgMedalCount(MD_EVOLVE, (uint16_t)t[2]);
  CHECK_EQ(q.coins, (uint16_t)(c0 + MEDAL_COINS[0] + MEDAL_COINS[1] + MEDAL_COINS[2]));
  CHECK(q.takeMedalNews(&n));
  CHECK_EQ(n.tier, (uint8_t)MTIER_GOLD);
  CHECK_EQ(n.coins, (uint16_t)(MEDAL_COINS[1] + MEDAL_COINS[2]));
  CHECK(!q.takeMedalNews(&n));
}

// el tick cuenta y paga en RAM, sin escribir (el loop guarda con la pantalla atenuada)
TEST(medals, el_tick_no_escribe_en_flash) {
  Pet p;
  mBorn(p);
  p.weight = MEDAL_FIT_WEIGHT + 5;  // (sin el "en forma")
  p.growMin = levelStart(MEDAL_PET_LV[0]) - 1;
  mSave(p);
  uint32_t before = p.medalValue(MD_LV10);
  long w0 = mockNvsWrites();
  mAdvance(p, 1);
  CHECK_EQ(p.level(), (uint16_t)10);
  CHECK_EQ(mockNvsWrites(), w0);
  CHECK(p.counted & PMB_LV10);
  CHECK_EQ(p.medalValue(MD_LV10), before + 1);
  CHECK(p.savePending());
}

// ---------------------------------------------------------------- contar una vez por bicho
TEST(medals, guardar_despertar_y_cambiar_de_hueco_nunca_cuenta_dos_veces) {
  Pet p;
  mBorn(p, 4);
  p.setLevel(MEDAL_PET_LV[1] + 5);
  p.bond = MEDAL_BOND;
  p.weight = 0;
  mSave(p);
  uint8_t want = PMB_LV10 | PMB_LV25 | PMB_BOND | PMB_FIT;
  CHECK_EQ(p.counted, want);
  uint32_t v[PMB_COUNT];
  for (uint8_t i = 0; i < PMB_COUNT; i++) v[i] = p.medalValue(i);
  CHECK_EQ(v[MD_LV10], (uint32_t)1);
  uint32_t a = p.uid;
  auto same = [&](Pet &x, const char *where) {
    for (uint8_t i = 0; i < PMB_COUNT; i++)
      CHECK_MSG(x.medalValue(i) == v[i], std::string(where) + ": medalla " + std::to_string(i));
  };
  CHECK(p.store(0, 0));      // a la caja (en casa, un huevo de la mochila)
  same(p, "guardado");
  CHECK(p.boxRecord(0).flags & REC_V3);
  CHECK_EQ((uint8_t)p.boxRecord(0).medals, want);
  CHECK(p.wake(0, 1));       // al hueco 1 (vacio)
  CHECK_EQ(p.home(1).uid, a);
  CHECK_EQ(p.home(1).counted, want);
  same(p, "despierto en el 1");
  Pet q;  // reiniciar: el hueco 1 vuelve de "hm1" con lo contado
  q.begin();
  CHECK_EQ(q.home(1).counted, want);
  same(q, "reinicio");
  mTap3(q);  // el huevo del 0 nace
  CHECK(q.home(0).hatched());
  CHECK(q.store(1, 1));      // el de casa a la caja otra vez
  same(q, "guardado desde el 1");
  CHECK(q.wake(1, 0));       // y se cambia con el del 0
  CHECK_EQ(q.home(0).uid, a);
  same(q, "cambio al 0");
  Pet r;  // el hueco 0 recuerda lo contado ("pmc")
  r.begin();
  CHECK_EQ(r.home(0).uid, a);
  CHECK_EQ(r.home(0).counted, want);
  same(r, "reinicio con el 0");
  mAdvance(r, 2);
  same(r, "dos minutos despues");
}

// uno que crece en la caja (estadio, salvaje, viaje) cuenta al despertarlo, sin escribir la caja en el tick
TEST(medals, el_que_crece_en_la_caja_cuenta_al_despertar) {
  Pet p;
  mBorn(p, 1);
  uint32_t b = mBox(p, 2, 4, MEDAL_PET_LV[0] - 1, 30, MEDAL_FIT_WEIGHT + 4, 0, false, false);  // Charmander Lv.9, aun sin contar
  uint32_t t[1] = { b };
  uint32_t before = p.medalValue(MD_LV10);
  p.stageDone(0, true, t, 1);  // +STAGE_GROW_MIN en su registro
  CHECK(levelFor(p.boxRecord(2).growMin) >= 10);
  CHECK_EQ(p.medalValue(MD_LV10), before);  // en la caja: aun no
  mAdvance(p, 3);
  CHECK_EQ(p.medalValue(MD_LV10), before);
  CHECK(p.wake(2, 1));
  CHECK_EQ(p.medalValue(MD_LV10), before + 1);  // al despertarlo
  CHECK(p.home(1).counted & PMB_LV10);
}

// ---------------------------------------------------------------- contadores de evento
TEST(medals, los_contadores_de_evento) {
  Pet p;
  mBorn(p, 4);  // Charmander
  // evolucionar
  p.setLevel(16);
  p.fullness = p.joy = p.energy = 80;
  CHECK(p.canEvolveNow());
  p.evolve();
  CHECK_EQ(p.medalValue(MD_EVOLVE), (uint32_t)1);
  // 10000 pasos: una vez al dia
  p.addSteps(STEP_GOALS[STEP_GOAL_TIERS - 1], p.dayNumber());
  CHECK_EQ(p.medalValue(MD_MARATHON), (uint32_t)1);
  p.addSteps(5000, p.dayNumber());
  CHECK_EQ(p.medalValue(MD_MARATHON), (uint32_t)1);
  p.setClock(M_T + 86400);
  p.addSteps(STEP_GOALS[STEP_GOAL_TIERS - 1], p.dayNumber());
  CHECK_EQ(p.medalValue(MD_MARATHON), (uint32_t)2);
  // ganar a un salvaje y capturarlo
  CHECK(p.encSpawnNow(16));
  EncReward r = p.encounterDone(true, p.uid);
  CHECK(r.won);
  CHECK_EQ(p.medalValue(MD_WILD), (uint32_t)1);
  p.items[IT_BALL] = 3;
  p.items[IT_EGG] = 0;
  CHECK(p.captureWild());
  CHECK_EQ(p.medalValue(MD_CATCH), (uint32_t)1);
  // perder no cuenta
  CHECK(p.encSpawnNow(16));
  p.encounterDone(false, p.uid);
  CHECK_EQ(p.medalValue(MD_WILD), (uint32_t)1);
  // un viaje que vuelve
  mBox(p, 0, 25, 12, 10, 0, 0, false, false);
  CHECK(p.startTravel(0, 1, 0));
  p.travelFinishNow();
  CHECK_EQ(p.medalValue(MD_TRIPS), (uint32_t)1);
  // una despedida completa (la ceremonia acaba en update)
  p.startFarewell();
  mockAdvanceMillis(CEREMONY_MS + 1);
  p.update(millis());
  CHECK_EQ(p.medalValue(MD_FAREWELL), (uint32_t)1);
  // y todo sobrevive a un reinicio
  Pet q;
  q.begin();
  CHECK_EQ(q.medalValue(MD_EVOLVE), (uint32_t)1);
  CHECK_EQ(q.medalValue(MD_MARATHON), (uint32_t)2);
  CHECK_EQ(q.medalValue(MD_WILD), (uint32_t)1);
  CHECK_EQ(q.medalValue(MD_CATCH), (uint32_t)1);
  CHECK_EQ(q.medalValue(MD_TRIPS), (uint32_t)1);
  CHECK_EQ(q.medalValue(MD_FAREWELL), (uint32_t)1);
}

TEST(medals, las_derivadas_leen_el_estado) {
  Pet p;
  mBorn(p, 4);
  uint32_t dex = p.registeredCount();
  CHECK_EQ(p.medalValue(MD_DEX), dex);
  p.dbgRegister(150, true);
  CHECK_EQ(p.medalValue(MD_DEX), dex + 1);
  CHECK_EQ(p.medalValue(MD_SHINY), (uint32_t)1);
  p.stepsTotal = 123456;
  CHECK_EQ(p.medalValue(MD_WALKER), (uint32_t)123456);
  p.bestStreak = 9;
  CHECK_EQ(p.medalValue(MD_STREAK), (uint32_t)9);
  p.setBadges(0x07);
  CHECK_EQ(p.medalValue(MD_STAGES), (uint32_t)3);
  p.setSouvenirMask(0x1F);
  CHECK_EQ(p.medalValue(MD_SOUV), (uint32_t)5);
  p.medalsUpdate();
  CHECK_EQ(p.medalTierPaid(MD_WALKER), medalTierOf(MD_WALKER, 123456));
  CHECK_EQ(p.medalTierPaid(MD_STREAK), medalTierOf(MD_STREAK, 9));
}

// v3.15: 45 recuerdos. Promesa de diseno: el Oro de Coleccionista es tenerlos todos, y los 45 dan para el
TEST(medals, coleccionista_oro_con_los_45_recuerdos) {
  CHECK_EQ(MEDAL_T[MD_SOUV][MEDAL_TIERS - 1], (uint32_t)SOUV_COUNT);
  Pet p;
  mBorn(p, 4);
  p.setSouvenirMask((1ull << 44) | 0x1Full);  // 6, uno de ellos del distrito toxico
  CHECK_EQ(p.medalValue(MD_SOUV), (uint32_t)6);
  p.setSouvenirMask(SOUV_ALL);
  CHECK_EQ(p.medalValue(MD_SOUV), (uint32_t)SOUV_COUNT);
  p.medalsUpdate();
  CHECK_EQ(p.medalTierPaid(MD_SOUV), (uint8_t)MTIER_GOLD);
}

// ---------------------------------------------------------------- la migracion (partida de la v3.3)
// dos en casa y dos en la caja de antes:
//   hueco 0: Charmander Nv.30, vinculo 100, peso 0, sin descuidos -> Nv.10, Nv.25, vinculo, en forma
//   hueco 1: Snorlax Nv.12 (forma final), peso 5, un descuido      -> Nv.10, forma final
//   caja 0:  Pikachu Nv.55, favorita descubierta, peso 3           -> Nv.10, Nv.25, Nv.50, favorita
//   caja 1:  Bulbasaur Nv.5                                        -> nada (aun)
struct Legacy {
  uint32_t uidK0, uidK1;
};
static Legacy mLegacySave() {
  Legacy L;
  Pet p;
  mBorn(p, 4);
  p.setLevel(MEDAL_PET_LV[1] + 5);
  p.bond = MEDAL_BOND;
  p.weight = 0;
  p.careMistakes = 0;
  mBox(p, 5, 143, MEDAL_PET_LV[0] + 2, 40, MEDAL_FIT_WEIGHT + 5, MEDAL_FIT_MISTAKES + 1, false, false);
  CHECK(p.wake(5, 1));  // al hueco 1
  p.slotRef(1).setLevel(MEDAL_PET_LV[0] + 2);
  p.slotRef(1).weight = MEDAL_FIT_WEIGHT + 5;
  p.slotRef(1).careMistakes = MEDAL_FIT_MISTAKES + 1;
  L.uidK0 = mBox(p, 0, 25, MEDAL_PET_LV[2] + 5, 20, MEDAL_FIT_WEIGHT + 3, MEDAL_FIT_MISTAKES + 2, true, true);
  L.uidK1 = mBox(p, 1, 1, MEDAL_PET_LV[0] > 5 ? MEDAL_PET_LV[0] - 5 : 1, 10, 0, 0, false, true);
  p.setCoins(0);  // (guarda)
  mMakeLegacy();
  return L;
}

TEST(medals, la_migracion_cuenta_casa_y_caja_una_vez_y_paga_todo) {
  Legacy L = mLegacySave();
  Pet q;
  q.begin();
  CHECK(q.medalsMigratedNow());
  CHECK_EQ(q.medalValue(MD_LV10), (uint32_t)3);
  CHECK_EQ(q.medalValue(MD_LV25), (uint32_t)2);
  CHECK_EQ(q.medalValue(MD_LV50), (uint32_t)1);
  CHECK_EQ(q.medalValue(MD_BOND), (uint32_t)1);
  CHECK_EQ(q.medalValue(MD_FINAL), (uint32_t)1);
  CHECK_EQ(q.medalValue(MD_FIT), (uint32_t)1);
  CHECK_EQ(q.medalValue(MD_FAV), (uint32_t)1);
  for (uint8_t i = PMB_COUNT; i < MEDAL_COUNT; i++)
    if (medalSource(i) == MSRC_EVENT) CHECK_EQ(q.medalValue(i), (uint32_t)0);  // los de evento empiezan a 0
  // cada tramo alcanzado, pagado entero, y un banner por medalla
  uint32_t pay = fullPayout(q);
  CHECK(pay > 0);
  CHECK_EQ((uint32_t)q.coins, pay);
  uint8_t medalsWithTier = 0;
  for (uint8_t i = 0; i < MEDAL_COUNT; i++) {
    CHECK_EQ(q.medalTierPaid(i), medalTierOf(i, q.medalValue(i)));
    medalsWithTier += q.medalTierPaid(i) > 0;
  }
  CHECK_EQ(q.medalNewsPending(), medalsWithTier);
  uint32_t sum = 0;
  MedalNews n;
  while (q.takeMedalNews(&n)) sum += n.coins;
  CHECK_EQ(sum, pay);
  // lo de la caja de antes, apuntado
  CHECK_EQ(q.legacyBitsOf(L.uidK0), (uint8_t)(PMB_LV10 | PMB_LV25 | PMB_LV50 | PMB_FAV));
  CHECK_EQ(q.legacyBitsOf(L.uidK1), (uint8_t)0);
  {  // las claves de las medallas de antes, borradas
    Preferences x;
    x.begin("tamapoke", true);
    CHECK(!x.isKey("medal"));
    CHECK(!x.isKey("tmedal"));
    CHECK(x.isKey("pm"));
  }
  // otro arranque no migra ni paga otra vez
  Pet r;
  r.begin();
  CHECK(!r.medalsMigratedNow());
  CHECK_EQ((uint32_t)r.coins, pay);
  CHECK_EQ(r.medalValue(MD_LV10), (uint32_t)3);
}

TEST(medals, uno_de_la_caja_de_antes_no_cuenta_dos_veces_al_despertar) {
  Legacy L = mLegacySave();
  Pet q;
  q.begin();
  uint32_t v[PMB_COUNT];
  for (uint8_t i = 0; i < PMB_COUNT; i++) v[i] = q.medalValue(i);
  uint16_t c = q.coins;
  // Pikachu (contado al migrar) se cambia con el del hueco 0: nada sube
  CHECK(q.wake(0, 0));
  CHECK_EQ(q.home(0).uid, L.uidK0);
  for (uint8_t i = 0; i < PMB_COUNT; i++) CHECK_EQ(q.medalValue(i), v[i]);
  CHECK_EQ(q.home(0).counted, (uint8_t)(PMB_LV10 | PMB_LV25 | PMB_LV50 | PMB_FAV));
  CHECK_EQ(q.coins, c);
  // Bulbasaur crecio en la caja hasta el 12 (como con un estadio o un viaje): cuenta al despertarlo, una vez
  PetRecord r = q.boxRecord(1);
  CHECK_EQ(r.uid, L.uidK1);
  r.growMin = levelStart(MEDAL_PET_LV[0] + 2);
  recordSeal(r);
  {
    Preferences x;
    x.begin("tamapoke", false);
    x.putBytes("box1", &r, sizeof(r));
  }
  Pet w;
  w.begin();
  CHECK(w.wake(1, 1));
  CHECK_EQ(w.medalValue(MD_LV10), v[MD_LV10] + 1);
  CHECK(w.store(2, 1));
  CHECK(w.wake(2, 1));
  CHECK_EQ(w.medalValue(MD_LV10), v[MD_LV10] + 1);
}

// un "pm" danado (CRC, version, tamano): se vuelve a contar como una migracion, sin romper nada
TEST(medals, un_pm_danado_se_vuelve_a_contar) {
  for (int kind = 0; kind < 3; kind++) {
    mLegacySave();
    Pet a;
    a.begin();  // migra y escribe "pm"
    uint32_t lv10 = a.medalValue(MD_LV10);
    uint16_t coins = a.coins;
    {
      Preferences x;
      x.begin("tamapoke", false);
      PlayerMedals m;
      CHECK_EQ(x.getBytes("pm", &m, sizeof(m)), sizeof(m));
      if (kind == 0) {
        m.cnt[MD_LV10] = 999;  // CRC malo
      } else if (kind == 1) {
        m.ver = PM_VER + 1;    // version desconocida
        m.crc = pmCrc(m);
      }
      if (kind < 2) x.putBytes("pm", &m, sizeof(m));
      else x.putBytes("pm", &m, 5);  // cortado
    }
    Pet b;
    b.begin();
    CHECK(b.medalsMigratedNow());
    CHECK_EQ(b.medalValue(MD_LV10), lv10);
    // un "pm" que existe pero no se puede leer se recuenta SIN pagar: lo alcanzado se da por cobrado
    CHECK_EQ(b.coins, coins);
    CHECK_EQ(b.medalNewsPending(), (uint8_t)0);
    for (uint8_t i = 0; i < MEDAL_COUNT; i++) CHECK_EQ(b.medalTierPaid(i), medalTierOf(i, b.medalValue(i)));
    Pet c;
    c.begin();
    CHECK(!c.medalsMigratedNow());
    CHECK_EQ(c.coins, b.coins);
  }
}

// ---------------------------------------------------------------- cortes de luz
// Un corte en cada escritura del arranque que migra: al volver, cada bicho cuenta una vez, los tramos son
// los de la partida, y las monedas son las de antes o las de antes + todo el pago (nunca el doble)
TEST(medals, corte_de_luz_en_la_migracion_nunca_paga_dos_veces) {
  mLegacySave();
  mockNvsSnapshot();
  long total = 0;
  uint32_t pay = 0, lv10 = 0, lv25 = 0;
  {
    Pet a;
    long w0 = mockNvsWrites();
    a.begin();
    total = mockNvsWrites() - w0;
    pay = a.coins;
    lv10 = a.medalValue(MD_LV10);
    lv25 = a.medalValue(MD_LV25);
  }
  CHECK(total > 2);
  CHECK(pay > 0);
  int sawPaid = 0, sawLost = 0;
  for (long n = 0; n <= total; n++) {
    mockNvsRestore();
    {
      mockNvsCutAfter(n);
      Pet a;
      a.begin();
      mockNvsCutAfter(-1);
    }
    Pet b;
    b.begin();
    std::string at = "corte " + std::to_string(n) + ": ";
    CHECK_MSG(b.coins == 0 || b.coins == pay, at + "monedas " + std::to_string(b.coins));
    sawPaid += b.coins == pay;
    sawLost += b.coins == 0;
    CHECK_MSG(b.medalValue(MD_LV10) == lv10, at + "Nv.10 contado " + std::to_string(b.medalValue(MD_LV10)));
    CHECK_MSG(b.medalValue(MD_LV25) == lv25, at + "Nv.25");
    for (uint8_t i = 0; i < MEDAL_COUNT; i++)
      CHECK_MSG(b.medalTierPaid(i) == medalTierOf(i, b.medalValue(i)), at + "tramo " + std::to_string(i));
    Pet c;  // y un tercer arranque no cambia nada
    c.begin();
    CHECK_MSG(c.coins == b.coins, at + "el tercer arranque pago");
    CHECK_MSG(c.medalValue(MD_LV10) == lv10, at + "el tercer arranque conto");
  }
  CHECK(sawPaid > 0);
  CHECK(sawLost > 0);  // el corte entre "pm" y "cn": el pago se pierde (aceptado)
}

// Un corte en cada escritura de un guardado que paga (la evolucion que llega al Bronce): las monedas de
// antes o con el Bronce, el tramo apuntado si se cobro, y nunca dos veces
TEST(medals, corte_de_luz_en_un_pago_nunca_paga_dos_veces) {
  Pet p;
  mBorn(p, 4);
  p.setLevel(16);
  p.fullness = p.joy = p.energy = 80;
  const uint32_t t0 = MEDAL_T[MD_EVOLVE][0];
  p.dbgMedalCount(MD_EVOLVE, (uint16_t)(t0 - 1));
  mSave(p);
  uint16_t c0 = p.coins;
  mockNvsSnapshot();
  long total = 0;
  {
    Pet a;
    a.begin();
    long w0 = mockNvsWrites();
    a.evolve();
    total = mockNvsWrites() - w0;
    CHECK_EQ(a.coins, (uint16_t)(c0 + MEDAL_COINS[0]));
  }
  int sawPaid = 0, sawLost = 0;
  for (long n = 0; n <= total; n++) {
    mockNvsRestore();
    {
      Pet a;
      a.begin();
      mockNvsCutAfter(n);
      a.evolve();
      mockNvsCutAfter(-1);
    }
    Pet b;
    b.begin();
    std::string at = "corte " + std::to_string(n) + ": ";
    int dc = (int)b.coins - c0;
    CHECK_MSG(dc == 0 || dc == MEDAL_COINS[0], at + "monedas +" + std::to_string(dc));
    uint32_t v = b.medalValue(MD_EVOLVE);
    CHECK_MSG(v == t0 - 1 || v == t0, at + "evoluciones " + std::to_string(v));
    if (dc) CHECK_MSG(b.medalTierPaid(MD_EVOLVE) == MTIER_BRONZE, at + "cobrado sin el tramo");
    CHECK_MSG(b.medalTierPaid(MD_EVOLVE) == medalTierOf(MD_EVOLVE, v), at + "tramo");
    sawPaid += dc != 0;
    sawLost += dc == 0;
    mSave(b);  // guardar otra vez no paga
    CHECK_MSG((int)b.coins - c0 == dc, at + "pago al volver a guardar");
  }
  CHECK(sawPaid > 0);
  CHECK(sawLost > 0);
}

// Un corte en cada escritura de ganar a un salvaje y capturarlo: al volver se puede repetir lo que no llego a
// guardarse (pelear otra vez, lanzar otra vez la bola), pero cada medalla cuenta ese encuentro una sola vez
TEST(medals, corte_de_luz_al_ganar_y_capturar_cuenta_una_vez) {
  Pet p;
  mBorn(p, 4);
  p.items[IT_BALL] = 3;
  p.items[IT_EGG] = 0;
  CHECK(p.encSpawnNow(16));
  mSave(p);
  mockNvsSnapshot();
  auto fight = [](Pet &a) {
    a.lastSeenEpoch = M_T;  // (begin no pone la hora: la pone setup con el RTC)
    if (a.encActive()) a.encounterDone(true, a.uid);
    if (a.encWonPending()) a.captureWild();
  };
  long total = 0;
  {
    Pet a;
    a.begin();
    long w0 = mockNvsWrites();
    fight(a);
    total = mockNvsWrites() - w0;
    CHECK_EQ(a.medalValue(MD_WILD), (uint32_t)1);
    CHECK_EQ(a.medalValue(MD_CATCH), (uint32_t)1);
  }
  int replayed = 0;
  for (long n = 0; n <= total; n++) {
    mockNvsRestore();
    {
      Pet a;
      a.begin();
      mockNvsCutAfter(n);
      fight(a);
      mockNvsCutAfter(-1);
    }
    Pet b;
    b.begin();
    b.lastSeenEpoch = M_T;
    replayed += b.encActive() || b.encWonPending();
    fight(b);  // lo que quedo a medias, otra vez
    std::string at = "corte " + std::to_string(n) + ": ";
    CHECK_MSG(b.medalValue(MD_WILD) == 1, at + "victorias " + std::to_string(b.medalValue(MD_WILD)));
    CHECK_MSG(b.medalValue(MD_CATCH) == 1, at + "capturas " + std::to_string(b.medalValue(MD_CATCH)));
    Pet c;  // y guardado
    c.begin();
    CHECK_MSG(c.medalValue(MD_WILD) == 1 && c.medalValue(MD_CATCH) == 1, at + "tras reiniciar");
  }
  CHECK(replayed > 0);
}

// Un corte a mitad de las monedas ("cn"): comprar nunca sale gratis, y el siguiente guardado escribe lo que falto
TEST(medals, un_corte_en_las_monedas_nunca_regala_y_se_repara) {
  Pet p;
  mBorn(p, 4);
  p.setCoins(500);
  uint8_t it0 = p.items[IT_GOLDEN];
  mockNvsSnapshot();
  long total = 0;
  {
    Pet a;
    a.begin();
    long w0 = mockNvsWrites();
    CHECK(a.buyItem(IT_GOLDEN));
    total = mockNvsWrites() - w0;
  }
  for (long n = 0; n <= total; n++) {
    mockNvsRestore();
    Pet a;
    a.begin();
    mockNvsCutAfter(n);
    a.buyItem(IT_GOLDEN);
    mockNvsCutAfter(-1);
    std::string at = "corte " + std::to_string(n) + ": ";
    {
      Pet b;  // la placa vuelve a arrancar
      b.begin();
      bool got = b.items[IT_GOLDEN] > it0, paid = b.coins < 500;
      CHECK_MSG(!got || paid, at + "objeto gratis");
    }
    mSave(a);  // la misma RAM guarda otra vez: lo que falto llega
    Pet c;
    c.begin();
    CHECK_MSG(c.coins == a.coins, at + "monedas " + std::to_string(c.coins) + " vs " + std::to_string(a.coins));
    CHECK_MSG(c.items[IT_GOLDEN] == a.items[IT_GOLDEN], at + "mochila");
  }
}
