// Carga de las capturas del acelerometro de test/data/imu/ y senales sinteticas
// para test_steps.cpp y test_motion.cpp.
#pragma once
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <functional>
#include <string>
#include <vector>

#ifndef TEST_IMU_DIR
#define TEST_IMU_DIR "data/imu"
#endif

struct ImuSample {
  uint32_t t;
  float x, y, z;  // g
};

// t_ms,ax_mg,ay_mg,az_mg,steps (capturadas con la sonda del firmware de bring-up, y desde
// v1.20 con tools/imu_capture.py sobre el juego)
static inline std::vector<ImuSample> loadCapture(const char *name) {
  std::vector<ImuSample> out;
  std::string path = std::string(TEST_IMU_DIR) + "/" + name + ".csv";
  FILE *f = fopen(path.c_str(), "r");
  if (!f) return out;
  char line[128];
  if (!fgets(line, sizeof(line), f)) { fclose(f); return out; }  // cabecera
  while (fgets(line, sizeof(line), f)) {
    unsigned long t;
    int x, y, z;
    if (sscanf(line, "%lu,%d,%d,%d", &t, &x, &y, &z) == 4)
      out.push_back({ (uint32_t)t, x / 1000.0f, y / 1000.0f, z / 1000.0f });
  }
  fclose(f);
  return out;
}

static const char *const CAPTURES_STILL[] = { "flat-up-1", "face-down-1", "upright-1", "tilt-right-left" };
static const char *const CAPTURES_WALK[] = { "walk-pocket-1", "walk-pocket-2", "walk-hand-1", "run-1",
                                              "walk-pocket-100-1", "walk-hand-2", "walk-hand-100-1" };

// senal sintetica a 62.5 Hz durante `ms`, desde t0: f(t_s) -> (x, y, z) en g
static inline std::vector<ImuSample> synth(uint32_t ms, std::function<void(float, float &, float &, float &)> f,
                                           uint32_t t0 = 0) {
  std::vector<ImuSample> out;
  for (uint32_t t = 0; t <= ms; t += 16) {
    ImuSample s;
    s.t = t0 + t;
    f(t / 1000.0f, s.x, s.y, s.z);
    out.push_back(s);
  }
  return out;
}

// caminar: placa de pie (gravedad en +Y) y un bote vertical de `amp` g a `hz`
static inline std::vector<ImuSample> synthWalk(uint32_t ms, float hz, float amp, uint32_t t0 = 0) {
  return synth(ms, [=](float t, float &x, float &y, float &z) {
    x = 0.03f;
    y = 1.0f + amp * sinf(2 * (float)M_PI * hz * t);
    z = 0.04f;
  }, t0);
}

// quieta de pie
static inline std::vector<ImuSample> synthStill(uint32_t ms, uint32_t t0 = 0) {
  return synth(ms, [](float, float &x, float &y, float &z) { x = 0.03f; y = 1.0f; z = 0.04f; }, t0);
}
