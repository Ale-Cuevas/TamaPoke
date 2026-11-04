# Device tests: Waveshare ESP32-S3-Touch-AMOLED-2.16

Everything in this port was built, unit-tested and rendered on a PC, but none
of it has run on a real 2.16" board yet. These are the checks to run when it
arrives, in order. Tick a box only after it passes on the board; if one fails,
write what you saw next to it. [PENDING_TESTS.md](PENDING_TESTS.md) is the
one-line summary.

## Flashing

```bash
pio run -e bringup -t upload       # hardware test firmware first
pio device monitor -e bringup
pio run -e tamapoke -t upload      # then the game
pio device monitor -e tamapoke
```

If the upload can't find the port: hold **BOOT**, tap **RESET** (or plug USB in
while holding BOOT), release BOOT, upload again. Use a USB-C cable that
carries data. Check the LiPo's polarity against the silkscreen before plugging
it into the MX1.25 header.

The web installer (`web/`, served locally, see `web/README.md`) flashes the
same game image and loads the sprites over USB.

## Phase 1: bring-up firmware (`bringup/main.cpp`)

### Power (AXP2101)
- [ ] `[pmu] AXP2101 ok, chip id 0x..`
- [ ] Every 2 s on USB: `vbus in` ~5000 mV, `sys` plausible.
- [ ] With a LiPo: `batt yes`, 3300-4200 mV, a percentage; `charging yes` on USB below full.
- [ ] Unplug USB: keeps running, `vbus out`.

### Display (CO5300 480x480)
- [ ] `[lcd] CO5300 ok, 480x480`
- [ ] Full red, then green, then blue (red and blue swapped = wrong colour order).
- [ ] White 1 px border visible on all four edges (a missing edge or a band of noise = wrong offset).
- [ ] Corners: red top-left, green top-right, blue bottom-left, yellow bottom-right. Where is the USB-C port? ____
- [ ] Text reads left to right, not mirrored.

### Touch (CST9220)
- [ ] `[touch] CST92xx ok` (the model may print as CST9217 or CST9220).
- [ ] Touching draws a dot under the finger; the corners give about (0,0), (479,0), (0,479), (479,479). If dots land mirrored or rotated, fix only `BOARD_TP_SWAP_XY` / `BOARD_TP_MIRROR_X` / `BOARD_TP_MIRROR_Y` in `board_amoled_216.h`.
- [ ] A slow drag leaves a continuous trail.

### IMU (QMI8658, not used by the game)
- [ ] `[imu] QMI8658 ok, chip id 0x05`; flat, face up, about 0 0 +1 g.

### RTC (PCF85063)
- [ ] `[rtc] PCF85063 ok, running`
- [ ] `settime 2026-11-04 12:00:00` over serial sets it; the clock ticks.
- [ ] Survives RESET, and USB unplugged with the battery in. Survives a full power-off (no USB, no battery)? ____

### Buttons
- [ ] BOOT (GPIO0), USER (GPIO18), PWR (GPIO16 sense) all `up` at boot.
- [ ] Each short press logs `down` then `up`. If PWR reads inverted, fix `BOARD_BTN_PWR_PRESSED_LEVEL`. Don't hold PWR (the PMU may cut power).

### microSD (SDMMC 1-bit)
- [ ] With a FAT card holding the sprites: `[sd] card ok, ... MB, N files in /mons`.
- [ ] Without a card: `[sd] no card`, everything else still works.

## Phase 2: the game (`tamapoke` env)

### Display and round frame
- [ ] The first screen (starter choice, on a fresh save) shows the round UI centred, with black around it and no noise or stripes in the border.
- [ ] Compare a few screens with `docs/images/screens-216.png` (or `tools/render/render.sh`): same layout, nothing clipped.
- [ ] The circle's edge looks smooth, not stepped; the black corners are fully off.
- [ ] The black border on the left/top (6 px) and right/bottom (8 px) is not noticeable.
- [ ] No tearing or flashes while the pet walks, during the minigame or the evolution animation.
- [ ] At the default 40 MHz QSPI the picture is clean.
- [ ] Optional, for about 10 ms less per frame: set `BOARD_LCD_QSPI_HZ` to `80000000` in `board_amoled_216.h`, flash, and repeat the checks above (no noise, shifted lines or flashes, including the minigame). If clean, keep 80 MHz and note it here: ____

### Touch
- [ ] On the starter screen, each row picks its starter.
- [ ] The four buttons along the bottom arc (feed, play, light, bath) respond when tapped in their centre and edges.
- [ ] Tap the pet: hearts. Long-press it 3 s: the release dialog; YES and NO work.
- [ ] Swipe left/right: Pokedex pages; swipe up: the stat card (4 pages); swipe down: settings. Try one swipe that starts in the black border.
- [ ] Settings: hour/minute +/-, sound on/off, language pill and OK all land where drawn.
- [ ] Rename keyboard (tap the name on the card): every key, backspace and OK.
- [ ] Minigame: tapping the ball bounces it; tapping the top quits.
- [ ] A two-finger touch does nothing odd (no crash, no stuck touch).

### Status row
- [ ] Battery icon at the top centre; with a streak of 1+ days, the flame and the day count next to it, both fully inside the circle.

### Power, sleep, buttons
- [ ] PWR short press turns the screen off and on; holding it 4 s powers the board off and the RTC keeps the time.
- [ ] After 90 s untouched the screen dims, after 5 min it is almost off; a touch wakes it (the waking touch does nothing else).
- [ ] On battery with the screen off, it still wakes on a touch (light sleep).
- [ ] Battery percentage close to the bring-up `[pmu]` reading; the charging bolt shows within a few seconds of plugging USB in.

### Save, time, sound, SD
- [ ] Note the stats, press RESET: pet, stats, language and sound setting come back.
- [ ] Set the clock, RESET: still right (from the RTC). Power off for N minutes: the pet aged about N minutes.
- [ ] Sound: the boot jingle, taps and eating; no hiss when the pet sleeps (amplifier off).
- [ ] Sprites load from the card (`cargado /mons/p025.bin` in the log); `python3 tools/send_sd.py` and the web installer's "Load sprites" can write to the card over USB.
- [ ] Without a card, the nine built-in sprites still work.
