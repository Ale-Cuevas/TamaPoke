#pragma once
// Premios de los dos bichos juntos (v3.22, docs/HOME_AND_PALS.md PALS, FR-PALS.3/.4/.9): los contadores diarios por
// bicho y su blob "pal" de la NVS. Logica pura (test/test_pal_perks.cpp). Los numeros salen de tuning.h.
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "box.h"  // crc16

#define PAL_VER 1
#define PAL_SLOTS 2  // = HOME_SLOTS (pet.h lo comprueba)

struct __attribute__((packed)) PalSlot {
  uint32_t uid;       // el bicho al que pertenecen (0 = nadie aun)
  uint32_t day;       // dia de los dos contadores (vuelven a 0 al cambiar el dia o el uid)
  uint8_t playsPaid;  // escenas de juego pagadas ese dia
  uint8_t cheersPaid; // animos pagados ese dia
};
struct __attribute__((packed)) PalSave {
  uint8_t ver;  // PAL_VER
  PalSlot slot[PAL_SLOTS];
  uint16_t crc;  // CRC-16 de todo lo anterior
};
static_assert(sizeof(PalSlot) == 10 && sizeof(PalSave) == 23, "\"pal\": 23 bytes");

inline uint16_t palCrc(const PalSave &p) { return crc16((const uint8_t *)&p, offsetof(PalSave, crc)); }
inline void palSeal(PalSave &p) {
  p.ver = PAL_VER;
  p.crc = palCrc(p);
}
inline bool palValid(const PalSave &p) { return p.ver == PAL_VER && p.crc == palCrc(p); }

// los contadores de un hueco para este bicho y este dia: si cambio el uid o el dia, vuelven a 0
inline void palRoll(PalSlot &s, uint32_t uid, uint32_t day) {
  if (s.uid != uid || s.day != day) {
    s.uid = uid;
    s.day = day;
    s.playsPaid = 0;
    s.cheersPaid = 0;
  }
}
