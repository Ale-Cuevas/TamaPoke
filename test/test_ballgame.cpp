// Minijuego de la pokeball (ballgame.h, v1.33): la velocidad sube con la puntuacion.
#include "framework.h"
#include "../ballgame.h"
#include <cmath>

static bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

TEST(ballgame, empieza_a_la_velocidad_de_salida_y_sube_cada_punto) {
  const float slope = (BALL_SPEED_MAX - BALL_SPEED_START) / (float)(BALL_SPEED_TOP_AT - 1);
  CHECK(near(ballSpeed(0), BALL_SPEED_START));
  CHECK(near(ballSpeed(1), BALL_SPEED_START));
  CHECK(near(ballSpeed(40), BALL_SPEED_START + 39.0f * slope));
  CHECK(near(ballSpeed(2), BALL_SPEED_START + slope));
  // v3.17, promesa de diseno: 1,5x con 1 punto, en linea recta hasta 3,5x con 100 puntos, y ese es el tope
  CHECK(near(BALL_SPEED_START, 1.5f));
  CHECK(near(BALL_SPEED_MAX, 3.5f));
  CHECK_EQ((int)BALL_SPEED_TOP_AT, 100);
  CHECK(ballSpeed(100) == BALL_SPEED_MAX);  // exacto, sin error de redondeo
}

TEST(ballgame, sigue_subiendo_hasta_el_tope) {
  int capAt = BALL_SPEED_TOP_AT;
  CHECK(ballSpeed(capAt - 1) < BALL_SPEED_MAX);
  CHECK(near(ballSpeed(capAt), BALL_SPEED_MAX));
  CHECK(near(ballSpeed(255), BALL_SPEED_MAX));
  for (uint16_t s = 1; s < 300; s++) CHECK(ballSpeed(s) >= ballSpeed(s - 1));  // nunca baja
  for (uint16_t s = 2; s <= capAt; s++) CHECK(ballSpeed(s) > ballSpeed(s - 1));  // y sube con cada punto
}

TEST(ballgame, el_frame_tardio_se_recorta_antes_de_la_velocidad) {
  float s1 = ballSpeed(1), s80 = ballSpeed(80);
  CHECK(near(ballStepK(85, 1), s1));
  CHECK(near(ballStepK(85, 80), s80));
  CHECK(near(ballStepK(170, 80), 2.0f * s80));
  CHECK(near(ballStepK(1000, 1), BALL_LATE_MAX * s1));    // 1 s de frame: como mucho 3 pasos
  CHECK(near(ballStepK(1000, 80), BALL_LATE_MAX * s80));  // ...por la velocidad: el tope no se la come
}

// frames de 85 ms que la bola pasa en el aire tras un toque, a la velocidad de `score`;
// lejos de la pared para que solo cuente la gravedad. Tambien devuelve la altura del arco.
static int framesInAir(uint16_t score, float *apex) {
  BallState b = { BALL_CX, 300.0f, 0, 0 };
  ballHit(b, 0);
  float k = ballStepK(85, score), y0 = b.y, top = b.y;
  int n = 0;
  do {
    ballStep(b, k);
    if (b.y < top) top = b.y;
    n++;
  } while (b.y < y0 && n < 1000);
  if (apex) *apex = y0 - top;
  return n;
}

TEST(ballgame, el_tiempo_en_el_aire_baja_con_la_velocidad) {
  // 2 * BALL_LIFT / BALL_GRAV = 33 pasos en el aire; a velocidad v son unos 33 / v frames de 85 ms
  float apex1;
  int f1 = framesInAir(1, &apex1);
  CHECK(apex1 > 50.0f && apex1 < 60.0f);
  const uint16_t scores[] = { 1, 40, 80, 120, 200 };
  for (uint16_t sc : scores) {
    float apex;
    int f = framesInAir(sc, &apex);
    float steps = f * ballSpeed(sc);
    CHECK(steps >= 33.0f && steps <= 33.0f + 1.0f + ballSpeed(sc));
    CHECK(std::fabs(apex - apex1) < 3.0f);  // el mismo arco: la parabola no depende del paso
  }
  // a mas velocidad, menos tiempo en el aire
  CHECK(framesInAir(80, nullptr) < f1);
  CHECK(framesInAir(200, nullptr) < framesInAir(80, nullptr));
}

TEST(ballgame, la_forma_del_arco_no_cambia_con_la_puntuacion) {
  // el toque da siempre el mismo impulso y la salida la misma deriva
  BallState a = { 200, 200, 0, 0 }, b = a;
  ballHit(a, 10);
  ballHit(b, 10);
  CHECK(near(a.vy, -BALL_LIFT));
  CHECK(near(a.vx, 10 * BALL_NUDGE));
  BallState s;
  ballSpawn(s, 180, 1);
  CHECK(near(s.vx, BALL_SPAWN_VX) && near(s.y, BALL_SPAWN_Y) && near(s.vy, 0));
  ballSpawn(s, 180, -1);
  CHECK(near(s.vx, -BALL_SPAWN_VX));
  // el empujon lateral tiene tope
  BallState c = { 200, 200, 0, 0 };
  ballHit(c, 500);
  CHECK(near(c.vx, BALL_VX_MAX));
  ballHit(c, -2000);
  CHECK(near(c.vx, -BALL_VX_MAX));
}

TEST(ballgame, rebota_en_la_pared_y_no_se_sale) {
  for (uint16_t score : { 1, 80, 159 }) {
    BallState b;
    ballSpawn(b, 330, 1);
    b.vx = BALL_VX_MAX;
    float k = ballStepK(85, score);
    bool bounced = false;
    for (int i = 0; i < 400 && !ballOnFloor(b); i++) {
      float vx = b.vx;
      ballStep(b, k);
      float dx = b.x - BALL_CX, dy = b.y - BALL_CY;
      CHECK(std::sqrt(dx * dx + dy * dy) <= BALL_WALL_R + 0.01f);
      if (vx > 0 && b.vx < 0) bounced = true;
    }
    CHECK(bounced);
    CHECK(ballOnFloor(b));  // y acaba cayendo al suelo
  }
}
