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
  // viajes (Y5b): sin viaje ni resultado, sin recuerdos y con la seleccion y los avisos a cero
  if (pet.travelling()) pet.travelFinishNow();
  {
    TravelResult tr;
    pet.takeTravelResult(&tr);
  }
  pet.setSouvenirMask(0);
  trvSelPet = trvSelTier = souvTip = trvMapPrev = 0;
  trvSelDest = 1;
  boxAwayUntil = 0;
  returnBannerShown = false;
  galleryOpen = cardOpen = bagOpen = kbOpen = clockOpen = gameOpen = sackOpen = zenOpen = dashOpen = false;
  yardDemoOpen = false;  // el patio (v2.0)
  yardDemoPage = 0;
  bagTab = 0; bagPage = 0;  // la tienda y el selector de fondos (Y4b)
  shopBuyId = shopToast = yardMapPreview = 0;
  pet.yardOwned = 0;
  pet.yardChosen = 0;
  pet.coins = 120;  // un saldo normal: las escenas comparten el pet (si no, se queda el 9999 de otra)
  // el patio (v2.0, Y2): bichos sin colocar, sin cosas en el suelo y el mapa del bioma
  yardW = YardWorld{};
  yardLastStep = 0;
  yardPoopHave[0] = yardPoopHave[1] = 0;
  yardPoopBooted = false;
  yardGiftN = 0;
  yardGiftTapped = -1;
  for (int s = 0; s < HOME_SLOTS; s++) {
    yardBubblePrev[s] = yardBubbleLatched[s] = nullptr;
    yardBubbleUntil[s] = 0;
  }
  if (yardMapForced) yardForceMap(0);
  // fase Y3: sin comida en el suelo ni huevo en el patio
  for (YardFood &f : yardFood) f = YardFood{};
  // v3.3: ni menu del bicho ni selector de bicho
  petMenuOpen = petMenuView = petFoodPage = 0;
  for (YardAside &a : yardAside) a = YardAside{};
  pmShakeIdx = -1;
  petPick = PICK_NONE;
  petPickArg = 0;
  fxo = FxOverlays{};  // ni "+N" ni otros efectos de la escena anterior
  yardHeartPopAt = 0;
  for (int s = 0; s < HOME_SLOTS; s++) {
    yardEggX[s] = -1;
    yardEggTapAt[s] = 0;
    yardEggInfoUntil[s] = 0;
  }
  // v3.0: cada escena empieza con el foco en el hueco 0 y el 1 vacio (secondPet() pone a otro)
  pet.focus(0);
  pet.other = vacantPetLife();
  yardHeartSlot = 0;
  poopPuffHome = 0;
  boxHome = boxMoveHome = 0;
  boxAskPick = false;
  pet.dbgEggClear();
  pet.dbgClearAnims();  // no eating, heart or emote left over from the scene before (the clock goes back)
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
  confirmUntil = choiceUntil = poopPuffUntil = 0;
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
  pet.satedUntil = 0;  // la tarta (v1.54) no pasa de una escena a otra
  pet.dbgClearGifts();  // ni los regalos (v1.55)
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
  for (auto &g : fxBondGate) g = FxGate{};
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
  // encuentros salvajes (v2.2): nada en el patio, pantallas cerradas, sin huevos de captura
  encUi = ENCU_OFF;
  encDenyUntil = 0;
  teamWild = false;
  yardEncX = -1;
  pet.encClear();
  pet.setBagEggs(0);
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

// a tap (or a finger held down) on card i of the open Play panel (v2.0, Y2)
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

// v3.0: a second home pet in slot 1 (hatched, healthy), or an egg in the yard there
static void secondPet(int16_t dex, uint16_t lv, bool shiny = false, const char *nick = "") {
  PetLife &l = pet.slotRef(1);
  l = PetLife{};
  l.speciesId = dex;
  l.prevSpeciesId = dex;
  l.shiny = shiny;
  l.uid = 0x5EC0D000u + (uint32_t)dex;
  l.setLevel(lv);
  l.growMin += 7;
  l.ageMinutes = l.growMin + 30;
  l.fullness = 66;
  l.joy = 74;
  l.energy = 70;
  l.hygiene = 85;
  l.bond = 20;
  l.geneAtk = l.geneDef = l.geneSpe = 100;
  strncpy(l.nick, nick, sizeof(l.nick) - 1);
  ensureMon();
}
static void secondEgg(int16_t dex, uint32_t mins) {
  PetLife &l = pet.slotRef(1);
  l = PetLife{};
  l.eggTarget = dex;
  l.eggPlaced = eggPlacedFor(pet.lastSeenEpoch, mins);
  ensureMon();
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
  pet.dbgEggSpecies(4);  // v2.4: the egg shows its species' colours; always Charmander unless a scene says so
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

// the yard egg (Y3): placed `mins` ago with `saved` minutes shaken off, a Box pet wandering around it
static void yardEggScene(uint32_t mins, uint8_t saved, bool box = true) {
  eggNow();
  pet.dbgEggElapsed(mins);
  pet.dbgEggSaved(saved);
  if (box) boxPet(0, 7, 12);
  yardForceMap(1);
}

// v3.3: a tap on home pet s in the yard (it opens its menu); the yard has to have been drawn once
static void tapPet(uint8_t s) {
  render();
  onTap((int16_t)yardW.p[s].x, (int16_t)(yardW.p[s].y - 20));
}
// ...a tap on the row of kind k (PM_*) of the open menu
static void menuTap(uint8_t k) {
  uint8_t s = petMenuOpen ? petMenuOpen - 1 : 0, items[PM_MAX], n = petMenuItems(s, items);
  for (uint8_t i = 0; i < n; i++) {
    if (items[i] != k) continue;
    int x, y, w, h;
    petMenuRow(i, n, x, y, w, h);
    onTap((int16_t)(x + w / 2), (int16_t)(y + h / 2));
    return;
  }
}
// ...a finger held on that row (pressed)
static void menuHold(uint8_t k) {
  uint8_t s = petMenuOpen ? petMenuOpen - 1 : 0, items[PM_MAX], n = petMenuItems(s, items);
  for (uint8_t i = 0; i < n; i++) {
    if (items[i] != k) continue;
    int x, y, w, h;
    petMenuRow(i, n, x, y, w, h);
    tX0 = tXl = (int16_t)(x + w / 2);
    tY0 = tYl = (int16_t)(y + h / 2);
    wasPressed = true;
    return;
  }
}
// ...in the food list: row i of the page, or the "more" cell
static void foodRowTap(uint8_t i) {
  uint8_t foods[FOOD_COUNT], nf = petFoods(foods), n = foodListRows(nf, petFoodPage);
  int x, y, w, h;
  petMenuRow(i, n, x, y, w, h);
  onTap((int16_t)(x + w / 2), (int16_t)(y + h / 2));
}
static void foodMoreTap() {
  uint8_t foods[FOOD_COUNT], nf = petFoods(foods), n = foodListRows(nf, petFoodPage);
  int x, y, w, h;
  if (foodNavCell(n, foodPages(nf) > 1, 1, x, y, w, h)) onTap((int16_t)(x + w / 2), (int16_t)(y + h / 2));
}
// the pet menu of Pikachu in the meadow (a second pet, Charmander, with it unless `alone`)
static void menuScene(bool alone = false, Lang l = LANG_EN) {
  pokemon(25, 13);
  if (!pet.uid) pet.uid = 0x7E57u;  // (pokemon() never hatched it: the food is tied to the uid)
  yardForceMap(1);
  if (!alone) secondPet(4, 14);
  setLang(l);
  applyLangFont();
  tapPet(0);
}

// v3.3: food in front: Pikachu's menu, Food, and row `row` of the food list (0 plain berry, 1 its favourite
// berry, 2 cake)
static void feedTap(uint8_t row, Lang l = LANG_EN) {
  pokemon(25, 13);
  if (!pet.uid) pet.uid = 0x7E57u;
  yardForceMap(1);
  pet.items[IT_CAKE] = 2;
  pet.items[IT_BERRY_R + favBerryOf(25)] = 2;  // v3.2: row 1 = Pikachu's favourite berry, row 2 = cake
  setLang(l);
  applyLangFont();
  tapPet(0);
  menuTap(PM_FOOD);
  foodRowTap(row);
}

// steps the yard 85 ms at a time until slot 0's food reaches `phase` (or 25 s)
static void feedUntil(uint8_t phase) {
  for (int i = 0; i < 300 && yardFood[0].phase != phase; i++) {
    gHostMillis += 85;
    render();
  }
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

// the Shop (Y4b; v3.3 its own screen from the dock) with `coins` and a few items already owned
static void shopOpen(uint16_t coins, std::initializer_list<uint8_t> have = { 0, 0, 0, 0, 0, 0, 0, 0, 0 }, uint8_t page = 0) {
  pokemon(25, 13);
  int i = 0;
  for (uint8_t n : have) pet.items[i++] = n;
  pet.coins = coins;
  openShop();
  bagPage = page;
}
static void shopAsk(uint8_t id) {
  shopBuyId = id + 1;
  shopBuyUntil = millis() + 10000;
}
static void shopToastNow(uint8_t kind) {
  shopToast = kind;
  shopToastUntil = millis() + 1000;
}
// the background picker on map n
static void pickerOpen(uint8_t n, uint16_t coins, uint8_t owned = 0) {
  shopOpen(coins);
  for (uint8_t m = 1; m <= YARD_MAPS; m++)
    if (owned & (1u << m)) pet.yardOwned |= (uint16_t)(1u << m);
  yardMapPreview = n;
}
static void es() {
  setLang(LANG_ES);
  applyLangFont();
}

// travel and the collection (Y5b)
static void ownMaps(std::initializer_list<uint8_t> maps) {
  for (uint8_t m : maps) pet.yardOwned |= (uint16_t)(1u << m);
}
// `n` pets in the Box (the first six of a fixed list), the given maps owned, the Play panel open and its Travel card tapped
static void travelSetup(int n, std::initializer_list<uint8_t> maps) {
  pokemon(25, 13);
  static const int16_t dex[BOX_SLOTS] = { 7, 133, 6, 143, 1, 4 };
  static const uint16_t lv[BOX_SLOTS] = { 12, 23, 41, 30, 8, 17 };
  for (int k = 0; k < n && k < BOX_SLOTS; k++) boxPet((uint8_t)k, dex[k], lv[k], k == 1, k == 2 ? "BLAZE" : "");
  ownMaps(maps);
  openPlayPanel();
  panelTapCard(PP_TRAVEL);
}
// a trip already under way: Box slot 1, map `dest`, tier `tier`, `elapsed` minutes in
static void travelRunning(uint8_t dest, uint8_t tier, uint32_t elapsed) {
  travelSetup(3, { 2, 3, 4 });
  pet.startTravel(1, dest, tier);
  if (elapsed) pet.dbgTravelElapsed(elapsed);
}
// a finished trip's result waiting on the yard home (the Box pet is back)
static void travelBack(uint8_t dest, uint32_t souvMask, bool fullBag) {
  pokemon(25, 13);
  boxPet(1, 133, 23, true);
  ownMaps({ 2, 3, 4 });
  pet.setSouvenirMask(souvMask);
  if (fullBag)
    for (uint8_t id : { 0, 1, 2, 3, 5, 7, 8 }) pet.items[id] = ITEM_MAX;
  pet.startTravel(1, dest, 2);
  pet.travelFinishNow();
}
// tap the pet back from its trip (in the yard as p[1]): on its body, or on its "!" bubble
static void tapTraveller(bool bubble) {
  YardBody b = yardBodyOf(YARD_TRV);  // v3.0: el viajero es p[2]
  int x = (int)yardW.p[YARD_TRV].x, y = (int)yardW.p[YARD_TRV].y - b.h / 2;
  if (bubble) {
    x += b.w / 2 + 14;
    y = (int)yardW.p[YARD_TRV].y - b.h - 12;
  }
  onTap(x, y);
}
// the collection tab with the given souvenirs owned
static void collection(uint32_t mask) {
  pokemon(25, 13);
  pet.setSouvenirMask(mask);
  openBag();
  bagTab = 2;
}
static void collectionTip(uint32_t mask, uint8_t id) {
  collection(mask);
  onTap(souvCellX(id) + SOUV_CELL_W / 2, souvCellY(id) + SOUV_CELL_H / 2);
}

// v2.2: a wild encounter in the yard: `dex` at level lv on map `map` (its object: bush, rock or sand)
static void encScene(int16_t dex, uint8_t map, uint8_t lv = 12, bool shiny = false, int hour = 13) {
  pokemon(25, hour);
  if (!pet.uid) pet.uid = 0x7E57u;  // pokemon() never hatched it: no uid
  yardForceMap(map);
  pet.encSpawnNow(dex);
  pet.dbgEncSet(lv, shiny);
  pet.takeEncSound();
  pet.items[IT_BALL] = BALL_START;
}
// ...the frame `ms` into its rustle (each one lasts 800 ms every 2.4 s)
static void encRustleAt(uint32_t ms) {
  uint32_t ph = gHostMillis % 2400;
  gHostMillis += (2400 + ms - ph - 85) % 2400;  // the capture adds 85
}
// tap the object in the yard (the intro opens)
static void encTapYard() {
  render();
  onTap(yardEncX, yardEncY - 30);
}
// the fight against it with the home pet at level `lv` (strong: it wins; weak: it loses), to the result
static void encFight(uint16_t lv) {
  pet.setLevel(lv);
  pet.trAtk = pet.trDef = pet.trSpe = lv > 30 ? 80 : 0;
  startWild(pet.uid);
  battleUntil([] { return bt.phase == BT_END; });
}
// ...and past the result (closeBattle: the Poke Ball question, or "fled")
static void encAfter(uint16_t lv) {
  encFight(lv);
  gHostMillis += BT_END_LOCK_MS;
  closeBattle();
}

// a tap on the Backpack slot of item `it` (v2.2: the grid is 4 x 3, the slot comes from BAG_ORDER)
static void bagTapItem(uint8_t it) {
  for (int k = 0; k < (int)BAG_SLOTS; k++) {
    if (BAG_ORDER[k] != it) continue;
    int x, y;
    bagSlotXY(k, x, y);
    bagTap(x + BAG_SLOT_W / 2, y + BAG_SLOT_H / 2);
    return;
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
    // el patio (v2.0, Y2): la casa es el mapa. Pradera de dia, de noche, al atardecer, la cantera, sin mapa,
    // con un bicho de la caja, cacas, regalos, el globo que pasa de una necesidad a otra, dormido y el dock
    { "yard-home", [] { pokemon(25, 13); yardForceMap(1); } },
    // Y4: the coin badge under the clock (1234 coins; 9999 is the widest)
    { "yard-home-coins", [] { pokemon(25, 13); yardForceMap(1); pet.coins = 1234; } },
    { "yard-home-coins-max", [] { pokemon(25, 13); yardForceMap(1); pet.coins = 9999; } },
    { "yard-home-night", [] { pokemon(133, 22); } },
    { "yard-home-dusk", [] { pokemon(1, 19); } },
    { "yard-home-quarry", [] { pokemon(74, 11); } },
    { "yard-home-fallback", [] { pokemon(25, 13); yardForceMap(YARD_MAP_NONE); } },
    { "yard-home-fallback-night", [] { pokemon(7, 23); pet.sleeping = true; yardForceMap(YARD_MAP_NONE); } },
    { "yard-home-two", [] { pokemon(25, 13); yardForceMap(1); secondPet(7, 12); }, 60 },
    { "yard-home-two-quarry", [] { pokemon(74, 15); secondPet(133, 20, true); }, 120 },
    // v3.0: two home pets, two eggs, one pet and an egg, a pet and the vacant slot
    { "yard-two-pets", [] { pokemon(25, 13); yardForceMap(1); secondPet(4, 14); pet.slotRef(1).fullness = 12; }, 30 },
    { "yard-two-pets-es", [] { pokemon(25, 13); yardForceMap(1); secondPet(4, 14); pet.slotRef(1).fullness = 12; es(); }, 30 },
    { "yard-two-eggs", [] { yardEggScene(40, 0, false); secondEgg(114, 90); }, 20 },
    { "yard-pet-and-egg", [] { pokemon(25, 13); yardForceMap(1); secondEgg(145, 60); }, 20 },
    { "yard-pet-and-vacant", [] { pokemon(7, 13); yardForceMap(1); }, 20 },
    { "yard-two-poops", [] { pokemon(1, 10); yardForceMap(1); secondPet(7, 9); pet.dbgSetPoops(2); pet.slotRef(1).dbgSetPoops(2); }, 20 },
    { "yard-two-asleep", [] { pokemon(25, 23); pet.sleeping = true; yardForceMap(1); secondPet(133, 20); pet.slotRef(1).sleeping = true; } },
    { "yard-tap-pet2", [] { pokemon(25, 13); yardForceMap(1); secondPet(4, 14); render(); tapPet(1); } },
    { "yard-home-poop", [] { pokemon(1, 10); pet.dbgSetPoops(3); } },
    { "yard-poop-tap", [] { pokemon(1, 10); pet.dbgSetPoops(3); render();
                            onTap(yardPoopX[0][1], yardPoopY[0][1] - 20); }, 3 },
    { "yard-home-gifts", [] { pokemon(25, 13); yardForceMap(1); for (int i = 0; i < 5; i++) pet.addGift(IT_CAKE); } },
    { "yard-bubble-cycle-0", [] { pokemon(25, 13); yardForceMap(1); pet.fullness = 10; pet.energy = 12; gHostMillis = 200000 - 85; } },
    { "yard-bubble-cycle-1", [] { pokemon(25, 13); yardForceMap(1); pet.fullness = 10; pet.energy = 12; gHostMillis = 201000 - 85; } },
    { "yard-bubble-cycle-0-es", [] { pokemon(25, 13); yardForceMap(1); pet.fullness = 10; pet.energy = 12; gHostMillis = 200000 - 85;
                                     lang(LANG_ES); } },
    { "yard-bubble-cycle-1-es", [] { pokemon(25, 13); yardForceMap(1); pet.fullness = 10; pet.energy = 12; gHostMillis = 201000 - 85;
                                     lang(LANG_ES); } },
    { "yard-home-asleep", [] { pokemon(25, 23); pet.sleeping = true; yardForceMap(1); boxPet(0, 7, 12); } },
    { "yard-home-asleep-es", [] { pokemon(25, 23); pet.sleeping = true; yardForceMap(1); lang(LANG_ES); } },
    { "yard-sleepy-es", [] { pokemon(25, 22); yardForceMap(1); lang(LANG_ES); } },
    { "yard-abandoned-es", [] { pokemon(4, 13); pet.fullness = pet.joy = pet.energy = 0; pet.bond = 60; lang(LANG_ES); } },
    { "yard-heart", [] { pokemon(133, 12); pet.items[IT_BERRY_R + favBerryOf(133)] = 1; pet.feedFood(FOOD_BERRY0 + favBerryOf(133)); gHostMillis += EAT_ANIM_MS + 100; }, 2 },
    { "yard-release-ask", [] { pokemon(25, 13); yardForceMap(1); confirmUntil = millis() + 5000; } },
    // v3.3: no more big evolve / farewell buttons: a "!" over the pet, and a highlighted row in its menu
    { "yard-evolve-mark", [] { pokemon(4, 11); pet.setLevel(18); pet.trAtk = pet.trDef = pet.trSpe = 40; } },
    { "yard-farewell-mark", [] { pokemon(55, 15); pet.dbgSetAge(FAREWELL_AGE_MIN + 60); pet.addGift(IT_CAKE); } },
    { "yard-mark-two", [] { pokemon(4, 11); pet.setLevel(18); pet.trAtk = pet.trDef = pet.trSpe = 40; yardForceMap(1); secondPet(25, 12); }, 20 },
    // v3.3: the dock is Play, Bag, Shop
    { "yard-dock-play", [] { pokemon(25, 13); yardForceMap(1); render(); onTap(yDockX(0), Y_DOCK_Y); } },
    { "yard-dock-bag", [] { pokemon(25, 13); yardForceMap(1); walker(0, 0, 0, { 2, 1, 0, 3, 1, 1, 2, 3, 3 }); render(); onTap(yDockX(1), Y_DOCK_Y); } },
    { "yard-dock-bag-es", [] { pokemon(25, 13); yardForceMap(1); walker(0, 0, 0, { 2, 1, 0, 3, 1, 1, 2, 3, 3 }); lang(LANG_ES); render(); onTap(yDockX(1), Y_DOCK_Y); } },
    { "yard-dock-shop", [] { pokemon(25, 13); yardForceMap(1); pet.coins = 245; render(); onTap(yDockX(2), Y_DOCK_Y); } },
    { "yard-dock-shop-es", [] { pokemon(25, 13); yardForceMap(1); pet.coins = 245; lang(LANG_ES); render(); onTap(yDockX(2), Y_DOCK_Y); } },
    // v3.3: the pet menu (tap a pet): Sleep, Food, Care, Status (+ Evolve / Farewell); the pet stays still with a ring
    { "yard-menu", [] { menuScene(); }, 3 },
    { "yard-menu-later", [] { menuScene(); }, 45 },  // the other pet has walked out from under the menu
    { "yard-menu-es", [] { menuScene(false, LANG_ES); }, 3 },
    { "yard-menu-alone", [] { menuScene(true); }, 3 },
    { "yard-menu-evolve", [] { pokemon(4, 11); pet.setLevel(18); pet.trAtk = pet.trDef = pet.trSpe = 40; yardForceMap(1); tapPet(0); }, 3 },
    { "yard-menu-evolve-es", [] { pokemon(4, 11); pet.setLevel(18); pet.trAtk = pet.trDef = pet.trSpe = 40; yardForceMap(1); es(); tapPet(0); }, 3 },
    { "yard-menu-farewell", [] { pokemon(55, 15); pet.dbgSetAge(FAREWELL_AGE_MIN + 60); yardForceMap(1); tapPet(0); }, 3 },
    { "yard-menu-farewell-es", [] { pokemon(55, 15); pet.dbgSetAge(FAREWELL_AGE_MIN + 60); yardForceMap(1); es(); tapPet(0); }, 3 },
    { "yard-menu-evolve-dialog", [] { pokemon(4, 11); pet.setLevel(18); pet.trAtk = pet.trDef = pet.trSpe = 40; yardForceMap(1); tapPet(0); menuTap(PM_EVOLVE); } },
    { "yard-menu-asleep", [] { pokemon(25, 23); pet.sleeping = true; yardForceMap(1); tapPet(0); }, 2 },
    { "yard-menu-asleep-es", [] { pokemon(25, 23); pet.sleeping = true; yardForceMap(1); es(); tapPet(0); }, 2 },
    { "yard-menu-asleep-deny", [] { pokemon(25, 23); pet.sleeping = true; yardForceMap(1); tapPet(0); menuTap(PM_FOOD); }, 2 },
    { "yard-menu-pressed", [] { menuScene(); menuHold(PM_CARE); } },
    { "yard-menu-switch", [] { menuScene(); render(); onTap((int16_t)yardW.p[1].x, (int16_t)(yardW.p[1].y - 20)); }, 3 },
    { "yard-menu-step-aside", [] { pokemon(25, 13); yardForceMap(1); render(); yardW.p[0].x = 360; yardW.p[0].y = 250;
                                   yardW.p[0].mode = YM_IDLE; onTap(360, 230); }, 50 },
    { "yard-menu-step-aside-sad", [] { pokemon(25, 13); pet.fullness = 10; yardForceMap(1); render(); yardW.p[0].x = 360;
                                       yardW.p[0].y = 250; yardW.p[0].mode = YM_IDLE; onTap(360, 230); }, 50 },
    { "yard-menu-step-aside-feed", [] { pokemon(25, 13); if (!pet.uid) pet.uid = 0x7E57u; pet.fullness = 10; yardForceMap(1); render();
                                        yardW.p[0].x = 360; yardW.p[0].y = 250; yardW.p[0].mode = YM_IDLE; onTap(360, 230);
                                        menuTap(PM_FOOD); foodRowTap(0); }, 68 },
    { "yard-menu-care", [] { menuScene(); pet.joy = 40; menuTap(PM_CARE); }, 3 },
    { "yard-menu-care-es", [] { menuScene(false, LANG_ES); pet.joy = 40; menuTap(PM_CARE); }, 3 },
    { "yard-menu-sleep", [] { pokemon(25, 21); yardForceMap(1); tapPet(0); menuTap(PM_SLEEP); }, 2 },
    { "yard-menu-wake", [] { pokemon(25, 23); pet.sleeping = true; yardForceMap(1); tapPet(0); menuTap(PM_SLEEP); }, 2 },
    { "yard-menu-status", [] { menuScene(); menuTap(PM_STATUS); } },
    { "yard-menu-closed", [] { menuScene(); onTap(110, 230); }, 3 },
    // v3.3: the food list in the menu (4 per page + back; with more, back | 1/N), food in front of the pet
    { "yard-food-list", [] { menuScene(); pet.items[IT_CAKE] = 2; pet.items[IT_BERRY_B] = 4; menuTap(PM_FOOD); }, 2 },
    { "yard-food-list-es", [] { menuScene(false, LANG_ES); pet.items[IT_CAKE] = 2; pet.items[IT_BERRY_B] = 4; menuTap(PM_FOOD); }, 2 },
    { "yard-food-list-plain", [] { menuScene(); menuTap(PM_FOOD); }, 2 },
    { "yard-food-list-star", [] { menuScene(); pet.berryKnown = true; pet.items[IT_CAKE] = 2; pet.items[IT_BERRY_R + favBerryOf(25)] = 3; menuTap(PM_FOOD); }, 2 },
    { "yard-food-list-star-es", [] { menuScene(false, LANG_ES); pet.berryKnown = true; pet.items[IT_CAKE] = 2; pet.items[IT_BERRY_R + favBerryOf(25)] = 3; menuTap(PM_FOOD); }, 2 },
    { "yard-food-list-2pages", [] { menuScene(); for (uint8_t b = IT_BERRY_R; b <= IT_BERRY_O; b++) pet.items[b] = (uint8_t)(b - IT_BERRY_R + 1);
                                    pet.items[IT_CAKE] = 2; pet.items[IT_GOLDEN] = 9; menuTap(PM_FOOD); }, 2 },
    { "yard-food-list-2pages-es", [] { menuScene(false, LANG_ES); for (uint8_t b = IT_BERRY_R; b <= IT_BERRY_O; b++) pet.items[b] = (uint8_t)(b - IT_BERRY_R + 1);
                                       pet.items[IT_CAKE] = 2; pet.items[IT_GOLDEN] = 9; menuTap(PM_FOOD); }, 2 },
    { "yard-food-list-page2", [] { menuScene(); for (uint8_t b = IT_BERRY_R; b <= IT_BERRY_O; b++) pet.items[b] = 1;
                                   menuTap(PM_FOOD); foodMoreTap(); }, 2 },
    { "yard-food-list-page3", [] { menuScene(); for (uint8_t b = IT_BERRY_R; b <= IT_BERRY_O; b++) pet.items[b] = 1;
                                   pet.items[IT_CAKE] = 2; pet.items[IT_GOLDEN] = 1; menuTap(PM_FOOD); foodMoreTap(); foodMoreTap(); }, 2 },
    { "yard-walk-up", [] { yardWalkScene(25, 330, 120); }, 6 },
    { "yard-walk-down", [] { yardWalkScene(25, 150, 360); }, 6 },
    { "yard-walk-up-charmander", [] { yardWalkScene(4, 330, 120); }, 6 },
    { "yard-walk-down-charmander", [] { yardWalkScene(4, 150, 360); }, 6 },
    // fase Y3 / v3.3: la comida aparece delante del bicho y se la come ya; el huevo en el patio
    { "yard-feed-front", [] { feedTap(0); }, 2 },
    { "yard-feed-front-cake", [] { feedTap(2); }, 5 },
    { "yard-feed-heart", [] { feedTap(1); feedUntil(YF_NONE); }, 4 },
    { "yard-feed-heart-es", [] { feedTap(1, LANG_ES); feedUntil(YF_NONE); }, 4 },
    { "yard-feed-busy", [] { feedTap(0); gHostMillis += 85; render(); }, 1 },
    { "yard-feed-busy-es", [] { feedTap(0, LANG_ES); gHostMillis += 85; render(); }, 1 },
    { "yard-feed-busy-deny", [] { feedTap(0); gHostMillis += 85; render(); foodRowTap(1); }, 1 },
    { "yard-egg", [] { yardEggScene(40, 0); }, 40 },
    // v2.4: the egg in its type's colours. Tangela, grass only: all green; Zapdos, electric/flying:
    // yellow top-left, sky blue bottom-right, blended along a ( / ) diagonal
    { "yard-egg-grass", [] { yardEggScene(40, 0); pet.dbgEggSpecies(114); }, 40 },
    { "yard-egg-zapdos", [] { yardEggScene(40, 0); pet.dbgEggSpecies(145); }, 40 },
    { "yard-egg-night", [] { yardEggScene(150, 30); pet.lastSeenEpoch = epochAt(22); }, 20 },
    { "yard-egg-shake", [] { yardEggScene(40, 0); render(); pet.shaken(); yardEggInfo(); }, 3 },
    { "yard-egg-tap", [] { yardEggScene(160, 0); render(); onTap(yardEggX[0], yardEggY[0] - YARD_EGG_MID); }, 3 },
    { "yard-egg-tap-hint", [] { yardEggScene(160, 0); render(); onTap(yardEggX[0], yardEggY[0] - YARD_EGG_MID); }, 12 },
    { "yard-egg-tap-es", [] { yardEggScene(160, 0); lang(LANG_ES); render(); onTap(yardEggX[0], yardEggY[0] - YARD_EGG_MID); }, 3 },
    { "yard-egg-tap-hint-es", [] { yardEggScene(160, 0); lang(LANG_ES); render(); onTap(yardEggX[0], yardEggY[0] - YARD_EGG_MID); }, 12 },
    { "yard-egg-hatch-shake", [] { yardEggScene(239, 0); render(); pet.hatchNow(); ensureMon(); fxo.push(FXO_HATCH, 0, 0, millis()); }, 6 },
    { "yard-egg-hatch-flash", [] { yardEggScene(239, 0); render(); pet.hatchNow(); ensureMon(); fxo.push(FXO_HATCH, 0, 0, millis()); }, 9 },
    { "yard-egg-hatch-reveal", [] { yardEggScene(239, 0); render(); pet.hatchNow(); ensureMon(); fxo.push(FXO_HATCH, 0, 0, millis()); }, 12 },
    { "yard-egg-hatched", [] { yardEggScene(239, 0); render(); pet.hatchNow(); ensureMon(); fxo.push(FXO_HATCH, 0, 0, millis()); }, 40 },
    { "yard-starter-egg", [] { eggNow(); } },
    { "yard-starter-egg-es", [] { eggNow(); lang(LANG_ES); } },
    { "yard-tap-pet", [] { pokemon(25, 13); yardForceMap(1); render(); onTap((int)yardW.p[0].x, (int)yardW.p[0].y - 20); } },
    { "yard-tilt", [] { pokemon(25, 13); yardForceMap(1); gStubTiltSide = -1; }, 80 },
    { "yard-orbit", [] { pokemon(25, 13); yardForceMap(1); OrbitShift o = orbitShift(6); roundCanvas->setShift(o.dx, o.dy); }, 2 },
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
    // v1.58: a tap on a poop cleans just that one (the middle one here: sparkles where it was, no HYG any more)
    { "poop-tap", [] { pokemon(1, 9); pet.dbgSetPoops(3); onTap(116, 282); }, 3 },
    // v1.60: neglect. The bond fades with all four needs at 0, then "very weak", then the Revive dialog
    { "abandoned", [] { pokemon(4, 13); pet.fullness = pet.joy = pet.energy = 0; pet.bond = 60; } },
    { "abandoned-es", [] { pokemon(4, 13); pet.fullness = pet.joy = pet.energy = 0; pet.bond = 60;
                           setLang(LANG_ES); applyLangFont(); } },
    // la noche (v1.63): con sueño desde las 22, el globo con "zzz" (a la izquierda si anda por la derecha)
    { "sleepy", [] { pokemon(25, 22); } },
    { "sleepy-es", [] { pokemon(25, 22); setLang(LANG_ES); applyLangFont(); } },
    { "weak", [] { pokemon(4, 13); pet.fullness = pet.joy = pet.energy = 0; pet.bond = 0; } },
    { "dying-revive", [] { pokemon(6, 13); pet.fullness = pet.joy = pet.energy = 0; pet.dbgSetPoops(2);
                           pet.items[IT_REVIVE] = 3; pet.startDying(); }, 2 },
    { "dying-revive-es", [] { pokemon(6, 13); pet.fullness = pet.joy = pet.energy = 0; pet.items[IT_REVIVE] = 3;
                              pet.startDying(); setLang(LANG_ES); applyLangFont(); }, 2 },
    // v1.64: la hora del Revivir; pasada, el dialogo dice que murio
    { "dying-late", [] { pokemon(6, 13); pet.fullness = pet.joy = pet.energy = 0; pet.items[IT_REVIVE] = 3;
                         pet.startDying(); pet.dbgDyingMin(REVIVE_LIMIT_MIN); }, 2 },
    { "dying-late-es", [] { pokemon(6, 13); pet.fullness = pet.joy = pet.energy = 0; pet.items[IT_REVIVE] = 3;
                            pet.startDying(); pet.dbgDyingMin(REVIVE_LIMIT_MIN); setLang(LANG_ES); applyLangFont(); }, 2 },
    { "dying-left-es", [] { pokemon(6, 13); pet.fullness = pet.joy = pet.energy = 0; pet.items[IT_REVIVE] = 3;
                            pet.startDying(); pet.dbgDyingMin(18); setLang(LANG_ES); applyLangFont(); }, 2 },
    { "dying-none", [] { pokemon(25, 21); pet.fullness = pet.joy = pet.energy = 0; pet.startDying(); }, 2 },
    { "dying-none-es", [] { pokemon(25, 21); pet.fullness = pet.joy = pet.energy = 0; pet.startDying();
                            setLang(LANG_ES); applyLangFont(); }, 2 },
    { "revived", [] { pokemon(6, 13); pet.fullness = pet.joy = pet.energy = 0; pet.items[IT_REVIVE] = 3;
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
    { "play-menu", [] { pokemon(133, 12); openPlayPanel(); } },
    // v1.53: the Backpack screen
    { "bag", [] { pokemon(25, 13); walker(1840, 52310, 2, { 2, 1, 0, 3, 1, 1, 2, 3, 3 }); openBag(); } },
    // v1.59: the Revive (9th slot, 3x3 grid); a tap on it says what it's for (it isn't used by hand)
    { "bag-revive-hint", [] { pokemon(25, 13); walker(1840, 52310, 2, { 2, 1, 0, 3, 1, 1, 2, 3, 3 }); openBag(); bagTapItem(IT_REVIVE); } },
    { "bag-revive-hint-es", [] { pokemon(25, 13); walker(1840, 52310, 2, { 2, 1, 0, 3, 1, 1, 2, 3, 3 }); openBag(); bagTapItem(IT_REVIVE);
                                 setLang(LANG_ES); applyLangFont(); } },
    // v1.54: the pink FOOD bar while the cake lasts
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
    { "gifts-farewell-mark", [] { pokemon(55, 15); pet.dbgSetAge(FAREWELL_AGE_MIN + 60); pet.addGift(IT_CAKE); pet.addGift(IT_CAKE); pet.addGift(IT_CAKE); } },
    // v2.0 (Y2): the Play panel that replaces the popup, in Spanish, and its taps
    { "play-menu-es", [] { pokemon(133, 12); openPlayPanel(); setLang(LANG_ES); applyLangFont(); } },
    { "play-menu-no-imu-es", [] { pokemon(133, 12); gStubImuPresent = false; openPlayPanel(); setLang(LANG_ES); applyLangFont(); } },
    { "play-pressed", [] { pokemon(133, 12); openPlayPanel(); panelHold(PP_SACK); } },
    { "play-tap-ball", [] { pokemon(133, 12); openPlayPanel(); panelTapCard(PP_BALL); }, 2 },
    { "play-tap-stages", [] { pokemon(133, 12); openPlayPanel(); panelTapCard(PP_BATTLE); } },
    { "play-tap-sack", [] { pokemon(133, 12); openPlayPanel(); panelTapCard(PP_SACK); }, 2 },
    { "play-tap-zen", [] { pokemon(7, 13); openPlayPanel(); panelTapCard(PP_ZEN); }, 2 },
    { "play-tap-dash", [] { pokemon(123, 13); openPlayPanel(); panelTapCard(PP_DASH); }, 2 },
    { "play-no-imu-deny", [] { pokemon(133, 12); gStubImuPresent = false; openPlayPanel(); panelTapCard(PP_DASH); } },
    { "play-asleep-box", [] { pokemon(133, 23); pet.sleeping = true; boxThree(); openPlayPanel(); } },
    { "panel-back", [] { pokemon(133, 12); openPlayPanel(); onTap(Y_BACK_X, Y_BACK_Y); } },
    { "panel-user-key", [] { pokemon(133, 12); openPlayPanel(); onBack(); } },
    // v3.3: two home pets awake: the game asks who plays (the picker), then that pet plays; an item from the Bag
    { "pick-game", [] { pokemon(25, 13); yardForceMap(1); secondPet(4, 14, false, "CHAR"); openPlayPanel(); panelTapCard(PP_BALL); } },
    { "pick-game-es", [] { pokemon(25, 13); yardForceMap(1); secondPet(4, 14, false, "CHAR"); es(); openPlayPanel(); panelTapCard(PP_BALL); } },
    { "pick-game-zen", [] { pokemon(7, 13); yardForceMap(1); secondPet(133, 22); openPlayPanel(); panelTapCard(PP_ZEN); } },
    { "pick-game-chosen", [] { pokemon(25, 13); yardForceMap(1); secondPet(4, 14); openPlayPanel(); panelTapCard(PP_SACK);
                               int x, y, w, h; petPickCard(1, x, y, w, h); onTap(x + w / 2, y + h / 2); }, 2 },
    { "pick-game-one-asleep", [] { pokemon(25, 13); yardForceMap(1); secondPet(4, 14); pet.slotRef(1).sleeping = true;
                                   openPlayPanel(); panelTapCard(PP_BALL); }, 2 },
    { "pick-item", [] { pokemon(25, 13); yardForceMap(1); secondPet(4, 14); pet.items[IT_PROTEIN] = 2; openBag(); bagTapItem(IT_PROTEIN); } },
    { "pick-item-es", [] { pokemon(25, 13); yardForceMap(1); secondPet(4, 14); pet.items[IT_PROTEIN] = 2; es(); openBag(); bagTapItem(IT_PROTEIN); } },
    { "pick-item-used", [] { pokemon(25, 13); yardForceMap(1); secondPet(4, 14); pet.items[IT_GOLDEN] = 2; pet.slotRef(1).fullness = 20;
                             openBag(); bagTapItem(IT_GOLDEN); int x, y, w, h; petPickCard(1, x, y, w, h); onTap(x + w / 2, y + h / 2); }, 3 },
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
    // Y4: the coin gain on each result screen
    { "dash-result-coins", [] { dashPlaying(123, 13, 0, 6000); dash.dodged = 61; dashNewHi = true; dashGainShown = 15; dash.hits = 3;
                                pet.lastCoins = 8; dashOverUntil = millis() + 4000; } },
    { "zen-result-coins", [] { zenPlaying(7, 13, 50); zen.t = ZEN_SESSION_MS; zen.inBandMs = 23400; zenGainShown = 11;
                               zenNewHi = true; pet.lastCoins = 8; zenOverUntil = millis() + 4000; } },
    { "sack-result-coins", [] { pokemon(66, 13); startSack(); sackHits = 31; sackGain = 7; pet.lastCoins = 6;
                                sackOverUntil = millis() + 3000; } },
    { "ball-result-coins", [] { pokemon(25, 13); startGame(); gameScore = 22; pet.lastCoins = 5; gameOverUntil = millis() + 4000; } },
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
    { "box-wake-vacant-end", [] { vacantNow(10); boxThree(); openBox(); boxStartMove(4, 0); }, 8 },
    { "box-store-vacant-end", [] { pokemon(25, 13); boxThree(); openBox(); boxStartMove(2, 0); }, 8 },
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
    // goal banners carry their coins (Y4): "+10" on the 3000 one, "+40" on the 10000 one
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
    // Y4b: the Backpack tabs, the Shop and the background picker
    { "bag-tabs", [] { pokemon(25, 13); pet.coins = 1234; walker(1840, 52310, 2, { 2, 1, 0, 3, 1, 1, 2, 3, 3 }); openBag(); } },
    { "bag-tabs-es", [] { pokemon(25, 13); pet.coins = 37; walker(1840, 52310, 2, { 2, 1, 0, 3, 1, 1, 2, 3, 3 }); openBag(); es(); } },
    { "shop-rich", [] { shopOpen(500, { 2, 1, 0, 3, 1, 1, 2, 3, 3 }); } },
    { "shop-poor", [] { shopOpen(35, { 0, 0, 0, 0, 0, 0, 0, 0, 1 }); } },
    { "shop-mid", [] { shopOpen(100, { 9, 0, 0, 0, 0, 4, 0, 1, 0 }); } },
    { "shop-rich-es", [] { shopOpen(500, { 2, 1, 0, 3, 1, 1, 2, 3, 3 }); es(); } },
    { "shop-poor-es", [] { shopOpen(35); es(); } },
    // v3.2: coloured berries (the pet menu's food list, Backpack page 2, Shop page 2, the card's favourite line)
    { "food-list-berries", [] { menuScene(); pet.items[IT_BERRY_B] = 4; pet.items[IT_BERRY_Y] = 1; pet.items[IT_CAKE] = 2; menuTap(PM_FOOD); }, 2 },
    { "food-list-berries-es", [] { menuScene(false, LANG_ES); pet.items[IT_BERRY_B] = 4; pet.items[IT_BERRY_Y] = 1; pet.items[IT_CAKE] = 2; menuTap(PM_FOOD); }, 2 },
    { "bag-berries", [] { pokemon(25, 13); pet.items[IT_BERRY_R] = 3; pet.items[IT_BERRY_G] = 1; pet.items[IT_BERRY_O] = 9; openBag(); bagPage = 1; } },
    { "bag-berries-es", [] { pokemon(25, 13); pet.items[IT_BERRY_R] = 3; pet.items[IT_BERRY_G] = 1; pet.items[IT_BERRY_O] = 9; openBag(); bagPage = 1; es(); } },
    { "bag-berries-confirm", [] { pokemon(25, 13); pet.items[IT_BERRY_P] = 2; openBag(); bagPage = 1; itemConfirm = IT_BERRY_P + 1; itemConfirmUntil = millis() + 10000; } },
    { "shop-berries", [] { shopOpen(500, { 0 }, 1); pet.items[IT_BERRY_B] = 2; } },
    { "shop-berries-es", [] { shopOpen(500, { 0 }, 1); pet.items[IT_BERRY_B] = 2; es(); } },
    { "card-fav-known", [] { pokemon(25, 13); pet.berryKnown = true; cardOpen = true; cardPage = 0; } },
    { "card-fav-known-es", [] { pokemon(25, 13); pet.berryKnown = true; es(); cardOpen = true; cardPage = 0; } },
    { "shop-confirm", [] { shopOpen(500); shopAsk(IT_RARECANDY); } },
    { "shop-confirm-poor", [] { shopOpen(35); shopAsk(IT_REVIVE); } },
    { "shop-confirm-es", [] { shopOpen(500); shopAsk(IT_RARECANDY); es(); } },
    { "shop-toast-ok", [] { shopOpen(500, { 3, 0, 0, 0, 0, 0, 0, 0, 0 }); shopToastNow(SHT_OK); } },
    { "shop-toast-full", [] { shopOpen(500, { 9, 0, 0, 0, 0, 0, 0, 0, 0 }); shopToastNow(SHT_FULL); } },
    { "shop-toast-broke-es", [] { shopOpen(5); shopToastNow(SHT_BROKE); es(); } },
    { "shop-toast-ok-es", [] { shopOpen(500); shopToastNow(SHT_OK); es(); } },
    { "shop-bought", [] {  // through the real taps: card, Buy, then the new count
       shopOpen(500, { 2, 0, 0, 0, 0, 0, 0, 0, 0 });
       render();
       int x, y;
       bagSlotXY(0, x, y);
       onTap(x + 30, y + 30);
       render();
       int xy, wy, xn, wn;
       shopAskPills(T(S_BUY), xy, wy, xn, wn);
       onTap(xy + wy / 2, BAG_ASK_PILL_Y + 14);
     } },
    { "picker-missing", [] { pickerOpen(4, 300); } },
    { "picker-missing-es", [] { pickerOpen(4, 300); es(); } },
    { "picker-owned", [] { pickerOpen(2, 50, 1 << 2); } },
    { "picker-notowned", [] { pickerOpen(4, 300); } },
    { "picker-broke", [] { pickerOpen(4, 10); } },
    { "picker-using", [] { pickerOpen(YARD_MAP_START, 50); } },
    { "picker-using-chosen", [] { pickerOpen(3, 50, 1 << 3); pet.yardChosen = 3; } },
    { "picker-bought", [] { pickerOpen(3, 300); shopToastNow(SHT_OK); pet.buyMap(3); pet.chooseMap(3); } },
    { "picker-owned-es", [] { pickerOpen(2, 50, 1 << 2); es(); } },
    { "picker-notowned-es", [] { pickerOpen(4, 300); es(); } },
    { "picker-broke-es", [] { pickerOpen(4, 10); shopToastNow(SHT_BROKE); es(); } },
    { "picker-using-es", [] { pickerOpen(YARD_MAP_START, 50); es(); } },
    { "card-profile-es", [] { pokemon(25, 13); pet.streak = 365; pet.bestStreak = 365; setLang(LANG_ES); applyLangFont(); cardOpen = true; cardPage = 0; } },
    // v2.0: las 4 necesidades en el perfil (en el patio ya no hay barras)
    { "card-profile-needs", [] { pokemon(25, 13); pet.fullness = 20; pet.joy = 45; pet.energy = 10; cardOpen = true; cardPage = 0; } },
    { "card-profile-needs-es", [] { pokemon(25, 13); pet.fullness = 20; pet.joy = 45; pet.energy = 10;
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
    // travel and the collection (v2.0, Y5b)
    { "travel-card", [] { pokemon(25, 13); openPlayPanel(); } },
    { "travel-card-es", [] { pokemon(25, 13); openPlayPanel(); es(); } },
    { "travel-setup", [] { travelSetup(3, { 2, 4 }); trvSelPet = 1; trvSelDest = 2; trvSelTier = 1; } },
    { "travel-setup-es", [] { travelSetup(3, { 2, 4 }); es(); trvSelPet = 2; trvSelDest = 4; trvSelTier = 2; } },
    { "travel-setup-1map", [] { travelSetup(2, {}); } },
    { "travel-setup-page2", [] { travelSetup(6, { 2, 3, 4, 5 }); trvSelPet = 4; trvSelDest = 5; } },
    { "travel-setup-nopets", [] { travelSetup(0, { 2 }); } },
    { "travel-setup-nopets-es", [] { travelSetup(0, { 2 }); es(); } },
    { "travel-setup-noclock", [] { travelSetup(3, { 2 }); pet.lastSeenEpoch = 0; } },
    { "travel-trip-early", [] { travelRunning(2, 1, 6); } },
    { "travel-trip-early-es", [] { travelRunning(4, 2, 12); es(); } },
    { "travel-trip-mid", [] { travelRunning(2, 1, 150); } },
    { "travel-trip-late", [] { travelRunning(4, 2, 470); } },
    { "travel-trip-late-es", [] { travelRunning(4, 2, 470); es(); } },
    { "travel-trip-last-es", [] { travelRunning(3, 2, 475); es(); } },
    { "travel-trip-hour", [] { travelRunning(1, 0, 20); } },
    { "travel-back-yard", [] { travelBack(2, 0, false); }, 3 },
    { "travel-back-tapbubble", [] { travelBack(2, 0, false); render(); tapTraveller(true); }, 3 },
    { "travel-back-new", [] { travelBack(2, 0, false); render(); tapTraveller(false); }, 3 },
    { "travel-back-new-es", [] { travelBack(4, 0, false); es(); render(); tapTraveller(false); }, 3 },
    { "travel-back-dup", [] { travelBack(2, 0x1FFFFFF, true); render(); tapTraveller(false); }, 3 },
    { "travel-back-dup-es", [] { travelBack(3, 0x1FFFFFF, true); es(); render(); tapTraveller(false); }, 3 },
    { "travel-back-early-tap", [] { travelBack(2, 0, false); render(); tapTraveller(false); render(); onTap(CX, CY); }, 3 },
    { "travel-back-dismiss", [] {
       travelBack(2, 0, false); render(); tapTraveller(false); render();
       returnOpenAt -= 2000; onTap(CX, CY);
     }, 3 },
    { "box-travel", [] { pokemon(25, 13); boxThree(); ownMaps({ 2 }); pet.startTravel(1, 2, 1); pet.dbgTravelElapsed(30); openBox(); } },
    { "box-travel-es", [] { pokemon(25, 13); boxThree(); ownMaps({ 2 }); pet.startTravel(1, 2, 2); pet.dbgTravelElapsed(30); openBox(); es(); } },
    { "box-travel-tap", [] {
       pokemon(25, 13); boxThree(); ownMaps({ 2 }); pet.startTravel(1, 2, 1); pet.dbgTravelElapsed(30); openBox();
       onTap(boxCellX(1) + BOX_CW / 2, boxCellY(1) + BOX_CH / 2);
     } },
    { "box-travel-tap-es", [] {
       pokemon(25, 13); boxThree(); ownMaps({ 2 }); pet.startTravel(1, 2, 1); pet.dbgTravelElapsed(30); openBox(); es();
       onTap(boxCellX(1) + BOX_CW / 2, boxCellY(1) + BOX_CH / 2);
     } },
    { "collection-empty", [] { collection(0); } },
    { "collection-partial", [] { collection(0x0A5B6D3); } },
    { "collection-partial-es", [] { collection(0x0A5B6D3); es(); } },
    { "collection-rare", [] { collection((1u << 4) | (1u << 9) | (1u << 19) | (1u << 0) | (1u << 6) | (1u << 22)); } },
    { "collection-full", [] { collection(0x1FFFFFF); } },
    { "collection-tip-owned", [] { collectionTip(0x0A5B6D3, 6); } },
    { "collection-tip-rare", [] { collectionTip(0x0A5B6D3 | (1u << 14), 14); } },
    { "collection-tip-unknown", [] { collectionTip(0x0A5B6D3, 2); } },
    { "collection-tip-es", [] { collection(0x0A5B6D3); es(); onTap(souvCellX(18) + 10, souvCellY(18) + 10); } },
    { "bag-tabs-es", [] { pokemon(25, 13); walker(1840, 52310, 2, { 2, 1, 0, 3, 0, 1, 0 }); openBag(); es(); } },
    { "shop-tabs-es", [] { shopOpen(120); es(); } },
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
    // v3.0: with two home pets the Box asks which one goes in, or which one the woken pet swaps with
    { "box-which-store", [] { pokemon(25, 13); secondPet(4, 14); boxThree(); openBox(); boxTap(boxCellX(2) + BOX_CW / 2, boxCellY(2) + BOX_CH / 2); } },
    { "box-which-store-es", [] { pokemon(25, 13); secondPet(4, 14); boxThree(); es(); openBox(); boxTap(boxCellX(2) + BOX_CW / 2, boxCellY(2) + BOX_CH / 2); } },
    { "box-which-swap", [] { pokemon(25, 13); secondPet(4, 14); boxThree(); openBox(); boxTap(boxCellX(4) + BOX_CW / 2, boxCellY(4) + BOX_CH / 2); } },
    { "box-which-swap-es", [] { pokemon(25, 13); secondPet(4, 14); boxThree(); es(); openBox(); boxTap(boxCellX(4) + BOX_CW / 2, boxCellY(4) + BOX_CH / 2); } },
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
    { "box-store-fx", [] { pokemon(25, 13); boxThree(); openBox(); boxStartMove(2, 0); }, 4 },
    { "box-swap-fx", [] { pokemon(25, 13); boxThree(); openBox(); boxStartMove(4, 0); }, 4 },
    { "box-store-end", [] { pokemon(25, 13); boxThree(); openBox(); boxStartMove(2, 0); }, 8 },
    // wild encounters (v2.2): the object in the yard on each map (rustling, still, shining), the intro, the
    // one-fighter picker, the fight, the result, the Poke Ball question, the throw and the end
    { "enc-yard-meadow", [] { encScene(16, 1); encRustleAt(350); } },
    { "enc-yard-meadow-still", [] { encScene(16, 1); encRustleAt(1500); } },
    { "enc-yard-forest", [] { encScene(10, 3); encRustleAt(350); } },
    { "enc-yard-quarry", [] { encScene(74, 2); encRustleAt(350); } },
    { "enc-yard-beach", [] { encScene(7, 4); encRustleAt(350); } },
    { "enc-yard-ruins", [] { encScene(95, 5); encRustleAt(350); } },
    { "enc-yard-exclusive", [] { encScene(147, 4, 20); encRustleAt(350); } },
    { "enc-yard-dusk", [] { encScene(16, 1, 12, false, 19); encRustleAt(350); } },
    { "enc-intro-bands", [] { encScene(16, 1); encTapYard(); }, 2 },
    { "enc-intro", [] { encScene(16, 1); encTapYard(); }, 14 },
    { "enc-intro-es", [] { encScene(16, 1); lang(LANG_ES); encTapYard(); }, 14 },
    { "enc-intro-shiny", [] { encScene(147, 4, 20, true); encTapYard(); }, 14 },
    { "enc-intro-shiny-es", [] { encScene(147, 4, 20, true); lang(LANG_ES); encTapYard(); }, 14 },
    { "enc-team", [] { encScene(16, 1); boxThree(); encTapYard(); gHostMillis += 1000; onTap(CX, CY); } },
    { "enc-team-es", [] { encScene(16, 1); boxThree(); lang(LANG_ES); encTapYard(); gHostMillis += 1000; onTap(CX, CY); } },
    { "enc-battle-vs", [] { encScene(16, 1); startWild(pet.uid); }, 2 },
    { "enc-battle-vs-es", [] { encScene(16, 1); lang(LANG_ES); startWild(pet.uid); }, 2 },
    { "enc-battle-mid", [] {
       encScene(16, 1);
       startWild(pet.uid);
       battleUntil([] { return bt.run.eng.attacks >= 3 && millis() - bt.evAt >= BT_HIT_MS + 150; });
     } },
    { "enc-result-win", [] { encScene(16, 1, 6); encFight(60); gHostMillis += BT_END_LOCK_MS; } },
    { "enc-result-win-es", [] { encScene(16, 1, 6); lang(LANG_ES); encFight(60); gHostMillis += BT_END_LOCK_MS; } },
    { "enc-result-lose", [] { encScene(149, 1, 70); encFight(2); gHostMillis += BT_END_LOCK_MS; } },
    { "enc-result-lose-es", [] { encScene(149, 1, 70); lang(LANG_ES); encFight(2); gHostMillis += BT_END_LOCK_MS; } },
    { "enc-ball", [] { encScene(16, 1, 6); encAfter(60); }, 2 },
    { "enc-ball-es", [] { encScene(16, 1, 6); lang(LANG_ES); encAfter(60); }, 2 },
    { "enc-ball-none-es", [] { encScene(16, 1, 6); lang(LANG_ES); encAfter(60); pet.items[IT_BALL] = 0; }, 2 },
    { "enc-ball-full", [] { encScene(16, 1, 6); encAfter(60); pet.items[IT_EGG] = ITEM_MAX; }, 2 },
    { "enc-throw-fly", [] { encScene(16, 1, 6); encAfter(60); gHostMillis += 1000; int xT, wT, xN, wN; encBallPills(xT, wT, xN, wN); onTap(xT + wT / 2, ENC_PILL_Y + 14); }, 3 },
    { "enc-throw-wobble", [] { encScene(16, 1, 6); encAfter(60); gHostMillis += 1000; int xT, wT, xN, wN; encBallPills(xT, wT, xN, wN); onTap(xT + wT / 2, ENC_PILL_Y + 14); }, 11 },
    { "enc-throw-click", [] { encScene(16, 1, 6); encAfter(60); gHostMillis += 1000; int xT, wT, xN, wN; encBallPills(xT, wT, xN, wN); onTap(xT + wT / 2, ENC_PILL_Y + 14); }, 27 },
    { "enc-caught", [] { encScene(16, 1, 6); encAfter(60); gHostMillis += 1000; int xT, wT, xN, wN; encBallPills(xT, wT, xN, wN); onTap(xT + wT / 2, ENC_PILL_Y + 14); }, 40 },
    { "enc-caught-es", [] { encScene(16, 1, 6); lang(LANG_ES); encAfter(60); gHostMillis += 1000; int xT, wT, xN, wN; encBallPills(xT, wT, xN, wN); onTap(xT + wT / 2, ENC_PILL_Y + 14); }, 40 },
    { "enc-left-es", [] { encScene(16, 1, 6); lang(LANG_ES); encAfter(60); gHostMillis += 1000; int xT, wT, xN, wN; encBallPills(xT, wT, xN, wN); onTap(xN + wN / 2, ENC_PILL_Y + 14); }, 4 },
    { "enc-fled", [] { encScene(149, 1, 70); encAfter(2); }, 4 },
    { "enc-fled-es", [] { encScene(149, 1, 70); lang(LANG_ES); encAfter(2); }, 10 },
    { "enc-yard-won", [] { encScene(16, 1, 6); encAfter(60); closeEncUi(); encRustleAt(350); } },
    { "enc-bag", [] { pokemon(25, 13); pet.items[IT_BALL] = 3; pet.items[IT_EGG] = 1; pet.items[IT_REVIVE] = 2; pet.items[IT_CAKE] = 4; openBag(); } },
    { "enc-bag-es", [] { pokemon(25, 13); lang(LANG_ES); pet.items[IT_BALL] = 3; pet.items[IT_EGG] = 1; pet.items[IT_REVIVE] = 2; pet.items[IT_CAKE] = 4; openBag(); } },
    { "enc-bag-ball-hint-es", [] {
       pokemon(25, 13);
       lang(LANG_ES);
       pet.items[IT_BALL] = 3;
       openBag();
       bagTapItem(IT_BALL);
     } },
    { "enc-shop", [] { pokemon(25, 13); pet.coins = 500; openBag(); bagTab = 1; } },
    { "enc-shop-es", [] { pokemon(25, 13); lang(LANG_ES); pet.coins = 500; openBag(); bagTab = 1; } },
    { "enc-bag-use-ask", [] { pokemon(25, 13); pet.items[IT_CAKE] = 2; openBag(); itemConfirm = IT_CAKE + 1; itemConfirmUntil = millis() + 5000; } },
    // stage select, team picker and the badges page (v1.65)
    { "stages-fresh", [] { pokemon(25, 13); stageSetup(0, 0, false); openStages(); } },
    { "stages-fresh-es", [] { pokemon(25, 13); stageSetup(0, 0, true); openStages(); } },
    { "stages-mid", [] { pokemon(25, 13); stageSetup(0x07, 0x03, false); openStages(); } },
    { "stages-mid-es", [] { pokemon(25, 13); stageSetup(0x07, 0x03, true); openStages(); } },
    { "stages-all", [] { pokemon(25, 13); stageSetup(0xFF, 0x1F, false); openStages(); } },
    { "stages-all-es", [] { pokemon(25, 13); stageSetup(0xFF, 0x1F, true); openStages(); } },
    { "team-home", [] { pokemon(25, 13); teamSetup({}, 1, false); } },
    { "team-two-home", [] { pokemon(25, 13); secondPet(4, 14); boxThree(); teamSetup({}, 1, false); } },
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
    { "battle-clear-coins", [] {  // first clear: item + badge + team + joy + 50 coins (the tallest card)
       pokemon(25, 13); boxThree(); stageGo(2, { -1, 4 }); stageResult(0, 2, 100, 1, BT_END_LOCK_MS); bt.rw.coins = ECO_STAGE_FIRST;
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
static const char *const SHEET[] = { "yard-home-two", "travel-back-yard", "travel-trip-late", "travel-back-new", "collection-partial", "shop-rich",
                                      "enc-yard-meadow", "enc-intro", "enc-ball", "enc-caught" };

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
  pet.timedEggs = true;  // as setup() does: eggs go to the yard (Y3)
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
