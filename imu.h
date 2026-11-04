#pragma once
#include <stdint.h>

// IMU QMI8658 de la placa: pasos (Tarea 1) y trucos de movimiento (Tarea 2).
// docs/FEATURES.md, "IMU sampling". Solo acelerometro (+-8 g, 62,5 Hz, sin
// filtro paso bajo), leido del FIFO en modo stream: cada muestra pasa por los
// detectores puros de motion.h / steps.h. Ni el podometro ni los motores de
// movimiento del chip se usan (en esta placa el podometro no cuenta nada).
//
// El sketch solo incluye esta cabecera: SensorLib vive dentro de imu.cpp, y el
// renderizador del PC la sustituye por stubs (tools/render/stubs.cpp).
//
// Todo el trafico I2C del IMU va en el hilo del loop (como el tactil y el PMU).

#define FIFO_READ_LIT_MS 100    // pantalla encendida: sacudida e inclinacion al momento
#define FIFO_READ_DARK_MS 1000  // a oscuras: el FIFO guarda ~2,1 s, de sobra

bool imuBegin();    // configura el chip; false si no contesta (todo lo del IMU se esconde)
bool imuPresent();
// lee el FIFO si toca (cada FIFO_READ_LIT_MS o FIFO_READ_DARK_MS) y alimenta
// los detectores; lit = pantalla encendida y sin atenuar del todo
void imuPoll(uint32_t now, bool lit);

uint32_t imuTakeSteps();     // pasos contados desde la ultima llamada
uint32_t imuStepsPending();  // ...sin recogerlos (comando IMU)
uint8_t imuTakeShakes();     // sacudidas desde la ultima llamada
int8_t imuTakeFaceDown();    // +1 se puso boca abajo, -1 la levantaron, 0 nada
bool imuFaceDown();          // boca abajo ahora mismo
int8_t imuTiltSide();        // -1 lado izquierdo abajo, 0 a nivel, +1 derecho abajo
bool imuTiltSlide();         // inclinada de mas: el bicho resbala
float imuTiltDeg();          // angulo lateral, + = lado derecho abajo

// IMULOG: vuelca cada muestra por serie como CSV (t_ms,ax_mg,ay_mg,az_mg,steps,
// el formato de test/data/imu/) durante ms; 0 lo para
void imuLog(uint32_t ms);
bool imuLogging();
void imuReport();  // comando IMU: lecturas, muestras/s, aceleracion, estado
