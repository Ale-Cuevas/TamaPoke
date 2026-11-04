#pragma once
#include <cstdint>
#include "driver/gpio.h"
inline int esp_sleep_enable_timer_wakeup(uint64_t) { return 0; }
inline int esp_sleep_enable_gpio_wakeup() { return 0; }
inline int esp_light_sleep_start() { return 0; }
