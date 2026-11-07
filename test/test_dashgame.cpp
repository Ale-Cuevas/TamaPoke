// Minijuego de la carrera en trineo (dashgame.h, DASH en docs/NEW_GAMES.md).
#include "framework.h"
#include "../dashgame.h"
#include <cmath>

static bool near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }

TEST(dashgame, direccion_zona_muerta_lineal_tope_y_signo) {
  CHECK(near(dashSteer(0), 0));
  CHECK(near(dashSteer(2.9f), 0));
  CHECK(near(dashSteer(-3.0f), 0));
  CHECK(near(dashSteer(12.5f), 0.5f));
  CHECK(near(dashSteer(-12.5f), -0.5f));
  CHECK(near(dashSteer(22), 1));
  CHECK(near(dashSteer(60), 1));
  CHECK(near(dashSteer(-60), -1));
  for (float a = 3; a < 22; a += 0.5f) CHECK(dashSteer(a + 0.5f) > dashSteer(a));  // sube sin saltos
}

TEST(dashgame, velocidad_1x_2x_a_los_40s_tope_3x) {
  CHECK(near(dashSpeed(0), 1));
  CHECK(near(dashSpeed(20000), 1.5f));
  CHECK(near(dashSpeed(40000), 2));
  CHECK(near(dashSpeed(80000), 3));
  CHECK(near(dashSpeed(200000), 3));
  for (uint32_t t = 0; t < 100000; t += 1000) CHECK(dashSpeed(t + 1000) >= dashSpeed(t));
}

TEST(dashgame, las_filas_se_juntan_y_se_doblan_con_la_velocidad) {
  CHECK(near(dashRowGap(1), DASH_ROW_GAP_START));
  CHECK(near(dashRowGap(3), DASH_ROW_GAP_END));
  CHECK(near(dashDoubleP(1), DASH_DOUBLE_P0));
  CHECK(near(dashDoubleP(3), DASH_DOUBLE_P1));
  // filas por segundo: crecen mas que la velocidad
  float rate1 = DASH_V0 * 1 / dashRowGap(1), rate3 = DASH_V0 * 3 / dashRowGap(3);
  CHECK(rate3 > 3 * rate1);
  CHECK(dashLatMax(3) > dashLatMax(1));
}

// FR-DASH.10 sobre 10 000 filas a cada velocidad: siempre hay un hueco de DASH_GAP_MIN libre, dentro
// de la pista, alcanzable desde el anterior, y los obstaculos no lo pisan ni se salen de la pista
TEST(dashgame, siempre_se_puede_pasar) {
  for (float f : { 1.0f, 1.5f, 2.0f, 2.5f, 3.0f }) {
    uint32_t rng = 2024;
    float prev = 0;
    int twos = 0;
    for (int i = 0; i < 10000; i++) {
      DashRow r = dashNextRow(rng, prev, f);
      float gl = r.gapX - DASH_GAP_MIN / 2, gr = r.gapX + DASH_GAP_MIN / 2;
      CHECK(gl >= -DASH_HALF_W - 1e-3f && gr <= DASH_HALF_W + 1e-3f);
      CHECK(std::fabs(r.gapX - prev) <= dashReach(f) + 1e-3f);
      CHECK(r.n == 1 || r.n == 2);
      for (uint8_t k = 0; k < r.n; k++) {
        const DashObs &o = r.o[k];
        CHECK(o.x1 > o.x0);
        CHECK(o.x0 >= -DASH_HALF_W - 1e-3f && o.x1 <= DASH_HALF_W + 1e-3f);
        CHECK(o.x1 <= gl + 1e-3f || o.x0 >= gr - 1e-3f);  // no pisa el hueco
        CHECK(o.x1 - o.x0 <= DASH_OBS_W_MAX + 1e-3f);
        CHECK(o.kind < DASH_KINDS);
      }
      twos += r.n == 2;
      prev = r.gapX;
    }
    // la proporcion de filas dobles sigue a dashDoubleP (con algo de margen: a veces no caben)
    CHECK(twos < 10000 * (dashDoubleP(f) + 0.03f));
    CHECK(twos > 10000 * (dashDoubleP(f) - 0.05f));
  }
}

// el hueco garantizado de cada fila deja pasar la caja del trineo entera
TEST(dashgame, el_hueco_deja_pasar_el_trineo) {
  CHECK(DASH_GAP_MIN > DASH_HIT_W);
  uint32_t rng = 9;
  for (int i = 0; i < 2000; i++) {
    DashRow r = dashNextRow(rng, 0, 2);
    for (uint8_t k = 0; k < r.n; k++) CHECK(!dashHitsObs(r.gapX, 100, 80, r.o[k]));
  }
}

TEST(dashgame, choques_con_la_caja_encogida) {
  DashObs o = { -50, 50, 0 };  // 100 de ancho, fila en y = 100..136
  CHECK(dashHitsObs(0, 120, 100, o));             // de lleno
  CHECK(!dashHitsObs(0, 99, 100, o));             // aun no ha llegado (trineo y 65..99)
  CHECK(!dashHitsObs(0, 100 + DASH_OBS_H + DASH_HIT_H + 1, 100, o));  // ya paso
  float edge = 50 - 100 * DASH_HIT_SHRINK / 2;    // el borde que choca, encogido
  CHECK(dashHitsObs(edge + DASH_HIT_W / 2 - 1, 120, 100, o));
  CHECK(!dashHitsObs(edge + DASH_HIT_W / 2 + 1, 120, 100, o));  // roza lo dibujado pero no choca
}

// carrera sin mover el trineo: acaba con 3 golpes, con invulnerabilidad entre ellos
TEST(dashgame, tres_golpes_y_se_acaba) {
  DashState d;
  dashStart(d, 1234);
  uint32_t prevHit = 0;
  uint8_t prevHits = 0;
  while (!dashOver(d) && d.t < 600000) {
    dashAdvance(d, 0, 20);
    if (d.hits != prevHits) {
      if (prevHits) CHECK(d.lastHitT - prevHit >= DASH_INVULN_MS);
      prevHit = d.lastHitT;
      prevHits = d.hits;
    }
  }
  CHECK(dashOver(d));
  CHECK_EQ(d.hits, (uint8_t)DASH_LIVES);
  dashAdvance(d, 0, 200);
  CHECK_EQ(d.hits, (uint8_t)DASH_LIVES);  // terminada, ya no avanza
}

// jugador perfecto: se va siempre al hueco de la fila que llega. Esquiva todo durante mucho rato
// (alcanzable a cualquier velocidad, FR-DASH.10)
TEST(dashgame, un_jugador_que_va_al_hueco_no_choca) {
  DashState d;
  dashStart(d, 77);
  while (d.t < 120000 && !dashOver(d)) {
    const DashRow *next = nullptr;
    for (const DashRow &r : d.rows)
      if (r.live && !r.counted && (!next || r.y < next->y)) next = &r;
    float steer = 0;
    if (next) {
      float dx = next->gapX - d.px;
      steer = dx > 3 ? 1 : (dx < -3 ? -1 : dx / 3);
    }
    dashAdvance(d, steer, 20);
  }
  CHECK_EQ(d.hits, (uint8_t)0);
  CHECK(d.dodged > 150);  // 2 minutos, hasta 3x
}

TEST(dashgame, la_fila_que_choca_no_cuenta_la_de_la_invulnerabilidad_si) {
  DashState d;
  dashStart(d, 5);
  // dos filas que cubren toda la pista, seguidas: la primera choca, la segunda cae en la invulnerabilidad
  for (int i = 0; i < 2; i++) {
    DashRow &r = d.rows[i];
    r.live = true;
    r.y = 60 + i * 90;  // la segunda pasa entera dentro de los 1,2 s de invulnerabilidad
    r.n = 1;
    r.o[0] = { -DASH_HALF_W, DASH_HALF_W, 0 };
  }
  d.nextRowY = 100000;  // sin mas filas
  for (int i = 0; i < 100; i++) dashAdvance(d, 0, 20);  // 2 s a DASH_V0: pasan las dos
  CHECK_EQ(d.hits, (uint8_t)1);
  CHECK_EQ(d.dodged, (uint16_t)1);
}

TEST(dashgame, los_bordes_paran_el_trineo_sin_golpe) {
  DashState d;
  dashStart(d, 3);
  d.nextRowY = 1e9f;  // pista vacia
  for (int i = 0; i < 150; i++) dashAdvance(d, 1, 20);
  CHECK(near(d.px, DASH_HALF_W - DASH_HIT_W / 2));
  CHECK(d.bump);
  CHECK_EQ(d.hits, (uint8_t)0);
  for (int i = 0; i < 250; i++) dashAdvance(d, -1, 20);
  CHECK(near(d.px, -(DASH_HALF_W - DASH_HIT_W / 2)));
}

TEST(dashgame, igual_a_30ms_que_a_85ms_por_frame) {
  DashState a, b;
  dashStart(a, 42);
  dashStart(b, 42);
  for (uint32_t t = 0; t < 12750; t += 30) dashAdvance(a, ((t / 1020) % 3) - 1.0f, 30);
  for (uint32_t t = 0; t < 12750; t += 85) dashAdvance(b, ((t / 1020) % 3) - 1.0f, 85);
  CHECK_EQ(a.t, b.t);
  CHECK(near(a.dist, b.dist, 1e-2f));
  CHECK(near(a.px, b.px, 1e-2f));
  CHECK_EQ(a.dodged, b.dodged);
  CHECK_EQ(a.hits, b.hits);
}

TEST(dashgame, un_frame_tardio_cuenta_como_mucho_250ms) {
  DashState d;
  dashStart(d, 1);
  dashAdvance(d, 0, 4000);
  CHECK_EQ(d.t, DASH_LATE_MAX_MS);
}

TEST(dashgame, ganancia_de_vel_1_cada_6_tope_18) {
  CHECK_EQ(dashGain(0), 0);
  CHECK_EQ(dashGain(5), 0);
  CHECK_EQ(dashGain(6), 1);
  CHECK_EQ(dashGain(50), 8);  // "una carrera larga" (v1.48)
  CHECK_EQ(dashGain(108), 18);
  CHECK_EQ(dashGain(500), 18);
}

TEST(dashgame, el_trineo_sigue_al_sprite_entre_topes) {
  CHECK_EQ(dashSledW(10), DASH_SLED_W_MIN);
  CHECK_EQ(dashSledW(80), 80 + DASH_SLED_MARGIN);
  CHECK_EQ(dashSledW(300), DASH_SLED_W_MAX);
  for (int w = 0; w < 300; w++) CHECK(dashSledW(w + 1) >= dashSledW(w));
  // la caja de choque no depende del dibujo
  CHECK(near(DASH_HIT_W, 60));
}

TEST(dashgame, calibracion_media_temblor_y_reintentos) {
  DashCal c;
  for (int i = 0; i < 60; i++) c.add(12.0f + ((i % 2) ? 1.0f : -1.0f));  // 12 +-1
  CHECK(near(c.mean(), 12, 1e-3f));
  CHECK(near(c.sd(), 1, 1e-3f));
  CHECK(!c.shaky(0));
  DashCal s;
  for (int i = 0; i < 60; i++) s.add((i % 2) ? 10.0f : -10.0f);  // temblor de +-10
  CHECK(s.shaky(0));
  CHECK(s.shaky(1));
  CHECK(!s.shaky(2));  // tras 2 repeticiones se acepta
  DashCal e;
  CHECK(near(e.mean(), 0) && near(e.sd(), 0));
}
