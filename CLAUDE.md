# TamaPoke

## Languages: English and Spanish only

The firmware has 8 languages (ES, EN, FR, DE, IT, PT, JA, KO), but only **English and Spanish** matter for now.

- Renders, layout checks and board test lists (`docs/PENDING_TESTS.md`, `docs/DEVICE_TESTS.md`) cover EN and ES only.
  Don't ask for checks in any other language on the board. French will never be of interest.
- New strings still need an entry in every language row of `i18n.cpp` (the table and
  `tools/test_i18n_formats.py` require it), but a plain translation is enough; don't spend time polishing them.
- Bugs that only affect other languages (for example Korean text not being centred) aren't worth raising or
  fixing unless asked.
