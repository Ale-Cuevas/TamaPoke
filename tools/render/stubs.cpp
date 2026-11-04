// Host stand-ins for the hardware modules the renderer leaves out:
// rtcbat.cpp (PCF85063 RTC + AXP2101 PMU), audio.cpp (ES8311 + I2S) and
// imu.cpp (QMI8658). A battery-powered board at 80 % whose RTC holds the time
// the scene set, with an IMU that is present and whose tilt the scene sets.
#include "../../audio.h"
#include "../../imu.h"
#include "../../rtcbat.h"

static uint32_t gEpoch = 0;
bool rtcBegin() { return true; }
uint32_t rtcEpoch() { return gEpoch; }
void rtcSetEpoch(uint32_t e) { gEpoch = e; }

bool batBegin() { return true; }
void batSetPollMs(uint32_t) {}
int batPercent() { return 80; }
bool batCharging() { return false; }
int batMillivolts() { return 3950; }
bool batShouldShutdown() { return false; }
void batPowerOff() {}
bool usbPresent() { return false; }
bool usbPresentNow() { return false; }
void pwrSetup() {}
bool pwrShortPressed() { return false; }

static bool gSound = true;
void audioBegin() {}
void sfxPlay(uint8_t) {}
void audioSetEnabled(bool on) { gSound = on; }
bool audioEnabled() { return gSound; }
void audioSetSleeping(bool) {}
bool audioBusy() { return false; }
void audioSetIdle(bool) {}

bool gStubImuPresent = true;  // render.cpp's scenes set these
int8_t gStubTiltSide = 0;
bool gStubTiltSlide = false;
bool imuBegin() { return gStubImuPresent; }
bool imuPresent() { return gStubImuPresent; }
void imuPoll(uint32_t, bool) {}
uint32_t imuTakeSteps() { return 0; }
uint32_t imuStepsPending() { return 0; }
uint8_t imuTakeShakes() { return 0; }
int8_t imuTakeFaceDown() { return 0; }
bool imuFaceDown() { return false; }
int8_t imuTiltSide() { return gStubTiltSide; }
bool imuTiltSlide() { return gStubTiltSlide; }
float imuTiltDeg() { return gStubTiltSide * (gStubTiltSlide ? 50.0f : 30.0f); }
void imuLog(uint32_t) {}
bool imuLogging() { return false; }
void imuReport() {}
