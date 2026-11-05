# Feature spec: steps, easter eggs and the board link

The 2.16" port runs on the board (v1.19), so the next work is new gameplay on
hardware the game ignores today: the **QMI8658 IMU** (only the bring-up
firmware reads it) and the **ESP32-S3 radio** (there is no WiFi, BLE or ESP-NOW
code anywhere). This page specifies three features. Each one is its own task,
on its own branch and release, and can be built and shipped without the others
except where a dependency is named.

| Task | What | Needs | Release |
|---|---|---|---|
| [1. Steps](#task-1-steps) | Steps counted from the IMU feed the pet: walk care and found items | — | 1.20 (built) |
| [2. Easter eggs](#task-2-easter-eggs) | Shake to wake, face-down lights out, tilt & slide | Task 1's IMU module | 1.20 (built, with Task 1) |
| [3. Link](#task-3-link-playdates-and-battles) | Two nearby boards meet for playdates and battles | 3c gifts need Task 1's Backpack | 3a in 1.22 (built); 3b–3e later |

Written 2026-11-04. **Tasks 1 and 2 are built in v1.20** (one branch,
`feature/step-counting`, one release): host-tested and rendered, not yet checked
on the board (`DEVICE_TESTS.md`, Steps and Motion). Where the build had to
decide something this spec left open, it says so under "Built in v1.20" at the
end of each task. Task 3 is not started. Every gameplay number is a
**starting value** held in one named constant, meant to be tuned after playing
with it; the constants tables list them. Requirements are numbered (FR-1.1, …)
so commits and tests can point at them.

## Contents

- [Common ground](#common-ground)
- [Measured on the board](#measured-on-the-board)
- [Task 1: Steps](#task-1-steps)
- [Task 2: Easter eggs](#task-2-easter-eggs)
- [Task 3: Link: playdates and battles](#task-3-link-playdates-and-battles)
- [Later, and prior art](#later-and-prior-art)

## Common ground

### Where the code goes

The repo already separates pure logic (host-tested) from hardware glue
(stubbed in the render harness). The new code follows the same split:

| Kind | New files | Same pattern as |
|---|---|---|
| Pure logic, header-only, tested on the PC | `steps.h`, `motion.h`, `battle.h`, `link_proto.h`, `friends.h` | `power.h`, `button.h` (`test/test_*.cpp`, listed in `test/Makefile`) |
| Game rules | `pet.cpp` / `pet.h` (new `Pet` methods) | `feedBerry()`, `playResult()` (`test/test_pet.cpp`) |
| Hardware glue | `imu.cpp` / `imu.h`, `link.cpp` / `link.h` | `rtcbat.cpp` (stubbed in `tools/render/stubs.cpp`) |
| UI | `TamaPoke.ino` | every existing screen |

- The sketch includes only `imu.h` and `link.h`. SensorLib, `WiFi.h` and
  `ESP32_NOW.h` stay inside the `.cpp` files, so the render harness only needs
  stubs, not fakes of those libraries.
- All IMU I2C traffic runs on the loop task. The audio task only touches I2C
  during init, so the shared bus (touch, PMU, IMU, RTC, codec) needs no lock.
- If the IMU does not answer at boot, `imuPresent()` is false and every IMU
  feature hides itself; the game runs as in v1.19.

### IMU sampling (Tasks 1 and 2)

Steps, shakes, face-down and tilt all come from one stream of accelerometer
samples, read from the QMI8658's FIFO. None of the chip's own engines are
used: its pedometer counted nothing on this board, and the light-sleep loop
can't poll fast enough to catch steps by itself (see
[Measured on the board](#measured-on-the-board)).

- **Configuration:** accelerometer ±8 g (±4 g clipped hard shakes and runs on
  the board), 62.5 Hz (59.5 Hz measured), gyroscope off, **low-pass filter
  off** (`LPF_OFF`; SensorLib turns on a 1.7 Hz filter by default).
- **FIFO:** stream mode, 128 samples, about 2 s of data. Read once a second
  while the screen is dark (light sleep already wakes the loop every 200 ms),
  and every 100 ms while it is lit, so tilt and shakes feel immediate. The
  FIFO also covers the ~1 s freeze of a flash write.
- **Own reader in `imu.cpp`:** SensorLib's `readFromFifo()` asks Wire for the
  whole FIFO in one transaction, more than arduino-esp32's 128-byte I2C buffer,
  and fails. The probe's chunked reader works: CTRL9 command 0x05 with the
  STATUSINT.bit7 handshake, `FIFO_DATA` read 120 bytes (20 samples) at a time,
  then `FIFO_CTRL` written back to leave read mode. The first sample after a
  FIFO reset is junk and is dropped.
- **I2C at 400 kHz.** At the default 100 kHz, one second of samples takes
  36 ms to read. All six chips on the bus support 400 kHz; check touch and
  sound after raising it.
- Samples get evenly spaced timestamps between reads and go, in order, to
  the step counter (`steps.h`) and the motion detectors (`motion.h`).

### Rules every task follows

- **No direct flash writes from motion or radio events.** A flash write
  freezes both cores for about 1 s (`pet.h`, the `pendingSave` comment). New
  effects set `pendingSave` and let the loop flush it while the screen is dim.
  `registerCare()` and `toggleLight()` call `save()` right away today; the
  tasks add deferred variants instead of calling those. `flushSave()` is
  skipped while the radio is on.
- **Daily caps roll over at midnight.** `bondToday` is only reset inside
  `registerCare()` and `hatch()`, and is not saved, so a cap can leak into the
  next day. Task 1 adds a `bondDay` next to it.
- **A new screen touches every UI hook:** the `render()` chain, `onTap()`,
  `onBack()` (USER key), both swipe handlers, the long-press guard,
  `staticScreen()` / `screenKey()`, `powerCtx()`, and `resetUi()` plus a scene
  in `tools/render/render.cpp`. A field missing from `screenKey()` means a
  static page that never redraws.
- **Every new string in all 8 languages** (ES, EN, FR, DE, IT, PT, JA, KO):
  Latin text with CP437 octal escapes, JA/KO only with glyphs the u8g2 fonts
  have, formatted strings added to `EXPECTED[]` in `test/test_i18n.cpp` and to
  `ARGS_S` in `test/test_tools.py` (which checks every `snprintf` buffer).
- **Each task ships with:** a CHANGELOG entry and version bump
  (`FW_VERSION` + `web/manifest.json`), a README manual section, a
  `DEVICE_TESTS.md` section and `PENDING_TESTS.md` rows, render scenes, host
  tests, and serial debug commands.

## Measured on the board

Facts the design depends on that only the board can answer, measured on
2026-11-04 with an IMU probe in the bring-up firmware (`bringup/main.cpp`:
BOOT records a labelled 15 s capture to the SD card, USER picks the label).
The captures that matter are kept in `test/data/imu/` (t_ms, ax, ay, az in
milli-g, ±4 g, 62.5 Hz, filter off), and `tools/imu_replay.py` runs them
through the prototype detectors.

| Question | Why it matters | Result |
|---|---|---|
| Which chip variant is it? | The QMI8658C has no pedometer | **QMI8658A**: WHO_AM_I 0x05, REVISION_ID 0x7C, which is the A datasheet's value (the C reads 0x79). The A has the pedometer, tap and motion engines. |
| Which way do the accelerometer's +X and +Y point, with USB-C at the bottom? | Tilt direction, face-down sign | **+X = screen right, +Y = screen top** (the buttons' edge), **+Z = out of the screen**. Upright, USB-C down: Y +1.01 g. Tilting the top to the right drives X negative (-0.77 g at about 50°), to the left positive. |
| Flat and face-down, what does Z read? | Face-down detection | Flat, face up: Z +1.02 g. Face-down: **Z -1.00 g**, X and Y within 0.02 g. |
| Does `begin()` reset the step counter? | Steps must be saved in NVS if so | **Yes.** `begin()` does a soft reset, and the A datasheet (11.6) says a power-on reset, a soft reset, re-enabling Pedo_EN or command 0x0F clear the count. The counter was also back at 0 after the board restarted. |
| Accelerometer low-pass filter | Everything below | SensorLib's `configAccelerometer()` turns on **LPF mode 0 by default: 2.66 % of the ODR, about 1.7 Hz at 62.5 Hz**. With it, no capture had energy above 3 Hz (a hard shake peaked at 0.6 Hz). Pass `LPF_OFF`. |
| Does the hardware pedometer count? | Task 1's step source | **No.** 0 steps over several 20-step walks (pocket and hand), runs and the user's own walks, with three configurations: ±4 g LPF off + any-motion; ±4 g LPF mode 3; and the SensorLib example exactly (±2 g, LPF mode 0, pedometer only). The registers read back right (`CTRL2` 0x17, `CTRL5` accel LPF off, `CTRL7` 0x01, `CTRL8` 0xDE, `STEP_CNT` 00 00 00) and every CTRL9 command completed. Cause unknown. **Task 1 counts steps in software.** |
| Can the FIFO deliver every sample? | Software steps and shakes while the board sleeps | **Yes, with our own reader.** SensorLib's `readFromFifo()` fails (`ESP_ERR_INVALID_ARG`: one 768-byte I2C read against Wire's 128-byte buffer) and stalls about 1 s per call. A chunked read got 357 samples in 6 s (**59.5 Hz** real rate), little-endian, flat Z +1.01 g; 36 ms per second of data at 100 kHz I2C. The first sample after a FIFO reset read -4/-4/+4 g (junk). |
| Walking, running and shaking, LPF off (`test/data/imu/`) | Thresholds for FR-1.1 and FR-2.1 | Walk in hand: 1.6 Hz, peaks 1.5 g. Walk in a pocket: 1.7 Hz, peaks 2.9-3.4 g. Run: 2.7 Hz, 4.3 g (clipped at ±4 g). Soft shake: 3.9-4.3 Hz, 4.0 g. Hard shake: 6.1 Hz, 6.9 g (clipped on every axis). Shake events (5 reversals beyond 1 g on one axis, each under 170 ms apart): run 0, soft shake 6, hard shake 7. |
| Software prototype on the captures (`tools/imu_replay.py`) | Feasibility of FR-1.1 / FR-2.1 | Steps: pocket walks 22 and 20, hand walk 17, run 31; still, face-down, upright and tilting 0; hard shake 0; soft shake 4. Shakes detected: hard 7, soft 6; walks, run, tilts 0. The true step counts of these walks weren't recorded, so accuracy is still open: the next capture round should count steps. |
| 100 steps counted by hand (v1.20 on the board; `tools/imu_capture.py` for the captures) | FR-1.1 accuracy | Pocket: the WALK page showed **114 for 100**. Its capture (`walk-pocket-100-1`): 102 during the walk (3 of them before the recording started), then **4 more from moving the board after stopping**. Taking it out of the pocket and looking at it, no walking (`handling-1`): **4 steps**, one burst of 4 weak, uneven peaks. Both bursts just passed the entry of 4. With an entry of 8 (v1.21) both count 0 and every walk and the run count the same. A hand walk (`walk-hand-2`, 73 s at 0.63 s a step, about 115 steps; the hand count wasn't given) counted 112: in the hand peaks are weaker (0.12-0.45 g against 0.2-1.0 g in a pocket) and about 7 steps were missed, so `STEP_PEAK_G` stays where it is. v1.21 on the board, 100 steps counted by hand in the hand (`walk-hand-100-1`): **105 (+5 %)**. |
| Does the STATUS1 any-motion bit latch? | Was a cheap light-sleep motion check | No longer needed: the FIFO carries every sample. One 1 Hz poll did catch an any-motion event after a brief movement. |
| INT1 (GPIO17) / INT2 (GPIO21) | Recorded for later; v1 does not use them | INT1 rests low and pulses on activity events (pedometer, motion). INT2 is not enabled; once it read about 36,000 edges a second with the pin low, once none: it floats (no pull). Don't use it without enabling it. |
| Free and minimum internal heap with the game running (`HEALTH`) | Budget for the WiFi/ESP-NOW stack | **290,900 B free, 285,848 B minimum** (`MALLOC_CAP_INTERNAL`, v1.19, a sprite loaded). |

## Task 1: Steps

### Goal

The pet comes along on walks. Steps counted by the board's IMU make the pet
happier and fitter, and now and then it finds something on the way.

User stories:
- As a player, I carry the board in a pocket or bag on a walk, and the pet
  gains joy and loses weight from it.
- As a player, I see how many steps we did today and how far I am from the
  daily goal.
- As a player, after a walk the pet shows me what it found, and I can use it
  later.

### Requirements

- **FR-1.1** Steps are counted whenever the board is on, including with the
  screen dimmed, off (PWR) or in light sleep. Accuracy: within ±10 % over 100
  steps with the board in a pocket or in hand.
- **FR-1.2** The game keeps **steps today** (reset at local midnight, the same
  day boundary as the streak) and a **lifetime total**. Both belong to the
  player: they survive a new egg and a restart.
- **FR-1.3** Walk care, applied only while the pet is hatched and awake:
  joy +1 per 100 steps (at most +30 a day), weight -1 per 250 steps, bond +1
  per 1000 steps (through `addBond()`, so the existing daily bond cap of 20
  applies). Walking costs no energy: an energy drop to 10 or below would count
  as a care mistake.
- **FR-1.4** Daily goal of 3000 steps. The first time it is reached each day:
  joy +10 (if hatched and awake), one bonus find, and the day counts as a day
  of care for the streak (deferred `registerCare()`). A banner and
  `SFX_MEDAL` play the next time the screen is lit.
- **FR-1.5** Found items: after a random 800-1200 *rewarded* steps (FR-1.3
  conditions), the pet finds an item, at most 5 finds a day (the goal's bonus
  find included). A popup shows it the next time the screen is lit; a tap
  dismisses it. Several finds queue up.
- **FR-1.6** Found items go into a **Backpack** (player-level, at most 9 of
  each item). It is not called "bag": the UI already uses that word for the
  punching bag.

  | Item | Odds | Effect when used |
  |---|---|---|
  | Golden Berry | 55 % | +50 food, +15 joy, a heart (pet awake) |
  | Vitamin: Protein / Iron / Carbos (1/3 each) | 35 % | +3 training in ATK / DEF / SPD, capped at 100 |
  | Shiny Charm | 10 % | Not used by hand: while held, the next egg's shiny odds are ×3 (never better than today's 1/8 floor). Consumed by that egg. At most 1 held; a second find becomes a Golden Berry. |

  Not a Rare Candy: the game already has a candy (the fattening snack), and
  +1 level means +60 `ageMinutes`, which would bring the farewell closer and
  inflate the age on the card.
- **FR-1.7** A new card page, **WALK** (the 5th): steps today with a ring
  toward the goal, the lifetime total, today's finds, and the Backpack. Tapping
  an item asks for confirmation and uses it. Held passives (Shiny Charm, and
  in Task 3 a friendship egg) are shown there too.
- **FR-1.8** Shaking the board must not farm steps or items: a detected shake
  pauses step counting, steps only count after a streak of 4, and finds are
  capped per day.
- **FR-1.9** With no IMU, the WALK page says so and nothing else changes.

### Design

**Where steps come from:** a software step counter fed by the FIFO samples
(see [IMU sampling](#imu-sampling-tasks-1-and-2)). The QMI8658A has a hardware
pedometer, but on this board it counted nothing in any configuration tried,
while the raw data showed every step clearly. `imu.cpp` keeps a switch to try
the hardware engine again if the cause is found.

**Counting** (`StepCounter` in `steps.h`, pure, ported from
`tools/imu_replay.py`):
- Per sample: the magnitude |a|, minus a slow moving average (gravity, 1 s),
  smoothed with a fast one (60 ms).
- A step is a local peak above `STEP_PEAK_G` at least `STEP_MIN_GAP_MS`
  after the previous one (about 3.5 steps/s at most).
- Counting starts after `STEP_ENTRY` steps in a row, each within
  `STEP_MAX_GAP_MS` of the last; then every step counts, the entry's too.
  Shorter bursts are dropped. v1.20 used 4, like the chip's own entry count;
  taking the board out of a pocket to look at it made bursts of 4 that counted,
  so v1.21 uses 8 (walks shorter than 8 steps don't count).
- A shake (Task 2's `ShakeDetector`, which Task 1 ships first) pauses
  counting for a second and breaks the streak, so shaking doesn't farm steps
  (FR-1.8).
- Counted steps accumulate in RAM and are handed to `Pet::addSteps()` every
  30 s, next to the existing RTC read. At most 30 s of steps are lost on a
  restart; the chip's FIFO is cleared by `begin()` anyway.

**Rules** (`Pet::addSteps(n, day)` in `pet.cpp`):
- Rolls `stepsToday` over when `day` changes (`today()`, the streak's day).
- Adds to `stepsToday` and `stepsTotal` always; applies FR-1.3 and advances
  the find counter only when hatched and awake.
- Keeps remainders (e.g. 99 steps toward the next joy point) so nothing is lost
  between reads.
- Sets `pendingSave`, never `save()`.
- Rolls finds with `random()`, so tests can force them with
  `mockForceRandom()`.

**Shiny Charm hook:** in `newEgg()`, right after the shiny roll's denominator
is computed and only when the starter isn't being chosen. Not in
`pickEggSpecies()`, which the `EGGS` serial command also uses to simulate
rolls.

**UI:**
- WALK page: goal ring (arc), steps today, total, finds today, Backpack grid
  with counts. The card's page count is hard-coded in five places (swipe
  clamp, page dispatch, page dots, tap zones, `screenKey()`), and the dots are
  re-centred for 5. `screenKey()` must include the step and Backpack fields or
  the page never refreshes.
- Found popup and goal banner reuse the medal celebration drawing
  (`drawCelebration()`), queued until the screen is lit and no other dialog is
  up.
- New icons (Golden Berry, three vitamins, Shiny Charm, goal flag) are drawn in
  `tools/sprites.py` and emitted into `species.h`.

**Persistence:** new NVS keys (short names, ≤ 15 chars): steps today, step
day, lifetime total, steps toward the next find, the next-find threshold, finds
today, joy-from-steps today, goal-done day, one count per Backpack item, the
charm flag, and `bondDay`. Old saves load with zero defaults (migration by key
presence, as today).

**Power:** the accelerometer runs all the time at 62.5 Hz with the gyro off,
and the ESP32 reads the FIFO once a second while dark: about 10 ms awake at
400 kHz I2C (36 ms at 100 kHz), on top of the 200 ms light-sleep wakes it
already does. The extra current must be measured; the device test compares an
8-hour battery drain with v1.19.

### Constants (starting values)

| Constant | Value | Meaning |
|---|---|---|
| `STEP_PEAK_G` | 0.12 | Smoothed dynamic \|a\| a step must exceed |
| `STEP_MIN_GAP_MS` / `STEP_MAX_GAP_MS` | 280 / 2000 | Fastest step, longest pause inside a streak |
| `STEP_ENTRY` | 8 | Steps in a row before counting starts (4 in v1.20) |
| `STEP_FLUSH_MS` | 30000 | How often counted steps go to `Pet::addSteps()` |
| `STEP_GOAL` | 3000 | Daily goal |
| `STEPS_PER_JOY` / `STEP_JOY_DAY_CAP` | 100 / 30 | Joy from walking |
| `STEPS_PER_WEIGHT` | 250 | Weight burned |
| `STEPS_PER_BOND` | 1000 | Bond from walking |
| `GOAL_JOY` | 10 | Goal bonus |
| `FIND_MIN_STEPS` / `FIND_MAX_STEPS` | 800 / 1200 | Distance between finds |
| `FINDS_PER_DAY` | 5 | Find cap |
| `ODDS_BERRY` / `ODDS_VITAMIN` / `ODDS_CHARM` | 55 / 35 / 10 | Item odds (%) |
| `ITEM_MAX` | 9 | Backpack cap per item |
| `GOLDEN_FOOD` / `GOLDEN_JOY` | 50 / 15 | Golden Berry |
| `VITAMIN_TRAIN` | 3 | Vitamin effect |
| `CHARM_SHINY_MULT` | 3 | Shiny Charm effect |

### Acceptance and tests

Host (`test/test_steps.cpp`, `test/test_pet.cpp`):
- `StepCounter` on the recorded captures in `test/data/imu/`: walks and the
  run count steps (within ±10 % of the true count, once captures with counted
  steps exist); still, face-down, upright and tilt captures count 0; a hard
  shake counts 0 and a soft one at most 5 (0 since v1.21); taking the board
  out of a pocket to look at it counts 0 (v1.21). Plus synthetic signals: a clean
  1.7 Hz walk, a burst shorter than `STEP_ENTRY`, a pause longer than
  `STEP_MAX_GAP_MS`.
- Joy, weight and bond per step, with remainders; joy cap; rewards skipped
  for an egg and a sleeping pet but steps still counted.
- Day rollover resets today's counters, not the total.
- Goal fires once a day, gives the bonus find, and credits the streak without
  an immediate save.
- Finds at the forced threshold, the daily cap, odds via forced random; the
  second Shiny Charm becomes a berry.
- Item effects and caps; the charm is consumed by `newEgg()` and not by the
  starter path.
- Backpack and step counters survive a simulated reboot and a new egg.

Render: WALK page (empty, mid-goal, goal done), found popup, item confirm.

Device (`DEVICE_TESTS.md`, new "Steps" section):
- 100 real steps, board in a pocket: counted within ±10 %. Same in hand.
- 10 s of hard shaking adds at most a few steps and no find.
- Screen off with PWR, on battery, a 200-step walk: counted when the screen
  comes back.
- Today's steps survive a restart (re-upload).
- Midnight rollover (set the clock to 23:59 with `RTCSET`).
- 8 h battery drain, screen off, against v1.19.

Serial: `STEPS n` (inject n steps through the same path), `IMU` (steps
pending, FIFO reads and samples per second, acceleration), `IMULOG <s>`
(stream samples as CSV, to record more fixtures).

### Open questions

- Is 3000 a good goal for a pocket pet? A walk-heavy player may want it
  configurable on the settings screen later.
- Why does the QMI8658A's own pedometer count nothing here? If it is ever
  made to work, it would save the once-a-second FIFO reads while dark.
- Should steps taken while the pet sleeps count toward finds? This spec says
  no (the pet is asleep), but players who walk at night lose them.

### Built in v1.20

Files: `steps.h` (`StepCounter`), `motion.h` (the detectors and `ImuPipeline`,
which feeds them in order), `imu.cpp` / `imu.h` (QMI8658 setup, the chunked FIFO
reader, timestamps), `Pet::addSteps()` and the Backpack in `pet.cpp`, the WALK
page and the popups in `TamaPoke.ino`. Decisions the spec left open:
- **Port fidelity:** the C++ counter gives the prototype's counts on every
  capture, ±1 (`test_steps.cpp`). Accuracy against hand-counted steps is still
  the device test's.
- **Timestamps:** samples are spread evenly between the previous read and this
  one; after a FIFO overflow or a gap that doesn't match the sample count, they
  are laid back from now at the measured 59.5 Hz, never before the last one.
- **WALK page** shows the pet's steps plus those the IMU counted but hasn't
  handed over yet (up to 30 s), so it moves while you look at it.
- **Goal:** the banner and the streak day are given whatever the pet is doing
  (the streak still needs a hatched pet, as `registerCare()` does); the +10 JOY
  and the bonus find only with the pet hatched and awake, like every walk reward.
- **Finds past the daily cap** don't carry over: the steps toward the next find
  start from 0 once the cap is reached.
- **Backpack full:** the find still counts; the popup says the Backpack is full
  and the item is lost.
- **Items:** using one is a tap, so it saves at once and counts as a day of care.
  A Golden Berry closes the card so you see the pet eat it. Vitamins can't be
  used once that training is at 100, so none are wasted.
- **Shiny Charm:** `Pet::shinyOdds(charm)` holds the odds formula, so the ×3 and
  the 1/8 floor are tested directly; `newEgg()` consumes the charm.
- **Saves:** every `addSteps()` with steps sets `pendingSave`; the loop flushes it
  once the screen dims, as before (NVS rewrites only the keys that changed).
- **Bond cap:** `bondDay` resets the daily cap in `addBond()` and
  `registerCare()`; `bondToday` is saved too, so a restart doesn't reopen it.
- **400 kHz I2C** is `BOARD_I2C_HZ` in the board header, set right after
  `Wire.begin()` (SensorLib's `begin()` doesn't change the clock).
- **Hardware pedometer switch:** `IMU_HW_PEDOMETER` in `imu.cpp` turns the chip's
  engine back on and prints its count next to ours in the `IMU` command.

## Task 2: Easter eggs

### Goal

The board reacts to being handled, without a menu: shake it and the screen and
the pet wake up, lay it face-down at night and the pet goes to bed, tilt it and
the pet slides. The tricks are not on any menu; the README lists them under a
spoiler.

### Requirements

- **FR-2.1 Shake.** A shake is a short, fast back-and-forth: on one axis, 5
  direction reversals beyond 1 g (gravity removed), each within 170 ms of the
  previous one, within 1.2 s. Frequency matters as much as strength: on the
  board a run reached 4.3 g at 2.7 Hz and never matched, while soft (4 Hz) and
  hard (6 Hz) shakes always did (see
  [Measured on the board](#measured-on-the-board)).

  | When | What happens |
  |---|---|
  | Screen dimmed or near-off | The screen wakes, as a touch would (`wakeUI()`). |
  | Screen turned off with PWR | Nothing: an explicit off is respected. |
  | Screen lit, pet asleep | The pet wakes up **grumpy**: joy -5, an anger mark over its head for 3 s, `SFX_DENY`. No care credit. A second grumpy wake within 60 s costs no extra joy. |
  | Screen lit, pet awake | The pet gets **dizzy**: swirling stars and a wobble for 3 s. Three shakes within 2 minutes cost joy -3, at most once every 10 minutes. |
  | Egg | The egg wobbles. |
  | Minigame, sack, link, evolution, ceremony | Ignored. |

  The sleeping pet only wakes from a shake while the screen is already lit.
  So the first shake lights a dark screen and a second one wakes the pet, and
  a jog with the board in a pocket can never wake it.
- **FR-2.2 Face-down lights out.** Screen-down and still for 3 s:
  - at night (20:00-06:00, the same hours the scenery uses) with the pet
    awake: the pet goes to sleep and the screen goes near-off;
  - in the daytime: the screen just goes near-off; the pet stays awake.
  - Picking the board up again lights the screen; a pet that went to sleep
    stays asleep.
- **FR-2.3 Tilt & slide.** On the home screen, pet awake and not busy:
  - tilt the board sideways more than 20° for half a second and the pet walks
    toward the low side;
  - more than 45° and it slides there in a surprised pose;
  - level it and the pet goes back to wandering.
  Left must be left: the direction follows the measured axes.
- **FR-2.4** None of this may cost battery while nothing moves, or wake the
  screen during an ordinary walk.

### Design

**Data first.** The orientation, walk, run and shake captures in
`test/data/imu/` set the thresholds below, and the detectors are tested
against them. More captures (face-down at night, pick-ups, the board in a bag)
are recorded with the bring-up probe as needed.

**Sampling:** the shared FIFO stream (see
[IMU sampling](#imu-sampling-tasks-1-and-2)). While the screen is dark the FIFO
is read once a second, so a shake lights the screen up to about a second after
it ends. No IMU interrupt pin is used: the QMI8658 sends pedometer, tap and
motion events to one shared activity interrupt, and the FIFO already carries
every sample.

Why the chip's own motion engines aren't used:
- SensorLib 0.3.3's `mgToBytes()` keeps only the fraction bits of the
  QMI8658's U3.5 motion thresholds, so a threshold of 1 g or more is written
  wrong.
- Wake-on-motion resets the chip and stops data output.
- The tap engine is tuned for about 500 Hz; at 62.5 Hz it is unreliable, so
  there is no double-tap gesture.

**Detectors** (`motion.h`, pure, fed one sample at a time with a timestamp,
ported from `tools/imu_replay.py`):
- `ShakeDetector`: per axis, removes gravity with a 400 ms moving average and
  counts sign reversals beyond `SHAKE_PEAK_G`; a reversal more than
  `SHAKE_MAX_GAP_MS` after the previous one restarts the count. Fires at
  `SHAKE_REVERSALS` within `SHAKE_WINDOW_MS`, then holds for a second.
- `FaceDownDetector`: Z at or below `FACEDOWN_Z_G`, X and Y near 0, and
  little change for `FACEDOWN_MS`. Reports both entering and leaving.
- `TiltEstimator`: low-passed sideways component of gravity in screen
  coordinates, as an angle (`asin(gx / |g|)`). That works whether the board is
  held upright or lies flat. On the 2.16 the right side going down reads X
  negative, so the pet moves toward screen-right when X < 0 (measured). Axis
  signs come from a per-board mapping in the board header, so the 1.75 (Task
  3e) can differ.

**Pet rules** (`pet.cpp`):
- `wakeGrumpy()`: wakes the pet with a deferred save, joy penalty with its
  cooldown, starts the emote timer.
- `shaken()`: dizzy timer, the 3-in-2-minutes joy penalty and its cooldown.
- `sleepDeferred()`: face-down's version of `toggleLight()`, without the
  immediate save.

**UI** (`TamaPoke.ino`): anger mark, dizzy stars and wobble, egg wobble; the
pet's wander walk (`WALKL` / `WALKR`) gets a target side and speed from the
tilt; the slide reuses the walk frame with a surprised pose and no stepping.

### Constants (starting values)

| Constant | Value | Meaning |
|---|---|---|
| `FIFO_READ_LIT_MS` / `FIFO_READ_DARK_MS` | 100 / 1000 | FIFO read interval |
| `SHAKE_REVERSALS` | 5 | Fast reversals on one axis |
| `SHAKE_WINDOW_MS` | 1200 | Window for them |
| `SHAKE_MAX_GAP_MS` | 170 | Reversal spacing (about 3 Hz) |
| `SHAKE_PEAK_G` | 1.0 | Swing beyond gravity |
| `SHAKE_HOLD_MS` | 1000 | No second shake, and no steps, for this long |
| `GRUMPY_JOY` / `GRUMPY_COOLDOWN_MS` | 5 / 60000 | Grumpy wake |
| `EMOTE_MS` | 3000 | Anger mark, dizzy stars |
| `DIZZY_SHAKES` / `DIZZY_WINDOW_MS` | 3 / 120000 | Dizzy penalty trigger |
| `DIZZY_JOY` / `DIZZY_COOLDOWN_MS` | 3 / 600000 | Dizzy penalty |
| `FACEDOWN_Z_G` / `FACEDOWN_XY_G` / `FACEDOWN_MS` | -0.8 / 0.3 / 3000 | Face-down |
| `TILT_WALK_DEG` / `TILT_SLIDE_DEG` / `TILT_HOLD_MS` | 20 / 45 / 500 | Tilt |

### Acceptance and tests

Host (`test/test_motion.cpp`, `test/test_pet.cpp`):
- Recorded traces in `test/data/imu/`: both shakes found (6 and 7 times in
  15 s on the prototype); no walk, run, tilt or still trace ever reports a
  shake.
- Synthetic traces: a 5 Hz, 2 g shake is detected; a 2 Hz walk and a 3 Hz,
  2.5 g run are not; a slow tilt is not a shake.
- Face-down enters after 3 s and not before; a brief flip does nothing;
  leaving is reported.
- Tilt angle sign and thresholds, with both axis mappings.
- `wakeGrumpy()`, `shaken()`, `sleepDeferred()` effects and cooldowns; none of
  them calls `save()` (NVS key count unchanged until `flushSave()`).

Render: anger mark, dizzy pet, egg wobble, sliding pet.

Device (`DEVICE_TESTS.md`, new "Motion" section):
- On battery, screen near-off (light sleep): a shake lights the screen; a
  second shake wakes a sleeping pet, grumpy.
- PWR-off screen: a shake does nothing.
- 2 minutes of walking with the board in a pocket, screen near-off: no wake.
- Face-down at 21:00 (set with `RTCSET`): the pet sleeps after 3 s; daytime:
  only the screen dims; picking it up lights it.
- Tilt left and right: the pet goes downhill both ways, not mirrored.
- 8 h battery drain with the motion checks on, against Task 1's figure.

Serial: `IMULOG` (CSV stream, any key stops it), `SHAKE` / `FACEDOWN` (inject
the events, to test the reactions without the IMU).

README: a `<details>` block, "Secrets (spoilers)", with the three tricks.

### Open questions

- Does reading the FIFO once a second while dark cost measurable battery
  (Task 1 measures it)? If it does, read every 2 s; the FIFO holds about 2.1 s.
- Should face-down in the daytime also put the pet down for a nap? This spec
  says no.

### Built in v1.20

- **Where a shake does something to the pet:** only on the home screen. With the
  stat card, gallery, keyboard or clock open it only keeps the screen awake (the
  pet isn't on screen), as in the minigame, the bag, an evolution or a ceremony.
- **Face-down** is ignored during the starter choice, the minigame, the bag, an
  evolution and a ceremony. Picking the board up lights the screen whenever the
  detector had seen it face-down, also when the screen had dimmed on its own.
- **Face-down detector:** besides Z ≤ −0.8 g and X, Y within 0.3 g, "still" means
  |a| within 0.15 g of 1 g for the whole 3 s; leaving needs Z above −0.5 g for
  120 ms, so a knock on the table doesn't count as a pick-up.
- **Tilt** uses a 250 ms low-pass, a 5° hysteresis on both thresholds, and the
  500 ms hold for starting to walk or slide (going level, or from sliding back to
  walking, is immediate). The pet only follows it when it is happy and nothing
  else is going on (not eating, sad, asleep, bathing or showing an emote).
  Walking downhill is 50 px/s, sliding 150 px/s.
- **Emotes** (`Pet::emote()`): the anger mark, the dizzy stars and wobble and the
  egg wobble last `EMOTE_MS`; the status line says "Grumpy!" / "So dizzy...".

## Task 3: Link: playdates and battles

### Goal

Two TamaPoke boards near each other can meet: the pets visit each other's
screen to play and swap gifts, or fight a short battle. Nothing needs a
router, a phone or pairing: both players open **Friends** and see each other.

The second board will arrive later and will probably be a Waveshare
**ESP32-S3-Touch-AMOLED-1.75**, the round board upstream TamaPoke was made
for. So the task includes building this fork for the 1.75 again (3e), and a
battle that runs on one board against a simulated opponent (3a), to develop and
play with before the second board exists.

The task is big, so it ships in five parts, each its own PR:

| Part | What | Testable with one board |
|---|---|---|
| [3a](#3a-battle-engine-and-practice-battle) | Battle engine, battle screen, practice battle against a simulated opponent (**built in v1.22**) | Yes |
| [3b](#3b-link-core) | Radio, discovery, invites, friend list, protocol; an on-device bot peer | Yes, with the bot |
| [3c](#3c-playdate) | Playdate: visit & play, gifts, friendship egg | Yes, with the bot |
| [3d](#3d-linked-battle) | 3a's battle over the link | Yes, with the bot |
| [3e](#3e-the-175-build) | `tamapoke175` build for the round 1.75 board | Needs the 1.75 |

### 3a. Battle engine and practice battle

**Requirements:**
- **FR-3.1** A battle is automatic: the pets take turns attacking until one
  faints. The only input is the special attack (FR-3.2).
- **FR-3.2** Tapping the battle screen fills a meter of 5 pips. When it is
  full, the pet's next attack is a **special**: 30 % more power, a flash and a
  screen shake. After a special the meter stays locked until the pet has made
  2 normal attacks, then it fills again. Taps during the intro don't count.
- **FR-3.3** The fight lasts **10-20 s**, not counting the sync and the ~3 s
  intro jingle.
- **FR-3.4** Stronger, better-trained pets win more often, and type matchups
  matter (Water beats Fire), but a weaker pet still lasts the minimum time.
- **FR-3.5** A practice battle against a simulated opponent runs on one board,
  from the Friends screen (or `BATTLE <dex>` over serial). Opponent: a random
  species of the same egg rarity as yours, at your level ±3, with average genes
  and your average training. Reward: joy +3, at most once an hour; it is not
  counted in your win/loss record.

**Engine** (`battle.h`, pure, deterministic for a given seed, xorshift32):
- Battle stats from the pet's own formula (`atkStat()` & co.: Gen 1 base ×
  genes + level + training), with the level clamped to 100. At level 999 the
  level term would decide everything.
  HP = `bHp` (unused today) + clamped level + 10.
- Damage per attack, Gen 1 style, all physical (the dex only has HP, ATK, DEF,
  SPD): `((2L/5 + 2) × P × A / D) / 50 + 2`, with L the clamped level, P = 40
  (52 for a special), then × type effectiveness, × a roll of 85-100 %, × 1.5 on
  a critical (1 in 16).
- Type effectiveness: the attacker's primary type against the defender's one
  or two types, from the 15-type Gen 1 chart (×0, ×0.5, ×1, ×2, multiplied for
  dual types).
- **Length guarantee:** each hit is clamped to 5-25 % of the defender's max HP
  (30 % for a special). A KO therefore takes at least 4 hits, so a battle is at
  least 7 attacks. At 1.3 s per attack that is about 10 s with the faint
  animation. A hard cap of 15 attacks (about 19.5 s) ends it; then the pet with
  the higher remaining HP % wins, and an exact tie is a draw.
- Turn order: higher SPD attacks first each round; a tie is a seeded coin
  flip.
- The engine takes "special armed" as an input per attack and returns one
  event per attack: attacker, damage, critical, special, effectiveness,
  both HPs, and whether it ended the battle.

**Type data** (`tools/dex_data.py`, `tools/gen_dex.py`):
- The dex has one type per species and 14 types: no Flying. Add `volador`
  (with an accent colour; a test requires one for every type) and a `TYPE2`
  table for the roughly 65-70 dual-type species (Charizard Fire/Flying, Pidgey
  Normal/Flying, Gengar Ghost/Poison, …).
- `gen_dex.py` emits `type1`, `type2` (enum, `type2` = none for single types)
  and `base` (the first form of the evolution line; Jolteon and Flareon have no
  predecessor in the table and need the same special case as Eevee's branch in
  `pet.cpp`). `base` is reused by the friendship egg (3c). CI's `generated` job
  checks `dex.h` matches the generator.

**Screen** (`TamaPoke.ino`):
- VS card with both names and the intro jingle, then the field: your pet
  bottom-left, the opponent top-right, HP bars with numbers, your special
  meter, a turn caption ("Super effective!", "Critical!", "Special!").
- Sprites: the PMD packs only have side views for walking. The pets walk in
  with `WALKR` / `WALKL`, then attack with the front-facing `PMD_ATTACK` plus a
  lunge offset and flinch with `PMD_HURT`. Every species has both. `ATTACK`
  frames can be large, so the battle draws at most ×2-3 and the attacker last.
- The opponent is a second `PmdMon` (the sketch already runs extra instances,
  `evoPmd` and `galleryPmd`). It is loaded before the intro starts, never
  during the fight, because SD loads block the loop. If its file is missing,
  its thumbnail is drawn, else a "?".
- Sound: a ~3 s intro jingle and short HIT, CRIT, SPECIAL, WIN, LOSE effects as
  new `Note` tables in `audio.cpp`, with a `static_assert` that `SFX[]` matches
  `SFX_COUNT` (a missing entry compiles and plays silence today). The audio
  task cannot stop or loop a sound, so there is no battle music loop in v1.
- `powerCtx().gameOpen`-style 85 ms frames during the battle.

**Built in v1.22** (`battle.h`, the type data in `tools/dex_data.py` /
`dex.h`, the screen at the end of `TamaPoke.ino`). Decisions the spec left open:
- **Where the practice battle lives.** The Friends screen comes with 3b, so
  until then PLAY opens a menu (ball game | battle) and the battle starts from
  there. That menu is 3b's generic menu (`menuKind` + `menuUntil`), and the
  swipe-over-the-food-menu quirk is fixed with it. When Friends lands, the
  battle entry becomes Friends, with "Practice" inside it.
- **No effect is 0 damage.** The 5 % floor applies to hits that land; an immune
  target takes nothing ("No effect..."). Two pets immune to each other (Rattata
  and Gastly) draw at the 15-attack cap. The length guarantee still holds: the
  pet that faints always takes at least 4 hits.
- **The opponent's special:** it "taps" `PRACTICE_AI_TAPS` (2) times before each
  of its attacks, so it charges a special about every third attack, with the
  same cooldown as yours.
- **The opponent's form:** a random base of your egg rarity, evolved along its
  line as far as its level allows (Eevee picks a branch), so a level-40 Fire
  line is a Charizard, not a Charmander. Never shiny.
- **"Once an hour"** is counted in RAM (`millis()`), like the grumpy cooldown: a
  restart allows another reward sooner. Win, lose or draw give the same +3.
- **Leaving:** USER quits at any point with no reward, and so does switching the
  screen off with PWR (nothing is drawn while it is off, and the battle only
  advances as it is drawn).
- **Ability to battle:** hatched, awake, no ceremony, not evolving. Practice
  costs no energy or food (those costs are 3d's, for linked battles).
- **Timing:** VS card 2 s, walk-in 1 s (`INTRO_MS` = 3 s), then attack k at
  intro end + k × 1.3 s, computed when it starts so a special charged a moment
  before still counts. The hit lands 350 ms in; the HP bar slides for 400 ms; a
  KO fades out over 900 ms before the result. 7 attacks take 9.1 s plus the
  fade, 15 take 19.5 s.
- **Measured on the engine** (all 151×151 at levels 5, 30, 100, specials on both
  sides): every battle lasts 7–15 attacks; at level 5 about 3 in 4 reach the
  cap, at level 100 about a third end at the 7-attack minimum. Among type-neutral
  pairs where one pet's stat total is at least 1.2× the other's, the stronger
  wins about 98 %; a type advantage between similar pets wins about 98 %.
  Specials are the player's way to beat those odds.
- **Sounds:** the intro jingle is 2.5 s; a hit, critical, special, "no effect"
  (the deny sound), win, lose or draw (the level-up sound).

### 3b. Link core

**Requirements:**
- **FR-3.6** No router, phone or pairing. Both players open **Friends** (PLAY
  button → "Ball game / Friends"); each sees the nearby pets within 2 s, at
  least at 10 m indoors.
- **FR-3.7** Tapping a friend offers Playdate or Battle. The other player gets
  an Accept / Decline dialog for 10 s.
- **FR-3.8** The radio is on only while Friends, a playdate or a battle is on
  screen. Friends closes itself after 2 minutes with nobody found. Below 10 %
  battery, Friends doesn't open.
- **FR-3.9** A lost connection ends the session within 5 s with a "connection
  lost" message and no rewards. Gifts already confirmed stay given.
- **FR-3.10** Boards with a different protocol version show up greyed out
  ("update the firmware") and can't be invited.
- **FR-3.11** The game remembers up to 8 friends: nickname, last species,
  last playdate day, and wins/losses against them.

**Transport: ESP-NOW.** It needs no access point or pairing, every board is an
equal peer, latency is a few ms, and the library ships with the core
(`ESP_NOW`, arduino-esp32 3.3.11). BLE was the alternative; it needs
central/peripheral roles and GATT for the same result.
- Frames stay at or below 250 bytes, the ESP-NOW v1 limit, so no feature
  depends on v2. Fixed channel 1 (both boards must agree, and there is no AP to
  pick one).
- Start: `WiFi.persistent(false)` (otherwise WiFi writes its settings to the
  20 KB NVS), `WiFi.mode(WIFI_STA)`, `WiFi.setChannel(1)`, wait for the STA
  interface to start, `WiFi.setSleep(false)`, `ESP_NOW.begin()`, add the
  broadcast peer.
- Peers must be added on `WIFI_IF_STA`: the library's default interface is the
  AP, and every send fails in STA mode.
- IDF 5.5 changed the send-callback signature; older snippets online don't
  compile.
- Callbacks run in the WiFi task on core 0. They only copy the frame into a
  FreeRTOS queue; `loop()` drains it. Unknown senders arrive through
  `onNewPeer()`, which is how beacons are received.
- Stop: `ESP_NOW.end()`, then `WiFi.mode(WIFI_OFF)` (fully deinits WiFi).
  Starting and stopping repeatedly is supported; the 50-cycle device test
  watches the heap.
- The first radio start ever writes about 2 KB of PHY calibration data to NVS:
  one freeze of about 1 s, once.
- Flash: about +0.5 MB, against 2.1 MB free in the 3 MB app slot. Internal RAM
  is the tighter budget: tens of KB while the radio is on (measured with
  `HEALTH`).
- While the radio is on: no light sleep, 85 ms frames (`PowerCtx.linkOpen`),
  no dimming, and `flushSave()` waits until the radio is off.

**Protocol** (`link_proto.h`, pure). Every frame starts with an 8-byte header:
magic `T` `P`, protocol version, message type, session id (16 bit, 0 outside a
session), sequence number, and the last sequence received (piggy-backed ack).

| Message | Sent | Payload | Acked |
|---|---|---|---|
| `HELLO` | Broadcast at 2 Hz while Friends is open | flags (busy, can battle), nickname, species, shiny, level, bond, board model | no |
| `INVITE` | To one peer | kind (playdate / battle), nonce | yes |
| `ACCEPT` / `DECLINE` | Reply | nonce / reason (busy, tired, asleep, declined) | yes |
| `PET_INFO` | Both, after accept | species, shiny, nickname, level, HP/ATK/DEF/SPD, types, line base, bond | yes |
| `START` | Host | start offset (+500 ms), session kind | yes |
| `TURN` | Host, battle | turn number, attacker, damage, flags, both HPs, end | yes |
| `SPECIAL_ARMED` | Guest, battle | — | yes |
| `GIFT` / `GIFT_ACK` | Playdate | gift id, item / gift id, accepted | yes |
| `PING` | Both, 1 Hz in a session | — | no |
| `BYE` | Either | reason | yes |

- Acked messages are resent every 300 ms, up to 10 times. 5 s without any
  frame from the peer ends the session (FR-3.9).
- **Everything received is treated as untrusted input:** nicknames are
  filtered to the rename keyboard's characters (A-Z, `.`, `-`) and terminated;
  a species outside 1-151 drops the frame before it can index `DEX_TBL`; stats
  and levels are clamped; unknown types and bad magic are ignored.

**Session** (`link.cpp` + the pure state machine):

```
 Friends open ── HELLO beacons ──> list of nearby pets
     │ tap a pet                       │ INVITE arrives
     v                                 v
  INVITING (10 s) ── ACCEPT ──> HANDSHAKE <── ACCEPT ── INVITED (10 s dialog)
                                  │ PET_INFO both ways, load friend sprite
                                  v
                       host sends START (+500 ms)
                                  v
                    PLAYDATE (3c)  or  BATTLE (3d)
                                  v
                    RESULT ── BYE ──> back to Friends
     any state: 5 s silence or BYE ──> "connection lost" ──> Friends
```

- **Host** = the board with the lower MAC address, so no negotiation is
  needed (the library's `ESP_NOW_Network` example also elects its master from
  the MAC). The host is authoritative for everything random.
- Shared seed = hash(host nonce, guest nonce). Things both boards must agree on
  without a message, like the playdate script and the friendship egg roll, come
  from it.
- The state machine talks to a `LinkTransport` interface with three backends:
  - **ESP-NOW** (`link.cpp`);
  - an **on-device bot**: a second session object running a simulated friend
    over an in-memory transport inside the same firmware. It is started with
    the `LINKBOT` serial command and also drives the practice battle. Until the
    second board arrives, it is how the whole flow is tested on one board,
    through the same code the radio uses;
  - a **host fake** in the tests, which can drop, duplicate and delay frames.

**Friend list** (`friends.h`, pure): 8 entries, least recently met replaced
first; each with MAC, nickname, last species, last playdate day, wins, losses.
Stored as one NVS blob with its own save, after the radio is off. It is kept
out of `Pet::save()`, which rewrites every key each time.

**UI:**
- The FOOD button's menu becomes a generic menu (`menuKind` + `menuUntil`) so
  PLAY can open "Ball game / Friends" the same way. This also fixes a quirk: a
  horizontal swipe can open the gallery on top of the food menu today.
- Friends screen: a round "radar" with up to 4 nearby pets as bubbles (sprite
  thumbnail, nickname, level), a searching animation, "Practice" when nobody
  is near.
- Invite dialog: the existing choice-dialog pattern, with a 10 s countdown.

### 3c. Playdate

**Requirements:**
- **FR-3.12 Visit & play.** About 20 s, scripted and mirrored on both
  screens: your pet on the left, the friend's on the right. They walk in,
  greet, play (hops, hearts, a ball passed between them), and wave goodbye.
  Tapping your own pet makes hearts, as at home.
- **FR-3.13 Rewards**, once per friend per day: joy +20, bond +3 (daily cap
  applies), food and energy -5, and the day counts as a day of care. Later
  playdates with the same friend that day are just for fun.
- **FR-3.14 Gifts.** During the playdate a Gift button opens your Backpack
  (Task 1); you pick one item and it moves to your friend's Backpack. At most 3
  gifts per playdate. An item is given exactly once: the sender removes it only
  when `GIFT_ACK` arrives, and the receiver ignores a gift id it has already
  taken. If the receiver's slot is full (9), the gift is refused and stays with
  the sender. An empty Backpack shows "Empty: go for a walk!".
- **FR-3.15 Friendship egg.** At the end of the first playdate with a friend
  each day, a roll from the shared seed decides whether the pets leave an egg.
  Chance: 5 % + (lower of the two bonds) / 10 %, at most 15 %. Both boards
  agree on the result. Each player gets an egg of the *other's* evolution line
  (its base form), never a legendary. You hold one at a time; it shows on the
  WALK page as "Egg from NICK", and the **next** new egg (after a farewell,
  runaway or release) hatches as that species. A player already holding one
  doesn't get another.

**Design notes:**
- The script's timing and choices come from the shared seed and `START`, so
  both screens play the same scene with only `PING`s in between.
- The friend's sprite: a `PmdMon friendPmd` loaded from **your own** SD card
  by species and shininess during the handshake, freed when the session ends.
  Every card holds all 151 species and their shinies (303 files), so only the
  number is sent. Missing file: thumbnail, then "?".
- Rewards are applied in RAM when the playdate ends and saved after the radio
  is off.
- Friendship egg hook: `newEgg()`, right after `pickEggSpecies()`, skipped
  while the starter is being chosen, and consumed there; not inside
  `pickEggSpecies()` (the `EGGS` simulation). `base` and legendary status come
  from 3a's dex fields.

### 3d. Linked battle

- **FR-3.16** Battle requirements on both sides: pet hatched, awake, not
  evolving, energy at least 20. Otherwise the invite is declined with the
  reason ("too tired", "asleep").
- **FR-3.17** Both screens show the same turns and the same winner.
- **FR-3.18** Rewards count for the first 3 battles of the day (any friends):
  win joy +15 and bond +2, loss joy +5; both pets lose energy -15 and food -5,
  never below 20, so a battle can't cause a care mistake. The win or loss is
  recorded for that friend. A battle ended by a lost connection has no
  result.

**Design:**
- The host runs 3a's engine. It schedules attack k at `START + intro +
  k × 1.3 s` and sends each result as a `TURN`. The guest renders `TURN`s in
  order, each at its scheduled time or on arrival, whichever is later.
- Each player's taps fill their own meter locally. When the guest's meter
  fills, it sends `SPECIAL_ARMED`; the host applies it to the guest's next
  attack that hasn't been computed yet. The host's own taps go straight into
  its engine.
- `START` carries a +500 ms offset so both intro jingles start together.

### 3e. The 1.75 build

So the second board can run the same firmware:
- A `tamapoke175` PlatformIO env and a `board_amoled_175.h`, rebuilt from
  v1.18's `pin_config.h` (`git show 1fff020:pin_config.h`). Most pins match the
  2.16. The differences:
  - CST9217 touch;
  - the panel needs the PMU's BLDO1 rail on before `begin()`;
  - the panel is natively 466×466, so the game draws straight to it, with no
    RoundCanvas offset or mask.
- Board selection at compile time (`-D BOARD_AMOLED_175`); CI builds both.
- To check on the 1.75's schematic before relying on them: the IMU's axis
  orientation (Task 2's mapping), whether it has a USER key (the back key), and
  its IMU interrupt pins.
- Optional: a second image in the web installer.
- The link only talks to builds of this fork; upstream's firmware has no
  link.

### Constants (starting values)

| Constant | Value | Meaning |
|---|---|---|
| `BATTLE_LEVEL_CAP` | 100 | Level used in battle math |
| `ATK_POWER` / `SPECIAL_POWER_PCT` | 40 / 130 | Attack power, special bonus |
| `SPECIAL_TAPS` / `SPECIAL_COOLDOWN` | 5 / 2 | Taps to charge, own attacks before recharging |
| `HIT_MIN_PCT` / `HIT_MAX_PCT` / `SPECIAL_MAX_PCT` | 5 / 25 / 30 | Per-hit clamp, % of max HP |
| `CRIT_ONE_IN` | 16 | Critical chance |
| `ATTACK_MS` | 1300 | Time per attack |
| `MAX_ATTACKS` | 15 | Hard cap |
| `INTRO_MS` | 3000 | VS card and jingle |
| `LINK_CHANNEL` | 1 | ESP-NOW channel |
| `HELLO_MS` / `PING_MS` | 500 / 1000 | Beacon and keep-alive |
| `RESEND_MS` / `RESEND_TRIES` / `SILENCE_MS` | 300 / 10 / 5000 | Reliability |
| `INVITE_MS` | 10000 | Invite dialog |
| `FRIENDS_IDLE_MS` | 120000 | Friends auto-close |
| `LINK_MIN_BATTERY` | 10 | % below which Friends doesn't open |
| `PLAYDATE_MS` | 20000 | Playdate length |
| `PLAYDATE_JOY` / `PLAYDATE_BOND` | 20 / 3 | Playdate rewards |
| `GIFTS_PER_PLAYDATE` | 3 | Gift cap |
| `FEGG_BASE_PCT` / `FEGG_MAX_PCT` | 5 / 15 | Friendship egg chance |
| `BATTLES_REWARDED` | 3 | Rewarded battles per day |
| `WIN_JOY` / `WIN_BOND` / `LOSS_JOY` | 15 / 2 / 5 | Battle rewards |
| `BATTLE_ENERGY` / `BATTLE_FOOD` / `BATTLE_FLOOR` | 15 / 5 / 20 | Battle costs |
| `PRACTICE_JOY` / `PRACTICE_EVERY_MS` | 3 / 3600000 | Practice reward |
| `FRIENDS_MAX` | 8 | Friend list size |

### Acceptance and tests

Host:
- `test/test_battle.cpp`: same seed and inputs give the same events; type spot
  checks (Water→Fire ×2, Electric→Ground ×0, Normal→Ghost ×0, dual types
  multiply); special power and cooldown, taps during the cooldown ignored; a
  Monte Carlo run over all 151×151 pairings at levels 5, 30 and 100 where every
  battle lasts 7-15 attacks and the stronger pet wins most of the time.
- `test/test_link.cpp`: every message packs into ≤ 250 bytes and round-trips;
  bad magic, a wrong version, a bad species and an unterminated nickname are
  rejected or cleaned; two sessions over the fake transport complete the
  handshake, a playdate and a battle with 20 % frame loss and duplicates; a gift
  moves exactly once; silence aborts both sides within 5 s with no rewards.
- `test/test_pet.cpp`: playdate rewards once per friend per day; battle
  rewards for the first 3 battles; floors hold; the friendship egg target is
  used by `newEgg()`, not by the starter or `EGGS`; legendary lines never give
  one.
- `friends.h`: least-recently-met eviction, blob round trip.
- `tools/test_tools.py`: the generated `type1` / `type2` / `base` fields.

Render: Friends (searching, 3 pets), invite dialog, VS card, mid-battle, a
special, the result, a playdate frame, a gift received, "connection lost".

Device with one board (`DEVICE_TESTS.md`, new "Link" section):
- Practice battle end to end; lengths between 10 and 20 s over 10 battles.
- `LINKBOT`: a playdate with a gift both ways, and a battle.
- Opening and closing Friends 50 times: `HEALTH` minimum heap stays flat.
- Current draw with Friends open, and back to normal after closing.
- The first radio start's NVS write happens once.

Device with two boards (the 2.16 and the 1.75):
- Discovery within 2 s at 1 m and at 10 m.
- Playdate on both; a gift arrives once and leaves the sender once.
- Battle: both screens show the same turns and winner; a special from each
  side lands.
- Unplug one board mid-battle: the other says "connection lost" within 5 s;
  no record, no rewards.

### Open questions

- Range and reliability at channel 1 next to busy WiFi: measure before
  choosing a channel-scan scheme.
- Should a playdate also register the friend's species in the Pokédex as
  "seen"? Not in this spec (the Pokédex only records pets you raised).
- How fair does a battle between a level-5 and a level-100 pet need to be?
  The clamps keep it 10-20 s long; the result is meant to favour the stronger
  pet.

## Later, and prior art

Later, once these are in:
- **Wild encounters**: the README roadmap's battle idea, reusing `battle.h`
  against a wild pet found on a walk.
- **Side-view attacks**: first make the sprite loader skip unknown action ids
  (`sdmon.cpp` rejects the whole file today), ship that, then repack the SD
  sprites with the extra rows (about 40 % bigger).
- **Battle music**: a loop needs the audio task to stop and repeat a sound.
- **A 9th medal** (walker, champion): the medal page's grid fits 8.

Prior art, for reference only: the community forks listed in the README.
ShadowEnemy's has a type-matchup battle system, and DylanPDao's has LAN
battles between two devices.
