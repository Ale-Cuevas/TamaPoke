#pragma once
#include <math.h>
#include <stdint.h>

// Minijuego "toques" con la pokeball (v1.33). Logica pura, con tests en test/test_ballgame.cpp.
//
// Hasta la v1.32 habia tres rampas segun la puntuacion (deriva de salida, gravedad e impulso
// del toque) y todas se paraban pronto: a los 16, 31 y 48 puntos. Como la gravedad y el
// impulso crecian juntos, el tiempo en el aire casi no cambiaba, y pasados los 48 puntos ya
// no cambiaba nada. Ahora el arco tiene siempre la forma que tenia con 1 punto y lo que sube
// es la velocidad: un factor que escala el tiempo de toda la fisica. Con 80 puntos la bola va
// el doble de rapida que con 1 (el mismo camino en la mitad de tiempo), y sigue subiendo al
// mismo ritmo hasta un tope de 3x (159 puntos).

#define BALL_STEP_MS 85.0f        // un paso de fisica: el periodo de frame con el que se afino
#define BALL_LATE_MAX 3.0f        // frame anormalmente tardio: como mucho 3 pasos de golpe
#define BALL_GRAV 0.413f          // gravedad por paso (la de 1 punto hasta la v1.32)
#define BALL_LIFT 6.82f           // impulso hacia arriba de cada toque (idem)
#define BALL_SPAWN_VX 1.65f       // deriva horizontal al salir (idem)
#define BALL_NUDGE 0.12f          // el toque empuja de lado segun donde se de
#define BALL_VX_MAX 6.5f
#define BALL_CX 233.0f            // centro de la pantalla redonda (CX, CY del sketch)
#define BALL_CY 233.0f
#define BALL_WALL_R 205.0f        // pared circular donde rebota
#define BALL_WALL_DAMP 0.85f      // energia que conserva al rebotar
#define BALL_SPAWN_Y 96.0f
#define BALL_FLOOR_Y 384.0f       // por debajo: al suelo (fallo)
#define BALL_SPEED_DOUBLE_AT 80   // con esta puntuacion va el doble de rapida que con 1
#define BALL_SPEED_MAX 3.0f       // tope de velocidad (con 159 puntos)

struct BallState {
  float x, y, vx, vy;
};

// velocidad del juego con `score` puntos: 1x con 0 o 1 punto, 2x con 80, tope 3x
inline float ballSpeed(uint16_t score) {
  if (score <= 1) return 1.0f;
  float s = 1.0f + (float)(score - 1) / (float)(BALL_SPEED_DOUBLE_AT - 1);
  return s > BALL_SPEED_MAX ? BALL_SPEED_MAX : s;
}

// pasos de fisica que tocan tras `dtMs` de reloj con `score` puntos. El tope de frame tardio
// va antes del factor de velocidad: la velocidad nunca se recorta por un frame lento.
inline float ballStepK(uint32_t dtMs, uint16_t score) {
  float k = (float)dtMs / BALL_STEP_MS;
  if (k > BALL_LATE_MAX) k = BALL_LATE_MAX;
  return k * ballSpeed(score);
}

// sale arriba en x, hacia la derecha (dir > 0) o la izquierda
inline void ballSpawn(BallState &b, float x, int dir) {
  b.x = x;
  b.y = BALL_SPAWN_Y;
  b.vx = dir > 0 ? BALL_SPAWN_VX : -BALL_SPAWN_VX;
  b.vy = 0;
}

// toque a la bola; dx = bola.x - dedo.x (darle por un lado la empuja hacia el otro)
inline void ballHit(BallState &b, float dx) {
  b.vy = -BALL_LIFT;
  b.vx += dx * BALL_NUDGE;
  if (b.vx > BALL_VX_MAX) b.vx = BALL_VX_MAX;
  if (b.vx < -BALL_VX_MAX) b.vx = -BALL_VX_MAX;
}

// avanza k pasos: parabola exacta (la gravedad es constante), asi el camino no depende de lo
// largo que sea el paso y a doble velocidad la bola recorre el mismo arco en la mitad de
// tiempo. Despues, el rebote en la pared circular.
inline void ballStep(BallState &b, float k) {
  b.x += b.vx * k;
  b.y += b.vy * k + 0.5f * BALL_GRAV * k * k;
  b.vy += BALL_GRAV * k;
  float dx = b.x - BALL_CX, dy = b.y - BALL_CY;
  float d = sqrtf(dx * dx + dy * dy);
  if (d > BALL_WALL_R) {
    float nx = dx / d, ny = dy / d;
    float dot = b.vx * nx + b.vy * ny;
    if (dot > 0) {
      b.vx = (b.vx - 2 * dot * nx) * BALL_WALL_DAMP;
      b.vy = (b.vy - 2 * dot * ny) * BALL_WALL_DAMP;
    }
    b.x = BALL_CX + nx * BALL_WALL_R;
    b.y = BALL_CY + ny * BALL_WALL_R;
  }
}

inline bool ballOnFloor(const BallState &b) { return b.y > BALL_FLOOR_Y; }
