# Design spec: the "Yard" UI (inspired by a desktop-pet video)

Reference: a 65 s vertical video (720x1280, Chinese with English subtitles) of a
round-screen desktop pet. Key frames are in `docs/images/yard-reference.png`
(eight screens: home, home with egg, shop, travel, collection, stage battle,
scene picker, beach home). This spec takes the video's **design language and
interaction ideas**, not its art: every tile, icon and sprite here comes from
TamaPoke's own sources (PMD SpriteCollab, procedural drawing) or a CC0 tileset.

Status: **draft for iteration.** Nothing here is built. Items marked
**[decide]** are open choices to settle before Claude Code starts.

---

## 1. What the video shows (by timestamp)

| Time | Screen | What matters for us |
|---|---|---|
| 0:00-0:10 | **Home** | Top-down (bird's-eye) pixel-art map fills the whole disc. Several pets wander freely. Big white clock at top (`12:30`) with a coin counter under it (`● 117`). A pet shows a speech bubble ("I want some water"). Bottom: a dock of **3 round icon buttons** with tiny labels (Functions, BAG, SHOP). |
| 0:11-0:16 | **Egg** | Buy an egg, drop it into the scene, shake the device: it hatches in about a day. Up to 4 pets share the screen. |
| 0:17 | **Shop** | Dark panel. Title with icon (`SHOP`), coin balance. Rows as rounded cards: item icon, name, price, green **Buy** pill. `<` `>` chevrons at the disc edge, page counter `2/3`. A green toast "Purchased" over the row. Dock still visible below. |
| 0:18 | **Bag** | Same panel pattern, items with a **Use** pill. |
| 0:20-0:25 | **Feeding** | Food is dropped *into the scene*; the pet shows `!`, walks over and eats it; a heart pops. |
| 0:26-0:34 | **Travel** | Title ("Traveling"), countdown + daily counter (`2:59:55 · today 1/50`). A wide banner illustration inside a rounded rect with the pet in it. Below: a timestamped event log with coloured highlights ("17:34 ... mood +2"). Round blue **back** button at the bottom. |
| 0:35-0:45 | **Collection** | Title, `collected 6/583`, page `1/10`. Dense grid of item icons, unknown ones dimmed. Tapping one shows a tooltip card (name, how it was found, date). |
| 0:46-0:54 | **Stage** | `STAGE 01`, subtitle. Two HP bars (green left, red right) with `48/52` and `98/100`. Battle banner (same style as travel). Two-line combat log. Back button. |
| 0:55-1:00 | **Scene picker** | Full-disc preview of a background with its name at the top ("Park 02", "Castle", "Moon", "Beach"). Swipe or `<` `>` to browse **12 backgrounds**; a price pill (`● 100`, 200, 600, 800) to buy. |
| 1:01-1:05 | **Home (beach)** | Same home with the newly picked background. |

## 2. Design language

### 2.1 Two surfaces

1. **World surface** (home, feeding, egg): edge-to-edge pixel-art map, no
   frame, no panels. UI floats on top with a soft shadow: clock, coins, dock.
2. **Panel surface** (shop, bag, travel, collection, stage, settings): a dark
   near-black disc, bold white title at the top, content in rounded cards, one
   round back button at the bottom centre. Black is free on an AMOLED, so this
   also saves power.

TamaPoke today mixes a side-view landscape with a light cream panel (stat bars,
4 white buttons). The proposal moves the home to surface 1 and every menu-type
screen to surface 2.

### 2.2 Palette (starting values, tune on the panel)

Sampled by eye from a phone video of a screen, so treat them as a direction,
not as exact values.

| Token | RGB | Use |
|---|---|---|
| `Y_PANEL_BG` | `#14161E` | panel disc background |
| `Y_CARD` | `#252838` | row cards, tooltip cards |
| `Y_CARD_HI` | `#30344A` | pressed / selected card |
| `Y_TEXT` | `#FFFFFF` | titles, names |
| `Y_TEXT_DIM` | `#8C90A6` | prices, subtitles, page counters |
| `Y_ACCENT_GO` | `#2EC48A` | Buy / Use / OK pills, toasts, ally HP |
| `Y_ACCENT_BAD` | `#D8344A` | foe HP, warnings |
| `Y_BACK` | `#5E8FE0` | the round back button |
| `Y_COIN` | `#F2C230` | coin glyph |
| `Y_HILITE` | `#FF6FA8` | positive values in logs ("mood +2") |

### 2.3 Type

- Titles: the existing Latin font at the large scale, white, centred, about
  y = 40-60 of the 466 canvas (inside the disc's safe area).
- Body: the existing small scale. CJK strings keep the unifont path already in
  place.
- Clock: the biggest text on the home, white with a 2 px dark drop shadow so
  it reads on any background.

### 2.4 Components (new, in a `yard_ui.h`)

| Component | Spec (466 canvas) |
|---|---|
| `drawPanelBg()` | `fillCircle(CX, CY, 231, Y_PANEL_BG)` |
| `drawTitle(text, sub)` | title at y≈48, optional dim subtitle at y≈74 |
| `drawCard(x, y, w, h)` | `fillRoundRect(..., 12, Y_CARD)` |
| `drawPill(x, y, w, label, color)` | 64x28 rounded-full pill, white label |
| `drawCoinBadge(x, y, n)` | coin glyph + number |
| `drawChevrons(showL, showR)` | `<` at x≈30, `>` at x≈436, y = CX; tap zones 60x120 |
| `drawPager(cur, n, y)` | `"2/3"` dim text (or the existing `drawPageDots`) |
| `drawBackBtn()` | circle r=20 at (CX, 418), `Y_BACK`, white curved arrow |
| `drawDock(items[3])` | 3 circles r=22 at y≈410, spaced 56 px, tiny label above each |
| `drawToast(text)` | green rounded rect over the active row, 900 ms |
| `drawBubble(x, y, text)` | white rounded speech bubble with tail, above a pet |
| `drawHpBar(x, y, w, cur, max, color)` | 10 px rounded bar + `cur/max` above it |
| `drawBanner(img, x, y, w, h)` | rounded-rect window onto an illustration (travel, stage) |

All hit zones are at least 48x48 px even when the drawn shape is smaller,
and every tappable element stays inside the round frame's safe radius.

## 3. Screen by screen: what changes in TamaPoke

### 3.1 Home becomes the Yard

- **Camera:** top-down map, 466x466, fixed (no scrolling in v1).
- **Pets:** the home pet walks freely around the yard on waypoints, idles,
  sleeps where it stands at night. **[decide]** Box pets visible as well
  (the video's "up to 4 on screen")? Option A: only the home pet. Option B:
  Box pets also wander but stay hibernating (no ageing, no needs), so the
  save rules from Task BOX don't change. Option B is the video's look; A is
  the smallest change.
- **HUD:** clock + coins at the top, the dock at the bottom. The stat bars
  move off the home: a tap on a pet opens its existing stat card. **[decide]**
  keep a tiny 4-dot need indicator next to the pet instead?
- **Dock (3 buttons):** `Care` (opens the current FEED/PLAY/CARE menus as a
  panel), `Bag` (the existing Backpack), `Shop` (new). **[decide]** labels and
  icons; the current 4-button row maps to Care + Bag.
- **Speech bubbles:** the existing emotes (`drawEmote`, `drawSleepyBubble`)
  grow into short text bubbles from the i18n table: "Hungry", "Let's play",
  "Sleepy", "I found something!". Max one bubble on screen, 3 s.
- **Day/night:** tint the whole map rather than switching sky colours
  (multiply toward blue at night, warm at dusk), so one background serves all
  hours. Stars and moon are dropped from the yard.
- **Swipes:** keep today's swipe gestures (right = Box, etc.); the dock only
  adds taps.

### 3.2 Drop-to-act feeding

Feeding moves into the world: pick food in the Bag, the item falls into the
yard at a spot near the pet, the pet shows `!`, walks to it and plays its PMD
`Eat` action, then a heart pops (reuse the FX banner from Task FX). The same
pattern can serve toys later.

### 3.3 Egg in the world

Today an egg comes from walks (v1.39) and hatches via the "Hatch an egg"
button. In the Yard the egg sits in the scene, and **shaking the board**
(the motion code already detects shakes) wobbles it and advances hatching.
The rule "eggs are found by walking" stays; only the presentation changes.

### 3.4 Shop (new) and coins (new)

- Coins earned from minigames, daily stage clears, walk goals and travel.
- Shop sells consumables that already exist in the Backpack (berries, potion,
  revive...) plus **backgrounds** (3.7). Prices in a constants table.
- **[decide]** whether coins exist at all. TamaPoke has no currency today;
  adding one touches the save (NVS budget section of `docs/NEXT_FEATURES.md`)
  and game balance (`docs/GAMEPLAY_TUNING.md`). A smaller option: no coins,
  backgrounds unlock by milestones (badges, Dex count, steps).

### 3.5 Panel restyle (Bag, Box, Dex, stat card, settings)

Same content, new chrome: dark disc, white title, card rows, chevrons and
pager, back button. This is mostly a find-and-replace of `UI_BG_DAY` and
white rounded rects for the panel components, screen by screen, checked with
`tools/render/render.sh` after each one.

### 3.6 Stage battle restyle

The v1.65 stages already have everything the video shows (team, HP, foe,
log). Restyle only: `STAGE 0N` title, stage name as subtitle, the two HP bars
with numbers above a **banner window** that shows the battle on the stage's
biome, and a two-line log under it. The 4-pill team strip stays.

### 3.7 Backgrounds and the scene picker

- 12 yard maps, unlocked one by one (by coins or milestones, see 3.4).
  Starting list, matched to TamaPoke's 9 biomes plus 3 extras: meadow, forest,
  beach, volcano, mountain, snow, city, swamp, ruins, park, castle, moon.
- Picker: full-disc preview, name at the top, `<` `>` and swipe, a price
  or "locked: reason" pill, back button. Choice saved in NVS (one byte).
- **[decide]** does the species' home biome still matter? Suggestion: the
  default background is the pet's biome; once others are unlocked the player
  picks.

### 3.8 Travel (optional, phase 4)

Send the pet out for a few hours: the yard shows it as away, the Travel panel
shows a countdown, a banner of where it is, and a log of 1-3 random events.
It comes back with an item for the Collection. Pairs well with the existing
walk finds. **[decide]** in or out of scope.

### 3.9 Collection (optional, phase 4)

A grid of every collectible (walk finds, travel souvenirs, badges, medals),
unknown ones dimmed, tap for a tooltip card with name, source and date. Could
absorb the badge page of the Box.

## 4. Technical notes

### 4.1 Backgrounds: tile packs + text maps + a packing script (decided)

The current scenes are drawn procedurally (`drawScene`, `drawMeadow`,
`drawSwamp`...). Yard backgrounds are **images built from CC0 tile packs**,
laid out in **plain-text map files**, and turned into SD files by a
**Python script** that Claude Code writes. Nobody draws the art by hand,
and no image generators are used.

#### Size: 240 → 480 → 466 (decided)

The game canvas stays **466x466** (the round 1.75" UI, placed at (6, 6) on
the 480x480 panel; see `docs/AMOLED-2.16.md`). Pixel art must scale by a
whole number and tiles must fill a whole grid, so:

```
map: 15 x 15 tiles of 16 px = 240 x 240 source pixels
  x2 (each source pixel becomes a 2x2 block)  ->  480 x 480
  drawn at canvas (-7, -7), clipped           ->  466 x 466 canvas
```

The 7 px lost on each side fall outside the round mask anyway. The file on
the SD keeps all 240x240 pixels; the crop happens when drawing.

#### Tile pack: Sunnyside World (decided)

**Sunnyside World** by Daniel Diggle (itch.io, free, 16x16, v2.1). Tested in
this design: two maps were built from it with the prototype script (below)
and rendered at 466 with the round mask.

What the pack gives us (all in one sheet, so one palette and one style):
- `Tileset/spr_tileset_sunnysideworld_16px.png`: 1024x1024, 64x64 tiles of
  16 px, **no spacing, no margin**.
- Ground: grass with texture variants, dirt paths, sand, water, cliffs.
- Non-vegetation: grey rocks, coal, blue-crystal and gold ore rocks in three
  sizes, pebbles, minecart rails, anvil, wells, gravestones, statues, fences,
  crates, barrels, buildings with several roof colours.
- Vegetation: round trees, pines, bushes (plain, berry, orange), flowers,
  mushrooms, stumps.
- Animals as separate strips (`Elements/Animals`: sheep, chicken, cow, pig,
  duck, bird), usable as yard decorations later.
- A GameMaker project (`Sunnyside_World_Gamemaker/`) whose tileset file
  defines **16-tile autotile sets** for edges: Land, Path 01/02/03, River,
  Building 01/02, Inner Walls, Clouds, Cloud Shadow. The script reads them
  from there (see "Autotiles").

A second pack is **not needed for v1**: rocks, ores, rails and the anvil cover
the "non-vegetation / light industry" look. If more industry is wanted later
(machines, pipes, conveyors), add a second pack for props only and let the
script remap its colours to the nearest Sunnyside colours.

**Licence handling.** Sunnyside's V1 licence allows free and commercial use
and changes, needs no credit, allows redistribution only inside tutorials or
courses (with a link), and forbids repackaging or reselling the assets. So:
- `tools/yard/tilesets/` is **git-ignored**. Each builder downloads the pack
  from itch.io and unzips it there. The script prints that instruction if the
  folder is missing.
- The generated `bgNN.bin` files contain the art, so they are git-ignored as
  well and never go into `web/` or a release. They live only on the SD card.
- The repo holds only maps, legends and the script. Credit line in
  `CREDITS.md` anyway (optional under the licence, but courteous).
- Builds without the pack get the procedural fallback (below).

Recolouring (snow, volcano, moon...) stays possible later as a palette map in
the legend; Sunnyside allows changes.

#### Repository layout

```
tools/yard/
  tilesets/sunnyside/       the unzipped pack, as shipped (git-ignored)
  legends/sunnyside.txt     characters -> tiles, stamps, autotiles
  maps/NN-name.txt          one file per background
  out/                      generated .bin + previews (git-ignored)
tools/pack_yard.py          maps -> out/NN-name.bin + previews
tools/send_sd.py            also sends out/*.bin to /yard/ on the SD
```

#### Map file format

Two layers of 15 lines x 15 characters each. The ground layer fills every
cell; the object layer is drawn on top (`.` = nothing). One character = one
16 px tile; a stamp character places a bigger object by its top-left cell.

```
# maps/01-meadow.txt
name: Meadow               # i18n key is derived from it: YARD_MEADOW
legend: sunnyside
seed: 7                    # fixes the random pick among tile variants
ground:
ggggggggggggggg
ggwwwgggggggggg            # w = water; edges are drawn automatically
...                        (15 lines of 15 characters)
objects:
...t.......T...            # t = small pine, T = tall pine (stamps)
..E..f..R......            # E = berry bush, f = flowers, R = big rock
...                        (15 lines of 15 characters)
```

Later fields (when 3.4 is settled): `price: 100` or `unlock: badge 1`.

#### Legend file format

Coordinates are **tile column,row** in the sheet (so `49,21` is the tile at
x = 784, y = 336).

```
# legends/sunnyside.txt
sheet = spr_tileset_sunnysideworld_16px.png
autotiles = ../../Sunnyside_World_Gamemaker/tilesets/tileset_sunnysideworld/tileset_sunnysideworld.yy

g = 2,1 2,2 3,2 1,2 2,3 3,3 2,4   weights 10 1 1 1 1 1 1   # grass, mostly plain
d = autotile "Path 01" over g fill 9,7 10,7 11,7 9,8 10,8  # dirt with automatic edges
w = autotile "River" over g fill 32,8 33,8 !               # water; "!" = blocks walking
R = stamp 2x2 49,21 !        # big grey rock
G = stamp 2x2 49,27 !        # gold ore rock
T = stamp 2x3 51,6 !         # tall pine
f = 33,1 33,2 33,3 34,3 35,3 # flowers (walkable)
l = 47,7                     # minecart rail, straight
```

Entry kinds:
- `c,r [c,r ...] [weights ...]`: one tile, or random variants (fixed seed).
- `stamp WxH c,r`: a block of W x H tiles, placed by its top-left cell.
- `autotile "Set" over b [fill c,r ...]`: ground terrain whose edges come
  from the named autotile set; `b` is the ground drawn underneath; `fill`
  gives textured variants for the solid interior.
- A trailing `!` marks the cells as blocking.

The prototype legend in `tools/yard/legends/sunnyside.txt` already maps
about 25 entries (grass, dirt, water, bushes, pines, four rock kinds, pebbles,
flowers, stump, mushrooms, well, anvil, rails).

Errors stop the script with file, line and column: unknown character, wrong
line count or width, a stamp running off the grid, two stamps overlapping,
a stamp or autotile in the wrong layer, an unknown autotile set.

#### Autotiles

GameMaker's 16-tile sets are stored as a list of 16 tile indices
(`index = row * 64 + col`, 0 = no tile). **The position in the list is a
bitmask of the tile's background corners**: top-left = 1, top-right = 2,
bottom-left = 4, bottom-right = 8. Position 0 is solid terrain, 15 is empty.
Checked by eye against Path 01, Path 03 and River.

How the script uses it: a corner (vertex) of the grid counts as terrain if
any of the up-to-4 cells touching it is marked with the terrain character.
Each cell then gets the tile for its 4 corners. Marked cells come out solid;
the ring of cells around them gets the edge and corner pieces. Some sets have
transparent edges (paths), so the ground under them (`over g`) is drawn
first. Order per map: ground tiles → autotile edges → objects.

#### What `pack_yard.py` does

1. Read a map and its legend; check sizes, characters and overlaps.
2. Compose the 240x240 image: ground, autotile edges, objects.
3. Quantise to a palette (Sunnyside maps come out at 60-80 colours).
4. Build the **walk mask**: 15x15 cells, 1 bit each, set where a blocking
   tile or stamp sits, under the solid part of blocking terrain, outside the
   round frame (12 px inset) and inside the HUD zones (below).
5. Write `out/NN-name.bin` (format below).
6. Write `out/NN-name_preview.png` (466x466: 2x scale, (-7, -7) crop, round
   mask, a mock clock/coins/dock, two Sunnyside animals for scale) and
   `out/NN-name_debug.png` (same, with the grid, blocked cells tinted red and
   the HUD zones outlined).

`python3 tools/pack_yard.py <pack_tileset_dir> tools/yard [NN]` packs
everything, or map NN only.

**A working prototype exists** (`pack_yard.py`, `legends/sunnyside.txt`,
`maps/01-meadow.txt`, `maps/02-quarry.txt`, made while writing this spec).
Results: both maps pack to about 57.8 KB with 63 and 76 colours, and
decoding the `.bin` gives back the composed image with **0 mismatched
pixels**. Claude Code should start from it rather than from scratch, then
tidy it (CLI flags, `out/` handling, tests).

#### HUD safe zones (canvas coordinates)

Keep busy detail (houses, trees, high-contrast props) out of:
- clock and coins: y 20-110, x 140-326
- dock: y 380-440, x 150-316

Plain ground there is fine. The preview outlines both zones so a map can be
checked at a glance.

#### File format `TPY1`

```
"TPY1"           4 bytes
u8  version      1
u8  w, h         240, 240
u8  palCount     1..256 (0 means 256)
u16 pal[]        RGB565, little-endian
u8  walk[29]     15x15 bits, row-major, bit k = byte k/8, bit k%8 (LSB first), 1 = blocked
u8  px[w*h]      palette indices, row-major
```

About 58 KB per map. On the board: read once when the background changes,
expand into a 466x466 RGB565 buffer in PSRAM (434 KB, 2x with the (-7, -7)
offset), then copy that buffer into the canvas at the start of each frame
before pets and HUD are drawn. A host test (`test/test_yard_bg.cpp`) checks
the reader with a small file made by the script, including a short file and
a wrong magic number.

#### Fallback

With no SD or a missing/bad `bgNN.bin`, the yard draws a flat procedural
ground in the biome's colour (`BIOME_SOIL`) with a few tufts, so the game
still runs.

### 4.2 Sprites need directions

`tools/pack_pmd.py` already packs `Walk` facing left (row 6) and right
(row 2) as two actions, plus one facing for everything else. PMD sheets have 8
rows (down, down-right, right, up-right, up, up-left, left, down-left). A
top-down yard also needs **down (row 0) and up (row 4)** for `Walk`: add them
as two new action ids (append at the end of `ACTIONS` so existing ids keep
their numbers). Diagonals can use the nearest left/right row. That adds about
twice the current walk data per species; check SD space and that the TPK2
reader ignores unknown action ids on old files.

### 4.3 Frame budget

- Background `memcpy` 434 KB in PSRAM per frame: measure; if it costs too
  much, redraw only dirty rects around pets and HUD (pets move a few px a
  frame).
- The round mask (`roundmask.h`) and the 6,6 panel offset stay unchanged.
- The dock and the clock are the only static UI on the home: shift them a
  pixel every few minutes or dim them when idle (see `burnin.h`).

### 4.4 Pet movement

A small wander controller in a new pure header (`yard.h`, host-tested like
`stage.h`): waypoints inside a walkable mask per map (a 29x29 coarse grid,
1 bit per cell), speed, idle time, facing from the movement vector, a "go to
point" mode for food and the egg. Pets avoid each other with a minimum
distance. Depth sort by y before drawing.

### 4.5 Strings

Every new string goes into all 8 language rows of `i18n.cpp`; only EN and ES
need care (see `CLAUDE.md`). Speech bubbles stay under ~14 characters in EN
and ES.

## 5. Phases for Claude Code

Each phase ends with host tests passing, `render.sh` screenshots of the
touched screens (EN and ES), and a line in `CHANGELOG.md`.

| Phase | Scope | Done when |
|---|---|---|
| **Y0** | `yard_ui.h` components + palette, a render-only demo screen | all components rendered on the PC, inside the safe radius |
| **Y1** | Panel restyle: Bag, Box, Dex, stat card, settings, stage | renders match the panel spec; no behaviour change; host tests unchanged |
| **Y1b** | Background pipeline from the prototype: `tools/yard/` layout, `.gitignore`, Sunnyside legend, `pack_yard.py`, previews, `TPY1` reader + host test, `send_sd.py` update | `01-meadow` and `02-quarry` previews match the prototype's; the reader test passes |
| **Y2** | Yard home: one background from SD, procedural fallback, home pet wandering (`yard.h` + tests), clock + dock HUD, tap pet → stat card | the pet walks, faces the right way, HUD readable on day and night tint |
| **Y3** | Sprite directions (`pack_pmd.py` + reader), drop-to-feed, egg-in-world with shake, speech bubbles | feeding and hatching work in the yard on the board |
| **Y4** | Backgrounds + picker (+ coins and Shop if chosen) | 12 maps selectable, choice survives a reboot |
| **Y5** | Optional: Box pets in the yard, Travel, Collection | per the decisions in section 6 |

Board checks for each phase go into `docs/DEVICE_TESTS.md` as usual.

## 6. Decisions to settle first

1. Box pets visible in the yard (A: home pet only / B: up to 4)?
2. Stat bars: gone from the home (tap a pet) or a mini indicator?
3. Dock: which 3 buttons, and where do PLAY and CARE live?
4. Coins and a Shop, or milestone unlocks?
5. ~~Background art source~~ **Decided:** tile pack, text maps,
   `pack_yard.py`, 240 → 466 (section 4.1). Pack: **Sunnyside World**
   (tested). Still open: how many maps in v1.
6. Does the species biome still pick the default background?
7. Travel and Collection: in scope or later?
8. Keep the side-view home as a setting (`-D TP_YARD=0`) during the change?

## 7. Kickoff prompt (after section 6 is settled)

> For the backgrounds (can start before the rest, since it is all tooling):
> Read `docs/DESIGN_YARD_UI.md` section 4.1 and `CLAUDE.md`. The prototype
> files are in `tools/yard/` (pack_yard.py, legends/sunnyside.txt, two maps)
> and I've unzipped Sunnyside World into `tools/yard/tilesets/sunnyside/`.
> Do phase Y1b: make the tileset folder and `out/` git-ignored, move the
> script to `tools/pack_yard.py` with proper arguments, keep its checks, add a
> Python test for the parser errors and a `TPY1` reader with a host test, and
> add `/yard/` to `send_sd.py`. Run it and show me both preview PNGs.
>
> For the UI: Read `docs/DESIGN_YARD_UI.md` and `CLAUDE.md`. Start with phase Y0: create
> `yard_ui.h` with the palette and components from section 2, and a demo
> screen reachable from the serial console. Render it with
> `tools/render/render.sh` in EN and ES and show me the PNGs. Don't change
> any existing screen yet.
