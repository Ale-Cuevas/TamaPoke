// Paneles Cuidar y Jugar del patio (yard_panels.h, v2.0 fase Y2): cada tarjeta se toca en al
// menos 48 px, cae entera dentro del circulo util, no pisa a otra ni al boton de volver.
#include "framework.h"
#include "../yard_panels.h"

static void checkPanel(uint8_t kind) {
  uint8_t n = panelCount(kind);
  CHECK(n > 0);
  for (uint8_t i = 0; i < n; i++) {
    int x, y, w, h;
    CHECK(panelCard(kind, i, x, y, w, h));
    CHECK(w >= Y_HIT_MIN && h >= Y_HIT_MIN);
    // las cuatro esquinas dentro del radio util
    CHECK(yInsideSafe(x, y, 0));
    CHECK(yInsideSafe(x + w, y, 0));
    CHECK(yInsideSafe(x, y + h, 0));
    CHECK(yInsideSafe(x + w, y + h, 0));
    // por debajo del titulo y por encima del boton de volver
    CHECK(y >= Y_SUB_Y);
    CHECK(y + h < Y_BACK_Y - Y_BACK_HIT_R);
    // el centro de cada tarjeta la devuelve a ella (no se solapan)
    CHECK_EQ(panelHit(kind, x + w / 2, y + h / 2), (int)i);
    CHECK_EQ(panelHit(kind, x, y), (int)i);
    CHECK_EQ(panelHit(kind, x + w - 1, y + h - 1), (int)i);
  }
  CHECK_EQ(panelHit(kind, Y_BACK_X, Y_BACK_Y), -1);
  CHECK_EQ(panelHit(kind, Y_CX, Y_TITLE_Y), -1);
}

TEST(yard_panels, cuidar_tarjetas_tocables_y_dentro) { checkPanel(PANEL_CARE); }

TEST(yard_panels, jugar_tarjetas_tocables_y_dentro) { checkPanel(PANEL_PLAY); }

TEST(yard_panels, sin_panel_no_hay_tarjetas) {
  CHECK_EQ(panelCount(PANEL_NONE), (uint8_t)0);
  CHECK_EQ(panelHit(PANEL_NONE, Y_CX, Y_CY), -1);
  CHECK_EQ(panelCount(PANEL_CARE), (uint8_t)6);
  CHECK_EQ(panelCount(PANEL_PLAY), (uint8_t)6);
  CHECK_EQ(panelCount(PANEL_TRAVEL), (uint8_t)0);  // la pantalla de viajes tiene su propia geometria (travel_ui.h)
}
