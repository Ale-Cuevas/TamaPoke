// Fases de combate (stage.h) y el modo fase del motor (Battle::beginCarry): la
// vida heredada y sin empates, la tirada de rivales contra la tabla, el relevo
// de aliados y de rivales sin curar a nadie, los medidores, la semilla, y un
// informe de balance (tasas de superacion por fase) que se imprime para afinar.
#include "framework.h"
#include "shim/Arduino.h"
#include "../stage.h"
#include <cstdio>
#include <vector>

// ---------------------------------------------------------------- helpers
static Fighter plain(int16_t dex, uint16_t level) { return makeFighter(dex, level, 100, 100, 100, 0, 0, 0); }

// lo que paso en una fase, ataque a ataque
struct StageLog {
  std::vector<BattleEvent> evs;
  std::vector<uint8_t> foeAt;  // foeIdx durante cada ataque
  std::vector<uint8_t> allyAt; // ally durante cada ataque
  std::vector<uint8_t> steps;  // lo que fue devolviendo advance()
};

// una fase entera como la moveria la pantalla; tapFull = cargas el medidor
// siempre que se deja (el mejor caso), si no, no tocas nunca
static uint8_t playStage(StageRun &run, bool tapFull, StageLog *log = nullptr) {
  for (int guard = 0; guard < 2000 && !run.over(); guard++) {
    if (run.koPending()) {
      uint8_t r = run.advance();
      if (log) log->steps.push_back(r);
      continue;
    }
    if (tapFull && run.nextAttacker() == 0)
      for (int i = 0; i < SPECIAL_TAPS; i++) run.tap();
    uint8_t f = run.foeIdx, a = run.ally;
    BattleEvent ev = run.attack();
    if (log) {
      log->evs.push_back(ev);
      log->foeAt.push_back(f);
      log->allyAt.push_back(a);
    }
  }
  return run.state;
}

static uint8_t runStage(uint8_t stage, const Fighter *team, uint8_t n, uint32_t seed, bool tapFull,
                        StageLog *log = nullptr, StageRun *out = nullptr) {
  StageRun run;
  run.begin(stage, team, n, seed);
  uint8_t r = playStage(run, tapFull, log);
  if (out) *out = run;
  return r;
}

// ---------------------------------------------------------------- modo fase del motor
TEST(stage, carry_conserva_la_vida_que_trae) {
  Fighter a = plain(25, 30), b = plain(74, 30);
  a.hp = 10;
  b.hp = 0;  // fallo de quien llama: entra con 1
  Battle bt;
  bt.beginCarry(a, b, 7);
  CHECK_EQ(bt.f[0].hp, (uint16_t)10);
  CHECK_EQ(bt.f[1].hp, (uint16_t)1);
  CHECK(bt.carry);
  a.hp = 9999;  // por encima del maximo: se topa
  bt.beginCarry(a, b, 7);
  CHECK_EQ(bt.f[0].hp, a.maxHp);
  // begin de siempre: vida llena y fuera del modo fase
  bt.begin(a, b, 7);
  CHECK(!bt.carry);
  CHECK_EQ(bt.f[0].hp, a.maxHp);
  CHECK_EQ(bt.f[1].hp, b.maxHp);
}

TEST(stage, carry_con_vida_llena_es_el_mismo_combate_hasta_el_tope) {
  // misma semilla, vida llena: los mismos golpes que begin (la misma secuencia de azar)
  Fighter A = makeFighter(6, 40, 104, 99, 101, 10, 5, 12), B = makeFighter(130, 41, 100, 100, 100, 9, 9, 9);
  Battle n, c;
  n.begin(A, B, 1234);
  c.beginCarry(A, B, 1234);
  while (!n.over()) {
    BattleEvent en = n.step(n.attacks % 4 == 3), ec = c.step(c.attacks % 4 == 3);
    CHECK_EQ(en.damage, ec.damage);
    CHECK_EQ(en.attacker, ec.attacker);
    CHECK_EQ(en.crit, ec.crit);
  }
}

TEST(stage, carry_nunca_empata_y_pasa_del_tope) {
  // los dos Chansey que en practica llegan a los 15 ataques: aqui siguen hasta el KO
  Battle bt;
  bt.beginCarry(plain(113, 5), makeFighter(113, 5, 100, 100, 100, 0, 9, 0), 5);
  while (!bt.over()) bt.step(false);
  CHECK(bt.attacks > MAX_ATTACKS);
  CHECK(bt.result == BR_WIN_A || bt.result == BR_WIN_B);
  CHECK(bt.f[0].hp == 0 || bt.f[1].hp == 0);
  // y en todos los cruces (muestreados): KO siempre, en menos de 160 ataques
  int maxAtk = 0;
  for (int a = 1; a <= 151; a += 3)
    for (int b = 1; b <= 151; b += 4)
      for (int L : { 5, 60 }) {
        Battle x;
        x.beginCarry(plain(a, L), plain(b, L), a * 151 + b + L);
        while (!x.over() && x.attacks < 200) x.step(x.attacks % 3 == 2);
        CHECK(x.result == BR_WIN_A || x.result == BR_WIN_B);
        CHECK(x.f[0].hp == 0 || x.f[1].hp == 0);
        if (x.attacks > maxAtk) maxAtk = x.attacks;
      }
  char msg[64];
  snprintf(msg, sizeof(msg), "el mas largo: %d ataques", maxAtk);
  CHECK_MSG(maxAtk < 160, msg);
}

TEST(stage, carry_inmunes_los_dos_se_rozan_hasta_el_ko) {
  Fighter A = plain(19, 20), B = plain(92, 20);  // Rattata (normal) contra un fantasma puro (a mano:
  B.type1 = TY_FANTASMA;                         // el veneno de Gastly si toca al normal)
  B.type2 = TY_NONE;
  Battle bt;
  bt.beginCarry(A, B, 11);
  while (!bt.over() && bt.attacks < 200) {
    BattleEvent ev = bt.step(false);
    CHECK_EQ(ev.eff4, (uint8_t)1);  // x0,25: poco eficaz, nunca "no le afecta"
    CHECK(ev.damage > 0);
  }
  CHECK(bt.result == BR_WIN_A || bt.result == BR_WIN_B);
  // con solo uno inmune, el otro sigue sin hacer nada (Pikachu contra Diglett)
  Battle p;
  p.beginCarry(plain(25, 40), plain(50, 40), 3);
  while (!p.over()) {
    BattleEvent ev = p.step(false);
    if (ev.attacker == 0) CHECK_EQ(ev.damage, (uint16_t)0);
  }
  CHECK_EQ(p.result, (uint8_t)BR_WIN_B);
}

// ---------------------------------------------------------------- la tabla
TEST(stage, la_tabla_de_fases_cuadra_con_la_dex) {
  static const char *const BOSS[STAGE_COUNT] = { "IVYSAUR",    "WARTORTLE", "GOLEM",   "ARCANINE",
                                                 "ELECTABUZZ", "GENGAR",    "NIDOKING", "DRAGONITE" };
  for (int s = 0; s < STAGE_COUNT; s++) {
    const StageDef &d = STAGES[s];
    CHECK_STREQ(DEX_TBL[d.bossDex].name, BOSS[s]);
    CHECK(d.lvMin <= d.lvMax && d.trMin <= d.trMax);
    CHECK_RANGE((int)d.poolN, 2, STAGE_POOL_MAX);
    if (s) CHECK(d.lvMax >= STAGES[s - 1].lvMax);  // cada fase pide al menos lo de la anterior
    // el jefe y cada esbirro tienen el tipo del tema (salvo el relleno del dragon)
    for (int i = -1; i < d.poolN; i++) {
      int16_t dex = i < 0 ? d.bossDex : d.pool[i];
      CHECK_RANGE(dex, (int16_t)1, (int16_t)DEX_COUNT);
      bool pad = s == 7 && (dex == 130 || dex == 142 || dex == 6);  // Gyarados, Aerodactyl, Charizard
      const DexEntry &e = DEX_TBL[dex];
      bool theme = e.type1 == d.theme1 || e.type2 == d.theme1 ||
                   (d.theme2 != TY_NONE && (e.type1 == d.theme2 || e.type2 == d.theme2));
      char msg[64];
      snprintf(msg, sizeof(msg), "fase %d: #%d %s no es del tema", s, dex, e.name);
      CHECK_MSG(theme || pad, msg);
      for (int j = 0; j < i; j++) CHECK(d.pool[j] != dex);  // sin repetidos
    }
  }
}

TEST(stage, la_tirada_respeta_la_tabla_en_todas_las_fases) {
  for (uint8_t s = 0; s < STAGE_COUNT; s++) {
    const StageDef &d = STAGES[s];
    bool visto[STAGE_POOL_MAX] = {};
    bool lvLo = false, lvHi = false, trLo = false, trHi = false;
    uint32_t rng = 1000 + s;
    for (int i = 0; i < 3000; i++) {
      StageFoe f[STAGE_FOES];
      stageRollFoes(s, rng, f);
      CHECK(f[0].dex != f[1].dex);
      for (int m = 0; m < 2; m++) {
        int k = -1;
        for (int j = 0; j < d.poolN; j++)
          if (d.pool[j] == f[m].dex) k = j;
        CHECK(k >= 0);
        if (k >= 0) visto[k] = true;
        CHECK_RANGE(f[m].level, d.lvMin, d.lvMax);
        for (uint8_t t : { f[m].tAtk, f[m].tDef, f[m].tSpe }) {
          CHECK_RANGE(t, d.trMin, d.trMax);
          trLo |= t == d.trMin;
          trHi |= t == d.trMax;
        }
        lvLo |= f[m].level == d.lvMin;
        lvHi |= f[m].level == d.lvMax;
        CHECK_EQ(f[m].aiTaps, (uint8_t)m);
      }
      CHECK_EQ(f[2].dex, d.bossDex);
      CHECK_EQ(f[2].level, d.lvMax);
      CHECK_EQ(f[2].tAtk, d.bossTr);
      CHECK_EQ(f[2].tDef, d.bossTr);
      CHECK_EQ(f[2].tSpe, d.bossTr);
      CHECK_EQ(f[2].aiTaps, (uint8_t)PRACTICE_AI_TAPS);
    }
    CHECK(lvLo && lvHi && trLo && trHi);  // los extremos del rango tambien salen
    for (int j = 0; j < d.poolN; j++) CHECK(visto[j]);
  }
  // las tres stats de un esbirro no van atadas
  uint32_t rng = 5;
  int distintas = 0;
  for (int i = 0; i < 200; i++) {
    StageFoe f[STAGE_FOES];
    stageRollFoes(0, rng, f);
    distintas += !(f[0].tAtk == f[0].tDef && f[0].tDef == f[0].tSpe);
  }
  CHECK(distintas > 150);
  // el Fighter del rival: genes 100 y la forma tal cual (sin evolucionar)
  StageFoe e;
  e.dex = 1;
  e.level = 20;
  e.tAtk = 20, e.tDef = 30, e.tSpe = 35;
  Fighter x = stageFoeFighter(e);
  Fighter y = makeFighter(1, 20, 100, 100, 100, 20, 30, 35);
  CHECK_EQ(x.dex, (int16_t)1);
  CHECK_EQ(x.atk, y.atk);
  CHECK_EQ(x.def, y.def);
  CHECK_EQ(x.spe, y.spe);
  CHECK_EQ(x.maxHp, y.maxHp);
}

// ---------------------------------------------------------------- la fase
TEST(stage, si_cae_el_aliado_el_rival_sigue_con_su_vida) {
  Fighter team[2] = { plain(129, 5), plain(150, 100) };  // un Magikarp de cebo y Mewtwo detras
  StageRun run;
  run.begin(0, team, 2, 3);
  CHECK_EQ(run.eng.f[0].hp, run.team[0].maxHp);
  CHECK_EQ(run.eng.f[1].hp, run.eng.f[1].maxHp);
  while (!run.koPending()) run.attack();
  CHECK_EQ(run.eng.result, (uint8_t)BR_WIN_B);
  uint16_t foeHp = run.eng.f[1].hp;
  CHECK(foeHp > 0 && foeHp <= run.eng.f[1].maxHp);
  CHECK_EQ(run.advance(), (uint8_t)SN_NEXT_ALLY);
  CHECK_EQ(run.ally, (uint8_t)1);
  CHECK_EQ(run.foeIdx, (uint8_t)0);
  CHECK_EQ(run.eng.f[1].hp, foeHp);  // sin curarse
  CHECK_EQ(run.foe.hp, foeHp);
  CHECK_EQ(run.eng.f[0].dex, (int16_t)150);
  CHECK_EQ(run.eng.f[0].hp, run.team[1].maxHp);
  CHECK_EQ(run.allyHp(0), (uint16_t)0);
  CHECK_EQ(run.allyHp(1), run.team[1].maxHp);
  CHECK(!run.over());
}

TEST(stage, si_cae_el_rival_tu_aliado_sigue_con_su_vida) {
  Fighter team[1] = { makeFighter(3, 40, 100, 100, 100, 20, 20, 20) };  // Venusaur
  StageRun run;
  run.begin(0, team, 1, 8);
  while (!run.koPending()) run.attack();
  CHECK_EQ(run.eng.result, (uint8_t)BR_WIN_A);
  uint16_t myHp = run.eng.f[0].hp;
  CHECK(myHp > 0 && myHp < run.team[0].maxHp);  // le dieron algun golpe
  CHECK_EQ(run.allyHp(0), myHp);
  CHECK_EQ(run.advance(), (uint8_t)SN_NEXT_FOE);
  CHECK_EQ(run.foeIdx, (uint8_t)1);
  CHECK_EQ(run.ally, (uint8_t)0);
  CHECK_EQ(run.eng.f[0].hp, myHp);
  CHECK_EQ(run.eng.f[1].dex, run.foes[1].dex);
  CHECK_EQ(run.eng.f[1].hp, run.eng.f[1].maxHp);  // el nuevo rival, entero
  CHECK(!run.isBoss());
}

TEST(stage, la_vida_nunca_se_recupera_dentro_de_la_fase) {
  Fighter team[4] = { plain(16, 25), plain(66, 25), plain(7, 25), plain(25, 25) };
  for (uint32_t seed = 1; seed <= 300; seed++) {
    StageRun run;
    run.begin(seed % STAGE_COUNT, team, 4, seed);
    uint16_t last[TEAM_MAX];
    for (int i = 0; i < 4; i++) last[i] = run.allyHp(i);
    for (int guard = 0; guard < 2000 && !run.over(); guard++) {
      if (run.koPending()) {
        uint16_t foeHp = run.eng.f[1].hp;
        uint8_t r = run.advance();
        if (r == SN_NEXT_ALLY) CHECK_EQ(run.eng.f[1].hp, foeHp);
        if (r == SN_NEXT_FOE) CHECK_EQ(run.eng.f[0].hp, last[run.ally]);
      } else {
        if (seed & 1) run.tap();
        run.attack();
      }
      for (int i = 0; i < 4; i++) {
        CHECK(run.allyHp(i) <= last[i]);
        last[i] = run.allyHp(i);
      }
      CHECK_EQ(run.eng.f[0].hp, run.allyHp(run.ally));  // la tira y el campo dicen lo mismo
    }
    CHECK(run.over());
    for (int i = 0; i < 4; i++) {
      if (i < run.ally) CHECK_EQ(run.allyHp(i), (uint16_t)0);  // los de antes cayeron
      if (i > run.ally) CHECK_EQ(run.allyHp(i), run.team[i].maxHp);  // los de detras no pelearon
    }
  }
}

TEST(stage, los_medidores_se_vacian_al_entrar_otro) {
  // cambio de rival: el tuyo se queda, el del rival se vacia
  Fighter strong[2] = { plain(150, 100), plain(150, 100) };
  StageRun run;
  run.begin(0, strong, 2, 4);
  while (!run.koPending()) run.attack();
  CHECK_EQ(run.eng.result, (uint8_t)BR_WIN_A);
  CHECK(!run.tap());  // con el KO pendiente, el toque no cuenta
  run.meter.pips = 3;
  run.foeMeter.pips = 4;
  run.foeMeter.cooldown = 1;
  CHECK_EQ(run.advance(), (uint8_t)SN_NEXT_FOE);
  CHECK_EQ(run.meter.pips, (uint8_t)3);
  CHECK_EQ(run.foeMeter.pips, (uint8_t)0);
  CHECK_EQ(run.foeMeter.cooldown, (uint8_t)0);
  // cambio de aliado: el tuyo se vacia, el del rival se queda
  Fighter weak[2] = { plain(129, 5), plain(129, 5) };
  run.begin(7, weak, 2, 4);
  while (!run.koPending()) run.attack();
  CHECK_EQ(run.eng.result, (uint8_t)BR_WIN_B);
  run.meter.pips = 3;
  run.meter.cooldown = 2;
  run.foeMeter.pips = 4;
  CHECK_EQ(run.advance(), (uint8_t)SN_NEXT_ALLY);
  CHECK_EQ(run.meter.pips, (uint8_t)0);
  CHECK_EQ(run.meter.cooldown, (uint8_t)0);
  CHECK_EQ(run.foeMeter.pips, (uint8_t)4);
}

TEST(stage, tus_toques_cargan_tu_especial) {
  Fighter team[1] = { plain(150, 100) };
  StageRun run;
  run.begin(0, team, 1, 2);
  for (int i = 0; i < SPECIAL_TAPS; i++) CHECK(run.tap());
  CHECK(!run.tap());  // lleno
  while (run.nextAttacker() != 0) run.attack();
  BattleEvent ev = run.attack();
  CHECK_EQ(ev.attacker, (uint8_t)0);
  CHECK(ev.special);
  CHECK(run.meter.locked());
}

TEST(stage, el_primer_rival_no_usa_especial_y_el_jefe_si) {
  long foeAtk[STAGE_FOES] = {}, foeSp[STAGE_FOES] = {}, mineSp = 0;
  Fighter team[4] = { plain(3, 30), plain(6, 36), plain(9, 36), plain(26, 30) };
  for (uint32_t seed = 1; seed <= 400; seed++) {
    StageLog log;
    runStage(0, team, 4, seed, false, &log);
    for (size_t i = 0; i < log.evs.size(); i++) {
      const BattleEvent &e = log.evs[i];
      if (e.attacker == 1) {
        foeAtk[log.foeAt[i]]++;
        foeSp[log.foeAt[i]] += e.special;
      } else {
        mineSp += e.special;
      }
    }
  }
  CHECK_EQ(mineSp, 0L);  // sin tocar, nunca
  CHECK_EQ(foeSp[0], 0L);
  CHECK(foeAtk[2] > 200);
  CHECK(foeSp[2] > 0);
  // el segundo, con 1 toque por ataque, menos a menudo que el jefe con 2
  char msg[96];
  snprintf(msg, sizeof(msg), "especiales 2o %ld/%ld, jefe %ld/%ld", foeSp[1], foeAtk[1], foeSp[2], foeAtk[2]);
  CHECK_MSG(foeSp[1] * foeAtk[2] < foeSp[2] * foeAtk[1], msg);
}

TEST(stage, misma_semilla_misma_fase) {
  Fighter team[3] = { plain(16, 30), plain(66, 30), plain(7, 30) };
  StageLog x, y, z;
  StageRun rx, ry, rz;
  runStage(1, team, 3, 777, true, &x, &rx);
  runStage(1, team, 3, 777, true, &y, &ry);
  runStage(1, team, 3, 778, true, &z, &rz);
  CHECK_EQ(rx.state, ry.state);
  CHECK_EQ(x.evs.size(), y.evs.size());
  CHECK(x.steps == y.steps);
  for (int i = 0; i < STAGE_FOES; i++) {
    CHECK_EQ(rx.foes[i].dex, ry.foes[i].dex);
    CHECK_EQ(rx.foes[i].level, ry.foes[i].level);
  }
  bool igual = x.evs.size() == z.evs.size();
  for (size_t i = 0; i < x.evs.size() && i < y.evs.size(); i++) {
    CHECK_EQ(x.evs[i].attacker, y.evs[i].attacker);
    CHECK_EQ(x.evs[i].damage, y.evs[i].damage);
    CHECK_EQ(x.evs[i].special, y.evs[i].special);
    CHECK_EQ(x.evs[i].hp[0], y.evs[i].hp[0]);
    CHECK_EQ(x.evs[i].hp[1], y.evs[i].hp[1]);
    if (i < z.evs.size() && x.evs[i].damage != z.evs[i].damage) igual = false;
  }
  CHECK_MSG(!igual, "otra semilla deberia dar otra fase");
}

TEST(stage, un_equipo_de_uno_pierde_al_primer_ko) {
  Fighter team[1] = { plain(129, 5) };
  StageLog log;
  StageRun run;
  CHECK_EQ(runStage(2, team, 1, 9, true, &log, &run), (uint8_t)SN_LOST);
  CHECK_EQ(log.steps.size(), (size_t)1);
  CHECK_EQ(log.steps[0], (uint8_t)SN_LOST);
  CHECK_EQ(run.allyHp(0), (uint16_t)0);
  CHECK_EQ(run.ally, (uint8_t)0);
  CHECK_EQ(run.teamN, (uint8_t)1);
  // y uno muy fuerte la supera solo
  Fighter mew[1] = { plain(150, 100) };
  CHECK_EQ(runStage(0, mew, 1, 9, true), (uint8_t)SN_CLEARED);
}

TEST(stage, se_pierde_cuando_caen_los_cuatro) {
  Fighter team[4] = { plain(129, 5), plain(10, 5), plain(13, 5), plain(129, 6) };
  StageLog log;
  StageRun run;
  CHECK_EQ(runStage(7, team, 4, 21, true, &log, &run), (uint8_t)SN_LOST);
  int allies = 0;
  for (uint8_t r : log.steps) allies += r == SN_NEXT_ALLY;
  CHECK_EQ(allies, 3);
  CHECK_EQ(log.steps.back(), (uint8_t)SN_LOST);
  for (int i = 0; i < 4; i++) CHECK_EQ(run.allyHp(i), (uint16_t)0);
  for (size_t i = 0; i < log.evs.size(); i++) CHECK(log.allyAt[i] <= 3);
  CHECK(run.over());
  CHECK_EQ(run.advance(), (uint8_t)SN_LOST);  // acabada, ya no cambia
  // con mas de 4 se topa y con 0 sale perdida
  Fighter six[6] = { plain(1, 5), plain(1, 5), plain(1, 5), plain(1, 5), plain(1, 5), plain(1, 5) };
  run.begin(0, six, 6, 1);
  CHECK_EQ(run.teamN, (uint8_t)TEAM_MAX);
  run.begin(0, six, 0, 1);
  CHECK(run.over());
  CHECK_EQ(run.state, (uint8_t)SN_LOST);
}

TEST(stage, se_supera_al_tumbar_al_jefe) {
  Fighter team[4] = { plain(150, 100), plain(150, 100), plain(150, 100), plain(150, 100) };
  for (uint8_t s = 0; s < STAGE_COUNT; s++) {
    StageLog log;
    StageRun run;
    CHECK_EQ(runStage(s, team, 4, 30 + s, true, &log, &run), (uint8_t)SN_CLEARED);
    CHECK(log.steps.size() >= 3);
    CHECK_EQ(log.steps.back(), (uint8_t)SN_CLEARED);
    int foes = 0;
    for (uint8_t r : log.steps) foes += r == SN_NEXT_FOE;
    CHECK_EQ(foes, STAGE_FOES - 1);
    CHECK(run.isBoss());
    CHECK_EQ(run.foeIdx, (uint8_t)(STAGE_FOES - 1));
    CHECK_EQ(run.eng.f[1].dex, STAGES[s].bossDex);
    CHECK_EQ(run.eng.f[1].hp, (uint16_t)0);
    CHECK_EQ(run.foe.hp, (uint16_t)0);
    // acabada: ni ataques ni toques cambian nada
    uint8_t n = run.eng.attacks;
    run.attack();
    CHECK(!run.tap());
    CHECK_EQ(run.eng.attacks, n);
    CHECK_EQ(run.advance(), (uint8_t)SN_CLEARED);
  }
}

// ---------------------------------------------------------------- informe de balance
// Tasas de superacion por fase para equipos de referencia, para afinar la
// tabla. Muestra de 6 lineas comunes de tipos variados (Pidgey, Charmander,
// Squirtle, Bulbasaur, Pikachu, Machop) en la forma que les toca a su nivel; el
// equipo de k es cada rotacion de la muestra cortada a k (asi cada especie abre
// igual de veces y no manda un solo emparejamiento de tipos). Genes 100.
//   (a) nivel minimo de la fase, entrenamiento = el minimo de los esbirros
//   (b) nivel alto de la fase, entrenamiento = el del jefe
//   (c) 10 niveles por encima del alto, entrenamiento del jefe
// Cada casilla: "tocando siempre / sin tocar" en %. Solo comprueba cordura.
#define BAL_SAMPLE 6
#define BAL_RUNS 2400  // fases por casilla (400 por rotacion)
static const int16_t BAL_DEX[BAL_SAMPLE] = { 16, 4, 7, 1, 25, 66 };

// only >= 0: solo esa rotacion (el equipo que abre esa especie)
static int balanceRate(uint8_t s, int caseK, uint8_t n, bool tapFull, int only = -1) {
  const StageDef &d = STAGES[s];
  uint16_t lv = caseK == 0 ? d.lvMin : caseK == 1 ? d.lvMax : d.lvMax + 10;
  uint8_t tr = caseK == 0 ? d.trMin : d.bossTr;
  long wins = 0, runs = 0;
  for (int r = 0; r < BAL_SAMPLE; r++) {
    if (only >= 0 && r != only) continue;
    Fighter team[TEAM_MAX];
    for (int i = 0; i < n; i++) {
      uint32_t evo = 1;
      int16_t dex = evolveForLevel(BAL_DEX[(r + i) % BAL_SAMPLE], lv, evo);
      team[i] = makeFighter(dex, lv, 100, 100, 100, tr, tr, tr);
    }
    for (int k = 0; k < BAL_RUNS / BAL_SAMPLE; k++, runs++) {
      uint32_t seed = (uint32_t)(s * 1000003u + caseK * 7919u + n * 104729u + r * 15485863u + k * 2654435761u);
      wins += runStage(s, team, n, seed, tapFull) == SN_CLEARED;
    }
  }
  return (int)((wins * 100 + runs / 2) / runs);
}

TEST(stage, informe_de_balance_tasas_de_superacion) {
  static const char *const NAME[STAGE_COUNT] = { "planta", "agua", "roca", "fuego",
                                                 "electr", "fantas", "veneno", "dragon" };
  static const uint8_t SIZES[3] = { 1, 2, 4 };
  int rate[STAGE_COUNT][3][3][2];  // fase, caso, tamano, (tocando, sin tocar)
  printf("\n  Informe de balance: %% de fases superadas (tocando siempre / sin tocar), %d fases por casilla\n",
         BAL_RUNS);
  printf("  muestra: Pidgey, Charmander, Squirtle, Bulbasaur, Pikachu, Machop (evolucionados a su nivel), genes 100\n");
  static const char *const CASE[3] = { " (a) nv. min, entr. min", " (b) nv. alto, entr. jefe",
                                        " (c) nv. alto+10, entr. jefe" };
  printf("  %-6s %-5s |", "", "");
  for (int c = 0; c < 3; c++) printf("%-27s |", CASE[c]);
  printf("\n  %-6s %-5s |", "fase", "nivel");
  for (int c = 0; c < 3; c++) printf(" %-8s %-8s %-8s |", "1 bicho", "2 bichos", "4 bichos");
  printf("\n");
  for (uint8_t s = 0; s < STAGE_COUNT; s++) {
    printf("  %-6s %2u-%-2u |", NAME[s], STAGES[s].lvMin, STAGES[s].lvMax);
    for (int c = 0; c < 3; c++) {
      for (int z = 0; z < 3; z++) {
        for (int t = 0; t < 2; t++) rate[s][c][z][t] = balanceRate(s, c, SIZES[z], t == 0);
        printf(" %3d/%-3d ", rate[s][c][z][0], rate[s][c][z][1]);
      }
      printf(" |");
    }
    printf("\n");
  }
  // cada especie sola, caso (b) tocando: explica los saltos de la columna de 1
  printf("\n  solo, caso (b), tocando: %%  ");
  for (int r = 0; r < BAL_SAMPLE; r++) printf(" %-10s", DEX_TBL[BAL_DEX[r]].name);
  printf("\n");
  for (uint8_t s = 0; s < STAGE_COUNT; s++) {
    printf("  %-6s %-19s", NAME[s], "");
    for (int r = 0; r < BAL_SAMPLE; r++) printf(" %-10d", balanceRate(s, 1, 1, true, r));
    printf("\n");
  }
  // cordura: mas fuerte no es peor (salvo ruido), tocar no es peor que no tocar
  const int NOISE = 4;
  for (int s = 0; s < STAGE_COUNT; s++)
    for (int c = 0; c < 3; c++)
      for (int z = 0; z < 3; z++)
        for (int t = 0; t < 2; t++) {
          char msg[96];
          snprintf(msg, sizeof(msg), "fase %d caso %d equipo %d tocar %d", s, c, SIZES[z], t == 0);
          int v = rate[s][c][z][t];
          if (c) CHECK_MSG(v >= rate[s][c - 1][z][t] - NOISE, msg);
          if (z) CHECK_MSG(v >= rate[s][c][z - 1][t] - NOISE, msg);
          if (t == 0) CHECK_MSG(v >= rate[s][c][z][1] - NOISE, msg);
        }
  // 4 bichos 10 niveles por encima superan la primera fase casi siempre
  CHECK(rate[0][2][2][0] >= 95);
  CHECK(rate[0][2][2][1] >= 90);
}
