// Minijuego de la pokeball (ballgame.h, v1.33): la velocidad sube con la puntuacion.
#include "framework.h"
#include "../ballgame.h"
#include <cmath>

static bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

TEST(ballgame, el_doble_de_rapida_con_80_que_con_1) {
  CHECK(near(ballSpeed(0), 1.0f));
  CHECK(near(ballSpeed(1), 1.0f));
  CHECK(near(ballSpeed(40), 1.0f + 39.0f / 79.0f));
  CHECK(near(ballSpeed(80), 2.0f));
}

TEST(ballgame, sigue_subiendo_pasados_los_80_hasta_3x) {
  CHECK(ballSpeed(120) > ballSpeed(80));
  CHECK(near(ballSpeed(159), 3.0f));
  CHECK(near(ballSpeed(160), 3.0f));
  CHECK(near(ballSpeed(255), 3.0f));
  for (uint16_t s = 1; s < 300; s++) CHECK(ballSpeed(s) >= ballSpeed(s - 1));  // nunca baja
  for (uint16_t s = 2; s <= 159; s++) CHECK(ballSpeed(s) > ballSpeed(s - 1));  // y sube con cada punto
}

TEST(ballgame, el_frame_tardio_se_recorta_antes_de_la_velocidad) {
  CHECK(near(ballStepK(85, 1), 1.0f));
  CHECK(near(ballStepK(85, 80), 2.0f));
  CHECK(near(ballStepK(170, 80), 4.0f));
  CHECK(near(ballStepK(1000, 1), 3.0f));   // 1 s de frame: como mucho 3 pasos
  CHECK(near(ballStepK(1000, 80), 6.0f));  // ...a doble velocidad, 6: el tope no se come la velocidad
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

TEST(ballgame, con_80_puntos_esta_la_mitad_de_tiempo_en_el_aire) {
  float apex1, apex80;
  int f1 = framesInAir(1, &apex1), f80 = framesInAir(80, &apex80);
  // 2 * BALL_LIFT / BALL_GRAV = 33 pasos en el aire: 34 frames a 1x, 17 a 2x
  CHECK(f1 >= 33 && f1 <= 34);
  CHECK(std::abs(2 * f80 - f1) <= 1);
  // el mismo arco: la parabola es exacta, no depende de lo largo del paso
  CHECK(apex1 > 50.0f && apex1 < 60.0f);
  CHECK(std::fabs(apex80 - apex1) < 2.0f);
  float apex159;
  int f159 = framesInAir(159, &apex159);  // 3x: un tercio
  CHECK(std::abs(3 * f159 - f1) <= 2);
  CHECK(std::fabs(apex159 - apex1) < 3.0f);
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
