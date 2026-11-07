// Huevos limitados (v1.39): solo se encuentran caminando (uno cada EGG_MIN..EGG_MAX pasos, ~3 a la
// semana con 5000 pasos al dia), van a la mochila y pasan a casa cuando queda libre; sin ninguno, la
// casa se queda vacia. Los cortes de luz de la caja con la casa vacia estan en test_box.cpp.
#include "framework.h"
#include "shim/Arduino.h"
#include "shim/Preferences.h"
#include "../pet.h"

static const uint32_t DAY = 20000;  // un dia cualquiera (numero de dia unix)

static void hatchIt(Pet &p) {
  p.eggTap();
  p.eggTap();
  p.eggTap();
}

// bicho recien nacido, con reloj y la NVS limpia; la mochila sin huevos
static void born(Pet &p, int16_t dex = 4) {
  mockClearForcedRandom();
  mockNvsReset();
  mockSetMillis(0);
  p.begin();
  if (p.awaitingStarter()) p.chooseStarter(dex);
  hatchIt(p);
  p.setClock(DAY * 86400 + 10 * 3600);
}

// la despedida entera (liberar + los 10 s de la ceremonia)
static void releaseAndWait(Pet &p) {
  p.release();
  mockAdvanceMillis(CEREMONY_MS + 1);
  p.update(millis());
}

static void minutes(Pet &p, uint32_t n) {
  for (uint32_t i = 0; i < n; i++) {
    mockAdvanceMillis(PET_TICK_MS);
    p.update(millis());
  }
}

// cuantos huevos hay en la cola de avisos (sin vaciarla)
static int eggNotices(Pet &p) {
  int n = 0;
  while (p.hasFound()) {
    if (p.peekFound() == IT_EGG) n++;
    p.popFound();
  }
  return n;
}

// ---------------------------------------------------------------- el ritmo
TEST(eggs, cinco_mil_pasos_al_dia_dan_unos_3_huevos_a_la_semana) {
  Pet p;
  born(p);
  randomSeed(777);
  const int WEEKS = 300;
  int total = 0, minWeek = 99, maxWeek = 0;
  for (int w = 0; w < WEEKS; w++) {
    int week = 0;
    for (int d = 0; d < 7; d++) {
      uint32_t day = DAY + w * 7 + d;
      for (int i = 0; i < 100; i++) p.addSteps(50, day);  // 5000 pasos en trozos, como llegan del IMU
      week += eggNotices(p);
      p.items[IT_EGG] = 0;  // los que se encuentran se van usando: la mochila nunca se llena
    }
    total += week;
    if (week < minWeek) minWeek = week;
    if (week > maxWeek) maxWeek = week;
  }
  double mean = (double)total / WEEKS;
  CHECK_MSG(mean >= 2.8 && mean <= 3.3, "media semanal " + std::to_string(mean));
  CHECK_MSG(minWeek >= 2, "una semana con " + std::to_string(minWeek));  // 35000 / 15000 = 2,3
  CHECK_MSG(maxWeek <= 5, "una semana con " + std::to_string(maxWeek));  // 35000 / 8000 = 4,4
}

TEST(eggs, nunca_antes_de_8000_pasos_ni_despues_de_15000) {
  for (unsigned seed = 1; seed <= 40; seed++) {
    Pet p;
    born(p);
    randomSeed(seed * 101);
    uint32_t steps = 0;
    while (!p.items[IT_EGG] && steps < EGG_MAX_STEPS + 10) {
      p.addSteps(1, DAY);
      steps++;
    }
    CHECK_MSG(steps >= EGG_MIN_STEPS && steps <= EGG_MAX_STEPS, "semilla " + std::to_string(seed) + ": " +
                                                                  std::to_string(steps) + " pasos");
  }
}

TEST(eggs, cuentan_dormido_y_con_un_huevo_en_casa) {
  Pet p;
  born(p);
  p.toggleLight();  // dormido: los hallazgos y la felicidad no, los huevos si
  CHECK(p.sleeping);
  p.addSteps(EGG_MAX_STEPS, DAY);
  CHECK_EQ(p.items[IT_EGG], (uint8_t)1);
  CHECK_EQ(p.findsToday, (uint8_t)0);

  Pet q;  // un huevo en casa, sin tocar: el que se encuentra va a la mochila
  mockNvsReset();
  q.begin();
  q.chooseStarter(1);
  q.addSteps(EGG_MAX_STEPS, DAY);
  CHECK(q.isEgg() && !q.isVacant());
  CHECK_EQ(q.items[IT_EGG], (uint8_t)1);
}

TEST(eggs, el_contador_sigue_tras_un_reinicio) {
  Pet p;
  born(p);
  p.addSteps(5000, DAY);
  uint32_t left = p.eggStepsLeft();
  CHECK(left > 0 && left <= EGG_MAX_STEPS - 5000);
  p.flushSave();
  Pet q;
  q.begin();
  CHECK_EQ(q.eggStepsLeft(), left);
  CHECK_EQ(q.eggStepsAt(), p.eggStepsAt());
}

TEST(eggs, con_la_mochila_llena_se_pierde_y_lo_dice) {
  Pet p;
  born(p);
  p.items[IT_EGG] = ITEM_MAX;
  p.addSteps(EGG_MAX_STEPS, DAY);
  CHECK_EQ(p.items[IT_EGG], (uint8_t)ITEM_MAX);
  bool full = false;
  bool sawEgg = false;
  while (p.hasFound()) {
    if (p.peekFound(&full) == IT_EGG) {
      sawEgg = true;
      CHECK(full);
    }
    p.popFound();
  }
  CHECK(sawEgg);
}

TEST(eggs, el_huevo_no_se_usa_a_mano) {
  Pet p;
  born(p);
  p.items[IT_EGG] = 2;
  CHECK(!p.canUseItem(IT_EGG));
  CHECK(!p.useItem(IT_EGG));
  CHECK_EQ(p.items[IT_EGG], (uint8_t)2);
  CHECK(!p.isEgg());
}

// ---------------------------------------------------------------- la casa vacia
TEST(eggs, un_final_sin_huevos_deja_la_casa_vacia) {
  Pet p;
  born(p);
  int16_t dex = p.speciesId;
  releaseAndWait(p);
  CHECK(p.isVacant());
  CHECK(p.isEgg());  // sin bicho nacido: todo lo que lo exige lo descarta
  CHECK_EQ(p.uid, (uint32_t)0);
  CHECK(!p.awaitingStarter());
  CHECK(p.isRegistered(dex));  // la Pokedex se queda
  // nada eclosiona ni envejece, ni tocando ni con el tiempo
  hatchIt(p);
  minutes(p, 30);
  CHECK(p.isVacant());
  CHECK_EQ(p.ageMinutes, (uint32_t)0);
  CHECK(!p.canBattle());
  for (uint8_t k = 0; k < BOX_SLOTS; k++) CHECK(!p.canStore(k));
  p.shaken();
  CHECK_EQ(p.emote(), (uint8_t)EMOTE_NONE);  // no hay huevo que se tambalee
  Pet q;  // se queda vacia tras un reinicio, y tras dos dias apagada
  q.begin();
  CHECK(q.isVacant());
  q.syncClock(DAY * 86400 + 10 * 3600 + 2 * 86400);
  CHECK(q.isVacant());
  CHECK_EQ(q.ageMinutes, (uint32_t)0);
}

TEST(eggs, un_final_con_huevos_saca_uno_de_la_mochila) {
  Pet p;
  born(p);
  p.items[IT_EGG] = 2;
  releaseAndWait(p);
  CHECK(p.isEgg());
  CHECK(!p.isVacant());
  CHECK_EQ(p.items[IT_EGG], (uint8_t)1);
  hatchIt(p);
  CHECK(!p.isEgg());
  Pet q;
  q.begin();
  CHECK_EQ(q.items[IT_EGG], (uint8_t)1);
}

TEST(eggs, con_la_casa_vacia_el_huevo_encontrado_va_a_casa_sin_escribir) {
  Pet p;
  born(p);
  releaseAndWait(p);
  CHECK(p.isVacant());
  long w0 = mockNvsWrites();
  p.addSteps(EGG_MAX_STEPS, DAY);
  CHECK_EQ(mockNvsWrites(), w0);  // los pasos nunca escriben: lo vuelca el loop
  CHECK(p.savePending());
  CHECK(p.isEgg());
  CHECK(!p.isVacant());
  CHECK_EQ(p.items[IT_EGG], (uint8_t)0);
  CHECK_EQ(eggNotices(p), 1);  // y se avisa con el hallazgo
  p.flushSave();
  Pet q;
  q.begin();
  CHECK(q.isEgg() && !q.isVacant());
  hatchIt(q);
  CHECK(!q.isEgg());
  CHECK(q.uid != 0);
}

TEST(eggs, un_arranque_con_la_casa_vacia_y_huevos_en_la_mochila_los_usa) {
  Pet p;
  born(p);
  releaseAndWait(p);
  {
    Preferences raw;  // como si un corte hubiera dejado la mochila con uno y la casa vacia
    raw.begin("tamapoke");
    uint8_t bag[IT_COUNT] = { 0 };
    bag[IT_EGG] = 1;
    raw.putBytes("bpack", bag, sizeof(bag));
  }
  Pet q;
  q.begin();
  CHECK(q.isEgg() && !q.isVacant());
  CHECK_EQ(q.items[IT_EGG], (uint8_t)0);
}

TEST(eggs, guardar_sin_huevos_deja_la_casa_vacia_y_despertar_la_llena) {
  Pet p;
  born(p, 7);
  uint32_t a = p.uid;
  CHECK(p.store(2));
  CHECK(p.isVacant());
  CHECK(!p.eggHeld());
  CHECK(p.canWake(2));
  CHECK(p.wake(2));
  CHECK(!p.isVacant());
  CHECK(!p.eggHeld());  // no habia huevo que esperara
  CHECK_EQ(p.uid, a);
  CHECK_EQ(p.speciesId, (int16_t)7);
  CHECK_EQ(p.boxState(2), (uint8_t)BOX_EMPTY);
  Pet q;
  q.begin();
  CHECK_EQ(q.uid, a);
  CHECK(!q.isVacant());
}

TEST(eggs, guardar_usa_antes_el_huevo_que_esperaba_que_la_mochila) {
  Pet p;
  born(p, 1);
  p.items[IT_EGG] = 1;
  CHECK(p.store(0));  // A a la caja: sale el huevo de la mochila
  CHECK_EQ(p.items[IT_EGG], (uint8_t)0);
  int16_t eggT = p.eggSpecies();
  p.eggTap();
  CHECK(p.wake(0));  // A vuelve, el huevo espera
  CHECK(p.eggHeld());
  p.items[IT_EGG] = 3;
  CHECK(p.store(1));
  CHECK(!p.isVacant());
  CHECK_EQ(p.eggSpecies(), eggT);  // el mismo, con su grieta
  CHECK_EQ(p.eggCracks(), (uint8_t)1);
  CHECK_EQ(p.items[IT_EGG], (uint8_t)3);  // la mochila, sin tocar
}

TEST(eggs, egg_por_serie_rellena_la_mochila_y_la_casa_vacia) {
  Pet p;
  born(p);
  releaseAndWait(p);
  p.setBagEggs(3);
  CHECK(!p.isVacant());
  CHECK_EQ(p.items[IT_EGG], (uint8_t)2);
  p.setBagEggs(50);
  CHECK_EQ(p.items[IT_EGG], (uint8_t)ITEM_MAX);
  p.setBagEggs(0);
  CHECK_EQ(p.items[IT_EGG], (uint8_t)0);
}

TEST(eggs, la_primera_partida_sigue_empezando_con_un_huevo) {
  mockNvsReset();
  Pet p;
  p.begin();
  CHECK(p.isEgg());
  CHECK(!p.isVacant());
  CHECK(p.awaitingStarter());
  CHECK_EQ(p.items[IT_EGG], (uint8_t)0);
}

TEST(eggs, un_guardado_anterior_conserva_su_bicho_y_empieza_a_contar) {
  Pet p;
  born(p, 25);
  {
    Preferences raw;  // como lo dejaba la v1.38: sin "vac", "eRem" ni "eAt", y la mochila de 6
    raw.begin("tamapoke");
    raw.remove("vac");
    raw.remove("eRem");
    raw.remove("eAt");
    uint8_t six[6] = { 1, 0, 0, 0, 1, 2 };
    raw.putBytes("bpack", six, sizeof(six));
  }
  Pet q;
  q.begin();
  CHECK(!q.isVacant());
  CHECK_EQ(q.speciesId, (int16_t)25);
  CHECK_EQ(q.items[IT_GOLDEN], (uint8_t)1);
  CHECK_EQ(q.items[IT_RARECANDY], (uint8_t)2);
  CHECK_EQ(q.items[IT_EGG], (uint8_t)0);
  CHECK_EQ(q.eggStepsAt(), (uint16_t)0);  // se sortea con el primer paso
  q.addSteps(1, DAY);
  CHECK(q.eggStepsAt() >= EGG_MIN_STEPS && q.eggStepsAt() <= EGG_MAX_STEPS);
}
