// Tests de los dos bichos de casa (v3.0): el foco (Pet = jugador + el hueco del foco, PetLife), los dos
// huecos que viven a la vez, el guardado del hueco 1 ("hm1", homerec.h) y las reglas que cambian con dos.
#include "framework.h"
#include "shim/Arduino.h"
#include "shim/Preferences.h"
#include "../pet.h"
#include "../homerec.h"
#include "../audio.h"
#include <string.h>

static const uint32_t H_DAY = 20000u * 86400;  // un dia cualquiera, a medianoche

static void hTap3(Pet &p) {
  p.eggTap();
  p.eggTap();
  p.eggTap();
}

static void hAdvance(Pet &p, uint32_t minutes) {
  for (uint32_t i = 0; i < minutes; i++) {
    mockAdvanceMillis(PET_TICK_MS);
    p.update(millis());
  }
}

// dos bichos en casa: B (dexB) en el hueco 0 y A (dexA) en el 1, la caja vacia y la mochila con huevos
static void twoPets(Pet &p, int16_t dexA = 4, int16_t dexB = 7, uint32_t epoch = H_DAY + 10 * 3600) {
  mockClearForcedRandom();
  mockNvsReset();
  mockSetMillis(1000);
  p.begin();
  if (p.awaitingStarter()) p.chooseStarter(dexA);
  p.items[IT_EGG] = ITEM_MAX;
  hTap3(p);  // A en el 0
  p.speciesId = dexA;
  p.setClock(epoch);
  CHECK(p.store(0, 0));  // A a la caja; en el 0, un huevo de la mochila
  hTap3(p);              // B en el 0
  p.speciesId = dexB;
  CHECK(p.home(1).isVacant());
  CHECK(p.wake(0, 1));   // A despierta en el hueco 1 (vacio)
  CHECK_EQ(p.boxUsed(), (uint8_t)0);
  CHECK(p.home(0).hatched());
  CHECK(p.home(1).hatched());
  CHECK_EQ(p.focusSlot, (uint8_t)0);
}

// valores reconocibles en el bicho del foco (sin guardar)
static void hDress(PetLife &l, uint8_t seed) {
  l.fullness = 50 + seed;
  l.joy = 60 + seed;
  l.energy = 40 + seed;
  l.weight = 10 + seed;
  l.trAtk = 5 + seed;
  l.trDef = 6 + seed;
  l.trSpe = 7 + seed;
  l.careMistakes = seed % 4;
  l.bond = 30 + seed;
  l.growMin = 500 + seed * 37u;
  l.ageMinutes = 1000 + seed * 100u;
  snprintf(l.nick, sizeof(l.nick), "H%u", seed);
}

static bool sameLife(const PetLife &a, const PetLife &b) { return memcmp(&a, &b, sizeof(PetLife)) == 0; }

// ---------------------------------------------------------------- el foco
TEST(home2, una_partida_nueva_empieza_con_el_hueco_2_vacio) {
  Pet p;
  mockNvsReset();
  mockSetMillis(0);
  p.begin();
  CHECK(p.home(1).isVacant());
  CHECK_EQ(p.homeVacant(), (uint8_t)1);
  Pet q;  // tambien sin begin(): el otro hueco nace vacio
  CHECK(q.other.isVacant());
}

TEST(home2, cambiar_el_foco_conserva_todo_de_los_dos) {
  Pet p;
  twoPets(p);
  hDress(p, 1);
  PetLife b = p.home(0);
  hDress(p.slotRef(1), 2);
  PetLife a = p.home(1);
  p.focus(1);
  CHECK_EQ(p.focusSlot, (uint8_t)1);
  CHECK(sameLife(p, a));             // el foco es A, entero
  CHECK(sameLife(p.other, b));       // y B espera entero
  CHECK(sameLife(p.home(0), b));
  CHECK(sameLife(p.home(1), a));
  CHECK_EQ(p.fullness, a.fullness);
  p.focus(0);
  CHECK(sameLife(p, b));
  CHECK(sameLife(p.other, a));
  p.focus(0);  // el mismo: nada
  CHECK(sameLife(p, b));
}

TEST(home2, onSlot_vuelve_al_foco_de_antes_y_avisa_la_reentrada) {
  Pet p;
  twoPets(p);
  uint32_t a = p.home(1).uid;
  uint32_t seen = 0;
  p.onSlot(1, [&] { seen = p.uid; });
  CHECK_EQ(seen, a);
  CHECK_EQ(p.focusSlot, (uint8_t)0);
  CHECK_EQ(p.focusErrors, (uint16_t)0);
  p.focusStrict = false;  // (en el PC una reentrada para las pruebas: aqui se busca a proposito)
  {
    Pet::FocusGuard g(p, 1);
    Pet::FocusGuard g2(p, 1);  // el mismo hueco: vale
    CHECK(g2.ok);
    CHECK_EQ(p.focusErrors, (uint16_t)0);
    Pet::FocusGuard g3(p, 0);  // otro, anidado: no se mueve y avisa
    CHECK(!g3.ok);
    CHECK_EQ(p.focusErrors, (uint16_t)1);
    CHECK_EQ(p.focusSlot, (uint8_t)1);
    bool ran = false;
    p.onSlot(0, [&] { ran = true; });  // lo de dentro no corre sobre el bicho equivocado
    CHECK(!ran);
    CHECK_EQ(p.focusErrors, (uint16_t)2);
  }
  CHECK_EQ(p.focusSlot, (uint8_t)0);
}

// ---------------------------------------------------------------- los dos viven
TEST(home2, los_dos_huecos_pasan_el_minuto) {
  Pet p;
  twoPets(p);
  p.slotRef(0).fullness = p.slotRef(1).fullness = 90;
  uint32_t g0 = p.home(0).growMin, g1 = p.home(1).growMin;
  uint32_t a0 = p.home(0).ageMinutes, a1 = p.home(1).ageMinutes;
  hAdvance(p, 10);
  CHECK_EQ(p.home(0).growMin, g0 + 10);
  CHECK_EQ(p.home(1).growMin, g1 + 10);
  CHECK_EQ(p.home(0).ageMinutes, a0 + 10);
  CHECK_EQ(p.home(1).ageMinutes, a1 + 10);
  CHECK(p.home(0).fullness < 90);
  CHECK(p.home(1).fullness < 90);
  CHECK_EQ(p.home(0).fullness, p.home(1).fullness);  // la misma bajada (sin tarta ninguno)
  CHECK_EQ(p.focusSlot, (uint8_t)0);
  p.focus(1);  // con el foco en el otro, igual
  hAdvance(p, 5);
  CHECK_EQ(p.home(0).growMin, g0 + 15);
  CHECK_EQ(p.home(1).growMin, g1 + 15);
  CHECK_EQ(p.focusSlot, (uint8_t)1);
}

TEST(home2, un_huevo_en_el_hueco_2_eclosiona_solo) {
  Pet p;
  twoPets(p);
  CHECK(p.store(1, 1));  // A a la caja: en el 1, un huevo de la mochila
  CHECK(p.home(1).isEgg());
  CHECK(!p.home(1).isVacant());
  hAdvance(p, STARTER_EGG_MIN + 1);  // (sin huevos del patio: el de antes, a los 3 min)
  CHECK(p.home(1).hatched());
  CHECK(p.home(1).uid != 0);
  CHECK(p.home(1).uid != p.home(0).uid);
}

// ---------------------------------------------------------------- el guardado
TEST(home2, guardar_y_cargar_da_lo_mismo_con_cualquier_foco) {
  for (uint8_t f = 0; f < 2; f++) {
    Pet p;
    twoPets(p);
    hDress(p.slotRef(0), 3);
    hDress(p.slotRef(1), 4);
    p.slotRef(1).sleeping = true;
    p.slotRef(1).nightSleep = true;
    p.slotRef(1).satedUntil = 5000;
    p.slotRef(1).careDay = 1234;
    p.slotRef(1).eggHeldFlag = true;
    p.slotRef(1).eggTarget = 25;
    p.slotRef(1).eggShiny = true;
    p.slotRef(1).eggTaps = 2;
    PetRecord r0 = p.home(0).record(0), r1 = p.home(1).record(0);
    p.focus(f);
    p.rename(f ? "H4" : "H3");  // guarda todo (el nombre ya era ese)
    Pet q;
    q.begin();
    CHECK_EQ(q.focusSlot, (uint8_t)0);
    PetRecord q0 = q.home(0).record(0);
    CHECK(memcmp(&r0, &q0, sizeof(r0)) == 0);
    PetRecord q1 = q.home(1).record(0);
    CHECK(memcmp(&r1, &q1, sizeof(r1)) == 0);
    CHECK_EQ(q.home(1).sleeping, true);
    CHECK_EQ(q.home(1).nightSleep, true);
    CHECK_EQ(q.home(1).satedUntil, (uint32_t)5000);
    CHECK_EQ(q.home(1).careDay, (uint32_t)1234);
    CHECK(q.home(1).eggHeld());
    CHECK_EQ(q.home(1).eggSpecies(), (int16_t)25);
    CHECK(q.home(1).eggIsShiny());
    CHECK_EQ(q.home(1).eggTaps, (uint8_t)2);
    CHECK_EQ(q.eggsOwned(), (uint8_t)(q.items[IT_EGG] + 1));
  }
}

TEST(home2, sin_hm1_el_hueco_2_esta_vacio) {
  Pet p;
  twoPets(p);
  {
    Preferences raw;  // como lo dejaba la v2.6: sin "hm1"
    raw.begin("tamapoke");
    raw.remove("hm1");
  }
  Pet q;
  q.begin();
  CHECK(q.home(0).hatched());
  CHECK(q.home(1).isVacant());
  CHECK_EQ(q.homeHatched(), (uint8_t)1);
}

TEST(home2, un_hm1_danado_deja_el_hueco_vacio) {
  Pet p;
  twoPets(p);
  {
    Preferences raw;
    raw.begin("tamapoke");
    HomeRec r;
    CHECK_EQ(raw.getBytes("hm1", &r, sizeof(r)), sizeof(r));
    ((uint8_t *)&r)[20] ^= 0x10;
    raw.putBytes("hm1", &r, sizeof(r));
  }
  Pet q;
  q.begin();
  CHECK(q.home(1).isVacant());
  CHECK(q.home(0).hatched());
}

TEST(home2, hm1_solo_se_escribe_si_cambia) {
  Pet p;
  mockNvsReset();
  mockSetMillis(0);
  p.begin();
  {
    Preferences raw;
    raw.begin("tamapoke");
    CHECK(!raw.isKey("hm1"));  // vacio sin usar: ni se escribe
  }
  twoPets(p);
  Preferences raw;
  raw.begin("tamapoke");
  CHECK(raw.isKey("hm1"));
  raw.remove("hm1");
  p.rename("X");  // el hueco 1 no cambio: no se reescribe
  CHECK(!raw.isKey("hm1"));
  p.slotRef(1).joy = 3;
  p.rename("Y");
  CHECK(raw.isKey("hm1"));
}

TEST(home2, el_registro_del_hueco_cabe_y_se_sella) {
  CHECK(sizeof(HomeRec) <= 100);
  PetLife l;
  l.speciesId = 25;
  l.uid = 77;
  HomeRec h = homeRecOf(l, 0);
  CHECK(homeRecValid(h));
  CHECK_EQ(h.kind, (uint8_t)HOME_PET);
  HomeRec x = h;
  ((uint8_t *)&x)[30] ^= 1;
  CHECK(!homeRecValid(x));
  PetLife v = vacantPetLife();
  HomeRec hv = homeRecOf(v, 0);
  CHECK(homeRecValid(hv));
  CHECK_EQ(hv.kind, (uint8_t)HOME_VACANT);
  PetLife back;
  homeRecApply(hv, back);
  CHECK(back.isVacant());
}

// ---------------------------------------------------------------- apagada
TEST(home2, el_tiempo_apagado_pasa_a_los_dos) {
  Pet p;
  twoPets(p, 4, 7, H_DAY + 10 * 3600);
  p.slotRef(0).fullness = p.slotRef(1).fullness = 90;
  uint32_t g0 = p.home(0).growMin, g1 = p.home(1).growMin;
  p.rename("");
  Pet q;
  q.begin();
  q.syncClock(H_DAY + 10 * 3600 + 60 * 60);  // una hora
  CHECK_EQ(q.home(0).growMin, g0 + 60);
  CHECK_EQ(q.home(1).growMin, g1 + 60);
  CHECK(q.home(0).fullness < 90);
  CHECK(q.home(1).fullness < 90);
  Pet r;  // y quedo guardado (los dos)
  r.begin();
  CHECK_EQ(r.home(1).growMin, g1 + 60);
}

// ---------------------------------------------------------------- cacas (v3.1)
TEST(home2, la_cascada_entera_quita_solo_las_cacas_del_que_juega) {
  for (uint8_t f = 0; f < 2; f++) {
    Pet p;
    twoPets(p);
    p.slotRef(0).dbgSetPoops(2);
    p.slotRef(1).dbgSetPoops(3);
    p.focus(f);
    p.zenResult(300);
    CHECK_EQ(p.home(f).poops, (uint8_t)0);
    CHECK_EQ(p.home(f).poopSlots(), (uint8_t)0);
    CHECK_EQ(p.home(1 - f).poops, (uint8_t)(f ? 2 : 3));  // el otro las conserva
    CHECK_EQ(p.home(1 - f).poopSlots(), (uint8_t)(f ? 0b11 : 0b111));
  }
}

TEST(home2, las_edades_de_las_cacas_sobreviven_a_guardar_y_cargar) {
  Pet p;
  twoPets(p);
  for (uint8_t s = 0; s < 2; s++) {
    PetLife &l = p.slotRef(s);
    l.ageMinutes = 1000 + s * 100;
    l.dbgSetPoops(2);
    l.poopAt[0] = l.ageMinutes - 50 - s;  // vieja
    l.poopAt[1] = l.ageMinutes - 2 - s;   // reciente
  }
  p.rename("H");  // guarda todo
  Pet q;
  q.begin();
  for (uint8_t s = 0; s < 2; s++) {
    const PetLife &l = q.home(s), &o = p.home(s);
    CHECK_EQ(l.poops, (uint8_t)2);
    uint32_t a[POOP_MAX], b[POOP_MAX];
    l.poopStamps(a);
    o.poopStamps(b);
    CHECK_EQ(a[0], b[0]);
    CHECK_EQ(a[1], b[1]);
    CHECK_EQ(l.stalePoops(), (uint8_t)1);  // la de hace 50 min si; la de hace 2, no
  }
}

TEST(home2, una_caca_suelta_se_guarda_en_su_orden_sin_huecos) {
  Pet p;
  twoPets(p);
  PetLife &l = p.slotRef(1);
  l.ageMinutes = 1000;
  l.dbgSetPoops(3);
  l.poopAt[0] = 900;
  l.poopAt[1] = 950;
  l.poopAt[2] = 990;
  p.focus(1);
  CHECK(p.cleanPoop(1));  // se quita la del medio: quedan las de 900 y 990
  p.focus(0);
  p.rename("H");
  Pet q;
  q.begin();
  uint32_t st[POOP_MAX];
  q.home(1).poopStamps(st);
  CHECK_EQ(q.home(1).poops, (uint8_t)2);
  CHECK_EQ(st[0], (uint32_t)900);
  CHECK_EQ(st[1], (uint32_t)990);
  CHECK_EQ(q.home(1).stalePoops(), (uint8_t)2);  // una de 100 min y otra de 10: las dos son viejas
}

TEST(home2, un_guardado_sin_poopA_da_cacas_recien_hechas) {
  Pet p;
  twoPets(p);
  PetLife &l = p.slotRef(0);
  l.ageMinutes = 1000;
  l.dbgSetPoops(2);
  l.poopAt[0] = l.poopAt[1] = 100;
  p.rename("H");
  {
    Preferences raw;  // como lo dejaba la v3.0: sin "poopA"
    raw.begin("tamapoke");
    raw.remove("poopA");
  }
  Pet q;
  q.begin();
  CHECK_EQ(q.home(0).poops, (uint8_t)2);
  CHECK_EQ(q.home(0).stalePoops(), (uint8_t)0);
}

TEST(home2, el_que_despierta_de_la_caja_trae_las_cacas_recien_hechas) {
  Pet p;
  twoPets(p);
  PetLife &l = p.slotRef(1);
  l.ageMinutes = 1000;
  l.dbgSetPoops(2);
  l.poopAt[0] = l.poopAt[1] = 10;  // viejisimas
  CHECK_EQ(l.stalePoops(), (uint8_t)2);
  CHECK(p.store(0, 1));
  CHECK(p.wake(0, 1));
  CHECK_EQ(p.home(1).poops, (uint8_t)2);
  CHECK_EQ(p.home(1).stalePoops(), (uint8_t)0);  // la caja no lleva la edad de las cacas
}

// ---------------------------------------------------------------- finales por hueco
TEST(home2, se_muere_el_del_hueco_2_y_el_otro_sigue) {
  Pet p;
  twoPets(p);
  p.slotRef(0).fullness = p.slotRef(0).joy = p.slotRef(0).energy = 90;
  p.slotRef(1).bond = 0;
  p.slotRef(1).dbgAbandon();
  hAdvance(p, DEATH_MIN + 1);
  CHECK(p.home(1).isDying());
  CHECK(!p.home(0).isDying());
  uint32_t g0 = p.home(0).growMin;
  hAdvance(p, 3);
  CHECK_EQ(p.home(0).growMin, g0 + 3);  // el otro no se para
  // el Revivir del hueco 2 (con el foco en el)
  p.focus(1);
  CHECK(p.revive());
  CHECK(!p.isDying());
  p.focus(0);
  CHECK(!p.home(1).isDying());
}

TEST(home2, la_ceremonia_de_un_hueco_acaba_sola_y_lo_deja_vacio) {
  Pet p;
  twoPets(p);
  uint32_t b = p.home(0).uid;
  p.focus(1);
  p.release();
  p.focus(0);
  CHECK(p.home(1).ceremony == CER_RELEASE);
  mockAdvanceMillis(CEREMONY_MS + 1);
  p.update(millis());
  CHECK(p.home(1).isVacant());
  CHECK_EQ(p.home(0).uid, b);
  CHECK(p.home(0).hatched());
  CHECK_EQ(p.focusSlot, (uint8_t)0);
}

TEST(home2, los_regalos_esperan_si_queda_otro_bicho_en_casa) {
  Pet p;
  twoPets(p);
  CHECK(p.addGift(IT_CAKE));
  uint8_t cakes = p.items[IT_CAKE];
  p.focus(1);
  p.release();
  mockAdvanceMillis(CEREMONY_MS + 1);
  p.update(millis());
  CHECK_EQ(p.giftCount(), (uint8_t)1);  // los abre el que queda
  CHECK_EQ(p.items[IT_CAKE], cakes);
  // y si se va tambien el ultimo, se desenvuelven
  p.focus(0);
  p.release();
  mockAdvanceMillis(CEREMONY_MS + 1);
  p.update(millis());
  CHECK_EQ(p.giftCount(), (uint8_t)0);
  CHECK_EQ(p.items[IT_CAKE], (uint8_t)(cakes + 1));
}

TEST(home2, el_huevo_de_regalo_solo_con_el_otro_hueco_vacio) {
  Pet p;
  twoPets(p);
  p.items[IT_EGG] = 0;
  p.focus(1);
  CHECK(!p.releaseGivesEgg());  // queda B en el otro hueco
  p.release();
  mockAdvanceMillis(CEREMONY_MS + 1);
  p.update(millis());
  CHECK_EQ(p.items[IT_EGG], (uint8_t)0);
  CHECK(p.home(1).isVacant());
  p.focus(0);
  CHECK(p.releaseGivesEgg());  // ahora el otro esta vacio, sin huevos y sin caja
  p.release();
  mockAdvanceMillis(CEREMONY_MS + 1);
  p.update(millis());
  CHECK_EQ(p.items[IT_EGG], (uint8_t)1);
}

TEST(home2, el_huevo_de_una_muerte_se_queda_en_su_hueco) {
  Pet p;
  twoPets(p);
  p.focus(1);
  p.setLevel(DEATH_EGG_LEVEL);
  p.bond = 0;
  mockForceRandom(0);  // el 60 %: deja huevo
  p.startDying();
  mockClearForcedRandom();
  CHECK(p.deathEggSpecies() > 0);
  int16_t heir = p.deathEggSpecies();
  p.letGo();
  mockAdvanceMillis(CEREMONY_MS + 1);
  p.update(millis());
  CHECK(p.isEgg());  // el foco sigue en el 1
  CHECK(!p.isVacant());
  CHECK_EQ(p.eggSpecies(), heir);
  CHECK(p.home(0).hatched());
}

TEST(home2, los_huevos_tuyos_cuentan_las_dos_incubadoras) {
  Pet p;
  twoPets(p);
  p.items[IT_EGG] = 2;
  p.slotRef(0).eggHeldFlag = true;
  p.slotRef(1).eggHeldFlag = true;
  CHECK_EQ(p.eggsOwned(), (uint8_t)4);
}

// ---------------------------------------------------------------- caminar
TEST(home2, caminar_alegra_a_los_dos_despiertos) {
  Pet p;
  twoPets(p);
  p.slotRef(0).joy = p.slotRef(1).joy = 40;
  p.addSteps(STEPS_PER_JOY * 5, p.dayNumber());
  CHECK_EQ(p.home(0).joy, (uint8_t)45);
  CHECK_EQ(p.home(1).joy, (uint8_t)45);
  CHECK_EQ(p.stepJoyToday, (uint8_t)5);  // el tope diario es del jugador: una vez
  p.slotRef(1).sleeping = true;  // dormido no
  p.addSteps(STEPS_PER_JOY * 2, p.dayNumber());
  CHECK_EQ(p.home(0).joy, (uint8_t)47);
  CHECK_EQ(p.home(1).joy, (uint8_t)45);
}

TEST(home2, las_metas_suben_de_nivel_a_los_dos_y_pagan_una_vez) {
  Pet p;
  twoPets(p);
  uint16_t l0 = p.home(0).level(), l1 = p.home(1).level();
  uint16_t c0 = p.coins;
  p.addSteps(STEP_GOALS[0], p.dayNumber());
  CHECK_EQ(p.home(0).level(), (uint16_t)(l0 + STEP_GOAL_LEVELS[0]));
  CHECK_EQ(p.home(1).level(), (uint16_t)(l1 + STEP_GOAL_LEVELS[0]));
  CHECK_EQ(p.coins, (uint16_t)(c0 + coinsForGoal(1)));
  CHECK_EQ(p.bonusFindsToday, (uint8_t)1);  // el regalo de la meta, uno
  uint8_t lv = 0;
  CHECK_EQ(p.takeGoalBanner(&lv), (uint8_t)1);
  CHECK_EQ(lv, (uint8_t)STEP_GOAL_LEVELS[0]);
}

TEST(home2, el_vinculo_de_caminar_lleva_el_tope_de_cada_uno) {
  Pet p;
  twoPets(p);
  p.slotRef(0).bondDay = p.dayNumber();
  p.slotRef(0).bondToday = BOND_DAY_CAP;  // el 0 ya lleno hoy
  p.slotRef(1).bondDay = p.dayNumber();
  p.slotRef(1).bondToday = 0;
  uint8_t b0 = p.home(0).bond, b1 = p.home(1).bond;
  p.addSteps(STEPS_PER_BOND * 2, p.dayNumber());
  CHECK_EQ(p.home(0).bond, b0);
  CHECK_EQ(p.home(1).bond, (uint8_t)(b1 + 2));
}

TEST(home2, un_huevo_caminando_va_a_casa_solo_con_los_dos_huecos_vacios) {
  Pet p;
  twoPets(p);
  p.addSteps(EGG_MAX_STEPS, p.dayNumber());  // un huevo seguro
  CHECK(p.giftEggs() >= 1);  // envuelto: hay bichos en casa
  // los dos vacios
  Pet v;
  mockNvsReset();
  mockSetMillis(0);
  v.begin();
  v.chooseStarter(4);
  hTap3(v);
  v.items[IT_EGG] = 0;
  v.release();
  mockAdvanceMillis(CEREMONY_MS + 1);
  v.update(millis());
  CHECK(v.home(0).isVacant());
  CHECK(v.home(1).isVacant());
  v.dbgClearGifts();
  v.addSteps(EGG_MAX_STEPS, v.dayNumber());
  CHECK(v.home(0).isEgg());
  CHECK(!v.home(0).isVacant());
  CHECK(v.home(1).isVacant());
}

// ---------------------------------------------------------------- equipo y premios
TEST(home2, el_equipo_reconoce_a_los_dos_de_casa) {
  Pet p;
  twoPets(p);
  uint32_t b = p.home(0).uid, a = p.home(1).uid;
  CHECK_EQ(p.teamSlotOf(b), (int8_t)TEAM_HOME0);
  CHECK_EQ(p.teamSlotOf(a), (int8_t)TEAM_HOME1);
  CHECK(teamIsHome(TEAM_HOME1));
  CHECK_EQ(teamHomeSlot(TEAM_HOME1), (uint8_t)1);
  CHECK(!teamIsHome(0));
  CHECK(p.teamCanFight(a));
  p.slotRef(1).sleeping = true;
  CHECK(!p.teamCanFight(a));
  CHECK(p.teamCanFight(b));
}

TEST(home2, el_premio_del_jefe_llega_al_del_hueco_2) {
  Pet p;
  twoPets(p);
  uint32_t a = p.home(1).uid;
  uint32_t g0 = p.home(0).growMin, g1 = p.home(1).growMin;
  uint8_t bo1 = p.home(1).bond;
  uint32_t t[1] = { a };
  StageReward r = p.stageDone(0, true, t, 1);
  CHECK(r.paid);
  CHECK_EQ(r.pets, (uint8_t)1);
  CHECK_EQ(p.home(1).growMin, g1 + STAGE_GROW_MIN);
  CHECK_EQ(p.home(1).bond, (uint8_t)(bo1 + STAGE_BOND));
  CHECK_EQ(p.home(0).growMin, g0);  // no peleo
  Pet q;
  q.begin();
  CHECK_EQ(q.home(1).growMin, g1 + STAGE_GROW_MIN);
}

TEST(home2, ganar_a_un_salvaje_con_el_del_hueco_2) {
  Pet p;
  twoPets(p);
  CHECK(p.encSpawnNow(25));
  uint32_t a = p.home(1).uid;
  uint32_t g1 = p.home(1).growMin;
  p.slotRef(1).joy = 50;
  EncReward r = p.encounterDone(true, a);
  CHECK(r.won);
  CHECK_EQ(p.home(1).growMin, g1 + ENC_WIN_GROW_MIN);
  CHECK_EQ(p.home(1).joy, (uint8_t)(50 + ENC_WIN_JOY));
}

TEST(home2, hay_encuentros_con_cualquiera_de_casa) {
  Pet p;
  twoPets(p);
  CHECK(p.store(0, 0));  // en el 0 un huevo; en el 1 sigue A, la caja con B
  CHECK(p.boxClear(0));  // y la caja vacia
  CHECK(p.home(0).isEgg());
  CHECK(p.home(1).hatched());
  CHECK(p.encCanRoll());  // el del hueco 2 puede pelear
  CHECK(p.store(1, 1));
  CHECK(p.boxClear(1));
  CHECK(!p.encCanRoll());  // ya nadie: dos huevos y la caja vacia
}

// ---------------------------------------------------------------- racha y vinculo
TEST(home2, la_racha_cuenta_una_vez_y_el_vinculo_a_cada_uno) {
  Pet p;
  twoPets(p);
  p.slotRef(0).careDay = p.slotRef(1).careDay = 0;
  p.lastCareDay = p.dayNumber() - 1;
  p.streak = 4;
  uint8_t b0 = p.home(0).bond, b1 = p.home(1).bond;
  p.caress();  // el 0
  CHECK_EQ(p.streak, (uint16_t)5);
  CHECK_EQ(p.home(0).bond, (uint8_t)(b0 + BOND_CARE_DAY + CARESS_BOND));
  p.onSlot(1, [&] { p.caress(); });
  CHECK_EQ(p.streak, (uint16_t)5);  // la racha, una vez al dia
  CHECK_EQ(p.home(1).bond, (uint8_t)(b1 + BOND_CARE_DAY + CARESS_BOND));
  mockAdvanceMillis(PET_BOND_COOLDOWN_MS + 1);
  p.onSlot(1, [&] { p.caress(); });  // ya lo tuvo hoy
  CHECK_EQ(p.home(1).bond, (uint8_t)(b1 + BOND_CARE_DAY + 2 * CARESS_BOND));
}

// ---------------------------------------------------------------- IMU
TEST(home2, agitar_marea_a_los_dos_y_despierta_a_los_dormidos) {
  Pet p;
  twoPets(p);
  p.slotRef(1).sleeping = true;
  CHECK(!p.shaken());
  CHECK_EQ(p.home(0).emote(), (uint8_t)EMOTE_DIZZY);
  CHECK_EQ(p.home(1).emote(), (uint8_t)EMOTE_NONE);  // dormido: shaken no
  p.wakeGrumpy();
  CHECK(!p.home(1).sleeping);
  CHECK_EQ(p.home(1).emote(), (uint8_t)EMOTE_ANGRY);
  CHECK_EQ(mockSfxCount(SFX_DENY), 1);
  p.sleepDeferred();
  CHECK(p.home(0).sleeping);
  CHECK(p.home(1).sleeping);
  CHECK(p.homeAsleep());
}
