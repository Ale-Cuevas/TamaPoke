# TamaPoke

## Game logic guide: `docs/GAME_LOGIC.md`

- Read `docs/GAME_LOGIC.md` first for how the game works and where each part lives (files, functions, `file:line`
  pointers, serial commands, tests). Open the source only for the parts the task touches; don't re-read the whole
  repo to rebuild the picture.
- **Any commit that changes game logic updates `docs/GAME_LOGIC.md` in the same commit**: the affected section,
  any pointers that moved, and the version + commit line at the top. If line numbers drift, prefer fixing them to
  leaving them stale.

## Balance numbers: `tuning.h`

- Every balance number (drain rates, timers, odds, rewards, prices) lives in `tuning.h`. New ones go there too, not
  as a literal in the code or a `#define` in a module.
- Keep values that size saved data (NVS blobs, item ids, the Box record) out of it: changing those needs a save
  migration.
- A test that checks a mechanism reads the value from `tuning.h`. A test that pins a design promise on purpose says
  so in a comment.

## Languages: English and Spanish only

The firmware has 8 languages (ES, EN, FR, DE, IT, PT, JA, KO), but only **English and Spanish** matter for now.

- Renders, layout checks and board test lists (`docs/PENDING_TESTS.md`, `docs/DEVICE_TESTS.md`) cover EN and ES only.
  Don't ask for checks in any other language on the board. French will never be of interest.
- New strings still need an entry in every language row of `i18n.cpp` (the table and
  `tools/test_i18n_formats.py` require it), but a plain translation is enough; don't spend time polishing them.
- Bugs that only affect other languages (for example Korean text not being centred) aren't worth raising or
  fixing unless asked.
