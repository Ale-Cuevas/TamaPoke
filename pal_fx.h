#pragma once
// Los dos bichos juntos (v3.22, PALS): los iconos que flotan sobre los bichos en las escenas del patio y su
// movimiento. Logica pura (test/test_pal_fx.cpp); el patio (TamaPoke.ino) los dibuja.
#include <stdint.h>

// lo que puede salir sobre un bicho en una escena (no son los EMOTE_* de PetLife: estos no tocan el juego)
enum PalEmote : uint8_t {
  PE_NONE = 0,
  PE_HEART,    // carino, las paces
  PE_NOTE,     // contento, jugando
  PE_EXCLAIM,  // "!" al verse
  PE_DOTS,     // "..." se quedo sin la baya
  PE_POUT,     // nubecita de enfado (rina)
  PE_GRUMPY,   // nube gris con rayo: lo despertaron de la siesta
  PE_SPARKLE,  // chispas del tipo (las tinta el patio con el color del tipo)
  PE_KINDS
};

#include "dex.h"
#include "eggcolor.h"  // TYPE_EGG_BASE, eggTypesOf
#include "species.h"   // generado: solo <stdint.h> + tablas de sprites, seguro en el PC

// ---- sprites -----------------------------------------------------------------------------------------------
// Filas del sprite del icono y su lado (16 o 32). nullptr = sin sprite: PE_EXCLAIM se dibuja con drawExclaim del
// .ino y PE_NONE no se dibuja. n queda a 0 en esos casos.
static inline const char *const *palEmoteSprite(PalEmote k, uint8_t &n) {
  n = 16;
  switch (k) {
    case PE_HEART:   n = 32; return SPR_HEART;
    case PE_NOTE:    return SPR_ICON_NOTE;
    case PE_DOTS:    return SPR_ICON_DOTS;
    case PE_POUT:    return SPR_ICON_POUT;
    case PE_GRUMPY:  return SPR_ICON_GRUMPY;
    case PE_SPARKLE: return SPR_ICON_SPARK;
    default: break;
  }
  n = 0;
  return nullptr;
}

// ---- particulas de tipo ------------------------------------------------------------------------------------
// forma de la particula; el patio la tinta con palTypeTint (son blancas)
enum PalParticle : uint8_t { PP_SPARKLE = 0, PP_FLAME, PP_DROP, PP_LEAF, PP_BOLT, PP_KINDS };

static inline PalParticle palParticleOf(uint8_t type) {
  switch (type) {
    case TY_FUEGO:     return PP_FLAME;
    case TY_AGUA:
    case TY_HIELO:     return PP_DROP;
    case TY_PLANTA:
    case TY_BICHO:     return PP_LEAF;
    case TY_ELECTRICO: return PP_BOLT;
    default:           return PP_SPARKLE;
  }
}

// sprite 8x8 de la particula
static inline const char *const *palParticleSprite(PalParticle p, uint8_t &n) {
  n = 8;
  switch (p) {
    case PP_FLAME: return SPR_PART_FLAME;
    case PP_DROP:  return SPR_PART_DROP;
    case PP_LEAF:  return SPR_PART_LEAF;
    case PP_BOLT:  return SPR_PART_BOLT;
    default:       return SPR_PART_STAR;
  }
}

// color RGB565 del primer tipo de la especie; blanco calido si dex no vale
#define PAL_TINT_FALLBACK 0xFFF6  // ~ #fffbf0
static inline uint16_t palTypeTint(int16_t dex) {
  uint8_t t1, t2;
  if (!eggTypesOf(dex, t1, t2)) return PAL_TINT_FALLBACK;
  return TYPE_EGG_BASE[t1];
}

// ---- movimiento (numeros de aspecto, no de balance) --------------------------------------------------------
#define PAL_FLOAT_RISE   28   // px que sube un icono desde encima de la cabeza
#define PAL_FLOAT_WOBBLE 5    // vaiven lateral maximo (notas), px
#define PAL_FLOAT_BLINK  90   // ms por parpadeo en el ultimo cuarto (el "fundido": la pantalla no tiene alfa)
#define PAL_BURST_R      30   // radio final del abanico de particulas, px
#define PAL_BURST_RISE   10   // las particulas ademas suben un poco, px

// 16 direcciones: seno * 100 de 0 a 360 grados (el coseno es el seno desplazado 4)
static const int8_t PAL_SIN16[16] = {0, 38, 71, 92, 100, 92, 71, 38, 0, -38, -71, -92, -100, -92, -71, -38};

// suavizado de salida 0..1000 -> 0..1000
static inline int palEaseOut(int32_t tMs, int32_t durMs) {
  if (durMs <= 0 || tMs >= durMs) return 1000;
  if (tMs <= 0) return 0;
  int32_t p = tMs * 1000 / durMs, q = 1000 - p;
  return (int)(1000 - q * q / 1000);
}

// Icono que sube y se apaga desde la cabeza del bicho. dx/dy son el desplazamiento respecto al punto de anclaje
// (dy negativo = arriba). visible = false fuera de [0, durMs) y parpadeando en el ultimo cuarto.
// wobble = true: vaiven senoidal (notas).
static inline void palFloat(int32_t tMs, int32_t durMs, int16_t *dx, int16_t *dy, bool *visible, bool wobble = false) {
  *dx = 0; *dy = 0; *visible = false;
  if (durMs <= 0 || tMs < 0 || tMs >= durMs) return;
  *dy = (int16_t)(-(PAL_FLOAT_RISE * palEaseOut(tMs, durMs)) / 1000);
  if (wobble) {
    int32_t ph = tMs * 16 / 700;  // una vuelta cada ~0,7 s
    *dx = (int16_t)(PAL_FLOAT_WOBBLE * PAL_SIN16[ph & 15] / 100);
  }
  *visible = true;
  if (tMs * 4 >= durMs * 3) *visible = ((tMs / PAL_FLOAT_BLINK) & 1) == 0;
}

// Particula i de n que sale en abanico desde un punto. Deterministico. false = aun no sale o ya acabo.
// Reparte las direcciones por la circunferencia (con un giro por indice para que no se solapen) y sube un poco.
static inline bool palBurst(int i, int n, int32_t tMs, int32_t durMs, int16_t *dx, int16_t *dy) {
  *dx = 0; *dy = 0;
  if (n <= 0 || durMs <= 0 || tMs < 0 || tMs >= durMs) return false;
  int dir = (i * 16 / n + (i & 1)) & 15;
  int e = palEaseOut(tMs, durMs);
  *dx = (int16_t)(PAL_BURST_R * PAL_SIN16[(dir + 4) & 15] / 100 * e / 1000);
  *dy = (int16_t)((PAL_BURST_R * PAL_SIN16[dir] / 100 - PAL_BURST_RISE) * e / 1000);
  return true;
}
