// Ritmo de los niveles (growth.h, v1.35): la curva, la tabla, los saltos de nivel y la
// migracion de los guardados de "1 nivel por hora de edad".
#include "framework.h"
#include "shim/Arduino.h"
#include "../growth.h"
#include "../pet.h"
#include <cmath>
#include <string>

// la formula de docs/GAMEPLAY_TUNING.md #5, en coma flotante del PC
static long formulaCost(int lv) {
  if (lv <= 10) return 20;
  if (lv >= 90) return 180;
  return lround(20.0 + 160.0 * std::log((double)(lv - 10)) / std::log(80.0));
}

TEST(growth, la_tabla_coincide_con_la_formula) {
  for (int lv = 1; lv <= 200; lv++)
    CHECK_MSG((long)levelCost((uint16_t)lv) == formulaCost(lv), std::string("coste del nivel ") + std::to_string(lv));
}

TEST(growth, los_extremos_de_la_curva_no_saltan) {
  CHECK_EQ(levelCost(1), (uint16_t)20);
  CHECK_EQ(levelCost(10), (uint16_t)20);
  CHECK_EQ(levelCost(11), (uint16_t)20);
  CHECK_EQ(levelCost(12), (uint16_t)45);
  CHECK_EQ(levelCost(89), (uint16_t)180);
  CHECK_EQ(levelCost(90), (uint16_t)180);
  CHECK_EQ(levelCost(200), (uint16_t)180);
  CHECK_EQ(levelCost(LEVEL_MAX), (uint16_t)180);
  for (uint16_t lv = 2; lv <= 120; lv++) CHECK(levelCost(lv) >= levelCost(lv - 1));  // nunca abarata
}

// los tiempos de la tabla del plan (desde que nace, sin pasear)
TEST(growth, tiempos_de_la_tabla_del_plan) {
  CHECK_EQ(levelStart(1), (uint32_t)0);
  CHECK_EQ(levelStart(2), (uint32_t)20);
  CHECK_EQ(levelStart(10), (uint32_t)180);   // 3 h
  CHECK_EQ(levelStart(11), (uint32_t)200);
  CHECK_EQ(levelStart(12), (uint32_t)220);
  CHECK_EQ(levelStart(16), (uint32_t)475);   // 7,9 h
  CHECK_EQ(levelStart(31), (uint32_t)2146);  // 35,8 h
  CHECK_EQ(levelStart(36), (uint32_t)2818);  // 47 h
  CHECK_EQ(levelStart(55), (uint32_t)5657);  // 3,9 dias
  CHECK_EQ(levelStart(90), (uint32_t)11616); // 8,1 dias
  CHECK_EQ(levelStart(91), (uint32_t)(11616 + 180));
  CHECK_EQ(levelFor(3UL * 24 * 60), (uint16_t)46);  // 3 dias de crecimiento
  CHECK_EQ(levelFor(4UL * 24 * 60), (uint16_t)55);  // 4 dias: la linea de Dratini completa
}

TEST(growth, nivel_y_comienzo_son_inversos) {
  for (uint16_t lv = 1; lv <= LEVEL_MAX; lv++) {
    uint32_t s = levelStart(lv);
    CHECK_EQ(levelFor(s), lv);
    CHECK_EQ(levelInto(s), (uint16_t)0);
    if (lv > 1) CHECK_EQ(levelFor(s - 1), (uint16_t)(lv - 1));
    if (lv < LEVEL_MAX) CHECK_EQ(levelInto(s + levelCost(lv) - 1), (uint16_t)(levelCost(lv) - 1));
  }
}

TEST(growth, el_tope_es_999_y_no_da_la_vuelta) {
  CHECK_EQ(levelFor(levelStart(LEVEL_MAX)), (uint16_t)LEVEL_MAX);
  CHECK_EQ(levelFor(0xFFFFFFFFUL), (uint16_t)LEVEL_MAX);
  CHECK(levelInto(0xFFFFFFFFUL) < levelCost(LEVEL_MAX));  // la barra no se sale
  CHECK_EQ(levelStart(LEVEL_MAX + 5), levelStart(LEVEL_MAX));
}

TEST(growth, subir_niveles_conserva_lo_lleno_de_la_barra) {
  // nivel 31 a medio camino (131 min): +1 -> nivel 32 a medio camino (133 min)
  uint32_t g = levelStart(31) + 65;
  uint32_t h = addLevels(g, 1);
  CHECK_EQ(levelFor(h), (uint16_t)32);
  CHECK_EQ(levelInto(h), (uint16_t)(65 * 133 / 131));
  // +2 de golpe
  CHECK_EQ(levelFor(addLevels(g, 2)), (uint16_t)33);
  // del tramo rapido al lento: nivel 10 al 75 % (15 de 20) -> nivel 12 al 75 % (33 de 45)
  uint32_t k = addLevels(levelStart(10) + 15, 2);
  CHECK_EQ(levelFor(k), (uint16_t)12);
  CHECK_EQ(levelInto(k), (uint16_t)33);
  // al principio del nivel se queda al principio
  CHECK_EQ(addLevels(levelStart(20), 1), levelStart(21));
  // 0 niveles: nada
  CHECK_EQ(addLevels(g, 0), g);
}

TEST(growth, subir_niveles_se_para_en_el_tope) {
  CHECK_EQ(levelFor(addLevels(levelStart(998) + 90, 2)), (uint16_t)LEVEL_MAX);
  uint32_t top = levelStart(LEVEL_MAX) + 7;
  CHECK_EQ(addLevels(top, 1), top);  // ya en el tope: no cambia
}

TEST(growth, migracion_mismo_nivel_y_misma_barra) {
  // la del plan: 30 h 15 min de edad = nivel 31 al 25 %
  uint32_t g = growthFromLegacyAge(30 * 60 + 15);
  CHECK_EQ(levelFor(g), (uint16_t)31);
  CHECK_EQ(levelInto(g), (uint16_t)(15 * 131 / 60));
  // cualquier edad: el nivel es el de antes (1 + edad / 60, tope 999)
  for (uint32_t age = 0; age < 1200UL * 60; age += 7) {
    uint32_t old = 1 + age / 60;
    if (old > LEVEL_MAX) old = LEVEL_MAX;
    uint32_t gg = growthFromLegacyAge(age);
    CHECK_EQ((uint32_t)levelFor(gg), old);
    if (old < LEVEL_MAX) CHECK_EQ((uint32_t)levelInto(gg), (age % 60) * levelCost((uint16_t)old) / 60);
  }
}

// ---------------------------------------------------------------- Pet: crecimiento y edad

static void freshPet(Pet &p, int16_t dex) {
  mockNvsReset();
  mockSetMillis(0);
  p.begin();
  if (p.awaitingStarter()) p.chooseStarter(dex);
  p.eggTap();
  p.eggTap();
  p.eggTap();
}

static void advanceMin(Pet &p, uint32_t minutes) {
  for (uint32_t i = 0; i < minutes; i++) {
    mockAdvanceMillis(PET_TICK_MS);
    p.update(millis());
  }
}

TEST(growth_pet, crece_con_la_edad_despierto_y_dormido) {
  Pet p;
  freshPet(p, 4);
  CHECK_EQ(p.growMin, (uint32_t)0);
  CHECK_EQ(p.level(), (uint16_t)1);
  uint32_t age = p.ageMinutes;
  advanceMin(p, 20);
  CHECK_EQ(p.growMin, (uint32_t)20);
  CHECK_EQ(p.ageMinutes, age + 20);
  CHECK_EQ(p.level(), (uint16_t)2);  // 20 min por nivel hasta el 10
  p.toggleLight();  // dormido tambien crece
  advanceMin(p, 160);
  CHECK_EQ(p.level(), (uint16_t)10);
}

TEST(growth_pet, el_huevo_no_crece) {
  mockNvsReset();
  mockSetMillis(0);
  Pet p;
  p.begin();
  p.chooseStarter(4);
  advanceMin(p, 2);
  CHECK(p.isEgg());
  CHECK_EQ(p.growMin, (uint32_t)0);
  advanceMin(p, 1);  // a los 3 min eclosiona solo; ese minuto no crece
  CHECK(!p.isEgg());
  CHECK_EQ(p.growMin, (uint32_t)0);
  advanceMin(p, 5);
  CHECK_EQ(p.growMin, (uint32_t)5);
}

TEST(growth_pet, apagado_crece_lo_mismo_que_la_edad) {
  Pet p;
  freshPet(p, 7);
  p.setClock(20000UL * 86400);
  uint32_t age = p.ageMinutes, grow = p.growMin;
  Pet q;
  q.begin();  // reinicio...
  q.syncClock(20000UL * 86400 + 600 * 60);  // ...10 h despues
  CHECK_EQ(q.ageMinutes, age + 600);
  CHECK_EQ(q.growMin, grow + 600);
  CHECK_EQ(q.level(), levelFor(grow + 600));
}

TEST(growth_pet, se_guarda_y_un_huevo_nuevo_empieza_de_cero) {
  Pet a;
  freshPet(a, 25);
  a.growMin = levelStart(31) + 40;
  a.ageMinutes = 5000;
  a.rename("SPARKY");  // guarda todo
  Pet b;
  b.begin();
  CHECK_EQ(b.growMin, levelStart(31) + 40);
  CHECK_EQ(b.ageMinutes, (uint32_t)5000);
  CHECK_EQ(b.level(), (uint16_t)31);
  b.newEgg();
  CHECK_EQ(b.growMin, (uint32_t)0);
}

// el guardado de la placa del usuario (Wartortle shiny nv.31, v1.30): sin la clave "grow"
TEST(growth_pet, un_guardado_sin_grow_conserva_el_nivel) {
  Pet a;
  freshPet(a, 8);  // WARTORTLE
  a.shiny = true;
  a.ageMinutes = 30 * 60 + 42;  // nivel 31 con la regla vieja, 42 min dentro de la hora
  a.rename("TORTI");
  {
    Preferences raw;
    raw.begin("tamapoke", false);
    raw.remove("grow");  // como lo dejo la v1.30
    raw.end();
  }
  Pet b;
  b.begin();
  CHECK_EQ(b.speciesId, (int16_t)8);
  CHECK(b.shiny);
  CHECK_EQ(b.level(), (uint16_t)31);
  CHECK_EQ(b.ageMinutes, (uint32_t)(30 * 60 + 42));      // la edad no cambia
  CHECK_EQ(b.levelMinutesInto(), (uint16_t)(42 * 131 / 60));  // la barra, igual de llena (70 %)
  CHECK_EQ(b.levelMinutesCost(), (uint16_t)131);
  // y tras el primer guardado ya lleva "grow": un segundo arranque da lo mismo
  b.flushSave();
  b.rename("TORTI");
  Pet c;
  c.begin();
  CHECK_EQ(c.growMin, b.growMin);
  CHECK_EQ(c.level(), (uint16_t)31);
  // con el tiempo apagado (como al flashear) sigue en el 31 y avanza desde ahi
  c.setClock(20000UL * 86400);
  Pet d;
  d.begin();
  d.syncClock(20000UL * 86400 + 5 * 60);
  CHECK_EQ(d.level(), (uint16_t)31);
  CHECK_EQ(d.growMin, c.growMin + 5);
}

TEST(growth_pet, un_huevo_de_un_guardado_viejo_empieza_en_cero) {
  mockNvsReset();
  mockSetMillis(0);
  Pet a;
  a.begin();
  a.chooseStarter(1);
  a.ageMinutes = 2;
  a.rename("EGG");
  {
    Preferences raw;
    raw.begin("tamapoke", false);
    raw.remove("grow");
    raw.end();
  }
  Pet b;
  b.begin();
  CHECK(b.isEgg());
  CHECK_EQ(b.growMin, (uint32_t)0);
}

TEST(growth_pet, el_comando_lvl_pone_el_principio_del_nivel) {
  Pet p;
  freshPet(p, 4);
  uint32_t age = p.ageMinutes;
  p.setLevel(16);
  CHECK_EQ(p.level(), (uint16_t)16);
  CHECK_EQ(p.levelMinutesInto(), (uint16_t)0);
  CHECK_EQ(p.ageMinutes, age);  // no envejece
}
