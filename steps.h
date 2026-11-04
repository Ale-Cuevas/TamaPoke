#pragma once
#include <math.h>
#include <stdint.h>

// Contador de pasos por software (docs/FEATURES.md, Tarea 1, FR-1.1 / FR-1.8).
// Logica pura, sin hardware: la alimenta imu.cpp con las muestras del FIFO del
// QMI8658 y se prueba en el PC con las capturas de test/data/imu/
// (test/test_steps.cpp). Portado de tools/imu_replay.py, que es el prototipo
// con el que se midieron los umbrales en la placa.
//
// El podometro del propio chip no conto ni un paso en esta placa con ninguna
// configuracion, y los datos crudos si muestran cada paso; por eso se cuenta aqui.
//
// Por muestra: |a|, menos una media lenta (la gravedad, 1 s), suavizado con una
// media rapida (60 ms). Un paso es un maximo local por encima de STEP_PEAK_G,
// al menos STEP_MIN_GAP_MS despues del anterior. Se empieza a contar tras
// STEP_ENTRY pasos seguidos, cada uno a menos de STEP_MAX_GAP_MS del anterior
// (como la "entrada" del podometro del chip): las rafagas cortas se descartan.
// Una sacudida (motion.h) pausa el contador y rompe la racha, para que agitar
// la placa no sume pasos.

#define STEP_PEAK_G 0.12f       // |a| dinamica suavizada que tiene que superar un paso
#define STEP_MIN_GAP_MS 280     // paso mas rapido (~3,5 pasos/s)
#define STEP_MAX_GAP_MS 2000    // pausa mas larga dentro de una racha
#define STEP_ENTRY 4            // pasos seguidos antes de empezar a contar
#define STEP_SLOW_TAU_MS 1000.0f  // media lenta: la gravedad en |a|
#define STEP_FAST_TAU_MS 60.0f    // suavizado de la senal de paso
#define STEP_FLUSH_MS 30000UL     // cada cuanto pasan los pasos contados a Pet::addSteps()

struct StepCounter {
  uint32_t total = 0;  // pasos contados desde reset()

  void reset() { *this = StepCounter(); }

  // Pausa el conteo hasta untilMs y rompe la racha (una sacudida).
  void pause(uint32_t untilMs) {
    paused = true;
    pauseUntil = untilMs;
    streak = 0;
  }

  // Una muestra: t en ms (creciente; las vueltas de millis() se toleran) y la
  // aceleracion en g. Devuelve los pasos que se acaban de contar: 0, 1 o
  // STEP_ENTRY (cuando una racha llega a la entrada se cuentan todos sus pasos).
  uint8_t feed(uint32_t t, float x, float y, float z) {
    float mag = sqrtf(x * x + y * y + z * z);
    if (!started) {
      started = true;
      slow = mag;
      fast = 0;
      prev = prev2 = 0;
      lastT = t;
      return 0;
    }
    float dt = (float)(int32_t)(t - lastT);
    if (dt < 1.0f) dt = 1.0f;
    lastT = t;
    slow += (1.0f - expf(-dt / STEP_SLOW_TAU_MS)) * (mag - slow);
    float dyn = mag - slow;
    fast += (1.0f - expf(-dt / STEP_FAST_TAU_MS)) * (dyn - fast);

    uint8_t counted = 0;
    // la muestra anterior es un maximo local por encima del umbral
    if (prev > prev2 && prev >= fast && prev > STEP_PEAK_G &&
        (!havePeak || (int32_t)(t - lastPeak) >= STEP_MIN_GAP_MS)) {
      havePeak = true;
      lastPeak = t;
      if (paused && (int32_t)(t - pauseUntil) <= 0) {
        streak = 0;  // durante una sacudida no se cuenta nada
      } else {
        paused = false;
        if (streak && (int32_t)(t - lastStep) > STEP_MAX_GAP_MS) streak = 0;
        lastStep = t;
        if (streak < 255) streak++;
        if (streak == STEP_ENTRY) counted = STEP_ENTRY;
        else if (streak > STEP_ENTRY) counted = 1;
      }
    }
    prev2 = prev;
    prev = fast;
    total += counted;
    return counted;
  }

  // racha en curso (pasos seguidos, contados o aun en la entrada)
  uint8_t streakLen() const { return streak; }

private:
  bool started = false;
  float slow = 0, fast = 0, prev = 0, prev2 = 0;
  uint32_t lastT = 0;
  bool havePeak = false;
  uint32_t lastPeak = 0;
  uint32_t lastStep = 0;
  uint8_t streak = 0;
  bool paused = false;
  uint32_t pauseUntil = 0;
};
