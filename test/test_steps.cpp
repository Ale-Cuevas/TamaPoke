// Contador de pasos por software (steps.h), sobre las capturas reales de la
// placa (test/data/imu/) y senales sinteticas. FR-1.1 / FR-1.8.
#include "framework.h"
#include "imu_data.h"
#include "../steps.h"
#include "../motion.h"

static uint32_t countSteps(const std::vector<ImuSample> &s) {
  StepCounter c;
  for (const ImuSample &m : s) c.feed(m.t, m.x, m.y, m.z);
  return c.total;
}

// como en el firmware: la sacudida pausa el contador (ImuPipeline)
static uint32_t pipelineSteps(const std::vector<ImuSample> &s) {
  ImuPipeline p;
  uint32_t n = 0;
  for (const ImuSample &m : s) p.feed(m.t, m.x, m.y, m.z);
  n = p.takeSteps();
  CHECK_EQ(p.steps.total, n);
  return n;
}

static std::vector<ImuSample> concat(std::vector<ImuSample> a, const std::vector<ImuSample> &b) {
  a.insert(a.end(), b.begin(), b.end());
  return a;
}

TEST(steps, las_capturas_se_cargan) {
  for (const char *n : CAPTURES_STILL) CHECK_MSG(loadCapture(n).size() > 700, n);
  for (const char *n : CAPTURES_WALK) CHECK_MSG(loadCapture(n).size() > 700, n);
  CHECK(loadCapture("shake-soft-1").size() > 700);
  CHECK(loadCapture("shake-hard-1").size() > 700);
}

// Los conteos del prototipo (tools/imu_replay.py) sobre las mismas capturas: el
// port en C++ tiene que dar lo mismo. Las cuatro primeras no tienen el numero
// real de pasos apuntado. walk-pocket-100-1 son 100 pasos contados a mano (unos
// 3 antes de empezar a grabar) y 4 movimientos ya parada que con la entrada de 4
// contaban: 103, con la de 8 los 99 del paseo. walk-hand-2 es un paseo en la
// mano de 73 s a 0,63 s el paso, unos 115 pasos.
TEST(steps, cuenta_lo_mismo_que_el_prototipo_en_las_capturas) {
  struct { const char *name; uint32_t proto; } cases[] = {
    { "walk-pocket-1", 22 }, { "walk-pocket-2", 20 }, { "walk-hand-1", 17 }, { "run-1", 31 },
    { "walk-pocket-100-1", 99 }, { "walk-hand-2", 112 },
  };
  for (auto &c : cases) {
    uint32_t n = pipelineSteps(loadCapture(c.name));
    char msg[64];
    snprintf(msg, sizeof(msg), "%s: %u (prototipo %u)", c.name, n, c.proto);
    CHECK_MSG(n + 1 >= c.proto && n <= c.proto + 1, msg);
  }
}

TEST(steps, quieta_boca_abajo_de_pie_o_inclinandola_no_cuenta) {
  for (const char *n : CAPTURES_STILL) CHECK_MSG(countSteps(loadCapture(n)) == 0, n);
}

TEST(steps, agitar_no_suma_pasos) {
  CHECK_EQ(pipelineSteps(loadCapture("shake-hard-1")), (uint32_t)0);
  CHECK_EQ(pipelineSteps(loadCapture("shake-soft-1")), (uint32_t)0);  // 4 con la entrada de 4
}

// de pie sin andar, sacarla del bolsillo, mirarla y guardarla 4-5 veces: con la
// entrada de 4 una de esas rafagas sumaba 4 pasos
TEST(steps, sacarla_del_bolsillo_para_mirarla_no_cuenta) {
  CHECK_EQ(pipelineSteps(loadCapture("handling-1")), (uint32_t)0);
}

TEST(steps, caminar_limpio_a_1_7_hz_cuenta_cada_paso) {
  // 20 s a 1,7 pasos/s = 34 pasos; el filtro de gravedad tarda en asentarse
  uint32_t n = countSteps(synthWalk(20000, 1.7f, 0.4f));
  CHECK_RANGE(n, (uint32_t)31, (uint32_t)35);
}

// pasos sinteticos a 1,7 Hz: duracion de `n` pasos
static uint32_t stepsMs(int n) { return (uint32_t)(n * 1000 / 1.7f); }

TEST(steps, una_rafaga_mas_corta_que_la_entrada_no_cuenta) {
  // STEP_ENTRY - 2 pasos y quieta
  uint32_t w = stepsMs(STEP_ENTRY - 2);
  auto s = concat(synthStill(3000), synthWalk(w, 1.7f, 0.4f, 3016));
  s = concat(s, synthStill(5000, 3032 + w));
  CHECK_EQ(countSteps(s), (uint32_t)0);
}

TEST(steps, al_llegar_a_la_entrada_cuenta_los_pasos_de_la_entrada) {
  StepCounter c;
  uint32_t first = 0;
  for (const ImuSample &m : synthWalk(8000, 1.7f, 0.4f)) {
    uint8_t got = c.feed(m.t, m.x, m.y, m.z);
    if (got && !first) {
      first = got;
      CHECK_EQ(got, (uint8_t)STEP_ENTRY);
    } else if (got) {
      CHECK_EQ(got, (uint8_t)1);
    }
  }
  CHECK(first);
}

TEST(steps, una_pausa_larga_rompe_la_racha) {
  // STEP_ENTRY - 2 pasos, 3 s parada (> STEP_MAX_GAP_MS), otros tantos: juntos
  // pasan de la entrada, pero nunca llegan a ella seguidos
  uint32_t w = stepsMs(STEP_ENTRY - 2);
  auto s = concat(synthStill(2000), synthWalk(w, 1.7f, 0.4f, 2016));
  s = concat(s, synthStill(3000, 2032 + w));
  s = concat(s, synthWalk(w, 1.7f, 0.4f, 5048 + w));
  s = concat(s, synthStill(3000, 5064 + 2 * w));
  CHECK_EQ(countSteps(s), (uint32_t)0);
}

TEST(steps, una_pausa_corta_no_rompe_la_racha) {
  // un paso que se retrasa 1,2 s (< STEP_MAX_GAP_MS) no corta la racha
  auto a = synthWalk(5900, 1.7f, 0.4f, 1016);
  auto s = concat(concat(synthStill(1000), a), synthStill(600, 6916));
  StepCounter c;
  for (const ImuSample &m : s) c.feed(m.t, m.x, m.y, m.z);
  uint32_t before = c.total;
  CHECK(before >= STEP_ENTRY);
  for (const ImuSample &m : synthWalk(3000, 1.7f, 0.4f, 7532)) c.feed(m.t, m.x, m.y, m.z);
  CHECK_MSG(c.total >= before + 4, "tras la pausa corta los pasos cuentan sin otra entrada");
}

TEST(steps, la_pausa_por_sacudida_no_cuenta_y_rompe_la_racha) {
  StepCounter c;
  auto s = synthWalk(14000, 1.7f, 0.4f);  // tras la pausa quedan ~13 pasos, mas que la entrada
  uint32_t mid = 0;
  for (const ImuSample &m : s) {
    if (m.t == 5008) { mid = c.total; c.pause(m.t + SHAKE_HOLD_MS); }
    c.feed(m.t, m.x, m.y, m.z);
  }
  CHECK(mid > 0);
  // 1 s de pausa (~1,7 pasos perdidos) + una entrada nueva que si se cuenta
  uint32_t sinPausa = countSteps(s);
  CHECK(c.total < sinPausa);
  CHECK(c.total + 3 >= sinPausa);
}

TEST(steps, sobrevive_a_la_vuelta_de_millis) {
  uint32_t t0 = 0xFFFFFFFFu - 4000;  // la vuelta cae a mitad del paseo
  uint32_t n = countSteps(synthWalk(12000, 1.7f, 0.4f, t0));
  CHECK_RANGE(n, (uint32_t)17, (uint32_t)21);
}

TEST(steps, reset_lo_deja_a_cero) {
  StepCounter c;
  for (const ImuSample &m : synthWalk(6000, 1.7f, 0.4f)) c.feed(m.t, m.x, m.y, m.z);
  CHECK(c.total > 0);
  c.reset();
  CHECK_EQ(c.total, (uint32_t)0);
  CHECK_EQ(c.streakLen(), (uint8_t)0);
}
