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
