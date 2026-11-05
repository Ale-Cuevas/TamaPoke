#pragma once
#include <stdint.h>
#include "dex.h"

// Motor de combate (docs/FEATURES.md, Tarea 3a). Logica pura y determinista para
// una semilla dada: el combate de practica de una sola placa y, mas adelante, el
// combate por radio (3d), donde el anfitrion lo ejecuta y manda cada ataque. Se
// prueba en el PC (test/test_battle.cpp), incluido un Monte Carlo de los 151x151.
//
// Un combate es automatico: los bichos se turnan para atacar hasta que uno cae.
// La unica entrada es el especial: tocar la pantalla llena un medidor de 5 y el
// siguiente ataque propio sale con un 30 % mas de potencia.
//
// Valores de partida, para afinar jugando (la tabla de constantes de FEATURES.md).

#define BATTLE_LEVEL_CAP 100   // nivel que entra en las cuentas (a nivel 999 decidiria todo)
#define ATK_POWER 40           // potencia de un ataque normal
#define SPECIAL_POWER_PCT 130  // el especial: +30 %
#define SPECIAL_TAPS 5         // toques para cargarlo
#define SPECIAL_COOLDOWN 2     // ataques normales propios antes de volver a cargar
#define HIT_MIN_PCT 5          // cada golpe, entre el 5 y el 25 % de la vida maxima del rival...
#define HIT_MAX_PCT 25
#define SPECIAL_MAX_PCT 30     // ...o el 30 % si es especial: un KO pide al menos 4 golpes
#define CRIT_ONE_IN 16         // golpe critico (x1,5)
#define MAX_ATTACKS 15         // tope: luego gana quien conserve mas % de vida
#define ATTACK_MS 1300         // tiempo por ataque en pantalla
#define INTRO_MS 3000          // tarjeta VS + jingle + entrada al campo
#define PRACTICE_AI_TAPS 2     // el rival simulado "toca" 2 veces por ataque suyo
#define PRACTICE_LEVEL_SPREAD 3  // nivel del rival de practica: el tuyo +-3

// resultado
enum : uint8_t { BR_NONE = 0, BR_WIN_A, BR_WIN_B, BR_DRAW };

// xorshift32: el mismo en las dos placas para el mismo resultado
static inline uint32_t battleRand(uint32_t &s) {
  if (!s) s = 0x9E3779B9u;
  s ^= s << 13;
  s ^= s >> 17;
  s ^= s << 5;
  return s;
}

// Tabla de eficacias de gen 1 (atacante en filas, defensor en columnas), en el
// orden TY_* de dex.h: NOR FUE AGU ELE PLA HIE LUC VEN TIE VOL PSI BIC ROC FAN DRA.
// '+' x2, '-' x0,5, '0' no le afecta, '.' normal. Con sus rarezas de gen 1: el
// hielo no se resiste al fuego, el bicho y el veneno se pegan x2 entre si y lo
// fantasma no afecta a lo psiquico.
static const char *const TYPE_CHART[TY_COUNT] = {
  "............-0.",  // normal
  ".--.++.....+-.-",  // fuego
  ".+-.-...+...+.-",  // agua
  "..+--...0+....-",  // electrico
  ".-+.-..-+-.-+.-",  // planta
  "..-.+-..++....+",  // hielo
  "+....+.-.---+0.",  // lucha
  "....+..--..+--.",  // veneno
  ".+.+-..+.0.-+..",  // tierra
  "...-+.+....+-..",  // volador
  "......++..-....",  // psiquico
  ".-..+.-+.-+..-.",  // bicho
  ".+...+-.-+.+...",  // roca
  "0.........0..+.",  // fantasma
  "..............+",  // dragon
};

// eficacia de un tipo atacante contra uno defensor, en mitades: 0, 1 (x0,5), 2 (x1), 4 (x2)
static inline uint8_t typeHalves(uint8_t atk, uint8_t def) {
  if (atk >= TY_COUNT || def >= TY_COUNT) return 2;
  char c = TYPE_CHART[atk][def];
  return c == '+' ? 4 : c == '-' ? 1 : c == '0' ? 0 : 2;
}

// contra uno o dos tipos, en cuartos: 0, 1 (x0,25), 2 (x0,5), 4 (x1), 8 (x2), 16 (x4)
static inline uint8_t typeEff4(uint8_t atk, uint8_t def1, uint8_t def2) {
  uint8_t e = typeHalves(atk, def1);
  return (def2 == TY_NONE) ? e * 2 : e * typeHalves(atk, def2);
}

struct Fighter {
  int16_t dex = 1;
  uint8_t level = 1;  // ya topado a BATTLE_LEVEL_CAP
  uint16_t maxHp = 1, hp = 1, atk = 1, def = 1, spe = 1;
  uint8_t type1 = TY_NORMAL, type2 = TY_NONE;
};

// las stats de combate de la ficha (base real de gen 1 x genes + nivel +
// entrenamiento, como Pet::atkStat) con el nivel topado; vida = base + nivel + 10
static inline Fighter makeFighter(int16_t dex, uint16_t level, uint8_t gAtk, uint8_t gDef, uint8_t gSpe,
                                  uint8_t tAtk, uint8_t tDef, uint8_t tSpe) {
  Fighter f;
  if (dex < 1 || dex > DEX_COUNT) dex = 1;
  const DexEntry &d = DEX_TBL[dex];
  uint16_t L = level < 1 ? 1 : (level > BATTLE_LEVEL_CAP ? BATTLE_LEVEL_CAP : level);
  f.dex = dex;
  f.level = (uint8_t)L;
  f.maxHp = f.hp = (uint16_t)(d.bHp + L + 10);
  f.atk = (uint16_t)((uint16_t)d.bAtk * gAtk / 100 + L + tAtk);
  f.def = (uint16_t)((uint16_t)d.bDef * gDef / 100 + L + tDef);
  f.spe = (uint16_t)((uint16_t)d.bSpe * gSpe / 100 + L + tSpe);
  if (!f.def) f.def = 1;
  f.type1 = d.type1;
  f.type2 = d.type2;
  return f;
}

// Dano de un golpe, estilo gen 1 y todo fisico (la Pokedex solo tiene PS, ATQ,
// DEF y VEL): ((2L/5 + 2) x P x A / D) / 50 + 2, por la eficacia, por una tirada
// del 85-100 % y x1,5 si es critico; luego entre HIT_MIN_PCT y HIT_MAX_PCT de la
// vida maxima del defensor (SPECIAL_MAX_PCT si es especial). "No le afecta" es 0.
static inline uint16_t hitDamage(const Fighter &a, const Fighter &d, uint8_t eff4, bool special, bool crit,
                                 uint8_t roll) {
  if (!eff4) return 0;
  uint32_t P = special ? (uint32_t)ATK_POWER * SPECIAL_POWER_PCT / 100 : ATK_POWER;
  uint32_t dmg = ((uint32_t)(2 * a.level / 5 + 2) * P * a.atk / d.def) / 50 + 2;
  dmg = dmg * eff4 / 4;
  dmg = dmg * roll / 100;
  if (crit) dmg = dmg * 3 / 2;
  uint32_t lo = (uint32_t)d.maxHp * HIT_MIN_PCT / 100;
  uint32_t hi = (uint32_t)d.maxHp * (special ? SPECIAL_MAX_PCT : HIT_MAX_PCT) / 100;
  if (lo < 1) lo = 1;
  if (hi < lo) hi = lo;
  if (dmg < lo) dmg = lo;
  if (dmg > hi) dmg = hi;
  return (uint16_t)dmg;
}

// un ataque, tal cual lo pinta la pantalla (y lo mandaria la radio en 3d)
struct BattleEvent {
  uint8_t index = 0;     // numero de ataque, desde 0
  uint8_t attacker = 0;  // 0 = A (tu bicho), 1 = B
  uint16_t damage = 0;   // lo que de verdad le quito
  uint8_t eff4 = 4;      // eficacia en cuartos (4 = normal)
  bool crit = false;
  bool special = false;
  uint16_t hp[2] = { 0, 0 };  // vida de los dos tras el golpe
  uint8_t result = BR_NONE;   // != BR_NONE: este ataque acabo el combate
};

struct Battle {
  Fighter f[2];
  uint32_t rng = 1;
  uint8_t attacks = 0;
  uint8_t result = BR_NONE;

  void begin(const Fighter &a, const Fighter &b, uint32_t seed) {
    f[0] = a;
    f[1] = b;
    f[0].hp = f[0].maxHp;
    f[1].hp = f[1].maxHp;
    rng = seed ^ 0xA5A5F00Du;
    if (!rng) rng = 1;
    attacks = 0;
    result = BR_NONE;
    first = roundOpener();
  }

  bool over() const { return result != BR_NONE; }

  // quien ataca ahora: en cada ronda abre el mas rapido (empate: a cara o cruz)
  uint8_t nextAttacker() const { return (attacks % 2 == 0) ? first : (uint8_t)(1 - first); }

  // el siguiente ataque; special = el atacante lo tenia cargado
  BattleEvent step(bool special) {
    BattleEvent ev;
    if (over()) {
      ev.result = result;
      ev.hp[0] = f[0].hp;
      ev.hp[1] = f[1].hp;
      return ev;
    }
    uint8_t a = nextAttacker(), d = (uint8_t)(1 - a);
    ev.index = attacks;
    ev.attacker = a;
    ev.special = special;
    ev.eff4 = typeEff4(f[a].type1, f[d].type1, f[d].type2);
    ev.crit = (battleRand(rng) % CRIT_ONE_IN) == 0;  // las dos tiradas siempre, para que la
    uint8_t roll = 85 + battleRand(rng) % 16;        // secuencia no dependa del resultado
    uint16_t dmg = hitDamage(f[a], f[d], ev.eff4, special, ev.crit, roll);
    if (!ev.eff4) ev.crit = false;  // "no le afecta" no es critico
    if (dmg > f[d].hp) dmg = f[d].hp;
    f[d].hp -= dmg;
    ev.damage = dmg;
    attacks++;
    if (f[d].hp == 0) {
      result = (a == 0) ? BR_WIN_A : BR_WIN_B;
    } else if (attacks >= MAX_ATTACKS) {
      // gana quien conserve mas % de vida (multiplicado en cruz: sin decimales)
      uint32_t pa = (uint32_t)f[0].hp * f[1].maxHp, pb = (uint32_t)f[1].hp * f[0].maxHp;
      result = pa > pb ? BR_WIN_A : pb > pa ? BR_WIN_B : BR_DRAW;
    } else if (attacks % 2 == 0) {
      first = roundOpener();  // ronda nueva
    }
    ev.hp[0] = f[0].hp;
    ev.hp[1] = f[1].hp;
    ev.result = result;
    return ev;
  }

private:
  uint8_t first = 0;
  uint8_t roundOpener() {
    if (f[0].spe != f[1].spe) return f[0].spe > f[1].spe ? 0 : 1;
    return battleRand(rng) & 1;
  }
};

// Medidor del especial (FR-3.2): cada toque suma una pastilla; con las 5, el
// siguiente ataque propio es especial. Despues queda bloqueado hasta 2 ataques
// normales propios. Los toques durante la intro no llegan aqui (los filtra la UI).
struct SpecialMeter {
  uint8_t pips = 0;
  uint8_t cooldown = 0;  // ataques normales propios que faltan para volver a cargar

  bool armed() const { return pips >= SPECIAL_TAPS && !cooldown; }
  bool locked() const { return cooldown > 0; }
  // un toque; false si no sirvio (bloqueado o ya lleno)
  bool tap() {
    if (cooldown || pips >= SPECIAL_TAPS) return false;
    pips++;
    return true;
  }
  // llamar al calcular cada ataque propio: true si sale especial (y lo gasta)
  bool useForAttack() {
    if (armed()) {
      pips = 0;
      cooldown = SPECIAL_COOLDOWN;
      return true;
    }
    if (cooldown) cooldown--;
    return false;
  }
};

// la forma de una linea que toca a ese nivel (la rama de Eevee, al azar)
static inline int16_t evolveForLevel(int16_t dex, uint16_t level, uint32_t &rng) {
  for (int guard = 0; guard < 4 && dex >= 1 && dex <= DEX_COUNT; guard++) {
    const DexEntry &d = DEX_TBL[dex];
    if (!d.evolvesTo || level < d.evolveLevel) break;
    dex = (dex == DEX_EEVEE) ? (int16_t)(134 + battleRand(rng) % 3) : (int16_t)d.evolvesTo;
  }
  return dex;
}

// Rival del combate de practica (FR-3.5): una linea de la misma rareza de huevo
// que la tuya, a tu nivel +-3, en la forma que le toca a ese nivel. Genes medios
// y tu entrenamiento medio los pone quien lo arma (makeFighter).
struct PracticeFoe {
  int16_t dex = 1;
  uint8_t level = 1;
};

static inline PracticeFoe pickPracticeFoe(int16_t mine, uint16_t myLevel, uint32_t &rng) {
  if (mine < 1 || mine > DEX_COUNT) mine = 1;
  uint8_t rar = DEX_TBL[DEX_TBL[mine].base].rarity;
  int16_t cand[DEX_COUNT];
  int n = 0;
  for (int16_t d = 1; d <= DEX_COUNT; d++)
    if (DEX_TBL[d].base == d && DEX_TBL[d].rarity == rar) cand[n++] = d;
  PracticeFoe foe;
  int16_t base = n ? cand[battleRand(rng) % n] : DEX_TBL[mine].base;
  int lv = (myLevel > BATTLE_LEVEL_CAP ? BATTLE_LEVEL_CAP : myLevel) +
           (int)(battleRand(rng) % (2 * PRACTICE_LEVEL_SPREAD + 1)) - PRACTICE_LEVEL_SPREAD;
  foe.level = (uint8_t)(lv < 1 ? 1 : (lv > BATTLE_LEVEL_CAP ? BATTLE_LEVEL_CAP : lv));
  foe.dex = evolveForLevel(base, foe.level, rng);
  return foe;
}
