// Mejoras de casa (v3.20, upgrade.h, docs/HOME_AND_PALS.md FR-HOME.1/.2/.3/.6/.7): la cama (energia dormido y
// los buenos dias), el gimnasio y el juguete en el tick (en vivo y apagado), la compra, "upg" en la NVS y la cola
// de eventos. Los numeros salen de tuning.h.
#include "framework.h"
#include "shim/Arduino.h"
#include "shim/Preferences.h"
#include "../pet.h"
#include "../upgrade.h"

static const uint32_t UDAY = 21000;  // un dia cualquiera (numero de dia unix)
static uint32_t uat(uint32_t day, int h, int m = 0) { return day * 86400 + h * 3600 + m * 60; }

// bicho recien nacido (Charmander) a las h:00 del dia UDAY, con la NVS limpia. El azar, fijo en 99: ni cacas ni
// juguete (cada prueba que quiera otra cosa lo cambia), asi dos gemelos hacen lo mismo minuto a minuto
static void uBorn(Pet &p, int h = 10, uint32_t day = UDAY) {
  mockClearForcedRandom();
  mockNvsReset();
  mockSetMillis(0);
  mockSerialSilence(true);
  p.begin();
  if (p.awaitingStarter()) p.chooseStarter(4);
  p.eggTap();
  p.eggTap();
  p.eggTap();
  p.setClock(uat(day, h));
  mockForceRandom(99);
}

// n minutos en vivo; keep: las tres necesidades a 100 antes de cada uno. Vacia la cola de eventos y devuelve
// cuantos habia del tipo kind
static int uLive(Pet &p, uint32_t n, bool keep = false, uint8_t kind = HE_GYM) {
  int gym = 0;
  for (uint32_t i = 0; i < n; i++) {
    if (keep) p.fullness = p.joy = p.energy = 100;
    p.lastSeenEpoch += 60;
    mockAdvanceMillis(PET_TICK_MS);
    p.update(millis());
    HomeEvt e;
    while (p.takeHomeEvt(e))
      if (e.kind == kind) gym++;
  }
  return gym;
}

// guardar ya (save() es privado; setClock con hora guarda)
static void uSave(Pet &p) { p.setClock(p.lastSeenEpoch); }

static int trSum(const Pet &p) { return p.trAtk + p.trDef + p.trSpe; }

// ------------------------------------------------------------------ camas: la energia dormido
TEST(upgrades, la_cama_sube_la_energia_por_minuto_dormido) {
  CHECK_EQ(BED_ENE_GAIN[0], (uint8_t)SLEEP_ENE_GAIN);  // sin cama, lo de siempre
  for (uint8_t t = 0; t <= UPG_TIERS; t++) {
    Pet p;
    uBorn(p, 13);
    p.dbgSetUpgTier(UPG_BED, t);
    p.toggleLight();  // una siesta: tambien cuenta
    p.energy = 20;
    uLive(p, 1);
    CHECK(p.sleeping);
    CHECK_EQ(p.energy, (uint8_t)(20 + BED_ENE_GAIN[t]));
  }
}

// ------------------------------------------------------------------ camas: los buenos dias
// una noche de las 23:00 a las WAKE_HOUR, con la cama del nivel t; devuelve la felicidad al despertar
static uint8_t uNight(Pet &p, uint8_t t, bool *evt = nullptr, uint8_t *evtA = nullptr) {
  uBorn(p, 23);
  p.dbgSetUpgTier(UPG_BED, t);
  p.joy = 70;
  p.toggleLight();
  uint32_t mins = (24 - 23 + WAKE_HOUR) * 60;
  for (uint32_t i = 0; i < mins; i++) {
    p.lastSeenEpoch += 60;
    mockAdvanceMillis(PET_TICK_MS);
    p.update(millis());
  }
  CHECK(!p.sleeping);
  HomeEvt e;
  bool got = false;
  while (p.takeHomeEvt(e))
    if (e.kind == HE_MORNING) {
      got = true;
      CHECK_EQ(e.slot, (uint8_t)0);
      if (evtA) *evtA = e.a;
    }
  if (evt) *evt = got;
  return p.joy;
}

TEST(upgrades, buenos_dias_tras_una_noche_entera) {
  Pet c;
  bool cEvt = true;
  uint8_t base = uNight(c, 0, &cEvt);
  CHECK(!cEvt);  // sin cama no hay buenos dias
  for (uint8_t t = 1; t <= UPG_TIERS; t++) {
    Pet p;
    bool evt = false;
    uint8_t a = 0;
    uint8_t j = uNight(p, t, &evt, &a);
    CHECK_EQ(j, (uint8_t)(base + BED_MORNING_JOY[t]));
    CHECK(evt);
    CHECK_EQ(a, BED_MORNING_JOY[t]);
    CHECK_EQ(p.upgSlotOf(0).sleptMin, (uint16_t)0);  // la cuenta vuelve a 0: una vez por noche
    // el resto del dia despierto no lo vuelve a dar
    uint8_t before = p.joy;
    uLive(p, 60);
    CHECK(p.joy <= before);
  }
}

TEST(upgrades, sin_buenos_dias_si_durmio_poco) {
  Pet p;
  uBorn(p, 4);  // a las 04:00: 5 h hasta las WAKE_HOUR, menos de BED_MORNING_MIN_SLEEP
  static_assert((WAKE_HOUR - 4) * 60 < BED_MORNING_MIN_SLEEP, "la prueba necesita una noche corta");
  p.dbgSetUpgTier(UPG_BED, UPG_TIERS);
  p.joy = 70;
  p.toggleLight();
  CHECK(p.nightSlept());
  uint8_t j0 = p.joy;
  CHECK_EQ(uLive(p, (WAKE_HOUR - 4) * 60, false, HE_MORNING), 0);
  CHECK(!p.sleeping);
  CHECK(p.joy <= j0);
}

TEST(upgrades, una_siesta_no_cuenta_para_la_noche) {
  Pet p;
  uBorn(p, 13);
  p.dbgSetUpgTier(UPG_BED, 1);
  p.toggleLight();
  CHECK(!p.nightSlept());
  uLive(p, 120);
  CHECK(p.sleeping);
  CHECK_EQ(p.upgSlotOf(0).sleptMin, (uint16_t)0);
}

TEST(upgrades, despertarlo_a_mano_o_agitarlo_no_da_los_buenos_dias) {
  for (int shake = 0; shake < 2; shake++) {
    Pet p;
    uBorn(p, 23);
    p.dbgSetUpgTier(UPG_BED, 1);
    p.joy = 70;
    p.toggleLight();
    uLive(p, (24 - 23 + WAKE_HOUR - 1) * 60 + 30);  // las WAKE_HOUR - 1:30, mas de BED_MORNING_MIN_SLEEP dormido
    CHECK(p.upgSlotOf(0).sleptMin >= BED_MORNING_MIN_SLEEP);
    if (shake) p.wakeGrumpy();
    else p.toggleLight();
    CHECK(!p.sleeping);
    CHECK_EQ(p.upgSlotOf(0).sleptMin, (uint16_t)0);
    p.toggleLight();  // otra vez a la cama, aun de noche: se despertara solo, pero sin la noche entera
    CHECK(p.nightSlept());
    CHECK_EQ(uLive(p, 30, false, HE_MORNING), 0);  // las WAKE_HOUR
    CHECK(!p.sleeping);
  }
}

TEST(upgrades, la_cuenta_de_la_noche_sobrevive_a_un_reinicio) {
  Pet p;
  uBorn(p, 23);
  p.dbgSetUpgTier(UPG_BED, 2);
  p.joy = 70;
  p.toggleLight();
  uLive(p, 5 * 60);  // las 04:00
  uSave(p);
  uint16_t slept = p.upgSlotOf(0).sleptMin;
  CHECK(slept > 0);
  Pet q;
  q.begin();
  CHECK_EQ(q.upgSlotOf(0).sleptMin, slept);
  CHECK(q.sleeping);
  q.lastSeenEpoch = p.lastSeenEpoch;
  CHECK_EQ(uLive(q, WAKE_HOUR * 60 - 4 * 60, false, HE_MORNING), 1);  // hasta las WAKE_HOUR, encendida
  CHECK(!q.sleeping);
}

TEST(upgrades, buenos_dias_tambien_apagada_sin_evento) {
  // dos noches apagadas iguales, con y sin cama: la diferencia es el bonus
  uint8_t joyAt[2];
  for (int bed = 0; bed < 2; bed++) {
    Pet p;
    uBorn(p, 23);
    p.dbgSetUpgTier(UPG_BED, bed ? UPG_TIERS : 0);
    p.joy = 70;
    p.toggleLight();  // guarda (y la hora)
    Pet q;
    q.begin();
    q.syncClock(uat(UDAY + 1, WAKE_HOUR, 1));
    CHECK(!q.sleeping);
    HomeEvt e;
    CHECK(!q.takeHomeEvt(e));  // apagada no hay nadie mirando
    joyAt[bed] = q.joy;
  }
  CHECK_EQ(joyAt[1], (uint8_t)(joyAt[0] + BED_MORNING_JOY[UPG_TIERS]));
}

// ------------------------------------------------------------------ gimnasio
TEST(upgrades, gym_elige_el_mas_bajo_bajo_el_techo) {
  CHECK_EQ(gymPick(0, 0, 0, 25), (int8_t)0);    // empate: ATK
  CHECK_EQ(gymPick(5, 3, 3, 25), (int8_t)1);    // empate DEF/SPD: DEF
  CHECK_EQ(gymPick(5, 6, 2, 25), (int8_t)2);
  CHECK_EQ(gymPick(30, 25, 10, 25), (int8_t)2);  // ATK por encima del techo no cuenta
  CHECK_EQ(gymPick(30, 25, 25, 25), (int8_t)-1); // nada que subir
}

TEST(upgrades, gym_un_punto_cada_GYM_EVERY_MIN_y_cuesta_energia) {
  for (uint8_t t = 1; t <= UPG_TIERS; t++) {
    Pet p, c;  // c: el gemelo sin gimnasio
    uBorn(c, 10);
    uLive(c, GYM_EVERY_MIN[t] - 1, true);
    c.fullness = c.joy = c.energy = 100;
    uLive(c, 1);
    uBorn(p, 10);
    p.dbgSetUpgTier(UPG_GYM, t);
    CHECK_EQ(uLive(p, GYM_EVERY_MIN[t] - 1, true), 0);
    CHECK_EQ(trSum(p), 0);
    CHECK_EQ(p.gymMinutes(0), (uint8_t)(GYM_EVERY_MIN[t] - 1));
    p.fullness = p.joy = p.energy = 100;
    p.lastSeenEpoch += 60;
    mockAdvanceMillis(PET_TICK_MS);
    p.update(millis());
    CHECK_EQ(p.trAtk, (uint8_t)1);
    CHECK_EQ(p.energy, (uint8_t)(c.energy - GYM_ENE_COST));
    CHECK_EQ(p.gymMinutes(0), (uint8_t)0);
    CHECK_EQ(p.upgSlotOf(0).gymGains, (uint8_t)1);
    HomeEvt e;
    CHECK(p.takeHomeEvt(e));
    CHECK_EQ(e.kind, (uint8_t)HE_GYM);
    CHECK_EQ(e.slot, (uint8_t)0);
    CHECK_EQ(e.a, (uint8_t)0);  // ATK
    CHECK(p.savePending());
  }
}

TEST(upgrades, gym_reparte_y_no_pasa_del_techo) {
  Pet p;
  uBorn(p, 9);
  p.dbgSetUpgTier(UPG_GYM, 1);
  p.trAtk = GYM_CAP[1] + 5;  // de los minijuegos: no baja
  p.trDef = GYM_CAP[1] - 1;
  p.trSpe = GYM_CAP[1] - 2;
  CHECK_EQ(uLive(p, GYM_EVERY_MIN[1], true), 1);
  CHECK_EQ(p.trSpe, (uint8_t)(GYM_CAP[1] - 1));
  CHECK_EQ(uLive(p, GYM_EVERY_MIN[1], true), 1);
  CHECK_EQ(p.trDef, (uint8_t)GYM_CAP[1]);
  CHECK_EQ(uLive(p, GYM_EVERY_MIN[1], true), 1);
  CHECK_EQ(p.trSpe, (uint8_t)GYM_CAP[1]);
  CHECK_EQ(uLive(p, 3 * GYM_EVERY_MIN[1], true), 0);  // todo en el techo: nada mas
  CHECK_EQ(p.trAtk, (uint8_t)(GYM_CAP[1] + 5));
  CHECK_EQ(p.trDef, (uint8_t)GYM_CAP[1]);
  CHECK_EQ(p.trSpe, (uint8_t)GYM_CAP[1]);
}

TEST(upgrades, gym_tope_diario_y_vuelve_al_dia_siguiente) {
  Pet p;
  uBorn(p, WAKE_HOUR);
  p.dbgSetUpgTier(UPG_GYM, 1);
  uint32_t awake = (SLEEPY_HOUR - WAKE_HOUR) * 60;
  CHECK(awake / GYM_EVERY_MIN[1] > GYM_DAY_MAX[1]);
  CHECK_EQ(uLive(p, awake, true), (int)GYM_DAY_MAX[1]);
  CHECK_EQ(p.upgSlotOf(0).gymGains, GYM_DAY_MAX[1]);
  CHECK_EQ(p.upgSlotOf(0).day, UDAY);
  // al dia siguiente, otra vez
  p.lastSeenEpoch = uat(UDAY + 1, WAKE_HOUR);
  p.sleeping = false;
  p.wokeUp();
  CHECK_EQ(uLive(p, GYM_EVERY_MIN[1], true), 1);
  CHECK_EQ(p.upgSlotOf(0).day, UDAY + 1);
  CHECK_EQ(p.upgSlotOf(0).gymGains, (uint8_t)1);
}

TEST(upgrades, gym_no_cuenta_con_hambre_de_noche_dormido_ni_con_un_juego_abierto) {
  for (int k = 0; k < 4; k++) {
    Pet p;
    uBorn(p, k == 1 ? SLEEPY_HOUR : 13);
    p.dbgSetUpgTier(UPG_GYM, UPG_TIERS);
    if (k == 2) p.toggleLight();  // una siesta
    if (k == 3) p.nightHold = true;
    for (int i = 0; i < GYM_EVERY_MIN[UPG_TIERS] + 5; i++) {
      p.fullness = p.joy = p.energy = 100;
      if (k == 0) p.joy = GYM_NEED_MIN - 1;
      p.lastSeenEpoch += 60;
      mockAdvanceMillis(PET_TICK_MS);
      p.update(millis());
    }
    CHECK_EQ(p.gymMinutes(0), (uint8_t)0);
    CHECK_EQ(trSum(p), 0);
  }
}

TEST(upgrades, gym_apagada_con_el_dia_de_cada_minuto) {
  // visto a las 21:00 del dia D, despierto, lleno (tarta) y descansado; vuelve el dia D+1 a las 08:00 (dormido
  // desde las 23:00). De 21:00 a 22:00 cuentan los minutos del dia D: con el tope de D ya hecho, nada
  for (int full = 0; full < 2; full++) {
    Pet p;
    uBorn(p, 21);
    p.dbgSetUpgTier(UPG_GYM, UPG_TIERS);
    p.fullness = p.joy = p.energy = 100;
    p.satedUntil = p.ageMinutes + 100000;
    UpgSlot *u = p.upgSlotNow();
    CHECK(u != nullptr);
    u->day = UDAY;
    u->gymGains = full ? GYM_DAY_MAX[UPG_TIERS] : 0;
    uSave(p);
    Pet q;
    q.begin();
    q.syncClock(uat(UDAY + 1, 8));
    CHECK(q.sleeping);
    CHECK_EQ(q.upgSlotOf(0).day, UDAY);  // el ultimo minuto que conto fue del dia D, no de "hoy"
    HomeEvt e;
    CHECK(!q.takeHomeEvt(e));
    // 59 minutos de las 21 (el de las 22:00 ya es de noche): uno cada GYM_EVERY_MIN
    int want = full ? 0 : 59 / GYM_EVERY_MIN[UPG_TIERS];
    CHECK_EQ(trSum(q), want);
    CHECK_EQ(q.upgSlotOf(0).gymGains, (uint8_t)(full ? GYM_DAY_MAX[UPG_TIERS] : want));
  }
}

// ------------------------------------------------------------------ juguete
TEST(upgrades, el_juguete_da_alegria_con_tope_diario) {
  for (uint8_t t = 1; t <= UPG_TIERS; t++) {
    Pet c, p;
    uBorn(c, 10);
    mockForceRandom(0);  // siempre toca jugar
    c.joy = 50;
    uLive(c, 1);
    uBorn(p, 10);
    p.dbgSetUpgTier(UPG_TOY, t);
    mockForceRandom(0);
    p.joy = 50;
    p.lastSeenEpoch += 60;
    mockAdvanceMillis(PET_TICK_MS);
    p.update(millis());
    CHECK_EQ(p.joy, (uint8_t)(c.joy + TOY_JOY[t]));
    HomeEvt e;
    CHECK(p.takeHomeEvt(e));
    CHECK_EQ(e.kind, (uint8_t)HE_TOY);
    CHECK_EQ(e.a, TOY_JOY[t]);
    // tope del dia
    for (int i = 0; i < 30; i++) {
      p.joy = 50;
      uLive(p, 1);
    }
    CHECK_EQ(p.upgSlotOf(0).toyPlays, TOY_DAY_MAX[t]);
    mockClearForcedRandom();
  }
}

TEST(upgrades, el_juguete_solo_con_poca_alegria_y_a_su_azar) {
  Pet p;
  uBorn(p, 10);
  p.dbgSetUpgTier(UPG_TOY, 1);
  mockForceRandom(0);
  p.joy = TOY_JOY_BELOW;
  p.fullness = p.energy = 100;
  p.lastSeenEpoch += 60;
  mockAdvanceMillis(PET_TICK_MS);
  p.update(millis());
  CHECK_EQ(p.upgSlotOf(0).toyPlays, (uint8_t)0);
  mockForceRandom(TOY_PLAY_CHANCE);  // justo fuera de la probabilidad
  p.joy = 40;
  uLive(p, 1);
  CHECK_EQ(p.upgSlotOf(0).toyPlays, (uint8_t)0);
  mockForceRandom(TOY_PLAY_CHANCE - 1);
  p.joy = 40;
  uLive(p, 1);
  CHECK_EQ(p.upgSlotOf(0).toyPlays, (uint8_t)1);
  mockClearForcedRandom();
}

// ------------------------------------------------------------------ contadores por bicho
TEST(upgrades, otro_bicho_en_el_hueco_empieza_de_cero) {
  Pet p;
  uBorn(p, 10);
  p.dbgSetUpgTier(UPG_GYM, 1);
  uLive(p, GYM_EVERY_MIN[1] + 10, true);
  CHECK_EQ(p.upgSlotOf(0).gymGains, (uint8_t)1);
  CHECK_EQ(p.gymMinutes(0), (uint8_t)10);
  uint32_t old = p.uid;
  p.uid = old + 1;  // otro individuo en el mismo hueco
  uLive(p, 1, true);
  CHECK_EQ(p.upgSlotOf(0).uid, old + 1);
  CHECK_EQ(p.upgSlotOf(0).gymGains, (uint8_t)0);
  CHECK_EQ(p.gymMinutes(0), (uint8_t)1);
}

// ------------------------------------------------------------------ la tienda
TEST(upgrades, upgCheck_primero_lo_que_no_se_arregla) {
  CHECK_EQ(upgCheck(UPG_LINES, 0, 60000), UB_BAD);
  CHECK_EQ(upgCheck(UPG_BED, UPG_TIERS, 0), UB_MAX);  // al maximo, aunque sin monedas
  CHECK_EQ(upgCheck(UPG_GYM, 0, UPG_PRICE[UPG_GYM][0] - 1), UB_BROKE);
  CHECK_EQ(upgCheck(UPG_GYM, 0, UPG_PRICE[UPG_GYM][0]), UB_OK);
  CHECK_EQ(upgCheck(UPG_TOY, 1, UPG_PRICE[UPG_TOY][0]), UPG_PRICE[UPG_TOY][0] < UPG_PRICE[UPG_TOY][1] ? UB_BROKE : UB_OK);
  CHECK_EQ(upgPriceNext(UPG_BED, UPG_TIERS), (uint16_t)0);
  CHECK_EQ(upgPriceNext(UPG_BED, 1), UPG_PRICE[UPG_BED][1]);
}

TEST(upgrades, buyUpgrade_cobra_sube_un_nivel_y_guarda) {
  Pet p;
  uBorn(p, 10);
  p.coins = UPG_PRICE[UPG_BED][0] - 1;
  CHECK(!p.buyUpgrade(UPG_BED));  // sin monedas: nada cambia
  CHECK_EQ(p.upgTier(UPG_BED), (uint8_t)0);
  CHECK_EQ(p.coins, (uint16_t)(UPG_PRICE[UPG_BED][0] - 1));
  CHECK(!p.buyUpgrade(UPG_LINES));
  p.coins = UPG_PRICE[UPG_BED][0] + UPG_PRICE[UPG_BED][1];
  CHECK(p.buyUpgrade(UPG_BED));  // el nivel 1 y despues el 2, en orden
  CHECK_EQ(p.upgTier(UPG_BED), (uint8_t)1);
  CHECK_EQ(p.coins, UPG_PRICE[UPG_BED][1]);
  CHECK(p.buyUpgrade(UPG_BED));
  CHECK_EQ(p.upgTier(UPG_BED), (uint8_t)2);
  CHECK_EQ(p.coins, (uint16_t)0);
  Pet q;
  q.begin();
  CHECK_EQ(q.upgTier(UPG_BED), (uint8_t)2);
  CHECK_EQ(q.coins, (uint16_t)0);
  // al maximo: no se compra ni se cobra
  p.dbgSetUpgTier(UPG_BED, UPG_TIERS);
  p.coins = 5000;
  CHECK(!p.buyUpgrade(UPG_BED));
  CHECK_EQ(p.coins, (uint16_t)5000);
  CHECK_EQ(p.upgTier(UPG_BED), (uint8_t)UPG_TIERS);
}

TEST(upgrades, un_corte_de_luz_al_comprar_nunca_regala_un_nivel) {
  Pet p;
  uBorn(p, 10);
  uint16_t price = UPG_PRICE[UPG_GYM][0];
  p.coins = price;
  uSave(p);
  mockNvsSnapshot();
  bool sawPaid = false, sawLost = false;
  for (long n = 0; n < 400 && !(sawPaid && sawLost); n++) {
    mockNvsRestore();
    Pet a;
    a.begin();
    CHECK_EQ(a.coins, price);
    mockNvsCutAfter(n);
    a.buyUpgrade(UPG_GYM);
    mockNvsCutAfter(-1);
    Pet q;
    q.begin();
    if (q.upgTier(UPG_GYM)) {
      CHECK_EQ(q.coins, (uint16_t)0);  // con el nivel, siempre pagado
      sawPaid = true;
    } else if (q.coins == 0) {
      sawLost = true;  // pagado y sin nivel: se pierde, nunca sale gratis
    }
  }
  CHECK(sawPaid);
  CHECK(sawLost);  // (la ventana entre "cn" y "upg" existe y la prueba la pisa)
}

// v3.20 (revision): si "upg" no se puede escribir, lo de despues (la mochila, los regalos...) se guarda igual y
// se reintenta (pendingSave); antes abortaba todo el guardado para siempre
TEST(upgrades, un_upg_que_no_se_escribe_no_corta_el_resto_del_guardado) {
  Pet p;
  uBorn(p, 10);
  p.dbgSetUpgTier(UPG_BED, 1);  // "upg" cambiado: hay que escribirlo
  p.items[IT_EGG] = 7;
  mockNvsFailKey("upg");
  uSave(p);
  CHECK(p.savePending());  // el reintento
  mockNvsFailKey(nullptr);
  Pet q;
  q.begin();
  CHECK_EQ(q.items[IT_EGG], (uint8_t)7);  // la mochila si llego
  CHECK_EQ(q.upgTier(UPG_BED), (uint8_t)0);  // y el nivel, no
  uSave(p);  // ya se puede: se escribe
  Pet r;
  r.begin();
  CHECK_EQ(r.upgTier(UPG_BED), (uint8_t)1);
}

// el gimnasio y el juguete del hueco 1: sus contadores, sus eventos (slot == 1); un hueco 1 vacio o con huevo
// no recibe nada
static void uTwoPets(Pet &p) {
  uBorn(p, 10);
  p.items[IT_EGG] = ITEM_MAX;
  CHECK(p.store(0, 0));  // el bicho a la caja; en el 0, un huevo de la mochila
  p.eggTap();
  p.eggTap();
  p.eggTap();
  CHECK(p.wake(0, 1));  // el de la caja despierta en el hueco 1
  CHECK(p.home(0).hatched());
  CHECK(p.home(1).hatched());
  p.setClock(uat(UDAY, 10));
  mockForceRandom(0);  // el juguete siempre toca
}

TEST(upgrades, el_hueco_1_cuenta_gimnasio_y_juguete_en_sus_contadores) {
  Pet p;
  uTwoPets(p);
  p.dbgSetUpgTier(UPG_GYM, UPG_TIERS);
  p.dbgSetUpgTier(UPG_TOY, UPG_TIERS);
  int gym[2] = { 0, 0 }, toy[2] = { 0, 0 };
  for (int i = 0; i < GYM_EVERY_MIN[UPG_TIERS] * 2 + 2; i++) {
    for (uint8_t s = 0; s < HOME_SLOTS; s++) {
      PetLife &l = const_cast<PetLife &>(p.home(s));
      l.fullness = l.energy = 100;
      l.joy = i % 3 == 0 ? 10 : 100;
    }
    p.lastSeenEpoch += 60;
    mockAdvanceMillis(PET_TICK_MS);
    p.update(millis());
    HomeEvt e;
    while (p.takeHomeEvt(e)) {
      CHECK(e.slot < HOME_SLOTS);
      if (e.kind == HE_GYM) gym[e.slot & 1]++;
      if (e.kind == HE_TOY) toy[e.slot & 1]++;
    }
  }
  CHECK(gym[1] > 0);
  CHECK(toy[1] > 0);
  CHECK_EQ((int)p.upgSlotOf(1).gymGains, gym[1]);
  CHECK_EQ((int)p.upgSlotOf(1).toyPlays, toy[1]);
  CHECK(p.upgSlotOf(1).uid != 0);
  CHECK(p.upgSlotOf(1).uid != p.upgSlotOf(0).uid);
  CHECK_EQ((int)p.upgSlotOf(0).gymGains, gym[0]);
  CHECK_EQ((int)p.upgSlotOf(0).toyPlays, toy[0]);
}

TEST(upgrades, un_hueco_1_vacio_no_recibe_nada) {
  Pet p;
  uBorn(p, 10);  // solo el hueco 0 tiene bicho
  CHECK(p.home(1).isVacant());
  p.dbgSetUpgTier(UPG_GYM, UPG_TIERS);
  p.dbgSetUpgTier(UPG_TOY, UPG_TIERS);
  mockForceRandom(0);
  int mine = 0;
  for (int i = 0; i < GYM_EVERY_MIN[UPG_TIERS] + 3; i++) {
    p.fullness = p.energy = 100;
    p.joy = i % 2 ? 10 : 100;
    p.lastSeenEpoch += 60;
    mockAdvanceMillis(PET_TICK_MS);
    p.update(millis());
    HomeEvt e;
    while (p.takeHomeEvt(e)) {
      CHECK_EQ(e.slot, (uint8_t)0);
      mine++;
    }
  }
  CHECK(mine > 0);  // (el hueco 0 si juega y entrena: la prueba no es vacua)
  const UpgSlot &u = p.upgSlotOf(1);
  CHECK_EQ(u.uid, (uint32_t)0);
  CHECK_EQ((int)u.gymGains, 0);
  CHECK_EQ((int)u.toyPlays, 0);
  CHECK_EQ((int)p.gymMinutes(1), 0);
}

// ------------------------------------------------------------------ "upg" en la NVS
TEST(upgrades, upg_ida_y_vuelta) {
  Pet p;
  uBorn(p, 10);
  p.dbgSetUpgTier(UPG_BED, 1);
  p.dbgSetUpgTier(UPG_GYM, 2);
  p.dbgSetUpgTier(UPG_TOY, 3);
  p.dbgSetUpgTier(UPG_TOY, 9);  // se recorta al maximo
  CHECK_EQ(p.upgTier(UPG_TOY), (uint8_t)UPG_TIERS);
  CHECK(p.savePending());
  UpgSlot *u = p.upgSlotNow();
  u->day = UDAY;
  u->gymGains = 3;
  u->toyPlays = 2;
  u->sleptMin = 77;
  uSave(p);
  Pet q;
  q.begin();
  CHECK_EQ(q.upgTier(UPG_BED), (uint8_t)1);
  CHECK_EQ(q.upgTier(UPG_GYM), (uint8_t)2);
  CHECK_EQ(q.upgTier(UPG_TOY), (uint8_t)3);
  CHECK_EQ(q.upgSlotOf(0).uid, p.uid);
  CHECK_EQ(q.upgSlotOf(0).day, UDAY);
  CHECK_EQ(q.upgSlotOf(0).gymGains, (uint8_t)3);
  CHECK_EQ(q.upgSlotOf(0).toyPlays, (uint8_t)2);
  CHECK_EQ(q.upgSlotOf(0).sleptMin, (uint16_t)77);
  CHECK_EQ(q.upgTier(UPG_LINES), (uint8_t)0);
}

TEST(upgrades, partida_nueva_o_sin_upg_empieza_sin_nada) {
  Pet p;
  uBorn(p, 10);
  for (uint8_t l = 0; l < UPG_LINES; l++) CHECK_EQ(p.upgTier(l), (uint8_t)0);
  p.dbgSetUpgTier(UPG_GYM, 2);
  uSave(p);
  Preferences pr;
  pr.begin("tamapoke", false);
  CHECK(pr.isKey("upg"));
  pr.remove("upg");
  Pet q;
  q.begin();
  CHECK_EQ(q.upgTier(UPG_GYM), (uint8_t)0);
  CHECK_EQ(q.upgSlotOf(0).uid, (uint32_t)0);
}

TEST(upgrades, upg_danado_o_de_otra_version_carga_sin_nada) {
  for (int k = 0; k < 4; k++) {
    Pet p;
    uBorn(p, 10);
    p.dbgSetUpgTier(UPG_BED, 3);
    uSave(p);
    Preferences pr;
    pr.begin("tamapoke", false);
    UpgSave u;
    CHECK_EQ(pr.getBytes("upg", &u, sizeof(u)), sizeof(u));
    if (k == 0) {
      u.tier[1] ^= 1;  // CRC malo
      pr.putBytes("upg", &u, sizeof(u));
    } else if (k == 1) {
      u.ver = UPG_VER + 1;  // otra version (con su CRC bueno)
      u.crc = upgCrc(u);
      pr.putBytes("upg", &u, sizeof(u));
    } else if (k == 2) {
      u.tier[0] = UPG_TIERS + 1;  // un nivel que no existe
      u.crc = upgCrc(u);
      pr.putBytes("upg", &u, sizeof(u));
    } else {
      pr.putBytes("upg", &u, sizeof(u) - 1);  // corto
    }
    Pet q;
    q.begin();
    for (uint8_t l = 0; l < UPG_LINES; l++) CHECK_EQ(q.upgTier(l), (uint8_t)0);
    // y el siguiente guardado lo arregla
    q.setClock(uat(UDAY, 11));
    Pet r;
    r.begin();
    CHECK_EQ(r.upgTier(UPG_BED), (uint8_t)0);
    CHECK(pr.getBytes("upg", &u, sizeof(u)) == sizeof(u) && upgValid(u));
  }
}

// ------------------------------------------------------------------ la cola de eventos
TEST(upgrades, la_cola_de_eventos_pierde_el_mas_viejo) {
  HomeEvtRing r;
  HomeEvt e;
  CHECK(!r.pop(e));
  for (uint8_t i = 0; i < HOME_EVT_Q + 2; i++) r.push(HE_TOY, i & 1, i);
  for (uint8_t i = 2; i < HOME_EVT_Q + 2; i++) {
    CHECK(r.pop(e));
    CHECK_EQ(e.kind, (uint8_t)HE_TOY);
    CHECK_EQ(e.a, i);
    CHECK_EQ(e.slot, (uint8_t)(i & 1));
  }
  CHECK(!r.pop(e));
  r.push(HE_GYM, 1, 2);
  CHECK(r.pop(e));
  CHECK_EQ(e.kind, (uint8_t)HE_GYM);
}
