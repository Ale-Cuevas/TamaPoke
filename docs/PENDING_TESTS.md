# Pending tests

Written, compiled, unit-tested and rendered on a PC, but not yet run on a real
ESP32-S3-Touch-AMOLED-2.16. Work through it top to bottom when the board
arrives; the step-by-step checks are in [DEVICE_TESTS.md](DEVICE_TESTS.md).
Write the result on each line: a tick, or what you saw.

| # | Check | Details | Result |
|---|---|---|---|
| 1 | PMU found, voltages plausible (bring-up) | [Power](DEVICE_TESTS.md#power-axp2101) | |
| 2 | Panel lights, colours right, no offset (bring-up) | [Display](DEVICE_TESTS.md#display-co5300-480x480) | |
| 3 | Touch found, dots under the finger (bring-up) | [Touch](DEVICE_TESTS.md#touch-cst9220) | |
| 4 | RTC keeps time across reset (bring-up) | [RTC](DEVICE_TESTS.md#rtc-pcf85063) | |
| 5 | BOOT, USER, PWR register (bring-up) | [Buttons](DEVICE_TESTS.md#buttons) | |
| 6 | microSD mounts over SDMMC 1-bit (bring-up) | [microSD](DEVICE_TESTS.md#microsd-sdmmc-1-bit) | |
| 7 | Round UI centred, black border clean, 80 MHz QSPI clean | [Display and round frame](DEVICE_TESTS.md#display-and-round-frame) | |
| 8 | Every button, swipe and dialog lands where drawn | [Touch](DEVICE_TESTS.md#touch) | |
| 9 | Battery and streak status row inside the circle | [Status row](DEVICE_TESTS.md#status-row) | |
| 10 | PWR key, dimming, light-sleep wake, battery reading | [Power, sleep, buttons](DEVICE_TESTS.md#power-sleep-buttons) | |
| 11 | Save, RTC aging, sound, sprites from the card | [Save, time, sound, SD](DEVICE_TESTS.md#save-time-sound-sd) | |

Questions only the board can answer:
- Where is the USB-C port relative to the screen's top? ____
- Does the RTC keep time with no USB and no battery? ____
- Is 80 MHz QSPI clean on this panel, or does it need 40 MHz? ____
