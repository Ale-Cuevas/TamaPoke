#pragma once
#include <math.h>
#include <stdint.h>
#include "steps.h"

// Detectores de movimiento del acelerometro (docs/FEATURES.md, Tarea 2) y la
// tuberia que reparte cada muestra entre ellos y el contador de pasos. Logica
// pura: imu.cpp la alimenta con el FIFO del QMI8658 y test/test_motion.cpp con
// las capturas de test/data/imu/. Los umbrales salen de esas capturas
// (tools/imu_replay.py es el prototipo).
//
// Ejes medidos en la AMOLED-2.16, con el USB-C abajo: +X = derecha de la
// pantalla, +Y = arriba (el canto de los botones), +Z = sale de la pantalla.
// Bajar el lado derecho da X negativa. El signo va en la cabecera de la placa
// (BOARD_IMU_X_RIGHT_DOWN_SIGN) para que la 1.75 pueda ser distinta.

// --- sacudida: ida y vuelta corta y rapida en un eje -------------------------
#define SHAKE_REVERSALS 5      // cambios de sentido rapidos en un mismo eje
#define SHAKE_WINDOW_MS 1200   // ventana para reunirlos
#define SHAKE_MAX_GAP_MS 170   // separacion maxima entre dos (~3 Hz)
#define SHAKE_PEAK_G 1.0f      // vaiven mas alla de la gravedad
#define SHAKE_HOLD_MS 1000     // sin otra sacudida, ni pasos, durante este rato
#define SHAKE_AXIS_TAU_MS 400.0f  // gravedad por eje (media movil)

// --- boca abajo y quieta -------------------------------------------------------
#define FACEDOWN_Z_G -0.8f    // Z por debajo de esto
#define FACEDOWN_XY_G 0.3f    // X e Y cerca de 0
#define FACEDOWN_STILL_G 0.15f  // ||a| - 1 g| por debajo de esto: quieta
#define FACEDOWN_MS 3000      // tiempo boca abajo y quieta para entrar
#define FACEDOWN_LEAVE_Z_G -0.5f  // Z por encima de esto: la han levantado
#define FACEDOWN_LEAVE_MS 120     // ...durante este rato (filtra un golpe)

// --- inclinacion lateral -------------------------------------------------------
#define TILT_WALK_DEG 20.0f   // camina hacia el lado bajo
#define TILT_SLIDE_DEG 45.0f  // resbala hacia el lado bajo
#define TILT_HOLD_MS 500      // inclinada este rato antes de reaccionar
#define TILT_HYST_DEG 5.0f    // histeresis al volver a nivel
#define TILT_TAU_MS 250.0f    // filtro paso bajo de la gravedad

#ifndef BOARD_IMU_X_RIGHT_DOWN_SIGN
#define BOARD_IMU_X_RIGHT_DOWN_SIGN -1  // AMOLED-2.16: bajar el lado derecho da X < 0
#endif

static inline bool timeReached(uint32_t now, uint32_t at) { return (int32_t)(now - at) >= 0; }

struct ShakeDetector {
  void reset() { *this = ShakeDetector(); }

  // true en la muestra en la que se reconoce la sacudida
  bool feed(uint32_t t, float x, float y, float z) {
    float v[3] = { x, y, z };
    if (!started) {
      started = true;
      for (int i = 0; i < 3; i++) grav[i] = v[i];
      lastT = t;
    }
    float dt = (float)(int32_t)(t - lastT);
    if (dt < 1.0f) dt = 1.0f;
    lastT = t;
    float k = 1.0f - expf(-dt / SHAKE_AXIS_TAU_MS);
    uint8_t best = 0;
    for (int i = 0; i < 3; i++) {
      grav[i] += k * (v[i] - grav[i]);
      float hv = v[i] - grav[i];
      int8_t s = hv > SHAKE_PEAK_G ? 1 : (hv < -SHAKE_PEAK_G ? -1 : 0);
      if (s && s != sign[i]) {
        if (sign[i]) {  // cambio de sentido
          if (n[i] && (int32_t)(t - rev[i][n[i] - 1]) > SHAKE_MAX_GAP_MS) n[i] = 0;
          if (n[i] == MAXREV) drop(i);
          rev[i][n[i]++] = t;
        }
        sign[i] = s;
      }
      while (n[i] && (int32_t)(t - rev[i][0]) > SHAKE_WINDOW_MS) drop(i);
      if (n[i] > best) best = n[i];
    }
    if (best >= SHAKE_REVERSALS && (!holding || (int32_t)(t - holdUntil) > 0)) {
      holding = true;
      holdUntil = t + SHAKE_HOLD_MS;
      n[0] = n[1] = n[2] = 0;
      count++;
      return true;
    }
    return false;
  }

  uint32_t count = 0;  // sacudidas desde reset()

private:
  static const uint8_t MAXREV = 8;
  void drop(int i) {
    for (uint8_t j = 1; j < n[i]; j++) rev[i][j - 1] = rev[i][j];
    n[i]--;
  }
  bool started = false;
  uint32_t lastT = 0;
  float grav[3] = { 0, 0, 0 };
  int8_t sign[3] = { 0, 0, 0 };
  uint32_t rev[3][MAXREV];
  uint8_t n[3] = { 0, 0, 0 };
  bool holding = false;
  uint32_t holdUntil = 0;
};

static constexpr int8_t FACEDOWN_NONE = 0, FACEDOWN_ENTER = 1, FACEDOWN_LEAVE = -1;

struct FaceDownDetector {
  bool down = false;  // estado actual (ya confirmado)

  void reset() { *this = FaceDownDetector(); }

  // FACEDOWN_ENTER al llevar FACEDOWN_MS boca abajo y quieta, FACEDOWN_LEAVE
  // al levantarla; FACEDOWN_NONE el resto de muestras
  int8_t feed(uint32_t t, float x, float y, float z) {
    if (!down) {
      float mag = sqrtf(x * x + y * y + z * z);
      bool cand = z <= FACEDOWN_Z_G && fabsf(x) <= FACEDOWN_XY_G && fabsf(y) <= FACEDOWN_XY_G &&
                  fabsf(mag - 1.0f) <= FACEDOWN_STILL_G;
      if (!cand) { since = 0; haveSince = false; return FACEDOWN_NONE; }
      if (!haveSince) { haveSince = true; since = t; }
      if ((int32_t)(t - since) >= FACEDOWN_MS) {
        down = true;
        haveSince = false;
        return FACEDOWN_ENTER;
      }
      return FACEDOWN_NONE;
    }
    if (z <= FACEDOWN_LEAVE_Z_G) { haveSince = false; return FACEDOWN_NONE; }
    if (!haveSince) { haveSince = true; since = t; }
    if ((int32_t)(t - since) >= FACEDOWN_LEAVE_MS) {
      down = false;
      haveSince = false;
      return FACEDOWN_LEAVE;
    }
    return FACEDOWN_NONE;
  }

private:
  bool haveSince = false;
  uint32_t since = 0;
};

static constexpr int8_t TILT_LEFT = -1, TILT_LEVEL = 0, TILT_RIGHT = 1;

// Inclinacion lateral: componente de la gravedad (paso bajo) a lo largo del eje
// X de la pantalla, como angulo asin(gx / |g|). Vale igual con la placa de pie
// que tumbada boca arriba. Positiva = lado derecho abajo.
struct TiltEstimator {
  float deg = 0;      // angulo filtrado, + = lado derecho abajo
  int8_t side = TILT_LEVEL;  // lado bajo confirmado (tras TILT_HOLD_MS)
  bool slide = false;        // pasa de TILT_SLIDE_DEG (confirmado)

  void reset() { *this = TiltEstimator(); }

  void feed(uint32_t t, float x, float y, float z, int8_t xRightDownSign = BOARD_IMU_X_RIGHT_DOWN_SIGN) {
    float v[3] = { x, y, z };
    if (!started) {
      started = true;
      for (int i = 0; i < 3; i++) g[i] = v[i];
      lastT = t;
    }
    float dt = (float)(int32_t)(t - lastT);
    if (dt < 1.0f) dt = 1.0f;
    lastT = t;
    float k = 1.0f - expf(-dt / TILT_TAU_MS);
    for (int i = 0; i < 3; i++) g[i] += k * (v[i] - g[i]);
    float mag = sqrtf(g[0] * g[0] + g[1] * g[1] + g[2] * g[2]);
    if (mag < 0.3f) return;  // caida libre o lectura rota: no hay gravedad de la que fiarse
    float s = g[0] * (float)xRightDownSign / mag;
    if (s > 1.0f) s = 1.0f;
    if (s < -1.0f) s = -1.0f;
    deg = asinf(s) * 57.29578f;

    // lado candidato, con histeresis respecto al confirmado
    float a = fabsf(deg);
    int8_t want = deg > 0 ? TILT_RIGHT : TILT_LEFT;
    float enter = TILT_WALK_DEG, keep = TILT_WALK_DEG - TILT_HYST_DEG;
    int8_t cand;
    if (side != TILT_LEVEL && want == side) cand = a >= keep ? side : TILT_LEVEL;
    else cand = a >= enter ? want : TILT_LEVEL;
    bool candSlide = cand != TILT_LEVEL &&
                     a >= (slide && cand == side ? TILT_SLIDE_DEG - TILT_HYST_DEG : TILT_SLIDE_DEG);

    if (cand == TILT_LEVEL) {  // a nivel: se nota al momento
      side = TILT_LEVEL;
      slide = false;
      pending = false;
      return;
    }
    if (cand == side && candSlide == slide) { pending = false; return; }
    if (!pending || cand != pendSide || candSlide != pendSlide) {
      pending = true;
      pendSide = cand;
      pendSlide = candSlide;
      pendSince = t;
    }
    // cambiar de lado o empezar a resbalar espera TILT_HOLD_MS; dejar de
    // resbalar (sigue inclinada pero menos) es inmediato
    if ((cand == side && !candSlide) || (int32_t)(t - pendSince) >= TILT_HOLD_MS) {
      side = cand;
      slide = candSlide;
      pending = false;
    }
  }

private:
  bool started = false;
  uint32_t lastT = 0;
  float g[3] = { 0, 0, 0 };
  bool pending = false;
  int8_t pendSide = TILT_LEVEL;
  bool pendSlide = false;
  uint32_t pendSince = 0;
};

// Inclinacion lateral rapida para la carrera (DASH, docs/NEW_GAMES.md FR-DASH.1): el mismo
// asin(gx / |g|) que TiltEstimator, pero con un filtro de ROLL_FAST_TAU_MS en vez de 250 ms y sin
// esperas ni umbrales. Como solo usa la X de la gravedad, no depende del cabeceo: mide igual con
// la placa de pie, inclinada hacia atras o tumbada boca arriba; solo cambia el cero, y eso lo
// pone la calibracion del juego.
#define ROLL_FAST_TAU_MS 60.0f

struct FastRoll {
  float deg = 0;  // + = lado derecho abajo

  void reset() { *this = FastRoll(); }

  void feed(uint32_t t, float x, float y, float z, int8_t xRightDownSign = BOARD_IMU_X_RIGHT_DOWN_SIGN) {
    float v[3] = { x, y, z };
    if (!started) {
      started = true;
      for (int i = 0; i < 3; i++) g[i] = v[i];
      lastT = t;
    }
    float dt = (float)(int32_t)(t - lastT);
    if (dt < 1.0f) dt = 1.0f;
    lastT = t;
    float k = 1.0f - expf(-dt / ROLL_FAST_TAU_MS);
    for (int i = 0; i < 3; i++) g[i] += k * (v[i] - g[i]);
    float mag = sqrtf(g[0] * g[0] + g[1] * g[1] + g[2] * g[2]);
    if (mag < 0.3f) return;  // caida libre o lectura rota
    float s = g[0] * (float)xRightDownSign / mag;
    if (s > 1.0f) s = 1.0f;
    if (s < -1.0f) s = -1.0f;
    deg = asinf(s) * 57.29578f;
  }

private:
  bool started = false;
  uint32_t lastT = 0;
  float g[3] = { 0, 0, 0 };
};

// Reparte cada muestra entre los detectores, en el orden del prototipo: la
// sacudida primero, para que pause el contador de pasos en la misma muestra.
// Los eventos se acumulan hasta que el juego los recoge (take*).
struct ImuPipeline {
  StepCounter steps;
  ShakeDetector shake;
  FaceDownDetector faceDown;
  TiltEstimator tilt;
  FastRoll roll;  // la carrera (v1.44)
  uint32_t samples = 0;

  void reset() { *this = ImuPipeline(); }

  void feed(uint32_t t, float x, float y, float z) {
    samples++;
    if (shake.feed(t, x, y, z)) {
      steps.pause(t + SHAKE_HOLD_MS);
      if (shakes < 255) shakes++;
    }
    pendingSteps += steps.feed(t, x, y, z);
    int8_t fd = faceDown.feed(t, x, y, z);
    if (fd) faceEvent = fd;  // el ultimo manda: entrar y salir en el mismo lote = salir
    tilt.feed(t, x, y, z);
    roll.feed(t, x, y, z);
  }

  // pasos contados desde la ultima llamada
  uint32_t takeSteps() { uint32_t n = pendingSteps; pendingSteps = 0; return n; }
  uint32_t stepsPending() const { return pendingSteps; }
  // sacudidas desde la ultima llamada
  uint8_t takeShakes() { uint8_t n = shakes; shakes = 0; return n; }
  // ultimo cambio de boca abajo desde la ultima llamada
  int8_t takeFaceDown() { int8_t e = faceEvent; faceEvent = FACEDOWN_NONE; return e; }

private:
  uint32_t pendingSteps = 0;
  uint8_t shakes = 0;
  int8_t faceEvent = FACEDOWN_NONE;
};
