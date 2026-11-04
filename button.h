#pragma once
#include <stdint.h>

// Antirrebote de una tecla fisica (USER = atras). Logica pura, con tests en
// test/test_button.cpp.
//
// Se acepta el primer cambio y se ignora cualquier otro durante KEY_DEBOUNCE_MS:
// el loop lee la tecla una vez por pase, y con un frame de por medio entre dos
// lecturas pasan ~60 ms, asi que esperar a varias lecturas iguales se comeria
// las pulsaciones rapidas. Los rebotes del contacto duran unos pocos ms, y caen
// dentro de la ventana.

#define KEY_DEBOUNCE_MS 30

struct KeyDebounce {
  bool stable = false;  // estado aceptado (true = pulsada)
  uint32_t since = 0;   // millis del ultimo cambio aceptado

  // raw = pulsada ahora. Devuelve true una sola vez por pulsacion, al bajar.
  bool update(bool raw, uint32_t now) {
    if (raw == stable) return false;
    if (now - since < KEY_DEBOUNCE_MS) return false;  // rebote del cambio anterior
    stable = raw;
    since = now;
    return stable;
  }
};
