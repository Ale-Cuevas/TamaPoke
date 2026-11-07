// Minijuego de la cascada (zengame.h, ZEN en docs/NEW_GAMES.md).
#include "framework.h"
#include "../zengame.h"
#include <cmath>
#include <deque>

static bool near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }

// sesion sin golpes de agua, para ver solo la fisica del dedo
static ZenState calm() {
  ZenState z;
  zenStart(z, 1);
  z.nextSurge = ZEN_SESSION_MS;
  return z;
}

TEST(zengame, empieza_en_el_centro_quieta) {
  ZenState z;
  zenStart(z, 42);
  CHECK(near(z.c, 50));
  CHECK(near(z.vc, 0));
  CHECK_EQ(z.t, 0u);
  CHECK_EQ(z.inBandMs, 0u);
  CHECK(!zenDone(z));
}

TEST(zengame, con_el_dedo_sube_y_sin_el_baja) {
  ZenState up = calm(), down = calm();
  zenAdvance(up, true, 200);
  zenAdvance(down, false, 200);
  CHECK(up.c > 50 && up.vc > 0);
  CHECK(down.c < 50 && down.vc < 0);
}

TEST(zengame, tiene_inercia_al_soltar_sigue_subiendo_un_momento) {
  ZenState z = calm();
  zenAdvance(z, true, 200);
  float c0 = z.c;
  zenAdvance(z, false, 50);  // soltar: la velocidad aun es positiva
  CHECK(z.c > c0);
  for (int i = 0; i < 100; i++) zenAdvance(z, false, 20);
  CHECK(z.c < c0);  // al final baja
}

TEST(zengame, se_para_en_0_y_en_100) {
  ZenState z = calm();
  for (int i = 0; i < 300; i++) zenAdvance(z, true, 20);
  CHECK(near(z.c, 100));
  CHECK(z.vc <= 0.01f);
  for (int i = 0; i < 400; i++) zenAdvance(z, false, 20);
  CHECK(near(z.c, 0));
  CHECK(z.vc >= -0.01f);
}

TEST(zengame, la_velocidad_tiene_tope) {
  ZenState z = calm();
  for (int i = 0; i < 40; i++) {
    zenAdvance(z, true, 20);
    CHECK(z.vc <= ZEN_VMAX + 1e-3f);
  }
}

TEST(zengame, la_franja_se_estrecha_despacio_y_a_los_30s_va_por_la_mitad) {
  CHECK(near(2 * zenBandHalf(0), ZEN_BAND_START));
  CHECK(near(2 * zenBandHalf(30000), (ZEN_BAND_START + ZEN_BAND_END) / 2));  // a los 30 s: 21,6
  CHECK(near(2 * zenBandHalf(ZEN_RAMP_MS), ZEN_BAND_END));
  CHECK(near(2 * zenBandHalf(ZEN_RAMP_MS + 5000), ZEN_BAND_END));
  CHECK(zenInBand(65, 0) && !zenInBand(66, 0));  // 34,4-65,6 al empezar
  CHECK_EQ(ZEN_SESSION_MS, 45000u);
  CHECK_EQ(ZEN_RAMP_MS, 60000u);
  CHECK(zenInBand(60, 29990) && !zenInBand(61, 29990));  // 39,2-60,8 justo antes de moverse
  CHECK(zenInBand(55, ZEN_RAMP_MS) && !zenInBand(57, ZEN_RAMP_MS));
  CHECK_EQ(zenZone(30, 0), -1);
  CHECK_EQ(zenZone(50, 0), 0);
  CHECK_EQ(zenZone(70, 0), 1);
}

TEST(zengame, la_concentracion_solo_cuenta_dentro_de_la_franja) {
  ZenState z = calm();
  z.c = 10;  // muy abajo y sin dedo: no entra nunca
  zenAdvance(z, false, 200);
  CHECK_EQ(z.inBandMs, 0u);
  ZenState w = calm();  // quieta en el centro (dedo puesto y quitado a ratos)
  for (int i = 0; i < 25; i++) zenAdvance(w, (i % 2) == 0, 20);
  CHECK_EQ(w.inBandMs, 500u);
  CHECK_EQ(zenFocusTenths(w), 5);
}

TEST(zengame, los_golpes_empiezan_a_los_5s_y_cada_vez_mas_seguidos) {
  ZenState z;
  zenStart(z, 12345);
  std::vector<uint32_t> at;
  uint16_t seen = 0;
  while (!zenDone(z)) {
    zenAdvance(z, z.c < 50, 10);
    if (z.surges != seen) { seen = z.surges; at.push_back(z.lastSurge); }
  }
  CHECK(at.size() >= 4);
  CHECK_EQ(at[0], ZEN_SURGE_FROM_MS);
  for (size_t i = 1; i < at.size(); i++) {
    uint32_t gap = at[i] - at[i - 1];
    CHECK(gap >= ZEN_SURGE_GAP_MIN * 3 / 4 - 10);
    CHECK(gap <= ZEN_SURGE_GAP_MAX * 5 / 4 + 10);
  }
  // la separacion base baja con el tiempo: al final, en media, mas seguidos que al principio
  uint32_t rng = 7, early = 0, late = 0;
  for (int i = 0; i < 200; i++) early += zenSurgeGap(0, rng), late += zenSurgeGap(ZEN_RAMP_MS, rng);
  CHECK(late < early * 6 / 10);
  CHECK(zenSurgeKick(ZEN_SESSION_MS) > zenSurgeKick(0));
}

TEST(zengame, cada_golpe_se_avisa_antes) {
  ZenState z;
  zenStart(z, 99);
  z.c = 50;
  bool warned = false;
  uint32_t warnFrom = 0;
  while (z.surges == 0) {
    bool w = zenSurgeWarning(z);
    if (w && !warned) warnFrom = z.t;
    warned = warned || w;
    zenAdvance(z, z.c < 50, 10);
  }
  CHECK(warned);
  CHECK_EQ(ZEN_SURGE_FROM_MS - warnFrom, ZEN_SURGE_WARN_MS);  // el aviso empieza 400 ms antes
  CHECK(!zenSurgeWarning(z) || z.nextSurge - z.t <= ZEN_SURGE_WARN_MS);  // y el siguiente, igual
}

TEST(zengame, un_golpe_tira_la_barra_hacia_abajo) {
  ZenState a, b;
  zenStart(a, 5);
  zenStart(b, 5);
  b.nextSurge = ZEN_SESSION_MS;  // la misma sesion sin golpes
  a.t = b.t = ZEN_SURGE_FROM_MS - 10;
  zenAdvance(a, true, 100);
  zenAdvance(b, true, 100);
  CHECK(a.c < b.c);
  CHECK_EQ(a.surges, 1);
}

// el resultado no depende de cada cuanto se llame (pasos fijos)
TEST(zengame, igual_a_30ms_que_a_85ms_por_frame) {
  ZenState a, b;
  zenStart(a, 77);
  zenStart(b, 77);
  for (uint32_t t = 0; t < 6120; t += 30) zenAdvance(a, (t / 510) % 2 == 0, 30);
  for (uint32_t t = 0; t < 6120; t += 85) zenAdvance(b, (t / 510) % 2 == 0, 85);
  CHECK_EQ(a.t, b.t);
  CHECK(near(a.c, b.c, 1e-3f));
  CHECK_EQ(a.inBandMs, b.inBandMs);
}

TEST(zengame, un_frame_tardio_cuenta_como_mucho_250ms) {
  ZenState z = calm();
  zenAdvance(z, false, 5000);
  CHECK_EQ(z.t, ZEN_LATE_MAX_MS);
}

TEST(zengame, la_sesion_dura_45s_y_no_pasa_de_ahi) {
  ZenState z = calm();
  for (int i = 0; i < 4000; i++) zenAdvance(z, z.c < 50, 20);
  CHECK(zenDone(z));
  CHECK_EQ(z.t, ZEN_SESSION_MS);
  CHECK(z.inBandMs <= ZEN_SESSION_MS);
}

// jugador de prueba: ve la barra con `lag` ms de retraso y la proyecta `lead` s hacia delante;
// pone el dedo si la ve por debajo de 50
static float scripted(uint32_t seed, int lag, float lead) {
  ZenState z;
  zenStart(z, seed);
  std::deque<float> seen;
  bool hold = false;
  while (!zenDone(z)) {
    seen.push_back(z.c + z.vc * lead - zenCenter(z.t, z.driftDir));  // lo que le falta para el centro
    if ((int)seen.size() > lag / 20) { hold = seen.front() < 0; seen.pop_front(); }
    zenAdvance(z, hold, 20);
  }
  return (float)z.inBandMs / ZEN_SESSION_MS;
}

TEST(zengame, se_puede_ganar_pero_no_es_trivial) {
  float good = 0, naive = 0;
  for (uint32_t s = 1; s <= 30; s++) {
    good += scripted(s * 7919, 200, 0.4f);  // reacciona en 200 ms y se anticipa
    naive += scripted(s * 7919, 200, 0.0f); // reacciona en 200 ms sin anticiparse
  }
  // v1.45/v1.46, mas facil a peticion del usuario: el que se anticipa la clava (~100 %), el que solo
  // reacciona se queda en ~60 % (era ~33 % con la v1.43)
  CHECK(good / 30 > 0.9f);
  CHECK(naive / 30 < 0.75f);
}

TEST(zengame, desde_los_30s_la_franja_sube_y_baja_sin_salirse) {
  CHECK(!zenDrifting(29990));
  CHECK(zenDrifting(30000));
  CHECK(near(zenCenter(0, 1), 50) && near(zenCenter(29990, -1), 50));
  CHECK(near(zenCenter(30000, 1), 50));                       // arranca desde el centro, sin salto
  CHECK(near(zenCenter(33000, 1), 50 + ZEN_DRIFT_AMP, 0.01f));  // un cuarto de vaiven: arriba del todo
  CHECK(near(zenCenter(33000, -1), 50 - ZEN_DRIFT_AMP, 0.01f)); // ...o abajo, segun el sentido
  float prev = zenCenter(30000, 1);
  for (uint32_t t = 30000; t <= ZEN_SESSION_MS; t += 10) {
    float m = zenCenter(t, 1), h = zenBandHalf(t);
    CHECK(m - h >= 0 && m + h <= 100);                         // nunca se sale de la barra
    CHECK(std::fabs(m - prev) <= 0.13f);                        // despacio: <= ~12 %/s
    prev = m;
  }
  // los dos sentidos salen con semillas distintas
  int up = 0;
  for (uint32_t s = 1; s <= 100; s++) { ZenState z; zenStart(z, s); up += z.driftDir > 0; }
  CHECK(up > 20 && up < 80);
}

TEST(zengame, la_concentracion_con_la_franja_moviendose_se_cuenta_aparte) {
  ZenState z;
  zenStart(z, 3);
  z.nextSurge = ZEN_SESSION_MS;
  while (!zenDone(z)) {  // sigue al centro perfectamente
    z.c = zenCenter(z.t, z.driftDir);
    z.vc = 0;
    zenAdvance(z, false, 10);
  }
  CHECK_EQ(z.inBandMs, ZEN_SESSION_MS);
  CHECK_EQ(z.driftInBandMs, ZEN_SESSION_MS - ZEN_DRIFT_FROM_MS);
  CHECK_EQ(zenDriftTenths(z), 150);
}

TEST(zengame, bonificacion_1_cada_1_5s_tope_10_y_el_total) {
  CHECK_EQ(zenBonus(0), 0);
  CHECK_EQ(zenBonus(15), 1);
  CHECK_EQ(zenBonus(150), 10);
  CHECK_EQ(zenBonus(400), 10);
  CHECK_EQ(zenTotalGain(450, 150), 25);  // sesion perfecta: 15 + 10
  CHECK_EQ(zenTotalGain(200, 60), 7 + 4);
  CHECK_EQ(zenTotalGain(100, 0), 5);
  CHECK_EQ(zenTotalGain(50, 90), 3);     // drift mayor que el total: se recorta
}

TEST(zengame, ganancia_de_def_1_cada_2s_tope_15) {
  CHECK_EQ(zenGain(0), 0);
  CHECK_EQ(zenGain(19), 0);
  CHECK_EQ(zenGain(20), 1);
  CHECK_EQ(zenGain(150), 7);
  CHECK_EQ(zenGain(300), 15);
  CHECK_EQ(zenGain(1000), 15);
}
