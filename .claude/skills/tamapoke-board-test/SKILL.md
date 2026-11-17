---
name: tamapoke-board-test
description: Test TamaPoke on the real Waveshare AMOLED-2.16 board over USB serial - flash, back up and restore the pet's save, drive the game with serial commands and simulated taps, read the logs, and fill in docs/PENDING_TESTS.md rows. Use whenever the user asks to flash the board, test or check something on the board/device, run a PENDING_TESTS row, reproduce a bug on hardware, or read the serial output.
---

# TamaPoke board tests

The board is the user's real pet: **its save is precious**. `tools/board.py` opens the serial port without
restarting the board, sends commands and waits for each `DONE`. Run it with PlatformIO's Python, which has
pyserial and esptool:

```bash
PY=~/.platformio/penv/bin/python
$PY tools/board.py cmd STATS "NEED 0 10"   # each command's answer, up to its DONE
$PY tools/board.py log 20 --grep FOOD      # listen 20 s (filter optional)
$PY tools/board.py backup                  # nvs_backup.bin (git-ignored); restarts the board
$PY tools/board.py restore nvs_backup.bin  # restarts the board
$PY tools/board.py reboot                  # restart on purpose to see the boot log
```

Serial and esptool need the sandbox disabled (`/dev/ttyACM0`, `~/.platformio`). If `ports` lists nothing, the
board is unplugged: ask the user to plug it in with a data cable and stop there.

## The procedure

1. **Back up first** if the test changes the save (needs, `HATCH`, `BYE`, `EGG`, `STEPS`, `COINS`, `BOXFILL`, battles
   that pay, `WIPE`...): `board.py backup`, then `board.py cmd STATS BOX` and keep that output to compare.
2. **Flash** if the build on the board is older than the code: `pio run -e tamapoke -t upload` (no sandbox). It
   doesn't touch the save. The boot log (`board.py reboot`) prints `TamaPoke fw vX.Y`; the clock/settings screen
   shows it too.
3. **Run the check**: set the state with serial commands, act with `TAP x y`, and read the answers and the
   `log` output. Most actions print a line (`FOOD drop`, `FOOD eaten kind=0 ok=1`, `tap x y battle=...`).
4. **Restore** the backup if you changed the save: `board.py restore nvs_backup.bin`, then `STATS` / `BOX` should match
   step 1 (only the offline minutes advance).
5. **Record**: update the row's Result in `docs/PENDING_TESTS.md`: `serial ✓ (what you saw)`, keeping
   `touch/visual: pending` for anything only eyes or fingers can judge (colours, animation, feel). Ask the user for
   those, naming exactly what to look at. **EN and ES only**: never ask for checks in other languages
   (the language is changed on the settings screen; there is no serial command for it).

## Commands

The full table is in `docs/GAME_LOGIC.md` §16; read it there, it stays current. The most used ones:
- `STATS`, `NEED i v` (0 food, 1 hygiene, 2 joy, 3 energy), `LVL n`, `SPEC n`, `TIME [epoch]`, `NIGHT`, `COINS n`, `GIFT item`.
- `HATCH`, `EGGD dex`, `BOX`, `ENC [dex]`, `STAGE n`, `BATTLE`, `TRAVEL`, `BALL`, `DASH`, `ZEN`.
- `TAP x y`, `BACK`, `FX name`, `PERF`, `HEALTH`.

## Tap coordinates

`TAP` takes **canvas coordinates** (466×466, centre 233,233), the same space as `onTap`, not the 480 panel. Derive
them from `yard_ui.h` and `yard_panels.h` if the layouts have changed. At v2.6:
- Dock (y 404): Care 161, Play 233, Bag 305.
- Care panel: food row y 168: red berry 98, blue 188, green 278, cake 368; care row y 292: light 113, bath 233, heart 353.
- Play panel: row 1 y 166: ball 121, battle 233, bag 345; row 2 y 290: sled 121, waterfall 233, travel 345.
- Header back area: y < 72 in the minigames.

## Board facts that bite

- Opening the port with pyserial's defaults resets the board. `board.py` sets `rts=False, dtr=True` first. The first
  open right after a USB replug may still restart it and garble the first command: wait ~8 s and resend.
- There is no RESET button (only PWR, BOOT, USER). Restart by re-uploading, `board.py reboot`, or PWR 4 s off / on.
  With a battery in, unplugging USB doesn't restart it.
- A restart clears the IMU step counter. Upload fails to find the port → unplug the battery, hold BOOT while plugging
  USB, release, upload again.
- `NEED`/`STATS` changes are saved like normal play. That is why step 1 backs up.
