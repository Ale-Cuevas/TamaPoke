// Estadios: medallas de estadio, el ultimo equipo y el premio del jefe (Pet::stageDone). Las medallas,
// el dia del premio y el equipo son del jugador; el premio llega al de casa y a los de la caja del
// equipo. Cortes de luz tras cada escritura de stageDone, con la NVS del shim.
#include "framework.h"
#include "shim/Arduino.h"
#include "shim/Preferences.h"
#include "../pet.h"
#include "../box.h"
#include "../dex.h"
#include <string.h>
#include <string>

static const uint32_t SDAY = 20100;                      // un dia cualquiera (numero de dia unix)
static const uint32_t ST = SDAY * 86400u + 10 * 3600u;   // ese dia a las 10:00

static void sHatch(Pet &p) {
  p.eggTap();
  p.eggTap();
  p.eggTap();
}

// bicho recien nacido y despierto, con reloj, la NVS limpia y huevos en la mochila
static void sBorn(Pet &p, int16_t dex = 4) {
  mockClearForcedRandom();
  mockNvsReset();
  mockSetMillis(1000);
  p.begin();
  if (p.awaitingStarter()) p.chooseStarter(dex);
  sHatch(p);
  p.items[IT_EGG] = ITEM_MAX;
  p.setClock(ST);  // guarda
}

// un bicho inventado en el hueco k (vacio) de la caja; devuelve su uid
static uint32_t boxed(Pet &p, uint8_t k, int16_t dex, uint32_t grow, uint8_t bond) {
  PetRecord r;
  memset(&r, 0, sizeof(r));
  r.speciesId = dex;
  r.growMin = grow;
  r.ageMinutes = grow + 10;
  r.bond = bond;
  r.fullness = r.joy = r.energy = 80;
  r.geneAtk = r.geneDef = r.geneSpe = 100;
  CHECK(p.boxPut(k, r));
  CHECK_EQ(p.boxState(k), (uint8_t)BOX_OK);
  return p.boxRecord(k).uid;
}

// otro arranque con el reloj (sin escribir: setClock guarda)
static void reboot(Pet &q, uint32_t epoch = ST) {
  q.begin();
  q.lastSeenEpoch = epoch;
}

static bool sameTeam(const uint32_t *a, const uint32_t *b) { return memcmp(a, b, TEAM_MAX * sizeof(uint32_t)) == 0; }

// ---------------------------------------------------------------- guardado
TEST(stage, un_guardado_anterior_carga_sin_medallas_ni_equipo) {
  Pet p;
  sBorn(p);
  {  // quita las claves de los estadios: como un guardado de antes
    Preferences nv;
    nv.begin("tamapoke", false);
    nv.remove("bdg");
    nv.remove("stD");
    nv.remove("stM");
    nv.remove("team");
    CHECK(!nv.isKey("team"));
  }
  Pet q;
  reboot(q);
  CHECK_EQ(q.badgeMask(), (uint8_t)0);
  CHECK_EQ(q.badgeCount(), (uint8_t)0);
  CHECK_EQ(q.stageRewardMask(), (uint8_t)0);
  for (uint8_t s = 0; s < STAGE_COUNT; s++) CHECK(q.stageRewardReady(s));
  for (uint8_t i = 0; i < TEAM_MAX; i++) CHECK_EQ(q.teamUids()[i], (uint32_t)0);
  CHECK(q.stageUnlocked(0));
  CHECK(!q.stageUnlocked(1));
  CHECK_EQ(q.speciesId, p.speciesId);  // el resto, como estaba
  CHECK_EQ(q.uid, p.uid);
}

TEST(stage, medallas_dia_y_equipo_sobreviven_a_un_reinicio) {
  Pet p;
  sBorn(p);
  uint32_t a = boxed(p, 2, 7, 500, 10);
  p.setBadges(0x25);
  uint32_t t[3] = { a, p.uid, 0 };
  long w0 = mockNvsWrites();
  p.setTeam(t, 3);
  CHECK_EQ(mockNvsWrites(), w0);  // setTeam no escribe: marca el guardado
  CHECK(p.savePending());
  p.flushSave();
  StageReward r = p.stageDone(0, true, t, 2);
  CHECK(r.paid);
  Pet q;
  reboot(q);
  CHECK_EQ(q.badgeMask(), (uint8_t)0x25);
  CHECK_EQ(q.badgeCount(), (uint8_t)3);
  CHECK(!q.stageRewardReady(0));
  CHECK(q.stageRewardReady(1));
  uint32_t want[TEAM_MAX] = { a, p.uid, 0, 0 };
  CHECK(sameTeam(q.teamUids(), want));
  CHECK_EQ(q.teamSlotOf(a), (int8_t)2);
  CHECK_EQ(q.teamSlotOf(q.uid), (int8_t)TEAM_HOME);
  CHECK_EQ(q.teamSlotOf(0), (int8_t)-1);
  CHECK_EQ(q.teamSlotOf(12345), (int8_t)-1);
  // n mayor que TEAM_MAX: solo los TEAM_MAX primeros; menos: el resto a 0
  uint32_t big[6] = { 1, 2, 3, 4, 5, 6 };
  q.setTeam(big, 6);
  CHECK_EQ(q.teamUids()[3], (uint32_t)4);
  q.setTeam(big, 1);
  CHECK_EQ(q.teamUids()[0], (uint32_t)1);
  CHECK_EQ(q.teamUids()[1], (uint32_t)0);
}

// lo del jugador sigue igual en RAM y tras un reinicio
static void checkKept(Pet &p, uint8_t badges, const uint32_t *team, const char *when) {
  std::string w = when;
  CHECK_MSG(p.badgeMask() == badges, w + ": medallas en RAM");
  CHECK_MSG(sameTeam(p.teamUids(), team), w + ": equipo en RAM");
  CHECK_MSG(!p.stageRewardReady(0), w + ": el premio de hoy en RAM");
  p.flushSave();
  Pet q;
  reboot(q);
  CHECK_MSG(q.badgeMask() == badges, w + ": medallas tras reiniciar");
  CHECK_MSG(sameTeam(q.teamUids(), team), w + ": equipo tras reiniciar");
  CHECK_MSG(!q.stageRewardReady(0), w + ": el premio de hoy tras reiniciar");
}

TEST(stage, medallas_y_equipo_sobreviven_a_eclosion_caja_soltar_despedida_y_muerte) {
  Pet p;
  sBorn(p);
  uint32_t first = p.uid;
  uint32_t t[TEAM_MAX] = { first, 77, 0, 0 };
  p.setTeam(t, TEAM_MAX);
  p.stageDone(0, true, t, 1);
  p.setBadges(0x0F);
  checkKept(p, 0x0F, t, "al empezar");
  CHECK(p.store(0));  // a la caja: en casa, un huevo de la mochila
  CHECK(p.isEgg());
  checkKept(p, 0x0F, t, "guardar en la caja");
  sHatch(p);
  CHECK(!p.isEgg());
  checkKept(p, 0x0F, t, "eclosion");
  CHECK(p.wake(0));  // intercambio
  CHECK_EQ(p.uid, first);
  checkKept(p, 0x0F, t, "despertar");
  p.release();
  mockAdvanceMillis(CEREMONY_MS + 1);
  p.update(millis());
  CHECK(p.isVacant());
  checkKept(p, 0x0F, t, "soltar");
  CHECK(p.hatchFromBag());
  sHatch(p);
  p.startFarewell();
  mockAdvanceMillis(CEREMONY_MS + 1);
  p.update(millis());
  CHECK(p.isVacant());
  checkKept(p, 0x0F, t, "despedida");
  CHECK(p.hatchFromBag());
  sHatch(p);
  p.startDying();
  CHECK(p.isDying());
  p.letGo();
  mockAdvanceMillis(CEREMONY_MS + 1);
  p.update(millis());
  CHECK(!p.isDying());
  checkKept(p, 0x0F, t, "muerte");
}

TEST(stage, la_escalera_de_estadios) {
  Pet p;
  sBorn(p);
  CHECK(p.stageUnlocked(0));
  for (uint8_t s = 1; s < STAGE_COUNT; s++) CHECK(!p.stageUnlocked(s));
  p.setBadges(0x01);
  CHECK(p.stageUnlocked(1));
  CHECK(!p.stageUnlocked(2));
  p.setBadges(0x07);
  CHECK(p.stageUnlocked(3));
  CHECK(!p.stageUnlocked(4));
  p.setBadges(0x04);  // solo la del 2: el 3 si, el 1 y el 2 no
  CHECK(p.stageUnlocked(0));
  CHECK(!p.stageUnlocked(1));
  CHECK(!p.stageUnlocked(2));
  CHECK(p.stageUnlocked(3));
  p.setBadges(0xFF);
  for (uint8_t s = 0; s < STAGE_COUNT; s++) CHECK(p.stageUnlocked(s));
  CHECK_EQ(p.badgeCount(), (uint8_t)8);
  CHECK(!p.stageUnlocked(STAGE_COUNT));  // fuera de rango
  CHECK(!p.hasBadge(STAGE_COUNT));
  CHECK(!p.stageRewardReady(STAGE_COUNT));
  CHECK(p.savePending());
  // los estadios ganados con stageDone abren el siguiente
  p.setBadges(0);
  uint32_t t[1] = { p.uid };
  CHECK(p.stageDone(0, true, t, 1).newBadge);
  CHECK(p.stageUnlocked(1));
  CHECK(!p.stageDone(0, true, t, 1).newBadge);  // la segunda vez ya no es nueva
}

// ---------------------------------------------------------------- stageDone
TEST(stage, perder_no_da_medalla_ni_premio_y_la_felicidad_es_la_de_la_practica) {
  Pet p;
  sBorn(p);
  uint32_t a = boxed(p, 1, 7, 900, 20);
  p.joy = 50;
  uint32_t grow = p.growMin;
  uint8_t bond = p.bond;
  uint32_t t[2] = { p.uid, a };
  StageReward r = p.stageDone(2, false, t, 2);
  CHECK(!r.paid);
  CHECK(!r.newBadge);
  CHECK_EQ(r.item, (uint8_t)IT_COUNT);
  CHECK_EQ(r.pets, (uint8_t)0);
  CHECK_EQ(r.joy, (uint8_t)PRACTICE_JOY);
  CHECK_EQ(p.joy, (uint8_t)(50 + PRACTICE_JOY));
  CHECK_EQ(p.growMin, grow);
  CHECK_EQ(p.bond, bond);
  CHECK_EQ(p.boxRecord(1).growMin, (uint32_t)900);
  CHECK_EQ(p.boxRecord(1).bond, (uint8_t)20);
  CHECK_EQ(p.giftCount(), (uint8_t)0);
  CHECK_EQ(p.badgeMask(), (uint8_t)0);
  CHECK(p.stageRewardReady(2));
  CHECK(!p.savePending());  // guarda por su cuenta
  // la felicidad, como mucho una vez por hora
  CHECK_EQ(p.stageDone(2, false, t, 2).joy, (uint8_t)0);
  mockAdvanceMillis(PRACTICE_EVERY_MS);
  CHECK_EQ(p.stageDone(2, false, t, 2).joy, (uint8_t)PRACTICE_JOY);
  CHECK_EQ(p.stageDone(2, false, t, 2).joy, (uint8_t)0);
}

TEST(stage, la_primera_victoria_paga_a_todo_el_equipo) {
  Pet p;
  sBorn(p);
  uint32_t a = boxed(p, 1, 7, 900, 40);
  uint32_t b = boxed(p, 4, 1, 2500, 97);
  p.bond = 98;
  p.growMin = 700;
  p.joy = 50;
  // el tope diario de vinculo ya esta lleno hoy: el del jefe va aparte
  for (int i = 0; i < 20; i++) p.caress(), mockAdvanceMillis(PET_BOND_COOLDOWN_MS + 1);
  CHECK_EQ(p.bondGainedToday(), (uint8_t)BOND_DAY_CAP);
  p.bond = 98;
  p.joy = 50;  // los mimos la llenaron
  uint32_t t[3] = { p.uid, a, b };
  mockForceRandom(0);  // la tabla del botin da tarta
  StageReward r = p.stageDone(0, true, t, 3);
  mockClearForcedRandom();
  CHECK(r.paid);
  CHECK(r.newBadge);
  CHECK_EQ(r.item, (uint8_t)IT_CAKE);
  CHECK_EQ(r.pets, (uint8_t)3);
  CHECK_EQ(r.joy, (uint8_t)PRACTICE_JOY);
  // el de casa
  CHECK_EQ(p.growMin, (uint32_t)(700 + STAGE_GROW_MIN));
  CHECK_EQ(p.bond, (uint8_t)100);  // 98 + 5, con tope
  CHECK_EQ(p.bondGainedToday(), (uint8_t)BOND_DAY_CAP);  // no gasta el tope diario
  // los de la caja, sellados en su hueco
  CHECK_EQ(p.boxRecord(1).growMin, (uint32_t)(900 + STAGE_GROW_MIN));
  CHECK_EQ(p.boxRecord(1).bond, (uint8_t)(40 + STAGE_BOND));
  CHECK_EQ(p.boxRecord(4).growMin, (uint32_t)(2500 + STAGE_GROW_MIN));
  CHECK_EQ(p.boxRecord(4).bond, (uint8_t)100);
  CHECK(recordValid(p.boxRecord(1)));
  CHECK(recordValid(p.boxRecord(4)));
  CHECK_EQ(p.boxRecord(1).ageMinutes, (uint32_t)910);  // crecer no los envejece
  // el objeto, envuelto en casa (no en la mochila)
  CHECK_EQ(p.giftCount(), (uint8_t)1);
  CHECK_EQ(p.giftItem(0), (uint8_t)IT_CAKE);
  CHECK_EQ(p.items[IT_CAKE], (uint8_t)0);
  CHECK(p.hasBadge(0));
  CHECK(p.stageUnlocked(1));
  CHECK(!p.stageRewardReady(0));
  CHECK(p.stageRewardReady(1));
  // en flash, lo mismo que en RAM
  Pet q;
  reboot(q);
  CHECK_EQ(q.growMin, p.growMin);
  CHECK_EQ(q.bond, (uint8_t)100);
  CHECK_EQ(q.giftCount(), (uint8_t)1);
  CHECK(q.hasBadge(0));
  CHECK(!q.stageRewardReady(0));
  for (uint8_t k = 0; k < BOX_SLOTS; k++) {
    CHECK_EQ(q.boxState(k), p.boxState(k));
    if (p.boxState(k) == BOX_OK) CHECK(memcmp(&q.boxRecord(k), &p.boxRecord(k), sizeof(PetRecord)) == 0);
  }
}

TEST(stage, el_objeto_del_jefe_sale_siempre_de_la_tabla) {
  Pet p;
  sBorn(p);
  mockForceRandom(99);  // la tabla da huevo: lo mas raro tambien sale
  uint32_t t[1] = { p.uid };
  StageReward r = p.stageDone(5, true, t, 1);
  mockClearForcedRandom();
  CHECK(r.paid);
  CHECK_EQ(r.item, (uint8_t)IT_EGG);
  CHECK_EQ(p.giftCount(), (uint8_t)1);
}

TEST(stage, la_segunda_victoria_del_dia_no_paga_otra_vez) {
  Pet p;
  sBorn(p);
  uint32_t a = boxed(p, 3, 7, 900, 40);
  uint32_t t[2] = { p.uid, a };
  CHECK(p.stageDone(0, true, t, 2).paid);
  uint32_t grow = p.growMin, growA = p.boxRecord(3).growMin;
  uint8_t gifts = p.giftCount();
  StageReward r = p.stageDone(0, true, t, 2);
  CHECK(!r.paid);
  CHECK(!r.newBadge);
  CHECK_EQ(r.item, (uint8_t)IT_COUNT);
  CHECK_EQ(r.pets, (uint8_t)0);
  CHECK_EQ(p.growMin, grow);
  CHECK_EQ(p.boxRecord(3).growMin, growA);
  CHECK_EQ(p.giftCount(), gifts);
  // otro estadio, el mismo dia, si paga
  StageReward r1 = p.stageDone(1, true, t, 2);
  CHECK(r1.paid);
  CHECK_EQ(p.growMin, grow + STAGE_GROW_MIN);
  CHECK_EQ(p.stageRewardMask(), (uint8_t)0x03);
  // y tras un reinicio el mismo dia, tampoco
  Pet q;
  reboot(q, ST + 3600);
  CHECK(!q.stageRewardReady(0));
  CHECK(!q.stageDone(0, true, t, 2).paid);
}

TEST(stage, al_dia_siguiente_paga_otra_vez) {
  Pet p;
  sBorn(p);
  uint32_t t[1] = { p.uid };
  CHECK(p.stageDone(0, true, t, 1).paid);
  CHECK(p.stageDone(1, true, t, 1).paid);
  uint32_t grow = p.growMin;
  p.setClock(ST + 86400);
  CHECK(p.stageRewardReady(0));
  CHECK(p.stageRewardReady(1));
  CHECK_EQ(p.stageRewardMask(), (uint8_t)0);
  StageReward r = p.stageDone(1, true, t, 1);
  CHECK(r.paid);
  CHECK(!r.newBadge);
  CHECK_EQ(p.growMin, grow + STAGE_GROW_MIN);
  CHECK_EQ(p.stageRewardMask(), (uint8_t)0x02);  // el dia nuevo empieza sin lo de ayer
  CHECK(p.stageRewardReady(0));
  Pet q;
  reboot(q, ST + 86400);
  CHECK_EQ(q.stageRewardMask(), (uint8_t)0x02);
  reboot(q, ST + 2 * 86400);
  CHECK_EQ(q.stageRewardMask(), (uint8_t)0);
  // clearStageDay (serie): los de hoy otra vez
  p.clearStageDay();
  CHECK(p.savePending());
  CHECK(p.stageRewardReady(1));
  CHECK(p.stageDone(1, true, t, 1).paid);
}

TEST(stage, un_uid_que_ya_no_existe_se_salta) {
  Pet p;
  sBorn(p);
  uint32_t a = boxed(p, 0, 7, 900, 40);
  uint32_t gone = boxed(p, 5, 1, 1200, 40);
  CHECK(p.boxClear(5));  // lo borraron (BOXCLR) despues de elegir el equipo
  uint32_t grow = p.growMin;
  uint32_t t[4] = { 424242, p.uid, gone, a };
  StageReward r = p.stageDone(0, true, t, 4);
  CHECK(r.paid);
  CHECK_EQ(r.pets, (uint8_t)2);
  CHECK_EQ(p.growMin, grow + STAGE_GROW_MIN);
  CHECK_EQ(p.boxRecord(0).growMin, (uint32_t)(900 + STAGE_GROW_MIN));
  CHECK_EQ(p.boxState(5), (uint8_t)BOX_EMPTY);
  // un uid repetido cobra una vez
  uint32_t d[3] = { a, a, a };
  StageReward r2 = p.stageDone(1, true, d, 3);
  CHECK_EQ(r2.pets, (uint8_t)1);
  CHECK_EQ(p.boxRecord(0).growMin, (uint32_t)(900 + 2 * STAGE_GROW_MIN));
  // un hueco danado no se toca
  uint32_t c = boxed(p, 2, 4, 300, 10);
  {
    Preferences nv;
    nv.begin("tamapoke", false);
    PetRecord bad = p.boxRecord(2);
    bad.crc ^= 0x5555;
    nv.putBytes("box2", &bad, sizeof(bad));
  }
  p.boxReload();
  CHECK_EQ(p.boxState(2), (uint8_t)BOX_DAMAGED);
  uint32_t e[1] = { c };
  StageReward r3 = p.stageDone(2, true, e, 1);
  CHECK(r3.paid);
  CHECK_EQ(r3.pets, (uint8_t)0);
  CHECK_EQ(p.boxState(2), (uint8_t)BOX_DAMAGED);
}

TEST(stage, un_equipo_sin_el_de_casa) {
  Pet p;
  sBorn(p);
  uint32_t a = boxed(p, 1, 7, 900, 40);
  uint32_t b = boxed(p, 2, 1, 1900, 60);
  p.joy = 50;
  uint32_t grow = p.growMin;
  uint8_t bond = p.bond;
  uint32_t t[2] = { b, a };
  CHECK(p.teamCanFight(a));
  CHECK(p.teamCanFight(b));
  StageReward r = p.stageDone(0, true, t, 2);
  CHECK(r.paid);
  CHECK_EQ(r.pets, (uint8_t)2);
  CHECK_EQ(p.growMin, grow);  // el de casa no combatio: sin premio del jefe...
  CHECK_EQ(p.bond, bond);
  CHECK_EQ(r.joy, (uint8_t)PRACTICE_JOY);  // ...pero la felicidad de la practica, si
  CHECK_EQ(p.boxRecord(1).growMin, (uint32_t)(900 + STAGE_GROW_MIN));
  CHECK_EQ(p.boxRecord(2).growMin, (uint32_t)(1900 + STAGE_GROW_MIN));
  CHECK_EQ(p.giftCount(), (uint8_t)1);
}

TEST(stage, con_un_huevo_o_la_casa_vacia) {
  Pet p;
  sBorn(p);
  uint32_t first = p.uid;
  CHECK(p.store(0));  // en casa, un huevo de la mochila
  CHECK(p.isEgg());
  CHECK(!p.teamCanFight(0));
  CHECK_EQ(p.teamSlotOf(0), (int8_t)-1);  // el huevo (uid 0) no es nadie
  CHECK(p.teamCanFight(first));
  uint32_t t[2] = { 0, first };
  mockForceRandom(0);
  StageReward r = p.stageDone(0, true, t, 2);
  mockClearForcedRandom();
  CHECK(r.paid);
  CHECK_EQ(r.joy, (uint8_t)0);
  CHECK_EQ(r.pets, (uint8_t)1);
  CHECK_EQ(r.item, (uint8_t)IT_CAKE);
  CHECK_EQ(p.giftCount(), (uint8_t)1);  // espera envuelto a que nazca
  CHECK(p.isEgg());
  CHECK_EQ(p.ageMinutes, (uint32_t)0);  // al huevo no le pasa nada
  // la casa vacia: nadie abre regalos, el objeto va a la mochila con su aviso
  Pet v;
  sBorn(v);
  v.items[IT_EGG] = 0;
  uint32_t id = v.uid;
  CHECK(v.store(1));
  CHECK(v.isVacant());
  while (v.hasFound()) v.popFound();
  uint32_t u[1] = { id };
  mockForceRandom(0);
  StageReward rv = v.stageDone(0, true, u, 1);
  mockClearForcedRandom();
  CHECK(rv.paid);
  CHECK_EQ(rv.pets, (uint8_t)1);
  CHECK_EQ(rv.joy, (uint8_t)0);
  CHECK_EQ(v.giftCount(), (uint8_t)0);
  CHECK_EQ(v.items[IT_CAKE], (uint8_t)1);
  CHECK(v.hasFound());
  CHECK(v.isVacant());
}

TEST(stage, el_de_casa_dormido_o_en_un_final_no_puede_combatir) {
  Pet p;
  sBorn(p);
  CHECK(p.teamCanFight(p.uid));
  p.toggleLight();
  CHECK(!p.teamCanFight(p.uid));
  CHECK_EQ(p.teamSlotOf(p.uid), (int8_t)TEAM_HOME);  // sigue en casa
  p.toggleLight();
  p.startDying();
  CHECK(!p.teamCanFight(p.uid));
  uint32_t grow = p.growMin;
  uint32_t t[1] = { p.uid };
  StageReward r = p.stageDone(0, true, t, 1);  // si aun asi llega: nada que crecer
  CHECK_EQ(r.pets, (uint8_t)0);
  CHECK_EQ(r.joy, (uint8_t)0);
  CHECK_EQ(p.growMin, grow);
}

// ---------------------------------------------------------------- niveles y evolucion
TEST(stage, el_de_casa_sube_de_nivel_cuenta_para_la_medalla_y_puede_evolucionar) {
  Pet p;
  sBorn(p, 4);  // Charmander: evoluciona en el 16
  CHECK_EQ(p.speciesId, (int16_t)4);
  p.setLevel(9);
  CHECK(!(p.counted & PMB_LV10));
  uint32_t before = p.medalValue(MD_LV10);
  uint32_t t[1] = { p.uid };
  p.stageDone(0, true, t, 1);
  CHECK_EQ(p.level(), (uint16_t)12);  // 60 min = 3 niveles de 20
  CHECK(p.counted & PMB_LV10);  // v3.4: cuenta para la medalla del jugador (en el save() de stageDone)
  CHECK_EQ(p.medalValue(MD_LV10), before + 1);
  // del 15 al 16 de golpe: listo para evolucionar
  p.setLevel(15);
  p.fullness = p.joy = p.energy = 80;
  CHECK(!p.canEvolveNow());
  p.stageDone(1, true, t, 1);
  CHECK_EQ(p.level(), (uint16_t)16);
  CHECK(p.canEvolveNow());
  CHECK(p.wantEvolveButton());
  p.evolve();
  CHECK_EQ(p.speciesId, (int16_t)5);
  CHECK(p.evolving());
}

TEST(stage, uno_de_la_caja_que_pasa_su_nivel_de_evolucion_evoluciona_al_despertar) {
  Pet p;
  sBorn(p, 1);
  uint32_t a = boxed(p, 2, 4, levelStart(15), 30);  // Charmander Lv.15
  uint32_t t[1] = { a };
  StageReward r = p.stageDone(0, true, t, 1);
  CHECK_EQ(r.pets, (uint8_t)1);
  // en la caja: sigue siendo Charmander, ya Lv.16, y el hueco esta bien
  const PetRecord &rec = p.boxRecord(2);
  CHECK_EQ(rec.speciesId, (int16_t)4);
  CHECK_EQ(levelFor(rec.growMin), (uint16_t)16);
  CHECK(recordValid(rec));
  CHECK_EQ(rec.medals, (uint16_t)0);  // v3.4: aun sin contar (cuenta al despertarlo)
  // hiberna: horas apagada no lo cambian
  Pet q;
  q.begin();
  q.syncClock(ST + 3 * 3600);
  CHECK_EQ(q.boxState(2), (uint8_t)BOX_OK);
  CHECK_EQ(q.boxRecord(2).growMin, levelStart(15) + STAGE_GROW_MIN);
  CHECK_EQ(q.boxRecord(2).speciesId, (int16_t)4);
  // al despertar: en casa, Lv.16, y el boton de evolucionar (fromRecord borra el "mantener forma")
  q.declineEvolve();
  CHECK(q.wake(2));
  CHECK_EQ(q.uid, a);
  CHECK_EQ(q.speciesId, (int16_t)4);
  CHECK_EQ(q.level(), (uint16_t)16);
  CHECK(q.canEvolveNow());
  CHECK(q.wantEvolveButton());
  CHECK(q.counted & PMB_LV10);  // v3.4: al despertarlo cuenta para la medalla del jugador (el save() de wake)
  q.evolve();
  CHECK_EQ(q.speciesId, (int16_t)5);
  CHECK(q.evolving());
  CHECK(q.isRegistered(5));
}

TEST(stage, uno_de_la_caja_dormido_evoluciona_cuando_se_despierta) {
  Pet p;
  sBorn(p, 1);
  PetRecord r;
  memset(&r, 0, sizeof(r));
  r.speciesId = 7;  // Squirtle: en el 16
  r.growMin = levelStart(15);
  r.flags = REC_SLEEP;
  r.fullness = r.joy = r.energy = 80;
  r.geneAtk = r.geneDef = r.geneSpe = 100;
  CHECK(p.boxPut(0, r));
  uint32_t t[1] = { p.boxRecord(0).uid };
  p.stageDone(0, true, t, 1);
  CHECK(p.wake(0));
  CHECK(p.sleeping);
  CHECK(!p.canEvolveNow());  // dormido, espera
  p.toggleLight();
  CHECK(p.canEvolveNow());
  p.evolve();
  CHECK_EQ(p.speciesId, (int16_t)8);
}

// ---------------------------------------------------------------- cortes de luz
// Corta la luz tras cada escritura de stageDone (0, 1, 2... todas) y arranca otra vez: ningun hueco
// danado, la copia en RAM igual que la NVS, cada bicho con el premio entero o sin el (nunca dos), la
// medalla antes que la marca de "pagado hoy", y el estadio nunca se paga dos veces ese dia (ni
// repitiendolo tras el arranque).
TEST(stage, corte_de_luz_en_cada_escritura_de_stagedone) {
  Pet p;
  sBorn(p, 4);
  uint32_t a = boxed(p, 1, 7, 2000, 30);
  uint32_t b = boxed(p, 4, 1, 3000, 99);
  p.growMin = 1000;
  p.bond = 50;
  p.setBadges(0x07);
  // ayer se pago el estadio 0: hoy hay que empezar la mascara de cero
  p.setClock(ST - 86400);
  uint32_t t0[1] = { p.uid };
  CHECK(p.stageDone(0, true, t0, 1).paid);
  while (p.giftCount()) p.openGift();
  p.growMin = 1000;
  p.bond = 50;
  p.setClock(ST);  // guarda
  const uint32_t homeUid = p.uid;
  const uint32_t g0 = 1000, ga = 2000, gb = 3000;
  const uint8_t b0 = 50, ba = 30, bb = 99;
  uint32_t t[3] = { homeUid, a, b };
  mockNvsSnapshot();
  long total = 0;
  {
    Pet x;
    reboot(x);
    long w0 = mockNvsWrites();
    CHECK(x.stageDone(3, true, t, 3).paid);
    total = mockNvsWrites() - w0;
  }
  CHECK_MSG(total > 6, "stageDone escribe en la NVS");
  long beforeMask = -1;  // escrituras tras las que el estadio ya consta pagado
  for (long n = 0; n <= total; n++) {
    std::string at = "corte tras " + std::to_string(n) + " de " + std::to_string(total) + ": ";
    mockNvsRestore();
    {
      Pet x;
      reboot(x);
      mockNvsCutAfter(n);
      x.stageDone(3, true, t, 3);
      mockNvsCutAfter(-1);
      Pet m;  // lo que hay en la NVS: la copia en RAM de x tiene que ser igual
      m.begin();
      for (uint8_t k = 0; k < BOX_SLOTS; k++) {
        CHECK_MSG(m.boxState(k) == x.boxState(k), at + "estado del hueco " + std::to_string(k));
        if (x.boxState(k) == BOX_OK)
          CHECK_MSG(memcmp(&m.boxRecord(k), &x.boxRecord(k), sizeof(PetRecord)) == 0,
                    at + "la RAM del hueco " + std::to_string(k) + " no es la NVS");
      }
    }
    Pet q;
    reboot(q);
    CHECK_MSG(q.uid == homeUid && q.speciesId == 4, at + "el de casa");
    for (uint8_t k = 0; k < BOX_SLOTS; k++) CHECK_MSG(q.boxState(k) != BOX_DAMAGED, at + "hueco danado");
    CHECK_MSG(q.boxState(1) == BOX_OK && q.boxRecord(1).uid == a, at + "A en su hueco");
    CHECK_MSG(q.boxState(4) == BOX_OK && q.boxRecord(4).uid == b, at + "B en su hueco");
    uint32_t qa = q.boxRecord(1).growMin, qb = q.boxRecord(4).growMin;
    CHECK_MSG(q.growMin == g0 || q.growMin == g0 + STAGE_GROW_MIN, at + "crecimiento del de casa");
    CHECK_MSG(qa == ga || qa == ga + STAGE_GROW_MIN, at + "crecimiento de A");
    CHECK_MSG(qb == gb || qb == gb + STAGE_GROW_MIN, at + "crecimiento de B");
    // crecimiento y vinculo de uno de la caja van en la misma escritura
    CHECK_MSG(q.boxRecord(1).bond == (qa == ga ? ba : ba + STAGE_BOND), at + "vinculo de A");
    CHECK_MSG(q.boxRecord(4).bond == (qb == gb ? bb : 100), at + "vinculo de B");
    CHECK_MSG(q.bond == b0 || q.bond == b0 + STAGE_BOND, at + "vinculo del de casa");
    bool landed = q.growMin != g0 || qa != ga || qb != gb || q.bond != b0 || q.giftCount() > 0;
    bool ready = q.stageRewardReady(3);
    CHECK_MSG(!(landed && ready), at + "llego parte del premio y el estadio se puede volver a cobrar");
    CHECK_MSG(ready || q.hasBadge(3), at + "pagado sin medalla");
    CHECK_MSG(q.stageRewardReady(0), at + "la mascara de ayer paso a hoy");
    CHECK_MSG(q.badgeMask() == 0x07 || q.badgeMask() == 0x0F, at + "medallas");
    if (!ready && beforeMask < 0) beforeMask = n;
    if (beforeMask >= 0) CHECK_MSG(!ready, at + "el estadio volvio a estar sin pagar");
    // repetirlo tras el arranque, el mismo dia: nunca dos premios
    StageReward r2 = q.stageDone(3, true, t, 3);
    CHECK_MSG(r2.paid == ready, at + "el segundo intento paga solo si el primero no llego a marcarlo");
    CHECK_MSG(q.hasBadge(3), at + "la medalla tras repetirlo");
    qa = q.boxRecord(1).growMin;
    qb = q.boxRecord(4).growMin;
    CHECK_MSG(q.growMin <= g0 + STAGE_GROW_MIN && qa <= ga + STAGE_GROW_MIN && qb <= gb + STAGE_GROW_MIN,
              at + "un bicho cobro dos veces");
    if (ready) {  // no habia llegado nada: el segundo intento lo paga entero
      CHECK_MSG(q.growMin == g0 + STAGE_GROW_MIN && qa == ga + STAGE_GROW_MIN && qb == gb + STAGE_GROW_MIN,
                at + "el premio entero al repetirlo");
      CHECK_MSG(q.giftCount() == 1, at + "un objeto al repetirlo");
    }
    CHECK_MSG(q.giftCount() <= 1, at + "dos objetos");
    Pet r;  // un segundo arranque no cambia nada
    reboot(r);
    for (uint8_t k = 0; k < BOX_SLOTS; k++) {
      CHECK_MSG(r.boxState(k) == q.boxState(k), at + "segundo arranque");
      if (q.boxState(k) == BOX_OK)
        CHECK_MSG(memcmp(&r.boxRecord(k), &q.boxRecord(k), sizeof(PetRecord)) == 0, at + "segundo arranque");
    }
    CHECK_EQ(r.growMin, q.growMin);
  }
  // la marca de pagado va antes que el premio: 1. "bdg", 2. "stM" = 0 (dia nuevo), 3. "stD", 4. "stM"
  CHECK_EQ(beforeMask, 4L);
  CHECK(beforeMask < total);
}
