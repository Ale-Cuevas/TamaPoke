// IMU QMI8658 (docs/FEATURES.md, "IMU sampling"): configuracion, lector propio
// del FIFO y reparto de las muestras a los detectores de motion.h / steps.h.
#include "imu.h"
#include <Arduino.h>
#include <Wire.h>
#include <SensorQMI8658.hpp>
#include "board_amoled_216.h"
#include "motion.h"

// El podometro del propio chip conto 0 pasos en esta placa con todas las
// configuraciones probadas (FEATURES.md, "Measured on the board"), y los pasos
// se cuentan por software. Con 1 se vuelve a encender junto al FIFO y el
// comando IMU muestra su cuenta al lado de la nuestra, por si se encuentra la
// causa; el juego sigue usando la de software.
#define IMU_HW_PEDOMETER 0

#define IMU_FIFO_SAMPLES 128      // FIFO en modo stream: ~2,1 s a 59,5 Hz
#define IMU_SAMPLE_MS 16.8f       // periodo real medido (59,5 Hz, no 62,5)
#define IMU_CHUNK 20              // muestras por lectura I2C: 120 bytes < 128 del buffer de Wire

// registros y ordenes del QMI8658 que usa el lector (datasheet del QMI8658A)
#define QMI_REG_CTRL9 0x0A
#define QMI_REG_FIFO_CTRL 0x14
#define QMI_REG_FIFO_SMPL_CNT 0x15  // + FIFO_STATUS en 0x16
#define QMI_REG_FIFO_DATA 0x17
#define QMI_REG_STATUSINT 0x2D
#define QMI_CMD_ACK 0x00
#define QMI_CMD_REQ_FIFO 0x05
#define QMI_FIFO_STATUS_OVF 0x20

static SensorQMI8658 qmi;
static bool present = false;
static ImuPipeline flow;
static float scale = 8.0f / 32768.0f;  // g por cuenta a +-8 g
static uint8_t fifoCtrl = 0;           // valor de FIFO_CTRL para salir del modo lectura

static uint32_t lastRead = 0;
static bool haveSample = false;
static uint32_t lastSampleT = 0;
static bool dropFirst = true;  // la primera muestra tras resetear el FIFO es basura

static int16_t rawBuf[IMU_FIFO_SAMPLES][3];
static float lastX = 0, lastY = 0, lastZ = 0;
static uint32_t logUntil = 0;

// estadisticas para el comando IMU
static uint32_t nReads = 0, nSamples = 0, nErrors = 0, nOverflows = 0;
static uint32_t lastReadUs = 0, statsSince = 0, statsSamples = 0;
static float rateHz = 0;

static bool regWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(BOARD_I2C_ADDR_IMU);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

static bool regRead(uint8_t reg, uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(BOARD_I2C_ADDR_IMU);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint16_t)BOARD_I2C_ADDR_IMU, (size_t)len) != len) return false;
  for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

// orden CTRL9 con el apreton de manos de STATUSINT.bit7 (CTRL8.bit7 = 1, como
// lo deja SensorLib al arrancar)
static bool ctrl9(uint8_t cmd) {
  uint8_t st = 0;
  if (!regWrite(QMI_REG_CTRL9, cmd)) return false;
  uint32_t t0 = millis();
  do {
    if (!regRead(QMI_REG_STATUSINT, &st, 1)) return false;
    if (millis() - t0 > 50) return false;
  } while (!(st & 0x80));
  regWrite(QMI_REG_CTRL9, QMI_CMD_ACK);
  t0 = millis();
  do {
    if (!regRead(QMI_REG_STATUSINT, &st, 1)) return false;
    if (millis() - t0 > 50) return false;
  } while (st & 0x80);
  return true;
}

// SensorLib (readFromFifo) pide el FIFO entero en una sola transaccion I2C, hasta
// 768 bytes contra los 128 del buffer de Wire, y falla (y se cuelga ~1 s). Aqui
// se lee a trozos de 20 muestras, como la sonda del bring-up. Devuelve las
// muestras leidas (0 si no habia o hubo error; *err distingue).
static uint16_t readFifo(bool *err, bool *overflow) {
  *err = false;
  *overflow = false;
  uint8_t cnt[2];
  if (!regRead(QMI_REG_FIFO_SMPL_CNT, cnt, 2)) { *err = true; return 0; }
  *overflow = cnt[1] & QMI_FIFO_STATUS_OVF;
  uint16_t bytes = 2 * ((((uint16_t)cnt[1] & 0x03) << 8) | cnt[0]);
  uint16_t samples = bytes / 6;  // solo acelerometro: 6 bytes por muestra
  if (samples > IMU_FIFO_SAMPLES) samples = IMU_FIFO_SAMPLES;
  if (!samples) return 0;
  if (!ctrl9(QMI_CMD_REQ_FIFO)) { *err = true; return 0; }
  uint16_t got = 0;
  while (got < samples) {
    uint8_t n = (uint8_t)min<uint16_t>(IMU_CHUNK, samples - got);
    uint8_t raw[IMU_CHUNK * 6];
    if (!regRead(QMI_REG_FIFO_DATA, raw, n * 6)) { *err = true; break; }
    for (uint8_t i = 0; i < n; i++)
      for (uint8_t a = 0; a < 3; a++)  // little-endian (CTRL1 tras el reset de SensorLib)
        rawBuf[got + i][a] = (int16_t)(raw[i * 6 + a * 2] | (raw[i * 6 + a * 2 + 1] << 8));
    got += n;
  }
  regWrite(QMI_REG_FIFO_CTRL, fifoCtrl);  // sale del modo lectura del FIFO
  return got;
}

bool imuBegin() {
  present = qmi.begin(Wire, BOARD_I2C_ADDR_IMU, BOARD_I2C_SDA, BOARD_I2C_SCL);
  if (!present) {
    Serial.println("[imu] QMI8658 no contesta: sin pasos ni trucos de movimiento");
    return false;
  }
  qmi.disableGyroscope();
  // +-8 g: a +-4 g se recortaban las sacudidas fuertes y la carrera. LPF_OFF:
  // SensorLib enciende por defecto un filtro de ~1,7 Hz que aplana cada paso.
  qmi.configAccelerometer(SensorQMI8658::ACC_RANGE_8G, SensorQMI8658::ACC_ODR_62_5Hz, SensorQMI8658::LPF_OFF);
  qmi.enableAccelerometer();
  scale = qmi.getAccelerometerScales();
#if IMU_HW_PEDOMETER
  qmi.configPedometer(50, 200, 100, 200, 20, 10, 0, 4);
  qmi.enablePedometer(SensorQMI8658::INTERRUPT_PIN_DISABLE);
#endif
  int r = qmi.configFIFO(SensorQMI8658::FIFO_MODE_STREAM, SensorQMI8658::FIFO_SAMPLES_128,
                         SensorQMI8658::INTERRUPT_PIN_DISABLE, 64);
  fifoCtrl = (uint8_t)((SensorQMI8658::FIFO_SAMPLES_128 << 2) | SensorQMI8658::FIFO_MODE_STREAM);
  if (r != 0) Serial.println("[imu] configFIFO fallo");
  flow.reset();
  dropFirst = true;
  haveSample = false;
  lastRead = statsSince = millis();
  Serial.printf("[imu] QMI8658 ok: +-8 g, 62.5 Hz, sin LPF, FIFO stream %u\n", IMU_FIFO_SAMPLES);
  return true;
}

bool imuPresent() { return present; }

static void readNow(uint32_t now) {
  bool err, ovf;
  uint32_t t0 = micros();
  uint16_t n = readFifo(&err, &ovf);
  lastReadUs = micros() - t0;
  nReads++;
  if (err) nErrors++;
  if (ovf) {  // se perdieron muestras (el loop estuvo parado mas de ~2 s): hueco
    nOverflows++;
    haveSample = false;
  }
  uint16_t first = 0;
  if (n && dropFirst) {
    first = 1;
    dropFirst = false;
  }
  uint16_t count = n - first;
  if (!count) return;

  // Marcas de tiempo repartidas por igual entre la ultima muestra y ahora. Si
  // el hueco no cuadra con las muestras leidas (primera lectura, desborde), se
  // supone el ritmo medido hacia atras desde ahora.
  uint32_t span = now - lastSampleT;
  uint32_t start = lastSampleT;
  if (!haveSample || span > (uint32_t)(count * IMU_SAMPLE_MS * 1.6f) || span < (uint32_t)(count * IMU_SAMPLE_MS * 0.4f))
    start = now - (uint32_t)(count * IMU_SAMPLE_MS);
  if (haveSample && (int32_t)(start - lastSampleT) < 0) start = lastSampleT;  // nunca hacia atras
  uint32_t width = now - start;
  for (uint16_t i = 0; i < count; i++) {
    const int16_t *v = rawBuf[first + i];
    float x = v[0] * scale, y = v[1] * scale, z = v[2] * scale;
    uint32_t t = start + (uint32_t)((uint64_t)(i + 1) * width / count);
    flow.feed(t, x, y, z);
    if (logUntil) {
      Serial.printf("%lu,%d,%d,%d,%lu\n", (unsigned long)t, (int)lroundf(x * 1000), (int)lroundf(y * 1000),
                    (int)lroundf(z * 1000), (unsigned long)flow.steps.total);
    }
    lastX = x;
    lastY = y;
    lastZ = z;
  }
  lastSampleT = now;
  haveSample = true;
  nSamples += count;
  statsSamples += count;
  if (now - statsSince >= 10000) {
    rateHz = statsSamples * 1000.0f / (now - statsSince);
    statsSince = now;
    statsSamples = 0;
  }
}

void imuPoll(uint32_t now, bool lit) {
  if (!present) return;
  if (logUntil && (int32_t)(now - logUntil) >= 0) {
    logUntil = 0;
    Serial.println("[imulog] end");
  }
  uint32_t every = (lit || logUntil) ? FIFO_READ_LIT_MS : FIFO_READ_DARK_MS;
  if (now - lastRead < every) return;
  lastRead = now;
  readNow(now);
}

uint32_t imuTakeSteps() { return flow.takeSteps(); }
uint32_t imuStepsPending() { return flow.stepsPending(); }
uint8_t imuTakeShakes() { return flow.takeShakes(); }
int8_t imuTakeFaceDown() { return flow.takeFaceDown(); }
bool imuFaceDown() { return flow.faceDown.down; }
int8_t imuTiltSide() { return flow.tilt.side; }
bool imuTiltSlide() { return flow.tilt.slide; }
float imuTiltDeg() { return flow.tilt.deg; }

void imuLog(uint32_t ms) {
  if (!present) return;
  logUntil = ms ? millis() + ms : 0;
  if (ms) Serial.println("t_ms,ax_mg,ay_mg,az_mg,steps");
  else Serial.println("[imulog] end");
}

bool imuLogging() { return logUntil != 0; }

void imuReport() {
  if (!present) {
    Serial.println("imu=none");
    return;
  }
  Serial.printf("imu reads=%lu samples=%lu rate=%.1fHz err=%lu ovf=%lu read=%luus pending=%lu counted=%lu\n",
                (unsigned long)nReads, (unsigned long)nSamples, rateHz, (unsigned long)nErrors,
                (unsigned long)nOverflows, (unsigned long)lastReadUs, (unsigned long)flow.stepsPending(),
                (unsigned long)flow.steps.total);
  Serial.printf("a=%+.2f,%+.2f,%+.2f g tilt=%+.1fdeg side=%d slide=%d facedown=%d shakes=%lu streak=%u\n", lastX,
                lastY, lastZ, flow.tilt.deg, flow.tilt.side, flow.tilt.slide ? 1 : 0, flow.faceDown.down ? 1 : 0,
                (unsigned long)flow.shake.count, flow.steps.streakLen());
#if IMU_HW_PEDOMETER
  Serial.printf("hw pedometer=%lu\n", (unsigned long)qmi.getPedometerCounter());
#endif
}
