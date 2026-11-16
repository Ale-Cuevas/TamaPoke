// Kit de interfaz del patio (yard_ui.h, v2.0 fase Y0): todo lo tocable mide al
// menos 48 px y lo dibujado cae dentro del circulo util.
#include "framework.h"
#include "../yard_ui.h"

TEST(yard_ui, la_paleta_en_rgb565) {
  CHECK_EQ(yRgb(0xFFFFFF), (uint16_t)0xFFFF);
  CHECK_EQ(yRgb(0x000000), (uint16_t)0x0000);
  CHECK_EQ(yRgb(0xFF0000), (uint16_t)0xF800);
  CHECK_EQ(yRgb(0x00FF00), (uint16_t)0x07E0);
  CHECK_EQ(yRgb(0x0000FF), (uint16_t)0x001F);
}

TEST(yard_ui, las_zonas_de_toque_miden_al_menos_48) {
  CHECK(2 * Y_BACK_HIT_R >= Y_HIT_MIN);
  CHECK(2 * Y_DOCK_HIT_R >= Y_HIT_MIN);
  CHECK(Y_CHEV_HIT_W >= Y_HIT_MIN && Y_CHEV_HIT_H >= Y_HIT_MIN);
  CHECK(yPillW(0) >= Y_HIT_MIN);
  // la pastilla, de 28 dibujada, se toca 10 px por encima y por debajo
  CHECK(yPillHit(100, 90, 100, 100, 64));
  CHECK(yPillHit(100, 137, 100, 100, 64));
  CHECK(!yPillHit(100, 89, 100, 100, 64));
  CHECK(!yPillHit(100, 138, 100, 100, 64));
}

TEST(yard_ui, lo_tocable_cae_dentro_del_circulo) {
  CHECK(yInsideSafe(Y_BACK_X, Y_BACK_Y, Y_BACK_HIT_R));
  for (int i = 0; i < Y_DOCK_N; i++) {
    CHECK(yInsideSafe(yDockX(i), Y_DOCK_Y, Y_DOCK_HIT_R));
  }
  CHECK(yInsideSafe(Y_CHEV_LX, Y_CY, Y_CHEV_HALF));
  CHECK(yInsideSafe(Y_CHEV_RX, Y_CY, Y_CHEV_HALF));
}

TEST(yard_ui, el_dock_y_el_volver_no_se_pisan) {
  // los circulos de toque vecinos del dock no se solapan
  CHECK(Y_DOCK_STEP >= 2 * Y_DOCK_HIT_R);
  CHECK_EQ(yDockHit(yDockX(0), Y_DOCK_Y), 0);
  CHECK_EQ(yDockHit(yDockX(1), Y_DOCK_Y), 1);
  CHECK_EQ(yDockHit(yDockX(2), Y_DOCK_Y), 2);
  CHECK_EQ(yDockHit(Y_CX, 233), -1);
  // el dock queda en la franja reservada del mapa (y 380-440, x 150-316 del diseno)
  CHECK(Y_DOCK_Y - Y_DOCK_R >= 380 - 10 && Y_DOCK_Y + Y_DOCK_R <= 440);
  CHECK(yDockX(0) - Y_DOCK_HIT_R >= 130 && yDockX(2) + Y_DOCK_HIT_R <= 336);
  CHECK(Y_DOCK_STEP - 2 * Y_DOCK_HIT_R >= 16);  // hueco muerto entre botones: sin toques al vecino
  CHECK(yBackHit(Y_BACK_X, Y_BACK_Y));
  CHECK(!yBackHit(Y_BACK_X, Y_BACK_Y - Y_BACK_HIT_R - 1));
}

TEST(yard_ui, las_flechas) {
  CHECK_EQ(yChevronHit(Y_CHEV_LX, Y_CY), -1);
  CHECK_EQ(yChevronHit(Y_CHEV_RX, Y_CY), 1);
  CHECK_EQ(yChevronHit(Y_CX, Y_CY), 0);
  CHECK_EQ(yChevronHit(Y_CHEV_LX, Y_CY - Y_CHEV_HIT_H / 2 - 1), 0);
}
