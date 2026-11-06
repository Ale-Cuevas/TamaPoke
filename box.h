#pragma once
#include <stddef.h>
#include <stdint.h>

// La caja (docs/NEXT_FEATURES.md, Tarea BOX, v1.37). Logica pura, sin hardware, probada en el
// PC (test/test_box.cpp): el registro de un bicho guardado, su CRC y las constantes. Los
// movimientos (guardar, despertar) y su seguridad frente a un corte de luz viven en Pet
// (pet.cpp), que es quien tiene abierta la NVS.
//
// Un bicho guardado hiberna: ni envejece, ni crece, ni le bajan las barras, ni suma descuidos.
// En la caja solo esta lo del individuo (especie, nombre, edad, crecimiento, barras, genes,
// entrenamiento, vinculo, medallas...). Lo del jugador (racha, pasos, mochila, records,
// Pokedex) y el huevo de la incubadora se quedan donde estaban.

#define BOX_SLOTS 6         // huecos de la caja
#define PET_RECORD_VER 1    // version del formato del registro (otra = hueco danado)
#define BOX_FX_MS 600UL     // efecto de guardar / despertar (la luz que vuela al hueco o sale de el)

// estado de un hueco
enum : uint8_t { BOX_EMPTY = 0, BOX_OK, BOX_DAMAGED };

// bits de PetRecord::flags
enum : uint8_t { REC_SHINY = 1 << 0, REC_BERRY = 1 << 1, REC_SLEEP = 1 << 2 };

// Un bicho en un hueco de la caja: un putBytes de 55 bytes por hueco (unas 4 entradas de la
// NVS). Empaquetado y en el orden de la memoria (la placa y el PC son little-endian).
struct __attribute__((packed)) PetRecord {
  uint8_t ver;          // PET_RECORD_VER
  uint8_t flags;        // REC_*
  uint32_t uid;         // identidad del individuo (se sortea al nacer; nunca 0)
  int16_t speciesId;    // 1..151
  char nick[12];        // apodo (vacio = nombre de la especie), como Pet::nick
  uint32_t ageMinutes;
  uint32_t growMin;     // crecimiento (v1.35): de el sale el nivel
  uint8_t fullness, joy, energy, hygiene, poops, weight;
  uint8_t geneAtk, geneDef, geneSpe;
  uint8_t trAtk, trDef, trSpe;
  uint8_t careMistakes, bond, bondToday;
  uint32_t bondDay;     // dia de bondToday: el tope diario de vinculo va con cada bicho
  uint16_t medals;
  uint32_t storedEpoch; // cuando entro en la caja (hora del RTC)
  uint16_t crc;         // CRC-16 de todo lo anterior
};
static_assert(sizeof(PetRecord) <= 64, "un registro de la caja cabe en 64 bytes");

// CRC-16/CCITT-FALSE (polinomio 0x1021, inicio 0xFFFF): "123456789" -> 0x29B1
inline uint16_t crc16(const uint8_t *p, size_t n) {
  uint16_t c = 0xFFFF;
  while (n--) {
    c ^= (uint16_t)(*p++) << 8;
    for (int i = 0; i < 8; i++) c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1);
  }
  return c;
}

inline uint16_t recordCrc(const PetRecord &r) { return crc16((const uint8_t *)&r, offsetof(PetRecord, crc)); }

// pone la version y el CRC: lo ultimo antes de escribirlo
inline void recordSeal(PetRecord &r) {
  r.ver = PET_RECORD_VER;
  r.crc = recordCrc(r);
}

// un registro que se puede cargar: version conocida, CRC bueno y un bicho de verdad. Uno que
// no lo es nunca se carga ni se sobrescribe (se ve como "?"); solo WIPE o BOXCLR lo borran.
inline bool recordValid(const PetRecord &r) {
  return r.ver == PET_RECORD_VER && r.crc == recordCrc(r) && r.uid != 0 && r.speciesId >= 1 &&
         r.speciesId <= 151;
}
