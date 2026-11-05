# Feature spec: feedback effects, the pet box and OTA updates

Written 2026-11-05, while the day-1 plan (v1.22–v1.27: pacing, brightness,
step tiers, meadow, biomes, burn-in) is being built. This page specifies three
more tasks. Each one is its own branch and release.

They are named **FX**, **BOX** and **OTA**, not numbered, so they don't clash
with Tasks 1–3 in [`FEATURES.md`](FEATURES.md) or the day-1 plan's 1–6.
Requirements are numbered (FR-FX.1, …) so commits and tests can point at them.
Every gameplay number is a starting value in one named constant.

The shared rules in `FEATURES.md` → *Rules every task follows* apply to all
three:
- no flash writes from motion or radio events;
- a new screen touches every UI hook;
- every string goes into all 8 languages;
- each task ships with a CHANGELOG entry, a version bump, README and test docs,
  renders, host tests and serial commands.

Code is referred to by function name, not line number: with several branches
in flight, line numbers move every day.

| Task | What | Needs | Size | Suggested release |
|---|---|---|---|---|
| [FX](#task-fx-feedback-for-what-the-pet-does) | Level-up sound and animation; feedback for stats, bond, training, hatching and buttons; one queue so celebrations never overlap | Best after day-1 Task 3 (Rare Candy) and Task 6 (screensaver) | M | 1.28 |
| [BOX](#task-box-store-a-pet-raise-another) | Put the pet into hibernation in a 6-slot box, raise a new egg, swap back any time | — (uses FX's overlay for its effect if FX is in) | M–L | 1.29 |
| [OTA](#task-ota-firmware-updates-over-wi-fi) | Update the firmware over Wi-Fi from GitHub Releases, with automatic rollback | — (shares the radio rules with the Link, `FEATURES.md` Task 3) | L | 1.30 |

**Suggested order: FX → BOX → OTA.**
- FX is the smallest, and the other two reuse its queue: BOX for the
  store/wake effect, OTA for the "Updated to vX" toast.
- BOX adds a per-pet `uid` that the Link's friend list can use later.
- OTA comes last because it brings in the radio and a release pipeline.

Moving OTA earlier only helps if other people's boards need updates soon. The
first firmware that can update over the air still has to be flashed over USB.

## Contents

- [Shared: the NVS budget](#shared-the-nvs-budget)
- [Task FX: feedback for what the pet does](#task-fx-feedback-for-what-the-pet-does)
- [Task BOX: store a pet, raise another](#task-box-store-a-pet-raise-another)
- [Task OTA: firmware updates over Wi-Fi](#task-ota-firmware-updates-over-wi-fi)

## Shared: the NVS budget

BOX and OTA both add keys to the 20 KB NVS partition (`nvs`, 0x5000).

**Space.**
- NVS keeps one of its 5 pages free for garbage collection. That leaves about
  **500 entries** of 32 bytes.
- A number takes 1 entry. A string or blob takes 2 or more, depending on its
  length.

**Estimated use.**
- Today: about 70 entries. `Pet::save()` writes ~57 keys, and sound, language
  and brightness add a few.
- BOX: about +24 (6 slots × ~4).
- OTA:
  - about +8 for the Wi-Fi name and password;
  - about +60 for the PHY calibration the radio writes the first time it starts
    (`FEATURES.md` 3b).

That leaves plenty of room, but nobody has measured it. **Whichever of BOX or
OTA lands first adds an `NVS` serial command.** It prints `nvs_get_stats()`
(used and free entries), and the numbers go into `PENDING_TESTS.md`.

---

## Task FX: feedback for what the pet does

### What's there today

| Event | Sound | Visual |
|---|---|---|
| Level up while awake (every 60 min of age) | `SFX_LEVEL` from `Pet::tick()`, **even with the screen off** | none |
| Level up while asleep, offline, or by the `LVL` command | none | none |
| Medal | `SFX_MEDAL` when earned, even with the screen off | 4 s banner. The timer starts when the medal is earned (`checkMedals()`), so a medal earned with the screen off is never seen |
| Streak milestone | none of its own | 4.5 s banner, timer started in `registerCare()` (same problem) |
| Step goal, found item | `SFX_MEDAL` | banners that wait for the screen to be on (`drawWalkNews()`): the good pattern |
| Ready to evolve | `SFX_MEDAL` (`loop()`) | the red button |
| Evolution | `SFX_EVOLVE` | 5.2 s halo, rays, flicker and flash (`drawEvolveFX()`): the quality bar for the rest |
| Hatching | `SFX_HATCH` | none: the egg is replaced by the pet in one frame |
| Feeding | `SFX_EAT` | 2.5 s eating animation; heart for the favourite berry |
| Petting | `SFX_HEART` | 1.5 s heart |
| Vitamins | `SFX_LEVEL` | none |
| Needs change (feed, clean, play, item) | — | the bars jump to the new value |
| Bond goes up | none | none |
| Care mistake | none | none |
| Action not allowed | `SFX_DENY` | none |
| Bottom buttons | `SFX_TAP` | no pressed state |

`SFX_LEVEL` and `SFX_MEDAL` are each used for four or more different events,
so the sound doesn't tell the player what happened.

### Goal

Every reward or change gets a visible and audible answer. Levelling up feels
like an event. Two celebrations never draw over each other, and none plays to
a dark screen.

### Requirements

**Level up**
- **FR-FX.1** Every time `level()` goes up, the home screen plays the level-up
  effect once, whatever caused it:
  - a tick, awake or asleep;
  - the offline catch-up at boot;
  - the Rare Candy (day-1 Task 3);
  - the `LVL` serial command.
- **FR-FX.2** Several levels at once (offline time, Rare Candy) play **one**
  effect that reads "Lv 12 → 18", not six.
- **FR-FX.3** The effect waits until all of these are true:
  - the screen is awake (`DIM_AWAKE`, not the day-1 Task 6 screensaver);
  - no dialog, card, game, sack, gallery, clock or keyboard is open;
  - the pet isn't evolving, in a ceremony, or asleep.

  This is the same "busy" rule as `drawWalkNews()`. The wait is RAM only: if
  the board restarts first, the effect is dropped. The level itself is never
  lost.
- **FR-FX.4** The level-up animation, about 2.4 s:
  - the pet hops once (`PMD_HOP`; `PMD_POSE` if the sprite has no hop; a small
    bounce for the black-and-white fallback);
  - 12 sparkles rise around it;
  - a "LEVEL UP!" pill with "Lv 13" appears above its head;
  - the level number in the header flashes.

  Then, for 1.5 s, a smaller line shows the stat gains, e.g. "ATK +2 DEF +1
  SPE +2". That is `atkStat()` / `defStat()` / `speStat()` at the new level
  minus the old one.
- **FR-FX.5** The level-up sound is a new `SFX_LEVELUP`: a rising 4-note
  arpeggio of about 500 ms that doesn't sound like any other effect. It plays
  when the animation starts. The `sfxPlay(SFX_LEVEL)` in `Pet::tick()` goes
  away.

**One celebration at a time**
- **FR-FX.6** Celebrations queue and play one at a time, in this order:
  1. level up
  2. ready to evolve
  3. medal
  4. streak milestone
  5. step goal
  6. found item

  Each banner's timer starts **when it is shown**, not when it is earned. For
  example, the Lv 10 medal is earned in the same tick as level 10: it now waits
  for the level-up to finish instead of drawing over it.

**Smaller feedback**
- **FR-FX.7 Needs bars.** When an action changes a need by 3 or more, the bar
  slides from the old value to the new one over 400 ms. A "+N" (green) or "−N"
  (red) floats up from it for 900 ms. The actions are feed, clean, play, using
  an item and the bath. The per-minute ticks don't trigger it.
- **FR-FX.8 Bond.** When bond goes up, a small heart with "+N" rises from the
  pet for 900 ms. At most one every 10 s. Petting already gives bond at most
  every 15 min since v1.22, so tapping the pet over and over shows nothing new.
- **FR-FX.9 Training.** Vitamins and the sack show "ATK +3" (or the stat that
  rose), and the pet plays one attack cycle (`PMD_ATTACK`).
- **FR-FX.10 Hatching.** A 1.2 s effect between the last crack and the pet:
  1. the egg shakes;
  2. shell pieces fly out;
  3. a white flash, and the pet appears with `PMD_POSE`.

  `SFX_HATCH` plays on the flash.
- **FR-FX.11 Not allowed.** The tapped button shakes sideways (±4 px, 3 times,
  300 ms) with `SFX_DENY`.
- **FR-FX.12 Buttons.** A tapped bottom button looks pressed (darker, 2 px
  lower) for 120 ms.
- **FR-FX.13 Care mistakes.** A soft, falling 2-note `SFX_OOPS` and a "!"
  bubble over the pet for 2 s. This only happens with the screen awake.
  Otherwise it stays silent: the profile already counts mistakes.
- **FR-FX.14 Banners.** Banners slide in from above over 200 ms instead of
  appearing all at once. This covers the medal, milestone, goal and found item
  banners and the level-up pill. Medals and the 10000-step goal also get a
  1.5 s confetti burst (32 particles at most).

**Rules for all effects**
- **FR-FX.15** With sound off (clock screen), every sound is silent but the
  animations still play. With the pet asleep, there are no sounds, as today.
- **FR-FX.16** All effects together add **≤ 3 ms** per frame, measured with the
  `PERF` command from day-1 Task 4. They use fixed arrays and allocate nothing
  per frame.

### Design

**`fx.h`: pure, header-only, host-tested** (like `power.h`)
- **Banner queue.** A fixed queue of 8 `FxEvent { kind, a, b }` for the
  celebrations: `FX_LEVELUP`, `FX_EVO_READY`, `FX_MEDAL`, `FX_MILESTONE`,
  `FX_GOAL`, `FX_FOUND`.
  - `fxPush()` merges level-ups: 12→13 followed by 13→14 becomes 12→14. It
    also drops duplicates.
  - `fxStep(now, busy)` returns the current event and how far it has played (t
    from 0 to 1). It starts the next event's timer only when the screen isn't
    busy.
- **Overlays.** A separate small array for effects that can play during a
  banner, each with its own timer: `FX_STAT`, `FX_BOND`, `FX_TRAIN`, `FX_DENY`,
  `FX_PRESS`, `FX_OOPS`.
- **`LevelWatch`.** Remembers the last level seen. It is set up in `setup()`
  **before** `pet.syncClock()`, so levels gained while the board was off show
  on the first awake frame. Each frame it compares `pet.level()` and pushes
  `FX_LEVELUP(from, to)` on a rise.
  - This catches every source (tick, offline, candy, `LVL`) without editing
    each one.
  - The save doesn't change.

**Changes in `Pet`**
- Medals and milestones become flags the UI collects, like the existing
  `takeGoalBanner()`:
  - Add `takeMedal()`, which returns the `newMedal` bits, and
    `takeMilestone()`.
  - Remove `medalUntil` and `milestoneUntil`. `showMedal()` and
    `showMilestone()` move into the UI.
  - The `sfxPlay(SFX_MEDAL)` in `checkMedals()` moves to when the banner shows.
- Two small values for the UI to read and clear:
  - `bondGain`, set in `addBond()` and `registerCare()`;
  - `mistakeNew`, set in `tick()`.

**Needs bars.** The UI keeps `shownNeed[4]` (fixed point) and moves it a little
toward the pet's real value each frame. The action handlers call
`fxStat(i, delta)`:
- feed, clean and play in `onTap()`;
- item use in `walkTap()`;
- the end of the bath.

**Drawing.** `drawFx()` runs after `drawCelebration()` in the home branch of
`render()`.
- It only uses `fillRect`, `fillCircle`, `drawLine` and the existing `drawMap`
  icons: no new sprite format.
- Sparkles reuse `drawEvolveFX()`'s 5×5 gold and white sparks.
- Effects only play on the home screen. It is redrawn every frame, so
  `screenKey()` doesn't change.

**Sounds.** These are square tones in the `N_*` tables in `audio.cpp`. New ids
go at the end of the `Sfx` enum, which follows the table order.

| Id | Notes (Hz, ms) | Use |
|---|---|---|
| `SFX_LEVELUP` | 784/60, 988/60, 1175/60, 1568/240 | level up. Higher than `SFX_HATCH` and `SFX_EVOLVE`, which start at 523 |
| `SFX_OOPS` | 392/90, 330/160 | care mistake. Softer than `SFX_DENY` (300 → 200) |
| `SFX_LEVEL` | unchanged | stays as the small "+" sound for items and the end of the minigame |

**Renders.** `fxDebug(kind, t)` freezes an effect at a given t, like
`clearEmote()` does for emotes, so `tools/render` scenes are repeatable.

### Interplay with the day-1 plan

- **Task 1, pacing (v1.22):** bond popups only appear when bond actually rises,
  so the petting cooldown shows as no popup.
- **Task 2, brightness toast (v1.23):** stays as it is.
- **Task 3, Rare Candy (v1.24):** its +N levels reach `LevelWatch` on their
  own. Its "bigger banner with a medal sound" for 10000 steps becomes `FX_GOAL`
  with the tier as its argument.
- **Task 6, screensaver (v1.27):** the busy rule includes the screensaver, so
  effects wait for the wake.

### Constants (starting values)

| Constant | Value | Meaning |
|---|---|---|
| `FX_LEVELUP_MS` | 2400 | level-up animation |
| `FX_GAINS_MS` | 1500 | stat-gain line after it |
| `FX_FLOAT_MS` | 900 | "+N" floats (needs, bond) |
| `FX_TWEEN_MS` | 400 | needs bar slide |
| `FX_STAT_MIN` | 3 | smallest change that shows a float |
| `FX_BOND_GAP_MS` | 10000 | at most one bond float per gap |
| `FX_HATCH_MS` | 1200 | hatching effect |
| `FX_BANNER_IN_MS` | 200 | banner slide-in |
| `FX_CONFETTI` | 32 | confetti particles |
| `FX_QUEUE` | 8 | banner queue length |

### Acceptance and tests

**Host:** a new `test/test_fx.cpp`, added to `test/Makefile`.
- Level-ups merge.
- Banners play in priority order.
- A timer starts when its banner shows, not when it's pushed.
- While busy, the queue waits.
- Overlay timers don't depend on each other.
- A `LevelWatch` set up before an offline jump reports one event.

**Host:** `test/test_pet.cpp`.
- `takeMedal()` returns the Lv 10 medal exactly once, and nothing in `Pet`
  times it any more.
- `bondGain` is set by petting (respecting the cooldown) and by the first care
  of the day.
- `mistakeNew` is set when a care mistake is counted.

**Renders:**
- level-up at t = 0.3 and 0.8;
- "Lv 12 → 18";
- the stat-gain line;
- +N floats on the bars;
- hatching halfway through;
- the medal banner sliding in with confetti;
- a frame of the button shake.

**Board** (`DEVICE_TESTS.md`, "Feedback"):
- `LVL 10`: the level-up plays, then the Lv 10 medal, one after the other.
- Power off for 3 h, then on: "Lv a → b" plays once.
- Screen off when a level is reached: the effect plays when the screen wakes,
  with no sound in the dark.
- Sound off: animations play silently.
- `PERF` with the level-up playing: no more than 3 ms above normal.

**Serial:** `FX <kind>` triggers any effect on the board.

### Open questions

- A level reached with the screen off: play the sound right away (a chime in
  the pocket) or wait for the screen? The spec says wait.
- Is the stat-gain line worth it? It makes the numbers on the stats card mean
  something.
- Confetti on every medal, or only the rare ones?

---

## Task BOX: store a pet, raise another

### Goal

The player can put the current pet into **hibernation** in a box of 6 slots,
start a new egg, and later swap any stored pet back.

Stored pets are frozen. They don't age or get hungry, and they don't pick up
care mistakes. They can't run away or reach farewell age.

### How the save splits today

`Pet` (`pet.h`) mixes three kinds of state. BOX moves only the first.

| Kind | Fields | In a swap |
|---|---|---|
| **The individual** | `speciesId`, `shiny`, `nick`, `ageMinutes`, `fullness`, `joy`, `energy`, `hygiene`, `poops`, `weight`, `geneAtk/Def/Spe`, `trAtk/Def/Spe`, `berryKnown`, `careMistakes`, `sleeping`, `bond`, `bondToday`, `bondDay`, `medals` | goes into the box |
| (individual, RAM only today) | `mistakeCooldown`, `neglectTicks`, `goodTicks`, `petBondUntil`, `evoDeclinedLv`, `farDeclinedAge` | reset on wake, as they are on a reboot |
| **The player** | `dexReg`, `dexShinyReg`, `streak`, `bestStreak`, `lastCareDay`, `lastMilestone`, `totalMedals`, `gameHi`, `strHi`, the step fields (`stepsToday` … `goalDay`, Task 3's `goalTier`, the remainders), `items`, `lastEnd`, `lastSeenEpoch` | stays |
| **The next egg** ("incubator") | `eggTarget`, `eggShiny`, `eggTaps` | stays |

### Requirements

- **FR-BOX.1** The box has `BOX_SLOTS` = 6 slots. Each slot shows:
  - the pet's thumbnail;
  - its name (nickname or species);
  - its level;
  - a star if it's shiny.

  Stored pets look asleep: dimmed, with a "z".
- **FR-BOX.2 Store.** With a hatched pet active, tap an empty slot and confirm
  "Store <name>?". The pet goes into that slot and an egg takes its place.
  - Storing isn't possible during an evolution, a ceremony, the minigame or the
    sack, or with a dialog open.
  - A sleeping pet can be stored, and it comes back asleep.
- **FR-BOX.3** The egg that replaces it is the held egg if there is one
  (FR-BOX.5). Otherwise `newEgg()` rolls one. Storing is not an ending:
  `lastEnd` keeps the last real ending (farewell, runaway or release).
- **FR-BOX.4 Wake.** Tap a stored pet and confirm "Wake <name>?". When the
  active pet is hatched, the dialog adds "<current> goes into the box". The two
  swap places in that slot. Since it's one tap, a full box can still swap.
- **FR-BOX.5** Waking a pet while an egg is active **holds the egg as it is**:
  species, shiny roll and cracks. It comes back as the next egg after the next
  store or ending.

  This way storing and waking can't re-roll eggs: each egg is rolled exactly
  once. That includes the shiny charm, which is spent when the egg is rolled.
- **FR-BOX.6** Hibernation is total. Nothing changes in the box, including
  during the offline catch-up: `syncClock()` only applies to the active pet, as
  today. Farewell age and evolution level only count time spent active.
- **FR-BOX.7** Player rewards stay player-wide: streak, steps, finds, goals,
  the Backpack, records and the Pokédex.
  - The first care of the day (+4 bond in `registerCare()`) goes to whichever
    pet is active at that moment, once a day per player. Swapping can't collect
    it six times.
  - Each pet keeps its own daily bond cap (`bondToday` and `bondDay` move with
    it).
- **FR-BOX.8** Medals stay with the individual. `totalMedals` stays
  player-wide; it is already counted when a medal is earned.
- **FR-BOX.9 A pet is never lost.**
  - A power cut at any moment of a store or a wake leaves, at worst, the same
    pet in two places. The next boot cleans that up (see *Power-cut safety*).
  - A damaged slot (bad CRC or unknown version) shows as "?". It is never
    loaded, and a store never overwrites it. Only `WIPE` or `BOXCLR n` clears
    it.
- **FR-BOX.10** Farewell, runaway and release only apply to the active pet. To
  release a stored pet, wake it first.
- **FR-BOX.11** Old saves load as they are, with an empty box. The current pet
  gets a `uid`.
- **FR-BOX.12** A store or wake writes to flash right away, like feeding does,
  since it's a user action on an awake screen. A 0.6 s effect covers the ~1 s
  write so it doesn't feel like a hang: the pet shrinks into a light that flies
  into the slot, or out of it. If FX is in, this is an FX overlay.

### Design

**Storage: NVS, not the SD card.**
- Keys `box0` … `box5` in the `tamapoke` namespace. Each is a `putBytes` of a
  packed `PetRecord` of at most 64 bytes:
  - a version number and the `uid`;
  - every "individual" field from the table above;
  - `storedEpoch` (when it went in);
  - a CRC-16.

  That's about 4 NVS entries per slot.
- **Why not the SD:** the card can be swapped or reformatted (the web loader
  rewrites it), and the active pet already lives in NVS. The box is written
  only on a store or a wake, so wear doesn't matter.
- The active pet keeps its current keys, so nothing needs migrating. `Pet`
  gains `toRecord()` and `fromRecord()`.
- **New keys:**
  - `uid` (uint32, rolled at hatch; given on load if missing). It drives the
    duplicate cleanup and can later identify pets in the Link's friend list
    (`FEATURES.md` FR-3.11).
  - `eggH` (bool): an egg is being held.

**Power-cut safety.** Every move first copies the pet being moved into a
transfer key `boxT`. Then it writes, and removes `boxT` last.

| Move | Writes, in order |
|---|---|
| Store A into empty slot k | 1. `box k` = A · 2. active = held egg or `newEgg()`, `save()` |
| Wake B from slot k, active pet A | 1. `boxT` = B · 2. `box k` = A · 3. active = B, `save()` · 4. remove `boxT` |
| Wake B from slot k, active is an egg | 1. `boxT` = B · 2. `eggH` = true · 3. active = B, `save()` · 4. clear `box k` · 5. remove `boxT` |

At boot, before anything else:
1. **If `boxT` exists:**
   - If the active pet's uid equals `boxT`'s, the wake already happened: just
     remove `boxT`.
   - Else, if some slot holds the active pet's uid, step 2 of a swap happened:
     load `boxT` as the active pet, then remove `boxT`.
   - Else nothing moved: remove `boxT`. If the active pet is an egg, set
     `eggH` = false.
2. **Then:** any slot holding the active pet's uid is cleared. That undoes a
   store that was cut after step 1; the player just stores again.

A host test cuts the power after every single write of every move (FR-BOX.9).

**UI**
- **Where the box is.** It becomes a **6th card page**, after Walk
  (`CARD_PAGES` 6).
  - Swiping up on the egg screen does nothing today. With BOX it opens the
    card straight on the Box page, with the other pages hidden. That way a
    stored pet can be woken while an egg is active.
- **Layout** inside the round frame:
  - "Box 3/6" at the top;
  - a 3×2 grid of 120×110 cells, each with the thumbnail (`thumbs.bin` through
    `drawThumb()`), the name and "Lv 14";
  - "Now: <name>" below the grid.
- **Taps.** An empty slot asks to store; a full slot asks to wake. Both use the
  same dialog style as "release?" and the item confirm (YES/NO).
- **After a swap:**
  - reload the sprites (`pmd.unload()` then `ensureMon()`);
  - reset the pet's wandering (`beh`);
  - close the card;
  - play the effect.
- **UI hooks** (the `FEATURES.md` checklist): the `render()` chain, `onTap()`,
  `onBack()`, both swipes, the long-press guard, `staticScreen()` /
  `screenKey()`, `powerCtx()`, `resetUi()`, and a render scene. The slots'
  contents and the open dialog go into `screenKey()`.
- **Strings**, in all 8 languages:
  - `S_BOX_FMT` ("Box %u/%u")
  - `S_STORE_Q_FMT` ("Store %s?")
  - `S_WAKE_Q_FMT` ("Wake %s?")
  - `S_SWAP_NOTE_FMT` ("%s goes in the box")
  - `S_NOW_FMT` ("Now: %s")
  - `S_BOX_EMPTY`, `S_BOX_DAMAGED`
- **Serial commands:**
  - `BOX` lists the slots (uid, species, level, CRC ok);
  - `STORE <k>`, `WAKE <k>`;
  - `BOXFILL` fills empty slots with random pets, for renders and tests;
  - `BOXCLR <k>`;
  - `NVS`, if OTA hasn't added it.

### Constants (starting values)

| Constant | Value | Meaning |
|---|---|---|
| `BOX_SLOTS` | 6 | slots in the box |
| `PET_RECORD_VER` | 1 | record layout version |
| `BOX_FX_MS` | 600 | store/wake effect |

### Acceptance and tests

**Host:** `test/test_pet.cpp`, with the Preferences shim.
- **Round trip:** a record keeps every individual field, and a swap leaves
  every player field untouched.
- **Store:** an egg takes the pet's place. A held egg is reused and the charm
  isn't spent twice. `lastEnd` doesn't change.
- **Wake with an egg active:** the egg is held, cracks included. After the next
  store, the same egg (`eggTarget`, `eggShiny`, `eggTaps`) comes back.
- **No re-rolling:** storing and waking 20 times never changes `eggTarget`.
- **Frozen:** across a 2-day `syncClock()`, a stored pet keeps its age and
  stats while the active one ages.
- **Bond:** the first care's +4 comes once a day across swaps; the daily cap is
  per pet.
- **Power cut:** the shim stops after write n, for every n of a store and of a
  swap. After the "boot", no pet is lost and no duplicate remains.
- **Damaged slot:** a bad CRC shows as damaged, is never loaded, and isn't
  overwritten by a store.
- **Full box:** storing is impossible, swapping still works.
- **Old save:** no `uid` and no box keys → loads with an empty box and gets a
  `uid`.

**Renders:**
- the box empty, with 3 pets, full, and with a damaged slot;
- the store dialog, and the wake dialog with the swap note;
- the Box page opened from the egg screen;
- the store effect halfway through.

**Board:**
- store and wake with the real sprites;
- a power cut during the effect (hold PWR);
- `STATS` for both pets before and after;
- the `NVS` numbers.

### Open questions

- 6 slots? NVS has room for more (about 4 entries each).
- Should a stored pet show how long it has been asleep (`storedEpoch`)?
- A small "missed you" effect on waking (e.g. joy −10)? The spec says none:
  hibernation is a pause.
- Release straight from the box, without waking the pet first?
- A card page, or a screen of its own?

---

## Task OTA: firmware updates over Wi-Fi

### What the board already has

Checked 2026-11-05 against `platformio.ini`, the core 3.3.11 sdkconfig and the
v1.21 build.

| What | State | Where |
|---|---|---|
| Two 3 MB app slots + `otadata` | **yes** | `app3M_fat9M_16MB.csv`, the same table in the arduino-cli FQBN and the web image |
| Firmware size | 0.89 MB (v1.21), ~2.1 MB free per slot. Wi-Fi and HTTPS are estimated at +0.6–0.9 MB: measure | `.pio/build/tamapoke/firmware.bin` |
| Rollback in the bootloader | **on**: `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y` | `framework-arduinoespressif32-libs/esp32s3/sdkconfig` |
| Arduino rollback hooks | weak `verifyRollbackLater()` / `verifyOta()`, called from `initArduino()` | `cores/esp32/esp32-hal-misc.c` |
| HTTPS certificate bundle | supported (`CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y`). `NetworkClientSecure::setCACertBundle(data, size)` needs the sketch to supply the bundle bytes | `NetworkClientSecure.h` |
| HTTPS memory | internal RAM only (`CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC=y`); Wi-Fi buffers aren't in PSRAM either | sdkconfig |
| Static internal RAM | ~35 KB (DRAM data + bss), 77 KB IRAM | `firmware.elf` |
| Task watchdog | on, 5 s; the loop task isn't watched unless `enableLoopWDT()` is called | sdkconfig |
| Pet save, sprites | NVS and SD: a firmware update touches neither | — |
| `ffat` partition (9.9 MB) | unused | could hold a downloaded sprite pack later |

**The partition table and the bootloader can't be changed over the air.** The
current table is already ready for OTA, so keep it. Changing it would mean one
more USB flash for every board.

The web installer flashes the merged factory image at 0x0. An OTA update needs
the **app-only** `firmware.bin` instead.

### Goal

A board on v1.30 or later updates itself from GitHub Releases over Wi-Fi:
- started from the settings (clock) screen;
- without USB;
- keeping the pet.

If the new firmware fails to start, the board returns to the previous one by
itself.

### Requirements

**Wi-Fi setup**
- **FR-OTA.1** The player sets up Wi-Fi once, from the web installer. After
  flashing, ESP Web Tools offers "Connect to Wi-Fi" through **Improv Wi-Fi over
  serial**, which the firmware implements. Nobody types a password on the
  board; the nickname keyboard only has letters anyway.
- **FR-OTA.2** Serial commands `WIFI <ssid> <password>` and `WIFI FORGET`, for
  development.
- **FR-OTA.3** The network name and password are stored in our own keys
  (namespace `net`).
  - `WiFi.persistent(false)` stops the Wi-Fi driver from writing its own copy
    into the 20 KB NVS.
  - The settings screen shows the network name and a "Forget Wi-Fi" option.

**Checking and installing**
- **FR-OTA.4** The clock screen already shows "TamaPoke vX". It gets an
  **Update** pill. A tap turns Wi-Fi on and fetches the manifest. The screen
  then shows either:
  - "Up to date"; or
  - "v1.31 available · 1.4 MB · Install?", with the release's one-line note.
- **FR-OTA.5** Nothing is automatic in v1: no background checks, and no install
  without a tap.
- **FR-OTA.6** Installing needs:
  - USB power, or a battery at 40 % or more;
  - the pet not evolving or in a ceremony;
  - the minigame closed.

  If one is missing, the pill says why.
- **FR-OTA.7** `flushSave()` runs before the radio starts. The rule "no flash
  saves while the radio is on" stays.
- **FR-OTA.8** The progress screen shows a bar, the percentage and "Don't turn
  it off".
  - The USER key cancels at any point before the final switch, and the current
    firmware stays.
  - Sounds are muted during the download.
- **FR-OTA.9** Checks before switching:
  - the manifest's board id matches this build (`amoled216`; the 1.75 build
    from `FEATURES.md` 3e gets its own);
  - the version is newer (no downgrades, except with `OTA FORCE`);
  - the image fits the slot;
  - the **SHA-256** of the downloaded bytes matches the manifest.

  Only then `Update.end()` and the switch. Otherwise `Update.abort()` and an
  error message.
- **FR-OTA.10** After the switch: radio off, save, restart. The new firmware
  shows "Updated to v1.31" once: it compares a saved `fwv` key with
  `FW_VERSION`. If FX is in, this goes through its queue.
- **FR-OTA.11** Every error is a message, never a stuck screen, and leaves the
  running firmware untouched. The errors:
  - no Wi-Fi set up;
  - network not found;
  - wrong password;
  - no internet;
  - certificate error;
  - server error;
  - no data for 20 s;
  - checksum mismatch;
  - write error;
  - battery low.
- **FR-OTA.12** Checking takes at most 10 s, and installing at most 2 min on
  home Wi-Fi.

**Safety**
- **FR-OTA.13 Rollback.** A new firmware is kept only after it proves it works.
  - It overrides `verifyRollbackLater()` to return true.
  - It calls `esp_ota_mark_app_valid_cancel_rollback()` once the display and
    touch are up, the save has loaded, and 30 s have passed on the home screen.
  - During those 30 s, `enableLoopWDT()` makes a hang reset the board too.

  Any reset before that point (a crash, the watchdog, or the player holding
  PWR) makes the bootloader go back to the previous firmware.
- **FR-OTA.14 HTTPS with certificate checks**, never `setInsecure()`.
  - The root certificates of GitHub's download hosts are built in as a small
    bundle.
  - A tool generates the bundle into a header, the way `gen_dex.py` makes
    `dex.h`, so both the pio and arduino-cli builds get it.
  - The bundle is refreshed with each release.
- **FR-OTA.15** An update keeps the pet, the box, the settings and the SD
  sprites. If a firmware needs a newer sprite pack, its manifest says so
  (`min_sprites`). The board then warns "This update needs new sprites: use the
  web installer" before installing.
- **FR-OTA.16** Wi-Fi is on only while the update screen is open;
  `WiFi.mode(WIFI_OFF)` runs when it closes. One radio user at a time: the
  update screen won't open while the Link (`FEATURES.md` Task 3) is on, and the
  Link won't start during an update.

**Release side**
- **FR-OTA.17** Pushing a tag `vX.Y` runs a CI release job. It builds with pio
  and attaches three files to the GitHub Release:
  - `tamapoke-amoled216.bin`, the app-only `.pio/build/tamapoke/firmware.bin`;
  - `ota-amoled216.json`, the manifest;
  - the factory image for the web installer.
- **FR-OTA.18** The manifest:
  `{"board":"amoled216","version":"1.31","size":1468000,"sha256":"…","url":"https://github.com/Ale-Cuevas/TamaPoke/releases/download/v1.31/tamapoke-amoled216.bin","min_sprites":1,"notes":"…"}`.
  The board fetches it from the fixed link
  `releases/latest/download/ota-amoled216.json`, so publishing a release is the
  whole rollout.
- **FR-OTA.19** Two serial commands for testing:
  - `OTA URL <url>` points the board at another manifest (a local test server)
    until the next reboot;
  - `OTA STATE` prints the running slot, its state, and the version in the
    other slot.

### Design

**Files**
- `ota.cpp` / `ota.h`: hardware glue. The sketch includes only `ota.h`, so
  `WiFi.h` stays inside the `.cpp` (the `FEATURES.md` rule).
- `ota_policy.h`: pure and host-tested. It holds the version compare, the
  manifest parser, the install conditions and the step machine (idle → Wi-Fi →
  manifest → download → verify → switch, plus an error state).

**Download in steps from `loop()`**, not one blocking call.
- `HTTPClient` follows redirects, since GitHub's download links redirect to its
  storage host. `NetworkClientSecure` checks the certificate.
- Each loop pass reads up to 4 KB and feeds it to `Update.write()` and to an
  mbedTLS SHA-256.
- The UI keeps drawing the bar and the USER key keeps working.
- `HTTPUpdate` isn't used: it does everything in one blocking call and only
  checks MD5.

**Manifest parser.** A small flat-JSON reader in `ota_policy.h`, so no new
library. Host-tested on good and broken manifests.

**Memory.** HTTPS needs roughly 40–50 KB of internal RAM, and the Wi-Fi driver
more.
- Check `heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)` before
  starting. If it's too small, say "not enough memory" instead of failing
  halfway.
- Record the minimum during a download in `PENDING_TESTS.md`.

**Improv.** Improv packets start with the bytes `IMPROV`. `handleSerial()`
spots them before the line parser and the SD `PUT` protocol see them. The
installer's `web/manifest.json` gets the Wi-Fi step.

**Versions.** `FW_VERSION` ("1.23") is compared as major and minor integers,
so 1.9 < 1.10.

**First radio start.** It writes the PHY calibration into NVS: about 2 KB and
one ~1 s freeze, once (`FEATURES.md` 3b). This happens on the "Checking…"
screen, before any download.

### Phases

- **OTA-a (this task):** everything above.
- **OTA-b (later):**
  - **sprite updates over Wi-Fi:** download changed sprite files to the SD.
    The 58 MB pack takes 8–10 min over USB; Wi-Fi should manage it in 1–2;
  - **a daily check while charging**, shown as a dot on the clock screen;
  - **an SD-card update:** a `firmware.bin` copied to `/update/` is applied at
    boot, for players without Wi-Fi.

### Decided, not open

**No secure boot or signed images.** They burn eFuses: a one-way change that
would also block flashing your own builds over USB.

The bar for v1 is:
- HTTPS with certificate checks;
- the SHA-256 from the manifest;
- a fixed manifest link.

### Acceptance and tests

**Host:** `test/test_ota.cpp`.
- **Versions:** 1.9 < 1.10, equal versions, malformed strings.
- **Manifest:** a missing field, the wrong board, an image too big for the
  slot.
- **Install conditions:** battery level, USB, an open ceremony.
- **Step machine:**
  - cancel at every step;
  - timeout;
  - a checksum mismatch aborts;
  - `end()` is never called before the check.

**Board** (`DEVICE_TESTS.md`, "OTA"):
- **Setup:** Improv from the web installer sets up Wi-Fi; `WIFI FORGET` clears
  it.
- **Normal update:** vA → vB from a real GitHub Release. `STATS` matches before
  and after: pet, box, steps, Backpack.
- **Cancel** at 50 %: still on vA, boots fine.
- **Power cut** at 50 % (hold PWR): still on vA.
- **Bad checksum** (manifest edited on a local server, through `OTA URL`):
  refused.
- **Crash build** (`-D OTA_TEST_CRASH`, panics 5 s after boot): back on vA by
  itself.
- **Hang build** (`-D OTA_TEST_HANG`): the watchdog fires and the board goes
  back to vA.
- **Network errors** (wrong password, router off, router without internet):
  the right message for each.
- **Battery** under 40 % and unplugged: refused, with the reason.
- **Heap:** after 20 check/cancel cycles, `HEALTH` shows the free heap back at
  its starting value.
- **Record:** firmware size, and the lowest free internal heap during HTTPS.

### Open questions

- GitHub Releases, or GitHub Pages next to the web installer? Releases give
  versioned files and a stable "latest" link. Pages is the same host as the
  installer.
- Is 40 % the right battery threshold?
- Should OTA-b's daily check while charging come in v1 after all?
