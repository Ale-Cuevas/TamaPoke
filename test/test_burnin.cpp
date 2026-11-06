// Proteccion contra el quemado del AMOLED (burnin.h, v1.28).
#include "framework.h"
#include "../ui_frame.h"
#include "../burnin.h"
#include <cmath>
#include <set>

TEST(burnin, la_orbita_tiene_9_posiciones_pares_dentro_del_panel) {
  std::set<int> seen;
  for (uint32_t s = 0; s < ORBIT_POSITIONS; s++) {
    OrbitShift o = orbitShift(s);
    CHECK(o.dx % 2 == 0 && o.dy % 2 == 0);  // el CO5300 solo acepta ventanas en pixel par
    CHECK(o.dx >= -ORBIT_MAX_PX && o.dx <= ORBIT_MAX_PX);
    CHECK(o.dy >= -ORBIT_MAX_PX && o.dy <= ORBIT_MAX_PX);
    int ox = UI_ORIGIN + o.dx, oy = UI_ORIGIN + o.dy;
    CHECK(ox >= 0 && ox + UI_SIZE <= BOARD_LCD_WIDTH);
    CHECK(oy >= 0 && oy + UI_SIZE <= BOARD_LCD_HEIGHT);
    seen.insert(o.dx * 100 + o.dy);
  }
  CHECK_EQ((int)seen.size(), ORBIT_POSITIONS);
  CHECK_EQ(orbitShift(0).dx, (int8_t)0);  // empieza donde siempre
  CHECK_EQ(orbitShift(0).dy, (int8_t)0);
  CHECK_EQ(orbitShift(ORBIT_POSITIONS).dx, orbitShift(0).dx);  // y vuelve a empezar
}

TEST(burnin, cada_paso_de_la_orbita_es_pequeno) {
  for (uint32_t s = 1; s < 2 * ORBIT_POSITIONS; s++) {
    OrbitShift a = orbitShift(s - 1), b = orbitShift(s);
    int d = abs(a.dx - b.dx) + abs(a.dy - b.dy);
    CHECK(d >= 1 && d <= 2 * ORBIT_STEP_PX);  // se mueve siempre, nunca de un lado al otro
  }
  CHECK_EQ(orbitStepAt(ORBIT_STEP_MS - 1), 0u);
  CHECK_EQ(orbitStepAt(ORBIT_STEP_MS), 1u);
}

// las esquinas del bicho y de la hora caen siempre dentro del marco redondo
TEST(burnin, el_salvapantallas_no_se_sale_del_circulo) {
  auto inside = [](int px, int py) {
    double dx = px - SAVER_CX, dy = py - SAVER_CY;
    return std::sqrt(dx * dx + dy * dy) <= SAVER_INNER_R;
  };
  for (uint32_t t = 0; t < 2 * SAVER_PERIOD_Y_MS; t += 997) {
    int x, y;
    saverPos(t, &x, &y);
    for (int sx : { -1, 1 }) {
      CHECK(inside(x + sx * SAVER_CLOCK_HALF_W, y));                  // hora: arriba
      CHECK(inside(x + sx * SAVER_CLOCK_HALF_W, y + SAVER_CLOCK_H));  // y abajo
      CHECK(inside(x + sx * SAVER_PET_HALF_W, y - SAVER_PET_GAP - SAVER_PET_H));  // cabeza del bicho
    }
  }
}

TEST(burnin, el_salvapantallas_va_despacio_y_recorre_la_pantalla) {
  int minX = 999, maxX = -999, minY = 999, maxY = -999;
  int px, py;
  saverPos(0, &px, &py);
  for (uint32_t t = 1000; t < SAVER_PERIOD_Y_MS; t += 1000) {  // un frame por segundo, como a oscuras
    int x, y;
    saverPos(t, &x, &y);
    CHECK(abs(x - px) <= 4 && abs(y - py) <= 4);  // sin saltos de un frame a otro
    px = x;
    py = y;
    if (x < minX) minX = x;
    if (x > maxX) maxX = x;
    if (y < minY) minY = y;
    if (y > maxY) maxY = y;
  }
  CHECK(maxX - minX > SAVER_RX && maxY - minY > SAVER_RY);  // y llega lejos en los dos ejes
}
