#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "box.h"     // crc16
#include "tuning.h"  // MEDAL_T (umbrales) y MEDAL_COINS (lo que paga cada tramo)

// Medallas del jugador (v3.4). Logica pura, sin hardware, probada en el PC (test/test_medals.cpp).
// Sustituyen a las medallas de cada bicho (v1.x - v3.3, MED_*): ahora son 19 medallas del jugador, cada una
// con tres tramos (Bronce, Plata, Oro) que pagan sus monedas una sola vez, fuera del tope diario.
//
// Cada medalla mide una cosa, de una de tres fuentes:
// - MSRC_PET: bichos que alguna vez llegaron a algo (Nv.10, vinculo 100, forma final...). Cada bicho lleva
//   sus bits "ya contado" (PetLife::counted: en PetRecord::medals con REC_V3 en la caja y en el hueco 1, y en
//   la clave "pmc" para el hueco 0): asi cuenta UNA vez aunque vaya y vuelva de la caja.
// - MSRC_EVENT: contadores que suben en el momento (evoluciones, despedidas, dias de 10000 pasos, salvajes
//   vencidos, capturas, viajes). Empiezan a 0 al actualizar.
// - MSRC_DERIVED: se leen del estado de siempre (Pokedex, shinies, mejor racha, pasos, estadios, recuerdos).
//
// "pm" (NVS) guarda en una sola entrada (version + CRC) el tramo pagado de cada medalla, los contadores y,
// para los registros de la caja de antes de la v3.4 (sin REC_V3), los bits con que se contaron al migrar.

// las 19 medallas. El ORDEN es del guardado ("pm" guarda por posicion): una nueva va al final
enum : uint8_t {
  MD_LV10 = 0, MD_LV25, MD_LV50, MD_BOND, MD_FINAL, MD_FIT, MD_FAV,  // de bichos (bit i de counted)
  MD_EVOLVE, MD_FAREWELL, MD_DEX, MD_SHINY, MD_STREAK, MD_WALKER, MD_MARATHON,
  MD_STAGES, MD_WILD, MD_CATCH, MD_TRIPS, MD_SOUV,
  MEDAL_COUNT
};
static_assert(sizeof(MEDAL_T) / sizeof(MEDAL_T[0]) == MEDAL_COUNT, "tuning.h: un umbral por medalla");

#define MEDAL_TIERS 3
enum : uint8_t { MTIER_NONE = 0, MTIER_BRONZE, MTIER_SILVER, MTIER_GOLD };
enum : uint8_t { MSRC_PET = 0, MSRC_EVENT, MSRC_DERIVED };

// bits de PetLife::counted: el bit i es la medalla i (MD_LV10..MD_FAV)
#define PMB_COUNT 7
#define PMB_ALL ((uint8_t)((1u << PMB_COUNT) - 1))
enum : uint8_t {
  PMB_LV10 = 1 << MD_LV10, PMB_LV25 = 1 << MD_LV25, PMB_LV50 = 1 << MD_LV50, PMB_BOND = 1 << MD_BOND,
  PMB_FINAL = 1 << MD_FINAL, PMB_FIT = 1 << MD_FIT, PMB_FAV = 1 << MD_FAV
};

struct MedalDef {
  uint8_t src;        // MSRC_*
  const uint32_t *t;  // umbrales de Bronce, Plata y Oro (tuning.h)
};

inline uint8_t medalSource(uint8_t i) {
  if (i < PMB_COUNT) return MSRC_PET;
  switch (i) {
    case MD_EVOLVE: case MD_FAREWELL: case MD_MARATHON: case MD_WILD: case MD_CATCH: case MD_TRIPS: return MSRC_EVENT;
    default: return MSRC_DERIVED;
  }
}

inline MedalDef medalDef(uint8_t i) {
  MedalDef d;
  d.src = medalSource(i);
  d.t = MEDAL_T[i < MEDAL_COUNT ? i : 0];
  return d;
}

// el tramo al que llega un valor (0 = ninguno, 3 = Oro)
inline uint8_t medalTier(const MedalDef &d, uint32_t v) {
  uint8_t t = 0;
  while (t < MEDAL_TIERS && v >= d.t[t]) t++;
  return t;
}
inline uint8_t medalTierOf(uint8_t i, uint32_t v) { return medalTier(medalDef(i), v); }

// el siguiente umbral (0 = ya tiene el Oro)
inline uint32_t medalNext(uint8_t i, uint32_t v) {
  MedalDef d = medalDef(i);
  uint8_t t = medalTier(d, v);
  return t < MEDAL_TIERS ? d.t[t] : 0;
}

// monedas del tramo t (1..3) y de pasar del tramo `from` al `to` (cada uno se paga entero)
inline uint16_t medalCoins(uint8_t tier) { return tier >= 1 && tier <= MEDAL_TIERS ? MEDAL_COINS[tier - 1] : 0; }
inline uint16_t medalPayout(uint8_t from, uint8_t to) {
  uint16_t c = 0;
  for (uint8_t t = from + 1; t <= to && t <= MEDAL_TIERS; t++) c += medalCoins(t);
  return c;
}

// a que medallas de bicho llega un individuo ahora (bits PMB_*). Los niveles, el vinculo y "en forma" (nivel,
// peso maximo y descuidos maximos) estan en tuning.h
inline uint8_t medalPetBits(uint16_t level, uint8_t bond, bool finalForm, uint8_t weight, uint8_t mistakes, bool favKnown) {
  uint8_t b = 0;
  if (level >= MEDAL_PET_LV[0]) b |= PMB_LV10;
  if (level >= MEDAL_PET_LV[1]) b |= PMB_LV25;
  if (level >= MEDAL_PET_LV[2]) b |= PMB_LV50;
  if (bond >= MEDAL_BOND) b |= PMB_BOND;
  if (finalForm) b |= PMB_FINAL;
  if (level >= MEDAL_FIT_LV && weight <= MEDAL_FIT_WEIGHT && mistakes <= MEDAL_FIT_MISTAKES) b |= PMB_FIT;
  if (favKnown) b |= PMB_FAV;
  return b;
}

// ---------- "pm": lo que se guarda ----------
// version del formato. Un "pm" con otra version (o danado) se recuenta SIN pagar (pmValid falla): subirla exige
// una migracion de verdad que lea el formato anterior, o los jugadores perderian sus contadores de evento
#define PM_VER 1
#define PM_LEGACY_MAX 8    // registros de antes de la v3.4 que se apuntan al migrar (la caja tiene 6)

struct __attribute__((packed)) PlayerMedals {
  uint8_t ver;                       // PM_VER
  uint8_t tier[MEDAL_COUNT];         // tramo ya pagado de cada medalla (0..3)
  uint16_t cnt[MEDAL_COUNT];         // contadores de las de bicho y de las de evento (las derivadas, 0)
  // el encuentro salvaje (su hora de salida, encSpawnAt) que ya conto su victoria / su captura: un corte de luz
  // antes de "enc" deja volver a pelearlo (o a lanzar la bola), y no cuenta otra vez
  uint32_t wildId, catchId;
  uint8_t legN;                      // registros de la caja de antes de la v3.4 contados al migrar...
  uint32_t legUid[PM_LEGACY_MAX];    // ...su uid...
  uint8_t legBits[PM_LEGACY_MAX];    // ...y con que bits se contaron (al despertarlos no vuelven a contar)
  uint16_t crc;                      // CRC-16 de todo lo anterior
};
static_assert(sizeof(PlayerMedals) <= 120, "pm cabe en una entrada pequena de la NVS");

inline uint16_t pmCrc(const PlayerMedals &m) { return crc16((const uint8_t *)&m, offsetof(PlayerMedals, crc)); }

inline void pmSeal(PlayerMedals &m) {
  m.ver = PM_VER;
  m.crc = pmCrc(m);
}

inline void pmClear(PlayerMedals &m) {
  memset(&m, 0, sizeof(m));
  pmSeal(m);
}

// se puede cargar: version conocida, CRC bueno y valores con sentido
inline bool pmValid(const PlayerMedals &m) {
  if (m.ver != PM_VER || m.crc != pmCrc(m) || m.legN > PM_LEGACY_MAX) return false;
  for (uint8_t i = 0; i < MEDAL_COUNT; i++)
    if (m.tier[i] > MEDAL_TIERS) return false;
  return true;
}

// los bits con que se conto al migrar un registro de antes de la v3.4 (0 si no se apunto)
inline uint8_t pmLegacyBits(const PlayerMedals &m, uint32_t uid) {
  if (!uid) return 0;
  for (uint8_t i = 0; i < m.legN && i < PM_LEGACY_MAX; i++)
    if (m.legUid[i] == uid) return m.legBits[i] & PMB_ALL;
  return 0;
}

// apunta (o amplia) los bits de un registro de la caja al migrar; false = la lista esta llena
inline bool pmLegacyAdd(PlayerMedals &m, uint32_t uid, uint8_t bits) {
  if (!uid) return false;
  for (uint8_t i = 0; i < m.legN && i < PM_LEGACY_MAX; i++)
    if (m.legUid[i] == uid) {
      m.legBits[i] |= bits & PMB_ALL;
      return true;
    }
  if (m.legN >= PM_LEGACY_MAX) return false;
  m.legUid[m.legN] = uid;
  m.legBits[m.legN] = bits & PMB_ALL;
  m.legN++;
  return true;
}

// un contador +n (sin dar la vuelta)
inline void pmBump(PlayerMedals &m, uint8_t i, uint16_t n = 1) {
  if (i >= MEDAL_COUNT) return;
  uint32_t v = (uint32_t)m.cnt[i] + n;
  m.cnt[i] = v > 0xFFFF ? 0xFFFF : (uint16_t)v;
}

// un bicho llega a `reached`: lo que aun no tenia contado suma 1 a su medalla, una sola vez. Devuelve los
// bits nuevos (y los deja en counted)
inline uint8_t pmCountPet(PlayerMedals &m, uint8_t &counted, uint8_t reached) {
  uint8_t fresh = (uint8_t)(reached & PMB_ALL & ~counted);
  for (uint8_t i = 0; i < PMB_COUNT; i++)
    if (fresh & (1u << i)) pmBump(m, i);
  counted |= fresh;
  return fresh;
}

// ---------- los banners que esperan (la UI los ensena con la pantalla a la vista) ----------
struct MedalNews {
  uint8_t medal;   // MD_*
  uint8_t tier;    // el tramo mas alto alcanzado
  uint16_t coins;  // monedas que entraron (todas las de los tramos nuevos de esa medalla)
};

// una medalla que ya espera se funde (el tramo mas alto y la suma de monedas): con una por medalla nunca
// se llena (cap = MEDAL_COUNT)
inline void medalNewsPush(MedalNews *q, uint8_t &n, uint8_t cap, uint8_t medal, uint8_t tier, uint16_t coins) {
  for (uint8_t i = 0; i < n; i++)
    if (q[i].medal == medal) {
      if (tier > q[i].tier) q[i].tier = tier;
      uint32_t c = (uint32_t)q[i].coins + coins;
      q[i].coins = c > 0xFFFF ? 0xFFFF : (uint16_t)c;
      return;
    }
  if (n >= cap) return;
  q[n].medal = medal;
  q[n].tier = tier;
  q[n].coins = coins;
  n++;
}

// numero corto para la pagina de medallas: 999, 52k, 250k, 1M, 1.2M (en buf, de al menos 8)
inline void medalShortNum(char *buf, size_t n, uint32_t v) {
  if (v < 10000) snprintf(buf, n, "%lu", (unsigned long)v);
  else if (v < 1000000) snprintf(buf, n, "%luk", (unsigned long)(v / 1000));
  else if (v % 1000000 == 0 || v >= 10000000) snprintf(buf, n, "%luM", (unsigned long)(v / 1000000));
  else snprintf(buf, n, "%lu.%luM", (unsigned long)(v / 1000000), (unsigned long)((v / 100000) % 10));
}
