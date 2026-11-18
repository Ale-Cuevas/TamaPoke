// Paneles y menus del patio (yard_panels.h): el panel Jugar (v2.0 fase Y2), el menu de un bicho, su lista de
// comida y el selector de bicho (v3.3). Cada zona se toca en al menos 48 px, cae entera dentro del circulo util
// y no pisa a otra, ni al dock ni al boton de volver.
#include "framework.h"
#include "../yard_panels.h"

// las cuatro esquinas del rectangulo dentro del radio util
static bool rectInside(int x, int y, int w, int h) {
  return yInsideSafe(x, y, 0) && yInsideSafe(x + w, y, 0) && yInsideSafe(x, y + h, 0) && yInsideSafe(x + w, y + h, 0);
}
// el rectangulo toca el circulo (cx, cy, r): el punto del rectangulo mas cercano al centro cae dentro
static bool rectHitsCircle(int x, int y, int w, int h, int cx, int cy, int r) {
  int nx = cx < x ? x : cx > x + w - 1 ? x + w - 1 : cx;
  int ny = cy < y ? y : cy > y + h - 1 ? y + h - 1 : cy;
  return yInCircle(nx, ny, cx, cy, r);
}
static bool rectsOverlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
  return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}
// ni los circulos tocables del dock ni el boton de volver
static bool clearOfDockAndBack(int x, int y, int w, int h) {
  for (int d = 0; d < Y_DOCK_N; d++)
    if (rectHitsCircle(x, y, w, h, yDockX(d), Y_DOCK_Y, Y_DOCK_HIT_R)) return false;
  return !rectHitsCircle(x, y, w, h, Y_BACK_X, Y_BACK_Y, Y_BACK_HIT_R);
}

static void checkPanel(uint8_t kind) {
  uint8_t n = panelCount(kind);
  CHECK(n > 0);
  for (uint8_t i = 0; i < n; i++) {
    int x, y, w, h;
    CHECK(panelCard(kind, i, x, y, w, h));
    CHECK(w >= Y_HIT_MIN && h >= Y_HIT_MIN);
    CHECK(rectInside(x, y, w, h));
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

TEST(yard_panels, jugar_tarjetas_tocables_y_dentro) { checkPanel(PANEL_PLAY); }

TEST(yard_panels, sin_panel_no_hay_tarjetas) {
  CHECK_EQ(panelCount(PANEL_NONE), (uint8_t)0);
  CHECK_EQ(panelHit(PANEL_NONE, Y_CX, Y_CY), -1);
  CHECK_EQ(panelCount(PANEL_PLAY), (uint8_t)6);
  CHECK_EQ(panelCount(PANEL_TRAVEL), (uint8_t)0);  // la pantalla de viajes tiene su propia geometria (travel_ui.h)
}

// ---- el menu de un bicho (v3.3) ----

// n = 1..PM_MAX filas: cada una tocable (48 px), dentro del circulo util, sin pisarse, ni el dock ni el boton de
// volver, debajo de la cartera del HUD; su centro la devuelve a ella
TEST(yard_panels, menu_del_bicho_filas_tocables_y_dentro) {
  for (uint8_t n = 1; n <= PM_MAX; n++) {
    for (uint8_t i = 0; i < n; i++) {
      int x, y, w, h;
      CHECK(petMenuRow(i, n, x, y, w, h));
      CHECK(w >= Y_HIT_MIN && h >= Y_HIT_MIN);
      CHECK_EQ(w, PM_W);
      CHECK(rectInside(x, y, w, h));
      CHECK(clearOfDockAndBack(x, y, w, h));
      CHECK(y >= PM_TOP);
      CHECK(y + h <= PM_BOTTOM);
      CHECK_EQ(petMenuHit(n, x + w / 2, y + h / 2), (int)i);
      CHECK_EQ(petMenuHit(n, x, y), (int)i);
      CHECK_EQ(petMenuHit(n, x + w - 1, y + h - 1), (int)i);
      for (uint8_t j = 0; j < i; j++) {
        int x2, y2, w2, h2;
        CHECK(petMenuRow(j, n, x2, y2, w2, h2));
        CHECK(!rectsOverlap(x, y, w, h, x2, y2, w2, h2));
      }
    }
    int x, y, w, h;
    CHECK(!petMenuRow(n, n, x, y, w, h));  // no hay fila n
  }
}

// v3.8/v3.9: el nombre y la caja de necesidades, entre la cartera del HUD y la primera fila, dentro del circulo
// util y sin pisarse, con cualquier numero de filas
TEST(yard_panels, menu_del_bicho_cabecera_con_necesidades) {
  for (uint8_t n = 1; n <= PM_MAX; n++) {
    int x, y, w, h;
    CHECK(petMenuRow(0, n, x, y, w, h));
    int ny = petMenuNameY(n), by = petMenuNeedsY(n);
    CHECK(ny >= PM_HUD_BOTTOM);               // debajo de la cartera
    CHECK(ny + PM_NAME_H <= by);              // el nombre encima de la caja
    CHECK(by + PM_NEED_BOX_H + 3 <= y);       // la caja (y su sombra) encima de la columna
    int br = petMenuHeadRight(n, by, PM_NEED_BOX_H);
    CHECK(br <= x + PM_W);
    CHECK(br - PM_NEED_BOX_W >= x);           // no sobresale por la izquierda de la columna
    CHECK(rectInside(br - PM_NEED_BOX_W, by, PM_NEED_BOX_W, PM_NEED_BOX_H));
    int nr = petMenuHeadRight(n, ny, PM_NAME_H);
    CHECK(rectInside(nr - 40, ny, 40, PM_NAME_H));
  }
}

TEST(yard_panels, menu_del_bicho_a_la_derecha_y_centrado) {
  // hasta 4 filas la columna queda centrada en Y_CY, con el hueco de siempre, y en la mitad derecha de la
  // pantalla (con 5, v3.9, baja un poco para dejar sitio a la cabecera; abajo)
  for (uint8_t n = 1; n <= 4; n++) {
    int x0, y0, w, h, xl, yl;
    CHECK(petMenuRow(0, n, x0, y0, w, h));
    CHECK(petMenuRow((uint8_t)(n - 1), n, xl, yl, w, h));
    int mid = (y0 + yl + h) / 2;
    CHECK(mid >= Y_CY - 1 && mid <= Y_CY + 1);
    CHECK_EQ(petMenuGap(n), PM_GAP);
    CHECK(x0 > Y_CX);
    // el borde derecho casi toca el circulo: PM_EDGE mas a la derecha ya se saldria por una esquina
    CHECK(!rectInside(x0 + PM_EDGE + 1, y0, w, yl + h - y0));
  }
  // con 5 filas (v3.9): empieza en PM_TOP, bajo la cabecera, y el hueco se encoge para no pisar el dock
  int x, y, w, h;
  CHECK(petMenuRow(0, 5, x, y, w, h));
  CHECK_EQ(y, PM_TOP);
  CHECK(x > Y_CX);
  CHECK(petMenuGap(5) < PM_GAP && petMenuGap(5) >= 2);
  CHECK(petMenuRow(4, 5, x, y, w, h));
  CHECK(y + h <= PM_BOTTOM);
}

TEST(yard_panels, menu_del_bicho_fuera_no_toca_nada) {
  for (uint8_t n = 1; n <= PM_MAX; n++) {
    int x, y, w, h;
    CHECK(petMenuRow(0, n, x, y, w, h));
    CHECK_EQ(petMenuHit(n, x - 1, y + h / 2), -1);        // a la izquierda de la columna
    CHECK_EQ(petMenuHit(n, x + w, y + h / 2), -1);        // a su derecha
    CHECK_EQ(petMenuHit(n, x + w / 2, y - 1), -1);        // encima de la primera
    CHECK(petMenuRow((uint8_t)(n - 1), n, x, y, w, h));
    CHECK_EQ(petMenuHit(n, x + w / 2, y + h), -1);        // debajo de la ultima
    CHECK_EQ(petMenuHit(n, Y_CX - 100, Y_CY), -1);        // el patio de la izquierda
    for (int d = 0; d < Y_DOCK_N; d++) CHECK_EQ(petMenuHit(n, yDockX(d), Y_DOCK_Y), -1);
    if (n > 1 && petMenuGap(n) > 0) {                     // el hueco entre dos filas
      CHECK(petMenuRow(0, n, x, y, w, h));
      CHECK_EQ(petMenuHit(n, x + w / 2, y + h), -1);
    }
  }
  CHECK_EQ(petMenuHit(0, Y_CX, Y_CY), -1);
  int x, y, w, h;
  CHECK(!petMenuRow(0, 0, x, y, w, h));
  CHECK(!petMenuRow(0, PM_MAX + 1, x, y, w, h));
}

// ---- la lista de comida ----

TEST(yard_panels, comida_paginas_de_cuatro_mas_volver) {
  CHECK_EQ(foodPages(0), (uint8_t)1);
  CHECK_EQ(foodPages(1), (uint8_t)1);
  CHECK_EQ(foodPages(FOOD_PER_PAGE), (uint8_t)1);
  CHECK_EQ(foodPages(FOOD_PER_PAGE + 1), (uint8_t)2);
  CHECK_EQ(foodPages(9), (uint8_t)3);  // la baya normal, 6 de colores, la tarta y la dorada
  CHECK_EQ(foodOnPage(9, 0), (uint8_t)FOOD_PER_PAGE);
  CHECK_EQ(foodOnPage(9, 2), (uint8_t)1);
  CHECK_EQ(foodOnPage(3, 1), (uint8_t)0);
  // nunca mas filas que el menu con Evolucionar: la misma columna
  for (uint8_t f = 1; f <= 9; f++)
    for (uint8_t p = 0; p < foodPages(f); p++) {
      CHECK(foodListRows(f, p) >= 2);
      CHECK(foodListRows(f, p) <= 5);
    }
}

TEST(yard_panels, comida_fila_de_volver_y_mas) {
  for (uint8_t n = 2; n <= 5; n++) {
    for (int more = 0; more <= 1; more++) {
      // las comidas: su centro devuelve su indice
      for (uint8_t i = 0; i + 1 < n; i++) {
        int x, y, w, h;
        CHECK(petMenuRow(i, n, x, y, w, h));
        CHECK_EQ(foodListHit(n, more, x + w / 2, y + h / 2), (int)i);
      }
      int bx, by, bw, bh, mx, my, mw, mh;
      CHECK(foodNavCell(n, more, 0, bx, by, bw, bh));
      CHECK(bw >= Y_HIT_MIN && bh >= Y_HIT_MIN);
      CHECK(rectInside(bx, by, bw, bh));
      CHECK(clearOfDockAndBack(bx, by, bw, bh));
      CHECK_EQ(foodListHit(n, more, bx + bw / 2, by + bh / 2), (int)FL_BACK);
      if (more) {
        CHECK(foodNavCell(n, more, 1, mx, my, mw, mh));
        CHECK(mw >= Y_HIT_MIN && mh >= Y_HIT_MIN);
        CHECK(rectInside(mx, my, mw, mh));
        CHECK(!rectsOverlap(bx, by, bw, bh, mx, my, mw, mh));
        CHECK(mx > bx);
        CHECK_EQ(foodListHit(n, more, mx + mw / 2, my + mh / 2), (int)FL_MORE);
        CHECK_EQ(foodListHit(n, more, bx + bw + FL_NAV_GAP / 2, by + bh / 2), -1);  // entre las dos
      } else {
        CHECK(!foodNavCell(n, more, 1, mx, my, mw, mh));
        CHECK_EQ(bw, PM_W);  // volver ocupa toda la fila
      }
      CHECK_EQ(foodListHit(n, more, Y_CX - 100, Y_CY), -1);
    }
  }
  int x, y, w, h;
  CHECK(!foodNavCell(0, false, 0, x, y, w, h));
}

// ---- el selector de bicho ----

TEST(yard_panels, selector_dos_tarjetas_tocables_y_dentro) {
  int x0, y0, w0, h0, x1, y1, w1, h1;
  petPickCard(0, x0, y0, w0, h0);
  petPickCard(1, x1, y1, w1, h1);
  for (int i = 0; i < 2; i++) {
    int x = i ? x1 : x0, y = i ? y1 : y0, w = i ? w1 : w0, h = i ? h1 : h0;
    CHECK(w >= Y_HIT_MIN && h >= Y_HIT_MIN);
    CHECK(rectInside(x, y, w, h));
    CHECK(clearOfDockAndBack(x, y, w, h));
    CHECK(y >= Y_SUB_Y + 14);  // debajo del titulo y su subtitulo
    CHECK_EQ(petPickHit(x + w / 2, y + h / 2), i);
    CHECK_EQ(petPickHit(x, y), i);
    CHECK_EQ(petPickHit(x + w - 1, y + h - 1), i);
  }
  CHECK(!rectsOverlap(x0, y0, w0, h0, x1, y1, w1, h1));
  CHECK_EQ(y0, y1);
  CHECK(x0 < Y_CX && x1 > Y_CX);  // una a cada lado
  CHECK_EQ(petPickHit(Y_CX, y0 + h0 / 2), -1);  // el hueco del medio
  CHECK_EQ(petPickHit(Y_BACK_X, Y_BACK_Y), -1);
  CHECK_EQ(petPickHit(Y_CX, Y_TITLE_Y), -1);
}
