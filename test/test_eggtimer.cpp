// El huevo del patio (v2.0, fase Y3; eggtimer.h + Pet): eclosiona solo a las 4 h de tiempo real desde
// que se pone en el patio; cada sacudida (una cada 30 s como mucho) le quita 5 min, hasta 2 h. El
// tiempo es el del RTC: corre tambien apagada. El inicial y uno a medias del sistema de toques (sin la
// clave "yegg") siguen con los 3 toques.
#include "framework.h"
#include "shim/Arduino.h"
#include "shim/Preferences.h"
#include "../pet.h"

static const uint32_t T0 = 20000UL * 86400 + 10 * 3600;  // un dia cualquiera a las 10:00

// ---------------------------------------------------------------- logica pura

TEST(eggtimer, cuatro_horas_de_tiempo_real) {
  CHECK_EQ(eggMinutesLeft(T0, T0, 0), (uint16_t)240);
  CHECK_EQ(eggMinutesLeft(T0, T0 + 59, 0), (uint16_t)240);
  CHECK_EQ(eggMinutesLeft(T0, T0 + 60, 0), (uint16_t)239);
  CHECK_EQ(eggMinutesLeft(T0, T0 + 3 * 3600, 0), (uint16_t)60);
  CHECK_EQ(eggMinutesLeft(T0, T0 + 4 * 3600, 0), (uint16_t)0);
  CHECK_EQ(eggMinutesLeft(T0, T0 + 30 * 86400, 0), (uint16_t)0);
  CHECK_EQ(eggMinutesLeft(T0, T0 - 3600, 0), (uint16_t)240);  // el reloj fue hacia atras: no avanza
}

TEST(eggtimer, sin_hora_no_avanza) {
  CHECK(!eggClockKnown(EGG_NO_CLOCK));
  CHECK(eggClockKnown(T0));
  CHECK_EQ(eggMinutesLeft(EGG_NO_CLOCK, T0, 0), (uint16_t)240);
  CHECK_EQ(eggMinutesLeft(EGG_NO_CLOCK, T0, 30), (uint16_t)210);  // las sacudidas si
}

TEST(eggtimer, las_sacudidas_quitan_hasta_dos_horas) {
  CHECK_EQ(eggMinutesLeft(T0, T0, 120), (uint16_t)120);
  CHECK_EQ(eggMinutesLeft(T0, T0, 200), (uint16_t)120);  // mas del tope no cuenta
  CHECK_EQ(eggMinutesLeft(T0, T0 + 2 * 3600, 120), (uint16_t)0);  // lo mas rapido: 2 h
  uint8_t saved = 0;
  uint32_t last = 0;
  bool have = false;
  CHECK(eggShake(&saved, &last, &have, 1000));
  CHECK_EQ(saved, (uint8_t)EGG_SHAKE_MIN);
  CHECK(!eggShake(&saved, &last, &have, 1000 + EGG_SHAKE_COOLDOWN_MS - 1));  // enfriamiento
  CHECK_EQ(saved, (uint8_t)EGG_SHAKE_MIN);
  CHECK(eggShake(&saved, &last, &have, 1000 + EGG_SHAKE_COOLDOWN_MS));
  CHECK_EQ(saved, (uint8_t)(2 * EGG_SHAKE_MIN));
  uint32_t t = 1000 + EGG_SHAKE_COOLDOWN_MS;
  int counted = 2;
  for (int k = 0; k < 100; k++) {
    t += EGG_SHAKE_COOLDOWN_MS;
    counted += eggShake(&saved, &last, &have, t);
  }
  CHECK_EQ(saved, (uint8_t)EGG_SHAKE_CAP);
  CHECK_EQ(counted, EGG_SHAKE_CAP / EGG_SHAKE_MIN);  // 24 sacudidas utiles
}

TEST(eggtimer, la_primera_sacudida_cuenta_aunque_millis_sea_pequeno) {
  uint8_t saved = 0;
  uint32_t last = 0;
  bool have = false;
  CHECK(eggShake(&saved, &last, &have, 5));
}

TEST(eggtimer, grietas_y_texto) {
  CHECK_EQ(eggCracksFor(240), (uint8_t)0);
  CHECK_EQ(eggCracksFor(121), (uint8_t)0);
  CHECK_EQ(eggCracksFor(120), (uint8_t)1);
  CHECK_EQ(eggCracksFor(31), (uint8_t)1);
  CHECK_EQ(eggCracksFor(30), (uint8_t)2);
  char s[16];
  eggLeftText(s, sizeof(s), 240);
  CHECK_STREQ(s, "4h 00m");
  eggLeftText(s, sizeof(s), 80);
  CHECK_STREQ(s, "1h 20m");
  eggLeftText(s, sizeof(s), 45);
  CHECK_STREQ(s, "45m");
  eggLeftText(s, sizeof(s), 0);
  CHECK_STREQ(s, "1m");
  CHECK_EQ(eggMinutesLeft(eggPlacedFor(T0, 100), T0, 0), (uint16_t)140);
  CHECK_EQ(eggMinutesLeft(eggPlacedFor(T0, 500), T0, 0), (uint16_t)0);
}

// ---------------------------------------------------------------- en Pet

static void minutes(Pet &p, uint32_t n) {
  for (uint32_t i = 0; i < n; i++) {
    mockAdvanceMillis(PET_TICK_MS);
    p.lastSeenEpoch += 60;
    p.update(millis());
  }
}

// partida nueva con el patio: el inicial, a 3 toques; luego una casa vacia con un huevo en la mochila
static void yardGame(Pet &p) {
  mockClearForcedRandom();
  mockNvsReset();
  mockSetMillis(0);
  p.timedEggs = true;
  p.begin();
  CHECK(p.awaitingStarter());
  p.chooseStarter(4);
  p.setClock(T0);
}

static void vacantWithEgg(Pet &p) {
  yardGame(p);
  p.hatchNow();
  CHECK(!p.isEgg());
  p.items[IT_EGG] = 1;
  CHECK(p.boxPut(5, p.toRecord()));  // alguien en la caja: soltar no trae el huevo de regalo
  p.release();
  mockAdvanceMillis(CEREMONY_MS + 1);
  p.update(millis());
  CHECK(p.isVacant());
}

TEST(yardegg, el_inicial_sigue_con_tres_toques) {
  Pet p;
  yardGame(p);
  CHECK(p.isEgg());
  CHECK(!p.yardEgg());
  CHECK_EQ(p.eggPlacedEpoch(), 0u);
  p.eggTap();
  p.eggTap();
  CHECK(p.isEgg());
  p.eggTap();
  CHECK(!p.isEgg());
  CHECK_EQ((int)p.speciesId, 4);
}

TEST(yardegg, abrir_uno_de_la_mochila_lo_pone_en_el_patio) {
  Pet p;
  vacantWithEgg(p);
  CHECK(p.hatchFromBag());
  CHECK(p.isEgg());
  CHECK(p.yardEgg());
  CHECK_EQ(p.eggPlacedEpoch(), p.lastSeenEpoch);
  CHECK_EQ(p.eggMinutesLeft(), (uint16_t)240);
  // tocarlo no lo abre
  for (int k = 0; k < 5; k++) p.eggTap();
  CHECK(p.isEgg());
  CHECK_EQ(p.eggCracks(), (uint8_t)0);
  minutes(p, 120);
  CHECK(p.isEgg());
  CHECK_EQ(p.eggCracks(), (uint8_t)1);
  minutes(p, 115);
  CHECK_EQ(p.eggCracks(), (uint8_t)2);
  CHECK(p.isEgg());
  minutes(p, 5);
  CHECK(!p.isEgg());  // a las 4 h justas
  CHECK(!p.yardEgg());
  CHECK_EQ(p.eggPlacedEpoch(), 0u);
}

TEST(yardegg, las_sacudidas_lo_adelantan_sin_escribir_en_flash) {
  Pet p;
  vacantWithEgg(p);
  CHECK(p.hatchFromBag());
  mockAdvanceMillis(1000);
  long w0 = mockNvsWrites();
  CHECK(p.shaken());
  CHECK(p.emote() == EMOTE_WOBBLE);
  CHECK(!p.shaken());  // enfriamiento: se tambalea igual, pero no cuenta
  CHECK(p.emote() == EMOTE_WOBBLE);
  CHECK_EQ(p.eggSavedMinutes(), (uint8_t)EGG_SHAKE_MIN);
  CHECK_EQ(mockNvsWrites(), w0);  // ni una escritura por sacudida
  CHECK(p.savePending());
  for (int k = 0; k < 40; k++) {
    mockAdvanceMillis(EGG_SHAKE_COOLDOWN_MS);
    p.shaken();
  }
  CHECK_EQ(p.eggSavedMinutes(), (uint8_t)EGG_SHAKE_CAP);
  CHECK_EQ(p.eggMinutesLeft(), (uint16_t)120);
  minutes(p, 119);
  CHECK(p.isEgg());
  minutes(p, 1);
  CHECK(!p.isEgg());  // lo mas rapido: 2 h
}

TEST(yardegg, sobrevive_a_un_reinicio_y_cuenta_apagada) {
  Pet p;
  vacantWithEgg(p);
  CHECK(p.hatchFromBag());
  mockAdvanceMillis(1000);
  CHECK(p.shaken());
  minutes(p, 10);
  p.flushSave();
  uint32_t placed = p.eggPlacedEpoch();
  Pet q;
  q.timedEggs = true;
  q.begin();
  CHECK(q.yardEgg());
  CHECK_EQ(q.eggPlacedEpoch(), placed);
  CHECK_EQ(q.eggSavedMinutes(), (uint8_t)EGG_SHAKE_MIN);
  // apagada 1 h: sigue siendo huevo, con una hora menos
  q.syncClock(p.lastSeenEpoch + 3600);
  CHECK(q.isEgg());
  CHECK_EQ(q.eggMinutesLeft(), (uint16_t)(240 - 10 - 60 - EGG_SHAKE_MIN));
  // apagada 5 h mas: eclosiona en tu ausencia, y el resto del tiempo ya es del bicho
  Pet r;
  r.timedEggs = true;
  r.begin();
  r.syncClock(p.lastSeenEpoch + 6 * 3600);
  CHECK(!r.isEgg());
  CHECK(r.level() >= 1);
  CHECK(r.growMin > 0);
}

// guardado de antes de la Y3 con un huevo a medias (sin "yegg"): lo termina a toques, como antes
TEST(yardegg, un_huevo_a_medias_del_sistema_de_antes_sigue_a_toques) {
  Pet p;
  vacantWithEgg(p);
  CHECK(p.hatchFromBag());
  p.flushSave();
  {
    Preferences pr;
    pr.begin("tamapoke", false);
    pr.remove("yegg");
    pr.remove("ysav");
    pr.putUChar("crack", 2);
    pr.end();
  }
  Pet q;
  q.timedEggs = true;
  q.begin();
  CHECK(q.isEgg());
  CHECK(!q.yardEgg());
  CHECK_EQ(q.eggCracks(), (uint8_t)2);
  q.eggTap();
  CHECK(!q.isEgg());
}

// la incubadora: el huevo que espera conserva su hora (y la sigue contando)
TEST(yardegg, el_de_la_incubadora_conserva_su_hora) {
  Pet p;
  vacantWithEgg(p);
  CHECK(p.hatchFromBag());
  uint32_t placed = p.eggPlacedEpoch();
  CHECK(p.wake(5));  // despierta al de la caja: el huevo espera
  CHECK(p.eggHeld());
  CHECK(!p.isEgg());
  CHECK(p.store(0));  // vuelve a la caja: el huevo vuelve con su hora
  CHECK(p.isEgg());
  CHECK(p.yardEgg());
  CHECK_EQ(p.eggPlacedEpoch(), placed);
}

// sin el patio (TP_YARD=0) todo sigue como antes: 3 toques o solo a los 3 min
TEST(yardegg, sin_patio_como_siempre) {
  Pet p;
  mockClearForcedRandom();
  mockNvsReset();
  mockSetMillis(0);
  p.begin();
  p.chooseStarter(1);
  p.setClock(T0);
  p.hatchNow();
  p.items[IT_EGG] = 1;
  CHECK(p.boxPut(5, p.toRecord()));
  p.release();
  mockAdvanceMillis(CEREMONY_MS + 1);
  p.update(millis());
  CHECK(p.hatchFromBag());
  CHECK(!p.yardEgg());
  CHECK_EQ(p.eggPlacedEpoch(), 0u);
  minutes(p, 3);
  CHECK(!p.isEgg());
}

TEST(yardegg, serie_eggt_y_hatch) {
  Pet p;
  vacantWithEgg(p);
  CHECK(p.hatchFromBag());
  p.dbgEggElapsed(200);
  CHECK_EQ(p.eggMinutesLeft(), (uint16_t)40);
  p.dbgEggElapsed(240);
  CHECK_EQ(p.eggMinutesLeft(), (uint16_t)0);
  minutes(p, 1);
  CHECK(!p.isEgg());
  // HATCH: tambien el del patio, al momento
  Pet q;
  vacantWithEgg(q);
  CHECK(q.hatchFromBag());
  q.hatchNow();
  CHECK(!q.isEgg());
}

// puesto sin hora (sin RTC): no avanza hasta saberla, y desde ahi cuenta
TEST(yardegg, puesto_sin_hora_se_fecha_al_saberla) {
  Pet p;
  vacantWithEgg(p);
  uint32_t now = p.lastSeenEpoch;
  p.lastSeenEpoch = 0;
  CHECK(p.hatchFromBag());
  CHECK(p.yardEgg());
  CHECK_EQ(p.eggPlacedEpoch(), (uint32_t)EGG_NO_CLOCK);
  p.lastSeenEpoch = now;
  mockAdvanceMillis(PET_TICK_MS);
  p.update(millis());
  CHECK_EQ(p.eggPlacedEpoch(), now);
  CHECK_EQ(p.eggMinutesLeft(), (uint16_t)240);
}
