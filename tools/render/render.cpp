// Renders TamaPoke's screens on the PC, the way the AMOLED-2.16 shows them.
//
// This translation unit is the sketch itself (TamaPoke.ino, turned into C++
// with PlatformIO's own converter by render.sh) plus a main() that puts the
// game in each state and calls the sketch's own render(). The canvas is the
// real RoundCanvas over Arduino_GFX's real Arduino_Canvas, blitted into a
// 480x480 "panel" (fakes/Arduino_GFX_Library.h), so the PNGs show exactly the
// pixels the firmware would send: offset, round frame and all.
//
// Usage: render <out_dir> [scene ...]     (no scenes = all of them)

#include TAMAPOKE_SKETCH_CPP  // .pio/render/TamaPoke.ino.cpp

#include <SD_MMC.h>
#include <sys/stat.h>
#include <zlib.h>

#include <functional>
#include <string>
#include <vector>

uint32_t gHostMillis = 1000;
uint32_t gHostRandom = 12345;
HardwareSerial Serial;
EspClass ESP;
TwoWire Wire;
SDMMCFS SD_MMC;
extern bool gStubImuPresent;  // stubs.cpp: el IMU de mentira
extern int8_t gStubTiltSide;
extern bool gStubTiltSlide;
extern uint8_t gStubVol;      // stubs.cpp: el volumen (volume.h)

// ---- PNG -------------------------------------------------------------------

static void putBE32(std::vector<uint8_t> &v, uint32_t x) {
  for (int s = 24; s >= 0; s -= 8) v.push_back((uint8_t)(x >> s));
}

static void pngChunk(FILE *f, const char *type, const std::vector<uint8_t> &data) {
  std::vector<uint8_t> c;
  putBE32(c, (uint32_t)data.size());
  c.insert(c.end(), type, type + 4);
  c.insert(c.end(), data.begin(), data.end());
  uint32_t crc = crc32(0, c.data() + 4, (uInt)(c.size() - 4));
  putBE32(c, crc);
  fwrite(c.data(), 1, c.size(), f);
}

// RGB888 image -> PNG file
static bool writePng(const std::string &path, const std::vector<uint8_t> &rgb, int w, int h) {
  std::vector<uint8_t> raw;
  raw.reserve((size_t)(w * 3 + 1) * h);
  for (int y = 0; y < h; y++) {
    raw.push_back(0);  // filter: none
    raw.insert(raw.end(), rgb.begin() + (size_t)y * w * 3, rgb.begin() + (size_t)(y + 1) * w * 3);
  }
  uLongf zlen = compressBound((uLong)raw.size());
  std::vector<uint8_t> z(zlen);
  if (compress2(z.data(), &zlen, raw.data(), (uLong)raw.size(), 9) != Z_OK) return false;
  z.resize(zlen);
  FILE *f = fopen(path.c_str(), "wb");
  if (!f) return false;
  static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
  fwrite(sig, 1, 8, f);
  std::vector<uint8_t> ihdr;
  putBE32(ihdr, (uint32_t)w);
  putBE32(ihdr, (uint32_t)h);
  ihdr.insert(ihdr.end(), { 8, 2, 0, 0, 0 });  // 8-bit RGB
  pngChunk(f, "IHDR", ihdr);
  pngChunk(f, "IDAT", z);
  pngChunk(f, "IEND", {});
  fclose(f);
  return true;
}

static void rgb565to888(uint16_t c, uint8_t *o) {
  uint8_t r = (c >> 11) & 0x1F, g = (c >> 5) & 0x3F, b = c & 0x1F;
  o[0] = (uint8_t)((r << 3) | (r >> 2));
  o[1] = (uint8_t)((g << 2) | (g >> 4));
  o[2] = (uint8_t)((b << 3) | (b >> 2));
}

static std::vector<uint8_t> panelRgb() {
  std::vector<uint8_t> rgb((size_t)BOARD_LCD_WIDTH * BOARD_LCD_HEIGHT * 3);
  const uint16_t *fb = panel->getFramebuffer();
  for (int i = 0; i < BOARD_LCD_WIDTH * BOARD_LCD_HEIGHT; i++) rgb565to888(fb[i], &rgb[(size_t)i * 3]);
  return rgb;
}

// ---- contact sheet: the screens side by side on the board's square glass ----

static void fillRect(std::vector<uint8_t> &img, int iw, int x, int y, int w, int h, uint32_t col) {
  for (int j = y; j < y + h; j++)
    for (int i = x; i < x + w; i++) {
      uint8_t *p = &img[((size_t)j * iw + i) * 3];
      p[0] = (uint8_t)(col >> 16);
      p[1] = (uint8_t)(col >> 8);
      p[2] = (uint8_t)col;
    }
}

// rounded square in the colour of the glass/bezel, with the panel pasted inside
static void pasteOnGlass(std::vector<uint8_t> &img, int iw, int x, int y, const std::vector<uint8_t> &panelImg) {
  const int pad = 18, side = BOARD_LCD_WIDTH + 2 * pad, rad = 34;
  const uint32_t glass = 0x111317;
  for (int j = 0; j < side; j++)
    for (int i = 0; i < side; i++) {
      int dx = i < rad ? rad - i : (i >= side - rad ? i - (side - rad - 1) : 0);
      int dy = j < rad ? rad - j : (j >= side - rad ? j - (side - rad - 1) : 0);
      if (dx * dx + dy * dy > rad * rad) continue;
      fillRect(img, iw, x + i, y + j, 1, 1, glass);
    }
  for (int j = 0; j < BOARD_LCD_HEIGHT; j++)
    memcpy(&img[((size_t)(y + pad + j) * iw + x + pad) * 3], &panelImg[(size_t)j * BOARD_LCD_WIDTH * 3],
           (size_t)BOARD_LCD_WIDTH * 3);
}

// ---- scenes ------------------------------------------------------------------

// 2026-06-15 at the given hour (UTC, as the firmware treats the RTC)
static uint32_t epochAt(int hour) { return 1781481600UL + (uint32_t)hour * 3600; }

// Every scene starts from the same clock, random sequence and pet behaviour, so
// it renders the same whether it runs alone or after the others.
static void resetUi() {
  gHostMillis = 107915;  // the first frame then lands with the clouds clear of the title
  gHostRandom = 12345;
  beh = decltype(beh){};
  walkLast = 0;
  galleryOpen = cardOpen = kbOpen = clockOpen = gameOpen = sackOpen = zenOpen = dashOpen = false;
  zenSerialHold = -1;
  dashSerialSteerOn = false;
  dashTrailN = 0;
  // la caja (v1.37): cerrada, vacia y sin huevo esperando (newEgg() lo devuelve a su sitio)
  boxOpen = false;
  boxAsk = 0;
  boxMove = BOXMV_NONE;
  boxShakeUntil = 0;
  boxShakeSlot = -1;
  for (uint8_t k = 0; k < BOX_SLOTS; k++) pet.boxClear(k);
  if (pet.eggHeld()) pet.newEgg();
  pet.vacant = false;  // huevos limitados (v1.39): cada escena empieza con alguien en casa
  galleryDetail = 0;
  galleryPage = 0;
  cardPage = 0;
  menuUntil = confirmUntil = choiceUntil = bathUntil = 0;
  menuKind = MENU_NONE;
  closeBattle();
  bt = decltype(bt){};
  choiceKind = 0;
  pet.sleeping = false;
  pet.ceremony = CER_NONE;
  // paseo e IMU (v1.20)
  itemConfirm = 0;
  foundShown = false;
  while (pet.hasFound()) pet.popFound();
  pet.takeGoalBanner();
  pet.takeMedal();       // (v1.30) las celebraciones pendientes no pasan de una escena a otra
  pet.takeMilestone();
  pet.clearEmote();
  pet.stepsToday = pet.stepsTotal = 0;
  pet.findsToday = 0;
  pet.goalDay = 0;
  pet.goalTier = 0;
  pet.bonusFindsToday = 0;
  // efectos (fx.h, v1.30): nada pendiente de la escena anterior; las barras arrancan quietas
  fxq.clear();
  fxo.clear();
  fxBan = nullptr;
  for (auto &b : fxBar) b.init = false;
  fxBondGate = FxGate{};
  memset(pet.items, 0, sizeof(pet.items));
  gStubImuPresent = true;
  gStubTiltSide = 0;
  gStubTiltSlide = false;
  gStubVol = VOL_DEFAULT;
  gBright = BRIGHT_DEFAULT;
  gFaceDown = false;
  dimStage = DIM_AWAKE;
  roundCanvas->setShift(0, 0);
  briToastUntil = briSaveAt = 0;
  screenKeyValid = false;
  if (gLang != LANG_EN) { setLang(LANG_EN); applyLangFont(); }
}

// what the loop does each pass for the effects (fx.h): the watches note the pet as it is now,
// then anything pending (a goal, a medal, a level the scene added) goes into the queue
static void collect() {
  fxArmWatches();
  fxCollect(millis());
}

// a level-up through the real path: the watch sees the level rise, as in the loop
static void levelUp(uint16_t levels) {
  fxArmWatches();
  pet.growMin = addLevels(pet.growMin, levels);
  fxCollect(millis());
}

// steps and Backpack for the WALK page scenes
static void walker(uint32_t today, uint32_t total, uint8_t finds, std::initializer_list<uint8_t> items,
                   uint8_t bonus = 0) {
  pet.stepDay = pet.dayNumber();
  pet.stepsToday = today;
  pet.stepsTotal = total;
  pet.findsToday = finds;
  pet.bonusFindsToday = bonus;
  int i = 0;
  for (uint8_t n : items) pet.items[i++] = n;
  pet.goalTier = 0;
  while (pet.goalTier < STEP_GOAL_TIERS && today >= STEP_GOALS[pet.goalTier]) pet.goalTier++;
  if (pet.goalTier) pet.goalDay = pet.dayNumber();
}

// a hatched pet of the given species, healthy, `hour` o'clock
static void pokemon(int16_t dex, int hour, bool shiny = false) {
  resetUi();
  if (pet.awaitingStarter()) pet.chooseStarter(dex);  // a fresh save asks for a starter first
  pet.speciesId = dex;
  pet.shiny = shiny;
  pet.ageMinutes = 14 * 60 + 20;                 // age: 14 h 20 min
  pet.growMin = levelStart(15) + levelCost(15) / 3;  // Lv.15, a third of the way to 16
  pet.fullness = 72;
  pet.joy = 88;
  pet.energy = 64;
  pet.hygiene = 91;
  pet.poops = 0;
  pet.careMistakes = 0;
  pet.lastSeenEpoch = epochAt(hour);
  pet.streak = 5;
  pet.bestStreak = 9;
  pet.bond = 42;
  pet.geneAtk = 104;
  pet.geneDef = 97;
  pet.geneSpe = 108;
  pet.trAtk = 6;
  pet.trSpe = 9;
  for (int d : { 1, 4, 7, 10, 16, 19, 25, 37, 52, 54, 58, 63, 79, 92, 129, 133 }) {
    pet.dexReg[(d - 1) >> 3] |= (uint8_t)(1 << ((d - 1) & 7));
  }
  ensureMon();
}

// the waterfall mid-session: 12 s in, the gauge at c, 9.4 s of focus, a 23.4 s record
// the sled run: calibrated, `t0` ms in (its speed), then `ms` of a player who heads for each gap,
// with the runner tracks filled as dashTick() does; `steer` is the lean shown in the frame
static void dashPlaying(int16_t dex, int hour, uint32_t t0, uint32_t ms, float steer = 0) {
  pokemon(dex, hour);
  startDash();
  dashCalibrating = false;
  dashRunStart = millis() - 5000;  // past the "tilt to steer" hint
  dash.t = t0;
  pet.dashHi = 57;
  for (uint32_t e = 0; e < ms; e += 20) {
    const DashRow *next = nullptr;
    for (const DashRow &r : dash.rows)
      if (r.live && !r.counted && (!next || r.y < next->y)) next = &r;
    float st = 0;
    if (next) {
      float dx = next->gapX - dash.px;
      st = dx > 3 ? 0.8f : (dx < -3 ? -0.8f : 0);
    }
    dashAdvance(dash, st, 20);
    while (dashTrailN == 0 || dash.dist - dashTrailY[(dashTrailN - 1) % DASH_TRAIL] >= DASH_TRAIL_STEP) {
      dashTrailX[dashTrailN % DASH_TRAIL] = dash.px;
      dashTrailY[dashTrailN % DASH_TRAIL] = dash.dist;
      dashTrailN++;
    }
  }
  dash.steer = steer;
}

static void zenPlaying(int16_t dex, int hour, float c) {
  pokemon(dex, hour);
  startZen();
  zenIntroUntil = 0;
  zenTouched = true;
  zen.t = 12000;
  zen.nextSurge = 14000;
  zen.c = c;
  zen.inBandMs = 9400;
  pet.zenHi = 234;
}

// a pet in box slot k (v1.37): Lv `lv`, asleep in the box as every stored pet
static void boxPet(uint8_t k, int16_t dex, uint16_t lv, bool shiny = false, const char *nick = "") {
  PetRecord r;
  memset(&r, 0, sizeof(r));
  r.speciesId = dex;
  r.growMin = levelStart(lv) + 5;
  r.ageMinutes = r.growMin + 45;
  r.flags = shiny ? REC_SHINY : 0;
  strncpy(r.nick, nick, sizeof(r.nick) - 1);
  r.fullness = r.joy = r.energy = 70;
  r.hygiene = 90;
  r.geneAtk = r.geneDef = r.geneSpe = 100;
  pet.boxPut(k, r);
}

// three pets in the box, one of them shiny and one with a nickname
static void boxThree() {
  boxPet(0, 7, 12);
  boxPet(1, 133, 23, true);
  boxPet(4, 6, 41, false, "BLAZE");
}

// the egg screen (not a starter pick), as "saver-egg-night" sets it
static void eggNow() {
  resetUi();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  pet.speciesId = -1;
  pet.lastSeenEpoch = epochAt(10);
}

// the empty home (v1.39): no pet and no egg, 3200 steps today, at `hour`
static void vacantNow(int hour) {
  eggNow();
  pet.vacant = true;
  pet.lastSeenEpoch = epochAt(hour);
  pet.stepDay = pet.dayNumber();
  pet.stepsToday = 3200;
  for (int d : { 1, 4, 7, 16, 19, 25, 54, 129, 133 }) pet.dexReg[(d - 1) >> 3] |= (uint8_t)(1 << ((d - 1) & 7));
}

// steps the clock 85 ms at a time through the battle logic until `done` (or 60 s)
static void battleUntil(bool (*done)()) {
  for (int i = 0; i < 700 && !done(); i++) {
    gHostMillis += 85;
    battleStep(millis());
  }
}

struct Scene {
  const char *name;
  std::function<void()> setup;
  int frames = 1;  // frames to step before the capture (animations)
};

static const std::vector<Scene> &scenes() {
  static const std::vector<Scene> s = {
    { "starter", [] { resetUi(); /* fresh save: the game asks for a starter */ } },
    { "egg", [] {
       resetUi();
       pet.chooseStarter(4);
       pet.eggTap();
       pet.eggTap();
       pet.lastSeenEpoch = epochAt(10);
     } },
    { "home", [] { pokemon(25, 13); } },
    { "home-dusk", [] { pokemon(7, 19); } },
    { "home-night", [] { pokemon(133, 23); pet.sleeping = true; } },
    { "home-forest", [] { pokemon(1, 9); pet.poops = 2; pet.fullness = 20; } },
    // the swaying tall-grass meadow (v1.26): mid-gust, at dusk
    { "home-gust", [] { pokemon(19, 11); gHostMillis = 114000; } },  // wind.h: the gust is mid-screen at 6 s of 12
    { "home-meadow-dusk", [] { pokemon(19, 19); } },
    // burn-in (v1.28): the screensaver after 2 min idle, and the picture moved by the orbit
    { "saver", [] { pokemon(25, 13); dimStage = DIM_OFF; }, 2 },
    // v1.31: asleep the clock keeps its usual gray (it was a darker one)
    { "saver-sleep", [] { pokemon(25, 2); pet.sleeping = true; dimStage = DIM_OFF; }, 2 },
    { "saver-egg-night", [] {
       resetUi();
       if (pet.awaitingStarter()) pet.chooseStarter(4);
       pet.speciesId = -1;
       pet.lastSeenEpoch = epochAt(23) + 17 * 60;
       gHostMillis = 190000;
       dimStage = DIM_OFF;
     }, 2 },
    { "home-orbit", [] { pokemon(25, 13); OrbitShift o = orbitShift(6); roundCanvas->setShift(o.dx, o.dy); }, 2 },
    // new biomes (v1.27): power plant (electric), swamp (poison), ruins (ghost, psychic)
    { "home-power-night", [] { pokemon(25, 22); }, 3 },
    { "home-swamp", [] { pokemon(23, 11); } },
    { "home-swamp-night", [] { pokemon(88, 23); } },
    { "home-ruins", [] { pokemon(92, 14); } },
    { "home-ruins-night", [] { pokemon(63, 23); } },
    { "minigame-swamp", [] { pokemon(23, 13); startGame(); gameScore = 3; }, 4 },
    { "feed", [] { pokemon(133, 12); menuKind = MENU_FEED; menuUntil = millis() + 6000; } },
    { "play-menu", [] { pokemon(133, 12); menuKind = MENU_PLAY; menuUntil = millis() + 6000; } },
    // the sled run (v1.44): calibration (and "hold still"), running, fast, a hit, leaning, tiny / huge /
    // flying pets, the snow and volcano habitats, night, the result, Japanese, and the menu without an IMU
    { "dash-cal", [] { pokemon(123, 13); startDash(); } },
    { "dash-cal-still", [] { pokemon(123, 13); startDash(); dashShaky = true; } },
    { "dash-run", [] { dashPlaying(123, 13, 0, 6000); } },
    { "dash-run-lean", [] { dashPlaying(25, 13, 0, 7000, -1); } },
    { "dash-fast", [] { dashPlaying(25, 13, 50000, 4000, 0.6f); } },
    { "dash-hit", [] { dashPlaying(25, 13, 10000, 3000); dash.hits = 1; dash.lastHitT = dash.t - 100; dash.invulnUntil = dash.t + 1000; } },
    { "dash-tiny", [] { dashPlaying(10, 13, 0, 5000, 0.5f); } },
    { "dash-huge", [] { dashPlaying(95, 13, 0, 5000, -0.5f); } },
    { "dash-flying", [] { dashPlaying(16, 13, 0, 5000); } },
    { "dash-snow", [] { dashPlaying(124, 13, 20000, 5000, 1); } },
    { "dash-volcano", [] { dashPlaying(126, 13, 20000, 5000); } },
    { "dash-night", [] { dashPlaying(123, 22, 30000, 5000, -1); } },
    { "dash-result", [] { dashPlaying(123, 13, 0, 6000); dash.dodged = 61; dashNewHi = true; dashGainShown = 15; dash.hits = 3;
                          dashOverUntil = millis() + 4000; } },
    { "dash-ja", [] { dashPlaying(123, 13, 0, 6000); dashRunStart = millis(); setLang(LANG_JA); applyLangFont(); } },
    { "play-menu-no-imu", [] { pokemon(133, 12); gStubImuPresent = false; menuKind = MENU_PLAY; menuUntil = millis() + 6000; } },
    // the waterfall (v1.43): intro, in the band, below (spray), above, a surge on its way, the result
    { "zen-intro", [] { pokemon(7, 13); startZen(); } },
    { "zen-band", [] { zenPlaying(7, 13, 52); } },
    { "zen-low", [] { zenPlaying(7, 13, 20); zen.surges = 1; zen.lastSurge = zen.t - 120; } },
    { "zen-high", [] { zenPlaying(7, 13, 86); } },
    { "zen-warn", [] { zenPlaying(7, 13, 50); zen.nextSurge = zen.t + 200; } },
    { "zen-warn-early", [] { zenPlaying(25, 13, 50); zen.nextSurge = zen.t + 380; } },
    { "zen-late-night", [] { zenPlaying(143, 22, 49); zen.t = 27000; } },
    // v1.47: from 30 s the band drifts and its focus gives bonus DEF
    { "zen-drift", [] { zenPlaying(7, 13, 68); zen.driftDir = 1; zen.t = 32500; zen.driftInBandMs = 2100; } },
    { "zen-result-bonus", [] { zenPlaying(7, 13, 50); zen.t = ZEN_SESSION_MS; zen.inBandMs = 38200; zenGainShown = 22;
                               zenBonusShown = 8; zenNewHi = true; zenOverUntil = millis() + 4000; } },
    { "zen-result-max", [] { zenPlaying(7, 13, 50); pet.trDef = 100; zen.t = ZEN_SESSION_MS; zen.inBandMs = 30000;
                             zenGainShown = 0; zenBonusShown = 5; zenOverUntil = millis() + 4000; } },
    { "sack-max", [] { pokemon(123, 13); startSack(); pet.trAtk = 100; sackHits = 31; sackGain = 0; sackOverUntil = millis() + 3000; } },
    { "zen-result", [] { zenPlaying(7, 13, 50); zen.t = ZEN_SESSION_MS; zen.inBandMs = 23400; zenGainShown = 11;
                         zenNewHi = true; zenOverUntil = millis() + 4000; } },
    { "zen-result-best", [] { zenPlaying(7, 13, 50); zen.t = ZEN_SESSION_MS; zen.inBandMs = 9100; zenGainShown = 4;
                              zenOverUntil = millis() + 4000; } },
    { "zen-ja", [] { zenPlaying(7, 13, 52); setLang(LANG_JA); applyLangFont(); zenTouched = false; } },
    { "zen-result-ko", [] { zenPlaying(7, 13, 50); setLang(LANG_KO); applyLangFont(); zen.t = ZEN_SESSION_MS;
                            zen.inBandMs = 23400; zenGainShown = 11; zenOverUntil = millis() + 4000; } },
    // no egg and nobody in the Box: a gift egg will come (v1.40); with pets in the Box, the home stays empty
    { "release", [] { pokemon(54, 15); confirmUntil = millis() + 10000; } },
    { "release-boxed", [] { pokemon(54, 15); boxThree(); confirmUntil = millis() + 10000; } },
    { "release-egg", [] { pokemon(54, 15); pet.items[IT_EGG] = 1; confirmUntil = millis() + 10000; } },
    // limited eggs (v1.39): the empty home, with pets in the box (the hint) and at night without
    { "home-vacant", [] { vacantNow(10); boxThree(); } },
    { "home-vacant-night", [] { vacantNow(22); } },
    { "home-vacant-ja", [] { vacantNow(10); boxThree(); setLang(LANG_JA); applyLangFont(); } },
    { "home-vacant-de", [] { vacantNow(10); boxThree(); setLang(LANG_DE); applyLangFont(); } },
    { "release-de", [] { pokemon(54, 15); setLang(LANG_DE); applyLangFont(); confirmUntil = millis() + 10000; } },
    // the empty home with eggs in the Backpack: the "Hatch an egg (N)" button
    { "home-vacant-eggs", [] { vacantNow(10); boxThree(); pet.items[IT_EGG] = 3; } },
    { "home-vacant-eggs-ja", [] { vacantNow(10); boxThree(); pet.items[IT_EGG] = 3; setLang(LANG_JA); applyLangFont(); } },
    { "home-vacant-eggs-es", [] { vacantNow(10); boxThree(); pet.items[IT_EGG] = 3; setLang(LANG_ES); applyLangFont(); } },
    // a farewell's egg (v1.41): "GIFT!" over the empty home, whose button now offers it
    { "farewell-gift", [] {
       pokemon(54, 15); boxThree(); pet.startFarewell();
       gHostMillis += CEREMONY_MS + 1; pet.update(millis());
     }, 2 },
    { "walk-eggs-pt", [] {
       pokemon(25, 13);
       walker(6420, 88100, 4, { 3, 1, 0, 2, 1, 1, 2 }, 2);
       cardOpen = true;
       cardPage = 4;
       setLang(LANG_PT);
       applyLangFont();
     } },
    { "saver-vacant", [] { vacantNow(23); dimStage = DIM_OFF; }, 2 },
    // an egg found while the home is empty: straight home, with the find popup
    { "found-egg", [] { vacantNow(10); pet.addSteps(EGG_MAX_STEPS, 0); }, 2 },
    { "box-vacant", [] { vacantNow(10); boxThree(); openBox(); } },
    { "box-wake-vacant-end", [] { vacantNow(10); boxThree(); openBox(); boxStartMove(4); }, 8 },
    { "box-store-vacant-end", [] { pokemon(25, 13); boxThree(); openBox(); boxStartMove(2); }, 8 },
    { "walk-eggs", [] {
       pokemon(25, 13);
       walker(6420, 88100, 4, { 3, 1, 0, 2, 1, 1, 2 }, 2);
       cardOpen = true;
       cardPage = 4;
     } },
    { "evolve", [] { pokemon(4, 11); pet.setLevel(18); choiceKind = 1; choiceUntil = millis() + 12000; } },
    { "card-profile", [] { pokemon(25, 13); cardOpen = true; cardPage = 0; } },
    { "card-battle", [] { pokemon(25, 13); cardOpen = true; cardPage = 1; } },
    // v1.49: barra fina de entrenamiento; un Gastly de nivel bajo con la FUE ya entrenada al tope
    { "card-battle-max", [] { pokemon(92, 6); pet.trAtk = 100; pet.trDef = 10; pet.trSpe = 13; cardOpen = true; cardPage = 1; } },
    { "card-battle-ja", [] { pokemon(92, 6); pet.trAtk = 100; pet.trDef = 55; setLang(LANG_JA); applyLangFont(); cardOpen = true; cardPage = 1; } },
    { "card-battle-ko", [] { pokemon(92, 6); pet.trAtk = 100; pet.trDef = 55; setLang(LANG_KO); applyLangFont(); cardOpen = true; cardPage = 1; } },
    { "card-battle-de", [] { pokemon(92, 6); pet.trAtk = 100; pet.trDef = 55; setLang(LANG_DE); applyLangFont(); cardOpen = true; cardPage = 1; } },
    { "card-battle-it", [] { pokemon(92, 6); pet.trAtk = 100; pet.trDef = 55; setLang(LANG_IT); applyLangFont(); cardOpen = true; cardPage = 1; } },
    { "card-battle-es", [] { pokemon(92, 6); pet.trAtk = 100; pet.trDef = 55; setLang(LANG_ES); applyLangFont(); cardOpen = true; cardPage = 1; } },
    { "card-medals", [] { pokemon(25, 13); cardOpen = true; cardPage = 2; } },
    { "card-progress", [] { pokemon(25, 13); cardOpen = true; cardPage = 3; } },
    { "settings", [] { pokemon(25, 13); openClock(); } },
    { "settings-bright-max", [] { pokemon(25, 13); openClock(); gBright = BRIGHT_LEVELS - 1; } },
    // volume pill: 4 levels including off (v1.50)
    { "settings-es", [] { pokemon(25, 13); setLang(LANG_ES); applyLangFont(); openClock(); } },
    { "settings-vol-off", [] { pokemon(25, 13); openClock(); gStubVol = 0; gBright = 0; } },
    { "settings-vol-low-es", [] { pokemon(25, 13); setLang(LANG_ES); applyLangFont(); openClock(); gStubVol = 1; } },
    { "settings-vol-max", [] { pokemon(25, 13); openClock(); gStubVol = VOL_LEVELS - 1; gBright = BRIGHT_LEVELS - 1; } },
    // brightness toast after a BOOT press (v1.23)
    { "bright-toast", [] { pokemon(25, 13); gBright = 0; cycleBright(); } },
    { "keyboard", [] { pokemon(25, 13); openKeyboard(); } },
    { "gallery", [] { pokemon(25, 13); galleryOpen = true; galleryDirty = true; } },
    { "gallery-detail", [] {
       pokemon(25, 13);
       galleryOpen = true;
       galleryDetail = 25;  // what galleryTap() does for a registered cell
       galleryPmd.load(25, false);
       galleryDirty = true;
     } },
    { "minigame", [] { pokemon(25, 13); startGame(); gameScore = 7; }, 9 },
    { "sack", [] { pokemon(66, 13); startSack(); sackHits = 12; }, 3 },
    { "home-ja", [] { pokemon(25, 13); setLang(LANG_JA); applyLangFont(); } },
    { "home-streak", [] { pokemon(130, 16); pet.streak = 365; } },  // widest status row
    // steps and the Backpack (Task 1)
    { "walk-empty", [] { pokemon(25, 13); cardOpen = true; cardPage = 4; } },
    { "walk-mid", [] { pokemon(25, 13); walker(1840, 52310, 2, { 2, 1, 0, 3, 0 }); cardOpen = true; cardPage = 4; } },
    { "walk-goal", [] { pokemon(25, 13); walker(3512, 61200, 4, { 4, 0, 9, 1, 1 }, 1); cardOpen = true; cardPage = 4; } },
    // the 6000 and 10000 goals and the Rare Candy (v1.25)
    { "walk-6000", [] { pokemon(25, 13); walker(7240, 65000, 6, { 4, 0, 9, 1, 1, 0 }, 2); cardOpen = true; cardPage = 4; } },
    { "walk-10000", [] { pokemon(25, 13); walker(11032, 71500, 8, { 5, 1, 9, 2, 1, 1 }, 3); cardOpen = true; cardPage = 4; } },
    { "walk-candy-confirm", [] {
       pokemon(25, 13);
       walker(11032, 71500, 8, { 5, 1, 9, 2, 1, 1 }, 3);
       cardOpen = true;
       cardPage = 4;
       itemConfirm = IT_RARECANDY + 1;
       itemConfirmUntil = millis() + 10000;
     } },
    { "goal-10000", [] {
       pokemon(7, 10);
       pet.lastCareDay = pet.dayNumber();
       pet.medals = (1 << MED_COUNT) - 1;  // every medal already won: only the goal banner on top
       pet.addSteps(STEP_GOALS[STEP_GOAL_TIERS - 1], pet.dayNumber());
       while (pet.hasFound()) pet.popFound();
       collect();
     }, 5 },
    // v1.36: each goal gives levels ("+1 Lv" on the 3000 banner); the 10000 one no longer
    // gives a Rare Candy, so the old found-candy scene is gone
    { "goal-3000", [] {
       pokemon(7, 10);
       pet.lastCareDay = pet.dayNumber();
       pet.medals = (1 << MED_COUNT) - 1;  // every medal already won: only the goal banner on top
       pet.addSteps(STEP_GOALS[0], pet.dayNumber());
       while (pet.hasFound()) pet.popFound();
       collect();
     }, 5 },
    { "card-medals-9", [] { pokemon(25, 13); pet.medals = MED_LV10 | MED_BERRY | MED_MARATHON; cardOpen = true; cardPage = 2; } },
    { "walk-confirm", [] {
       pokemon(25, 13);
       walker(1840, 52310, 2, { 2, 1, 0, 3, 0 });
       cardOpen = true;
       cardPage = 4;
       itemConfirm = IT_GOLDEN + 1;
       itemConfirmUntil = millis() + 10000;
     } },
    { "walk-noimu", [] { pokemon(25, 13); gStubImuPresent = false; cardOpen = true; cardPage = 4; } },
    { "walk-ja", [] {
       pokemon(25, 13);
       walker(1840, 52310, 2, { 2, 1, 0, 3, 1 });
       cardOpen = true;
       cardPage = 4;
       setLang(LANG_JA);
       applyLangFont();
     } },
    { "found", [] { pokemon(25, 13); pet.addSteps(FIND_MAX_STEPS, pet.dayNumber()); }, 2 },
    { "goal", [] {
       pokemon(7, 10);
       pet.lastCareDay = pet.dayNumber();  // already cared for today: no streak/medal banner on top
       pet.addSteps(STEP_GOAL, pet.dayNumber());
       collect();
     }, 4 },
    // feedback effects (fx.h, v1.30). Frames run 85 ms each: the count picks the moment shown
    { "levelup-slide", [] { pokemon(25, 13); levelUp(1); }, 1 },
    { "levelup", [] { pokemon(25, 13); levelUp(1); }, 7 },
    { "levelup-late", [] { pokemon(4, 16); levelUp(1); }, 18 },
    { "levelup-multi", [] { pokemon(7, 10); levelUp(6); }, 7 },
    { "levelup-gains", [] { pokemon(25, 13); levelUp(1); }, 32 },
    { "levelup-ja", [] { pokemon(25, 13); setLang(LANG_JA); applyLangFont(); levelUp(1); }, 7 },
    { "levelup-gains-ja", [] { pokemon(25, 13); setLang(LANG_JA); applyLangFont(); levelUp(1); }, 32 },
    { "levelup-ko", [] { pokemon(25, 13); setLang(LANG_KO); applyLangFont(); levelUp(1); }, 7 },
    { "levelup-gains-fr", [] { pokemon(25, 13); setLang(LANG_FR); applyLangFont(); levelUp(1); }, 32 },
    { "medal-slide", [] { pokemon(25, 13); fxq.push(FXB_MEDAL, 0); }, 1 },
    { "medal-confetti", [] { pokemon(25, 13); fxq.push(FXB_MEDAL, 0); }, 6 },
    { "streak-banner", [] { pokemon(25, 13); fxq.push(FXB_MILESTONE, 7); }, 4 },
    // (needs set by hand, not feedBerry(): its eating timer would leak into the next scenes,
    // which restart the host clock)
    { "stat-float", [] {
       pokemon(133, 12);
       pet.fullness = 40;
       FxNeedSnap ns = fxSnap();
       pet.fullness = 75;
       pet.joy = 98;
       fxNeeds(ns);
     }, 4 },
    { "stat-tween", [] {
       pokemon(133, 12);
       pet.fullness = 30;
       render();
       FxNeedSnap ns = fxSnap();
       pet.fullness = 65;
       fxNeeds(ns);
     }, 2 },
    { "bond-float", [] { pokemon(25, 13); fxo.push(FXO_BOND, 0, 2, millis()); }, 4 },
    { "train-float", [] { pokemon(66, 13); fxo.push(FXO_TRAIN, 0, 3, millis()); }, 5 },
    { "oops", [] { pokemon(25, 13); fxo.push(FXO_OOPS, 0, 0, millis()); }, 4 },
    { "button-press", [] { pokemon(25, 13); fxo.push(FXO_PRESS, 1, 0, millis()); }, 1 },
    { "button-deny", [] { pokemon(25, 13); pet.sleeping = true; fxo.push(FXO_DENY, 0, 0, millis()); }, 2 },
    { "hatch-shake", [] { pokemon(25, 13); fxo.push(FXO_HATCH, 0, 0, millis()); }, 6 },
    { "hatch-flash", [] { pokemon(25, 13); fxo.push(FXO_HATCH, 0, 0, millis()); }, 9 },
    { "hatch-reveal", [] { pokemon(25, 13); fxo.push(FXO_HATCH, 0, 0, millis()); }, 12 },
    // the box (v1.37): swipe right from home. Frames run 85 ms: the light flies for 600 ms
    { "box-empty", [] { pokemon(25, 13); openBox(); } },
    { "box", [] { pokemon(25, 13); boxThree(); openBox(); } },
    { "box-full", [] {
       pokemon(25, 13);
       const int16_t dex[BOX_SLOTS] = { 1, 4, 7, 16, 133, 143 };
       for (uint8_t k = 0; k < BOX_SLOTS; k++) boxPet(k, dex[k], 8 + k * 9, k == 3);
       openBox();
     } },
    { "box-damaged", [] {
       pokemon(25, 13);
       boxThree();
       Preferences raw;  // a slot whose CRC doesn't match: "?", never loaded
       raw.begin("tamapoke");
       PetRecord bad = pet.boxRecord(0);
       bad.joy ^= 1;
       raw.putBytes("box2", &bad, sizeof(bad));
       pet.boxReload();
       openBox();
     } },
    { "box-store-ask", [] { pokemon(25, 13); boxThree(); openBox(); boxAsk = 3; boxAskUntil = millis() + 10000; } },
    { "box-store-ask-egg", [] {  // an egg in the Backpack takes its place: no note (v1.39)
       pokemon(25, 13);
       pet.items[IT_EGG] = 1;
       boxThree();
       openBox();
       boxAsk = 3;
       boxAskUntil = millis() + 10000;
     } },
    { "box-wake-ask", [] { pokemon(25, 13); boxThree(); openBox(); boxAsk = 5; boxAskUntil = millis() + 10000; } },
    { "box-wake-ask-fr", [] {
       pokemon(25, 13);
       setLang(LANG_FR);
       applyLangFont();
       boxPet(0, 115, 30);  // Kangourex (9): the swap note at its longest
       strcpy(pet.nick, "SPARKYBOLTS");  // a nickname of 11 letters
       openBox();
       boxAsk = 1;
       boxAskUntil = millis() + 10000;
     } },
    { "box-egg", [] { eggNow(); boxThree(); openBox(); } },
    { "box-held", [] {
       eggNow();
       boxThree();
       pet.wake(1);  // woken with an egg active: the egg waits, shown asleep by "Now"
       ensureMon();
       openBox();
     } },
    { "box-store-fx", [] { pokemon(25, 13); boxThree(); openBox(); boxStartMove(2); }, 4 },
    { "box-swap-fx", [] { pokemon(25, 13); boxThree(); openBox(); boxStartMove(4); }, 4 },
    { "box-store-end", [] { pokemon(25, 13); boxThree(); openBox(); boxStartMove(2); }, 8 },
    { "box-ja", [] { pokemon(25, 13); setLang(LANG_JA); applyLangFont(); boxThree(); openBox(); } },
    { "box-ko", [] { pokemon(25, 13); setLang(LANG_KO); applyLangFont(); boxThree(); openBox(); boxAsk = 5; boxAskUntil = millis() + 10000; } },
    { "box-de", [] { pokemon(25, 13); setLang(LANG_DE); applyLangFont(); boxThree(); boxPet(5, 2, 17); openBox(); } },
    // back home: the pet that is now active comes out of a light (FXO_BOX)
    { "box-appear", [] { pokemon(6, 15); fxo.push(FXO_BOX, 0, 0, millis()); }, 3 },
    { "box-appear-egg", [] { eggNow(); fxo.push(FXO_BOX, 0, 0, millis()); }, 2 },
    // motion easter eggs (Task 2)
    { "grumpy", [] { pokemon(133, 23); pet.sleeping = true; pet.wakeGrumpy(); }, 4 },
    { "dizzy", [] { pokemon(25, 13); pet.shaken(); }, 4 },
    { "egg-wobble", [] {
       resetUi();
       if (pet.awaitingStarter()) pet.chooseStarter(4);
       pet.speciesId = -1;
       pet.lastSeenEpoch = epochAt(10);
       pet.shaken();
     }, 3 },
    { "tilt-walk", [] { pokemon(25, 13); gStubTiltSide = 1; }, 8 },
    { "tilt-slide", [] { pokemon(25, 13); gStubTiltSide = -1; gStubTiltSlide = true; }, 8 },
    // practice battle (Task 3a): driven to a given moment with the sketch's own battleStep()
    { "battle-vs", [] { pokemon(25, 13); startPractice(79); }, 2 },
    { "battle-walkin", [] { pokemon(25, 13); startPractice(79); }, 30 },
    { "battle-special", [] {
       pokemon(25, 13);
       startPractice(79);
       bt.meter.pips = SPECIAL_TAPS;  // charged before the first attack
       battleUntil([] { return bt.haveEv && bt.ev.special && millis() - bt.evAt >= BT_HIT_MS + 100; });
     }, 1 },
    { "battle-mid", [] {
       pokemon(25, 13);
       startPractice(79);
       battleUntil([] { return bt.eng.attacks >= 4 && millis() - bt.evAt >= BT_HIT_MS + 150; });
       bt.meter.pips = 3;
     }, 1 },
    { "battle-result", [] {
       pokemon(25, 13);
       startPractice(129);  // Magikarp: a win
       battleUntil([] { return bt.phase == BT_END; });
     }, 4 },
    // v1.38: "> > > > >" and the "Tap!" box from the first moment of the fight; three charged
    { "battle-tap", [] {
       pokemon(25, 13);
       startPractice(79);
       battleUntil([] { return bt.phase == BT_FIGHT; });
     }, 1 },
    { "battle-charging", [] {
       pokemon(25, 13);
       startPractice(79);
       battleUntil([] { return bt.eng.attacks >= 2 && millis() - bt.evAt >= BT_HIT_MS + 900; });
       bt.meter.pips = 3;
     }, 1 },
    { "battle-armed", [] {
       pokemon(25, 13);
       startPractice(79);
       battleUntil([] { return bt.eng.attacks >= 2 && millis() - bt.evAt >= BT_HIT_MS + 900; });
       bt.meter.pips = SPECIAL_TAPS;
     }, 1 },
    { "battle-locked", [] {  // after a special: grey outlines, no box
       pokemon(25, 13);
       startPractice(79);
       battleUntil([] { return bt.eng.attacks >= 2 && millis() - bt.evAt >= BT_HIT_MS + 900; });
       bt.meter.cooldown = SPECIAL_COOLDOWN;
     }, 1 },
    { "battle-tap-ja", [] {
       pokemon(4, 19);
       setLang(LANG_JA);
       applyLangFont();
       startPractice(7);
       battleUntil([] { return bt.phase == BT_FIGHT; });
       bt.meter.pips = 2;
     }, 1 },
    { "battle-tap-ko", [] {
       pokemon(4, 13);
       setLang(LANG_KO);
       applyLangFont();
       startPractice(7);
       battleUntil([] { return bt.phase == BT_FIGHT; });
     }, 1 },
    // the result ignores taps for BT_END_LOCK_MS: "battle-result" is inside the lock (no "tap: back"),
    // "battle-result-open" just after it
    { "battle-result-open", [] {
       pokemon(25, 13);
       startPractice(129);
       battleUntil([] { return bt.phase == BT_END; });
       gHostMillis += BT_END_LOCK_MS;
     }, 1 },
    { "battle-draw", [] {
       pokemon(19, 20);
       startPractice(92);  // Rattata vs Gastly: neither can touch the other
       battleUntil([] { return bt.phase == BT_END; });
     }, 4 },
    { "battle-ja", [] {
       pokemon(4, 19);
       setLang(LANG_JA);
       applyLangFont();
       startPractice(7);
       battleUntil([] { return bt.eng.attacks >= 3 && millis() - bt.evAt >= BT_HIT_MS + 150; });
     }, 1 },
  };
  return s;
}

// the README's contact sheet: a few screens on the square glass
static const char *const SHEET[] = { "home", "home-night", "card-profile", "gallery", "minigame", "settings" };

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s <out_dir> [scene ...]\n", argv[0]);
    return 2;
  }
  std::string out = argv[1];
  mkdir(out.c_str(), 0755);

  // the same display bring-up as setup(): panel, canvas, black border, frame
  if (!gfx->begin(BOARD_LCD_QSPI_HZ) || !panel->begin()) return 1;
  roundDisplayBegin(bus, panel, roundCanvas);
  roundCanvas->overlay = drawBrightToast;  // as setup() does
  applyLangFont();
  pet.begin();
  sdBegin();
  thumbs.load();
  if (!sdReady) fprintf(stderr, "note: no sprite folder found, rendering the flash fallback sprites\n");

  std::vector<std::string> want(argv + 2, argv + argc);
  std::vector<std::pair<std::string, std::vector<uint8_t>>> shots;
  for (const Scene &sc : scenes()) {
    bool all = want.empty();
    bool picked = all;
    for (auto &w : want) picked |= (w == sc.name);
    if (!picked) continue;
    sc.setup();
    for (int i = 0; i < sc.frames; i++) {
      gHostMillis += 85;
      screenKeyValid = false;
      render();
    }
    std::vector<uint8_t> rgb = panelRgb();
    std::string path = out + "/" + sc.name + ".png";
    if (!writePng(path, rgb, BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT)) {
      fprintf(stderr, "cannot write %s\n", path.c_str());
      return 1;
    }
    printf("%s\n", path.c_str());
    shots.emplace_back(sc.name, std::move(rgb));
  }

  if (want.empty()) {
    const int n = sizeof(SHEET) / sizeof(SHEET[0]), cols = 3, gap = 28;
    const int cell = BOARD_LCD_WIDTH + 36, rows = (n + cols - 1) / cols;
    const int w = cols * cell + (cols + 1) * gap, h = rows * cell + (rows + 1) * gap;
    std::vector<uint8_t> sheet((size_t)w * h * 3);
    fillRect(sheet, w, 0, 0, w, h, 0x2B2F36);
    for (int i = 0; i < n; i++)
      for (auto &s : shots)
        if (s.first == SHEET[i])
          pasteOnGlass(sheet, w, gap + (i % cols) * (cell + gap), gap + (i / cols) * (cell + gap), s.second);
    std::string path = out + "/screens.png";
    if (!writePng(path, sheet, w, h)) return 1;
    printf("%s\n", path.c_str());
  }
  return 0;
}
