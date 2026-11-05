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
  galleryOpen = cardOpen = kbOpen = clockOpen = gameOpen = sackOpen = false;
  galleryDetail = 0;
  galleryPage = 0;
  cardPage = 0;
  feedMenuUntil = confirmUntil = choiceUntil = bathUntil = 0;
  choiceKind = 0;
  pet.sleeping = false;
  pet.ceremony = CER_NONE;
  // paseo e IMU (v1.20)
  itemConfirm = 0;
  goalBannerUntil = 0;
  foundShown = false;
  while (pet.hasFound()) pet.popFound();
  pet.takeGoalBanner();
  pet.clearEmote();
  pet.stepsToday = pet.stepsTotal = 0;
  pet.findsToday = 0;
  pet.goalDay = 0;
  memset(pet.items, 0, sizeof(pet.items));
  gStubImuPresent = true;
  gStubTiltSide = 0;
  gStubTiltSlide = false;
  gBright = BRIGHT_DEFAULT;
  briToastUntil = briSaveAt = 0;
  screenKeyValid = false;
  if (gLang != LANG_EN) { setLang(LANG_EN); applyLangFont(); }
}

// steps and Backpack for the WALK page scenes
static void walker(uint32_t today, uint32_t total, uint8_t finds, std::initializer_list<uint8_t> items) {
  pet.stepDay = pet.dayNumber();
  pet.stepsToday = today;
  pet.stepsTotal = total;
  pet.findsToday = finds;
  int i = 0;
  for (uint8_t n : items) pet.items[i++] = n;
  if (today >= STEP_GOAL) pet.goalDay = pet.dayNumber();
}

// a hatched pet of the given species, healthy, `hour` o'clock
static void pokemon(int16_t dex, int hour, bool shiny = false) {
  resetUi();
  if (pet.awaitingStarter()) pet.chooseStarter(dex);  // a fresh save asks for a starter first
  pet.speciesId = dex;
  pet.shiny = shiny;
  pet.ageMinutes = 14 * MINUTES_PER_LEVEL + 20;
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
    { "feed", [] { pokemon(133, 12); feedMenuUntil = millis() + 6000; } },
    { "release", [] { pokemon(54, 15); confirmUntil = millis() + 10000; } },
    { "evolve", [] { pokemon(4, 11); pet.ageMinutes = 17 * MINUTES_PER_LEVEL; choiceKind = 1; choiceUntil = millis() + 12000; } },
    { "card-profile", [] { pokemon(25, 13); cardOpen = true; cardPage = 0; } },
    { "card-battle", [] { pokemon(25, 13); cardOpen = true; cardPage = 1; } },
    { "card-medals", [] { pokemon(25, 13); cardOpen = true; cardPage = 2; } },
    { "card-progress", [] { pokemon(25, 13); cardOpen = true; cardPage = 3; } },
    { "settings", [] { pokemon(25, 13); openClock(); } },
    { "settings-bright-max", [] { pokemon(25, 13); openClock(); gBright = BRIGHT_LEVELS - 1; } },
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
    { "walk-goal", [] { pokemon(25, 13); walker(3512, 61200, 4, { 4, 0, 9, 1, 1 }); cardOpen = true; cardPage = 4; } },
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
     }, 2 },
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
