# Smoother minigames (spec, 2026-11-24)

Goal: minigames above today's ~11-12 fps, ideally ~16-18 fps, with the same game feel. Step 1 (faster flush) built, see "Step 1 results" at the end; step 2 not built.

## Measured on the board (v3.22, bc61c38)

Temporary timing build (instrumentation below), averages of 60 frames, minigame open, no input:

| Game | 40 MHz period | 40 MHz frame | 40 MHz send | 80 MHz frame | 80 MHz send |
|---|---|---|---|---|---|
| Ball | 85 ms | 75 ms | 57 ms | 64 ms | 46 ms |
| Sled | 89-91 ms | 86-88 ms | 57 ms | 75-77 ms | 46 ms |
| Waterfall | 88-89 ms | 86-88 ms | 57 ms | 75 ms | 46 ms |
| Battle | 86 ms | 82-83 ms | 57 ms | 71-72 ms | 46 ms |

- "frame" = whole `render()`; "send" = `Arduino_Canvas::flush` (the panel transfer); mask/border/overlay before it
  is ~4 ms; drawing the game itself is ~14 ms (ball) to ~25 ms (sled, waterfall).
- **Sled and waterfall already miss the 85 ms target** (`frameIntervalMs`, `power.h:103`), so their per-frame
  physics run ~5 % slow today.
- Doubling the clock to 80 MHz only saves 11 ms: the send is mostly overhead, not bandwidth. Raw 466×466×2 B at
  40 MHz QSPI is ~22 ms (~11 ms at 80 MHz).

## Why the send is slow

`Arduino_ESP32QSPI::writePixels` (`.pio/libdeps/tamapoke/GFX Library for Arduino/src/databus/Arduino_ESP32QSPI.cpp:322`):
chunks of `ESP32QSPI_MAX_PIXELS_AT_ONCE` = 1024 px (~212 per frame); for each the CPU byte-swaps into one DMA
buffer (`MSB_32_16_16_SET`), then `POLL_START/POLL_END` waits for it. CPU and SPI never overlap, and every chunk
pays the transaction setup. The QSPI bus is created with `max_transfer_sz = 1024*16 + 8`.

## Plan

### Step 1: faster flush (do first, display layer only)

- Own send path for the canvas flush in `round_display.h` (`RoundCanvas::flush`, `:66`), replacing the call to
  `Arduino_Canvas::flush` for the full frame: set the window (CASET/RASET as the library does for the CO5300),
  then stream the framebuffer with **two DMA buffers ping-pong**: byte-swap chunk N+1 while chunk N is in flight
  (`spi_device_queue_trans` + `spi_device_get_trans_result` instead of polling), with **bigger chunks** (try 8-16 K
  px; raise the bus `max_transfer_sz` to match, it is set in the library's `begin`, so either override
  `ESP32QSPI_MAX_PIXELS_AT_ONCE` with a `-D` build flag first as the cheapest experiment, or create our own bus).
- Cheapest first experiment: `-D ESP32QSPI_MAX_PIXELS_AT_ONCE=8192` (or 4096) in `platformio.ini` only, and
  measure. If that alone cuts the send a lot, the ping-pong is a smaller follow-up.
- No game changes. The yard and every screen get faster too.
- Expected (estimate, not measured): send ~25-30 ms at 40 MHz, frame ~45-55 ms, so ~16-18 fps possible.
- Check for: torn/shifted frames, the round mask and the full-yard border (`TP_FULL_YARD`) still right, the
  brightness toast overlay, burn-in orbit moves (`moveTo`), DMA-capable memory for the chunk buffers (internal
  RAM, not PSRAM).

### Step 2: game physics on real time (only once step 1 gives the headroom)

- Today each minigame moves a fixed amount per frame, tuned to 85 ms. Lowering the game interval without this
  makes the games play faster.
- Per game (ball, sled, waterfall, bag, battle animations): fixed-step accumulator at the old 85 ms step
  (simplest, keeps tuning exact, but needs interpolation to look smoother) or scale per-frame motion by
  `dt / 85`. Prefer scaling by dt where motion is simple; keep scores/rewards/timers identical.
- New game interval goes in `tuning.h` (balance numbers rule), and `power.h:103` reads it.
- User must replay every game on the board for feel (the sled is a favourite: keep it the same).

### Step 3 (probably skip): draw while sending

Second 434 KB framebuffer in PSRAM, send from the other core while the next frame draws. Gains the remaining
~15-25 ms of draw time but costs memory and needs care with the pre-send work (mask, border, overlay) and
PSRAM-DMA alignment. Only if step 1 + 2 fall short.

### Optional: 80 MHz QSPI

`BOARD_LCD_QSPI_HZ` (`board_amoled_216.h:50`). Ran fine for the timing test, but the 2.16" panel hasn't been
checked visually at 80 MHz (noise, glitches). Combine with step 1 only after a visual check by the user.

## Measuring (temporary instrumentation, don't commit)

What was used on 2026-11-24:
- `round_display.h` `flush()`: time from entry to the `Arduino_Canvas::flush` call (`prep`) and the call itself
  (`send`) with `micros()`, into two globals.
- `TamaPoke.ino` loop around `render()` (`:739`): when `pc.gameOpen` and a frame was drawn, accumulate the period
  (`now - lastRender`), render time, prep and send; print `FT n=60 period= max= render= prep= send=` every 60
  frames.
- Run: back up the save (`tools/board.py backup`), flash, `board.py cmd BALL` (then `DASH`, `ZEN`, `BATTLE`),
  `board.py log 13 --grep FT`, `BACK`; afterwards reflash the release build and `board.py restore nvs_backup.bin`.
- Worth considering: make it permanent as part of the `PERF` serial command instead of a throwaway.

## Release notes for whoever builds it

- Branch, version bump, CHANGELOG, `docs/GAME_LOGIC.md` (frame interval table §power, display layer, any moved
  pointers), a `docs/PENDING_TESTS.md` row for the user's feel check: use the `tamapoke-release` skill.
- Board checks EN/ES only; this change has no strings.

## Step 1 results (built 2026-11-25, branch `feature/smooth-games`)

Built: `qspi_fast.h` (`TpQspi`, a subclass of the library's `Arduino_ESP32QSPI` that replaces `writePixels` with a
ping-pong send, 4096 px chunks, two own DMA buffers in internal RAM; the library header is included with
`private` -> `public` just there, nothing vendored) and permanent frame timing in the `PERF` serial command
(`frame game ...` / `frame all ...` lines). Selectable: `-D TP_FAST_QSPI=0` is the old library send.

Measured on the board with that timing (60-120 frames per cell, averages in ms; "send" = panel transfer, "prep" =
mask/border/overlay before it, "render" = the whole `render()`; game interval is the 85 ms fixed one):

| Variant (40 MHz unless noted) | Free heap | Yard send | Ball render / send | Sled render / send | Waterfall render / send | Battle render / send |
|---|---|---|---|---|---|---|
| Baseline, library, 1024 px | 277.6 K | 56.9 | 74.0 / 56.2 | 88.6 / 56.9 | 85.7 / 56.9 | 83.7 / 56.9 |
| Library only, `ESP32QSPI_MAX_PIXELS_AT_ONCE=4096` | 265.3 K | 54.3 | 71.4 / 53.6 | 85.7 / 53.8 | 83.1 / 54.3 | 80.7 / 54.3 |
| Ping-pong, 1024 px | 277.9 K | 27.5 | 44.9 / 27.1 | 58.4 / 27.5 | 56.3 / 27.5 | 53.7 / 27.5 |
| Ping-pong, 4096 px (**shipped**) | 261.5 K | 25.5 | 42.9 / 25.2 | 56.3 / 25.5 | 54.3 / 25.5 | 52.0 / 25.5 |
| Ping-pong, 8192 px | 248.5 K | 25.5 | 42.9 / 25.2 | 56.5 / 25.5 | 54.3 / 25.5 | 51.8 / 25.5 |
| Ping-pong, 4096 px, **80 MHz** (not shipped) | 265.3 K | 25.3 | 42.8 / 25.0 | 56.3 / 25.3 | 54.1 / 25.3 | 51.8 / 25.3 |

- Prep stays 4.0-4.1 ms in games (6.8 ms in the yard, which repaints the border band source) and the period is the
  85 ms interval in every game now (before, sled / waterfall / battle ran 86-91 ms because render > 85 ms).
- The bigger library chunk alone only saves ~2.6 ms: the win is overlapping the byte swap with the transfer.
- 80 MHz gives nothing more (25.3 vs 25.5 ms): the send is now limited by the CPU swapping the framebuffer out of
  PSRAM (~17 MB/s), not by the bus. Keep 40 MHz. Raw 466x466x2 B at 40 MHz is ~22 ms, so ~3 ms of the 25.5 is
  chunk-start overhead.
- Draw time without the flush and prep: ball ~14 ms, sled ~26, waterfall ~24, battle ~22 ms.

Headroom for step 2: the frame costs render = draw + prep + send ~ 43 ms (ball) to 56 ms (sled). A game interval
of ~60 ms (16-17 fps) is safe for every game, ~48 ms (~20 fps) for the ball only; 55 ms sled is the floor.
Measured only; the game interval and physics are untouched (still 85 ms).

To check by eye (cannot be seen from serial): no tearing / shifted rows / colour swaps in any screen; round mask and
the full-yard border (`TP_FULL_YARD`) intact; brightness toast; burn-in orbit moves (the border is repainted after
a move); saver screen.

## Step 2 results (built 2026-11-25, branch `feature/smooth-games-dt`)

Game interval 85 ms -> **60 ms** (`GAME_FRAME_MS` in `tuning.h`, read by `frameIntervalMs`; `GAME_PHYS_REF_MS` 85 is the
reference the physics was tuned at). Low battery still does not double it.

What was "per frame" and how it was converted (everything else already ran on real time):

| Part | Before | Now |
|---|---|---|
| Ball fall, speed ramp, wall | `ballStepK(dt, score)`: steps of 85 ms, `<= 3` per frame | unchanged (`BALL_STEP_MS` = `GAME_PHYS_REF_MS`); parabola exact at any step |
| Ball: pet following it | `gamePetX += clamp(0.12 d, 7) * k` per frame | `ballPetFollow`: `1-(1-f)^r` of the gap, cap `7*speed*r`, `r = dt/85` (identical at 85 ms) |
| Bag: shake decay | `sackShake *= 0.84` per frame | `*= 0.84^(dt/85)` (`gameDecay`, dt clamped to 2 steps) |
| Sled (`dashTick`), waterfall (`zenTick`) | fixed 10 ms steps from every loop pass | unchanged; render only draws |
| Battle, encounter, Box light | `millis()` schedules | unchanged |
| Hit tests | taps are events (ball radius test at tap time); sled/waterfall are 10 ms steps | no change, no tunnelling change |

No random per-frame chance exists in any game (spawns are `random` once, rows use a seeded RNG per 10 ms step).
Known tiny difference: the ball's wall bounce is evaluated once per frame, so a frame length changes the exact moment
of a bounce by under one step (position differs by a few px after a bounce, exit velocity equal within 0.4).

Tests: `test/test_gametime.cpp` (power test now reads `GAME_FRAME_MS`).

Measured on the board at 60 ms (`PERF` `frame game`, averages; max in brackets), v3.22 + this change:

| Game | period | render | prep | send |
|---|---|---|---|---|
| Ball | 60.0 ms (max 60) | 42.5-43.7 (45.6) | 4.0 | 25.0-25.5 |
| Sled | 60.9-61.1 (max 97 on first frames, 65) | 54.6-57.2 (59.0) | 4.1 | 25.4-25.5 |
| Waterfall | 60.0-60.1 (63) | 54.3-54.6 (59.3) | 4.1 | 25.5 |
| Battle | 60.3-61.0 (173 / 105: sprite loads from the SD) | 50.3-51.7 (54.3, 105 on a load) | 4.1 | 25.5 |

The sled is the tight one: render 57 ms for a 60 ms slot, so its period drifts up to ~61 ms; the physics does not care.

Real time checks over serial: waterfall `ZEN?` t advanced 1800 ms per 1.8 s of wall clock and reached the 45000 ms
end 47.2 s after `ZEN` (1.5 s intro + 45 s); sled `DASH?` race ms per wall ms = 1.004 over 5.5 s.

What the user should replay: see the report of this step (ball, bag, sled, waterfall, battle feel).
