# Pending tests

First run on a real ESP32-S3-Touch-AMOLED-2.16 on 2026-11-04: bring-up and
game work. The open items are the ones not tried yet; the step-by-step checks
are in [DEVICE_TESTS.md](DEVICE_TESTS.md).
Write the result on each line: a tick, or what you saw.

| # | Check | Details | Result |
|---|---|---|---|
| 1 | PMU found, voltages plausible (bring-up) | [Power](DEVICE_TESTS.md#power-axp2101) | AXP2101 0x4A; VBUS 4.84 V, SYS 4.41 V, batt 4.20 V 90 % charging. USB unplug: pending |
| 2 | Panel lights, colours right, no offset (bring-up) | [Display](DEVICE_TESTS.md#display-co5300-480x480) | ✓ colours, corners, text, 2 px border (a 1 px edge frame doesn't show; alignment probe: pending) |
| 3 | Touch found, dots under the finger (bring-up) | [Touch](DEVICE_TESTS.md#touch-cst9220) | ✓ dots under the finger, corners (22,19)…(459,462), drag unbroken |
| 4 | RTC keeps time across reset (bring-up) | [RTC](DEVICE_TESTS.md#rtc-pcf85063) | settime ✓, ticks; survives RESET: check in the game |
| 5 | BOOT, USER, PWR register (bring-up) | [Buttons](DEVICE_TESTS.md#buttons) | ✓ all three up at boot, down/up on press |
| 6 | microSD mounts over SDMMC 1-bit, or only over SPI (bring-up) | [microSD](DEVICE_TESTS.md#microsd-sdmmc-1-bit-spi-fallback) | ✓ SDMMC 1-bit (30 GB card, 303 files) |
| 7 | Round UI centred, black border clean, 40 MHz QSPI clean | [Display and round frame](DEVICE_TESTS.md#display-and-round-frame) | ✓ at 40 MHz |
| 8 | Every button, swipe and dialog lands where drawn | [Touch](DEVICE_TESTS.md#touch) | ✓ buttons, pet, swipes, keyboard, minigame; settings buttons and two fingers not tried |
| 9 | Battery and streak status row inside the circle | [Status row](DEVICE_TESTS.md#status-row) | streak needs a 1+ day streak: pending |
| 10 | PWR key, dimming, light-sleep wake, battery reading | [Power, sleep, buttons](DEVICE_TESTS.md#power-sleep-buttons) | ✓ PWR, 90 s dim, runs on battery; light-sleep wake, % vs bring-up: pending |
| 11 | Save, RTC aging, sound, sprites from the card | [Save, time, sound, SD](DEVICE_TESTS.md#save-time-sound-sd) | ✓ save, clock across restart, sound, sprites; power-off aging, USB upload: pending |
| 12 | USER key = back on every screen; volume at -10 dB | [Power, sleep, buttons](DEVICE_TESTS.md#power-sleep-buttons), [Save, time, sound, SD](DEVICE_TESTS.md#save-time-sound-sd) | ✓ back; waking from dim and from light sleep, volume: pending |
| 13 | IMU probe: chip, axes, filter, FIFO, hardware pedometer (bring-up) | [IMU](DEVICE_TESTS.md#imu-qmi8658-as-the-game-uses-it-from-v120) | QMI8658A, axes and FIFO ✓; hardware pedometer counts 0 (steps go to software); walks with steps counted by hand: pending |
| 14 | v1.20 steps: FIFO rate, 400 kHz I2C (touch, sound), ±10 % over 100 steps (pocket, hand), shaking adds none, PWR-off walk, restart, midnight | [Steps](DEVICE_TESTS.md#steps) | ✓ FIFO 60/s, touch and sound at 400 kHz, restart, shaking, PWR-off walk. **Pocket: 114 for 100 (+14 %), outside ±10 %** in v1.20; v1.21 (entry 8): **hand 105 for 100 (+5 %)** ✓; pocket re-walk, bag, midnight: pending |
| 15 | v1.20 WALK page, goal banner, find popup, using items | [Steps](DEVICE_TESTS.md#steps) | ✓ page, banner, popups, a Golden Berry used; a vitamin and the awake goal bonus: pending |
| 16 | v1.20 motion: shake to wake (and not in a pocket), grumpy / dizzy, face-down, tilt both ways | [Motion](DEVICE_TESTS.md#motion) | ✓ tilt, face-down by day and at night, shake to wake on battery, a pocket walk never wakes the screen, grumpy and dizzy (serial); shake with the screen off (PWR), the egg: pending |
| 17 | v1.20 8 h battery drain, screen off, against v1.19 | [Steps](DEVICE_TESTS.md#steps) | screen-off drain: pending; a real day on v1.21, light use, lasted ~20 h on battery (23:00 → 19:00, 2026-11-05) |
| 18 | v1.22 pacing: candy weight lasts, 1000 steps = −1 weight, petting bond once per 15 min, bond ≤ 14 a day | [Pacing](DEVICE_TESTS.md#pacing-v122) | pending |
| 19 | v1.23 BOOT and the clock pill cycle 4 brightness levels; wake-only on a dim screen; kept after a restart | [Power, sleep, buttons](DEVICE_TESTS.md#power-sleep-buttons) | pending |
| 20 | v1.24 practice battle: PLAY menu, VS card and jingle, attacks and captions, special (taps, flash, cooldown), result and reward | [Battle](DEVICE_TESTS.md#battle) | ✓ "works like a charm" (user, 2026-11-05) |
| 21 | v1.24 battle lengths 10–20 s; big Attack sprites without stutter; heap flat over 20 battles | [Battle](DEVICE_TESTS.md#battle) | ✓ battles fine on the board (user, 2026-11-05); 20-battle heap check not run |
| 22 | v1.25 goals at 3000 / 6000 / 10000: banners, rewards, Rare Candy found and used (+1 level), Marathon medal, WALK ring and flags | [Steps](DEVICE_TESTS.md#steps) | pending |
| 23 | v1.26 swaying meadow (`BIOME 0`): smooth sway, gust every 12 s, front tufts over the feet, night tint; forest tips sway | [Backgrounds](DEVICE_TESTS.md#backgrounds-v126) | draw time ✓ (16.7 ms vs 14.8 beach); look: pending |
| 24 | v1.27 new biomes (`BIOME 6/7/8`): power plant, swamp, ruins by day and night; Pikachu, Ekans, Gastly and friends get them | [Backgrounds](DEVICE_TESTS.md#backgrounds-v126) | draw time ✓; swamp (by day), power plant and ruins (at night) ✓ on the board (user, 2026-11-05) |
| 25 | v1.28 burn-in: screensaver after 5 min (look, wake paths), orbit every 3 min (no edge garbage, taps land), a battery day | [Power, sleep, buttons](DEVICE_TESTS.md#power-sleep-buttons) | serial ✓ (brightness, wake, face-down, origins); screensaver look and wake ✓, orbit edges clean and taps land while moved ✓ (user, 2026-11-05); v1.29 bigger clock ✓ (user); battery: pending |
| 26 | v1.30 feedback effects: level-up (jingle in the dark, animation on wake), banners one at a time, "+N" floats, hatch, button shake | [Feedback](DEVICE_TESTS.md#feedback-v130) | serial ✓ (2026-11-05, pet asleep): every `FX` overlay draws in at most 1.5 ms (40 us idle, hatch 1.05 ms); banners wait while the pet sleeps (`queue=2 busy=1 sleep=1`). With the pet awake: look, sounds, banner draw time pending |
| 29 | v1.33 ball game: speed rises with every point (`BALL 1`, `BALL 80`: same arcs, half the time in the air at 80), still playable at 80 and fun on the way | [Games](DEVICE_TESTS.md#games-v133) | pending |
| 30 | v1.34 games menu: PLAY has ball, battle and training bag; each slot starts its game; the stat card has no training button | [Games menu](DEVICE_TESTS.md#games-menu-v134) | pending |

Questions only the board can answer:
- Where is the USB-C port relative to the screen's top? Bottom edge (buttons on top).
- Does the RTC keep time with no USB and no battery? ____
- Is 80 MHz QSPI also clean on this panel (about 10 ms faster per frame)? ____
