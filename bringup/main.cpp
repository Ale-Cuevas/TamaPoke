// Bring-up firmware for the Waveshare ESP32-S3-Touch-AMOLED-2.16, adapted from
// the Digipet fork (MIT). Exercises each peripheral on its own and logs to USB
// serial, so a failure points at one chip. Init sequences follow Waveshare's
// demo sketches; see board_amoled_216.h for where each value came from.
// Not part of the TamaPoke sketch: build and flash it with PlatformIO,
//   pio run -e bringup -t upload && pio device monitor -e bringup
// The checks to run with it are in docs/DEVICE_TESTS.md (Phase 1).

#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>

#include "../board_amoled_216.h"  // before XPowersLib: it selects the PMIC driver
#include <XPowersLib.h>
#include <FS.h>
#include <SD.h>
#include <SD_MMC.h>
#include <SPI.h>
#include <TouchDrvCSTXXX.hpp>
#include <SensorQMI8658.hpp>
#include <SensorPCF85063.hpp>

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    BOARD_LCD_CS, BOARD_LCD_SCLK, BOARD_LCD_SDIO0, BOARD_LCD_SDIO1, BOARD_LCD_SDIO2, BOARD_LCD_SDIO3);
Arduino_CO5300 *panel = new Arduino_CO5300(
    bus, BOARD_LCD_RESET, 0, BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT,
    BOARD_LCD_COL_OFFSET, BOARD_LCD_ROW_OFFSET, BOARD_LCD_COL_OFFSET, BOARD_LCD_ROW_OFFSET);

XPowersPMU pmu;
TouchDrvCST92xx touch;
SensorQMI8658 imu;
SensorPCF85063 rtc;
bool pmuOk = false;
bool displayOk = false;
bool touchOk = false;
bool imuOk = false;
bool rtcOk = false;
const char *sdMode = nullptr;  // "SDMMC 1-bit", "SPI", or nullptr (no card)
volatile bool touchPending = false;

struct ButtonInput {
  const char *name;
  uint8_t pin;
  uint8_t pressedLevel;
  bool pressed;
};

ButtonInput buttons[] = {
    {"BOOT", BOARD_BTN_BOOT, BOARD_BTN_PRESSED_LEVEL, false},
    {"USER", BOARD_BTN_USER, BOARD_BTN_PRESSED_LEVEL, false},
    {"PWR", BOARD_BTN_PWR_SENSE, BOARD_BTN_PWR_PRESSED_LEVEL, false},
};

const uint8_t BRINGUP_BRIGHTNESS = 160;
const uint32_t STATUS_INTERVAL_MS = 2000;
const uint16_t RTC_VALID_FROM_YEAR = 2024;  // older means never set or lost power
const int16_t STATUS_BAND_X = 32;  // clear of the 24 px corner markers
const int16_t STATUS_BAND_Y = 300;

void initPmu() {
  pmuOk = pmu.begin(Wire, BOARD_I2C_ADDR_PMIC, BOARD_I2C_SDA, BOARD_I2C_SCL);
  if (!pmuOk) {
    Serial.println("[pmu] AXP2101 not found");
    return;
  }
  pmu.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);
  pmu.enableBattDetection();
  pmu.enableVbusVoltageMeasure();
  pmu.enableBattVoltageMeasure();
  pmu.enableSystemVoltageMeasure();
  Serial.printf("[pmu] AXP2101 ok, chip id 0x%02X\n", pmu.getChipID());
}

void logPmu() {
  if (!pmuOk) return;
  Serial.printf("[pmu] vbus %s %u mV, sys %u mV, batt %s %u mV %d%%, charging %s\n",
                pmu.isVbusIn() ? "in" : "out", pmu.getVbusVoltage(), pmu.getSystemVoltage(),
                pmu.isBatteryConnect() ? "yes" : "no", pmu.getBattVoltage(),
                pmu.isBatteryConnect() ? pmu.getBatteryPercent() : -1,
                pmu.isCharging() ? "yes" : "no");
}

void initDisplay() {
  displayOk = panel->begin();
  if (!displayOk) {
    Serial.println("[lcd] panel->begin() failed");
    return;
  }
  bus->writeC8D8(BOARD_LCD_MADCTL_REG, BOARD_LCD_MADCTL_VALUE);
  panel->setBrightness(BRINGUP_BRIGHTNESS);
  Serial.printf("[lcd] CO5300 ok, %dx%d\n", panel->width(), panel->height());
}

void IRAM_ATTR onTouchInterrupt() {
  touchPending = true;
}

void initTouch() {
  touch.setPins(BOARD_TP_RST, BOARD_TP_INT);
  touchOk = touch.begin(Wire, BOARD_I2C_ADDR_TOUCH, BOARD_I2C_SDA, BOARD_I2C_SCL);
  if (!touchOk) {
    Serial.println("[touch] CST92xx not found");
    return;
  }
  // TamaPoke resets the chip after begin(): on the 1.75" board begin() left the
  // CST9217 in command mode and it reported nothing until reset. Same here, so
  // this test sees the touch the way the game does.
  touch.reset();
  touch.setMaxCoordinates(BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT);
  touch.setSwapXY(BOARD_TP_SWAP_XY);
  touch.setMirrorXY(BOARD_TP_MIRROR_X, BOARD_TP_MIRROR_Y);
  pinMode(BOARD_TP_INT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BOARD_TP_INT), onTouchInterrupt, FALLING);
  Serial.printf("[touch] %s ok, %u points\n", touch.getModelName(), touch.getSupportTouchPoint());
}

// Logs each touch and leaves a dot on screen, so the mapping can be checked
// against the corner markers.
void pollTouch() {
  if (!touchOk) return;
  noInterrupts();
  const bool pending = touchPending;
  touchPending = false;
  interrupts();
  if (!pending) return;

  // getPoint() writes every finger the chip reports (CST92XX_MAX_FINGER_NUM,
  // 2) whatever count it is asked for: the arrays must hold that many.
  int16_t x[2] = {0}, y[2] = {0};
  const uint8_t points = touch.getPoint(x, y, touch.getSupportTouchPoint());
  for (uint8_t i = 0; i < points; i++) {
    Serial.printf("[touch] #%u x=%d y=%d\n", i, x[i], y[i]);
    if (displayOk) {
      // 8x8 square on even coordinates (CO5300 window alignment, see the frame).
      const int16_t dx = constrain(x[i] - 4, 0, BOARD_LCD_WIDTH - 8) & ~1;
      const int16_t dy = constrain(y[i] - 4, 0, BOARD_LCD_HEIGHT - 8) & ~1;
      panel->fillRect(dx, dy, 8, 8, i == 0 ? RGB565_CYAN : RGB565_MAGENTA);
    }
  }
}

// Same accelerometer config as the demo, plus the gyro so both halves of
// the chip get exercised.
void initImu() {
  imuOk = imu.begin(Wire, BOARD_I2C_ADDR_IMU, BOARD_I2C_SDA, BOARD_I2C_SCL);
  if (!imuOk) {
    Serial.println("[imu] QMI8658 not found");
    return;
  }
  imu.configAccelerometer(SensorQMI8658::ACC_RANGE_4G, SensorQMI8658::ACC_ODR_1000Hz, SensorQMI8658::LPF_MODE_0);
  imu.configGyroscope(SensorQMI8658::GYR_RANGE_256DPS, SensorQMI8658::GYR_ODR_896_8Hz, SensorQMI8658::LPF_MODE_0);
  imu.enableAccelerometer();
  imu.enableGyroscope();
  // SensorLib's getChipID() reads the revision register (0x01), not WHO_AM_I
  // (0x00, always 0x05): print both so the log matches the datasheet.
  Serial.printf("[imu] QMI8658 ok, who-am-i 0x%02X, revision 0x%02X\n",
                (unsigned)imu.whoAmI(), (unsigned)imu.getChipID());
}

// --- IMU probe ---------------------------------------------------------------
// Groundwork for docs/FEATURES.md ("Measured on the board"). After the Phase 1
// checks, runs the IMU the way the game is meant to (accel +-4 g at 62.5 Hz,
// gyro off, the hardware pedometer, the any-motion engine, activity events on
// INT1) and shows what it sees in big type, so it can be read while walking
// with the USB cable out:
//   BOOT  records a 15 s capture of the current label to the SD card, as
//         /imu/<label>-<n>.csv, after a 3 s countdown
//   USER  picks the next label
// Serial: imulog <s> (live CSV for s seconds), imureinit (re-runs begin(): does
// it clear the step counter?), imudump (prints every capture), imurm (deletes
// them), rec, label.
const bool IMU_PROBE = true;          // false: the plain Phase 1 bring-up
const uint32_t PROBE_SAMPLE_MS = 16;  // the 62.5 Hz output data rate
const uint32_t PROBE_REC_MS = 15000;
const uint32_t PROBE_COUNTDOWN_MS = 3000;
const uint32_t PROBE_STATUS_MS = 1000;  // STATUS1 poll, panel redraw, log line
const uint16_t PROBE_MAX_SAMPLES = PROBE_REC_MS / PROBE_SAMPLE_MS + 16;
const char *const PROBE_LABELS[] = {"flat-up", "face-down", "upright", "tilt-right", "walk-pocket",
                                    "walk-hand", "run", "shake-soft", "shake-hard"};
const uint8_t PROBE_LABEL_COUNT = sizeof(PROBE_LABELS) / sizeof(PROBE_LABELS[0]);

struct ProbeSample {
  uint32_t t;
  int16_t ax, ay, az;  // milli-g
  uint32_t steps;      // raw hardware counter
};

ProbeSample probeBuf[PROBE_MAX_SAMPLES];
bool probeOn = false;
uint8_t probeLabel = 0;
enum RecState { REC_IDLE, REC_COUNTDOWN, REC_RUNNING };
RecState recState = REC_IDLE;
uint32_t recStart = 0;
uint16_t recCount = 0;
char recLastFile[48] = "-";
uint32_t streamUntil = 0;     // imulog end time, 0 = not streaming
uint32_t anyMotionPolls = 0;  // 1 Hz STATUS1 reads with the any-motion bit set
uint32_t pedoPolls = 0;       // ... with the pedometer bit set
int lastStatus1 = -1;
float accX = 0, accY = 0, accZ = 0;
uint32_t stepsRaw = 0;
volatile uint32_t int1Edges = 0;
volatile uint32_t int2Edges = 0;

void IRAM_ATTR onImuInt1() { int1Edges = int1Edges + 1; }
void IRAM_ATTR onImuInt2() { int2Edges = int2Edges + 1; }

int16_t toMilliG(float g) { return (int16_t)lroundf(g * 1000.0f); }

fs::FS *probeFs() {
  if (!sdMode) return nullptr;
  if (strcmp(sdMode, "SPI") == 0) return &SD;
  return &SD_MMC;
}

// The axis gravity pulls on most, with its sign: "-Y" means Y reads about -1 g.
const char *downAxis() {
  const float ax = fabsf(accX), ay = fabsf(accY), az = fabsf(accZ);
  if (ax >= ay && ax >= az) return accX >= 0 ? "+X" : "-X";
  if (ay >= az) return accY >= 0 ? "+Y" : "-Y";
  return accZ >= 0 ? "+Z" : "-Z";
}

// "imucfg <n>" or holding USER for 1 s picks the configuration; 0 is the one
// the game is meant to run. The first probe run used the library's default
// low-pass filter (LPF_MODE_0, 2.66 % of the ODR: about 1.7 Hz at 62.5 Hz),
// which flattened every step and shake and the pedometer counted nothing.
//   0  4 G, LPF off, pedometer + any-motion
//   1  4 G, LPF mode 3 (13.37 % of the ODR, about 8 Hz), pedometer + any-motion
//   2  2 G, LPF mode 0, pedometer only: exactly the SensorLib pedometer example
const uint8_t PROBE_CFG_COUNT = 3;
const char *const PROBE_CFG_NAMES[PROBE_CFG_COUNT] = {"4G LPF off", "4G LPF 8Hz", "2G LPF 1.7Hz"};
uint8_t probeCfg = 0;

void configureImuProbe() {
  const bool twoG = probeCfg == 2;
  imu.disableMotionDetect();
  imu.disablePedometer();
  imu.disableGyroscope();
  const SensorQMI8658::LpfMode lpf = probeCfg == 0 ? SensorQMI8658::LPF_OFF
                                     : probeCfg == 1 ? SensorQMI8658::LPF_MODE_3
                                                     : SensorQMI8658::LPF_MODE_0;
  imu.configAccelerometer(twoG ? SensorQMI8658::ACC_RANGE_2G : SensorQMI8658::ACC_RANGE_4G,
                          SensorQMI8658::ACC_ODR_62_5Hz, lpf);
  imu.enableAccelerometer();
  // The SensorLib pedometer example's values: 50-sample window, peak-to-peak
  // 200 mg, peak 100 mg, step timeout 200 samples, shortest step 20 samples,
  // 10 steps in a row before counting starts, registers updated every 4 steps.
  imu.configPedometer(50, 200, 100, 200, 20, 10, 0, 4);
  const bool ped = imu.enablePedometer(SensorQMI8658::INTERRUPT_PIN_1);
  bool mot = false;
  if (probeCfg != 2) {
    // Any-motion on all three axes: a slope over 300 mg for one sample. Kept
    // under 1 g because SensorLib's mgToBytes() drops the integer bits.
    const uint8_t ctrl = SensorQMI8658::ANY_MOTION_EN_X | SensorQMI8658::ANY_MOTION_EN_Y |
                         SensorQMI8658::ANY_MOTION_EN_Z;
    imu.configMotion(ctrl, 300, 300, 300, 1, 100, 100, 100, 1, 1, 1);
    mot = imu.enableMotionDetect(SensorQMI8658::INTERRUPT_PIN_1);
  }
  Serial.printf("[probe] cfg %u (%s): 62.5Hz, gyro off, pedometer %s, any-motion %s, activity on INT1\n",
                probeCfg, PROBE_CFG_NAMES[probeCfg], ped ? "on" : "FAILED", mot ? "on" : "off");
}

// "imufifo <s>": the FIFO in stream mode (128 samples, about 2 s at 62.5 Hz),
// read once a second, to see whether a sleeping game could get every sample.
const uint32_t FIFO_READ_MS = 1000;
IMUdata fifoBuf[128];
uint32_t fifoUntil = 0;
uint32_t fifoLastRead = 0;
uint32_t fifoStart = 0;
uint32_t fifoTotal = 0;

void startFifoTest(uint32_t secs) {
  const int r = imu.configFIFO(SensorQMI8658::FIFO_MODE_STREAM, SensorQMI8658::FIFO_SAMPLES_128,
                               SensorQMI8658::INTERRUPT_PIN_DISABLE, 64);
  fifoStart = fifoLastRead = millis();
  fifoUntil = fifoStart + secs * 1000UL;
  fifoTotal = 0;
  Serial.printf("[fifo] stream mode, 128 samples: %s\n", r == 0 ? "ok" : "FAILED");
}

bool imuWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(BOARD_I2C_ADDR_IMU);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool imuRead(uint8_t reg, uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(BOARD_I2C_ADDR_IMU);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint16_t)BOARD_I2C_ADDR_IMU, (size_t)len) != len) return false;
  for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

// CTRL9 command with the STATUSINT.bit7 handshake (CTRL8.bit7 = 1).
bool imuCtrl9(uint8_t cmd) {
  uint8_t st = 0;
  if (!imuWrite(0x0A, cmd)) return false;
  uint32_t t0 = millis();
  do {
    if (!imuRead(0x2D, &st, 1)) return false;
    if (millis() - t0 > 50) return false;
  } while (!(st & 0x80));
  imuWrite(0x0A, 0x00);  // CTRL_CMD_ACK
  t0 = millis();
  do {
    if (!imuRead(0x2D, &st, 1)) return false;
    if (millis() - t0 > 50) return false;
  } while (st & 0x80);
  return true;
}

// SensorLib's readFromFifo() asks Wire for the whole FIFO (up to 768 bytes) in
// one transaction; arduino-esp32's I2C buffer is 128 bytes, so it fails with
// ESP_ERR_INVALID_ARG. This reads it 20 samples (120 bytes) at a time.
uint16_t readFifoChunked(IMUdata *out, uint16_t maxSamples, float scale) {
  uint8_t cnt[2];
  if (!imuRead(0x15, cnt, 2)) return 0;  // FIFO_SMPL_CNT, FIFO_STATUS
  uint16_t bytes = 2 * (((cnt[1] & 0x03) << 8) | cnt[0]);
  uint16_t samples = bytes / 6;
  if (samples > maxSamples) samples = maxSamples;
  if (!samples) return 0;
  if (!imuCtrl9(0x05)) return 0;  // CTRL_CMD_REQ_FIFO: FIFO read mode
  uint16_t got = 0;
  while (got < samples) {
    const uint8_t n = (uint8_t)min<uint16_t>(20, samples - got);
    uint8_t raw[120];
    if (!imuRead(0x17, raw, n * 6)) break;  // FIFO_DATA
    for (uint8_t i = 0; i < n; i++) {
      const int16_t *v = reinterpret_cast<const int16_t *>(&raw[i * 6]);
      out[got + i] = {v[0] * scale, v[1] * scale, v[2] * scale};
    }
    got += n;
  }
  imuWrite(0x14, (3 << 2) | 2);  // FIFO_CTRL: 128 samples, stream, read mode off
  return got;
}

void fifoTick() {
  if (!fifoUntil) return;
  const uint32_t now = millis();
  if (now - fifoLastRead < FIFO_READ_MS) return;
  fifoLastRead = now;
  const uint32_t t0 = micros();
  const uint16_t n = readFifoChunked(fifoBuf, 128, imu.getAccelerometerScales());
  const uint32_t us = micros() - t0;
  fifoTotal += n;
  float minA = 99, maxA = 0;
  for (uint16_t i = 0; i < n; i++) {
    const IMUdata &d = fifoBuf[i];
    const float m = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
    minA = min(minA, m);
    maxA = max(maxA, m);
  }
  if (n) {
    Serial.printf("[fifo] %u samples, read in %lu us, first %+.2f %+.2f %+.2f, last %+.2f %+.2f %+.2f, |a| %.2f..%.2f g\n",
                  n, (unsigned long)us, fifoBuf[0].x, fifoBuf[0].y, fifoBuf[0].z, fifoBuf[n - 1].x,
                  fifoBuf[n - 1].y, fifoBuf[n - 1].z, minA, maxA);
  } else {
    Serial.printf("[fifo] 0 samples, read in %lu us\n", (unsigned long)us);
  }
  if ((int32_t)(now - fifoUntil) >= 0) {
    Serial.printf("[fifo] %lu samples in %lu ms = %.1f Hz\n", (unsigned long)fifoTotal,
                  (unsigned long)(now - fifoStart), fifoTotal * 1000.0f / (now - fifoStart));
    imu.configFIFO(SensorQMI8658::FIFO_MODE_BYPASS);
    fifoUntil = 0;
  }
}

// Raw register dump over Wire, independent of SensorLib's view of the chip.
void dumpImuRegs() {
  struct {
    const char *name;
    uint8_t reg, len;
  } blocks[] = {{"CTRL1..CTRL9", 0x02, 9}, {"STATUSINT STATUS0 STATUS1", 0x2D, 3}, {"STEP_CNT L M H", 0x5A, 3}};
  for (auto &b : blocks) {
    Wire.beginTransmission(BOARD_I2C_ADDR_IMU);
    Wire.write(b.reg);
    const uint8_t err = Wire.endTransmission(false);
    const uint8_t got = Wire.requestFrom((uint16_t)BOARD_I2C_ADDR_IMU, (size_t)b.len);
    Serial.printf("[imuregs] %s (0x%02X):", b.name, b.reg);
    for (uint8_t i = 0; i < got; i++) Serial.printf(" %02X", Wire.read());
    Serial.printf("%s\n", err || got != b.len ? "  (read error)" : "");
  }
}

void drawProbePanel() {
  if (!displayOk) return;
  const int16_t x = 40;
  panel->fillRect(24, 40, BOARD_LCD_WIDTH - 48, BOARD_LCD_HEIGHT - 64, RGB565_BLACK);
  panel->setTextColor(RGB565_WHITE);
  panel->setTextSize(3);
  panel->setCursor(x, 48);
  panel->printf("cfg %u: %s", probeCfg, PROBE_CFG_NAMES[probeCfg]);
  panel->setTextSize(8);
  panel->setCursor(x, 88);
  panel->printf("%lu", (unsigned long)stepsRaw);
  panel->setTextSize(2);
  panel->setCursor(x, 160);
  panel->print("steps (hardware counter)");
  panel->setCursor(x, 196);
  panel->printf("acc %+.2f %+.2f %+.2f g", accX, accY, accZ);
  panel->setCursor(x, 220);
  panel->printf("down %s   STATUS1 0x%02X", downAxis(), lastStatus1 < 0 ? 0xFF : lastStatus1);
  panel->setCursor(x, 244);
  panel->printf("any-motion %lu  pedo %lu", (unsigned long)anyMotionPolls, (unsigned long)pedoPolls);
  panel->setCursor(x, 268);
  panel->printf("INT1 %lu edges  INT2 %lu", (unsigned long)int1Edges, (unsigned long)int2Edges);
  panel->setTextSize(3);
  panel->setTextColor(RGB565_YELLOW);
  panel->setCursor(x, 306);
  panel->printf("> %s", PROBE_LABELS[probeLabel]);
  panel->setTextSize(2);
  panel->setCursor(x, 346);
  const uint32_t now = millis();
  if (recState == REC_COUNTDOWN) {
    panel->setTextColor(RGB565_CYAN);
    panel->printf("get ready... %lu", (unsigned long)((PROBE_COUNTDOWN_MS - (now - recStart)) / 1000 + 1));
  } else if (recState == REC_RUNNING) {
    panel->setTextColor(RGB565_RED);
    panel->printf("RECORDING %lu s", (unsigned long)((PROBE_REC_MS - (now - recStart)) / 1000 + 1));
  } else {
    panel->setTextColor(RGB565_WHITE);
    panel->print("BOOT: record 15 s");
  }
  panel->setTextColor(RGB565_WHITE);
  panel->setCursor(x, 370);
  panel->printf("last: %s", recLastFile);
  panel->setCursor(x, 394);
  panel->print("USER: label, hold 1s: cfg");
}

void startImuProbe() {
  if (!imuOk) {
    Serial.println("[probe] no IMU, probe not started");
    return;
  }
  configureImuProbe();
  pinMode(BOARD_IMU_INT1, INPUT);
  pinMode(BOARD_IMU_INT2, INPUT);
  Serial.printf("[probe] INT1 (GPIO%d) reads %d, INT2 (GPIO%d) reads %d\n", BOARD_IMU_INT1,
                digitalRead(BOARD_IMU_INT1), BOARD_IMU_INT2, digitalRead(BOARD_IMU_INT2));
  attachInterrupt(digitalPinToInterrupt(BOARD_IMU_INT1), onImuInt1, CHANGE);
  attachInterrupt(digitalPinToInterrupt(BOARD_IMU_INT2), onImuInt2, CHANGE);
  Serial.printf("[probe] step counter after begin() and config: %lu\n",
                (unsigned long)imu.getPedometerCounter());
  probeOn = true;
  if (displayOk) panel->fillScreen(RGB565_BLACK);
  drawProbePanel();
}

void saveCapture() {
  fs::FS *fs = probeFs();
  if (!fs) {
    strlcpy(recLastFile, "no SD card: dropped", sizeof(recLastFile));
    Serial.println("[probe] no SD card, capture dropped");
    return;
  }
  if (!fs->exists("/imu")) fs->mkdir("/imu");
  char path[48];
  for (int n = 1; n < 1000; n++) {
    snprintf(path, sizeof(path), "/imu/%s-c%u-%d.csv", PROBE_LABELS[probeLabel], probeCfg, n);
    if (!fs->exists(path)) break;
  }
  File f = fs->open(path, FILE_WRITE);
  if (!f) {
    strlcpy(recLastFile, "SD write failed", sizeof(recLastFile));
    Serial.printf("[probe] could not open %s\n", path);
    return;
  }
  f.println("t_ms,ax_mg,ay_mg,az_mg,steps");
  for (uint16_t i = 0; i < recCount; i++) {
    const ProbeSample &s = probeBuf[i];
    f.printf("%lu,%d,%d,%d,%lu\n", (unsigned long)s.t, s.ax, s.ay, s.az, (unsigned long)s.steps);
  }
  f.close();
  strlcpy(recLastFile, path + 5, sizeof(recLastFile));  // without "/imu/"
  Serial.printf("[probe] saved %s, %u samples\n", path, recCount);
}

void startRecording() {
  if (recState != REC_IDLE) return;
  recState = REC_COUNTDOWN;
  recStart = millis();
  Serial.printf("[probe] recording '%s' in %lu s\n", PROBE_LABELS[probeLabel],
                (unsigned long)(PROBE_COUNTDOWN_MS / 1000));
  drawProbePanel();
}

void nextLabel() {
  if (recState != REC_IDLE) return;
  probeLabel = (probeLabel + 1) % PROBE_LABEL_COUNT;
  Serial.printf("[probe] label %s\n", PROBE_LABELS[probeLabel]);
  drawProbePanel();
}

// One accelerometer and step-counter read per output sample, into the live
// stream and the capture buffer.
void probeSample() {
  static uint32_t last = 0;
  const uint32_t now = millis();
  if (now - last < PROBE_SAMPLE_MS) return;
  last = now;
  float x, y, z;
  if (imu.getAccelerometer(x, y, z)) {
    accX = x;
    accY = y;
    accZ = z;
  }
  stepsRaw = imu.getPedometerCounter();
  if (streamUntil) {
    if ((int32_t)(now - streamUntil) >= 0) {
      streamUntil = 0;
      Serial.println("[imulog] end");
    } else {
      Serial.printf("%lu,%d,%d,%d,%lu\n", (unsigned long)now, toMilliG(accX), toMilliG(accY),
                    toMilliG(accZ), (unsigned long)stepsRaw);
    }
  }
  if (recState == REC_COUNTDOWN && now - recStart >= PROBE_COUNTDOWN_MS) {
    recState = REC_RUNNING;
    recStart = now;
    recCount = 0;
    Serial.printf("[probe] recording '%s'\n", PROBE_LABELS[probeLabel]);
  }
  if (recState == REC_RUNNING) {
    if (recCount < PROBE_MAX_SAMPLES) {
      probeBuf[recCount++] = {now - recStart, toMilliG(accX), toMilliG(accY), toMilliG(accZ), stepsRaw};
    }
    if (now - recStart >= PROBE_REC_MS) {
      recState = REC_IDLE;
      saveCapture();
      drawProbePanel();
    }
  }
}

// Once a second: STATUS1 (does the any-motion bit latch between reads?), a
// log line and the panel.
void probeStatus() {
  static uint32_t last = 0;
  if (millis() - last < PROBE_STATUS_MS) return;
  last = millis();
  lastStatus1 = imu.getStatusRegister();
  if (lastStatus1 >= 0) {
    if (lastStatus1 & SensorQMI8658::EVENT_ANY_MOTION) anyMotionPolls++;
    if (lastStatus1 & SensorQMI8658::EVENT_PEDOMETER_MOTION) pedoPolls++;
  }
  if (!streamUntil) {
    Serial.printf("[probe] steps %lu, acc %+.2f %+.2f %+.2f g, down %s, STATUS1 0x%02X, "
                  "any-motion %lu, pedo %lu, INT1 %lu edges (now %d), INT2 %lu edges (now %d)\n",
                  (unsigned long)stepsRaw, accX, accY, accZ, downAxis(), lastStatus1 & 0xFF,
                  (unsigned long)anyMotionPolls, (unsigned long)pedoPolls,
                  (unsigned long)int1Edges, digitalRead(BOARD_IMU_INT1),
                  (unsigned long)int2Edges, digitalRead(BOARD_IMU_INT2));
  }
  drawProbePanel();
}

// Prints every capture on the card between "== <path> ==" and "== end ==".
void dumpCaptures(bool remove) {
  fs::FS *fs = probeFs();
  if (!fs) {
    Serial.println("[probe] no SD card");
    return;
  }
  File dir = fs->open("/imu");
  if (!dir || !dir.isDirectory()) {
    Serial.println("[probe] no captures");
    return;
  }
  static char names[64][48];  // off the loop task's 8 KB stack
  uint8_t count = 0;
  for (File f = dir.openNextFile(); f && count < 64; f = dir.openNextFile()) {
    snprintf(names[count++], sizeof(names[0]), "/imu/%s", f.name());
  }
  dir.close();
  for (uint8_t i = 0; i < count; i++) {
    if (remove) {
      fs->remove(names[i]);
      Serial.printf("[probe] removed %s\n", names[i]);
      continue;
    }
    File f = fs->open(names[i]);
    if (!f) continue;
    Serial.printf("== %s ==\n", names[i]);
    while (f.available()) Serial.write(f.read());
    f.close();
  }
  Serial.println(remove ? "[probe] captures removed" : "== end ==");
}

// Read-only: the RTC is never written unless asked to over serial, so a
// device test can check that it kept time across a reset or power cycle.
void initRtc() {
  rtcOk = rtc.begin(Wire, BOARD_I2C_SDA, BOARD_I2C_SCL);
  if (!rtcOk) {
    Serial.println("[rtc] PCF85063 not found");
    return;
  }
  Serial.printf("[rtc] PCF85063 ok, %s\n", rtc.isRunning() ? "running" : "STOPPED");
}

uint32_t countSprites(fs::FS &fs) {
  uint32_t n = 0;
  File dir = fs.open("/mons");
  if (dir && dir.isDirectory()) {
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) n++;
  }
  return n;
}

// Settles how the slot is wired: SDMMC 1-bit on CLK 2 / CMD 1 / D0 3, as
// Waveshare's board package and the game do, then SPI on HSPI, as the
// schematic labels the same lines. If only SPI mounts, the game's sdBegin()
// has to move to SPI. Read-only: never formats the card (the game does that
// if needed).
void initSd() {
  SD_MMC.setPins(BOARD_SD_CLK, BOARD_SD_CMD, BOARD_SD_D0);
  if (SD_MMC.begin("/sdcard", true /* 1-bit */, false /* don't format */)) {
    sdMode = "SDMMC 1-bit";
    Serial.printf("[sd] card ok over SDMMC 1-bit, %llu MB, %u files in /mons\n",
                  SD_MMC.cardSize() / (1024ULL * 1024ULL), countSprites(SD_MMC));
    return;
  }
  Serial.println("[sd] SDMMC 1-bit: no card or not FAT; trying SPI");

  static SPIClass sdSpi(HSPI);  // the display's QSPI bus owns SPI2/FSPI
  sdSpi.begin(BOARD_SD_SCK, BOARD_SD_MISO, BOARD_SD_MOSI, BOARD_SD_CS);
  if (SD.begin(BOARD_SD_CS, sdSpi) && SD.cardType() != CARD_NONE) {
    sdMode = "SPI";
    Serial.printf("[sd] card ok over SPI, %llu MB, %u files in /mons\n",
                  SD.cardSize() / (1024ULL * 1024ULL), countSprites(SD));
    return;
  }
  SD.end();
  sdSpi.end();
  Serial.println("[sd] SPI: no card or not FAT");
}

void initButtons() {
  for (ButtonInput &b : buttons) {
    pinMode(b.pin, INPUT);  // all three have external 10K pull-ups [sch]
    b.pressed = digitalRead(b.pin) == b.pressedLevel;
    Serial.printf("[btn] %s on GPIO%u is %s at boot\n", b.name, b.pin, b.pressed ? "PRESSED" : "up");
  }
}

void pollButtons() {
  for (ButtonInput &b : buttons) {
    const bool pressed = digitalRead(b.pin) == b.pressedLevel;
    if (pressed == b.pressed) continue;
    b.pressed = pressed;
    Serial.printf("[btn] %s %s\n", b.name, pressed ? "down" : "up");
    if (probeOn && pressed && b.pin == BOARD_BTN_BOOT) startRecording();
    // USER acts on release: a short press picks the next label, a 1 s hold
    // the next configuration.
    static uint32_t userDownAt = 0;
    if (probeOn && b.pin == BOARD_BTN_USER) {
      if (pressed) {
        userDownAt = millis();
      } else if (millis() - userDownAt >= 1000 && recState == REC_IDLE) {
        probeCfg = (probeCfg + 1) % PROBE_CFG_COUNT;
        configureImuProbe();
        drawProbePanel();
      } else {
        nextLabel();
      }
    }
  }
}

// "settime YYYY-MM-DD HH:MM:SS" over serial writes the RTC.
void pollSerialCommands() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  int y, mo, d, h, mi, s;
  if (sscanf(line.c_str(), "settime %d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &s) == 6) {
    if (!rtcOk) {
      Serial.println("[rtc] not available");
      return;
    }
    rtc.setDateTime(y, mo, d, h, mi, s);
    Serial.println("[rtc] time set");
  } else if (probeOn && sscanf(line.c_str(), "imulog %d", &s) == 1) {
    Serial.println("[imulog] t_ms,ax_mg,ay_mg,az_mg,steps");
    streamUntil = s > 0 ? millis() + (uint32_t)s * 1000UL : 0;
    if (!streamUntil) Serial.println("[imulog] end");
  } else if (probeOn && line == "imureinit") {
    const uint32_t before = imu.getPedometerCounter();
    imuOk = imu.begin(Wire, BOARD_I2C_ADDR_IMU, BOARD_I2C_SDA, BOARD_I2C_SCL);
    const uint32_t afterBegin = imu.getPedometerCounter();
    configureImuProbe();
    Serial.printf("[probe] step counter: %lu before begin(), %lu after it, %lu after config\n",
                  (unsigned long)before, (unsigned long)afterBegin,
                  (unsigned long)imu.getPedometerCounter());
  } else if (probeOn && sscanf(line.c_str(), "imucfg %d", &s) == 1 && s >= 0 && s < PROBE_CFG_COUNT) {
    probeCfg = (uint8_t)s;
    configureImuProbe();
    drawProbePanel();
  } else if (probeOn && sscanf(line.c_str(), "imufifo %d", &s) == 1 && s > 0) {
    startFifoTest((uint32_t)s);
  } else if (probeOn && line == "imuregs") {
    dumpImuRegs();
  } else if (probeOn && line == "imudump") {
    dumpCaptures(false);
  } else if (probeOn && line == "imurm") {
    dumpCaptures(true);
  } else if (probeOn && line == "rec") {
    startRecording();
  } else if (probeOn && line.startsWith("label ")) {
    const String want = line.substring(6);
    for (uint8_t i = 0; i < PROBE_LABEL_COUNT; i++) {
      if (want == PROBE_LABELS[i]) probeLabel = i;
    }
    Serial.printf("[probe] label %s\n", PROBE_LABELS[probeLabel]);
    drawProbePanel();
  } else if (line.length() > 0) {
    Serial.println("commands: settime YYYY-MM-DD HH:MM:SS");
    if (probeOn) Serial.println("  imulog <s> | imureinit | imucfg 0-2 | imuregs | imufifo <s> | imudump | imurm | rec | label <name>");
  }
}

// Writes IMU, RTC and button state to serial and to a band on screen.
void logSensors() {
  char imuLine[64] = "IMU: MISSING";
  if (imuOk) {
    float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;
    if (imu.getDataReady()) {
      imu.getAccelerometer(ax, ay, az);
      imu.getGyroscope(gx, gy, gz);
    }
    Serial.printf("[imu] acc %.2f %.2f %.2f g, gyr %.1f %.1f %.1f dps\n", ax, ay, az, gx, gy, gz);
    snprintf(imuLine, sizeof(imuLine), "IMU: %+.2f %+.2f %+.2f g", ax, ay, az);
  }

  char rtcLine[64] = "RTC: MISSING";
  if (rtcOk) {
    RTC_DateTime now = rtc.getDateTime();
    const bool valid = now.getYear() >= RTC_VALID_FROM_YEAR;
    snprintf(rtcLine, sizeof(rtcLine), "RTC: %04u-%02u-%02u %02u:%02u:%02u%s",
             now.getYear(), now.getMonth(), now.getDay(), now.getHour(), now.getMinute(),
             now.getSecond(), valid ? "" : " (unset)");
    Serial.printf("[rtc] %s\n", rtcLine + 5);
  }

  char btnLine[64];
  snprintf(btnLine, sizeof(btnLine), "BTN: BOOT %s USER %s PWR %s",
           buttons[0].pressed ? "dn" : "up", buttons[1].pressed ? "dn" : "up",
           buttons[2].pressed ? "dn" : "up");

  if (!displayOk) return;
  const int16_t lineH = 24;
  panel->fillRect(STATUS_BAND_X, STATUS_BAND_Y, BOARD_LCD_WIDTH - 2 * STATUS_BAND_X, lineH * 3, RGB565_BLACK);
  panel->setTextSize(2);
  panel->setTextColor(RGB565_WHITE);
  const char *lines[] = {imuLine, rtcLine, btnLine};
  for (uint8_t i = 0; i < 3; i++) {
    panel->setCursor(STATUS_BAND_X, STATUS_BAND_Y + i * lineH);
    panel->print(lines[i]);
  }
}

// One full-screen fill per primary so a wrong color order or offset is
// obvious, then corner markers to check orientation and edge clipping.
void drawTestScreen() {
  const uint16_t fills[] = {RGB565_RED, RGB565_GREEN, RGB565_BLUE};
  for (uint16_t color : fills) {
    panel->fillScreen(color);
    delay(400);
  }
  panel->fillScreen(RGB565_BLACK);

  const int16_t w = panel->width();
  const int16_t h = panel->height();
  const int16_t m = 24;
  // 2 px white frame, not drawRect(): a 1 px frame along the edges never
  // showed on the board (CO5300 window alignment or the glass mask; the
  // probe below tells which), while this one shows on all four edges.
  const int16_t b = 2;
  panel->fillRect(0, 0, w, b, RGB565_WHITE);      // top
  panel->fillRect(0, h - b, w, b, RGB565_WHITE);  // bottom
  panel->fillRect(0, 0, b, h, RGB565_WHITE);      // left
  panel->fillRect(w - b, 0, b, h, RGB565_WHITE);  // right
  // Alignment probe, away from the edges: the upper line is 1 px tall (an
  // unaligned window), the lower one 2 px. Only the lower one visible = the
  // CO5300 drops unaligned windows; both visible = the edge pixel is masked.
  panel->drawFastHLine(96, 420, 288, RGB565_WHITE);
  panel->fillRect(96, 432, 288, 2, RGB565_WHITE);
  panel->fillRect(0, 0, m, m, RGB565_RED);          // top-left
  panel->fillRect(w - m, 0, m, m, RGB565_GREEN);    // top-right
  panel->fillRect(0, h - m, m, m, RGB565_BLUE);     // bottom-left
  panel->fillRect(w - m, h - m, m, m, RGB565_YELLOW);  // bottom-right

  panel->setTextColor(RGB565_WHITE);
  panel->setTextSize(3);
  panel->setCursor(m + 8, m + 8);
  panel->println("Bring-up");
  panel->setTextSize(2);
  panel->setCursor(m + 8, m + 44);
  panel->println("AMOLED 2.16 bring-up");
  panel->setCursor(m + 8, m + 68);
  panel->printf("PMIC: %s", pmuOk ? "ok" : "MISSING");
  panel->setCursor(m + 8, m + 92);
  panel->printf("Touch: %s", touchOk ? "ok, draw here" : "MISSING");
  panel->setCursor(m + 8, m + 116);
  panel->printf("SD: %s", sdMode ? sdMode : "no card");
}

void setup() {
  Serial.begin(115200);
  delay(1500);  // give USB CDC time to enumerate so early logs are not lost
  Serial.println("\n[bringup] Waveshare ESP32-S3-Touch-AMOLED-2.16");

  Wire.begin(BOARD_I2C_SDA, BOARD_I2C_SCL);
  initPmu();
  initDisplay();
  initTouch();
  initImu();
  initRtc();
  initSd();
  initButtons();
  if (displayOk) drawTestScreen();
  if (IMU_PROBE) {
    logPmu();
    logSensors();  // the Phase 1 lines, once, before the probe takes the screen
    delay(2500);
    startImuProbe();
  }
}

void loop() {
  static uint32_t lastStatus = 0;
  if (!probeOn && millis() - lastStatus >= STATUS_INTERVAL_MS) {
    lastStatus = millis();
    logPmu();
    logSensors();
  }
  if (probeOn) {
    probeSample();
    probeStatus();
    fifoTick();
  }
  pollTouch();
  pollButtons();
  pollSerialCommands();
  delay(probeOn ? 1 : 10);
}
