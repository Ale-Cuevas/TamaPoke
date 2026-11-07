// Detectores de movimiento (motion.h): sacudida, boca abajo e inclinacion,
// sobre las capturas reales (test/data/imu/) y senales sinteticas. FR-2.1..2.3.
#include "framework.h"
#include "imu_data.h"
#include "../motion.h"

static uint32_t countShakes(const std::vector<ImuSample> &s) {
  ShakeDetector d;
  for (const ImuSample &m : s) d.feed(m.t, m.x, m.y, m.z);
  return d.count;
}

// vaiven sinusoidal en X de `amp` g a `hz`, placa de pie
static std::vector<ImuSample> synthShake(uint32_t ms, float hz, float amp, uint32_t t0 = 0) {
  return synth(ms, [=](float t, float &x, float &y, float &z) {
    x = amp * sinf(2 * (float)M_PI * hz * t);
    y = 1.0f;
    z = 0.04f;
  }, t0);
}

// ---------------------------------------------------------------- sacudida
TEST(motion, encuentra_las_dos_sacudidas_grabadas) {
  // prototipo: 7 la fuerte, 6 la suave en 15 s
  CHECK_RANGE(countShakes(loadCapture("shake-hard-1")), (uint32_t)6, (uint32_t)8);
  CHECK_RANGE(countShakes(loadCapture("shake-soft-1")), (uint32_t)5, (uint32_t)7);
}

TEST(motion, caminar_correr_inclinar_o_dejarla_quieta_no_es_sacudida) {
  for (const char *n : CAPTURES_WALK) CHECK_MSG(countShakes(loadCapture(n)) == 0, n);
  for (const char *n : CAPTURES_STILL) CHECK_MSG(countShakes(loadCapture(n)) == 0, n);
}

TEST(motion, una_sacudida_sintetica_de_5_hz_se_detecta) {
  CHECK(countShakes(synthShake(3000, 5.0f, 2.0f)) >= 1);
}

TEST(motion, un_paseo_a_2_hz_y_una_carrera_a_3_hz_no_son_sacudida) {
  CHECK_EQ(countShakes(synthShake(10000, 2.0f, 1.5f)), (uint32_t)0);   // 2 Hz: 250 ms entre giros
  CHECK_EQ(countShakes(synthShake(10000, 2.7f, 2.5f)), (uint32_t)0);   // como la carrera grabada
  CHECK_EQ(countShakes(synthWalk(10000, 2.0f, 0.8f)), (uint32_t)0);
}

TEST(motion, una_inclinacion_lenta_no_es_sacudida) {
  auto s = synth(8000, [](float t, float &x, float &y, float &z) {
    float a = 1.2f * sinf(0.5f * (float)M_PI * t);  // va y vuelve en 4 s
    x = sinf(a);
    y = cosf(a);
    z = 0.03f;
  });
  CHECK_EQ(countShakes(s), (uint32_t)0);
}

TEST(motion, tras_una_sacudida_espera_un_segundo_para_la_siguiente) {
  ShakeDetector d;
  uint32_t first = 0, second = 0;
  for (const ImuSample &m : synthShake(6000, 6.0f, 2.5f)) {
    if (d.feed(m.t, m.x, m.y, m.z)) {
      if (!first) first = m.t;
      else if (!second) second = m.t;
    }
  }
  CHECK(first);
  CHECK(second);
  CHECK(second - first > SHAKE_HOLD_MS);
}

TEST(motion, la_sacudida_pausa_los_pasos_en_la_tuberia) {
  ImuPipeline p;
  auto s = synthShake(3000, 5.0f, 2.0f);
  for (const ImuSample &m : s) p.feed(m.t, m.x, m.y, m.z);
  CHECK(p.takeShakes() >= 1);
  CHECK_EQ(p.takeShakes(), (uint8_t)0);  // take* vacia el contador
  CHECK(p.takeSteps() <= 5);
}

// ---------------------------------------------------------------- boca abajo
TEST(motion, boca_abajo_entra_a_los_3_s_y_no_antes) {
  FaceDownDetector d;
  uint32_t entered = 0;
  auto s = loadCapture("face-down-1");
  for (const ImuSample &m : s)
    if (d.feed(m.t, m.x, m.y, m.z) == FACEDOWN_ENTER) entered = m.t;
  CHECK(d.down);
  CHECK_RANGE(entered, (uint32_t)(s.front().t + FACEDOWN_MS), (uint32_t)(s.front().t + FACEDOWN_MS + 40));
}

TEST(motion, boca_arriba_de_pie_o_caminando_nunca_entra) {
  for (const char *n : { "flat-up-1", "upright-1", "tilt-right-left", "walk-pocket-1", "walk-hand-1", "run-1",
                         "shake-hard-1" }) {
    FaceDownDetector d;
    bool any = false;
    for (const ImuSample &m : loadCapture(n)) any |= d.feed(m.t, m.x, m.y, m.z) != FACEDOWN_NONE;
    CHECK_MSG(!any, n);
  }
}

static std::vector<ImuSample> faceDown(uint32_t ms, uint32_t t0) {
  return synth(ms, [](float, float &x, float &y, float &z) { x = 0.01f; y = 0.02f; z = -1.0f; }, t0);
}

TEST(motion, darle_la_vuelta_un_momento_no_hace_nada) {
  FaceDownDetector d;
  bool any = false;
  for (auto part : { synthStill(1000), faceDown(2000, 1016), synthStill(2000, 3032), faceDown(2500, 5048) })
    for (const ImuSample &m : part) any |= d.feed(m.t, m.x, m.y, m.z) != FACEDOWN_NONE;
  CHECK(!any);
  CHECK(!d.down);
}

TEST(motion, moverla_boca_abajo_reinicia_la_cuenta) {
  // boca abajo pero meneandose (|a| lejos de 1 g): no esta "quieta"
  FaceDownDetector d;
  auto s = synth(6000, [](float t, float &x, float &y, float &z) {
    x = 0.0f; y = 0.0f; z = -1.0f - 0.4f * sinf(2 * (float)M_PI * 1.0f * t);
  });
  bool any = false;
  for (const ImuSample &m : s) any |= d.feed(m.t, m.x, m.y, m.z) != FACEDOWN_NONE;
  CHECK(!any);
}

TEST(motion, levantarla_avisa_de_la_salida) {
  FaceDownDetector d;
  int8_t last = FACEDOWN_NONE;
  uint32_t left = 0;
  for (const ImuSample &m : faceDown(4000, 0)) d.feed(m.t, m.x, m.y, m.z);
  CHECK(d.down);
  for (const ImuSample &m : synthStill(1000, 4016)) {
    int8_t e = d.feed(m.t, m.x, m.y, m.z);
    if (e) { last = e; left = m.t; }
  }
  CHECK_EQ(last, (int8_t)FACEDOWN_LEAVE);
  CHECK(!d.down);
  CHECK_RANGE(left, (uint32_t)(4016 + FACEDOWN_LEAVE_MS), (uint32_t)(4016 + FACEDOWN_LEAVE_MS + 40));
}

TEST(motion, un_golpe_boca_abajo_no_cuenta_como_levantarla) {
  FaceDownDetector d;
  for (const ImuSample &m : faceDown(4000, 0)) d.feed(m.t, m.x, m.y, m.z);
  // 50 ms con Z positiva (un golpe en la mesa) y sigue boca abajo
  bool any = false;
  for (const ImuSample &m : synth(48, [](float, float &x, float &y, float &z) { x = 0; y = 0; z = 0.2f; }, 4016))
    any |= d.feed(m.t, m.x, m.y, m.z) != FACEDOWN_NONE;
  for (const ImuSample &m : faceDown(2000, 4080)) any |= d.feed(m.t, m.x, m.y, m.z) != FACEDOWN_NONE;
  CHECK(!any);
  CHECK(d.down);
}

// ---------------------------------------------------------------- inclinacion
TEST(motion, la_inclinacion_grabada_va_a_la_derecha_y_luego_a_la_izquierda) {
  // captura: de pie, lado derecho abajo (~3-7 s, X negativa), a nivel, lado
  // izquierdo abajo (~11-14 s, X positiva). En la 2.16 bajar la derecha da X < 0.
  TiltEstimator t;
  bool sawRight = false, sawLeft = false, sawRightFirst = false;
  float maxDeg = 0, minDeg = 0;
  for (const ImuSample &m : loadCapture("tilt-right-left")) {
    t.feed(m.t, m.x, m.y, m.z, -1);
    if (t.side == TILT_RIGHT) { if (!sawLeft) sawRightFirst = true; sawRight = true; }
    if (t.side == TILT_LEFT) sawLeft = true;
    if (t.deg > maxDeg) maxDeg = t.deg;
    if (t.deg < minDeg) minDeg = t.deg;
  }
  CHECK(sawRight);
  CHECK(sawLeft);
  CHECK(sawRightFirst);
  CHECK_RANGE(maxDeg, 40.0f, 60.0f);
  CHECK_RANGE(minDeg, -60.0f, -40.0f);
}

TEST(motion, el_signo_del_eje_viene_de_la_placa) {
  // con el eje al reves (otra placa), la misma captura se lee en espejo
  TiltEstimator a, b;
  for (const ImuSample &m : loadCapture("tilt-right-left")) {
    a.feed(m.t, m.x, m.y, m.z, -1);
    b.feed(m.t, m.x, m.y, m.z, +1);
    CHECK_EQ(a.side, (int8_t)-b.side);
  }
  CHECK_EQ((int)BOARD_IMU_X_RIGHT_DOWN_SIGN, -1);
}

static void holdAngle(TiltEstimator &t, float deg, uint32_t t0, uint32_t ms, int8_t sign = -1) {
  float r = deg / 57.29578f;
  for (const ImuSample &m : synth(ms, [=](float, float &x, float &y, float &z) {
         x = sinf(r) * sign;  // + deg = lado derecho abajo (X < 0 con signo -1)
         y = cosf(r);
         z = 0.02f;
       }, t0))
    t.feed(m.t, m.x, m.y, m.z, sign);
}

TEST(motion, umbrales_de_caminar_y_resbalar_con_espera) {
  TiltEstimator t;
  holdAngle(t, 0, 0, 1000);
  CHECK_EQ(t.side, (int8_t)TILT_LEVEL);
  holdAngle(t, 10, 1016, 2000);  // poco: nada
  CHECK_EQ(t.side, (int8_t)TILT_LEVEL);
  holdAngle(t, 30, 3032, 400);   // camina... pero aun no ha pasado medio segundo
  CHECK_EQ(t.side, (int8_t)TILT_LEVEL);
  holdAngle(t, 30, 3448, 1000);
  CHECK_EQ(t.side, (int8_t)TILT_RIGHT);
  CHECK(!t.slide);
  holdAngle(t, 60, 4464, 1500);  // mucho: resbala
  CHECK_EQ(t.side, (int8_t)TILT_RIGHT);
  CHECK(t.slide);
  holdAngle(t, 30, 5980, 600);   // menos: deja de resbalar y sigue caminando
  CHECK(!t.slide);
  CHECK_EQ(t.side, (int8_t)TILT_RIGHT);
  holdAngle(t, -60, 6596, 1500); // al otro lado del todo
  CHECK_EQ(t.side, (int8_t)TILT_LEFT);
  CHECK(t.slide);
  holdAngle(t, 0, 8112, 600);    // a nivel: vuelve a pasear
  CHECK_EQ(t.side, (int8_t)TILT_LEVEL);
  CHECK(!t.slide);
}

TEST(motion, tumbada_boca_arriba_tambien_mide_la_inclinacion) {
  TiltEstimator t;
  float r = 35 / 57.29578f;
  for (const ImuSample &m : synth(2000, [=](float, float &x, float &y, float &z) {
         x = -sinf(r); y = 0.01f; z = cosf(r);
       }))
    t.feed(m.t, m.x, m.y, m.z, -1);
  CHECK_RANGE(t.deg, 33.0f, 37.0f);
  CHECK_EQ(t.side, (int8_t)TILT_RIGHT);
}

// la inclinacion rapida de la carrera (FR-DASH.1): el mismo angulo lateral con la placa de pie,
// a 45 grados o tumbada boca arriba (no depende del cabeceo), y sigue al giro en ~0,2 s
static float rollAt(float rollDeg, float pitchDeg, uint32_t ms) {
  FastRoll f;
  float r = rollDeg / 57.29578f, p = pitchDeg / 57.29578f;
  for (const ImuSample &m : synth(ms, [=](float, float &x, float &y, float &z) {
         x = -sinf(r);               // lado derecho abajo: X < 0 en la 2.16
         y = cosf(r) * cosf(p);      // de pie (p = 0) la gravedad va por Y...
         z = cosf(r) * sinf(p);      // ...tumbada (p = 90) por Z
       }))
    f.feed(m.t, m.x, m.y, m.z, -1);
  return f.deg;
}

TEST(motion, la_inclinacion_rapida_no_depende_del_cabeceo) {
  for (float roll : { -25.0f, -8.0f, 0.0f, 12.0f, 30.0f })
    for (float pitch : { 0.0f, 45.0f, 90.0f }) {
      float d = rollAt(roll, pitch, 1000);
      CHECK_RANGE(d, roll - 0.5f, roll + 0.5f);
    }
}

TEST(motion, la_inclinacion_rapida_sigue_al_giro_enseguida) {
  FastRoll f;
  TiltEstimator slow;
  auto feedAngle = [&](float deg, uint32_t t0, uint32_t ms) {
    float r = deg / 57.29578f;
    for (const ImuSample &m : synth(ms, [=](float, float &x, float &y, float &z) {
           x = -sinf(r); y = 0.02f; z = cosf(r);  // tumbada boca arriba
         }, t0)) {
      f.feed(m.t, m.x, m.y, m.z, -1);
      slow.feed(m.t, m.x, m.y, m.z, -1);
    }
  };
  feedAngle(0, 0, 1000);
  feedAngle(20, 1016, 200);  // gira a 20 grados: 200 ms despues
  CHECK(f.deg > 18.0f);      // la rapida ya casi esta (3 constantes de tiempo)
  CHECK(slow.deg < f.deg - 5.0f);  // la del paseo va muy por detras
}

TEST(motion, la_tuberia_alimenta_la_inclinacion_rapida) {
  ImuPipeline p;
  float r = 15 / 57.29578f;
  for (const ImuSample &m : synth(1000, [=](float, float &x, float &y, float &z) {
         x = -sinf(r) * (float)-BOARD_IMU_X_RIGHT_DOWN_SIGN; y = cosf(r); z = 0.02f;
       }))
    p.feed(m.t, m.x, m.y, m.z);
  CHECK_RANGE(p.roll.deg, 14.5f, 15.5f);
}

TEST(motion, quieta_no_se_inclina) {
  for (const char *n : { "flat-up-1", "upright-1", "face-down-1" }) {
    TiltEstimator t;
    bool any = false;
    for (const ImuSample &m : loadCapture(n)) { t.feed(m.t, m.x, m.y, m.z, -1); any |= t.side != TILT_LEVEL; }
    CHECK_MSG(!any, n);
  }
}
