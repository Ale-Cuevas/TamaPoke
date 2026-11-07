// TamaPoke - tamagotchi pixel art inspirado en la gen 1
// para Waveshare ESP32-S3-Touch-AMOLED-2.16 (AMOLED cuadrada de 480x480). La UI
// se diseno para la pantalla redonda de 466x466 de la AMOLED-1.75: se sigue
// dibujando en ese espacio y round_display.h la coloca en el centro del panel
// con un marco redondo negro.
//
// Librerias (Library Manager o repo de Waveshare):
//   - "GFX Library for Arduino" (moononournation), con soporte CO5300 QSPI
//   - "SensorLib" (Lewis He), driver tactil CST9220 (TouchDrvCST92xx)
//   - "XPowersLib" (Lewis He), PMU AXP2101
//   - "U8g2" (olikraus), solo los datos de las fuentes CJK
// Versiones fijadas en platformio.ini (y en la CI para arduino-cli).
//
// Placa: ESP32S3 Dev Module | Flash 16MB | PSRAM: OPI PSRAM | USB CDC On Boot: Enabled
//
// Los sprites y la tabla de especies se generan con tools/sprites.py (emit).

#include <Arduino.h>
#include <Wire.h>
#include "Arduino_GFX_Library.h"
#include "TouchDrvCSTXXX.hpp"
#include <U8g2lib.h>  // fuentes CJK (japones); ver applyLangFont()
#include <esp_sleep.h>
#include <driver/gpio.h>
#include <nvs.h>  // comando NVS: entradas usadas y libres (la caja, v1.37)
#include "board_amoled_216.h"
#include "round_display.h"
#include "species.h"
#include "dex.h"
#include "pet.h"
#include "sdmon.h"
#include "rtcbat.h"
#include "i18n.h"
#include "audio.h"
#include "blit.h"
#include "power.h"
#include "button.h"
#include "imu.h"    // pasos y trucos de movimiento (QMI8658); SensorLib queda en imu.cpp
#include "steps.h"  // STEP_FLUSH_MS
#include "battle.h" // motor del combate de practica (logica pura)
#include "wind.h"   // viento del paisaje: hierba, flores y copas que se mecen (logica pura)
#include "burnin.h" // contra el quemado del AMOLED: orbita de la imagen y salvapantallas (logica pura)
#include "fx.h"     // efectos de respuesta: celebraciones en cola, "+N", botones, eclosion (logica pura)
#include "ballgame.h" // minijuego de la pokeball: velocidad segun la puntuacion y la fisica (logica pura)

// Version del firmware. Subir este numero en cada release (y manifest.json para
// el instalador web). Se muestra en la pantalla de ajustes y por serie al arrancar.
#define FW_VERSION "1.39"

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  BOARD_LCD_CS, BOARD_LCD_SCLK, BOARD_LCD_SDIO0, BOARD_LCD_SDIO1, BOARD_LCD_SDIO2, BOARD_LCD_SDIO3);
Arduino_CO5300 *panel = new Arduino_CO5300(
  bus, BOARD_LCD_RESET, 0 /*rotation*/, BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT,
  BOARD_LCD_COL_OFFSET, BOARD_LCD_ROW_OFFSET, BOARD_LCD_COL_OFFSET, BOARD_LCD_ROW_OFFSET);
// Framebuffer de 466x466 en PSRAM: dibujamos todo y hacemos flush() (sin
// parpadeo). RoundCanvas lo vuelca centrado en el panel de 480x480 con el marco
// redondo aplicado (round_display.h).
RoundCanvas *roundCanvas = new RoundCanvas(panel);
Arduino_Canvas *gfx = roundCanvas;

TouchDrvCST92xx touch;
// fuente CJK activa: de momento siempre false, la carga llegara con el
// soporte de japones/coreano/chino (ver PR #13)
bool gCjkFont = false;
Pet pet;

// sprite animado de la SD para la especie actual (si existe el archivo)
SdMon mon;          // sprite B/N (respaldo y minijuego si no hay PMD)
PmdMon pmd;         // sprite PMD multi-accion (pantalla principal)
PmdMon evoPmd;      // forma anterior, solo durante el parpadeo de evolucion
int16_t monFor = -2;
bool monShinyFor = false;

// comportamiento del bicho en pantalla
struct {
  uint8_t mode = 0;     // 0 idle, 1 paseo, 2 gesto one-shot
  uint8_t act = PMD_IDLE;
  uint32_t t0 = 0;      // inicio de la animacion en curso
  uint32_t until = 0;   // fin del estado actual
  float x = 233, targetX = 233;
} beh;
uint32_t walkLast = 0;  // instante del ultimo paso del paseo (0 = ninguno aun)
#define PET_GROUND 304  // linea de suelo de la mascota
PmdMon galleryPmd;  // sprite grande de la vista detalle de la galeria (PMD/TPK2, legal)

// galeria pokedex
bool galleryOpen = false;
bool galleryDirty = false;
int galleryPage = 0;        // 10 paginas de 16
int16_t galleryDetail = 0;  // dex en vista detalle, 0 = rejilla

// la caja (v1.37, docs/NEXT_FEATURES.md BOX): deslizar a la derecha desde casa
bool boxOpen = false;
uint8_t boxAsk = 0;           // dialogo de guardar / despertar: hueco + 1 (0 = ninguno)
uint32_t boxAskUntil = 0;     // se cierra solo, como "soltar?"
enum : uint8_t { BOXMV_NONE = 0, BOXMV_STORE, BOXMV_WAKE, BOXMV_SWAP };
uint8_t boxMove = BOXMV_NONE; // movimiento en marcha: la luz vuela, despues se escribe en flash
uint8_t boxMoveSlot = 0;
uint32_t boxMoveStart = 0;
bool boxMoveDrawnEnd = false; // ya se vio el ultimo cuadro (como quedara): se puede escribir
int8_t boxShakeSlot = -1;     // hueco que no se puede usar: tiembla (como un boton, fx.h)
uint32_t boxShakeUntil = 0;
int boxNowCx = 233;           // centro de la miniatura del activo (de ahi sale o ahi llega la luz)

bool screenOff = false;       // pulsacion corta del boton PWR (la tercera de cada vuelta, v1.32)
// salvapantallas puesto con PWR (v1.32): comer, el corazon o una evolucion no lo quitan solos,
// como hacen con la pantalla atenuada; cualquier toque, tecla o sacudida si (wakeUI)
bool saverPinned = false;
bool cardOpen = false;        // ficha del bicho (deslizar vertical)
bool kbOpen = false;          // teclado para renombrar al bicho
char nameBuf[12] = "";
uint8_t nameLen = 0;
uint8_t cardPage = 0;         // 0 perfil, 1 combate, 2 medallas, 3 progreso, 4 paseo
#define CARD_PAGES 5
uint8_t itemConfirm = 0;      // pagina de paseo: objeto de la mochila a confirmar + 1 (0 = ninguno)
uint32_t itemConfirmUntil = 0;
bool foundShown = false;      // el ultimo frame enseno un hallazgo: un toque lo cierra
static_assert(S_IT_EGG - S_IT_GOLDEN == IT_EGG - IT_GOLDEN && IT_EGG + 1 == IT_COUNT, "nombres de objeto en el orden de IT_*");
bool clockOpen = false;       // pantalla de ajuste de hora (deslizar abajo)
int clockH = 12, clockM = 0;  // hora en edicion

// escena de bano: espuma sobre el bicho y limpieza al reventar
uint32_t bathUntil = 0;
bool bathPending = false;
struct { int16_t x, y; uint8_t r, ph; } bubbles[14];
// menu emergente sobre la fila de botones: comida (COMER) o juego/combate (JUGAR)
enum : uint8_t { MENU_NONE = 0, MENU_FEED, MENU_PLAY };
uint8_t menuKind = MENU_NONE;
uint32_t menuUntil = 0;       // se cierra solo a este millis
// menu de JUGAR (v1.34): pelota | combate | saco de entrenamiento, 3 huecos de 66 como los
// del menu de comida, 198 x 64. Todos los juegos estan aqui; el saco antes salia de la ficha.
#define PLAY_MENU_SLOT 66
#define PLAY_MENU_W (3 * PLAY_MENU_SLOT)
#define PLAY_MENU_X (CX - PLAY_MENU_W / 2)

// minijuego "toques": mantener la pokeball en el aire
bool gameOpen = false;
uint32_t gameOverUntil = 0;
BallState ball;              // la pokeball (ballgame.h)
float gamePetX;
uint32_t lastGameStep = 0;  // ultima llamada a stepGame(): fisica por tiempo real, no por frame
uint8_t gameScore, gameMisses;
bool gameNoSave = false;    // partida abierta con el comando BALL: no toca el record ni da premio
float hitX, hitY;             // ultimo golpe (anillo de impacto)
uint32_t hitTime = 0;
bool gameNewHi = false;

// saco de entrenamiento (entrena la fuerza)
bool sackOpen = false;
uint32_t sackUntil = 0, sackOverUntil = 0;
uint16_t sackHits = 0;
float sackShake = 0;
uint8_t sackGain = 0;
bool sackNewHi = false;

// definidas al final; se declaran aqui porque el conversor .ino de PlatformIO no
// genera prototipos para funciones que devuelven punteros
const char *eggMsg();
const char *statusMsg();

// las 9 especies con sprite propio en flash (respaldo sin SD): dex -> indice
int flashIdxForDex(int16_t dex) {
  static const int8_t IDX[10] = { -1, 3, 4, 5, 0, 1, 2, 6, 7, 8 };
  return (dex >= 1 && dex <= 9) ? IDX[dex] : -1;
}

#define CX 233  // centro de la pantalla redonda
#define CY 233
#define PET_CY 202  // centro vertical del sprite

static const uint16_t INK_K = 0x18C4;  // spriteColor('k')

// botones de icono siguiendo el arco inferior de la pantalla redonda
// (los exteriores van mas altos para no salirse del circulo)
struct Btn {
  int16_t cx, cy;
  const char *const *icon;
};
Btn buttons[4] = {
  { 140, 390, SPR_ICON_FOOD },   // comer
  { 202, 404, SPR_ICON_PLAY },   // jugar
  { 264, 404, SPR_ICON_LIGHT },  // luz
  { 326, 390, SPR_ICON_CLEAN },  // bano
};
#define BTN_HALF 26  // boton de 52x52
#define BTN_HIT 36   // radio tactil (un poco mas generoso)

// grietas del huevo (pixeles 'k' sobre el sprite)
static const uint8_t CRACK1[][2] = { {15,8},{16,9},{15,10} };
static const uint8_t CRACK2[][2] = { {11,13},{12,14},{11,15},{20,12},{19,13},{20,14} };
// estrellas del modo noche
static const uint16_t STARS[][2] = { {120,140},{330,120},{370,210},{95,230},{280,90},{160,95} };

bool wasPressed = false;
// puntos que getPoint() puede llegar a escribir: el CST92xx reporta hasta 2
// (CST92XX_MAX_FINGER_NUM en SensorLib, protegido); 5 cubre cualquier CSTxxx
#define TOUCH_MAX_POINTS 5
// eleccion de inicial (primera partida): Bulbasaur / Charmander / Squirtle, 3 filas
static const int16_t STARTER_DEX[3] = { 1, 4, 7 };
#define STARTER_ROW_Y 110
#define STARTER_ROW_H 70
#define STARTER_ROW_GAP 8
// boton-CTA de evolucion (centrado, mitad de pantalla)
#define EVO_BTN_W 256
#define EVO_BTN_H 64
#define EVO_BTN_X (CX - EVO_BTN_W / 2)
#define EVO_BTN_Y 172
// boton-CTA de despedida (mas ancho: lleva el nombre + frase)
#define FAR_BTN_W 408
#define FAR_BTN_H 58
#define FAR_BTN_X (CX - FAR_BTN_W / 2)
#define FAR_BTN_Y 176
// el CST9220 avisa por el pin INT cuando hay datos tactiles; lo usamos para no
// leer el bus I2C mientras el chip esta dormido (esa lectura se colgaba ~1s)
volatile bool gTouchIrq = false;
// static: asi el conversor .ino de PlatformIO no le genera un prototipo (que
// llevaria IRAM_ATTR con otra seccion y el compilador avisaria del conflicto)
static void IRAM_ATTR touchIsr() { gTouchIrq = true; }
uint32_t lastRender = 0;
uint32_t sdEpoch = 0;  // +1 cada vez que se recarga la SD (las miniaturas cambian)
// proteccion del AMOLED: atenuado por inactividad
uint32_t lastInteract = 0;
uint8_t dimStage = 0;        // 0 despierto, 1 atenuado (90s), 2 casi apagado (5min)
// nivel de brillo del jugador (power.h, v1.23): tecla BOOT o la pildora del reloj.
// Se guarda en "bri" unos segundos despues del ultimo cambio (varias pulsaciones seguidas
// = una sola escritura en la NVS)
uint8_t gBright = BRIGHT_DEFAULT;
uint32_t briSaveAt = 0;      // millis en que se guarda el nivel (0 = nada pendiente)
uint32_t briToastUntil = 0;  // aviso del nivel en pantalla
#define BRI_SAVE_DELAY_MS 3000
#define BRI_TOAST_MS 1500
// tiempo de dibujo del paisaje (comando serie PERF): el ultimo y el peor desde el ultimo PERF
uint32_t gSceneUs = 0, gSceneMaxUs = 0;
// BIOME <n> (serie): muestra otro paisaje sin tocar el bicho ni el guardado; -1 = el suyo
int8_t gBiomeOverride = -1;
// contra el quemado (burnin.h, v1.28): paso de la orbita en uso (ORBIT <n> lo fija por serie),
// y si esta boca abajo (entonces no hay salvapantallas: casi apagada, como siempre)
uint8_t gOrbitStep = 0;
int8_t gOrbitForce = -1;
bool gFaceDown = false;
// efectos de respuesta (fx.h, v1.30). Las celebraciones (subida de nivel, medalla, hito, meta)
// salen de una en una cuando la pantalla esta libre; los efectos pequenos ("+N", botones, "!",
// eclosion) esperan a verse. Los vigilantes ven subir nivel, vinculo, descuidos y entrenamiento
// sin tocar cada sitio que los cambia.
FxQueue fxq;
FxOverlays fxo;
FxWatch fxLevelW, fxBondW, fxMistakeW, fxTrainW[3];
FxGate fxBondGate;
FxTween fxBar[4];                 // barras de necesidades que se deslizan
bool fxWasEgg = false;            // para ver la eclosion
const FxEvent *fxBan = nullptr;   // celebracion en pantalla este frame (la fija fxFrame)
int16_t fxGain[3] = { 0, 0, 0 };  // ATK/DEF/VEL que gano la subida que se ve
uint32_t gFxUs = 0, gFxMaxUs = 0; // PERF: lo que tardan los efectos en dibujarse
#define BIOME_COUNT 9  // paisajes (dex.h: biome)
bool swallowGesture = false; // el toque que despierta no acciona nada
uint32_t holdStart = 0;     // pulsacion larga sobre el bicho
uint32_t confirmUntil = 0;  // dialogo "soltar?" activo hasta este millis
uint8_t choiceKind = 0;     // dialogo de decision: 0 ninguno, 1 evolucion, 2 despedida
uint32_t choiceUntil = 0;   // se cierra solo a este millis
int16_t tX0, tY0, tXl, tYl; // gesto en curso (inicio y ultima posicion)
uint32_t tStart = 0;
bool holdFired = false;

void setup() {
  Serial.setRxBufferSize(8192);  // la transferencia a SD llega en bloques de 2 KB
  Serial.begin(115200);
  // CRITICO: sin esto, Serial.print BLOQUEA el juego cuando no hay un
  // monitor serie abierto en el host (el bufer TX del USB CDC se llena
  // y nadie lo vacia) -> con timeout 0 los mensajes se descartan
  Serial.setTxTimeoutMs(0);
  Serial.printf("TamaPoke fw v%s\n", FW_VERSION);
  loadLang();  // idioma guardado (ES por defecto)
  Wire.begin(BOARD_I2C_SDA, BOARD_I2C_SCL);
  // CST9220 (tactil), AXP2101 (PMU), PCF85063 (RTC) y ES8311 comparten este bus I2C.
  // Red de seguridad para PMU/RTC (SensorLib NO respeta este timeout en el
  // tactil; el cuelgue del tactil dormido se resuelve gateando por INT, ver
  // handleTouch).
  Wire.setTimeOut(50);
  // 400 kHz (board_amoled_216.h): un segundo de muestras del IMU se lee en ~10 ms
  // en vez de 36. SensorLib llama a Wire.begin() sin tocar el reloj: se queda asi.
  Wire.setClock(BOARD_I2C_HZ);

  // En la AMOLED-2.16 no se toca ningun rail del AXP2101: la demo de Waveshare
  // deja todos en su valor de arranque y el panel funciona asi. (En la 1.75 el
  // panel colgaba de BLDO1 y habia que encenderlo aqui.)

  // QSPI a BOARD_LCD_QSPI_HZ (40MHz, como la demo de Waveshare de la 2.16). En
  // la 1.75 iba a 80MHz: el flush del framebuffer es el cuello de botella del
  // fps (~56ms a 40MHz, ~46ms a 80MHz). Subirlo a 80M en board_amoled_216.h
  // cuando se compruebe en la placa que el panel no muestra basura.
  if (!gfx->begin(BOARD_LCD_QSPI_HZ)) Serial.println("gfx->begin() fallo");
  roundDisplayBegin(bus, panel, roundCanvas);  // orientacion, borde negro y marco redondo
  {
    Preferences p;
    p.begin("tamapoke", true);
    gBright = p.getUChar("bri", BRIGHT_DEFAULT);
    p.end();
    if (gBright >= BRIGHT_LEVELS) gBright = BRIGHT_DEFAULT;
  }
  panel->setBrightness(brightLevelValue(gBright));
  roundCanvas->overlay = drawBrightToast;
  applyLangFont();  // fuente del idioma guardado (clasica salvo CJK)

  touch.setPins(BOARD_TP_RST, BOARD_TP_INT);
  bool touchOk = false;
  for (int i = 0; i < 3 && !touchOk; i++) {  // a veces falla al primer intento
    touchOk = touch.begin(Wire, BOARD_I2C_ADDR_TOUCH, BOARD_I2C_SDA, BOARD_I2C_SCL);
    if (!touchOk) delay(150);
  }
  if (!touchOk) Serial.println("CST9220 no detectado");
  // begin() deja el chip en modo comando (lee la identidad y no sale);
  // hace falta un reset por hardware para que vuelva a reportar toques
  touch.reset();
  // coordenadas del panel (480x480) en la orientacion de MADCTL 0xA0, como en
  // la demo de Waveshare; handleTouch() las pasa al espacio del juego
  touch.setMaxCoordinates(BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT);
  touch.setSwapXY(BOARD_TP_SWAP_XY);
  touch.setMirrorXY(BOARD_TP_MIRROR_X, BOARD_TP_MIRROR_Y);
  // INT activo-bajo: salta cuando hay datos. Gatea las lecturas I2C (ver loop)
  pinMode(BOARD_TP_INT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BOARD_TP_INT), touchIsr, FALLING);
  // tecla USER = atras (ver onBack). Pull-up externo de 10K: baja al pulsar
  pinMode(BOARD_BTN_USER, INPUT);
  // tecla BOOT = brillo (ver onBootKey). Igual: pull-up externo, baja al pulsar. Es un pin
  // de arranque, pero solo cuenta al encender: despues es una entrada normal
  pinMode(BOARD_BTN_BOOT, INPUT);

  pet.begin();
  sdBegin();
  thumbs.load();

  // reloj real: aplica el tiempo que estuvo apagado
  rtcBegin();
  batBegin();
  pwrSetup();
  imuBegin();  // sin IMU, la pagina de paseo lo dice y nada mas cambia
  uint32_t e = rtcEpoch();
  if (e == 0) {
    // Sin pila de respaldo, el PCF85063 pierde la hora al cortar la
    // alimentacion. Sembrar siempre la fecha fija haria RETROCEDER el tiempo de
    // juego respecto a lo ya guardado, y eso rompe la racha, la edad y la hora
    // de la escena. Se siembra con la ultima hora vista, si es posterior, para
    // que el tiempo no vaya nunca hacia atras. Los minutos apagado se pierden
    // igual (sin RTC no hay forma de saberlos), pero nada se descuadra.
    uint32_t seed = 1767225600UL;             // RTC virgen de verdad
    uint32_t seen = pet.savedEpoch();         // usa la NVS que pet ya tiene abierta
    if (seen > seed) seed = seen;
    rtcSetEpoch(seed);
    e = rtcEpoch();
    Serial.printf("RTC sin hora: sembrado en %u%s\n", seed,
                  seen > 1767225600UL ? " (desde la ultima hora guardada)" : "");
  }
  // los vigilantes de efectos se arman ANTES de aplicar el tiempo apagado: asi los niveles que
  // subio sin verse se anuncian (una sola vez, "Nv.12 > Nv.18") en la primera vuelta del loop
  fxArmWatches();
  pet.syncClock(e);

  audioBegin();  // ES8311 + I2S + amplificador (suena un jingle de arranque)

  lastInteract = millis();
}

// carga/descarga el sprite de SD cuando cambia la especie
void ensureMon() {
  if (pet.speciesId == monFor && monShinyFor == pet.shiny && !sdDirty) return;
  // los ficheros recien recibidos pueden incluir un thumbs.bin nuevo, que se
  // cargaba solo en setup(): sin esto no se veia en la galeria hasta reiniciar
  if (sdDirty) { thumbs.unload(); thumbs.load(); sdEpoch++; }
  sdDirty = false;
  monFor = pet.speciesId;
  monShinyFor = pet.shiny;
  mon.unload();
  pmd.unload();
  beh.x = beh.targetX = 233;
  beh.mode = 0;
  beh.until = 0;
  if (pet.speciesId >= 1 && pet.speciesId <= DEX_COUNT) {
    pmd.load(pet.speciesId, pet.shiny);          // principal: PMD
    if (!pmd.loaded) mon.load(pet.speciesId, pet.shiny);  // respaldo: B/N
  }
}

void loop() {
  uint32_t now = millis();
  pet.update(now);

  // el amplificador sigue al estado de sueno (la llamada sale sola si no cambia).
  // Aqui cubre todas las vias: el boton de luz, el sueno nocturno y el que llega
  // aplicado desde la progresion offline.
  audioSetSleeping(pet.sleeping);
  fxCollect(now);  // subidas de nivel, medallas, metas... (fx.h)

  // avisa con un sonido cuando el bicho pasa a estar listo para evolucionar
  // (incluye el caso de cumplir al despertar). canEvolveNow es false durmiendo.
  static bool wasEvoReady = false;
  bool evoReady = pet.wantEvolveButton();
  if (evoReady && !wasEvoReady) sfxPlay(SFX_MEDAL);
  wasEvoReady = evoReady;
  // aviso sombrio cuando el bicho esta a punto de escaparse por abandono
  static bool wasRunReady = false;
  bool runReady = pet.canRunawayNow();
  if (runReady && !wasRunReady) sfxPlay(SFX_DENY);
  wasRunReady = runReady;

  handleTouch();
  static KeyDebounce backKey;  // tecla USER = atras
  if (backKey.update(digitalRead(BOARD_BTN_USER) == BOARD_BTN_PRESSED_LEVEL, millis())) onBack();
  static KeyDebounce bootKey;  // tecla BOOT = brillo
  if (bootKey.update(digitalRead(BOARD_BTN_BOOT) == BOARD_BTN_PRESSED_LEVEL, millis())) onBootKey();
  if (briSaveAt && !timeLeft(briSaveAt)) saveBright();
  handleSerial();
  ensureMon();
  imuTick(now);  // pasos, sacudidas, boca abajo

  // pulsacion corta del PWR: juego -> salvapantallas -> apagada -> juego (v1.32)
  static uint32_t lastPwr = 0;
  if (now - lastPwr > 250) {
    lastPwr = now;
    if (pwrShortPressed()) pwrPress();
  }

  updateBrightness(now);
  boxTick(millis());  // caja (v1.37): cuando la luz llega a su hueco, se escribe en flash

  // bateria al limite seguro (sin USB): se guarda la partida y se apaga el PMU antes de que la
  // celda baje mas; el RTC conserva la hora
  if (batShouldShutdown() && !usbPresent()) {
    pet.flushSave();
    panel->setBrightness(0);
    batPowerOff();
  }

  // vuelca el autoguardado periodico SOLO con la pantalla atenuada/apagada o
  // durmiendo: la escritura a NVS congela ~1s ambos cores (caché de flash off),
  // y aqui no hay animacion que se corte ni dedo esperando respuesta. Con 90s
  // de inactividad la pantalla ya atenua, asi que se vuelca enseguida; el uso
  // activo persiste igual por los guardados de cada accion (comer/jugar/...).
  if (pet.savePending() && (screenOff || dimStage >= 1 || pet.sleeping)) {
    pet.flushSave();
  }

  // anota la hora real cada 30 s (se persiste en cada save del juego)
  static uint32_t lastClock = 0;
  if (now - lastClock > 30000) {
    lastClock = now;
    uint32_t e = rtcEpoch();
    if (e) pet.lastSeenEpoch = e;
  }

  // latido de salud cada 5 min (para el soak test; se descarta si no hay monitor)
  static uint32_t lastHealth = 0;
  if (now - lastHealth > 300000) {
    lastHealth = now;
    Serial.printf("HEALTH up=%lus heap=%u min=%u bat=%d%% mv=%d chg=%d usb=%d dim=%u off=%d bri=%u orbit=%u saver=%d\n",
                  (unsigned long)(now / 1000), ESP.getFreeHeap(), ESP.getMinFreeHeap(),
                  batPercent(), batMillivolts(), batCharging() ? 1 : 0,
                  usbPresent() ? 1 : 0, dimStage, screenOff ? 1 : 0, gBright, gOrbitStep,
                  saverOn() ? 1 : 0);
  }

  // Ritmo de dibujo (power.h): 10 fps con el bicho en uso, menos cuanto mas rato sin tocar,
  // 85 ms en juego/saco (margen seguro para que el redibujado no pise el envio DMA del frame
  // anterior: a 40-65 ms solapaba y causaba flashes negros).
  // Con la pantalla apagada no se dibuja: a brillo 0 no se ve nada, pero el redibujado y el
  // flush DMA del framebuffer del lienzo seguian corriendo a 10 fps, y medido son 70 ms de
  // trabajo por cada 100 (46 de ellos solo el volcado). Cada render repinta la escena
  // entera, asi que no queda nada a medias; y al quedarse lastRender congelado, el primer
  // frame tras despertar sale en el acto.
  PowerCtx pc = powerCtx(millis());
  // orbita contra el quemado: la imagen cambia de sitio cada ORBIT_STEP_MS (en el proximo flush)
  uint32_t orbit = gOrbitForce >= 0 ? (uint32_t)gOrbitForce : orbitStepAt(now);
  gOrbitStep = orbit % ORBIT_POSITIONS;
  OrbitShift osh = orbitShift(gOrbitStep);
  roundCanvas->setShift(osh.dx, osh.dy);
  audioSetIdle(!amplifierWanted(pc));  // el amplificador consume en reposo
  batSetPollMs(powerCacheMs(pc));
  uint32_t interval = frameIntervalMs(pc);
  bool drewFrame = false;
  if (interval && now - lastRender >= interval) {
    lastRender = now;
    drewFrame = render();
  }
  // cede la CPU en los pases que no dibujan: sin esto el loop gira a pleno rendimiento
  // entre frames; 1 ms no se nota en el tactil (se lee cada 20 ms) y el frame no se retrasa
  if (!drewFrame) idleWait(pc);
}

// ---------- IMU: pasos y trucos de movimiento (docs/FEATURES.md, Tareas 1 y 2) ----------

void onShake();
void onFaceDown(bool down);

// lee el FIFO (cada 100 ms encendida, cada segundo a oscuras), pasa los pasos al
// bicho cada 30 s y atiende los eventos. Nada de esto escribe en flash al momento.
void imuTick(uint32_t now) {
  if (!imuPresent()) return;
  imuPoll(now, !screenOff && dimStage < DIM_OFF);
  static uint32_t lastFlush = 0;
  if (now - lastFlush >= STEP_FLUSH_MS) {
    lastFlush = now;
    pet.addSteps(imuTakeSteps(), pet.dayNumber());  // con 0 pasos tambien pasa el dia
  }
  if (imuTakeShakes()) onShake();
  int8_t fd = imuTakeFaceDown();
  if (fd) onFaceDown(fd > 0);
}

// Sacudida (FR-2.1). A oscuras solo enciende la pantalla, como un toque: asi un
// trote con la placa en el bolsillo nunca despierta al bicho. Con la pantalla
// encendida, el dormido se despierta de mal humor y el despierto se marea.
void onShake() {
  if (screenOff) return;  // apagada con PWR: se respeta
  if (dimStage > 0) { wakeUI(); return; }
  wakeUI();
  if (pet.awaitingStarter() || pet.ceremony || pet.evolving() || gameOpen || sackOpen || battleOpen()) return;
  if (galleryOpen || boxOpen || cardOpen || kbOpen || clockOpen) return;  // el bicho no esta en pantalla
  if (pet.sleeping) pet.wakeGrumpy();
  else pet.shaken();  // tambien el huevo: se tambalea
}

// Boca abajo y quieta 3 s (FR-2.2): de noche el bicho se va a dormir; de dia solo
// se apaga la pantalla. Al levantarla se enciende (y quien se durmio sigue dormido).
void onFaceDown(bool down) {
  if (screenOff) return;
  if (!down) { wakeUI(); return; }
  if (pet.awaitingStarter() || pet.ceremony || pet.evolving() || gameOpen || sackOpen || battleOpen()) return;
  int h = sceneHour();
  if ((h >= 20 || h < 6) && !pet.isEgg() && !pet.sleeping) pet.sleepDeferred();
  lastInteract = millis() - POWER_DIM2_MS - 1;  // casi apagada, como tras 5 min sin tocar
  gFaceDown = true;  // y sin salvapantallas: nadie la mira
}

// estado de consumo actual (ver power.h)
PowerCtx powerCtx(uint32_t now) {
  PowerCtx c;
  c.idleMs = idleSince(now, lastInteract);
  c.screenOff = screenOff;
  c.sleeping = pet.sleeping;
  c.gameOpen = gameOpen || sackOpen || battleOpen() || boxMove;  // combate y luz de la caja, tambien a 85 ms
  c.usb = usbPresent();
  c.charging = batCharging();
  c.batPct = batPercent();
  c.brightLevel = gBright;
  c.faceDown = gFaceDown;
  return c;
}

// el usuario ha interactuado: vuelve el ritmo normal de dibujo (sin esperar al proximo
// frame de un ritmo lento) y el amplificador se enciende antes de que suene nada
void wakeUI() {
  uint32_t now = millis();
  uint32_t iv = frameIntervalMs(powerCtx(now));
  bool slow = !iv || iv > 100;  // pantalla apagada o ritmo lento: se dibuja ya, sin esperar al frame
  lastInteract = now;
  gFaceDown = false;
  saverPinned = false;
  // el frame forzado respeta los 85 ms entre redibujados (si no, solapa el DMA del anterior)
  if (slow) lastRender = (now - lastRender >= 85) ? 0 : lastRender - 15;
  audioSetIdle(false);
}

// Pulsacion corta del PWR (v1.32, power.h): pasa a la siguiente vista y la devuelve.
//  - salvapantallas: el mismo truco que el comando SAVER (como tras 5 min sin tocar), asi que
//    su brillo, su ritmo, el light sleep y el amplificador apagado salen solos; y fijado, para
//    que comer o el corazon no lo quiten al instante (updateBrightness).
//  - apagada: como siempre.
//  - el juego: como siempre, como un toque.
// El combate solo avanza al dibujarse: en el salvapantallas o apagada no se dibuja, y al volver
// soltaria de golpe los ataques atrasados. Pasar a cualquiera de las dos lo abandona.
PwrView pwrPress() {
  uint32_t now = millis();
  PwrView next = pwrNext(pwrViewOf(powerCtx(now)));
  if (next == VIEW_ON) {
    screenOff = false;
    wakeUI();
    return next;
  }
  if (battleOpen()) closeBattle();
  if (next == VIEW_SAVER) {
    lastInteract = now - POWER_SAVER_IDLE_MS;
    gFaceDown = false;
    saverPinned = true;
    // el salvapantallas sale ya, no al siguiente frame de su ritmo lento (1 s); con 85 ms
    // desde el ultimo volcado como minimo, para no pisar su DMA
    uint32_t iv = frameIntervalMs(powerCtx(now));
    uint32_t since = now - lastRender;
    lastRender = now + (since >= 85 ? 0 : 85 - since) - iv;
  } else {
    screenOff = true;
    saverPinned = false;
  }
  return next;
}

static const char *const PWR_VIEW_NAME[] = { "on", "saver", "off" };

// brillo segun sueno + inactividad (proteccion del AMOLED) y bateria
void updateBrightness(uint32_t now) {
  // los eventos visibles despiertan la pantalla solos (no el salvapantallas puesto con PWR)
  if (!screenOff && !saverPinned && (pet.evolving() || pet.ceremony || pet.eating() || pet.showHeart())) {
    if (dimStageFor(idleSince(now, lastInteract)) != DIM_AWAKE) wakeUI();
    else lastInteract = now;
  }
  PowerCtx c = powerCtx(now);
  dimStage = dimStageFor(c.idleMs);
  uint8_t target = brightnessFor(c);
  static uint8_t current = 255;
  if (target != current) {
    current = target;
    panel->setBrightness(target);
  }
}

// Pausa de un pase del bucle que no ha dibujado. Con la pantalla apagada, o encendida pero casi
// apagada (5 min sin tocar), y sin USB se duerme la CPU (light sleep) hasta el proximo frame o
// 200 ms: despierta por el temporizador o por un toque (el INT del tactil baja). Con USB no, para
// no perder el puerto serie de la tarjeta SD: se mira sin cache, por si se acaba de enchufar.
void idleWait(PowerCtx c) {
  // no se duerme justo tras dibujar (el volcado DMA del frame sigue en marcha) ni con un aviso sonando
  if (lightSleepAllowed(c) && millis() - lastRender >= 100 && !audioBusy()) {
    c.usb = usbPresentNow();
    if (!lightSleepAllowed(c)) { delay(idleDelayMs(c)); return; }
    if (digitalRead(BOARD_TP_INT) == LOW) { gTouchIrq = true; delay(1); return; }  // toque pendiente: no se duerme
    if (digitalRead(BOARD_BTN_USER) == BOARD_BTN_PRESSED_LEVEL) { delay(1); return; }  // tecla USER pulsada
    if (digitalRead(BOARD_BTN_BOOT) == BOARD_BTN_PRESSED_LEVEL) { delay(1); return; }  // tecla BOOT pulsada
    uint32_t ms = POWER_SLEEP_MS;
    uint32_t iv = frameIntervalMs(c);
    if (iv) {  // pantalla encendida: hasta el proximo frame
      uint32_t due = lastRender + iv - millis();
      if ((int32_t)due < (int32_t)ms) ms = (int32_t)due > 1 ? due : 1;
    }
    esp_sleep_enable_timer_wakeup((uint64_t)ms * 1000);
    gpio_wakeup_enable((gpio_num_t)BOARD_TP_INT, GPIO_INTR_LOW_LEVEL);
    // la tecla USER tambien despierta: una pulsacion corta cabe entera en un sueno de 200 ms
    gpio_wakeup_enable((gpio_num_t)BOARD_BTN_USER, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable((gpio_num_t)BOARD_BTN_BOOT, GPIO_INTR_LOW_LEVEL);  // y BOOT
    esp_sleep_enable_gpio_wakeup();
    esp_light_sleep_start();
    gpio_wakeup_disable((gpio_num_t)BOARD_BTN_USER);
    gpio_set_intr_type((gpio_num_t)BOARD_BTN_USER, GPIO_INTR_DISABLE);  // sin interrupcion: se lee en el loop
    gpio_wakeup_disable((gpio_num_t)BOARD_BTN_BOOT);
    gpio_set_intr_type((gpio_num_t)BOARD_BTN_BOOT, GPIO_INTR_DISABLE);
    // la interrupcion del tactil vuelve a ser por flanco (como la dejo attachInterrupt); un toque
    // que ya haya bajado la linea se registra a mano
    gpio_wakeup_disable((gpio_num_t)BOARD_TP_INT);
    gpio_set_intr_type((gpio_num_t)BOARD_TP_INT, GPIO_INTR_NEGEDGE);
    if (digitalRead(BOARD_TP_INT) == LOW) gTouchIrq = true;
    return;
  }
  delay(idleDelayMs(c));
}

// ---------- consola serie (provision de SD + depuracion) ----------

void handleSerial() {
  if (!Serial.available()) return;
  if (imuLogging()) {  // IMULOG en marcha: cualquier tecla lo para
    imuLog(0);
    while (Serial.available()) Serial.read();
    Serial.println("DONE");
    return;
  }
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;
  if (sdSerialCommand(line)) return;

  if (line == "HATCH") {
    pet.eggTap(); pet.eggTap(); pet.eggTap();
    Serial.println("DONE");
  } else if (line.startsWith("SPEC ")) {
    int n = line.substring(5).toInt();
    if (n >= 1 && n <= DEX_COUNT) {
      pet.prevSpeciesId = pet.speciesId;
      pet.speciesId = n;
      Serial.printf("especie #%d %s\n", n, DEX_TBL[n].name);
    }
    Serial.println("DONE");
  } else if (line.startsWith("LVL ")) {  // al principio de ese nivel (crecimiento, no edad: v1.35)
    int n = line.substring(4).toInt();
    if (n >= 1 && n <= LEVEL_MAX) pet.setLevel((uint16_t)n);
    Serial.printf("lv=%u grow=%lu\n", pet.level(), (unsigned long)pet.growMin);
    Serial.println("DONE");
  } else if (line.startsWith("TIME ")) {
    uint32_t e = (uint32_t)line.substring(5).toInt();
    rtcSetEpoch(e);
    pet.setClock(e);
    Serial.printf("rtc=%u\n", rtcEpoch());
    Serial.println("DONE");
  } else if (line.startsWith("RTCSET ")) {  // solo RTC (simular apagados en pruebas)
    rtcSetEpoch((uint32_t)line.substring(7).toInt());
    Serial.printf("rtc=%u\n", rtcEpoch());
    Serial.println("DONE");
  } else if (line == "TIME") {
    Serial.printf("rtc=%u\n", rtcEpoch());
    Serial.println("DONE");
  } else if (line == "GAL") {
    galleryOpen = !galleryOpen;
    galleryDetail = 0;
    galleryDirty = true;
    if (!galleryOpen) galleryPmd.unload();
    Serial.println("DONE");
  } else if (line == "EGGS") {
    // simula 20 tiradas de huevo (no cambia el estado del juego)
    for (int i = 0; i < 20; i++) {
      int16_t d = pet.pickEggSpecies();
      Serial.printf("%d:%s(r%u) ", d, DEX_TBL[d].name, DEX_TBL[d].rarity);
    }
    Serial.println();
    Serial.println("DONE");
  } else if (line == "SHINY") {  // alterna shiny del actual (pruebas)
    pet.shiny = !pet.shiny;
    Serial.printf("shiny=%d\n", pet.shiny);
    Serial.println("DONE");
  } else if (line.startsWith("NICK ")) {
    pet.rename(line.substring(5).c_str());
    Serial.printf("nick=%s\n", pet.nick);
    Serial.println("DONE");
  } else if (line == "CAREDAY") {  // simula un dia nuevo cuidado (pruebas)
    pet.setClock(pet.lastSeenEpoch + 86400);
    pet.caress();
    Serial.printf("streak=%u bond=%u medals=0x%X\n", pet.streak, pet.bond, pet.medals);
    Serial.println("DONE");
  } else if (line == "BYE") {
    pet.startFarewell();
    Serial.println("DONE");
  } else if (line == "RUN") {
    pet.startRunaway();
    Serial.println("DONE");
  } else if (line == "BEEP") {
    sfxPlay(SFX_HATCH);  // prueba de audio
    Serial.println("DONE");
  } else if (line == "BACK") {
    onBack();               // como pulsar la tecla USER
    Serial.println("DONE");
  } else if (line == "ABANDON") {
    pet.dbgRunawayReady();  // fuerza el estado "lista para escaparse" (test del boton)
    Serial.println("DONE");
  } else if (line == "WIPE") {
    pet.factoryReset();     // borra NVS y reinicia -> partida nueva (eleccion de inicial)
    Serial.println("DONE");
    delay(100);
    ESP.restart();
  } else if (line == "REG") {
    Serial.printf("pokedex %u/151:", pet.registeredCount());
    for (int i = 1; i <= 151; i++)
      if (pet.isRegistered(i)) Serial.printf(" %d", i);
    Serial.println();
    Serial.println("DONE");
  } else if (line.startsWith("BIOME")) {  // BIOME <n>: ver otro paisaje (solo RAM); BIOME: el del bicho
    gBiomeOverride = line.length() > 6 ? (int8_t)line.substring(6).toInt() : -1;
    if (gBiomeOverride >= BIOME_COUNT) gBiomeOverride = -1;
    wakeUI();
    Serial.printf("biome=%u%s\n", homeBiome(), gBiomeOverride >= 0 ? " (override)" : "");
    Serial.println("DONE");
  } else if (line.startsWith("ORBIT")) {  // ORBIT <n>: fija la posicion de la orbita; ORBIT: la del reloj
    gOrbitForce = line.length() > 6 ? (int8_t)(line.substring(6).toInt() % ORBIT_POSITIONS) : -1;
    wakeUI();
    Serial.printf("orbit=%u%s origin=%d,%d\n", gOrbitStep, gOrbitForce >= 0 ? " (forced)" : "",
                  roundCanvas->originX(), roundCanvas->originY());
    Serial.println("DONE");
  } else if (line == "PWR") {  // como una pulsacion corta del PWR (v1.32): juego -> salvapantallas -> apagada
    PwrView v = pwrPress();
    updateBrightness(millis());
    Serial.printf("pwr view=%s off=%d saver=%d pinned=%d bri=%u\n", PWR_VIEW_NAME[v], screenOff ? 1 : 0,
                  saverOn() ? 1 : 0, saverPinned ? 1 : 0, brightnessFor(powerCtx(millis())));
    Serial.println("DONE");
  } else if (line == "SAVER") {  // como tras 5 min sin tocar: salvapantallas
    lastInteract = millis() - POWER_DIM2_MS - 1;
    gFaceDown = false;
    Serial.println("DONE");
  } else if (line == "PERF") {  // cuanto tarda el paisaje (drawScene) en dibujarse, en us
    Serial.printf("scene=%luus max=%luus biome=%u fx=%luus fxmax=%luus\n", (unsigned long)gSceneUs,
                  (unsigned long)gSceneMaxUs, homeBiome(), (unsigned long)gFxUs, (unsigned long)gFxMaxUs);
    gSceneMaxUs = 0;
    gFxMaxUs = 0;
    Serial.println("DONE");
  } else if (line.startsWith("FX")) {  // FX <efecto>: lanza un efecto de respuesta para verlo en la placa
    String k = line.length() > 3 ? line.substring(3) : String("");
    k.trim();
    if (!fxDebug(k)) Serial.println("FX levelup|levelup6|medal|streak|goal|goal3|stat|bond|train|deny|press|oops|hatch|box");
    Serial.println("DONE");
  } else if (line.startsWith("BRIGHT")) {  // BRIGHT: nivel y brillo; BRIGHT <n>: lo fija (como BOOT)
    if (line.length() > 7) {
      int lv = line.substring(7).toInt();
      if (lv >= 0 && lv < BRIGHT_LEVELS) {
        gBright = (uint8_t)lv;
        briSaveAt = millis() + BRI_SAVE_DELAY_MS;
        briToastUntil = millis() + BRI_TOAST_MS;
        wakeUI();
      }
    }
    Serial.printf("bri=%u/%u value=%u now=%u\n", gBright, BRIGHT_LEVELS - 1, brightLevelValue(gBright),
                  brightnessFor(powerCtx(millis())));
    Serial.println("DONE");
  } else if (line == "HEALTH") {
    Serial.printf("up=%lus heap=%u min=%u sd=%d mon=%d\n",
                  (unsigned long)(millis() / 1000), ESP.getFreeHeap(),
                  ESP.getMinFreeHeap(), sdReady, pmd.loaded || mon.loaded);
    Serial.println("DONE");
  } else if (line.startsWith("STEPS ")) {  // pasos inyectados por el mismo camino que el IMU
    long n = line.substring(6).toInt();
    if (n > 0) pet.addSteps((uint32_t)n, pet.dayNumber());
    Serial.printf("steps today=%lu total=%lu finds=%u bonus=%u goal=%u\n", (unsigned long)pet.stepsToday,
                  (unsigned long)pet.stepsTotal, pet.findsToday, pet.bonusFindsToday,
                  pet.goalTierOn(pet.dayNumber()));
    Serial.println("DONE");
  } else if (line == "IMU") {
    imuReport();
    Serial.println("DONE");
  } else if (line.startsWith("IMULOG")) {  // IMULOG <s>: muestras en CSV (cualquier tecla para)
    long secs = line.length() > 6 ? line.substring(7).toInt() : 10;
    imuLog((uint32_t)(secs > 0 ? secs : 10) * 1000UL);
  } else if (line == "BATTLE" || line.startsWith("BATTLE ")) {  // combate de practica [contra <dex>]
    startPractice(line.length() > 7 ? (int16_t)line.substring(7).toInt() : 0);
    if (!battleOpen()) Serial.println("battle: no puede (huevo, dormido, evolucionando o ya en combate)");
    Serial.println("DONE");
  } else if (line == "BALL" || line.startsWith("BALL ")) {  // BALL <puntos>: el minijuego con esa puntuacion
    long n = line.length() > 5 ? line.substring(5).toInt() : 0;  // para notar 1x, 1,5x, 2x sin llegar jugando
    if (n < 0) n = 0;
    if (n > 255) n = 255;
    wakeUI();
    menuKind = MENU_NONE;
    startGame();
    if (!gameOpen) {
      Serial.println("ball: no puede (huevo, dormido o despedida)");
    } else {
      gameScore = (uint8_t)n;
      gameNoSave = true;  // ni record ni premio: el guardado del bicho no cambia
      Serial.printf("ball: score=%ld speed=%.2f\n", n, ballSpeed((uint16_t)n));
    }
    Serial.println("DONE");
  } else if (line == "EGG" || line.startsWith("EGG ")) {  // huevos (v1.39); EGG <n>: n en la mochila (pruebas)
    if (line.length() > 4) {
      long n = line.substring(4).toInt();
      pet.setBagEggs((uint8_t)(n < 0 ? 0 : (n > ITEM_MAX ? ITEM_MAX : n)));
      monFor = -2;  // con la casa vacia, el huevo que llega se dibuja ya
      ensureMon();
      fxArmWatches();
    }
    eggReport();
    Serial.println("DONE");
  } else if (line.startsWith("TAP ")) {  // TAP <x> <y>: un toque como el de un dedo (pruebas por serie)
    String a = line.substring(4);
    a.trim();
    int sp = a.indexOf(' ');
    int16_t x = (int16_t)a.substring(0, sp).toInt(), y = sp > 0 ? (int16_t)a.substring(sp + 1).toInt() : 0;
    wakeUI();
    if (battleOpen()) battleTap(x, y);  // como handleTouch: el combate se lleva los toques
    else onTap(x, y);
    Serial.printf("tap %d %d battle=%d phase=%u\n", x, y, battleOpen() ? 1 : 0, battlePhase());
    Serial.println("DONE");
  } else if (line == "BOX") {  // la caja (v1.37): cada hueco con uid, especie, nivel y CRC
    boxReport();
    Serial.println("DONE");
  } else if (line.startsWith("STORE ") || line.startsWith("WAKE ")) {  // STORE <k> / WAKE <k>, k = 0-5
    bool toStore = line.startsWith("STORE ");
    String a = line.substring(toStore ? 6 : 5);
    a.trim();
    const char *c = a.c_str();
    int k = (a.length() == 1 && c[0] >= '0' && c[0] <= '9') ? c[0] - '0' : -1;
    bool ok = k >= 0 && k < BOX_SLOTS && (toStore ? pet.store((uint8_t)k) : pet.wake((uint8_t)k));
    if (ok) boxAfterMove();  // como al tocar SI, sin la luz
    else Serial.printf("%s: no se puede (hueco 0-%u %s; sin despedida, evolucion ni eleccion de inicial)\n",
                       toStore ? "store" : "wake", BOX_SLOTS - 1, toStore ? "vacio y un bicho nacido" : "con un bicho");
    boxReport();
    Serial.println("DONE");
  } else if (line == "BOXFILL") {  // bichos inventados en los huecos vacios (pruebas, capturas)
    boxFill();
    boxReport();
    Serial.println("DONE");
  } else if (line.startsWith("BOXCLR ")) {  // BOXCLR <k>: vacia un hueco (tambien uno danado)
    String a = line.substring(7);
    a.trim();
    const char *c = a.c_str();
    int k = (a.length() == 1 && c[0] >= '0' && c[0] <= '9') ? c[0] - '0' : -1;
    if (k < 0 || !pet.boxClear((uint8_t)k)) Serial.printf("boxclr: hueco 0-%u\n", BOX_SLOTS - 1);
    boxReport();
    Serial.println("DONE");
  } else if (line == "NVS") {  // entradas de la NVS (20 KB, ~500 de 32 bytes): cuanto ocupa la partida
    nvs_stats_t st;
    if (nvs_get_stats(NULL, &st) == ESP_OK) {
      size_t mine = 0;
      nvs_handle_t h;
      if (nvs_open("tamapoke", NVS_READONLY, &h) == ESP_OK) {
        nvs_get_used_entry_count(h, &mine);
        nvs_close(h);
      }
      Serial.printf("nvs used=%u free=%u available=%u total=%u namespaces=%u tamapoke=%u\n", (unsigned)st.used_entries,
                    (unsigned)st.free_entries, (unsigned)st.available_entries, (unsigned)st.total_entries,
                    (unsigned)st.namespace_count, (unsigned)mine);
    } else {
      Serial.println("nvs: sin estadisticas");
    }
    Serial.println("DONE");
  } else if (line == "SHAKE") {            // como una sacudida de verdad
    onShake();
    Serial.println("DONE");
  } else if (line.startsWith("FACEDOWN")) {  // FACEDOWN: boca abajo; FACEDOWN 0: la levantan
    onFaceDown(!(line.length() > 9 && line.substring(9).toInt() == 0));
    Serial.println("DONE");
  } else if (line == "STATS") {
    Serial.printf("spec=%d nv=%u com=%u fel=%u ene=%u lim=%u desc=%u sd=%d mon=%d bat=%d usb=%d rtc=%u\n",
                  pet.speciesId, pet.level(), pet.fullness, pet.joy, pet.energy,
                  pet.hygiene, pet.careMistakes, sdReady, mon.loaded,
                  batPercent(), usbPresent(), rtcEpoch());
    Serial.printf("peso=%u fue=%u def=%u vel=%u genes=%u/%u/%u tr=%u/%u/%u baya=%d\n",
                  pet.weight, pet.atkStat(), pet.defStat(), pet.speStat(),
                  pet.geneAtk, pet.geneDef, pet.geneSpe,
                  pet.trAtk, pet.trDef, pet.trSpe, pet.berryKnown);
    Serial.printf("shiny=%d streak=%u/%u bond=%u bondT=%u medals=0x%X(%u) nick=%s\n",
                  pet.shiny, pet.streak, pet.bestStreak, pet.bond, pet.bondGainedToday(), pet.medals,
                  pet.totalMedals, pet.nick);
    // crecimiento (v1.35, growth.h): de el sale el nivel; la edad va aparte
    Serial.printf("grow=%lu lv=%u into=%u/%u age=%lu\n", (unsigned long)pet.growMin, pet.level(),
                  pet.levelMinutesInto(), pet.levelMinutesCost(), (unsigned long)pet.ageMinutes);
    Serial.printf("steps=%lu/%lu finds=%u bonus=%u goal=%u bpack=%u/%u/%u/%u charm=%u candy=%u imu=%d\n",
                  (unsigned long)pet.stepsToday, (unsigned long)pet.stepsTotal, pet.findsToday,
                  pet.bonusFindsToday, pet.goalTierOn(pet.dayNumber()), pet.items[IT_GOLDEN],
                  pet.items[IT_PROTEIN], pet.items[IT_IRON], pet.items[IT_CARBOS], pet.items[IT_CHARM],
                  pet.items[IT_RARECANDY], imuPresent() ? 1 : 0);
    // efectos (v1.30): celebraciones en cola y la que se ve, y por que podria estar esperando
    Serial.printf("fx queue=%u showing=%d kind=%d busy=%d sleep=%d dim=%u off=%d saver=%d\n", fxq.n,
                  fxq.active ? 1 : 0, fxq.active ? fxq.cur.kind : -1, fxScreenBusy() ? 1 : 0, pet.sleeping ? 1 : 0,
                  dimStage, screenOff ? 1 : 0, saverOn() ? 1 : 0);
    // la caja (v1.37): quien es el activo y si hay un huevo esperando
    Serial.printf("box used=%u/%u uid=%08lX eggHeld=%d\n", pet.boxUsed(), BOX_SLOTS, (unsigned long)pet.uid,
                  pet.eggHeld() ? 1 : 0);
    eggReport();  // huevos limitados (v1.39)
    Serial.println("DONE");
  }
}

// ---------- entrada tactil ----------

bool inPetZone(int16_t x, int16_t y) {
  return x > 110 && x < 356 && y > 95 && y < 310;
}

// el toque se resuelve al LEVANTAR el dedo para distinguir tap de deslizar
void handleTouch() {
  static uint32_t lastPoll = 0;
  if (millis() - lastPoll < 20) return;  // 50 Hz le sobra a un dedo
  lastPoll = millis();
  // solo tocamos el bus si el chip aviso por INT o si el dedo sigue abajo (hay
  // que detectar el levantamiento). Leer el CST9217 dormido se colgaba ~1s y
  // congelaba el loop entero; SensorLib no respeta el timeout de Wire.
  // (Medido en la 1.75 con el CST9217; el CST9220 de la 2.16 es de la misma
  // familia y usa el mismo driver.)
  if (!gTouchIrq && !wasPressed) return;
  gTouchIrq = false;
  // Un ciclo de solo direccion antes de leer. Sin esto, getPoint() se colgaba
  // exactamente 1000 ms (el timeout por defecto del driver I2C, que SensorLib no
  // acota) y congelaba el loop entero: es lo que se percibia como "el minijuego
  // se congela 2-3 segundos al tocar la bola" (issue #16), porque ahi los toques
  // son rapidos y seguidos.
  //
  // Medido en placa, jugando lo mismo (~470 lecturas, marcador ~50):
  //   sin esta linea: 5 parones de 1000 ms en 60 s
  //   con ella:       0
  // El contador de rechazos salio 0 en ambos casos, asi que NO funciona
  // saltandose lecturas cuando el chip no contesta: lo que hace es despertarlo,
  // para que la lectura siguiente no se encuentre el CST9217 dormido.
  Wire.beginTransmission(BOARD_I2C_ADDR_TOUCH);
  if (Wire.endTransmission() != 0) return;  // no responde: se reintenta en 20 ms
  // getPoint() copia TODOS los dedos que reporta el chip (hasta 2), pida los que
  // pida: con un int16_t suelto, dos dedos escribian fuera de la variable
  int16_t xs[TOUCH_MAX_POINTS] = { 0 }, ys[TOUCH_MAX_POINTS] = { 0 };
  bool pressed = touch.getPoint(xs, ys, 1) > 0;
  // del panel de 480x480 al lienzo de 466x466 donde vive la UI (round_display.h), que la
  // orbita contra el quemado mueve unos pixeles: se resta donde esta ahora, no UI_ORIGIN
  int16_t x = xs[0] - roundCanvas->originX(), y = ys[0] - roundCanvas->originY();

  // saco de entrenamiento: cada toque cuenta al instante (aporrear rapido)
  if (sackOpen) {
    if (pressed && !wasPressed) {
      wakeUI();
      if (y < 72) sackOpen = false;  // tocar arriba = abandonar
      else sackTap();
    }
    wasPressed = pressed;
    return;
  }

  // combate: cada toque carga el especial al momento (tocar rapido)
  if (battleOpen()) {
    if (pressed && !wasPressed) {
      wakeUI();
      battleTap(x, y);
      if (!battleOpen()) {  // ese toque cerro el resultado: el resto del gesto no acciona nada
        tX0 = tXl = x;
        tY0 = tYl = y;
        tStart = millis();
        holdFired = true;
        swallowGesture = true;
      }
    }
    wasPressed = pressed;
    return;
  }

  if (pressed && !wasPressed) {  // empieza el gesto
    tX0 = tXl = x;
    tY0 = tYl = y;
    tStart = millis();
    holdFired = false;
    swallowGesture = (dimStage > 0) || screenOff;  // si estaba a oscuras, solo despierta
    screenOff = false;
    wakeUI();
  } else if (pressed) {  // sigue apoyado
    tXl = x;
    tYl = y;
    // pulsacion larga sin moverse sobre el bicho -> dialogo de soltar
    if (!holdFired && !swallowGesture && !galleryOpen && !boxOpen && !cardOpen && !kbOpen && !clockOpen && !menuKind && millis() - tStart > 3000 &&
        abs(tXl - tX0) < 30 && abs(tYl - tY0) < 30 && inPetZone(tX0, tY0) &&
        !pet.isEgg() && !confirmUntil && !pet.ceremony) {
      confirmUntil = millis() + 10000;
      holdFired = true;
    }
  } else if (wasPressed) {  // levanta el dedo: resolver gesto
    wakeUI();
    int dx = tXl - tX0, dy = tYl - tY0;
    uint32_t dt = millis() - tStart;
    if (!holdFired && !swallowGesture) {
      if (abs(dx) > 80 && abs(dy) < 70 && dt < 800) onSwipe(dx > 0 ? 1 : -1);
      else if (abs(dy) > 80 && abs(dx) < 70 && dt < 800) onSwipeV(dy > 0 ? 1 : -1);
      else if (dt < 1500 && abs(dx) < 40 && abs(dy) < 40) onTap(tX0, tY0);
    }
  }
  wasPressed = pressed;
}

// deslizar vertical: abre/cierra la ficha del bicho
void openClock();  // prototipo

void onSwipeV(int dir) {
  if (pet.awaitingStarter()) return;  // bloqueado durante la eleccion de inicial
  if (gameOpen || galleryOpen || boxOpen || kbOpen || sackOpen || battleOpen() || pet.ceremony) return;
  if (clockOpen) { clockOpen = false; return; }
  if (cardOpen) {
    if (dir < 0) { cardOpen = false; itemConfirm = 0; }  // arriba cierra la ficha
    return;
  }
  if (dir > 0) {                    // deslizar abajo: ajustar hora
    if (!confirmUntil && !menuKind) openClock();
  } else if (!pet.isEgg() && !confirmUntil && !menuKind) {
    cardOpen = true;                // deslizar arriba: ficha
    cardPage = 0;
  }
}

// deslizar: dir +1 = hacia la derecha
void onSwipe(int dir) {
  if (pet.awaitingStarter()) return;  // bloqueado durante la eleccion de inicial
  if (gameOpen || kbOpen || clockOpen || battleOpen()) return;
  if (cardOpen) {  // dentro de la ficha: cambiar entre las CARD_PAGES paginas
    int p = (int)cardPage + (dir > 0 ? -1 : 1);  // izquierda avanza
    cardPage = p < 0 ? 0 : (p > CARD_PAGES - 1 ? CARD_PAGES - 1 : p);
    itemConfirm = 0;
    return;
  }
  if (boxOpen) {  // la caja es una sola pagina: a la izquierda vuelve a casa, a la derecha nada
    if (dir < 0 && !boxMove) closeBox();
    return;
  }
  if (!galleryOpen) {
    // con un menu abierto no: la galeria se abria encima del de comida
    if (!pet.ceremony && !confirmUntil && !menuKind) {
      if (dir > 0) {  // a la derecha: la caja (v1.37), desde el bicho o desde el huevo
        if (boxCanOpen()) openBox();
      } else {        // a la izquierda: la Pokedex
        galleryOpen = true;
        galleryPage = 0;
        galleryDetail = 0;
        galleryDirty = true;
      }
    }
    return;
  }
  if (galleryDetail) {  // en detalle: volver a la rejilla
    galleryDetail = 0;
    galleryPmd.unload();
    galleryDirty = true;
    return;
  }
  int np = galleryPage - dir;  // deslizar a la izquierda avanza pagina
  if (np < 0) {                // retroceder desde la primera = salir
    galleryOpen = false;
    galleryPmd.unload();
    return;
  }
  if (np > 9) np = 9;
  if (np != galleryPage) {
    galleryPage = np;
    galleryDirty = true;
  }
}

// tecla USER = atras: cierra la capa de arriba (en el mismo orden que onTap y render),
// con el mismo codigo con que se cierra cada pantalla al tocar o deslizar. Sin sonido,
// como al cerrar con el dedo.
// cambia el nivel de brillo (tecla BOOT o la pildora del reloj): aviso en pantalla y
// guardado diferido
void cycleBright() {
  gBright = nextBrightLevel(gBright);
  briSaveAt = millis() + BRI_SAVE_DELAY_MS;
  briToastUntil = millis() + BRI_TOAST_MS;
  sfxPlay(SFX_TAP);
}

void saveBright() {
  briSaveAt = 0;
  Preferences p;
  p.begin("tamapoke", false);
  p.putUChar("bri", gBright);
  p.end();
}

// tecla BOOT = brillo. A oscuras solo despierta, como USER y el primer toque
void onBootKey() {
  if (screenOff || dimStage > 0) {
    screenOff = false;
    wakeUI();
    return;
  }
  wakeUI();
  cycleBright();
}

void onBack() {
  if (screenOff || dimStage > 0) {  // a oscuras solo despierta (como el primer toque)
    screenOff = false;
    wakeUI();
    return;
  }
  wakeUI();
  if (pet.awaitingStarter()) return;  // hay que elegir inicial
  if (sackOpen) {                     // abandona (sin premio si aun corre el tiempo)
    sackOpen = false;
    return;
  }
  if (battleOpen()) {                 // abandona el combate (sin premio)
    closeBattle();
    return;
  }
  if (galleryOpen) {
    if (galleryDetail) {  // detalle -> rejilla
      galleryDetail = 0;
      galleryDirty = true;
    } else {              // rejilla -> fuera
      galleryOpen = false;
    }
    galleryPmd.unload();
    return;
  }
  if (boxOpen) {
    if (boxMove) return;                   // la luz ya vuela: el movimiento no se corta
    if (boxAsk) { boxAsk = 0; return; }    // "guardar?" / "despertar?": NO
    closeBox();
    return;
  }
  if (kbOpen) { kbOpen = false; return; }        // descarta el nombre y vuelve a la ficha
  if (clockOpen) { clockOpen = false; return; }  // sin aplicar la hora, como al deslizar
  if (pet.ceremony) return;                      // durante la despedida no hay vuelta atras
  if (cardOpen && itemConfirm) { itemConfirm = 0; return; }  // "usar objeto?": NO
  if (cardOpen) { cardOpen = false; return; }
  if (gameOpen) { gameOpen = false; return; }    // abandona sin premio, como tocar la cabecera
  if (choiceKind) { choiceKind = 0; return; }    // ni evoluciona ni rechaza: el boton sigue ahi
  if (confirmUntil) { confirmUntil = 0; return; }  // "soltar?": NO
  if (menuKind) { menuKind = MENU_NONE; return; }
  if (foundShown) { pet.popFound(); foundShown = false; return; }  // cierra el hallazgo
}

void onTap(int16_t x, int16_t y) {
  // Serial.printf("TOUCH %d %d\n", x, y);  // diagnostico (silenciado: satura el log)
  if (pet.awaitingStarter()) {  // primera partida: elegir inicial
    for (int i = 0; i < 3; i++) {
      int ry = STARTER_ROW_Y + i * (STARTER_ROW_H + STARTER_ROW_GAP);
      if (x >= 70 && x <= 396 && y >= ry && y <= ry + STARTER_ROW_H) {
        pet.chooseStarter(STARTER_DEX[i]);
        sfxPlay(SFX_TAP);
        break;
      }
    }
    return;
  }
  if (galleryOpen) {
    galleryTap(x, y);
    return;
  }
  if (boxOpen) {
    boxTap(x, y);
    return;
  }
  if (kbOpen) {
    keyboardTap(x, y);
    return;
  }
  if (clockOpen) {
    clockTap(x, y);
    return;
  }
  if (pet.ceremony) return;  // durante la despedida no hay botones
  if (cardOpen) {
    if (cardPage == 4) { walkTap(x, y); return; }  // paseo: mochila
    if (cardPage == 0 && y < 84) openKeyboard();  // tocar el nombre = renombrar
    else cardOpen = false;
    return;
  }
  if (gameOpen) {
    gameTap(x, y);
    return;
  }
  if (choiceKind) {          // dialogo de decision: boton accion (arriba) / mantener (abajo)
    bool b1 = (x >= 93 && x <= 373 && y >= 206 && y <= 258);  // accion
    bool b2 = (x >= 93 && x <= 373 && y >= 268 && y <= 320);  // mantener / quedaros
    if (choiceKind == 1) {                 // evolucion
      if (b1) { int16_t old = pet.speciesId; pet.evolve(); evoPmd.load(old, pet.shiny); }
      else if (b2) pet.declineEvolve();
    } else if (choiceKind == 2) {          // despedida
      if (b1) pet.startFarewell();
      else if (b2) pet.declineFarewell();
    }
    choiceKind = 0;
    return;
  }
  if (confirmUntil) {        // dialogo "soltar?": SI / NO
    if (timeLeft(confirmUntil) && x >= 118 && x <= 218 && y >= 252 && y <= 304) {
      pet.release();
    }
    confirmUntil = 0;
    return;
  }
  if (menuKind) {            // menu de comida o de juego: un toque elige o lo cierra
    uint8_t kind = menuKind;
    bool live = timeLeft(menuUntil);
    menuKind = MENU_NONE;
    if (!live || y < 288 || y > 352) return;
    if (kind == MENU_FEED && x >= 101 && x <= 365) {
      int item = (x - 101) / 66;
      FxNeedSnap ns = fxSnap();
      if (item == 3) pet.feedCandy();
      else pet.feedBerry(item);
      fxNeeds(ns);  // "+25" sobre la barra de comida
      sfxPlay(SFX_EAT);
    } else if (kind == MENU_PLAY && x >= PLAY_MENU_X && x < PLAY_MENU_X + PLAY_MENU_W) {
      sfxPlay(SFX_TAP);
      int slot = (x - PLAY_MENU_X) / PLAY_MENU_SLOT;  // pelota | combate | saco
      if (slot == 0) startGame();
      else if (slot == 2) startSack();
      else if (pet.canBattle()) startPractice(0);
      else {
        sfxPlay(SFX_DENY);
        fxo.push(FXO_DENY, 1, 0, millis());  // tiembla JUGAR, debajo del menu ya cerrado
      }
    }
    return;
  }
  if (foundShown) {          // hallazgo del paseo en pantalla: un toque lo cierra
    pet.popFound();
    foundShown = false;
    sfxPlay(SFX_TAP);
    return;
  }
  if (pet.isVacant()) {  // nada que tocar: el mensaje dice que hay que caminar
    sfxPlay(SFX_DENY);
    return;
  }
  if (pet.isEgg()) {
    pet.eggTap();
    sfxPlay(SFX_TAP);
    return;
  }
  // boton de evolucion: abre el dialogo evolucionar/mantener
  if (pet.wantEvolveButton() && x >= EVO_BTN_X && x <= EVO_BTN_X + EVO_BTN_W &&
      y >= EVO_BTN_Y && y <= EVO_BTN_Y + EVO_BTN_H) {
    choiceKind = 1; choiceUntil = millis() + 12000;
    return;
  }
  // botones de final (mismo recuadro): escapada directa; despedida abre dialogo
  if (x >= FAR_BTN_X && x <= FAR_BTN_X + FAR_BTN_W &&
      y >= FAR_BTN_Y && y <= FAR_BTN_Y + FAR_BTN_H) {
    if (pet.canRunawayNow()) { pet.startRunaway(); return; }
    if (pet.wantFarewellButton()) { choiceKind = 2; choiceUntil = millis() + 12000; return; }
  }
  for (int i = 0; i < 4; i++) {
    int dx = x - buttons[i].cx, dy = y - buttons[i].cy;
    if (dx * dx + dy * dy <= BTN_HIT * BTN_HIT) {
      Serial.printf("BTN %d\n", i);
      fxo.push(FXO_PRESS, i, 0, millis());
      // dormido solo funciona LUZ, y el bano no se repite a medias: el boton tiembla (fx.h)
      if ((pet.sleeping && i != 2) || (i == 3 && bathUntil)) {
        sfxPlay(SFX_DENY);
        fxo.push(FXO_DENY, i, 0, millis());
        return;
      }
      sfxPlay(SFX_TAP);
      if (i == 0) {
        if (!pet.sleeping) { menuKind = MENU_FEED; menuUntil = millis() + 6000; }
      } else if (i == 1) {     // jugar: pelota o combate de practica
        if (!pet.isEgg() && !pet.sleeping && !pet.ceremony) { menuKind = MENU_PLAY; menuUntil = millis() + 6000; }
      } else if (i == 2) {
        pet.toggleLight();
      } else {
        startBath();
      }
      return;
    }
  }
  // tocar al bicho = caricia
  if (inPetZone(x, y)) {
    Serial.println("PET");
    pet.caress();
    if (!pet.sleeping) sfxPlay(SFX_HEART);
  }
}

// ---------- render ----------

bool gNight = false;  // noche real (por hora) o durmiendo: lo fija render()
uint16_t inkColor() { return gNight ? UI_INK_NIGHT : UI_INK; }

// ---------- escena de fondo: bioma del tipo + hora real del RTC ----------

#define C565(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
#define BOX_NOTE_INK C565(0x6a, 0x6a, 0x7a)  // segunda linea de un dialogo (caja; "soltar?" sin huevo, v1.39)
#define HORIZON 232  // linea donde el cielo se encuentra con el suelo

uint16_t lerp565(uint16_t a, uint16_t b, int i, int n) {
  if (n <= 0) return a;
  int ar = (a >> 11) & 31, ag = (a >> 5) & 63, ab = a & 31;
  int br = (b >> 11) & 31, bg = (b >> 5) & 63, bb = b & 31;
  return (uint16_t)((((ar + (br - ar) * i / n) << 11)) |
                    (((ag + (bg - ag) * i / n) << 5)) | (ab + (bb - ab) * i / n));
}

// hora del dia 0-23 (de la hora real cacheada cada 30s; 13 si no hay reloj)
int sceneHour() {
  uint32_t e = pet.lastSeenEpoch;
  return e ? (int)((e / 3600) % 24) : 13;
}

// suelo de cada bioma de dia (de noche se mezcla hacia el azul nocturno)
static const uint16_t BIOME_SOIL[BIOME_COUNT] = {
  C565(0x7e, 0xc0, 0x7f),  // 0 pradera
  C565(0xdc, 0xca, 0x94),  // 1 playa (arena)
  C565(0x4f, 0x8a, 0x55),  // 2 bosque
  C565(0x8a, 0x55, 0x44),  // 3 volcan
  C565(0xa8, 0x90, 0x6a),  // 4 montana
  C565(0xe6, 0xee, 0xf5),  // 5 nieve
  C565(0xa6, 0xb0, 0x8a),  // 6 central electrica: hierba seca junto a la ciudad
  C565(0x6e, 0x80, 0x5c),  // 7 pantano: barro verdoso
  C565(0x9a, 0x92, 0xa8),  // 8 ruinas: losas de piedra lavanda
};
static_assert(sizeof(BIOME_SOIL) / sizeof(BIOME_SOIL[0]) == BIOME_COUNT, "un suelo por bioma");

void drawClouds(uint32_t now, uint16_t col) {
  for (int k = 0; k < 2; k++) {
    int cx = (int)((now / 50 + k * 250) % 560) - 40;
    int cy = 70 + k * 34;
    gfx->fillCircle(cx, cy, 16, col);
    gfx->fillCircle(cx + 18, cy + 3, 13, col);
    gfx->fillCircle(cx - 15, cy + 4, 12, col);
  }
}

// paisaje de la pantalla principal: el del tipo del bicho (el prado para el huevo)
uint8_t homeBiome() {
  if (gBiomeOverride >= 0) return (uint8_t)gBiomeOverride;
  return pet.isEgg() ? 0 : DEX_TBL[pet.speciesId].biome;
}

// salvapantallas (v1.28): a los 5 min sin tocar, salvo boca abajo o apagada con PWR
bool saverOn() { return !screenOff && !gFaceDown && dimStage == DIM_OFF; }

void perfNote(uint32_t us) {
  gSceneUs = us;
  if (us > gSceneMaxUs) gSceneMaxUs = us;
}

// ---------- prado de hierba alta (bioma 0, v1.26) ----------
// Hierba que se mece con el viento (wind.h), en tres planos: un borde dentado de hierba
// sobre la loma, matas altas en medio con flores, y matas delante que rozan los pies del
// bicho (drawSceneFront, despues de dibujarlo). Posiciones fijas, sorteadas una vez con
// una semilla propia: el prado es siempre el mismo y no gasta el random() del juego.
#define MEADOW_EDGE_STEP 6     // un diente del borde cada 6 px
#define MEADOW_MID 15          // matas de en medio
#define MEADOW_FRONT 9         // matas de delante (con huecos: el bicho se ve entre ellas)
#define MEADOW_TUFT 5          // briznas por mata, abiertas en abanico
#define MEADOW_FLOWERS 7
#define MEADOW_EDGE_N (466 / MEADOW_EDGE_STEP + 2)
struct Tuft { int16_t x; int16_t y; uint8_t h[MEADOW_TUFT]; };
static Tuft meadowMid[MEADOW_MID], meadowFront[MEADOW_FRONT];
static uint8_t meadowEdge[MEADOW_EDGE_N];  // altura de cada diente del borde
static int16_t meadowFlower[MEADOW_FLOWERS][4];  // x, y, alto del tallo, color
static const uint16_t FLOWER_COL[3] = { C565(0xff, 0xff, 0xff), C565(0xff, 0xd9, 0x5e), C565(0xf0, 0x8a, 0xa4) };

static void meadowInit() {
  static bool done = false;
  if (done) return;
  done = true;
  uint32_t seed = 0x5eed1e5u;
  auto rnd = [&](int n) { seed = seed * 1664525u + 1013904223u; return (int)((seed >> 16) % (uint32_t)n); };
  for (auto &e : meadowEdge) e = (uint8_t)(8 + rnd(9));
  for (int i = 0; i < MEADOW_MID; i++) {
    meadowMid[i].x = (int16_t)(12 + i * 446 / MEADOW_MID + rnd(16));
    meadowMid[i].y = (int16_t)(HORIZON + 34 + rnd(14));
    for (auto &h : meadowMid[i].h) h = (uint8_t)(12 + rnd(11));
  }
  for (int i = 0; i < MEADOW_FLOWERS; i++) {
    meadowFlower[i][0] = (int16_t)(30 + i * 410 / MEADOW_FLOWERS + rnd(30));
    meadowFlower[i][1] = (int16_t)(HORIZON + 30 + rnd(20));
    meadowFlower[i][2] = (int16_t)(17 + rnd(7));
    meadowFlower[i][3] = (int16_t)rnd(3);
  }
  for (int i = 0; i < MEADOW_FRONT; i++) {
    meadowFront[i].x = (int16_t)(20 + i * 430 / (MEADOW_FRONT - 1) - 8 + rnd(16));
    meadowFront[i].y = 313;  // la base queda bajo el panel de abajo
    for (int b = 0; b < MEADOW_TUFT; b++) meadowFront[i].h[b] = (uint8_t)((b == 2 ? 18 : 10) + rnd(8));
  }
}

// una brizna: triangulo con la base en (x, y) y la punta h px mas arriba, inclinada por el
// viento (las altas, mas) y por `splay`, lo que se abre hacia fuera dentro de su mata.
// (enteros y no un struct: el conversor del .ino pone los prototipos antes de los structs)
static void drawBlade(int x, int y, int h, int w, int splay, uint32_t now, uint16_t col) {
  int lean = splay + windSway(now, x) * h / 16;
  gfx->fillTriangle(x - w, y, x + w, y, x + lean, y - h, col);
}

// mata de MEADOW_TUFT briznas en abanico, alternando el tono oscuro y el claro
static void drawTuft(const int16_t x, const int16_t y, const uint8_t *h, uint32_t now, uint16_t dark, uint16_t light) {
  for (int b = 0; b < MEADOW_TUFT; b++) {
    int off = (b - MEADOW_TUFT / 2) * 3;
    drawBlade(x + off, y, h[b], 2, off * h[b] / 14, now, (b & 1) ? light : dark);
  }
}

static void drawMeadow(uint32_t now, bool night, uint16_t soil, uint16_t dk) {
  meadowInit();
  uint16_t back = lerp565(soil, dk, 5, 16), mid = lerp565(soil, dk, 10, 16);
  uint16_t light = lerp565(soil, night ? C565(0x30, 0x40, 0x48) : C565(0xd8, 0xf0, 0xa0), 6, 16);
  // borde dentado de hierba sobre la loma: se mece todo junto, como un campo visto de lejos
  for (int i = 0; i < MEADOW_EDGE_N; i++) {
    int x = i * MEADOW_EDGE_STEP;
    drawBlade(x, HORIZON + 8, meadowEdge[i], 4, 0, now, back);
  }
  gfx->fillRect(0, HORIZON + 8, 466, 6, back);  // la base del borde, sin huecos
  for (auto &f : meadowFlower) {  // flores: tallo que se mece y cabeza que lo sigue
    int lean = windSway(now, f[0]) * f[2] / 16;
    int hx = f[0] + lean, hy = f[1] - f[2];
    gfx->drawLine(f[0], f[1], hx, hy, mid);
    gfx->drawLine(f[0] + 1, f[1], hx + 1, hy, mid);
    uint16_t pc = night ? lerp565(FLOWER_COL[f[3]], C565(0x16, 0x1c, 0x30), 9, 16) : FLOWER_COL[f[3]];
    gfx->fillCircle(hx, hy, 4, pc);
    gfx->fillRect(hx - 1, hy - 1, 2, 2, night ? C565(0x80, 0x70, 0x30) : C565(0xe8, 0xa2, 0x3c));
  }
  for (auto &t : meadowMid) drawTuft(t.x, t.y, t.h, now, mid, light);
}

// las matas de delante, encima del bicho (render() la llama tras dibujarlo y antes del panel)
void drawSceneFront(uint8_t biome, uint32_t now, bool night) {
  if (biome != 0) return;
  meadowInit();
  uint16_t soil = BIOME_SOIL[0];
  if (night) soil = lerp565(soil, C565(0x16, 0x1c, 0x30), 9, 16);
  uint16_t dk = lerp565(soil, C565(0x10, 0x18, 0x20), night ? 11 : 7, 16);
  uint16_t front = lerp565(soil, dk, 13, 16), frontL = lerp565(soil, dk, 9, 16);
  for (auto &t : meadowFront) drawTuft(t.x, t.y, t.h, now, front, frontL);
}

// ---------- central electrica (bioma 6, v1.27) ----------
// Siluetas de edificios en el horizonte, con ventanas que de noche se encienden (y alguna
// parpadea), y delante dos torres de alta tension con cables que se mecen y, de vez en
// cuando, una chispa.
static const int16_t SKYLINE[][3] = {  // x, ancho, alto
  { 0, 34, 54 }, { 30, 26, 76 }, { 60, 40, 44 }, { 104, 22, 64 }, { 300, 30, 58 },
  { 334, 24, 84 }, { 362, 38, 48 }, { 404, 28, 70 }, { 436, 30, 52 },
};

void drawSkyline(uint32_t now, bool night, uint16_t sky) {
  uint16_t bld = night ? C565(0x22, 0x28, 0x3c) : lerp565(sky, C565(0x40, 0x48, 0x5c), 9, 16);
  uint16_t win = night ? C565(0xff, 0xd9, 0x5e) : lerp565(bld, C565(0x20, 0x24, 0x30), 6, 16);
  int k = 0;
  for (auto &b : SKYLINE) {
    int x = b[0], w = b[1], h = b[2], y = HORIZON - h;
    gfx->fillRect(x, y, w, h, bld);
    for (int wy = y + 6; wy < HORIZON - 16; wy += 10)
      for (int wx = x + 4; wx + 4 <= x + w - 3; wx += 8, k++) {
        // de noche no todas: unas apagadas y alguna que se enciende y se apaga
        uint32_t hsh = (uint32_t)k * 2654435761u;
        if (night && (hsh >> 29) == 0) continue;
        if (night && (hsh >> 28) == 3 && ((now / (1800 + (hsh >> 20) % 900)) & 1)) continue;
        gfx->fillRect(wx, wy, 4, 5, win);
      }
  }
  // chimenea de la central, con humo que sube y se va
  gfx->fillRect(200, HORIZON - 70, 18, 70, bld);
  gfx->fillRect(200, HORIZON - 62, 18, 4, night ? C565(0xe8, 0x50, 0x3a) : C565(0xd8, 0xd8, 0xd8));
  uint16_t smoke = night ? C565(0x50, 0x56, 0x6a) : C565(0xe8, 0xec, 0xf0);
  for (int p = 0; p < 3; p++) {
    uint32_t t = (now / 40 + p * 33) % 100;  // 0..99: sube 30 px (sin llegar al texto de arriba) y crece
    int sx = 209 + (int)t / 5 + windSway(now, 209) * (int)t / 40;
    gfx->fillCircle(sx, HORIZON - 76 - (int)t * 3 / 10, 3 + (int)t / 20, smoke);
  }
}

// cable entre dos postes, colgando en una curva que el viento empuja un poco
static void drawCable(int x0, int y0, int x1, int y1, int sag, uint32_t now, uint16_t col) {
  const int N = 8;
  int px = x0, py = y0;
  for (int i = 1; i <= N; i++) {
    int x = x0 + (x1 - x0) * i / N;
    int lin = y0 + (y1 - y0) * i / N;
    int u = i * (N - i);  // 0 en los extremos, maximo en medio
    int y = lin + sag * u * 4 / (N * N) + windSway(now, x) * u / (N * N);
    gfx->drawLine(px, py, x, y, col);
    px = x;
    py = y;
  }
}

void drawPylons(uint32_t now, bool night, uint16_t dk) {
  uint16_t steel = night ? C565(0x50, 0x58, 0x70) : lerp565(dk, C565(0x60, 0x68, 0x78), 8, 16);
  const int PX[2] = { 128, 344 };
  const int base = HORIZON + 24, topY = HORIZON - 64;
  for (int px : PX) {  // torre de celosia: patas, cruces y dos brazos
    gfx->drawLine(px - 14, base, px - 3, topY, steel);
    gfx->drawLine(px + 14, base, px + 3, topY, steel);
    gfx->drawLine(px - 13, base, px - 2, topY, steel);
    gfx->drawLine(px + 13, base, px + 2, topY, steel);
    for (int y = topY + 14; y < base - 6; y += 16) {
      int hw = 3 + (y - topY) * 11 / (base - topY);
      gfx->drawLine(px - hw, y, px + hw, y + 14, steel);
      gfx->drawLine(px + hw, y, px - hw, y + 14, steel);
    }
    gfx->fillRect(px - 20, topY + 6, 41, 3, steel);
    gfx->fillRect(px - 14, topY + 20, 29, 3, steel);
  }
  uint16_t wire = night ? C565(0x70, 0x78, 0x90) : C565(0x38, 0x3c, 0x48);
  for (int a = 0; a < 2; a++) {  // dos cables por brazo, de borde a borde
    int y = topY + 8 + a * 14, hw = a ? 14 : 20;
    drawCable(-10, y + 10, PX[0] - hw, y, 8, now, wire);
    drawCable(PX[0] + hw, y, PX[1] - hw, y, 14, now, wire);
    drawCable(PX[1] + hw, y, 476, y + 10, 8, now, wire);
  }
  // chispa: cada ~7 s, 200 ms en la punta de un brazo
  uint32_t t = now % 7300;
  if (t < 200) {
    int sx = (now / 7300) % 2 ? PX[1] + 20 : PX[0] - 20, sy = topY + 6;
    uint16_t sp = C565(0xff, 0xf0, 0x80);
    gfx->drawLine(sx, sy, sx - 5, sy + 6, sp);
    gfx->drawLine(sx - 5, sy + 6, sx + 2, sy + 9, sp);
    gfx->drawLine(sx + 2, sy + 9, sx - 3, sy + 16, sp);
    gfx->fillCircle(sx, sy, 3, UI_WHITE);
  }
}

// ---------- pantano (bioma 7, v1.27) ----------
// Una charca turbia con ondas, juncos que se mecen con el viento (wind.h), burbujas que
// suben y revientan, y un arbol seco a un lado.
static const int16_t REEDS[][3] = {  // x, base y, alto
  { 26, 270, 34 }, { 34, 272, 26 }, { 44, 270, 38 }, { 150, 262, 30 }, { 158, 264, 22 },
  { 300, 266, 34 }, { 309, 268, 26 }, { 404, 270, 40 }, { 414, 272, 30 }, { 424, 270, 24 },
};

void drawSwamp(uint32_t now, bool night, uint16_t soil, uint16_t dk) {
  // arbol seco a la izquierda, en silueta
  uint16_t bark = lerp565(dk, C565(0x30, 0x24, 0x20), 6, 16);
  gfx->fillRect(78, HORIZON - 40, 6, 56, bark);
  gfx->drawLine(81, HORIZON - 30, 60, HORIZON - 54, bark);
  gfx->drawLine(82, HORIZON - 30, 61, HORIZON - 54, bark);
  gfx->drawLine(81, HORIZON - 20, 104, HORIZON - 46, bark);
  gfx->drawLine(82, HORIZON - 20, 105, HORIZON - 46, bark);
  gfx->drawLine(96, HORIZON - 37, 108, HORIZON - 38, bark);
  // charca
  uint16_t pond = night ? C565(0x26, 0x22, 0x3a) : C565(0x5e, 0x58, 0x7c);
  uint16_t shine = night ? C565(0x40, 0x3a, 0x5a) : C565(0x96, 0x8e, 0xb0);
  gfx->fillRoundRect(60, HORIZON + 14, 346, 34, 16, pond);
  for (int i = 0; i < 4; i++) {  // ondas que van y vienen
    int wx = 96 + i * 76 + (int)((now / 90 + i * 17) % 24) - 12;
    gfx->fillRect(wx, HORIZON + 24 + (i % 2) * 10, 20, 2, shine);
  }
  // burbujas: suben desde el fondo de la charca y revientan arriba
  uint16_t bub = night ? C565(0x9a, 0xd0, 0x6a) : C565(0xb8, 0xe0, 0x7c);
  for (int b = 0; b < 5; b++) {
    uint32_t period = 2600 + b * 410;
    uint32_t t = (now + b * 977) % period;
    int bx = 110 + b * 62;
    if (t < period - 300) {
      int by = HORIZON + 42 - (int)(t * 24 / (period - 300));
      gfx->drawCircle(bx, by, 2 + b % 2, bub);
    } else {  // revienta: un anillo que se abre
      gfx->drawCircle(bx, HORIZON + 18, 4 + (int)(t - (period - 300)) / 60, shine);
    }
  }
  // juncos con su espiga, meciendose
  uint16_t reed = lerp565(soil, dk, 12, 16), head = night ? C565(0x40, 0x2c, 0x22) : C565(0x7e, 0x55, 0x30);
  for (auto &r : REEDS) {
    int lean = windSway(now, r[0]) * r[2] / 20;
    gfx->fillTriangle(r[0] - 2, r[1], r[0] + 2, r[1], r[0] + lean, r[1] - r[2], reed);
    gfx->fillRoundRect(r[0] + lean * 3 / 4 - 2, r[1] - r[2] * 3 / 4 - 6, 5, 11, 2, head);
  }
}

// ---------- ruinas (bioma 8, v1.27) ----------
// Columnas rotas de piedra bajo un cielo violeta, una caida en el suelo, y fuegos fatuos
// que flotan (mas de noche).
static const int16_t PILLARS[][3] = {  // x, ancho, alto
  { 40, 26, 92 }, { 104, 24, 58 }, { 338, 26, 104 }, { 408, 24, 46 },
};

void drawRuins(uint32_t now, bool night, uint16_t soil, uint16_t dk) {
  uint16_t stone = lerp565(soil, dk, 5, 16), shade = lerp565(soil, dk, 10, 16);
  uint16_t crack = lerp565(soil, dk, 15, 16);
  for (auto &p : PILLARS) {
    int x = p[0], w = p[1], h = p[2], y = HORIZON + 16 - h;
    gfx->fillRect(x, y, w, h, stone);
    gfx->fillRect(x + w - 6, y, 6, h, shade);                 // lado en sombra
    gfx->fillRect(x - 4, HORIZON + 10, w + 8, 8, shade);      // basa
    for (int f = x + 6; f < x + w - 6; f += 6) gfx->fillRect(f, y + 8, 2, h - 14, shade);  // estrias
    // parte de arriba rota: dientes de distinta altura
    gfx->fillTriangle(x, y, x + w / 2, y, x, y - 10, stone);
    gfx->fillTriangle(x + w / 2, y, x + w, y, x + w, y - 5, shade);
    gfx->drawLine(x + 8, y + 18, x + 13, y + 34, crack);
    gfx->drawLine(x + 13, y + 34, x + 10, y + 46, crack);
  }
  // columna caida, de lado
  gfx->fillRoundRect(196, HORIZON + 30, 96, 18, 6, stone);
  gfx->fillRect(196, HORIZON + 42, 96, 6, shade);
  gfx->fillRect(286, HORIZON + 28, 10, 22, shade);
  // fuegos fatuos: flotan, suben y bajan, con un halo
  int n = night ? 6 : 3;
  uint16_t core = night ? C565(0xd8, 0xc8, 0xff) : C565(0xf4, 0xee, 0xff);
  uint16_t halo = night ? C565(0x6a, 0x50, 0xb0) : C565(0xb8, 0xa4, 0xe8);
  for (int i = 0; i < n; i++) {
    uint32_t ph = now / 40 + i * 23;
    int wx = 60 + i * 69 + WIND_SIN[(ph / 3) & 63] * 18 / 127;
    int wy = HORIZON - 40 - (i % 3) * 22 + WIND_SIN[(ph + i * 9) & 63] * 8 / 127;
    gfx->fillCircle(wx, wy, 6, halo);
    gfx->fillCircle(wx, wy, 3, core);
  }
}

void drawScene(uint8_t biome, uint32_t now, bool night) {
  int h = sceneHour();
  uint16_t top, bot;
  if (night)            { top = C565(0x0c, 0x12, 0x24); bot = C565(0x1e, 0x26, 0x46); }
  else if (h < 8)       { top = C565(0xd1, 0x6a, 0x86); bot = C565(0xf3, 0xb8, 0x7c); }  // amanecer
  else if (h < 18)      { top = C565(0x8f, 0xc8, 0xea); bot = C565(0xdc, 0xee, 0xe6); }  // dia
  else                  { top = C565(0xc7, 0x5a, 0x4a); bot = C565(0xf0, 0xae, 0x64); }  // atardecer
  if (biome == 8) {  // ruinas: el cielo tira a violeta a cualquier hora
    top = lerp565(top, C565(0x5a, 0x3c, 0x8c), 6, 16);
    bot = lerp565(bot, C565(0xb8, 0x9c, 0xd8), 6, 16);
  }

  // cielo en bandas
  for (int y = 0; y < HORIZON; y += 8)
    gfx->fillRect(0, y, 466, 8, lerp565(top, bot, y, HORIZON));

  // sol o luna
  if (night) {
    gfx->fillCircle(360, 78, 24, C565(0xe8, 0xee, 0xf5));
    gfx->fillCircle(370, 72, 22, lerp565(top, bot, 78, HORIZON));  // creciente
    for (auto &st : STARS) gfx->fillRect(st[0], st[1], 4, 4, UI_WHITE);
  } else if (h < 18) {
    gfx->fillCircle(360, 84, 26, h < 8 ? C565(0xff, 0xd9, 0x8a) : C565(0xff, 0xe7, 0x9f));
    drawClouds(now, C565(0xff, 0xff, 0xff));
  } else {
    gfx->fillCircle(233, HORIZON - 6, 34, C565(0xff, 0xf1, 0xc8));  // sol poniente
  }

  // mar de la playa: una franja de agua sobre la arena
  uint16_t soil = BIOME_SOIL[biome < BIOME_COUNT ? biome : 0];
  if (night) soil = lerp565(soil, C565(0x16, 0x1c, 0x30), 9, 16);
  if (biome == 1) {
    uint16_t sea = night ? C565(0x1c, 0x34, 0x52) : C565(0x4f, 0x96, 0xc4);
    gfx->fillRect(0, HORIZON - 26, 466, 26, sea);
    for (int i = 0; i < 3; i++) {
      int wy = HORIZON - 22 + i * 7;
      uint16_t fc = night ? C565(0x3a, 0x58, 0x78) : C565(0xbf, 0xe6, 0xf5);
      gfx->fillRect(60 + ((now / 60 + i * 30) % 60), wy, 26, 2, fc);
      gfx->fillRect(300 - ((now / 60 + i * 20) % 60), wy, 26, 2, fc);
    }
  }

  if (biome == 6) drawSkyline(now, night, lerp565(top, bot, 3, 4));  // la ciudad, detras de la loma

  // suelo
  gfx->fillRect(0, HORIZON, 466, 466 - HORIZON, soil);
  uint16_t hill = lerp565(soil, night ? C565(0x0c, 0x12, 0x24) : C565(0xff, 0xff, 0xff), 3, 16);
  gfx->fillRoundRect(-60, HORIZON - 14, 586, 60, 30, hill);

  // detalles del bioma
  uint16_t dk = lerp565(soil, C565(0x10, 0x18, 0x20), night ? 11 : 7, 16);
  if (biome == 2) {  // bosque: coniferas en silueta
    for (int tx : { 60, 150, 360, 416 }) {
      int sw = windSway(now, tx);  // las copas se mecen un poco con el viento (v1.26)
      gfx->fillTriangle(tx + sw / 3, HORIZON - 46, tx - 16, HORIZON, tx + 16, HORIZON, dk);
      gfx->fillTriangle(tx + sw / 2, HORIZON - 60, tx - 12, HORIZON - 28, tx + 12, HORIZON - 28, dk);
    }
  } else if (biome == 3) {  // volcan: rocas y brasas
    gfx->fillTriangle(70, HORIZON, 40, HORIZON + 30, 100, HORIZON + 30, dk);
    gfx->fillTriangle(400, HORIZON + 4, 372, HORIZON + 30, 430, HORIZON + 30, dk);
    if (!night)
      for (int e = 0; e < 4; e++)
        gfx->fillRect(120 + e * 70, HORIZON + 8 + (e % 2) * 6, 4, 4, C565(0xff, 0x9b, 0x3a));
  } else if (biome == 4) {  // montana: cumbres al fondo
    gfx->fillTriangle(140, HORIZON - 50, 60, HORIZON, 220, HORIZON, dk);
    gfx->fillTriangle(330, HORIZON - 38, 250, HORIZON, 410, HORIZON, dk);
  } else if (biome == 5 && !night) {  // nieve: copos cayendo
    for (int f = 0; f < 10; f++) {
      int fx = (f * 53 + now / 40) % 466;
      int fy = (f * 90 + now / 18) % HORIZON;
      gfx->fillRect(fx, fy, 3, 3, UI_WHITE);
    }
  } else if (biome == 0) {  // pradera: hierba alta que se mece (la fila de delante, en drawSceneFront)
    drawMeadow(now, night, soil, dk);
  } else if (biome == 6) {
    drawPylons(now, night, dk);
  } else if (biome == 7) {
    drawSwamp(now, night, soil, dk);
  } else if (biome == 8) {
    drawRuins(now, night, soil, dk);
  }
}

// primera partida: elige inicial entre Bulbasaur / Charmander / Squirtle
void renderStarterSelect() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  const char *t = T(S_CHOOSE_STARTER);
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(centerX(t, 2), 68);
  printT(t);
  for (int i = 0; i < 3; i++) {
    int16_t d = STARTER_DEX[i];
    const DexEntry &de = DEX_TBL[d];
    int ry = STARTER_ROW_Y + i * (STARTER_ROW_H + STARTER_ROW_GAP);
    gfx->fillRoundRect(70, ry, 326, STARTER_ROW_H, 14, lerp565(de.accent, UI_WHITE, 6, 8));
    gfx->drawRoundRect(70, ry, 326, STARTER_ROW_H, 14, de.accent);
    const uint8_t *th = thumbs.get(d);     // miniatura del inicial (si la SD esta lista)
    if (th) drawThumb(th, 76, ry - 5, 3, false);
    gfx->setTextColor(UI_INK);
    setSize(3);
    setCur(178, ry + 24);
    printT(dexName(d));
  }
  gfx->flush();
}

// ---------- texto: tamano y cursor (preparado para fuentes CJK) ----------
// La fuente clasica ancla el cursor en la ESQUINA SUPERIOR del texto, y las 74
// coordenadas Y del codigo estan escritas asi. Las fuentes U8g2 lo anclan en la
// LINEA BASE (drawChar hace curY = y - baseline*size), asi que al cambiar de
// fuente todo el texto subiria. setCur() compensa esa diferencia en un solo
// sitio, para que la Y siga significando "arriba" con cualquier fuente.
//
// setSize() existe solo porque la libreria tiene textsize_x protegido y setCur()
// necesita saber la escala activa para multiplicar el ascenso.
//
// Con la fuente clasica gFontAscent es 0, asi que setCur(x,y) es exactamente
// setCur(x,y): esto no mueve un pixel en los seis idiomas actuales.
uint8_t gTextSize = 1;
int gFontAscent = 0;  // px del borde superior a la linea base, 0 = fuente clasica

// --- ajuste de la fuente CJK (los dos valores van juntos) ---
// La fuente clasica mide 8 px de alto a escala 1. Si la CJK mide mas, hay que
// dividir la escala para que los tamanos cuadren... pero dividir ADELGAZA el
// trazo: a escala 1 la unifont pinta lineas de 1 px mientras el latino a escala
// 2 las pinta de 2, y el japones se ve tenue aunque mida igual.
//
// Por eso conviene una fuente base pequena Y EN NEGRITA (_b_), que permita usar
// la MISMA escala que el latino: asi el grosor coincide y no hay que dividir.
//
// Se probo una fuente realmente negrita (b10_b) y NO sirve a este tamano: los
// trazos se juntan y los kana pierden detalle, ilegibles incluso para un lector
// nativo. Se vuelve a la unifont, que es la mas legible, y el trazo fino se
// compensa en printT() repintando 1 px desplazado (pseudo-negrita).
// japanese3 y no japanese1: los subconjuntos 1 y 2 traen kana y kanji pero
// NINGUNA puntuacion CJK, y Arduino_GFX no dibuja nada cuando le falta el
// glifo (ni avanza el cursor), asi que los 25 signos ！ y ？ de las cadenas
// desaparecian. Es la misma familia unifont: cabecera de metricas identica y
// superconjunto estricto, comprobado glifo a glifo sobre los 226 codepoints
// que usa el firmware. Cuesta 102 KB mas de flash y no mueve un pixel.
// El coreano necesita una fuente con hangul, y CJK_FONT era un solo #define.
// En vez de dar por hecho que un subconjunto sirve para las dos escrituras, la
// fuente se elige por idioma. korean2 y no korean1: el coreano de aqui usa 296
// silabas distintas y todas estan en KS X 1001, el conjunto de 2350 que es el
// tamano medido para korean2; korean1 trae 478. La comprobacion mira la
// pertenencia a KS X 1001, no recorre la tabla de la fuente, asi que se apoya
// en que el conjunto de korean2 sea ese; el ASCII se da por presente.
// Sin comprobar en placa: si CJK_SIZE_DIV 2 le sienta a korean2 igual que a la
// japonesa. El ascenso si se mide en tiempo de ejecucion en applyLangFont().
#define CJK_FONT_JA u8g2_font_unifont_t_japanese3
#define CJK_FONT_KO u8g2_font_unifont_t_korean2
#define CJK_SIZE_DIV 2

void setSize(uint8_t n) {
  // gTextSize guarda la escala REALMENTE aplicada, no la pedida: setCur()
  // multiplica el ascenso por ella y tiene que cuadrar con lo que se pinta.
  gTextSize = gCjkFont ? (n >= CJK_SIZE_DIV ? n / CJK_SIZE_DIV : 1) : n;
  gfx->setTextSize(gTextSize);
}

void setCur(int x, int y) {
  gfx->setCursor(x, y + gFontAscent * gTextSize);
}

// Fija la fuente del idioma activo. Es el UNICO sitio que la toca: el resto del
// codigo dibuja igual para todos los idiomas gracias a textW()/setCur().
//
// El ascenso se mide en vez de codificarlo: getTextBounds() devuelve y1 como
// desplazamiento del borde superior respecto al cursor, negativo con las fuentes
// U8g2 (que anclan en la linea base). Asi setCur() puede seguir tratando la Y
// como "arriba" sea cual sea la fuente, sin constantes magicas por fuente.
void applyLangFont() {
  gCjkFont = LANG_IS_CJK(gLang);
  if (!gCjkFont) {
    gfx->setFont();            // 5x7 clasica, CP437: la Y ya es el borde superior
    gfx->setUTF8Print(false);
    gFontAscent = 0;
    return;
  }
  gfx->setFont((gLang == LANG_KO) ? CJK_FONT_KO : CJK_FONT_JA);
  gfx->setUTF8Print(true);     // las cadenas CJK son UTF-8 multibyte
  int16_t x1, y1;
  uint16_t w, h;
  uint8_t antes = gTextSize;
  gfx->setTextSize(1);
  gfx->getTextBounds("A", 0, 0, &x1, &y1, &w, &h);
  gFontAscent = -y1;           // y1 negativo: subir desde la linea base
  gfx->setTextSize(antes);
}

// ---------- medida de texto (preparado para fuentes CJK) ----------
// La fuente clasica de Arduino_GFX avanza 6 px por caracter a tamano 1, y la UI
// centraba con strlen(s)*6*n a pelo. Eso solo vale para un byte por caracter y
// ancho fijo: con una fuente U8g2 para japones/coreano/chino, ni strlen() cuenta
// caracteres (UTF-8 es multibyte) ni todos miden lo mismo. Centralizarlo aqui es
// lo que permitira anadir esos idiomas sin tocar cada punto de dibujado.
//
// Para la fuente clasica devuelve EXACTAMENTE la misma cuenta que habia antes,
// asi que este cambio no mueve un pixel en los seis idiomas actuales.
uint16_t textW(const char *s, uint8_t size) {
  if (!gCjkFont) return (uint16_t)strlen(s) * 6 * size;
  int16_t x1, y1;
  uint16_t w, h;
  gfx->getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  return w;
}

// x del cursor para dejar el texto centrado en CX
int centerX(const char *s, uint8_t size) { return CX - textW(s, size) / 2; }

// Imprime respetando la fuente activa. Con la CJK repinta el texto desplazado
// 1 px: la unifont a escala reducida deja trazos de 1 px y se ve tenue, y una
// fuente de verdad negrita a este tamano junta los trazos y estropea los kana.
// Repintar engorda el trazo sin deformar el glifo. Probado en placa por
// usakomint, que comparo las dos opciones en japones.
// El teclado de apodos es solo alfabeto (se deja asi a proposito), de modo que
// aqui nunca hay CJK y no hace falta engordar nada.
void printT(char c) { gfx->print(c); }

void printT(const char *s) {
  if (gCjkFont) {
    int16_t x = gfx->getCursorX(), y = gfx->getCursorY();
    gfx->print(s);
    gfx->setCursor(x + 1, y);
  }
  gfx->print(s);
}

// Pantallas que solo cambian con un toque (o con un dato que cambia muy de vez en cuando):
// mientras su "clave" no cambie no se repintan ni se vuelcan por QSPI.
static bool staticScreen() {
  if (pet.awaitingStarter()) return true;
  if (galleryOpen || gameOpen || sackOpen || battleOpen()) return false;  // la galeria gestiona su propio repintado
  if (boxOpen) return !boxMove && !timeLeft(boxShakeUntil);  // la caja, quieta salvo la luz o un hueco que tiembla
  if (kbOpen || clockOpen) return true;
  return cardOpen && cardPage != 0;  // la pagina 0 tiene el retrato animado
}
uint32_t walkStepsToday();
static uint32_t mixKey(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }
static uint32_t screenKey() {
  uint32_t h = 2166136261u;
  h = mixKey(h, pet.awaitingStarter() ? 1 : boxOpen ? 20 : kbOpen ? 2 : clockOpen ? 3 : 4 + cardPage);
  h = mixKey(h, sdEpoch);
  h = mixKey(h, gBright);                      // el aviso de brillo va encima de todo
  h = mixKey(h, timeLeft(briToastUntil) > 0);  // y al irse tiene que repintarse
  h = mixKey(h, gOrbitStep);                   // la orbita la mueve: hay que volver a mandarla
  if (boxOpen) {
    // cada hueco (su CRC cambia con cualquier cosa del bicho), el activo, el huevo que espera y el dialogo
    for (uint8_t k = 0; k < BOX_SLOTS; k++) {
      h = mixKey(h, pet.boxState(k));
      if (pet.boxState(k) == BOX_OK) h = mixKey(h, pet.boxRecord(k).crc);
    }
    for (uint32_t v : { (uint32_t)pet.speciesId, (uint32_t)pet.shiny, pet.uid, (uint32_t)pet.eggHeld(),
                        (uint32_t)pet.canBoxMove(), (uint32_t)boxAsk, (uint32_t)gLang })
      h = mixKey(h, v);
    for (const char *c = pet.nick; *c; c++) h = mixKey(h, (uint8_t)*c);
  } else if (kbOpen) {
    h = mixKey(h, nameLen);
    for (int i = 0; i < nameLen; i++) h = mixKey(h, (uint8_t)nameBuf[i]);
  } else if (clockOpen) {
    h = mixKey(h, clockH * 60 + clockM);
    h = mixKey(h, audioEnabled());
    h = mixKey(h, gLang);
  } else if (cardOpen) {
    // growMin: el nivel y su barra (v1.35); un Caramelo Raro o una meta lo saltan sin tocar la edad
    for (uint32_t v : { (uint32_t)pet.speciesId, (uint32_t)pet.shiny, (uint32_t)pet.ageMinutes, pet.growMin, (uint32_t)pet.streak,
                        (uint32_t)pet.bestStreak, (uint32_t)pet.bond, (uint32_t)pet.medals, (uint32_t)pet.weight,
                        (uint32_t)pet.trAtk, (uint32_t)pet.trDef, (uint32_t)pet.trSpe, (uint32_t)pet.geneAtk,
                        (uint32_t)pet.geneDef, (uint32_t)pet.geneSpe, (uint32_t)pet.careMistakes,
                        (uint32_t)pet.fullness, (uint32_t)pet.joy, (uint32_t)pet.energy, (uint32_t)pet.hygiene,
                        (uint32_t)pet.berryKnown, (uint32_t)pet.strHi, (uint32_t)gLang })
      h = mixKey(h, v);
    // pagina de paseo: sin estos campos no se repintaria al llegar pasos
    for (uint32_t v : { walkStepsToday(), pet.stepsTotal, (uint32_t)pet.findsToday, pet.stepDay, pet.goalDay,
                        (uint32_t)pet.goalTier, (uint32_t)pet.bonusFindsToday,
                        (uint32_t)imuPresent(), (uint32_t)itemConfirm, pet.dayNumber() })
      h = mixKey(h, v);
    for (uint8_t n : pet.items) h = mixKey(h, n);
    for (const char *c = pet.nick; *c; c++) h = mixKey(h, (uint8_t)*c);
  }
  return h;
}
bool screenKeyValid = false;  // (el renderer del host lo invalida entre escenas)
uint32_t lastScreenKey = 0;

// Dibuja un frame y devuelve true si lo ha volcado al panel.
bool render() {
  if (itemConfirm && !timeLeft(itemConfirmUntil)) itemConfirm = 0;  // antes de la clave: asi se repinta
  if (boxAsk && !timeLeft(boxAskUntil)) boxAsk = 0;                  // (lo mismo, el dialogo de la caja)
  foundShown = false;  // lo vuelve a poner drawWalkNews() si el hallazgo sigue en pantalla
  if (saverOn()) {  // 5 min sin tocar: salvapantallas, encima de cualquier pantalla
    screenKeyValid = false;  // al despertar, la pantalla de debajo se vuelve a pintar entera
    renderScreensaver();
    return true;
  }
  if (staticScreen()) {
    uint32_t k = screenKey();
    if (screenKeyValid && k == lastScreenKey) return false;
    lastScreenKey = k;
    screenKeyValid = true;
  } else {
    screenKeyValid = false;
  }
  if (pet.awaitingStarter()) {  // primera partida: elegir inicial (prioridad total)
    renderStarterSelect();
    return true;
  }
  if (galleryOpen) {
    renderGallery();
    return true;
  }
  if (boxOpen) {
    renderBox();
    return true;
  }
  if (gameOpen) {
    renderGame();
    return true;
  }
  if (sackOpen) {
    renderSack();
    return true;
  }
  if (battleOpen() && renderBattle()) return true;  // false: se acaba de cerrar, se pinta la casa
  if (kbOpen) {
    renderKeyboard();
    return true;
  }
  if (clockOpen) {
    renderClock();
    return true;
  }
  if (cardOpen) {
    renderCard();
    return true;
  }
  int h = sceneHour();
  gNight = pet.sleeping || h < 6 || h >= 20;
  // drawScene cubre los 466x466 completos: sin fillScreen(NEGRO) previo para
  // que un flush DMA solapado nunca capture negro a medias (anti-parpadeo)
  uint32_t sceneT0 = micros();
  drawScene(homeBiome(), millis(), gNight);
  perfNote(micros() - sceneT0);

  if (pet.ceremony) {
    const DexEntry &d = DEX_TBL[pet.speciesId];
    const char *msg = (pet.ceremony == CER_FAREWELL) ? T(S_FAREWELL)
                      : (pet.ceremony == CER_RUNAWAY) ? T(S_RUNAWAY)
                                                      : T(S_GOODBYE);
    drawHeader(dexName(pet.speciesId), d.accent, msg);
    drawCeremony();
    gfx->flush();
    return true;
  }

  // efectos (fx.h): la cola avanza antes de dibujar, para saber si hay celebracion en este
  // frame (fija la pose y oculta los botones de evolucion o despedida mientras se ve)
  uint32_t fxT0 = micros();
  fxFrame(millis());
  uint32_t fxUs = micros() - fxT0;

  if (pet.isVacant()) {
    drawVacantHome();
  } else if (pet.isEgg()) {
    drawHeader(T(S_EGG_HDR), inkColor(), eggMsg());
    int dx = 0;
    if (pet.emote() == EMOTE_WOBBLE)  // agitado: se tambalea y se va calmando
      dx = (int)(8.0f * (1.0f - pet.emoteT()) * sinf(millis() * 0.035f));
    drawEggSprite(dx, pet.eggCracks());
    drawBoxAppear(millis());  // guardado: el huevo sale de una luz
    if (pet.eggRarity() >= R_RARO) {
      const char *rar = (pet.eggRarity() == R_LEGENDARIO) ? T(S_EGG_LEGEND) : T(S_EGG_RARE);
      gfx->setTextColor(pet.eggRarity() == R_LEGENDARIO ? UI_BAR_WARN : 0x4C98);
      setSize(2);
      setCur(centerX(rar, 2), 316);
      printT(rar);
    }
    char reg[24];
    snprintf(reg, sizeof(reg), T(S_POKEDEX_FMT), pet.registeredCount());
    gfx->fillRect(0, 312, 466, 154, gNight ? UI_BG_NIGHT : UI_BG_DAY);
    gfx->setTextColor(inkColor());
    setSize(2);
    setCur(centerX(reg, 2), 348);
    printT(reg);
  } else {
    const DexEntry &d = DEX_TBL[pet.speciesId];
    char name[28];
    const char *base = pet.nick[0] ? pet.nick : dexName(pet.speciesId);
    snprintf(name, sizeof(name), T(S_NAME_FMT), pet.shiny ? "*" : "", base, pet.level());
    uint16_t nameCol = gNight ? UI_INK_NIGHT : d.accent;
    if (fxBan && fxBan->kind == FXB_LEVELUP) {  // subida de nivel: el nivel de la cabecera parpadea
      uint32_t e = fxq.elapsed(millis());
      if (e < 1200 && (e / 150) % 2 == 0) nameCol = UI_BAR_WARN;
    }
    if (fxHatchHidesPet(millis())) drawHeader(T(S_EGG_HDR), inkColor(), T(S_EGG_ALMOST));  // aun se rompe el huevo
    else drawHeader(name, nameCol, statusMsg());  // con la racha
    drawPet();
    drawSceneFront(homeBiome(), millis(), gNight);  // hierba delante de los pies
    drawBath();
    drawPoops();
    drawBoxAppear(millis());  // recien guardado o despertado (caja): sale de una luz
    // panel inferior: base limpia para barras y botones sobre el paisaje
    gfx->fillRect(0, 312, 466, 154, gNight ? UI_BG_NIGHT : UI_BG_DAY);
    drawBars();
    drawButtons();
    fxT0 = micros();
    drawFxOverlays(millis());  // "+N", corazon de vinculo, entrenamiento, "!", aro de la eclosion
    fxUs += micros() - fxT0;
    if (!fxBan) {  // con una celebracion encima, los botones grandes esperan a que acabe
      if (pet.wantEvolveButton()) drawEvolveButton();        // CTA rojo: evolucionar
      else if (pet.canRunawayNow()) drawRunawayButton();     // CTA sombrio: escapada (abandono)
      else if (pet.wantFarewellButton()) drawFarewellButton();  // CTA dorado: despedida
    }
  }

  if (pet.sleeping) {
    gfx->setTextColor(UI_INK_NIGHT);
    setSize(3);
    setCur(320, 130);
    printT("Zz");
  }

  // noticias del paseo (meta y hallazgos), por encima de todo lo de la pantalla
  // principal (tambien del boton de evolucion: un toque cierra antes el hallazgo)
  // y por debajo de los dialogos, que las aplazan (drawWalkNews mira si hay alguno)
  fxT0 = micros();
  drawFxBanner(millis());  // celebracion en curso (fx.h): subida, medalla, hito o meta
  fxPerfNote(fxUs + (micros() - fxT0));
  drawWalkNews();

  // menus emergentes: comida (4 opciones) o juego (pelota | combate | saco)
  if (menuKind && !timeLeft(menuUntil)) menuKind = MENU_NONE;
  if (menuKind == MENU_PLAY) {
    gfx->fillRoundRect(PLAY_MENU_X, 288, PLAY_MENU_W, 64, 14, UI_WHITE);
    gfx->drawRoundRect(PLAY_MENU_X, 288, PLAY_MENU_W, 64, 14, inkColor());
    drawMap(SPR_ICON_PLAY, 16, PLAY_MENU_X + 9, 296, 3, false);
    drawMap(SPR_ICON_BATTLE, 16, PLAY_MENU_X + PLAY_MENU_SLOT + 9, 296, 3, false);
    drawMap(SPR_ICON_SACK, 16, PLAY_MENU_X + 2 * PLAY_MENU_SLOT + 9, 296, 3, false);
  }
  if (menuKind == MENU_FEED) {
    {
      gfx->fillRoundRect(101, 288, 264, 64, 14, UI_WHITE);
      gfx->drawRoundRect(101, 288, 264, 64, 14, inkColor());
      drawMap(SPR_ICON_FOOD, 16, 110, 296, 3, false);
      drawMap(SPR_ICON_BERRY_B, 16, 176, 296, 3, false);
      drawMap(SPR_ICON_BERRY_G, 16, 242, 296, 3, false);
      drawMap(SPR_ICON_CANDY, 16, 308, 296, 3, false);
    }
  }

  // dialogo "soltar?" (pulsacion larga sobre el bicho)
  if (confirmUntil) {
    if (!timeLeft(confirmUntil)) {
      confirmUntil = 0;
    } else {
      gfx->fillRoundRect(94, 168, 278, 152, 16, UI_WHITE);
      gfx->drawRoundRect(94, 168, 278, 152, 16, UI_INK);
      // 48 y no 28: el buffer se dimensiono cuando toda cadena era de un byte
      // por caracter. Con una fila UTF-8, "%s 놓아줄까요?" mas el nombre pasa
      // de 28 y snprintf corta a mitad de una secuencia de 3 bytes, que la
      // fuente ya no sabe dibujar.
      char q[48];
      snprintf(q, sizeof(q), T(S_RELEASE_FMT), dexName(pet.speciesId));
      bool empty = !pet.eggHeld() && !pet.items[IT_EGG];  // v1.39: ningun huevo ocupara su sitio
      gfx->setTextColor(UI_INK);
      setSize(2);
      setCur(centerX(q, 2), empty ? 186 : 196);
      printT(q);
      if (empty) {
        gfx->setTextColor(BOX_NOTE_INK);
        printTight(T(S_NO_EGG_NOTE), CX, 216, 262);
        setSize(2);  // printTight puede dejar el tamano 1: los botones van a 2
      }
      gfx->fillRoundRect(118, 252, 100, 52, 12, UI_BAR_OK);
      gfx->setTextColor(UI_WHITE);
      setCur(118 + (100 - textW(T(S_YES), 2)) / 2, 270);
      printT(T(S_YES));
      gfx->fillRoundRect(248, 252, 100, 52, 12, UI_BAR_BAD);
      setCur(248 + (100 - textW(T(S_NO), 2)) / 2, 270);
      printT(T(S_NO));
    }
  }

  // dialogo de decision (evolucionar/mantener, despedirse/quedaros)
  if (choiceKind) {
    if (!timeLeft(choiceUntil)) choiceKind = 0;
    else drawChoiceDialog();
  }

  gfx->flush();
  return true;
}

// ---------- minijuego: toques con la pokeball ----------

void startGame() {
  if (pet.isEgg() || pet.sleeping || pet.ceremony) return;
  gameOpen = true;
  gameOverUntil = 0;
  gameScore = 0;
  gameMisses = 0;
  gameNewHi = false;
  gameNoSave = false;
  hitTime = 0;
  gamePetX = 233;
  lastGameStep = millis();
  respawnBall();
}

void respawnBall() {
  ballSpawn(ball, 150 + random(166), random(2) ? 1 : -1);
}

void gameTap(int16_t x, int16_t y) {
  if (gameOverUntil) return;
  if (y < 72) {  // tocar la cabecera = abandonar sin premio
    gameOpen = false;
    return;
  }
  float dx = ball.x - x, dy = ball.y - y;
  if (dx * dx + dy * dy < 74 * 74) {  // toque a la bola!
    gameScore++;
    sfxPlay(SFX_PLAY);
    ballHit(ball, dx);  // siempre el mismo impulso: lo que sube con los puntos es la velocidad
    hitX = ball.x;
    hitY = ball.y;
    hitTime = millis();
  }
}

void stepGame() {
  uint32_t now = millis();
  // pasos de 85 ms, el periodo de frame con el que se afinaron las constantes (ver el
  // planificador de render en loop()): sin esto la fisica avanzaba "un paso por frame" y el
  // juego iba mas rapido o mas lento segun lo que tardara en dibujarse el frame anterior, que
  // depende del tamano del sprite cargado. Por encima, el factor de velocidad de la
  // puntuacion (ballgame.h, v1.33): 1x con 1 punto, 2x con 80.
  float k = lastGameStep ? ballStepK(now - lastGameStep, gameScore) : ballSpeed(gameScore);
  lastGameStep = now;

  ballStep(ball, k);  // gravedad, movimiento y rebote en la pared circular
  if (ballOnFloor(ball)) {
    if (++gameMisses >= 3) {
      if (gameNoSave) {  // partida del comando BALL: ni record ni premio
        gameNewHi = false;
        sfxPlay(SFX_LEVEL);
      } else {
        gameNewHi = (gameScore > pet.gameHi);
        FxNeedSnap ns = fxSnap();
        pet.playResult(gameScore);  // actualiza el record y da felicidad
        fxNeeds(ns);  // los "+N" salen al volver a la pantalla principal
        sfxPlay(gameNewHi && gameScore > 0 ? SFX_MEDAL : SFX_LEVEL);
      }
      gameOverUntil = millis() + 4000;
    } else {
      respawnBall();
    }
  }
  // el bicho la sigue por abajo (a la velocidad del juego: k ya la lleva)
  float chase = (ball.x - gamePetX) * 0.12f;
  if (chase > 7) chase = 7;
  if (chase < -7) chase = -7;
  gamePetX += chase * k;
}

// ---------- saco de entrenamiento (entrena la fuerza) ----------

void startSack() {
  if (pet.isEgg() || pet.sleeping || pet.ceremony) return;
  sackOpen = true;
  sackUntil = millis() + 10000;
  sackOverUntil = 0;
  sackHits = 0;
  sackShake = 0;
  sackNewHi = false;
}

void sackTap() {
  if (!timeLeft(sackUntil)) return;  // ya termino el tiempo
  sackHits++;
  sackShake = 16;  // sacude el saco
}

void drawGameScene();  // prototipo (definida mas abajo)

void renderSack() {
  uint32_t now = millis();
  drawGameScene();  // fondo del habitat
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  uint16_t ink = night ? UI_INK_NIGHT : UI_INK;

  // pantalla de resultado
  if (sackOverUntil) {
    if (!timeLeft(sackOverUntil)) { sackOpen = false; return; }
    char b[20];
    snprintf(b, sizeof(b), T(S_HITS_FMT), sackHits);
    gfx->setTextColor(ink);
    setSize(4);
    setCur(centerX(b, 4), 150);
    printT(b);
    char g[18];
    snprintf(g, sizeof(g), T(S_STR_GAIN_FMT), sackGain);
    gfx->setTextColor(UI_BAR_BAD);
    setSize(3);
    setCur(centerX(g, 3), 210);
    printT(g);
    setSize(2);
    if (sackNewHi && sackHits > 0) {
      gfx->setTextColor(UI_BAR_WARN);
      setCur(centerX(T(S_NEW_RECORD), 2), 256);
      printT(T(S_NEW_RECORD));
    } else {
      char r[18];
      snprintf(r, sizeof(r), T(S_RECORD_FMT), pet.strHi);
      gfx->setTextColor(ink);
      setCur(centerX(r, 2), 256);
      printT(r);
    }
    gfx->flush();
    return;
  }

  // se acabaron los 10 s: aplicar entrenamiento
  if (!timeLeft(sackUntil)) {
    sackNewHi = (sackHits > pet.strHi);
    FxNeedSnap ns = fxSnap();
    sackGain = pet.trainStrength(sackHits);
    fxNeeds(ns);
    sfxPlay(sackNewHi ? SFX_MEDAL : SFX_PLAY);
    sackOverUntil = now + 3500;
    gfx->flush();
    return;
  }

  // aporreo activo
  sackShake *= 0.84f;
  int off = (int)(sackShake * sinf(now * 0.05f));
  int sx = CX + off, top = 86, sy = 150;
  gfx->fillRect(CX - 3, 56, 6, top - 56, ink);          // gancho/cuerda
  gfx->fillRect(sx - 4, top - 30, 8, 34, ink);          // cadena
  gfx->fillRoundRect(sx - 42, top, 84, 150, 26, C565(0xb5, 0x3a, 0x3a));  // saco
  gfx->fillRoundRect(sx - 42, top, 84, 22, 18, C565(0x7e, 0x28, 0x28));   // tapa
  gfx->drawRoundRect(sx - 42, top, 84, 150, 26, ink);
  gfx->fillRect(sx - 42, top + 70, 84, 4, C565(0x7e, 0x28, 0x28));        // costura

  // contador de golpes
  char buf[8];
  snprintf(buf, sizeof(buf), "%u", sackHits);
  gfx->setTextColor(ink);
  setSize(6);
  setCur(centerX(buf, 6), 268);
  printT(buf);

  setSize(2);
  setCur(centerX(T(S_HIT_FAST), 2), 322);
  printT(T(S_HIT_FAST));

  // barra de tiempo
  uint32_t left = sackUntil - now;
  int bw = 280, fw = (int)((uint32_t)bw * left / 10000);
  gfx->fillRoundRect(CX - bw / 2, 350, bw, 16, 5, UI_TRACK);
  if (fw > 2) gfx->fillRoundRect(CX - bw / 2, 350, fw, 16, 5, UI_BAR_OK);

  gfx->flush();
}

// fondo del minijuego: hatibat del bicho (cielo por hora + suelo del bioma)
void drawGameScene() {
  int hh = sceneHour();
  bool night = hh < 6 || hh >= 20;
  uint16_t top, bot;
  if (night)       { top = C565(0x0c, 0x12, 0x24); bot = C565(0x1e, 0x26, 0x46); }
  else if (hh < 8) { top = C565(0xd1, 0x6a, 0x86); bot = C565(0xf3, 0xb8, 0x7c); }
  else if (hh < 18){ top = C565(0x8f, 0xc8, 0xea); bot = C565(0xdc, 0xee, 0xe6); }
  else             { top = C565(0xc7, 0x5a, 0x4a); bot = C565(0xf0, 0xae, 0x64); }
  int hor = 376;
  for (int y = 0; y < hor; y += 8)
    gfx->fillRect(0, y, 466, 8, lerp565(top, bot, y, hor));
  if (night)
    for (auto &st : STARS) gfx->fillRect(st[0], st[1], 4, 4, UI_WHITE);
  uint8_t bio = pet.isEgg() ? 0 : DEX_TBL[pet.speciesId].biome;
  uint16_t soil = BIOME_SOIL[bio < BIOME_COUNT ? bio : 0];
  if (night) soil = lerp565(soil, C565(0x16, 0x1c, 0x30), 9, 16);
  gfx->fillRect(0, hor, 466, 466 - hor, soil);
}

void renderGame() {
  // sin fillScreen(NEGRO): drawGameScene cubre los 466x466 completos. Si el
  // DMA del flush anterior aun lee el buffer, vera contenido valido (no negro
  // a medio pintar), que era el parpadeo a 25 fps.
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  uint16_t ink = night ? UI_INK_NIGHT : UI_INK;

  if (gameOverUntil) {
    drawGameScene();
    if (!timeLeft(gameOverUntil)) {
      gameOpen = false;
      return;
    }
    char buf[22];
    snprintf(buf, sizeof(buf), T(S_SCORE_FMT), gameScore);
    gfx->setTextColor(ink);
    setSize(4);
    setCur(centerX(buf, 4), 160);
    printT(buf);
    setSize(2);
    if (gameNewHi && gameScore > 0) {
      gfx->setTextColor(UI_BAR_WARN);
      setCur(centerX(T(S_NEW_RECORD), 2), 214);
      printT(T(S_NEW_RECORD));
    } else {
      char rec[20];
      snprintf(rec, sizeof(rec), T(S_RECORD_FMT), pet.gameHi);
      gfx->setTextColor(ink);
      setCur(centerX(rec, 2), 214);
      printT(rec);
    }
    const char *msg = gameScore >= 10 ? T(S_GREAT_JOY) : T(S_PLUS_JOY);
    gfx->setTextColor(ink);
    setCur(centerX(msg, 2), 250);
    printT(msg);
    gfx->flush();
    return;
  }

  drawGameScene();
  stepGame();

  // marcador, record y vidas
  char buf[8];
  snprintf(buf, sizeof(buf), "%u", gameScore);
  gfx->setTextColor(ink);
  setSize(4);
  setCur(centerX(buf, 4), 30);
  printT(buf);
  // 24 y no 12: en japones "きろく %u" son 10 bytes antes de la cifra y el
  // record se quedaba en su primer digito ("きろく 2" con 219). Las tallas de
  // los buffers de texto las vigila TestBuffersDeTexto en test/test_tools.py.
  char rec[24];
  snprintf(rec, sizeof(rec), T(S_REC_FMT), pet.gameHi);
  setSize(2);
  setCur(centerX(rec, 2), 76);
  printT(rec);
  for (int i = 0; i < 3; i++) {
    if (i < 3 - gameMisses) gfx->fillCircle(180 + i * 28, 104, 6, UI_BAR_BAD);
    else gfx->drawCircle(180 + i * 28, 104, 6, UI_TRACK);
  }

  if (pmd.loaded) {
    uint8_t act = (ball.x > gamePetX + 4) ? PMD_WALKR : (ball.x < gamePetX - 4) ? PMD_WALKL : PMD_IDLE;
    if (!pmd.has(act)) act = PMD_IDLE;
    drawPmdAct(act, (int)gamePetX, 394, millis(), true, false, 3);
  } else if (mon.loaded) {
    int s = (mon.h * 2 > 130) ? 1 : 2;
    int w = mon.w * s, h = mon.h * s;
    uint16_t fm = mon.frameMs ? mon.frameMs : 100;
    uint16_t fi = (millis() / fm) % mon.frames;
    const uint8_t *fr = mon.data + (uint32_t)fi * mon.w * mon.h;
    int px = (int)gamePetX - w / 2, py = 394 - h;
    for (int r = 0; r < mon.h; r++) {
      const uint8_t *row = fr + r * mon.w;
      forRuns(mon.w, [&](int c) { return indexedKey(row[c], false); },
              [&](int c0, int len, int k) { gfx->fillRect(px + c0 * s, py + r * s, len * s, s, mon.pal[k]); });
    }
  }

  // anillo de impacto que se expande y desvanece (feedback suave del golpe)
  uint32_t ht = millis() - hitTime;
  if (hitTime && ht < 260) {
    int rad = 22 + (int)(ht / 6);
    gfx->drawCircle((int)hitX, (int)hitY, rad, C565(0xff, 0xe7, 0x9f));
    gfx->drawCircle((int)hitX, (int)hitY, rad - 2, C565(0xff, 0xd9, 0x8a));
  }

  // la pokeball
  drawMap(SPR_ICON_PLAY, 16, (int)ball.x - 24, (int)ball.y - 24, 3, false);

  gfx->flush();
}

// ---------- ficha del bicho (deslizar vertical) ----------

// x donde arrancan las barras de la ficha. Estaba fijo en 150, que daba de sobra
// para etiquetas latinas de 3 caracteres pero no para las japonesas, mas anchas:
// こうげき se metia dentro de la barra. Se calcula a partir de la etiqueta mas
// larga para que las cuatro barras sigan alineadas en cualquier idioma, con el
// valor original como suelo (asi en los idiomas latinos no cambia nada).
// Requiere tener ya puesto el tamano de texto 2, porque textW() lo necesita.
static int statBarX() {
  const StrId ids[] = { S_STAT_ATK, S_STAT_DEF, S_STAT_SPE, S_STAT_WGT, S_VIN };
  int ancho = 0;
  for (StrId id : ids) {
    int w = textW(T(id), 2);
    if (w > ancho) ancho = w;
  }
  int x = 96 + ancho + 12;   // 12 px de aire entre etiqueta y barra
  return x < 150 ? 150 : x;
}

void drawCardStat(int y, const char *label, uint16_t val, uint16_t maxBar, uint16_t color) {
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(96, y);
  printT(label);
  char num[8];
  snprintf(num, sizeof(num), "%u", val);
  setCur(330, y);
  printT(num);
  int bx = statBarX();
  int bw = 310 - bx;   // la barra siempre acaba en 310, dejando aire hasta el numero
  int fw = (int)val * bw / maxBar;
  if (fw > bw) fw = bw;
  gfx->fillRoundRect(bx, y + 2, bw, 11, 3, UI_TRACK);
  if (fw > 2) gfx->fillRoundRect(bx, y + 2, fw, 11, 3, color);
}

// ---------- ajuste de hora en pantalla (deslizar abajo) ----------
// El usuario pone su hora LOCAL a ojo; el firmware la usa tal cual, asi que
// no hay que gestionar zona horaria. Preserva el dia (no rompe racha/edad).

void openClock() {
  uint32_t e = pet.lastSeenEpoch ? pet.lastSeenEpoch : rtcEpoch();
  clockH = (e / 3600) % 24;
  clockM = (e / 60) % 60;
  clockOpen = true;
}

void applyClock() {
  uint32_t base = pet.lastSeenEpoch ? pet.lastSeenEpoch : rtcEpoch();
  uint32_t e = (base / 86400) * 86400 + (uint32_t)clockH * 3600 + (uint32_t)clockM * 60;
  rtcSetEpoch(e);
  pet.setClock(e);
  clockOpen = false;
}

void drawClockBtn(int x, int y, const char *l) {
  gfx->fillRoundRect(x, y, 58, 58, 12, UI_WHITE);
  gfx->drawRoundRect(x, y, 58, 58, 12, UI_INK);
  gfx->setTextColor(UI_INK);
  setSize(4);
  setCur(x + 17, y + 15);
  printT(l);
}

// pildoras de idioma centradas en y; rellena la activa
#define LANG_PILL_Y 296
#define LANG_PILL_H 30
#define LANG_PILL_X 336          // pildora de idioma (cicla LANG_COUNT al tocar)
#define LANG_PILL_W 96
static const char *const LANG_CODES[LANG_COUNT] = { "ES", "EN", "FR", "DE", "IT", "PT", "JA", "KO" };

// pildora de brillo, en el hueco entre la de sonido y la de idioma (v1.23)
#define BRI_PILL_X 146
#define BRI_PILL_W 174

// sol: disco y 8 rayos cortos (sin texto: igual en todos los idiomas)
void drawSunIcon(int x, int y, int r, uint16_t col) {
  gfx->fillCircle(x, y, r, col);
  int d = r + 4, e = d * 7 / 10;
  const int8_t dir[8][2] = { { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 }, { -1, 0 }, { -1, -1 }, { 0, -1 }, { 1, -1 } };
  for (auto &v : dir) {
    int k = (v[0] && v[1]) ? e : d;
    gfx->fillCircle(x + v[0] * k, y + v[1] * k, r > 6 ? 2 : 1, col);
  }
}

// nivel de brillo como BRIGHT_LEVELS puntos: rellenos hasta el nivel actual
void drawBrightDots(int x, int y, int r, int gap, uint16_t on, uint16_t off) {
  for (int i = 0; i < BRIGHT_LEVELS; i++) {
    int cx = x + i * gap;
    if (i <= gBright) gfx->fillCircle(cx, y, r, on);
    else gfx->drawCircle(cx, y, r, off);
  }
}

// aviso del nivel de brillo, encima de cualquier pantalla (RoundCanvas::overlay)
void drawBrightToast() {
  if (!timeLeft(briToastUntil)) return;
  const int w = 200, h = 56, x = CX - w / 2, y = 142;
  gfx->fillRoundRect(x, y, w, h, 16, UI_INK);
  gfx->drawRoundRect(x, y, w, h, 16, UI_WHITE);
  drawSunIcon(x + 34, y + h / 2, 9, UI_BAR_WARN);
  drawBrightDots(x + 78, y + h / 2, 8, 32, UI_BAR_WARN, 0x8410);
}

void renderClock() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  gfx->setTextColor(UI_INK);
  setSize(3);
  setCur(centerX(T(S_SET_TIME), 3), 44);
  printT(T(S_SET_TIME));

  char t[8];
  snprintf(t, sizeof(t), "%02d:%02d", clockH, clockM);
  setSize(7);
  setCur(CX - 105, 108);
  printT(t);

  drawClockBtn(104, 190, "-");  // hora -
  drawClockBtn(170, 190, "+");  // hora +
  drawClockBtn(252, 190, "-");  // min -
  drawClockBtn(318, 190, "+");  // min +
  setSize(2);
  gfx->setTextColor(UI_TRACK);
  setCur(120, 256);
  printT(T(S_HOUR));
  setCur(276, 256);
  printT(T(S_MIN));

  // interruptor de sonido (izquierda de la fila de idioma)
  bool snd = audioEnabled();
  const char *sl = snd ? T(S_SND_ON) : T(S_SND_OFF);
  gfx->fillRoundRect(34, LANG_PILL_Y, 96, LANG_PILL_H, 8, snd ? UI_BAR_OK : UI_WHITE);
  gfx->drawRoundRect(34, LANG_PILL_Y, 96, LANG_PILL_H, 8, UI_INK);
  gfx->setTextColor(snd ? UI_BG_DAY : UI_INK);
  setSize(2);
  setCur(34 + (96 - textW(sl, 2)) / 2, LANG_PILL_Y + 8);
  printT(sl);

  // brillo: sol y un punto por nivel; cicla al tocar, como la tecla BOOT
  gfx->fillRoundRect(BRI_PILL_X, LANG_PILL_Y, BRI_PILL_W, LANG_PILL_H, 8, UI_WHITE);
  gfx->drawRoundRect(BRI_PILL_X, LANG_PILL_Y, BRI_PILL_W, LANG_PILL_H, 8, UI_INK);
  drawSunIcon(BRI_PILL_X + 22, LANG_PILL_Y + LANG_PILL_H / 2, 5, UI_BAR_WARN);
  drawBrightDots(BRI_PILL_X + 56, LANG_PILL_Y + LANG_PILL_H / 2, 6, 32, UI_BAR_WARN, 0x9492);

  // selector de idioma: una pildora que cicla los 6 idiomas al tocar
  gfx->fillRoundRect(LANG_PILL_X, LANG_PILL_Y, LANG_PILL_W, LANG_PILL_H, 8, UI_WHITE);
  gfx->drawRoundRect(LANG_PILL_X, LANG_PILL_Y, LANG_PILL_W, LANG_PILL_H, 8, UI_INK);
  char lp[10];
  snprintf(lp, sizeof(lp), "%s >", LANG_CODES[gLang]);
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(LANG_PILL_X + (LANG_PILL_W - textW(lp, 2)) / 2, LANG_PILL_Y + 8);
  printT(lp);

  gfx->fillRoundRect(133, 340, 200, 48, 14, UI_BAR_OK);
  gfx->setTextColor(UI_BG_DAY);
  setSize(3);
  setCur(CX - 18, 352);
  printT("OK");

  gfx->setTextColor(UI_TRACK);
  setSize(2);
  setCur(centerX(T(S_CLOCK_CANCEL), 2), 410);
  printT(T(S_CLOCK_CANCEL));

  // version del firmware (discreta, abajo del todo)
  char ver[20];
  snprintf(ver, sizeof(ver), "TamaPoke v%s", FW_VERSION);
  setSize(1);
  setCur(centerX(ver, 1), 436);
  printT(ver);
  gfx->flush();
}

void clockTap(int16_t x, int16_t y) {
  if (y >= 190 && y <= 248) {  // fila de botones +/-
    if (x >= 104 && x < 162) clockH = (clockH + 23) % 24;
    else if (x >= 170 && x < 228) clockH = (clockH + 1) % 24;
    else if (x >= 252 && x < 310) clockM = (clockM + 59) % 60;
    else if (x >= 318 && x < 376) clockM = (clockM + 1) % 60;
    return;
  }
  if (y >= LANG_PILL_Y && y <= LANG_PILL_Y + LANG_PILL_H) {
    if (x >= 34 && x < 130) {                  // interruptor de sonido
      audioSetEnabled(!audioEnabled());
      if (audioEnabled()) sfxPlay(SFX_TAP);    // confirma al encender
      return;
    }
    if (x >= BRI_PILL_X && x < BRI_PILL_X + BRI_PILL_W) {  // cicla el brillo
      cycleBright();
      briToastUntil = 0;  // aqui la pildora ya lo muestra
      return;
    }
    if (x >= LANG_PILL_X && x < LANG_PILL_X + LANG_PILL_W) {  // cicla idioma
      setLang((Lang)((gLang + 1) % LANG_COUNT));
      applyLangFont();  // la fuente cambia con el idioma
      sfxPlay(SFX_TAP);
      return;
    }
  }
  if (y >= 340 && y <= 388 && x >= 133 && x <= 333) { applyClock(); return; }
}

// iconos de la mochila, en el orden de IT_* (pet.h)
static const char *const *const ITEM_ICON[IT_COUNT] = {
  SPR_ICON_GOLDEN, SPR_ICON_PROTEIN, SPR_ICON_IRON, SPR_ICON_CARBOS, SPR_ICON_CHARM, SPR_ICON_RARECANDY, SPR_ICON_EGG,
};

// llama + numero de racha arriba a la izquierda
// insignia de racha (llama + dias). Ancho en px, 0 si no hay racha.
int streakBadgeW() {
  if (pet.streak < 1) return 0;
  char s[6];
  snprintf(s, sizeof(s), "%u", pet.streak);
  setSize(2);
  return 22 + textW(s, 2);
}

void drawStreakBadge(int x, int y) {
  gfx->fillTriangle(x + 8, y, x + 1, y + 17, x + 15, y + 17, UI_BAR_BAD);
  gfx->fillTriangle(x + 8, y + 7, x + 4, y + 17, x + 12, y + 17, UI_BAR_WARN);
  char s[6];
  snprintf(s, sizeof(s), "%u", pet.streak);
  gfx->setTextColor(inkColor());
  setSize(2);
  setCur(x + 22, y + 2);
  printT(s);
}

// ---------- efectos de respuesta (fx.h, v1.30) ----------

// los vigilantes apuntan los valores de ahora (sin evento): al arrancar, antes del tiempo
// apagado, y en el renderizador del PC entre escenas
void fxArmWatches() {
  if (pet.isEgg()) fxLevelW.disarm();
  else fxLevelW.reset(pet.level());
  fxBondW.reset(pet.bond);
  fxMistakeW.reset(pet.careMistakes);
  fxTrainW[0].reset(pet.trAtk);
  fxTrainW[1].reset(pet.trDef);
  fxTrainW[2].reset(pet.trSpe);
  fxWasEgg = pet.isEgg();
}

// alguien puede estar mirando: pantalla encendida, sin atenuar y sin salvapantallas
bool fxScreenAwake() { return !screenOff && dimStage == DIM_AWAKE && !saverOn(); }

// En cada vuelta del loop (tambien con la pantalla apagada, que no dibuja): recoge lo que el
// bicho dejo pendiente y lo que vigilan los vigilantes. La subida de nivel suena AL MOMENTO,
// tambien a oscuras, sin encender la pantalla (decidido el 2026-11-05); su animacion espera.
void fxCollect(uint32_t now) {
  bool awake = fxScreenAwake();
  if (fxWasEgg && !pet.isEgg()) {  // eclosion: con la pantalla a la vista, animacion y sonido en su destello
    if (awake) fxo.push(FXO_HATCH, 0, 0, now);
    else sfxPlay(SFX_HATCH);
  }
  fxWasEgg = pet.isEgg();
  if (pet.isEgg() || pet.ceremony) {  // ni nivel ni vinculo que vigilar
    fxLevelW.disarm();
    fxBondW.disarm();
    fxMistakeW.disarm();
    for (auto &w : fxTrainW) w.disarm();
  } else {
    uint32_t from = 0;
    if (fxLevelW.rose(pet.level(), &from)) {
      bool chime = !pet.sleeping;  // dormido no suena (como todo): sonara al verse, cuando despierte
      if (chime) sfxPlay(SFX_LEVELUP);
      fxq.push(FXB_LEVELUP, (uint16_t)from, pet.level(), chime);
    }
    if (fxBondW.rose(pet.bond, &from) && awake && fxBondGate.allow(now, FX_BOND_GAP_MS))
      fxo.push(FXO_BOND, 0, (int16_t)(pet.bond - from), now);
    if (fxMistakeW.rose(pet.careMistakes) && awake && !pet.sleeping) {
      fxo.push(FXO_OOPS, 0, 0, now);
      sfxPlay(SFX_OOPS);
    }
    const uint8_t tr[3] = { pet.trAtk, pet.trDef, pet.trSpe };
    for (uint8_t i = 0; i < 3; i++)
      if (fxTrainW[i].rose(tr[i], &from)) fxo.push(FXO_TRAIN, i, (int16_t)(tr[i] - from), now);
  }
  uint16_t m = pet.takeMedal();
  for (uint8_t i = 0; i < MED_COUNT; i++)
    if (m & (1 << i)) fxq.push(FXB_MEDAL, i);
  uint16_t days = pet.takeMilestone();
  if (days) fxq.push(FXB_MILESTONE, days);
  uint8_t goalLv = 0;
  uint8_t tier = pet.takeGoalBanner(&goalLv);
  if (tier) fxq.push(FXB_GOAL, tier, goalLv);  // b = niveles que dieron (v1.36): "+N Lv"
  fxo.step(now, false);  // caducan los que no llegan a verse
}

// la pantalla principal no puede empezar una celebracion: nadie la ve, el bicho duerme o hay
// algo encima (dialogo, menu, bano, evolucion, despedida, eclosion)
bool fxScreenBusy() {
  return !fxScreenAwake() || pet.sleeping || pet.ceremony || pet.evolving() || choiceKind || confirmUntil ||
         menuKind || bathUntil || fxo.find(FXO_HATCH) || fxo.waiting(FXO_HATCH) || fxo.find(FXO_BOX) ||
         fxo.waiting(FXO_BOX);
}

// pose de un gesto del bicho (PMD): una vez, sosteniendo el ultimo frame hasta minMs
void fxPose(uint8_t act, uint32_t now, uint32_t minMs) {
  if (!pmd.has(act)) return;
  uint32_t ms = pmdActTotalMs(pmd.acts[act]);
  beh.mode = 2;
  beh.act = act;
  beh.t0 = now;
  beh.until = now + (ms < minMs ? minMs : ms);
}

// cada frame de la pantalla principal, antes de dibujar: avanza la cola y los efectos
void fxFrame(uint32_t now) {
  fxo.step(now, fxScreenAwake());
  bool started = false;
  fxBan = fxq.step(now, fxScreenBusy(), &started);
  if (!started || !fxBan) return;
  if (fxBan->kind == FXB_LEVELUP) {
    if (!fxBan->chimed) sfxPlay(SFX_LEVELUP);
    for (uint8_t w = 0; w < 3; w++)
      fxGain[w] = (int16_t)pet.statAt(w, fxBan->b) - (int16_t)pet.statAt(w, fxBan->a);
    // la pose de alegria; el salto (Hop) solo si no tiene: sube tanto que pisa la cabecera
    fxPose(pmd.has(PMD_POSE) ? PMD_POSE : PMD_HOP, now, 1200);
  } else {
    sfxPlay(SFX_MEDAL);
  }
}

// comando serie FX <efecto>: lanza cada efecto para mirarlo en la placa. Solo se ve: no cambia
// el nivel ni el guardado (para una subida de verdad, LVL <n>)
bool fxDebug(const String &k) {
  uint32_t now = millis();
  uint16_t lv = pet.level();
  if (k == "levelup") fxq.push(FXB_LEVELUP, lv, lv + 1, false);
  else if (k == "levelup6") fxq.push(FXB_LEVELUP, lv, lv + 6, false);
  else if (k == "medal") fxq.push(FXB_MEDAL, 0);
  else if (k == "streak") fxq.push(FXB_MILESTONE, 7);
  else if (k == "goal") fxq.push(FXB_GOAL, 1, STEP_GOAL_LEVELS[0]);
  else if (k == "goal3") fxq.push(FXB_GOAL, STEP_GOAL_TIERS, STEP_GOAL_LEVELS[STEP_GOAL_TIERS - 1]);
  else if (k == "stat") {
    fxo.push(FXO_STAT, 0, 25, now);
    fxo.push(FXO_STAT, 1, 10, now);
    fxo.push(FXO_STAT, 2, -12, now);
  } else if (k == "bond") fxo.push(FXO_BOND, 0, 2, now);
  else if (k == "train") fxo.push(FXO_TRAIN, 0, 3, now);
  else if (k == "deny") {
    fxo.push(FXO_DENY, 0, 0, now);
    sfxPlay(SFX_DENY);
  } else if (k == "press") fxo.push(FXO_PRESS, 1, 0, now);
  else if (k == "oops") {
    fxo.push(FXO_OOPS, 0, 0, now);
    sfxPlay(SFX_OOPS);
  } else if (k == "hatch" && !pet.isEgg()) fxo.push(FXO_HATCH, 0, 0, now);
  else if (k == "box") fxo.push(FXO_BOX, 0, 0, now);  // el activo sale de una luz (caja, v1.37)
  else return false;
  wakeUI();
  return true;
}

void fxPerfNote(uint32_t us) {
  gFxUs = us;
  if (us > gFxMaxUs) gFxMaxUs = us;
}

// cabeza del bicho en pantalla, aproximada (donde van el "!", el entrenamiento y la pildora)
int fxHeadY() {
  if (!pmd.loaded) return PET_CY - 60;
  int head = PET_GROUND - pmdIdleHeight() + 24;
  return head < 130 ? 130 : head;
}

// desplazamiento de un banner que entra desde arriba (0 al llegar a su sitio)
int fxSlide(uint32_t e) {
  if (e >= FX_BANNER_IN_MS) return 0;
  float k = 1.0f - (float)e / FX_BANNER_IN_MS;
  return -(int)(k * k * 240);
}

// numero "+N"/"-N" en una pastilla blanca, centrado en cx
void fxNumber(int cx, int y, int v, uint16_t col) {
  char s[8];
  snprintf(s, sizeof(s), v > 0 ? "+%d" : "%d", v);
  setSize(2);
  int w = textW(s, 2);
  gfx->fillRoundRect(cx - w / 2 - 5, y - 3, w + 10, 22, 7, UI_WHITE);
  gfx->drawRoundRect(cx - w / 2 - 5, y - 3, w + 10, 22, 7, UI_INK);
  gfx->setTextColor(col);
  setCur(cx - w / 2, y);
  printT(s);
}

// confeti que cae por delante del banner (medallas y la meta de 10000)
void drawConfetti(uint32_t e) {
  if (e >= FX_CONFETTI_MS) return;
  static const uint16_t COL[5] = { UI_BAR_WARN, UI_BAR_BAD, UI_BAR_OK, 0x4C98, UI_WHITE };
  for (int i = 0; i < FX_CONFETTI; i++) {
    uint32_t h = fxHash((uint32_t)i * 7919u + 13u);
    int v = 150 + (int)((h >> 9) % 170);  // px/s
    int y = 100 - (int)((h >> 17) % 90) + (int)(v * e / 1000);
    if (y > 300) continue;  // no cae sobre las barras
    int x = 76 + (int)(h % 314) + (int)(10 * sinf(e * 0.008f + i));
    bool flat = (((e / 90) + i) & 1) != 0;  // gira: alterna ancho y alto
    gfx->fillRect(x, y, flat ? 8 : 3, flat ? 3 : 8, COL[i % 5]);
  }
}

// subida de nivel: chispas que suben alrededor del bicho y la pildora "LEVEL UP!" con el
// nivel; en el ultimo tramo, lo que gano en ATK/DEF/VEL
void drawLevelUpFx(const FxEvent &ev, uint32_t e) {
  int px = (int)beh.x;
  if (e < 2000) {
    for (int i = 0; i < FX_SPARKS; i++) {
      uint32_t h = fxHash((uint32_t)i * 2654435761u + 7u);
      uint32_t ph = (e + h % 700) % 700;  // cada chispa sube en 0,7 s y vuelve a empezar
      int side = (i & 1) ? 1 : -1;
      int x = px + side * (44 + (int)((h >> 8) % 64));
      int y = PET_GROUND - 12 - (int)(ph * 190 / 700);
      uint16_t c = (i % 3) ? C565(0xff, 0xdc, 0x4a) : UI_WHITE;
      gfx->fillRect(x - 1, y - 5, 3, 11, c);
      gfx->fillRect(x - 5, y - 1, 11, 3, c);
    }
  }
  int w = 320, h = 74, x = CX - w / 2, y = 112 + fxSlide(e);
  gfx->fillRoundRect(x, y, w, h, 18, UI_BAR_WARN);
  gfx->drawRoundRect(x, y, w, h, 18, UI_INK);
  gfx->drawRoundRect(x + 2, y + 2, w - 4, h - 4, 16, UI_WHITE);
  const char *t1 = T(S_LEVEL_UP);
  gfx->setTextColor(UI_INK);
  setSize(3);
  setCur(centerX(t1, 3), y + 12);
  printT(t1);
  char l2[80];
  if (e < FX_LEVELUP_MS) {
    char lvNew[16];
    snprintf(lvNew, sizeof(lvNew), T(S_LVL_FMT), (unsigned)ev.b);
    if (ev.b > ev.a + 1) {  // varios de golpe (apagada, Caramelo Raro): "Lv.12 > Lv.18"
      char lvOld[16];
      snprintf(lvOld, sizeof(lvOld), T(S_LVL_FMT), (unsigned)ev.a);
      snprintf(l2, sizeof(l2), "%s > %s", lvOld, lvNew);
    } else {
      snprintf(l2, sizeof(l2), "%s", lvNew);
    }
  } else {
    snprintf(l2, sizeof(l2), "%s +%d  %s +%d  %s +%d", T(S_STAT_ATK), fxGain[0], T(S_STAT_DEF), fxGain[1],
             T(S_STAT_SPE), fxGain[2]);
  }
  setSize(2);
  setCur(centerX(l2, 2), y + 46);
  printT(l2);
}

// la celebracion en pantalla (la eligio fxFrame en este frame)
void drawFxBanner(uint32_t now) {
  if (!fxBan) return;
  const FxEvent &ev = *fxBan;
  uint32_t e = fxq.elapsed(now);
  if (ev.kind == FXB_LEVELUP) {
    drawLevelUpFx(ev, e);
    return;
  }
  int oy = fxSlide(e);
  if (ev.kind == FXB_GOAL) {
    uint8_t tier = ev.a ? (uint8_t)ev.a : 1;
    char l2[32];
    snprintf(l2, sizeof(l2), T(S_STEPS_FMT), (unsigned long)STEP_GOALS[tier - 1]);
    // niveles que dio la meta (v1.36): "+1 Lv" / "+2 Lv"; nada si se cruzo dormido
    char lvl[12] = "";
    if (ev.b) snprintf(lvl, sizeof(lvl), "+%u Lv", (unsigned)ev.b);
    if (tier == STEP_GOAL_TIERS) {  // 10000: banner mayor, con una bandera por meta y "+2 Lv"
      gfx->fillRoundRect(53, 128 + oy, 360, 140, 18, UI_BAR_WARN);
      gfx->drawRoundRect(53, 128 + oy, 360, 140, 18, UI_INK);
      gfx->drawRoundRect(56, 131 + oy, 354, 134, 16, UI_INK);
      for (int k = 0; k < STEP_GOAL_TIERS; k++) drawMap(SPR_ICON_FLAG, 16, CX - 44 + k * 30 - 8, 142 + oy, 2, false);
      gfx->setTextColor(UI_INK);
      setSize(3);
      setCur(centerX(T(S_GOAL_DONE), 3), 180 + oy);
      printT(T(S_GOAL_DONE));
      setSize(2);
      setCur(centerX(l2, 2), 212 + oy);
      printT(l2);
      if (lvl[0]) {
        setSize(3);
        setCur(centerX(lvl, 3), 236 + oy);
        printT(lvl);
      }
      drawConfetti(e);
      return;
    }
    int bh = lvl[0] ? 120 : 96;  // con "+N Lv", una linea mas
    gfx->fillRoundRect(73, 150 + oy, 320, bh, 16, UI_BAR_WARN);
    gfx->drawRoundRect(73, 150 + oy, 320, bh, 16, UI_INK);
    drawMap(SPR_ICON_FLAG, 16, 88, 166 + oy, 2, false);
    gfx->setTextColor(UI_INK);
    setSize(3);
    setCur(centerX(T(S_GOAL_DONE), 3), 176 + oy);
    printT(T(S_GOAL_DONE));
    setSize(2);
    setCur(centerX(l2, 2), 212 + oy);
    printT(l2);
    if (lvl[0]) {
      setCur(centerX(lvl, 2), 238 + oy);
      printT(lvl);
    }
    return;
  }
  // medalla nueva o hito de racha
  const char *l1, *l2;
  // 32 y no 20: "%u にちれんぞく！" ya pasa de 20 con una sola cifra, y snprintf
  // cortaba a mitad de un caracter de 3 bytes.
  char buf[32];
  if (ev.kind == FXB_MEDAL) {
    l1 = T(S_MEDAL_BANNER);
    l2 = medalName(ev.a);
  } else {
    snprintf(buf, sizeof(buf), T(S_STREAK_DAYS_FMT), (unsigned)ev.a);
    l1 = T(S_GREAT);
    l2 = buf;
  }
  gfx->fillRoundRect(73, 150 + oy, 320, 96, 16, UI_BAR_WARN);
  gfx->drawRoundRect(73, 150 + oy, 320, 96, 16, UI_INK);
  gfx->setTextColor(UI_INK);
  setSize(3);
  setCur(centerX(l1, 3), 176 + oy);
  printT(l1);
  setSize(2);
  setCur(centerX(l2, 2), 212 + oy);
  printT(l2);
  if (ev.kind == FXB_MEDAL) drawConfetti(e);
}

// caja de la barra i (0 comida, 1 felicidad, 2 energia, 3 higiene), como la pinta drawBar
void fxBarBox(int i, int *bx, int *by, int *bw) {
  setSize(2);  // barLabelGap mide las etiquetas a tamano 2
  int gap = barLabelGap();
  int w = 232 - (78 + gap);
  if (w > 100) w = 100;
  *bx = (i % 2 ? 244 : 78) + gap;
  *by = i < 2 ? 318 : 346;
  *bw = w;
}

// necesidades antes y despues de una accion: los cambios de FX_STAT_MIN o mas sacan su "+N"
FxNeedSnap fxSnap() { return FxNeedSnap{ { pet.fullness, pet.joy, pet.energy, pet.hygiene } }; }
void fxNeeds(const FxNeedSnap &before) {
  const uint8_t v[4] = { pet.fullness, pet.joy, pet.energy, pet.hygiene };
  uint32_t now = millis();
  for (uint8_t i = 0; i < 4; i++) {
    int d = (int)v[i] - (int)before.v[i];
    if (d >= FX_STAT_MIN || d <= -FX_STAT_MIN) fxo.push(FXO_STAT, i, (int16_t)d, now);
  }
}

// eclosion: mientras se rompe el huevo (las primeras tres cuartas partes) el bicho aun no se ve
bool fxHatchHidesPet(uint32_t now) {
  for (auto &x : fxo.o)
    if (x.on && x.kind == FXO_HATCH && (!x.started || FxOverlays::t(x, now) < 0.75f)) return true;
  return false;
}

// el huevo de la pantalla principal (y el de la eclosion), desplazado dx, con sus grietas
void drawEggSprite(int dx, uint8_t cracks) {
  int s = 5, x = CX - 16 * s + dx, y = PET_CY - 16 * s;
  drawMap(SPR_EGG, SPRITE_H, x, y, s, false);
  if (cracks >= 1)
    for (auto &c : CRACK1) gfx->fillRect(x + c[0] * s, y + c[1] * s, s, s, INK_K);
  if (cracks >= 2)
    for (auto &c : CRACK2) gfx->fillRect(x + c[0] * s, y + c[1] * s, s, s, INK_K);
}

// eclosion (FX_HATCH_MS): el huevo tiembla cada vez mas, salta la cascara con un destello
// blanco y aparece el bicho haciendo su pose. true = esta parte ocupa el sitio del bicho.
bool drawHatchFx(uint32_t now) {
  const FxOverlay *h = fxo.find(FXO_HATCH);
  if (!h) return false;
  float t = FxOverlays::t(*h, now);
  static uint32_t soundFor = 0, poseFor = 0;
  if (t < 0.55f) {
    float k = t / 0.55f;
    drawEggSprite((int)(k * 10 * sinf(now * 0.06f)), 2);
    return true;
  }
  if (soundFor != h->start) {
    soundFor = h->start;
    sfxPlay(SFX_HATCH);
  }
  if (t < 0.75f) {
    float k = (t - 0.55f) / 0.2f;
    gfx->fillCircle(CX, PET_CY, 40 + (int)(k * 110), UI_WHITE);
    const uint16_t shell = C565(0xf6, 0xea, 0xc2);
    for (int i = 0; i < 10; i++) {  // trozos de cascara volando (y cayendo), girando
      float a = i * 0.628f + 0.3f;
      int d = 120 + (int)(k * 90);
      int sx = CX + (int)(cosf(a) * d), sy = PET_CY + (int)(sinf(a) * d * 0.7f) + (int)(k * k * 70);
      float r = a + k * 5.0f;
      int x1 = sx + (int)(cosf(r) * 12), y1 = sy + (int)(sinf(r) * 12);
      int x2 = sx + (int)(cosf(r + 2.3f) * 10), y2 = sy + (int)(sinf(r + 2.3f) * 10);
      int x3 = sx + (int)(cosf(r + 4.1f) * 8), y3 = sy + (int)(sinf(r + 4.1f) * 8);
      gfx->fillTriangle(x1, y1, x2, y2, x3, y3, shell);
      gfx->drawTriangle(x1, y1, x2, y2, x3, y3, INK_K);
    }
    return true;
  }
  if (poseFor != h->start) {
    poseFor = h->start;
    fxPose(PMD_POSE, now, 0);
  }
  return false;
}

// efectos pequenos de la pantalla principal (los botones los pinta drawButtons)
void drawFxOverlays(uint32_t now) {
  int head = fxHeadY();
  for (auto &x : fxo.o) {
    if (!x.on || !x.started) continue;
    float t = FxOverlays::t(x, now);
    if (x.kind == FXO_STAT) {  // "+25" que sube desde su barra
      int bx, by, bw;
      fxBarBox(x.a, &bx, &by, &bw);
      fxNumber(bx + bw / 2, by - 6 - (int)(t * 26), x.v, x.v > 0 ? UI_BAR_OK : UI_BAR_BAD);
    } else if (x.kind == FXO_BOND) {  // corazon pequeno con "+N", a la izquierda del bicho
      int cx = (int)beh.x - 78, y = PET_GROUND - 150 - (int)(t * 40);
      drawMap(SPR_HEART, 32, cx - 34, y, 1, false);
      fxNumber(cx + 18, y + 6, x.v, UI_BAR_BAD);
    } else if (x.kind == FXO_TRAIN) {  // "ATK +3" sobre la cabeza, y un ataque
      static uint32_t poseFor = 0;
      if (poseFor != x.start) {
        poseFor = x.start;
        fxPose(PMD_ATTACK, now, 0);
      }
      char s[24];
      snprintf(s, sizeof(s), "%s +%d", T((StrId)(S_STAT_ATK + x.a)), x.v);
      setSize(2);
      int w = textW(s, 2), y = head - 44 - x.a * 30 - (int)(t * 16);
      gfx->fillRoundRect(CX - w / 2 - 8, y - 4, w + 16, 24, 8, UI_BAR_OK);
      gfx->drawRoundRect(CX - w / 2 - 8, y - 4, w + 16, 24, 8, UI_INK);
      gfx->setTextColor(UI_WHITE);
      setCur(CX - w / 2, y);
      printT(s);
    } else if (x.kind == FXO_OOPS) {  // "!" en un bocadillo junto a la cabeza
      int bx = (int)beh.x + 58, by = head - 40 + (int)(3 * sinf(now * 0.012f));
      gfx->fillTriangle(bx - 14, by + 10, bx - 4, by + 16, bx - 22, by + 26, UI_WHITE);
      gfx->fillCircle(bx, by, 19, UI_WHITE);
      gfx->drawCircle(bx, by, 19, UI_INK);
      gfx->setTextColor(UI_BAR_BAD);
      setSize(3);
      setCur(bx - 7, by - 11);
      printT("!");
    } else if (x.kind == FXO_HATCH && t >= 0.75f) {  // aro de luz que se abre al nacer
      float k = (t - 0.75f) / 0.25f;
      for (int r = 0; r < 3; r++) gfx->drawCircle(CX, PET_CY, 120 + (int)(k * 110) + r * 3, UI_WHITE);
    }
  }
}

// Noticias del paseo sobre la pantalla principal: los hallazgos (hasta que se toca). Esperan a
// que la pantalla este encendida y no haya otro dialogo, celebracion o animacion encima
// (FR-1.5). El banner de la meta va en la cola de celebraciones (fx.h) desde la v1.30.
void drawWalkNews() {
  bool busy = screenOff || dimStage != DIM_AWAKE || choiceKind || confirmUntil || menuKind ||
              pet.evolving() || bathUntil || fxq.busy();
  static bool wasShown = false;
  if (busy || !pet.hasFound()) { wasShown = false; return; }
  bool full = false;
  uint8_t it = pet.peekFound(&full);
  if (!wasShown) sfxPlay(SFX_MEDAL);  // suena una vez por hallazgo
  wasShown = true;
  foundShown = true;
  int by = 138, bh = full ? 140 : 124;
  gfx->fillRoundRect(73, by, 320, bh, 16, UI_BAR_WARN);
  gfx->drawRoundRect(73, by, 320, bh, 16, UI_INK);
  gfx->setTextColor(UI_INK);
  setSize(3);
  setCur(centerX(T(S_FOUND), 3), by + 14);
  printT(T(S_FOUND));
  drawMap(ITEM_ICON[it], 16, CX - 24, by + 44, 3, false);
  const char *nm = T((StrId)(S_IT_GOLDEN + it));
  setSize(2);
  setCur(centerX(nm, 2), by + 98);
  printT(nm);
  if (full) {
    gfx->setTextColor(UI_BAR_BAD);
    setCur(centerX(T(S_BACKPACK_FULL), 2), by + 118);
    printT(T(S_BACKPACK_FULL));
  }
}

// medallas en la ficha: badge con etiqueta, color si conseguida
void drawMedalBadge(int x, int y, int i) {
  bool got = pet.hasMedal(1 << i);
  gfx->fillRoundRect(x, y, 100, 24, 6, got ? UI_BAR_OK : UI_TRACK);
  if (!got) gfx->drawRoundRect(x, y, 100, 24, 6, UI_TRACK);
  gfx->setTextColor(got ? UI_BG_DAY : 0x9492);
  setSize(2);
  setCur(x + (100 - textW(medalLabel(i), 2)) / 2, y + 5);
  printT(medalLabel(i));
}

// pagina 0: perfil (retrato grande, identidad, racha, vinculo, baya)
void renderCardProfile() {
  const DexEntry &d = DEX_TBL[pet.speciesId];
  const char *nm = pet.nick[0] ? pet.nick : dexName(pet.speciesId);
  char head[32];
  snprintf(head, sizeof(head), T(S_NAME_FMT), pet.shiny ? "*" : "", nm, pet.level());
  gfx->setTextColor(d.accent);
  // auto-encoge: a tamano 3 los nombres largos no caben en la franja estrecha de
  // arriba de la pantalla redonda, asi que se cortaban por el borde. El ancho se
  // MIDE con textW(): strlen() cuenta bytes, y en una fila UTF-8 no hay un byte
  // por caracter ni todos miden lo mismo. Con la fuente clasica textW() devuelve
  // strlen()*6*size, o sea exactamente la cuenta anterior: no mueve un pixel en
  // los seis idiomas latinos (11 caracteres a tamano 3 son esos 198 px).
  setSize(3);
  int hts = (textW(head, 3) <= 198) ? 3 : 2;
  setSize(hts);
  setCur(centerX(head, hts), hts == 3 ? 34 : 40);
  printT(head);
  if (pet.nick[0]) {  // especie real bajo el apodo
    const char *sp = dexName(pet.speciesId);
    char par[32];
    snprintf(par, sizeof(par), "(%s)", sp);
    gfx->setTextColor(UI_TRACK);
    setSize(2);
    setCur(centerX(par, 2), 64);   // medido, no contado en bytes
    printT(par);
  }

  // retrato grande animado
  if (pmd.loaded) drawPmdAct(PMD_IDLE, CX, 206, millis(), true, false, 4);

  // racha con llama
  int sx = 138, sy = 224;
  gfx->fillTriangle(sx + 8, sy, sx + 1, sy + 18, sx + 15, sy + 18, UI_BAR_BAD);
  gfx->fillTriangle(sx + 8, sy + 7, sx + 4, sy + 18, sx + 12, sy + 18, UI_BAR_WARN);
  char rl[40];  // en japones pierde cifras con 30 a partir de 100 dias
  snprintf(rl, sizeof(rl), T(S_STREAK_FMT), pet.streak, pet.bestStreak);
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(sx + 24, sy + 2);
  printT(rl);

  drawCardStat(258, T(S_VIN), pet.bond, 100, C565(0xd4, 0x52, 0x7e));

  const char *berry = !pet.berryKnown ? T(S_BERRY_UNK)
                      : pet.lovesBerry(0) ? T(S_BERRY_RED)
                      : pet.lovesBerry(1) ? T(S_BERRY_BLUE)
                                          : T(S_BERRY_GREEN);
  char info[48];
  snprintf(info, sizeof(info), T(S_INFO_FMT), berry,
           (unsigned long)(pet.ageMinutes / 1440));
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(centerX(info, 2), 296);
  printT(info);

  gfx->setTextColor(UI_TRACK);
  setCur(centerX(T(S_RENAME_HINT), 2), 332);
  printT(T(S_RENAME_HINT));
}

// pagina 1: combate (4 barras). El saco de entrenamiento ya no sale de aqui sino del menu de
// JUGAR (v1.34), con los otros juegos: las barras ocupan el sitio del boton.
void renderCardStats() {
  gfx->setTextColor(UI_INK);
  setSize(3);
  setCur(centerX(T(S_BATTLE), 3), 48);
  printT(T(S_BATTLE));

  drawCardStat(124, T(S_STAT_ATK), pet.atkStat(), 260, UI_BAR_BAD);
  drawCardStat(180, T(S_STAT_DEF), pet.defStat(), 260, 0x4C98);
  drawCardStat(236, T(S_STAT_SPE), pet.speStat(), 260, UI_BAR_WARN);
  drawCardStat(292, T(S_STAT_WGT), pet.weight, 100, 0xB3C8);
}

// pagina 2: medallas con etiqueta descriptiva
void renderCardMedals() {
  int got = 0;
  for (int i = 0; i < MED_COUNT; i++)
    if (pet.hasMedal(1 << i)) got++;
  char head[24];
  snprintf(head, sizeof(head), T(S_MEDALS_FMT), got, MED_COUNT);
  gfx->setTextColor(UI_INK);
  setSize(3);
  setCur(centerX(head, 3), 48);
  printT(head);

  for (int i = 0; i < MED_COUNT; i++) {
    int x = 28 + (i % 2) * 206, y = 104 + (i / 2) * 54;
    if (i == MED_COUNT - 1 && MED_COUNT % 2) x = CX - 98;  // la impar (MARATON), centrada abajo
    bool g = pet.hasMedal(1 << i);
    gfx->fillRoundRect(x, y, 196, 44, 10, g ? UI_BAR_OK : UI_TRACK);
    if (g) {  // marca de conseguida
      gfx->fillCircle(x + 22, y + 22, 11, UI_BG_DAY);
      gfx->setTextColor(UI_BAR_OK);
      setSize(2);
      setCur(x + 16, y + 13);
      printT("v");
    }
    gfx->setTextColor(g ? UI_BG_DAY : 0x8410);
    setSize(2);
    setCur(x + 44, y + 14);
    printT(medalDesc(i));
  }
}

// pagina 3: progreso (nivel, evolucion, descuidos) — saca a la luz mecanicas
// que antes eran invisibles (cuanto falta para subir/evolucionar y por que)
void renderCardProgress() {
  const DexEntry &d = DEX_TBL[pet.speciesId];
  gfx->setTextColor(UI_INK);
  setSize(3);
  setCur(centerX(T(S_PROGRESS), 3), 44);
  printT(T(S_PROGRESS));

  // nivel grande
  char lv[10];
  snprintf(lv, sizeof(lv), T(S_LVL_FMT), pet.level());
  setSize(5);
  setCur(centerX(lv, 5), 86);
  printT(lv);

  // barra de progreso al siguiente nivel (v1.35: cada nivel cuesta lo suyo, 20 a 180 min,
  // growth.h). En el tope (999) la barra queda llena y sin "siguiente nivel".
  bool lvMax = pet.level() >= LEVEL_MAX;
  uint16_t into = pet.levelMinutesInto(), cost = pet.levelMinutesCost();
  int bx = 93, bw = 280, by = 158, bh = 22;
  gfx->fillRoundRect(bx, by, bw, bh, 6, UI_TRACK);
  int fw = lvMax ? bw - 4 : (int)((uint32_t)(bw - 4) * into / cost);
  if (fw > 0) gfx->fillRoundRect(bx + 2, by + 2, fw, bh - 4, 5, UI_BAR_OK);
  if (!lvMax) {
    char nx[32];
    snprintf(nx, sizeof(nx), T(S_NEXT_LVL_FMT), (unsigned)(cost - into), (unsigned)(pet.level() + 1));
    gfx->setTextColor(UI_INK);
    setSize(2);
    setCur(centerX(nx, 2), by + 32);
    printT(nx);
  }

  // estado de evolucion
  gfx->setTextColor(UI_TRACK);
  setCur(centerX(T(S_EVO_LABEL), 2), 230);
  printT(T(S_EVO_LABEL));
  char evoBuf[32];
  const char *evo;
  uint16_t evoCol = UI_INK;
  if (d.evolvesTo == 0) {
    evo = T(S_FINAL_FORM);
  } else {
    int needed = d.evolveLevel + pet.careMistakes;
    if (pet.level() >= needed) {
      if (pet.lowestStat() >= 40) { evo = T(S_EVO_READY); evoCol = UI_BAR_OK; }
      else { evo = T(S_EVO_BLOCKED); evoCol = UI_BAR_BAD; }
    } else {
      snprintf(evoBuf, sizeof(evoBuf), T(S_EVO_IN_FMT), needed - pet.level());
      evo = evoBuf;
    }
  }
  gfx->setTextColor(evoCol);
  setCur(centerX(evo, 2), 256);
  printT(evo);

  // descuidos (retrasan la evolucion)
  char ms[24];
  snprintf(ms, sizeof(ms), T(S_MISTAKES_FMT), pet.careMistakes);
  gfx->setTextColor(pet.careMistakes > 0 ? UI_BAR_BAD : UI_INK);
  setCur(centerX(ms, 2), 312);
  printT(ms);
}

// pagina 4: paseo (FR-1.7). Pasos de hoy con un anillo hacia la meta, el total,
// los hallazgos de hoy y la mochila; tocar un objeto pide confirmacion y lo usa.
#define WALK_RING_Y 140
#define WALK_RING_R 60   // radio exterior del anillo (el interior, 13 px menos)
#define WALK_SLOT_Y 262
#define WALK_SLOT_W 52   // 7 casillas (v1.39, el huevo); con 6 eran 60 (v1.25) y con 5, 64
#define WALK_SLOT_H 62
#define WALK_SLOT_GAP 5
#define WALK_SLOT_X (CX - (IT_COUNT * WALK_SLOT_W + (IT_COUNT - 1) * WALK_SLOT_GAP) / 2)

// pasos de hoy en pantalla: los del bicho mas los que el IMU aun no le ha pasado
uint32_t walkStepsToday() { return pet.stepsOn(pet.dayNumber()) + imuStepsPending(); }

// un mapa de caracteres pintado de un solo color (objeto que no se tiene)
void drawMapTint(const char *const *map, int n, int x, int y, int s, uint16_t col) {
  for (int r = 0; r < n; r++) {
    const char *row = map[r];
    forRuns(n, [&](int c) { return charKey(row[c], true); },
            [&](int c0, int len, int) { gfx->fillRect(x + c0 * s, y + r * s, len * s, s, col); });
  }
}

void renderCardWalk() {
  gfx->setTextColor(UI_INK);
  setSize(3);
  setCur(centerX(T(S_WALK), 3), 40);
  printT(T(S_WALK));

  uint32_t day = pet.dayNumber();
  if (!imuPresent()) {  // FR-1.9: sin IMU lo dice y nada mas cambia
    gfx->setTextColor(UI_TRACK);
    setSize(2);
    setCur(centerX(T(S_NO_IMU), 2), WALK_RING_Y - 8);
    printT(T(S_NO_IMU));
  } else {
    uint32_t st = walkStepsToday();
    // metas alcanzadas hoy (las del bicho, o las que los pasos pendientes ya cruzan)
    uint8_t tiers = pet.goalTierOn(day);
    while (tiers < STEP_GOAL_TIERS && st >= STEP_GOALS[tiers]) tiers++;
    // el anillo se llena hacia la siguiente meta; con las tres, entero y verde
    uint32_t next = STEP_GOALS[tiers < STEP_GOAL_TIERS ? tiers : STEP_GOAL_TIERS - 1];
    gfx->fillArc(CX, WALK_RING_Y, WALK_RING_R, WALK_RING_R - 13, 0, 360, UI_TRACK);
    float frac = tiers >= STEP_GOAL_TIERS ? 1.0f : (float)st / next;
    if (frac >= 1.0f) gfx->fillArc(CX, WALK_RING_Y, WALK_RING_R, WALK_RING_R - 13, 0, 360, UI_BAR_OK);
    else if (frac > 0.003f)  // de las 12 en el sentido de las agujas del reloj
      gfx->fillArc(CX, WALK_RING_Y, WALK_RING_R, WALK_RING_R - 13, 270, 270 + 360 * frac, 0x4C98);
    char num[12];
    snprintf(num, sizeof(num), "%lu", (unsigned long)st);
    uint8_t ns = strlen(num) <= 5 ? 3 : 2;
    gfx->setTextColor(UI_INK);
    setSize(ns);
    setCur(centerX(num, ns), WALK_RING_Y - (ns == 3 ? 18 : 14));
    printT(num);
    char goal[10];
    snprintf(goal, sizeof(goal), "/%lu", (unsigned long)next);
    gfx->setTextColor(UI_TRACK);
    setSize(2);
    setCur(centerX(goal, 2), WALK_RING_Y + 12);
    printT(goal);
    // una bandera por meta de hoy, en fila arriba a la derecha del anillo
    for (int k = 0; k < tiers; k++) drawMap(SPR_ICON_FLAG, 16, CX + 48 + k * 26, WALK_RING_Y - 74, 2, false);

    char tot[32];
    snprintf(tot, sizeof(tot), T(S_STEPS_TOTAL_FMT), (unsigned long)(pet.stepsTotal + imuStepsPending()));
    gfx->setTextColor(UI_INK);
    setCur(centerX(tot, 2), 212);
    printT(tot);
    char fnd[32];
    // hallazgos al azar hacia el tope, y aparte los regalos de las metas
    bool isToday = pet.stepDay == day;
    snprintf(fnd, sizeof(fnd), T(S_FINDS_FMT), (unsigned)(isToday ? pet.randomFindsToday() : 0),
             (unsigned)FINDS_PER_DAY);
    if (isToday && pet.bonusFindsToday) {
      size_t L = strlen(fnd);
      snprintf(fnd + L, sizeof(fnd) - L, " +%u", (unsigned)pet.bonusFindsToday);
    }
    setCur(centerX(fnd, 2), 236);
    printT(fnd);
  }

  // mochila: una casilla por objeto, con su cuenta; el amuleto, enmarcado si se lleva
  for (int i = 0; i < IT_COUNT; i++) {
    int x = WALK_SLOT_X + i * (WALK_SLOT_W + WALK_SLOT_GAP), y = WALK_SLOT_Y;
    uint8_t n = pet.items[i];
    bool held = (i == IT_CHARM) && n;
    gfx->fillRoundRect(x, y, WALK_SLOT_W, WALK_SLOT_H, 10, n ? UI_WHITE : UI_BG_DAY);
    gfx->drawRoundRect(x, y, WALK_SLOT_W, WALK_SLOT_H, 10, held ? UI_BAR_WARN : (n ? UI_INK : UI_TRACK));
    if (held) gfx->drawRoundRect(x + 1, y + 1, WALK_SLOT_W - 2, WALK_SLOT_H - 2, 9, UI_BAR_WARN);
    if (n) drawMap(ITEM_ICON[i], 16, x + (WALK_SLOT_W - 32) / 2, y + 6, 2, false);
    else drawMapTint(ITEM_ICON[i], 16, x + (WALK_SLOT_W - 32) / 2, y + 6, 2, UI_TRACK);
    char c[6];
    snprintf(c, sizeof(c), "x%u", n);
    gfx->setTextColor(n ? UI_INK : UI_TRACK);
    setSize(2);
    setCur(x + (WALK_SLOT_W - textW(c, 2)) / 2, y + 42);
    printT(c);
  }
  // una pista: los huevos (v1.39), el amuleto o la mochila vacia
  const char *hint = pet.items[IT_EGG]     ? T(S_EGG_HINT)
                     : pet.items[IT_CHARM] ? T(S_CHARM_HELD)
                     : (pet.itemCount() == 0 ? T(S_BACKPACK_EMPTY) : nullptr);
  if (hint) {
    gfx->setTextColor(pet.itemCount() ? UI_INK : UI_TRACK);
    setSize(2);
    setCur(centerX(hint, 2), 336);
    printT(hint);
  }

  // "usar objeto?" (mismo dialogo que "soltar?")
  if (itemConfirm) {
    uint8_t it = itemConfirm - 1;
    gfx->fillRoundRect(94, 168, 278, 152, 16, UI_WHITE);
    gfx->drawRoundRect(94, 168, 278, 152, 16, UI_INK);
    drawMap(ITEM_ICON[it], 16, CX - 16, 176, 2, false);
    char q[48];
    snprintf(q, sizeof(q), T(S_USE_FMT), T((StrId)(S_IT_GOLDEN + it)));
    gfx->setTextColor(UI_INK);
    setSize(2);
    setCur(centerX(q, 2), 218);
    printT(q);
    gfx->fillRoundRect(118, 252, 100, 52, 12, UI_BAR_OK);
    gfx->setTextColor(UI_WHITE);
    setCur(118 + (100 - textW(T(S_YES), 2)) / 2, 270);
    printT(T(S_YES));
    gfx->fillRoundRect(248, 252, 100, 52, 12, UI_BAR_BAD);
    setCur(248 + (100 - textW(T(S_NO), 2)) / 2, 270);
    printT(T(S_NO));
  }
}

// toques en la pagina de paseo: la fila de la mochila no cierra la ficha
void walkTap(int16_t x, int16_t y) {
  if (itemConfirm) {
    uint8_t it = itemConfirm - 1;
    itemConfirm = 0;
    if (timeLeft(itemConfirmUntil) && x >= 118 && x <= 218 && y >= 252 && y <= 304) {
      FxNeedSnap ns = fxSnap();
      if (pet.useItem(it)) {
        fxNeeds(ns);
        sfxPlay(it == IT_GOLDEN ? SFX_EAT : SFX_LEVEL);
        // a la pantalla principal a verlo: comer, subir de nivel o entrenar (v1.30, "ATK +3")
        cardOpen = false;
      } else {
        sfxPlay(SFX_DENY);
      }
    }
    return;
  }
  if (y >= WALK_SLOT_Y && y < WALK_SLOT_Y + WALK_SLOT_H) {
    if (x < WALK_SLOT_X) return;
    int i = (x - WALK_SLOT_X) / (WALK_SLOT_W + WALK_SLOT_GAP);
    int xi = (x - WALK_SLOT_X) % (WALK_SLOT_W + WALK_SLOT_GAP);
    if (i >= IT_COUNT || xi >= WALK_SLOT_W) return;
    if (pet.canUseItem(i)) {
      itemConfirm = i + 1;
      itemConfirmUntil = millis() + 10000;
      sfxPlay(SFX_TAP);
    } else if (pet.items[i]) {
      sfxPlay(SFX_DENY);  // el amuleto se lleva y el huevo va solo a casa; o dormido, o entrenamiento al maximo
    }
    return;
  }
  cardOpen = false;
}

void renderCard() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  if (cardPage == 0) renderCardProfile();
  else if (cardPage == 1) renderCardStats();
  else if (cardPage == 2) renderCardMedals();
  else if (cardPage == 3) renderCardProgress();
  else renderCardWalk();

  // indicador de paginas + ayuda
  for (int i = 0; i < CARD_PAGES; i++) {
    int dx = CX - (CARD_PAGES - 1) * 13 + i * 26;
    if (i == cardPage) gfx->fillCircle(dx, 374, 5, UI_INK);
    else gfx->drawCircle(dx, 374, 4, UI_INK);
  }
  gfx->setTextColor(UI_TRACK);
  setSize(2);
  setCur(centerX(T(S_BACK), 2), 398);
  printT(T(S_BACK));
  gfx->flush();
}

// ---------- teclado para renombrar ----------

static const char KB_KEYS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ.-";  // 28 + DEL + OK = 30
#define KB_COLS 6
#define KB_X 40
#define KB_Y 150
#define KB_W 64
#define KB_H 52

void openKeyboard() {
  kbOpen = true;
  strncpy(nameBuf, pet.nick, sizeof(nameBuf) - 1);
  nameBuf[sizeof(nameBuf) - 1] = 0;
  nameLen = strlen(nameBuf);
}

void renderKeyboard() {
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(centerX(T(S_NAME), 2), 56);
  printT(T(S_NAME));
  // buffer actual
  gfx->fillRoundRect(83, 84, 300, 40, 8, UI_WHITE);
  gfx->drawRoundRect(83, 84, 300, 40, 8, UI_INK);
  setSize(3);
  setCur(95, 94);
  printT(nameLen ? nameBuf : "_");

  for (int i = 0; i < 30; i++) {
    int x = KB_X + (i % KB_COLS) * KB_W, y = KB_Y + (i / KB_COLS) * KB_H;
    bool special = (i >= 28);
    gfx->fillRoundRect(x, y, KB_W - 6, KB_H - 6, 6, special ? UI_BAR_WARN : UI_WHITE);
    gfx->drawRoundRect(x, y, KB_W - 6, KB_H - 6, 6, UI_INK);
    gfx->setTextColor(UI_INK);
    setSize(2);
    if (i < 28) {
      setCur(x + KB_W / 2 - 9, y + KB_H / 2 - 10);
      printT(KB_KEYS[i]);
    } else {
      const char *lab = (i == 28) ? "<-" : "OK";
      setCur(x + KB_W / 2 - 15, y + KB_H / 2 - 10);
      printT(lab);
    }
  }
  gfx->flush();
}

void keyboardTap(int16_t x, int16_t y) {
  // filtra ANTES de dividir: con x/y menores que KB_X/KB_Y la resta da negativo
  // y la division entera trunca hacia cero (no hacia -inf), asi que col/row
  // saldria 0 en vez de negativo y el filtro de abajo no lo detectaria
  if (x < KB_X || y < KB_Y) return;
  int col = (x - KB_X) / KB_W, row = (y - KB_Y) / KB_H;
  if (col >= KB_COLS || row >= 5) return;
  int i = row * KB_COLS + col;
  if (i >= 30) return;
  if (i == 28) {  // borrar
    if (nameLen) nameBuf[--nameLen] = 0;
  } else if (i == 29) {  // OK
    pet.rename(nameBuf);
    kbOpen = false;
  } else if (nameLen < sizeof(nameBuf) - 1) {
    nameBuf[nameLen++] = KB_KEYS[i];
    nameBuf[nameLen] = 0;
  }
}

// ---------- galeria pokedex ----------

#define GAL_X 73
#define GAL_Y 84
#define GAL_CELL 80

// dibuja una miniatura centrada en su celda; sil=true la pinta en tinta
void drawThumb(const uint8_t *b, int x, int y, int s, bool sil) {
  uint8_t w = b[0], h = b[1], n = b[2];
  const uint8_t *pal = b + 3;
  const uint8_t *d = pal + n * 2;
  int ox = x + (GAL_CELL - w * s) / 2;
  int oy = y + (GAL_CELL - h * s) / 2;
  for (int r = 0; r < h; r++) {
    const uint8_t *row = d + r * w;
    forRuns(w, [&](int c) { return indexedKey(row[c], sil); },
            [&](int c0, int len, int k) {
              uint16_t col = sil ? INK_K : (uint16_t)(pal[k * 2] | (pal[k * 2 + 1] << 8));
              gfx->fillRect(ox + c0 * s, oy + r * s, len * s, s, col);
            });
  }
}

void renderGallery() {
  if (galleryDetail) {  // vista detalle: se redibuja siempre (animada)
    gfx->fillScreen(RGB565_BLACK);
    gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
    const DexEntry &d = DEX_TBL[galleryDetail];
    bool reg = pet.isRegistered(galleryDetail);
    char head[32];
    snprintf(head, sizeof(head), "N.%03d %s%s", galleryDetail,
             pet.isShinyRegistered(galleryDetail) ? "*" : "", reg ? dexName(galleryDetail) : "???");
    gfx->setTextColor(reg ? d.accent : UI_INK);
    // auto-encoge nombres largos (no caben a t3), midiendo en vez de contar
    // bytes; 13 caracteres a tamano 3 son los 234 px de antes.
    setSize(3);
    int gts = (textW(head, 3) <= 234) ? 3 : 2;
    setSize(gts);
    setCur(centerX(head, gts), gts == 3 ? 56 : 60);
    printT(head);
    if (galleryPmd.loaded) {
      // animado y a color si esta registrado; silueta estatica si no (estilo "?")
      drawPmdActM(galleryPmd, PMD_IDLE, CX, 300, reg ? millis() : 0, true, !reg, 6);
    } else {
      const uint8_t *t = thumbs.get(galleryDetail);
      if (t) drawThumb(t, CX - GAL_CELL, 135, 4, !reg);
    }
    gfx->setTextColor(UI_INK);
    setSize(2);
    setCur(centerX(T(S_DETAIL_BACK), 2), 408);
    printT(T(S_DETAIL_BACK));
    gfx->flush();
    return;
  }

  if (!galleryDirty) return;  // la rejilla es estatica
  galleryDirty = false;

  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);
  char head[24];
  snprintf(head, sizeof(head), T(S_POKEDEX_FMT), pet.registeredCount());
  gfx->setTextColor(UI_INK);
  setSize(3);
  setCur(centerX(head, 3), 36);
  printT(head);

  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      int16_t dex = galleryPage * 16 + r * 4 + c + 1;
      if (dex > 151) break;
      int x = GAL_X + c * GAL_CELL, y = GAL_Y + r * GAL_CELL;
      const uint8_t *t = thumbs.get(dex);
      if (t) {
        drawThumb(t, x, y, 2, !pet.isRegistered(dex));
        if (pet.isShinyRegistered(dex)) {
          gfx->setTextColor(UI_BAR_WARN);
          setSize(2);
          setCur(x + 62, y + 4);
          printT("*");
        }
      } else {
        char num[6];
        snprintf(num, sizeof(num), "%d", dex);
        gfx->setTextColor(UI_TRACK);
        setSize(2);
        setCur(x + 24, y + 32);
        printT(num);
      }
    }
  }
  // puntos de pagina
  for (int i = 0; i < 10; i++) {
    if (i == galleryPage) gfx->fillCircle(170 + i * 14, 436, 4, UI_INK);
    else gfx->drawCircle(170 + i * 14, 436, 3, UI_INK);
  }
  gfx->flush();
}

void galleryTap(int16_t x, int16_t y) {
  if (galleryDetail) {  // volver a la rejilla
    galleryDetail = 0;
    galleryPmd.unload();
    galleryDirty = true;
    return;
  }
  if (y < 72) {  // tocar la cabecera = salir
    galleryOpen = false;
    galleryPmd.unload();
    return;
  }
  // mismo motivo que en el teclado: filtrar antes de dividir
  if (x < GAL_X || y < GAL_Y) return;
  int c = (x - GAL_X) / GAL_CELL, r = (y - GAL_Y) / GAL_CELL;
  if (c > 3 || r > 3) return;
  int16_t dex = galleryPage * 16 + r * 4 + c + 1;
  if (dex > 151) return;
  galleryDetail = dex;
  galleryPmd.load(dex, pet.isShinyRegistered(dex));
}

// ---------- la caja (docs/NEXT_FEATURES.md, Tarea BOX, v1.37) ----------
// Deslizar a la derecha desde casa (a la izquierda sigue la Pokedex). Seis huecos en una rejilla
// de 3x2: uno vacio pregunta "guardar?" y uno ocupado "despertar?". Al decir SI, una luz vuela
// entre el bicho activo (la fila de abajo) y el hueco en BOX_FX_MS; el ultimo cuadro ya ensena
// como queda, y con el en pantalla se escribe en la NVS (40-180 ms en la placa, con el sprite). Luego se
// vuelve a casa, donde el que queda activo sale de una luz (FXO_BOX).

#define BOX_X 47       // rejilla de 3x2 huecos de 124x110 (las esquinas quedan ~8 px dentro del circulo)
#define BOX_Y 92
#define BOX_CW 124
#define BOX_CH 110
#define BOX_NOW_Y 350  // centro de la fila "Ahora: ..."
#define BOX_SLEEP_BG C565(0xe2, 0xe5, 0xf4)  // fondo de un hueco ocupado: azulado, duermen
#define BOX_LIGHT C565(0xff, 0xe7, 0x9f)     // halo dorado de la luz

int boxCellX(uint8_t k) { return BOX_X + (k % 3) * BOX_CW; }
int boxCellY(uint8_t k) { return BOX_Y + (k / 3) * BOX_CH; }

int boxSlotAt(int16_t x, int16_t y) {
  if (x < BOX_X || y < BOX_Y) return -1;  // filtrar antes de dividir, como en la Pokedex
  int c = (x - BOX_X) / BOX_CW, r = (y - BOX_Y) / BOX_CH;
  if (c > 2 || r > 1) return -1;
  return r * 3 + c;
}

const char *recName(const PetRecord &r) { return r.nick[0] ? r.nick : dexName(r.speciesId); }
const char *petName() { return pet.nick[0] ? pet.nick : dexName(pet.speciesId); }

// la caja se abre desde la casa sin nada encima: ni dialogo, ni menu, ni bano, ni evolucion
bool boxCanOpen() {
  return !pet.ceremony && !confirmUntil && !choiceKind && !menuKind && !bathUntil && !pet.evolving();
}

void openBox() {
  boxOpen = true;
  boxAsk = 0;
  boxMove = BOXMV_NONE;
  boxShakeUntil = 0;
}

void closeBox() {
  boxOpen = false;
  boxAsk = 0;
}

// Texto centrado en cx a tamano 2 sin pasar de maxW: si no cabe, junta las letras (1 px, o 2) en
// vez de bajar a tamano 1, que en este panel no se lee. Con la fuente CJK se pinta tal cual (los
// nombres en japones o coreano caben).
void printTight(const char *s, int cx, int y, int maxW) {
  setSize(2);
  int w = textW(s, 2);
  if (gCjkFont || w <= maxW) {
    setCur(cx - w / 2, y);
    printT(s);
    return;
  }
  int n = (int)strlen(s);
  int adv = (n * 11 - 1 <= maxW) ? 11 : 10;
  if (n * adv - 2 > maxW) {  // ni asi: el ultimo recurso
    setSize(1);
    setCur(cx - textW(s, 1) / 2, y + 4);
    printT(s);
    return;
  }
  int x = cx - (n * adv - 2) / 2;
  for (int i = 0; i < n; i++) {
    setCur(x + i * adv, y);
    printT(s[i]);
  }
}

// Miniatura de thumbs.bin sin su margen transparente (los fotogramas PMD traen mucho aire: lo
// visible es como mucho 33x28 de 40x40), centrada en (cx, cy), a 2x si cabe en maxW x maxH y si no a
// 1x. dim = dormido: los colores se acercan al fondo bg.
void drawThumbFit(const uint8_t *b, int cx, int cy, int maxW, int maxH, bool dim, uint16_t bg) {
  uint8_t w = b[0], h = b[1], n = b[2];
  const uint8_t *pal = b + 3;
  const uint8_t *d = pal + n * 2;
  int x0 = w, y0 = h, x1 = -1, y1 = -1;
  for (int r = 0; r < h; r++)
    for (int c = 0; c < w; c++)
      if (d[r * w + c] != 0xFF) {
        if (c < x0) x0 = c;
        if (c > x1) x1 = c;
        if (r < y0) y0 = r;
        if (r > y1) y1 = r;
      }
  if (x1 < 0) return;  // toda transparente
  int bw = x1 - x0 + 1, bh = y1 - y0 + 1;
  int s = (bw * 2 <= maxW && bh * 2 <= maxH) ? 2 : 1;
  int ox = cx - bw * s / 2, oy = cy - bh * s / 2;
  for (int r = y0; r <= y1; r++) {
    const uint8_t *row = d + r * w + x0;
    forRuns(bw, [&](int c) { return indexedKey(row[c], false); },
            [&](int c0, int len, int k) {
              uint16_t col = (uint16_t)(pal[k * 2] | (pal[k * 2 + 1] << 8));
              if (dim) col = lerp565(col, bg, 9, 16);
              gfx->fillRect(ox + c0 * s, oy + (r - y0) * s, len * s, s, col);
            });
  }
}

// un hueco: miniatura dormida (con su "z" y la estrella si es shiny), nombre y nivel; "?" si esta
// danado; vacio, con un "+" si el activo se puede guardar. hide = su miniatura se fue con la luz
void drawBoxSlot(uint8_t k, uint8_t st, const PetRecord *r, bool hide, int dx) {
  int x = boxCellX(k) + 4 + dx, y = boxCellY(k) + 4, w = BOX_CW - 8, h = BOX_CH - 8;
  int cx = x + w / 2;
  if (st == BOX_OK && r) {
    gfx->fillRoundRect(x, y, w, h, 12, BOX_SLEEP_BG);
    gfx->drawRoundRect(x, y, w, h, 12, UI_INK);
    if (!hide) {
      const uint8_t *t = thumbs.get(r->speciesId);
      if (t) {
        drawThumbFit(t, cx, y + 34, w - 40, 58, true, BOX_SLEEP_BG);
      } else {  // sin thumbs.bin: el numero, como la Pokedex
        char num[6];
        snprintf(num, sizeof(num), "%d", r->speciesId);
        gfx->setTextColor(UI_TRACK);
        setSize(3);
        setCur(cx - textW(num, 3) / 2, y + 22);
        printT(num);
      }
    }
    gfx->setTextColor(0x4C98);  // "z": duerme
    setSize(2);
    setCur(x + w - 18, y + 6);
    printT("z");
    if (r->flags & REC_SHINY) {
      gfx->setTextColor(UI_BAR_WARN);
      setCur(x + 7, y + 6);
      printT("*");
    }
    gfx->setTextColor(UI_INK);
    printTight(recName(*r), cx, y + 68, w - 6);
    char lv[16];
    snprintf(lv, sizeof(lv), T(S_LVL_FMT), (unsigned)levelFor(r->growMin));
    setSize(2);
    setCur(cx - textW(lv, 2) / 2, y + 86);
    printT(lv);
    return;
  }
  if (st == BOX_DAMAGED) {
    gfx->drawRoundRect(x, y, w, h, 12, UI_BAR_BAD);
    gfx->drawRoundRect(x + 1, y + 1, w - 2, h - 2, 11, UI_BAR_BAD);
    gfx->setTextColor(UI_BAR_BAD);
    setSize(4);
    setCur(cx - textW("?", 4) / 2, y + 18);
    printT("?");
    printTight(T(S_BOX_DAMAGED), cx, y + 74, w - 4);
    return;
  }
  gfx->drawRoundRect(x, y, w, h, 12, UI_TRACK);
  gfx->drawRoundRect(x + 1, y + 1, w - 2, h - 2, 11, UI_TRACK);
  if (pet.canStore(k)) {  // aqui cabe el activo
    gfx->fillRoundRect(cx - 14, y + 34, 28, 6, 3, UI_TRACK);
    gfx->fillRoundRect(cx - 3, y + 23, 6, 28, 3, UI_TRACK);
  }
  gfx->setTextColor(UI_TRACK);
  printTight(T(S_BOX_EMPTY), cx, y + 74, w - 4);
}

// fila del activo: su miniatura (o el huevo) y "Ahora: <nombre>"; con un huevo esperando a su turno,
// el huevo dormido a la derecha. Deja en boxNowCx el centro de la miniatura (de ahi sale la luz).
// vacant = la casa vacia (v1.39): "Ahora: Vacio", con la silueta del huevo en gris
void drawBoxNow(const PetRecord *r, bool hide, bool held, bool vacant) {
  char line[48];
  snprintf(line, sizeof(line), T(S_NOW_FMT), r ? recName(*r) : T(vacant ? S_BOX_EMPTY : S_EGG_HDR));
  setSize(2);
  int tw = textW(line, 2);
  int total = 48 + tw + (held ? 46 : 0);
  int x = CX - total / 2;
  boxNowCx = x + 20;
  if (!hide) {
    if (r) {
      const uint8_t *t = thumbs.get(r->speciesId);
      if (t) drawThumbFit(t, boxNowCx, BOX_NOW_Y, 58, 58, false, UI_BG_DAY);
    } else if (vacant) {
      drawMapTint(SPR_EGG, SPRITE_H, boxNowCx - 16, BOX_NOW_Y - 16, 1, UI_TRACK);
    } else {
      drawMap(SPR_EGG, SPRITE_H, boxNowCx - 16, BOX_NOW_Y - 16, 1, false);
    }
  }
  gfx->setTextColor(UI_INK);
  setCur(x + 48, BOX_NOW_Y - 7);
  printT(line);
  if (held) {  // el huevo que espera (FR-BOX.5): no se ha perdido
    int ex = x + 48 + tw + 10;
    drawMapTint(SPR_EGG, SPRITE_H, ex, BOX_NOW_Y - 16, 1, UI_TRACK);
    gfx->setTextColor(0x4C98);
    setSize(2);
    setCur(ex + 26, BOX_NOW_Y - 22);
    printT("z");
  }
}

// la luz: primero envuelve al bicho (lo "encoge"), luego vuela en arco de a a b con su estela
void drawLightBall(int x, int y, int r) {
  gfx->fillCircle(x, y, r + 4, BOX_LIGHT);
  gfx->fillCircle(x, y, r, UI_WHITE);
}

// bend: hacia donde se curva el arco (en un cambio van dos luces entre los mismos puntos: una por
// arriba y otra por abajo, para que no se tapen)
void boxLightAt(int ax, int ay, int bx, int by, int bend, float u, int *x, int *y) {
  int mx = (ax + bx) / 2 + bend, my = (bend < 0 ? (ay < by ? ay : by) - 70 : (ay + by) / 2 + 40);
  float v = 1.0f - u;
  *x = (int)(v * v * ax + 2 * v * u * mx + u * u * bx);
  *y = (int)(v * v * ay + 2 * v * u * my + u * u * by);
}

void drawBoxLight(int ax, int ay, int bx, int by, int bend, float t) {
  if (t < 0.3f) {
    drawLightBall(ax, ay, 4 + (int)(t / 0.3f * 18));
    return;
  }
  float u = (t - 0.3f) / 0.55f;
  if (u > 1) u = 1;
  for (int i = 3; i >= 1; i--) {  // estela
    float uu = u - i * 0.07f;
    if (uu < 0) continue;
    int x, y;
    boxLightAt(ax, ay, bx, by, bend, uu, &x, &y);
    gfx->fillCircle(x, y, 9 - 2 * i, BOX_LIGHT);
  }
  int x, y;
  boxLightAt(ax, ay, bx, by, bend, u, &x, &y);
  drawLightBall(x, y, 12 - (int)(u * 4));
}

// al llegar: un aro que se abre y se va (el ultimo cuadro queda limpio)
void drawBoxLanding(int x, int y, float t) {
  if (t >= 1) return;
  float k = (t - 0.85f) / 0.15f;
  int r = 14 + (int)(k * 40);
  gfx->drawCircle(x, y, r, UI_WHITE);
  gfx->drawCircle(x, y, r + 1, BOX_LIGHT);
  gfx->drawCircle(x, y, r + 3, UI_WHITE);
}

// "guardar a X?" / "despertar a X?" (+ "Y va a la caja" si hay un bicho activo), SI / NO como "soltar?"
void drawBoxAsk() {
  uint8_t k = boxAsk - 1;
  bool wake = pet.boxState(k) == BOX_OK;
  char q[64];
  char note[64];
  note[0] = 0;
  if (wake) {
    snprintf(q, sizeof(q), T(S_WAKE_Q_FMT), recName(pet.boxRecord(k)));
    if (!pet.isEgg()) snprintf(note, sizeof(note), T(S_SWAP_NOTE_FMT), petName());
  } else {
    snprintf(q, sizeof(q), T(S_STORE_Q_FMT), petName());
    // v1.39: sin huevo que esperaba ni en la mochila, la casa se queda vacia
    if (!pet.eggHeld() && !pet.items[IT_EGG]) snprintf(note, sizeof(note), "%s", T(S_NO_EGG_NOTE));
  }
  gfx->fillRoundRect(73, 160, 320, 160, 16, UI_WHITE);
  gfx->drawRoundRect(73, 160, 320, 160, 16, UI_INK);
  gfx->setTextColor(UI_INK);
  printTight(q, CX, note[0] ? 182 : 198, 296);
  if (note[0]) {
    gfx->setTextColor(BOX_NOTE_INK);
    printTight(note, CX, 214, 296);
  }
  setSize(2);
  gfx->fillRoundRect(118, 252, 100, 52, 12, UI_BAR_OK);
  gfx->setTextColor(UI_WHITE);
  setCur(118 + (100 - textW(T(S_YES), 2)) / 2, 270);
  printT(T(S_YES));
  gfx->fillRoundRect(248, 252, 100, 52, 12, UI_BAR_BAD);
  setCur(248 + (100 - textW(T(S_NO), 2)) / 2, 270);
  printT(T(S_NO));
}

void renderBox() {
  uint32_t now = millis();
  gfx->fillScreen(RGB565_BLACK);
  gfx->fillCircle(CX, CY, 231, UI_BG_DAY);

  // el movimiento en marcha (0..1); desde 0,85 se ve como quedara
  float t = 0;
  if (boxMove) {
    t = (float)(now - boxMoveStart) / (float)BOX_FX_MS;
    if (t >= 1) {
      t = 1;
      boxMoveDrawnEnd = true;
    }
  }
  bool outcome = boxMove && t >= 0.85f;

  unsigned used = pet.boxUsed();
  if (outcome && boxMove == BOXMV_STORE) used++;
  if (outcome && boxMove == BOXMV_WAKE) used--;
  char head[32];
  snprintf(head, sizeof(head), T(S_BOX_FMT), used, (unsigned)BOX_SLOTS);
  gfx->setTextColor(UI_INK);
  setSize(3);
  setCur(centerX(head, 3), 36);
  printT(head);
  bool hatched = !pet.isEgg();
  PetRecord act;
  memset(&act, 0, sizeof(act));
  if (hatched) act = pet.toRecord();
  uint8_t mk = boxMoveSlot;

  for (uint8_t k = 0; k < BOX_SLOTS; k++) {
    uint8_t st = pet.boxState(k);
    const PetRecord *r = st == BOX_OK ? &pet.boxRecord(k) : nullptr;
    bool hide = false;
    if (boxMove && k == mk) {
      if (outcome) {  // guardar o cambiar: el activo en el hueco; despertar con huevo: vacio
        st = boxMove == BOXMV_WAKE ? BOX_EMPTY : BOX_OK;
        r = boxMove == BOXMV_WAKE ? nullptr : &act;
      } else if (boxMove != BOXMV_STORE && t >= 0.3f) {
        hide = true;  // el de la caja ya es la luz
      }
    }
    int dx = 0;
    if (k == boxShakeSlot && timeLeft(boxShakeUntil)) {
      float s = 1.0f - (float)timeLeft(boxShakeUntil) / (float)FX_DENY_MS;
      dx = (int)(4.0f * (1.0f - s) * sinf(s * 6.0f * (float)PI));
    }
    drawBoxSlot(k, st, r, hide, dx);
  }

  // la fila del activo, tambien como quedara al final
  const PetRecord *nowRec = hatched ? &act : nullptr;
  bool held = pet.eggHeld();
  bool vacant = pet.isVacant();
  if (outcome) {
    nowRec = boxMove == BOXMV_STORE ? nullptr : &pet.boxRecord(mk);
    // despertar: con un huevo, el huevo espera; con la casa vacia, nada. Guardar: el huevo que
    // esperaba o uno de la mochila; si no hay, la casa queda vacia (v1.39)
    if (boxMove == BOXMV_WAKE) held = !vacant;
    else if (boxMove == BOXMV_STORE) held = false;
    if (boxMove == BOXMV_STORE) vacant = !pet.eggHeld() && !pet.items[IT_EGG];
    else vacant = false;
  }
  bool hideNow = boxMove && !outcome && boxMove != BOXMV_WAKE && t >= 0.3f;
  drawBoxNow(nowRec, hideNow, held, vacant);

  if (boxMove) {
    int sx = boxCellX(mk) + BOX_CW / 2, sy = boxCellY(mk) + 4 + 34;
    if (!outcome) {
      if (boxMove != BOXMV_WAKE) drawBoxLight(boxNowCx, BOX_NOW_Y, sx, sy, -1, t);  // el activo, al hueco, por arriba
      if (boxMove != BOXMV_STORE)  // el de la caja, fuera: por abajo si se cruza con el otro
        drawBoxLight(sx, sy, boxNowCx, BOX_NOW_Y, boxMove == BOXMV_SWAP ? 60 : -1, t);
    } else {
      if (boxMove != BOXMV_WAKE) drawBoxLanding(sx, sy, t);
      if (boxMove != BOXMV_STORE) drawBoxLanding(boxNowCx, BOX_NOW_Y, t);
    }
  }

  if (boxAsk) drawBoxAsk();
  gfx->flush();
}

// un hueco que no se puede usar tiembla, con el sonido de "no" (como un boton, FR-FX.11)
void boxDeny(int k) {
  sfxPlay(SFX_DENY);
  boxShakeSlot = (int8_t)k;
  boxShakeUntil = millis() + FX_DENY_MS;
}

void boxStartMove(uint8_t k) {
  bool toStore = pet.boxState(k) == BOX_EMPTY;
  if (toStore ? !pet.canStore(k) : !pet.canWake(k)) {
    boxDeny(k);
    return;
  }
  boxMove = toStore ? BOXMV_STORE : (pet.isEgg() ? BOXMV_WAKE : BOXMV_SWAP);
  boxMoveSlot = k;
  boxMoveStart = millis();
  boxMoveDrawnEnd = false;
  sfxPlay(toStore ? SFX_STORE : SFX_WAKE);
}

void boxTap(int16_t x, int16_t y) {
  if (boxMove) return;  // la luz vuela: un momento
  if (boxAsk) {         // dialogo: SI / NO (o fuera = NO)
    uint8_t k = boxAsk - 1;
    boxAsk = 0;
    if (timeLeft(boxAskUntil) && x >= 118 && x <= 218 && y >= 252 && y <= 304) boxStartMove(k);
    return;
  }
  if (y < 72) {  // tocar la cabecera = salir, como en la Pokedex
    closeBox();
    return;
  }
  int k = boxSlotAt(x, y);
  if (k < 0) return;
  bool ok = pet.boxState(k) == BOX_EMPTY ? pet.canStore(k) : pet.canWake(k);
  if (!ok) {  // vacio con un huevo activo (nada que guardar), o un hueco danado
    boxDeny(k);
    return;
  }
  boxAsk = (uint8_t)(k + 1);
  boxAskUntil = millis() + 10000;
  sfxPlay(SFX_TAP);
}

// Despues de cualquier movimiento (tambien STORE / WAKE por serie): sprites del activo nuevo, su
// paseo desde el centro, y los efectos del otro bicho fuera (fx.h: subida y medalla pendientes, "+N",
// barras). Los vigilantes miran al nuevo sin evento: si no, verian "subir" su nivel o "nacer" al
// que sustituye a un huevo. De vuelta a casa, el activo sale de una luz.
void boxAfterMove() {
  boxOpen = false;
  boxAsk = 0;
  boxMove = BOXMV_NONE;
  monFor = -2;  // recarga aunque sea la misma especie: ensureMon tambien vuelve a centrar el paseo
  ensureMon();
  fxq.drop(FXB_LEVELUP);
  fxq.drop(FXB_MEDAL);
  fxo.clear();
  for (auto &b : fxBar) b.init = false;
  fxArmWatches();
  if (!pet.isVacant()) fxo.push(FXO_BOX, 0, 0, millis());  // casa vacia (v1.39): nadie sale de la luz
  wakeUI();  // la escritura tardo ~1 s: el siguiente cuadro, ya
}

// en cada vuelta del loop: cuando la luz llega (y se ha visto el cuadro final) se escribe en flash
void boxTick(uint32_t now) {
  if (!boxMove) return;
  uint32_t e = now - boxMoveStart;
  if (e < BOX_FX_MS) return;
  if (!boxMoveDrawnEnd && e < BOX_FX_MS + 400) return;  // a oscuras no se dibuja: no se espera mas
  uint8_t mv = boxMove, k = boxMoveSlot;
  boxMove = BOXMV_NONE;
  bool ok = (mv == BOXMV_STORE) ? pet.store(k) : pet.wake(k);  // ~1 s con la pantalla quieta
  if (!ok) {  // no cambio nada (p. ej. la NVS llena): se queda en la caja
    boxDeny(k);
    wakeUI();
    return;
  }
  boxAfterMove();
}

// el activo que sale de una luz en casa, tras guardar o despertar (FXO_BOX): la luz se encoge y un
// aro se abre con chispas
void drawBoxAppear(uint32_t now) {
  const FxOverlay *o = fxo.find(FXO_BOX);
  if (!o) return;
  float t = FxOverlays::t(*o, now);
  int cx = pet.isEgg() ? CX : (int)beh.x;
  int cy = (pet.isEgg() || !pmd.loaded) ? PET_CY : PET_GROUND - pmdIdleHeight() / 2;
  float k = 1.0f - t;
  int r = (int)(118 * k * k);
  if (r > 2) {
    gfx->fillCircle(cx, cy, r + 6, BOX_LIGHT);
    gfx->fillCircle(cx, cy, r, UI_WHITE);
  }
  int rr = 40 + (int)(t * 140);
  gfx->drawCircle(cx, cy, rr, UI_WHITE);
  gfx->drawCircle(cx, cy, rr + 2, BOX_LIGHT);
  for (int i = 0; i < 8; i++) {  // chispas en el aro
    float a = i * 0.785f + t * 1.5f;
    int x = cx + (int)(cosf(a) * rr), y = cy + (int)(sinf(a) * rr * 0.8f);
    uint16_t c = (i & 1) ? BOX_LIGHT : UI_WHITE;
    gfx->fillRect(x - 1, y - 5, 3, 11, c);
    gfx->fillRect(x - 5, y - 1, 11, 3, c);
  }
}

// huevos limitados (v1.39): los de la mochila, cuantos pasos faltan para el siguiente y si la casa esta vacia
void eggReport() {
  Serial.printf("eggs bag=%u next=%lu/%u vacant=%d home=%s\n", pet.items[IT_EGG], (unsigned long)pet.eggStepsLeft(),
                pet.eggStepsAt(), pet.isVacant() ? 1 : 0,
                pet.isVacant() ? "empty" : (pet.isEgg() ? "egg" : "pet"));
}

// comando BOX: la caja por serie (uid, especie, nivel, CRC) y el huevo de la incubadora
void boxReport() {
  Serial.printf("active uid=%08lX spec=%d lv=%u egg=%d vacant=%d eggHeld=%d eggT=%d eggShiny=%d box=%u/%u bagEggs=%u\n",
                (unsigned long)pet.uid, pet.speciesId, pet.isEgg() ? 0 : pet.level(),
                (pet.isEgg() && !pet.isVacant()) ? 1 : 0, pet.isVacant() ? 1 : 0, pet.eggHeld() ? 1 : 0,
                pet.eggSpecies(), pet.eggIsShiny() ? 1 : 0, pet.boxUsed(), BOX_SLOTS, pet.items[IT_EGG]);
  for (uint8_t k = 0; k < BOX_SLOTS; k++) {
    uint8_t st = pet.boxState(k);
    if (st == BOX_EMPTY) {
      Serial.printf("box%u empty\n", k);
    } else if (st == BOX_DAMAGED) {
      Serial.printf("box%u damaged (bad CRC or unknown version: BOXCLR %u clears it)\n", k, k);
    } else {
      const PetRecord &r = pet.boxRecord(k);
      Serial.printf("box%u uid=%08lX spec=%d %s lv=%u grow=%lu age=%lu shiny=%d sleep=%d nick=%s stored=%lu crc=ok\n",
                    k, (unsigned long)r.uid, r.speciesId, DEX_TBL[r.speciesId].name, levelFor(r.growMin),
                    (unsigned long)r.growMin, (unsigned long)r.ageMinutes, (r.flags & REC_SHINY) ? 1 : 0,
                    (r.flags & REC_SLEEP) ? 1 : 0, r.nick, (unsigned long)r.storedEpoch);
    }
  }
}

// BOXFILL: bichos inventados en los huecos vacios, para pruebas y capturas (al despertarlos no
// cuentan en la Pokedex: no los ha criado nadie)
void boxFill() {
  for (uint8_t k = 0; k < BOX_SLOTS; k++) {
    if (pet.boxState(k) != BOX_EMPTY) continue;
    PetRecord r;
    memset(&r, 0, sizeof(r));
    r.speciesId = (int16_t)(1 + random(151));
    r.growMin = levelStart((uint16_t)(2 + random(58))) + (uint32_t)random(20);
    r.ageMinutes = r.growMin + (uint32_t)random(600);
    r.flags = (random(8) == 0 ? REC_SHINY : 0) | (random(2) ? REC_SLEEP : 0);
    r.fullness = (uint8_t)(40 + random(61));
    r.joy = (uint8_t)(40 + random(61));
    r.energy = (uint8_t)(40 + random(61));
    r.hygiene = (uint8_t)(40 + random(61));
    r.geneAtk = (uint8_t)(90 + random(21));
    r.geneDef = (uint8_t)(90 + random(21));
    r.geneSpe = (uint8_t)(90 + random(21));
    r.trAtk = (uint8_t)random(30);
    r.trDef = (uint8_t)random(30);
    r.trSpe = (uint8_t)random(30);
    r.bond = (uint8_t)random(100);
    // las medallas que ya tiene por su estado, como un bicho criado de verdad: sin ellas, al
    // despertarlo checkMedals() se las daria y subiria totalMedals, que es del jugador
    uint16_t lv = levelFor(r.growMin);
    r.medals = (lv >= 10 ? MED_LV10 : 0) | (lv >= 25 ? MED_LV25 : 0) | (lv >= 50 ? MED_LV50 : 0) |
               (DEX_TBL[r.speciesId].evolvesTo == 0 ? MED_FINAL : 0) | (lv >= 5 ? MED_FIT : 0) |  // peso 0, sin descuidos
               (pet.streak >= 7 ? MED_STREAK7 : 0);
    r.storedEpoch = pet.lastSeenEpoch;
    pet.boxPut(k, r);
  }
}

#define BAT_ICON_W 27  // 24 de cuerpo + 3 de borne

void drawBattery(int x) {
  int pc = batPercent();
  if (pc < 0) return;  // sin bateria conectada
  int y = 12, w = 24, h = 11;
  bool charging = batCharging();
  uint16_t col = charging ? UI_BAR_OK
                 : (pc >= 40) ? inkColor()
                 : (pc >= 15) ? UI_BAR_WARN
                              : UI_BAR_BAD;
  gfx->drawRoundRect(x, y, w, h, 2, col);
  gfx->fillRect(x + w, y + 3, 3, 5, col);  // borne
  if (charging) {
    // rayo de carga (zigzag) en vez de la barra de nivel
    uint16_t bolt = C565(0xff, 0xd9, 0x4a);
    int bx = x + w / 2;
    gfx->fillTriangle(bx + 3, y + 1, bx - 4, y + 6, bx + 1, y + 6, bolt);
    gfx->fillTriangle(bx - 1, y + 5, bx + 4, y + 5, bx - 3, y + 10, bolt);
  } else {
    int fw = (w - 4) * pc / 100;
    if (fw > 0) gfx->fillRect(x + 2, y + 2, fw, h - 4, col);
  }
}

void drawHeader(const char *name, uint16_t nameColor, const char *msg) {
  // Fila de estado: la bateria y, con el bicho en pantalla, la racha a su lado,
  // centradas como grupo. Arriba del todo el circulo solo deja ~140 px; la
  // insignia de racha estaba en la esquina (26, 16) del lienzo, fuera del
  // circulo, y no se veia ni en la pantalla redonda original.
  int sw = (!pet.isEgg() && !pet.ceremony) ? streakBadgeW() : 0;
  bool bat = batPercent() >= 0;
  int gap = (sw && bat) ? 10 : 0;
  int groupW = (bat ? BAT_ICON_W : 0) + gap + sw;
  int x0 = sw ? CX - groupW / 2 : CX - 14;  // sin racha, la bateria donde siempre
  drawBattery(x0);
  if (sw) drawStreakBadge(x0 + (bat ? BAT_ICON_W + gap : 0), 9);
  gfx->setTextColor(nameColor);
  setSize(3);
  setCur(centerX(name, 3), 52);
  printT(name);
  gfx->setTextColor(inkColor());
  setSize(2);
  setCur(centerX(msg, 2), 90);
  printT(msg);
}

// animacion de la ceremonia (10s): despedida = reverencia con corazones y se
// aleja caminando; escapada = se asusta y sale corriendo. Sustituye al idle.
void drawCeremony() {
  if (!pmd.loaded) { drawPet(); return; }  // respaldo si no hay sprite PMD
  uint32_t now = millis();
  float t = pet.ceremonyT();               // 0..1 a lo largo de los 10s
  bool panic = (pet.ceremony == CER_RUNAWAY);
  int x = CX, y = PET_GROUND;
  uint8_t act = PMD_IDLE;

  if (panic) {
    // final triste: penumbra azulada + lluvia
    for (int i = 0; i < 46; i++) {
      int rx = (i * 47 + now / 3) % 466;
      int ry = (i * 91 + now / 2) % 470;
      gfx->drawLine(rx, ry, rx - 3, ry + 12, C565(0x6a, 0x84, 0xb0));
    }
    bool fade = false;
    if (t < 0.30f) {                       // cabizbajo, temblando
      act = pmd.has(PMD_HURT) ? PMD_HURT : PMD_IDLE;
      x = CX + (int)(4 * sinf(now * 0.04f));
    } else {                               // se aleja despacio y se desvanece
      act = pmd.has(PMD_WALKL) ? PMD_WALKL : PMD_IDLE;
      x = CX - (int)(((t - 0.30f) / 0.70f) * (CX + 120));
      fade = (t > 0.6f) && ((now / 160) % 2 == 0);  // parpadea hacia la silueta
    }
    drawPmdAct(act, x, y, now, true, fade, 5);  // fade=silueta: se difumina al irse
    // lagrima cayendo del bicho
    if (t < 0.55f) {
      int ty = y - 150 + (int)((now / 6) % 40);
      gfx->fillRect(x + 6, ty, 3, 6, C565(0x9a, 0xc4, 0xe8));
    }
    return;
  }

  // despedida epica: halo dorado pulsante + chispas y corazones que ascienden
  int gcy = PET_GROUND - 96;
  for (int k = 0; k < 4; k++) {
    int r = 60 + k * 34 + (int)(10 * sinf(now * 0.02f));
    gfx->drawCircle(CX, gcy, r, C565(0xff, 0xdf, 0x8a));
  }
  for (int i = 0; i < 16; i++) {
    int px = (i * 71 + 28) % 466;
    int py = 410 - (int)((now / 8 + i * 70) % 360);   // suben y reaparecen abajo
    if (py < 30) continue;
    if (i % 4 == 0) drawMap(SPR_HEART, 32, px - 8, py - 8, 1, false);  // corazoncito
    else gfx->fillRect(px, py, 4, 4, (i % 2) ? C565(0xff, 0xe7, 0x9f) : C565(0xff, 0x9a, 0xc0));
  }

  if (t < 0.45f) {                         // reverencia / pose de despedida
    act = pmd.has(PMD_POSE) ? PMD_POSE : (pmd.has(PMD_NOD) ? PMD_NOD : PMD_IDLE);
  } else {                                 // se aleja por la derecha
    act = pmd.has(PMD_WALKR) ? PMD_WALKR : PMD_IDLE;
    x = CX + (int)(((t - 0.45f) / 0.55f) * (CX + 140));
  }
  drawPmdAct(act, x, y, now, true, false, 5);
  if (pet.showHeart())                     // corazon grande siguiendo al bicho
    drawMap(SPR_HEART, 32, x + 50, y - 190, 2, false);
}

// dialogo de decision (2 botones apilados): evolucionar/mantener o despedirse/quedaros
void drawChoiceDialog() {
  const char *q, *o1, *o2;
  uint16_t c1, c2, t1, t2;
  if (choiceKind == 1) {  // evolucion
    q = T(S_EVO_Q); o1 = T(S_EVO_TAP); o2 = T(S_EVO_KEEP);
    c1 = UI_BAR_BAD; t1 = UI_WHITE; c2 = UI_TRACK; t2 = UI_INK;
  } else {                // despedida
    q = T(S_FAR_Q); o1 = T(S_FAR_GO); o2 = T(S_FAR_STAY);
    c1 = UI_BAR_WARN; t1 = UI_INK; c2 = UI_BAR_OK; t2 = UI_WHITE;
  }
  gfx->fillRoundRect(73, 156, 320, 188, 16, UI_WHITE);
  gfx->drawRoundRect(73, 156, 320, 188, 16, UI_INK);
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(centerX(q, 2), 176);
  printT(q);
  gfx->fillRoundRect(93, 206, 280, 52, 12, c1);     // boton accion
  gfx->setTextColor(t1);
  setCur(centerX(o1, 2), 224);
  printT(o1);
  gfx->fillRoundRect(93, 268, 280, 52, 12, c2);     // boton mantener/quedaros
  gfx->setTextColor(t2);
  setCur(centerX(o2, 2), 286);
  printT(o2);
}

// boton-CTA rojo y grande para evolucionar (pulsa para llamar la atencion)
void drawEvolveButton() {
  uint32_t now = millis();
  int p = (int)(5 * sinf(now * 0.006f));  // late: -5..5
  int x = EVO_BTN_X - p, y = EVO_BTN_Y - p, w = EVO_BTN_W + 2 * p, h = EVO_BTN_H + 2 * p;
  gfx->fillRoundRect(x, y, w, h, 18, UI_BAR_BAD);
  gfx->drawRoundRect(x, y, w, h, 18, UI_WHITE);
  gfx->drawRoundRect(x + 2, y + 2, w - 4, h - 4, 16, UI_WHITE);
  gfx->setTextColor(UI_WHITE);
  setSize(3);
  const char *t = T(S_EVO_TAP);
  setCur(centerX(t, 3), y + h / 2 - 11);
  printT(t);
}

// boton-CTA dorado de despedida: "<nombre> quiere decirte algo..."
void drawFarewellButton() {
  uint32_t now = millis();
  int p = (int)(4 * sinf(now * 0.005f));
  int x = FAR_BTN_X - p, y = FAR_BTN_Y - p, w = FAR_BTN_W + 2 * p, h = FAR_BTN_H + 2 * p;
  gfx->fillRoundRect(x, y, w, h, 16, UI_BAR_WARN);
  gfx->drawRoundRect(x, y, w, h, 16, UI_INK);
  char buf[52];
  const char *nm = pet.nick[0] ? pet.nick : dexName(pet.speciesId);
  snprintf(buf, sizeof(buf), T(S_FAREWELL_BTN), nm);
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(centerX(buf, 2), y + h / 2 - 8);
  printT(buf);
}

// boton-CTA sombrio de escapada por abandono: "<nombre> se siente abandonado..."
// (final triste: azul-gris oscuro, latido lento y apagado)
void drawRunawayButton() {
  uint32_t now = millis();
  int p = (int)(3 * sinf(now * 0.003f));
  int x = FAR_BTN_X - p, y = FAR_BTN_Y - p, w = FAR_BTN_W + 2 * p, h = FAR_BTN_H + 2 * p;
  gfx->fillRoundRect(x, y, w, h, 16, C565(0x3a, 0x44, 0x5a));
  gfx->drawRoundRect(x, y, w, h, 16, C565(0x70, 0x80, 0x98));
  char buf[52];
  const char *nm = pet.nick[0] ? pet.nick : dexName(pet.speciesId);
  snprintf(buf, sizeof(buf), T(S_RUNAWAY_BTN), nm);
  gfx->setTextColor(C565(0xc8, 0xd2, 0xe0));
  setSize(2);
  setCur(centerX(buf, 2), y + h / 2 - 8);
  printT(buf);
}

// animacion epica de evolucion: halo radial + rayos giratorios + parpadeo del
// sprite acelerando + chispas que salen disparadas + fogonazo final
void drawEvolveFX(uint32_t now) {
  float t = pet.evolveT();          // 0..1
  int cx = CX, cy = PET_GROUND - 96;

  // halo radial que crece y pulsa
  int halo = 36 + (int)(t * 150) + (int)(8 * sinf(now * 0.02f));
  for (int k = 0; k < 4; k++) {
    int r = halo - k * 7;
    if (r > 0) gfx->drawCircle(cx, cy, r, UI_WHITE);
  }
  // rayos giratorios desde el centro del bicho
  float base = now * 0.004f;
  for (int i = 0; i < 12; i++) {
    float a = base + i * (float)(PI / 6);
    int len = 90 + (int)(70 * (0.5f + 0.5f * sinf(now * 0.012f + i)));
    gfx->drawLine(cx, cy, cx + (int)(cosf(a) * len), cy + (int)(sinf(a) * len), UI_WHITE);
  }
  // parpadeo entre la forma ANTERIOR y la NUEVA (siluetas), acelerando; al
  // final (t>0.9) se queda fija en la nueva para el fogonazo de revelado
  int period = 60 + (int)(220 * (1.0f - t));
  bool showOld = t < 0.9f && evoPmd.loaded && ((now / period) % 2) == 0;
  if (showOld) drawPmdActM(evoPmd, PMD_IDLE, cx, PET_GROUND, 0, true, true, 5);
  else drawPmdAct(PMD_IDLE, cx, PET_GROUND, 0, true, true, 5);
  // chispas que salen disparadas
  for (int i = 0; i < 10; i++) {
    float a = i * (float)(PI / 5) + t * 4.0f;
    int d = (int)((now / 14 + i * 33) % 200);
    int sx = cx + (int)(cosf(a) * d), sy = cy + (int)(sinf(a) * d);
    gfx->fillRect(sx - 2, sy - 2, 5, 5, (i & 1) ? C565(0xff, 0xe0, 0x70) : UI_WHITE);
  }
  // fogonazo final antes de revelar la forma nueva
  if (t > 0.9f) gfx->fillCircle(cx, cy, (int)(300 * (t - 0.9f) / 0.1f), UI_WHITE);
}

void drawPet() {
  if (drawHatchFx(millis())) return;  // eclosion (fx.h): aun se rompe el huevo
  if (pmd.loaded) {
    drawPetPMD();
    return;
  }
  if (mon.loaded) {
    drawPetSD();
    return;
  }
  int fi = flashIdxForDex(pet.speciesId);
  if (fi < 0) {
    // sin SD y sin sprite de flash: aviso claro de que faltan sprites
    gfx->setTextColor(inkColor());
    setSize(6);
    setCur(CX - 18, PET_CY - 80);
    printT("?");
    setSize(2);
    const char *l1 = T(S_NO_SPRITES);
    setCur(centerX(l1, 2), PET_CY - 4);
    printT(l1);
    const char *l2 = T(S_LOAD_SPRITES);
    setCur(centerX(l2, 2), PET_CY + 20);
    printT(l2);
    return;
  }
  const Species &sp = SPECIES[fi];
  int s = sp.scale;
  int x = CX - 16 * s + dizzyWobble(millis());
  int y = PET_CY - 16 * s;

  // animacion de evolucion: alterna la silueta de la forma anterior y la nueva
  if (pet.evolving()) {
    bool flash = (millis() / 300) % 2;
    int16_t showDex = (flash && pet.prevSpeciesId >= 0) ? pet.prevSpeciesId : pet.speciesId;
    int sfi = flashIdxForDex(showDex);
    if (sfi >= 0) {
      const Species &show = SPECIES[sfi];
      drawMap(show.sprite, SPRITE_H, CX - 16 * show.scale, PET_CY - 16 * show.scale, show.scale, flash);
    }
    return;
  }

  PetMood m = pet.mood();
  if (m == MOOD_HAPPY && (millis() / 500) % 2) y -= 6;  // saltito

  drawMap(sp.sprite, SPRITE_H, x, y, s, false);

  // expresiones superpuestas usando las anclas de la especie
  bool blink = (millis() % 3500 < 300);
  if (m == MOOD_SLEEPING || blink) {
    overlayEye(sp, x, y, s, sp.eyeColL);
    overlayEye(sp, x, y, s, sp.eyeColR);
  }
  if (m == MOOD_EATING) overlayMouth(sp, x, y, s, true);
  else if (m == MOOD_SAD) overlayMouth(sp, x, y, s, false);

  if (pet.showHeart()) drawMap(SPR_HEART, 32, x + 20 * s, y - 2 * s, 2, false);
  drawEmote(x + 16 * s, y + 2 * s);
}

// ---------- escena de bano ----------

void startBath() {
  if (pet.isEgg() || pet.sleeping || pet.ceremony || bathUntil) return;
  bathUntil = millis() + 3000;
  bathPending = true;
  int cx = (int)beh.x;
  for (auto &b : bubbles) {
    b.x = cx - 70 + random(140);
    b.y = PET_GROUND - random(150);
    b.r = 8 + random(16);
    b.ph = random(64);
  }
}

void drawBath() {
  uint32_t now = millis();
  if (!timeLeft(bathUntil)) {
    bathUntil = 0;
    if (bathPending) {
      bathPending = false;
      FxNeedSnap ns = fxSnap();
      pet.clean();
      fxNeeds(ns);  // "+60" de higiene
      // pose de alegria al quedar limpio
      if (pmd.has(PMD_POSE)) {
        beh.mode = 2;
        beh.act = PMD_POSE;
        beh.t0 = now;
        beh.until = now + pmdActTotalMs(pmd.acts[PMD_POSE]) * 2;
      }
    }
    return;
  }
  uint32_t left = bathUntil - now;
  if (left > 800) {
    // espuma: pompas meciendose y subiendo poco a poco
    float t = now / 220.0f;
    for (auto &b : bubbles) {
      int bx = b.x + (int)(sinf(t + b.ph) * 6);
      int by = b.y - (int)((3000 - left) / 90);
      gfx->fillCircle(bx, by, b.r, UI_WHITE);
      gfx->drawCircle(bx, by, b.r, 0x7E3D);
      gfx->fillCircle(bx - b.r / 3, by - b.r / 3, b.r / 4, UI_BG_DAY);
    }
  } else {
    // las pompas revientan: destellos
    for (int i = 0; i < 8; i++) {
      auto &b = bubbles[i];
      int sx = b.x + (i % 3) * 6 - 6, sy = b.y - 18;
      uint16_t col = (i % 2) ? UI_BAR_WARN : UI_WHITE;
      gfx->fillRect(sx - 6, sy - 1, 13, 3, col);
      gfx->fillRect(sx - 1, sy - 6, 3, 13, col);
    }
  }
}

// ---------- mascota PMD: comportamiento ----------

uint32_t pmdActTotalMs(const PmdAct &a) {
  uint32_t t = 0;
  for (uint8_t i = 0; i < a.frames; i++) t += a.ms[i];
  return t ? t : 100;
}

uint8_t pmdFrameAt(const PmdAct &a, uint32_t t, bool loop) {
  uint32_t total = pmdActTotalMs(a);
  if (!loop && t >= total) return a.frames - 1;
  t %= total;
  uint8_t i = 0;
  // guarda: nunca mas vueltas que frames, por si un .bin corrupto trae ms[]=0
  // pese al saneo de la carga (defensa en profundidad)
  for (uint8_t guard = 0; guard < a.frames && t >= a.ms[i]; guard++) {
    t -= a.ms[i];
    i = (i + 1) % a.frames;
  }
  return i;
}

// dibuja una accion anclada por la base (centro-x, suelo) y devuelve su escala
// dibuja una accion de un PmdMon concreto (m); drawPmdAct usa el global pmd
void drawPmdActM(PmdMon &m, uint8_t actId, int cx, int groundY, uint32_t t, bool loop, bool sil, uint8_t maxS) {
  const PmdAct &a = m.acts[actId];
  if (!a.frames) return;
  uint8_t sBase = m.acts[PMD_IDLE].h ? 170 / m.acts[PMD_IDLE].h : 5;
  if (sBase < 2) sBase = 2;
  if (sBase > maxS) sBase = maxS;
  uint8_t s = sBase;
  while (s > 2 && a.h * s > 250) s--;  // acciones con frame grande (ataque)
  uint8_t fi = pmdFrameAt(a, t, loop);
  const uint8_t *fr = a.data + (uint32_t)fi * a.w * a.h;
  // anclar por los pies (a.base), no por el alto del lienzo: asi las acciones
  // con padding distinto (Hurt, Eat...) quedan todas a la misma altura de suelo
  int x0 = cx - a.w * s / 2, y0 = groundY - (a.base ? a.base : a.h) * s;
  for (int r = 0; r < a.h; r++) {
    const uint8_t *row = fr + r * a.w;
    forRuns(a.w, [&](int c) { return indexedKey(row[c], sil); },
            [&](int c0, int len, int k) { gfx->fillRect(x0 + c0 * s, y0 + r * s, len * s, s, sil ? INK_K : m.pal[k]); });
  }
}
void drawPmdAct(uint8_t actId, int cx, int groundY, uint32_t t, bool loop, bool sil, uint8_t maxS) {
  drawPmdActM(pmd, actId, cx, groundY, t, loop, sil, maxS);
}

// ---------- reacciones al movimiento (Tarea 2) ----------

// marca de enfado (cuatro esquinas hacia fuera) centrada en (cx, cy)
void drawAngerMark(int cx, int cy) {
  uint16_t red = C565(0xe2, 0x2c, 0x2c);
  for (int sx = -1; sx <= 1; sx += 2)
    for (int sy = -1; sy <= 1; sy += 2) {
      int x0 = cx + sx * 4, y0 = cy + sy * 4;
      gfx->fillRect(sx > 0 ? x0 : x0 - 9, sy > 0 ? y0 : y0 - 3, 10, 4, red);  // brazo horizontal
      gfx->fillRect(sx > 0 ? x0 : x0 - 3, sy > 0 ? y0 : y0 - 9, 4, 10, red);  // brazo vertical
    }
}

// tres estrellas girando sobre la cabeza (mareo)
void drawDizzyStars(int cx, int cy, uint32_t now) {
  for (int i = 0; i < 3; i++) {
    float a = now * 0.007f + i * 2.0944f;
    int x = cx + (int)(44 * cosf(a)), y = cy + (int)(11 * sinf(a));
    uint16_t c = (i == 1) ? UI_WHITE : C565(0xff, 0xdc, 0x4a);
    gfx->fillRect(x - 1, y - 6, 3, 13, c);
    gfx->fillRect(x - 6, y - 1, 13, 3, c);
    gfx->fillRect(x - 2, y - 2, 5, 5, c);
  }
}

// emote encima del bicho: (cx, headY) es el centro de su cabeza mas o menos
void drawEmote(int cx, int headY) {
  uint8_t e = pet.emote();
  if (e == EMOTE_ANGRY) drawAngerMark(cx + 46, headY - 8);
  else if (e == EMOTE_DIZZY) drawDizzyStars(cx, headY - 18, millis());
}

// alto en pantalla del sprite PMD en reposo, de los pies hacia arriba (para
// poner los emotes sobre la cabeza); misma escala que drawPmdActM
int pmdIdleHeight() {
  const PmdAct &a = pmd.acts[PMD_IDLE];
  if (!a.h) return 140;
  int s = 170 / a.h;
  s = s < 2 ? 2 : (s > 5 ? 5 : s);
  return (a.base ? a.base : a.h) * s;
}

// desplazamiento del vaiven del mareo, que se va calmando
int dizzyWobble(uint32_t now) {
  if (pet.emote() != EMOTE_DIZZY) return 0;
  return (int)(7.0f * (1.0f - pet.emoteT()) * sinf(now * 0.03f));
}

// elige el siguiente capricho del bicho cuando esta contento
void behNext() {
  uint32_t now = millis();
  beh.t0 = now;
  int r = random(100);
  if (r < 35 && (pmd.has(PMD_WALKL) || pmd.has(PMD_WALKR))) {
    beh.mode = 1;  // paseo
    beh.targetX = 150 + random(176);
    beh.until = now + 15000;
  } else if (r < 60) {
    // gesto aleatorio entre los disponibles
    // (Hop fuera: salta demasiado alto; Sit fuera: mira hacia atras)
    static const uint8_t flair[] = { PMD_POSE, PMD_NOD, PMD_BREATH };
    uint8_t pick[3], n = 0;
    for (uint8_t f : flair)
      if (pmd.has(f)) pick[n++] = f;
    if (n) {
      beh.mode = 2;
      beh.act = pick[random(n)];
      beh.until = now + pmdActTotalMs(pmd.acts[beh.act]);
      return;
    }
    beh.mode = 0;
    beh.until = now + 2000 + random(3000);
  } else {
    beh.mode = 0;  // mirar al frente
    beh.until = now + 2000 + random(3000);
  }
}

void drawPetPMD() {
  uint32_t now = millis();

  if (pet.evolving()) {
    drawEvolveFX(now);
    return;
  }
  if (evoPmd.loaded) evoPmd.unload();  // termino la evolucion: libera la forma anterior

  PetMood m = pet.mood();
  uint8_t act;
  bool loop = true;
  int8_t surprised = 0;  // resbalando por la inclinacion: lado hacia el que va
  if (m == MOOD_SLEEPING && pmd.has(PMD_SLEEP)) {
    act = PMD_SLEEP;
    beh.mode = 0;
  } else if (m == MOOD_EATING && pmd.has(PMD_EAT)) {
    act = PMD_EAT;
    beh.t0 = 0;
  } else if (m == MOOD_SAD && pmd.has(PMD_HURT)) {
    act = PMD_HURT;
  } else if (imuPresent() && imuTiltSide() && !bathUntil && pet.emote() == EMOTE_NONE) {
    // inclinada (FR-2.3): camina hacia el lado bajo; de mas, resbala sorprendido
    // con el frame de andar quieto. Al nivelarla, vuelve a pasear enseguida.
    int8_t side = imuTiltSide();
    bool slide = imuTiltSlide();
    static uint32_t tiltLast = 0;
    uint32_t dt = (tiltLast && now - tiltLast < 400) ? now - tiltLast : 100;
    tiltLast = now;
    float edge = side > 0 ? 334 : 132;
    float d = edge - beh.x;
    float step = (slide ? 150.0f : 50.0f) * dt / 1000.0f;
    if (step > fabsf(d)) step = fabsf(d);
    beh.x += d > 0 ? step : -step;
    beh.mode = 0;
    beh.until = now + 800;  // nivelada, mira al frente un momento y sigue con lo suyo
    walkLast = 0;
    act = side > 0 ? PMD_WALKR : PMD_WALKL;
    if (!pmd.has(act)) act = PMD_IDLE;
    if (slide) {
      beh.t0 = now;  // frame 0 del paseo, sin dar pasos
      loop = false;
      surprised = side;
    } else if (fabsf(d) < 1) {
      act = PMD_IDLE;  // en el borde: espera
    }
  } else {
    // contento: el planificador decide (idle / paseo / gesto)
    if (now > beh.until) behNext();
    if (beh.mode != 1) walkLast = 0;  // al volver a caminar no se cuenta el tiempo parado
    if (beh.mode == 1) {
      float d = beh.targetX - beh.x;
      if (fabsf(d) < 4) {
        behNext();
        act = PMD_IDLE;
      } else {
        // 30 px/s de diseno (3 px por frame a 10 fps), midiendo el tiempo real: con menos frames
        // por segundo camina igual de rapido
        uint32_t dt = walkLast ? now - walkLast : 100;
        uint32_t cap = frameIntervalMs(powerCtx(now)) + 100;  // un hueco mayor (otra pantalla de por medio) no es un paso largo
        if (dt > cap) dt = cap;
        walkLast = now;
        float step = 30.0f * dt / 1000.0f;
        if (step > fabsf(d)) step = fabsf(d);  // un frame lento no sobrepasa el destino
        beh.x += d > 0 ? step : -step;
        act = (d > 0) ? PMD_WALKR : PMD_WALKL;
      }
    } else {
      act = (beh.mode == 2) ? beh.act : PMD_IDLE;
      loop = false;
    }
    if (!pmd.has(act)) act = PMD_IDLE;
  }

  int px = (int)beh.x + dizzyWobble(now);
  drawPmdAct(act, px, PET_GROUND, now - beh.t0, loop || act == PMD_IDLE, false, 5);

  if (pet.showHeart()) drawMap(SPR_HEART, 32, (int)beh.x + 50, PET_GROUND - 190, 2, false);
  int head = PET_GROUND - pmdIdleHeight() + 24;
  if (head < 130) head = 130;  // sin pisar la cabecera
  drawEmote(px, head);
  if (surprised) {  // "!" de sorpresa, por detras de la marcha
    setSize(4);
    gfx->setTextColor(UI_BAR_BAD);
    setCur(px + (surprised > 0 ? -60 : 40), head - 30);
    printT("!");
  }
}

// sprite animado desde la SD: zoom entero por pixel, frames a su ritmo
void drawPetSD() {
  int s = mon.scale;
  int w = mon.w * s, h = mon.h * s;
  int x = CX - w / 2 + dizzyWobble(millis());
  int y = PET_CY - h / 2;

  bool sil = false;
  if (pet.evolving()) {
    sil = (millis() / 300) % 2;
  } else if (fxBan && fxBan->kind == FXB_LEVELUP && fxq.elapsed(millis()) < 1200) {
    y -= (int)(14 * fabsf(sinf(fxq.elapsed(millis()) * 0.008f)));  // sube de nivel: rebota (sin PMD)
  } else if (pet.mood() == MOOD_HAPPY && (millis() / 500) % 2) {
    y -= 6;  // saltito
  }

  uint16_t fm = mon.frameMs ? mon.frameMs : 100;
  uint16_t fi = pet.sleeping ? 0 : (millis() / fm) % mon.frames;
  const uint8_t *fr = mon.data + (uint32_t)fi * mon.w * mon.h;
  for (int r = 0; r < mon.h; r++) {
    const uint8_t *row = fr + r * mon.w;
    forRuns(mon.w, [&](int c) { return indexedKey(row[c], sil); },
            [&](int c0, int len, int k) { gfx->fillRect(x + c0 * s, y + r * s, len * s, s, sil ? INK_K : mon.pal[k]); });
  }

  // emotes en vez de expresiones (los sprites importados no tienen anclas)
  if (pet.showHeart()) drawMap(SPR_HEART, 32, x + w - 30, y - 50, 2, false);
  drawEmote(x + w / 2, y + 10);
}

// ojo cerrado: borra el ojo 3x4 y dibuja el parpado
void overlayEye(const Species &sp, int x, int y, int s, int col) {
  gfx->fillRect(x + col * s, y + sp.eyeRow * s, 3 * s, 4 * s, sp.bodyColor);
  gfx->fillRect(x + col * s, y + (sp.eyeRow + 2) * s, 3 * s, s, INK_K);
}

// borra la sonrisa base y pinta boca abierta (comer) o ceno (triste)
void overlayMouth(const Species &sp, int x, int y, int s, bool open) {
  int mc = sp.mouthCol, mr = sp.mouthRow;
  gfx->fillRect(x + (mc - 3) * s, y + mr * s, 7 * s, 2 * s, sp.bodyColor);
  if (open) {
    gfx->fillRect(x + (mc - 2) * s, y + mr * s, 5 * s, 2 * s, INK_K);
  } else {
    gfx->fillRect(x + (mc - 2) * s, y + mr * s, 5 * s, s, INK_K);
    gfx->fillRect(x + (mc - 3) * s, y + (mr + 1) * s, s, s, INK_K);
    gfx->fillRect(x + (mc + 3) * s, y + (mr + 1) * s, s, s, INK_K);
  }
}

void drawPoops() {
  for (int i = 0; i < pet.poops; i++) {
    drawMap(SPR_POOP, 32, 36 + i * 46, 244, 2, false);
  }
}

void drawBars() {
  // cada barra se desliza a su valor nuevo en FX_TWEEN_MS (fx.h) en vez de saltar
  uint32_t now = millis();
  const uint8_t v[4] = { pet.fullness, pet.joy, pet.energy, pet.hygiene };
  for (int i = 0; i < 4; i++) fxBar[i].target(v[i], now);
  drawBar(78, 318, T(S_BAR_FOOD), (uint8_t)fxBar[0].value(now));
  drawBar(244, 318, T(S_BAR_JOY), (uint8_t)fxBar[1].value(now));
  drawBar(78, 346, T(S_BAR_ENE), (uint8_t)fxBar[2].value(now));
  drawBar(244, 346, T(S_BAR_HYG), (uint8_t)fxBar[3].value(now));
}

// Separacion entre la etiqueta y su barra en la fila de necesidades. Estaba fija
// en 48 px, justo lo que ocupan 4 letras latinas a escala 2, sin margen. Las
// etiquetas japonesas son mas anchas (ごきげん son 4 kana, ~64 px) y se metian
// dentro de la barra. Se deriva de la etiqueta mas larga, con el 48 de suelo
// para que en los idiomas latinos no cambie nada.
// Requiere el tamano de texto 2 ya puesto, porque textW() lo necesita.
static int barLabelGap() {
  const StrId ids[] = { S_BAR_FOOD, S_BAR_JOY, S_BAR_ENE, S_BAR_HYG };
  int ancho = 0;
  for (StrId id : ids) {
    int w = textW(T(id), 2);
    if (w > ancho) ancho = w;
  }
  // por debajo del umbral original se deja EXACTAMENTE 48, para no mover nada en
  // los idiomas latinos; por encima se anade aire para que la etiqueta respire
  return ancho <= 48 ? 48 : ancho + 8;
}

void drawBar(int x, int y, const char *label, uint8_t val) {
  gfx->setTextColor(inkColor());
  setSize(2);
  setCur(x, y);
  printT(label);
  int gap = barLabelGap();
  // las dos columnas empiezan en 78 y 244: la barra de la primera tiene que
  // acabar antes de la etiqueta de la segunda, y ambas se pintan igual de anchas
  int bw = 232 - (78 + gap);
  if (bw > 100) bw = 100;   // con etiquetas latinas sale 100, como estaba
  int bx = x + gap, bh = 15;
  uint16_t fill = (val >= 50) ? UI_BAR_OK : (val >= 25) ? UI_BAR_WARN : UI_BAR_BAD;
  gfx->fillRoundRect(bx, y, bw, bh, 4, UI_TRACK);
  int fw = (bw - 4) * val / 100;
  if (fw > 0) gfx->fillRoundRect(bx + 2, y + 2, fw, bh - 4, 3, fill);
}

void drawButtons() {
  uint32_t now = millis();
  for (int i = 0; i < 4; i++) {
    bool off = pet.sleeping && i != 2;  // durmiendo solo funciona LUZ
    // efectos (fx.h): el boton tocado se hunde 2 px; si no se puede usar, tiembla de lado
    int ox = 0, oy = 0;
    bool pressed = fxo.find(FXO_PRESS, i) != nullptr;
    if (pressed) oy = 2;
    if (const FxOverlay *d = fxo.find(FXO_DENY, i)) {
      float t = FxOverlays::t(*d, now);
      ox = (int)(4.0f * (1.0f - t) * sinf(t * 6.0f * (float)PI));  // 3 vaivenes que se calman
    }
    int bx = buttons[i].cx - BTN_HALF + ox, by = buttons[i].cy - BTN_HALF + oy;
    if (!pet.sleeping || pressed) gfx->fillRoundRect(bx, by, 2 * BTN_HALF, 2 * BTN_HALF, 14, pressed ? UI_TRACK : UI_WHITE);
    gfx->drawRoundRect(bx, by, 2 * BTN_HALF, 2 * BTN_HALF, 14, inkColor());
    if (!off) drawMap(buttons[i].icon, 16, buttons[i].cx - 16 + ox, buttons[i].cy - 16 + oy, 2, false);
  }
}

// Casa vacia (v1.39): sin bicho ni huevo hasta encontrar uno caminando. La silueta del huevo donde
// estaria, los pasos de hoy y la Pokedex; si hay alguien en la caja, como despertarlo.
void drawVacantHome() {
  drawHeader(T(S_NOEGG_HDR), inkColor(), T(S_NOEGG_MSG));
  int s = 5;  // donde y como drawEggSprite(), en gris y quieta
  uint16_t ghost = gNight ? C565(0x3a, 0x42, 0x5c) : lerp565(UI_TRACK, UI_BG_DAY, 1, 2);
  drawMapTint(SPR_EGG, SPRITE_H, CX - 16 * s, PET_CY - 16 * s, s, ghost);
  gfx->fillRect(0, 312, 466, 154, gNight ? UI_BG_NIGHT : UI_BG_DAY);
  gfx->setTextColor(inkColor());
  setSize(2);
  char line[32];
  snprintf(line, sizeof(line), T(S_STEPS_FMT), (unsigned long)walkStepsToday());
  setCur(centerX(line, 2), 326);
  printT(line);
  snprintf(line, sizeof(line), T(S_POKEDEX_FMT), pet.registeredCount());
  setCur(centerX(line, 2), 352);
  printT(line);
  bool boxed = false;
  for (uint8_t k = 0; k < BOX_SLOTS; k++) boxed |= pet.boxState(k) == BOX_OK;
  if (boxed) {
    gfx->setTextColor(UI_TRACK);
    setCur(centerX(T(S_NOEGG_BOX), 2), 378);
    printT(T(S_NOEGG_BOX));
  }
}

const char *eggMsg() {
  switch (pet.eggCracks()) {
    case 0: return T(S_EGG_TOUCH);
    case 1: return T(S_EGG_MOVES);
    default: return T(S_EGG_ALMOST);
  }
}

const char *statusMsg() {
  if (pet.evolving()) return T(S_EVOLVING);
  if (bathUntil) return "Splish splash!";  // onomatopeya universal
  if (pet.emote() == EMOTE_ANGRY) return T(S_GRUMPY);
  if (pet.emote() == EMOTE_DIZZY) return T(S_DIZZY);
  if (pet.sleeping) return "Zzz...";
  if (pet.eating()) return T(S_EATING);
  if (pet.showHeart()) return T(S_LIKES);
  if (pet.fullness < 25) return T(S_HUNGRY);
  if (pet.hygiene < 25) return T(S_NEEDS_BATH);
  if (pet.energy < 25) return T(S_EXHAUSTED);
  if (pet.joy < 25) return T(S_SAD);
  if (pet.weight > 60) return T(S_CHUBBY);
  if (pet.shiny && pet.ageMinutes < 15) return T(S_IS_SHINY);
  return T(S_HAPPY);
}

// dibuja un mapa de n x n pixeles escalado; silhouette=true lo pinta en tinta
void drawMap(const char *const *map, int n, int x, int y, int s, bool silhouette) {
  for (int r = 0; r < n; r++) {
    const char *row = map[r];
    forRuns(n, [&](int c) { return charKey(row[c], silhouette); },
            [&](int c0, int len, int k) { gfx->fillRect(x + c0 * s, y + r * s, len * s, s, silhouette ? INK_K : spriteColor((char)k)); });
  }
}

// ---------- combate de practica (docs/FEATURES.md, Tarea 3a) ----------
// Un combate automatico contra un rival simulado (FR-3.1..3.5): tarjeta VS con
// el jingle, entrada al campo y un ataque cada ATTACK_MS. Tocar la pantalla
// carga el especial (5 toques). El motor puro (battle.h) decide cada ataque en el
// momento en que empieza; aqui solo se anima y se cuenta. El combate por radio
// (3d) reutilizara esta pantalla con los ataques que mande el anfitrion.

enum : uint8_t { BT_OFF = 0, BT_INTRO, BT_FIGHT, BT_END };
#define BT_VS_MS 2000      // tarjeta VS; el resto de INTRO_MS es la entrada al campo
#define BT_HIT_MS 350      // el golpe llega a mitad del avance del atacante
#define BT_HPBAR_MS 400    // la barra de vida baja en este tiempo
#define BT_FAINT_MS 900    // el derrotado se desvanece
#define BT_END_MS 7000     // el resultado se cierra solo (o con un toque)
// v1.38: el resultado no atiende toques durante 1,8 s. Se suele llegar al final aporreando la
// pantalla para cargar el especial, y el siguiente toque cerraba el resultado sin dejarlo ver.
// La pista "toca: volver" aparece cuando ya se puede tocar.
#define BT_END_LOCK_MS 1800
#define BT_HORIZON 150
#define BT_ME_X 140        // tu bicho: abajo a la izquierda
#define BT_ME_GROUND 352
#define BT_FOE_X 318       // el rival: arriba a la derecha
#define BT_FOE_GROUND 198

PmdMon foePmd;  // sprite del rival: se carga antes de la intro, nunca durante el combate
struct {
  uint8_t phase = BT_OFF;
  uint32_t t0 = 0;       // inicio de la fase en curso
  Battle eng;
  SpecialMeter meter;    // el tuyo: lo cargan tus toques
  SpecialMeter foeMeter; // el del rival simulado: PRACTICE_AI_TAPS por ataque suyo
  BattleEvent ev;        // ultimo ataque calculado
  bool haveEv = false;
  bool evSounded = false;
  uint32_t evAt = 0;     // instante (programado) en que empezo ese ataque
  uint16_t hpBefore[2] = { 0, 0 };
  int16_t foeDex = 1;
  uint8_t joy = 0;       // premio dado al acabar
} bt;

bool battleOpen() { return bt.phase != BT_OFF; }
uint8_t battlePhase() { return bt.phase; }  // comando TAP: 0 cerrado, 1 intro, 2 combate, 3 resultado

void closeBattle() {
  bt.phase = BT_OFF;
  foePmd.unload();
}

// empieza un combate de practica; forceDex 1-151 elige al rival (comando BATTLE <dex>)
void startPractice(int16_t forceDex) {
  if (!pet.canBattle() || battleOpen()) return;
  uint32_t rng = (uint32_t)random(0x7FFFFFFF) ^ (millis() << 7);
  PracticeFoe pf = pickPracticeFoe(pet.speciesId, pet.level(), rng);
  if (forceDex >= 1 && forceDex <= DEX_COUNT) pf.dex = forceDex;
  uint8_t tr = (uint8_t)((pet.trAtk + pet.trDef + pet.trSpe) / 3);  // tu entrenamiento medio
  Fighter me = makeFighter(pet.speciesId, pet.level(), pet.geneAtk, pet.geneDef, pet.geneSpe, pet.trAtk,
                           pet.trDef, pet.trSpe);
  Fighter foe = makeFighter(pf.dex, pf.level, 100, 100, 100, tr, tr, tr);  // genes medios
  bt = decltype(bt){};
  bt.foeDex = pf.dex;
  bt.eng.begin(me, foe, battleRand(rng));
  bt.hpBefore[0] = me.maxHp;
  bt.hpBefore[1] = foe.maxHp;
  foePmd.unload();
  foePmd.load(pf.dex, false);  // antes de la intro: leer la SD bloquea el loop
  menuKind = MENU_NONE;
  bt.phase = BT_INTRO;
  bt.t0 = millis();
  sfxPlay(SFX_BATTLE);
  wakeUI();
  Serial.printf("battle: #%d nv.%u contra #%d nv.%u\n", me.dex, me.level, foe.dex, foe.level);
}

void battleTap(int16_t x, int16_t y) {
  if (bt.phase == BT_END) {
    if (millis() - bt.t0 >= BT_END_LOCK_MS) closeBattle();  // antes, el toque no hace nada
    return;
  }
  if (bt.phase != BT_FIGHT || bt.eng.over()) return;  // en la intro los toques no cuentan
  if (bt.meter.tap()) sfxPlay(SFX_TAP);
}

// avanza el combate: calcula el siguiente ataque cuando le toca y lo hace sonar
void battleStep(uint32_t now) {
  if (bt.phase == BT_INTRO && now - bt.t0 >= INTRO_MS) {
    bt.phase = BT_FIGHT;
    bt.t0 += INTRO_MS;
  }
  if (bt.phase != BT_FIGHT) return;
  if (!bt.eng.over()) {
    uint32_t due = bt.t0 + (uint32_t)bt.eng.attacks * ATTACK_MS;
    if ((int32_t)(now - due) >= 0) {
      uint8_t who = bt.eng.nextAttacker();
      bool special;
      if (who == 0) {
        special = bt.meter.useForAttack();
      } else {
        for (int i = 0; i < PRACTICE_AI_TAPS; i++) bt.foeMeter.tap();
        special = bt.foeMeter.useForAttack();
      }
      bt.hpBefore[0] = bt.eng.f[0].hp;
      bt.hpBefore[1] = bt.eng.f[1].hp;
      bt.ev = bt.eng.step(special);
      bt.haveEv = true;
      bt.evSounded = false;
      bt.evAt = due;  // el programado, no "ahora": un frame lento no desplaza el resto
    }
  }
  if (bt.haveEv && !bt.evSounded && (int32_t)(now - (bt.evAt + BT_HIT_MS)) >= 0) {
    bt.evSounded = true;
    sfxPlay(!bt.ev.eff4 ? SFX_DENY : bt.ev.special ? SFX_SPECIAL : bt.ev.crit ? SFX_CRIT : SFX_HIT);
  }
  if (bt.eng.over() && bt.haveEv) {
    bool ko = bt.ev.hp[0] == 0 || bt.ev.hp[1] == 0;
    uint32_t endAt = bt.evAt + ATTACK_MS + (ko ? BT_FAINT_MS : 0);
    if ((int32_t)(now - endAt) >= 0) {
      bt.phase = BT_END;
      bt.t0 = now;
      bt.joy = pet.practiceDone();  // gane quien gane; como mucho una vez por hora
      sfxPlay(bt.eng.result == BR_WIN_A ? SFX_WIN : bt.eng.result == BR_WIN_B ? SFX_LOSE : SFX_LEVEL);
      Serial.printf("battle: %s en %u ataques, +%u felicidad\n",
                    bt.eng.result == BR_WIN_A ? "gana" : bt.eng.result == BR_WIN_B ? "pierde" : "empate",
                    bt.eng.attacks, bt.joy);
    }
  }
}

// vida que ensena la barra: baja durante BT_HPBAR_MS desde que llega el golpe
uint16_t battleShownHp(uint8_t side, uint32_t now) {
  if (!bt.haveEv) return bt.eng.f[side].hp;
  uint32_t dt = now - bt.evAt;
  uint16_t a = bt.hpBefore[side], b = bt.ev.hp[side];
  if (dt < BT_HIT_MS) return a;
  if (dt >= BT_HIT_MS + BT_HPBAR_MS) return b;
  return (uint16_t)(a + ((int32_t)b - a) * (int32_t)(dt - BT_HIT_MS) / BT_HPBAR_MS);
}

const char *battleName(uint8_t side) {
  if (side == 1) return dexName(bt.foeDex);
  return pet.nick[0] ? pet.nick : dexName(pet.speciesId);
}

// fondo del campo: cielo por la hora, el suelo del bioma de tu bicho y dos tarimas
void drawBattleField(int shake) {
  int hh = sceneHour();
  bool night = hh < 6 || hh >= 20;
  uint16_t top, bot;
  if (night)       { top = C565(0x0c, 0x12, 0x24); bot = C565(0x1e, 0x26, 0x46); }
  else if (hh < 8) { top = C565(0xd1, 0x6a, 0x86); bot = C565(0xf3, 0xb8, 0x7c); }
  else if (hh < 18){ top = C565(0x8f, 0xc8, 0xea); bot = C565(0xdc, 0xee, 0xe6); }
  else             { top = C565(0xc7, 0x5a, 0x4a); bot = C565(0xf0, 0xae, 0x64); }
  for (int y = 0; y < BT_HORIZON; y += 8) gfx->fillRect(0, y, 466, 8, lerp565(top, bot, y, BT_HORIZON));
  if (night)
    for (auto &st : STARS) gfx->fillRect(st[0], st[1] / 2 + 20, 4, 4, UI_WHITE);
  uint8_t bio = DEX_TBL[pet.speciesId].biome;
  uint16_t soil = BIOME_SOIL[bio < BIOME_COUNT ? bio : 0];
  if (night) soil = lerp565(soil, C565(0x16, 0x1c, 0x30), 9, 16);
  gfx->fillRect(0, BT_HORIZON, 466, 466 - BT_HORIZON, soil);
  uint16_t pad = lerp565(soil, C565(0x10, 0x18, 0x20), 4, 16);
  gfx->fillEllipse(BT_FOE_X + shake, BT_FOE_GROUND - 4, 74, 15, pad);
  gfx->fillEllipse(BT_ME_X + shake, BT_ME_GROUND - 4, 88, 17, pad);
}

// uno de los dos: su sprite PMD; si no lo hay, la miniatura; si tampoco, "?"
// maxS: tu bicho x3 y el rival x2 en el campo (los frames de ATTACK son grandes)
void drawFighter(uint8_t side, uint8_t act, int x, int ground, uint32_t t, bool loop, bool sil, uint8_t maxS) {
  PmdMon &m = side == 0 ? pmd : foePmd;
  int16_t dex = side == 0 ? pet.speciesId : bt.foeDex;
  if (m.loaded) {
    if (!m.has(act)) act = PMD_IDLE;
    drawPmdActM(m, act, x, ground, t, loop, sil, maxS);
    return;
  }
  const uint8_t *th = thumbs.get(dex);
  if (th) {
    drawThumb(th, x - GAL_CELL / 2, ground - GAL_CELL, 2, sil);
    return;
  }
  gfx->setTextColor(UI_INK);
  setSize(6);
  setCur(x - 18, ground - 60);
  printT("?");
}

// Fondo negro con el bicho pequeno y la hora, recorriendo la pantalla despacio (burnin.h):
// casi todos los pixeles apagados, y nada se queda quieto en el mismo sitio.
void renderScreensaver() {
  uint32_t now = millis();
  gfx->fillScreen(RGB565_BLACK);
  int x, y;
  saverPos(now, &x, &y);
  int ground = y - SAVER_PET_GAP;  // los pies del bicho; la hora, debajo (burnin.h)
  if (pet.awaitingStarter() || pet.isVacant()) {
    // nada que mostrar (aun): solo la hora
  } else if (pet.isEgg()) {
    drawMap(SPR_EGG, SPRITE_H, x - 32, ground - 64, 2, false);
  } else if (pmd.loaded) {
    uint8_t act = pet.sleeping && pmd.has(PMD_SLEEP) ? PMD_SLEEP : PMD_IDLE;
    drawPmdAct(act, x, ground, now, true, false, 2);
  } else if (const uint8_t *th = thumbs.get(pet.speciesId)) {
    drawThumb(th, x - GAL_CELL / 2, ground - GAL_CELL, 2, false);
  } else {
    int fi = flashIdxForDex(pet.speciesId);
    if (fi >= 0) drawMap(SPECIES[fi].sprite, SPRITE_H, x - 32, ground - 64, 2, false);
  }
  uint32_t e = pet.lastSeenEpoch;
  if (e) {
    char t[8];
    snprintf(t, sizeof(t), "%02u:%02u", (unsigned)((e / 3600) % 24), (unsigned)((e / 60) % 60));
    gfx->setTextColor(0x8410);  // gris tenue, dormido o no (v1.31: dormido era 0x4208 y no se leia)
    setSize(SAVER_CLOCK_SIZE);
    setCur(x - textW(t, SAVER_CLOCK_SIZE) / 2, y);
    printT(t);
  }
  gfx->flush();
}

// cuadro de vida: nombre, nivel, barra (y numeros en el tuyo)
void drawHpBox(uint8_t side, int x, int y, uint16_t hp) {
  const Fighter &f = bt.eng.f[side];
  int h = side == 0 ? 70 : 54;
  gfx->fillRoundRect(x, y, 182, h, 10, UI_WHITE);
  gfx->drawRoundRect(x, y, 182, h, 10, UI_INK);
  gfx->setTextColor(side == 0 ? DEX_TBL[pet.speciesId].accent : DEX_TBL[bt.foeDex].accent);
  setSize(2);
  setCur(x + 10, y + 8);
  printT(battleName(side));
  char lv[10];
  snprintf(lv, sizeof(lv), T(S_LVL_FMT), (unsigned)f.level);
  gfx->setTextColor(UI_INK);
  setCur(x + 10, y + 30);
  printT(lv);
  int bx = x + 92, bw = 80, by = y + 32;
  int fw = f.maxHp ? (int)((uint32_t)(bw - 4) * hp / f.maxHp) : 0;
  uint32_t pct = f.maxHp ? (uint32_t)hp * 100 / f.maxHp : 0;
  uint16_t col = pct > 50 ? UI_BAR_OK : pct > 20 ? UI_BAR_WARN : UI_BAR_BAD;
  gfx->fillRoundRect(bx, by, bw, 12, 4, UI_TRACK);
  if (fw > 0) gfx->fillRoundRect(bx + 2, by + 2, fw, 8, 3, col);
  if (side == 0) {
    char n[16];
    snprintf(n, sizeof(n), "%u/%u", (unsigned)hp, (unsigned)f.maxHp);
    setCur(x + 172 - textW(n, 2), y + 50);
    printT(n);
  }
}

// Una flecha ">" sin palo (v1.38): dos brazos de METER_T px de grueso que se juntan en la punta.
// x = su borde izquierdo, y = su centro. fill = 0 la deja hueca (solo el contorno).
#define METER_W 14     // ancho de una flecha
#define METER_H 12     // media altura
#define METER_T 6      // grueso de cada brazo, en horizontal
#define METER_PITCH 21
#define METER_X 236    // la primera flecha; el cartel "Toca!" va detras de la ultima
#define METER_Y 350
void drawChevron(int x, int y, bool filled, uint16_t fill, uint16_t edge) {
  int tip = x + METER_W, notch = x + METER_W - METER_T;
  if (filled) {
    gfx->fillTriangle(x, y - METER_H, x + METER_T, y - METER_H, tip, y, fill);  // brazo de arriba
    gfx->fillTriangle(x, y - METER_H, tip, y, notch, y, fill);
    gfx->fillTriangle(notch, y, tip, y, x + METER_T, y + METER_H, fill);        // brazo de abajo
    gfx->fillTriangle(notch, y, x + METER_T, y + METER_H, x, y + METER_H, fill);
  }
  gfx->drawLine(x, y - METER_H, x + METER_T, y - METER_H, edge);
  gfx->drawLine(x + METER_T, y - METER_H, tip, y, edge);
  gfx->drawLine(tip, y, x + METER_T, y + METER_H, edge);
  gfx->drawLine(x + METER_T, y + METER_H, x, y + METER_H, edge);
  gfx->drawLine(x, y + METER_H, notch, y, edge);
  gfx->drawLine(notch, y, x, y - METER_H, edge);
}

// medidor del especial (v1.38): "> > > > >" y un cartel "Toca!" mientras los toques cargan.
// Llenas laten cuando esta listo; bloqueado tras un especial, en gris y sin cartel.
void drawSpecialMeter(uint32_t now) {
  bool armed = bt.meter.armed(), locked = bt.meter.locked();
  for (int i = 0; i < SPECIAL_TAPS; i++) {
    int x = METER_X + i * METER_PITCH;
    if (locked) {
      drawChevron(x, METER_Y, false, 0, UI_TRACK);
    } else if (i < bt.meter.pips) {
      uint16_t c = (armed && (now / 160) % 2) ? UI_WHITE : UI_BAR_WARN;
      drawChevron(x, METER_Y, true, c, UI_INK);
    } else {
      drawChevron(x, METER_Y, true, UI_WHITE, UI_INK);
    }
  }
  // el cartel: solo cuando un toque cuenta (ni lleno, ni bloqueado, ni con el combate decidido)
  if (locked || bt.meter.pips >= SPECIAL_TAPS || bt.eng.over()) return;
  const char *tap = T(S_TAP_BOX);
  setSize(2);
  // ancho minimo: con la fuente coreana textW() se queda corto (mide la tinta, no el avance)
  int tw = textW(tap, 2), bw = tw + 16 < 64 ? 64 : tw + 16, bh = 28;
  int bx = METER_X + (SPECIAL_TAPS - 1) * METER_PITCH + METER_W + 10, by = METER_Y - bh / 2;
  gfx->fillRoundRect(bx, by, bw, bh, 8, UI_BAR_WARN);
  gfx->drawRoundRect(bx, by, bw, bh, 8, UI_INK);
  gfx->setTextColor(UI_INK);
  setCur(bx + (bw - tw) / 2, by + (bh - 14) / 2);
  printT(tap);
}

// tarjeta VS: el rival arriba, tu bicho abajo, cada mitad del color de su tipo
void drawVsCard() {
  uint16_t ink = UI_INK;
  gfx->fillRect(0, 0, 466, CY - 6, lerp565(DEX_TBL[bt.foeDex].accent, UI_WHITE, 5, 8));
  gfx->fillRect(0, CY + 6, 466, 466 - CY - 6, lerp565(DEX_TBL[pet.speciesId].accent, UI_WHITE, 5, 8));
  gfx->fillRect(0, CY - 6, 466, 12, ink);
  gfx->setTextColor(ink);
  setSize(2);
  setCur(centerX(T(S_PRACTICE), 2), 54);
  printT(T(S_PRACTICE));
  char lv[10];
  for (uint8_t side = 0; side < 2; side++) {
    const char *nm = battleName(side);
    int y = side ? 100 : 292;
    setSize(3);
    uint8_t ts = textW(nm, 3) <= 300 ? 3 : 2;
    setSize(ts);
    gfx->setTextColor(ink);
    setCur(centerX(nm, ts), y);
    printT(nm);
    snprintf(lv, sizeof(lv), T(S_LVL_FMT), (unsigned)bt.eng.f[side].level);
    gfx->setTextColor(ink);
    setSize(2);
    setCur(centerX(lv, 2), y + 38);
    printT(lv);
  }
  gfx->fillCircle(CX, CY, 46, UI_WHITE);
  gfx->drawCircle(CX, CY, 46, ink);
  gfx->drawCircle(CX, CY, 45, ink);
  gfx->setTextColor(UI_BAR_BAD);
  setSize(5);
  setCur(CX - 29, CY - 18);
  printT("VS");
}

// rotulos del ultimo golpe (desde que llega hasta el siguiente)
void drawBattleCaption(uint32_t now) {
  const char *l1 = nullptr, *l2 = nullptr;
  if (!bt.haveEv || now - bt.evAt < BT_HIT_MS) {
    if (!bt.haveEv && bt.phase == BT_FIGHT) l2 = T(S_TAP_CHARGE);  // pista hasta el primer golpe
  } else if (!bt.ev.eff4) {
    l1 = T(S_NO_EFFECT);
  } else {
    if (bt.ev.special) l1 = T(S_SPECIAL);
    else if (bt.ev.crit) l1 = T(S_CRITICAL);
    if (bt.ev.eff4 >= 8) l2 = T(S_SUPER_EFF);
    else if (bt.ev.eff4 <= 2) l2 = T(S_NOT_EFF);
  }
  setSize(2);
  if (l1) {
    gfx->setTextColor(bt.ev.special ? UI_BAR_BAD : UI_INK);
    setCur(centerX(l1, 2), l2 ? 382 : 392);
    printT(l1);
  }
  if (l2) {
    gfx->setTextColor(bt.haveEv ? UI_INK : UI_TRACK);
    setCur(centerX(l2, 2), l1 ? 404 : 392);
    printT(l2);
  }
}

// Dibuja un frame del combate. false = se acaba de cerrar (render pinta la casa).
bool renderBattle() {
  uint32_t now = millis();
  battleStep(now);
  if (bt.phase == BT_END && now - bt.t0 >= BT_END_MS) {
    closeBattle();
    return false;
  }
  if (bt.phase == BT_INTRO && now - bt.t0 < BT_VS_MS) {
    drawVsCard();
    gfx->flush();
    return true;
  }
  if (bt.phase == BT_END) {
    drawBattleResult(now);
    gfx->flush();
    return true;
  }

  // que hace cada uno en este frame
  uint8_t act[2] = { PMD_IDLE, PMD_IDLE };
  uint32_t at[2] = { now, now };
  bool loop[2] = { true, true }, sil[2] = { false, false }, hidden[2] = { false, false };
  int x[2] = { BT_ME_X, BT_FOE_X }, g[2] = { BT_ME_GROUND, BT_FOE_GROUND };
  int shake = 0;
  bool flash = false;
  if (bt.phase == BT_INTRO) {  // entran al campo andando, cada uno desde su lado
    float p = (float)(now - bt.t0 - BT_VS_MS) / (INTRO_MS - BT_VS_MS);
    if (p > 1) p = 1;
    x[0] = (int)(-70 + (BT_ME_X + 70) * p);
    x[1] = (int)(536 - (536 - BT_FOE_X) * p);
    act[0] = PMD_WALKR;
    act[1] = PMD_WALKL;
  } else if (bt.haveEv) {
    uint8_t a = bt.ev.attacker, d = 1 - a;
    uint32_t dt = now - bt.evAt;
    if (dt < 700) {  // el atacante se lanza hacia el otro y vuelve
      act[a] = PMD_ATTACK;
      at[a] = dt;
      loop[a] = false;
      float k = sinf((float)PI * (dt < 600 ? dt : 600) / 600.0f);
      x[a] += (int)(k * (a == 0 ? 34 : -34));
      g[a] += (int)(k * (a == 0 ? -24 : 24));
    }
    bool ko = bt.ev.hp[d] == 0;
    if (dt >= BT_HIT_MS && dt < BT_HIT_MS + 500 && bt.ev.damage) {  // encaja el golpe
      act[d] = PMD_HURT;
      at[d] = dt - BT_HIT_MS;
      x[d] += (int)(4 * sinf(dt * 0.12f));
    }
    if (ko && dt >= BT_HIT_MS + 400) {  // se desvanece y se hunde
      uint32_t f = dt - BT_HIT_MS - 400;
      if (f >= BT_FAINT_MS || bt.phase == BT_END) hidden[d] = true;
      sil[d] = (f / 90) % 2;
      g[d] += (int)(f * 40 / BT_FAINT_MS);
      act[d] = PMD_HURT;
    }
    if (bt.ev.special && dt >= BT_HIT_MS && dt < BT_HIT_MS + 350) {  // destello y temblor
      shake = (int)(8 * sinf(dt * 0.15f));
      flash = dt < BT_HIT_MS + 90;
    }
  }

  drawBattleField(shake);
  // el rival primero (esta mas lejos); en pleno ataque, el atacante encima
  uint8_t order[2] = { 1, 0 };
  if (bt.haveEv && bt.phase == BT_FIGHT) {
    order[0] = 1 - bt.ev.attacker;
    order[1] = bt.ev.attacker;
  }
  for (uint8_t side : order)
    if (!hidden[side])
      drawFighter(side, act[side], x[side] + shake, g[side], at[side], loop[side], sil[side], side == 0 ? 3 : 2);
  drawHpBox(1, 80, 64, battleShownHp(1, now));
  drawHpBox(0, 222, 258, battleShownHp(0, now));
  if (bt.phase == BT_FIGHT) {
    drawSpecialMeter(now);
    drawBattleCaption(now);
  }
  if (flash) gfx->fillCircle(CX, CY, 231, UI_WHITE);
  gfx->flush();
  return true;
}

// Resultado: el ganador posa en el centro (en un empate, los dos), el titulo
// arriba y el premio abajo. Un toque, la tecla USER o BT_END_MS lo cierran.
void drawBattleResult(uint32_t now) {
  drawBattleField(0);
  uint32_t t = now - bt.t0;
  if (bt.eng.result == BR_DRAW) {
    drawFighter(0, PMD_IDLE, CX - 84, 300, t, true, false, 3);
    drawFighter(1, PMD_IDLE, CX + 84, 300, t, true, false, 3);
  } else {
    drawFighter(bt.eng.result == BR_WIN_A ? 0 : 1, PMD_POSE, CX, 306, t, true, false, 4);
  }
  const char *l1 = bt.eng.result == BR_WIN_A ? T(S_WIN) : bt.eng.result == BR_WIN_B ? T(S_LOSE) : T(S_DRAW);
  gfx->fillRoundRect(98, 50, 270, 50, 14, UI_BAR_WARN);
  gfx->drawRoundRect(98, 50, 270, 50, 14, UI_INK);
  gfx->setTextColor(UI_INK);
  setSize(3);
  uint8_t ts = textW(l1, 3) <= 250 ? 3 : 2;
  setSize(ts);
  setCur(centerX(l1, ts), ts == 3 ? 64 : 68);
  printT(l1);
  char l2[40];
  if (bt.joy) snprintf(l2, sizeof(l2), T(S_PRACTICE_JOY_FMT), (unsigned)bt.joy);
  const char *r = bt.joy ? l2 : T(S_PRACTICE_NONE);
  gfx->fillRoundRect(83, 330, 300, 64, 14, UI_WHITE);
  gfx->drawRoundRect(83, 330, 300, 64, 14, UI_INK);
  gfx->setTextColor(bt.joy ? UI_BAR_OK : UI_INK);
  setSize(2);
  setCur(centerX(r, 2), 342);
  printT(r);
  if (t >= BT_END_LOCK_MS) {  // ya se puede cerrar con un toque (v1.38)
    gfx->setTextColor(UI_TRACK);
    setCur(centerX(T(S_BACK), 2), 368);
    printT(T(S_BACK));
  }
}
