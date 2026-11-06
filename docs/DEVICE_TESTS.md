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

### IMU (QMI8658, as the game uses it from v1.20)
- [x] `[imu] QMI8658 ok, who-am-i 0x05, revision 0x..`; flat, face up, about 0 0 +1 g. (0.02 0.03 1.05 g;
  revision 0x7C. Before the who-am-i fix the log said `chip id 0x7C`: SensorLib's `getChipID()` reads the revision)

IMU probe (after the Phase 1 lines, the bring-up shows a big IMU panel: BOOT records a 15 s capture of the
yellow label to `/imu/` on the SD card, USER picks the label, a 1 s USER hold switches configuration; serial
`imudump`, `imuregs`, `imufifo <s>`, `imulog <s>`). Results and captures: [FEATURES.md](FEATURES.md#measured-on-the-board).
- [x] Revision 0x7C is the QMI8658**A** (the C reads 0x79).
- [x] Axes: +X screen right, +Y screen top (buttons), +Z out of the screen. Face-down Z -1.00 g; upright Y +1.01 g;
  tilting the top right drives X negative.
- [x] Accel low-pass filter: SensorLib's default (mode 0) cuts at about 1.7 Hz at 62.5 Hz. Every capture lost
  everything above 3 Hz until it was turned off (`LPF_OFF`).
- [ ] Hardware pedometer counts steps. (**Fails:** 0 steps over 20-step walks in pocket and hand and a run, with
  LPF off, LPF mode 3, and the SensorLib example's exact settings; registers read back right. The game counts
  steps in software instead.)
- [x] FIFO delivers every sample. (SensorLib's `readFromFifo()` fails with `ESP_ERR_INVALID_ARG`, a 768-byte
  read against Wire's 128-byte buffer; the probe's chunked reader got 59.5 samples/s, 36 ms per second of data at
  100 kHz I2C.)
- [x] Soft reset clears the step counter (datasheet 11.6; also back to 0 after a restart).
- [ ] Walk captures with the steps counted by hand, to measure the software step counter's accuracy: ____

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
- [ ] v1.23 BOOT = brightness: each press steps 4 levels (the toast shows sun + dots for 1.5 s), the 4th press goes
  back to the dimmest; the screen gets visibly brighter / dimmer at each. `BRIGHT` over serial prints the level.
- [ ] v1.23 BOOT on a dim screen (90 s) only wakes it (no level change); on battery with the screen off (PWR) it
  wakes it too (light sleep).
- [ ] v1.23 the sun pill on the clock screen (swipe down) cycles the same level; after a restart (re-upload) the
  level is kept. At level 0 (dimmest) the 90 s dim doesn't get brighter.
- [ ] Battery percentage close to the bring-up `[pmu]` reading; the charging bolt shows within a few seconds of plugging USB in.

### Save, time, sound, SD
- [x] Note the stats, restart the board: pet, stats, language and sound setting come back.
- [ ] Set the clock, restart: still right (from the RTC). Power off for N minutes: the pet aged about N minutes. (restart ✓; power-off aging not tried)
- [x] Sound: the boot jingle, taps and eating; no hiss when the pet sleeps (amplifier off).
- [ ] Volume at -10 dB (DAC register 0x32 = 0xAB in `audio.cpp`): about half as loud as before, still clear. Too quiet or loud: change it in steps of 2 (1 dB).
- [ ] Sprites load from the card (`cargado /mons/p025.bin` in the log); `python3 tools/send_sd.py` and the web installer's "Load sprites" can write to the card over USB. (loading ✓: `cargado /mons/ps007.bin`, 151 thumbnails; writing over USB not tried)
- [ ] Without a card, the nine built-in sprites still work.

### Steps
(v1.20, [FEATURES.md Task 1](FEATURES.md#task-1-steps).) Serial: `IMU` shows FIFO reads, samples per second
and the steps not yet handed to the pet; `STATS` shows steps today/total and the Backpack.
- [x] Boot log: `[imu] QMI8658 ok: +-8 g, 62.5 Hz, sin LPF, FIFO stream 128`. `IMU` a minute later: about
  59.5 samples/s, `err=0`, `ovf=0`, the read under 15 ms (`read=...us`). (60.0/s, 2.3 ms a read with the screen
  lit; 60.7/s, a read a second, 10.4 ms with it near-off; `err=0 ovf=0` after 10 min of use, 2026-11-04)
- [x] **400 kHz I2C** (`BOARD_I2C_HZ`): touch still lands where drawn (buttons, swipes, the minigame), the sound
  still plays, the battery reading and the RTC are unchanged. If not, set it back to `100000` and note it: ____
  (touch ✓ buttons, swipes, minigame; battery 96-98 % charging and RTC ticking ✓; sound ✓.
  The first boot after flashing logged `i2cWrite(): ... ESP_ERR_INVALID_STATE` and `TouchDrvCST92xx
  getAttribute(): Firmware info read error`; touch still started (the 3-try loop) and the next boot was clean)
- [ ] 100 real steps, counted by hand, board in a trouser pocket: the WALK page (stat card, 5th page) counts
  them within ±10 %: **114 for 100 (+14 %), fails** . Same in hand: **105 for 100 (+5 %, v1.21)** . Same in a bag: ____
  (2026-11-04, v1.20. The extra steps were bursts from taking the board out of the pocket; v1.21 needs 8 steps
  in a row instead of 4. v1.21, 100 steps in a pocket: ____)
- [x] 10 s of hard shaking: at most a few steps and no find. (few steps ✓; "no find" not really tested: the
  `STEPS` tests had used up the day's 5 finds)
- [x] Screen off with PWR, on battery, a 200-step walk: the steps are there when the screen comes back.
- [x] Today's steps and the Backpack survive a restart (re-upload) once the screen has dimmed (the save is
  deferred until then). (`FACEDOWN` to dim, restart: 13600 steps and the Backpack back)
- [ ] Midnight: `RTCSET` to 23:59:30 of today, wait a minute: steps today back to 0, the total kept. (not
  tried: the jump would age the pet; `a_medianoche_hoy_vuelve_a_cero_y_el_total_no` covers it on the PC)
- [ ] `STEPS 3000` with the pet awake: the goal banner and jingle on the lit screen, JOY +10, streak +1 that
  day, a bonus find. `STEPS 900` again: a find popup; tap dismisses it. Use a Golden Berry and a vitamin from
  the WALK page (confirm dialog, YES and NO; USER closes it). (`STEPS 3000` with the pet asleep: goal day
  marked, banner, no JOY or find, as specced; awake, `STEPS 1200`: +12 JOY, +1 bond, a Golden Berry; caps of 5
  finds and +30 JOY a day ✓. WALK page, banner and find popups look right; a Golden Berry used. The awake goal
  bonus and a vitamin: not tried)
- [ ] No visible stall while walking with the screen lit (steps are only saved with the screen dimmed).
- [ ] 8 h battery drain, screen off, against v1.19: ____ % vs ____ %. (Not measured as specced, but a real
  day on v1.21 with light use ran on battery from 23:00 to 19:00, about 20 h, 2026-11-05)
- [ ] `IMULOG 15` streams CSV that `tools/imu_replay.py` reads; any key stops it. Record a few walks with the
  steps counted by hand into `test/data/imu/` for the accuracy test (`python3 tools/imu_capture.py <name>`
  does the recording). (`IMULOG 3`: 180 samples ✓; the script
  choked on the closing `[imulog] end` and now skips any line that isn't a sample. Walks: pending)

### Pacing (v1.22)
Weight and bond move slower. `STATS` prints them; `STEPS <n>` injects steps.
- [ ] A candy: the WGT bar (Battle page) goes up 12 and is still above 0 an hour later (−1 every 15 min
  awake): ____
- [ ] Awake, `STEPS 1000`: weight −1, bond +1 (not −4 as in v1.21): ____
- [ ] Tap the pet 10 times in a row: JOY goes up each time, bond only +1. Again 15 min later: +1: ____
- [ ] A day of normal care: bond grows at most 14 (10 from care + 4 for the first care of the day): ____

### Motion
(v1.20, [FEATURES.md Task 2](FEATURES.md#task-2-easter-eggs).) `SHAKE` and `FACEDOWN` / `FACEDOWN 0` inject
the events without moving the board.
- [x] On battery, screen near-off (5 min, light sleep): a shake lights the screen within about a second. A
  second shake wakes a sleeping pet, grumpy (anger mark, −5 JOY, deny sound).
- [ ] With the screen on and the pet awake: a shake makes it dizzy (stars, wobble); three in 2 minutes cost
  3 JOY once. The egg wobbles. (serial `SHAKE`: the sleeping pet woke grumpy, −5 JOY; awake, −3 JOY on the 3rd
  shake and nothing on a 4th. The egg: not tried)
- [ ] Screen off with PWR: a shake does nothing.
- [x] 2 minutes of walking with the board in a pocket, screen near-off: the screen never wakes.
- [x] Face-down at 21:00 (`RTCSET`): after 3 s the pet sleeps and the screen goes dark; picking it up lights
  it, the pet still asleep. In the daytime only the screen goes dark. (daytime ✓ at 19:45 on the pet's clock,
  night ✓ after 20:00)
- [x] Tilt left and right on the home screen: the pet walks downhill both ways (left is left), slides with a
  "!" past about 45°, and wanders again when level.
- [ ] 8 h battery drain with the motion checks on (the same firmware as the steps test: one figure covers
  both).

### Battle
(v1.24, [FEATURES.md 3a](FEATURES.md#3a-battle-engine-and-practice-battle).) `BATTLE` or `BATTLE <dex>` over
serial starts one; the log prints the matchup and the result.
- [ ] PLAY opens the menu (ball | crossed swords); the ball starts the minigame as before, the swords a battle.
  Asleep or as an egg, PLAY does nothing. A horizontal swipe with a menu open no longer opens the Pokédex.
- [ ] The VS card and jingle play together, the pets walk in from the sides, then the attacks start: Attack frames
  lunge, the target flinches (Hurt), the HP bar slides down, a sound per hit, captions under the field.
- [ ] Taps fill the 5 pips (none count during the VS card); full, they blink and the next attack of yours is a
  Special: white flash, shake, "Special!". The pips then show grey for 2 of your attacks.
- [ ] 10 battles at different levels: each lasts 10–20 s after the intro (time a few): ____
- [ ] The result screen shows the winner posing, "Joy +3" (or "once an hour" on a second battle within the hour);
  a tap or USER closes it, and it closes by itself after 7 s. USER or PWR mid-battle quits with no reward.
- [ ] Opponent without a sprite on the card (rename `/mons/pNNN.bin` for one species and `BATTLE NNN`): its
  thumbnail is drawn instead; with no thumbnail either, a "?".
- [ ] No stutter in the 85 ms frames when big Attack frames are drawn (Onix, Gyarados, Dragonite: `SPEC 95`, then
  `BATTLE 130`).
- [ ] `HEALTH` before and after 20 battles: the minimum heap stays flat (the opponent's sprite is freed each time).

