#pragma once
#include <stdint.h>
#include "battle.h"

// Fases de combate: 8 fases tematicas, cada una con 3 rivales seguidos (2
// esbirros y un jefe) contra un equipo ordenado de 1 a 4 bichos. La vida no se
// recupera dentro de una fase: si cae un aliado entra el siguiente contra el
// mismo rival con la vida que le quede; si cae el rival, el mismo aliado sigue
// con la suya contra el siguiente. Se supera al tumbar al jefe y se pierde
// cuando caen todos los aliados. Logica pura (test/test_stage.cpp).
//
// API para la pantalla:
//   stageRollFoes(stage, rng, out)  los 3 rivales de una fase (StageFoe)
//   stageFoeFighter(foe)            el Fighter de un StageFoe (genes 100)
//   StageRun run;
//   run.begin(stage, team, n, seed) empieza: el equipo entra con la vida llena
//   run.nextAttacker()              0 = tu aliado, 1 = el rival (como Battle)
//   run.attack()                    el siguiente ataque, con los medidores ya
//                                   gastados (tu meter, y aiTaps toques del rival)
//   run.tap()                       un toque tuyo al medidor (false si no sirvio)
//   run.koPending()                 el ultimo ataque fue un KO: toca advance()
//   run.advance()                   tras el KO: SN_NEXT_ALLY, SN_NEXT_FOE,
//                                   SN_CLEARED o SN_LOST; arma el siguiente combate
//   run.over() / run.state          fase acabada (SN_CLEARED / SN_LOST)
//   run.ally, run.foeIdx, run.isBoss()   quien pelea ahora
//   run.allyHp(i), run.teamN, run.team[i].maxHp   la tira de 4 pastillas
//   run.eng.f[0] / run.eng.f[1]     los dos en el campo (vida en curso incluida)
//   run.meter / run.foeMeter        los medidores (para pintar las pastillas)

#ifndef STAGE_COUNT
#define STAGE_COUNT 8  // pet.h define lo mismo con la misma guarda
#endif
#ifndef TEAM_MAX
#define TEAM_MAX 4
#endif
#define STAGE_FOES 3      // 2 esbirros y el jefe
#define STAGE_POOL_MAX 7  // especies de esbirro por fase, como mucho

struct StageDef {
  uint8_t lvMin, lvMax;     // nivel de los esbirros; el jefe, siempre lvMax
  uint8_t trMin, trMax;     // entrenamiento de cada stat de un esbirro
  uint8_t bossTr;           // entrenamiento del jefe (las tres stats)
  int16_t bossDex;
  uint8_t theme1, theme2;   // tipos del tema (TY_NONE si solo uno)
  uint8_t poolN;
  int16_t pool[STAGE_POOL_MAX];  // esbirros posibles, en la forma tal cual
};

static const StageDef STAGES[STAGE_COUNT] = {
  // planta: Oddish, Bellsprout, Paras, Exeggcute, Bulbasaur; jefe Ivysaur
  { 15, 20, 20, 35, 40, 2, TY_PLANTA, TY_NONE, 5, { 43, 69, 46, 102, 1 } },
  // agua: Psyduck, Poliwhirl, Staryu, Horsea, Krabby, Goldeen, Shellder; jefe Wartortle
  { 25, 30, 30, 40, 50, 8, TY_AGUA, TY_NONE, 7, { 54, 61, 120, 116, 98, 118, 90 } },
  // roca/tierra: Graveler, Sandslash, Dugtrio, Marowak, Onix; jefe Golem
  { 35, 40, 50, 55, 60, 76, TY_ROCA, TY_TIERRA, 5, { 75, 28, 51, 105, 95 } },
  // fuego: Ninetales, Rapidash, Magmar, Flareon, Charmeleon; jefe Arcanine
  { 40, 45, 60, 65, 65, 59, TY_FUEGO, TY_NONE, 5, { 38, 78, 126, 136, 5 } },
  // electrico: Raichu, Magneton, Electrode, Jolteon; jefe Electabuzz
  { 50, 55, 60, 65, 65, 125, TY_ELECTRICO, TY_NONE, 4, { 26, 82, 101, 135 } },
  // fantasma/psiquico: Haunter, Kadabra, Hypno, Mr. Mime, Jynx, Slowbro; jefe Gengar
  { 60, 65, 60, 65, 70, 94, TY_FANTASMA, TY_PSIQUICO, 6, { 93, 64, 97, 122, 124, 80 } },
  // veneno/bicho: Arbok, Weezing, Golbat, Venomoth, Beedrill, Pinsir, Scyther; jefe Nidoking
  { 65, 70, 60, 65, 70, 34, TY_VENENO, TY_BICHO, 7, { 24, 110, 42, 49, 15, 127, 123 } },
  // dragon: Dragonair, y de relleno Gyarados, Aerodactyl y Charizard; jefe Dragonite
  { 70, 80, 70, 75, 80, 149, TY_DRAGON, TY_NONE, 4, { 148, 130, 142, 6 } },
};

// un rival de la fase, tal cual sale de la tirada
struct StageFoe {
  int16_t dex = 1;
  uint8_t level = 1;
  uint8_t tAtk = 0, tDef = 0, tSpe = 0;  // entrenamiento
  uint8_t aiTaps = 0;  // toques al medidor antes de cada ataque suyo: 0, 1 o 2 (el jefe)
};

static inline Fighter stageFoeFighter(const StageFoe &e) {
  return makeFighter(e.dex, e.level, 100, 100, 100, e.tAtk, e.tDef, e.tSpe);
}

static inline uint8_t stageRange(uint8_t lo, uint8_t hi, uint32_t &rng) {
  return hi > lo ? (uint8_t)(lo + battleRand(rng) % (hi - lo + 1)) : lo;
}

// Los 3 rivales: dos esbirros de especies distintas del pool, cada uno a un
// nivel al azar del rango y cada stat entrenada a su propio valor al azar; el
// jefe, su especie fija al nivel alto con el entrenamiento de jefe. El primero
// nunca carga el especial, el segundo poco y el jefe como el rival de practica.
static inline void stageRollFoes(uint8_t stage, uint32_t &rng, StageFoe out[STAGE_FOES]) {
  if (stage >= STAGE_COUNT) stage = STAGE_COUNT - 1;
  const StageDef &s = STAGES[stage];
  uint8_t n = s.poolN < 1 ? 1 : s.poolN;
  uint8_t pick[2];
  pick[0] = (uint8_t)(battleRand(rng) % n);
  pick[1] = n > 1 ? (uint8_t)(battleRand(rng) % (n - 1)) : 0;
  if (n > 1 && pick[1] >= pick[0]) pick[1]++;
  for (int i = 0; i < 2; i++) {
    StageFoe &e = out[i];
    e.dex = s.pool[pick[i]];
    e.level = stageRange(s.lvMin, s.lvMax, rng);
    e.tAtk = stageRange(s.trMin, s.trMax, rng);
    e.tDef = stageRange(s.trMin, s.trMax, rng);
    e.tSpe = stageRange(s.trMin, s.trMax, rng);
    e.aiTaps = (uint8_t)i;
  }
  StageFoe &b = out[STAGE_FOES - 1];
  b.dex = s.bossDex;
  b.level = s.lvMax;
  b.tAtk = b.tDef = b.tSpe = s.bossTr;
  b.aiTaps = PRACTICE_AI_TAPS;
}

// que paso tras un KO (advance) y estado de la fase
enum : uint8_t { SN_NONE = 0, SN_NEXT_ALLY, SN_NEXT_FOE, SN_CLEARED, SN_LOST };

struct StageRun {
  uint8_t stage = 0;
  Fighter team[TEAM_MAX];  // en orden; hp = la vida que le queda (0 = cayo)
  uint8_t teamN = 0;
  StageFoe foes[STAGE_FOES];
  uint8_t foeN = STAGE_FOES;  // rivales de la fase: 3; un encuentro salvaje (v2.2, beginWild), 1
  Fighter foe;             // el rival en curso, con la vida que le queda
  uint8_t ally = 0;        // indice del aliado en el campo
  uint8_t foeIdx = 0;      // 0, 1 esbirros; 2 el jefe
  Battle eng;              // el combate en curso (modo fase)
  SpecialMeter meter;      // el tuyo: se vacia al entrar otro aliado
  SpecialMeter foeMeter;   // el del rival: se vacia al entrar otro rival
  uint8_t state = SN_NONE; // SN_NONE en curso; SN_CLEARED o SN_LOST al acabar
  uint32_t rng = 1;

  // empieza la fase: tira los rivales y arma el primer combate. El equipo entra
  // con la vida llena; n se topa a TEAM_MAX y con n = 0 la fase sale perdida.
  void begin(uint8_t stg, const Fighter *mates, uint8_t n, uint32_t seed) {
    *this = StageRun{};
    stage = stg >= STAGE_COUNT ? STAGE_COUNT - 1 : stg;
    rng = seed ^ 0x5EA6E5u;
    if (!rng) rng = 1;
    teamN = n > TEAM_MAX ? TEAM_MAX : n;
    for (uint8_t i = 0; i < teamN; i++) {
      team[i] = mates[i];
      team[i].hp = team[i].maxHp;
    }
    stageRollFoes(stage, rng, foes);
    foe = stageFoeFighter(foes[0]);
    if (!teamN) {
      state = SN_LOST;
      return;
    }
    eng.beginCarry(team[0], foe, battleRand(rng));
  }

  // un encuentro salvaje (v2.2, encounter.h): un solo rival y, en el equipo, un solo bicho. Mismas reglas
  // que una fase (vida que no se recupera, muerte subita, medidores); el salvaje toca su medidor aiTaps
  // veces por ataque. stage queda a 0 (la pantalla no lo ensena)
  void beginWild(const Fighter &mate, const StageFoe &wild, uint32_t seed) {
    *this = StageRun{};
    rng = seed ^ 0x5EA6E5u;
    if (!rng) rng = 1;
    teamN = 1;
    team[0] = mate;
    team[0].hp = team[0].maxHp;
    foeN = 1;
    foes[0] = wild;
    foe = stageFoeFighter(foes[0]);
    eng.beginCarry(team[0], foe, battleRand(rng));
  }

  bool over() const { return state == SN_CLEARED || state == SN_LOST; }
  bool koPending() const { return !over() && eng.over(); }
  bool isWild() const { return foeN == 1; }
  bool isBoss() const { return !isWild() && foeIdx == foeN - 1; }
  uint8_t nextAttacker() const { return eng.nextAttacker(); }
  uint16_t allyHp(uint8_t i) const { return i < teamN ? team[i].hp : 0; }
  const StageFoe &foeInfo() const { return foes[foeIdx]; }

  // un toque tuyo al medidor; false si no sirvio (bloqueado, lleno o sin combate)
  bool tap() { return !over() && !eng.over() && meter.tap(); }

  // el siguiente ataque, como battleStep: tu medidor decide tu especial; el
  // rival toca aiTaps veces antes de cada ataque suyo. Tras un KO no hace nada
  // (devuelve el estado tal cual) hasta llamar a advance().
  BattleEvent attack() {
    if (over() || eng.over()) return eng.step(false);
    bool special;
    if (eng.nextAttacker() == 0) {
      special = meter.useForAttack();
    } else {
      for (uint8_t i = 0; i < foes[foeIdx].aiTaps; i++) foeMeter.tap();
      special = foeMeter.useForAttack();
    }
    BattleEvent ev = eng.step(special);
    team[ally].hp = eng.f[0].hp;
    foe.hp = eng.f[1].hp;
    return ev;
  }

  // tras el KO: entra el siguiente aliado (el rival sigue con su vida) o el
  // siguiente rival (tu aliado sigue con la suya), o se acaba la fase. Sin KO
  // pendiente devuelve SN_NONE (o el estado final si ya acabo) y no toca nada.
  uint8_t advance() {
    if (over()) return state;
    if (!eng.over()) return SN_NONE;
    if (eng.result == BR_WIN_B) {
      team[ally].hp = 0;
      if (++ally >= teamN) {
        ally = teamN - 1;
        return state = SN_LOST;
      }
      meter = SpecialMeter{};
      eng.beginCarry(team[ally], foe, battleRand(rng));
      return SN_NEXT_ALLY;
    }
    foe.hp = 0;
    if (++foeIdx >= foeN) {
      foeIdx = foeN - 1;
      return state = SN_CLEARED;
    }
    foe = stageFoeFighter(foes[foeIdx]);
    foeMeter = SpecialMeter{};
    eng.beginCarry(team[ally], foe, battleRand(rng));
    return SN_NEXT_FOE;
  }
};
