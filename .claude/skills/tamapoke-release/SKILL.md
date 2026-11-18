---
name: tamapoke-release
description: Ship a finished TamaPoke change as a new firmware version - branch, version bump, CHANGELOG, GAME_LOGIC.md guide update (header + stale file:line pointers), PENDING_TESTS row, build, commit, merge to main and push (the web installer only for major updates). Use whenever a TamaPoke feature or fix is done and needs releasing, or the user says "release", "ship it", "bump the version", "commit and merge", or asks to fix stale GAME_LOGIC pointers.
---

# TamaPoke release

Every change that reaches main gets its own `FW_VERSION`. `tools/release.py` does the mechanical parts. You write
the text the player and the next developer read.

## Before you start

- Work on a branch: `git checkout -b feature/<name>` or `fix/<name>`. Leave the user's own uncommitted edits (often
  `tuning.h`) out of your commits. Stage files by name, never `git add -A`.
- Take the next free version: the top of `CHANGELOG.md` + 0.1. **Check memory for reserved numbers** (the OTA
  branch `feature/ota` claims a version of its own and is never merged by you).

## Steps

1. **Bump**: `python3 tools/release.py bump X.Y`. This sets `FW_VERSION` in `TamaPoke.ino` and adds a
   `## [X.Y] - today` skeleton with a `- TODO` to `CHANGELOG.md`. **The web installer is not touched by default**:
   while the user iterates, only major updates (or an explicit ask) refresh it. For those, `bump X.Y --web` also
   sets `web/manifest.json`, and step 5 rebuilds the image.
2. **CHANGELOG**: replace the TODO. Write for players: what changed in the game, in plain words, under `### Added`,
   `### Changed` or `### Fixed`. A bold one-line summary, then 1-3 sentences. List new serial commands too.
3. **Guide** (`docs/GAME_LOGIC.md`; CLAUDE.md requires it in the same commit when game logic changes):
   - Update the section that describes what changed.
   - Add the version to the header line at the top (`> **Written from firmware ...`), e.g.
     `... and v2.6 hungry-pet food fix (branch `fix/hungry-food`, 2026-11-17).**`.
   - `python3 tools/release.py pointers --fix` re-points every `` `name` `file:N` `` and `` `name` `:N` `` to where
     `name` is defined now. Lines it prints as `check` (no definition found) need a look by hand. Pointers with no
     name in front (ranges like `TamaPoke.ino:90-390`) are not touched; fix them by hand if your change moved them.
   - If README.md's "Game manual" quotes a number or behaviour you changed, update it as well.
4. **Board check row**: `python3 tools/release.py row "<check>" "[CHANGELOG X.Y](../CHANGELOG.md)" "<result>"`.
   - Check: what to do and what should happen, as serial commands where possible (`NEED 0 10` → ...), EN and ES only
     (never other languages).
   - Result: what you verified, e.g. `build ✓, host tests ✓. Board: pending`.
5. **Build and test**:
   - `pio run -e tamapoke`. PlatformIO writes a lock in `~/.platformio`, which the sandbox blocks, so run pio commands
     with the sandbox disabled.
   - `./test/run_tests.sh` when `pet.cpp` or a pure header changed. A new header or test goes in `test/Makefile`.
   - `python3 tools/test_i18n_formats.py` when strings changed (every language row needs an entry).
   - `tools/render/render.sh` for screens that changed; look at the PNGs in `.pio/render/screens/`.
   - Major update / user asked only: `bash tools/build_web.sh` (also without the sandbox) to refresh
     `web/firmware/tamapoke.bin` and `web/sprites.pak`. Otherwise skip it and leave every `web/` file alone.
6. **Gate**: `python3 tools/release.py check` must print `OK`. It checks that FW_VERSION, the top CHANGELOG entry
   and the guide header (first 8 lines) agree, that no TODO is left, and that no pointer is stale. It also prints the
   web installer's version; behind FW_VERSION is expected between major updates.
7. **Commit**: one commit, `vX.Y: <what the player gets>`, with a body that says why. Include code, CHANGELOG,
   GAME_LOGIC.md, PENDING_TESTS.md, and the `web/` files only on a major update. End with the Co-Authored-By line from the system reminder.
8. **Merge and push**: the user allows a no-ff merge to main and a push without asking (sole developer):
   `git checkout main && git merge --no-ff <branch> -m "Merge <branch>: vX.Y <summary>" && git push origin main`.
   Push needs the sandbox disabled. Don't push tags (the user does that).
9. **Memory**: save or update one memory note: version, main merge commit, date, the PENDING_TESTS row, and any
   choices the user made.

## Report to the user

What changed and why, the version and commit, what was verified (build, tests, renders) and what wasn't (the board),
the new PENDING_TESTS row, and anything left uncommitted on purpose.
