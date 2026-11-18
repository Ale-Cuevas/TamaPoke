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

1. **Back up first** if the test changes the save (needs, `HATCH`, `BYE`, `EGG`, `STEPS`, `COINS`, `BOXFILL`, `WAKE`,
   `STORE`, battles that pay, `WIPE`...): `board.py backup`, then `board.py cmd STATS BOX` and keep that output to
   compare. Before flashing a version that changes the save format, also keep a dated copy
   (`cp nvs_backup.bin nvs_backup_vX.Y_DATE.bin`; `nvs_backup*.bin` is git-ignored): the next backup overwrites
   `nvs_backup.bin`.
2. **Flash** if the build on the board is older than the code: `pio run -e tamapoke -t upload` (no sandbox). It
   doesn't touch the save. The boot log (`board.py reboot`) prints `TamaPoke fw vX.Y`; the clock/settings screen
   shows it too.
   - **Flash what you mean to flash.** If the working tree has unfinished edits (an agent mid-task, the user's own
     `tuning.h`), don't build from it: build a released commit in a throwaway worktree
     (`git worktree add --detach $TMPDIR/tp-vX.Y <commit>`, `pio run -e tamapoke -t upload` there, then
     `git worktree remove --force`; worktree commands need the sandbox disabled).
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
- `STATS`, `NEED i v` (0 food, 1 hygiene (removed from v3.1), 2 joy, 3 energy), `LVL n`, `SPEC n`, `TIME [epoch]`,
  `NIGHT`, `COINS n`, `GIFT item`.
- `HATCH`, `EGGD dex`, `BOX`, `ENC [dex]`, `STAGE n`, `BATTLE`, `TRAVEL`, `BALL`, `DASH`, `ZEN`.
- `TAP x y`, `BACK`, `FX name`, `PERF`, `HEALTH` (uptime, heap, SD, sprite; it does **not** show PSRAM).

### Two home pets (v3.0+)

Home has two places (slots 0 and 1); each is a pet, an egg or empty. The pet commands (`NEED`, `LVL`, `SPEC`,
`HATCH`, `NICK`, `DIE`...) act on the **focused** slot, as do the Care panel and the games.
- `FOCUS n`: which home slot the other commands act on. Refused (and logged) while a modal screen is open.
- `STATS` starts with one line per slot (`home0* pet ...`, `home1 vacant ...`; `*` = focused) and `focusErrors`,
  which must stay 0. `BOX` lists `active home0` / `other home1`, then the 6 Box slots.
- `STORE k [s]`: store home slot s (default: focused) into Box slot k.
- `WAKE k [s]`: wake Box slot k into home slot s (default: the first empty slot, then an egg's slot, then the
  focused one; a full slot swaps).
- To check something on the second pet: `FOCUS 1`, run the commands, `FOCUS 0` when done.

## Tap coordinates

`TAP` takes **canvas coordinates** (466×466, centre 233,233), the same space as `onTap`, not the 480 panel. Derive
them from `yard_ui.h` and `yard_panels.h` if the layouts have changed. At v3.3:
- Dock (y 404), since v3.3: Play 161, Bag 233, Shop 305. (v3.0-v3.2: Care 161, Play 233, Bag 305.)
- Pet menu (v3.3): open it with `MENU s` (pets walk, so don't tap them); `MENU` alone prints every row with its tap
  point. 4 rows (x 348): Sleep 152, Food 206, Care 260, Status 314. 5 rows with Evolve/Farewell last (x 331):
  y 125, 179, 233, 287, 341. Food list: same rows as the 5-row menu; Back (292,341), more (370,341).
- Pet picker (v3.3): card 0 (150,208), card 1 (316,208), back (233,418); or `PICK s`.
- `FEED f` (v3.2) feeds the focused pet directly (0 plain, 1-6 berries R B G Y P O, 7 cake, 8 golden); `BUY id`
  (berries are items 10-15); `STATS` prints `berries=` and `fav0/fav1` with `known`.
- Pets walk, so there is no fixed spot to tap one: use `FOCUS n` for logic checks and leave "tap a pet" to the user.
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
