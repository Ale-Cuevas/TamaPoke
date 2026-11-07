// Tests de la logica de la mascota (pet.cpp) sobre los shims del host.
#include "framework.h"
#include "shim/Arduino.h"
#include "../pet.h"
#include "../zengame.h"
#include "../dashgame.h"
#include "../dex.h"
#include "../audio.h"
#include <string.h>

// ---------------------------------------------------------------- helpers
static void advance(Pet &p, uint32_t minutes) {
  for (uint32_t i = 0; i < minutes; i++) {
    mockAdvanceMillis(PET_TICK_MS);
    p.update(millis());
  }
}

// mascota recien eclosionada de la especie pedida, con la NVS limpia
static void makePet(Pet &p, int16_t dex) {
  mockNvsReset();
  mockSetMillis(0);
  p.begin();
  if (p.awaitingStarter()) p.chooseStarter(dex);
  p.eggTap();
  p.eggTap();
  p.eggTap();  // 3 toques = eclosion inmediata
}

static void setStats(Pet &p, uint8_t f, uint8_t j, uint8_t e, uint8_t h) {
  p.fullness = f; p.joy = j; p.energy = e; p.hygiene = h;
}

// ---------------------------------------------------------------- huevo
TEST(egg, primera_partida_pide_inicial) {
  mockNvsReset();
  Pet p;
  p.begin();
  CHECK(p.isEgg());
  CHECK(p.awaitingStarter());
  CHECK_EQ(p.eggCracks(), (uint8_t)0);
  CHECK_EQ(p.registeredCount(), (uint16_t)0);
  CHECK_EQ(p.ageMinutes, (uint32_t)0);
}

// regresion v1.3: el huevo eclosionaba solo a los 3 min mientras el jugador
// aun estaba eligiendo inicial, y se perdia la eleccion
TEST(egg, no_eclosiona_mientras_se_elige_inicial) {
  mockNvsReset();
  Pet p;
  p.begin();
  CHECK(p.awaitingStarter());
  advance(p, 600);  // 10 horas de juego
  CHECK_MSG(p.isEgg(), "el huevo NO debe eclosionar durante la eleccion");
  CHECK_EQ(p.ageMinutes, (uint32_t)0);
  p.chooseStarter(7);
  CHECK(!p.awaitingStarter());
  advance(p, 3);
  CHECK(!p.isEgg());
  CHECK_EQ(p.speciesId, (int16_t)7);
}

TEST(egg, eclosiona_sola_a_los_3_minutos) {
  mockNvsReset();
  Pet p;
  p.begin();
  p.chooseStarter(1);
  advance(p, 2);
  CHECK(p.isEgg());
  advance(p, 1);
  CHECK(!p.isEgg());
  CHECK_EQ(p.speciesId, (int16_t)1);
}

TEST(egg, tres_toques_eclosionan) {
  mockNvsReset();
  Pet p;
  p.begin();
  p.chooseStarter(4);
  p.eggTap();
  CHECK_EQ(p.eggCracks(), (uint8_t)1);
  CHECK(p.isEgg());
  p.eggTap();
  CHECK_EQ(p.eggCracks(), (uint8_t)2);
  CHECK(p.isEgg());
  p.eggTap();
  CHECK(!p.isEgg());
  CHECK_EQ(p.speciesId, (int16_t)4);
}

TEST(egg, eclosionar_sortea_genes_y_resetea_al_individuo) {
  Pet p;
  makePet(p, 4);
  CHECK_RANGE(p.geneAtk, 90, 110);
  CHECK_RANGE(p.geneDef, 90, 110);
  CHECK_RANGE(p.geneSpe, 90, 110);
  CHECK_EQ(p.trAtk, (uint8_t)0);
  CHECK_EQ(p.trDef, (uint8_t)0);
  CHECK_EQ(p.trSpe, (uint8_t)0);
  CHECK_EQ(p.bond, (uint8_t)0);
  CHECK_EQ(p.berryKnown, false);
  CHECK_STREQ(p.nick, "");
  CHECK_MSG(p.isRegistered(4), "criar = registrar en la pokedex");
  CHECK_EQ(p.registeredCount(), (uint16_t)1);
}

TEST(egg, tocar_huevo_no_hace_nada_si_ya_eclosiono) {
  Pet p;
  makePet(p, 4);
  uint8_t cracks = p.eggCracks();
  p.eggTap();
  CHECK_EQ(p.eggCracks(), cracks);
  CHECK_EQ(p.speciesId, (int16_t)4);
}

TEST(egg, rareza_del_huevo_es_valida) {
  mockNvsReset();
  Pet p;
  p.begin();
  CHECK_RANGE(p.eggRarity(), (uint8_t)R_COMUN, (uint8_t)R_LEGENDARIO);
}

// ---------------------------------------------------------------- comida
TEST(feed, baya_normal_sube_25_y_topa_en_100) {
  Pet p;
  makePet(p, 4);
  p.fullness = 50;
  uint8_t notLoved = (uint8_t)((p.speciesId + 1) % 3);
  p.feedBerry(notLoved);
  CHECK_EQ(p.fullness, (uint8_t)75);
  p.feedBerry(notLoved);
  CHECK_EQ(p.fullness, (uint8_t)100);  // 100 en vez de 105
}

TEST(feed, baya_favorita_sube_mas_y_da_corazon) {
  Pet p;
  makePet(p, 4);  // 4 % 3 == 1 -> le gusta la azul
  CHECK(p.lovesBerry(1));
  CHECK(!p.lovesBerry(0));
  CHECK(!p.lovesBerry(2));
  p.fullness = 10;
  p.joy = 10;
  p.feedBerry(1);
  CHECK_EQ(p.fullness, (uint8_t)45);
  CHECK_EQ(p.joy, (uint8_t)20);
  CHECK(p.berryKnown);
  CHECK(p.showHeart());
  CHECK(p.eating());
}

TEST(feed, chuche_engorda) {
  Pet p;
  makePet(p, 4);
  p.fullness = 50;
  p.joy = 50;
  p.feedCandy();
  CHECK_EQ(p.fullness, (uint8_t)60);
  CHECK_EQ(p.joy, (uint8_t)62);
  CHECK_EQ(p.weight, (uint8_t)12);
}

TEST(feed, dormido_o_huevo_no_come) {
  Pet p;
  makePet(p, 4);
  p.fullness = 40;
  p.toggleLight();  // a dormir
  CHECK(p.sleeping);
  p.feedBerry(1);
  p.feedCandy();
  CHECK_EQ(p.fullness, (uint8_t)40);

  Pet e;
  mockNvsReset();
  e.begin();
  e.chooseStarter(4);
  e.fullness = 40;
  e.feedBerry(0);
  CHECK_EQ(e.fullness, (uint8_t)40);
}

// ---------------------------------------------------------------- tick
TEST(tick, despierto_baja_comida_2_y_energia_1_por_minuto) {
  Pet p;
  makePet(p, 4);
  setStats(p, 100, 100, 100, 100);
  p.poops = 0;
  mockForceRandom(99);  // random(100) < 15 falso: sin cacas
  advance(p, 10);
  mockClearForcedRandom();
  CHECK_EQ(p.fullness, (uint8_t)80);
  CHECK_EQ(p.energy, (uint8_t)90);
  CHECK_EQ(p.hygiene, (uint8_t)90);
  CHECK_EQ(p.joy, (uint8_t)90);
  CHECK_EQ(p.ageMinutes, (uint32_t)10);
  CHECK_EQ(p.poops, (uint8_t)0);
}

TEST(tick, las_barras_nunca_bajan_de_cero) {
  Pet p;
  makePet(p, 4);
  setStats(p, 5, 3, 2, 1);
  advance(p, 30);
  CHECK_EQ(p.fullness, (uint8_t)0);
  CHECK_EQ(p.joy, (uint8_t)0);
  CHECK_EQ(p.energy, (uint8_t)0);
  CHECK_EQ(p.hygiene, (uint8_t)0);
}

TEST(tick, dormir_recupera_energia_con_suelos) {
  Pet p;
  makePet(p, 4);
  setStats(p, 100, 100, 10, 100);
  p.toggleLight();
  advance(p, 20);
  CHECK_MSG(p.energy == 100, "durmiendo la energia sube 6/min hasta 100");
  CHECK_MSG(p.fullness > 80, "durmiendo el hambre baja ~4x mas lento");
  advance(p, 2000);  // dormir muchisimo: los suelos protegen
  CHECK_MSG(p.fullness >= 30, "suelo de comida durmiendo = 30");
  CHECK_MSG(p.joy >= 35, "suelo de felicidad durmiendo = 35");
  CHECK_MSG(p.hygiene >= 45, "suelo de higiene durmiendo = 45");
  CHECK_EQ(p.careMistakes, (uint8_t)0);
}

TEST(tick, sobrepeso_se_quema_solo) {
  Pet p;
  makePet(p, 4);
  p.weight = 30;
  advance(p, 30);
  CHECK_EQ(p.weight, (uint8_t)28);  // -1 cada WEIGHT_BURN_AWAKE_MIN (15) minutos
}

TEST(tick, descuido_cuenta_una_vez_por_hora) {
  Pet p;
  makePet(p, 4);
  setStats(p, 0, 0, 0, 0);
  advance(p, 1);
  CHECK_EQ(p.careMistakes, (uint8_t)1);
  advance(p, 50);
  CHECK_MSG(p.careMistakes == 1, "el enfriamiento evita contar el mismo descuido cada minuto");
  advance(p, 11);
  CHECK_EQ(p.careMistakes, (uint8_t)2);
}

// regresion v1.4: el descuido restaba 3 de vinculo cada 30 min y era imposible
// recuperarlo; ahora resta 1 y nunca lo deja en 0
TEST(tick, descuido_enfria_el_vinculo_sin_arrasarlo) {
  Pet p;
  makePet(p, 4);
  p.bond = 50;
  setStats(p, 0, 0, 0, 0);
  advance(p, 1);
  CHECK_EQ(p.bond, (uint8_t)49);
  p.bond = 1;
  advance(p, 61);
  CHECK_MSG(p.bond == 1, "el vinculo no cae por debajo de 1");
}

TEST(tick, buen_cuidado_12h_forja_defensa) {
  Pet p;
  makePet(p, 4);
  CHECK_EQ(p.trDef, (uint8_t)0);
  for (int h = 0; h < 720; h++) {  // mantener todo >= 40 durante 12 h
    setStats(p, 100, 100, 100, 100);
    advance(p, 1);
  }
  CHECK_EQ(p.trDef, (uint8_t)1);
}

// ---------------------------------------------------------------- niveles
// v1.35: el nivel sale del crecimiento (growth.h), no de la edad
TEST(level, sale_del_crecimiento_no_de_la_edad) {
  Pet p;
  makePet(p, 4);
  CHECK_EQ(p.level(), (uint16_t)1);
  p.growMin = 19;
  CHECK_EQ(p.level(), (uint16_t)1);
  p.growMin = 20;  // 20 min por nivel hasta el 10
  CHECK_EQ(p.level(), (uint16_t)2);
  p.ageMinutes = 24 * 60;  // la edad sola ya no sube de nivel
  CHECK_EQ(p.level(), (uint16_t)2);
  p.growMin = levelStart(25);
  CHECK_EQ(p.level(), (uint16_t)25);
}

// Arreglado en v1.8: level() era uint8_t y daba la vuelta a 0 a los ~10,6 dias
TEST(level, no_desborda_con_mascotas_longevas) {
  Pet p;
  makePet(p, 4);
  p.setLevel(255);
  uint16_t before = p.level();
  CHECK_EQ(before, (uint16_t)255);
  p.setLevel(256);  // nivel 256 -> deberia seguir subiendo (o saturar)
  CHECK_MSG(p.level() >= before, "el nivel nunca deberia bajar al crecer");
  p.growMin = 0xFFFFFFF0UL;
  CHECK_EQ(p.level(), (uint16_t)LEVEL_MAX);
}

// ---------------------------------------------------------------- evolucion
TEST(evolve, requiere_nivel_y_buen_cuidado) {
  Pet p;
  makePet(p, 4);  // CHARMANDER evoluciona a nivel 16
  p.setLevel(15);  // nivel 15
  CHECK(!p.canEvolveNow());
  p.setLevel(16);  // nivel 16
  CHECK(p.canEvolveNow());
  p.fullness = 39;  // una barra por debajo de 40 lo bloquea
  CHECK(!p.canEvolveNow());
  p.fullness = 40;
  CHECK(p.canEvolveNow());
}

TEST(evolve, cada_descuido_retrasa_un_nivel) {
  Pet p;
  makePet(p, 4);
  p.setLevel(16);  // nivel 16
  p.careMistakes = 1;
  CHECK(!p.canEvolveNow());
  p.setLevel(17);  // nivel 17
  CHECK(p.canEvolveNow());
}

// Arreglado en v1.8: el cast a uint8_t de (evolveLevel + careMistakes) desbordaba
TEST(evolve, muchos_descuidos_no_adelantan_la_evolucion) {
  Pet p;
  makePet(p, 4);       // CHARMANDER: evoluciona a nivel 16
  p.careMistakes = 250;  // 250 descuidos: deberia estar lejisimos de evolucionar
  p.setLevel(10);  // nivel 10
  CHECK_MSG(!p.canEvolveNow(), "con 250 descuidos no puede evolucionar a nivel 10");
}

TEST(evolve, no_evoluciona_dormido_ni_de_huevo_ni_en_ceremonia) {
  Pet p;
  makePet(p, 4);
  p.setLevel(21);
  CHECK(p.canEvolveNow());
  p.toggleLight();
  CHECK(!p.canEvolveNow());
  p.toggleLight();
  p.startFarewell();
  CHECK(!p.canEvolveNow());

  Pet e;
  mockNvsReset();
  e.begin();
  CHECK(!e.canEvolveNow());
}

TEST(evolve, transforma_registra_y_anima) {
  Pet p;
  makePet(p, 4);
  p.setLevel(21);
  p.evolve();
  CHECK_EQ(p.speciesId, (int16_t)5);
  CHECK_EQ(p.prevSpeciesId, (int16_t)4);
  CHECK(p.isRegistered(5));
  CHECK(p.evolving());
  CHECK_RANGE(p.evolveT(), 0.0f, 1.0f);
  mockAdvanceMillis(EVOLVE_ANIM_MS + 1);
  CHECK(!p.evolving());
}

TEST(evolve, forma_final_no_evoluciona) {
  Pet p;
  makePet(p, 6);  // CHARIZARD
  p.setLevel(201);
  CHECK(!p.canEvolveNow());
  p.evolve();
  CHECK_EQ(p.speciesId, (int16_t)6);
}

TEST(evolve, eevee_se_ramifica_en_134_136) {
  for (int seed = 1; seed <= 12; seed++) {
    randomSeed(seed);
    Pet p;
    makePet(p, DEX_EEVEE);
    p.setLevel(41);
    CHECK(p.canEvolveNow());
    p.evolve();
    CHECK_RANGE(p.speciesId, (int16_t)134, (int16_t)136);
    CHECK(p.isRegistered(p.speciesId));
  }
}

TEST(evolve, boton_de_evolucion_se_pospone_al_declinar) {
  Pet p;
  makePet(p, 4);
  p.setLevel(16);  // nivel 16
  CHECK(p.wantEvolveButton());
  p.declineEvolve();
  CHECK_MSG(!p.wantEvolveButton(), "'mantener forma' quita el boton");
  CHECK_MSG(p.canEvolveNow(), "pero la evolucion sigue disponible");
  p.setLevel(17);  // sube de nivel: se re-ofrece
  CHECK(p.wantEvolveButton());
}

// ---------------------------------------------------------------- stats
TEST(stats, formula_base_por_genes_mas_nivel_y_entreno) {
  Pet p;
  makePet(p, 4);  // CHARMANDER: atk 52, def 43, spe 65
  p.geneAtk = p.geneDef = p.geneSpe = 100;
  p.trAtk = p.trDef = p.trSpe = 0;
  p.setLevel(1);   // nivel 1
  CHECK_EQ(p.atkStat(), (uint16_t)(52 + 1));
  CHECK_EQ(p.defStat(), (uint16_t)(43 + 1));
  CHECK_EQ(p.speStat(), (uint16_t)(65 + 1));
  p.setLevel(10);  // nivel 10
  p.trAtk = 20;
  CHECK_EQ(p.atkStat(), (uint16_t)(52 + 10 + 20));
  p.geneAtk = 110;
  CHECK_EQ(p.atkStat(), (uint16_t)(52 * 110 / 100 + 10 + 20));
}

TEST(stats, el_huevo_no_tiene_stats) {
  mockNvsReset();
  Pet p;
  p.begin();
  CHECK_EQ(p.atkStat(), (uint16_t)0);
  CHECK_EQ(p.defStat(), (uint16_t)0);
  CHECK_EQ(p.speStat(), (uint16_t)0);
}

TEST(train, saco_da_un_punto_cada_4_golpes_con_tope) {
  Pet p;
  makePet(p, 4);
  CHECK_EQ(p.trainStrength(0), (uint8_t)0);
  CHECK_EQ(p.trainStrength(40), (uint8_t)10);
  CHECK_EQ(p.trAtk, (uint8_t)10);
  CHECK_MSG(p.trainStrength(1000) == 18, "tope de 18 por sesion");
  CHECK_EQ(p.strHi, (uint16_t)1000);
}

// Arreglado al portar la bateria: trainStrength() devolvia la subida teorica
TEST(train, el_saco_anuncia_lo_que_de_verdad_sube) {
  Pet p;
  makePet(p, 4);
  p.trAtk = 95;
  uint8_t before = p.trAtk;
  uint8_t gain = p.trainStrength(72);  // 72/4 = 18, pero solo caben 5
  CHECK_EQ(gain, (uint8_t)(p.trAtk - before));
}

TEST(train, entrenamiento_topa_en_100) {
  Pet p;
  makePet(p, 4);
  for (int i = 0; i < 20; i++) p.trainStrength(80);
  CHECK_EQ(p.trAtk, (uint8_t)100);
}

TEST(train, el_saco_cansa_y_quema_peso) {
  Pet p;
  makePet(p, 4);
  p.weight = 40;
  p.energy = 80;
  p.fullness = 80;
  p.trainStrength(30);
  CHECK_EQ(p.energy, (uint8_t)68);
  CHECK_EQ(p.fullness, (uint8_t)75);
  CHECK_EQ(p.weight, (uint8_t)37);  // 30 golpes / TRAIN_WEIGHT_DIV
  p.energy = 3;  // el suelo protege de dejarlo a cero
  p.trainStrength(30);
  CHECK_EQ(p.energy, (uint8_t)3);
}

TEST(train, cascada_da_def_lava_y_guarda_record) {
  Pet p;
  makePet(p, 4);
  p.trDef = 10;
  p.hygiene = 50;
  p.energy = 80;
  p.fullness = 80;
  p.weight = 40;
  CHECK_EQ(p.zenResult(150), (uint8_t)4);  // 15 s en la franja: 1 punto cada 3,5 s
  CHECK_EQ(p.trDef, (uint8_t)14);
  CHECK_EQ(p.hygiene, (uint8_t)(50 + ZEN_WASH));
  CHECK_EQ(p.energy, (uint8_t)72);
  CHECK_EQ(p.fullness, (uint8_t)77);
  CHECK_EQ(p.weight, (uint8_t)40);  // sentado no quema peso
  CHECK_EQ(p.zenHi, (uint16_t)150);
  CHECK_MSG(p.zenResult(300) == ZEN_GAIN_MAX, "30 s perfectos: tope de 8");
  CHECK_EQ(p.zenHi, (uint16_t)300);
  p.zenResult(100);
  CHECK_EQ(p.zenHi, (uint16_t)300);  // un peor resultado no baja el record
}

TEST(train, la_cascada_suma_la_bonificacion_de_la_franja_en_movimiento) {
  Pet p;
  makePet(p, 4);
  p.trDef = 10;
  CHECK_EQ(p.zenResult(450, 150), (uint8_t)11);  // 30 s normales (8) + 15 s moviendose (3)
  CHECK_EQ(p.trDef, (uint8_t)21);
  CHECK_EQ(p.zenHi, (uint16_t)450);
}

TEST(train, la_cascada_anuncia_lo_que_de_verdad_sube_y_topa) {
  Pet p;
  makePet(p, 4);
  p.trDef = 97;
  p.hygiene = 95;
  CHECK_EQ(p.zenResult(300), (uint8_t)3);
  CHECK_EQ(p.trDef, (uint8_t)100);
  CHECK_EQ(p.hygiene, (uint8_t)100);
  CHECK_EQ(p.zenResult(300), (uint8_t)0);
}

TEST(train, la_cascada_no_vale_con_huevo) {
  mockNvsReset();
  Pet p;
  p.begin();
  CHECK(p.isEgg());
  CHECK_EQ(p.zenResult(300), (uint8_t)0);
  CHECK_EQ(p.zenHi, (uint16_t)0);
}

TEST(train, el_record_de_la_cascada_sobrevive_a_un_reinicio) {
  mockNvsReset();
  Pet p;
  makePet(p, 4);
  p.zenResult(123);
  p.flushSave();
  Pet q;
  q.begin();
  CHECK_EQ(q.zenHi, (uint16_t)123);
}

TEST(train, carrera_da_vel_cansa_quema_peso_y_guarda_record) {
  Pet p;
  makePet(p, 4);
  p.trSpe = 10;
  p.energy = 80;
  p.fullness = 80;
  p.weight = 40;
  p.joy = 10;
  CHECK_EQ(p.dashResult(41), (uint8_t)6);  // 1 punto cada 6 filas
  CHECK_EQ(p.trSpe, (uint8_t)16);
  CHECK_EQ(p.energy, (uint8_t)68);
  CHECK_EQ(p.fullness, (uint8_t)75);
  CHECK_EQ(p.weight, (uint8_t)35);  // 40 - 41/8
  CHECK_EQ(p.joy, (uint8_t)45);     // +5 +30
  CHECK_EQ(p.dashHi, (uint16_t)41);
  CHECK_MSG(p.dashResult(500) == DASH_GAIN_MAX, "tope de 18 por carrera");
  CHECK_EQ(p.dashHi, (uint16_t)500);
  p.dashResult(3);
  CHECK_EQ(p.dashHi, (uint16_t)500);
}

TEST(train, la_carrera_anuncia_lo_que_de_verdad_sube) {
  Pet p;
  makePet(p, 4);
  p.trSpe = 95;
  CHECK_EQ(p.dashResult(72), (uint8_t)5);
  CHECK_EQ(p.trSpe, (uint8_t)100);
}

TEST(train, la_carrera_no_vale_con_huevo_y_su_record_se_guarda) {
  mockNvsReset();
  Pet e;
  e.begin();
  CHECK_EQ(e.dashResult(50), (uint8_t)0);
  Pet p;
  makePet(p, 4);
  p.dashResult(37);
  p.flushSave();
  Pet q;
  q.begin();
  CHECK_EQ(q.dashHi, (uint16_t)37);
}

TEST(play, minijuego_entrena_velocidad_y_guarda_record) {
  Pet p;
  makePet(p, 4);
  p.joy = 10;
  p.weight = 50;
  p.playResult(20);
  CHECK_EQ(p.trSpe, (uint8_t)4);
  CHECK_EQ(p.joy, (uint8_t)45);   // +5 +30 por marcar mas de 15
  CHECK_EQ(p.weight, (uint8_t)40);  // 50 - 20/GAME_WEIGHT_DIV
  CHECK_EQ(p.gameHi, (uint16_t)20);
  p.playResult(5);
  CHECK_MSG(p.gameHi == 20, "un resultado peor no baja el record");
}

TEST(play, jugar_sube_felicidad_y_cansa) {
  Pet p;
  makePet(p, 4);
  setStats(p, 50, 50, 50, 50);
  p.play();
  CHECK_EQ(p.joy, (uint8_t)75);
  CHECK_EQ(p.energy, (uint8_t)40);
  CHECK_EQ(p.fullness, (uint8_t)45);
  CHECK(p.showHeart());
}

TEST(care, limpiar_quita_cacas_y_deja_higiene_a_tope) {
  Pet p;
  makePet(p, 4);
  p.poops = 3;
  p.hygiene = 10;
  p.clean();
  CHECK_EQ(p.poops, (uint8_t)0);
  CHECK_EQ(p.hygiene, (uint8_t)100);
}

TEST(care, mimar_sube_felicidad_y_vinculo) {
  Pet p;
  makePet(p, 4);
  p.joy = 50;
  p.caress();
  CHECK_EQ(p.joy, (uint8_t)55);
  CHECK_EQ(p.bond, (uint8_t)1);
  CHECK(p.showHeart());
}

TEST(care, el_vinculo_tiene_tope_diario) {
  Pet p;
  makePet(p, 4);
  for (int i = 0; i < 50; i++) p.play();  // +2 de vinculo cada vez
  CHECK_EQ(p.bond, (uint8_t)BOND_DAY_CAP);  // tope diario (sin reloj no hay +4 del dia)
}

TEST(mood, refleja_el_estado) {
  Pet p;
  makePet(p, 4);
  setStats(p, 80, 80, 80, 80);
  CHECK_EQ((int)p.mood(), (int)MOOD_HAPPY);
  p.joy = 20;
  CHECK_EQ((int)p.mood(), (int)MOOD_SAD);
  p.joy = 80;
  p.feedBerry(0);
  CHECK_EQ((int)p.mood(), (int)MOOD_EATING);
  mockAdvanceMillis(EAT_ANIM_MS + 1);
  p.toggleLight();
  CHECK_EQ((int)p.mood(), (int)MOOD_SLEEPING);
}

// ---------------------------------------------------------------- pokedex
TEST(dexreg, bitmap_cubre_1_a_151) {
  Pet p;
  makePet(p, 4);
  memset(p.dexReg, 0, sizeof(p.dexReg));
  CHECK_EQ(p.registeredCount(), (uint16_t)0);
  for (int16_t d = 1; d <= 151; d++) {
    p.dexReg[(d - 1) >> 3] |= (uint8_t)(1 << ((d - 1) & 7));
    CHECK(p.isRegistered(d));
  }
  CHECK_EQ(p.registeredCount(), (uint16_t)151);
}

TEST(dexreg, fuera_de_rango_no_esta_registrado) {
  Pet p;
  makePet(p, 4);
  memset(p.dexReg, 0xFF, sizeof(p.dexReg));
  CHECK(!p.isRegistered(0));
  CHECK(!p.isRegistered(-1));
  CHECK(!p.isRegistered(152));
  CHECK(!p.isRegistered(32767));
  CHECK_EQ(p.registeredCount(), (uint16_t)151);
}

TEST(dexreg, linea_incompleta_se_detecta) {
  Pet p;
  makePet(p, 1);  // BULBASAUR -> IVYSAUR -> VENUSAUR
  CHECK(p.lineHasUnregistered(1));
  p.dexReg[(2 - 1) >> 3] |= (uint8_t)(1 << ((2 - 1) & 7));
  p.dexReg[(3 - 1) >> 3] |= (uint8_t)(1 << ((3 - 1) & 7));
  CHECK_MSG(!p.lineHasUnregistered(1), "linea 1-2-3 completa");
}

TEST(dexreg, la_rama_de_eevee_cuenta_las_tres) {
  Pet p;
  makePet(p, DEX_EEVEE);
  CHECK(p.lineHasUnregistered(DEX_EEVEE));
  for (int16_t d = 134; d <= 136; d++)
    p.dexReg[(d - 1) >> 3] |= (uint8_t)(1 << ((d - 1) & 7));
  CHECK(!p.lineHasUnregistered(DEX_EEVEE));
}

TEST(egg, el_sorteo_siempre_da_una_especie_valida) {
  Pet p;
  makePet(p, 4);
  for (int i = 0; i < 400; i++) {
    randomSeed(i * 7919 + 1);
    int16_t d = p.pickEggSpecies();
    CHECK_RANGE(d, (int16_t)1, (int16_t)151);
    CHECK_MSG(DEX_TBL[d].rarity != R_EVO, "de un huevo nunca sale una forma evolucionada");
  }
}

TEST(egg, el_primer_huevo_es_un_inicial_clasico) {
  for (int seed = 1; seed <= 20; seed++) {
    randomSeed(seed);
    mockNvsReset();
    Pet p;
    p.begin();
    int16_t d = p.pickEggSpecies();
    bool classic = false;
    for (int i = 0; i < NUM_CLASSIC_DEX; i++)
      if (CLASSIC_DEX[i] == d) classic = true;
    CHECK_MSG(classic, "sin pokedex, solo salen iniciales clasicos");
  }
}

TEST(egg, una_escapada_castiga_la_rareza) {
  Pet p;
  makePet(p, 4);
  p.lastEnd = CER_RUNAWAY;
  for (int seed = 1; seed <= 60; seed++) {
    randomSeed(seed);
    int16_t d = p.pickEggSpecies();
    CHECK_MSG(DEX_TBL[d].rarity == R_COMUN, "tras una escapada solo salen comunes");
  }
}

// ---------------------------------------------------------------- racha
TEST(streak, el_primer_cuidado_del_dia_suma) {
  Pet p;
  makePet(p, 4);
  p.setClock(100u * 86400);
  p.caress();
  CHECK_EQ(p.streak, (uint16_t)1);
  CHECK_EQ(p.bestStreak, (uint16_t)1);
  p.caress();
  CHECK_MSG(p.streak == 1, "el segundo cuidado del mismo dia no suma");
  p.setClock(101u * 86400);
  p.caress();
  CHECK_EQ(p.streak, (uint16_t)2);
}

TEST(streak, un_dia_perdido_reinicia) {
  Pet p;
  makePet(p, 4);
  p.setClock(100u * 86400);
  p.caress();
  p.setClock(101u * 86400);
  p.caress();
  p.setClock(105u * 86400);  // hueco de dias
  p.caress();
  CHECK_EQ(p.streak, (uint16_t)1);
  CHECK_MSG(p.bestStreak == 2, "el record de racha se conserva");
}

TEST(streak, sin_reloj_no_hay_racha) {
  Pet p;
  makePet(p, 4);
  p.caress();
  CHECK_EQ(p.streak, (uint16_t)0);
}

TEST(streak, celebra_los_hitos) {
  Pet p;
  makePet(p, 4);
  for (uint32_t d = 100; d < 103; d++) {
    p.setClock(d * 86400);
    p.caress();
  }
  CHECK_EQ(p.streak, (uint16_t)3);
  CHECK_EQ(p.takeMilestone(), (uint16_t)3);  // hito de 3 dias, para celebrar
  CHECK_EQ(p.takeMilestone(), (uint16_t)0);  // se recoge una vez
  CHECK_EQ(p.lastMilestone, (uint16_t)3);
}

TEST(bonus, racha_y_vinculo_mejoran_el_huevo) {
  Pet p;
  makePet(p, 4);
  CHECK_EQ(p.careBonus(), 0);
  p.streak = 30;
  p.bond = 100;
  CHECK_EQ(p.careBonus(), 14);
  p.streak = 300;  // la racha se capa a 30
  CHECK_EQ(p.careBonus(), 14);
}

// ---------------------------------------------------------------- medallas
TEST(medals, nivel_10_da_medalla) {
  Pet p;
  makePet(p, 4);
  CHECK(!p.hasMedal(MED_LV10));
  p.growMin = levelStart(10) - 1;
  advance(p, 1);
  CHECK(p.hasMedal(MED_LV10));
  CHECK(p.takeMedal() & MED_LV10);  // (con EN FORMA a la vez: nivel >= 5 sin descuidos)
}

// sin sobrepeso, nivel 5 y cero descuidos
TEST(medals, en_forma_llega_al_nivel_5) {
  Pet p;
  makePet(p, 4);
  p.weight = 0;
  p.growMin = levelStart(5) - 1;
  advance(p, 1);
  CHECK(p.hasMedal(MED_FIT));
  CHECK_EQ(p.totalMedals, (uint16_t)1);

  Pet q;
  makePet(q, 4);
  q.careMistakes = 1;   // un descuido la deja fuera
  q.growMin = levelStart(5) - 1;
  advance(q, 1);
  CHECK(!q.hasMedal(MED_FIT));
}

TEST(medals, cada_medalla_nueva_suma_al_total) {
  Pet p;
  makePet(p, 4);
  p.weight = 0;
  p.growMin = levelStart(10) - 1;
  advance(p, 1);  // cruza nivel 10: MED_LV10 + MED_FIT de golpe
  CHECK(p.hasMedal(MED_LV10));
  CHECK(p.hasMedal(MED_FIT));
  CHECK_MSG(p.totalMedals == 2, "el total cuenta cada bit ganado, no cada evento");
}

TEST(medals, forma_final_al_nacer) {
  Pet p;
  makePet(p, 143);  // SNORLAX ya es forma final
  CHECK(p.hasMedal(MED_FINAL));
  CHECK_EQ(p.totalMedals, (uint16_t)1);
}

TEST(medals, la_baya_favorita_da_medalla) {
  Pet p;
  makePet(p, 4);
  p.feedBerry(1);  // su favorita
  CHECK(p.berryKnown);
  advance(p, 1);
  CHECK(p.hasMedal(MED_BERRY));
}

TEST(medals, el_total_acumula_entre_crianzas) {
  Pet p;
  makePet(p, 143);
  CHECK_EQ(p.totalMedals, (uint16_t)1);
  p.newEgg();
  p.chooseStarter(144);  // ARTICUNO, otra forma final
  p.eggTap(); p.eggTap(); p.eggTap();
  CHECK_MSG(p.medals == MED_FINAL, "las medallas del individuo se reinician");
  CHECK_MSG(p.totalMedals == 2, "el total del jugador se conserva");
}

// ---------------------------------------------------------------- ceremonias
TEST(ceremony, despedida_solo_en_forma_final_y_a_los_3_dias) {
  Pet p;
  makePet(p, 6);  // CHARIZARD, forma final
  p.ageMinutes = FAREWELL_AGE_MIN - 1;
  CHECK(!p.canFarewellNow());
  p.ageMinutes = FAREWELL_AGE_MIN;
  CHECK(p.canFarewellNow());

  Pet q;
  makePet(q, 4);  // CHARMANDER no es forma final
  q.ageMinutes = FAREWELL_AGE_MIN * 3;
  CHECK(!q.canFarewellNow());
}

TEST(ceremony, la_despedida_deja_un_huevo_nuevo) {
  Pet p;
  makePet(p, 6);
  p.ageMinutes = FAREWELL_AGE_MIN;
  p.startFarewell();
  CHECK_EQ(p.ceremony, (uint8_t)CER_FAREWELL);
  CHECK_RANGE(p.ceremonyT(), 0.0f, 1.0f);
  mockAdvanceMillis(CEREMONY_MS + 1);
  p.update(millis());
  CHECK(p.isEgg());
  CHECK_EQ(p.ceremony, (uint8_t)CER_NONE);
  CHECK_EQ(p.lastEnd, (uint8_t)CER_FAREWELL);
  CHECK_EQ(p.ageMinutes, (uint32_t)0);
}

TEST(ceremony, escaparse_pide_una_hora_de_abandono_total) {
  Pet p;
  makePet(p, 4);
  setStats(p, 0, 0, 0, 0);
  CHECK(!p.canRunawayNow());
  advance(p, RUNAWAY_TICKS - 1);
  CHECK(!p.canRunawayNow());
  advance(p, 1);
  CHECK(p.canRunawayNow());
}

TEST(ceremony, un_solo_cuidado_salva_del_abandono) {
  Pet p;
  makePet(p, 4);
  p.dbgRunawayReady();
  CHECK(p.canRunawayNow());
  p.feedBerry(0);
  advance(p, 1);
  CHECK_MSG(!p.canRunawayNow(), "comer resetea el contador de abandono");
}

TEST(ceremony, durante_la_ceremonia_no_se_puede_interactuar) {
  Pet p;
  makePet(p, 4);
  setStats(p, 50, 50, 50, 50);
  p.poops = 2;
  p.startRunaway();
  p.feedBerry(0);
  p.feedCandy();
  p.play();
  p.clean();
  p.caress();
  p.toggleLight();
  CHECK_EQ(p.fullness, (uint8_t)50);
  CHECK_EQ(p.joy, (uint8_t)50);
  CHECK_EQ(p.poops, (uint8_t)2);
  CHECK(!p.sleeping);
}

TEST(ceremony, soltar_marca_el_final_como_liberacion) {
  Pet p;
  makePet(p, 4);
  p.release();
  CHECK_EQ(p.ceremony, (uint8_t)CER_RELEASE);
  CHECK_EQ(p.lastEnd, (uint8_t)CER_RELEASE);
  mockAdvanceMillis(CEREMONY_MS + 1);
  p.update(millis());
  CHECK(p.isEgg());
}

TEST(ceremony, un_huevo_no_se_despide_ni_se_suelta) {
  mockNvsReset();
  Pet p;
  p.begin();
  p.startFarewell();
  p.release();
  p.startRunaway();
  CHECK_EQ(p.ceremony, (uint8_t)CER_NONE);
}

// ---------------------------------------------------------------- offline
TEST(offline, aplica_el_tiempo_apagado_con_suelo_de_15) {
  Pet p;
  makePet(p, 4);
  setStats(p, 80, 80, 80, 100);
  p.setClock(1000u * 86400);
  p.syncClock(1000u * 86400 + 600 * 60);  // 600 minutos apagado
  CHECK_EQ(p.ageMinutes, (uint32_t)600);
  CHECK_MSG(p.fullness == 15, "suelo de comida offline = 15");
  CHECK_MSG(p.energy == 15, "suelo de energia offline = 15");
  CHECK_MSG(p.joy == 15, "suelo de felicidad offline = 15");
  CHECK_MSG(p.hygiene == 15, "suelo de higiene offline = 15");
  CHECK_MSG(p.careMistakes == 0, "en ausencia no se acumulan descuidos");
  CHECK_EQ(p.poops, (uint8_t)2);  // una caca cada 4 h
}

TEST(offline, tope_de_dos_semanas) {
  Pet p;
  makePet(p, 4);
  p.setClock(1000u * 86400);
  p.syncClock(1000u * 86400 + 90u * 86400);  // 90 dias
  CHECK_EQ(p.ageMinutes, (uint32_t)(14u * 24 * 60));
}

TEST(offline, menos_de_dos_minutos_no_hace_nada) {
  Pet p;
  makePet(p, 4);
  setStats(p, 80, 80, 80, 100);
  p.setClock(1000u * 86400);
  p.syncClock(1000u * 86400 + 90);
  CHECK_EQ(p.ageMinutes, (uint32_t)0);
  CHECK_EQ(p.fullness, (uint8_t)80);
}

TEST(offline, el_huevo_eclosiona_en_tu_ausencia) {
  Pet p;
  makePet(p, 4);
  p.newEgg();  // ya hay pokedex: no pide inicial
  CHECK(!p.awaitingStarter());
  p.setClock(1000u * 86400);
  p.syncClock(1000u * 86400 + 600);  // 10 minutos
  CHECK(!p.isEgg());
  CHECK_RANGE(p.speciesId, (int16_t)1, (int16_t)151);
}

TEST(offline, durmiendo_se_descansa_tambien_apagado) {
  Pet p;
  makePet(p, 4);
  setStats(p, 90, 90, 20, 90);
  p.toggleLight();
  p.setClock(1000u * 86400);
  p.syncClock(1000u * 86400 + 120 * 60);
  CHECK_EQ(p.energy, (uint8_t)100);
  CHECK_MSG(p.fullness >= 30, "suelos de sueno tambien offline");
  CHECK_MSG(p.poops == 0, "durmiendo no ensucia");
}

TEST(offline, sin_reloj_no_se_aplica_nada) {
  Pet p;
  makePet(p, 4);
  setStats(p, 80, 80, 80, 100);
  p.syncClock(0);
  CHECK_EQ(p.ageMinutes, (uint32_t)0);
  CHECK_EQ(p.fullness, (uint8_t)80);
}

// ---------------------------------------------------------------- guardado
TEST(save, el_estado_sobrevive_a_un_reinicio) {
  Pet a;
  makePet(a, 25);  // PIKACHU
  a.ageMinutes = 1234;
  a.streak = 9;
  a.bestStreak = 11;
  a.bond = 44;
  a.trAtk = 17;
  a.weight = 33;
  a.gameHi = 21;
  a.rename("SPARKY");  // rename persiste todo el estado

  Pet b;
  b.begin();  // mismo "NVS": simula un reinicio
  CHECK_EQ(b.speciesId, (int16_t)25);
  CHECK_EQ(b.ageMinutes, (uint32_t)1234);
  CHECK_EQ(b.streak, (uint16_t)9);
  CHECK_EQ(b.bestStreak, (uint16_t)11);
  CHECK_EQ(b.bond, (uint8_t)44);
  CHECK_EQ(b.trAtk, (uint8_t)17);
  CHECK_EQ(b.weight, (uint8_t)33);
  CHECK_EQ(b.gameHi, (uint16_t)21);
  CHECK_STREQ(b.nick, "SPARKY");
  CHECK(b.isRegistered(25));
  CHECK(!b.awaitingStarter());
}

TEST(save, el_apodo_se_recorta_a_11_caracteres) {
  Pet p;
  makePet(p, 4);
  p.rename("NOMBRE-LARGUISIMO-QUE-NO-CABE");
  CHECK_EQ((int)strlen(p.nick), 11);
  CHECK_STREQ(p.nick, "NOMBRE-LARG");
}

TEST(save, borrado_de_fabrica_deja_todo_como_nuevo) {
  Pet a;
  makePet(a, 4);
  a.factoryReset();
  Pet b;
  b.begin();
  CHECK(b.isEgg());
  CHECK(b.awaitingStarter());
  CHECK_EQ(b.registeredCount(), (uint16_t)0);
}

TEST(save, el_autoguardado_se_marca_y_se_vuelca) {
  Pet p;
  makePet(p, 4);
  CHECK(!p.savePending());
  advance(p, 5);
  CHECK_MSG(p.savePending(), "cada 5 ticks queda un guardado pendiente");
  p.flushSave();
  CHECK(!p.savePending());
}

// ---------------------------------------------------------------- pasos (Tarea 1)
static const uint32_t DIA = 20000;  // un dia cualquiera (numero de dia unix)

// mascota recien nacida con reloj, sin hallazgos ni cacas por medio
static void makeWalker(Pet &p, int16_t dex = 4) {
  makePet(p, dex);
  p.setClock(DIA * 86400 + 10 * 3600);
  p.flushSave();
}

// lo que hay ahora mismo en la "NVS" (un reinicio lo leeria asi)
static uint32_t savedStepsToday() {
  Pet b;
  b.begin();
  return b.stepsToday;
}

TEST(steps, felicidad_peso_y_vinculo_por_pasos_con_restos) {
  Pet p;
  makeWalker(p);
  setStats(p, 80, 50, 80, 100);
  p.weight = 20;
  mockForceRandom(400);  // primer hallazgo a 1200 pasos: lejos
  p.addSteps(99, DIA);
  CHECK_EQ(p.joy, (uint8_t)50);  // 99: aun no
  p.addSteps(1, DIA);
  CHECK_EQ(p.joy, (uint8_t)51);  // el resto no se pierde entre lecturas
  p.addSteps(150, DIA);          // 250 en total
  CHECK_EQ(p.joy, (uint8_t)52);
  CHECK_EQ(p.weight, (uint8_t)20);  // 250 pasos ya no queman: hacen falta 1000
  CHECK_EQ(p.bond, (uint8_t)0);
  p.addSteps(750, DIA);          // 1000 en total
  CHECK_EQ(p.bond, (uint8_t)1);
  CHECK_EQ(p.weight, (uint8_t)19);
  CHECK_EQ(p.joy, (uint8_t)60);
  CHECK_EQ(p.stepsToday, (uint32_t)1000);
  CHECK_EQ(p.stepsTotal, (uint32_t)1000);
  CHECK_EQ(p.energy, (uint8_t)80);  // caminar no gasta energia
}

TEST(steps, la_felicidad_por_pasos_tiene_tope_diario) {
  Pet p;
  makeWalker(p);
  setStats(p, 80, 10, 80, 100);
  mockForceRandom(400);
  for (int i = 0; i < 40; i++) p.addSteps(100, DIA);  // 4000 pasos: 40 puntos, tope 30
  CHECK_EQ(p.stepJoyToday, (uint8_t)STEP_JOY_DAY_CAP);
  CHECK_EQ(p.joy, (uint8_t)(10 + STEP_JOY_DAY_CAP + GOAL_JOY));
}

TEST(steps, huevo_o_dormido_cuentan_pasos_sin_premio) {
  mockNvsReset();
  Pet egg;
  egg.begin();
  egg.chooseStarter(4);
  egg.setClock(DIA * 86400);
  egg.addSteps(1500, DIA);
  CHECK(egg.isEgg());
  CHECK_EQ(egg.stepsToday, (uint32_t)1500);
  CHECK_EQ(egg.findsToday, (uint8_t)0);

  Pet p;
  makeWalker(p);
  setStats(p, 80, 50, 80, 100);
  p.toggleLight();  // a dormir
  p.addSteps(2000, DIA);
  CHECK_EQ(p.joy, (uint8_t)50);
  CHECK_EQ(p.findsToday, (uint8_t)0);
  CHECK_EQ(p.stepsToday, (uint32_t)2000);
  CHECK_EQ(p.stepsTotal, (uint32_t)2000);
}

TEST(steps, a_medianoche_hoy_vuelve_a_cero_y_el_total_no) {
  Pet p;
  makeWalker(p);
  setStats(p, 80, 10, 80, 100);
  mockForceRandom(0);
  for (int i = 0; i < 40; i++) p.addSteps(100, DIA);  // agota felicidad y hallazgos
  CHECK_EQ(p.stepJoyToday, (uint8_t)STEP_JOY_DAY_CAP);
  CHECK_EQ(p.randomFindsToday(), (uint8_t)FINDS_PER_DAY);
  CHECK_EQ(p.bonusFindsToday, (uint8_t)1);  // el regalo de la meta de 3000
  p.addSteps(10, DIA + 1);
  CHECK_EQ(p.bonusFindsToday, (uint8_t)0);
  CHECK_EQ(p.stepsToday, (uint32_t)10);
  CHECK_EQ(p.stepsTotal, (uint32_t)4010);
  CHECK_EQ(p.findsToday, (uint8_t)0);
  CHECK_EQ(p.stepJoyToday, (uint8_t)0);
  CHECK_EQ(p.stepsOn(DIA + 1), (uint32_t)10);
  CHECK_EQ(p.stepsOn(DIA + 2), (uint32_t)0);  // otro dia sin pasos aun: 0
  p.addSteps(0, DIA + 2);                     // cero pasos tambien pasan el dia
  CHECK_EQ(p.stepsToday, (uint32_t)0);
}

TEST(steps, la_meta_salta_una_vez_al_dia_con_regalo_y_racha) {
  Pet p;
  makeWalker(p);
  setStats(p, 80, 40, 80, 100);
  mockForceRandom(400);  // hallazgos cada 1200 pasos
  p.addSteps(2999, DIA);
  CHECK(!p.goalDoneOn(DIA));
  CHECK_EQ(p.findsToday, (uint8_t)2);
  CHECK_EQ(p.streak, (uint16_t)0);
  uint8_t joy = p.joy;
  p.flushSave();
  p.addSteps(1, DIA);
  CHECK(p.goalDoneOn(DIA));
  CHECK(p.takeGoalBanner());
  CHECK(!p.takeGoalBanner());
  CHECK_EQ(p.joy, (uint8_t)(joy + 1 + GOAL_JOY));  // el paso 3000 completa un punto, mas la meta
  CHECK_EQ(p.findsToday, (uint8_t)3);  // el regalo de la meta
  CHECK_EQ(p.streak, (uint16_t)1);     // cuenta como dia de cuidado
  CHECK_MSG(p.savePending() && savedStepsToday() < 3000, "la meta no escribe en flash al momento");
  joy = p.joy;
  p.addSteps(1000, DIA);
  CHECK(!p.takeGoalBanner());  // no repite el mismo dia
  CHECK_EQ(p.joy, (uint8_t)joy);
  p.setClock((DIA + 1) * 86400 + 3600);  // el reloj (lastSeenEpoch) y el dia van juntos
  p.addSteps(3000, DIA + 1);
  CHECK(p.takeGoalBanner());   // al dia siguiente, otra vez
  CHECK_EQ(p.streak, (uint16_t)2);
}

TEST(steps, hallazgo_al_umbral_sorteado) {
  Pet p;
  makeWalker(p);
  mockForceRandom(0);  // umbral 800, baya dorada
  p.addSteps(799, DIA);
  CHECK(!p.hasFound());
  p.addSteps(1, DIA);
  CHECK(p.hasFound());
  bool full = true;
  CHECK_EQ(p.peekFound(&full), (uint8_t)IT_GOLDEN);
  CHECK(!full);
  CHECK_EQ(p.items[IT_GOLDEN], (uint8_t)1);
  p.popFound();
  CHECK(!p.hasFound());
  p.addSteps(1600, DIA);  // dos mas en una sola lectura: se encolan
  CHECK_EQ(p.findsToday, (uint8_t)3);
  CHECK(p.hasFound());
  p.popFound();
  CHECK(p.hasFound());
  p.popFound();
  CHECK(!p.hasFound());
}

TEST(steps, tope_de_hallazgos_al_dia) {
  Pet p;
  makeWalker(p);
  mockForceRandom(0);
  p.addSteps(2900, DIA);  // 3 hallazgos, sin la meta
  p.addSteps(20000, DIA + 1);  // otro dia: 5 al azar, y aparte los regalos de 3000 y 6000
  CHECK_EQ(p.randomFindsToday(), (uint8_t)FINDS_PER_DAY);
  CHECK_EQ(p.bonusFindsToday, (uint8_t)(STEP_GOAL_TIERS - 1));  // v1.36: la de 10000 da niveles, no objeto
  CHECK_EQ(p.findsToday, (uint8_t)(FINDS_PER_DAY + STEP_GOAL_TIERS - 1));
  CHECK_EQ(p.items[IT_GOLDEN], (uint8_t)ITEM_MAX);  // azar 0: todo bayas (3 + 5 + 2, tope 9)
  CHECK_EQ(p.items[IT_RARECANDY], (uint8_t)0);
  // la cola guarda los ultimos FOUND_QUEUE avisos: los 7 de hoy caben enteros (se pierden
  // los mas antiguos, de ayer)
  uint8_t n = 0;
  while (p.hasFound()) { n++; p.popFound(); }
  CHECK_EQ(n, (uint8_t)FOUND_QUEUE);
}

TEST(steps, probabilidades_de_cada_objeto) {
  Pet p;
  makeWalker(p);
  mockForceRandom(54);  // < 55: baya
  p.addSteps(FIND_MIN_STEPS + 54, DIA);
  CHECK_EQ(p.items[IT_GOLDEN], (uint8_t)1);
  mockForceRandom(60);  // 55..89: vitamina (random(3) forzado = 2: carburante)
  p.addSteps(FIND_MIN_STEPS + 60, DIA);
  CHECK_EQ(p.items[IT_CARBOS], (uint8_t)1);
  mockForceRandom(95);  // 90..99: amuleto
  p.addSteps(FIND_MIN_STEPS + 95, DIA);
  CHECK_EQ(p.items[IT_CHARM], (uint8_t)1);
  p.addSteps(FIND_MIN_STEPS + 95, DIA + 1);  // el segundo amuleto se vuelve baya (otro dia: sin la meta)
  CHECK_EQ(p.items[IT_CHARM], (uint8_t)1);
  CHECK_EQ(p.items[IT_GOLDEN], (uint8_t)2);
}

TEST(steps, las_tres_vitaminas_salen_con_el_azar_real) {
  Pet p;
  makeWalker(p);
  for (uint32_t d = 0; d < 60; d++) p.addSteps(6000, DIA + d);
  CHECK(p.items[IT_PROTEIN] > 0);
  CHECK(p.items[IT_IRON] > 0);
  CHECK(p.items[IT_CARBOS] > 0);
}

TEST(steps, la_mochila_tiene_tope_por_objeto) {
  Pet p;
  makeWalker(p);
  p.items[IT_GOLDEN] = ITEM_MAX;
  mockForceRandom(0);
  p.addSteps(800, DIA);
  CHECK_EQ(p.items[IT_GOLDEN], (uint8_t)ITEM_MAX);
  bool full = false;
  CHECK_EQ(p.peekFound(&full), (uint8_t)IT_GOLDEN);
  CHECK(full);
}

TEST(steps, addSteps_nunca_escribe_en_flash) {
  Pet p;
  makeWalker(p);
  CHECK(!p.savePending());
  mockForceRandom(0);
  p.addSteps(900, DIA);  // con hallazgo
  CHECK(p.savePending());
  CHECK_EQ(savedStepsToday(), (uint32_t)0);
  p.flushSave();
  CHECK_EQ(savedStepsToday(), (uint32_t)900);
}

TEST(items, baya_dorada_y_vitaminas) {
  Pet p;
  makeWalker(p);
  setStats(p, 30, 40, 80, 100);
  p.items[IT_GOLDEN] = 1;
  p.items[IT_PROTEIN] = 1;
  p.items[IT_IRON] = 1;
  p.items[IT_CARBOS] = 1;
  CHECK(p.useItem(IT_GOLDEN));
  CHECK_EQ(p.fullness, (uint8_t)(30 + GOLDEN_FOOD));
  CHECK_EQ(p.joy, (uint8_t)(40 + GOLDEN_JOY));
  CHECK(p.showHeart());
  CHECK_EQ(p.items[IT_GOLDEN], (uint8_t)0);
  CHECK(!p.useItem(IT_GOLDEN));  // ya no queda
  uint8_t a = p.trAtk, d = p.trDef, s = p.trSpe;
  CHECK(p.useItem(IT_PROTEIN));
  CHECK(p.useItem(IT_IRON));
  CHECK(p.useItem(IT_CARBOS));
  CHECK_EQ(p.trAtk, (uint8_t)(a + VITAMIN_TRAIN));
  CHECK_EQ(p.trDef, (uint8_t)(d + VITAMIN_TRAIN));
  CHECK_EQ(p.trSpe, (uint8_t)(s + VITAMIN_TRAIN));
  CHECK_EQ(p.itemCount(), (uint8_t)0);
}

TEST(items, topes_y_cuando_no_se_pueden_usar) {
  Pet p;
  makeWalker(p);
  p.items[IT_PROTEIN] = 2;
  p.trAtk = 99;
  CHECK(p.useItem(IT_PROTEIN));
  CHECK_EQ(p.trAtk, (uint8_t)100);
  CHECK(!p.canUseItem(IT_PROTEIN));  // entrenamiento al maximo: no se gasta
  CHECK_EQ(p.items[IT_PROTEIN], (uint8_t)1);
  p.items[IT_CHARM] = 1;
  CHECK(!p.useItem(IT_CHARM));  // el amuleto no se usa a mano
  p.items[IT_GOLDEN] = 1;
  p.toggleLight();
  CHECK(!p.useItem(IT_GOLDEN));  // dormido no come
  CHECK(!p.useItem(IT_COUNT));
}

TEST(items, el_amuleto_lo_gasta_el_siguiente_huevo_y_no_el_inicial) {
  // primera partida: el amuleto no se toca al elegir inicial
  mockNvsReset();
  Pet a;
  a.items[IT_CHARM] = 1;
  a.begin();  // newEgg() del inicial
  CHECK(a.awaitingStarter());
  CHECK_EQ(a.items[IT_CHARM], (uint8_t)1);

  Pet p;
  makeWalker(p);
  p.items[IT_CHARM] = 1;
  p.items[IT_EGG] = 1;  // v1.39: el huevo sale de la mochila
  p.release();
  mockAdvanceMillis(CEREMONY_MS + 1);
  p.update(millis());  // fin de la ceremonia: la casa vacia (v1.41), el amuleto aun sin gastar
  CHECK(p.isVacant());
  CHECK_EQ(p.items[IT_CHARM], (uint8_t)1);
  CHECK(p.hatchFromBag());  // el boton de la casa vacia: huevo nuevo
  CHECK(p.isEgg());
  CHECK(!p.isVacant());
  CHECK_EQ(p.items[IT_EGG], (uint8_t)0);
  CHECK_EQ(p.items[IT_CHARM], (uint8_t)0);

  // x3: 1/16 en vez de 1/48 tras una liberacion, sin racha ni vinculo
  CHECK_EQ(p.shinyOdds(false), 48);
  CHECK_EQ(p.shinyOdds(true), 16);
  p.lastEnd = CER_FAREWELL;  // 1/24 -> 1/8, el suelo de siempre
  CHECK_EQ(p.shinyOdds(true), 8);
  p.streak = 30;             // racha y vinculo a tope: 1/10, y con amuleto el suelo
  p.bond = 100;
  CHECK_EQ(p.shinyOdds(false), 10);
  CHECK_EQ(p.shinyOdds(true), 8);
  // la tirada usa ese denominador: con 0 forzado sale shiny
  p.items[IT_CHARM] = 1;
  mockForceRandom(0);
  p.newEgg();
  mockClearForcedRandom();
  CHECK_EQ(p.items[IT_CHARM], (uint8_t)0);
  p.eggTap(); p.eggTap(); p.eggTap();
  CHECK(p.shiny);
}

TEST(items, la_simulacion_de_huevos_no_gasta_el_amuleto) {
  Pet p;
  makeWalker(p);
  p.items[IT_CHARM] = 1;
  for (int i = 0; i < 20; i++) p.pickEggSpecies();  // comando EGGS
  CHECK_EQ(p.items[IT_CHARM], (uint8_t)1);
}

TEST(steps, mochila_y_pasos_sobreviven_a_un_reinicio_y_a_un_huevo_nuevo) {
  Pet a;
  makeWalker(a);
  mockForceRandom(0);
  a.addSteps(1234, DIA);
  a.items[IT_IRON] = 4;
  a.flushSave();

  Pet b;
  b.begin();
  CHECK_EQ(b.stepsToday, (uint32_t)1234);
  CHECK_EQ(b.stepsTotal, (uint32_t)1234);
  CHECK_EQ(b.stepDay, DIA);
  CHECK_EQ(b.findsToday, (uint8_t)1);
  CHECK_EQ(b.items[IT_GOLDEN], (uint8_t)1);
  CHECK_EQ(b.items[IT_IRON], (uint8_t)4);
  b.release();
  mockAdvanceMillis(CEREMONY_MS + 1);
  b.update(millis());  // huevo nuevo
  CHECK(b.isEgg());
  CHECK_EQ(b.stepsToday, (uint32_t)1234);
  CHECK_EQ(b.items[IT_IRON], (uint8_t)4);
}

TEST(steps, los_restos_sobreviven_a_un_reinicio) {
  Pet a;
  makeWalker(a);
  setStats(a, 80, 50, 80, 100);
  mockForceRandom(400);
  a.addSteps(1234, DIA);  // 12 puntos de felicidad y 34 pasos de resto
  CHECK_EQ(a.joy, (uint8_t)62);
  a.flushSave();
  Pet b;
  b.begin();
  b.addSteps(66, DIA);    // 34 + 66 = otro punto
  CHECK_EQ(b.joy, (uint8_t)63);
  CHECK_EQ(b.stepJoyToday, (uint8_t)13);
}

TEST(steps, un_guardado_antiguo_carga_con_todo_a_cero) {
  Pet a;
  makePet(a, 4);  // guardado de la v1.19: sin claves de pasos
  Pet b;
  b.begin();
  CHECK_EQ(b.stepsToday, (uint32_t)0);
  CHECK_EQ(b.stepsTotal, (uint32_t)0);
  CHECK_EQ(b.itemCount(), (uint8_t)0);
}

TEST(care, el_tope_diario_de_vinculo_vuelve_a_cero_a_medianoche) {
  Pet p;
  makeWalker(p);
  for (int i = 0; i < 20; i++) p.play();
  CHECK_EQ(p.bond, (uint8_t)(BOND_DAY_CAP + BOND_CARE_DAY));  // tope + primer cuidado del dia
  p.setClock((DIA + 1) * 86400 + 3600);
  p.addSteps(1000, DIA + 1);  // +1 de vinculo caminando, sin cuidado previo hoy
  CHECK_EQ(p.bond, (uint8_t)(BOND_DAY_CAP + BOND_CARE_DAY + 1));
}

// v1.22: mimar no llena el tope del dia a toques; da vinculo cada PET_BOND_COOLDOWN_MS
TEST(care, mimar_da_vinculo_con_enfriamiento) {
  Pet p;
  makePet(p, 4);
  p.joy = 20;
  mockAdvanceMillis(1000);
  for (int i = 0; i < 10; i++) p.caress();
  CHECK_EQ(p.bond, (uint8_t)1);  // diez toques seguidos: uno solo cuenta
  CHECK_EQ(p.joy, (uint8_t)70);  // la felicidad si sube con cada toque
  mockAdvanceMillis(PET_BOND_COOLDOWN_MS - 1);
  p.caress();
  CHECK_EQ(p.bond, (uint8_t)1);
  mockAdvanceMillis(1);
  p.caress();
  CHECK_EQ(p.bond, (uint8_t)2);
}

// v1.22: un +2 a un punto del tope no lo pasa (antes 19 + 2 = 21)
TEST(care, el_tope_de_vinculo_no_se_pasa) {
  Pet p;
  makePet(p, 4);
  p.clean();
  for (int i = 1; i < BOND_DAY_CAP - 1; i++) p.clean();  // +1 cada bano: tope - 1
  CHECK_EQ(p.bond, (uint8_t)(BOND_DAY_CAP - 1));
  p.play();  // +2
  CHECK_EQ(p.bond, (uint8_t)BOND_DAY_CAP);
}

// v1.22: el vinculo maximo cuesta 5-8 dias aun cuidandolo al maximo cada dia
TEST(care, el_vinculo_maximo_cuesta_mas_de_cinco_dias) {
  Pet p;
  makeWalker(p);
  int dayFull = 0;
  for (int d = 0; d < 12 && !dayFull; d++) {
    p.setClock((DIA + d) * 86400 + 10 * 3600);
    for (int i = 0; i < 30; i++) p.play();  // muchisimo cuidado
    mockAdvanceMillis(PET_BOND_COOLDOWN_MS);
    p.caress();
    if (p.bond >= 100) dayFull = d + 1;
  }
  CHECK_MSG(dayFull >= 5 && dayFull <= 8, "vinculo 100 entre el dia 5 y el 8");
  CHECK_EQ(dayFull, 8);
}

// ---------------------------------------------------------------- metas de 3000 / 6000 / 10000 (v1.25)
TEST(steps, las_tres_metas_saltan_una_vez_al_dia_y_en_orden) {
  Pet p;
  makeWalker(p);
  setStats(p, 80, 20, 80, 100);
  mockForceRandom(400);  // hallazgos al azar cada 1200 pasos
  uint8_t lv = 0;
  CHECK_EQ(p.level(), (uint16_t)1);
  p.addSteps(3000, DIA);
  CHECK_EQ(p.goalTierOn(DIA), (uint8_t)1);
  CHECK_EQ(p.takeGoalBanner(&lv), (uint8_t)1);
  CHECK_EQ(lv, (uint8_t)1);
  CHECK_EQ(p.level(), (uint16_t)2);  // v1.36: +1 nivel
  CHECK_EQ(p.streak, (uint16_t)1);
  uint8_t joy = p.joy, bond = p.bond, bonus = p.bonusFindsToday;
  p.addSteps(2999, DIA);
  CHECK_EQ(p.goalTierOn(DIA), (uint8_t)1);
  CHECK_EQ(p.takeGoalBanner(), (uint8_t)0);
  CHECK_EQ(p.level(), (uint16_t)2);
  p.addSteps(1, DIA);  // 6000
  CHECK_EQ(p.goalTierOn(DIA), (uint8_t)2);
  CHECK_EQ(p.takeGoalBanner(&lv), (uint8_t)2);
  CHECK_EQ(lv, (uint8_t)1);
  CHECK_EQ(p.level(), (uint16_t)3);  // +1 mas
  CHECK_EQ(p.bonusFindsToday, (uint8_t)(bonus + 1));
  CHECK_MSG(p.joy >= joy + GOAL2_JOY, "la meta de 6000 da felicidad");
  CHECK_MSG(p.bond >= bond + GOAL2_BOND, "y vinculo");
  CHECK_EQ(p.items[IT_RARECANDY], (uint8_t)0);
  bonus = p.bonusFindsToday;
  p.addSteps(4000, DIA);  // 10000
  CHECK_EQ(p.goalTierOn(DIA), (uint8_t)3);
  CHECK_EQ(p.takeGoalBanner(&lv), (uint8_t)3);
  CHECK_EQ(lv, (uint8_t)2);
  CHECK_EQ(p.level(), (uint16_t)5);  // +2: la de 10000 ya no da Caramelo Raro
  CHECK_EQ(p.items[IT_RARECANDY], (uint8_t)0);
  CHECK_EQ(p.bonusFindsToday, bonus);  // ni un regalo en la mochila
  CHECK(p.hasMedal(MED_MARATHON));
  CHECK_EQ(p.streak, (uint16_t)1);  // la racha solo la cuenta la primera
  uint16_t total = p.totalMedals;
  p.addSteps(5000, DIA);
  CHECK_EQ(p.takeGoalBanner(), (uint8_t)0);  // ninguna se repite el mismo dia
  CHECK_EQ(p.level(), (uint16_t)5);
  // al dia siguiente, otra vez las tres; la medalla no se cuenta dos veces
  p.setClock((DIA + 1) * 86400 + 3600);
  p.addSteps(10000, DIA + 1);
  CHECK_EQ(p.takeGoalBanner(&lv), (uint8_t)3);  // cruzadas de golpe: el banner muestra la mas alta
  CHECK_EQ(lv, (uint8_t)4);                     // y los niveles de las tres
  CHECK_EQ(p.level(), (uint16_t)9);
  CHECK_EQ(p.goalTierOn(DIA + 1), (uint8_t)3);
  CHECK_EQ(p.goalTierOn(DIA), (uint8_t)0);
  CHECK_EQ(p.bonusFindsToday, (uint8_t)(STEP_GOAL_TIERS - 1));  // los regalos de 3000 y 6000
  CHECK_EQ(p.items[IT_RARECANDY], (uint8_t)0);
  CHECK_EQ(p.streak, (uint16_t)2);
  CHECK_EQ(p.totalMedals, total);
}

// v1.36: la barra conserva lo que llevaba y la edad no cambia
TEST(steps, los_niveles_de_las_metas_no_tiran_el_progreso_ni_envejecen) {
  Pet p;
  makeWalker(p);
  p.growMin = levelStart(31) + 65;  // nivel 31 a medio camino (131 min)
  uint32_t age = p.ageMinutes;
  p.addSteps(3000, DIA);
  CHECK_EQ(p.level(), (uint16_t)32);
  CHECK_EQ(p.levelMinutesInto(), (uint16_t)(65 * levelCost(32) / levelCost(31)));
  CHECK_EQ(p.ageMinutes, age);
}

TEST(steps, los_niveles_de_las_metas_se_paran_en_el_tope) {
  Pet p;
  makeWalker(p);
  p.setLevel(LEVEL_MAX - 1);
  uint8_t lv = 0;
  p.addSteps(10000, DIA);  // +4, pero solo queda 1
  CHECK_EQ(p.level(), (uint16_t)LEVEL_MAX);
  CHECK_EQ(p.takeGoalBanner(&lv), (uint8_t)3);
  CHECK_EQ(lv, (uint8_t)1);
}

// el nivel 10 de una meta da su medalla al momento, no en el siguiente minuto
TEST(steps, un_nivel_de_meta_da_la_medalla_de_nivel) {
  Pet p;
  makeWalker(p);
  p.setLevel(9);
  p.medals = 0;
  p.addSteps(3000, DIA);
  CHECK_EQ(p.level(), (uint16_t)10);
  CHECK(p.hasMedal(MED_LV10));
}

TEST(steps, el_vinculo_de_las_metas_va_fuera_del_tope_diario) {
  Pet p;
  makeWalker(p);
  for (int i = 0; i < 20; i++) p.play();  // tope del dia lleno
  uint8_t bond = p.bond;
  mockForceRandom(400);
  p.addSteps(6000, DIA);  // la de 3000 (ya cuidado hoy: sin +4) y la de 6000
  CHECK_EQ(p.bond, (uint8_t)(bond + GOAL2_BOND));
  p.addSteps(4000, DIA);
  CHECK_EQ(p.bond, (uint8_t)(bond + GOAL2_BOND + GOAL3_BOND));
}

TEST(steps, dormido_las_metas_marcan_el_dia_sin_premio) {
  Pet p;
  makeWalker(p);
  setStats(p, 80, 50, 80, 100);
  p.toggleLight();  // a dormir
  uint16_t lvBefore = p.level();
  uint8_t lv = 9;
  p.addSteps(10000, DIA);
  CHECK_EQ(p.goalTierOn(DIA), (uint8_t)3);
  CHECK_EQ(p.takeGoalBanner(&lv), (uint8_t)3);
  CHECK_EQ(lv, (uint8_t)0);                 // v1.36: dormido, sin niveles...
  CHECK_EQ(p.level(), lvBefore);
  CHECK_EQ(p.items[IT_RARECANDY], (uint8_t)0);
  CHECK(!p.hasMedal(MED_MARATHON));
  CHECK_EQ(p.joy, (uint8_t)50);
  CHECK_EQ(p.streak, (uint16_t)1);  // la racha si: es el dia del jugador
}

TEST(items, el_caramelo_raro_sube_un_nivel_y_abre_la_evolucion) {
  Pet p;
  makePet(p, 4);  // Charmander: evoluciona a Charmeleon a nivel 16
  setStats(p, 80, 80, 80, 80);
  p.growMin = levelStart(15) + 40;  // nivel 15 (79 min), a poco mas de medio camino del 16
  uint32_t age = p.ageMinutes;
  CHECK_EQ(p.level(), (uint16_t)15);
  CHECK(!p.canEvolveNow());
  CHECK(!p.canUseItem(IT_RARECANDY));  // no hay
  p.items[IT_RARECANDY] = 2;
  CHECK(p.useItem(IT_RARECANDY));
  CHECK_EQ(p.level(), (uint16_t)(15 + RARE_CANDY_LEVELS));
  CHECK_EQ(p.levelMinutesInto(), (uint16_t)(40 * levelCost(16) / levelCost(15)));  // la barra conserva lo que llevaba
  CHECK_EQ(p.ageMinutes, age);  // v1.35: no envejece
  CHECK(p.canEvolveNow());
  CHECK_EQ(p.items[IT_RARECANDY], (uint8_t)1);
  p.toggleLight();
  CHECK(!p.useItem(IT_RARECANDY));  // dormido no
  p.toggleLight();
  p.medals &= ~MED_LV10;  // la medalla de nivel 10 al cruzarlo
  p.growMin = levelStart(9) + 19;  // nivel 9
  CHECK(p.useItem(IT_RARECANDY));
  CHECK_EQ(p.level(), (uint16_t)10);
  CHECK(p.hasMedal(MED_LV10));
  CHECK_EQ(p.items[IT_RARECANDY], (uint8_t)0);
}

TEST(save, metas_y_caramelo_sobreviven_y_un_guardado_de_la_v124_migra) {
  Pet p;
  makeWalker(p);
  mockForceRandom(400);
  p.addSteps(6000, DIA);
  p.items[IT_RARECANDY] = 3;
  p.flushSave();
  Pet b;
  b.begin();
  CHECK_EQ(b.goalTierOn(DIA), (uint8_t)2);
  CHECK_EQ(b.bonusFindsToday, p.bonusFindsToday);
  CHECK_EQ(b.items[IT_RARECANDY], (uint8_t)3);

  // v1.24: una sola meta (goalD) y una mochila de 5 objetos
  {
    Preferences raw;
    raw.begin("tamapoke", false);
    raw.remove("goalN");
    raw.remove("fndB");
    uint8_t old[5] = { 2, 1, 0, 3, 1 };
    raw.putBytes("bpack", old, sizeof(old));
    raw.end();
  }
  Pet c;
  c.begin();
  CHECK_EQ(c.goalTierOn(DIA), (uint8_t)1);  // tenia la meta: cuenta como la primera
  CHECK_EQ(c.bonusFindsToday, (uint8_t)0);
  CHECK_EQ(c.items[IT_GOLDEN], (uint8_t)2);
  CHECK_EQ(c.items[IT_CHARM], (uint8_t)1);
  CHECK_EQ(c.items[IT_RARECANDY], (uint8_t)0);
  c.addSteps(1, DIA);  // 6001: la de 3000 no se repite, la de 6000 ya estaba cruzada
  CHECK_EQ(c.goalTierOn(DIA), (uint8_t)2);
  CHECK_EQ(c.takeGoalBanner(), (uint8_t)2);
}

// ---------------------------------------------------------------- trucos del IMU (Tarea 2)
TEST(imu, agitarlo_dormido_lo_despierta_de_mal_humor) {
  Pet p;
  makeWalker(p);
  setStats(p, 80, 50, 80, 100);
  p.toggleLight();
  uint16_t streak = p.streak;
  p.wakeGrumpy();
  CHECK(!p.sleeping);
  CHECK_EQ(p.joy, (uint8_t)(50 - GRUMPY_JOY));
  CHECK_EQ(p.emote(), (uint8_t)EMOTE_ANGRY);
  CHECK_EQ(p.streak, streak);  // sin credito de cuidado
  mockAdvanceMillis(EMOTE_MS + 1);
  CHECK_EQ(p.emote(), (uint8_t)EMOTE_NONE);
  // otra vez antes de 60 s: no resta mas
  p.sleepDeferred();
  p.wakeGrumpy();
  CHECK_EQ(p.joy, (uint8_t)(50 - GRUMPY_JOY));
  mockAdvanceMillis(GRUMPY_COOLDOWN_MS);
  p.sleepDeferred();
  p.wakeGrumpy();
  CHECK_EQ(p.joy, (uint8_t)(50 - 2 * GRUMPY_JOY));
  p.wakeGrumpy();  // ya despierto: nada
  CHECK_EQ(p.joy, (uint8_t)(50 - 2 * GRUMPY_JOY));
}

TEST(imu, tres_sacudidas_en_dos_minutos_marean_y_restan) {
  Pet p;
  makeWalker(p);
  setStats(p, 80, 50, 80, 100);
  p.shaken();
  CHECK_EQ(p.emote(), (uint8_t)EMOTE_DIZZY);
  mockAdvanceMillis(30000);
  p.shaken();
  CHECK_EQ(p.joy, (uint8_t)50);
  mockAdvanceMillis(30000);
  p.shaken();
  CHECK_EQ(p.joy, (uint8_t)(50 - DIZZY_JOY));
  for (int i = 0; i < 6; i++) { mockAdvanceMillis(20000); p.shaken(); }
  CHECK_MSG(p.joy == 50 - DIZZY_JOY, "como mucho una vez cada 10 minutos");
  mockAdvanceMillis(DIZZY_COOLDOWN_MS);
  p.shaken(); p.shaken(); p.shaken();
  CHECK_EQ(p.joy, (uint8_t)(50 - 2 * DIZZY_JOY));
}

TEST(imu, sacudidas_espaciadas_no_restan) {
  Pet p;
  makeWalker(p);
  setStats(p, 80, 50, 80, 100);
  for (int i = 0; i < 6; i++) { p.shaken(); mockAdvanceMillis(70000); }
  CHECK_EQ(p.joy, (uint8_t)50);
}

TEST(imu, el_huevo_se_tambalea) {
  mockNvsReset();
  Pet p;
  p.begin();
  p.chooseStarter(4);
  p.shaken();
  CHECK_EQ(p.emote(), (uint8_t)EMOTE_WOBBLE);
  CHECK(p.isEgg());
}

TEST(imu, boca_abajo_lo_duerme_sin_guardar_al_momento) {
  Pet p;
  makeWalker(p);
  p.flushSave();
  p.sleepDeferred();
  CHECK(p.sleeping);
  CHECK(p.savePending());
  Pet b;
  b.begin();
  CHECK_MSG(!b.sleeping, "no escribe en flash hasta flushSave()");
  p.flushSave();
  Pet c;
  c.begin();
  CHECK(c.sleeping);
}

TEST(imu, nada_de_esto_en_una_ceremonia) {
  Pet p;
  makeWalker(p);
  setStats(p, 80, 50, 80, 100);
  p.release();
  p.shaken();
  p.sleepDeferred();
  CHECK(!p.sleeping);
  CHECK_EQ(p.emote(), (uint8_t)EMOTE_NONE);
  CHECK_EQ(p.joy, (uint8_t)50);
}

// ---------------------------------------------------------------- combate de practica (Tarea 3a)
TEST(battle, solo_combate_nacido_y_despierto) {
  mockNvsReset();
  Pet egg;
  egg.begin();
  CHECK(!egg.canBattle());  // huevo (y eligiendo inicial)
  egg.chooseStarter(4);
  CHECK(!egg.canBattle());
  Pet p;
  makePet(p, 4);
  CHECK(p.canBattle());
  p.toggleLight();
  CHECK(!p.canBattle());
  p.toggleLight();
  p.release();
  CHECK(!p.canBattle());
}

TEST(battle, la_practica_da_felicidad_una_vez_por_hora) {
  Pet p;
  makePet(p, 25);
  setStats(p, 80, 50, 80, 100);
  CHECK(p.practiceRewardReady());
  CHECK_EQ(p.practiceDone(), (uint8_t)PRACTICE_JOY);
  CHECK_EQ(p.joy, (uint8_t)(50 + PRACTICE_JOY));
  CHECK(!p.practiceRewardReady());
  mockAdvanceMillis(PRACTICE_EVERY_MS - 1000);
  CHECK_EQ(p.practiceDone(), (uint8_t)0);  // otra dentro de la hora: sin premio
  CHECK_EQ(p.joy, (uint8_t)(50 + PRACTICE_JOY));
  mockAdvanceMillis(1000);
  CHECK_EQ(p.practiceDone(), (uint8_t)PRACTICE_JOY);
  CHECK_EQ(p.joy, (uint8_t)(50 + 2 * PRACTICE_JOY));
  p.joy = 99;
  mockAdvanceMillis(PRACTICE_EVERY_MS);
  CHECK_EQ(p.practiceDone(), (uint8_t)1);  // tope de 100: dice lo que de verdad dio
  CHECK_EQ(p.joy, (uint8_t)100);
}

TEST(battle, la_practica_no_toca_energia_ni_comida) {
  Pet p;
  makePet(p, 25);
  setStats(p, 60, 50, 40, 100);
  p.practiceDone();
  CHECK_EQ(p.fullness, (uint8_t)60);
  CHECK_EQ(p.energy, (uint8_t)40);
}

// ---------------------------------------------------------------- efectos (v1.30, fx.h)
// El bicho ya no cronometra ni hace sonar sus celebraciones: deja pendientes que la UI recoge
// y ensena cuando se pueden ver. Asi una medalla ganada con la pantalla apagada no caduca.
TEST(fx_pet, la_medalla_se_recoge_una_vez_y_el_bicho_no_la_hace_sonar) {
  Pet p;
  makePet(p, 4);
  mockSfxReset();
  p.growMin = levelStart(10) - 1;
  advance(p, 1);
  CHECK(p.hasMedal(MED_LV10));
  CHECK_EQ(mockSfxCount(SFX_MEDAL), 0);  // suena al verse el banner, no al ganarla
  mockAdvanceMillis(60UL * 60 * 1000);   // una hora despues sigue pendiente
  CHECK(p.takeMedal() & MED_LV10);       // (y EN FORMA, ganada en el mismo salto)
  CHECK_EQ(p.takeMedal(), (uint16_t)0);
}

TEST(fx_pet, dos_medallas_sin_recoger_se_suman) {
  Pet p;
  makePet(p, 4);
  p.takeMedal();
  p.weight = 0;
  p.growMin = levelStart(5) - 1;
  advance(p, 1);  // nivel 5: EN FORMA
  p.growMin = levelStart(10) - 1;
  advance(p, 1);  // nivel 10, sin haber recogido la anterior
  CHECK_EQ(p.takeMedal(), (uint16_t)(MED_FIT | MED_LV10));
}

TEST(fx_pet, subir_de_nivel_no_suena_en_el_bicho) {
  Pet p;
  makePet(p, 4);
  mockSfxReset();
  uint16_t lv = p.level();
  advance(p, levelCost(lv));
  CHECK_EQ(p.level(), (uint16_t)(lv + 1));
  CHECK_EQ(mockSfxCount(SFX_LEVEL), 0);  // la UI lo detecta y suena SFX_LEVELUP
  CHECK_EQ(mockSfxTotal(), 0);
}

TEST(fx_pet, la_eclosion_no_suena_en_el_bicho) {
  Pet p;
  mockNvsReset();
  mockSetMillis(0);
  p.begin();
  if (p.awaitingStarter()) p.chooseStarter(4);
  mockSfxReset();
  p.eggTap();
  p.eggTap();
  p.eggTap();
  CHECK(!p.isEgg());
  CHECK_EQ(mockSfxCount(SFX_HATCH), 0);  // suena en el destello de la animacion (UI)
}

TEST(fx_pet, el_hito_de_racha_queda_pendiente_hasta_recogerlo) {
  Pet p;
  makePet(p, 4);
  for (uint32_t d = 200; d < 207; d++) {
    p.setClock(d * 86400);
    p.caress();
  }
  CHECK_EQ(p.streak, (uint16_t)7);
  CHECK_EQ(p.takeMilestone(), (uint16_t)7);  // el de 3 dias se piso con el de 7: se ve el ultimo
  CHECK_EQ(p.takeMilestone(), (uint16_t)0);
}

TEST(fx_pet, las_estadisticas_a_un_nivel_cuadran_con_las_de_ahora) {
  Pet p;
  makePet(p, 4);
  p.setLevel(13);
  uint16_t lv = p.level();
  CHECK_EQ(p.statAt(0, lv), p.atkStat());
  CHECK_EQ(p.statAt(1, lv), p.defStat());
  CHECK_EQ(p.statAt(2, lv), p.speStat());
  for (uint8_t w = 0; w < 3; w++) CHECK(p.statAt(w, lv + 6) > p.statAt(w, lv));  // 6 niveles mas: sube
  Pet egg;
  mockNvsReset();
  egg.begin();
  CHECK_EQ(egg.statAt(0, 10), (uint16_t)0);
}
