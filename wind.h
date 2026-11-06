#pragma once
#include <stdint.h>

// Viento para el paisaje (v1.26): cuanto se inclina una brizna, una flor o la punta de un
// arbol en el instante `now` y en la columna `x`. Logica pura, sin pantalla, con tests en
// test/test_wind.cpp.
//
// Dos cosas sumadas:
//  - un vaiven suave (WIND_SWAY_PX a cada lado, periodo de WIND_PERIOD_MS), desfasado con x
//    para que el prado ondule en vez de moverse todo a la vez;
//  - una rafaga que cruza la pantalla de izquierda a derecha cada WIND_GUST_MS: lo que va
//    pillando se inclina hasta WIND_GUST_PX mas hacia la derecha y vuelve.
// El resultado esta siempre en [-WIND_MAX_PX, WIND_MAX_PX] y depende solo de (now, x):
// el mismo instante da el mismo dibujo (el renderer del PC y la placa coinciden).

#define WIND_SWAY_PX 2
#define WIND_GUST_PX 5
#define WIND_MAX_PX (WIND_SWAY_PX + WIND_GUST_PX)
#define WIND_PERIOD_MS 3200   // 64 pasos de 50 ms
#define WIND_GUST_MS 12000    // una rafaga cada 12 s
#define WIND_GUST_W 110       // media anchura de la rafaga (px)
#define WIND_SPAN 466         // anchura del lienzo que cruza

// seno * 127 en 64 pasos
static const int8_t WIND_SIN[64] = {
  0, 12, 25, 37, 49, 60, 71, 81, 90, 98, 106, 112, 117, 122, 125, 126,
  127, 126, 125, 122, 117, 112, 106, 98, 90, 81, 71, 60, 49, 37, 25, 12,
  0, -12, -25, -37, -49, -60, -71, -81, -90, -98, -106, -112, -117, -122, -125, -126,
  -127, -126, -125, -122, -117, -112, -106, -98, -90, -81, -71, -60, -49, -37, -25, -12,
};

// fuerza de la rafaga en x (0..127): un triangulo que avanza de izquierda a derecha,
// entrando por fuera de la pantalla y saliendo por el otro lado
inline int windGust(uint32_t now, int x) {
  int32_t travel = WIND_SPAN + 2 * WIND_GUST_W;
  int32_t front = (int32_t)((now % WIND_GUST_MS) * (uint32_t)travel / WIND_GUST_MS) - WIND_GUST_W;
  int32_t d = x - front;
  if (d < 0) d = -d;
  return d >= WIND_GUST_W ? 0 : (int)(127 - d * 127 / WIND_GUST_W);
}

// inclinacion en px (positivo = a la derecha) de lo que crece en la columna x
inline int windSway(uint32_t now, int x) {
  int phase = (int)((now / (WIND_PERIOD_MS / 64) + (uint32_t)(x / 7)) & 63);
  int base = WIND_SIN[phase] * WIND_SWAY_PX;
  int gust = windGust(now, x) * WIND_GUST_PX;
  int v = (base + gust) / 127;
  return v > WIND_MAX_PX ? WIND_MAX_PX : v < -WIND_MAX_PX ? -WIND_MAX_PX : v;
}
