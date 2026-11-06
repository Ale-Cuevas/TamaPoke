// Motor de combate (battle.h): eficacias de gen 1, danyo con sus topes, el
// especial y su medidor, el orden de turno, el tope de ataques y el rival de
// practica. Incluye un Monte Carlo de los 151x151 a niveles 5, 30 y 100.
#include "framework.h"
#include "shim/Arduino.h"
#include "../battle.h"
#include <vector>

// ---------------------------------------------------------------- helpers
static Fighter plain(int16_t dex, uint16_t level) { return makeFighter(dex, level, 100, 100, 100, 0, 0, 0); }

// combate completo con los dos medidores en manos de la "IA" de practica
static std::vector<BattleEvent> fight(const Fighter &a, const Fighter &b, uint32_t seed, bool specials = true,
                                      Battle *out = nullptr) {
  Battle bt;
  bt.begin(a, b, seed);
  SpecialMeter m[2];
  std::vector<BattleEvent> evs;
  while (!bt.over()) {
    uint8_t who = bt.nextAttacker();
    if (specials)
      for (int i = 0; i < PRACTICE_AI_TAPS; i++) m[who].tap();
    evs.push_back(bt.step(specials && m[who].useForAttack()));
  }
  if (out) *out = bt;
  return evs;
}

// ---------------------------------------------------------------- tipos
TEST(battle, la_tabla_de_tipos_esta_completa) {
  for (int a = 0; a < TY_COUNT; a++) {
    CHECK_EQ((int)strlen(TYPE_CHART[a]), (int)TY_COUNT);
    for (int d = 0; d < TY_COUNT; d++) CHECK(strchr(".+-0", TYPE_CHART[a][d]) != nullptr);
  }
}

TEST(battle, eficacias_de_gen_1) {
  CHECK_EQ(typeEff4(TY_AGUA, TY_FUEGO, TY_NONE), (uint8_t)8);       // agua -> fuego x2
  CHECK_EQ(typeEff4(TY_FUEGO, TY_AGUA, TY_NONE), (uint8_t)2);       // y al reves x0,5
  CHECK_EQ(typeEff4(TY_ELECTRICO, TY_TIERRA, TY_NONE), (uint8_t)0); // no le afecta
  CHECK_EQ(typeEff4(TY_NORMAL, TY_FANTASMA, TY_NONE), (uint8_t)0);
  CHECK_EQ(typeEff4(TY_FANTASMA, TY_PSIQUICO, TY_NONE), (uint8_t)0);  // la rareza de gen 1
  CHECK_EQ(typeEff4(TY_HIELO, TY_FUEGO, TY_NONE), (uint8_t)4);      // en gen 1, normal
  CHECK_EQ(typeEff4(TY_BICHO, TY_VENENO, TY_NONE), (uint8_t)8);
  CHECK_EQ(typeEff4(TY_VENENO, TY_BICHO, TY_NONE), (uint8_t)8);
  CHECK_EQ(typeEff4(TY_PSIQUICO, TY_LUCHA, TY_NONE), (uint8_t)8);
  CHECK_EQ(typeEff4(TY_DRAGON, TY_DRAGON, TY_NONE), (uint8_t)8);
  CHECK_EQ(typeEff4(TY_NORMAL, TY_NORMAL, TY_NONE), (uint8_t)4);
}

TEST(battle, los_dos_tipos_se_multiplican) {
  const DexEntry &gyarados = DEX_TBL[130], &charizard = DEX_TBL[6], &dragonite = DEX_TBL[149];
  CHECK_EQ(typeEff4(TY_ELECTRICO, gyarados.type1, gyarados.type2), (uint8_t)16);  // agua/volador: x4
  CHECK_EQ(typeEff4(TY_PLANTA, charizard.type1, charizard.type2), (uint8_t)1);    // fuego/volador: x0,25
  CHECK_EQ(typeEff4(TY_TIERRA, charizard.type1, charizard.type2), (uint8_t)0);    // el volador no se toca
  CHECK_EQ(typeEff4(TY_HIELO, dragonite.type1, dragonite.type2), (uint8_t)16);    // dragon/volador: x4
  CHECK_EQ(typeEff4(TY_ROCA, charizard.type1, charizard.type2), (uint8_t)16);
}

// ---------------------------------------------------------------- stats
TEST(battle, stats_de_la_ficha_con_el_nivel_topado) {
  Fighter p = makeFighter(25, 30, 104, 97, 108, 6, 2, 9);  // Pikachu
  const DexEntry &d = DEX_TBL[25];
  CHECK_EQ(p.atk, (uint16_t)(d.bAtk * 104 / 100 + 30 + 6));
  CHECK_EQ(p.def, (uint16_t)(d.bDef * 97 / 100 + 30 + 2));
  CHECK_EQ(p.spe, (uint16_t)(d.bSpe * 108 / 100 + 30 + 9));
  CHECK_EQ(p.maxHp, (uint16_t)(d.bHp + 30 + 10));
  CHECK_EQ(p.type1, (uint8_t)TY_ELECTRICO);
  Fighter old = makeFighter(25, 999, 100, 100, 100, 0, 0, 0);
  CHECK_EQ(old.level, (uint8_t)BATTLE_LEVEL_CAP);
  CHECK_EQ(old.maxHp, (uint16_t)(d.bHp + BATTLE_LEVEL_CAP + 10));
  CHECK_EQ(makeFighter(0, 5, 100, 100, 100, 0, 0, 0).dex, (int16_t)1);    // fuera de rango: no indexa basura
  CHECK_EQ(makeFighter(400, 5, 100, 100, 100, 0, 0, 0).dex, (int16_t)1);
}

// ---------------------------------------------------------------- danyo
TEST(battle, cada_golpe_entre_el_5_y_el_25_por_ciento) {
  for (int L : { 5, 30, 100 })
    for (int a = 1; a <= 151; a += 7)
      for (int b = 1; b <= 151; b += 5) {
        Fighter A = plain(a, L), B = plain(b, L);
        Battle bt;
        bt.begin(A, B, a * 1000 + b);
        while (!bt.over()) {
          uint8_t who = bt.nextAttacker();
          const Fighter &def = bt.f[1 - who];
          uint16_t before = def.hp;
          bool sp = (bt.attacks % 3) == 2;
          BattleEvent ev = bt.step(sp);
          if (!ev.eff4) { CHECK_EQ(ev.damage, (uint16_t)0); continue; }
          uint32_t lo = def.maxHp * HIT_MIN_PCT / 100, hi = def.maxHp * (sp ? SPECIAL_MAX_PCT : HIT_MAX_PCT) / 100;
          if (lo < 1) lo = 1;
          if (ev.damage < before) CHECK_RANGE((uint32_t)ev.damage, lo, hi);  // el golpe final puede quedarse corto
        }
      }
}

TEST(battle, el_especial_pega_mas_y_sube_el_tope) {
  // mismo combate y misma semilla: solo cambia que el primer ataque sea especial
  Fighter A = plain(150, 100), B = plain(129, 100);  // Mewtwo contra Magikarp: siempre al tope
  Battle n, s;
  n.begin(A, B, 42);
  s.begin(A, B, 42);
  CHECK_EQ(n.nextAttacker(), (uint8_t)0);  // Mewtwo es mas rapido
  BattleEvent en = n.step(false), es = s.step(true);
  CHECK(es.special && !en.special);
  CHECK_EQ(en.damage, (uint16_t)(B.maxHp * HIT_MAX_PCT / 100));
  CHECK_EQ(es.damage, (uint16_t)(B.maxHp * SPECIAL_MAX_PCT / 100));
  // sin tope de por medio, el especial es ~30 % mas fuerte
  Fighter C = plain(19, 30), D = plain(95, 30);  // Rattata contra Onix (roca: x0,5)
  Battle n2, s2;
  n2.begin(C, D, 7);
  s2.begin(C, D, 7);
  if (n2.nextAttacker() != 0) { n2.step(false); s2.step(false); }
  BattleEvent a = n2.step(false), b = s2.step(true);
  CHECK(b.damage >= a.damage);
}

TEST(battle, no_le_afecta_es_cero_y_sin_critico) {
  Fighter A = plain(25, 40), B = plain(50, 40);  // Pikachu contra Diglett (tierra)
  Battle bt;
  bt.begin(A, B, 3);
  bool vioUno = false;
  while (!bt.over()) {
    BattleEvent ev = bt.step(false);
    if (ev.attacker == 0) {
      CHECK_EQ(ev.eff4, (uint8_t)0);
      CHECK_EQ(ev.damage, (uint16_t)0);
      CHECK(!ev.crit);
      vioUno = true;
    }
  }
  CHECK(vioUno);
  CHECK_EQ(bt.result, (uint8_t)BR_WIN_B);  // Diglett gana sin despeinarse
}

TEST(battle, inmunes_los_dos_acaba_en_empate_al_tope) {
  Fighter A = plain(19, 20), B = plain(92, 20);  // Rattata (normal) contra Gastly (fantasma/veneno)
  CHECK_EQ(typeEff4(A.type1, B.type1, B.type2), (uint8_t)0);
  CHECK_EQ(typeEff4(B.type1, A.type1, A.type2), (uint8_t)0);
  Battle bt;
  auto evs = fight(A, B, 11, true, &bt);
  CHECK_EQ((int)evs.size(), MAX_ATTACKS);
  CHECK_EQ(bt.result, (uint8_t)BR_DRAW);
}

TEST(battle, al_tope_gana_quien_conserva_mas_porcentaje) {
  // dos Chansey a nivel 5: mucha vida, cada golpe al minimo del 5 %, se llega a los 15
  Battle bt;
  auto evs = fight(plain(113, 5), makeFighter(113, 5, 100, 100, 100, 0, 9, 0), 5, false, &bt);
  CHECK_EQ((int)evs.size(), MAX_ATTACKS);
  const BattleEvent &last = evs.back();
  uint32_t pa = (uint32_t)last.hp[0] * bt.f[1].maxHp, pb = (uint32_t)last.hp[1] * bt.f[0].maxHp;
  CHECK(last.hp[0] > 0 && last.hp[1] > 0);
  CHECK_EQ(bt.result, (uint8_t)(pa > pb ? BR_WIN_A : pb > pa ? BR_WIN_B : BR_DRAW));
  CHECK_EQ(last.result, bt.result);
}

// ---------------------------------------------------------------- turnos
TEST(battle, el_mas_rapido_abre_cada_ronda) {
  Fighter fast = plain(101, 50), slow = plain(79, 50);  // Electrode contra Slowpoke
  CHECK(fast.spe > slow.spe);
  auto evs = fight(slow, fast, 9, false);
  for (size_t i = 0; i < evs.size(); i++) CHECK_EQ(evs[i].attacker, (uint8_t)(i % 2 == 0 ? 1 : 0));
}

TEST(battle, un_empate_de_velocidad_se_sortea) {
  Fighter a = plain(25, 30), b = plain(25, 30);
  int abreA = 0, abreB = 0;
  for (uint32_t s = 1; s <= 200; s++) {
    Battle bt;
    bt.begin(a, b, s);
    (bt.nextAttacker() == 0 ? abreA : abreB)++;
  }
  CHECK(abreA > 50 && abreB > 50);
}

TEST(battle, misma_semilla_mismo_combate) {
  Fighter A = makeFighter(6, 40, 104, 99, 101, 10, 5, 12), B = makeFighter(130, 41, 100, 100, 100, 9, 9, 9);
  auto x = fight(A, B, 1234), y = fight(A, B, 1234), z = fight(A, B, 4321);
  CHECK_EQ(x.size(), y.size());
  bool igual = x.size() == z.size();
  for (size_t i = 0; i < x.size() && i < y.size(); i++) {
    CHECK_EQ(x[i].attacker, y[i].attacker);
    CHECK_EQ(x[i].damage, y[i].damage);
    CHECK_EQ(x[i].crit, y[i].crit);
    CHECK_EQ(x[i].hp[0], y[i].hp[0]);
    CHECK_EQ(x[i].hp[1], y[i].hp[1]);
    if (i < z.size() && (x[i].damage != z[i].damage || x[i].crit != z[i].crit)) igual = false;
  }
  CHECK_MSG(!igual, "otra semilla deberia dar otro combate");
}

TEST(battle, despues_de_acabar_step_no_cambia_nada) {
  Battle bt;
  fight(plain(150, 100), plain(129, 5), 1, true, &bt);
  CHECK(bt.over());
  uint16_t h0 = bt.f[0].hp, h1 = bt.f[1].hp;
  uint8_t n = bt.attacks;
  BattleEvent ev = bt.step(true);
  CHECK_EQ(ev.result, bt.result);
  CHECK_EQ(bt.f[0].hp, h0);
  CHECK_EQ(bt.f[1].hp, h1);
  CHECK_EQ(bt.attacks, n);
}

// ---------------------------------------------------------------- medidor
TEST(battle, cinco_toques_cargan_el_especial) {
  SpecialMeter m;
  for (int i = 0; i < SPECIAL_TAPS - 1; i++) CHECK(m.tap());
  CHECK(!m.armed());
  CHECK(m.tap());
  CHECK(m.armed());
  CHECK(!m.tap());  // lleno: el sexto no cuenta
  CHECK(m.useForAttack());
  CHECK(!m.armed());
}

TEST(battle, tras_el_especial_dos_ataques_normales_de_espera) {
  SpecialMeter m;
  for (int i = 0; i < SPECIAL_TAPS; i++) m.tap();
  CHECK(m.useForAttack());
  CHECK(m.locked());
  CHECK(!m.tap());  // durante la espera, los toques se pierden
  CHECK_EQ(m.pips, (uint8_t)0);
  CHECK(!m.useForAttack());  // 1er ataque normal
  CHECK(!m.tap());
  CHECK(!m.useForAttack());  // 2o
  CHECK(!m.locked());
  CHECK(m.tap());            // ya carga otra vez
  CHECK_EQ(m.pips, (uint8_t)1);
}

TEST(battle, sin_cargar_el_ataque_es_normal) {
  SpecialMeter m;
  m.tap();
  m.tap();
  CHECK(!m.useForAttack());
  CHECK_EQ(m.pips, (uint8_t)2);  // lo cargado se guarda para el siguiente
}

// ---------------------------------------------------------------- Monte Carlo
// FR-3.3 / FR-3.4: todos los cruces a 3 niveles. Cada combate dura de 7 a 15
// ataques; el claramente mas fuerte (sin ventaja de tipo de por medio) gana casi
// siempre, y la ventaja de tipo pesa entre bichos parecidos.
TEST(battle, monte_carlo_151x151_duracion_y_justicia) {
  for (int L : { 5, 30, 100 }) {
    long fuerteN = 0, fuerteGana = 0, tipoN = 0, tipoGana = 0, cortos = 0, largos = 0;
    for (int a = 1; a <= 151; a++)
      for (int b = 1; b <= 151; b++) {
        Fighter A = plain(a, L), B = plain(b, L);
        Battle bt;
        auto evs = fight(A, B, (uint32_t)(a * 389 + b * 7 + L), true, &bt);
        int n = (int)evs.size();
        if (n < 7) cortos++;
        if (n > MAX_ATTACKS) largos++;
        // el que cae recibe al menos 4 golpes (el tope del 25-30 %)
        if (bt.result == BR_WIN_A || bt.result == BR_WIN_B) {
          uint8_t loser = bt.result == BR_WIN_A ? 1 : 0;
          int golpes = 0;
          for (auto &e : evs) golpes += (e.attacker != loser);
          CHECK(golpes >= 4);
        }
        int pa = A.maxHp + A.atk + A.def + A.spe, pb = B.maxHp + B.atk + B.def + B.spe;
        uint8_t eab = typeEff4(A.type1, B.type1, B.type2), eba = typeEff4(B.type1, A.type1, A.type2);
        if (eab == 4 && eba == 4 && pa * 100 >= pb * 120) {
          fuerteN++;
          fuerteGana += bt.result == BR_WIN_A;
        }
        if (eab >= 8 && eba <= 4 && pa * 100 >= pb * 90 && pa * 100 <= pb * 110) {
          tipoN++;
          tipoGana += bt.result == BR_WIN_A;
        }
      }
    char msg[160];
    snprintf(msg, sizeof(msg), "L%d: %ld combates de menos de 7 ataques, %ld de mas de 15", L, cortos, largos);
    CHECK_MSG(cortos == 0 && largos == 0, msg);
    snprintf(msg, sizeof(msg), "L%d: el mas fuerte gana %ld de %ld", L, fuerteGana, fuerteN);
    CHECK_MSG(fuerteN > 1000 && fuerteGana * 100 >= fuerteN * 90, msg);
    snprintf(msg, sizeof(msg), "L%d: la ventaja de tipo gana %ld de %ld", L, tipoGana, tipoN);
    CHECK_MSG(tipoN > 500 && tipoGana * 100 >= tipoN * 85, msg);
  }
}

TEST(battle, el_entrenamiento_y_los_genes_cuentan) {
  // el mismo bicho, uno a tope de entrenamiento: gana la gran mayoria
  int gana = 0;
  for (uint32_t s = 1; s <= 200; s++) {
    Battle bt;
    fight(makeFighter(4, 30, 110, 110, 110, 40, 40, 40), plain(4, 30), s, true, &bt);
    gana += bt.result == BR_WIN_A;
  }
  CHECK(gana >= 180);
}

// ---------------------------------------------------------------- rival de practica
TEST(battle, el_rival_de_practica_es_de_tu_rareza_y_tu_nivel) {
  uint32_t rng = 99;
  for (int16_t mine : { 1, 6, 25, 58, 130, 133, 136, 144, 150 }) {
    uint8_t rar = DEX_TBL[DEX_TBL[mine].base].rarity;
    for (int i = 0; i < 50; i++) {
      PracticeFoe f = pickPracticeFoe(mine, 30, rng);
      CHECK_RANGE(f.dex, (int16_t)1, (int16_t)151);
      CHECK_EQ(DEX_TBL[DEX_TBL[f.dex].base].rarity, rar);
      CHECK_RANGE((int)f.level, 27, 33);
    }
  }
  PracticeFoe lo = pickPracticeFoe(25, 1, rng);
  CHECK_RANGE((int)lo.level, 1, 4);
  PracticeFoe hi = pickPracticeFoe(25, 999, rng);
  CHECK_RANGE((int)hi.level, 97, 100);
}

TEST(battle, el_rival_evoluciona_segun_su_nivel) {
  uint32_t rng = 5;
  CHECK_EQ(evolveForLevel(4, 10, rng), (int16_t)4);   // Charmander
  CHECK_EQ(evolveForLevel(4, 16, rng), (int16_t)5);   // Charmeleon
  CHECK_EQ(evolveForLevel(4, 36, rng), (int16_t)6);   // Charizard
  CHECK_EQ(evolveForLevel(150, 100, rng), (int16_t)150);
  bool ramas[3] = { false, false, false };
  for (int i = 0; i < 60; i++) {
    int16_t e = evolveForLevel(DEX_EEVEE, 40, rng);
    CHECK_RANGE(e, (int16_t)134, (int16_t)136);
    ramas[e - 134] = true;
  }
  CHECK(ramas[0] && ramas[1] && ramas[2]);
  CHECK_EQ(evolveForLevel(DEX_EEVEE, 20, rng), (int16_t)DEX_EEVEE);
}
