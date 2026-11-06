// Viento del paisaje (wind.h, v1.26).
#include "framework.h"
#include "../wind.h"

TEST(wind, la_inclinacion_nunca_se_sale_del_rango) {
  for (uint32_t t = 0; t < 2 * WIND_GUST_MS; t += 37)
    for (int x = -60; x < 530; x += 5) {
      int v = windSway(t, x);
      CHECK(v >= -WIND_MAX_PX && v <= WIND_MAX_PX);
    }
}

TEST(wind, el_mismo_instante_da_el_mismo_dibujo) {
  for (uint32_t t : { 0u, 1234u, 99999u, 4000000000u })
    for (int x : { 0, 100, 233, 465 }) CHECK_EQ(windSway(t, x), windSway(t, x));
  // la rafaga se repite cada WIND_GUST_MS
  CHECK_EQ(windGust(500, 200), windGust(500 + WIND_GUST_MS, 200));
}

TEST(wind, la_rafaga_cruza_de_izquierda_a_derecha) {
  // a la izquierda llega antes que a la derecha
  uint32_t tl = 0, tr = 0;
  for (uint32_t t = 0; t < WIND_GUST_MS; t += 10) {
    if (!tl && windGust(t, 40) == 127) tl = t;
    if (!tr && windGust(t, 420) >= 126) tr = t;
  }
  CHECK(tl > 0 && tr > tl);
  // en medio de la rafaga todo se inclina a la derecha
  CHECK(windSway(tl, 40) > WIND_SWAY_PX);
  // fuera de ella solo queda el vaiven
  int maxCalm = 0;
  for (uint32_t t = 0; t < WIND_PERIOD_MS; t += 10) {
    if (windGust(t + tl + WIND_GUST_MS / 2, 40)) continue;
    int v = windSway(t + tl + WIND_GUST_MS / 2, 40);
    if (v < 0) v = -v;
    if (v > maxCalm) maxCalm = v;
  }
  CHECK(maxCalm <= WIND_SWAY_PX);
}

TEST(wind, columnas_vecinas_ondulan_desfasadas) {
  int diff = 0;
  for (uint32_t t = 0; t < WIND_PERIOD_MS; t += 50)
    if (windSway(t, 0) != windSway(t, 230)) diff++;
  CHECK(diff > 10);
}
