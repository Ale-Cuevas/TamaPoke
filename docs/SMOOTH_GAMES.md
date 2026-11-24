# Smoother minigames (spec, 2026-11-24)

Goal: minigames above today's ~11-12 fps, ideally ~16-18 fps, with the same game feel. Not built yet.

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
