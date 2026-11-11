#pragma once
#include <stdint.h>

// Volumen del altavoz: 4 niveles que cicla la pildora de sonido de los ajustes, con el
// apagado incluido. Logica pura, sin hardware, probada en el PC (test/test_volume.cpp); la
// escritura en el codec y la NVS viven en audio.cpp.
//
// El nivel se aplica en el registro 0x32 del ES8311 (volumen del DAC): pasos de 0,5 dB,
// 0xBF = 0 dB. Hasta la v1.49 el volumen era fijo a 0xAB (-10 dB), que es el nivel 2 (el de
// por defecto): quien no toca nada oye lo mismo que antes.
#define VOL_LEVELS 4
#define VOL_DEFAULT 2
static const uint8_t VOL_DAC_REG[VOL_LEVELS] = {
  0x00,  // 0: apagado (el amplificador se apaga y no se encola nada; el valor no se usa)
  0x9B,  // 1: bajo, ~-18 dB
  0xAB,  // 2: medio, -10 dB (el volumen de siempre)
  0xBB,  // 3: alto, ~-2 dB
};

// un valor guardado fuera de rango (NVS corrupta, una version futura) vuelve al de por defecto
inline uint8_t volClamp(uint8_t level) { return level < VOL_LEVELS ? level : VOL_DEFAULT; }

// cicla: apagado -> bajo -> medio -> alto -> apagado
inline uint8_t volNext(uint8_t level) { return (uint8_t)((volClamp(level) + 1) % VOL_LEVELS); }

inline uint8_t volDacReg(uint8_t level) { return VOL_DAC_REG[volClamp(level)]; }

// hasta la v1.49 solo habia "snd" (encendido/apagado): encendido era el volumen fijo de entonces
inline uint8_t volFromLegacy(bool snd) { return snd ? VOL_DEFAULT : 0; }

// el nivel al arrancar: "vol" si existe; si no, el viejo "snd" (que por defecto era encendido)
inline uint8_t volFromStored(bool hasVol, uint8_t vol, bool legacySnd) {
  return hasVol ? volClamp(vol) : volFromLegacy(legacySnd);
}
