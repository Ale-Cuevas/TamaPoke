# Two new games: Dash (tilt, sled, trains SPD) and Waterfall (hold, trains DEF)

Asked for on 2026-11-07, on top of v1.41 (main `b2f98b6`). Today PLAY has three games:
ball (`startGame()`), battle (`startPractice()`) and the training bag (`startSack()`, trains ATK).
This spec adds two more, each training one stat:

| Code | Game | Input | Trains | Size | Version |
|------|------|-------|--------|------|---------|
| **DASH** | The pet rides a sled down a slope; tilt the board to dodge obstacles that come faster and faster | IMU tilt (accelerometer) | **SPD** (`trSpe`) | M–L | given at merge |
| **ZEN** | The pet meditates under a waterfall; hold the screen to keep its focus gauge in the middle band | Touch held / released | **DEF** (`trDef`) | M | given at merge |

v1.42 is taken by OTA (branch `feature/ota`, not merged yet). Give each game its version when it
merges, in merge order (v1.43, v1.44 if OTA goes first).

"Gyroscope" in the request means tilting the board. The firmware reads only the QMI8658's
accelerometer (`imu.h`), and tilt is the direction of gravity, so no gyroscope is needed. The
gravity direction is also steadier than an integrated gyro, which drifts.

---

## Contents

- [Shared: PLAY menu, power, records, tests](#shared-play-menu-power-records-tests)
- [Task DASH: tilt to dodge](#task-dash-tilt-to-dodge)
- [Task ZEN: meditate under the waterfall](#task-zen-meditate-under-the-waterfall)
- [Order and parallel work](#order-and-parallel-work)
- [Open questions](#open-questions)

---

## Shared: PLAY menu, power, records, tests

### PLAY menu with 5 slots

- **FR-G.1** The PLAY menu goes from 3 slots to 5: **ball | battle | bag | dash | waterfall**.
  Change `PLAY_MENU_W` (`TamaPoke.ino:133`) from `3 * PLAY_MENU_SLOT` to `5 * PLAY_MENU_SLOT`, so the
  menu is 330 px wide (x 68–398). At y 288–352 the round screen is about 400 px wide, so it fits.
  Add slots 3 and 4 to the tap handler (`TamaPoke.ino:1262`) and the menu drawing
  (`TamaPoke.ino:2101`).
- **FR-G.2** Two new 16×16 icons go in `species.h` next to `SPR_ICON_SACK`, drawn at 3× like the
  others:
  - `SPR_ICON_DASH`: a running shoe or speed lines.
  - `SPR_ICON_ZEN`: a waterfall over a rock.
- **FR-G.3** Each game refuses to start the way the others do: egg, asleep, during a ceremony →
  `return`. DASH also refuses when `imuPresent()` is false. Its icon is greyed out, and a tap gives
  `SFX_DENY` and the `FXO_DENY` shake, like battle when `!pet.canBattle()`.

### Power and screen while playing

The ball game keeps the screen awake because the player keeps tapping. In DASH nobody touches the
screen, and in ZEN the finger stays down without new touch events. So:

- **FR-G.4** `powerCtx()` (`TamaPoke.ino:544`) adds `dashOpen || zenOpen` to `c.gameOpen`, so both
  games draw at 85 ms frames.
- **FR-G.5** While either game is open, `lastInteract` is refreshed every frame. The screen never
  dims to `POWER_DIM1_MS` or goes to the screensaver in the middle of a run.
- **FR-G.6** While DASH is open:
  - the face-down detector doesn't dim the screen. Someone playing on their back holds the screen
    facing down.
  - the home tilt reactions (the pet walking or sliding, `TamaPoke.ino:520`/`531`) are off. Add
    `dashOpen`/`zenOpen` to every list that already checks `gameOpen || sackOpen || battleOpen()`.
- **FR-G.7** PWR short press and a tap on the header (y < 72) quit with no reward, like ball and bag.

### Records and save

- **FR-G.8** Two new player-wide records, next to `gameHi`/`strHi` (`pet.h:162`):
  - `dashHi` (`uint16_t`, NVS key `"dhi"`): most obstacles dodged in one run.
  - `zenHi` (`uint16_t`, key `"zhi"`): most focus time in one session, in tenths of a second.

  They are player fields like the other records (FR-BOX.7): the Box doesn't store them. That's 2
  more NVS entries, well inside the budget (`NEXT_FEATURES.md`, "the NVS budget"). Old saves load
  with 0.

### Code layout and tests (both games)

- **FR-G.9** All game rules go in a pure header, like `ballgame.h`: `dashgame.h` and `zengame.h`,
  with host tests in `test/test_dashgame.cpp` and `test/test_zengame.cpp`, added to `test/Makefile`.
  The sketch only draws, reads input and calls these functions.
- **FR-G.10** The reward goes in `Pet` (`pet.cpp`, next to `trainStrength()`), returns the stat
  points that actually went in (capped at 100, like the v1.33 fix in `trainStrength`), and is
  covered in `test/test_pet.cpp`.
- **FR-G.11** Every new text goes in `i18n.h`/`i18n.cpp` for all 8 languages. The buffer sizes are
  checked by `TestBuffersDeTexto` in `test/test_tools.py`. Japanese is the longest; size buffers for
  it.
- **FR-G.12** `tools/render/render.cpp` gets scenes for each game: playing, early and late (fast or
  narrow), and the result screen. These go in the render sheet so the look can be checked without
  the board.
- **FR-G.13** Serial commands for board checks. They only change what's on screen: no reward, no
  record, nothing saved (like `BALL n` with `gameNoSave`). The board has the user's real pet. See
  the commands in each task.

---

## Task DASH: tilt to dodge

### Goal

The pet rides a sled down a slope toward the viewer. It stays in the middle of the screen, and
the ground scrolls up past it. Tilting the board left or right steers the sled across the track to
dodge obstacles coming up from the bottom. Obstacles come faster the longer the run lasts, until the
third hit. It trains SPD.

**Why a sled (decided 2026-11-07).** The SD sprites have no running-toward-the-viewer animation.
Idle and Sit already face the viewer (sheet row 0), but a pet idling while the ground races past
looks like it hops in place. In a sled the pet sits still for a reason. The sled hides its legs,
and steering by leaning fits the tilt control. A downhill slope also explains going faster and
faster. So DASH needs **no new sprites, no SD repack and no loader change**. The sled is drawn with
primitives, so it can lean into turns even though sprites can't rotate (`blit.h`). Its hitbox is
the same for every species, so a big pet isn't harder to steer than a small one.

The board is held in the hand at any angle, from upright to lying face up. Each run starts with a
short calibration that takes the angle the player is holding as "straight".

### How tilt is read today, and why the game needs its own reading

- `TiltEstimator` (`motion.h:148`) low-pass filters gravity (`TILT_TAU_MS` = 250 ms) and gives
  `deg = asin(gx / |g|)`. This is the side-to-side angle, with + meaning the right side is down.
  Because it uses only the X part of gravity, **it doesn't depend on pitch**. It reads the same
  with the board upright, slanted back, or flat face up. Only the zero point differs, and the
  calibration takes care of that.
- It is too slow for a game:
  - 250 ms filter;
  - `side` waits `TILT_HOLD_MS` = 500 ms;
  - when the screen is lit the FIFO is read every `FIFO_READ_LIT_MS` = 100 ms.

  All that adds up to 300–600 ms of lag.

### Requirements

**Input and calibration**

- **FR-DASH.1 Fast roll.** `ImuPipeline` gets a second, fast roll output: the same
  `asin(gx·sign/|g|)` with its own low-pass `DASH_TILT_TAU_MS`. `imu.h` exposes it as
  `float imuRollFast()`. While DASH is open, `imuPoll` reads the FIFO every `DASH_FIFO_MS`, through
  a new `imuSetFast(bool)` that the game turns on when it starts and off when it ends.
- **FR-DASH.2 Calibration at start.** Starting DASH shows the pet standing, a 3-2-1 countdown and
  "Hold it the way you'll play" (`DASH_CAL_MS`). The baseline is the mean of `imuRollFast()` over
  the last second of the countdown.
  - If that second moves too much (standard deviation > `DASH_CAL_MAX_SD`), show "Hold still" and
    restart the countdown.
  - After 2 restarts, accept the baseline anyway, so a shaky hand still gets to play.
  - Every run calibrates again. Nothing is saved.
- **FR-DASH.3 Steering.** Use `rel = roll − baseline`.
  - Up to `DASH_DEAD_DEG` it counts as straight.
  - From there to `DASH_FULL_DEG` it maps linearly to a steer value of 0…1; beyond that it stays at
    1. The sign follows the tilt: right side down = run right.
  - Steering sets the **sideways speed**: `steer × DASH_LAT_MAX`. That speed rises with the run
    speed as `sqrt(speedFactor)`, so dodging stays possible at 3×.
  - A steer value that sets the pet's position directly would make the pet shake with the hand, and
    a face-up hold has little room to tilt. Speed control is easier to hold.
- **FR-DASH.4** While DASH is open, the step counter is paused (`StepCounter::pause`), so tilting
  back and forth doesn't count as steps.

**Camera, track and pet**

- **FR-DASH.5** The sled and pet are drawn at a fixed screen spot: x = `CX`, the sled's bottom at
  `DASH_PET_Y`. It is centred left to right. It sits a little above the middle of the screen, so
  more track is visible ahead. The world moves, the sled doesn't. The sled has a sideways position
  `px` on a track of
  ±`DASH_HALF_W`. On screen, an object at track x is drawn at `CX + (x − px)`. Tilting slides the
  track and obstacles the other way.
- **FR-DASH.6** The track edges are drawn (grass, cliff, or the biome's border). `px` stops at the
  edges with a small bump, but it doesn't count as a hit.
- **FR-DASH.7** The ground scrolls up at the run speed. It has texture marks (tufts, pebbles,
  stripes) so the speed shows. The sled leaves two runner tracks behind it (above it on screen),
  which bend when it steers. From 2× on, add speed lines at the sides.
  - Colours come from the home biome (`homeBiome()`), with a night version like
    `drawGameScene()`.
  - Draw it all with primitives, like the training bag. No new image assets.
- **FR-DASH.8 Pet in a sled.** The pet rides a sled drawn with primitives. No new sprites.
  - **Pet sprite:** `PMD_SIT` if the card has it, else `PMD_IDLE`. Both face the viewer (sheet
    row 0). `PMD_HURT` on a hit (FR-DASH.11).
  - **Draw order:** sled back and seat, then the pet, then the sled's front lip over the pet's lower
    body. The lip covers the feet, so the sprite's resting pose looks like sitting. A tall pet sticks
    out of the top, which is fine.
  - **Size:** the sled's width follows the sprite's visible width (`PmdAct` w and `base`, times the
    draw scale) plus a margin, clamped to `DASH_SLED_W_MIN`–`DASH_SLED_W_MAX`. Only the drawing
    scales. The hitbox doesn't (FR-DASH.11).
  - **Shape:** a rounded body, a raised front lip and two runners, seen from the front at a slight
    angle from above. Colours come from the home biome palette (a red wooden sled on snow, a
    darker wood on grass or sand…), with a night version.
  - **Lean:** the sled tilts up to `DASH_SLED_LEAN_DEG` toward the turn, in proportion to the steer
    value. It's a rotated polygon, which is cheap. The pet sprite can't rotate: it shifts 2–4 px
    toward the turn and 1–2 px down on the low side.
  - **Speed feel:** a small bounce every few hundred ms, faster at higher speed, and snow or dust
    puffs from the runners from 2× on.

**Obstacles**

- **FR-DASH.9** Obstacles come in **rows** that spawn just below the bottom edge and scroll up with
  the ground.
  - Each row has 1–2 obstacles, `DASH_OBS_W_MIN`–`DASH_OBS_W_MAX` wide.
  - Kinds by biome: rock, log, bush, puddle, ice block… Drawn with primitives, 2–3 kinds per biome.
    They are all the same to the rules.
- **FR-DASH.10 Always passable.** Every row leaves at least one gap of `DASH_GAP_MIN` (the pet's
  hitbox plus a margin).
  - The middle of the next row's gap must be reachable: the sideways distance to it must be at
    most `0.7 × lateral max × time between rows`.
  - The generator is a pure function with a seed (`dashNextRow(rng, prevGapX, speedFactor)`). Host
    tests run 10 000 rows at every speed and check both rules.
- **FR-DASH.11 Hits.** The hitbox is the sled's, a fixed `DASH_HIT_W` × `DASH_HIT_H` box at the
  sled's spot, the same for every species and whatever size the sled is drawn. Obstacle boxes are
  shrunk by `DASH_HIT_SHRINK` to be fair.
  - A hit costs one of 3 hearts and plays `PMD_HURT` and `SFX_DENY`.
  - The sled jolts sideways, the screen shakes for 4 px, and sled and pet blink, invulnerable, for
    `DASH_INVULN_MS`.
  - The run speed doesn't change.
- **FR-DASH.12 Score** = rows that went past the pet without a hit. A row hit during the
  invulnerable time still counts as dodged. It is shown big at the top with the record under it,
  like the ball game, and the hearts under that.

**Faster and faster**

- **FR-DASH.13** Run speed is `v = DASH_V0 × f(t)`, where t is the run time without the
  calibration, `f(t) = min(1 + t / DASH_DOUBLE_S, DASH_SPEED_MAX)`. 2× at 40 s, top 3× at 80 s.
- **FR-DASH.14** Rows also get closer together: the spacing goes linearly from
  `DASH_ROW_GAP_START` px at 1× to `DASH_ROW_GAP_END` px at 3×. Rows per second therefore rise
  faster than the speed itself. Rows with 2 obstacles also get more common: the chance
  `DASH_DOUBLE_P` goes from 0.15 at 1× to 0.6 at 3×.
- **FR-DASH.15** Time steps are fixed and capped, like `ballStepK`, so the game runs at the same
  speed whatever the frame time is. A late frame counts as at most 3 steps.

**End and reward**

- **FR-DASH.16** The third hit ends the run. The result screen stays 4 s, like the ball game:
  "Dodged N", record or "New record!", and "+N SPD".
- **FR-DASH.17** `uint8_t Pet::dashResult(uint16_t dodged)`:
  - SPD: `gain = min(dodged / DASH_PER_POINT, DASH_GAIN_MAX)` into `trSpe` (capped at 100), and
    returns what went in.
  - Energy `dropTo(energy, 12, 5)`, fullness `dropTo(fullness, 5, 5)`.
  - Weight `−dodged / DASH_WEIGHT_DIV`.
  - Joy `+5 + min(dodged, 15) × 2`.
  - Heart if `dodged ≥ 10`.
  - `addBond(2)`, `registerCare()`, `save()`.
  - Record `dashHi`. Use `fxSnap`/`fxNeeds` so the "+N" effects show on the home screen afterwards.
- **FR-DASH.18 Serial** (screen only, FR-G.13):
  - `DASH` starts a no-save run.
  - `DASH <s>` starts it at the speed of second s (`DASH 40` → 2×).
  - `STEER <deg>` replaces the tilt with a fixed angle relative to the baseline; `STEER OFF` goes
    back to the real tilt.
  - `IMU` prints `rollFast` and the baseline too.

### Constants (starting values, tune on the board)

| Constant | Value | Note |
|---|---|---|
| `DASH_TILT_TAU_MS` | 60 | fast roll filter |
| `DASH_FIFO_MS` | 40 | FIFO read while playing (62.5 Hz → ~2–3 samples per read) |
| `DASH_CAL_MS` | 2000 | countdown; baseline = mean of the last 1000 ms |
| `DASH_CAL_MAX_SD` | 4° | "Hold still" above this |
| `DASH_DEAD_DEG` / `DASH_FULL_DEG` | 3° / 22° | steering dead zone / full steer |
| `DASH_LAT_MAX` | 300 px/s | sideways speed at full steer, 1× |
| `DASH_HALF_W` | 170 px | half the track width |
| `DASH_PET_Y` | 200 | sled bottom on screen (CY = 233) |
| `DASH_HIT_W` / `DASH_HIT_H` | 60 / 34 px | sled hitbox, the same for every species |
| `DASH_SLED_W_MIN` / `_MAX` | 70 / 150 px | drawn sled width, follows the sprite |
| `DASH_SLED_LEAN_DEG` | 12° | sled tilt at full steer |
| `DASH_HIT_SHRINK` | 15 % | obstacle box shrink |
| `DASH_V0` | 160 px/s | starting speed |
| `DASH_DOUBLE_S` / `DASH_SPEED_MAX` | 40 s / 3× | speed ramp |
| `DASH_ROW_GAP_START` / `_END` | 260 / 170 px | row spacing at 1× / 3× |
| `DASH_GAP_MIN` | 110 px | free gap in every row |
| `DASH_OBS_W_MIN` / `_MAX` | 50 / 140 px | obstacle width |
| `DASH_INVULN_MS` | 1200 | after a hit |
| `DASH_PER_POINT` / `DASH_GAIN_MAX` | 4 / 18 | SPD: 1 point per 4 dodged, same cap as the bag |
| `DASH_WEIGHT_DIV` | 8 | weight burn |

### Acceptance

- Host tests (`test_dashgame.cpp`):
  - steering curve: dead zone, linear part, the cap, the sign;
  - speed ramp (1× at 0 s, 2× at 40 s, 3× at 80 s and after);
  - row spacing;
  - generator rules over 10 000 rows (FR-DASH.10);
  - hitbox overlap cases;
  - fixed-step physics doesn't change with frame time;
  - calibration: mean, the shaky case, 2 retries then accept.
- `test_motion.cpp`: the fast roll is pitch-independent. Replay `test/data/imu/` captures. Synthetic
  gravity at 0°, 45° and 90° pitch with the same roll gives the same `rollFast`.
- Sled size (pure function): clamps at both ends, follows the sprite width in between; the hitbox
  doesn't change with it.
- `test_pet.cpp`: `dashResult` gains and cap, stat cap at 100, record, refused while an egg or
  during a ceremony.
- Render sheet: early run, 3× run with speed lines, a hit (blink), result. Sleds with a tiny pet
  (Caterpie), a huge one (Onix) and a flying one (Pidgey), leaning left and right, in a snow
  habitat and a grass one, by day and by night.
- Board: the user plays held upright, slanted and flat face up. Each one must steer the same after
  calibration. A run at 1× is easy. At 2× (≈ 40 s) it's hard, but every row can be dodged. Frame
  time stays ≤ 85 ms (serial `scene`).

---

## Task ZEN: meditate under the waterfall

### Goal

The pet sits in a pool under a waterfall, eyes closed, concentrating. The concentration gauge
**rises while the screen is touched and falls when it's released**. The goal is to keep it in a
band around the middle for the whole session. Too low, the water knocks the pet out of focus. Too
high, it gets restless. The water gets stronger and the band narrows as the session goes on. It
trains DEF, the stat for standing firm.

### Requirements

**Scene**

- **FR-ZEN.1 Waterfall.** Drawn with primitives, the same in every biome; it's a trip, not the
  home:
  - dark rock walls left and right;
  - a water column about 150 px wide down the middle from the top edge, with lighter streaks
    scrolling down fast;
  - a pool at the bottom with foam circles where the water lands;
  - the pet on a flat rock in the pool, under the falling water, centred at about y 260.

  There is a night palette (`sceneHour()` like `drawGameScene()`).
- **FR-ZEN.2 Pet sprite by state** (fallback `PMD_IDLE` for any action the card lacks):
  - **in the band:** `PMD_SIT`, or `PMD_BREATH` (DeepBreath) if it has one, plus a soft glow ring
    that grows while the focus lasts;
  - **below the band:** `PMD_HURT` flinch, with spray bursting off the pet;
  - **above the band:** `PMD_HOP`, restless, with a small "!" over it.

**Gauge and control**

- **FR-ZEN.3 Gauge.** A vertical bar at the right (x ≈ 392–412, y ≈ 120–346). Holding pushes it
  up, and up means more focus.
  - The track is grey. The target band is green, and it turns brighter while the gauge is inside
    it.
  - A marker line shows the current value.
- **FR-ZEN.4 Feel: inertia, not a plain rate.** The concentration `c` (0–100) has a speed `vc`.
  - Holding accelerates it up by `ZEN_PUSH`. Releasing pulls it down by `ZEN_PULL`.
  - Damping `ZEN_DAMP`, and `|vc| ≤ ZEN_VMAX`.
  - At 0 or 100 it stops (`vc = 0`).

  With a plain "up while held, down when released" rate, quick taps are enough to stay in the band.
  With inertia it takes skill to stop the gauge in time (the fishing-bar feel), and the player
  learns to feather the touch.
- **FR-ZEN.5 Input.** Any touch held below the header counts. Read the touch state at the loop's
  rate (the `pressed` flag in the touch handler, `TamaPoke.ino:983`), not only on touch-down.
  Physics uses fixed steps of ≤ 50 ms, like `ballStepK`.
- **FR-ZEN.6 Session.** `ZEN_INTRO_MS` intro, "Keep the gauge in the green", then `ZEN_SESSION_MS`
  of play. `c` starts at 50 and `vc` at 0. A thin time bar under the header counts down, like the
  bag's.

**Harder over the session**

- **FR-ZEN.7 Narrowing band.** The band is centred at 50. Its width goes linearly from
  `ZEN_BAND_START` to `ZEN_BAND_END` over the session.
- **FR-ZEN.8 Water surges.** From `ZEN_SURGE_FROM_MS` on, a surge comes every
  `ZEN_SURGE_GAP_MAX` → `ZEN_SURGE_GAP_MIN` ms (random within ±25 %, more often later).
  - A surge kicks the gauge down: `vc −= ZEN_SURGE_KICK`, from 25 → 40 over the session.
  - **Each surge is shown `ZEN_SURGE_WARN_MS` before it hits**: a white swell at the top of the
    water column. A good player can start holding early.
  - It plays a splash sound (`SFX_` reuse is fine).
- **FR-ZEN.9** Surge timing comes from a seeded RNG inside `zengame.h`, so host tests can replay a
  whole session.

**Score, end and reward**

- **FR-ZEN.10 Score** = total time inside the band, in tenths of a second (0–300 for 30 s). It is
  shown live as "Focus 12.3 s" at the top, and the record under it.
- **FR-ZEN.11 Result** (3.5 s, like the bag): "Focus 23.4 s", record or "New record!", "+N DEF".
- **FR-ZEN.12** `uint8_t Pet::zenResult(uint16_t focusTenths)`:
  - DEF: `gain = min(focusTenths / ZEN_TENTHS_PER_POINT, ZEN_GAIN_MAX)` into `trDef` (capped at
    100), and returns what went in.
  - Energy `dropTo(energy, 8, 5)`, fullness `dropTo(fullness, 3, 5)`, joy `+6`.
  - Heart if focus ≥ 20 s.
  - `addBond(2)`, `registerCare()`, `save()`.
  - Record `zenHi`.

  It doesn't burn weight: sitting still isn't exercise. The 12 h good care that gives +1 DEF
  (`pet.cpp:261`) stays as it is.
- **FR-ZEN.13 Serial** (screen only, FR-G.13):
  - `ZEN` starts a no-save session.
  - `ZEN <s>` starts it at second s (narrow band, strong surges).
  - `HOLD 1` / `HOLD 0` fakes the finger down or up.
  - `ZEN?` prints `c`, `vc`, the band, the next surge time and the focus so far.

### Constants (starting values, tune on the board)

| Constant | Value | Note |
|---|---|---|
| `ZEN_SESSION_MS` / `ZEN_INTRO_MS` | 30000 / 1500 | |
| `ZEN_PUSH` / `ZEN_PULL` | 220 / 180 %/s² | hold / release |
| `ZEN_DAMP` | 1.5 /s | `vc *= exp(−ZEN_DAMP·dt)` |
| `ZEN_VMAX` | 60 %/s | |
| `ZEN_BAND_START` / `_END` | 24 / 12 | band width in %: 38–62 at the start, 44–56 at the end |
| `ZEN_SURGE_FROM_MS` | 5000 | |
| `ZEN_SURGE_GAP_MAX` / `_MIN` | 6000 / 3000 ms | |
| `ZEN_SURGE_KICK` | 25 → 40 %/s | |
| `ZEN_SURGE_WARN_MS` | 400 | the swell before a surge |
| `ZEN_TENTHS_PER_POINT` / `ZEN_GAIN_MAX` | 20 / 15 | DEF: 1 point per 2 s in the band, 15 for a perfect session |

### Acceptance

- Host tests (`test_zengame.cpp`):
  - holding from 50 goes up and releasing goes down;
  - inertia: after a release the gauge keeps rising for a moment;
  - stops at 0/100;
  - band width at 0 s, 15 s and 30 s;
  - surge times grow more frequent and each warning comes `ZEN_SURGE_WARN_MS` before its kick;
  - focus counts only inside the band;
  - fixed-step physics gives the same result at 30 ms and 85 ms frames;
  - a scripted "bang-bang" player (hold below 50, release above) scores > 60 % at the start values.
    That shows the game can be won, not that it is trivial.
- `test_pet.cpp`: `zenResult` gains and cap, stat cap at 100, record, refused while an egg or
  during a ceremony.
- Render sheet: intro, mid-session in the band (glow), below the band (spray), a surge warning,
  result.
- Board: the user plays three sessions. The first is learnable. Staying in the band in the last 10 s
  is hard but possible. Holding the finger down doesn't dim the screen.

---

## Order and parallel work

The two games don't depend on each other. Each runs on its own branch off `main`, after or beside
OTA, following the shared checklist in `GAMEPLAY_TUNING.md`:
- version bump, `CHANGELOG.md`, `README.md` games list;
- `PENDING_TESTS.md` / `DEVICE_TESTS.md` rows;
- `test/run_tests.sh`, `tools/render/render.sh`, `pio run -e tamapoke`;
- serial checks on the board;
- `--no-ff` merge.

Leave French out of the board checklists.

They only conflict on the shared parts: the PLAY menu (FR-G.1/2), `powerCtx()` and the "game open"
lists (FR-G.4–6), and the new `Pet` record fields. The first branch to merge adds the 5-slot menu
with the other slot showing nothing yet. The second fills its slot in.

**Either order works.** With the sled, neither game needs SD changes, so both are firmware only.
ZEN is still the smaller one (touch only), so it's a good first branch for the 5-slot menu and the
power rules. DASH adds the IMU fast path (FR-DASH.1) on top.

**A separate small fix, not needed for DASH:** the sprite loader (`sdmon.cpp:43`) rejects a whole
sprite file if it holds an action id it doesn't know. If a future pack adds an action, older
firmware would lose that pet's sprite. Skipping unknown ids instead keeps future SD packs safe.
Worth doing on any branch that touches `sdmon.cpp`.

Board safety: the board has the user's real pet. Do the gameplay checks with the no-save serial
commands. A real run that gives stats is the user's choice, or use the NVS backup and restore
(esptool `read_flash`/`write_flash 0x9000 0x5000`).

---

## Open questions

1. **Does the ball game keep training SPD?** Today `playResult()` gives `trSpe += score / 5` with no
   cap per session (`pet.cpp:606`). With DASH, each stat could have its own game: bag = ATK,
   waterfall = DEF, dash = SPD, and ball = joy and weight only. The proposal is to **leave ball as it
   is** for now and decide after playing DASH.
2. **The waterfall washes the pet?** For example, hygiene +10 after a full session (not when
   quitting). It's a nice touch, but it overlaps with bathing. Off by default.
3. **A raft in water habitats?** Same rules, a log raft outline instead of the sled, splashes
   instead of snow puffs. Later, optional; the sled ships first in every habitat.
4. **Gauge shape in ZEN:** a vertical bar (FR-ZEN.3), or an arc around the bottom of the round
   screen. The bar is simpler to draw and read. An arc can come later if the bar looks out of place.
