#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "box.h"      // PetRecord y crc16
#include "petlife.h"  // lo que se guarda

// El segundo hueco de casa (v3.0) en la NVS: UNA entrada "hm1" (un putBytes: o esta entera o no esta),
// con version y CRC. El primer hueco sigue con las claves de siempre ("full", "joy", "uid"...), asi un
// guardado de la v2.x carga igual y su hueco 2 sale vacio (sin "hm1"). Logica pura, probada en el PC
// (test/test_home2.cpp). save() la escribe despues de "uid" y antes de "bpack", y solo si cambio.
//
// Lo que no va (como en la caja): lo que solo vive en RAM (animaciones, enfriamientos, "mantener forma",
// la cuenta de minutos despierto de noche, la ultima sacudida del huevo).

#define HOME_REC_VER 1  // version del formato (otra = se ignora y el hueco 2 sale vacio)

enum : uint8_t { HOME_VACANT = 0, HOME_EGG, HOME_PET };
// bits de HomeRec::flags
enum : uint8_t { HRF_EGG_SHINY = 1 << 0, HRF_EGG_CAP = 1 << 1, HRF_EGG_HELD = 1 << 2, HRF_DYING = 1 << 3,
                 HRF_NIGHT = 1 << 4 };

struct __attribute__((packed)) HomeRec {
  uint8_t ver;          // HOME_REC_VER
  uint8_t kind;         // HOME_*
  uint8_t flags;        // HRF_*
  PetRecord pet;        // el individuo (con HOME_EGG/VACANT solo valen la edad y el crecimiento)
  int16_t eggTarget;    // la incubadora (o el huevo de casa)
  uint8_t eggTaps, eggSaved;
  uint32_t eggPlaced;
  uint32_t satedUntil;  // la tarta
  uint8_t dyingMin, neglectMin, deathMin;
  int16_t deathEgg;
  uint32_t bedNight;
  uint32_t careDay;
  uint32_t poopAt[POOP_MAX];
  uint16_t crc;         // CRC-16 de todo lo anterior
};
static_assert(sizeof(HomeRec) <= 100, "hm1 cabe en una entrada pequena de la NVS");

inline uint16_t homeRecCrc(const HomeRec &h) { return crc16((const uint8_t *)&h, offsetof(HomeRec, crc)); }

inline void homeRecSeal(HomeRec &h) {
  h.ver = HOME_REC_VER;
  h.crc = homeRecCrc(h);
}

// se puede cargar: version conocida, CRC bueno, y un bicho de verdad si dice que lo es
inline bool homeRecValid(const HomeRec &h) {
  if (h.ver != HOME_REC_VER || h.crc != homeRecCrc(h) || h.kind > HOME_PET) return false;
  return h.kind != HOME_PET || recordValid(h.pet);
}

// el hueco tal cual, sellado. epoch = la hora del registro del individuo (storedEpoch)
inline HomeRec homeRecOf(const PetLife &l, uint32_t epoch) {
  HomeRec h;
  memset(&h, 0, sizeof(h));
  h.kind = l.vacant ? HOME_VACANT : l.isEgg() ? HOME_EGG : HOME_PET;
  h.flags = (l.eggShiny ? HRF_EGG_SHINY : 0) | (l.eggCap ? HRF_EGG_CAP : 0) | (l.eggHeldFlag ? HRF_EGG_HELD : 0) |
            (l.dying ? HRF_DYING : 0) | (l.nightSleep ? HRF_NIGHT : 0);
  h.pet = l.record(epoch);
  h.eggTarget = l.eggTarget;
  h.eggTaps = l.eggTaps;
  h.eggSaved = l.eggSaved;
  h.eggPlaced = l.eggPlaced;
  h.satedUntil = l.satedUntil;
  h.dyingMin = l.dyingMin;
  h.neglectMin = l.neglectMin;
  h.deathMin = l.deathMin;
  h.deathEgg = l.deathEgg;
  h.bedNight = l.bedNight;
  h.careDay = l.careDay;
  {  // v3.1: seguidas desde el primer hueco (al cargar se ocupan los primeros). Via un array suelto: HomeRec es packed
    uint32_t st[POOP_MAX];
    l.poopStamps(st);
    memcpy(h.poopAt, st, sizeof(st));
  }
  homeRecSeal(h);
  return h;
}

// lo contrario: el hueco pasa a ser el de h (que ya se comprobo con homeRecValid)
inline void homeRecApply(const HomeRec &h, PetLife &l) {
  l = PetLife{};
  if (h.kind == HOME_PET) {
    l.loadRecord(h.pet);
  } else {
    l.speciesId = -1;
    l.prevSpeciesId = -1;
    l.uid = 0;
    l.vacant = h.kind == HOME_VACANT;
    l.ageMinutes = h.pet.ageMinutes;  // la edad del huevo (el inicial eclosiona por edad)
    l.growMin = 0;
  }
  l.eggTarget = h.eggTarget;
  l.eggShiny = (h.flags & HRF_EGG_SHINY) != 0;
  l.eggCap = (h.flags & HRF_EGG_CAP) != 0;
  l.eggHeldFlag = (h.flags & HRF_EGG_HELD) != 0;
  l.eggTaps = h.eggTaps;
  l.eggSaved = h.eggSaved > EGG_SHAKE_CAP ? EGG_SHAKE_CAP : h.eggSaved;
  l.eggPlaced = h.eggPlaced;
  if (h.kind != HOME_PET) return;
  l.satedUntil = h.satedUntil;
  l.dying = (h.flags & HRF_DYING) != 0;
  l.dyingMin = h.dyingMin > REVIVE_LIMIT_MIN ? REVIVE_LIMIT_MIN : h.dyingMin;
  l.neglectMin = h.neglectMin;
  l.deathMin = h.deathMin > DEATH_MIN ? DEATH_MIN : h.deathMin;
  l.deathEgg = (h.deathEgg < 0 || h.deathEgg > 151) ? 0 : h.deathEgg;
  l.nightSleep = l.sleeping && (h.flags & HRF_NIGHT);
  l.bedNight = h.bedNight;
  l.careDay = h.careDay;
  for (uint8_t i = 0; i < POOP_MAX; i++)  // v3.1: una marca de despues de la edad no vale: recien hecha
    l.poopAt[i] = h.poopAt[i] <= l.ageMinutes ? h.poopAt[i] : l.ageMinutes;
}
