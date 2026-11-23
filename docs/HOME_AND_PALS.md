# Feature spec: home upgrades and pet friendships

Written 2026-11-22 from a design interview, while main is at **v3.14** (the Lure). This page specifies two
tasks. Each one is its own branch and release:

| Task | What | Size | Order |
|---|---|---|---|
| [HOME](#task-home-permanent-upgrades) | Expensive permanent upgrades in a new Shop tab: **beds**, a **gym set** and a **toy**, 3 tiers each, drawn in the yard; pets sleep, train and play with them on their own | L | **1st** |
| [PALS](#task-pals-the-two-pets-interact) | Light, charming autonomous scenes between the two home pets: greetings, play, care, sleeping together and harmless squabbles, with small perks | L | **2nd** (its scenes reuse HOME's objects) |

Requirements are numbered (FR-HOME.1, FR-PALS.1, …) so commits and tests can point at them. **Every number in
this page is a starting value**: it goes into `tuning.h` under the named constant and is expected to move after
board testing. Code is referred to by function name; see `docs/GAME_LOGIC.md` for where each one lives.

## Rules both tasks follow

- **Backward compatibility with old saves is not required.** The user is the only player and will start a new
  save when these tasks ship. Don't write save migrations, don't keep old blob layouts readable, and don't keep
  older firmware able to read the new save. A save layout can change freely (bump its version and let the load
  treat an old blob as missing). The save code must still be correct: version + CRC checks, defaults for a
  missing key, no crash on a bad blob.
- Read `docs/GAME_LOGIC.md` first (§4 tick and sleep, §7 yard, §11 coins and Shop, §13 persistence, §14 FX
  queue). Update it in the same commit as the logic, with the version + commit line at the top.
- Every balance number goes in `tuning.h`. Values that size saved data (blob layouts, tier counts, item ids) do
  not.
- Every new string gets a row in all 8 languages in `i18n.cpp` (`tools/test_i18n_formats.py` checks it). Only
  EN and ES need care; the others can be a plain translation.
- No flash writes from per-frame yard logic. Pet logic only marks a pending save; purchases save at once (like
  `buyItem`/`buyMap`).
- Host tests read mechanism values from `tuning.h`. A test that pins a design promise on purpose says so in a
  comment.
- Each task ships with a CHANGELOG entry, a version bump, renders in EN and ES, `PENDING_TESTS.md` rows (EN/ES
  only), serial commands, and the `tamapoke-release` flow. Web installer only if the release is called major.
- Box pets are hibernating: nothing in this page affects them, and they don't count toward anything.
- Agents: the cross-cutting logic (HOME's tick effects and `upg` key, PALS's scene director) goes to an
  `implementer` on **Opus**; sprites, Shop UI and yard animation can go to Sonnet. Run the `reviewer` before merging each task.

---

## Task HOME: permanent upgrades

### Goal

Give coins a long-term goal. Three lines of furniture, each with 3 tiers, bought once and kept forever
(player-wide, they survive every pet, ending and map change). They show up as objects in the yard, and the pets
use them on their own. The effects run in `Pet::tick`, so they work with the screen off, in the pocket and at
night; the yard only shows the pets using them when it is on screen.

### The three lines

| Line | Tier 1 | Tier 2 | Tier 3 | Effect (per tier) | Price (coins) |
|---|---|---|---|---|---|
| **Beds** (2 beds, one per home slot) | Straw bed | Cushion bed | Royal bed | Faster ENE while asleep + good-morning JOY | 600 / 1000 / 1500 |
| **Gym set** (shared) | Log weights | Dumbbells + punching bag | Full gym (weights, bag, track) | Self-training of ATK/DEF/SPD up to a ceiling | 800 / 1200 / 1800 |
| **Toy** (shared) | Rubber ball | Ball + tug rope | Swing set | Solo play JOY + used in PALS play scenes | 500 / 800 / 1200 |

Total for everything: 9 400 coins, about a week per tier with the 150/day game cap plus uncapped sources
(walk goals, trips, medals, first stage clears). Tiers are bought in order: tier 2 requires tier 1.

### FR-HOME.1 Beds

- Owning bed tier *t* raises the ENE gain while asleep (`sleepMinute`, today `SLEEP_ENE_GAIN` 6/min) to
  `BED_ENE_GAIN[t]` = **7 / 8 / 10**. It applies to naps and night sleep alike. The FOOD/JOY drain and floors are
  unchanged (the user chose not to touch them).
- **Good-morning bonus.** When a night sleep (`nightSleep`) ends with the 09:00 auto-wake and lasted at least
  `BED_MORNING_MIN_SLEEP` **360 min**, the pet gets `BED_MORNING_JOY[t]` = **+5 / +10 / +15 JOY**. Once a night per
  pet. Not paid for a nap, a manual early wake, or a grumpy shake wake. In the yard: a stretch/yawn gesture and a
  small sun emote, queued on the FX queue like other banners.
- One purchase covers both home slots: the yard shows **two matching beds side by side**, the left one for
  slot 0, the right one for slot 1. Side by side on purpose: in PALS, two pets in bed *is* "sleeping together".
- Yard behaviour: when a pet falls asleep with the yard on screen, it walks to its bed and lies down there (the
  sleep itself starts at once in the logic; the walk is cosmetic, at most `BED_WALK_MAX_MS` 4 s, else it
  teleports). A pet found asleep when the yard opens is drawn in its bed. On waking it steps out to the nearest
  free cell. Without a bed, today's behaviour stays.

### FR-HOME.2 Gym set (self-training)

- Every awake minute that qualifies adds 1 to a per-pet counter. At `GYM_EVERY_MIN[t]` = **60 / 45 / 30**
  counted minutes the pet gains **+1 training point** and loses `GYM_ENE_COST` **3 ENE** (with `dropTo`, floor 5).
- **Qualifies:** hatched home pet, awake, not between `SLEEPY_HOUR` and `WAKE_HOUR`, every need ≥ `GYM_NEED_MIN`
  **40**, not in a ceremony, dying, evolving, a game, a battle or a trip.
- **Which stat:** the lowest of `trAtk/trDef/trSpe` that is still below the ceiling (ties go ATK → DEF → SPD).
  One set trains all three, so a pet with no minigame training evens out over time.
- **Ceiling per tier:** `GYM_CAP[t]` = **25 / 40 / 60**. Self-training never raises a stat above it, and never
  lowers a stat already above it from the minigames. 100 stays reachable only through the minigames, vitamins
  and the rest.
- **Daily max per pet:** `GYM_DAY_MAX[t]` = **4 / 6 / 8** points. Counted per slot and keyed by the pet's `uid`,
  so a pet moved in from the Box starts fresh; resets at the calendar day change.
- It is not care: no `registerCare`, no bond, no coins.
- The minute counter is RAM-only (a reboot loses at most one partial point). Gains go straight into the
  training fields, which are already saved. Offline catch-up counts the same as the live tick (whatever
  §13's catch-up already does for the other per-minute effects).
- Yard behaviour (cosmetic): now and then a qualifying pet walks to the gym and does a short workout (bob,
  sweat drop); when a point is gained with the yard on screen, a "+1 ATK" style popup through the FX queue.
  Check which PMD sprite animations exist before designing the poses.

### FR-HOME.3 Toy (solo play)

- Each awake, hatched home pet with JOY < `TOY_JOY_BELOW` **70** has a `TOY_PLAY_CHANCE` **2 %** chance per minute
  to play with the toy: `TOY_JOY[t]` = **+3 / +4 / +5 JOY**, at most `TOY_DAY_MAX[t]` = **3 / 5 / 7** times a day
  per pet (same per-slot, per-uid daily counter as the gym). Not at night, not asleep, not during the blocking
  states listed in FR-HOME.2.
- Yard behaviour: the pet walks to the toy and plays with it (ball roll, swing). When the toy is busy, the other
  pet waits, or in PALS starts a "toy tug" squabble.
- Tier 3 (swing set) can be drawn bigger than tiers 1-2.
- PALS uses the toy in its play scenes (FR-PALS.3), purely visual.

### FR-HOME.4 Shop: a new "Home" page

- The Bag screen has no tab strip (since v3.4 the Shop is its own dock button), so **Home is a new page of the
  Shop**, reached by swiping past the item pages, titled "Home". One full-width card per line: the next
  tier's sprite, its name, one line of effect ("Energy +8/min asleep", "Trains up to 40", …) and the price. A line
  at tier 3 shows **MAX** and the tier 3 sprite.
- Buying: check in order *max tier*, then *enough coins*. Because the prices are high, a YES/NO confirmation
  ("Buy Cushion bed for 1000?") comes first. Then `spendCoins`, set the tier, save at once, and play a purchase
  celebration (FX queue). The next time the yard shows, the new object is there.
- When coins are short the price is grey, as in `shopCell` (`SHOP_PRICE_OFF`).
- Optional (nice to have): tapping an owned object in the yard shows a small bubble with its name and tier.
  The tap must never steal a pet tap.

### FR-HOME.5 Objects in the yard

- **Fixed spots per map.** Each of the 9 maps (meadow, quarry, forest, beach, ruins, volcano, power plant, sky
  tower, toxic district) has a hand-picked spot for
  the **bed pair** (2 cells side by side), the **gym** and the **toy**. Recommended: a firmware table
  (`yard_spots.h`, keyed by map id, in cells) instead of a change to the TPY1 `.bin` format, so the SD maps don't
  need regenerating. Extend `tools/pack_yard.py`'s preview to draw the spots so they can be chosen visually.
- **Walk mask.** Owned objects block their cells in `yardWalkNow()`, like the egg and the wild-encounter cells.
  The spots must keep the walkable region connected and stay clear of the clock/dock and the circle edge. Add
  a host test that, for every map and every combination of owned objects, the region stays connected and both
  pets still have room. The yard egg and the encounter cells must avoid object cells.
- **Drawing.** Objects draw under the pets, sorted by y with them, tinted by the hour like the map
  (`yardTintAt`). One sprite per tier.
- **Art.** Pixel art drawn in code in `tools/sprites.py`, like `gift()` and `poop()`, with a palette per tier
  (straw → cushion → royal). Renders of each tier on each map go to the user for review before release.

### FR-HOME.6 Save data

- New NVS key `upg` (version byte + CRC): the 3 tiers (0-3), plus per slot `{uid, day, gymGains, toyPlays}`.
  Missing or unreadable key = everything at tier 0 (a new save starts with nothing). No migration path (see
  *Rules*).
- It is written when buying (at once) and with the normal pending save for the daily counters.
- No change to `PetLife`, `bpack`/`bpk2`/`bpk3` or the Box record.

### FR-HOME.7 Serial and tests

- Serial: `UPG` prints tiers and today's counters; `UPG <line> <tier>` sets a tier (debug).
- Host tests (`test/test_upgrades.cpp`): bed ENE gain per tier; morning bonus paid once, only after a full
  night ended by the auto-wake; gym rate, stat choice, ceiling, daily max, ENE cost, gating by needs/night/sleep;
  toy JOY and daily max; shop order (tier 2 refused without tier 1, MAX refused, not enough coins refused);
  `upg` round trip, missing key, bad CRC; the walk-mask connectivity test from FR-HOME.5.
- `PENDING_TESTS.md` rows (EN/ES): buy each line, objects appear on every owned map, pet walks to bed at
  night and is in bed on wake-up of the screen, morning bonus banner, gym workout + popup, toy play, Home tab
  layout.

---

## Task PALS: the two pets interact

### Goal

Make the yard feel alive when two pets share it: they greet each other, play, look after each other, sleep
together and now and then have a tiny, harmless tiff. **Always light**: a squabble never costs anything and
always ends in a make-up heart within seconds. Perks are small and never a must-do.

### FR-PALS.1 Where it runs

- **Only while the yard is on screen.** The user's choice: scenes and their perks happen only when someone is
  watching (no screensaver, no other screen open). Nothing happens in the pocket or offline. (HOME's effects
  are the opposite on purpose: they run in the tick.)
- Logic in a pure header, `pals.h` (like `yard.h`): a scene director that, given both walkers, the two pets' state
  and a seed, decides when a scene starts and drives it step by step. Deterministic and host-testable.
- Perks are applied through `Pet` methods with daily caps (FR-PALS.5), never directly from the yard.
- **Needs two hatched home pets.** With an egg in the other slot, only "egg-sitting" (FR-PALS.4) can happen.
  With a vacant slot, nothing.
- **Never starts while:** a pet menu is open or holding a pet (`yardMenuHold`), food is in flight (except the
  "same berry" squabble), a pet is evolving, hatching, in a ceremony or dying, a wild encounter or traveller
  screen is open, any modal is open, or the board is tilted (pets sliding). A tap on a pet during a scene
  cancels the scene and opens the menu as today.

### FR-PALS.2 Meet & greet

- When both are awake and their walks bring them within `PAL_GREET_DIST` **90 px**, they stop, face each other
  and show an emote (heart, music note or "!") for about 1.5 s, then go on.
- Cooldown `PAL_GREET_COOLDOWN_S` **180 s** per pair. No perk.

### FR-PALS.3 Play scenes

- While both are awake, a play scene starts on average `PAL_SCENES_PER_HOUR` **3 times an hour of yard-on-screen
  time** (random gap of 12-28 min, `PAL_SCENE_GAP_MIN/MAX_S`). Each lasts 5-8 s, then they go back to walking.
- Scene catalog (picked at random, weighted):
  - **Chase:** one runs, the other follows, they swap roles once.
  - **Hop in sync:** both hop on the spot together a few times.
  - **Nuzzle:** they walk up, bump heads, hearts pop.
  - **Toy pass** (only with a HOME toy): they kick the ball to each other / one pushes the other on the swing.
  - **Escort walk** (only for big & small, FR-PALS.6): the big one walks slowly beside the small one.
- **Perk:** `PAL_PLAY_JOY` **+3 JOY** to both, paid at most `PAL_PLAY_PAY_DAY` **4 scenes a day** per pet. Later
  scenes still play, they just don't pay.

### FR-PALS.4 Care for each other

- **Cheer-up:** when one awake pet has JOY < `PAL_CHEER_BELOW` **25** and the other has JOY ≥ `PAL_CHEER_FROM`
  **50**, the happy one walks over and nuzzles it. The sad one gets `PAL_CHEER_JOY` **+8 JOY**, at most
  `PAL_CHEER_DAY` **2 a day** per pet. It goes before a random play scene when both are possible.
- **Egg-sitting:** when the other slot is an egg, the pet now and then walks to the egg and sits next to it for a
  few seconds. No perk.
- **Waiting at food** (big & small only): when food drops for the small one, the big one walks over and waits
  beside it while it eats, instead of the "same berry" squabble.

### FR-PALS.5 Sleep together

- **With HOME beds:** both asleep means both in their side-by-side beds. Add a small touch: their "zzz" bubbles
  sync, and now and then one rolls toward the other.
- **Without beds:** when the second pet falls asleep with the yard on screen and the first is already asleep, it
  walks over and lies down next to it (cosmetic; the logic sleep starts at once).
- **One awake, one napping in the evening** (from `EVENING_HOUR`): the awake one sometimes sits beside the
  sleeper for a while.

### FR-PALS.6 Personality

- **Big & small:** a pair counts as big & small when one is at a higher evolution stage than the other, or its
  level is at least `PAL_BIG_LEVEL_GAP` **20** higher. Then: escort walk (FR-PALS.3), waiting at food
  (FR-PALS.4), and the big one goes first to cheer up the small one.
- **Type flavour:** emote particles are tinted with the pet's first type colour (reuse the v2.4 egg type
  colours), with a type-shaped particle for a few types (flame, drop, leaf, spark) and a generic sparkle for the
  rest. No type rivalry.

### FR-PALS.7 Squabbles (harmless)

Rare: at most `PAL_SQUABBLE_PER_HOUR` **1 an hour** of yard-on-screen time. **Never a stat change, never a
logic wake-up.** Each one ends with a make-up heart within about `PAL_MAKEUP_S` **3 s**.

- **Same berry:** food dropped for A; B trots over too, they pout at each other, A eats it, B shows "…", then
  the heart. A's food goes through the normal `yardFoodEaten` path, unchanged.
- **Bumped awake:** only on a day nap, never at night. An awake pet bumps the napping one; it shows a grumpy
  cloud for a moment and dozes on. It must **not** call `wakeGrumpy` or change `sleeping`: no JOY loss.
- **Toy tug** (only with a HOME toy): both reach the toy at once, a 2 s tug, then they take turns.

### FR-PALS.8 The Playmates medal

- A new player medal, **Playmates** ("Compañeros de juego"; "Best friends" is already the bond-100 medal): an *event* medal that counts play scenes and cheer-ups seen (greetings
  and squabbles don't count). Tiers `MEDAL_T` **10 / 50 / 200**, paying the usual `MEDAL_COINS` 25 / 75 / 200.
- **Save layout.** `PlayerMedals` (`medals.h`) sizes `tier[]` and `cnt[]` by `MEDAL_COUNT`, and `PM_VER` is 1.
  Adding a 20th medal changes the blob: bump `PM_VER` to 2 and don't migrate v1 (see *Rules*: the user starts a
  new save). An old blob only has to fail the version check cleanly.

### FR-PALS.9 Save data

- Daily perk counters per slot `{uid, day, playsPaid, cheersPaid}`: a new NVS key `pal`, or added to HOME's
  `upg` with a version bump (no migration needed, see *Rules*). Missing key = nothing paid today.
- Cooldowns and scene timers are RAM-only.

### FR-PALS.10 Serial and tests

- Serial: `PAL` prints the director state, cooldowns and today's counters; `PAL <scene>` forces a scene now
  (greet, chase, hop, nuzzle, toypass, escort, cheer, eggsit, berry, bump, tug), ignoring the gap but not the
  blocking rules.
- Host tests (`test/test_pals.cpp`): director timing (gap, cooldown, per-hour rates on simulated yard time),
  every blocking rule, big & small detection, cheer-up priority, toy scenes only with a toy, squabbles never
  change any `Pet` field (snapshot before/after), daily perk caps, uid change resets counters, the new medal's
  count and payout, and a v1 `pm` blob being rejected cleanly.
- `PENDING_TESTS.md` rows (EN/ES): each scene forced over serial and seen on the board, a natural hour of
  watching with two pets (count the scenes), cheer-up with a sad pet, sleeping together with and without beds,
  the medal banner.

---

## Decided in the interview

| Topic | Choice |
|---|---|
| Interaction moments | All four: meet & greet, play together, care for each other, sleep together |
| Weight | Small perks (JOY on play, cheer-up, Friends medal); no "company slows drain" |
| Tone | Mild squabbles allowed: same berry, bumped awake, toy tug (no snub) |
| Personality | Big & small, type flavour (no same-line special) |
| Trigger | Autonomous only, no new buttons |
| Frequency | Greeting when close (cooldown), 2-4 bigger scenes an hour |
| Interactions screen-off | Only while the yard is on screen |
| Upgrade lines | Beds, gym set, toy (no food bowl) |
| Upgrades in the yard | Visible objects that pets walk to, at fixed spots per map |
| Upgrade effects screen-off | Yes, in the tick |
| Tiers and price | 3 tiers, about a week each (500-1800) |
| Bed effect | Faster ENE + good-morning bonus (no drain/floor change) |
| Gym | One set for all three stats, ceiling per tier, small ENE cost, needs ≥ 40 |
| Toy | Solo play + used in play scenes (no passive drain change) |
| Beds | One purchase gives two beds, side by side |
| Shop | New "Home" tab |
| Art | Pixel art in `tools/sprites.py`, reviewed as renders |
| Order | HOME first, then PALS |

## Open points (Claude's defaults, revisit on the board)

- Every number in this page (prices, rates, caps, chances, distances).
- Gym trains the lowest stat first; another option would be the stat of the last minigame played.
- The morning bonus needs 6 h of sleep; a pet put to bed at 03:00 gets nothing.
- Toy solo play needs JOY < 70; maybe it should also need the other pet to be busy or asleep.
- The purchase confirmation dialog: keep it, or skip it for tier 1.
- Whether the gym and toy also show a small "in use" state for the second pet.

---

## Build plan: phase 1 (HOME, v3.20)

Branch `feature/home` from main v3.19. Its first commit is the **contract** that the three parallel tasks
build against, so they can work in separate worktrees without waiting for each other:

- `tuning.h`: every HOME number (block "Mejoras de casa").
- `upgrade.h`: `UpgLine` (`UPG_BED`, `UPG_GYM`, `UPG_TOY`), `upgPriceNext`, `upgCheck` → `UpgBuy`,
  `HomeEvt` / `HomeEvtKind` (`HE_MORNING`, `HE_GYM`, `HE_TOY`).
- `Pet` API (`pet.h`): `upgTier(line)`, `buyUpgrade(line)`, `takeHomeEvt(e)`, `dbgSetUpgTier`, field
  `upgTiers[]`. `pet.cpp` has a minimal stub (no NVS, no events) that task H1 replaces.

### Facts found while planning (they shape the tasks)

- The yard has 9 maps. The map `.bin` files and tilesets are **not in git** (licence); `tools/yard/out/*.bin`
  exist only locally, so tests that read real maps skip without them.
- `sleepMinute` is the only place `SLEEP_ENE_GAIN` is used, and it runs both live and in the offline replay
  (`syncHome`). There is **no sleep-duration counter**: the morning bonus needs one.
- The offline replay (`syncHome`) writes the awake minute out by hand: gym and toy must be added there
  explicitly, and their daily caps must use the replayed minute's day, not `today()`.
- `morningWake` is player-wide, not per slot; the per-slot bonus needs its own event (`HE_MORNING`).
- Gym gains already show "ATK +1" popups for free: `fxTrainW` watches `trAtk/trDef/trSpe` per slot.
- Pet knows ceremony, dying, evolving and vacant; games, battles, encounters and Box moves only reach it through
  `nightHold`, which the gym/toy gate reads.
- Sleeping pets never walk today (`yardStep`, `YH_SLEEP`); `yardAdvance` keeps pets 64 px apart, so pets in
  neighbouring beds need an exception.
- The Bag has no tab strip (the Shop is its own dock button), so Home is a page of the Shop pager.

### Tasks (parallel, one worktree each)

| Task | Agent | Owns | Must not touch |
|---|---|---|---|
| **H1 logic** | implementer, Opus | `pet.cpp`/`pet.h`/`petlife.h` HOME logic, NVS `upg`, bed ENE + sleep counter + morning bonus, gym, toy (live and offline), events, serial `UPG`, `test/test_upgrades.cpp`, GAME_LOGIC §4/§11/§13/§16 | yard drawing, Shop UI, `i18n.*`, `tools/sprites.py`, `species.h` |
| **H2 yard** | implementer, Opus | `tools/sprites.py` (9 sprites) → `species.h`, `yard_spots.h` (9 maps), walk-mask blocking, drawing + tint, walk to bed / sleep in bed, gym and toy visits from `takeHomeEvt`, the morning popup, connectivity test, yard renders, GAME_LOGIC §7 | `pet.cpp` logic, Shop UI, `i18n.*` (no new strings) |
| **H3 shop** | implementer, Sonnet | Shop "Home" page, confirm, toast, **all new strings** in `i18n.*`, shop renders, GAME_LOGIC §11 Shop paragraph | `pet.cpp`, yard code, sprites (uses a placeholder icon until H2's `SPR_*` land, then the real ones after the merge) |

Then, in order: merge H1 → H2 → H3 into `feature/home` (fix conflicts, swap H3's placeholder icons for the
H2 sprites), full host tests + i18n check + firmware build + renders, the `reviewer` on the whole diff, release
v3.20 with the `tamapoke-release` skill (CHANGELOG, GAME_LOGIC header, PENDING_TESTS rows), flash, and start a
new save on the board.

---

## Build plan: phase 2 (PALS, v3.22)

Branch `feature/pals` from main v3.21. Contract commit `643cc4d`: the PAL numbers in `tuning.h`, the `Pet` perk API
(`palPlayPerk(slot)`, `palCheerPerk(slot)`, `palSceneSeen()`, stubs) and `pal_fx.h` (`PalEmote` kinds).

User choices made while planning (2026-11-23): the medal is **Playmates** ("Compañeros de juego"); the `PM_VER`
bump resets event-medal counters on the current save (no conversion; tiers and coins already earned stay).

Facts that shape it: the yard has no per-scene exception to `YARD_MIN_DIST` (only bed tasks); a `YT_SCENE` task
blocks gym/toy visits for free (`yardPetFree` needs `YT_NONE`); `yardTick` only runs with the yard on screen;
`bedSnap` marks a reopened yard; PALS emotes must be yard-local (a `PetLife::emote` holds the pet); there is no
evolution-stage index (walk `DEX_TBL[base].evolvesTo`); no music-note or cloud sprite yet; yard code uses
`YardWorld.rng`.

| Task | Agent | Owns |
|---|---|---|
| **P1 logic** | implementer, Sonnet | perks with daily caps (`uid`, day) in a new NVS key `pal`, `MD_PLAYMATES` (event medal, `PM_VER` 2, `MEDAL_T` row, names in all 8 rows), tests, GAME_LOGIC §4/§13 |
| **P2 director + yard** | implementer, Opus | `pals.h` pure scene director + `YT_SCENE` in `yard.h`, hook in `yardTick`, every scene/squabble, sleep together, egg-sitting, waiting at food, big & small, emote drawing, serial `PAL`, `test/test_pals.cpp`, renders, GAME_LOGIC §7/§16 |
| **P3 emote art** | implementer, Sonnet | sprites (note, cloud, grumpy cloud, dots, sparkle and type particles) in `tools/sprites.py` → `species.h`, `pal_fx.h` sprite table, type-tint and float/burst motion helpers, `test/test_pal_fx.cpp` |

Then merge P1 → P3 → P2, refresh pointers, tests + build + renders, reviewer, release v3.22, flash.
