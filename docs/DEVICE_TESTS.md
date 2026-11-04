# Device tests: Waveshare ESP32-S3-Touch-AMOLED-2.16

First run on a real 2.16" board on 2026-11-04: the bring-up and the game both
work. The boxes still open below were not tried that day. These are the checks,
in order. Tick a box only after it passes on the board; if one fails,
write what you saw next to it. [PENDING_TESTS.md](PENDING_TESTS.md) is the
one-line summary.

## Flashing

```bash
pio run -e bringup -t upload       # hardware test firmware first
pio device monitor -e bringup
pio run -e tamapoke -t upload      # then the game
pio device monitor -e tamapoke
```

The board has no RESET button, only PWR, BOOT and USER. To restart it, run the
upload again (it only rewrites the program: the save and the clock stay), or,
in the game, hold PWR 4 s to power off and press it again to power on. With a
battery in, unplugging USB does not restart it.

If the upload can't find the port: unplug the battery, hold **BOOT** while
plugging USB in, release BOOT, upload again. Use a USB-C cable that
carries data. Check the LiPo's polarity against the silkscreen before plugging
it into the MX1.25 header.

The web installer (`web/`, served locally, see `web/README.md`) flashes the
same game image and loads the sprites over USB.

## Phase 1: bring-up firmware (`bringup/main.cpp`)

### Power (AXP2101)
- [x] `[pmu] AXP2101 ok, chip id 0x..` (0x4A)
- [x] Every 2 s on USB: `vbus in` ~5000 mV, `sys` plausible. (vbus 4830-4855 mV, sys ~4412 mV)
- [x] With a LiPo: `batt yes`, 3300-4200 mV, a percentage; `charging yes` on USB below full.
- [x] Unplug USB: keeps running, `vbus out`. (checked in the game)

### Display (CO5300 480x480)
- [x] `[lcd] CO5300 ok, 480x480`
- [x] Full red, then green, then blue (red and blue swapped = wrong colour order).
- [x] White 2 px border visible on all four edges (a missing edge or a band of noise = wrong offset).
  (The first version drew a 1 px `drawRect()`, which did not show at all: either the CO5300 drops
  unaligned windows or the edge pixel is under the glass. The next check tells which.)
- [ ] Alignment probe, two white lines near the bottom: the upper is 1 px, the lower 2 px. Does the
  upper one show? ____ (no = the CO5300 drops unaligned windows; yes = the edge pixel is masked)
- [x] Corners: red top-left, green top-right, blue bottom-left, yellow bottom-right. Where is the USB-C port? **bottom edge; the buttons are on the top edge**
- [x] Text reads left to right, not mirrored.

### Touch (CST9220)
- [x] `[touch] CST92xx ok` (the model may print as CST9217 or CST9220). (printed CST9220, 2 points)
- [x] Touching draws a dot under the finger; the corners give about (0,0), (479,0), (0,479), (479,479). If dots land mirrored or rotated, fix only `BOARD_TP_SWAP_XY` / `BOARD_TP_MIRROR_X` / `BOARD_TP_MIRROR_Y` in `board_amoled_216.h`.
  (Corner taps: (22,19), (454,25), (24,469), (459,462). Flags unchanged.)
- [x] A slow drag leaves a continuous trail.

### IMU (QMI8658, not used by the game)
- [x] `[imu] QMI8658 ok, who-am-i 0x05, revision 0x..`; flat, face up, about 0 0 +1 g. (0.02 0.03 1.05 g;
  revision 0x7C. Before the who-am-i fix the log said `chip id 0x7C`: SensorLib's `getChipID()` reads the revision)

### RTC (PCF85063)
- [x] `[rtc] PCF85063 ok, running` (unset, 2000-01-22, ticking)
- [x] `settime 2026-11-04 12:00:00` over serial sets it; the clock ticks.
- [x] Survives a restart, and USB unplugged with the battery in. Survives a full power-off (no USB, no battery)? ____

### Buttons
- [x] BOOT (GPIO0), USER (GPIO18), PWR (GPIO16 sense) all `up` at boot.
- [x] Each short press logs `down` then `up`. If PWR reads inverted, fix `BOARD_BTN_PWR_PRESSED_LEVEL`. Don't hold PWR (the PMU may cut power).

### microSD (SDMMC 1-bit, SPI fallback)
- [x] With a FAT card holding the sprites: `[sd] card ok over SDMMC 1-bit, ... MB, N files in /mons`.
  If instead it logs `trying SPI` and then `card ok over SPI`, the slot only works
  over SPI: the game's `sdBegin()` (`sdmon.cpp`) has to move to `SD` on HSPI
  before Phase 2's sprite checks can pass. Mode that mounted: **SDMMC 1-bit** (30436 MB card, 303 files in /mons, 2026-11-04)
- [ ] Without a card: both modes report `no card or not FAT`, everything else still works.

## Phase 2: the game (`tamapoke` env)

### Display and round frame
- [x] The first screen (starter choice, on a fresh save) shows the round UI centred, with black around it and no noise or stripes in the border.
- [ ] Compare a few screens with `docs/images/screens-216.png` (or `tools/render/render.sh`): same layout, nothing clipped.
- [x] The circle's edge looks smooth, not stepped; the black corners are fully off.
- [x] The black border on the left/top (6 px) and right/bottom (8 px) is not noticeable.
- [x] No tearing or flashes while the pet walks, during the minigame or the evolution animation. (evolution not seen yet)
- [x] At the default 40 MHz QSPI the picture is clean.
- [ ] Optional, for about 10 ms less per frame: set `BOARD_LCD_QSPI_HZ` to `80000000` in `board_amoled_216.h`, flash, and repeat the checks above (no noise, shifted lines or flashes, including the minigame). If clean, keep 80 MHz and note it here: ____

### Touch
- [x] On the starter screen, each row picks its starter.
- [x] The four buttons along the bottom arc (feed, play, light, bath) respond when tapped in their centre and edges.
- [x] Tap the pet: hearts. Long-press it 3 s: the release dialog; YES and NO work.
- [x] Swipe left/right: Pokedex pages; swipe up: the stat card (4 pages); swipe down: settings. Try one swipe that starts in the black border.
- [ ] Settings: hour/minute +/-, sound on/off, language pill and OK all land where drawn.
- [x] Rename keyboard (tap the name on the card): every key, backspace and OK.
- [x] Minigame: tapping the ball bounces it; tapping the top quits.
- [ ] A two-finger touch does nothing odd (no crash, no stuck touch).

### Status row
- [ ] Battery icon at the top centre; with a streak of 1+ days, the flame and the day count next to it, both fully inside the circle.

### Power, sleep, buttons
- [x] PWR short press turns the screen off and on; holding it 4 s powers the board off and the RTC keeps the time.
- [ ] After 90 s untouched the screen dims, after 5 min it is almost off; a touch wakes it (the waking touch does nothing else). (90 s dim and touch wake ✓; 5 min not tried)
- [ ] On battery with the screen off, it still wakes on a touch (light sleep).
- [x] USER = back. A press (or `BACK` over serial) closes: gallery detail → grid → home; the stat card from any page; the rename keyboard (the name is not saved, back on the card); the clock screen (time not applied); the minigame and the bag (no reward); the feed menu and the release, evolve and farewell dialogs (the evolve/farewell button stays). On the home screen and during the starter choice or a farewell it does nothing.
- [ ] USER on a dim screen (90 s) only wakes it. On battery with the screen off (PWR), a short USER press wakes it too (light sleep).
- [ ] Battery percentage close to the bring-up `[pmu]` reading; the charging bolt shows within a few seconds of plugging USB in.

### Save, time, sound, SD
- [x] Note the stats, restart the board: pet, stats, language and sound setting come back.
- [ ] Set the clock, restart: still right (from the RTC). Power off for N minutes: the pet aged about N minutes. (restart ✓; power-off aging not tried)
- [x] Sound: the boot jingle, taps and eating; no hiss when the pet sleeps (amplifier off).
- [ ] Volume at -10 dB (DAC register 0x32 = 0xAB in `audio.cpp`): about half as loud as before, still clear. Too quiet or loud: change it in steps of 2 (1 dB).
- [ ] Sprites load from the card (`cargado /mons/p025.bin` in the log); `python3 tools/send_sd.py` and the web installer's "Load sprites" can write to the card over USB. (loading ✓: `cargado /mons/ps007.bin`, 151 thumbnails; writing over USB not tried)
- [ ] Without a card, the nine built-in sprites still work.
