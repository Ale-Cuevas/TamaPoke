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

// bicho recien nacido de la especie pedida, con la NVS limpia
static void freshPet(Pet &p, int16_t dex) {
  mockNvsReset();
  mockSetMillis(0);
  p.begin();
  if (p.awaitingStarter()) p.chooseStarter(dex);
  hatchNow(p);
}

// valores reconocibles en todo lo del individuo, y guardados (rename guarda el estado entero)
static void dress(Pet &p, uint8_t seed) {
  p.fullness = 50 + seed;
  p.joy = 60 + seed;
  p.energy = 40 + seed;
  p.hygiene = 70 + seed;
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
  p.medals = MED_LV10 | ((seed & 1) ? MED_BERRY : 0);
  p.berryKnown = seed & 1;
  p.shiny = (seed & 2) != 0;
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
  CHECK_EQ(q.hygiene, p.hygiene);
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
  CHECK_EQ(q.medals, p.medals);
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
  p.fullness = p.joy = p.energy = p.hygiene = 90;
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

TEST(box, el_primer_cuidado_del_dia_da_vinculo_una_vez_aunque_se_cambie) {
  Pet p;
  freshPet(p, 1);
  p.setClock(DAY0 + 10 * 3600);
  CHECK(p.store(0));
  hatchNow(p);  // A activo, B en la caja
  uint8_t a0 = p.bond;
  p.caress();  // primer cuidado del dia: +4, y +1 del mimo
  CHECK_EQ(p.bond, (uint8_t)(a0 + BOND_CARE_DAY + 1));
  CHECK(p.wake(0));  // B
  uint8_t b0 = p.bond;
  p.caress();  // el dia ya conto: solo el mimo
  CHECK_EQ(p.bond, (uint8_t)(b0 + 1));
  p.setClock(DAY0 + 34 * 3600);  // al dia siguiente, a B le toca a el
  mockAdvanceMillis(PET_BOND_COOLDOWN_MS + 1);
  p.caress();
  CHECK_EQ(p.bond, (uint8_t)(b0 + 1 + BOND_CARE_DAY + 1));
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
  p.play();
  CHECK_EQ(p.bond, (uint8_t)(b0 + 2));
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
  p.totalMedals = 7;
  p.gameHi = 31;
  p.strHi = 44;
  p.items[IT_GOLDEN] = 3;
  p.items[IT_RARECANDY] = 2;
  p.lastEnd = CER_FAREWELL;
  p.addSteps(4321, p.dayNumber());  // pasa la meta de 3000: la racha y una medalla suben
  p.rename("A");
  uint16_t streak = p.streak, best = p.bestStreak, totalMed = p.totalMedals;
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
    CHECK_EQ(x->totalMedals, totalMed);
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

TEST(box, las_medallas_son_del_individuo) {
  Pet p;
  freshPet(p, 1);
  p.setLevel(10);
  advance(p, 1);  // checkMedals: Nv.10
  CHECK(p.hasMedal(MED_LV10));
  uint16_t total = p.totalMedals;
  CHECK(p.store(0));
  hatchNow(p);  // otro bicho: sin medallas, el total del jugador sigue
  CHECK(!p.hasMedal(MED_LV10));
  CHECK(p.totalMedals >= total);
  uint16_t total2 = p.totalMedals;
  CHECK(p.wake(0));
  CHECK(p.hasMedal(MED_LV10));
  CHECK_EQ(p.totalMedals, total2);  // volver con su medalla no la cuenta otra vez
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
    CHECK_MSG(q.isEgg() || q.eggHeld(), at + "B despierto pero el huevo no espera");
    CHECK_MSG(q.eggSpecies() == eggT && q.eggIsShiny() == eggS, at + "el huevo cambio");
    CHECK_MSG(q.eggCracks() == 2, at + "el huevo perdio sus grietas");
  });
}
