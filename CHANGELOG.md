# Changelog

All notable changes to the firmware. Versions match the number shown at the
bottom of the clock/settings screen (swipe down) and `web/manifest.json`.

Updating from the web installer (`web/`) **without** ticking "Erase device"
keeps your Pokémon.

## [Unreleased]

## [1.36] - 2026-11-06

Walking speeds up levelling ([GAMEPLAY_TUNING.md, #6 STEPLV](docs/GAMEPLAY_TUNING.md#6-steplv-walking-speeds-up-levelling)).

### Changed

- **The daily step goals give levels:** 3000 → +1, 6000 → +1, 10000 → +2, so up to 4 a day.
  The level bar keeps how full it was and the pet doesn't age. Several goals crossed at once
  give all their levels together.
- The goal banner shows "+1 Lv" / "+2 Lv" (or the total, "+4 Lv", when the goals came
  together); the level-up animation and jingle play as well, one after the other.
- **The 10000 goal no longer gives a Rare Candy**: the +2 levels replace it. Its joy, bond
  and the Marathon medal stay. Candies already in the Backpack still work (+1).
- As with every walking reward, a goal crossed while the pet sleeps gives no levels: it only
  marks the day and the streak.
- `FX goal` / `FX goal3` show the banners with "+1 Lv" / "+2 Lv".

## [1.35] - 2026-11-06

Level pace ([GAMEPLAY_TUNING.md, #5 CURVE](docs/GAMEPLAY_TUNING.md#5-curve-the-level-pace-follows-how-hard-the-pet-is-to-raise)):
until now every level took one hour of age, from Lv 1 to Lv 999.

### Changed

- **Each level costs more than the last**, as a stronger pet is harder to raise:
  - Lv 1–10: 20 min per level;
  - Lv 11–89: a log curve, `20 + 160 · ln(L − 10) / ln 80` minutes, from 20 up to 180;
  - Lv 90 on: 3 h per level.

  First evolutions come sooner (Lv 16 after ~8 h instead of 15 h), later ones take longer
  (Lv 36 after ~2 days instead of 1.5). Three days of growth are Lv 46; Lv 90 takes ~8 days.
- **Levels come from a growth counter, not from age.** It runs with the age once the pet has
  hatched (awake, asleep and while powered off), and the Rare Candy pushes it ahead. Age stays
  real time: the days on the card and the 3-day farewell don't move when a candy is eaten.
- **Rare Candy:** still +1 level, and now the level bar keeps how full it was (it used to jump
  to the start of the next level). It no longer ages the pet.
- **Progress card:** the bar and "N min to Lv.X" follow the cost of the current level
  (20–180 min). At Lv 999 the bar stays full with no "next level" line.
- **Serial:** `LVL n` now puts the pet at the start of level n (it used to give level n + 1,
  through the age). `STATS` prints a new line, `grow=… lv=… into=…/… age=…`.

### Updating

A pet from an earlier version keeps its level and how full its bar was: the save has no
growth yet, so it's worked out once from the age (old level = 1 + age / 60) and saved from
then on.

## [1.30] - 2026-11-05

Feedback effects ([NEXT_FEATURES.md, Task FX](docs/NEXT_FEATURES.md#task-fx-feedback-for-what-the-pet-does)):
until now a level-up only beeped (even with the screen off), the bars jumped, and a medal
earned with the screen off was never seen.

### Added

- **Level-up animation and sound.** Every level-up, whatever caused it (the hour, time
  powered off, a Rare Candy, `LVL`), plays a new rising jingle (`SFX_LEVELUP`) and then, on
  the home screen:
  - sparkles rise around the pet;
  - it does its pose;
  - a "LEVEL UP!" pill shows "Lv.13", or "Lv.12 > Lv.18" for several levels at once;
  - the level in the header flashes;
  - for the last 1.5 s the pill shows the ATK/DEF/SPD gained.

  The jingle plays **as soon as the level is reached**, even with the screen off, and
  doesn't turn the screen on. The animation waits until someone can see it and is then
  silent. With the pet asleep there's no jingle; animation and sound come when it wakes.
- **One celebration at a time.** Level-up, medal, streak milestone and step goal banners
  queue in that order. Each banner's time starts when it's shown, not when it's earned:
  - a medal earned with the screen off is no longer lost;
  - the Lv.10 medal now follows the level-up instead of drawing over it.

  Banners slide in from above. Medals and the 10000-step goal add confetti. The evolve,
  farewell and runaway buttons wait while a banner is up. The streak milestone now has a
  sound.
- **Needs bars slide** to their new value instead of jumping. Feeding, the bath, items, the
  minigame and the training bag float a green "+N" (or red "−N") up from each bar that
  changed by 3 or more.
- **Small effects:**
  - a little heart with "+N" when bond rises (at most every 10 s);
  - "ATK +3" (or DEF/SPD) with an attack when training rises: vitamins, the bag, the
    minigame;
  - a "!" bubble and a soft falling sound (`SFX_OOPS`) when a care slip-up is counted,
    only with the screen on;
  - a hatching effect: the egg shakes, the shell bursts with a white flash, and the pet
    appears posing;
  - tapped buttons look pressed;
  - FOOD, PLAY and BATH shake (with the deny sound) when they can't be used: asleep, or
    already bathing.
- `FX <effect>` over serial shows any effect without changing the save. `PERF` adds the
  effects' draw time, and `STATS` adds the queue and why it waits. 21 tests in
  `test/test_fx.cpp`, 6 more in `test/test_pet.cpp`, and 23 render scenes.

### Changed

- Vitamins close the Pokémon card like the Golden Berry and the Rare Candy, so you see the
  "ATK +3".
- `Pet` no longer times or plays its celebrations. Medals and milestones are flags the UI
  collects (`takeMedal()`, `takeMilestone()`). Level-up, medal and hatch sounds moved to the
  UI.
- Two medals earned before either is shown both get their banner. Before, the second
  overwrote the first.

## [1.29] - 2026-11-05

### Changed

- **The screensaver's clock is three times bigger** (text size 9, was 3): it
  was hard to read from a distance. The pet and the clock now drift over a
  smaller area (±50 px, was ±112 / ±96) so the wider clock never reaches the
  round edge; the test checks the clock's and the pet's corners, not a circle.

## [1.28] - 2026-11-05

AMOLED burn-in protection: the background and the UI chrome were static, and the
only protection was idle dimming.

### Added

- **Pixel orbit.** Every 3 minutes the whole picture moves 4 px to the next of 9
  positions around its usual place (`burnin.h`), so no fixed edge (header,
  bars, buttons, horizon) burns into the same pixels. Positions stay even, as
  the CO5300 needs; the strip the picture uncovers is painted black on the
  panel, static screens are sent again at the new place, and taps are mapped
  through the current position.
- **Screensaver.** After 5 minutes untouched the screen no longer shows the
  whole scene at brightness 8: it goes black with the pet (its idle or sleep
  animation) and the clock drifting slowly around the circle, at brightness 40
  (16 while the pet sleeps, never above your level). A touch, USER, BOOT or a
  shake brings back what was open, the first touch only waking. Face-down keeps
  the old dark screen. Frame rate (1 fps) and light sleep are unchanged.
- `SAVER` and `ORBIT [n]` over serial; `HEALTH` prints the orbit step and
  whether the screensaver is on. Tests in `test/test_burnin.cpp` (even, in-panel
  positions, small steps; the screensaver stays inside the circle and moves
  slowly) and `test_power.cpp`; render scenes for the screensaver and a moved
  picture.

## [1.27] - 2026-11-05

### Added

- **Three new backgrounds**, for the types that all shared the meadow:
  - **Power plant** (electric: Pikachu, Magnemite, Voltorb, Jolteon, Zapdos…):
    a skyline behind the hill with a chimney puffing smoke, two pylons with
    cables that sway in the wind and a spark every ~7 s; at night the windows
    light up and a few blink.
  - **Swamp** (poison: Ekans, Nidoran, Grimer, Koffing…): a murky pond with
    ripples and bubbles that rise and pop, reeds swaying, a bare tree.
  - **Ruins** (ghost and psychic: Gastly, Abra, Drowzee, Mewtwo…): broken stone
    pillars and a fallen one under a violet-tinted sky, with floating wisps
    (more at night).
  Normal, fighting and flying stay in the meadow. The minigame, the training bag
  and the battle field use the new biomes' ground colours. Each draws within
  2 ms of the beach on the board (`PERF`).

### Changed

- `dex.h` carries biomes 6–8 (`tools/gen_dex.py`'s `TYPE_BIOME`); 34 species
  moved. A pet keeps its species, so it just wakes up somewhere new.

## [1.26] - 2026-11-05

### Added

- **The meadow is tall grass that sways.** The meadow background (normal,
  electric, fighting, poison, psychic and ghost types, and the egg) used to have
  four static tufts. It now has a jagged edge of grass along the hill, tufts of
  fanned blades with flowers in the middle, and a front row of tufts drawn over
  the pet's feet. Everything sways with `wind.h`: a gentle back-and-forth,
  out of phase across the screen, plus a gust that sweeps from left to right
  every 12 s. At night the grass and flowers darken with the rest.
- The forest's conifer tips sway with the same wind.
- `BIOME <n>` over serial previews another biome on the home screen without
  touching the pet or the save (`BIOME` alone goes back); `PERF` prints the
  background's draw time. On the board the meadow takes 16.7 ms against the
  beach's 14.8 ms.
- `test/test_wind.cpp`: the sway stays in range, is the same for the same
  instant, the gust crosses left to right, and neighbouring columns move out of
  phase. Render scenes for the meadow mid-gust and at dusk.

## [1.25] - 2026-11-05

Saves from v1.24 load as they are: a goal already reached today counts as the
3000 one, and the Backpack gets an empty Rare Candy slot.

### Added

- **Step goals at 3000, 6000 and 10000.** Each fires once a day, in order: 3000
  as before (+10 JOY, a bonus find, a streak day); 6000 gives +10 JOY, +2 bond
  and a bonus find; 10000 gives +15 JOY, +3 bond and a **Rare Candy**, and the
  first time the **Marathon** medal (the 9th). The goals' bond comes on top of
  the daily bond cap and their gifts on top of the 5 random finds a day. If one
  batch of steps crosses several goals, all of them pay out and the banner
  shows the highest; the 10000 banner is larger, with three flags and the candy.
- **Rare Candy**, a new Backpack item (at most 9), only from the 10000 goal:
  using it gives +1 level, which can make the pet ready to evolve.
- The WALK page's ring fills toward the next goal (`/6000`, `/10000`), with a
  flag per goal reached; FINDS shows the random finds out of 5 and `+n` for the
  goals' gifts. The Backpack has 6 slots, slightly narrower.
- `STEPS` and `STATS` over serial print the goal reached today, the gift finds
  and the Rare Candy count.

### Changed

- The found-item popups now keep the newest when more than 8 pile up with the
  screen dark (they used to keep the oldest 6 and drop the rest), so the Rare
  Candy's popup isn't lost. The items always reach the Backpack either way.

## [1.24] - 2026-11-05

Practice battles, part 3a of the link task in
[docs/FEATURES.md](docs/FEATURES.md#3a-battle-engine-and-practice-battle): the
battle engine, its screen, and a battle against a simulated opponent on one
board. Saves from v1.23 load as they are.

### Added

- **Practice battle**, from the new PLAY menu (ball game | battle) or `BATTLE
  [<dex>]` over serial. An automatic battle against a line of your egg rarity at
  your level ±3, with average genes and your average training: a VS card with
  an intro jingle, the pets walk in, then one attack every 1.3 s with the PMD
  Attack and Hurt animations, HP bars, and captions (super effective, not very
  effective, no effect, critical, special). Your pet stands bottom-left, the
  opponent top-right. The result screen shows the winner posing.
- **The special attack:** tapping the battle screen fills a 5-pip meter; when
  full, your next attack has 30 % more power, a flash and a shake, then the
  meter rests for 2 attacks. Taps during the intro don't count.
- **Reward:** +3 JOY at most once an hour, win or lose; USER quits with none.
- `battle.h`, the pure engine (shared with the coming linked battles): card
  stats with the level capped at 100, Gen 1 damage, every hit 5–25 % of max HP
  (30 % for a special), 1 in 16 critical, faster first, 15 attacks at most,
  deterministic for a seed (xorshift32).
- **Gen 1 types:** the Pokédex gets Flying and the second type of the 62
  dual-type species (`TYPE2` in `tools/dex_data.py`), plus each line's base form;
  `dex.h` carries `type1`, `type2` and `base`. Battles use the full 15-type
  chart, with its Gen 1 quirks.
- Sounds: the intro jingle and hit, critical, special, win and lose effects. A
  crossed-swords icon for the menu. The new texts in all 8 languages.
- Tests: `test/test_battle.cpp` (type spot checks, damage clamps, the special
  and its cooldown, turn order, determinism, and all 151×151 pairings at levels
  5, 30 and 100: every battle lasts 7–15 attacks and the clearly stronger pet
  wins), the type and base fields in `test_dex.cpp` and `test_tools.py`, the
  practice reward in `test_pet.cpp`. Render scenes for the menu, the VS card,
  the field, a special, the result and a draw.

### Changed

- The FOOD menu is now one of two pop-up menus (`menuKind`), and neither lets a
  horizontal swipe open the Pokédex on top of it any more.
- `audio.cpp`'s effect table is sized by its entries and checked against
  `SFX_COUNT`: a sound added to the enum and not to the table no longer
  compiles (it used to play silence).
## [1.23] - 2026-11-05

### Added

- **Brightness levels.** The BOOT key, unused by the game until now, steps
  through 4 levels (50 / 100 / 150 / 220): a sun and 4 dots show the level for
  a moment, and the 4th press goes back to the dimmest. The clock screen
  (swipe down) has the same control as a sun pill between sound and language.
  The level is saved (`bri`, written 3 s after the last change) and kept across
  restarts; BOOT also wakes the board from light sleep, and on a dim or dark
  screen the first press only wakes it. `BRIGHT [n]` over serial reads or sets
  it; the `HEALTH` log prints it.

### Changed

- The idle dimming (60, then 8) and the sleeping pet's 25 never go above the
  chosen level. The fixed 145 on battery / 180 on USB is gone: the default level
  is 150 either way.

## [1.22] - 2026-11-05

After a day on the board, weight never left 0 and bond filled far too fast.
Saves from v1.21 load as they are.

### Changed

- **Weight burns off slowly.** Weight is still extra fat (0 = fit), but the pet
  burns 1 every 15 minutes awake and every 30 asleep, instead of 1 every 3
  minutes: a candy (+12) used to vanish in 36 minutes and now lasts about 3
  hours. Steps burn 1 per 1000 (was 250), the minigame `score / 2` (was
  `score × 2`) and the training bag `hits / 10` (was `hits / 3`).
- **Bond takes about 8 days to max.** The daily cap through care is 10 (was
  20), plus 4 for the first care of the day: at most 14 a day, so 100 on day 8
  of full care instead of day 5.
- **Petting gives bond once every 15 minutes.** JOY and the heart still come on
  every tap; ten taps in a row used to fill half the day's cap.

### Fixed

- A +2 bond one point below the daily cap no longer goes past it.
- The README's bond cap ("+8/day", from before v1.4) and firmware badge (v1.20).

## [1.21] - 2026-11-04

### Changed

- **Steps start counting after 8 in a row, not 4.** On the board, 100 steps
  counted by hand in a pocket came out as 114: taking the board out of the
  pocket to look at it, or moving it after stopping, made bursts of 4 that
  passed the old entry. With 8 they count nothing, while every recorded walk
  and run counts the same. Walks shorter than 8 steps no longer count.

### Added

- `tools/imu_capture.py` records a walk from the running game over USB
  (`IMULOG`, without restarting the board) into `test/data/imu/`, with the
  game's own count. Four new captures there: 100-step walks in a pocket and
  in the hand, another hand walk, and taking the board out of a pocket to look
  at it. On the board, v1.21 counts the 100 steps in the hand as 105.

### Fixed

- `tools/imu_replay.py` read `IMULOG` output only after deleting its closing
  `[imulog] end` line by hand; it now skips any line that isn't a sample.

## [1.20] - 2026-11-04

Steps and the motion tricks, from the board's QMI8658 IMU (Tasks 1 and 2 of
[docs/FEATURES.md](docs/FEATURES.md), shipped together). Saves from v1.19 load
as they are: the new counters and the Backpack start at zero.

### Added

- **Steps.** The accelerometer runs all the time (±8 g, 62.5 Hz, gyro and the
  chip's low-pass filter off) and the game reads its FIFO with its own chunked
  reader, once a second while the screen is dark and every 100 ms while it is
  lit. Steps are counted in software (`steps.h`; the chip's pedometer counts
  nothing on this board): 4 in a row before counting starts, and a shake pauses
  it. Steps today (reset at midnight) and a lifetime total are the player's.
- **Walk care** while the pet is hatched and awake: +1 JOY per 100 steps (max
  +30 a day), −1 weight per 250, +1 bond per 1000. A **daily goal** of 3000
  steps gives +10 JOY, a bonus find and a day of streak, with a banner.
- **Finds and the Backpack.** Every 800–1200 rewarded steps (max 5 a day) the
  pet finds a Golden Berry, a vitamin (Protein / Iron / Carbos: +3 ATK / DEF /
  SPD training) or a Shiny Charm (×3 shiny odds for the next egg). A popup shows
  each find; the Backpack holds 9 of each.
- **WALK page**, the stat card's 5th: steps today with a ring toward the goal,
  the total, today's finds and the Backpack; tap an item to use it. Without an
  IMU it says so.
- **Motion tricks** (`motion.h`): shake to light the screen, and with it lit to
  wake a sleeping pet (grumpy) or make an awake one dizzy; lay the board
  face-down for 3 s to put the screen (and at night the pet) to sleep; tilt it
  and the pet walks or slides downhill. Spoilers in the README.
- New icons (Golden Berry, the three vitamins, Shiny Charm, goal flag) in
  `tools/sprites.py`, and the new texts in all 8 languages.
- Serial: `STEPS <n>`, `IMU`, `IMULOG <s>`, `SHAKE`, `FACEDOWN [0]`; `STATS`
  prints the steps and the Backpack.
- Tests: `test/test_steps.cpp` and `test/test_motion.cpp` run the detectors on
  the recorded captures in `test/data/imu/` and on synthetic signals; the walk
  rules, the Backpack, the charm and the reactions are in `test/test_pet.cpp`.
  Render scenes for the WALK page, the popups and the reactions.

### Changed

- **I2C at 400 kHz** (`BOARD_I2C_HZ`), up from Wire's 100 kHz default, so a
  second of IMU samples reads in about 10 ms instead of 36.
- The daily bond cap now resets at midnight (`bondDay`); before, yesterday's
  cap held until the first care of the day.

### Added (since 1.19, before the steps)

- **USER button = back.** It closes the screen or dialog on top: gallery detail →
  grid → home, stat card, rename keyboard (name not saved), clock screen (time not
  applied), minigame and bag (quit without reward), and the feed / release /
  evolve / farewell dialogs. On a dim or dark screen the first press only wakes it,
  and it also wakes the board from light sleep. `BACK` over serial does the same.

- **Feature spec** ([docs/FEATURES.md](docs/FEATURES.md)) for the next three
  tasks: steps counted from the IMU, motion easter eggs, and playdates and
  battles between two boards. The first two are in this release.

- **Bring-up: IMU probe.** After the Phase 1 checks the bring-up firmware runs
  the QMI8658 the way the steps and motion features will, shows it in big type,
  records labelled 15 s captures to the SD card (BOOT; USER picks the label) and
  dumps registers or tests the FIFO over serial. What it found is in
  `docs/FEATURES.md` ("Measured on the board"); the captures are in
  `test/data/imu/` and `tools/imu_replay.py` replays them through the prototype
  step and shake detectors.

### Changed (since 1.19, before the steps)

- **Quieter sound:** the DAC volume drops from 0 dB to -10 dB (about half as
  loud).

## [1.19] - 2026-11-04

The port to the **Waveshare ESP32-S3-Touch-AMOLED-2.16** (square 480x480
AMOLED). This version targets the 2.16" board only; v1.18 and earlier are for
the round 1.75". Details and sources: [docs/AMOLED-2.16.md](docs/AMOLED-2.16.md).

### Changed

- **Round screen on a square panel.** The game still draws its 466x466 round
  UI; the display layer places it on the 480x480 panel and frames it with an
  anti-aliased round mask, so it looks like the original with black around it
  (black AMOLED pixels are off). `-D TP_ROUND_FRAME=0` shows the square canvas.
- Board support for the 2.16": `board_amoled_216.h` replaces `pin_config.h`;
  panel offsets 0/0 and MADCTL 0xA0; CST9220 touch with its own axis mapping,
  converted to the game's coordinates; no PMU rail is touched (the 1.75" needed
  BLDO1 for the panel); 4.2 V charge target as in Waveshare's demo.
- QSPI clock back to 40 MHz, the speed Waveshare's demo runs this panel at
  (`BOARD_LCD_QSPI_HZ`). The 1.75" ran at 80 MHz, about 10 ms faster per
  frame; it can return once the 2.16" panel is shown to be clean at 80 MHz.
- Built with PlatformIO (`platformio.ini`), with the platform, core and every
  library pinned. arduino-cli still builds it, with the same versions in CI.

### Added

- `bringup/`: a hardware test firmware for a new board (panel, touch, PMU,
  IMU, RTC, buttons, microSD), flashed with `pio run -e bringup -t upload`.
- `tools/render/`: renders every screen on the PC as the PNG the panel would
  show, from the real drawing code.
- `test/test_roundmask.cpp`: the round frame's geometry and edge.
- [docs/DEVICE_TESTS.md](docs/DEVICE_TESTS.md): what to check on the board.

### Fixed

- The daily streak badge was drawn in a corner outside the round screen and
  never showed. It now sits next to the battery icon.
- A two-finger touch could write past the touch coordinates: SensorLib returns
  every finger the chip reports, not just the one asked for.

## [1.18] - 2026-10-27

Three contributions from **ViezeVingertjes** (PRs #25, #26 and #27), merged together.

### Added

- **Battery percentage from a LiPo discharge curve** (`batcurve.h`). On battery
  the figure comes from a typical single-cell curve over a smoothed voltage, and
  only goes down while discharging, so the charge-inflated voltage right after
  unplugging does not raise it. On USB or while charging the PMU's own gauge is
  still used.
- **Safe power-off.** When the smoothed voltage stays under 3.30 V for 30 s (and
  not in the first 30 s after boot) the pet is saved and the PMU powers off. The
  PMU is also set to cut at 3.1 V as a backstop. Assumes a 4.2 V LiPo/Li-ion
  cell, like the one the board is designed for.
- **Power policy** (`power.h`): the frame rate follows activity (10 fps in use,
  down to 1 fps when the screen is almost off, 2 fps while the pet sleeps
  untouched), static screens (keyboard, clock, card pages 2-4, starter choice)
  are painted once, the speaker amplifier is off while the pet sleeps or the
  screen is dimmed, and under 20 % battery the brightness is capped and frames
  are slower. About 68 % fewer frames reach the panel over 10 idle minutes.
- **Light sleep** with the screen off or almost off, on battery only. A touch
  wakes it. Never with USB connected, so the serial port for the SD card stays
  available.

### Changed

- Sprites are drawn as runs of equal pixels instead of one rectangle per pixel
  (same output, far fewer draw calls), and `loop()` yields 1 ms on passes that
  do not draw.
- The pet walks by real time, so a lower frame rate does not slow it down.
- Battery is read over I2C every 10 s instead of 2 s when the screen is off.

## [1.17] - 2026-09-18

### Added

- **Korean, the eighth language.** Full UI (85 strings), the three medal name
  lengths and the 151 gen-1 species names, on the same `STRINGS[LANG][StrId]`
  structure as every other language. The text measurement centralised in 1.15
  carries almost all of it; the four byte-based spots a UTF-8 row turned up are
  listed under Fixed.
- `u8g2_font_unifont_t_korean2` for Hangul, selected per language in
  `applyLangFont()` rather than through a single `CJK_FONT`. The Korean added
  uses 296 distinct syllables, all of them in KS X 1001, the 2350-syllable set
  that matches the size of `korean2`; `korean1` holds 478. Coverage was checked
  as membership in KS X 1001 rather than by walking the font table, and the
  ASCII in the strings is assumed present. Not yet checked on the board:
  whether `CJK_SIZE_DIV` suits `korean2` as it does the Japanese subset.

### Fixed

- **The release dialog could cut a Korean name mid-character.**
  `renderRelease()` built into `char q[28]`. `"%s 놓아줄까요?"` is 17 bytes
  before the name, so names of four syllables or more (34 of the 151) did not
  fit, and `snprintf` truncated inside a 3-byte sequence, leaving a tail the
  font cannot decode. Now 48; the longest case needs 33 bytes including the
  terminator.
- **Three places sized and positioned text with `strlen()`**, which counts
  bytes: the stat-card header, the species name under a nickname, and the
  gallery detail header. On a UTF-8 row the centring offset and the auto-shrink
  threshold worked off a byte count rather than a width. All three now go
  through `textW()` / `centerX()`. With the classic font `textW()` returns
  `strlen()*6*size`, the same expression as before, so the six Latin languages
  land on the same coordinate at the same size, checked at every length from 1
  to 25.
- **Three Japanese texts were cut short**, all already in 1.15 and 1.16.
  Their buffers were sized when every string was one byte per character, and
  `snprintf` truncates without a trace. The minigame record read `きろく 2`
  with a best of 219, since `char rec[12]` left room for one digit. The streak
  milestone banner never fit at all: `"%u にちれんぞく！"` passes 20 bytes with a
  single digit, and the cut landed mid-character. The streak line on the stat
  card lost digits from 100 days on. The release dialog fixed above was the
  fourth.
- **Every formatted text is now checked against its buffer, in all eight
  languages.** A new test in `test/test_tools.py` finds each
  `snprintf(buf, sizeof(buf), T(...))` in the sketch, reads the size of `buf`,
  and computes the worst case per language from the argument types (5 digits
  for `%u`/`%d`, 10 for `%lu`, and a declared maximum for each `%s`). It is what
  caught the three above, and it would have caught all four. To keep it passing
  by type rather than by typical value, four more buffers get headroom they had
  not yet needed: the medal count, the next-level line, the evolution countdown
  and the profile line.
- **`tools/test_i18n_formats.py` reads only the first `len(LANGS)` language
  blocks**, and `LANGS` listed six, so later rows were skipped without a
  warning. It now lists all eight, and the docstring notes that it needs to
  stay complete.
- **`test/test_i18n.cpp`'s `LANG_NAME[LANG_COUNT]` had six entries**, fewer
  than `LANG_COUNT`, so every entry past the sixth was null. Four of the tests
  build a label from `LANG_NAME[lang]` on every iteration, not only when a
  check fails, so each run passed a null pointer to `%s`. Now lists all eight.

## [1.16] - 2026-09-09

### Fixed

- **Japanese was silently dropping every `！` and `？`.** 21 exclamation marks
  and 4 question marks — a quarter of the Japanese strings, including the mood
  line on the main screen (`ごきげん！` rendered as `ごきげん`). The cause is that
  `u8g2_font_unifont_t_japanese1` carries kana and kanji but no CJK punctuation
  at all — no `！？。、「」` — and `Arduino_GFX` draws nothing for a glyph it can't
  find, without even advancing the cursor, so the loss is invisible in the
  source and on screen. Switched to `japanese3`, the only U8g2 Japanese font
  that has them. Same unifont family: the metrics header is byte-identical and
  all 226 codepoints the firmware uses have byte-identical bitmaps, so nothing
  moves. Costs 102 KB of flash (705 KB → 810 KB, 22% → 25% of the app
  partition); RAM is unchanged.
- **Nidoran♀ and Nidoran♂ both showed as ニドラン in Japanese**, leaving #29
  and #32 indistinguishable in the Pokédex. No U8g2 unifont subset carries
  ♀ (U+2640) or ♂ (U+2642) — not `japanese*`, `korean*` or `chinese*` — and
  `Arduino_GFX` draws nothing for a glyph it can't find, without even
  advancing the cursor. `gen_names.py` already substituted F and M on the
  Latin path; it now does the same on the UTF-8 one, so the names read
  ニドランF and ニドランM. Convention chosen by @usakomint, who preferred it
  over ニドラン(女)/(男) as closer to a species name and shorter on a small
  round screen.

Both confirmed on hardware by @usakomint, who also — without knowing it —
supplied the evidence for the first one: the missing `！` was visible in a
photo he posted of the v1.15 main screen.

## [1.15] - 2026-09-08

### Added

- **Japanese.** UI, medals and all 151 species names in katakana (フシギダネ,
  ピカチュウ). Selectable from the settings screen like any other language.

### Changed

- Text measurement and cursor positioning are now centralized (`textW()`,
  `centerX()`, `setCur()`, `setSize()`, `printT()`), replacing ~180 sites that
  assumed one byte per character and a fixed 6 px width. Latin languages render
  identically — the equivalence is arithmetic, not approximate. This is what
  makes CJK possible without a per-language branch at every draw site, and
  Chinese and Korean now only need their strings.
- Bar positions on the stat card and the care row are derived from the width of
  the widest translated label instead of fixed coordinates, so they stay aligned
  in any language. With Latin labels the numbers come out identical to before.
- New dependency: **U8g2**, for its CJK font data only.

Japanese font handling, readability and layout were all tuned against real
hardware by @usakomint, who found and diagnosed every issue in the process.

## [1.11] - 2026-09-07

### Fixed

- **The whole loop froze for exactly 1 second when touching the screen.**
  `touch.getPoint()` hung on a sleeping CST9217 for the ESP32 I2C driver's
  default 1000 ms timeout, which `Wire.setTimeOut(50)` doesn't bound because
  SensorLib doesn't route through it. Most visible in the ball minigame, where
  taps come fast and two or three stalls ran together — reported as "freezes for
  2–3 seconds on ball tap". An address-only Wire transaction before the read
  fixes it. Measured on hardware over equivalent play (~470 touch reads): 5
  stalls in 60 s before, 0 after. Reported by @gingsiro.

## [1.10] - 2026-09-07

### Fixed

- TPK2 sprites with zero-length frame durations spun `pmdFrameAt()` forever,
  hanging the render loop. These files arrive over USB, so they aren't trusted
  input: durations are clamped at load time and the loop is bounded regardless.
- `thumbs.bin` was mapped into PSRAM without bounding its size or checking that
  the offsets it contains point inside what was actually read. A truncated file
  (plausible — the transfer takes minutes) could read past the allocation.
- The serial `PUT` handler ignored `f.write()`'s return value, so a full SD card
  still reported `DONE` while leaving a truncated file behind. It also took the
  destination path straight from the serial line, so a crafted `PUT` could write
  anywhere on the card; writes are now confined to `/mons/`.
- Touches just outside the Pokédex grid and the on-screen keyboard were treated
  as cell 0. The guard divided first and then checked for a negative index, but
  integer division truncates toward zero, so it never saw one. On a round screen
  that strip is exactly where a finger lands near the edge.

All four from @ajtudela.

## [1.9] - 2026-09-07

### Fixed

- All ~13 game timers compared `millis()` against an absolute deadline, which
  gives the wrong answer once `millis()` wraps at ~49.7 days of uptime: a timer
  could stay active forever, or a ceremony cut off instantly.
- The sprite transfer restarted from scratch on every attempt. It now records
  each confirmed file, so a reload after a dropped connection doesn't redo the
  full ~40 MB / ~10 min.
- Minigame physics advanced per frame rather than per unit of time, so the ball
  fell measurably slower with a large sprite on screen (Charizard) than a small
  one (Diglett) — high scores weren't comparable between species.

### Security

- `esp-web-tools` was loaded from a floating `@10` range with no integrity
  check. Pinned to 10.4.0 with a verified sha384 SRI hash.

All from @ajtudela.

## [1.8] - 2026-09-07

### Fixed

- **The level counter wrapped to 0 after ~10.6 days of play.** `level()`
  returned `uint8_t` and, at 60 minutes per level, the level is the hour count —
  so it rolled over at 256. Reachable in ordinary play, since "stay together"
  postpones the farewell indefinitely. Now `uint16_t`, capped at 999.
- Two related truncations found while fixing it: `calcStat()` took the level as
  `uint8_t`, so stats would have overflowed anyway; and `canEvolveNow()` compared
  against `(uint8_t)(evolveLevel + careMistakes)`, which wrapped with many care
  mistakes and let evolution happen early. The stat card already computed that
  threshold as `int`, so the screen and the actual check could disagree.

Diagnosed by @ajtudela.

## [1.7] - 2026-09-07

### Fixed

- **The UI had no accents, and the assumption behind that was wrong.** The code
  stated in two places that the font had none. In fact `glcdfont.h` is a full
  256-glyph CP437 table and `write()` doesn't filter bytes ≥ 0x80. Spanish now
  has its tildes and ñ, German its umlauts, French its accents. Characters are
  written as single bytes in octal, never UTF-8, because the UI centres with
  `strlen(t) * 6`. Only strings whose length was unchanged were touched.
  Uppercase accented vowels other than É aren't in CP437, so `EVOLUCION` and
  `PROGRES` stay unaccented, as do Portuguese ã/õ.
- German grammar, from the same contributor: "Wähle deinen Starter" (accusative),
  "Leb wohl" as two words, "Entwicklung" rather than "Entwickelt", and more.

Discovered by @danielberndt.

## [1.6] - 2026-09-07

### Fixed

- **Short sound effects were inaudible.** The amplifier was switched on 8 ms
  before each effect, far less than the NS4150B needs to settle, so a 35 ms tap
  finished before the speaker woke up. The boot jingle survived because it's
  ~440 ms — which is why sound seemed to work at startup and nowhere else. The
  amplifier now stays powered while sound is on and the pet is awake, matching
  Waveshare's reference. Notes and amplitude are unchanged: this doesn't alter
  the sound design, it just makes it audible.

Diagnosed by @djyf1.

## [1.5] - 2026-08-19

### Added

- Localized Pokémon names in French and German (Bulbizarre, Bisasam…). Only
  those two differ in gen 1; Spanish, Italian and Portuguese officially use the
  English names. Names come from PokéAPI via `tools/gen_names.py`.

## [1.4] - 2026-08-07

### Fixed

- **Bond was mathematically stuck.** It could gain at most 12 points a day, but
  a single care mistake cost 3 and could repeat every 30 minutes — up to 144 a
  day. With poops draining hygiene, mistakes chained and bond never recovered.
  Two players independently reported it pinned at exactly 10, which is the
  first-day ceiling of 13 minus one mistake. Daily cap 8 → 20, penalty 3 → 1,
  cooldown 30 → 60 min.

## [1.3] - 2026-08-07

### Fixed

- **The egg hatched by itself during starter selection.** Game time kept running
  while the starter screen was open, so taking more than 3 minutes to choose
  meant the egg hatched into the species already rolled — no egg animation, and
  the player's choice ignored. Three community reports matched (Pikachu twice,
  Eevee once). Time now freezes until a starter is chosen.

## [1.2] - 2026-06

### Added

- Firmware version shown at the bottom of the clock/settings screen.
