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
  galleryOpen = cardOpen = bagOpen = kbOpen = clockOpen = gameOpen = sackOpen = zenOpen = dashOpen = false;
  yardDemoOpen = false;  // el patio (v2.0)
  yardDemoPage = 0;
#if TP_YARD
  // el patio (v2.0, Y2): bichos sin colocar, sin cosas en el suelo y el mapa del bioma
  yardW = YardWorld{};
  yardLastStep = 0;
  yardPoopHave = 0;
  yardPoopBooted = false;
  yardGiftN = 0;
  yardGiftTapped = -1;
  yardBubblePrev = yardBubbleLatched = nullptr;
  yardBubbleUntil = 0;
  if (yardMapForced) yardForceMap(0);
#endif
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
  // fases (v1.65): menus cerrados, sin insignias, sin premios cobrados y sin equipo anterior
  stagesOpen = teamOpen = false;
  boxPage = 0;
  teamPickN = 0;
  stShakeUntil = 0;
  stShakeIdx = -1;
  pet.setBadges(0);
  pet.clearStageDay();
  pet.setTeam(nullptr, 0);
  confirmUntil = choiceUntil = bathUntil = poopPuffUntil = 0;
  panelKind = PANEL_NONE;  // paneles Cuidar / Jugar (v2.0, Y2)
  wasPressed = holdFired = swallowGesture = false;
  pet.berryKnown = false;  // (v1.57) la favorita no pasa de una escena a otra
  panelShakeIdx = -1;
  panelShakeUntil = 0;
  closeBattle();
  bt = decltype(bt){};
  pet.setBadges(0);      // estadios: sin insignias, nada cobrado hoy y sin equipo guardado
  pet.clearStageDay();
  pet.setTeam(nullptr, 0);
  choiceKind = 0;
  pet.sleeping = false;
  pet.ceremony = CER_NONE;
  pet.dbgClearDying();  // v1.60: nadie empieza muriendose
  revivedMsgUntil = bagReviveHintUntil = 0;
  // paseo e IMU (v1.20)
  itemConfirm = 0;
  foundShown = false;
  cakeHintUntil = 0;
  pet.satedUntil = 0;  // la tarta (v1.54) no pasa de una escena a otra
  pet.dbgClearGifts();  // ni los regalos (v1.55)
  tastesMsgUntil = 0;
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

// a tap (or a finger held down) on card i of the open Care / Play panel (v2.0, Y2)
static void panelTapCard(uint8_t i) {
  int x, y, w, h;
  panelCard(panelKind, i, x, y, w, h);
  onTap(x + w / 2, y + h / 2);
}
static void panelHold(uint8_t i) {
  int x, y, w, h;
  panelCard(panelKind, i, x, y, w, h);
  tX0 = tXl = x + w / 2;
  tY0 = tYl = y + h / 2;
  wasPressed = true;
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
  pet.dbgSetPoops(0);
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

// stage menus (v1.65). `badges` = the earned mask, `paid` = stages whose boss reward was already taken today
static void stageSetup(uint8_t badges, uint8_t paid, bool es) {
  if (es) { setLang(LANG_ES); applyLangFont(); }
  if (!pet.uid && !pet.isEgg()) pet.uid = 0x5EED0001;  // the scenes set the species directly: no hatch, so no uid
  for (uint8_t i = 0; i < STAGE_COUNT; i++)
    if (paid & (1u << i)) pet.stageDone(i, true, &pet.uid, 1);
  pet.setBadges(badges);
}
// the team picker: the last team is set from `picks` (-1 = the home pet, k = Box slot k), then the picker opens
static void teamSetup(std::initializer_list<int> picks, uint8_t stage, bool es) {
  stageSetup(0x7F, 0, es);
  uint32_t u[TEAM_MAX] = { 0 };
  uint8_t n = 0;
  for (int k : picks) u[n++] = k < 0 ? pet.uid : pet.boxRecord((uint8_t)k).uid;
  pet.setTeam(u, n);
  openStages();
  openTeam(stage);
}
static void boxSix() {
  const int16_t dex[BOX_SLOTS] = { 1, 4, 7, 16, 133, 143 };
  for (uint8_t k = 0; k < BOX_SLOTS; k++) boxPet(k, dex[k], 8 + k * 9, k == 3);
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

// the language of a scene, set after pokemon() (which goes back to English)
static void lang(Lang l) {
  setLang(l);
  applyLangFont();
}

// a stage run from a scene, with the sketch's own startStage(): `order` lists the team, -1 = the home
// pet, k = box slot k
static uint32_t gTeam[TEAM_MAX];
static uint8_t gTeamN = 0;
static void stageGo(uint8_t stage, std::initializer_list<int> order) {
  if (!pet.uid) pet.uid = 0x7E57u;  // pokemon() sets the species by hand: the pet never hatched, no uid
  gTeamN = 0;
  for (int k : order)
    if (gTeamN < TEAM_MAX) gTeam[gTeamN++] = k < 0 ? pet.uid : pet.boxRecord((uint8_t)k).uid;
  startStage(stage, gTeam, gTeamN);
}

// the run put straight into a fight: the allies before `ally` fainted, the foes before `foe` beaten,
// the two on the field at allyPct / foePct % of their HP, the fight just starting (sprites loaded as
// the hand-over does)
static void stageJump(uint8_t ally, uint8_t foe, uint8_t allyPct, uint8_t foePct) {
  StageRun &r = bt.run;
  if (ally >= r.teamN) ally = r.teamN - 1;
  for (uint8_t i = 0; i < ally; i++) r.team[i].hp = 0;
  r.ally = ally;
  r.team[ally].hp = (uint16_t)std::max(1, r.team[ally].maxHp * allyPct / 100);
  r.foeIdx = foe;
  r.foe = stageFoeFighter(r.foes[foe]);
  r.foe.hp = (uint16_t)std::max(1, r.foe.maxHp * foePct / 100);
  r.meter = SpecialMeter{};
  r.foeMeter = SpecialMeter{};
  r.eng.beginCarry(r.team[ally], r.foe, 777);
  battleLoadFoe();
  battleLoadAlly();
  bt.haveEv = false;
  bt.phase = BT_FIGHT;
  bt.t0 = millis();
}

// a fight that ends in one KO (one side at 1 HP), then 1.3 s into the hand-over: the caption and
// the newcomer half-way in (the capture frame adds 85 ms)
static void stageSwap(uint8_t ally, uint8_t foe, uint8_t allyPct, uint8_t foePct) {
  stageJump(ally, foe, allyPct, foePct);
  battleUntil([] { return bt.phase == BT_SWAP; });
  gHostMillis += 1300 - 85;
}

// the last KO of the run, then `ms` into the result. The hourly joy depends on which scenes ran
// before (Pet keeps its clock): it is shown as due
static void stageResult(uint8_t ally, uint8_t foe, uint8_t allyPct, uint8_t foePct, uint32_t ms) {
  stageJump(ally, foe, allyPct, foePct);
  battleUntil([] { return bt.phase == BT_END; });
  bt.rw.joy = PRACTICE_JOY;
  gHostMillis += ms;
}

#if TP_YARD
// el de casa andando en el patio (Y3): desde la celda libre mas cercana a (CX, fromY) hasta la mas cercana
// a (CX, toY); los frames de la escena lo llevan un trecho por el camino
static void yardWalkScene(uint8_t dex, int fromY, int toY) {
  pokemon(dex, 13);
  yardForceMap(1);
  render();  // mete al bicho en el patio
  int from = yardNearestFreeCell(yardWalk, CX, fromY), to = yardNearestFreeCell(yardWalk, CX, toY);
  YardPet &p = yardW.p[0];
  p.x = (float)yardCellCenter(yardCellCX(from));
  p.y = (float)yardCellCenter(yardCellCY(from));
  yardWalkTo(yardW, yardWalk, 0, to, millis());
}
#endif

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
#if TP_YARD
    // el patio (v2.0, Y2): la casa es el mapa. Pradera de dia, de noche, al atardecer, la cantera, sin mapa,
    // con un bicho de la caja, cacas, regalos, el globo que pasa de una necesidad a otra, dormido y el dock
    { "yard-home", [] { pokemon(25, 13); yardForceMap(1); } },
    { "yard-home-night", [] { pokemon(133, 22); } },
    { "yard-home-dusk", [] { pokemon(1, 19); } },
    { "yard-home-quarry", [] { pokemon(74, 11); } },
    { "yard-home-fallback", [] { pokemon(25, 13); yardForceMap(YARD_MAP_NONE); } },
    { "yard-home-fallback-night", [] { pokemon(7, 23); pet.sleeping = true; yardForceMap(YARD_MAP_NONE); } },
    { "yard-home-two", [] { pokemon(25, 13); yardForceMap(1); boxPet(0, 7, 12); }, 60 },
    { "yard-home-two-quarry", [] { pokemon(74, 15); boxPet(0, 133, 20, true); }, 120 },
    { "yard-home-poop", [] { pokemon(1, 10); pet.dbgSetPoops(3); pet.hygiene = 40; } },
    { "yard-poop-tap", [] { pokemon(1, 10); pet.dbgSetPoops(3); pet.hygiene = 40; render();
                            onTap(yardPoopX[1], yardPoopY[1] - 20); }, 3 },
    { "yard-home-gifts", [] { pokemon(25, 13); yardForceMap(1); for (int i = 0; i < 5; i++) pet.addGift(IT_CAKE); } },
    { "yard-bubble-cycle-0", [] { pokemon(25, 13); yardForceMap(1); pet.fullness = 10; pet.hygiene = 12; gHostMillis = 200000 - 85; } },
    { "yard-bubble-cycle-1", [] { pokemon(25, 13); yardForceMap(1); pet.fullness = 10; pet.hygiene = 12; gHostMillis = 201000 - 85; } },
    { "yard-bubble-cycle-0-es", [] { pokemon(25, 13); yardForceMap(1); pet.fullness = 10; pet.hygiene = 12; gHostMillis = 200000 - 85;
                                     lang(LANG_ES); } },
    { "yard-bubble-cycle-1-es", [] { pokemon(25, 13); yardForceMap(1); pet.fullness = 10; pet.hygiene = 12; gHostMillis = 201000 - 85;
                                     lang(LANG_ES); } },
    { "yard-home-asleep", [] { pokemon(25, 23); pet.sleeping = true; yardForceMap(1); boxPet(0, 7, 12); } },
    { "yard-home-asleep-es", [] { pokemon(25, 23); pet.sleeping = true; yardForceMap(1); lang(LANG_ES); } },
    { "yard-sleepy-es", [] { pokemon(25, 22); yardForceMap(1); lang(LANG_ES); } },
    { "yard-abandoned-es", [] { pokemon(4, 13); pet.fullness = pet.joy = pet.energy = pet.hygiene = 0; pet.bond = 60; lang(LANG_ES); } },
    { "yard-heart", [] { pokemon(133, 12); pet.feedBerry(0); gHostMillis += EAT_ANIM_MS + 100; }, 2 },
    { "yard-bath", [] { pokemon(25, 13); yardForceMap(1); startBath(); }, 8 },
    { "yard-release-ask", [] { pokemon(25, 13); yardForceMap(1); confirmUntil = millis() + 5000; } },
    { "yard-evolve-btn", [] { pokemon(4, 11); pet.setLevel(18); pet.trAtk = pet.trDef = pet.trSpe = 40; } },
    { "yard-farewell-btn", [] { pokemon(55, 15); pet.dbgSetAge(FAREWELL_AGE_MIN + 60); pet.addGift(IT_CAKE); } },
    { "yard-dock-care", [] { pokemon(25, 13); yardForceMap(1); render(); onTap(yDockX(0), Y_DOCK_Y); } },
    { "yard-dock-play", [] { pokemon(25, 13); yardForceMap(1); render(); onTap(yDockX(1), Y_DOCK_Y); } },
    { "yard-dock-care-es", [] { pokemon(25, 13); yardForceMap(1); lang(LANG_ES); render(); onTap(yDockX(0), Y_DOCK_Y); } },
    { "yard-walk-up", [] { yardWalkScene(25, 330, 120); }, 6 },
    { "yard-walk-down", [] { yardWalkScene(25, 150, 360); }, 6 },
    { "yard-walk-up-charmander", [] { yardWalkScene(4, 330, 120); }, 6 },
    { "yard-walk-down-charmander", [] { yardWalkScene(4, 150, 360); }, 6 },
    { "yard-tap-pet", [] { pokemon(25, 13); yardForceMap(1); render(); onTap((int)yardW.p[0].x, (int)yardW.p[0].y - 20); } },
    { "yard-tilt", [] { pokemon(25, 13); yardForceMap(1); gStubTiltSide = -1; }, 80 },
    { "yard-orbit", [] { pokemon(25, 13); yardForceMap(1); OrbitShift o = orbitShift(6); roundCanvas->setShift(o.dx, o.dy); }, 2 },
#endif
    // el patio (v2.0, fase Y0): el kit de interfaz nuevo, en sus tres paginas de muestra
    { "yard-ui-shop", [] { pokemon(25, 13); yardDemoOpen = true; yardDemoPage = 0; } },
    { "yard-ui-world", [] { pokemon(25, 13); yardDemoOpen = true; yardDemoPage = 1; } },
    { "yard-ui-stage", [] { pokemon(25, 13); yardDemoOpen = true; yardDemoPage = 2; } },
    { "yard-ui-shop-es", [] { pokemon(25, 13); yardDemoOpen = true; yardDemoPage = 0; setLang(LANG_ES); applyLangFont(); } },
    { "yard-ui-world-es", [] { pokemon(25, 13); yardDemoOpen = true; yardDemoPage = 1; setLang(LANG_ES); applyLangFont(); } },
    { "yard-ui-stage-es", [] { pokemon(25, 13); yardDemoOpen = true; yardDemoPage = 2; setLang(LANG_ES); applyLangFont(); } },
    { "home-dusk", [] { pokemon(7, 19); } },
    { "home-night", [] { pokemon(133, 23); pet.sleeping = true; } },
    { "home-forest", [] { pokemon(1, 9); pet.dbgSetPoops(2); pet.fullness = 20; } },
    // v1.58: a tap on a poop cleans just that one (the middle one here: sparkles where it was, "+10" HYG)
    { "poop-tap", [] { pokemon(1, 9); pet.dbgSetPoops(3); pet.hygiene = 40; onTap(116, 282); }, 3 },
    // v1.60: neglect. The bond fades with all four needs at 0, then "very weak", then the Revive dialog
    { "abandoned", [] { pokemon(4, 13); pet.fullness = pet.joy = pet.energy = pet.hygiene = 0; pet.bond = 60; } },
    { "abandoned-es", [] { pokemon(4, 13); pet.fullness = pet.joy = pet.energy = pet.hygiene = 0; pet.bond = 60;
                           setLang(LANG_ES); applyLangFont(); } },
    // la noche (v1.63): con sueño desde las 22, el globo con "zzz" (a la izquierda si anda por la derecha)
    { "sleepy", [] { pokemon(25, 22); } },
    { "sleepy-es", [] { pokemon(25, 22); setLang(LANG_ES); applyLangFont(); } },
    { "sleepy-right", [] { pokemon(7, 22); beh.x = beh.targetX = 330; } },
    { "weak", [] { pokemon(4, 13); pet.fullness = pet.joy = pet.energy = pet.hygiene = 0; pet.bond = 0; } },
    { "dying-revive", [] { pokemon(6, 13); pet.fullness = pet.joy = pet.energy = pet.hygiene = 0; pet.dbgSetPoops(2);
                           pet.items[IT_REVIVE] = 3; pet.startDying(); }, 2 },
    { "dying-revive-es", [] { pokemon(6, 13); pet.fullness = pet.joy = pet.energy = pet.hygiene = 0; pet.items[IT_REVIVE] = 3;
                              pet.startDying(); setLang(LANG_ES); applyLangFont(); }, 2 },
    // v1.64: la hora del Revivir; pasada, el dialogo dice que murio
    { "dying-late", [] { pokemon(6, 13); pet.fullness = pet.joy = pet.energy = pet.hygiene = 0; pet.items[IT_REVIVE] = 3;
                         pet.startDying(); pet.dbgDyingMin(REVIVE_LIMIT_MIN); }, 2 },
    { "dying-late-es", [] { pokemon(6, 13); pet.fullness = pet.joy = pet.energy = pet.hygiene = 0; pet.items[IT_REVIVE] = 3;
                            pet.startDying(); pet.dbgDyingMin(REVIVE_LIMIT_MIN); setLang(LANG_ES); applyLangFont(); }, 2 },
    { "dying-left-es", [] { pokemon(6, 13); pet.fullness = pet.joy = pet.energy = pet.hygiene = 0; pet.items[IT_REVIVE] = 3;
                            pet.startDying(); pet.dbgDyingMin(18); setLang(LANG_ES); applyLangFont(); }, 2 },
    { "dying-none", [] { pokemon(25, 21); pet.fullness = pet.joy = pet.energy = pet.hygiene = 0; pet.startDying(); }, 2 },
    { "dying-none-es", [] { pokemon(25, 21); pet.fullness = pet.joy = pet.energy = pet.hygiene = 0; pet.startDying();
                            setLang(LANG_ES); applyLangFont(); }, 2 },
    { "revived", [] { pokemon(6, 13); pet.fullness = pet.joy = pet.energy = pet.hygiene = 0; pet.items[IT_REVIVE] = 3;
                      pet.startDying(); onTap(DIE_YES_X + 50, DIE_BTN_Y + 20); }, 3 },
    { "death-ceremony", [] { pokemon(25, 13); pet.startDying(); pet.letGo(); gHostMillis += 4000; }, 2 },
    { "death-egg", [] { pokemon(6, 13); pet.setLevel(DEATH_EGG_LEVEL);
                        for (int i = 0; i < 50 && !pet.deathEggSpecies(); i++) { pet.dbgClearDying(); pet.startDying(); }
                        pet.letGo(); gHostMillis += 8200; }, 2 },
    { "death-egg-es", [] { pokemon(6, 13); pet.setLevel(DEATH_EGG_LEVEL);
                           for (int i = 0; i < 50 && !pet.deathEggSpecies(); i++) { pet.dbgClearDying(); pet.startDying(); }
                           pet.letGo(); gHostMillis += 8200; setLang(LANG_ES); applyLangFont(); }, 2 },
    { "poop-gap", [] { pokemon(1, 9); pet.dbgSetPoops(3); onTap(116, 282); gHostMillis += 2000; } },
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
    { "feed", [] { pokemon(133, 12); openCarePanel(); } },
    { "play-menu", [] { pokemon(133, 12); openPlayPanel(); } },
    // v1.53: the Care button (sleep | clean) and the Backpack screen
    { "care-menu", [] { pokemon(133, 12); pet.dbgSetPoops(2); openCarePanel(); } },
    { "care-menu-asleep", [] { pokemon(133, 23); pet.sleeping = true; openCarePanel(); } },
    { "bag", [] { pokemon(25, 13); walker(1840, 52310, 2, { 2, 1, 0, 3, 1, 1, 2, 3, 3 }); openBag(); } },
    // v1.59: the Revive (9th slot, 3x3 grid); a tap on it says what it's for (it isn't used by hand)
    { "bag-revive-hint", [] { pokemon(25, 13); walker(1840, 52310, 2, { 2, 1, 0, 3, 1, 1, 2, 3, 3 }); openBag(); bagTap(CX + 86, 94 + 2 * 90 + 40); } },
    { "bag-revive-hint-es", [] { pokemon(25, 13); walker(1840, 52310, 2, { 2, 1, 0, 3, 1, 1, 2, 3, 3 }); openBag(); bagTap(CX + 86, 94 + 2 * 90 + 40);
                                 setLang(LANG_ES); applyLangFont(); } },
    // v1.54: the cake in the food menu (with how many are left, grey with none), the pink FOOD bar while
    // it lasts, and a tap on it with none
    { "feed-cake", [] { pokemon(133, 12); pet.items[IT_CAKE] = 3; openCarePanel(); } },
    { "home-sated", [] { pokemon(133, 12); pet.items[IT_CAKE] = 1; pet.eatCake(); gHostMillis += EAT_ANIM_MS + 100; }, 2 },
    { "home-sated-es", [] { pokemon(133, 12); pet.items[IT_CAKE] = 1; pet.eatCake(); gHostMillis += EAT_ANIM_MS + 100;
                            setLang(LANG_ES); applyLangFont(); }, 2 },
    // v1.55: found items wait wrapped on the floor; a tap opens one (the pet poses, "GIFT!")
    { "gifts-1", [] { pokemon(25, 13); pet.addGift(IT_CAKE); } },
    { "gifts-3", [] { pokemon(25, 13); pet.addGift(IT_CAKE); pet.addGift(IT_IRON); pet.addGift(IT_GOLDEN); } },
    { "gifts-5-night", [] { pokemon(133, 22); for (int i = 0; i < 5; i++) pet.addGift(IT_GOLDEN); } },
    { "gifts-forest", [] { pokemon(1, 9); pet.dbgSetPoops(3); pet.addGift(IT_EGG); pet.addGift(IT_CAKE); } },
    { "gift-open", [] { pokemon(25, 13); pet.addGift(IT_CAKE); pet.addGift(IT_IRON); openGiftTap(); }, 6 },
    { "gift-open-es", [] { pokemon(25, 13); pet.addGift(IT_CAKE); openGiftTap(); setLang(LANG_ES); applyLangFont(); }, 6 },
    { "gift-full", [] { pokemon(25, 13); gHostMillis += 3000; pet.items[IT_GOLDEN] = ITEM_MAX; pet.addGift(IT_GOLDEN); openGiftTap(); }, 6 },
    { "gifts-farewell-btn", [] { pokemon(55, 15); pet.dbgSetAge(FAREWELL_AGE_MIN + 60); pet.addGift(IT_CAKE); pet.addGift(IT_CAKE); pet.addGift(IT_CAKE); } },
    // v1.57: a star on the favourite berry once it's known; "Its tastes changed!" after an evolution that changed it
    { "feed-star", [] { pokemon(133, 12); pet.berryKnown = true; pet.items[IT_CAKE] = 2; openCarePanel(); } },
    { "feed-star-red", [] { pokemon(25, 12); pet.berryKnown = true; openCarePanel(); } },
    { "feed-star-night", [] { pokemon(7, 21); pet.berryKnown = true; openCarePanel(); } },
    // v2.0 (Y2): the Care and Play panels that replace the popups, in Spanish, and their taps
    { "feed-es", [] { pokemon(133, 12); openCarePanel(); setLang(LANG_ES); applyLangFont(); } },
    { "feed-star-es", [] { pokemon(133, 12); pet.berryKnown = true; pet.items[IT_CAKE] = 2; openCarePanel(); setLang(LANG_ES); applyLangFont(); } },
    { "care-menu-asleep-es", [] { pokemon(133, 23); pet.sleeping = true; openCarePanel(); setLang(LANG_ES); applyLangFont(); } },
    { "play-menu-es", [] { pokemon(133, 12); openPlayPanel(); setLang(LANG_ES); applyLangFont(); } },
    { "play-menu-no-imu-es", [] { pokemon(133, 12); gStubImuPresent = false; openPlayPanel(); setLang(LANG_ES); applyLangFont(); } },
    { "care-no-cake", [] { pokemon(133, 12); openCarePanel(); panelTapCard(CP_CAKE); } },
    { "care-no-cake-es", [] { pokemon(133, 12); openCarePanel(); setLang(LANG_ES); applyLangFont(); panelTapCard(CP_CAKE); } },
    { "care-pressed", [] { pokemon(133, 12); pet.items[IT_CAKE] = 1; openCarePanel(); panelHold(CP_BLUE); } },
    { "play-pressed", [] { pokemon(133, 12); openPlayPanel(); panelHold(PP_SACK); } },
    { "care-asleep-deny", [] { pokemon(133, 23); pet.sleeping = true; openCarePanel(); panelTapCard(CP_GREEN); } },
    { "care-tap-berry", [] { pokemon(133, 12); openCarePanel(); panelTapCard(CP_RED); }, 3 },
    { "care-tap-cake", [] { pokemon(133, 12); pet.items[IT_CAKE] = 2; openCarePanel(); panelTapCard(CP_CAKE); }, 3 },
    { "care-tap-light", [] { pokemon(133, 21); openCarePanel(); panelTapCard(CP_LIGHT); }, 2 },
    { "care-tap-wake", [] { pokemon(133, 23); pet.sleeping = true; openCarePanel(); panelTapCard(CP_LIGHT); }, 2 },
    { "care-tap-bath", [] { pokemon(133, 12); pet.dbgSetPoops(1); openCarePanel(); panelTapCard(CP_BATH); }, 4 },
    { "play-tap-ball", [] { pokemon(133, 12); openPlayPanel(); panelTapCard(PP_BALL); }, 2 },
    { "play-tap-stages", [] { pokemon(133, 12); openPlayPanel(); panelTapCard(PP_BATTLE); } },
    { "play-tap-sack", [] { pokemon(133, 12); openPlayPanel(); panelTapCard(PP_SACK); }, 2 },
    { "play-tap-zen", [] { pokemon(7, 13); openPlayPanel(); panelTapCard(PP_ZEN); }, 2 },
    { "play-tap-dash", [] { pokemon(123, 13); openPlayPanel(); panelTapCard(PP_DASH); }, 2 },
    { "play-no-imu-deny", [] { pokemon(133, 12); gStubImuPresent = false; openPlayPanel(); panelTapCard(PP_DASH); } },
    { "play-asleep-box", [] { pokemon(133, 23); pet.sleeping = true; boxThree(); openPlayPanel(); } },
    { "panel-back", [] { pokemon(133, 12); openPlayPanel(); onTap(Y_BACK_X, Y_BACK_Y); } },
    { "panel-user-key", [] { pokemon(133, 12); openCarePanel(); onBack(); } },
    { "tastes-changed", [] { pokemon(5, 12); tastesMsgUntil = millis() + 6000; } },
    { "tastes-changed-es", [] { pokemon(5, 12); tastesMsgUntil = millis() + 6000; setLang(LANG_ES); applyLangFont(); } },
    { "no-cake", [] { pokemon(133, 12); gHostMillis += 3000; /* past an earlier scene's heart */ cakeHintUntil = millis() + 2500; fxo.push(FXO_DENY, 0, 0, millis()); } },
    { "no-cake-es", [] { pokemon(133, 12); gHostMillis += 3000; cakeHintUntil = millis() + 2500; setLang(LANG_ES); applyLangFont(); } },
    { "bag-es", [] { pokemon(25, 13); walker(1840, 52310, 2, { 2, 1, 0, 3, 0, 1, 0 }); openBag(); setLang(LANG_ES); applyLangFont(); } },
    { "bag-empty", [] { pokemon(25, 13); openBag(); } },
    { "bag-empty-es", [] { pokemon(25, 13); openBag(); setLang(LANG_ES); applyLangFont(); } },
    { "bag-asleep", [] { pokemon(25, 23); pet.sleeping = true; walker(0, 52310, 0, { 2, 1, 0, 3, 0, 1, 0 }); openBag(); } },
    { "bag-asleep-es", [] { pokemon(25, 23); pet.sleeping = true; walker(0, 52310, 0, { 2, 1, 0, 3, 0, 1, 0 }); openBag(); setLang(LANG_ES); applyLangFont(); } },
    { "bag-hint", [] { pokemon(25, 13); walker(1840, 52310, 2, { 2, 1, 0, 3, 0, 1, 0 }); openBag(); } },
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
    { "play-menu-no-imu", [] { pokemon(133, 12); gStubImuPresent = false; openPlayPanel(); } },
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
    // yard panel restyle (v2.0, Y1): Spanish, an unseen detail, and the new tap zones (chevrons, back)
    { "gallery-es", [] { pokemon(25, 13); setLang(LANG_ES); applyLangFont(); galleryOpen = true; galleryDirty = true; } },
    { "gallery-detail-es", [] {
       pokemon(25, 13);
       setLang(LANG_ES);
       applyLangFont();
       galleryOpen = true;
       galleryDirty = true;
       galleryTap(GAL_X + 3 * GAL_GC + 10, GAL_Y + 10);  // cell 4, Charmander (registered)
     } },
    { "gallery-detail-unseen", [] {
       pokemon(25, 13);
       galleryOpen = true;
       galleryDirty = true;
       galleryTap(GAL_X + GAL_GC + 10, GAL_Y + 10);  // cell 2, Ivysaur (not registered)
     } },
    { "gallery-chev-tap", [] { pokemon(25, 13); galleryOpen = true; galleryDirty = true; onTap(Y_CHEV_RX, Y_CY); } },
    { "gallery-last", [] {  // page 10: 7 cells, no right chevron
       pokemon(25, 13);
       galleryOpen = true;
       galleryDirty = true;
       for (int i = 0; i < 12; i++) onTap(Y_CHEV_RX, Y_CY);
     } },
    { "gallery-back-tap", [] { pokemon(25, 13); galleryOpen = true; galleryDirty = true; onTap(Y_BACK_X, Y_BACK_Y); } },
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
    { "bag-candy-confirm", [] {
       pokemon(25, 13);
       walker(11032, 71500, 8, { 5, 1, 9, 2, 1, 1 }, 3);
       openBag();
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
    { "bag-confirm", [] {
       pokemon(25, 13);
       walker(1840, 52310, 2, { 2, 1, 0, 3, 0 });
       openBag();
       itemConfirm = IT_GOLDEN + 1;
       itemConfirmUntil = millis() + 10000;
     } },
    // yard Y1: the panel restyle of the backpack and the stat card, in Spanish too
    { "bag-confirm-es", [] {
       pokemon(25, 13);
       walker(11032, 71500, 8, { 5, 1, 9, 2, 1, 1 }, 3);
       openBag();
       itemConfirm = IT_RARECANDY + 1;
       itemConfirmUntil = millis() + 10000;
       setLang(LANG_ES);
       applyLangFont();
     } },
    { "card-profile-es", [] { pokemon(25, 13); pet.streak = 365; pet.bestStreak = 365; setLang(LANG_ES); applyLangFont(); cardOpen = true; cardPage = 0; } },
    // v2.0: las 4 necesidades en el perfil (en el patio ya no hay barras)
    { "card-profile-needs", [] { pokemon(25, 13); pet.fullness = 20; pet.joy = 45; pet.energy = 80; pet.hygiene = 10; cardOpen = true; cardPage = 0; } },
    { "card-profile-needs-es", [] { pokemon(25, 13); pet.fullness = 20; pet.joy = 45; pet.energy = 80; pet.hygiene = 10;
                                    setLang(LANG_ES); applyLangFont(); cardOpen = true; cardPage = 0; } },
    { "card-profile-nick", [] { pokemon(6, 36); pet.rename("SPARKY"); pet.shiny = true; cardOpen = true; cardPage = 0; } },
    { "card-medals-es", [] { pokemon(25, 13); pet.medals = MED_LV10 | MED_BERRY | MED_MARATHON; setLang(LANG_ES); applyLangFont(); cardOpen = true; cardPage = 2; } },
    { "card-progress-es", [] { pokemon(25, 13); setLang(LANG_ES); applyLangFont(); cardOpen = true; cardPage = 3; } },
    { "walk-goal-es", [] { pokemon(25, 13); walker(3512, 61200, 4, { 4, 0, 9, 1, 1, 0, 2 }, 1); setLang(LANG_ES); applyLangFont(); cardOpen = true; cardPage = 4; } },
    { "keyboard-es", [] { pokemon(25, 13); pet.rename("PIKA"); openKeyboard(); setLang(LANG_ES); applyLangFont(); } },
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
    // stage select, team picker and the badges page (v1.65)
    { "stages-fresh", [] { pokemon(25, 13); stageSetup(0, 0, false); openStages(); } },
    { "stages-fresh-es", [] { pokemon(25, 13); stageSetup(0, 0, true); openStages(); } },
    { "stages-mid", [] { pokemon(25, 13); stageSetup(0x07, 0x03, false); openStages(); } },
    { "stages-mid-es", [] { pokemon(25, 13); stageSetup(0x07, 0x03, true); openStages(); } },
    { "stages-all", [] { pokemon(25, 13); stageSetup(0xFF, 0x1F, false); openStages(); } },
    { "stages-all-es", [] { pokemon(25, 13); stageSetup(0xFF, 0x1F, true); openStages(); } },
    { "team-home", [] { pokemon(25, 13); teamSetup({}, 1, false); } },
    { "team-home-es", [] { pokemon(25, 13); teamSetup({}, 4, true); } },
    { "team-full", [] { pokemon(25, 13); boxSix(); teamSetup({ 2, -1, 5, 0 }, 2, false); } },
    { "team-full-es", [] { pokemon(25, 13); boxSix(); teamSetup({ 2, -1, 5, 0 }, 4, true); } },
    { "team-asleep", [] { pokemon(25, 22); boxThree(); pet.sleeping = true; teamSetup({ 1 }, 0, false); } },
    { "team-asleep-es", [] { pokemon(25, 22); boxThree(); pet.sleeping = true; teamSetup({ 1 }, 0, true); } },
    { "team-vacant", [] { vacantNow(10); boxThree(); teamSetup({}, 0, false); } },
    { "team-vacant-es", [] { vacantNow(10); boxThree(); teamSetup({ 4 }, 3, true); } },
    { "box-dots", [] { pokemon(25, 13); boxThree(); openBox(); } },
    { "box-dots-es", [] { pokemon(25, 13); boxThree(); setLang(LANG_ES); applyLangFont(); openBox(); } },
    { "badges-0", [] { pokemon(25, 13); stageSetup(0, 0, false); openBox(); boxPage = 1; } },
    { "badges-0-es", [] { pokemon(25, 13); stageSetup(0, 0, true); openBox(); boxPage = 1; } },
    { "badges-3", [] { pokemon(25, 13); stageSetup(0x07, 0, false); openBox(); boxPage = 1; } },
    { "badges-3-es", [] { pokemon(25, 13); stageSetup(0x07, 0, true); openBox(); boxPage = 1; } },
    { "badges-8", [] { pokemon(25, 13); stageSetup(0xFF, 0, false); openBox(); boxPage = 1; } },
    { "badges-8-es", [] { pokemon(25, 13); stageSetup(0xFF, 0, true); openBox(); boxPage = 1; } },
    { "box-chev-tap", [] { pokemon(25, 13); stageSetup(0x07, 0, false); boxThree(); openBox(); onTap(Y_CHEV_RX, Y_CY); } },
    { "box-chev-back-tap", [] {  // badges, then < back to the box
       pokemon(25, 13);
       boxThree();
       openBox();
       boxPage = 1;
       onTap(Y_CHEV_LX, Y_CY);
     } },
    { "box-back-tap", [] { pokemon(25, 13); boxThree(); openBox(); onTap(Y_BACK_X, Y_BACK_Y); } },
    { "box-wake-ask-es", [] {
       pokemon(25, 13);
       setLang(LANG_ES);
       applyLangFont();
       boxThree();
       openBox();
       onTap(BOX_X + BOX_CW + 10, BOX_Y + BOX_CH + 10);  // slot 4 (BLAZE): "despertar?"
     } },
    { "box-store-ask-es", [] { pokemon(25, 13); setLang(LANG_ES); applyLangFont(); boxThree(); openBox(); boxAsk = 3; boxAskUntil = millis() + 10000; } },
    { "box-held-es", [] {
       eggNow();
       setLang(LANG_ES);
       applyLangFont();
       boxThree();
       pet.wake(1);
       ensureMon();
       openBox();
     } },
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
    // stage battles: driven to a given moment with the sketch's own battleStep(), or put straight
    // into a fight with stageJump(). Team order: -1 = the home pet, k = box slot k.
    { "battle-vs", [] { pokemon(25, 13); boxThree(); stageGo(0, { -1, 4, 1 }); }, 2 },
    { "battle-vs-es", [] { pokemon(25, 13); boxThree(); lang(LANG_ES); stageGo(0, { -1, 4, 1 }); }, 2 },
    { "battle-walkin", [] { pokemon(25, 13); stageGo(0, { -1 }); }, 30 },
    { "battle-special", [] {
       pokemon(25, 13);
       stageGo(0, { -1 });
       bt.run.meter.pips = SPECIAL_TAPS;  // charged before the first attack
       battleUntil([] { return bt.haveEv && bt.ev.special && millis() - bt.evAt >= BT_HIT_MS + 100; });
     }, 1 },
    { "battle-mid", [] {
       pokemon(25, 13);
       stageGo(0, { -1 });
       battleUntil([] { return bt.run.eng.attacks >= 4 && millis() - bt.evAt >= BT_HIT_MS + 150; });
       bt.run.meter.pips = 3;
     }, 1 },
    // v1.38: "> > > > >" and the "Tap!" box from the first moment of the fight; three charged
    { "battle-tap", [] {
       pokemon(25, 13);
       stageGo(0, { -1 });
       battleUntil([] { return bt.phase == BT_FIGHT; });
     }, 1 },
    { "battle-charging", [] {
       pokemon(25, 13);
       stageGo(0, { -1 });
       battleUntil([] { return bt.run.eng.attacks >= 2 && millis() - bt.evAt >= BT_HIT_MS + 900; });
       bt.run.meter.pips = 3;
     }, 1 },
    { "battle-armed", [] {
       pokemon(25, 13);
       stageGo(0, { -1 });
       battleUntil([] { return bt.run.eng.attacks >= 2 && millis() - bt.evAt >= BT_HIT_MS + 900; });
       bt.run.meter.pips = SPECIAL_TAPS;
     }, 1 },
    { "battle-locked", [] {  // after a special: grey outlines, no box
       pokemon(25, 13);
       stageGo(0, { -1 });
       battleUntil([] { return bt.run.eng.attacks >= 2 && millis() - bt.evAt >= BT_HIT_MS + 900; });
       bt.run.meter.cooldown = SPECIAL_COOLDOWN;
     }, 1 },
    // four pets: the first one fainted, the second (Squirtle, from the box) against the second foe
    { "battle-team4", [] {
       pokemon(25, 13);
       boxThree();
       stageGo(1, { -1, 0, 4, 1 });
       stageJump(1, 1, 80, 60);
       battleUntil([] { return bt.run.eng.attacks >= 2 && millis() - bt.evAt >= BT_HIT_MS + 450; });
     }, 1 },
    { "battle-team4-es", [] {
       pokemon(25, 13);
       boxThree();
       lang(LANG_ES);
       stageGo(1, { -1, 0, 4, 1 });
       stageJump(1, 1, 80, 60);
       battleUntil([] { return bt.run.eng.attacks >= 2 && millis() - bt.evAt >= BT_HIT_MS + 450; });
     }, 1 },
    // a boxed pet (BLAZE, Charizard) leads: its own sprite, name, colour and soil; the home pet second
    { "battle-boxed", [] {
       pokemon(25, 13);
       boxThree();
       stageGo(3, { 4, -1 });
       battleUntil([] { return bt.run.eng.attacks >= 3 && millis() - bt.evAt >= BT_HIT_MS + 450; });
     }, 1 },
    { "battle-boxed-es", [] {
       pokemon(25, 13);
       boxThree();
       lang(LANG_ES);
       stageGo(3, { 4, -1 });
       battleUntil([] { return bt.run.eng.attacks >= 3 && millis() - bt.evAt >= BT_HIT_MS + 450; });
     }, 1 },
    // v1.67 sudden death: the 21st attack of a fight, the recoil just landing (the attacker's bar
    // dropping after the defender's); "Sudden death!" and "Recoil!" together
    { "battle-sudden", [] {
       pokemon(25, 13);
       stageGo(0, { -1 });
       stageJump(0, 0, 100, 100);
       bt.run.eng.attacks = SUDDEN_DEATH_AT;
       battleUntil([] { return bt.haveEv && bt.ev.recoil && millis() - bt.evAt >= BT_RECOIL_MS + 120; });
     }, 1 },
    { "battle-sudden-es", [] {
       pokemon(25, 13);
       lang(LANG_ES);
       stageGo(0, { -1 });
       stageJump(0, 0, 100, 100);
       bt.run.eng.attacks = SUDDEN_DEATH_AT;
       battleUntil([] { return bt.haveEv && bt.ev.recoil && millis() - bt.evAt >= BT_RECOIL_MS + 120; });
     }, 1 },
    // ...and just before the recoil: the defender's bar already down, "Sudden death!" alone
    { "battle-sudden-pre", [] {
       pokemon(25, 13);
       stageGo(0, { -1 });
       stageJump(0, 0, 100, 100);
       bt.run.eng.attacks = SUDDEN_DEATH_AT;
       battleUntil([] { return bt.haveEv && millis() - bt.evAt >= BT_HIT_MS + 300; });
     }, 1 },
    // the hand-over after a KO: caption + the newcomer walking in, the survivor in place
    { "battle-swap-foe", [] { pokemon(25, 13); boxThree(); stageGo(0, { -1, 4 }); stageSwap(0, 0, 100, 1); }, 1 },
    { "battle-swap-foe-es", [] {
       pokemon(25, 13); boxThree(); lang(LANG_ES); stageGo(0, { -1, 4 }); stageSwap(0, 0, 100, 1);
     }, 1 },
    { "battle-swap-boss", [] { pokemon(25, 13); boxThree(); stageGo(0, { -1, 4 }); stageSwap(1, 1, 70, 1); }, 1 },
    { "battle-swap-boss-es", [] {
       pokemon(25, 13); boxThree(); lang(LANG_ES); stageGo(0, { -1, 4 }); stageSwap(1, 1, 70, 1);
     }, 1 },
    { "battle-swap-ally", [] { pokemon(25, 13); boxThree(); stageGo(0, { -1, 4, 1 }); stageSwap(0, 1, 1, 100); }, 1 },
    { "battle-swap-ally-es", [] {
       pokemon(25, 13); boxThree(); lang(LANG_ES); stageGo(0, { -1, 4, 1 }); stageSwap(0, 1, 1, 100);
     }, 1 },
    // the boss of the rock stage (Golem), the team down to its last pet
    { "battle-boss", [] {
       pokemon(25, 13);
       boxThree();
       stageGo(2, { 0, 1, -1 });
       stageJump(2, 2, 65, 75);
       battleUntil([] { return bt.run.eng.attacks >= 2 && millis() - bt.evAt >= BT_HIT_MS + 450; });
     }, 1 },
    { "battle-boss-es", [] {
       pokemon(25, 13);
       boxThree();
       lang(LANG_ES);
       stageGo(2, { 0, 1, -1 });
       stageJump(2, 2, 65, 75);
       battleUntil([] { return bt.run.eng.attacks >= 2 && millis() - bt.evAt >= BT_HIT_MS + 450; });
     }, 1 },
    // results: the boss falls (first time, reward not taken today: item + team + badge + joy); the
    // same with today's reward already taken; the last ally falls. "battle-result" is inside the tap
    // lock (no "tap: back"), the others just after it.
    { "battle-result", [] { pokemon(25, 13); boxThree(); stageGo(2, { -1, 4 }); stageResult(0, 2, 100, 1, 0); }, 4 },
    { "battle-clear", [] {
       pokemon(25, 13); boxThree(); stageGo(2, { -1, 4 }); stageResult(0, 2, 100, 1, BT_END_LOCK_MS);
     }, 1 },
    { "battle-clear-es", [] {
       pokemon(25, 13); boxThree(); lang(LANG_ES); stageGo(2, { -1, 4 }); stageResult(0, 2, 100, 1, BT_END_LOCK_MS);
     }, 1 },
    { "battle-clear-none", [] {
       pokemon(25, 13);
       boxThree();
       stageGo(2, { -1, 4 });
       pet.stageDone(2, true, gTeam, gTeamN);  // cleared once already today: badge and reward taken
       stageResult(0, 2, 100, 1, BT_END_LOCK_MS);
     }, 1 },
    { "battle-clear-none-es", [] {
       pokemon(25, 13);
       boxThree();
       lang(LANG_ES);
       stageGo(2, { -1, 4 });
       pet.stageDone(2, true, gTeam, gTeamN);
       stageResult(0, 2, 100, 1, BT_END_LOCK_MS);
     }, 1 },
    { "battle-lost", [] { pokemon(25, 13); boxThree(); stageGo(2, { 4, -1 }); stageResult(1, 2, 1, 100, BT_END_LOCK_MS); }, 1 },
    { "battle-lost-es", [] {
       pokemon(25, 13); boxThree(); lang(LANG_ES); stageGo(2, { 4, -1 }); stageResult(1, 2, 1, 100, BT_END_LOCK_MS);
     }, 1 },
    { "battle-tap-ja", [] {
       pokemon(4, 19);
       lang(LANG_JA);
       stageGo(1, { -1 });
       battleUntil([] { return bt.phase == BT_FIGHT; });
       bt.run.meter.pips = 2;
     }, 1 },
    { "battle-tap-ko", [] {
       pokemon(4, 13);
       lang(LANG_KO);
       stageGo(1, { -1 });
       battleUntil([] { return bt.phase == BT_FIGHT; });
     }, 1 },
    { "battle-ja", [] {
       pokemon(4, 19);
       lang(LANG_JA);
       stageGo(1, { -1 });
       battleUntil([] { return bt.run.eng.attacks >= 3 && millis() - bt.evAt >= BT_HIT_MS + 150; });
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
