// Ritmo de los niveles (growth.h, v1.35; curva v2 tras la v1.49): la curva, la tabla, los saltos
// de nivel y las migraciones de los guardados ("1 nivel por hora de edad" y la curva v1 de 3 h).
#include "framework.h"
#include "shim/Arduino.h"
#include "../growth.h"
#include "../pet.h"
#include <cmath>
#include <string>

// la formula de docs/GAMEPLAY_TUNING.md #5, en coma flotante del PC: la curva v2 (2 h) y la v1 (3 h)
static long formulaCost(int lv, double k = 100.0, long top = 120) {
  if (lv <= 10) return 20;
  if (lv >= 90) return top;
  return lround(20.0 + k * std::log((double)(lv - 10)) / std::log(80.0));
}

TEST(growth, la_tabla_coincide_con_la_formula) {
  for (int lv = 1; lv <= 200; lv++)
    CHECK_MSG((long)levelCost((uint16_t)lv) == formulaCost(lv), std::string("coste del nivel ") + std::to_string(lv));
}

TEST(growth, la_tabla_v1_coincide_con_su_formula) {
  for (int lv = 1; lv <= 200; lv++)
    CHECK_MSG((long)levelCostV1((uint16_t)lv) == formulaCost(lv, 160.0, 180),
              std::string("coste v1 del nivel ") + std::to_string(lv));
}

TEST(growth, los_extremos_de_la_curva_no_saltan) {
  CHECK_EQ(levelCost(1), (uint16_t)20);
  CHECK_EQ(levelCost(10), (uint16_t)20);
  CHECK_EQ(levelCost(11), (uint16_t)20);
  CHECK_EQ(levelCost(12), (uint16_t)36);
  CHECK_EQ(levelCost(89), (uint16_t)120);
  CHECK_EQ(levelCost(90), (uint16_t)120);
  CHECK_EQ(levelCost(200), (uint16_t)120);
  CHECK_EQ(levelCost(LEVEL_MAX), (uint16_t)120);
  for (uint16_t lv = 2; lv <= 120; lv++) CHECK(levelCost(lv) >= levelCost(lv - 1));  // nunca abarata
}

// los tiempos de la tabla del plan (desde que nace, sin pasear)
TEST(growth, tiempos_de_la_tabla_del_plan) {
  CHECK_EQ(levelStart(1), (uint32_t)0);
  CHECK_EQ(levelStart(2), (uint32_t)20);
  CHECK_EQ(levelStart(10), (uint32_t)180);   // 3 h
  CHECK_EQ(levelStart(11), (uint32_t)200);
  CHECK_EQ(levelStart(12), (uint32_t)220);
  CHECK_EQ(levelStart(16), (uint32_t)410);   // 6,8 h
  CHECK_EQ(levelStart(31), (uint32_t)1567);  // 26,1 h
  CHECK_EQ(levelStart(36), (uint32_t)2025);  // 33,8 h
  CHECK_EQ(levelStart(55), (uint32_t)3940);  // 2,7 dias: la linea de Dratini completa
  CHECK_EQ(levelStart(90), (uint32_t)7926);  // 5,5 dias
  CHECK_EQ(levelStart(91), (uint32_t)(7926 + 120));
  CHECK_EQ(levelFor(3UL * 24 * 60), (uint16_t)58);  // 3 dias de crecimiento
  CHECK_EQ(levelFor(4UL * 24 * 60), (uint16_t)71);  // 4 dias
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
  // nivel 31 a medio camino (89 min): +1 -> nivel 32 a medio camino (91 min)
  uint32_t g = levelStart(31) + 44;
  uint32_t h = addLevels(g, 1);
  CHECK_EQ(levelFor(h), (uint16_t)32);
  CHECK_EQ(levelInto(h), (uint16_t)(44 * 91 / 89));
  // +2 de golpe
  CHECK_EQ(levelFor(addLevels(g, 2)), (uint16_t)33);
  // del tramo rapido al lento: nivel 10 al 75 % (15 de 20) -> nivel 12 al 75 % (27 de 36)
  uint32_t k = addLevels(levelStart(10) + 15, 2);
  CHECK_EQ(levelFor(k), (uint16_t)12);
  CHECK_EQ(levelInto(k), (uint16_t)27);
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
  CHECK_EQ(levelInto(g), (uint16_t)(15 * 89 / 60));
  // cualquier edad: el nivel es el de antes (1 + edad / 60, tope 999)
  for (uint32_t age = 0; age < 1200UL * 60; age += 7) {
    uint32_t old = 1 + age / 60;
    if (old > LEVEL_MAX) old = LEVEL_MAX;
    uint32_t gg = growthFromLegacyAge(age);
    CHECK_EQ((uint32_t)levelFor(gg), old);
    if (old < LEVEL_MAX) CHECK_EQ((uint32_t)levelInto(gg), (age % 60) * levelCost((uint16_t)old) / 60);
  }
}

// curva v1 (3 h) -> v2 (2 h): mismo nivel, barra igual de llena (por abajo, al minuto)
static void checkV1(uint16_t lv, uint32_t intoV1) {
  uint32_t g = growthFromCurveV1(levelStartV1(lv) + intoV1);
  std::string at = "nivel " + std::to_string(lv) + " +" + std::to_string(intoV1);
  CHECK_MSG(levelFor(g) == lv, at);
  uint32_t into = levelInto(g);
  CHECK_MSG(into == intoV1 * levelCost(lv) / levelCostV1(lv), at);
  // la fraccion de la barra no se mueve mas de un minuto de la curva nueva
  double fOld = (double)intoV1 / levelCostV1(lv), fNew = (double)into / levelCost(lv);
  CHECK_MSG(fNew <= fOld && fOld - fNew < 1.0 / levelCost(lv), at);
}

TEST(growth, la_curva_v1_es_la_de_antes) {
  CHECK_EQ(levelCostV1(12), (uint16_t)45);
  CHECK_EQ(levelCostV1(90), (uint16_t)180);
  CHECK_EQ(levelStartV1(31), (uint32_t)2146);
  CHECK_EQ(levelStartV1(90), (uint32_t)11616);
  for (uint16_t lv = 1; lv <= LEVEL_MAX; lv++) {
    CHECK_EQ(levelForV1(levelStartV1(lv)), lv);
    if (lv > 1) CHECK_EQ(levelForV1(levelStartV1(lv) - 1), (uint16_t)(lv - 1));
  }
}

TEST(growth, de_la_curva_v1_conserva_nivel_y_barra) {
  // los del encargo: Lv 40 al 25 % sigue Lv 40 al 25 %; Lv 95, Lv 5
  checkV1(40, 36);  // 36 de 144 -> 24 de 98 (24,5 %)
  CHECK_EQ(levelInto(growthFromCurveV1(levelStartV1(40) + 36)), (uint16_t)24);
  checkV1(95, 90);  // 90 de 180 -> 60 de 120
  CHECK_EQ(levelInto(growthFromCurveV1(levelStartV1(95) + 90)), (uint16_t)60);
  checkV1(5, 10);   // Lv 1-10 cuesta lo mismo: no cambia nada
  CHECK_EQ(growthFromCurveV1(levelStartV1(5) + 10), levelStart(5) + 10);
  CHECK_EQ(growthFromCurveV1(0), (uint32_t)0);  // un huevo o un recien nacido
  // todos los niveles, al principio, a medias y en el ultimo minuto
  for (uint16_t lv = 1; lv < LEVEL_MAX; lv++) {
    checkV1(lv, 0);
    checkV1(lv, levelCostV1(lv) / 2);
    checkV1(lv, levelCostV1(lv) - 1);
  }
  // el tope: se queda en LEVEL_MAX, al principio (como la migracion de la edad)
  CHECK_EQ(growthFromCurveV1(levelStartV1(LEVEL_MAX)), levelStart(LEVEL_MAX));
  CHECK_EQ(growthFromCurveV1(levelStartV1(LEVEL_MAX) + 50), levelStart(LEVEL_MAX));
  CHECK_EQ(growthFromCurveV1(0xFFFFFFFFUL), levelStart(LEVEL_MAX));
  CHECK_EQ(levelFor(growthFromCurveV1(levelStartV1(998) + 179)), (uint16_t)998);
  // nunca sube: la curva nueva es mas barata, el contador baja o se queda
  for (uint32_t g = 0; g < 20000; g += 13) CHECK(growthFromCurveV1(g) <= g);
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
  CHECK_EQ(b.levelMinutesInto(), (uint16_t)(42 * 89 / 60));  // la barra, igual de llena (70 %)
  CHECK_EQ(b.levelMinutesCost(), (uint16_t)89);
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

// ---------------------------------------------------------------- Pet: curva v1 -> v2 ("gcv")

// un guardado de la v1.35-v1.49: "grow" en la curva v1 y sin "gcv"
static void dropGcv() {
  Preferences raw;
  raw.begin("tamapoke", false);
  raw.remove("gcv");
  raw.end();
}

static bool hasKey(const char *k) {
  Preferences raw;
  raw.begin("tamapoke", false);
  bool r = raw.isKey(k);
  raw.end();
  return r;
}

TEST(growth_pet, partida_nueva_y_wipe_ya_son_de_la_curva_v2) {
  Pet p;
  freshPet(p, 4);
  CHECK(hasKey("gcv"));
  p.growMin = levelStart(40) + 50;
  p.rename("NUEVO");
  Pet q;
  q.begin();  // no se migra otra vez
  CHECK_EQ(q.growMin, levelStart(40) + 50);
  // WIPE: borra la NVS y reinicia
  q.factoryReset();
  Pet r;
  r.begin();
  CHECK(hasKey("gcv"));
  r.chooseStarter(1);
  r.eggTap();
  r.eggTap();
  r.eggTap();
  r.growMin = levelStart(30) + 7;
  r.rename("OTRO");
  Pet t;
  t.begin();
  CHECK_EQ(t.growMin, levelStart(30) + 7);
}

TEST(growth_pet, un_guardado_de_la_curva_v1_conserva_el_nivel) {
  Pet a;
  freshPet(a, 8);
  a.growMin = levelStartV1(40) + 36;  // Lv 40 al 25 % en la curva v1
  a.ageMinutes = 5000;
  a.rename("TORTI");
  dropGcv();
  Pet b;
  b.begin();
  CHECK_EQ(b.level(), (uint16_t)40);
  CHECK_EQ(b.levelMinutesInto(), (uint16_t)24);  // 36 de 144 -> 24 de 98
  CHECK_EQ(b.levelMinutesCost(), (uint16_t)98);
  CHECK_EQ(b.ageMinutes, (uint32_t)5000);  // la edad no cambia
  CHECK(hasKey("gcv"));
  CHECK(!hasKey("gmig"));
  // ya escrito: otro arranque no lo convierte otra vez
  Pet c;
  c.begin();
  CHECK_EQ(c.growMin, b.growMin);
  CHECK_EQ(c.level(), (uint16_t)40);
}

TEST(growth_pet, un_guardado_sin_grow_ni_gcv_solo_migra_la_edad) {
  Pet a;
  freshPet(a, 8);
  a.ageMinutes = 30 * 60 + 42;
  a.rename("TORTI");
  {
    Preferences raw;
    raw.begin("tamapoke", false);
    raw.remove("grow");
    raw.remove("gcv");
    raw.end();
  }
  Pet b;
  b.begin();
  CHECK_EQ(b.level(), (uint16_t)31);
  CHECK_EQ(b.levelMinutesInto(), (uint16_t)(42 * 89 / 60));
  CHECK(hasKey("gcv"));
}

// la caja: cada hueco bueno y un despertar a medias ("boxT") pasan a la curva v2; uno danado no
TEST(growth_pet, la_caja_y_boxT_pasan_a_la_curva_v2) {
  Pet a;
  freshPet(a, 4);
  a.items[IT_EGG] = ITEM_MAX;
  a.growMin = levelStartV1(25) + 100;  // Lv 25 en la curva v1 (119 min)
  a.rename("CAJA");
  CHECK(a.store(0));  // a la caja; a casa, un huevo de la mochila
  a.eggTap();
  a.eggTap();
  a.eggTap();
  a.growMin = levelStartV1(40) + 36;  // el activo, Lv 40
  a.rename("ACT");
  PetRecord ra = a.toRecord();
  // un intercambio cortado: el activo ya en el hueco 1, B solo en "boxT"
  PetRecord rb = ra;
  rb.uid = ra.uid + 1;
  rb.speciesId = 7;
  rb.growMin = levelStartV1(95) + 90;
  recordSeal(rb);
  uint8_t junk[sizeof(PetRecord)];
  memset(junk, 0x5A, sizeof(junk));
  {
    Preferences raw;
    raw.begin("tamapoke", false);
    raw.putBytes("box1", &ra, sizeof(ra));
    raw.putBytes("box2", junk, sizeof(junk));  // danado
    raw.putBytes("boxT", &rb, sizeof(rb));
    raw.remove("gcv");
    raw.end();
  }
  Pet q;
  q.begin();
  CHECK_EQ(q.uid, rb.uid);  // boxBoot termino el despertar con B, ya migrado
  CHECK_EQ(q.level(), (uint16_t)95);
  CHECK_EQ(q.levelMinutesInto(), (uint16_t)60);
  CHECK_EQ(q.boxState(0), (uint8_t)BOX_OK);
  CHECK_EQ(levelFor(q.boxRecord(0).growMin), (uint16_t)25);
  CHECK_EQ(q.boxRecord(0).growMin, levelStart(25) + 100 * 82 / 119);
  CHECK_EQ(q.boxState(1), (uint8_t)BOX_OK);
  CHECK_EQ(q.boxRecord(1).uid, ra.uid);
  CHECK_EQ(levelFor(q.boxRecord(1).growMin), (uint16_t)40);
  CHECK_EQ(q.boxRecord(1).growMin, levelStart(40) + 24);
  CHECK_EQ(q.boxState(2), (uint8_t)BOX_DAMAGED);
  {
    Preferences raw;
    raw.begin("tamapoke", false);
    uint8_t back[sizeof(PetRecord)];
    CHECK_EQ(raw.getBytes("box2", back, sizeof(back)), sizeof(back));
    CHECK(memcmp(back, junk, sizeof(junk)) == 0);  // ni lo toca
    raw.end();
  }
}

// un corte de luz en cualquier escritura de la migracion: el siguiente arranque llega al mismo
// sitio y nunca convierte dos veces (el nivel subiria)
TEST(growth_pet, un_corte_en_la_migracion_no_convierte_dos_veces) {
  Pet a;
  freshPet(a, 4);
  a.items[IT_EGG] = ITEM_MAX;
  a.growMin = levelStartV1(60) + 120;
  a.rename("UNO");
  CHECK(a.store(0));
  a.eggTap();
  a.eggTap();
  a.eggTap();
  a.growMin = levelStartV1(33) + 70;
  a.rename("DOS");
  dropGcv();
  mockNvsSnapshot();
  uint32_t wantAct, wantBox;
  long total;
  {
    Pet p;
    long w0 = mockNvsWrites();
    p.begin();
    total = mockNvsWrites() - w0;
    wantAct = p.growMin;
    wantBox = p.boxRecord(0).growMin;
  }
  CHECK_EQ(levelFor(wantAct), (uint16_t)33);
  CHECK_EQ(levelFor(wantBox), (uint16_t)60);
  CHECK(total >= 4);
  for (long n = 0; n <= total; n++) {
    mockNvsRestore();
    {
      Pet p;
      mockNvsCutAfter(n);
      p.begin();
      mockNvsCutAfter(-1);
    }
    Pet q;
    q.begin();
    CHECK_MSG(q.growMin == wantAct, "activo, corte tras " + std::to_string(n));
    CHECK_MSG(q.boxState(0) == BOX_OK && q.boxRecord(0).growMin == wantBox, "caja, corte tras " + std::to_string(n));
    CHECK_MSG(hasKey("gcv") && !hasKey("gmig"), "terminado, corte tras " + std::to_string(n));
  }
}
