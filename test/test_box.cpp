// Tests de la caja (docs/NEXT_FEATURES.md, Tarea BOX, v1.37): el registro (box.h) y los
// movimientos de Pet sobre la NVS del shim, cortes de luz incluidos.
#include "framework.h"
#include "shim/Arduino.h"
#include "shim/Preferences.h"
#include "../pet.h"
#include "../box.h"
#include <string.h>
#include <functional>
#include <vector>

static const uint32_t DAY0 = 20000u * 86400;  // un dia cualquiera, a medianoche

static void advance(Pet &p, uint32_t minutes) {
  for (uint32_t i = 0; i < minutes; i++) {
    mockAdvanceMillis(PET_TICK_MS);
    p.update(millis());
  }
}

static void hatchNow(Pet &p) {
  p.eggTap();
  p.eggTap();
  p.eggTap();
}

// bicho recien nacido de la especie pedida, con la NVS limpia. Con la mochila llena de huevos
// (v1.39): cada guardar en la caja deja uno en casa, como hacia antes de limitarlos
static void freshPet(Pet &p, int16_t dex) {
  mockNvsReset();
  mockSetMillis(0);
  p.begin();
  if (p.awaitingStarter()) p.chooseStarter(dex);
  p.items[IT_EGG] = ITEM_MAX;
  hatchNow(p);  // hatch() guarda: la mochila tambien
}

// valores reconocibles en todo lo del individuo, y guardados (rename guarda el estado entero)
static void dress(Pet &p, uint8_t seed) {
  p.fullness = 50 + seed;
  p.joy = 60 + seed;
  p.energy = 40 + seed;
  p.poops = seed % 3;
  p.weight = 10 + seed;
  p.geneAtk = 91 + seed;
  p.geneDef = 92 + seed;
  p.geneSpe = 93 + seed;
  p.trAtk = 5 + seed;
  p.trDef = 6 + seed;
  p.trSpe = 7 + seed;
  p.careMistakes = seed % 4;
  p.bond = 30 + seed;
  p.ageMinutes = 1000 + seed * 100u;
  p.growMin = 500 + seed * 37u;
  p.counted = PMB_LV10 | ((seed & 1) ? PMB_FAV : 0);
  p.berryKnown = seed & 1;
  p.shiny = (seed & 2) != 0;
  if (p.shiny) p.dbgRegister(p.speciesId, true);  // (como al nacer shiny: v3.4, la medalla Shiny lo cuenta)
  char nm[12];
  snprintf(nm, sizeof(nm), "PET%u", seed);
  p.rename(nm);
}

// lo del individuo es lo mismo (todo menos la hora de entrada y el CRC)
static bool sameIndividual(PetRecord a, PetRecord b) {
  a.storedEpoch = b.storedEpoch = 0;
  a.crc = b.crc = 0;
  return memcmp(&a, &b, sizeof(a)) == 0;
}

// los bichos que hay tras un arranque: el activo (si ha nacido) y los de la caja
struct World {
  std::vector<PetRecord> pets;
  int damaged = 0;
};
static World worldOf(Pet &p) {
  World w;
  if (!p.isEgg()) w.pets.push_back(p.toRecord());
  if (p.home(1).hatched()) w.pets.push_back(p.home(1).record(p.lastSeenEpoch));  // v3.0: el del hueco 2 ("hm1")
  for (uint8_t k = 0; k < BOX_SLOTS; k++) {
    if (p.boxState(k) == BOX_OK) w.pets.push_back(p.boxRecord(k));
    else if (p.boxState(k) == BOX_DAMAGED) w.damaged++;
  }
  return w;
}

// cada bicho de want esta exactamente una vez, igual que era, y no hay ninguno mas
static bool samePets(const std::vector<PetRecord> &want, const World &got, std::string *why) {
  if (got.damaged) { *why = "un hueco quedo danado"; return false; }
  if (got.pets.size() != want.size()) {
    *why = "hay " + std::to_string(got.pets.size()) + " bichos, tenia que haber " + std::to_string(want.size());
    return false;
  }
  for (const PetRecord &w : want) {
    int n = 0;
    for (const PetRecord &g : got.pets) n += (g.uid == w.uid);
    if (n != 1) { *why = "el uid " + std::to_string(w.uid) + " esta " + std::to_string(n) + " veces"; return false; }
    for (const PetRecord &g : got.pets)
      if (g.uid == w.uid && !sameIndividual(g, w)) { *why = "el uid " + std::to_string(w.uid) + " cambio"; return false; }
  }
  return true;
}

// Corta la luz tras cada escritura del movimiento (0, 1, 2... todas), arranca otra vez y comprueba
// que ningun bicho se pierde ni se repite, y lo que pida el escenario (check). Un segundo arranque
// no cambia nada: el arreglo ya quedo hecho.
static void cutEveryWrite(const std::function<void(Pet &)> &move, const std::vector<PetRecord> &pets,
                          const std::function<void(Pet &, long)> &check) {
  mockNvsSnapshot();
  long total = 0;
  {
    Pet p;
    p.begin();
    long w0 = mockNvsWrites();
    move(p);
    total = mockNvsWrites() - w0;
  }
  CHECK_MSG(total > 3, "el movimiento escribe en la NVS");
  for (long n = 0; n <= total; n++) {
    mockNvsRestore();
    {
      Pet p;
      p.begin();
      mockNvsCutAfter(n);
      move(p);
      mockNvsCutAfter(-1);
    }
    Pet q;
    q.begin();
    std::string why;
    World w = worldOf(q);
    CHECK_MSG(samePets(pets, w, &why), "corte tras " + std::to_string(n) + " de " + std::to_string(total) + ": " + why);
    CHECK_MSG(!(q.isEgg() && q.eggHeld()), "corte tras " + std::to_string(n) + ": un huevo activo no espera a otro");
    CHECK_MSG(!(q.home(1).isEgg() && q.home(1).eggHeld()), "corte tras " + std::to_string(n) + ": un huevo del hueco 2 no espera a otro");
    check(q, n);
    Pet r;
    r.begin();
    World w2 = worldOf(r);
    CHECK_MSG(samePets(w.pets, w2, &why), "segundo arranque tras el corte " + std::to_string(n) + ": " + why);
    CHECK_EQ(r.uid, q.uid);
  }
}

// ---------------------------------------------------------------- el registro
TEST(box, el_crc_es_el_ccitt_false) {
  CHECK_EQ(crc16((const uint8_t *)"123456789", 9), (uint16_t)0x29B1);
}

TEST(box, el_registro_cabe_en_64_bytes_y_se_sella) {
  CHECK_EQ(sizeof(PetRecord), (size_t)55);
  PetRecord r;
  memset(&r, 0, sizeof(r));
  r.uid = 0x1234;
  r.speciesId = 25;
  recordSeal(r);
  CHECK(recordValid(r));
  CHECK_EQ(r.ver, (uint8_t)PET_RECORD_VER);
  for (size_t i = 0; i < sizeof(r); i++) {  // cualquier byte cambiado lo delata
    PetRecord x = r;
    ((uint8_t *)&x)[i] ^= 0x40;
    CHECK_MSG(!recordValid(x), "byte " + std::to_string(i));
  }
  PetRecord v = r;  // otra version, aunque su CRC cuadre
  v.ver = PET_RECORD_VER + 1;
  v.crc = recordCrc(v);
  CHECK(!recordValid(v));
  PetRecord z = r;  // sin uid, o sin especie, no es un bicho
  z.uid = 0;
  recordSeal(z);
  CHECK(!recordValid(z));
  z = r;
  z.speciesId = -1;
  recordSeal(z);
  CHECK(!recordValid(z));
}

TEST(box, el_registro_conserva_todo_el_individuo) {
  Pet p;
  freshPet(p, 25);
  p.setClock(DAY0 + 3600);
  p.caress();  // primer cuidado del dia: bondToday y bondDay de hoy
  dress(p, 3);
  p.sleeping = true;
  PetRecord r = p.toRecord();
  CHECK(recordValid(r));
  Pet q;
  q.fromRecord(r);
  CHECK(sameIndividual(q.toRecord(), r));
  CHECK_EQ(q.uid, p.uid);
  CHECK_EQ(q.speciesId, (int16_t)25);
  CHECK_EQ(q.shiny, p.shiny);
  CHECK(strcmp(q.nick, "PET3") == 0);
  CHECK_EQ(q.ageMinutes, p.ageMinutes);
  CHECK_EQ(q.growMin, p.growMin);
  CHECK_EQ(q.level(), p.level());
  CHECK_EQ(q.levelMinutesInto(), p.levelMinutesInto());
  CHECK_EQ(q.fullness, p.fullness);
  CHECK_EQ(q.joy, p.joy);
  CHECK_EQ(q.energy, p.energy);
  CHECK_EQ(q.poops, p.poops);
  CHECK_EQ(q.weight, p.weight);
  CHECK_EQ(q.geneAtk, p.geneAtk);
  CHECK_EQ(q.geneDef, p.geneDef);
  CHECK_EQ(q.geneSpe, p.geneSpe);
  CHECK_EQ(q.trAtk, p.trAtk);
  CHECK_EQ(q.trDef, p.trDef);
  CHECK_EQ(q.trSpe, p.trSpe);
  CHECK_EQ(q.berryKnown, p.berryKnown);
  CHECK_EQ(q.careMistakes, p.careMistakes);
  CHECK_EQ(q.sleeping, true);
  CHECK_EQ(q.bond, p.bond);
  CHECK_EQ(q.bondGainedToday(), p.bondGainedToday());
  CHECK_EQ(q.counted, p.counted);
  CHECK_EQ(q.atkStat(), p.atkStat());
}

// ---------------------------------------------------------------- guardar y despertar
TEST(box, guardar_deja_un_huevo_y_no_es_un_final) {
  Pet p;
  freshPet(p, 4);
  p.lastEnd = CER_RUNAWAY;  // la ultima crianza acabo mal: guardar no lo cambia
  uint32_t a = p.uid;
  CHECK(a != 0);
  CHECK(p.canStore(0));
  CHECK(p.store(0));
  CHECK(p.isEgg());
  CHECK_EQ(p.uid, (uint32_t)0);
  CHECK(!p.awaitingStarter());
  CHECK_EQ(p.lastEnd, (uint8_t)CER_RUNAWAY);
  CHECK_EQ(p.boxState(0), (uint8_t)BOX_OK);
  CHECK_EQ(p.boxRecord(0).uid, a);
  CHECK_EQ(p.boxRecord(0).speciesId, (int16_t)4);
  CHECK_EQ(p.boxUsed(), (uint8_t)1);
  Pet q;  // se queda guardado
  q.begin();
  CHECK(q.isEgg());
  CHECK_EQ(q.boxState(0), (uint8_t)BOX_OK);
  CHECK_EQ(q.boxRecord(0).uid, a);
}

TEST(box, despertar_intercambia_en_el_mismo_hueco) {
  Pet p;
  freshPet(p, 1);
  dress(p, 1);
  PetRecord b = p.toRecord();
  CHECK(p.store(2));
  hatchNow(p);
  dress(p, 2);
  PetRecord a = p.toRecord();
  CHECK(a.uid != b.uid);
  CHECK(p.wake(2));
  CHECK(sameIndividual(p.toRecord(), b));
  CHECK_EQ(p.boxState(2), (uint8_t)BOX_OK);
  CHECK(sameIndividual(p.boxRecord(2), a));
  CHECK_EQ(p.boxUsed(), (uint8_t)1);
  Pet q;
  q.begin();
  CHECK(sameIndividual(q.toRecord(), b));
  CHECK(sameIndividual(q.boxRecord(2), a));
}

TEST(box, el_nivel_y_su_barra_vuelven_igual) {
  Pet p;
  freshPet(p, 4);
  p.growMin = levelStart(14) + 25;  // Lv.14 y 25 minutos de su barra
  p.rename("");
  CHECK(p.store(0));
  hatchNow(p);
  CHECK(p.wake(0));
  CHECK_EQ(p.level(), (uint16_t)14);
  CHECK_EQ(p.levelMinutesInto(), (uint16_t)25);
}

TEST(box, dormido_se_guarda_y_vuelve_dormido) {
  Pet p;
  freshPet(p, 7);
  p.toggleLight();
  CHECK(p.sleeping);
  CHECK(p.store(0));
  CHECK(!p.sleeping);  // el huevo no duerme
  CHECK(p.wake(0));
  CHECK(p.sleeping);
}

// v3.19: la caja distingue dormir de noche (REC_NIGHT) de una siesta. Antes todo bicho dormido salia de la caja
// como "de noche" y, despertado de dia (WAKE_HOUR..EVENING_HOUR), se despertaba solo al minuto siguiente

// minutos en vivo con el reloj andando (como liveMinutes de test_pet.cpp)
static void boxLiveMinutes(Pet &p, uint32_t minutes) {
  for (uint32_t i = 0; i < minutes; i++) {
    p.lastSeenEpoch += 60;
    mockAdvanceMillis(PET_TICK_MS);
    p.update(millis());
  }
}

TEST(box, una_siesta_guardada_sigue_siendo_siesta_al_despertarla_de_dia) {
  const int napH = EVENING_HOUR - 6;  // una siesta de tarde (14:00)
  CHECK(napH > WAKE_HOUR);
  Pet p;
  freshPet(p, 7);
  p.lastSeenEpoch = DAY0 + napH * 3600u;
  p.toggleLight();
  CHECK(p.sleeping);
  CHECK(!p.nightSlept());
  CHECK(p.store(0));
  hatchNow(p);
  p.lastSeenEpoch = DAY0 + (napH + 1) * 3600u;  // una hora despues, de dia
  CHECK(p.wake(0));
  CHECK(p.sleeping);
  CHECK_MSG(!p.nightSlept(), "una siesta sale de la caja como siesta");
  boxLiveMinutes(p, (EVENING_HOUR - napH - 1) * 60 - 1);  // hasta las EVENING_HOUR - 1:59
  CHECK_MSG(p.sleeping, "una siesta no se despierta sola de dia");
  CHECK(!p.morningWake);
}

TEST(box, dormido_de_noche_guardado_amanece_al_despertarlo_de_dia) {
  Pet p;
  freshPet(p, 7);
  p.lastSeenEpoch = DAY0 + BED_HOUR * 3600u;  // acostado a las BED_HOUR (23:00)
  p.toggleLight();
  CHECK(p.nightSlept());
  CHECK(p.store(0));
  CHECK((p.boxRecord(0).flags & REC_NIGHT) != 0);
  hatchNow(p);
  p.lastSeenEpoch = DAY0 + 86400u + (WAKE_HOUR + 1) * 3600u;  // a la mañana siguiente
  CHECK(p.wake(0));
  CHECK(p.sleeping);
  CHECK(p.nightSlept());
  boxLiveMinutes(p, 1);
  CHECK_MSG(!p.sleeping, "dormia de noche: de dia se despierta solo al minuto siguiente");
}

TEST(box, un_registro_dormido_sin_REC_NIGHT_carga_como_siesta) {
  Pet p;
  freshPet(p, 7);
  p.sleeping = true;
  PetRecord r = p.toRecord();
  CHECK((r.flags & REC_SLEEP) != 0);
  r.flags &= (uint8_t)~REC_NIGHT;  // un registro de antes de v3.19
  recordSeal(r);
  CHECK(recordValid(r));
  Pet q;
  q.fromRecord(r);
  CHECK(q.sleeping);
  CHECK(!q.nightSlept());
  r.flags |= REC_NIGHT;  // y con el bit, de noche
  recordSeal(r);
  q.fromRecord(r);
  CHECK(q.nightSlept());
  r.flags &= (uint8_t)~REC_SLEEP;  // REC_NIGHT sin REC_SLEEP no lo duerme
  recordSeal(r);
  q.fromRecord(r);
  CHECK(!q.sleeping);
  CHECK(!q.nightSlept());
}

TEST(box, un_huevo_no_se_guarda_ni_durante_evolucion_o_despedida) {
  Pet p;
  freshPet(p, 4);
  CHECK(p.store(0));
  CHECK(!p.canStore(1));  // es un huevo
  CHECK(!p.store(1));
  CHECK(p.canWake(0));  // despertar con un huevo, si
  hatchNow(p);
  // evolucionando (Charmander, si el huevo era otro: se fuerza la especie)
  p.speciesId = 4;
  p.setLevel(16);
  p.fullness = p.joy = p.energy = 90;
  p.careMistakes = 0;
  CHECK(p.canEvolveNow());
  p.evolve();
  CHECK(p.evolving());
  CHECK(!p.canStore(1));
  CHECK(!p.store(1));
  CHECK(!p.wake(0));
  mockAdvanceMillis(EVOLVE_ANIM_MS + 1);
  CHECK(p.canStore(1));
  // despidiendose
  p.startFarewell();
  CHECK(!p.store(1));
  CHECK(!p.wake(0));
}

TEST(box, el_huevo_que_espera_vuelve_con_sus_grietas) {
  Pet p;
  freshPet(p, 7);
  CHECK(p.store(0));
  p.eggTap();  // una grieta
  int16_t eggT = p.eggSpecies();
  bool eggS = p.eggIsShiny();
  CHECK(p.wake(0));
  CHECK(!p.isEgg());
  CHECK_EQ(p.speciesId, (int16_t)7);
  CHECK(p.eggHeld());
  CHECK_EQ(p.boxState(0), (uint8_t)BOX_EMPTY);  // su hueco queda libre
  Pet r;  // tambien tras un reinicio
  r.begin();
  CHECK(r.eggHeld());
  CHECK(r.store(3));
  CHECK(r.isEgg());
  CHECK(!r.eggHeld());
  CHECK_EQ(r.eggSpecies(), eggT);
  CHECK_EQ(r.eggIsShiny(), eggS);
  CHECK_EQ(r.eggCracks(), (uint8_t)1);
}

TEST(box, el_amuleto_no_se_gasta_dos_veces) {
  Pet p;
  freshPet(p, 7);
  p.items[IT_CHARM] = 1;
  CHECK(p.store(0));  // huevo nuevo: el amuleto se gasta en el
  CHECK_EQ(p.items[IT_CHARM], (uint8_t)0);
  CHECK(p.wake(0));
  p.items[IT_CHARM] = 1;  // encuentra otro
  CHECK(p.store(1));  // vuelve el huevo de antes: no se tira otra vez ni gasta el nuevo
  CHECK_EQ(p.items[IT_CHARM], (uint8_t)1);
}

TEST(box, guardar_y_despertar_20_veces_no_vuelve_a_tirar_el_huevo) {
  Pet p;
  freshPet(p, 1);
  CHECK(p.store(0));
  int16_t eggT = p.eggSpecies();
  bool eggS = p.eggIsShiny();
  for (int i = 0; i < 20; i++) {
    CHECK(p.wake(0));
    CHECK_EQ(p.eggSpecies(), eggT);
    CHECK(p.store(0));
    CHECK_EQ(p.eggSpecies(), eggT);
    CHECK_EQ(p.eggIsShiny(), eggS);
  }
}

TEST(box, un_final_devuelve_el_huevo_que_esperaba) {
  Pet p;
  freshPet(p, 4);
  CHECK(p.store(0));
  int16_t eggT = p.eggSpecies();
  CHECK(p.wake(0));
  p.startFarewell();
  mockAdvanceMillis(CEREMONY_MS + 1);
  p.update(millis());
  CHECK(p.isEgg());
  CHECK(!p.eggHeld());
  CHECK_EQ(p.eggSpecies(), eggT);
  CHECK_EQ(p.lastEnd, (uint8_t)CER_FAREWELL);
}

TEST(box, con_la_caja_llena_no_se_guarda_pero_se_intercambia) {
  Pet p;
  freshPet(p, 25);
  for (uint8_t k = 0; k < BOX_SLOTS; k++) {
    PetRecord r = p.toRecord();
    r.speciesId = 10 + k;
    CHECK(p.boxPut(k, r));
  }
  CHECK_EQ(p.boxUsed(), (uint8_t)BOX_SLOTS);
  for (uint8_t k = 0; k < BOX_SLOTS; k++) CHECK(!p.canStore(k));
  uint32_t a = p.uid;
  CHECK(p.wake(3));
  CHECK_EQ(p.speciesId, (int16_t)13);
  CHECK_EQ(p.boxRecord(3).uid, a);
  CHECK_EQ(p.boxUsed(), (uint8_t)BOX_SLOTS);
}

TEST(box, boxfill_da_un_uid_nuevo_y_solo_en_huecos_vacios) {
  Pet p;
  freshPet(p, 25);
  PetRecord r = p.toRecord();
  CHECK(p.boxPut(0, r));
  CHECK(p.boxRecord(0).uid != 0);
  CHECK(p.boxRecord(0).uid != p.uid);
  CHECK(p.boxPut(1, r));
  CHECK(p.boxRecord(1).uid != p.boxRecord(0).uid);
  CHECK(!p.boxPut(1, r));  // ocupado
  r.speciesId = 0;
  CHECK(!p.boxPut(2, r));  // no es un bicho
  CHECK(!p.boxPut(BOX_SLOTS, p.toRecord()));
}

// ---------------------------------------------------------------- hibernacion y jugador
TEST(box, la_caja_hiberna_durante_el_tiempo_apagado) {
  Pet p;
  freshPet(p, 1);
  p.setClock(DAY0);
  dress(p, 4);
  PetRecord b = p.toRecord();
  CHECK(p.store(0));
  hatchNow(p);
  uint32_t ageA = p.ageMinutes;
  Pet q;  // dos dias apagada, como en setup()
  q.begin();
  q.syncClock(DAY0 + 2 * 86400);
  CHECK_EQ(q.ageMinutes, ageA + 2 * 1440);  // el activo envejece
  CHECK(sameIndividual(q.boxRecord(0), b));  // el de la caja, no
  CHECK(q.wake(0));
  CHECK_EQ(q.ageMinutes, b.ageMinutes);
  CHECK_EQ(q.growMin, b.growMin);
  CHECK_EQ(q.fullness, b.fullness);
  CHECK_EQ(q.careMistakes, b.careMistakes);
}

// v3.0: el primer cuidado del dia da su vinculo a CADA bicho (la racha cuenta una vez, del jugador), y
// guardarlo y despertarlo el mismo dia no se lo da otra vez (REC_CARED)
TEST(box, el_primer_cuidado_del_dia_da_vinculo_a_cada_bicho_una_vez) {
  Pet p;
  freshPet(p, 1);
  p.setClock(DAY0 + 10 * 3600);
  CHECK(p.store(0));
  hatchNow(p);  // A activo, B en la caja (sin cuidar hoy)
  uint8_t a0 = p.bond;
  uint16_t st0 = p.streak;
  p.caress();  // primer cuidado del dia: +4, y +1 del mimo
  CHECK_EQ(p.bond, (uint8_t)(a0 + BOND_CARE_DAY + 1));
  CHECK_EQ(p.streak, (uint16_t)(st0 + 1));
  CHECK(p.wake(0));  // B: su primer cuidado de hoy aun no lo tuvo
  uint8_t b0 = p.bond;
  p.caress();
  CHECK_EQ(p.bond, (uint8_t)(b0 + BOND_CARE_DAY + 1));
  CHECK_EQ(p.streak, (uint16_t)(st0 + 1));  // la racha, una vez
  CHECK(p.wake(0));  // A otra vez: hoy ya lo tuvo, solo el mimo
  uint8_t a1 = p.bond;
  mockAdvanceMillis(PET_BOND_COOLDOWN_MS + 1);
  p.caress();
  CHECK_EQ(p.bond, (uint8_t)(a1 + 1));
  p.setClock(DAY0 + 34 * 3600);  // al dia siguiente, otra vez el suyo
  mockAdvanceMillis(PET_BOND_COOLDOWN_MS + 1);
  p.caress();
  CHECK_EQ(p.bond, (uint8_t)(a1 + 1 + BOND_CARE_DAY + 1));
}

TEST(box, el_tope_diario_de_vinculo_es_de_cada_bicho) {
  Pet p;
  freshPet(p, 1);
  p.setClock(DAY0 + 10 * 3600);
  CHECK(p.store(0));
  hatchNow(p);  // A
  for (int i = 0; i < 6; i++) p.play();  // +2 cada vez hasta el tope (10)
  CHECK_EQ(p.bondGainedToday(), (uint8_t)BOND_DAY_CAP);
  uint8_t a1 = p.bond;
  CHECK(p.wake(0));  // B no ha ganado nada hoy
  CHECK_EQ(p.bondGainedToday(), (uint8_t)0);
  uint8_t b0 = p.bond;
  p.play();  // v3.0: tambien su primer cuidado del dia (fuera del tope)
  CHECK_EQ(p.bond, (uint8_t)(b0 + BOND_CARE_DAY + 2));
  CHECK(p.wake(0));  // otra vez A: sigue en su tope
  CHECK_EQ(p.bond, a1);
  CHECK_EQ(p.bondGainedToday(), (uint8_t)BOND_DAY_CAP);
  p.play();
  CHECK_EQ(p.bond, a1);
}

TEST(box, un_intercambio_no_toca_nada_del_jugador) {
  Pet p;
  freshPet(p, 1);
  p.setClock(DAY0 + 3600);
  CHECK(p.store(0));
  hatchNow(p);
  p.streak = 12;
  p.bestStreak = 20;
  p.lastCareDay = 19999;
  p.gameHi = 31;
  p.strHi = 44;
  p.items[IT_GOLDEN] = 3;
  p.items[IT_RARECANDY] = 2;
  p.lastEnd = CER_FAREWELL;
  p.addSteps(4321, p.dayNumber());  // pasa la meta de 3000: la racha y una medalla suben
  p.rename("A");
  uint16_t streak = p.streak, best = p.bestStreak;
  uint8_t medTiers[MEDAL_COUNT];
  for (uint8_t i = 0; i < MEDAL_COUNT; i++) medTiers[i] = p.medalTierPaid(i);
  uint32_t careDay = p.lastCareDay;
  uint8_t dex[19], dexS[19];
  memcpy(dex, p.dexReg, sizeof(dex));
  memcpy(dexS, p.dexShinyReg, sizeof(dexS));
  uint32_t steps = p.stepsToday, total = p.stepsTotal, goalD = p.goalDay;
  uint8_t tier = p.goalTier, finds = p.findsToday;
  uint8_t items[IT_COUNT];
  memcpy(items, p.items, sizeof(items));
  CHECK(p.wake(0));
  Pet q;  // tambien en la NVS
  q.begin();
  for (Pet *x : { &p, &q }) {
    CHECK_EQ(x->streak, streak);
    CHECK_EQ(x->bestStreak, best);
    CHECK_EQ(x->lastCareDay, careDay);
    for (uint8_t i = 0; i < MEDAL_COUNT; i++) CHECK_EQ(x->medalTierPaid(i), medTiers[i]);  // v3.4: del jugador
    CHECK_EQ(x->gameHi, (uint16_t)31);
    CHECK_EQ(x->strHi, (uint16_t)44);
    CHECK_EQ(x->lastEnd, (uint8_t)CER_FAREWELL);
    CHECK_EQ(x->stepsToday, steps);
    CHECK_EQ(x->stepsTotal, total);
    CHECK_EQ(x->goalDay, goalD);
    CHECK_EQ(x->goalTier, tier);
    CHECK_EQ(x->findsToday, finds);
    CHECK(memcmp(x->items, items, sizeof(items)) == 0);
    CHECK(memcmp(x->dexReg, dex, sizeof(dex)) == 0);
    CHECK(memcmp(x->dexShinyReg, dexS, sizeof(dexS)) == 0);
  }
}

// v3.4: lo que un bicho ya conto para las medallas del jugador va con el (y no vuelve a contar)
TEST(box, lo_contado_para_las_medallas_es_del_individuo) {
  Pet p;
  freshPet(p, 1);
  p.setLevel(10);
  advance(p, 1);  // el tick lo cuenta: Nv.10
  CHECK(p.counted & PMB_LV10);
  uint32_t n = p.medalValue(MD_LV10);
  CHECK(p.store(0));
  CHECK_EQ(p.boxRecord(0).medals & PMB_LV10, (int)PMB_LV10);
  CHECK(p.boxRecord(0).flags & REC_V3);
  hatchNow(p);  // otro bicho: sin nada contado, el total del jugador sigue
  CHECK(!(p.counted & PMB_LV10));
  CHECK_EQ(p.medalValue(MD_LV10), n);
  CHECK(p.wake(0));
  CHECK(p.counted & PMB_LV10);
  CHECK_EQ(p.medalValue(MD_LV10), n);  // volver no lo cuenta otra vez
}

// ---------------------------------------------------------------- huecos danados y guardados viejos
TEST(box, un_hueco_danado_no_se_carga_ni_se_pisa) {
  Pet p;
  freshPet(p, 25);
  CHECK(p.store(0));
  std::vector<uint8_t> bad(sizeof(PetRecord));
  {
    Preferences raw;
    raw.begin("tamapoke");
    CHECK_EQ(raw.getBytes("box0", bad.data(), bad.size()), sizeof(PetRecord));
    bad[10] ^= 0x01;  // un bit cambiado en el apodo
    raw.putBytes("box0", bad.data(), bad.size());
    PetRecord v2 = p.boxRecord(0);  // y una version que esta placa no conoce
    v2.ver = PET_RECORD_VER + 1;
    v2.crc = recordCrc(v2);
    raw.putBytes("box1", &v2, sizeof(v2));
    raw.putBytes("box2", "corto", 5);  // y un tamano que no es
  }
  Pet q;
  q.begin();
  hatchNow(q);
  for (uint8_t k = 0; k < 3; k++) {
    CHECK_EQ(q.boxState(k), (uint8_t)BOX_DAMAGED);
    CHECK(!q.canStore(k));
    CHECK(!q.store(k));
    CHECK(!q.canWake(k));
    CHECK(!q.wake(k));
  }
  CHECK_EQ(q.boxUsed(), (uint8_t)3);
  {
    Preferences raw;
    raw.begin("tamapoke");
    std::vector<uint8_t> now(sizeof(PetRecord));
    raw.getBytes("box0", now.data(), now.size());
    CHECK(now == bad);  // intacto
  }
  CHECK(q.boxClear(0));  // BOXCLR
  CHECK_EQ(q.boxState(0), (uint8_t)BOX_EMPTY);
  CHECK(q.store(0));
}

TEST(box, un_guardado_anterior_carga_con_la_caja_vacia_y_recibe_uid) {
  Pet p;
  freshPet(p, 25);
  dress(p, 5);
  {
    Preferences raw;  // como lo dejaba la v1.36: sin uid, sin eggH, sin caja
    raw.begin("tamapoke");
    raw.remove("uid");
    raw.remove("eggH");
  }
  Pet q;
  q.begin();
  CHECK(q.uid != 0);
  CHECK_EQ(q.boxUsed(), (uint8_t)0);
  CHECK(!q.eggHeld());
  CHECK_EQ(q.speciesId, (int16_t)25);
  CHECK(strcmp(q.nick, "PET5") == 0);
  Pet r;  // y la guarda
  r.begin();
  CHECK_EQ(r.uid, q.uid);
}

TEST(box, el_uid_se_sortea_al_nacer_y_no_cambia) {
  Pet p;
  mockNvsReset();
  p.begin();
  CHECK(p.isEgg());
  CHECK_EQ(p.uid, (uint32_t)0);
  p.chooseStarter(4);
  hatchNow(p);
  uint32_t a = p.uid;
  CHECK(a != 0);
  advance(p, 30);
  CHECK_EQ(p.uid, a);
  Pet q;
  q.begin();
  CHECK_EQ(q.uid, a);
}

// ---------------------------------------------------------------- cortes de luz (FR-BOX.9)
TEST(box, corte_de_luz_en_cada_escritura_de_guardar) {
  Pet p;
  freshPet(p, 1);
  dress(p, 1);
  CHECK(p.store(1));  // un tercero que mira: B en el hueco 1
  hatchNow(p);
  dress(p, 2);  // A activo
  std::vector<PetRecord> pets = { p.toRecord(), p.boxRecord(1) };
  cutEveryWrite([](Pet &x) { x.store(3); }, pets, [](Pet &q, long n) {
    CHECK_MSG(!q.eggHeld(), "corte " + std::to_string(n) + ": no habia huevo esperando");
  });
}

TEST(box, corte_de_luz_en_cada_escritura_de_guardar_con_huevo_que_espera) {
  Pet p;
  freshPet(p, 4);
  dress(p, 3);
  CHECK(p.store(0));
  p.eggTap();  // el huevo, con una grieta...
  int16_t eggT = p.eggSpecies();
  bool eggS = p.eggIsShiny();
  CHECK(p.wake(0));  // ...espera mientras A esta despierto
  CHECK(p.eggHeld());
  std::vector<PetRecord> pets = { p.toRecord() };
  cutEveryWrite([](Pet &x) { x.store(2); }, pets, [&](Pet &q, long n) {
    std::string at = "corte " + std::to_string(n) + ": ";
    // el huevo nunca se pierde ni se vuelve a tirar: o es el activo, o sigue esperando
    CHECK_MSG(q.isEgg() || q.eggHeld(), at + "el huevo que esperaba se perdio");
    CHECK_MSG(q.eggSpecies() == eggT && q.eggIsShiny() == eggS, at + "el huevo cambio");
    CHECK_MSG(q.eggCracks() == 1, at + "el huevo perdio su grieta");
  });
}

TEST(box, corte_de_luz_en_cada_escritura_de_un_intercambio) {
  Pet p;
  freshPet(p, 1);
  dress(p, 1);
  CHECK(p.store(2));  // B en el 2
  hatchNow(p);
  dress(p, 2);
  CHECK(p.store(4));  // C en el 4 (mira)
  hatchNow(p);
  dress(p, 3);  // A activo
  PetRecord a = p.toRecord(), b = p.boxRecord(2);
  std::vector<PetRecord> pets = { a, b, p.boxRecord(4) };
  cutEveryWrite([](Pet &x) { x.wake(2); }, pets, [&](Pet &q, long n) {
    std::string at = "corte " + std::to_string(n) + ": ";
    // o no paso nada, o el cambio esta hecho, siempre en el hueco 2
    bool before = q.uid == a.uid && q.boxRecord(2).uid == b.uid;
    bool after = q.uid == b.uid && q.boxRecord(2).uid == a.uid;
    CHECK_MSG(before || after, at + "ni antes ni despues del cambio");
  });
}

TEST(box, corte_de_luz_en_cada_escritura_de_despertar_con_un_huevo) {
  Pet p;
  freshPet(p, 7);
  dress(p, 4);
  PetRecord b = p.toRecord();
  CHECK(p.store(1));
  p.eggTap();
  p.eggTap();  // el huevo activo, con dos grietas
  int16_t eggT = p.eggSpecies();
  bool eggS = p.eggIsShiny();
  std::vector<PetRecord> pets = { b };
  cutEveryWrite([](Pet &x) { x.wake(1); }, pets, [&](Pet &q, long n) {
    std::string at = "corte " + std::to_string(n) + ": ";
    CHECK_MSG(q.isEgg() || q.uid == b.uid, at + "el activo no es ni el huevo ni B");
    CHECK_MSG(!q.isVacant(), at + "el huevo se perdio: la casa quedo vacia (v1.39)");
    CHECK_MSG(q.isEgg() || q.eggHeld(), at + "B despierto pero el huevo no espera");
    CHECK_MSG(q.eggSpecies() == eggT && q.eggIsShiny() == eggS, at + "el huevo cambio");
    CHECK_MSG(q.eggCracks() == 2, at + "el huevo perdio sus grietas");
  });
}

// ---------------------------------------------------------------- cortes de luz con huevos limitados (v1.39)
TEST(box, corte_de_luz_en_cada_escritura_de_guardar_dejando_la_casa_vacia) {
  Pet p;
  freshPet(p, 1);
  dress(p, 1);
  CHECK(p.store(1));  // B en el 1 (mira)
  hatchNow(p);
  p.items[IT_EGG] = 0;  // sin huevos: guardar deja la casa vacia
  dress(p, 2);          // A activo (dress guarda, mochila incluida)
  uint32_t a = p.uid;
  std::vector<PetRecord> pets = { p.toRecord(), p.boxRecord(1) };
  cutEveryWrite([](Pet &x) { x.store(3); }, pets, [&](Pet &q, long n) {
    std::string at = "corte " + std::to_string(n) + ": ";
    CHECK_MSG(q.uid == a || q.isVacant(), at + "ni A en casa ni la casa vacia");
    CHECK_MSG(!(q.isEgg() && !q.isVacant()), at + "aparecio un huevo");
    CHECK_MSG(q.items[IT_EGG] == 0, at + "aparecio un huevo en la mochila");
  });
}

TEST(box, corte_de_luz_en_cada_escritura_de_guardar_con_un_huevo_de_la_mochila) {
  Pet p;
  freshPet(p, 4);
  p.items[IT_EGG] = 2;
  dress(p, 3);  // A activo, dos huevos en la mochila
  uint32_t a = p.uid;
  std::vector<PetRecord> pets = { p.toRecord() };
  int extra = 0;
  cutEveryWrite([](Pet &x) { x.store(0); }, pets, [&](Pet &q, long n) {
    std::string at = "corte " + std::to_string(n) + ": ";
    CHECK_MSG(!q.isVacant(), at + "la casa vacia con huevos en la mochila");
    if (q.uid == a) {  // no llego a guardarse: la mochila, intacta
      CHECK_MSG(q.items[IT_EGG] == 2, at + "A en casa y la mochila cambio");
      return;
    }
    CHECK_MSG(q.isEgg(), at + "ni A ni un huevo en casa");
    int eggs = q.items[IT_EGG] + 1;  // los de la mochila y el de casa
    CHECK_MSG(eggs == 2 || eggs == 3, at + "huevos: " + std::to_string(eggs));
    extra += (eggs == 3);
  });
  CHECK_EQ(extra, 1);  // solo el corte entre el uid y la mochila: uno de mas, nunca uno perdido
}

TEST(box, corte_de_luz_en_cada_escritura_de_despertar_con_la_casa_vacia) {
  Pet p;
  freshPet(p, 7);
  p.items[IT_EGG] = 0;
  dress(p, 4);
  PetRecord b = p.toRecord();
  CHECK(p.store(1));
  CHECK(p.isVacant());
  std::vector<PetRecord> pets = { b };
  cutEveryWrite([](Pet &x) { x.wake(1); }, pets, [&](Pet &q, long n) {
    std::string at = "corte " + std::to_string(n) + ": ";
    bool before = q.isVacant() && q.boxState(1) == BOX_OK && q.boxRecord(1).uid == b.uid;
    bool after = q.uid == b.uid && !q.isVacant() && q.boxState(1) == BOX_EMPTY;
    CHECK_MSG(before || after, at + "ni antes ni despues del despertar");
    CHECK_MSG(!(q.isEgg() && !q.isVacant()), at + "aparecio un huevo");
    CHECK_MSG(!q.eggHeld(), at + "un huevo espera sin haberlo");
  });
}

// ---------------------------------------------------------------- el hueco 2 de casa (v3.0)
// B en el hueco 0 de casa, A en el 1 (despertado desde la caja), la caja vacia
static void twoHome(Pet &p) {
  freshPet(p, 1);
  dress(p, 1);  // A
  CHECK(p.store(0, 0));
  hatchNow(p);
  dress(p, 2);  // B en el 0
  CHECK(p.wake(0, 1));  // A al hueco 1 (vacio)
  CHECK(p.home(1).hatched());
  CHECK_EQ(p.boxUsed(), (uint8_t)0);
}

TEST(box, guardar_y_despertar_con_el_hueco_2) {
  Pet p;
  twoHome(p);
  PetRecord a = p.home(1).record(0), b = p.toRecord();
  CHECK(p.store(3, 1));
  CHECK_EQ(p.focusSlot, (uint8_t)0);
  CHECK(p.home(1).isEgg());  // un huevo de la mochila
  CHECK(sameIndividual(p.boxRecord(3), a));
  CHECK(sameIndividual(p.toRecord(), b));  // el del 0, ni se entero
  Pet q;
  q.begin();
  CHECK(q.home(1).isEgg());
  CHECK(sameIndividual(q.boxRecord(3), a));
  CHECK(q.wake(3, 1));  // vuelve al 1: el huevo espera en su incubadora
  CHECK(sameIndividual(q.home(1).record(0), a));
  CHECK(q.home(1).eggHeld());
  CHECK(!q.eggHeld());  // la del 0, no
  CHECK_EQ(q.boxState(3), (uint8_t)BOX_EMPTY);
  Pet r;
  r.begin();
  CHECK(sameIndividual(r.home(1).record(0), a));
  CHECK(r.home(1).eggHeld());
  CHECK(!r.home(1).isVacant());
}

TEST(box, corte_de_luz_en_cada_escritura_de_guardar_el_del_hueco_2) {
  Pet p;
  twoHome(p);
  uint32_t a = p.home(1).uid;
  std::vector<PetRecord> pets = { p.toRecord(), p.home(1).record(0) };
  cutEveryWrite([](Pet &x) { x.store(4, 1); }, pets, [&](Pet &q, long n) {
    std::string at = "corte " + std::to_string(n) + ": ";
    CHECK_MSG(q.home(1).uid == a || q.home(1).isEgg(), at + "ni A ni un huevo en el hueco 2");
    CHECK_MSG(!q.home(1).isVacant(), at + "el hueco 2 vacio con huevos en la mochila");
  });
}

TEST(box, corte_de_luz_en_cada_escritura_de_un_intercambio_con_el_hueco_2) {
  Pet p;
  twoHome(p);
  CHECK(p.store(2, 0));  // B a la caja (2)
  hatchNow(p);
  dress(p, 3);  // C en el 0
  PetRecord a = p.home(1).record(0), b = p.boxRecord(2);
  std::vector<PetRecord> pets = { p.toRecord(), a, b };
  cutEveryWrite([](Pet &x) { x.wake(2, 1); }, pets, [&](Pet &q, long n) {
    std::string at = "corte " + std::to_string(n) + ": ";
    bool before = q.home(1).uid == a.uid && q.boxRecord(2).uid == b.uid;
    bool after = q.home(1).uid == b.uid && q.boxRecord(2).uid == a.uid;
    CHECK_MSG(before || after, at + "ni antes ni despues del cambio en el hueco 2");
    Preferences raw;
    raw.begin("tamapoke");
    CHECK_MSG(!raw.isKey("boxT") && !raw.isKey("boxTs"), at + "quedo la copia de transferencia");
  });
}

TEST(box, corte_de_luz_en_cada_escritura_de_despertar_al_hueco_2_con_un_huevo) {
  Pet p;
  twoHome(p);
  CHECK(p.store(1, 1));  // A a la caja (1); en el hueco 2, un huevo de la mochila
  p.onSlot(1, [&] { p.eggTap(); });  // con una grieta
  int16_t eggT = p.home(1).eggSpecies();
  bool eggS = p.home(1).eggIsShiny();
  PetRecord a = p.boxRecord(1);
  std::vector<PetRecord> pets = { p.toRecord(), a };
  cutEveryWrite([](Pet &x) { x.wake(1, 1); }, pets, [&](Pet &q, long n) {
    std::string at = "corte " + std::to_string(n) + ": ";
    const PetLife &h = q.home(1);
    CHECK_MSG(h.isEgg() || h.uid == a.uid, at + "el hueco 2 no es ni el huevo ni A");
    CHECK_MSG(!h.isVacant(), at + "el huevo se perdio");
    CHECK_MSG(h.isEgg() || h.eggHeld(), at + "A despierto pero el huevo no espera");
    CHECK_MSG(h.eggSpecies() == eggT && h.eggIsShiny() == eggS, at + "el huevo cambio");
    CHECK_MSG(h.eggTaps == 1, at + "el huevo perdio su grieta");
    CHECK_MSG(!q.eggHeld(), at + "el huevo paso a la incubadora del hueco 1");
  });
}

TEST(box, corte_de_luz_en_cada_escritura_de_despertar_al_hueco_2_vacio) {
  Pet p;
  freshPet(p, 7);
  dress(p, 4);
  PetRecord a = p.toRecord();
  CHECK(p.store(1, 0));  // A a la caja; en el 0 un huevo
  hatchNow(p);
  dress(p, 5);  // B en el 0, el hueco 2 vacio
  CHECK(p.home(1).isVacant());
  std::vector<PetRecord> pets = { p.toRecord(), a };
  cutEveryWrite([](Pet &x) { x.wake(1, 1); }, pets, [&](Pet &q, long n) {
    std::string at = "corte " + std::to_string(n) + ": ";
    bool before = q.home(1).isVacant() && q.boxState(1) == BOX_OK && q.boxRecord(1).uid == a.uid;
    bool after = q.home(1).uid == a.uid && q.boxState(1) == BOX_EMPTY;
    CHECK_MSG(before || after, at + "ni antes ni despues del despertar al hueco 2");
    CHECK_MSG(!q.home(1).eggHeld(), at + "un huevo espera sin haberlo");
  });
}

// un corte justo entre "boxTs" y "boxT": el arranque no toca nada y borra la marca
TEST(box, un_corte_entre_boxTs_y_boxT_no_cambia_nada) {
  Pet p;
  twoHome(p);
  CHECK(p.store(2, 1));  // A a la caja (2); en el hueco 2 un huevo
  PetRecord a = p.boxRecord(2);
  int16_t eggT = p.home(1).eggSpecies();
  mockNvsSnapshot();
  {
    Pet x;
    x.begin();
    mockNvsCutAfter(1);  // solo llega "boxTs"
    x.wake(2, 1);
    mockNvsCutAfter(-1);
  }
  {
    Preferences raw;
    raw.begin("tamapoke");
    CHECK(raw.isKey("boxTs"));
    CHECK(!raw.isKey("boxT"));
  }
  Pet q;
  q.begin();
  CHECK(q.home(1).isEgg());
  CHECK_EQ(q.home(1).eggSpecies(), eggT);
  CHECK(!q.home(1).eggHeld());
  CHECK(sameIndividual(q.boxRecord(2), a));
  Preferences raw;
  raw.begin("tamapoke");
  CHECK(!raw.isKey("boxTs"));
}

// un despertar al hueco 0 despues de uno al 2: "boxTs" no queda y no confunde al siguiente arranque
TEST(box, un_despertar_al_hueco_1_no_deja_boxTs) {
  Pet p;
  twoHome(p);
  CHECK(p.store(2, 1));
  CHECK(p.wake(2, 1));
  CHECK(p.store(3, 0));
  CHECK(p.wake(3, 0));
  Preferences raw;
  raw.begin("tamapoke");
  CHECK(!raw.isKey("boxTs"));
  CHECK(!raw.isKey("boxT"));
}

// boxBoot: el mismo uid en la caja y en el hueco 2 (un guardar cortado): gana la copia de la caja
TEST(box, el_arranque_arregla_un_repetido_del_hueco_2) {
  Pet p;
  twoHome(p);
  PetRecord a = p.home(1).record(0);
  {
    Preferences raw;
    raw.begin("tamapoke");
    PetRecord r = a;
    r.joy = 11;  // la copia de la caja, distinta
    recordSeal(r);
    raw.putBytes("box5", &r, sizeof(r));
  }
  Pet q;
  q.begin();
  CHECK_EQ(q.home(1).uid, a.uid);
  CHECK_EQ(q.home(1).joy, (uint8_t)11);
  CHECK_EQ(q.boxState(5), (uint8_t)BOX_EMPTY);
}

// ---------------------------------------------------------------- finales con cortes de luz (v3.0)
// los huevos de alguien: la mochila, los que esperan en las incubadoras y los de casa
static int eggsOf(Pet &q) {
  int n = q.items[IT_EGG];
  for (uint8_t s = 0; s < HOME_SLOTS; s++) {
    const PetLife &l = q.home(s);
    n += l.eggHeldFlag ? 1 : 0;
    if (l.isEgg() && !l.isVacant()) n += 1;
  }
  return n;
}

// el final del bicho del hueco s (start lo empieza con el foco en el), cortando la luz tras cada
// escritura: ningun huevo se pierde (los de antes siguen) y, si el hueco ya quedo libre, el que trae el
// final (la despedida) tambien esta
// maxExtra: los huevos de mas que puede dejar un corte. El hueco 1 (claves sueltas) puede quedar a medias
// entre "dexn" y "uid" y cargar como un huevo (pasaba igual antes de la v3.0): uno de mas; "hm1" no
static void cutEveryWriteEnd(uint8_t s, const std::function<void(Pet &)> &start, int gained, int maxExtra = 1) {
  Pet p0;
  p0.begin();
  int eggs0 = eggsOf(p0);
  uint32_t uid0 = p0.home(s).uid;
  auto run = [&](Pet &p) {
    p.onSlot(s, [&] { start(p); });
    advance(p, 1);
    mockAdvanceMillis(CEREMONY_MS + 10);
    p.update(millis());
  };
  mockNvsSnapshot();
  long total = 0;
  {
    Pet p;
    p.begin();
    long w0 = mockNvsWrites();
    run(p);
    total = mockNvsWrites() - w0;
    CHECK(p.home(s).uid != uid0);
    CHECK_EQ(eggsOf(p), eggs0 + gained);
  }
  CHECK_MSG(total > 3, "el final escribe en la NVS");
  for (long n = 0; n <= total; n++) {
    mockNvsRestore();
    {
      Pet p;
      p.begin();
      mockNvsCutAfter(n);
      run(p);
      mockNvsCutAfter(-1);
    }
    Pet q;
    q.begin();
    std::string at = "corte tras " + std::to_string(n) + " de " + std::to_string(total) + ": ";
    bool gone = q.home(s).uid != uid0;
    int want = eggs0 + (gone ? gained : 0);
    CHECK_MSG(eggsOf(q) >= want, at + "se perdio un huevo (" + std::to_string(eggsOf(q)) + " de " + std::to_string(want) + ")");
    CHECK_MSG(eggsOf(q) <= want + maxExtra, at + "demasiados huevos de mas");
  }
}

TEST(box, corte_de_luz_al_soltar_al_del_hueco_2_con_su_huevo_esperando) {
  Pet p;
  twoHome(p);
  p.items[IT_EGG] = 0;
  CHECK(p.store(3, 1));  // A a la caja: el hueco 2 vacio (sin huevos)
  p.items[IT_EGG] = 1;
  p.onSlot(1, [&] { CHECK(p.hatchFromBag()); });  // un huevo en el hueco 2...
  CHECK(p.wake(3, 1));                               // ...que espera mientras A esta despierto
  CHECK(p.home(1).eggHeld());
  CHECK(p.boxClear(0));
  p.rename("B");
  cutEveryWriteEnd(1, [](Pet &x) { x.release(); }, 0);
}

TEST(box, corte_de_luz_al_morir_el_del_hueco_2_con_heredero_y_huevo_esperando) {
  Pet p;
  twoHome(p);
  p.items[IT_EGG] = 0;
  CHECK(p.store(3, 1));
  p.items[IT_EGG] = 1;
  p.onSlot(1, [&] { CHECK(p.hatchFromBag()); });
  CHECK(p.wake(3, 1));
  p.onSlot(1, [&] {
    p.setLevel(DEATH_EGG_LEVEL);
    mockForceRandom(0);  // deja huevo
    p.startDying();
    mockClearForcedRandom();
    CHECK(p.deathEggSpecies() > 0);
  });
  // el del heredero entra en casa y el que esperaba va a la mochila: uno mas en total
  cutEveryWriteEnd(1, [](Pet &x) { x.letGo(); }, 1);
}

TEST(box, corte_de_luz_en_la_despedida_del_hueco_1_y_del_2) {
  for (uint8_t s = 0; s < HOME_SLOTS; s++) {
    Pet p;
    twoHome(p);
    p.items[IT_EGG] = 2;
    p.rename("B");
    // el huevo de premio, a la mochila (hueco 1: mas el huevo del hueco a medias, ver cutEveryWriteEnd)
    cutEveryWriteEnd(s, [](Pet &x) { x.startFarewell(); }, 1, s == 0 ? 2 : 1);
  }
}

// "hm1" que no llega (un corte en cualquier escritura): guardar o despertar con el hueco 2 solo dice que
// si cuando el hueco 2 quedo escrito, y nunca se pierde nadie
TEST(box, guardar_y_despertar_con_el_hueco_2_solo_dicen_que_si_si_quedo_escrito) {
  for (int mv = 0; mv < 2; mv++) {
    Pet p;
    twoHome(p);
    if (mv) {
      CHECK(p.store(2, 0));  // B a la caja (2): despertar al hueco 2 es un cambio
      hatchNow(p);
    }
    PetRecord a = p.home(1).record(0);
    uint32_t b = mv ? p.boxRecord(2).uid : 0;
    mockNvsSnapshot();
    long total = 0;
    {
      Pet x;
      x.begin();
      long w0 = mockNvsWrites();
      CHECK(mv ? x.wake(2, 1) : x.store(4, 1));
      total = mockNvsWrites() - w0;
    }
    for (long n = 0; n <= total; n++) {
      mockNvsRestore();
      bool ok;
      {
        Pet x;
        x.begin();
        mockNvsCutAfter(n);
        ok = mv ? x.wake(2, 1) : x.store(4, 1);
        mockNvsCutAfter(-1);
      }
      Pet q;
      q.begin();
      std::string at = (mv ? "despertar" : "guardar") + std::string(", corte tras ") + std::to_string(n) + ": ";
      if (ok && mv) CHECK_MSG(q.home(1).uid == b, at + "dijo que si y B no esta en el hueco 2");
      if (ok && !mv) CHECK_MSG(q.home(1).uid != a.uid && q.boxRecord(4).uid == a.uid, at + "dijo que si y A no esta en la caja");
      int seen = (q.home(1).uid == a.uid) + (q.boxState(4) == BOX_OK && q.boxRecord(4).uid == a.uid) +
                 (q.boxState(2) == BOX_OK && q.boxRecord(2).uid == a.uid);
      CHECK_MSG(seen == 1, at + "A esta " + std::to_string(seen) + " veces");
    }
  }
}
