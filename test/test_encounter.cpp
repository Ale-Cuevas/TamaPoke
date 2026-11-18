// Encuentros salvajes (v2.2, encounter.h + Pet): las tiradas del dia (10 %, 25 % paseando, 3 % por hora de
// viaje, uno al dia), las 6 h en el patio, el nivel alrededor del ultimo estadio, la especie (5 % exclusivas,
// el mapa pesa x3), el combate de un solo rival, el premio, la Poke Ball y el huevo de captura (forma base,
// nivel 1), y lo que sobrevive a un reinicio.
#include "framework.h"
#include "shim/Arduino.h"
#include "shim/Preferences.h"
#include "../pet.h"
#include "../encounter.h"
#include "../shop.h"

static const uint32_t EDAY = 20000;                        // un dia cualquiera (numero de dia unix)
static uint32_t at(uint32_t day, int h, int m = 0) { return day * 86400 + h * 3600 + m * 60; }

// bicho recien nacido (Charmander), con reloj a las 8:00 y la NVS limpia
static void eBorn(Pet &p) {
  mockClearForcedRandom();
  mockNvsReset();
  mockSetMillis(0);
  mockSerialSilence(true);
  p.begin();
  if (p.awaitingStarter()) p.chooseStarter(4);
  p.eggTap();
  p.eggTap();
  p.eggTap();
  p.setClock(at(EDAY, 8));
}

// fuerza random() a un valor con el que la tirada de encuentro (Pet: rng = v << 16 | v) sale con permil
static void forceEncHit(uint16_t permil) {
  for (uint32_t v = 1; v < 0x10000; v++) {
    uint32_t rng = (v << 16) | v;
    if (encHit(permil, rng)) {
      mockForceRandom((long)v);
      return;
    }
  }
}

// un encuentro ya en el patio (Pidgey nv. del sorteo)
static void eSpawn(Pet &p, int16_t dex = 16) {
  CHECK(p.encSpawnNow(dex));
  CHECK(p.encActive());
}

// ------------------------------------------------------------------ logica pura
TEST(encounter, la_hora_de_la_tirada_base_va_de_9_a_20_y_es_fija_por_dia) {
  int lo = 9999, hi = 0, distinct = 0;
  uint16_t prev = 0;
  for (uint32_t d = 0; d < 5000; d++) {
    uint16_t m = encBaseMinute(EDAY + d);
    CHECK_EQ(m, encBaseMinute(EDAY + d));
    if (m < lo) lo = m;
    if (m > hi) hi = m;
    distinct += m != prev;
    prev = m;
  }
  CHECK(lo >= ENC_FIRST_MIN);
  CHECK(hi <= ENC_LAST_MIN);
  CHECK(lo < ENC_FIRST_MIN + 30);  // cubre todo el rango
  CHECK(hi > ENC_LAST_MIN - 30);
  CHECK(distinct > 4900);
}

TEST(encounter, dura_6_horas_y_un_reloj_que_vuelve_atras_no_lo_eterniza) {
  uint32_t s = at(EDAY, 12);
  CHECK(!encAlive(0, s));
  CHECK(encAlive(s, s));
  CHECK(encAlive(s, s + 359 * 60));
  CHECK(!encAlive(s, s + 360 * 60));
  CHECK_EQ(encMinutesLeft(s, s), (uint16_t)360);
  CHECK_EQ(encMinutesLeft(s, s + 60 * 60), (uint16_t)300);
  CHECK(encAlive(s, s - 3600));          // una hora atras: sigue
  CHECK(!encAlive(s, s - 7 * 3600));     // un dia atras: se acabo
}

TEST(encounter, la_tirada_del_viaje_es_3_por_ciento_por_hora) {
  CHECK_EQ(encTravelPermil(1), (uint16_t)30);
  CHECK_EQ(encTravelPermil(3), (uint16_t)90);
  CHECK_EQ(encTravelPermil(8), (uint16_t)240);
  CHECK_EQ(encTravelPermil(100), (uint16_t)1000);
  for (int k = 0; k < 3; k++) CHECK_EQ(encTravelPermil(TRV_MIN[k] / 60), (uint16_t)(TRV_MIN[k] / 60 * 30));
}

TEST(encounter, el_objeto_del_patio_depende_del_mapa) {
  CHECK_EQ(encMapObject(1), (uint8_t)ENC_OBJ_BUSH);
  CHECK_EQ(encMapObject(2), (uint8_t)ENC_OBJ_ROCK);
  CHECK_EQ(encMapObject(3), (uint8_t)ENC_OBJ_BUSH);
  CHECK_EQ(encMapObject(4), (uint8_t)ENC_OBJ_SAND);
  CHECK_EQ(encMapObject(5), (uint8_t)ENC_OBJ_ROCK);
  CHECK_EQ(encMapObject(0), (uint8_t)ENC_OBJ_BUSH);
}

TEST(encounter, el_nivel_ronda_el_ultimo_estadio_vencido) {
  CHECK_EQ(encLevelCenter(0), (uint8_t)ENC_LV_CENTER_NONE);
  CHECK_EQ(encLevelCenter(0x01), STAGES[0].lvMax);
  CHECK_EQ(encLevelCenter(0x07), STAGES[2].lvMax);
  CHECK_EQ(encLevelCenter(0x80), STAGES[7].lvMax);
  CHECK_EQ(encLevelCenter(0xFF), STAGES[7].lvMax);
  for (uint8_t center : { (uint8_t)10, (uint8_t)40, (uint8_t)80 }) {
    uint32_t rng = 12345 + center;
    const int N = 20000;
    int near = 0, far = 0;
    double sum = 0;
    for (int i = 0; i < N; i++) {
      int lv = encRollLevel(center, rng);
      int dv = lv - center;
      CHECK_RANGE(lv, ENC_LV_MIN, ENC_LV_MAX);
      if (center > 20) CHECK_RANGE(dv, -15, 15);
      sum += lv;
      near += dv >= -7 && dv <= 7;
      far += dv <= -12 || dv >= 12;
    }
    if (center > 20) CHECK_RANGE(sum / N, center - 0.5, center + 0.5);
    CHECK_MSG(near * 100 / N >= 70, "casi siempre cerca del centro");
    if (center > 20) CHECK_MSG(far > N / 50 && far < N / 10, "alguna vez a 12-15 niveles: " + std::to_string(far));
  }
  uint32_t rng = 9;
  for (int i = 0; i < 2000; i++) CHECK(encRollLevel(2, rng) >= ENC_LV_MIN);  // sin estadios no baja de 2
}

TEST(encounter, un_5_por_ciento_son_exclusivas_y_mew_es_la_mas_rara) {
  uint32_t rng = 777;
  const int N = 100000;
  int excl = 0, mew = 0, per[DEX_COUNT + 1] = { 0 };
  for (int i = 0; i < N; i++) {
    bool ex = false;
    int16_t b = encPickBase(1, rng, ex);
    CHECK_RANGE(b, (int16_t)1, (int16_t)DEX_COUNT);
    per[b]++;
    if (ex) {
      excl++;
      mew += b == 151;
      CHECK_EQ((int)DEX_TBL[b].rarity, (int)R_ENCUENTRO);
    } else {
      // el sorteo normal: formas base comunes o raras; ni legendarias ni exclusivas
      CHECK_MSG(DEX_TBL[b].rarity == R_COMUN || DEX_TBL[b].rarity == R_RARO, std::string("no deberia salir ") + DEX_TBL[b].name);
    }
  }
  CHECK_RANGE(excl * 100.0 / N, 4.5, 5.5);
  CHECK_RANGE(mew * 100.0 / excl, 2.5, 5.5);  // 4 de cada 100 exclusivas
  for (int16_t d : { 138, 140, 142, 147 }) CHECK_RANGE(per[d] * 100.0 / excl, 21.0, 27.0);
  CHECK_EQ(per[144] + per[145] + per[146] + per[150], 0);  // las legendarias no salen aqui
}

TEST(encounter, el_mapa_hace_mas_probables_sus_tipos) {
  auto waterShare = [](uint8_t map) {
    uint32_t rng = 4242;
    int water = 0, normal = 0;
    for (int i = 0; i < 60000; i++) {
      bool ex;
      int16_t b = encPickBase(map, rng, ex);
      if (ex) continue;
      normal++;
      water += DEX_TBL[b].type1 == TY_AGUA || DEX_TBL[b].type2 == TY_AGUA;
    }
    return water * 100.0 / normal;
  };
  double beach = waterShare(4), quarry = waterShare(2);
  CHECK_MSG(beach > quarry * 1.8, "en la playa salen mas de agua: " + std::to_string(beach) + " vs " + std::to_string(quarry));
  CHECK_EQ(encLineWeight(7, 4), (uint8_t)ENC_MAP_WEIGHT);  // Squirtle en la playa
  CHECK_EQ(encLineWeight(7, 2), (uint8_t)1);
  CHECK_EQ(encLineWeight(74, 2), (uint8_t)ENC_MAP_WEIGHT);  // Geodude en la cantera
  CHECK_EQ(encLineWeight(2, 1), (uint8_t)0);                // Ivysaur: evolucion
  CHECK_EQ(encLineWeight(150, 1), (uint8_t)0);              // Mewtwo: legendario
  CHECK_EQ(encLineWeight(147, 4), (uint8_t)0);              // Dratini: solo por el 5 %
}

TEST(encounter, el_salvaje_sale_en_la_forma_de_su_nivel_con_entrenamiento_del_estadio) {
  uint32_t rng = 31337;
  for (int i = 0; i < 5000; i++) {
    EncWild w = encRollWild(4, 0x7F, rng);  // 7 estadios: centro 70
    CHECK_RANGE((int)w.level, 55, 85);
    const DexEntry &d = DEX_TBL[w.dex];
    // a ese nivel, ya evoluciono todo lo que tocaba
    if (d.evolvesTo) CHECK_MSG(w.level < d.evolveLevel, std::string(d.name) + " deberia haber evolucionado");
    CHECK_RANGE(w.tAtk, STAGES[6].trMin, STAGES[6].trMax);
    CHECK_RANGE(w.tSpe, STAGES[6].trMin, STAGES[6].trMax);
  }
  for (int i = 0; i < 500; i++) {
    EncWild w = encRollWild(1, 0, rng);
    CHECK(w.tAtk <= ENC_TR_NONE_MAX && w.tDef <= ENC_TR_NONE_MAX);
  }
}

TEST(encounter, el_combate_salvaje_es_uno_contra_uno) {
  StageFoe wild;
  wild.dex = 19;  // Rattata nv. 3
  wild.level = 3;
  wild.aiTaps = 1;
  Fighter me = makeFighter(6, 60, 100, 100, 100, 50, 50, 50);  // Charizard nv. 60
  StageRun r;
  r.beginWild(me, wild, 99);
  CHECK(r.isWild());
  CHECK(!r.isBoss());
  CHECK_EQ(r.teamN, (uint8_t)1);
  CHECK_EQ(r.foeN, (uint8_t)1);
  int guard = 0;
  while (!r.koPending() && guard++ < 100) r.attack();
  CHECK(r.koPending());
  CHECK_EQ(r.advance(), (uint8_t)SN_CLEARED);
  // y al reves: si cae el tuyo, se pierde (no hay relevo)
  StageFoe big;
  big.dex = 149;
  big.level = 90;
  Fighter weak = makeFighter(10, 2, 100, 100, 100, 0, 0, 0);
  r.beginWild(weak, big, 7);
  guard = 0;
  while (!r.koPending() && guard++ < 100) r.attack();
  CHECK_EQ(r.advance(), (uint8_t)SN_LOST);
  // una fase normal sigue teniendo 3 rivales y jefe
  r.begin(0, &me, 1, 5);
  CHECK_EQ(r.foeN, (uint8_t)STAGE_FOES);
  CHECK(!r.isWild());
}

// ------------------------------------------------------------------ Pet: las tiradas del dia
// tasa de dias con encuentro: cada dia a las 21:00, con (o sin) 6000 pasos
static double dailyRate(bool walk, int days) {
  Pet p;
  eBorn(p);
  randomSeed(2026);
  int hits = 0;
  for (int i = 1; i <= days; i++) {
    uint32_t d = EDAY + i;
    p.lastSeenEpoch = at(d, 21);
    if (walk) p.addSteps(ENC_WALK_STEPS, d);
    else p.encCheck();
    if (p.encActive()) {
      hits++;
      p.encClear();
    }
  }
  return hits * 100.0 / days;
}

// las probabilidades salen de tuning.h: hoy un 10 % (ENC_BASE_PERMIL) y, paseando, un 25 % el dia
TEST(encounter, un_dia_normal_y_paseando_salen_a_su_probabilidad) {
  double base = dailyRate(false, 4000), walk = dailyRate(true, 4000);
  double wantBase = ENC_BASE_PERMIL / 10.0;
  double wantWalk = 100.0 * (1.0 - (1.0 - ENC_BASE_PERMIL / 1000.0) * (1.0 - ENC_WALK_PERMIL / 1000.0));
  CHECK_MSG(base > wantBase - 1.5 && base < wantBase + 1.5, "base " + std::to_string(base));
  CHECK_MSG(walk > wantWalk - 2.5 && walk < wantWalk + 2.5, "paseando " + std::to_string(walk));
}

TEST(encounter, la_tirada_base_espera_a_su_hora_y_solo_uno_al_dia) {
  Pet p;
  eBorn(p);
  uint16_t m = encBaseMinute(EDAY);
  p.lastSeenEpoch = at(EDAY, 0) + (m - 1) * 60;
  p.encCheck();
  CHECK_EQ(p.encRolledMask() & ENC_ROLL_BASE, 0);  // aun no
  forceEncHit(ENC_BASE_PERMIL);  // la tirada sale
  p.lastSeenEpoch = at(EDAY, 0) + m * 60;
  p.encCheck();
  CHECK(p.encActive());
  CHECK(p.encRolledMask() & ENC_ROLL_SPAWNED);
  CHECK(p.encSoundPending());
  p.takeEncSound();
  CHECK(!p.encSoundPending());
  p.encClear();
  // el mismo dia ya no sale otro: ni paseando ni viajando
  p.addSteps(ENC_WALK_STEPS, EDAY);
  p.encTravelRoll(8);
  CHECK(!p.encActive());
  // al dia siguiente, otra vez
  p.lastSeenEpoch = at(EDAY + 1, 21);
  p.encCheck();
  CHECK(p.encActive());
  mockClearForcedRandom();
}

TEST(encounter, viajar_tira_al_volver) {
  Pet p;
  eBorn(p);
  PetRecord r = p.toRecord();
  r.speciesId = 25;
  CHECK(p.boxPut(0, r));
  uint32_t d = EDAY;
  while (encBaseMinute(d) < 16 * 60) d++;  // un dia con la tirada base tarde: a las 15:00 aun no toca
  p.lastSeenEpoch = at(d, 7);
  CHECK(p.startTravel(0, 1, 2));  // 8 h
  forceEncHit(encTravelPermil(8));
  p.lastSeenEpoch = at(d, 15, 1);
  p.travelCheck();
  CHECK(p.encActive());
  CHECK_EQ(p.encRolledMask() & ENC_ROLL_BASE, 0);  // fue el viaje
  mockClearForcedRandom();
}

TEST(encounter, sin_nadie_con_quien_pelear_o_sin_hora_no_se_tira) {
  Pet p;
  eBorn(p);
  mockForceRandom(0);
  p.lastSeenEpoch = 0;  // sin hora
  p.encCheck();
  CHECK(!p.encActive());
  CHECK(!p.encSpawnNow());
  // casa vacia y caja vacia
  p.setClock(at(EDAY, 21));
  p.release();
  mockAdvanceMillis(CEREMONY_MS + 1);
  p.update(millis());
  p.setBagEggs(0);
  CHECK(p.isVacant());
  p.encCheck();
  CHECK(!p.encActive());
  CHECK_EQ(p.encRolledMask() & ENC_ROLL_BASE, 0);  // la tirada se guarda para cuando haya alguien
  mockClearForcedRandom();
}

TEST(encounter, se_va_a_las_6_horas_sin_pelear) {
  Pet p;
  eBorn(p);
  p.lastSeenEpoch = at(EDAY, 12);
  eSpawn(p);
  p.lastSeenEpoch = at(EDAY, 17, 59);
  p.encCheck();
  CHECK(p.encActive());
  CHECK_EQ(p.encMinutesLeft(), (uint16_t)1);
  p.lastSeenEpoch = at(EDAY, 18);
  p.encCheck();
  CHECK(!p.encActive());
  CHECK(!p.encShown());
}

TEST(encounter, sobrevive_a_un_reinicio) {
  Pet p;
  eBorn(p);
  p.lastSeenEpoch = at(EDAY, 12);
  eSpawn(p, 147);
  CHECK(p.encExclusive());  // Dratini: de las exclusivas
  int16_t dex = p.encDex();
  uint8_t lv = p.encLevel();
  p.flushSave();
  Pet b;
  b.begin();
  b.syncClock(at(EDAY, 13));
  CHECK(b.encActive());
  CHECK_EQ(b.encDex(), dex);
  CHECK_EQ(b.encLevel(), lv);
  CHECK(b.encExclusive());
  CHECK_EQ(b.encSpawnEpoch(), at(EDAY, 12));
  CHECK(b.encRolledMask() & ENC_ROLL_SPAWNED);
  // apagada mas de 6 h: al volver ya no esta
  Pet c;
  c.begin();
  c.syncClock(at(EDAY, 19));
  CHECK(!c.encActive());
}

// ------------------------------------------------------------------ Pet: pelear y capturar
TEST(encounter, perder_lo_espanta_sin_premio) {
  Pet p;
  eBorn(p);
  eSpawn(p);
  uint32_t g = p.growMin;
  EncReward r = p.encounterDone(false, p.uid);
  CHECK(!r.won);
  CHECK_EQ(r.item, (uint8_t)IT_COUNT);
  CHECK_EQ(p.growMin, g);
  CHECK(!p.encShown());
  CHECK_EQ(p.captureBlock(), (uint8_t)CAP_NONE);
  CHECK(!p.captureWild());
}

TEST(encounter, ganar_da_una_hora_y_quiza_un_objeto_y_luego_la_poke_ball_captura) {
  Pet p;
  eBorn(p);
  p.joy = 50;
  eSpawn(p, 148);  // Dragonair: el huevo sera de Dratini
  bool shiny = p.encShiny();
  uint32_t g = p.growMin;
  mockForceRandom(0);  // el objeto sale (0 < 50) y es una tarta
  EncReward r = p.encounterDone(true, p.uid);
  mockClearForcedRandom();
  CHECK(r.won);
  CHECK_EQ(p.growMin, g + ENC_WIN_GROW_MIN);
  CHECK_EQ(p.joy, (uint8_t)(50 + ENC_WIN_JOY));
  CHECK_EQ(r.item, (uint8_t)IT_CAKE);
  CHECK_EQ(p.items[IT_CAKE], (uint8_t)1);
  CHECK(!p.encActive());
  CHECK(p.encWonPending());
  CHECK(p.encShown());
  // otra vez no paga
  EncReward again = p.encounterDone(true, p.uid);
  CHECK(!again.won);
  CHECK_EQ(p.growMin, g + ENC_WIN_GROW_MIN);
  // la Poke Ball
  CHECK_EQ(p.items[IT_BALL], (uint8_t)BALL_START);
  uint8_t eggs = p.items[IT_EGG];
  CHECK_EQ(p.captureBlock(), (uint8_t)CAP_OK);
  CHECK(p.captureWild());
  CHECK_EQ(p.items[IT_BALL], (uint8_t)(BALL_START - 1));
  CHECK_EQ(p.items[IT_EGG], (uint8_t)(eggs + 1));
  CHECK_EQ(p.captureEggs(), (uint8_t)1);
  CHECK_EQ(p.captureEggSpecies(0), (int16_t)147);  // la forma base
  CHECK(!p.encShown());
  // y sobrevive a un reinicio
  Pet b;
  b.begin();
  CHECK_EQ(b.captureEggs(), (uint8_t)1);
  CHECK_EQ(b.captureEggSpecies(0), (int16_t)147);
  (void)shiny;
}

TEST(encounter, el_objeto_sale_la_mitad_de_las_veces_y_nunca_monedas_ni_bolas) {
  Pet p;
  eBorn(p);
  randomSeed(11);
  const int N = 20000;
  int n[IT_COUNT] = { 0 };
  for (int i = 0; i < N; i++) n[p.rollWildItem()]++;
  CHECK_EQ(n[IT_BALL] + n[IT_EGG], 0);
  CHECK_RANGE(n[IT_CAKE] * 100.0 / N, WDROP_CAKE - 2.0, WDROP_CAKE + 2.0);
  CHECK_RANGE(n[IT_RARECANDY] * 100.0 / N, WDROP_RARECANDY - 1.0, WDROP_RARECANDY + 1.0);
  CHECK_RANGE(n[IT_REVIVE] * 100.0 / N, WDROP_REVIVE - 1.0, WDROP_REVIVE + 1.0);
  CHECK_RANGE(n[IT_CHARM] * 100.0 / N, WDROP_CHARM - 1.0, WDROP_CHARM + 1.0);
  int colored = 0;  // v3.2: una baya de color al azar, de los seis
  for (int c = 0; c < BERRY_COLORS; c++) {
    colored += n[IT_BERRY_R + c];
    CHECK(n[IT_BERRY_R + c] > 0);
  }
  CHECK_RANGE(colored * 100.0 / N, WDROP_COLORED - 1.5, WDROP_COLORED + 1.5);
  int drops = 0;
  uint16_t coins = p.coins;
  for (int i = 0; i < 400; i++) {
    p.lastSeenEpoch = at(EDAY, 12);
    eSpawn(p);
    for (uint8_t &it : p.items) it = 0;
    EncReward r = p.encounterDone(true, p.uid);
    drops += r.item != IT_COUNT;
    p.releaseWild();
  }
  CHECK_RANGE(drops, 160, 240);
  CHECK_EQ(p.coins, coins);
}

TEST(encounter, el_de_la_caja_tambien_crece_al_ganar) {
  Pet p;
  eBorn(p);
  PetRecord r = p.toRecord();
  r.speciesId = 25;
  r.growMin = 500;
  CHECK(p.boxPut(3, r));
  uint32_t id = p.boxRecord(3).uid;
  eSpawn(p);
  uint32_t g = p.growMin;
  EncReward w = p.encounterDone(true, id);
  CHECK(w.won);
  CHECK_EQ(p.boxRecord(3).growMin, (uint32_t)(500 + ENC_WIN_GROW_MIN));
  CHECK_EQ(p.growMin, g);  // el de casa no
}

TEST(encounter, sin_bolas_o_sin_sitio_no_se_captura) {
  Pet p;
  eBorn(p);
  eSpawn(p);
  CHECK(p.encounterDone(true, p.uid).won);
  p.items[IT_BALL] = 0;
  CHECK_EQ(p.captureBlock(), (uint8_t)CAP_NO_BALL);
  CHECK(!p.captureWild());
  p.items[IT_BALL] = 1;
  p.items[IT_EGG] = ITEM_MAX;
  CHECK_EQ(p.captureBlock(), (uint8_t)CAP_NO_ROOM);
  p.items[IT_EGG] = 0;
  CHECK_EQ(p.captureBlock(), (uint8_t)CAP_OK);
  p.releaseWild();  // "no": vuelve a la hierba, la bola no se gasta
  CHECK_EQ(p.items[IT_BALL], (uint8_t)1);
  CHECK(!p.encShown());
  // la cola de huevos de captura tiene tope
  for (int i = 0; i < CAP_EGG_MAX; i++) {
    p.items[IT_BALL] = 1;
    eSpawn(p, 19);
    CHECK(p.encounterDone(true, p.uid).won);
    CHECK(p.captureWild());
  }
  CHECK_EQ(p.captureEggs(), (uint8_t)CAP_EGG_MAX);
  p.items[IT_BALL] = 1;
  eSpawn(p, 19);
  CHECK(p.encounterDone(true, p.uid).won);
  CHECK_EQ(p.captureBlock(), (uint8_t)CAP_NO_ROOM);
}

TEST(encounter, el_huevo_de_captura_sale_primero_y_nace_a_nivel_1_con_su_shiny) {
  Pet p;
  eBorn(p);
  eSpawn(p, 141);  // Kabutops
  CHECK(p.encounterDone(true, p.uid).won);
  bool sh = p.encShiny();
  CHECK(p.captureWild());
  CHECK_EQ(p.items[IT_EGG], (uint8_t)1);
  // la casa queda libre: el huevo de la mochila es el de captura
  p.release();
  mockAdvanceMillis(CEREMONY_MS + 1);
  p.update(millis());
  CHECK(p.isVacant());
  CHECK(p.hatchFromBag());
  CHECK_EQ(p.eggSpecies(), (int16_t)140);  // Kabuto
  CHECK_EQ(p.eggIsShiny(), sh);
  CHECK(p.eggIsCaptured());
  CHECK_EQ(p.captureEggs(), (uint8_t)0);
  p.hatchNow();
  CHECK_EQ(p.speciesId, (int16_t)140);
  CHECK_EQ(p.level(), (uint16_t)1);
  CHECK(!p.eggIsCaptured());
}

TEST(encounter, el_huevo_de_captura_de_la_incubadora_vuelve_a_la_cola) {
  Pet p;
  eBorn(p);
  // un bicho en la caja para despertarlo encima del huevo
  PetRecord r = p.toRecord();
  r.speciesId = 25;
  CHECK(p.boxPut(4, r));
  eSpawn(p, 151);  // Mew
  CHECK(p.encounterDone(true, p.uid).won);
  CHECK(p.captureWild());
  CHECK(p.store(0));  // el de casa a la caja: sale el huevo de captura
  CHECK(p.isEgg());
  CHECK_EQ(p.eggSpecies(), (int16_t)151);
  CHECK(p.eggIsCaptured());
  CHECK(p.wake(4));  // Pikachu a casa: el huevo espera en la incubadora
  CHECK(p.eggHeld());
  p.release();  // se va Pikachu: el huevo de la incubadora vuelve a la mochila...
  mockAdvanceMillis(CEREMONY_MS + 1);
  p.update(millis());
  CHECK(p.isVacant());
  CHECK_EQ(p.captureEggs(), (uint8_t)1);  // ...como huevo de captura
  CHECK_EQ(p.captureEggSpecies(0), (int16_t)151);
  CHECK(p.hatchFromBag());
  CHECK_EQ(p.eggSpecies(), (int16_t)151);
}

TEST(encounter, las_exclusivas_ya_no_salen_de_huevos) {
  Pet p;
  eBorn(p);
  memset(p.dexReg, 0xFF, sizeof(p.dexReg));  // la pokedex completa: legendarios posibles
  randomSeed(5);
  for (int i = 0; i < 30000; i++) {
    int16_t d = p.pickEggSpecies();
    CHECK_MSG(DEX_TBL[d].rarity != R_ENCUENTRO, std::string("de un huevo salio ") + DEX_TBL[d].name);
  }
}

TEST(encounter, un_guardado_de_antes_recibe_3_poke_balls_una_vez) {
  Pet p;
  eBorn(p);
  p.setBalls(0);
  Preferences nv;
  nv.begin("tamapoke", false);
  nv.remove("ballg");  // como un guardado de la v2.1
  nv.end();
  Pet b;
  b.begin();
  CHECK_EQ(b.items[IT_BALL], (uint8_t)BALL_START);
  Pet c;
  c.begin();
  CHECK_EQ(c.items[IT_BALL], (uint8_t)BALL_START);  // no otra vez
}

TEST(encounter, la_poke_ball_se_vende_y_no_se_usa_a_mano) {
  CHECK_EQ(priceOfItem(IT_BALL), (uint16_t)100);
  bool inShop = false;
  for (int k = 0; k < SHOP_ITEMS; k++) inShop |= SHOP_ORDER[k] == IT_BALL;
  CHECK(inShop);
  Pet p;
  eBorn(p);
  CHECK(p.items[IT_BALL] > 0);
  CHECK(!p.canUseItem(IT_BALL));
  CHECK(!p.useItem(IT_BALL));
}
