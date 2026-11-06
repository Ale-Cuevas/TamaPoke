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
- [ ] v1.28 screensaver: after 5 min untouched (or `SAVER` over serial) the screen goes black with the pet and the
  clock drifting slowly; the clock reads the right time. A touch (it does nothing else), USER, BOOT and a shake each
  bring back what was open (home, a card page, the clock screen). Face-down (`FACEDOWN`) gives the old dark screen
  instead. (Serial on the board 2026-11-05: `SAVER` → brightness 16 with the pet asleep, `BACK` → 25, `FACEDOWN`
  → 8, lifted → 25 ✓; the look: pending)
- [ ] v1.28 orbit: `ORBIT 2` then `ORBIT 6` (or wait 3 min each): the picture moves 4 px diagonally, with no stale
  strip or garbage along the edges and no tearing. Taps still land on the buttons at `ORBIT 6` (picture up-left)
  and `ORBIT 2` (down-right). `ORBIT` alone goes back to the clock. (Serial: origin 10,10 and 2,2 ✓)
- [ ] v1.28 a day on battery with the screensaver against the v1.21 day (~20 h, light use): ____
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

- [ ] v1.25 goals: `STEPS 6000` with the pet awake on a day without steps: the 6000 banner, JOY up, bond +2 even
  with the day's cap full (`STATS`: `goal=2`, `bonus=2`). `STEPS 4000` more: the larger 10000 banner with three
  flags and the candy, the Marathon medal banner, the Rare Candy popup (`candy=1`). The WALK ring shows `/6000`
  after 3000 and `/10000` after 6000, with a flag per goal; FINDS shows `n/5 +k`. Use the Rare Candy from the
  Backpack: the level goes up by 1 (and the evolve button appears if that level is the evolution level).
- [ ] v1.25 a real 10000-step day: the goals fire as they're reached, with the screen dark in between.

### Backgrounds (v1.26)
`BIOME <n>` shows biome n on the home screen without changing the pet (`BIOME` alone goes back); `PERF` prints
the background's draw time.
- [ ] `BIOME 0`: tall grass on the hill, tufts with flowers, front tufts over the pet's feet; it sways smoothly,
  and every 12 s a gust leans it to the right from left to right. At night (or with the pet asleep) it darkens.
- [ ] `BIOME 2`: the forest's conifer tips sway a little with the same gust.
- [ ] v1.27 `BIOME 6` (power plant): skyline, chimney smoke staying below the text, two pylons with cables that
  sway, a spark every ~7 s; at night the windows light up and a few blink.
- [ ] v1.27 `BIOME 7` (swamp): the pond with ripples, bubbles rising and popping, reeds swaying, the bare tree.
- [ ] v1.27 `BIOME 8` (ruins): violet sky, broken pillars, a fallen one, wisps floating (6 at night, 3 by day).
- [x] v1.27 `PERF` for the new biomes within ~3 ms of the beach (14.8 ms): power plant 16.7, swamp 15.5, ruins
  15.8 ms (2026-11-05, on the board).
- [ ] v1.26 `PERF` with `BIOME 0` within ~3 ms of the beach (`BIOME 1`). (2026-11-05, v1.26 on the board: beach 14.8 ms,
  meadow 16.7 ms, forest 15.0 ms ✓)

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

### Feedback (v1.30)
([NEXT_FEATURES.md, Task FX](NEXT_FEATURES.md#task-fx-feedback-for-what-the-pet-does).) `FX <effect>` over
serial shows any effect without touching the save. `STATS` prints the queue (`fx queue=… showing=… busy=…
sleep=…`). Banners wait while the pet sleeps, so wake it first. `PERF` prints the effects' draw time
(`fx=…us fxmax=…us`).
- [ ] `FX levelup`, pet awake: the jingle plays, the pet poses, sparkles rise, "LEVEL UP!" slides in with the
  level, and the header level flashes. For the last 1.5 s the pill shows "ATK +n DEF +n SPD +n".
  `FX levelup6`: "Lv.a > Lv.b".
- [ ] A real level-up with the screen off (PWR, then wait for the hour): the jingle plays at once and the screen
  stays off. On the next wake the animation plays with no second jingle.
- [ ] A level-up while the pet sleeps: no jingle. On waking it, the animation plays with its jingle.
- [ ] Power off for 2+ hours, then on: one "Lv.a > Lv.b" after the boot jingle.
- [ ] A level that earns a medal (Lv.10, 25, 50): the level-up first, then the medal banner with confetti, never
  both at once. The evolve button waits until both are gone.
- [ ] `FX medal`, `FX streak`, `FX goal`, `FX goal3`: each banner slides in from above. Medal and goal3 have
  confetti. Several at once come one after another in the order level, medal, streak, goal.
- [ ] Feeding: the FOOD bar slides up and "+25" (or "+35" for the favourite) floats from it. The same for the
  bath (HYG), items, and after the minigame and the bag.
- [ ] Bond: a small heart with "+n" left of the pet when bond rises (petting gives it at most every 15 min;
  `FX bond`).
- [ ] Vitamins from the Backpack: the card closes and "ATK +3" (or DEF/SPD) floats while the pet attacks
  (`FX train`).
- [ ] With the pet asleep, FOOD, PLAY and BATH shake with the deny sound. LIGHT still works. Every tapped button
  looks pressed for a moment.
- [ ] Hatching (the third egg tap, or `FX hatch` on a hatched pet): the egg shakes harder, the shell bursts in a
  white flash with the hatch jingle, and the pet appears posing inside a ring of light. The header says EGG
  until then.
- [ ] A care slip-up with the screen on: "!" bubble and a soft falling sound (`FX oops`).
- [ ] `PERF` while a level-up, the floats and the hatch play: `fx` stays under 3000 us.

### Levels (v1.35)
([GAMEPLAY_TUNING.md, #5 CURVE](GAMEPLAY_TUNING.md#5-curve-the-level-pace-follows-how-hard-the-pet-is-to-raise).)
`STATS` prints `grow=… lv=… into=…/… age=…`: the growth minutes, the level, the minutes done of the current
level and what it costs (20 up to Lv 10, then 45, 60, … up to 180 from Lv 89), and the age. On a board with a
real pet don't use `LVL`: it moves the level.
- [ ] Before flashing v1.35, `STATS` on the old firmware: note `nv=`. After flashing, `STATS`: the same level on
  `lv=`, `age=` unchanged, `into` about (old minutes into the hour) × cost / 60.
- [ ] A restart after that (RESET or unplug): `grow=` the same or a few minutes more, same level. It no longer
  comes from the age.
- [ ] Progress card (swipe up, then left three times): the bar's fill and "N min to Lv.X" match `into` and the
  cost from `STATS`.
- [ ] A Rare Candy from the Backpack: +1 level, the level-up animation, the bar about as full as before; the age
  on the profile card doesn't change.
- [ ] Wait out a level with the pet awake: `grow=` goes up one a minute, and the level-up plays when `into`
  reaches the cost.
- [ ] v1.36 step goals ([#6 STEPLV](GAMEPLAY_TUNING.md#6-steplv-walking-speeds-up-levelling)), pet awake:
  `FX goal` shows the 3000 banner with "+1 Lv", `FX goal3` the 10000 one with "+2 Lv" (look only, no save
  change).
- [ ] A real walk past 3000 steps with the pet awake: `STATS` shows `lv=` one higher, `into` about as full as
  before, `age=` not pushed. The level-up plays, then the goal banner with "+1 Lv". The same at 6000 (+1) and
  10000 (+2, no Rare Candy in the Backpack, the Marathon medal the first time).
- [ ] A goal crossed while the pet sleeps: the banner when it wakes has no "+N Lv", and the level didn't move.

