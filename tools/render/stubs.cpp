// Host stand-ins for the two hardware modules the renderer leaves out:
// rtcbat.cpp (PCF85063 RTC + AXP2101 PMU) and audio.cpp (ES8311 + I2S).
// A battery-powered board at 80 % whose RTC holds the time the scene set.
#include "../../audio.h"
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
