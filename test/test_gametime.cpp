// Fisica por tiempo real (gametime.h, v3.23): el mismo tiempo simulado da el mismo resultado a 85 ms por
// frame (el ritmo de la v3.22) y a GAME_FRAME_MS. Los numeros salen de tuning.h.
#include "framework.h"
#include "../gametime.h"
#include "../ballgame.h"
#include "../dashgame.h"
#include "../zengame.h"
#include <cmath>

static bool near(float a, float b, float eps) { return std::fabs(a - b) <= eps; }

TEST(gametime, el_ritmo_de_juego_cabe_en_un_frame_medido) {
  // el render mas lento medido (trineo, ~56 ms, docs/SMOOTH_GAMES.md) tiene que caber en el intervalo
  CHECK(GAME_FRAME_MS >= 56);
  CHECK(GAME_FRAME_MS < GAME_PHYS_REF_MS);
}

TEST(gametime, escala_y_decaimiento_por_tiempo) {
  CHECK(near(gameDtScale(GAME_PHYS_REF_MS), 1.0f, 1e-5f));
  CHECK(near(gameDtScale(10000), 2.0f, 1e-5f));  // un frame tardio se recorta
  CHECK(near(gameDecay(0.84f, GAME_PHYS_REF_MS), 0.84f, 1e-5f));
  float two = gameDecay(0.84f, GAME_FRAME_MS) * gameDecay(0.84f, GAME_FRAME_MS);
  CHECK(near(two, gameDecay(0.84f, 2 * GAME_FRAME_MS), 1e-5f));
}

TEST(gametime, la_sacudida_del_saco_decae_igual_por_segundo) {
  float a = 16, b = 16;
  // 2040 ms = 24 x 85 = 34 x 60
  for (uint32_t t = 0; t < 2040; t += GAME_PHYS_REF_MS) a *= gameDecay(0.84f, GAME_PHYS_REF_MS);
  for (uint32_t t = 0; t < 2040; t += GAME_FRAME_MS) b *= gameDecay(0.84f, GAME_FRAME_MS);
  CHECK(near(a, b, 0.001f));
  CHECK(near(a, 16 * std::pow(0.84f, 24), 1e-3f));  // a 85 ms, exactamente lo de antes
}

// la bola y el bicho que la sigue, `ms` de reloj en frames de `frame` ms (sin toques: parabola y pared)
struct BallRun { BallState b; float pet; };
static BallRun runBall(uint32_t frame, uint32_t ms, uint16_t score, float vx) {
  BallRun r;
  ballSpawn(r.b, 233, 1);
  r.b.vx = vx;
  r.pet = 233;
  for (uint32_t t = 0; t < ms; t += frame) {
    ballStep(r.b, ballStepK(frame, score));
    r.pet = ballPetFollow(r.pet, r.b.x, frame, score);
  }
  return r;
}

TEST(gametime, la_pelota_cae_igual_a_85_y_a_60_ms) {
  // 1020 ms = 12 x 85 = 17 x 60. A 1 y a 60 puntos, sin tocar la pared: la parabola es exacta
  const uint16_t scores[] = { 1, 30, 60 };
  for (uint16_t sc : scores) {
    BallRun a = runBall(GAME_PHYS_REF_MS, 1020, sc, 0.5f), b = runBall(GAME_FRAME_MS, 1020, sc, 0.5f);
    CHECK_MSG(near(a.b.y, b.b.y, 0.5f), "y igual");
    CHECK_MSG(near(a.b.x, b.b.x, 0.5f), "x igual");
    CHECK_MSG(near(a.b.vy, b.b.vy, 0.05f), "vy igual");
    CHECK_MSG(near(a.pet, b.pet, 1.0f), "el bicho llega al mismo sitio");
  }
}

TEST(gametime, el_rebote_en_la_pared_sale_parecido) {
  // con rebote (vx fuerte): una vez rebotado, la salida (velocidad) es la misma y la posicion difiere
  // como mucho lo que anda la bola en un paso del frame mas largo
  BallRun a = runBall(GAME_PHYS_REF_MS, 1020, 1, 6.0f), b = runBall(GAME_FRAME_MS, 1020, 1, 6.0f);
  CHECK(near(a.b.vx, b.b.vx, 0.4f));
  CHECK(near(a.b.vy, b.b.vy, 0.4f));
  CHECK(near(a.b.x, b.b.x, 25.0f));
}

TEST(gametime, el_bicho_sigue_a_la_bola_igual_por_segundo) {
  // bola quieta cerca: sin llegar al tope, a los dos ritmos recorre lo mismo en 1020 ms
  const uint16_t scores[] = { 1, 50, 100 };
  for (uint16_t sc : scores) {
    float a = 233, b = 233;
    for (uint32_t t = 0; t < 1020; t += GAME_PHYS_REF_MS) a = ballPetFollow(a, 260, GAME_PHYS_REF_MS, sc);
    for (uint32_t t = 0; t < 1020; t += GAME_FRAME_MS) b = ballPetFollow(b, 260, GAME_FRAME_MS, sc);
    CHECK(near(a, b, 0.01f));
  }
  // a 85 ms, el tope es el de siempre: 7 px por paso x velocidad
  CHECK(near(ballPetFollow(0, 10000, GAME_PHYS_REF_MS, 1), BALL_PET_MAX * ballSpeed(1), 1e-3f));
  // y a otro ritmo, el mismo por segundo (12 pasos de referencia en 1020 ms)
  float f = 0;
  for (uint32_t t = 0; t < 1020; t += GAME_FRAME_MS) f = ballPetFollow(f, 100000, GAME_FRAME_MS, 1);
  CHECK(near(f, 12 * BALL_PET_MAX * ballSpeed(1), 1.0f));
}

TEST(gametime, el_trineo_y_la_cascada_no_dependen_del_frame) {
  // ya iban por pasos fijos de 10 ms: se fija que lo sigan haciendo (40,8 s = 480 x 85 = 680 x 60)
  DashState a, b;
  dashStart(a, 7);
  dashStart(b, 7);
  for (uint32_t t = 0; t < 40800; t += GAME_PHYS_REF_MS) dashAdvance(a, 0.4f, GAME_PHYS_REF_MS);
  for (uint32_t t = 0; t < 40800; t += GAME_FRAME_MS) dashAdvance(b, 0.4f, GAME_FRAME_MS);
  CHECK(a.t == b.t);
  CHECK(near(a.dist, b.dist, 0.01f));
  CHECK(near(a.px, b.px, 0.01f));
  CHECK(a.dodged == b.dodged);
  ZenState za, zb;
  zenStart(za, 3);
  zenStart(zb, 3);
  for (uint32_t t = 0; t < 40800; t += GAME_PHYS_REF_MS) zenAdvance(za, (t / 700) % 2, GAME_PHYS_REF_MS);
  for (uint32_t t = 0; t < 40800; t += GAME_FRAME_MS) zenAdvance(zb, (t / 700) % 2, GAME_FRAME_MS);
  CHECK(za.t == zb.t);  // el dedo cambia en frames distintos: la barra difiere un poco, la hora no
}
