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
#include "eggcolor.h" // huevos con el color del tipo del bicho que llevan (v2.4, logica pura)
#include "power.h"
#include "volume.h" // volumen del altavoz: 4 niveles con el apagado (logica pura)
#include "button.h"
#include "imu.h"    // pasos y trucos de movimiento (QMI8658); SensorLib queda en imu.cpp
#include "steps.h"  // STEP_FLUSH_MS
#include "battle.h" // motor del combate (logica pura)
#include "stage.h"  // fases de combate: 3 rivales seguidos contra un equipo de 1 a 4 (logica pura)
#include "encounter.h" // encuentros salvajes (v2.2): tiradas del dia, nivel y especie (logica pura)
#include "wind.h"   // viento del paisaje: hierba, flores y copas que se mecen (logica pura)
#include "burnin.h" // contra el quemado del AMOLED: orbita de la imagen y salvapantallas (logica pura)
#include "fx.h"     // efectos de respuesta: celebraciones en cola, "+N", botones, eclosion (logica pura)
#include "ballgame.h" // minijuego de la pokeball: velocidad segun la puntuacion y la fisica (logica pura)
#include "zengame.h"  // cascada (v1.43): la barra de concentracion, la franja y los golpes de agua (logica pura)
#include "dashgame.h" // carrera en trineo (v1.44): direccion, velocidad, filas de obstaculos y choques (logica pura)
#include "yard_panels.h"  // el patio (v2.0, Y2): paneles Cuidar y Jugar (geometria)
#include "yard_ui.h"  // el patio (v2.0): paleta, geometria y toques del kit de interfaz nuevo (logica pura)
#include "yardbg.h"   // fondos del patio (v2.0): lector de TPY1, mascara de paso y tinte (logica pura)
#include "yard.h"     // el patio (v2.0, Y2): los bichos que pasean por el mapa (logica pura)
#include "shop.h"     // la tienda de la mochila y el selector de fondos (v2.0, Y4b): logica pura
#include "travel_ui.h"      // los viajes y la coleccion (v2.0, Y5b): geometria y logica pura de las pantallas
#include "souvenir_icons.h"  // los 25 recuerdos y la maleta (generado por tools/souvenirs.py)
// (eggtimer.h, el huevo del patio de la Y3, llega con pet.h)

// Version del firmware. Subir este numero en cada release (y manifest.json para
// el instalador web). Se muestra en la pantalla de ajustes y por serie al arrancar.
#define FW_VERSION "3.8"

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
SdMon mon;          // sprite B/N del bicho del foco (respaldo y minijuego si no hay PMD)
// v3.0: el sprite PMD multi-accion de cada hueco de casa; `pmd` es el del bicho del foco (pet.focusSlot)
PmdMon homeMon[HOME_SLOTS];
#define pmd (homeMon[pet.focusSlot])
PmdMon evoPmd;      // forma anterior, solo durante el parpadeo de evolucion
int16_t monFor[HOME_SLOTS] = { -2, -2 };  // especie y shiny con que se cargo cada homeMon
bool monShinyFor[HOME_SLOTS] = { false, false };
int16_t monBwFor = -2;                    // ...y el B/N (mon), del foco
bool monBwShiny = false;

#define PET_GROUND 304  // linea de suelo de la mascota (las pantallas de la despedida y el Revivir)
// el patio (v2.0, Y2): el bicho de casa (p[0]) y el primero de la caja (p[1], hibernando: solo pasea)
YardWorld yardW;
PmdMon yardBoxPmd;              // sprite del de la caja (si no hay, su miniatura)
int8_t yardBoxSlot = -1;        // hueco de la caja que se ve en el patio (-1 = nadie)
int16_t yardBoxDex = -2;
bool yardBoxShiny = false;
uint32_t yardBoxEpoch = 0;      // sdEpoch con que se cargo
uint32_t yardLastStep = 0;      // ultimo paso del planificador (0 = ninguno aun)
uint32_t gYardPetsUs = 0, gYardPetsMaxUs = 0;  // PERF: los bichos, sus cosas y el HUD
// fase Y3 (acciones en el mundo): la comida del patio (yard.h) y el huevo del patio. v3.3: una por hueco de casa,
// la que se elige en el menu del bicho y que aparece delante de el
YardFood yardFood[HOME_SLOTS];
uint32_t yardHeartPopAt = 0;          // al acabarse la comida sale un corazon
// v3.0: uno por hueco de casa
int16_t yardEggX[HOME_SLOTS] = { -1, -1 }, yardEggY[HOME_SLOTS] = { 0, 0 };  // donde esta el huevo (los pies; solo RAM, -1 = sin sitio aun)
uint32_t yardEggTapAt[HOME_SLOTS] = { 0, 0 };      // tocado: se tambalea y dice lo que falta
uint32_t yardEggInfoUntil[HOME_SLOTS] = { 0, 0 };  // el globo del huevo (tras un toque o una sacudida que conto)
uint8_t yardHeartSlot = 0;                         // el que se comio la comida (su corazon)
#define YARD_EGG_S 3                  // el huevo del patio, a escala 3 (~51x69 px de tinta)
#define YARD_EGG_MID (13 * YARD_EGG_S)  // de sus pies al centro del sprite
int yardFeedFront(uint8_t s, uint8_t kind);  // (en la seccion del patio) 1 come, 0 ya tiene una, -1 sin patio
const uint8_t *yardWalkNow();         // la mascara de paso con el huevo (seccion del patio)
// como se dibuja cada bicho del patio (aqui arriba: el conversor .ino pone los prototipos antes que
// el resto): el PMD a escala fija, el sprite B/N, el de flash o la miniatura de la caja
enum : uint8_t { YB_NONE = 0, YB_PMD, YB_TPK1, YB_FLASH, YB_THUMB };
struct YardBody {
  uint8_t kind;
  uint8_t s;   // escala
  int16_t w;   // ancho visible
  int16_t h;   // alto visible, de los pies a la cabeza
};
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
uint8_t boxMoveHome = 0;      // v3.0: el hueco de casa del movimiento (0 o 1)
uint8_t boxHome = 0;          // v3.0: el hueco de casa que ensena la fila "Ahora" (al abrir, el del foco)
bool boxAskPick = false;      // v3.0: el dialogo pregunta CUAL de los dos de casa (guardar o cambiar)
uint32_t boxMoveStart = 0;
bool boxMoveDrawnEnd = false; // ya se vio el ultimo cuadro (como quedara): se puede escribir
int8_t poopPuffSlot = -1;      // caca recien limpiada (v1.58): destellos en su hueco...
uint8_t poopPuffHome = 0;      // ...de las del bicho de este hueco de casa (v3.0)...
uint32_t poopPuffUntil = 0;    // ...hasta aqui
#define POOP_PUFF_MS 450
int8_t boxShakeSlot = -1;     // hueco que no se puede usar: tiembla (como un boton, fx.h)
uint32_t boxShakeUntil = 0;
// fases de combate (v1.65): la seleccion de fase y el selector de equipo (menus estaticos, como la caja)
bool stagesOpen = false;
bool teamOpen = false;
uint8_t stageSel = 0;         // la fase elegida (el selector de equipo la lleva)
uint32_t teamPick[TEAM_MAX] = { 0 };  // uids elegidos, en orden
uint8_t teamPickN = 0;
int8_t stShakeIdx = -1;       // casilla (fase o bicho) que no se puede usar: tiembla
uint32_t stShakeUntil = 0;
// encuentros salvajes (v2.2, encounter.h): la pantalla del encuentro (bandas negras y el salvaje en un circulo),
// el selector de equipo en modo "un luchador" y lo que hay en el patio
enum : uint8_t { ENCU_OFF = 0, ENCU_INTRO, ENCU_BALL, ENCU_THROW, ENCU_DONE };
enum : uint8_t { ENCD_CAUGHT = 0, ENCD_LEFT, ENCD_FLED };
uint8_t encUi = ENCU_OFF;     // la pantalla del encuentro abierta (y en que paso)
uint32_t encUiAt = 0;         // cuando empezo ese paso (los toques de justo despues no cuentan)
uint8_t encDoneKind = ENCD_CAUGHT;
int16_t encShowDex = 0;       // el salvaje que ensena la pantalla (al acabar ya no esta en Pet)
uint8_t encShowLv = 0;
bool encShowShiny = false;
uint32_t encDenyUntil = 0;    // la pastilla de Lanzar tiembla (sin bolas o sin sitio)
bool teamWild = false;        // el selector de equipo elige el luchador del encuentro (uno solo)
int16_t yardEncX = -1, yardEncY = 0;  // donde se mueve en el patio (los pies; solo RAM, -1 = sin sitio)
#define ENC_TAP_LOCK_MS 700
uint8_t encSndStep = 0;       // los sonidos de la Poke Ball ya dados (uno por fase de la animacion)
#define ENC_THROW_MS 2600     // la Poke Ball: vuela, se cierra, 3 sacudidas y click
#define encOpen() (encUi != ENCU_OFF)  // (macro: una funcion aqui arriba adelantaria los prototipos del .ino)
int boxNowCx = 233;           // centro de la miniatura del activo (de ahi sale o ahi llega la luz)

bool screenOff = false;       // pulsacion corta del boton PWR (la tercera de cada vuelta, v1.32)
// salvapantallas puesto con PWR (v1.32): comer, el corazon o una evolucion no lo quitan solos,
// como hacen con la pantalla atenuada; cualquier toque, tecla o sacudida si (wakeUI)
bool saverPinned = false;
bool cardOpen = false;        // ficha del bicho (deslizar vertical)
#define CARD_NAME_TAP_Y 96  // ficha, pagina 0: tocar arriba de esto (el nombre y la especie) = renombrar
bool kbOpen = false;          // teclado para renombrar al bicho
char nameBuf[12] = "";
uint8_t nameLen = 0;
uint8_t cardPage = 0;         // v3.4: 0 perfil (con descuidos y evolucion), 1 estadisticas (la de combate)
#define CARD_PAGES 2
// la pagina del jugador (v3.4, deslizar arriba en el patio): medallas, paseo, insignias y coleccion
bool userOpen = false;
uint8_t userPage = 0;         // USERP_*
uint8_t userMedalPage = 0;    // medallas: la sub-pagina (USER_MEDAL_ROWS filas cada una)
int8_t userMedalTip = -1;     // medallas: la tarjeta abierta (MD_*) o -1
enum : uint8_t { USERP_MEDALS = 0, USERP_WALK, USERP_BADGES, USERP_COLLECTION, USER_PAGES };
#define USER_MEDAL_ROWS 5
#define USER_MEDAL_PAGES ((MEDAL_COUNT + USER_MEDAL_ROWS - 1) / USER_MEDAL_ROWS)
bool bagOpen = false;         // mochila (v1.53): su boton en la fila de abajo, antes la pagina de paseo de la ficha
uint8_t itemConfirm = 0;      // mochila: objeto a confirmar + 1 (0 = ninguno)
int8_t itemConfirmSlot = -1;  // v3.3: el bicho de casa en el que se usara (el que podia al tocarlo; -1 = ninguno)
uint32_t itemConfirmUntil = 0;
// la tienda de la mochila y el selector de fondos (v2.0, Y4b)
uint8_t bagPage = 0;          // v3.2: Objetos y Tienda tienen 2 paginas (la 2.a, las bayas de colores)
uint8_t bagTab = 0;           // 0 = Objetos, 1 = Tienda (v3.3: su pantalla, del dock). v3.4: la Coleccion, a la pagina del jugador
uint8_t souvTip = 0;          // coleccion (pagina del jugador): recuerdo con la tarjeta abierta + 1 (0 = ninguna)
uint8_t shopBuyId = 0;        // tienda: objeto a comprar + 1 (0 = ninguno)
uint32_t shopBuyUntil = 0;
uint8_t shopToast = 0;        // aviso verde / rojo (SHT_*) y hasta cuando
uint32_t shopToastUntil = 0;
uint8_t yardMapPreview = 0;   // selector de fondos abierto en el mapa n (1..YARD_MAPS); solo RAM; 0 = cerrado
enum : uint8_t { SHT_NONE = 0, SHT_OK, SHT_BROKE, SHT_FULL };
#define SHOP_TOAST_MS 1500UL
uint32_t bagReviveHintUntil = 0;  // la pista del Revivir en la mochila (v1.59)...
uint8_t bagHintItem = IT_REVIVE;  // ...o la de la Poke Ball (v2.2): las dos dicen para que sirven
uint32_t revivedMsgUntil = 0;     // "Revivido!" en el estado tras usar un Revivir (v1.60)
bool foundShown = false;      // el ultimo frame enseno un hallazgo: un toque lo cierra
static_assert(S_IT_BERRY_O - S_IT_GOLDEN == IT_BERRY_O - IT_GOLDEN && IT_BERRY_O + 1 == IT_COUNT, "nombres de objeto en el orden de IT_*");
bool clockOpen = false;       // pantalla de ajuste de hora (deslizar abajo)
static int barLabelGap();  // (mas abajo, con drawBar; la usan antes la ficha y el patio)
#define YDEMO_PAGES 3
bool yardDemoOpen = false;    // el patio (v2.0, Y0): muestra del kit de interfaz (serie YARDUI)
uint8_t yardDemoPage = 0;     // 0 panel (tienda), 1 mundo (casa), 2 fase
int clockH = 12, clockM = 0;  // hora en edicion

// el panel Jugar (v2.0, fase Y2; yard_panels.h; v3.3 sin el de Cuidar). Es una pantalla: no se cierra sola;
// se sale con volver o con USER
uint8_t panelKind = PANEL_NONE;
int8_t panelShakeIdx = -1;     // la tarjeta que tiembla al negarse (como las casillas de las fases)
uint32_t panelShakeUntil = 0;
// los viajes (v2.0, Y5b): la pantalla PANEL_TRAVEL, un nivel por encima del panel Jugar
uint8_t trvSelPet = 0;         // bichos que pueden viajar: el elegido (indice en la lista, no hueco)
uint8_t trvSelDest = 1;        // mapa elegido (1..TRV_DESTS, uno propio)
uint8_t trvSelTier = 0;        // duracion elegida (0..2)
uint8_t trvMapPrev = 0;        // el mapa que ensena la ventana (yardEnsureMap lo carga mientras el panel esta abierto); solo RAM
uint8_t boxAwaySlot = 0;       // la caja: se toco un bicho de viaje (aviso de unos segundos)
uint32_t boxAwayUntil = 0;
bool returnBannerShown = false;  // el ultimo frame enseno el aviso de vuelta: un toque lo cierra
bool returnOpen = false;         // el jugador toco al viajero del patio (el del "!"): el aviso se ve
uint32_t returnOpenAt = 0;       // cuando se abrio el aviso: los toques de justo despues no lo cierran
#define RETURN_TAP_LOCK_MS 700
// el menu de un bicho (v3.3, yard_panels.h): tocar a un bicho del patio abre a la derecha, encima del patio,
// sus acciones (dormir, comida, mimo, ficha y evolucionar / despedirse) y la lista de comida
uint8_t petMenuOpen = 0;        // hueco de casa + 1 (0 = cerrado)
uint8_t petMenuView = 0;        // 0 las acciones, 1 la lista de comida
uint8_t petFoodPage = 0;        // pagina de la lista de comida
YardAside yardAside[HOME_SLOTS];  // con el menu abierto, el bicho que quede debajo se aparta (yard.h, yardMenuHold)
int8_t pmShakeIdx = -1;         // la fila que tiembla al negarse (como las tarjetas de los paneles)
uint32_t pmShakeUntil = 0;
#define PET_MENU_IDLE_MS 30000UL  // sin tocar nada, el menu se cierra solo (y el bicho vuelve a pasear)
// el selector de bicho (v3.3): quien juega un minijuego, o a quien se le da un objeto de la mochila, cuando
// los dos de casa pueden
enum : uint8_t { PICK_NONE = 0, PICK_GAME, PICK_ITEM };
uint8_t petPick = PICK_NONE;
uint8_t petPickArg = 0;         // el juego (PP_*) o el objeto (IT_*)
void openTravel();
void travelTap(int16_t x, int16_t y);
void renderTravel();
void openPlayPanel();
void closePetMenu();
void panelTap(int16_t x, int16_t y);
void renderPanel();
void yardShowFull();
extern bool gYardFull;  // (con el fondo del patio, mas abajo)

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

// cascada (v1.43, entrena la defensa): el dedo puesto sube la concentracion (zengame.h)
bool zenOpen = false;
ZenState zen;
uint32_t zenIntroUntil = 0, zenOverUntil = 0, zenLastTick = 0;
bool zenHold = false;        // dedo puesto ahora (handleTouch)
bool zenTouched = false;     // ya se toco una vez: se quita la explicacion
bool zenNoSave = false;      // sesion del comando ZEN: ni premio ni record
bool zenNewHi = false;
int8_t zenSerialHold = -1;   // comando HOLD: 1 dedo puesto, 0 quitado, -1 el dedo de verdad
uint8_t zenGainShown = 0, zenBonusShown = 0;
bool zenDriftAnnounced = false;  // ya sono el aviso de que la franja empieza a moverse
uint16_t zenSurgesHeard = 0;
uint32_t zenDrawUs = 0, zenDrawMaxUs = 0, zenFlushUs = 0;  // lo que tarda un frame de la cascada (ZEN?), sin el volcado
// colores de la cascada (aqui y no junto a su funcion: el conversor .ino pone los prototipos arriba)
struct ZenPal { uint16_t rock, rockDk, moss, water, waterLt, pool, foam; };

// carrera en trineo (v1.44, entrena la velocidad): inclinar la placa esquiva (dashgame.h)
bool dashOpen = false;
DashState dash;
DashCal dashCal;
uint32_t dashCalStart = 0, dashOverUntil = 0, dashLastTick = 0, dashRunStart = 0;
uint8_t dashCalRetries = 0, dashGainShown = 0, dashHitsSeen = 0;
bool dashCalibrating = false;  // cuenta atras: se mide la inclinacion "recta" del jugador
bool dashShaky = false;        // la ultima cuenta temblaba: "Quieta un momento..."
bool dashNoSave = false;       // carrera del comando DASH: ni premio ni record
bool dashNewHi = false;
float dashBase = 0;            // inclinacion de la calibracion (grados)
bool dashSerialSteerOn = false;  // comando STEER: un angulo fijo en vez de la placa
float dashSerialSteer = 0;
int dashSledPx = 80;           // ancho dibujado del trineo (sigue al sprite)
#define DASH_TRAIL 12          // huella de los patines: x del trineo cada DASH_TRAIL_STEP px
#define DASH_TRAIL_STEP 24.0f
float dashTrailX[DASH_TRAIL], dashTrailY[DASH_TRAIL];
uint32_t dashTrailN = 0;
uint32_t dashDrawUs = 0, dashDrawMaxUs = 0;
struct DashPal { uint16_t track, outside, edge, mark, rock, log, bush, sled, sledDk, runner; };

// definidas al final; se declaran aqui porque el conversor .ino de PlatformIO no
// genera prototipos para funciones que devuelven punteros
const char *eggMsg();
const char *battlePhaseName();  // comando TAP

// las 9 especies con sprite propio en flash (respaldo sin SD): dex -> indice
int flashIdxForDex(int16_t dex) {
  static const int8_t IDX[10] = { -1, 3, 4, 5, 0, 1, 2, 6, 7, 8 };
  return (dex >= 1 && dex <= 9) ? IDX[dex] : -1;
}

#define CX 233  // centro de la pantalla redonda
#define CY 233
#define PET_CY 202  // centro vertical del sprite

static const uint16_t INK_K = 0x18C4;  // spriteColor('k')

// indices de los botones del dock del patio para los efectos (fx.h): Jugar 1, Tienda 2 (v3.3, era Cuidar), Mochila 3
#define BTN_SHOP 2
#define BTN_BAG 3

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
// boton "Abrir un huevo (N)" de la casa vacia con huevos en la mochila (v1.40), bajo el huevo gris
#define VAC_BTN_W 260
#define VAC_BTN_H 46
#define VAC_BTN_X (CX - VAC_BTN_W / 2)
#define VAC_BTN_Y 318
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
uint8_t dimStage = 0;        // 0 despierto, 1 atenuado (90s), 2 salvapantallas o casi apagado (2min)
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
// v3.0: uno por hueco de casa (los efectos salen sobre su bicho: FxEvent/FxOverlay llevan el hueco)
FxWatch fxLevelW[HOME_SLOTS], fxBondW[HOME_SLOTS], fxMistakeW[HOME_SLOTS], fxTrainW[HOME_SLOTS][3];
// avisos sonoros (v1.50): una caca nueva y una necesidad que se vacia (comida, felicidad,
// energia, en el orden de fxNeedW). Suenan tambien con la pantalla atenuada, en el salvapantallas o
// apagada: son para enterarse sin mirar
FxWatch fxPoopW[HOME_SLOTS];
FxZeroWatch fxNeedW[HOME_SLOTS][3];
FxGate fxBondGate[HOME_SLOTS];
bool fxWasEgg[HOME_SLOTS] = { false, false };  // para ver la eclosion
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

  pet.timedEggs = true;  // Y3: los huevos se ponen en el patio y eclosionan a las 2 h (EGG_HATCH_MIN)
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

// los sprites se recargan aunque sea la misma especie (v3.0: los de los dos huecos y el B/N). (Aqui y no
// junto a sus variables: el conversor del .ino pone los prototipos antes de la primera funcion)
void monReset() { monFor[0] = monFor[1] = monBwFor = -2; }

// v3.0: hay algo abierto que trabaja con el bicho del foco: no se le cambia (un juego, un combate, la ficha,
// un menu, el teclado, un dialogo, un movimiento de la caja, el Revivir o una despedida; v3.3: el menu del
// bicho y el selector de bicho)
bool uiModalOpen() {
  return gameOpen || sackOpen || zenOpen || dashOpen || battleOpen() || encOpen() || teamOpen || stagesOpen ||
         cardOpen || userOpen || kbOpen || clockOpen || bagOpen || panelKind || boxOpen || boxMove != BOXMV_NONE || galleryOpen ||
         confirmUntil || choiceKind || itemConfirm || petMenuOpen || petPick || pet.isDying() || pet.ceremony || pet.evolving() ||
         pet.home(0).isDying() || pet.home(1).isDying() || pet.home(0).ceremony || pet.home(1).ceremony;
}

// la UI cambia el foco solo por aqui: false si hay algo abierto (o el hueco no existe)
bool uiFocus(uint8_t s) {
  if (s >= HOME_SLOTS) return false;
  if (s == pet.focusSlot) return true;
  if (uiModalOpen()) return false;
  pet.focus(s);
  Serial.printf("FOCUS %u\n", s);
  return true;
}

// en cada vuelta del loop: el foco va al que se muere (el dialogo del Revivir es del foco) y, con el
// del foco en un hueco vacio, al otro si tiene a alguien
void homeFocusTick() {
  if (pet.isDying() || pet.ceremony) return;
  for (uint8_t s = 0; s < HOME_SLOTS; s++) {
    const PetLife &l = pet.home(s);
    if (s != pet.focusSlot && (l.isDying() || l.ceremony)) {
      closeForDying();
      pet.focus(s);
      return;
    }
  }
  if (pet.isVacant() && !pet.other.isVacant() && !uiModalOpen()) {
    pet.focus(pet.focusSlot ^ 1);
  }
}

// carga/descarga los sprites de SD cuando cambia la especie (v3.0: el de cada hueco de casa, y el B/N de
// respaldo solo para el del foco)
void ensureMon() {
  // los ficheros recien recibidos pueden incluir un thumbs.bin nuevo, que se
  // cargaba solo en setup(): sin esto no se veia en la galeria hasta reiniciar
  if (sdDirty) {
    thumbs.unload();
    thumbs.load();
    sdEpoch++;
    monReset();
  }
  sdDirty = false;
  for (uint8_t s = 0; s < HOME_SLOTS; s++) {
    const PetLife &l = pet.home(s);
    if (l.speciesId == monFor[s] && l.shiny == monShinyFor[s]) continue;
    monFor[s] = l.speciesId;
    monShinyFor[s] = l.shiny;
    homeMon[s].unload();
    if (l.speciesId >= 1 && l.speciesId <= DEX_COUNT) homeMon[s].load((uint8_t)l.speciesId, l.shiny);  // principal: PMD
  }
  int16_t bw = (!pmd.loaded && pet.speciesId >= 1 && pet.speciesId <= DEX_COUNT) ? pet.speciesId : -1;
  if (bw != monBwFor || (bw >= 0 && pet.shiny != monBwShiny)) {
    monBwFor = bw;
    monBwShiny = pet.shiny;
    mon.unload();
    if (bw >= 0) mon.load((uint8_t)bw, pet.shiny);  // respaldo: B/N
  }
}

void loop() {
  uint32_t now = millis();
  // la noche (v1.63): con un juego, combate, evolucion o movimiento de la caja en marcha no se duerme
  // solo a las 23 (lo hace al acabar)
  pet.nightHold = pet.home(0).evolving() || pet.home(1).evolving() || gameOpen || sackOpen || zenOpen || dashOpen ||
                  battleOpen() || boxMove != BOXMV_NONE || encOpen() || teamWild;
  pet.update(now);

  // el amplificador sigue al estado de sueno (la llamada sale sola si no cambia).
  // Aqui cubre todas las vias: el boton de luz, el sueno nocturno y el que llega
  // aplicado desde la progresion offline. v3.0: con los dos de casa dormidos
  audioSetSleeping(pet.homeAsleep());
  // se desperto solo a las 8: suena, tambien con la pantalla apagada (despues de audioSetSleeping,
  // que dormido tiene el amplificador apagado)
  if (pet.morningWake) {
    pet.morningWake = false;
    sfxPlay(SFX_MORNING);
  }
  // v2.2: algo se mueve en el patio. Suena una vez, tambien con la pantalla apagada; dormido espera a que se
  // despierte (el amplificador esta apagado), y en pleno combate, a que acabe
  if (pet.encSoundPending() && !pet.homeAsleep() && !battleOpen()) {
    pet.takeEncSound();
    sfxPlay(SFX_WILD);
  }
  fxCollect(now);  // subidas de nivel, medallas, metas... (fx.h)

  // avisa con un sonido cuando el bicho pasa a estar listo para evolucionar
  // (incluye el caso de cumplir al despertar). canEvolveNow es false durmiendo.
  static bool wasEvoReady[HOME_SLOTS] = { false, false };
  for (uint8_t s = 0; s < HOME_SLOTS; s++) {  // v3.0: cada bicho de casa, el suyo
    bool evoReady = pet.home(s).wantEvolveButton();
    if (evoReady && !wasEvoReady[s]) sfxPlay(SFX_MEDAL);
    wasEvoReady[s] = evoReady;
  }
  // se muere de abandono (v1.60): se cierra lo que hubiera abierto para que el dialogo se vea (el
  // sonido lo pone el bicho al empezar a morirse). v3.0: cualquiera de los dos; el foco va a el
  static bool wasDying[HOME_SLOTS] = { false, false };
  for (uint8_t s = 0; s < HOME_SLOTS; s++) {
    bool dyingNow = pet.home(s).isDying();
    if (dyingNow && !wasDying[s]) closeForDying();
    if (dyingNow && boxOpen && boxMove == BOXMV_NONE) closeBox();  // empezo a morirse en mitad de un movimiento
    wasDying[s] = dyingNow;
    if (pet.takeDiedNow(s)) sfxPlay(SFX_BYE);  // v1.64: paso la hora sin Revivir (el dialogo pasa a decir que murio)
  }
  homeFocusTick();
  petMenuCheck();  // v3.3: el menu del bicho se cierra si su bicho ya no puede usarlo (o si se dejo abierto)

  handleTouch();
  zenTick(millis());  // cascada: fisica con el dedo de ahora, en cada vuelta
  static KeyDebounce backKey;  // tecla USER = atras
  if (backKey.update(digitalRead(BOARD_BTN_USER) == BOARD_BTN_PRESSED_LEVEL, millis())) onBack();
  static KeyDebounce bootKey;  // tecla BOOT = brillo
  if (bootKey.update(digitalRead(BOARD_BTN_BOOT) == BOARD_BTN_PRESSED_LEVEL, millis())) onBootKey();
  if (briSaveAt && !timeLeft(briSaveAt)) saveBright();
  handleSerial();
  ensureMon();
  imuTick(now);  // pasos, sacudidas, boca abajo
  dashTick(millis());  // carrera: calibracion o fisica con la inclinacion que acaba de leer

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
  if (pet.savePending() && (screenOff || dimStage >= 1 || pet.homeAsleep())) {
    pet.flushSave();
  }

  // anota la hora real cada 30 s (se persiste en cada save del juego); con la cuenta atras de un viaje a la
  // vista, cada segundo (Y5b: lo que se ve tiene que bajar de uno en uno)
  static uint32_t lastClock = 0;
  if (now - lastClock > (panelKind == PANEL_TRAVEL && pet.travelling() ? 1000UL : 30000UL)) {
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
  if (pet.awaitingStarter() || pet.ceremony || pet.evolving() || gameOpen || sackOpen || zenOpen || dashOpen || battleOpen()) return;
  if (galleryOpen || boxOpen || stagesOpen || teamOpen || cardOpen || userOpen || bagOpen || kbOpen || clockOpen || panelKind || encOpen()) return;  // el bicho no esta en pantalla
  // v3.0: a los dos de casa: los despiertos se marean (y los huevos se tambalean; el del patio dice lo que le
  // queda) y despues los dormidos se despiertan de mal humor
  if (pet.shaken()) yardEggInfo();
  pet.wakeGrumpy();
}

// Boca abajo y quieta 3 s (FR-2.2): de noche el bicho se va a dormir; de dia solo
// se apaga la pantalla. Al levantarla se enciende (y quien se durmio sigue dormido).
void onFaceDown(bool down) {
  if (screenOff) return;
  if (!down) { wakeUI(); return; }
  if (pet.awaitingStarter() || pet.ceremony || pet.evolving() || gameOpen || sackOpen || zenOpen || dashOpen || battleOpen()) return;
  int h = sceneHour();
  if (h >= 20 || h < 6) pet.sleepDeferred();  // v3.0: los dos de casa (los que esten despiertos)
  lastInteract = millis() - POWER_DIM2_MS - 1;  // casi apagada, como tras 2 min sin tocar
  gFaceDown = true;  // y sin salvapantallas: nadie la mira
}

// estado de consumo actual (ver power.h)
PowerCtx powerCtx(uint32_t now) {
  PowerCtx c;
  c.idleMs = idleSince(now, lastInteract);
  c.screenOff = screenOff;
  c.sleeping = pet.homeAsleep();  // v3.0: los dos de casa
  c.gameOpen = gameOpen || sackOpen || zenOpen || dashOpen || battleOpen() || boxMove || encOpen();  // combate, luz de la caja y encuentro (v2.2), a 85 ms
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
//  - salvapantallas: el mismo truco que el comando SAVER (como tras 2 min sin tocar), asi que
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
  if (encOpen()) closeEncUi();  // v2.2: el encuentro sigue en el patio (o esperando la Poke Ball)
  zenOpen = false;  // la cascada tambien se abandona (sin premio)
  if (dashOpen) closeDash();  // y la carrera
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
// apagada (2 min sin tocar), y sin USB se duerme la CPU (light sleep) hasta el proximo frame o
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

  if (line == "HATCH") {  // el huevo de casa eclosiona ya (tambien el del patio)
    pet.hatchNow();
    Serial.println("DONE");
  } else if (line == "EGGT" || line.startsWith("EGGT ")) {  // EGGT <min>: el huevo del patio, puesto hace min minutos
    if (line.length() > 5) {
      long m = line.substring(5).toInt();
      pet.dbgEggElapsed((uint32_t)(m < 0 ? 0 : m));
      yardEggInfo();
      wakeUI();
    }
    Serial.printf("egg yard=%d placed=%lu left=%u saved=%u cracks=%u\n", pet.yardEgg() ? 1 : 0,
                  (unsigned long)pet.eggPlacedEpoch(), pet.yardEgg() ? pet.eggMinutesLeft() : 0, pet.eggSavedMinutes(),
                  pet.eggCracks());
    Serial.println("DONE");
  } else if (line.startsWith("EGGD ")) {  // EGGD <dex> (v2.4): el huevo de casa pasa a llevar a dex, con su color
    pet.dbgEggSpecies((int16_t)line.substring(5).toInt());
    wakeUI();
    Serial.printf("egg dex=%d\n", pet.eggSpecies());
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
  } else if (line.startsWith("AGE ")) {  // AGE <min>: edad del bicho (pruebas: 4320 = lista para despedirse)
    long n = line.substring(4).toInt();
    if (n >= 0 && !pet.isEgg()) {
      pet.dbgSetAge((uint32_t)n);
    }
    Serial.printf("age=%lu farewell=%d\n", (unsigned long)pet.ageMinutes, pet.wantFarewellButton() ? 1 : 0);
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
    Serial.printf("streak=%u bond=%u counted=0x%02X\n", pet.streak, pet.bond, pet.counted);
    Serial.println("DONE");
  } else if (line == "BYE") {
    pet.startFarewell();
    Serial.println("DONE");
  } else if (line == "RELEASE") {  // soltar sin el dialogo (v1.40)
    pet.release();
    Serial.println("DONE");
  } else if (line == "BEEP") {
    sfxPlay(SFX_HATCH);  // prueba de audio
    Serial.println("DONE");
  } else if (line == "BACK") {
    onBack();               // como pulsar la tecla USER
    Serial.println("DONE");
  } else if (line == "ABANDON") {
    pet.dbgAbandon();  // todo a cero: empieza a apagarse el vinculo (v1.60)
    Serial.println("DONE");
  } else if (line.startsWith("NEGLECT ")) {  // v1.60: adelanta la cuenta del abandono (minutos)
    long m = line.substring(8).toInt();
    pet.dbgNeglect((uint16_t)(m < 0 ? 0 : (m > 60 ? 60 : m)));
    Serial.printf("bond=%u ngl=%u dth=%u\n", pet.bond, pet.neglectMinutes(), pet.deathMinutes());
    Serial.println("DONE");
  } else if (line.startsWith("DYING ")) {  // v1.64: minutos ya pasados muriendose (60 = paso la hora)
    long m = line.substring(6).toInt();
    pet.dbgDyingMin((uint8_t)(m < 0 ? 0 : (m > REVIVE_LIMIT_MIN ? REVIVE_LIMIT_MIN : m)));
    Serial.printf("dying=%d dym=%u left=%u late=%d\n", pet.isDying() ? 1 : 0, pet.dyingMinutes(),
                  pet.reviveMinutesLeft(), pet.reviveExpired() ? 1 : 0);
    Serial.println("DONE");
  } else if (line == "NIGHT") {  // v1.63: la noche (con TIME para saltar a las 21:59, 22:59 o 07:59)
    Serial.printf("hour=%d sleepy=%d sleep=%d nslp=%d nawake=%u bedn=%lu night=%lu bond=%u\n", pet.localHour(),
                  pet.sleepy() ? 1 : 0, pet.sleeping ? 1 : 0, pet.nightSlept() ? 1 : 0, pet.nightAwakeMinutes(),
                  (unsigned long)pet.bedNightId(), (unsigned long)Pet::nightId(pet.lastSeenEpoch),
                  pet.bond);
    Serial.println("DONE");
  } else if (line == "DIE") {  // v1.60: se muere ya (el dialogo del Revivir)
    pet.startDying();
    Serial.printf("dying=%d degg=%d revive=%u\n", pet.isDying() ? 1 : 0, pet.deathEggSpecies(), pet.items[IT_REVIVE]);
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
  } else if (line.startsWith("YARDBG")) {  // YARDBG <n>: fuerza el mapa n del patio (0 = el elegido, 255 = sin mapa)
    if (line.length() > 7) yardForceMap((uint8_t)line.substring(7).toInt());
    wakeUI();
    yardReport();
    Serial.println("DONE");
  } else if (line.startsWith("YARDFULL")) {  // YARDFULL 0|1: el patio a toda la pantalla (v2.5) o en su circulo negro
    if (line.length() > 9) gYardFull = line.substring(9).toInt() != 0;
    wakeUI();
    Serial.printf("yardfull=%u (build %u)\n", gYardFull ? 1 : 0, (unsigned)TP_FULL_YARD);
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
  } else if (line == "SAVER") {  // como tras 2 min sin tocar: salvapantallas
    lastInteract = millis() - POWER_DIM2_MS - 1;
    gFaceDown = false;
    Serial.println("DONE");
  } else if (line == "PERF") {  // cuanto tarda el paisaje (drawScene) en dibujarse, en us
    Serial.printf("scene=%luus max=%luus biome=%u fx=%luus fxmax=%luus\n", (unsigned long)gSceneUs,
                  (unsigned long)gSceneMaxUs, homeBiome(), (unsigned long)gFxUs, (unsigned long)gFxMaxUs);
    // el patio: scene = copiar el mapa al lienzo (y re-expandirlo si cambio); pets = bichos, cosas y HUD
    Serial.printf("yard=%d pets=%luus petsmax=%luus n=%u map=%u\n", yardHomeNow() ? 1 : 0, (unsigned long)gYardPetsUs,
                  (unsigned long)gYardPetsMaxUs, yardPetsShown(), yardMapNow());
    gYardPetsMaxUs = 0;
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
  } else if (line == "POOP") {  // una caca mas (tope 3), como en tick(): suena SFX_POOP (v1.50)
    if (!pet.isEgg()) pet.dbgAddPoop();
    Serial.printf("poops=%u sleep=%d\n", pet.poops, pet.sleeping ? 1 : 0);
    Serial.println("DONE");
  } else if (line.startsWith("NEED ")) {  // NEED <i> <v>: i = 0 comida, 2 felicidad, 3 energia (el 1, la higiene, ya no existe)
    int sp = line.indexOf(' ', 5);
    int i = line.substring(5, sp < 0 ? line.length() : sp).toInt();
    int v = sp < 0 ? 0 : line.substring(sp + 1).toInt();  // sin valor: 0 (suena SFX_ALERT, v1.50)
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    uint8_t *need[4] = { &pet.fullness, nullptr, &pet.joy, &pet.energy };  // v3.1: el 1 (higiene) se quito
    if (i == 1) Serial.println("hygiene removed in v3.1 (NEED: 0 food, 2 joy, 3 energy)");
    else if (i >= 0 && i < 4 && !pet.isEgg()) *need[i] = (uint8_t)v;
    Serial.printf("com=%u fel=%u ene=%u sleep=%d\n", pet.fullness, pet.joy, pet.energy, pet.sleeping ? 1 : 0);
    Serial.println("DONE");
  } else if (line.startsWith("VOL")) {  // VOL: nivel de volumen; VOL <n>: lo fija (como la pildora)
    if (line.length() > 4) {
      int lv = line.substring(4).toInt();
      if (lv >= 0 && lv < VOL_LEVELS) {
        audioSetLevel((uint8_t)lv);
        sfxPlay(SFX_TAP);
      }
    }
    Serial.printf("vol=%u/%u reg=0x%02X\n", audioLevel(), VOL_LEVELS - 1, audioLevel() ? volDacReg(audioLevel()) : 0);
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
  } else if (line == "BATTLE" || line.startsWith("STAGE ")) {  // STAGE <1-8>: esa fase, aunque este cerrada
    long s = line == "BATTLE" ? 1 : line.substring(6).toInt();  // (BATTLE = STAGE 1)
    if (s < 1 || s > STAGE_COUNT) {
      Serial.printf("stage: 1-%u\n", STAGE_COUNT);
    } else {
      startStageAuto((uint8_t)(s - 1));  // el ultimo equipo, o el de casa y los de la caja
      if (!battleOpen()) Serial.println("stage: no puede (nadie puede combatir o ya en combate)");
    }
    Serial.println("DONE");
  } else if (line == "STAGEDAY") {  // los premios del jefe de hoy, otra vez
    pet.clearStageDay();
    Serial.println("DONE");
  } else if (line == "YARDUI" || line.startsWith("YARDUI ")) {  // YARDUI [pagina]: muestra del kit del patio (v2.0)
    wakeUI();
    yardDemoPage = line.length() > 7 ? (uint8_t)(line.substring(7).toInt() % YDEMO_PAGES) : 0;
    yardDemoOpen = true;
    Serial.printf("yardui page=%u\n", yardDemoPage);
    Serial.println("DONE");
  } else if (line == "BADGES" || line.startsWith("BADGES ")) {  // BADGES <mascara>: las insignias (0-255)
    if (line.length() > 7) pet.setBadges((uint8_t)line.substring(7).toInt());
    Serial.printf("badges=%u hoy=%u equipo=%lu,%lu,%lu,%lu\n", pet.badgeMask(), pet.stageRewardMask(),
                  (unsigned long)pet.teamUids()[0], (unsigned long)pet.teamUids()[1],
                  (unsigned long)pet.teamUids()[2], (unsigned long)pet.teamUids()[3]);
    Serial.println("DONE");
  } else if (line == "BALL" || line.startsWith("BALL ")) {  // BALL <puntos>: el minijuego con esa puntuacion
    long n = line.length() > 5 ? line.substring(5).toInt() : 0;  // para notar 1x, 1,5x, 2x sin llegar jugando
    if (n < 0) n = 0;
    if (n > 255) n = 255;
    wakeUI();
    panelKind = PANEL_NONE;
    startGame();
    if (!gameOpen) {
      Serial.println("ball: no puede (huevo, dormido o despedida)");
    } else {
      gameScore = (uint8_t)n;
      gameNoSave = true;  // ni record ni premio: el guardado del bicho no cambia
      Serial.printf("ball: score=%ld speed=%.2f\n", n, ballSpeed((uint16_t)n));
    }
    Serial.println("DONE");
  } else if (line == "DASH?") {  // estado de la carrera
    Serial.printf("dash: open=%d cal=%d retries=%u over=%d t=%lu speed=%.2f dist=%.0f px=%.1f steer=%.2f roll=%+.1f base=%+.1f dodged=%u hits=%u invuln=%d sled=%d draw=%luus max=%luus\n",
                  dashOpen, dashCalibrating, dashCalRetries, dashOverUntil ? 1 : 0, (unsigned long)dash.t, dashSpeed(dash.t),
                  dash.dist, dash.px, dash.steer, imuRollFast(), dashBase, dash.dodged, dash.hits, dashInvuln(dash) ? 1 : 0,
                  dashSledPx, (unsigned long)dashDrawUs, (unsigned long)dashDrawMaxUs);
    dashDrawMaxUs = 0;
    Serial.println("DONE");
  } else if (line == "DASH" || line.startsWith("DASH ")) {  // DASH [s]: la carrera sin premio, a la velocidad del segundo s
    long sec = line.length() > 5 ? line.substring(5).toInt() : 0;
    if (sec < 0) sec = 0;
    if (sec > 120) sec = 120;
    wakeUI();
    panelKind = PANEL_NONE;
    startDash();
    if (!dashOpen) {
      Serial.println("dash: no puede (huevo, dormido, despedida, sin IMU u otro juego abierto)");
    } else {
      dashNoSave = true;  // ni record ni premio: el guardado del bicho no cambia
      if (sec) {  // sin calibrar (la linea base, la inclinacion de ahora) y a esa velocidad
        dashCalibrating = false;
        dashBase = imuRollFast();
        dash.t = (uint32_t)sec * 1000;
        dashLastTick = dashRunStart = millis();
      }
      Serial.printf("dash: t=%lu speed=%.2f sled=%d\n", (unsigned long)dash.t, dashSpeed(dash.t), dashSledPx);
    }
    Serial.println("DONE");
  } else if (line.startsWith("STEER ")) {  // STEER <grados> / STEER OFF: inclinacion fija para la carrera
    String a = line.substring(6);
    a.trim();
    dashSerialSteerOn = a != "OFF";
    dashSerialSteer = dashSerialSteerOn ? (float)atof(a.c_str()) : 0;
    Serial.printf("steer=%s %.1f -> %.2f\n", dashSerialSteerOn ? "on" : "off", dashSerialSteer, dashSteer(dashSerialSteer));
    Serial.println("DONE");
  } else if (line == "ZEN?") {  // estado de la cascada
    Serial.printf("zen: open=%d intro=%d over=%d t=%lu c=%.1f vc=%.1f band=%.1f-%.1f dir=%d next=%lu surges=%u focus=%u.%u drift=%u.%u bonus=%u hold=%d serial=%d nosave=%d draw=%luus max=%luus flush=%luus\n",
                  zenOpen, timeLeft(zenIntroUntil) ? 1 : 0, zenOverUntil ? 1 : 0, (unsigned long)zen.t, zen.c, zen.vc,
                  zenCenter(zen.t, zen.driftDir) - zenBandHalf(zen.t), zenCenter(zen.t, zen.driftDir) + zenBandHalf(zen.t),
                  zen.driftDir, (unsigned long)zen.nextSurge, zen.surges,
                  zenFocusTenths(zen) / 10, zenFocusTenths(zen) % 10, zenDriftTenths(zen) / 10, zenDriftTenths(zen) % 10,
                  zenBonus(zenDriftTenths(zen)), zenHold, zenSerialHold, zenNoSave,
                  (unsigned long)zenDrawUs, (unsigned long)zenDrawMaxUs, (unsigned long)zenFlushUs);
    zenDrawMaxUs = 0;
    Serial.println("DONE");
  } else if (line == "ZEN" || line.startsWith("ZEN ")) {  // ZEN [s]: la cascada sin premio, desde el segundo s
    long sec = line.length() > 4 ? line.substring(4).toInt() : 0;
    if (sec < 0) sec = 0;
    if (sec > (long)(ZEN_SESSION_MS / 1000) - 1) sec = ZEN_SESSION_MS / 1000 - 1;
    wakeUI();
    panelKind = PANEL_NONE;
    startZen();
    if (!zenOpen) {
      Serial.println("zen: no puede (huevo, dormido, despedida u otro juego abierto)");
    } else {
      zenNoSave = true;  // ni record ni premio: el guardado del bicho no cambia
      if (sec) {  // se salta la intro y empieza con la franja y los golpes de ese segundo
        zenIntroUntil = 0;
        zen.t = (uint32_t)sec * 1000;
        while (zen.nextSurge <= zen.t) zen.nextSurge += zenSurgeGap(zen.nextSurge, zen.rng);
      }
      Serial.printf("zen: t=%lu band=%.1f kick=%.1f\n", (unsigned long)zen.t, 2 * zenBandHalf(zen.t), zenSurgeKick(zen.t));
    }
    Serial.println("DONE");
  } else if (line.startsWith("HOLD ")) {  // HOLD 1 / HOLD 0 / HOLD OFF: el dedo de la cascada por serie
    String a = line.substring(5);
    a.trim();
    zenSerialHold = a == "1" ? 1 : (a == "0" ? 0 : -1);
    if (zenSerialHold == 1) zenTouched = true;
    Serial.printf("hold=%d\n", zenSerialHold);
    Serial.println("DONE");
  } else if (line == "EGG" || line.startsWith("EGG ")) {  // huevos (v1.39); EGG <n>: n en la mochila (pruebas)
    if (line.length() > 4) {
      long n = line.substring(4).toInt();
      pet.setBagEggs((uint8_t)(n < 0 ? 0 : (n > ITEM_MAX ? ITEM_MAX : n)));
      ensureMon();
      fxArmWatches();
    }
    eggReport();
    Serial.println("DONE");
  } else if (line == "REVIVES" || line.startsWith("REVIVES ")) {  // Revivir (v1.59); REVIVES <n>: n en la mochila
    if (line.length() > 8) {
      long n = line.substring(8).toInt();
      pet.setRevives((uint8_t)(n < 0 ? 0 : n));
    }
    Serial.printf("revive=%u\n", pet.items[IT_REVIVE]);
    Serial.println("DONE");
  } else if (line == "GIFTS" || line.startsWith("GIFT ")) {  // regalos (v1.55); GIFT <objeto>: uno mas
    if (line.startsWith("GIFT ")) {
      long it = line.substring(5).toInt();
      bool ok = it >= 0 && it < IT_COUNT && pet.addGift((uint8_t)it);
      Serial.println(ok ? "gift added" : "gift refused (item 0-15, at most 8)");
      pet.flushSave();
    }
    Serial.printf("gifts n=%u:", pet.giftCount());
    for (uint8_t i = 0; i < pet.giftCount(); i++) Serial.printf(" %u", pet.giftItem(i));
    Serial.printf(" canOpen=%d\n", pet.canOpenGift() ? 1 : 0);
    Serial.println("DONE");
  } else if (line.startsWith("TAP ")) {  // TAP <x> <y>: un toque como el de un dedo (pruebas por serie)
    String a = line.substring(4);
    a.trim();
    int sp = a.indexOf(' ');
    int16_t x = (int16_t)a.substring(0, sp).toInt(), y = sp > 0 ? (int16_t)a.substring(sp + 1).toInt() : 0;
    wakeUI();
    if (battleOpen()) battleTap(x, y);  // como handleTouch: el combate se lleva los toques
    else if (dashOpen) { if (y < 72 && !dashOverUntil) closeDash(); }  // la carrera: solo la cabecera
    else if (zenOpen) {  // la cascada: la cabecera abandona; otro toque, un toque corto
      if (y < 72 && !zenOverUntil) zenOpen = false;
      else zenTouched = true;
    }
    else if (sackOpen) { if (y < 72) sackOpen = false; else sackTap(); }
    else onTap(x, y);
    Serial.printf("tap %d %d battle=%d phase=%u %s\n", x, y, battleOpen() ? 1 : 0, battlePhase(), battlePhaseName());
    Serial.println("DONE");
  } else if (line == "BOX") {  // la caja (v1.37): cada hueco con uid, especie, nivel y CRC
    boxReport();
    Serial.println("DONE");
  } else if (line.startsWith("STORE ") || line.startsWith("WAKE ")) {  // STORE <k> [s] / WAKE <k> [s], k = 0-5, s = hueco de casa
    // v3.0: sin s, STORE guarda el del foco y WAKE despierta al primer hueco vacio (o con huevo), si no al del foco
    bool toStore = line.startsWith("STORE ");
    String a = line.substring(toStore ? 6 : 5);
    a.trim();
    const char *c = a.c_str();
    int k = (a.length() >= 1 && c[0] >= '0' && c[0] <= '9' && (a.length() == 1 || c[1] == ' ')) ? c[0] - '0' : -1;
    int hs = (a.length() == 3 && (c[2] == '0' || c[2] == '1')) ? c[2] - '0' : -1;
    if (hs < 0 && k >= 0 && k < BOX_SLOTS) hs = toStore ? pet.focusSlot : (boxWakeTarget((uint8_t)k) >= 0 ? boxWakeTarget((uint8_t)k) : pet.focusSlot);
    bool ok = k >= 0 && k < BOX_SLOTS && hs >= 0 &&
              (toStore ? pet.store((uint8_t)k, (uint8_t)hs) : pet.wake((uint8_t)k, (uint8_t)hs));
    if (ok) {
      boxMoveHome = (uint8_t)hs;
      boxAfterMove();  // como al tocar SI, sin la luz
    }
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
  } else if (line == "ENC?") {  // v2.2: el encuentro de hoy (tiradas, hora de la base, el salvaje)
    encReport();
    Serial.println("DONE");
  } else if (line == "ENC" || line.startsWith("ENC ")) {  // ENC [dex]: sale ya (dex: esa especie)
    long d = line.length() > 4 ? line.substring(4).toInt() : 0;
    if (!pet.encSpawnNow((int16_t)(d >= 1 && d <= DEX_COUNT ? d : 0))) Serial.println("enc: sin hora (pon TIME)");
    yardEncX = -1;  // se coloca de nuevo
    wakeUI();
    encReport();
    Serial.println("DONE");
  } else if (line == "ENCEND") {  // se va sin mas
    if (encOpen()) closeEncUi();
    pet.encClear();
    encReport();
    Serial.println("DONE");
  } else if (line == "BALLS" || line.startsWith("BALLS ")) {  // BALLS <n>: Poke Balls en la mochila
    if (line.length() > 6) pet.setBalls((uint8_t)constrain(line.substring(6).toInt(), 0, ITEM_MAX));
    Serial.printf("balls=%u capEggs=%u eggs=%u\n", pet.items[IT_BALL], pet.captureEggs(), pet.items[IT_EGG]);
    Serial.println("DONE");
  } else if (line == "TRAVELEND") {  // TRAVELEND: el viaje termina ya (y se cobra)
    pet.travelFinishNow();
    travelReport();
    Serial.println("DONE");
  } else if (line == "TRAVELT" || line.startsWith("TRAVELT ")) {  // TRAVELT <min>: el viaje salio hace <min> minutos mas
    if (line.length() > 8) {
      long m = line.substring(8).toInt();
      pet.dbgTravelElapsed((uint32_t)(m < 0 ? 0 : m));
      wakeUI();
    }
    travelReport();
    Serial.println("DONE");
  } else if (line == "TRAVEL" || line.startsWith("TRAVEL ")) {  // TRAVEL [hueco mapa duracion]: ver / empezar un viaje
    if (line.length() > 7) {
      int sl = -1, de = 0, ti = 0;
      if (sscanf(line.c_str() + 7, "%d %d %d", &sl, &de, &ti) == 3) {
        bool ok = sl >= 0 && sl < 256 && de >= 0 && de < 256 && ti >= 0 && ti < 256 &&
                  pet.startTravel((uint8_t)sl, (uint8_t)de, (uint8_t)ti);
        Serial.printf("travel start %d %d %d -> %s\n", sl, de, ti, ok ? "ok" : "no");
        wakeUI();
      } else {
        Serial.println("travel: TRAVEL <hueco 0-5> <mapa 1-5> <duracion 0-2 (1 h, 3 h, 8 h)>");
      }
    }
    travelReport();
    Serial.println("DONE");
  } else if (line == "SOUV" || line.startsWith("SOUV ")) {  // SOUV [mascara]: los recuerdos; con mascara (0x.. o decimal), la pone
    if (line.length() > 5) {
      pet.setSouvenirMask((uint32_t)strtoul(line.c_str() + 5, nullptr, 0));
      wakeUI();
    }
    Serial.printf("souvenirs %u/%u mask=0x%X\n", pet.souvenirCount(), SOUV_COUNT, (unsigned)pet.souvenirMask());
    for (uint8_t i = 0; i < SOUV_COUNT; i++)
      Serial.printf("  %2u map %u rar %u %s%s\n", i, souvenirDest(i), souvenirRarity(i), T((StrId)(S_SOUV_0 + i)),
                    pet.hasSouvenir(i) ? "  [have]" : "");
    Serial.println("DONE");
  } else if (line.startsWith("COINS")) {  // COINS [n]: pone las monedas en n (0..9999); sin n, solo las muestra
    if (line.length() > 6) {
      long n = line.substring(6).toInt();
      pet.setCoins((uint16_t)(n < 0 ? 0 : (n > COIN_MAX ? COIN_MAX : n)));
      wakeUI();
    }
    Serial.printf("coins=%u earnedToday=%u/%u owned=0x%X chosen=%u\n", pet.coins, pet.coinsEarnedToday(),
                  ECO_DAILY_CAP, pet.yardOwned, pet.yardChosen);
    Serial.println("DONE");
  } else if (line == "SHOP" || line.startsWith("BUY")) {  // SHOP: la tienda; BUY <idObjeto> / BUYMAP <n>: comprar (pruebas)
    if (line.startsWith("BUYMAP") && line.length() > 7) {
      uint8_t n = (uint8_t)line.substring(7).toInt();
      bool ok = pet.buyMap(n);
      if (ok) pet.chooseMap(n);
      Serial.printf("buymap %u -> %s\n", n, ok ? "ok" : "no");
    } else if (line.startsWith("BUY ")) {
      uint8_t id = (uint8_t)line.substring(4).toInt();
      ShopBuy r = shopCheckItem(id, pet.coins, id < IT_COUNT ? pet.items[id] : 0, ITEM_MAX);
      bool ok = r == SB_OK && pet.buyItem(id);
      Serial.printf("buy %u -> %s\n", id, ok ? "ok" : r == SB_FULL ? "full" : r == SB_NOT_SOLD ? "not sold" : "no coins");
    }
    wakeUI();
    Serial.printf("coins=%u tab=%u\n", pet.coins, bagTab);
    for (int k = 0; k < SHOP_ITEMS; k++) {
      uint8_t id = SHOP_ORDER[k];
      Serial.printf("  item %u %s price=%u have=%u\n", id, T((StrId)(S_IT_GOLDEN + id)), priceOfItem(id), pet.items[id]);
    }
    for (uint8_t n = 1; n <= YARD_MAPS; n++)
      Serial.printf("  map %u %s price=%u owned=%d chosen=%d wanted=%d%s\n", n, T((StrId)(S_MAP_1 + n - 1)), priceOfMap(n),
                    pet.ownsMap(n), pet.yardChosen == n, pet.yardMapWanted() == n, n == YARD_MAP_START ? " (start)" : "");
    Serial.println("DONE");
  } else if (line == "FEED" || line.startsWith("FEED ")) {  // FEED <comida> (v3.2): da de comer al del foco, sin el patio
    // 0 baya normal, 1-6 bayas de colores (roja, azul, verde, amarilla, rosa, naranja), 7 tarta, 8 dorada
    String fa = line.length() > 5 ? line.substring(5) : String("");
    fa.trim();
    bool num = fa.length() > 0;
    for (size_t i = 0; i < fa.length(); i++) num = num && fa[i] >= '0' && fa[i] <= '9';
    long f = num ? fa.toInt() : -1;
    if (!num || f < 0 || f >= FOOD_COUNT) {
      Serial.println("feed: 0-8 (0 plain, 1-6 berries R B G Y P O, 7 cake, 8 golden)");
    } else {
      bool ok = pet.feedFood((uint8_t)f);
      Serial.printf("feed %ld -> %s fav=%u known=%d full=%u joy=%u bond=%u\n", f, ok ? "ok" : "refused", favBerryOf(pet.speciesId),
                    pet.berryKnown, pet.fullness, pet.joy, pet.bond);
      wakeUI();
    }
    Serial.println("DONE");
  } else if (line == "MENU" || line.startsWith("MENU ")) {  // MENU [s] (v3.3): abre el menu del bicho s, como tocarlo
    if (line.length() > 5) {
      int ms = line.substring(5).toInt();
      wakeUI();
      if (ms < 0 || ms >= HOME_SLOTS) Serial.println("menu: 0-1");
      else if (petMenuOpen == ms + 1) Serial.printf("MENU %d already open\n", ms);
      else {
        if (petMenuOpen) closePetMenu();
        if (!openPetMenu((uint8_t)ms)) Serial.printf("MENU %d refused (egg, vacant, ending or something open)\n", ms);
      }
    }
    petMenuReport();  // filas y sus centros (para TAP)
    Serial.println("DONE");
  } else if (line == "PICK" || line.startsWith("PICK ")) {  // PICK [s] (v3.3): en el selector de bicho, elige al s
    if (line.length() > 5) {
      int ps = line.substring(5).toInt();
      wakeUI();
      if (!petPick) Serial.println("pick: no picker open");
      else if (ps < 0 || ps >= HOME_SLOTS) Serial.println("pick: 0-1");
      else {
        int x, y, w, h;
        petPickCard((uint8_t)ps, x, y, w, h);
        petPickTap((int16_t)(x + w / 2), (int16_t)(y + h / 2));
      }
    }
    Serial.printf("pick=%u arg=%u\n", petPick, petPickArg);
    Serial.println("DONE");
  } else if (line == "MEDALS") {  // v3.4: cada medalla del jugador (valor, tramo pagado, siguiente umbral) y lo contado
    for (uint8_t i = 0; i < MEDAL_COUNT; i++) {
      uint32_t v = pet.medalValue(i);
      Serial.printf("medal %2u src=%u v=%lu tier=%u next=%lu  %s\n", i, medalSource(i), (unsigned long)v,
                    pet.medalTierPaid(i), (unsigned long)medalNext(i, v), medalName(i));
    }
    for (uint8_t hs = 0; hs < HOME_SLOTS; hs++)
      Serial.printf("home%u counted=0x%02X uid=%08lX\n", hs, pet.home(hs).counted, (unsigned long)pet.home(hs).uid);
    for (uint8_t k = 0; k < BOX_SLOTS; k++)
      if (pet.boxState(k) == BOX_OK) {
        const PetRecord &r = pet.boxRecord(k);
        Serial.printf("box%u counted=0x%02X%s legacy=0x%02X\n", k, (r.flags & REC_V3) ? r.medals : 0,
                      (r.flags & REC_V3) ? "" : " (sin REC_V3)", pet.legacyBitsOf(r.uid));
      }
    Serial.printf("tiers=%u/%u coins=%u news=%u migrated=%d\n", pet.medalTiersTotal(), (unsigned)(MEDAL_COUNT * MEDAL_TIERS),
                  pet.coins, pet.medalNewsPending(), pet.medalsMigratedNow() ? 1 : 0);
    Serial.println("DONE");
  } else if (line.startsWith("MEDAL ")) {  // MEDAL <i> <v> (v3.4): el contador de una de bicho (0-6) o de evento
    String a = line.substring(6);
    a.trim();
    int sp = a.indexOf(' ');
    int i = sp > 0 ? a.substring(0, sp).toInt() : -1;
    long v = sp > 0 ? a.substring(sp + 1).toInt() : -1;
    if (i >= 0 && i < MEDAL_COUNT && medalSource((uint8_t)i) != MSRC_DERIVED && v >= 0 && v <= 0xFFFF) {
      pet.dbgMedalCount((uint8_t)i, (uint16_t)v);
      Serial.printf("medal %d v=%lu tier=%u coins=%u\n", i, (unsigned long)pet.medalValue((uint8_t)i),
                    pet.medalTierPaid((uint8_t)i), pet.coins);
    } else {
      Serial.println("MEDAL <i> <v>: i de bicho (0-6) o de evento (7, 8, 13, 15, 16, 17)");
    }
    Serial.println("DONE");
  } else if (line == "USER" || line.startsWith("USER ")) {  // USER [pagina] (v3.4): 0 medallas, 1 paseo, 2 insignias, 3 coleccion
    int pg = line.length() > 5 ? line.substring(5).toInt() : 0;
    closeForDying();  // (cierra todo lo que tape la pantalla principal, como para el Revivir)
    openUser((uint8_t)(pg >= 0 && pg < USER_PAGES ? pg : 0));
    wakeUI();
    Serial.printf("USER page=%u\n", userPage);
    Serial.println("DONE");
  } else if (line == "FOCUS" || line.startsWith("FOCUS ")) {  // FOCUS <0|1> (v3.0): el bicho de casa que es "pet"
    if (line.length() > 6) {
      int fs = line.substring(6).toInt();
      if (fs < 0 || fs >= HOME_SLOTS || !uiFocus((uint8_t)fs)) Serial.println("focus: 0-1, y sin nada abierto");
    }
    Serial.printf("focus=%u\n", pet.focusSlot);
    Serial.println("DONE");
  } else if (line == "STATS") {
    // v3.0: los dos huecos de casa en una linea cada uno; lo de debajo es el del foco
    for (uint8_t hs = 0; hs < HOME_SLOTS; hs++) {
      const PetLife &l = pet.home(hs);
      Serial.printf("home%u%s %s spec=%d nv=%u com=%u fel=%u ene=%u poop=%u bond=%u sleep=%d dying=%d cer=%u uid=%08lX egg=%d eggH=%d grow=%lu\n",
                    hs, hs == pet.focusSlot ? "*" : " ", l.isVacant() ? "vacant" : (l.isEgg() ? "egg" : "pet"), l.speciesId,
                    l.isEgg() ? 0 : l.level(), l.fullness, l.joy, l.energy, l.poops, l.bond, l.sleeping ? 1 : 0,
                    l.isDying() ? 1 : 0, l.ceremony, (unsigned long)l.uid, l.eggSpecies(), l.eggHeld() ? 1 : 0,
                    (unsigned long)l.growMin);
    }
    Serial.printf("focus=%u focusErrors=%u\n", pet.focusSlot, pet.focusErrors);
    // v3.2: bayas de colores (R B G Y P O) y la favorita de cada hueco (oculta: known=1 si ya la descubrio)
    Serial.printf("berries=%u/%u/%u/%u/%u/%u", pet.items[IT_BERRY_R], pet.items[IT_BERRY_B], pet.items[IT_BERRY_G],
                  pet.items[IT_BERRY_Y], pet.items[IT_BERRY_P], pet.items[IT_BERRY_O]);
    for (uint8_t hs = 0; hs < HOME_SLOTS; hs++)
      Serial.printf(" fav%u=%u(known=%d)", hs, favBerryOf(pet.home(hs).speciesId), pet.home(hs).berryKnown ? 1 : 0);
    Serial.println();
    Serial.printf("spec=%d nv=%u com=%u fel=%u ene=%u desc=%u sd=%d mon=%d bat=%d usb=%d rtc=%u\n",
                  pet.speciesId, pet.level(), pet.fullness, pet.joy, pet.energy,
                  pet.careMistakes, sdReady, mon.loaded,
                  batPercent(), usbPresent(), rtcEpoch());
    Serial.printf("peso=%u fue=%u def=%u vel=%u genes=%u/%u/%u tr=%u/%u/%u baya=%d\n",
                  pet.weight, pet.atkStat(), pet.defStat(), pet.speStat(),
                  pet.geneAtk, pet.geneDef, pet.geneSpe,
                  pet.trAtk, pet.trDef, pet.trSpe, pet.berryKnown);
    Serial.printf("shiny=%d streak=%u/%u bond=%u bondT=%u counted=0x%02X nick=%s\n",
                  pet.shiny, pet.streak, pet.bestStreak, pet.bond, pet.bondGainedToday(), pet.counted, pet.nick);
    {  // v3.4: las medallas del jugador, en resumen (MEDALS: el detalle)
      uint8_t nt[MEDAL_TIERS + 1] = { 0 };
      for (uint8_t i = 0; i < MEDAL_COUNT; i++) nt[pet.medalTierPaid(i)]++;
      Serial.printf("medals: %u/%u tramos (oro %u, plata %u, bronce %u) news=%u%s\n", pet.medalTiersTotal(),
                    (unsigned)(MEDAL_COUNT * MEDAL_TIERS), nt[MTIER_GOLD], nt[MTIER_SILVER], nt[MTIER_BRONZE],
                    pet.medalNewsPending(), pet.medalsMigratedNow() ? " (migradas en este arranque)" : "");
    }
    // abandono (v1.60): minutos apagando el vinculo, minutos hacia la muerte, el dialogo y su huevo
    Serial.printf("ngl=%u dth=%u dying=%d dym=%u degg=%d revive=%u lend=%u\n", pet.neglectMinutes(),
                  pet.deathMinutes(), pet.isDying() ? 1 : 0, pet.dyingMinutes(), pet.deathEggSpecies(),
                  pet.items[IT_REVIVE], pet.lastEnd);
    Serial.printf("coins=%u earnedToday=%u/%u maps owned=0x%X chosen=%u wanted=%u\n", pet.coins,
                  pet.coinsEarnedToday(), ECO_DAILY_CAP, pet.yardOwned, pet.yardChosen, pet.yardMapWanted());
    Serial.printf("travel %s slot=%d map=%u tier=%u left=%lus souvenirs=%u/%u mask=0x%X pending=%d\n",
                  pet.travelling() ? "away" : "none", pet.travelling() ? (int)pet.travelSlot() : -1, pet.travelDest(),
                  pet.travelTier(), (unsigned long)pet.travelSecondsLeft(), pet.souvenirCount(), SOUV_COUNT,
                  (unsigned)pet.souvenirMask(), pet.travelResultPending() ? 1 : 0);
    Serial.printf("rec ball=%u bag=%u zen=%u.%u dash=%u\n", pet.gameHi, pet.strHi, pet.zenHi / 10, pet.zenHi % 10, pet.dashHi);
    // crecimiento (v1.35, growth.h): de el sale el nivel; la edad va aparte
    Serial.printf("grow=%lu lv=%u into=%u/%u age=%lu\n", (unsigned long)pet.growMin, pet.level(),
                  pet.levelMinutesInto(), pet.levelMinutesCost(), (unsigned long)pet.ageMinutes);
    Serial.printf("steps=%lu/%lu finds=%u bonus=%u goal=%u bpack=%u/%u/%u/%u charm=%u candy=%u cake=%u revive=%u sated=%lu imu=%d\n",
                  (unsigned long)pet.stepsToday, (unsigned long)pet.stepsTotal, pet.findsToday,
                  pet.bonusFindsToday, pet.goalTierOn(pet.dayNumber()), pet.items[IT_GOLDEN],
                  pet.items[IT_PROTEIN], pet.items[IT_IRON], pet.items[IT_CARBOS], pet.items[IT_CHARM],
                  pet.items[IT_RARECANDY], pet.items[IT_CAKE], pet.items[IT_REVIVE], (unsigned long)pet.satedMinutesLeft(),
                  imuPresent() ? 1 : 0);
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

// pulsacion larga: sobre el bicho de casa y no sobre una caca (el patio: el que pasea, fuera del dock)
// v3.0: el hueco de casa del bicho bajo el dedo, o -1
int holdOnPet(int16_t x, int16_t y) {
  if (yardPoopAt(x, y) >= 0 || yDockHit(x, y) >= 0) return -1;
  int i = yardPetAt(x, y);
  return (i >= 0 && i < HOME_SLOTS) ? i : -1;
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

  // cascada: cuenta el dedo PUESTO, no solo el toque (FR-ZEN.5); la cabecera abandona sin premio
  if (zenOpen) {
    if (pressed && !wasPressed) {
      wakeUI();
      if (y < 72 && !zenOverUntil) {
        zenOpen = false;
        zenHold = false;
        wasPressed = pressed;
        return;
      }
      zenTouched = true;
    }
    zenHold = pressed && y >= 72;
    wasPressed = pressed;
    return;
  }

  // carrera: solo la cabecera cuenta (abandonar sin premio); se juega inclinando
  if (dashOpen) {
    if (pressed && !wasPressed) {
      wakeUI();
      if (y < 72 && !dashOverUntil) closeDash();
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
    // pulsacion larga sin moverse sobre un bicho de casa -> dialogo de soltar (v3.0: el que esta bajo el
    // dedo, que pasa a tener el foco)
    int hs = -1;
    if (!holdFired && !swallowGesture && !yardDemoOpen && !galleryOpen && !boxOpen && !stagesOpen && !teamOpen && !cardOpen && !userOpen && !bagOpen && !kbOpen && !clockOpen && !panelKind && !petMenuOpen && !petPick && !encOpen() && millis() - tStart > 3000 &&
        abs(tXl - tX0) < 30 && abs(tYl - tY0) < 30 && (hs = holdOnPet(tX0, tY0)) >= 0 &&
        !pet.home((uint8_t)hs).isEgg() && !confirmUntil && !pet.home((uint8_t)hs).busyEnding() && uiFocus((uint8_t)hs)) {
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
  if (petPick) return;                // v3.3: el selector de bicho se cierra con volver o USER
  if (petMenuOpen) closePetMenu();    // v3.3: deslizar cierra el menu del bicho (y sigue con lo suyo)
  if (gameOpen || galleryOpen || boxOpen || stagesOpen || teamOpen || bagOpen || panelKind || kbOpen || sackOpen || zenOpen || dashOpen || battleOpen() || encOpen() || pet.busyEnding()) return;
  if (clockOpen) { clockOpen = false; return; }
  if (cardOpen) {
    if (dir < 0) cardOpen = false;  // arriba cierra la ficha
    return;
  }
  if (userOpen) {  // v3.4: en las medallas, arriba / abajo pasan sus sub-paginas; abajo desde la primera (o en otra pagina) cierra
    userSwipeV(dir);
    return;
  }
  if (dir > 0) {                    // deslizar abajo: ajustar hora
    if (!confirmUntil && !panelKind) openClock();
  } else if (!confirmUntil && !panelKind) {
    openUser(USERP_MEDALS);         // v3.4: deslizar arriba: la pagina del jugador (la ficha, desde Estado del menu del bicho)
  }
}

// deslizar: dir +1 = hacia la derecha
void onSwipe(int dir) {
  if (pet.awaitingStarter()) return;  // bloqueado durante la eleccion de inicial
  if (yardDemoOpen) {  // muestra del kit: pasa de pagina como las flechas
    yardDemoPage = (uint8_t)((yardDemoPage + YDEMO_PAGES + (dir > 0 ? -1 : 1)) % YDEMO_PAGES);
    return;
  }
  if (petPick) return;  // v3.3: el selector de bicho se cierra con volver o USER
  if (bagOpen) {  // v2.0 (Y4b): las paginas de Objetos y de la Tienda, la Coleccion y los mapas del selector
    bagSwipe(dir);
    return;
  }
  if (petMenuOpen) closePetMenu();  // v3.3: deslizar cierra el menu del bicho (y sigue con lo suyo)
  if (gameOpen || sackOpen || zenOpen || dashOpen || panelKind || kbOpen || clockOpen || battleOpen() || encOpen()) return;
  if (pet.isDying()) return;  // v1.60: el dialogo del Revivir no se esquiva
  if (cardOpen) {  // dentro de la ficha: cambiar entre las CARD_PAGES paginas
    int p = (int)cardPage + (dir > 0 ? -1 : 1);  // izquierda avanza
    cardPage = p < 0 ? 0 : (p > CARD_PAGES - 1 ? CARD_PAGES - 1 : p);
    return;
  }
  if (userOpen) {  // v3.4: la pagina del jugador, 4 paginas; a la derecha desde la primera, a casa
    userSwipe(dir);
    return;
  }
  if (stagesOpen || teamOpen) return;  // menus de fase: ni a casa ni a la Pokedex (se sale con USER o la cabecera)
  if (boxOpen) {  // la caja (v3.4: una sola pagina; las insignias, a la pagina del jugador): a la izquierda, a casa
    if (boxMove) return;
    if (dir < 0) closeBox();
    return;
  }
  if (!galleryOpen) {
    // con un menu abierto no: la galeria se abria encima del de comida
    if (!pet.ceremony && !confirmUntil && !panelKind) {
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
  if (pet.isDying()) return;          // v1.60: el dialogo del Revivir no se cierra con USER
  if (yardDemoOpen) {
    yardDemoOpen = false;
    return;
  }
  if (sackOpen) {                     // abandona (sin premio si aun corre el tiempo)
    sackOpen = false;
    return;
  }
  if (zenOpen) {                      // abandona la cascada (sin premio si no habia acabado)
    zenOpen = false;
    return;
  }
  if (dashOpen) {                     // abandona la carrera (sin premio si no habia acabado)
    closeDash();
    return;
  }
  if (battleOpen()) {                 // abandona el combate (sin premio; el salvaje se escapa)
    closeBattle();
    return;
  }
  if (encOpen()) {                    // v2.2: el encuentro se queda en el patio (o esperando la Poke Ball)
    closeEncUi();
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
  if (teamOpen) { closeTeam(); return; }  // el selector vuelve a la seleccion de fase
  if (stagesOpen) { closeStages(); return; }
  if (boxOpen) {
    if (boxMove) return;                   // la luz ya vuela: el movimiento no se corta
    if (boxAsk) { boxAsk = 0; return; }    // "guardar?" / "despertar?": NO
    closeBox();
    return;
  }
  if (kbOpen) { kbOpen = false; return; }        // descarta el nombre y vuelve a la ficha
  if (clockOpen) { clockOpen = false; return; }  // sin aplicar la hora, como al deslizar
  if (pet.ceremony) return;                      // durante la despedida no hay vuelta atras
  if (petPick) { closePetPick(); return; }       // v3.3: el selector de bicho -> el panel Jugar o la mochila
  if (bagOpen && yardMapPreview) { yardMapPreview = 0; return; }  // selector de fondos -> tienda
  if (bagOpen && shopBuyId) { shopBuyId = 0; return; }             // "comprar?": NO
  if (bagOpen && itemConfirm) { itemConfirm = 0; return; }  // "usar objeto?": NO
  if (bagOpen) { closeBag(); return; }
  if (cardOpen) { cardOpen = false; return; }
  if (userOpen) {                                // v3.4: la tarjeta abierta -> la pagina -> casa
    if (souvTip || userMedalTip >= 0) {
      souvTip = 0;
      userMedalTip = -1;
    } else {
      userOpen = false;
    }
    return;
  }
  if (gameOpen) { gameOpen = false; return; }    // abandona sin premio, como tocar la cabecera
  if (choiceKind) { choiceKind = 0; return; }    // ni evoluciona ni rechaza: el boton sigue ahi
  if (confirmUntil) { confirmUntil = 0; return; }  // "soltar?": NO
  if (petMenuOpen) {  // v3.3: la lista de comida -> las acciones -> cerrado
    if (petMenuView) petMenuBackToActions();
    else closePetMenu();
    return;
  }
  if (panelKind == PANEL_TRAVEL) { panelKind = PANEL_PLAY; return; }  // viajes: un nivel atras, al panel Jugar
  if (panelKind) { panelKind = PANEL_NONE; return; }  // panel Jugar: a casa
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
  if (yardDemoOpen) {
    yardDemoTap(x, y);
    return;
  }
  if (encOpen()) {  // v2.2
    encTap(x, y);
    return;
  }
  if (galleryOpen) {
    galleryTap(x, y);
    return;
  }
  if (teamOpen) {
    teamTap(x, y);
    return;
  }
  if (stagesOpen) {
    stagesTap(x, y);
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
  if (petPick) {  // v3.3: el selector de bicho (encima del panel Jugar o de la mochila)
    petPickTap(x, y);
    return;
  }
  if (bagOpen) {
    bagTap(x, y);
    return;
  }
  if (panelKind) {  // el panel Jugar (v2.0, Y2)
    panelTap(x, y);
    return;
  }
  if (pet.ceremony) return;  // durante la despedida no hay botones
  if (pet.isDying()) {       // v1.60: solo cuenta el dialogo del Revivir
    dyingTap(x, y);
    return;
  }
  if (returnBannerShown) {   // el aviso de que el viaje acabo: un toque lo quita (y cobra el "visto")
    if (millis() - returnOpenAt < RETURN_TAP_LOCK_MS) return;  // el toque que lo abrio (o uno de mas) no lo cierra
    TravelResult tr;
    returnOpen = false;
    pet.takeTravelResult(&tr);
    returnBannerShown = false;
    sfxPlay(SFX_TAP);
    return;
  }
  if (cardOpen) {
    if (cardPage == 0 && y < CARD_NAME_TAP_Y) openKeyboard();  // tocar el nombre = renombrar
    else cardOpen = false;
    return;
  }
  if (userOpen) {  // v3.4
    userTap(x, y);
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
      if (b1) {
        int16_t old = pet.speciesId;
        pet.evolve();
        evoPmd.load(old, pet.shiny);
      }
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
  if (foundShown) {          // hallazgo del paseo en pantalla: un toque lo cierra
    pet.popFound();
    foundShown = false;
    sfxPlay(SFX_TAP);
    return;
  }
  if (pet.isVacant()) {  // solo el boton de abrir un huevo, si hay en la mochila (v1.40)
    if (pet.items[IT_EGG] && x >= VAC_BTN_X && x <= VAC_BTN_X + VAC_BTN_W && y >= VAC_BTN_Y &&
        y <= VAC_BTN_Y + VAC_BTN_H && pet.hatchFromBag()) {
      sfxPlay(SFX_TAP);
      monReset();  // el huevo que llega se dibuja ya
      ensureMon();
      return;
    }
    sfxPlay(SFX_DENY);  // nada mas que tocar: el mensaje dice que hay que caminar
    return;
  }
  // el patio: el menu del bicho (v3.3; evolucionar y despedirse estan en el), el dock, las cacas, los regalos y
  // los bichos
  yardTap(x, y);
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

// salvapantallas (v1.28): a los 2 min sin tocar, salvo boca abajo o apagada con PWR
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
void mixPanelKey(uint32_t &h);
void mixTravelKey(uint32_t &h);
static bool staticScreen() {
  if (pet.awaitingStarter()) return true;
  if (galleryOpen || gameOpen || sackOpen || zenOpen || dashOpen || battleOpen() || encOpen()) return false;  // la galeria gestiona su propio repintado
  if (stagesOpen || teamOpen) return !timeLeft(stShakeUntil);  // menus quietos, salvo la casilla que tiembla
  if (petPick) return true;  // v3.3: el selector de bicho (miniaturas quietas)
  if (panelKind) return !timeLeft(panelShakeUntil);  // el panel Jugar (v2.0, Y2): quieto salvo una tarjeta que tiembla
  if (boxOpen) return !boxMove && !timeLeft(boxShakeUntil);  // la caja, quieta salvo la luz o un hueco que tiembla
  if (yardDemoOpen || kbOpen || clockOpen || bagOpen) return true;
  if (userOpen) return true;  // v3.4: la pagina del jugador (sin animaciones)
  return cardOpen && cardPage != 0;  // la pagina 0 tiene el retrato animado
}
uint32_t walkStepsToday();
static uint32_t mixKey(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }
static uint32_t screenKey() {
  uint32_t h = 2166136261u;
  h = mixKey(h, pet.awaitingStarter() ? 1 : petPick ? 24 : teamOpen ? 23 : stagesOpen ? 22 : boxOpen ? 20 : kbOpen ? 2 : clockOpen ? 3 : bagOpen ? 21 : userOpen ? 30 + userPage : 4 + cardPage);
  h = mixKey(h, sdEpoch);
  h = mixKey(h, gBright);                      // el aviso de brillo va encima de todo
  h = mixKey(h, timeLeft(briToastUntil) > 0);  // y al irse tiene que repintarse
  h = mixKey(h, gOrbitStep);                   // la orbita la mueve: hay que volver a mandarla
  h = mixKey(h, yardDemoOpen ? 100u + yardDemoPage : 0u);
  h = mixKey(h, gLang);
  if (petPick) {  // v3.3: el selector de bicho: que elige y los dos de casa (especie, nombre, nivel)
    h = mixKey(h, (uint32_t)petPick | ((uint32_t)petPickArg << 8));
    for (uint8_t s = 0; s < HOME_SLOTS; s++) {
      const PetLife &l = pet.home(s);
      h = mixKey(h, (uint32_t)(uint16_t)l.speciesId | ((uint32_t)l.shiny << 16) | ((uint32_t)l.level() << 17));
      for (const char *c = l.nick; *c; c++) h = mixKey(h, (uint8_t)*c);
    }
  } else if (stagesOpen || teamOpen) {
    mixStageKey(h);
  } else if (panelKind) {
    mixPanelKey(h);
  } else if (boxOpen) {
    // cada hueco (su CRC cambia con cualquier cosa del bicho), el activo, el huevo que espera y el dialogo
    for (uint8_t k = 0; k < BOX_SLOTS; k++) {
      h = mixKey(h, pet.boxState(k));
      if (pet.boxState(k) == BOX_OK) h = mixKey(h, pet.boxRecord(k).crc);
    }
    for (uint32_t v : { (uint32_t)pet.speciesId, (uint32_t)pet.shiny, pet.uid, (uint32_t)pet.eggHeld(),
                        (uint32_t)pet.canBoxMove(), (uint32_t)boxAsk, (uint32_t)gLang,
                        // Y5b: el viaje (hueco, minutos que faltan: se ven como H:MM) y el aviso de "de viaje"
                        (uint32_t)pet.travelSlot(), (uint32_t)(pet.travelling() ? (pet.travelSecondsLeft() + 59) / 60 : 0),
                        (uint32_t)(boxAwayUntil ? 1 + boxAwaySlot : 0) })
      h = mixKey(h, v);
    for (const char *c = pet.nick; *c; c++) h = mixKey(h, (uint8_t)*c);
  } else if (bagOpen) {
    for (uint8_t n : pet.items) h = mixKey(h, n);
    for (uint32_t v : { (uint32_t)pet.eggsOwned(), (uint32_t)itemConfirm, (uint32_t)pet.sleeping, (uint32_t)gLang,
                        (uint32_t)(timeLeft(bagReviveHintUntil) > 0), (uint32_t)bagHintItem,
                        // v2.0 (Y4b): pestana, tienda, aviso, selector de fondos y lo que lo cambia
                        (uint32_t)bagTab | ((uint32_t)bagPage << 8), (uint32_t)shopBuyId, (uint32_t)shopToast, (uint32_t)yardMapPreview,
                        (uint32_t)pet.coins, (uint32_t)pet.yardOwned, (uint32_t)pet.yardChosen,
                        (uint32_t)pet.yardMapWanted(), (uint32_t)(yardMapPreview ? sceneHour() : 0),
                        (uint32_t)gLang })
      h = mixKey(h, v);
  } else if (kbOpen) {
    h = mixKey(h, nameLen);
    for (int i = 0; i < nameLen; i++) h = mixKey(h, (uint8_t)nameBuf[i]);
  } else if (clockOpen) {
    h = mixKey(h, clockH * 60 + clockM);
    h = mixKey(h, audioLevel());
    h = mixKey(h, gLang);
  } else if (userOpen) {
    // v3.4: la pagina del jugador. Medallas (tramos y lo que mide cada una), insignias, coleccion y el paseo
    for (uint32_t v : { (uint32_t)userMedalPage, (uint32_t)(userMedalTip + 1), (uint32_t)souvTip, pet.souvenirMask(),
                        (uint32_t)pet.badgeMask(), (uint32_t)gLang })
      h = mixKey(h, v);
    for (uint8_t i = 0; i < MEDAL_COUNT; i++) h = mixKey(h, pet.medalValue(i) * 4u + pet.medalTierPaid(i));
    for (uint32_t v : { walkStepsToday(), pet.stepsTotal, (uint32_t)pet.findsToday, pet.stepDay, pet.goalDay,
                        (uint32_t)pet.goalTier, (uint32_t)pet.bonusFindsToday, (uint32_t)pet.eggsOwned(),
                        (uint32_t)imuPresent(), pet.dayNumber() })
      h = mixKey(h, v);
  } else if (cardOpen) {
    // growMin: el nivel y su barra (v1.35); un Caramelo Raro o una meta lo saltan sin tocar la edad
    for (uint32_t v : { (uint32_t)pet.speciesId, (uint32_t)pet.shiny, (uint32_t)pet.ageMinutes, pet.growMin, (uint32_t)pet.streak,
                        (uint32_t)pet.bestStreak, (uint32_t)pet.bond, (uint32_t)pet.sleeping, (uint32_t)pet.weight,
                        (uint32_t)pet.trAtk, (uint32_t)pet.trDef, (uint32_t)pet.trSpe, (uint32_t)pet.geneAtk,
                        (uint32_t)pet.geneDef, (uint32_t)pet.geneSpe, (uint32_t)pet.careMistakes,
                        (uint32_t)pet.fullness, (uint32_t)pet.joy, (uint32_t)pet.energy,
                        (uint32_t)pet.berryKnown, (uint32_t)pet.strHi, (uint32_t)gLang })
      h = mixKey(h, v);
    for (const char *c = pet.nick; *c; c++) h = mixKey(h, (uint8_t)*c);
  }
  return h;
}
bool screenKeyValid = false;  // (el renderer del host lo invalida entre escenas)
uint32_t lastScreenKey = 0;

// Dibuja un frame y devuelve true si lo ha volcado al panel.
bool render() {
  if (itemConfirm && !timeLeft(itemConfirmUntil)) itemConfirm = 0;  // antes de la clave: asi se repinta
  if (shopBuyId && !timeLeft(shopBuyUntil)) shopBuyId = 0;          // (lo mismo, "comprar?" y el aviso de la tienda)
  if (shopToast && !timeLeft(shopToastUntil)) shopToast = 0;
  if (boxAsk && !timeLeft(boxAskUntil)) boxAsk = 0;                  // (lo mismo, el dialogo de la caja)
  if (boxAwayUntil && !timeLeft(boxAwayUntil)) boxAwayUntil = 0;     // (y el aviso de "de viaje")
  returnBannerShown = false;  // lo vuelve a poner render() si el aviso de vuelta sigue en pantalla
  foundShown = false;  // lo vuelve a poner drawWalkNews() si el hallazgo sigue en pantalla
  if (saverOn()) {  // 2 min sin tocar: salvapantallas, encima de cualquier pantalla
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
  if (yardDemoOpen) {  // muestra del kit del patio (serie YARDUI)
    renderYardDemo();
    return true;
  }
  if (galleryOpen) {
    renderGallery();
    return true;
  }
  if (encOpen()) {  // v2.2: el encuentro salvaje (bandas negras y el salvaje en su circulo)
    renderEncounter(millis());
    return true;
  }
  if (teamOpen) {
    renderTeam();
    return true;
  }
  if (stagesOpen) {
    renderStages();
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
  if (dashOpen) {
    uint32_t t0 = micros();
    renderDash();
    dashDrawUs = micros() - t0;
    if (dashDrawUs > dashDrawMaxUs) dashDrawMaxUs = dashDrawUs;
    return true;
  }
  if (zenOpen) {
    uint32_t t0 = micros();
    renderZen();
    zenDrawUs = micros() - t0;
    if (zenDrawUs > zenDrawMaxUs) zenDrawMaxUs = zenDrawUs;
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
  if (userOpen) {  // v3.4: la pagina del jugador
    renderUser();
    return true;
  }
  if (petPick && (pet.ceremony || pet.isDying())) petPick = PICK_NONE;  // (como los paneles, abajo)
  if (petPick) {  // v3.3: el selector de bicho (encima del panel Jugar o de la mochila)
    renderPetPick();
    return true;
  }
  if (bagOpen) {
    renderBag();
    return true;
  }
  if (panelKind && (pet.ceremony || pet.isDying())) panelKind = PANEL_NONE;  // la despedida o el Revivir se ven en casa
  if (panelKind) {  // el panel Jugar (v2.0, Y2)
    renderPanel();
    return true;
  }
  int h = sceneHour();
  gNight = pet.sleeping || h < 6 || h >= 20;
  // el patio (v2.0): la casa con un bicho o un huevo (Y3) es el mapa; la casa vacia, la despedida y el
  // Revivir tienen su propia pantalla sobre el paisaje de lado
  bool yard = yardHomeNow();
  if (!yard) yardW.p[0].on = yardW.p[1].on = yardW.p[2].on = false;
  // drawScene cubre los 466x466 completos: sin fillScreen(NEGRO) previo para
  // que un flush DMA solapado nunca capture negro a medias (anti-parpadeo)
  uint32_t sceneT0 = micros();
  if (yard) {
    yardDrawBg();  // el mapa ya expandido (en PSRAM), copiado al lienzo
    yardShowFull();  // v2.5: y alrededor del circulo, hasta el borde del panel
  } else {
    drawScene(homeBiome(), millis(), gNight);
    hatchAt(CX, PET_CY, 5);  // la eclosion, en el centro (el patio pone la suya)
  }
  perfNote(micros() - sceneT0);

  if (pet.ceremony) {
    const DexEntry &d = DEX_TBL[pet.speciesId];
    const char *msg = (pet.ceremony == CER_FAREWELL) ? T(S_FAREWELL)
                      : (pet.ceremony == CER_DEATH)  ? T(S_DIED)
                                                     : T(S_GOODBYE);
    drawHeader(dexName(pet.speciesId), d.accent, msg);
    drawCeremony();
    gfx->flush();
    return true;
  }
  if (pet.isDying()) {  // v1.60: se muere de abandono; el dialogo del Revivir espera
    drawDyingScreen();
    gfx->flush();
    return true;
  }

  // efectos (fx.h): la cola avanza antes de dibujar, para saber si hay celebracion en este frame (fija la pose)
  uint32_t fxT0 = micros();
  fxFrame(millis());
  uint32_t fxUs = micros() - fxT0;

  if (yard) {
    uint32_t yT0 = micros();
    renderYardHome(millis());  // los bichos, la caca, los regalos, el globo, la hora, la bateria y el dock
    fxT0 = micros();
    drawFxOverlays(millis());
    uint32_t ov = micros() - fxT0;
    fxUs += ov;
    if (petMenuOpen) drawPetMenu(millis());  // v3.3: el menu del bicho, encima del patio a la derecha
    gYardPetsUs = micros() - yT0 - ov;
    if (gYardPetsUs > gYardPetsMaxUs) gYardPetsMaxUs = gYardPetsUs;
  } else {  // la casa vacia (la despedida y el Revivir ya salieron arriba)
    drawVacantHome();
  }

  if (pet.sleeping && !yard) {  // (el patio lo dice en el globo)
    gfx->setTextColor(UI_INK_NIGHT);
    setSize(3);
    setCur(320, 130);
    printT("Zz");
  }

  // noticias del paseo (meta y hallazgos), por encima de todo lo de la pantalla
  // principal (un toque cierra antes el hallazgo)
  // y por debajo de los dialogos, que las aplazan (drawWalkNews mira si hay alguno)
  fxT0 = micros();
  drawFxBanner(millis());  // celebracion en curso (fx.h): subida, medalla, hito o meta
  fxPerfNote(fxUs + (micros() - fxT0));
  drawWalkNews();
  // el aviso de que un viaje acabo (Y5b): solo en la casa, de uno en uno con las demas celebraciones y noticias
  // (v2.1: el viajero espera en el patio con un "!" y el aviso sale al tocarlo; si ya no esta en la caja, sale solo)
  static bool retWas = false, backWas = false;
  bool backNow = pet.travelResultPending();
  if (backNow && !backWas && yard) sfxPlay(SFX_LEVEL);  // ha llegado
  backWas = backNow;
  if (!backNow) returnOpen = false;
  bool retNow = yard && !fxBan && !foundShown && !confirmUntil && !choiceKind && !petMenuOpen && !pet.evolving() && fxScreenAwake() &&
                backNow && (returnOpen || trvBackSlot() < 0);
  if (retNow) {
    drawReturnBanner();
    if (!retWas) {
      sfxPlay(SFX_LEVELUP);
      if (!returnOpen) returnOpenAt = millis();
    }
  }
  retWas = retNow;
  returnBannerShown = retNow;

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
      // sin huevos (v1.39), la nota dice que pasara: casa vacia o, sin nadie en la caja, un huevo de
      // regalo a la mochila (v1.40). Con huevos no hay nota: la casa vacia tiene su boton.
      bool empty = !pet.eggsOwned() && !pet.giftEggs();  // v1.55: un huevo envuelto tambien cuenta
      gfx->setTextColor(UI_INK);
      setSize(2);
      setCur(centerX(q, 2), empty ? 186 : 196);
      printT(q);
      if (empty) {
        bool gift = pet.releaseGivesEgg();
        gfx->setTextColor(gift ? UI_INK : BOX_NOTE_INK);
        printTight(T(gift ? S_GIFT_EGG_NOTE : S_NO_EGG_NOTE), CX, 216, 262);
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
        pet.lastCoins = 0;
        sfxPlay(SFX_LEVEL);
      } else {
        gameNewHi = (gameScore > pet.gameHi);
        FxNeedSnap ns = fxSnap();
        pet.lastCoins = 0;
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
  sackUntil = millis() + SACK_MS;
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
void drawCoinGain(int y, uint16_t n, uint16_t col);  // moneda + "+N" centrados (patio, Y4)
const char *medalTierName(uint8_t tier);  // v3.4 (el conversor del .ino no saca el prototipo de un "const char *")

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
    if (sackGain == 0 && pet.trAtk >= 100) snprintf(g, sizeof(g), "%s", T(S_ATK_MAX));  // ya al tope: no "+0"
    else snprintf(g, sizeof(g), T(S_STR_GAIN_FMT), sackGain);
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
    drawCoinGain(292, pet.lastCoins, ink);
    gfx->flush();
    return;
  }

  // se acabaron los 10 s: aplicar entrenamiento
  if (!timeLeft(sackUntil)) {
    sackNewHi = (sackHits > pet.strHi);
    pet.lastCoins = 0;  // trainStrength lo pone si paga (un huevo o un final no)
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
  int bw = 280, fw = (int)((uint32_t)bw * left / SACK_MS);
  gfx->fillRoundRect(CX - bw / 2, 350, bw, 16, 5, UI_TRACK);
  if (fw > 2) gfx->fillRoundRect(CX - bw / 2, 350, fw, 16, 5, UI_BAR_OK);

  gfx->flush();
}

// ---------- cascada: meditar bajo el agua (entrena la defensa), v1.43 ----------
// docs/NEW_GAMES.md, Task ZEN. Las reglas estan en zengame.h; aqui el dibujo, el dedo y el premio.

// el bicho del minijuego con la accion `act` (o en reposo si su sprite no la tiene), pies en
// (x, footY). Sin sprite PMD usa el de flash, como el minijuego de la pokeball.
void drawGamePet(uint8_t act, int x, int footY) {
  if (pmd.loaded) {
    if (!pmd.has(act)) act = PMD_IDLE;
    drawPmdAct(act, x, footY, millis(), true, false, 3);
  } else if (mon.loaded) {
    int s = (mon.h * 2 > 130) ? 1 : 2;
    int w = mon.w * s, h = mon.h * s;
    uint16_t fm = mon.frameMs ? mon.frameMs : 100;
    uint16_t fi = (millis() / fm) % mon.frames;
    const uint8_t *fr = mon.data + (uint32_t)fi * mon.w * mon.h;
    int px = x - w / 2, py = footY - h;
    for (int r = 0; r < mon.h; r++) {
      const uint8_t *row = fr + r * mon.w;
      forRuns(mon.w, [&](int c) { return indexedKey(row[c], false); },
              [&](int c0, int len, int k) { gfx->fillRect(px + c0 * s, py + r * s, len * s, s, mon.pal[k]); });
    }
  }
}

// hay un juego en pantalla (desde el menu nunca se abren dos, pero los comandos serie podrian)
bool anyGameOpen() { return gameOpen || sackOpen || zenOpen || dashOpen || battleOpen(); }

void startZen() {
  if (pet.isEgg() || pet.sleeping || pet.ceremony || anyGameOpen()) return;
  zenOpen = true;
  zenStart(zen, (uint32_t)random(1, 0x7fffffff));
  uint32_t now = millis();
  zenIntroUntil = now + ZEN_INTRO_MS;
  zenOverUntil = 0;
  zenLastTick = now;
  zenHold = false;
  zenTouched = false;
  zenNoSave = false;
  zenNewHi = false;
  zenGainShown = 0;
  zenBonusShown = 0;
  zenDriftAnnounced = false;
  zenSurgesHeard = 0;
}

// la sesion entera ha terminado: premio (salvo la partida del comando ZEN) y resultado
void finishZen(uint32_t now) {
  uint16_t focus = zenFocusTenths(zen), drift = zenDriftTenths(zen);
  zenBonusShown = zenBonus(drift);
  if (zenNoSave) {
    zenNewHi = false;
    zenGainShown = zenTotalGain(focus, drift);  // lo que daria, para verlo; no se aplica
    pet.lastCoins = 0;
    sfxPlay(SFX_LEVEL);
  } else {
    zenNewHi = focus > pet.zenHi;
    FxNeedSnap ns = fxSnap();
    pet.lastCoins = 0;
    zenGainShown = pet.zenResult(focus, drift);  // DEF (con la bonificacion), lavado, record
    fxNeeds(ns);  // los "+N" salen al volver a la pantalla principal
    sfxPlay(zenNewHi && focus > 0 ? SFX_MEDAL : SFX_PLAY);
  }
  zenOverUntil = now + 4000;
}

// fisica de la cascada en cada vuelta del loop (no solo en cada frame): asi un toque corto entre
// dos frames de 85 ms tambien cuenta. Mientras dura, la pantalla no se atenua (nadie da toques nuevos).
void zenTick(uint32_t now) {
  if (!zenOpen || zenOverUntil) return;
  lastInteract = now;  // FR-G.5: sin atenuar ni salvapantallas a mitad de sesion
  if (timeLeft(zenIntroUntil)) { zenLastTick = now; return; }
  bool hold = zenSerialHold >= 0 ? zenSerialHold == 1 : zenHold;
  zenAdvance(zen, hold, now - zenLastTick);
  zenLastTick = now;
  if (zenDrifting(zen.t) && !zenDriftAnnounced) {  // la franja empieza a moverse: bonificacion
    zenDriftAnnounced = true;
    sfxPlay(SFX_LEVEL);
  }
  if (zen.surges != zenSurgesHeard) {  // golpe de agua
    zenSurgesHeard = zen.surges;
    sfxPlay(SFX_HIT);
  }
  if (zenDone(zen)) finishZen(now);
}

// colores de la cascada, de dia y de noche (ZenPal esta arriba, con las globales)
static ZenPal zenPalette(bool night) {
  ZenPal p = { C565(0x6a, 0x6e, 0x7a), C565(0x4a, 0x4e, 0x5a), C565(0x4c, 0x8a, 0x52), C565(0x4f, 0x93, 0xc4),
               C565(0xa8, 0xdc, 0xf6), C565(0x2f, 0x6c, 0x9c), UI_WHITE };
  if (night) {
    uint16_t n = C565(0x10, 0x16, 0x2c);
    p.rock = lerp565(p.rock, n, 9, 16);
    p.rockDk = lerp565(p.rockDk, n, 9, 16);
    p.moss = lerp565(p.moss, n, 9, 16);
    p.water = lerp565(p.water, n, 7, 16);
    p.waterLt = lerp565(p.waterLt, n, 6, 16);
    p.pool = lerp565(p.pool, n, 7, 16);
    p.foam = lerp565(p.foam, n, 4, 16);
  }
  return p;
}

#define ZEN_WATER_L (CX - 75)
#define ZEN_WATER_W 150
#define ZEN_POOL_Y 330
#define ZEN_PET_FOOT 344
#define ZEN_GAUGE_X 394
#define ZEN_GAUGE_W 20
#define ZEN_GAUGE_TOP 128
#define ZEN_GAUGE_BOT 346

// manchas fijas de la roca (x, y, w, h, musgo?)
static const int16_t ZEN_ROCKS[][5] = {
  { 40, 120, 60, 26, 0 }, { 96, 210, 48, 20, 1 }, { 20, 280, 70, 24, 0 }, { 110, 60, 40, 18, 0 },
  { 60, 360, 50, 18, 1 }, { 330, 90, 56, 22, 0 }, { 352, 200, 30, 60, 0 }, { 318, 300, 44, 18, 1 },
  { 400, 380, 40, 18, 0 }, { 120, 150, 34, 14, 1 }, { 316, 160, 30, 14, 1 },
};

// warn: 0 sin aviso; 1..ZEN_SURGE_WARN_MS: ms que faltan para el golpe de agua
void drawZenScene(uint32_t now, int8_t zone, uint32_t warn, bool splash) {
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  ZenPal p = zenPalette(night);
  // la roca, con manchas oscuras y musgo
  gfx->fillRect(0, 0, 466, ZEN_POOL_Y, p.rock);
  for (auto &r : ZEN_ROCKS) gfx->fillRoundRect(r[0], r[1], r[2], r[3], 8, r[4] ? p.moss : p.rockDk);
  // la cascada: columna azul con vetas claras que bajan deprisa
  int wl = ZEN_WATER_L, ww = ZEN_WATER_W;
  if (warn) { wl -= 10; ww += 20; }  // se hincha antes del golpe
  gfx->fillRect(wl, 0, ww, ZEN_POOL_Y, p.water);
  gfx->fillRect(wl - 4, 0, 4, ZEN_POOL_Y, p.rockDk);
  gfx->fillRect(wl + ww, 0, 4, ZEN_POOL_Y, p.rockDk);
  // detras del bicho el agua corre mas honda (mas oscura): asi un bicho azul no se pierde en ella
  gfx->fillRoundRect(CX - 50, 196, 100, ZEN_POOL_Y - 196, 34, night ? lerp565(C565(0x2a, 0x52, 0x78), C565(0x10, 0x16, 0x2c), 7, 16) : C565(0x2a, 0x52, 0x78));
  uint32_t fall = now * 7 / 10;  // 0,7 px/ms
  for (int i = 0; i < 12; i++) {
    int x = wl + 6 + i * (ww - 12) / 12;
    uint32_t ph = (uint32_t)(i * 2654435761u) >> 24;  // fase fija por veta
    int len = 40 + (int)(ph % 50);
    int period = 150 + (int)(ph % 70);
    for (int y0 = -period + (int)((fall + ph * 3) % period); y0 < ZEN_POOL_Y; y0 += period) {
      int a = y0 < 0 ? 0 : y0, b = y0 + len > ZEN_POOL_Y ? ZEN_POOL_Y : y0 + len;
      if (b > a) gfx->fillRect(x, a, 3, b - a, p.waterLt);
    }
  }
  if (warn) {  // la ola que viene: baja por la cascada y llega al bicho justo con el golpe
    int y = 110 + (int)((ZEN_SURGE_WARN_MS - warn) * (ZEN_PET_FOOT - 120 - 110) / ZEN_SURGE_WARN_MS);
    gfx->fillRoundRect(wl - 6, y, ww + 12, 30, 14, p.foam);
    gfx->fillRect(wl, y - 18, ww, 14, p.waterLt);
  }
  // la poza y la espuma donde cae el agua
  gfx->fillRect(0, ZEN_POOL_Y, 466, 466 - ZEN_POOL_Y, p.pool);
  for (int i = 0; i < 9; i++) {
    int r = 8 + (int)((now / 90 + i * 5) % 10);
    int x = ZEN_WATER_L - 10 + i * 21;
    gfx->fillCircle(x, ZEN_POOL_Y + 4 + (i % 3) * 5, r, p.foam);
  }
  // la roca plana donde medita
  gfx->fillRoundRect(CX - 72, ZEN_PET_FOOT - 6, 144, 34, 14, p.rockDk);
  gfx->fillRoundRect(CX - 66, ZEN_PET_FOOT - 8, 132, 12, 6, p.rock);

  // el bicho: medita en la franja, se encoge por debajo, se agita por encima
  uint8_t act = zone < 0 ? PMD_HURT : zone > 0 ? PMD_HOP : (pmd.has(PMD_BREATH) ? PMD_BREATH : PMD_SIT);
  if (zone == 0 && !zenOverUntil) {  // aura de calma que crece mientras dura
    int r = 70 + (int)((now / 60) % 12);
    gfx->drawCircle(CX, ZEN_PET_FOOT - 60, r, C565(0xff, 0xf0, 0xa0));
    gfx->drawCircle(CX, ZEN_PET_FOOT - 60, r + 1, C565(0xff, 0xf0, 0xa0));
  }
  drawGamePet(act, CX, ZEN_PET_FOOT);
  // el agua le cae encima: vetas sobre la cabeza
  for (int i = 0; i < 5; i++) {
    int x = CX - 40 + i * 20, y = 150 + (int)((now / 3 + i * 37) % 60);
    gfx->fillRect(x, y, 3, 22, p.waterLt);
  }
  if (splash || zone < 0) {  // salpicadura: el agua lo saca de la concentracion
    for (int i = 0; i < 10; i++) {
      float a = i * 0.628f + now * 0.004f;
      int d = 50 + (int)((now / 25 + i * 13) % 40);
      gfx->fillCircle(CX + (int)(d * cosf(a)), 210 + (int)(d * 0.5f * sinf(a)), 5, p.foam);
    }
  }
  if (zone > 0) {  // inquieto: "!" encima
    gfx->fillRoundRect(CX + 60, 120, 10, 34, 4, UI_BAR_WARN);
    gfx->fillRoundRect(CX + 60, 160, 10, 10, 4, UI_BAR_WARN);
  }
}

// barra de concentracion vertical a la derecha, con la franja verde (FR-ZEN.3)
void drawZenGauge(float c, uint32_t t, int8_t dir, bool inBand) {
  int h = ZEN_GAUGE_BOT - ZEN_GAUGE_TOP;
  auto yOf = [&](float v) { return ZEN_GAUGE_BOT - (int)(v * h / 100.0f); };
  gfx->fillRoundRect(ZEN_GAUGE_X - 3, ZEN_GAUGE_TOP - 3, ZEN_GAUGE_W + 6, h + 6, 8, UI_INK);
  gfx->fillRect(ZEN_GAUGE_X, ZEN_GAUGE_TOP, ZEN_GAUGE_W, h, UI_TRACK);
  int fy = yOf(c);
  if (ZEN_GAUGE_BOT > fy) gfx->fillRect(ZEN_GAUGE_X, fy, ZEN_GAUGE_W, ZEN_GAUGE_BOT - fy, C565(0xa8, 0xdc, 0xf6));
  float half = zenBandHalf(t), mid = zenCenter(t, dir);
  int b0 = yOf(mid + half), b1 = yOf(mid - half);
  uint16_t band = inBand ? C565(0x6c, 0xe0, 0x7c) : C565(0x3c, 0x8a, 0x4c);
  gfx->fillRect(ZEN_GAUGE_X, b0, ZEN_GAUGE_W, b1 - b0, band);
  gfx->fillRect(ZEN_GAUGE_X - 8, fy - 3, ZEN_GAUGE_W + 16, 6, UI_INK);  // marcador
  gfx->fillRect(ZEN_GAUGE_X - 6, fy - 1, ZEN_GAUGE_W + 12, 2, UI_WHITE);
}

void renderZen() {
  uint32_t now = millis();
  bool intro = timeLeft(zenIntroUntil);
  int8_t zone = intro ? 0 : zenZone(zen.c, zen.t, zen.driftDir);
  bool splash = zen.surges && zen.t - zen.lastSurge < 500 && !zenOverUntil;
  uint32_t warn = !intro && !zenOverUntil && zenSurgeWarning(zen) ? zen.nextSurge - zen.t : 0;
  drawZenScene(now, zenOverUntil ? 0 : zone, warn, splash);

  if (zenOverUntil) {  // resultado
    if (!timeLeft(zenOverUntil)) { zenOpen = false; return; }
    uint16_t f = zenFocusTenths(zen);
    gfx->fillRoundRect(CX - 150, 86, 300, 224, 20, UI_WHITE);
    gfx->drawRoundRect(CX - 150, 86, 300, 224, 20, UI_INK);
    char b[40];
    snprintf(b, sizeof(b), T(S_FOCUS_FMT), f / 10, f % 10);
    gfx->setTextColor(UI_INK);
    setSize(3);
    setCur(centerX(b, 3), 116);
    printT(b);
    char g[24];
    if (zenGainShown == 0 && pet.trDef >= 100) snprintf(g, sizeof(g), "%s", T(S_DEF_MAX));  // ya al tope
    else snprintf(g, sizeof(g), T(S_DEF_GAIN_FMT), zenGainShown);
    gfx->setTextColor(C565(0x4f, 0x93, 0xc4));
    setCur(centerX(g, 3), 156);
    printT(g);
    setSize(2);
    if (zenBonusShown && !(zenGainShown == 0 && pet.trDef >= 100)) {
      char bo[32];  // la parte de la bonificacion (franja en movimiento), incluida en la cifra de arriba
      snprintf(bo, sizeof(bo), T(S_ZEN_BONUS_FMT), zenBonusShown);
      gfx->setTextColor(UI_BAR_WARN);
      setCur(centerX(bo, 2), 198);
      printT(bo);
    }
    if (zenNewHi && f > 0) {
      gfx->setTextColor(UI_BAR_WARN);
      setCur(centerX(T(S_NEW_RECORD), 2), 232);
      printT(T(S_NEW_RECORD));
    } else {
      char r[40];
      snprintf(r, sizeof(r), T(S_ZEN_BEST_FMT), pet.zenHi / 10, pet.zenHi % 10);
      gfx->setTextColor(UI_INK);
      setCur(centerX(r, 2), 232);
      printT(r);
    }
    drawCoinGain(284, pet.lastCoins, UI_INK);
    gfx->setTextColor(UI_BAR_OK);
    setCur(centerX(T(S_CLEAN), 2), 258);
    printT(T(S_CLEAN));
    gfx->flush();
    return;
  }

  // concentracion en vivo y record, arriba
  uint16_t f = zenFocusTenths(zen);
  char b[40];
  snprintf(b, sizeof(b), T(S_FOCUS_FMT), f / 10, f % 10);
  gfx->setTextColor(UI_WHITE);
  setSize(3);
  setCur(centerX(b, 3), 34);
  printT(b);
  char r[40];
  if (!intro && zenDrifting(zen.t)) {  // franja en movimiento: la bonificacion que lleva, en dorado
    snprintf(r, sizeof(r), T(S_ZEN_BONUS_FMT), zenBonus(zenDriftTenths(zen)));
    gfx->setTextColor(UI_BAR_WARN);
  } else {
    snprintf(r, sizeof(r), T(S_ZEN_BEST_FMT), pet.zenHi / 10, pet.zenHi % 10);
  }
  setSize(2);
  setCur(centerX(r, 2), 74);
  printT(r);
  gfx->setTextColor(UI_WHITE);
  // tiempo que queda
  uint32_t left = intro ? ZEN_SESSION_MS : ZEN_SESSION_MS - (zen.t < ZEN_SESSION_MS ? zen.t : ZEN_SESSION_MS);
  int bw = 200, fw = (int)((uint32_t)bw * left / ZEN_SESSION_MS);
  gfx->fillRoundRect(CX - bw / 2, 102, bw, 10, 4, UI_TRACK);
  if (fw > 2) gfx->fillRoundRect(CX - bw / 2, 102, fw, 10, 4, UI_BAR_OK);

  drawZenGauge(intro ? 50 : zen.c, intro ? 0 : zen.t, zen.driftDir, zone == 0);

  if (intro || !zenTouched) {  // la explicacion, hasta el primer toque
    const char *l1 = T(S_ZEN_INTRO), *l2 = T(S_ZEN_HOLD);
    int w = textW(l1, 2);
    if (textW(l2, 2) > w) w = textW(l2, 2);
    w += 28;
    if (w > 380) w = 380;
    gfx->fillRoundRect(CX - w / 2, 126, w, 62, 14, UI_WHITE);
    gfx->setTextColor(UI_INK);
    setCur(centerX(l1, 2), 136);
    printT(l1);
    gfx->setTextColor(C565(0x4f, 0x93, 0xc4));
    setCur(centerX(l2, 2), 162);
    printT(l2);
  }
  uint32_t tf = micros();
  gfx->flush();
  zenFlushUs = micros() - tf;
}

// ---------- carrera en trineo: inclinar para esquivar (entrena la velocidad), v1.44 ----------
// docs/NEW_GAMES.md, Task DASH. Las reglas estan en dashgame.h; aqui la calibracion, el dibujo
// (trineo y obstaculos hechos con primitivas, sin sprites nuevos) y el premio.

// ancho visible en pantalla del sprite en reposo (de su primer frame), para el ancho del trineo
int petSpriteVisibleW(uint8_t act) {
  if (pmd.loaded) {
    if (!pmd.has(act)) act = PMD_IDLE;
    const PmdAct &a = pmd.acts[act];
    if (!a.frames) return 80;
    uint8_t s = pmd.acts[PMD_IDLE].h ? 170 / pmd.acts[PMD_IDLE].h : 5;  // la escala de drawPmdActM
    if (s < 2) s = 2;
    if (s > 3) s = 3;
    while (s > 2 && a.h * s > 250) s--;
    int c0 = a.w, c1 = -1;
    for (int r = 0; r < a.h; r++)
      for (int c = 0; c < a.w; c++)
        if (a.data[r * a.w + c] != 0xFF) {
          if (c < c0) c0 = c;
          if (c > c1) c1 = c;
        }
    return c1 >= c0 ? (c1 - c0 + 1) * s : 80;
  }
  if (mon.loaded) return mon.w * ((mon.h * 2 > 130) ? 1 : 2);
  return 80;
}

void startDash() {
  if (pet.isEgg() || pet.sleeping || pet.ceremony || !imuPresent() || anyGameOpen()) return;
  dashOpen = true;
  dashStart(dash, (uint32_t)random(1, 0x7fffffff));
  uint32_t now = millis();
  dashCalibrating = true;
  dashCalStart = now;
  dashCal.reset();
  dashCalRetries = 0;
  dashShaky = false;
  dashBase = 0;
  dashOverUntil = 0;
  dashLastTick = now;
  dashNoSave = false;
  dashNewHi = false;
  dashGainShown = 0;
  dashHitsSeen = 0;
  dashTrailN = 0;
  dashSledPx = dashSledW(petSpriteVisibleW(PMD_IDLE));
  imuSetFast(true);  // FIFO cada 40 ms y los pasos en pausa (FR-DASH.1, FR-DASH.4)
}

// cierra la carrera por cualquier camino (fin, cabecera, USER, PWR): el IMU vuelve a su ritmo
void closeDash() {
  dashOpen = false;
  imuSetFast(false);
}

void finishDash(uint32_t now) {
  imuSetFast(false);
  if (dashNoSave) {
    dashNewHi = false;
    dashGainShown = dashGain(dash.dodged);  // lo que daria, para verlo; no se aplica
    pet.lastCoins = 0;
    sfxPlay(SFX_LEVEL);
  } else {
    dashNewHi = dash.dodged > pet.dashHi;
    FxNeedSnap ns = fxSnap();
    pet.lastCoins = 0;
    dashGainShown = pet.dashResult(dash.dodged);
    fxNeeds(ns);
    sfxPlay(dashNewHi && dash.dodged > 0 ? SFX_MEDAL : SFX_PLAY);
  }
  dashOverUntil = now + 4000;
}

// la direccion de ahora: la inclinacion rapida respecto a la calibracion (o la del comando STEER)
float dashSteerNow() {
  float rel = dashSerialSteerOn ? dashSerialSteer : imuRollFast() - dashBase;
  return dashSteer(rel);
}

void dashTick(uint32_t now) {
  if (!dashOpen || dashOverUntil) return;
  lastInteract = now;  // FR-G.5: nadie toca la pantalla, pero no se atenua
  if (dashCalibrating) {
    uint32_t el = now - dashCalStart;
    if (el >= DASH_CAL_MS - DASH_CAL_WINDOW_MS) dashCal.add(imuRollFast());
    if (el >= DASH_CAL_MS) {
      if (dashCal.shaky(dashCalRetries)) {  // tiembla: "Quieta un momento..." y otra cuenta
        dashCalRetries++;
        dashShaky = true;
        dashCalStart = now;
        dashCal.reset();
      } else {
        dashBase = dashCal.mean();
        dashCalibrating = false;
        dashLastTick = now;
        dashRunStart = now;
        sfxPlay(SFX_TAP);
      }
    }
    return;
  }
  uint16_t before = dash.dodged;
  dashAdvance(dash, dashSteerNow(), now - dashLastTick);
  dashLastTick = now;
  // huella de los patines: la x del trineo cada DASH_TRAIL_STEP px recorridos
  while (dashTrailN == 0 || dash.dist - dashTrailY[(dashTrailN - 1) % DASH_TRAIL] >= DASH_TRAIL_STEP) {
    dashTrailX[dashTrailN % DASH_TRAIL] = dash.px;
    dashTrailY[dashTrailN % DASH_TRAIL] = dash.dist;
    dashTrailN++;
  }
  if (dash.hits != dashHitsSeen) {  // golpe
    dashHitsSeen = dash.hits;
    sfxPlay(SFX_HIT);
  }
  if (dash.dodged / 10 != before / 10) sfxPlay(SFX_PLAY);  // cada 10 esquivados
  if (dashOver(dash)) finishDash(now);
}

// colores de la pista segun el bioma de casa
static DashPal dashPalette(bool night) {
  uint8_t bio = pet.isEgg() ? 0 : DEX_TBL[pet.speciesId].biome;
  if (bio >= BIOME_COUNT) bio = 0;
  uint16_t soil = BIOME_SOIL[bio];
  DashPal p;
  p.track = lerp565(soil, UI_WHITE, 7, 16);  // clara (v1.45): los obstaculos oscuros resaltan
  p.outside = lerp565(soil, C565(0x20, 0x30, 0x20), 5, 16);
  p.edge = lerp565(soil, C565(0x10, 0x10, 0x10), 8, 16);
  p.mark = lerp565(soil, C565(0x20, 0x20, 0x20), 4, 16);
  // v1.45: oscuros y saturados sobre la pista clara (contraste >= 3:1 en todos los biomas, y el
  // contorno oscuro de drawDashObstacle llega a 6-13:1). El matorral verde oscuro casi no se veia
  // sobre la hierba: ahora es magenta (abeto en la nieve, lava roja en el volcan).
  p.rock = bio == 5 ? C565(0x4c, 0x8c, 0xd0) : bio == 3 ? C565(0x34, 0x2a, 0x2e) : bio == 8 ? C565(0x4a, 0x40, 0x5c)
                                                                                  : C565(0x56, 0x5a, 0x68);
  p.log = C565(0x7a, 0x4a, 0x26);
  p.bush = bio == 5 ? C565(0x2a, 0x62, 0x40) : bio == 3 ? C565(0x9a, 0x1c, 0x12) : C565(0x9c, 0x2c, 0x7c);
  p.sled = bio == 5 ? C565(0xc8, 0x3a, 0x30) : C565(0x9a, 0x5a, 0x2c);
  p.sledDk = lerp565(p.sled, C565(0x10, 0x10, 0x10), 6, 16);
  p.runner = C565(0x48, 0x4c, 0x58);
  if (night) {
    uint16_t n = C565(0x10, 0x16, 0x2c);
    p.track = lerp565(p.track, n, 8, 16);
    p.outside = lerp565(p.outside, n, 8, 16);
    p.edge = lerp565(p.edge, n, 8, 16);
    p.mark = lerp565(p.mark, n, 8, 16);
    p.rock = lerp565(p.rock, n, 6, 16);
    p.log = lerp565(p.log, n, 6, 16);
    p.bush = lerp565(p.bush, n, 6, 16);
    p.sled = lerp565(p.sled, n, 5, 16);
    p.sledDk = lerp565(p.sledDk, n, 5, 16);
  }
  return p;
}

// poligono convexo girado `a` radianes alrededor de (cx, cy): n vertices relativos al centro,
// rellenado en abanico desde el primero
void fillPolyRot(int cx, int cy, const float (*q)[2], int n, float a, uint16_t col) {
  float c = cosf(a), s = sinf(a);
  int x0 = cx + (int)lroundf(q[0][0] * c - q[0][1] * s), y0 = cy + (int)lroundf(q[0][0] * s + q[0][1] * c);
  int px = 0, py = 0;
  for (int i = 1; i < n; i++) {
    int x = cx + (int)lroundf(q[i][0] * c - q[i][1] * s), y = cy + (int)lroundf(q[i][0] * s + q[i][1] * c);
    if (i >= 2) gfx->fillTriangle(x0, y0, px, py, x, y, col);
    px = x;
    py = y;
  }
}

// el trineo, de frente y un poco desde arriba: una cubeta redonda con el borde delantero curvado y
// una tabla de patines debajo. back = el fondo de la cubeta (detras del bicho); si no, el borde
// delantero (tapa sus patas) y los patines. Todo se inclina `leanDeg` grados.
void drawSled(int cx, int by, int w, float leanDeg, const DashPal &p, bool back) {
  float a = leanDeg / 57.29578f;
  float hw = w / 2.0f;
  int cy = by - 16;  // centro de giro
  const int N = 14;
  float q[N + 2][2];
  if (back) {  // fondo: elipse entera, oscura
    for (int i = 0; i < N; i++) {
      float t = i * 6.2832f / N;
      q[i][0] = (hw - 3) * cosf(t);
      q[i][1] = -10 + 11 * sinf(t);
    }
    fillPolyRot(cx, cy, q, N, a, p.sledDk);
    return;
  }
  // tabla de patines y sus puntas curvadas
  const float base[4][2] = { { -hw + 8, 9 }, { hw - 8, 9 }, { hw - 8, 15 }, { -hw + 8, 15 } };
  fillPolyRot(cx, cy, base, 4, a, p.runner);
  float c = cosf(a), s = sinf(a);
  for (int k = -1; k <= 1; k += 2) {
    float rx = k * (hw - 12), ry = 15;
    gfx->fillCircle(cx + (int)lroundf(rx * c - ry * s), cy + (int)lroundf(rx * s + ry * c), 4, p.runner);
  }
  // borde delantero: media elipse por debajo de la linea y = -6
  for (int i = 0; i <= N; i++) {
    float t = i * 3.1416f / N;
    q[i][0] = hw * cosf(t);
    q[i][1] = -6 + 17 * sinf(t);
  }
  fillPolyRot(cx, cy, q, N + 1, a, p.sled);
  const float shine[4][2] = { { -hw + 10, -3 }, { hw - 10, -3 }, { hw - 12, 0 }, { -hw + 12, 0 } };
  fillPolyRot(cx, cy, shine, 4, a, lerp565(p.sled, UI_WHITE, 7, 16));
}

void drawDashObstacle(int x0, int x1, int y, uint8_t kind, const DashPal &p) {
  int w = x1 - x0, h = (int)DASH_OBS_H;
  // todos llevan un contorno oscuro de 2 px (v1.45), para verse sobre cualquier pista
  if (kind == 0) {  // roca (o bloque de hielo, o piedra de las ruinas)
    gfx->fillRoundRect(x0 - 2, y - 2, w + 4, h + 4, 14, UI_INK);
    gfx->fillRoundRect(x0, y, w, h, 12, p.rock);
    gfx->fillRoundRect(x0 + 6, y + 4, w / 2, 8, 4, lerp565(p.rock, UI_WHITE, 6, 16));
  } else if (kind == 1) {  // tronco atravesado
    gfx->fillRoundRect(x0 - 2, y + 4, w + 4, h - 8, 12, UI_INK);
    gfx->fillRoundRect(x0, y + 6, w, h - 12, 10, p.log);
    gfx->fillRect(x0 + 8, y + 12, w - 16, 3, lerp565(p.log, C565(0, 0, 0), 5, 16));
    gfx->fillCircle(x1 - 10, y + h / 2, (h - 12) / 2 - 2, lerp565(p.log, UI_WHITE, 5, 16));
  } else {  // matorral (o monticulo de nieve, o roca de lava)
    int n = w / 26 + 1;
    for (int i = 0; i < n; i++) {
      int cx = x0 + 13 + i * (w - 26) / (n > 1 ? n - 1 : 1);
      gfx->fillCircle(cx, y + h / 2 + 2, 17, UI_INK);
    }
    for (int i = 0; i < n; i++) {
      int cx = x0 + 13 + i * (w - 26) / (n > 1 ? n - 1 : 1);
      gfx->fillCircle(cx, y + h / 2 + 2, 15, p.bush);
    }
    gfx->fillCircle(x0 + w / 2, y + h / 2 - 6, 12, lerp565(p.bush, UI_WHITE, 3, 16));
  }
}

// el mundo: fuera de pista, la pista con sus bordes, marcas del suelo, huella y obstaculos
void drawDashWorld(uint32_t now, int shx, const DashPal &p) {
  float f = dashSpeed(dash.t);
  gfx->fillRect(0, 0, 466, 466, p.outside);
  int tl = CX + shx + (int)(-DASH_HALF_W - dash.px), tr = CX + shx + (int)(DASH_HALF_W - dash.px);
  gfx->fillRect(tl, 0, tr - tl, 466, p.track);
  gfx->fillRect(tl - 8, 0, 8, 466, p.edge);
  gfx->fillRect(tr, 0, 8, 466, p.edge);
  // marcas que pasan hacia arriba (enseñan la velocidad): una cada 40 px de pista
  float y0 = dash.dist - DASH_PET_Y - 40, y1 = dash.dist + (466 - DASH_PET_Y) + 40;
  for (int b = (int)floorf(y0 / 40); b * 40 < y1; b++) {
    uint32_t h = (uint32_t)b * 2654435761u;
    float wx = -DASH_HALF_W - 90 + (float)((h >> 8) % 520);
    int sx = CX + shx + (int)(wx - dash.px), sy = DASH_PET_Y + (int)(b * 40 - dash.dist);
    bool on = wx > -DASH_HALF_W && wx < DASH_HALF_W;
    uint16_t c = on ? p.mark : p.edge;
    if ((h >> 4) & 1) gfx->fillRect(sx, sy, 10, 3, c);
    else { gfx->fillRect(sx, sy, 3, 8, c); gfx->fillRect(sx + 5, sy + 2, 3, 6, c); }
  }
  // huella de los dos patines detras del trineo (arriba en pantalla): se curva al girar
  int n = dashTrailN < DASH_TRAIL ? dashTrailN : DASH_TRAIL;
  for (int k = -1; k <= 1; k += 2)
    for (int i = 1; i < n; i++) {
      int ia = (dashTrailN - i) % DASH_TRAIL, ib = (dashTrailN - i - 1) % DASH_TRAIL;
      int xa = CX + shx + (int)(dashTrailX[ia] - dash.px) + k * (dashSledPx / 2 - 10);
      int xb = CX + shx + (int)(dashTrailX[ib] - dash.px) + k * (dashSledPx / 2 - 10);
      int ya = DASH_PET_Y - 30 + (int)(dashTrailY[ia] - dash.dist), yb = DASH_PET_Y - 30 + (int)(dashTrailY[ib] - dash.dist);
      if (yb < -10) break;
      for (int o = 0; o < 3; o++) gfx->drawLine(xa + o, ya, xb + o, yb, p.mark);
    }
  // obstaculos
  for (const DashRow &r : dash.rows) {
    if (!r.live) continue;
    int sy = DASH_PET_Y + (int)(r.y - dash.dist);
    if (sy > 470 || sy + DASH_OBS_H < -4) continue;
    for (uint8_t i = 0; i < r.n; i++)
      drawDashObstacle(CX + shx + (int)(r.o[i].x0 - dash.px), CX + shx + (int)(r.o[i].x1 - dash.px), sy, r.o[i].kind, p);
  }
  // lineas de velocidad a los lados desde 2x
  if (f >= 2.0f) {
    int len = 30 + (int)((f - 2) * 40);
    for (int i = 0; i < 6; i++) {
      int x = i < 3 ? 40 + i * 22 : 466 - 40 - (i - 3) * 22;
      int y = 466 - (int)((uint32_t)(now * f * 0.5f + i * 97) % 520);
      gfx->fillRect(x, y, 3, len, UI_WHITE);
    }
  }
}

void renderDash() {
  uint32_t now = millis();
  bool night = sceneHour() < 6 || sceneHour() >= 20;
  DashPal p = dashPalette(night);
  bool justHit = dash.hits && dash.t - dash.lastHitT < 220 && !dashOverUntil && !dashCalibrating;
  int shx = justHit ? ((now / 40) % 2 ? 4 : -4) : 0;
  drawDashWorld(now, shx, p);

  // trineo y bicho (parpadean mientras es invulnerable)
  bool blink = !dashCalibrating && !dashOverUntil && dashInvuln(dash) && (now / 100) % 2;
  if (!blink) {
    float f = dashSpeed(dash.t);
    int bounce = (dashCalibrating || dashOverUntil) ? 0 : ((now / (uint32_t)(240 / f)) % 2 ? 2 : 0);
    float steer = dashCalibrating ? 0 : dash.steer;
    float lean = steer * DASH_SLED_LEAN_DEG;
    int sx = CX + shx, by = DASH_PET_Y - bounce;
    if (justHit) sx += (now / 30) % 2 ? 6 : -6;  // sacudida del trineo
    drawSled(sx, by, dashSledPx, lean, p, true);
    // en reposo (de frente en todos los sprites: el Sit de algunos, como Pikachu, mira hacia atras)
    uint8_t act = (dash.hits && dash.t - dash.lastHitT < 600 && !dashCalibrating) ? PMD_HURT : PMD_IDLE;
    drawGamePet(act, sx + (int)(steer * 3), by - 16);
    drawSled(sx, by, dashSledPx, lean, p, false);
    if (!dashCalibrating && f >= 2.0f) {  // polvo o nieve que levantan los patines
      for (int i = 0; i < 4; i++) {
        int k = i % 2 ? 1 : -1;
        int px = sx + k * (dashSledPx / 2 - 10) + (int)((now / 20 + i * 7) % 9) - 4;
        int py = by - 30 - (int)((now / 15 + i * 23) % 26);
        gfx->fillCircle(px, py, 4 - i % 2, lerp565(p.track, UI_WHITE, 10, 16));
      }
    }
  }

  gfx->setTextColor(UI_INK);
  if (dashOverUntil) {  // resultado
    if (!timeLeft(dashOverUntil)) { closeDash(); return; }
    gfx->fillRoundRect(CX - 150, 230, 300, 160, 20, UI_WHITE);
    gfx->drawRoundRect(CX - 150, 230, 300, 160, 20, UI_INK);
    char b[40];
    snprintf(b, sizeof(b), T(S_DODGED_FMT), dash.dodged);
    setSize(3);
    setCur(centerX(b, 3), 250);
    printT(b);
    char g[28];
    if (dashGainShown == 0 && pet.trSpe >= 100) snprintf(g, sizeof(g), "%s", T(S_SPE_MAX));  // ya al tope
    else snprintf(g, sizeof(g), T(S_SPD_GAIN_FMT), dashGainShown);
    gfx->setTextColor(UI_BAR_WARN);
    setCur(centerX(g, 3), 294);
    printT(g);
    setSize(2);
    if (dashNewHi && dash.dodged > 0) {
      gfx->setTextColor(UI_BAR_OK);
      setCur(centerX(T(S_NEW_RECORD), 2), 342);
      printT(T(S_NEW_RECORD));
    } else {
      char r[24];
      snprintf(r, sizeof(r), T(S_RECORD_FMT), pet.dashHi);
      gfx->setTextColor(UI_INK);
      setCur(centerX(r, 2), 342);
      printT(r);
    }
    drawCoinGain(364, pet.lastCoins, UI_INK);
    gfx->flush();
    return;
  }

  if (dashCalibrating) {  // cuenta atras de la calibracion
    uint32_t el = millis() - dashCalStart;
    char c[4];
    snprintf(c, sizeof(c), "%lu", (unsigned long)((DASH_CAL_MS - (el < DASH_CAL_MS ? el : DASH_CAL_MS)) / 700 + 1));
    gfx->fillCircle(CX, 296, 34, UI_WHITE);
    gfx->drawCircle(CX, 296, 34, UI_INK);
    setSize(5);
    setCur(centerX(c, 5), 277);
    printT(c);
    const char *msg = dashShaky ? T(S_DASH_STILL) : T(S_DASH_HOLD);
    int w = textW(msg, 2) + 28;
    if (w > 350) w = 350;
    gfx->fillRoundRect(CX - w / 2, 344, w, 38, 12, UI_WHITE);
    gfx->setTextColor(dashShaky ? UI_BAR_BAD : UI_INK);
    setSize(2);
    setCur(centerX(msg, 2), 355);
    printT(msg);
    gfx->flush();
    return;
  }

  // marcador, record y vidas, como en el minijuego de la pokeball
  char buf[8];
  snprintf(buf, sizeof(buf), "%u", dash.dodged);
  gfx->setTextColor(UI_INK);
  // pastilla compacta arriba (el bicho empieza unos 100 px por encima de DASH_PET_Y): la cifra y,
  // debajo, el record con las vidas al lado
  char rec[24];
  snprintf(rec, sizeof(rec), T(S_REC_FMT), pet.dashHi);
  setSize(2);
  int rw = textW(rec, 2), lw = rw + 14 + DASH_LIVES * 20;
  int pw = lw + 28 < 150 ? 150 : lw + 28;
  gfx->fillRoundRect(CX - pw / 2, 14, pw, 74, 16, UI_WHITE);
  setSize(4);
  setCur(centerX(buf, 4), 20);
  printT(buf);
  setSize(2);
  int lx = CX - lw / 2;
  setCur(lx, 62);
  printT(rec);
  for (int i = 0; i < DASH_LIVES; i++) {
    int hx = lx + rw + 14 + i * 20 + 6;
    if (i < DASH_LIVES - dash.hits) gfx->fillCircle(hx, 69, 6, UI_BAR_BAD);
    else gfx->drawCircle(hx, 69, 6, UI_TRACK);
  }
  if (now - dashRunStart < 3000) {  // pista: inclinar para girar
    const char *t = T(S_DASH_TILT);
    int w = textW(t, 2) + 24;
    gfx->fillRoundRect(CX - w / 2, 372, w, 34, 10, UI_WHITE);
    setCur(centerX(t, 2), 381);
    printT(t);
  }
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
    drawCoinGain(286, pet.lastCoins, ink);
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
  gfx->setTextColor(Y_TEXT);  // panel del patio (Y1): texto blanco, barra sobre pista oscura
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
  gfx->fillRoundRect(bx, y + 2, bw, 11, 3, Y_CARD_HI);
  if (fw > 2) gfx->fillRoundRect(bx, y + 2, fw, 11, 3, color);
}

// Barra fina bajo una de combate (v1.49): el entrenamiento de ese stat, 0-100. El numero de
// arriba es el stat de combate (base x genes + nivel + entrenamiento), que sigue subiendo con
// el nivel; esta es solo la parte entrenada, la que topa en 100 y hace que el juego diga
// "<stat> MAX". Sin ella, ese MAX con un stat de combate bajo parecia un fallo.
void drawCardTrain(int y, uint8_t tr, uint16_t color) {
  int bx = statBarX();
  int bw = 310 - bx;
  int fw = (int)tr * bw / 100;
  if (fw > bw) fw = bw;
  gfx->fillRoundRect(bx, y + 24, bw, 6, 2, Y_CARD_HI);
  if (fw > 2) gfx->fillRoundRect(bx, y + 24, fw, 6, 2, color);
  char num[8];
  if (tr >= 100) snprintf(num, sizeof(num), "MAX");
  else snprintf(num, sizeof(num), "%u", tr);
  gfx->setTextColor(tr >= 100 ? color : Y_TEXT_DIM);
  setSize(2);
  setCur(330, y + 20);
  printT(num);
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

// Panel de ajustes (v2.0, Y1): disco oscuro del patio. Arriba el titulo; una tarjeta con la hora y
// sus cuatro botones -/+; debajo la fila de pildoras (volumen, brillo, idioma), el OK verde, la
// version y el boton redondo de volver (= tecla USER y deslizar arriba: cancela sin aplicar).
#define CLK_CARD_X 88
#define CLK_CARD_Y 80
#define CLK_CARD_W 290
#define CLK_CARD_H 156
#define CLK_TIME_Y 92     // la hora, a tamano 7
#define CLK_BTN_Y 150     // botones -/+ de 52x52 (se tocan 56x56)
#define CLK_BTN_S 52
#define CLK_BTN_GAP 6
#define CLK_LBL_Y 210     // "HORA" / "MIN" bajo cada pareja
#define CLK_PAIR_DX 63    // centro de cada pareja: CX -/+ 63, bajo las horas y bajo los minutos
#define CLK_OK_X 133
#define CLK_OK_Y 310
#define CLK_OK_W 200
#define CLK_OK_H 48
#define CLK_VER_Y 374

// x del boton i (0 hora-, 1 hora+, 2 min-, 3 min+)
static int clkBtnX(int i) {
  int pc = CX + (i < 2 ? -CLK_PAIR_DX : CLK_PAIR_DX);
  return (i % 2 == 0) ? pc - CLK_BTN_GAP / 2 - CLK_BTN_S : pc + CLK_BTN_GAP / 2;
}

// boton -/+ dibujado con barras (sin depender de la fuente)
void drawClockBtn(int x, int y, bool plus) {
  gfx->fillRoundRect(x, y, CLK_BTN_S, CLK_BTN_S, Y_CARD_RAD, Y_CARD_HI);
  int cx = x + CLK_BTN_S / 2, cy = y + CLK_BTN_S / 2;
  gfx->fillRect(cx - 10, cy - 2, 20, 4, Y_TEXT);
  if (plus) gfx->fillRect(cx - 2, cy - 10, 4, 20, Y_TEXT);
}

// pildoras de volumen, brillo e idioma en una fila; se tocan con 48 px de alto
#define LANG_PILL_Y 254
#define LANG_PILL_H 40
#define LANG_PILL_X 336          // pildora de idioma (cicla LANG_COUNT al tocar)
#define LANG_PILL_W 96
#define CLK_PILL_HIT_Y (LANG_PILL_Y - (Y_HIT_MIN - LANG_PILL_H) / 2)
static const char *const LANG_CODES[LANG_COUNT] = { "ES", "EN", "FR", "DE", "IT", "PT", "JA", "KO" };

// pildora de volumen a la izquierda (v1.50: 4 niveles con el apagado, antes SON ON/OFF) y la de
// brillo en el hueco hasta la de idioma (v1.23). Las dos sin texto: iguales en todos los idiomas
#define SND_PILL_X 33
#define SND_PILL_W 140
#define BRI_PILL_X 185
#define BRI_PILL_W 140

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

// altavoz: cuerpo y cono, de x a x+14, centrado en y. Silenciado (off), una X al lado
void drawSpeakerIcon(int x, int y, uint16_t col, bool off) {
  gfx->fillRect(x, y - 4, 6, 9, col);
  gfx->fillTriangle(x + 4, y, x + 14, y - 10, x + 14, y + 10, col);
  if (off) {
    gfx->drawLine(x + 19, y - 4, x + 27, y + 4, UI_BAR_BAD);
    gfx->drawLine(x + 20, y - 4, x + 28, y + 4, UI_BAR_BAD);
    gfx->drawLine(x + 19, y + 4, x + 27, y - 4, UI_BAR_BAD);
    gfx->drawLine(x + 20, y + 4, x + 28, y - 4, UI_BAR_BAD);
  }
}

// nivel de volumen como VOL_LEVELS-1 barras que crecen, rellenas hasta el nivel (0: todas huecas).
// x = borde izquierdo de la primera, y = base comun
void drawVolBars(int x, int y, uint8_t level, uint16_t on, uint16_t off) {
  for (int i = 0; i < VOL_LEVELS - 1; i++) {
    int h = 8 + i * 5, bx = x + i * 22;
    if (i < level) gfx->fillRect(bx, y - h, 12, h, on);
    else gfx->drawRect(bx, y - h, 12, h, off);
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
  yPanelBg();
  yTitle(T(S_SET_TIME), nullptr);

  // tarjeta de la hora: la hora grande, sus -/+ y las etiquetas
  yCard(CLK_CARD_X, CLK_CARD_Y, CLK_CARD_W, CLK_CARD_H, false);
  char t[8];
  snprintf(t, sizeof(t), "%02d:%02d", clockH, clockM);
  gfx->setTextColor(Y_TEXT);
  setSize(7);
  setCur(centerX(t, 7), CLK_TIME_Y);
  printT(t);
  for (int i = 0; i < 4; i++) drawClockBtn(clkBtnX(i), CLK_BTN_Y, i % 2 == 1);
  gfx->setTextColor(Y_TEXT_DIM);
  setSize(2);
  setCur(CX - CLK_PAIR_DX - textW(T(S_HOUR), 2) / 2, CLK_LBL_Y);
  printT(T(S_HOUR));
  setCur(CX + CLK_PAIR_DX - textW(T(S_MIN), 2) / 2, CLK_LBL_Y);
  printT(T(S_MIN));

  const int r = LANG_PILL_H / 2, my = LANG_PILL_Y + LANG_PILL_H / 2;
  // volumen: altavoz y una barra por nivel; cicla al tocar (alto -> apagado)
  uint8_t vol = audioLevel();
  gfx->fillRoundRect(SND_PILL_X, LANG_PILL_Y, SND_PILL_W, LANG_PILL_H, r, Y_CARD);
  drawSpeakerIcon(SND_PILL_X + 22, my, vol ? Y_TEXT : Y_TEXT_DIM, vol == 0);
  drawVolBars(SND_PILL_X + 58, my + 9, vol, Y_ACCENT_GO, Y_TEXT_DIM);

  // brillo: sol y un punto por nivel; cicla al tocar, como la tecla BOOT
  gfx->fillRoundRect(BRI_PILL_X, LANG_PILL_Y, BRI_PILL_W, LANG_PILL_H, r, Y_CARD);
  drawSunIcon(BRI_PILL_X + 20, my, 5, UI_BAR_WARN);
  drawBrightDots(BRI_PILL_X + 48, my, 6, 24, UI_BAR_WARN, Y_TEXT_DIM);

  // selector de idioma: una pildora que cicla los idiomas al tocar
  gfx->fillRoundRect(LANG_PILL_X, LANG_PILL_Y, LANG_PILL_W, LANG_PILL_H, r, Y_CARD);
  char lp[10];
  snprintf(lp, sizeof(lp), "%s >", LANG_CODES[gLang]);
  gfx->setTextColor(Y_TEXT);
  setSize(2);
  setCur(LANG_PILL_X + (LANG_PILL_W - textW(lp, 2)) / 2, my - 7);
  printT(lp);

  // OK: aplica la hora y cierra
  gfx->fillRoundRect(CLK_OK_X, CLK_OK_Y, CLK_OK_W, CLK_OK_H, CLK_OK_H / 2, Y_ACCENT_GO);
  gfx->setTextColor(Y_TEXT);
  setSize(3);
  setCur(centerX("OK", 3), CLK_OK_Y + (CLK_OK_H - 21) / 2);
  printT("OK");

  // version del firmware (discreta, encima del boton de volver)
  char ver[20];
  snprintf(ver, sizeof(ver), "TamaPoke v%s", FW_VERSION);
  gfx->setTextColor(Y_TEXT_DIM);
  setSize(1);
  setCur(centerX(ver, 1), CLK_VER_Y);
  printT(ver);
  yBackBtn();  // cancela sin aplicar, como deslizar arriba o la tecla USER
  gfx->flush();
}

void clockTap(int16_t x, int16_t y) {
  if (yBackHit(x, y)) {  // volver: sin aplicar la hora
    clockOpen = false;
    return;
  }
  for (int i = 0; i < 4; i++) {  // botones -/+ (zona 2 px mas ancha que el dibujo por cada lado)
    if (!yInRect(x, y, clkBtnX(i) - 2, CLK_BTN_Y - 2, CLK_BTN_S + 4, CLK_BTN_S + 4)) continue;
    if (i == 0) clockH = (clockH + 23) % 24;
    else if (i == 1) clockH = (clockH + 1) % 24;
    else if (i == 2) clockM = (clockM + 59) % 60;
    else clockM = (clockM + 1) % 60;
    return;
  }
  if (y >= CLK_PILL_HIT_Y && y < CLK_PILL_HIT_Y + Y_HIT_MIN) {
    if (x >= SND_PILL_X && x < SND_PILL_X + SND_PILL_W) {  // cicla el volumen
      audioSetLevel(volNext(audioLevel()));
      sfxPlay(SFX_TAP);  // se oye al volumen nuevo (apagado, nada: sfxPlay no encola)
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
  if (yInRect(x, y, CLK_OK_X, CLK_OK_Y, CLK_OK_W, CLK_OK_H)) { applyClock(); return; }
}

// iconos de la mochila, en el orden de IT_* (pet.h)
// insignia de cada fase (v1.65), en el orden de STAGES: la gana quien tumba al jefe
static const char *const *const BADGE_ICON[STAGE_COUNT] = {
  SPR_BADGE_GRASS, SPR_BADGE_WATER, SPR_BADGE_ROCK, SPR_BADGE_FIRE,
  SPR_BADGE_ELECTRIC, SPR_BADGE_GHOST, SPR_BADGE_POISON, SPR_BADGE_DRAGON,
};

static const char *const *const ITEM_ICON[IT_COUNT] = {
  SPR_ICON_GOLDEN, SPR_ICON_PROTEIN, SPR_ICON_IRON, SPR_ICON_CARBOS, SPR_ICON_CHARM, SPR_ICON_RARECANDY, SPR_ICON_EGG,
  SPR_ICON_CAKE, SPR_ICON_REVIVE, SPR_ICON_PLAY,  // la Poke Ball (v2.2) es la del boton Jugar
  SPR_ICON_BERRY_R, SPR_ICON_BERRY_B, SPR_ICON_BERRY_G, SPR_ICON_BERRY_Y, SPR_ICON_BERRY_P, SPR_ICON_BERRY_O,  // v3.2
};
// el icono de cada comida (FOOD_*, food.h): la baya normal, las 6 de colores, la tarta y la dorada
static const char *const *const FOOD_ICON[FOOD_COUNT] = {
  SPR_ICON_BERRY, SPR_ICON_BERRY_R, SPR_ICON_BERRY_B, SPR_ICON_BERRY_G, SPR_ICON_BERRY_Y, SPR_ICON_BERRY_P,
  SPR_ICON_BERRY_O, SPR_ICON_CAKE, SPR_ICON_GOLDEN,
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
  for (uint8_t s = 0; s < HOME_SLOTS; s++) {  // v3.0: los dos huecos de casa
    const PetLife &l = pet.home(s);
    if (l.isEgg()) fxLevelW[s].disarm();
    else fxLevelW[s].reset(l.level());
    fxBondW[s].reset(l.bond);
    fxMistakeW[s].reset(l.careMistakes);
    fxTrainW[s][0].reset(l.trAtk);
    fxTrainW[s][1].reset(l.trDef);
    fxTrainW[s][2].reset(l.trSpe);
    // los avisos sonoros se desarman en vez de apuntar el valor: la primera vuelta del loop solo lo
    // apunta, asi las cacas y los vacios del tiempo apagado (o del bicho que sale de la caja) no suenan
    fxPoopW[s].disarm();
    for (auto &w : fxNeedW[s]) w.disarm();
    fxWasEgg[s] = l.isEgg();
  }
}

// alguien puede estar mirando: pantalla encendida, sin atenuar y sin salvapantallas
bool fxScreenAwake() { return !screenOff && dimStage == DIM_AWAKE && !saverOn(); }

// En cada vuelta del loop (tambien con la pantalla apagada, que no dibuja): recoge lo que el
// bicho dejo pendiente y lo que vigilan los vigilantes. La subida de nivel suena AL MOMENTO,
// tambien a oscuras, sin encender la pantalla (decidido el 2026-11-05); su animacion espera.
void fxCollect(uint32_t now) {
  bool awake = fxScreenAwake();
  bool poopSfx = false, emptySfx = false;
  for (uint8_t s = 0; s < HOME_SLOTS; s++) {  // v3.0: cada hueco con sus vigilantes
    const PetLife &l = pet.home(s);
    if (fxWasEgg[s] && !l.isEgg()) {  // eclosion: con la pantalla a la vista, animacion y sonido en su destello
      if (awake) fxo.push(FXO_HATCH, 0, 0, now, s);
      else sfxPlay(SFX_HATCH);
    }
    fxWasEgg[s] = l.isEgg();
    if (l.isEgg() || l.ceremony) {  // ni nivel ni vinculo que vigilar
      fxLevelW[s].disarm();
      fxBondW[s].disarm();
      fxMistakeW[s].disarm();
      for (auto &w : fxTrainW[s]) w.disarm();
      fxPoopW[s].disarm();
      for (auto &w : fxNeedW[s]) w.disarm();
    } else {
      uint32_t from = 0;
      if (fxLevelW[s].rose(l.level(), &from)) {
        bool chime = !l.sleeping;  // dormido no suena (como todo): sonara al verse, cuando despierte
        if (chime) sfxPlay(SFX_LEVELUP);
        fxq.push(FXB_LEVELUP, (uint16_t)from, l.level(), chime, s);
      }
      if (fxBondW[s].rose(l.bond, &from) && awake && fxBondGate[s].allow(now, FX_BOND_GAP_MS))
        fxo.push(FXO_BOND, 0, (int16_t)(l.bond - from), now, s);
      if (fxMistakeW[s].rose(l.careMistakes) && awake && !l.sleeping) {
        fxo.push(FXO_OOPS, 0, 0, now, s);
        sfxPlay(SFX_OOPS);
      }
      const uint8_t tr[3] = { l.trAtk, l.trDef, l.trSpe };
      for (uint8_t i = 0; i < 3; i++)
        if (fxTrainW[s][i].rose(tr[i], &from)) fxo.push(FXO_TRAIN, i, (int16_t)(tr[i] - from), now, s);
      // avisos sonoros (v1.50), sin mirar `awake`: tambien a oscuras. Dormido no suena nada (y ni
      // ensucia ni se vacia: las necesidades tienen suelo), pero los vigilantes siguen al dia
      bool poop = fxPoopW[s].rose(l.poops);
      const uint8_t need[3] = { l.fullness, l.joy, l.energy };
      bool empty = false;
      for (uint8_t i = 0; i < 3; i++)
        if (fxNeedW[s][i].hitZero(need[i])) empty = true;  // sin cortar: los tres se apuntan
      if (!l.sleeping) {
        poopSfx |= poop;
        emptySfx |= empty;
      }
    }
  }
  // v3.4: las medallas del jugador (un banner por medalla, con sus monedas). Esperan en Pet mientras la cola
  // este llena (la migracion puede dejar muchas a la vez)
  MedalNews mn;
  while (fxq.n < FX_QUEUE - 2 && pet.takeMedalNews(&mn))  // (deja sitio a lo demas: subidas, hitos, metas) fxq.push(FXB_MEDAL, mn.medal, mn.tier, false, 0, mn.coins);
  if (poopSfx) sfxPlay(SFX_POOP);
  if (emptySfx) sfxPlay(SFX_ALERT);  // uno solo aunque se vacien varias a la vez (o en los dos)
  uint16_t days = pet.takeMilestone();
  if (days) fxq.push(FXB_MILESTONE, days);
  uint8_t goalLv = 0;
  uint16_t goalCoins = 0;
  uint8_t tier = pet.takeGoalBanner(&goalLv, &goalCoins);
  // b = niveles que dieron (v1.36): "+N Lv"; en los 4 bits de abajo, y encima (<< 4) las monedas (Y4)
  if (tier) fxq.push(FXB_GOAL, tier, (uint16_t)((goalLv & 15) | (goalCoins << 4)));
  fxo.step(now, false);  // caducan los que no llegan a verse
}

// la pantalla principal no puede empezar una celebracion: nadie la ve, el bicho duerme o hay
// algo encima (dialogo, menu, evolucion, despedida, eclosion)
bool fxScreenBusy() {
  return !fxScreenAwake() || pet.homeAsleep() || pet.busyEnding() || pet.evolving() || choiceKind || confirmUntil ||
         panelKind || petMenuOpen || pet.travelResultPending() || fxo.find(FXO_HATCH) || fxo.waiting(FXO_HATCH) || fxo.find(FXO_BOX) ||
         fxo.waiting(FXO_BOX);
}

// pose de un gesto del bicho (PMD): una vez, sosteniendo el ultimo frame hasta minMs. v3.0: el del
// hueco s (fxPose: el del foco); en el patio, su paseante es p[s]
void fxPoseSlot(uint8_t s, uint8_t act, uint32_t now, uint32_t minMs) {
  PmdMon &m = homeMon[s & 1];
  if (!m.has(act)) return;
  uint32_t ms = pmdActTotalMs(m.acts[act]);
  yardGesture(yardW, s & 1, act, ms < minMs ? minMs : ms, now);  // el del patio, igual
}
void fxPose(uint8_t act, uint32_t now, uint32_t minMs) { fxPoseSlot(pet.focusSlot, act, now, minMs); }

// cada frame de la pantalla principal, antes de dibujar: avanza la cola y los efectos
void fxFrame(uint32_t now) {
  fxo.step(now, fxScreenAwake());
  bool started = false;
  fxBan = fxq.step(now, fxScreenBusy(), &started);
  if (!started || !fxBan) return;
  if (fxBan->kind == FXB_LEVELUP) {
    if (!fxBan->chimed) sfxPlay(SFX_LEVELUP);
    const PetLife &l = pet.home(fxBan->slot);  // v3.0: el bicho de esa subida
    for (uint8_t w = 0; w < 3; w++)
      fxGain[w] = (int16_t)l.statAt(w, fxBan->b) - (int16_t)l.statAt(w, fxBan->a);
    // la pose de alegria; el salto (Hop) solo si no tiene: sube tanto que pisa la cabecera
    fxPoseSlot(fxBan->slot, homeMon[fxBan->slot & 1].has(PMD_POSE) ? PMD_POSE : PMD_HOP, now, 1200);
  } else {
    sfxPlay(SFX_MEDAL);
  }
}

// comando serie FX <efecto>: lanza cada efecto para mirarlo en la placa. Solo se ve: no cambia
// el nivel ni el guardado (para una subida de verdad, LVL <n>)
bool fxDebug(const String &k) {
  uint32_t now = millis();
  uint16_t lv = pet.level();
  uint8_t fs = pet.focusSlot;  // v3.0: sobre el bicho del foco
  if (k == "levelup") fxq.push(FXB_LEVELUP, lv, lv + 1, false, fs);
  else if (k == "levelup6") fxq.push(FXB_LEVELUP, lv, lv + 6, false, fs);
  else if (k == "medal") fxq.push(FXB_MEDAL, MD_LV10, MTIER_SILVER, false, 0, medalCoins(MTIER_SILVER));
  else if (k == "streak") fxq.push(FXB_MILESTONE, 7);
  else if (k == "goal") fxq.push(FXB_GOAL, 1, STEP_GOAL_LEVELS[0] | (coinsForGoal(1) << 4));
  else if (k == "goal3") fxq.push(FXB_GOAL, STEP_GOAL_TIERS, STEP_GOAL_LEVELS[STEP_GOAL_TIERS - 1] | (coinsForGoal(STEP_GOAL_TIERS) << 4));
  else if (k == "stat") {
    fxo.push(FXO_STAT, 0, 25, now, fs);
    fxo.push(FXO_STAT, 1, 10, now, fs);
    fxo.push(FXO_STAT, 2, -12, now, fs);
  } else if (k == "bond") fxo.push(FXO_BOND, 0, 2, now, fs);
  else if (k == "train") fxo.push(FXO_TRAIN, 0, 3, now, fs);
  else if (k == "deny") {
    fxo.push(FXO_DENY, 0, 0, now);
    sfxPlay(SFX_DENY);
  } else if (k == "press") fxo.push(FXO_PRESS, 1, 0, now);
  else if (k == "oops") {
    fxo.push(FXO_OOPS, 0, 0, now, fs);
    sfxPlay(SFX_OOPS);
  } else if (k == "hatch" && !pet.isEgg()) fxo.push(FXO_HATCH, 0, 0, now, fs);
  else if (k == "box") fxo.push(FXO_BOX, 0, 0, now, fs);  // el activo sale de una luz (caja, v1.37)
  else return false;
  wakeUI();
  return true;
}

void fxPerfNote(uint32_t us) {
  gFxUs = us;
  if (us > gFxMaxUs) gFxMaxUs = us;
}

// cabeza del bicho en pantalla, aproximada (donde van el "!", el entrenamiento y la pildora). v3.0: los "Of"
// son los del hueco s (su paseante es p[s]); sin "Of", el del foco
int fxHeadYOf(uint8_t s) {
  if (yardW.p[s & 1].on) return yardPetHeadY(s & 1) - 14;  // por encima del globo del estado
  if (!homeMon[s & 1].loaded) return PET_CY - 60;
  int head = PET_GROUND - pmdIdleHeight() + 24;
  return head < 130 ? 130 : head;
}
int fxHeadY() { return fxHeadYOf(pet.focusSlot); }

// el bicho de casa esta en el patio (v2.0): los efectos salen de donde pasea
bool petInYard() {
  return yardW.p[pet.focusSlot].on;
}

// los pies del bicho de casa en pantalla: en el patio, donde pasea; si no, el centro
int petFootXOf(uint8_t s) {
  if (yardW.p[s & 1].on) return (int)yardW.p[s & 1].x;
  return CX;
}
int petFootYOf(uint8_t s) {
  if (yardW.p[s & 1].on) return (int)yardW.p[s & 1].y;
  return PET_GROUND;
}
int petFootX() { return petFootXOf(pet.focusSlot); }
int petFootY() { return petFootYOf(pet.focusSlot); }
// alto del bicho de casa sobre sus pies (para los efectos que salen de el)
int petBodyHOf(uint8_t s) {
  if (yardW.p[s & 1].on) return petFootYOf(s) - yardPetHeadY(s & 1);
  return homeMon[s & 1].loaded ? pmdIdleHeight() : 140;
}
int petBodyH() { return petBodyHOf(pet.focusSlot); }

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
  int px = petFootX(), foot = petFootY(), rise = petBodyH() * 190 / 140;
  int spread = petInYard() ? 24 : 44;
  if (e < 2000) {
    for (int i = 0; i < FX_SPARKS; i++) {
      uint32_t h = fxHash((uint32_t)i * 2654435761u + 7u);
      uint32_t ph = (e + h % 700) % 700;  // cada chispa sube en 0,7 s y vuelve a empezar
      int side = (i & 1) ? 1 : -1;
      int x = px + side * (spread + (int)((h >> 8) % 64) * spread / 44);
      int y = foot - 12 - (int)(ph * rise / 700);
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
    if (ev.b & 15) snprintf(lvl, sizeof(lvl), "+%u Lv", (unsigned)(ev.b & 15));
    uint16_t coinsWon = ev.b >> 4;
    if (tier == STEP_GOAL_TIERS) {  // 10000: banner mayor, con una bandera por meta y "+2 Lv"
      int bigH = coinsWon ? 172 : 140;
      gfx->fillRoundRect(53, 128 + oy, 360, bigH, 18, UI_BAR_WARN);
      gfx->drawRoundRect(53, 128 + oy, 360, bigH, 18, UI_INK);
      gfx->drawRoundRect(56, 131 + oy, 354, bigH - 6, 16, UI_INK);
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
      drawCoinGain(274 + oy, coinsWon, UI_INK);
      drawConfetti(e);
      return;
    }
    int bh = (lvl[0] ? 120 : 96) + (coinsWon ? 26 : 0);  // con "+N Lv" y con monedas, una linea mas cada una
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
    drawCoinGain((lvl[0] ? 264 : 238) + oy, coinsWon, UI_INK);
    return;
  }
  // v3.4: medalla del jugador: "MEDALLA!", su nombre y el tramo, y las monedas que pago
  if (ev.kind == FXB_MEDAL) {
    int bh = ev.c ? 132 : 106;
    gfx->fillRoundRect(73, 146 + oy, 320, bh, 16, UI_BAR_WARN);
    gfx->drawRoundRect(73, 146 + oy, 320, bh, 16, UI_INK);
    drawMedalDisc(110, 190 + oy, 18, (uint8_t)ev.b);
    gfx->drawCircle(110, 190 + oy, 19, UI_INK);  // (el oro y el bronce, sobre el ambar del banner)
    gfx->setTextColor(UI_INK);
    setSize(3);
    setCur(centerX(T(S_MEDAL_BANNER), 3) + 16, 168 + oy);
    printT(T(S_MEDAL_BANNER));
    setSize(2);
    printTight(medalName(ev.a), CX, 204 + oy, 290);
    printTight(medalTierName((uint8_t)ev.b), CX, 226 + oy, 290);
    if (ev.c) drawCoinGain(250 + oy, ev.c, UI_INK);
    drawConfetti(e);
    return;
  }
  // hito de racha
  // 32 y no 20: "%u にちれんぞく！" ya pasa de 20 con una sola cifra, y snprintf
  // cortaba a mitad de un caracter de 3 bytes.
  char buf[32];
  snprintf(buf, sizeof(buf), T(S_STREAK_DAYS_FMT), (unsigned)ev.a);
  const char *l1 = T(S_GREAT), *l2 = buf;
  gfx->fillRoundRect(73, 150 + oy, 320, 96, 16, UI_BAR_WARN);
  gfx->drawRoundRect(73, 150 + oy, 320, 96, 16, UI_INK);
  gfx->setTextColor(UI_INK);
  setSize(3);
  setCur(centerX(l1, 3), 176 + oy);
  printT(l1);
  setSize(2);
  setCur(centerX(l2, 2), 212 + oy);
  printT(l2);
}

// necesidades antes y despues de una accion: los cambios de FX_STAT_MIN o mas sacan su "+N"
FxNeedSnap fxSnap() { return FxNeedSnap{ { pet.fullness, pet.joy, pet.energy } }; }
void fxNeeds(const FxNeedSnap &before) {
  const uint8_t v[3] = { pet.fullness, pet.joy, pet.energy };
  uint32_t now = millis();
  for (uint8_t i = 0; i < 3; i++) {
    int d = (int)v[i] - (int)before.v[i];
    if (d >= FX_STAT_MIN || d <= -FX_STAT_MIN) fxo.push(FXO_STAT, i, (int16_t)d, now, pet.focusSlot);
  }
}

// eclosion: mientras se rompe el huevo (las primeras tres cuartas partes) el bicho aun no se ve
// v3.0: la del hueco s (fxHatchHidesPet: la del foco)
bool fxHatchHidesPetSlot(uint8_t s, uint32_t now) {
  for (auto &x : fxo.o)
    if (x.on && x.kind == FXO_HATCH && x.slot == (s & 1) && (!x.started || FxOverlays::t(x, now) < 0.75f)) return true;
  return false;
}
bool fxHatchHidesPet(uint32_t now) { return fxHatchHidesPetSlot(pet.focusSlot, now); }

// donde se rompe el huevo (drawHatchFx): el centro del sprite y su escala. La pantalla del huevo, en el
// centro a escala 5; el patio (Y3) lo pone donde esta su huevo, a escala 3 (hatchAt)
// v3.0: uno por hueco de casa (hatchAt: los dos, para la pantalla sin patio)
int gHatchX[HOME_SLOTS] = { CX, CX }, gHatchY[HOME_SLOTS] = { PET_CY, PET_CY }, gHatchS[HOME_SLOTS] = { 5, 5 };
void hatchAtSlot(uint8_t slot, int cx, int cy, int s) {
  gHatchX[slot & 1] = cx;
  gHatchY[slot & 1] = cy;
  gHatchS[slot & 1] = s;
}
void hatchAt(int cx, int cy, int s) {
  hatchAtSlot(0, cx, cy, s);
  hatchAtSlot(1, cx, cy, s);
}

// el huevo con el centro del sprite en (cx, cy), a escala s y con sus grietas. v2.4: pintado con el color
// del tipo del bicho dex que lleva (eggcolor.h); dex 0 = el huevo clasico
void drawEggAt(int cx, int cy, int s, uint8_t cracks, int16_t dex) {
  int x = cx - 16 * s, y = cy - 16 * s;
  uint8_t t1, t2;
  if (!eggTypesOf(dex, t1, t2)) {
    drawMap(SPR_EGG, SPRITE_H, x, y, s, false);
  } else {
    for (int r = 0; r < SPRITE_H; r++) {
      const char *row = SPR_EGG[r];
      forRuns(SPRITE_H, [&](int c) {
        if (row[c] == '.') return -1;
        uint16_t col;
        return (int)(eggCellColor(row[c], t1, t2, c, r, col) ? col : spriteColor(row[c]));
      }, [&](int c0, int len, int k) { gfx->fillRect(x + c0 * s, y + r * s, len * s, s, (uint16_t)k); });
    }
  }
  if (cracks >= 1)
    for (auto &c : CRACK1) gfx->fillRect(x + c[0] * s, y + c[1] * s, s, s, INK_K);
  if (cracks >= 2)
    for (auto &c : CRACK2) gfx->fillRect(x + c[0] * s, y + c[1] * s, s, s, INK_K);
}

// eclosion (FX_HATCH_MS): el huevo tiembla cada vez mas, salta la cascara con un destello
// blanco y aparece el bicho haciendo su pose. true = esta parte ocupa el sitio del bicho.
// v3.0: la del hueco slot (drawHatchFx: la del foco)
bool drawHatchFxSlot(uint8_t slot, uint32_t now) {
  slot &= 1;
  const FxOverlay *h = fxo.find(FXO_HATCH, -1, slot);
  if (!h) return false;
  float t = FxOverlays::t(*h, now);
  static uint32_t soundFor[HOME_SLOTS] = { 0, 0 }, poseFor[HOME_SLOTS] = { 0, 0 };
  // escala del sitio (5 en la pantalla del huevo): el destello y la cascara, en proporcion
  const int S = gHatchS[slot], HX = gHatchX[slot], HY = gHatchY[slot];
  if (t < 0.55f) {
    float k = t / 0.55f;
    drawEggAt(HX + (int)(k * 2 * S * sinf(now * 0.06f)), HY, S, 2, pet.home(slot).speciesId);
    return true;
  }
  if (soundFor[slot] != h->start) {
    soundFor[slot] = h->start;
    sfxPlay(SFX_HATCH);
  }
  if (t < 0.75f) {
    float k = (t - 0.55f) / 0.2f;
    gfx->fillCircle(HX, HY, (40 + (int)(k * 110)) * S / 5, UI_WHITE);
    const uint16_t shell = C565(0xf6, 0xea, 0xc2);
    for (int i = 0; i < 10; i++) {  // trozos de cascara volando (y cayendo), girando
      float a = i * 0.628f + 0.3f;
      int d = (120 + (int)(k * 90)) * (S + 2) / 7;
      int sx = HX + (int)(cosf(a) * d), sy = HY + (int)(sinf(a) * d * 0.7f) + (int)(k * k * 70);
      float r = a + k * 5.0f;
      int x1 = sx + (int)(cosf(r) * 12), y1 = sy + (int)(sinf(r) * 12);
      int x2 = sx + (int)(cosf(r + 2.3f) * 10), y2 = sy + (int)(sinf(r + 2.3f) * 10);
      int x3 = sx + (int)(cosf(r + 4.1f) * 8), y3 = sy + (int)(sinf(r + 4.1f) * 8);
      gfx->fillTriangle(x1, y1, x2, y2, x3, y3, shell);
      gfx->drawTriangle(x1, y1, x2, y2, x3, y3, INK_K);
    }
    return true;
  }
  if (poseFor[slot] != h->start) {
    poseFor[slot] = h->start;
    fxPoseSlot(slot, PMD_POSE, now, 0);
  }
  return false;
}
bool drawHatchFx(uint32_t now) { return drawHatchFxSlot(pet.focusSlot, now); }

// "!" en un bocadillo blanco (el del descuido, FXO_OOPS, y el de ver la comida del patio), con la punta hacia (bx - 22, by + 26)
void drawExclaim(int bx, int by) {
  gfx->fillTriangle(bx - 14, by + 10, bx - 4, by + 16, bx - 22, by + 26, UI_WHITE);
  gfx->fillCircle(bx, by, 19, UI_WHITE);
  gfx->drawCircle(bx, by, 19, UI_INK);
  gfx->setTextColor(UI_BAR_BAD);
  setSize(3);
  setCur(bx - 7, by - 11);
  printT("!");
}

// efectos pequenos de la pantalla principal (la casa del patio)
void drawFxOverlays(uint32_t now) {
  for (auto &x : fxo.o) {
    if (!x.on || !x.started) continue;
    uint8_t ps = x.slot & 1;  // v3.0: sobre el bicho de su hueco
    int head = fxHeadYOf(ps);
    float t = FxOverlays::t(x, now);
    if (x.kind == FXO_STAT) {  // "FEL +20" sube sobre el bicho, una fila por necesidad
      if (yardW.p[ps].on) yardStatPill(ps, x.a, x.v, t);
      continue;
    } else if (x.kind == FXO_BOND) {  // corazon pequeno con "+N", a la izquierda del bicho
      int cx = petFootXOf(ps) - 78, y = petFootYOf(ps) - petBodyHOf(ps) * 150 / 140 - (int)(t * 40);
      drawMap(SPR_HEART, 32, cx - 34, y, 1, false);
      fxNumber(cx + 18, y + 6, x.v, UI_BAR_BAD);
    } else if (x.kind == FXO_TRAIN) {  // "ATK +3" sobre la cabeza, y un ataque
      static uint32_t poseFor = 0;
      if (poseFor != x.start) {
        poseFor = x.start;
        fxPoseSlot(ps, PMD_ATTACK, now, 0);
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
      drawExclaim(petFootXOf(ps) + 58, head - 40 + (int)(3 * sinf(now * 0.012f)));
    } else if (x.kind == FXO_HATCH && t >= 0.75f) {  // aro de luz que se abre al nacer
      float k = (t - 0.75f) / 0.25f;
      for (int r = 0; r < 3; r++) gfx->drawCircle(gHatchX[ps], gHatchY[ps], (120 + (int)(k * 110)) * gHatchS[ps] / 5 + r * 3, UI_WHITE);
    }
  }
}

// Noticias del paseo sobre la pantalla principal: los hallazgos (hasta que se toca). Esperan a
// que la pantalla este encendida y no haya otro dialogo, celebracion o animacion encima
// (FR-1.5). El banner de la meta va en la cola de celebraciones (fx.h) desde la v1.30.
void drawWalkNews() {
  bool busy = screenOff || dimStage != DIM_AWAKE || choiceKind || confirmUntil || panelKind || petMenuOpen ||
              pet.evolving() || fxq.busy() || pet.isDying();
  static bool wasShown = false;
  if (busy || !pet.hasFound()) { wasShown = false; return; }
  bool full = false, gift = false;
  uint8_t it = pet.peekFound(&full, &gift);
  const char *title = T(gift ? S_GIFT : S_FOUND);  // v1.40: el huevo de la despedida o el de regalo
  if (!wasShown) sfxPlay(SFX_MEDAL);  // suena una vez por hallazgo
  wasShown = true;
  foundShown = true;
  int by = 138, bh = full ? 140 : 124;
  gfx->fillRoundRect(73, by, 320, bh, 16, UI_BAR_WARN);
  gfx->drawRoundRect(73, by, 320, bh, 16, UI_INK);
  gfx->setTextColor(UI_INK);
  setSize(3);
  setCur(centerX(title, 3), by + 14);
  printT(title);
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

// Ficha en el panel del patio (Y1): disco oscuro, titulo blanco, filas en tarjetas, puntos de
// pagina claros y el boton redondo de volver. Las tarjetas de fila van de CARD_ROW_X a +CARD_ROW_W.
#define UI_BAR_SATED 0xF474  // #f08ca0, el rosa de la tarta: la comida esta llena y no baja (v1.54)
#define CARD_ROW_X 70
#define CARD_ROW_W 326
#define CARD_DOTS_Y 386  // v3.4: mas abajo (el perfil lleva 5 filas y la pista de renombrar)

// tarjeta de fila de alto h, centrada en un texto de tamano 2 que empieza en ty
static void cardRow(int ty, int h) { yCard(CARD_ROW_X, ty + 7 - h / 2, CARD_ROW_W, h, false); }
// el perfil (v3.4): las necesidades y debajo 5 filas (racha, vinculo, baya y edad, evolucion, descuidos)
#define CARD_NEEDS_Y 150
#define CARD_ROW0_Y 222
#define CARD_ROW_STEP 28
#define CARD_ROW_H 24
#define CARD_HINT_Y 358   // "toca el nombre: renombrar", bajo las filas

// una barra de necesidad en la tarjeta del perfil: etiqueta, pista oscura y relleno
// verde / ambar / rojo (rosa con la tarta)
// el color de una barra de necesidad: saciado, bien (>= 50), aviso (>= 25) o mal (ficha y menu del bicho)
static uint16_t needBarColor(uint8_t val, bool sated) {
  return sated ? UI_BAR_SATED : (val >= 50) ? UI_BAR_OK : (val >= 25) ? UI_BAR_WARN : UI_BAR_BAD;
}

static void cardNeedBar(int x, int y, StrId label, uint8_t val, bool sated) {
  gfx->setTextColor(Y_TEXT);
  setSize(2);
  setCur(x, y);
  printT(T(label));
  int gap = barLabelGap();
  int bx = x + gap + 4, bw = 144 - gap - 4;
  uint16_t fill = needBarColor(val, sated);
  gfx->fillRoundRect(bx, y + 1, bw, 12, 4, Y_CARD_HI);
  int fw = (bw - 4) * val / 100;
  if (fw > 0) gfx->fillRoundRect(bx + 2, y + 3, fw, 8, 3, fill);
}

// tarjeta con las 3 necesidades (v3.1: sin limpieza): COM y FEL arriba, ENE abajo, arriba en y0
static void cardNeeds(int y0) {
  yCard(CARD_ROW_X, y0, CARD_ROW_W, 60, false);
  cardNeedBar(84, y0 + 10, S_BAR_FOOD, pet.fullness, pet.sated());
  cardNeedBar(240, y0 + 10, S_BAR_JOY, pet.joy, false);
  cardNeedBar(84, y0 + 36, S_BAR_ENE, pet.energy, false);
}

// pagina 0: perfil (retrato, identidad, necesidades, racha, vinculo, baya)
void renderCardProfile() {
  const char *nm = pet.nick[0] ? pet.nick : dexName(pet.speciesId);
  char head[32];
  snprintf(head, sizeof(head), T(S_NAME_FMT), pet.shiny ? "*" : "", nm, pet.level());
  gfx->setTextColor(Y_TEXT);
  // auto-encoge: a tamano 3 los nombres largos no caben en la franja estrecha de
  // arriba de la pantalla redonda, asi que se cortaban por el borde. El ancho se
  // MIDE con textW(): strlen() cuenta bytes, y en una fila UTF-8 no hay un byte
  // por caracter ni todos miden lo mismo.
  setSize(3);
  int hts = (textW(head, 3) <= 198) ? 3 : 2;
  setSize(hts);
  setCur(centerX(head, hts), hts == 3 ? Y_TITLE_Y : Y_TITLE_Y + 4);
  printT(head);
  if (pet.nick[0]) {  // especie real bajo el apodo
    const char *sp = dexName(pet.speciesId);
    char par[32];
    snprintf(par, sizeof(par), "(%s)", sp);
    gfx->setTextColor(Y_TEXT_DIM);
    setSize(2);
    setCur(centerX(par, 2), Y_SUB_Y);   // medido, no contado en bytes
    printT(par);
  }

  // retrato animado. v3.4: a escala 2 (debajo van las necesidades, la racha, el vinculo, la baya, la
  // evolucion y los descuidos, que eran de la pagina de progreso)
  if (pmd.loaded) drawPmdAct(PMD_IDLE, CX, pet.nick[0] ? 146 : 140, millis(), true, false, 2);

  // las 3 necesidades (v2.0): en el patio ya no hay barras, y al tocar al bicho se ve aqui
  cardNeeds(CARD_NEEDS_Y);

  // racha con llama, centradas como grupo en su tarjeta
  char rl[40];  // en japones pierde cifras con 30 a partir de 100 dias
  snprintf(rl, sizeof(rl), T(S_STREAK_FMT), pet.streak, pet.bestStreak);
  setSize(2);
  int ry = CARD_ROW0_Y;
  cardRow(ry, CARD_ROW_H);
  int sx = CX - (24 + textW(rl, 2)) / 2, sy = ry - 3;
  gfx->fillTriangle(sx + 8, sy, sx + 1, sy + 18, sx + 15, sy + 18, UI_BAR_BAD);
  gfx->fillTriangle(sx + 8, sy + 7, sx + 4, sy + 18, sx + 12, sy + 18, UI_BAR_WARN);
  gfx->setTextColor(Y_TEXT);
  setCur(sx + 24, ry);
  printT(rl);

  ry += CARD_ROW_STEP;
  cardRow(ry, CARD_ROW_H);
  drawCardStat(ry - 2, T(S_VIN), pet.bond, 100, C565(0xd4, 0x52, 0x7e));

  const char *berry = !pet.berryKnown ? T(S_BERRY_UNK) : T((StrId)(S_IT_BERRY_R + favBerryOf(pet.speciesId)));
  char info[48];
  snprintf(info, sizeof(info), T(S_INFO_FMT), berry,
           (unsigned long)(pet.ageMinutes / 1440));
  ry += CARD_ROW_STEP;
  cardRow(ry, CARD_ROW_H);
  gfx->setTextColor(Y_TEXT);
  setSize(2);
  setCur(centerX(info, 2), ry);
  printT(info);

  // v3.4: el estado de la evolucion (era de la pagina de progreso): lista, bloqueada por una necesidad baja
  // (EVOLVE_MIN_NEED), cuantos niveles faltan (cada descuido suma uno) o forma final
  const DexEntry &d = DEX_TBL[pet.speciesId];
  char evoBuf[32];
  const char *evo;
  uint16_t evoCol = Y_TEXT;
  if (d.evolvesTo == 0) {
    evo = T(S_FINAL_FORM);
  } else {
    int needed = d.evolveLevel + pet.careMistakes;
    if (pet.level() >= needed) {
      if (pet.lowestStat() >= EVOLVE_MIN_NEED) { evo = T(S_EVO_READY); evoCol = Y_ACCENT_GO; }
      else { evo = T(S_EVO_BLOCKED); evoCol = Y_ACCENT_BAD; }
    } else {
      snprintf(evoBuf, sizeof(evoBuf), T(S_EVO_IN_FMT), (unsigned)(needed - pet.level()));
      evo = evoBuf;
    }
  }
  ry += CARD_ROW_STEP;
  cardRow(ry, CARD_ROW_H);
  gfx->setTextColor(evoCol);
  printTight(evo, CX, ry, CARD_ROW_W - 16);

  // descuidos (retrasan la evolucion)
  char ms[24];
  snprintf(ms, sizeof(ms), T(S_MISTAKES_FMT), pet.careMistakes);
  ry += CARD_ROW_STEP;
  cardRow(ry, CARD_ROW_H);
  gfx->setTextColor(pet.careMistakes > 0 ? Y_ACCENT_BAD : Y_TEXT);
  setSize(2);
  setCur(centerX(ms, 2), ry);
  printT(ms);

  gfx->setTextColor(Y_TEXT_DIM);  // tocar el nombre lo cambia (el teclado)
  printTight(T(S_RENAME_HINT), CX, CARD_HINT_Y, 300);
}

// pagina 1: estadisticas (v3.4, era "Combate"; 4 barras). El saco de entrenamiento ya no sale de aqui sino
// del menu de JUGAR (v1.34), con los otros juegos: las barras ocupan el sitio del boton.
void renderCardStats() {
  yTitle(T(S_STATS), nullptr);

  // cada stat en su tarjeta: la barra de combate y debajo la fina del entrenamiento
  const int y0 = 104, step = 56;
  for (int r = 0; r < 3; r++) yCard(CARD_ROW_X, y0 + r * step - 9, CARD_ROW_W, 50, false);
  yCard(CARD_ROW_X, y0 + 3 * step - 9, CARD_ROW_W, 32, false);
  drawCardStat(y0, T(S_STAT_ATK), pet.atkStat(), 260, UI_BAR_BAD);
  drawCardTrain(y0, pet.trAtk, UI_BAR_BAD);
  drawCardStat(y0 + step, T(S_STAT_DEF), pet.defStat(), 260, 0x4C98);
  drawCardTrain(y0 + step, pet.trDef, 0x4C98);
  drawCardStat(y0 + 2 * step, T(S_STAT_SPE), pet.speStat(), 260, UI_BAR_WARN);
  drawCardTrain(y0 + 2 * step, pet.trSpe, UI_BAR_WARN);
  drawCardStat(y0 + 3 * step, T(S_STAT_WGT), pet.weight, 100, 0xB3C8);

  // que es la barra fina (v1.49)
  gfx->setTextColor(Y_TEXT_DIM);
  setSize(2);
  setCur(centerX(T(S_TRAIN_LEGEND), 2), 338);
  printT(T(S_TRAIN_LEGEND));
}

// la pagina del paseo (FR-1.7; v3.4 en la pagina del jugador, era la 5.a de la ficha). Pasos de hoy con un anillo
// hacia la meta, el total y los hallazgos de hoy. La mochila tiene su boton desde la v1.53 (renderBag).
#define WALK_RING_Y 150
#define WALK_RING_R 60   // radio exterior del anillo (el interior, 13 px menos)

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

void renderUserWalk() {
  yTitle(T(S_WALK), nullptr);

  uint32_t day = pet.dayNumber();
  if (!imuPresent()) {  // FR-1.9: sin IMU lo dice y nada mas cambia
    gfx->setTextColor(Y_TEXT_DIM);
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
    gfx->fillArc(CX, WALK_RING_Y, WALK_RING_R, WALK_RING_R - 13, 0, 360, Y_CARD_HI);
    float frac = tiers >= STEP_GOAL_TIERS ? 1.0f : (float)st / next;
    if (frac >= 1.0f) gfx->fillArc(CX, WALK_RING_Y, WALK_RING_R, WALK_RING_R - 13, 0, 360, Y_ACCENT_GO);
    else if (frac > 0.003f)  // de las 12 en el sentido de las agujas del reloj
      gfx->fillArc(CX, WALK_RING_Y, WALK_RING_R, WALK_RING_R - 13, 270, 270 + 360 * frac, Y_BACK);
    char num[12];
    snprintf(num, sizeof(num), "%lu", (unsigned long)st);
    uint8_t ns = strlen(num) <= 5 ? 3 : 2;
    gfx->setTextColor(Y_TEXT);
    setSize(ns);
    setCur(centerX(num, ns), WALK_RING_Y - (ns == 3 ? 18 : 14));
    printT(num);
    char goal[10];
    snprintf(goal, sizeof(goal), "/%lu", (unsigned long)next);
    gfx->setTextColor(Y_TEXT_DIM);
    setSize(2);
    setCur(centerX(goal, 2), WALK_RING_Y + 12);
    printT(goal);
    // una bandera por meta de hoy, en fila arriba a la derecha del anillo
    for (int k = 0; k < tiers; k++) drawMap(SPR_ICON_FLAG, 16, CX + 52 + k * 26, WALK_RING_Y - 70, 2, false);

    // el total y los hallazgos, en una tarjeta
    yCard(CARD_ROW_X, 228, CARD_ROW_W, 60, false);
    char tot[32];
    snprintf(tot, sizeof(tot), T(S_STEPS_TOTAL_FMT), (unsigned long)(pet.stepsTotal + imuStepsPending()));
    gfx->setTextColor(Y_TEXT);
    setCur(centerX(tot, 2), 238);
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
    setCur(centerX(fnd, 2), 262);
    printT(fnd);
  }

  // la mochila tiene su propio boton desde la v1.53: aqui solo la pista de los huevos
  if (pet.eggsOwned()) {
    gfx->setTextColor(Y_TEXT);
    setSize(2);
    setCur(centerX(T(S_EGG_HINT), 2), 312);
    printT(T(S_EGG_HINT));
  }
}

// ---------- mochila (v1.53) ----------
// Pantalla propia, desde su boton en la fila de abajo (antes era una fila de la pagina de paseo de
// la ficha). Una rejilla de 4 x 3 casillas (v2.2, era 3 x 3: entra la Poke Ball), cada objeto con su
// cuenta; tocar uno que se pueda usar pide confirmacion y lo usa. Se abre tambien con el bicho
// dormido, para mirar: canUseItem ya no deja usar nada dormido.
#define BAG_COLS 4  // v2.2: 4 x 3 (era 3 x 3: entra la Poke Ball; la tienda, 8 objetos y los Fondos)
#define BAG_ROWS 3
#define BAG_SLOT_W 76
#define BAG_SLOT_H 76
#define BAG_GAP 8
#define BAG_X (CX - (BAG_COLS * BAG_SLOT_W + (BAG_COLS - 1) * BAG_GAP) / 2)
#define BAG_Y 98  // panel del patio (Y1/Y4b): las dos pestanas y las monedas arriba, la rejilla debajo
#define BAG_HINT_Y (BAG_Y + BAG_ROWS * BAG_SLOT_H + (BAG_ROWS - 1) * BAG_GAP + 14)
#define BAG_REVIVE_HINT_MS 3000UL  // tocar el Revivir: dice para que sirve (no se usa a mano)
// "usar objeto?" (Y1): una tarjeta que tapa la rejilla entera, con dos pastillas, Usar (verde) y NO
#define BAG_ASK_X (BAG_X - 4)
#define BAG_ASK_Y (BAG_Y - 4)
#define BAG_ASK_W (BAG_COLS * BAG_SLOT_W + (BAG_COLS - 1) * BAG_GAP + 8)
#define BAG_ASK_H (BAG_ROWS * BAG_SLOT_H + (BAG_ROWS - 1) * BAG_GAP + 8)
#define BAG_ASK_PILL_Y (BAG_ASK_Y + 170)
#define BAG_ASK_PILL_GAP 24
// orden en pantalla: lo que se come, las vitaminas, el caramelo y lo que no se usa a mano
static const uint8_t BAG_ORDER[] = { IT_GOLDEN, IT_CAKE, IT_RARECANDY, IT_PROTEIN, IT_IRON, IT_CARBOS,
                                     IT_CHARM, IT_EGG, IT_REVIVE, IT_BALL,
                                     IT_BERRY_R, IT_BERRY_B, IT_BERRY_G, IT_BERRY_Y, IT_BERRY_P, IT_BERRY_O };  // v3.2: 2.a pagina
#define BAG_SLOTS (sizeof(BAG_ORDER) / sizeof(BAG_ORDER[0]))
#define BAG_PAGE0 10   // casillas de la 1.a pagina; el resto (las 6 bayas de colores) van en la 2.a
#define BAG_PAGES 2
#define BAG_PAGER_X (CX + 104)  // el "1/2" a la derecha de las monedas
#define BAG_PAGER_Y (BAG_COIN_Y + 2)
static_assert(BAG_SLOTS == IT_COUNT, "cada objeto tiene su casilla en la mochila");

// la casilla k de la mochila (o la tienda) en su pagina: las de la 2.a vuelven a empezar arriba a la izquierda
static int bagPageOf(int k) { return k < BAG_PAGE0 ? 0 : 1; }
static int bagCellOf(int k) { return k < BAG_PAGE0 ? k : k - BAG_PAGE0; }

void bagSlotXY(int k, int &x, int &y) {
  x = BAG_X + (k % BAG_COLS) * (BAG_SLOT_W + BAG_GAP);
  y = BAG_Y + (k / BAG_COLS) * (BAG_SLOT_H + BAG_GAP);
}

// las dos pastillas del "usar objeto?": x de cada una (centradas como pareja) y sus anchos
static void bagAskPills(int &xUse, int &wUse, int &xNo, int &wNo) {
  wUse = yPillWidth(T(S_USE));
  wNo = yPillWidth(T(S_NO));
  xUse = CX - (wUse + BAG_ASK_PILL_GAP + wNo) / 2;
  xNo = xUse + wUse + BAG_ASK_PILL_GAP;
}

// la mochila (dock: Objetos y la pestana de la Coleccion) o, v3.3, la tienda (su propio boton del dock, la
// misma pantalla con bagTab = 1 y sin pestanas)
void openBagTab(uint8_t tab) {
  bagOpen = true;
  bagTab = tab;
  bagPage = 0;
  itemConfirm = 0;
  shopBuyId = 0;
  shopToast = 0;
  yardMapPreview = 0;
  bagReviveHintUntil = 0;
}
void openBag() { openBagTab(0); }
void openShop() { openBagTab(1); }

// cerrar la mochila del todo (el selector de fondos suelta su mapa de prueba: el patio recarga el suyo)
void closeBag() {
  bagOpen = false;
  itemConfirm = 0;
  shopBuyId = 0;
  shopToast = 0;
  yardMapPreview = 0;
}

// ---------- pestanas y tienda (v2.0, Y4b) ----------
#define BAG_COIN_Y 78     // las monedas, centradas bajo las pestanas
#define SHOP_BG_SLOT 8    // el 9o hueco (v2.2; era el 8o): la tarjeta "Fondos", de ancho doble
#define SHOP_PRICE_OFF yRgb(0x5A5E74)  // precio que no se puede pagar
#define PICK_TOAST_Y 282
#define PICK_PAGER_Y 322
#define PICK_PILL_Y 350

// la cabecera: v3.4, sin pestanas (la Coleccion se fue a la pagina del jugador). La mochila se titula
// "Mochila" y la Tienda (v3.3, su boton del dock) "Tienda"; debajo, las monedas
static void drawBagTabs() {
  yTitle(T(bagTab == 1 ? S_SHOP : S_BACKPACK), nullptr);
  yCoinBadge(CX, BAG_COIN_Y, pet.coins, false);
}

// moneda pequena + precio, centrados como grupo en cx (y = arriba del texto)
static void shopPrice(int cx, int y, uint16_t price, uint16_t col) {
  char s[8];
  snprintf(s, sizeof(s), "%u", (unsigned)price);
  setSize(2);
  int tw = textW(s, 2), w = 14 + 5 + tw, x = cx - w / 2;
  yCoin(x + 7, y + 7, 7);
  gfx->setTextColor(col);
  setCur(x + 19, y);
  printT(s);
}

// v3.2: "1/2" de las paginas de Objetos y Tienda (tocarlo pasa de pagina)
static void bagPager() {
  char s[12];
  snprintf(s, sizeof(s), "%d/%d", bagPage + 1, BAG_PAGES);
  gfx->setTextColor(Y_TEXT_DIM);
  setSize(2);
  setCur(BAG_PAGER_X - textW(s, 2) / 2, BAG_PAGER_Y);
  printT(s);
}

// aviso (verde: comprado; rojo: no se pudo) con texto centrado
static void shopDrawToast(int y) {
  if (!shopToast) return;
  const char *t = shopToast == SHT_OK ? T(S_PURCHASED) : shopToast == SHT_BROKE ? T(S_NOT_ENOUGH) : T(S_BAG_FULL);
  setSize(2);
  int w = textW(t, 2) + 32;
  yToastC(CX - w / 2, y, w, 32, t, shopToast == SHT_OK ? Y_ACCENT_GO : Y_ACCENT_BAD);
}

static void shopSetToast(uint8_t kind) {
  shopToast = kind;
  shopToastUntil = millis() + SHOP_TOAST_MS;
}

// una tarjeta de la tienda: icono, cuanto hay (x3) y el precio, gris si no se puede pagar
static void shopCell(int k, uint8_t id) {
  int x, y;
  bagSlotXY(k, x, y);
  uint8_t n = pet.items[id];
  uint16_t pr = priceOfItem(id);
  bool can = shopCheckItem(id, pet.coins, n, ITEM_MAX) == SB_OK;
  yCard(x, y, BAG_SLOT_W, BAG_SLOT_H, false);
  drawMap(ITEM_ICON[id], 16, x + 6, y + 6, 2, false);
  char c[6];
  snprintf(c, sizeof(c), "x%u", n);
  gfx->setTextColor(n >= ITEM_MAX ? UI_BAR_WARN : n ? Y_TEXT : Y_TEXT_DIM);
  setSize(2);
  setCur(x + 42, y + 13);
  printT(c);
  shopPrice(x + BAG_SLOT_W / 2, y + 50, pr, can ? Y_TEXT : SHOP_PRICE_OFF);
}

// la tarjeta "Fondos": un paisaje en miniatura y su nombre
static void shopBgCell() {
  int x, y;
  bagSlotXY(SHOP_BG_SLOT, x, y);
  int w = 2 * BAG_SLOT_W + BAG_GAP;
  yCard(x, y, w, BAG_SLOT_H, false);
  gfx->fillRoundRect(x + 16, y + 8, w - 32, 34, 6, Y_BACK);
  gfx->fillCircle(x + w - 40, y + 20, 6, Y_COIN);
  gfx->fillRoundRect(x + 16, y + 26, w - 32, 16, 6, Y_ACCENT_GO);
  const char *t = T(S_BACKGROUNDS);
  gfx->setTextColor(Y_TEXT);
  setSize(2);
  setCur(x + (w - textW(t, 2)) / 2, y + 50);
  printT(t);
}

static void shopAskPills(const char *yes, int &xY, int &wY, int &xN, int &wN) {
  wY = yPillWidth(yes);
  wN = yPillWidth(T(S_NO));
  xY = CX - (wY + BAG_ASK_PILL_GAP + wN) / 2;
  xN = xY + wY + BAG_ASK_PILL_GAP;
}

static void renderShop() {
  for (int k = 0; k < SHOP_ITEMS; k++)
    if (k >= SHOP_PAGE0 ? bagPage == 1 : bagPage == 0) shopCell(k >= SHOP_PAGE0 ? k - SHOP_PAGE0 : k, SHOP_ORDER[k]);
  if (bagPage == 0) shopBgCell();
  if (!shopBuyId) bagPager();
  if (shopToast) {
    shopDrawToast(BAG_HINT_Y - 8);
  } else {
    setSize(2);
    gfx->setTextColor(Y_TEXT_DIM);
    setCur(centerX(T(S_SHOP_HINT), 2), BAG_HINT_Y);
    printT(T(S_SHOP_HINT));
  }
  if (shopBuyId) {  // "comprar?": la tarjeta tapa la rejilla
    uint8_t it = shopBuyId - 1;
    uint16_t pr = priceOfItem(it);
    bool can = shopCheckItem(it, pet.coins, pet.items[it], ITEM_MAX) == SB_OK;
    yCard(BAG_ASK_X, BAG_ASK_Y, BAG_ASK_W, BAG_ASK_H, true);
    drawMap(ITEM_ICON[it], 16, CX - 32, BAG_ASK_Y + 30, 4, false);
    char q[48];
    snprintf(q, sizeof(q), T(S_BUY_FMT), T((StrId)(S_IT_GOLDEN + it)));
    gfx->setTextColor(Y_TEXT);
    setSize(2);
    setCur(centerX(q, 2), BAG_ASK_Y + 108);
    printT(q);
    shopPrice(CX, BAG_ASK_Y + 140, pr, can ? Y_TEXT : SHOP_PRICE_OFF);
    int xy, wy, xn, wn;
    shopAskPills(T(S_BUY), xy, wy, xn, wn);
    yPill(xy, BAG_ASK_PILL_Y, T(S_BUY), can ? Y_ACCENT_GO : SHOP_PRICE_OFF);
    yPill(xn, BAG_ASK_PILL_Y, T(S_NO), Y_ACCENT_BAD);
  }
}

// el selector de fondos: el mapa a pantalla completa tal como lo dibuja el patio
static void pickerChevrons(uint8_t n) {
  if (n > 1) {
    yChevron(Y_CHEV_LX + 2, Y_CY + 2, -1, Y_SHADOW);
    yChevron(Y_CHEV_LX, Y_CY, -1, Y_TEXT);
  }
  if (n < YARD_MAPS) {
    yChevron(Y_CHEV_RX + 2, Y_CY + 2, 1, Y_SHADOW);
    yChevron(Y_CHEV_RX, Y_CY, 1, Y_TEXT);
  }
}

// la pastilla de abajo: ancho y pintado segun su estado; el ancho tambien sirve para tocarla
static int pickerPillW(PickerPill st, uint8_t n) {
  if (st == PK_BUY || st == PK_BROKE) {
    char s[8];
    snprintf(s, sizeof(s), "%u", (unsigned)priceOfMap(n));
    setSize(2);
    return 14 + 6 + textW(s, 2) + 2 * Y_PILL_PAD + 4;
  }
  return yPillWidth(st == PK_USING ? T(S_USING) : T(S_USE));
}

static void pickerDrawPill(PickerPill st, uint8_t n) {
  int w = pickerPillW(st, n), x = CX - w / 2, y = PICK_PILL_Y;
  gfx->fillRoundRect(x + 2, y + 2, w, Y_PILL_H, Y_PILL_H / 2, Y_SHADOW);
  if (st == PK_BUY || st == PK_BROKE) {
    gfx->fillRoundRect(x, y, w, Y_PILL_H, Y_PILL_H / 2, st == PK_BUY ? Y_ACCENT_GO : Y_CARD_HI);
    char s[8];
    snprintf(s, sizeof(s), "%u", (unsigned)priceOfMap(n));
    yCoin(x + Y_PILL_PAD + 9, y + Y_PILL_H / 2, 7);
    gfx->setTextColor(st == PK_BUY ? Y_TEXT : SHOP_PRICE_OFF);
    setSize(2);
    setCur(x + Y_PILL_PAD + 22, y + (Y_PILL_H - 14) / 2);
    printT(s);
  } else if (st == PK_USE) {
    yPill(x, y, T(S_USE), Y_ACCENT_GO);
  } else {
    yPill(x, y, T(S_USING), Y_CARD_HI);
  }
}

static void renderPicker() {
  uint8_t n = yardMapPreview;
  yardDrawBg();  // honra yardMapPreview: carga el mapa n (o el de respaldo si falta)
  bool ok = yardMapNow() == n;  // el que de verdad se ve
  if (ok) yardShowFull();  // v2.5: el mapa como se vera, a toda la pantalla
  const char *name = T((StrId)(S_MAP_1 + n - 1));
  if (!ok) yPanelBg();  // sin el .bin (o sin SD): un panel oscuro con el aviso
  yShadowText(centerX(name, 3), Y_TITLE_Y, name, 3, Y_TEXT);
  if (!ok) {
    const char *m = T(S_NOT_ON_SD);
    yShadowText(centerX(m, 2), Y_CY - 7, m, 2, Y_TEXT_DIM);
  }
  pickerChevrons(n);
  char pg[12];
  snprintf(pg, sizeof(pg), "%u/%d", n, YARD_MAPS);
  yShadowText(centerX(pg, 2), PICK_PAGER_Y, pg, 2, Y_TEXT);
  pickerDrawPill(pickerPill(n, pet.yardMapWanted(), pet.ownsMap(n), pet.coins), n);
  shopDrawToast(PICK_TOAST_Y);
  yBackBtn();
  gfx->flush();
}

static void pickerTap(int16_t x, int16_t y) {
  uint8_t n = yardMapPreview;
  if (yBackHit(x, y)) {
    yardMapPreview = 0;
    return;
  }
  int c = yChevronHit(x, y);
  if (c) {
    uint8_t m = pickerStep(n, c);
    if (m) {
      yardMapPreview = m;
      shopToast = 0;
      sfxPlay(SFX_TAP);
    }
    return;
  }
  PickerPill st = pickerPill(n, pet.yardMapWanted(), pet.ownsMap(n), pet.coins);
  int w = pickerPillW(st, n);
  if (!yPillHit(x, y, CX - w / 2, PICK_PILL_Y, w)) return;
  if (st == PK_USE) {
    pet.chooseMap(n);
    sfxPlay(SFX_LEVEL);
  } else if (st == PK_BUY) {
    if (pet.buyMap(n)) {
      pet.chooseMap(n);
      shopSetToast(SHT_OK);
      sfxPlay(SFX_LEVELUP);
    } else {
      shopSetToast(SHT_BROKE);
      sfxPlay(SFX_DENY);
    }
  } else if (st == PK_BROKE) {
    shopSetToast(SHT_BROKE);
    sfxPlay(SFX_DENY);
  }
}

// toques en la tienda: una tarjeta pide confirmacion (Comprar / NO); "Fondos" abre el selector
static void shopTap(int16_t x, int16_t y) {
  if (shopBuyId) {  // solo la pastilla de Comprar compra; cualquier otro toque es NO
    uint8_t it = shopBuyId - 1;
    shopBuyId = 0;
    int xy, wy, xn, wn;
    shopAskPills(T(S_BUY), xy, wy, xn, wn);
    if (!timeLeft(shopBuyUntil) || !yPillHit(x, y, xy, BAG_ASK_PILL_Y, wy)) return;
    ShopBuy r = shopCheckItem(it, pet.coins, pet.items[it], ITEM_MAX);
    if (r == SB_OK && pet.buyItem(it)) {
      shopSetToast(SHT_OK);
      sfxPlay(SFX_LEVELUP);
    } else {
      shopSetToast(r == SB_FULL ? SHT_FULL : SHT_BROKE);
      sfxPlay(SFX_DENY);
    }
    return;
  }
  for (int k = 0; k < SHOP_ITEMS; k++) {
    if ((k >= SHOP_PAGE0 ? 1 : 0) != bagPage) continue;
    int sx, sy;
    bagSlotXY(k >= SHOP_PAGE0 ? k - SHOP_PAGE0 : k, sx, sy);
    if (!yInRect(x, y, sx, sy, BAG_SLOT_W, BAG_SLOT_H)) continue;
    shopBuyId = SHOP_ORDER[k] + 1;
    shopBuyUntil = millis() + 10000;
    shopToast = 0;
    sfxPlay(SFX_TAP);
    return;
  }
  int bx, by;
  bagSlotXY(SHOP_BG_SLOT, bx, by);
  if (bagPage == 0 && yInRect(x, y, bx, by, 2 * BAG_SLOT_W + BAG_GAP, BAG_SLOT_H)) {
    uint8_t w = pet.yardMapWanted();
    yardMapPreview = w >= 1 && w <= YARD_MAPS ? w : 1;  // se abre en el que se usa
    shopToast = 0;
    sfxPlay(SFX_TAP);
  }
}

// deslizar dentro de la mochila: en el selector, de mapa en mapa; en la tienda, de pagina en pagina; en
// Objetos, sus paginas y despues la Coleccion (izquierda = la siguiente). v3.3: la tienda ya no es pestana
void bagSwipe(int dir) {
  if (yardMapPreview) {
    uint8_t m = pickerStep(yardMapPreview, dir > 0 ? -1 : 1);
    if (m) {
      yardMapPreview = m;
      shopToast = 0;
    }
    return;
  }
  if (itemConfirm || shopBuyId) return;  // con una pregunta abierta, no
  int d = dir > 0 ? -1 : 1;  // derecha = la anterior, izquierda = la siguiente
  int p = (int)bagPage + d;  // v3.2: Objetos y Tienda, sus 2 paginas (v3.4: la Coleccion ya no va detras)
  if (p >= 0 && p < BAG_PAGES) {
    bagPage = (uint8_t)p;
    shopToast = 0;
  }
}

// ---------- la coleccion (v2.0, Y5b; v3.4: 4.a pagina del jugador, era una pestana de la mochila) ----------
// Una rejilla de 5 x 5: una fila por mapa, de comun a raro de izquierda a derecha. Los que se tienen, en
// color (el raro, con marco dorado); los que no, en silueta. Tocar uno abre su tarjeta.
static const char *souvDestName(uint8_t id) { return T((StrId)(S_MAP_1 + souvenirDest(id) - 1)); }

static void souvFrame(int x, int y, int w, int h, uint16_t col) {
  gfx->drawRoundRect(x, y, w, h, Y_CARD_RAD, col);
  gfx->drawRoundRect(x + 1, y + 1, w - 2, h - 2, Y_CARD_RAD - 1, col);
}

static void renderSouvTip(uint8_t id) {
  bool have = pet.hasSouvenir(id);
  uint8_t rar = souvenirRarity(id);
  yCard(SOUV_TIP_X, SOUV_TIP_Y, SOUV_TIP_W, SOUV_TIP_H, true);
  if (have && rar == SOUV_RARE) souvFrame(SOUV_TIP_X, SOUV_TIP_Y, SOUV_TIP_W, SOUV_TIP_H, UI_BAR_WARN);
  int iy = SOUV_TIP_Y + 18;
  if (have) drawMap(SOUV_ICON[id], 16, CX - 32, iy, 4, false);
  else drawMapTint(SOUV_ICON[id], 16, CX - 32, iy, 4, Y_PANEL_BG);
  gfx->setTextColor(Y_TEXT);
  const char *nm = have ? T((StrId)(S_SOUV_0 + id)) : "???";
  setSize(2);
  printTight(nm, CX, SOUV_TIP_Y + 98, SOUV_TIP_W - 24);
  gfx->setTextColor(Y_TEXT_DIM);
  printTight(souvDestName(id), CX, SOUV_TIP_Y + 124, SOUV_TIP_W - 24);
  if (!have) return;
  gfx->setTextColor(rar == SOUV_RARE ? UI_BAR_WARN : rar == SOUV_UNCOMMON ? Y_BACK : Y_TEXT_DIM);
  printTight(T((StrId)(S_RAR_0 + rar)), CX, SOUV_TIP_Y + 150, SOUV_TIP_W - 24);
  uint32_t ep = pet.souvenirFoundEpoch(id);
  if (ep) {
    char d[8];
    char f[40];
    trvDateStr(d, sizeof(d), ep);
    snprintf(f, sizeof(f), T(S_TRV_FOUND_FMT), d);
    gfx->setTextColor(Y_TEXT_DIM);
    printTight(f, CX, SOUV_TIP_Y + 178, SOUV_TIP_W - 24);
  }
}

static void renderCollection() {
  char head[32];
  snprintf(head, sizeof(head), "%s %u/%u", T(S_COLLECTION), (unsigned)pet.souvenirCount(), (unsigned)SOUV_COUNT);
  yTitleFit(head);  // v3.4: el titulo de la pagina del jugador
  for (int id = 0; id < SOUV_COUNT; id++) {
    int x = souvCellX(id), y = souvCellY(id);
    bool have = pet.hasSouvenir((uint8_t)id), rare = souvenirRarity((uint8_t)id) == SOUV_RARE;
    int ix = x + (SOUV_CELL_W - 32) / 2, iy = y + (SOUV_CELL_H - 32) / 2;
    if (have) {
      yCard(x, y, SOUV_CELL_W, SOUV_CELL_H, false);
      drawMap(SOUV_ICON[id], 16, ix, iy, 2, false);
      if (rare) souvFrame(x, y, SOUV_CELL_W, SOUV_CELL_H, UI_BAR_WARN);
    } else {
      gfx->drawRoundRect(x, y, SOUV_CELL_W, SOUV_CELL_H, Y_CARD_RAD, Y_CARD);
      drawMapTint(SOUV_ICON[id], 16, ix, iy, 2, Y_CARD_HI);
    }
  }
  if (souvTip) renderSouvTip(souvTip - 1);
}

static void collectionTap(int16_t x, int16_t y) {
  int id = souvAt(x, y);
  if (id < 0) return;
  souvTip = (uint8_t)(id + 1);
  sfxPlay(SFX_TAP);
}

void renderBag() {
  if (yardMapPreview) {
    renderPicker();
    return;
  }
  yPanelBg();
  drawBagTabs();
  if (bagTab == 1) {
    renderShop();
    yBackBtn();
    gfx->flush();
    return;
  }
  // una tarjeta por objeto, con su cuenta; el amuleto, enmarcado si se lleva. Sin ninguno, la
  // tarjeta queda hundida (solo el borde) y el icono en silueta.
  for (int k = 0; k < (int)BAG_SLOTS; k++) {
    if (bagPageOf(k) != bagPage) continue;
    uint8_t i = BAG_ORDER[k];
    int x, y;
    bagSlotXY(bagCellOf(k), x, y);
    // los huevos (v1.40): tambien el que espera en la incubadora
    uint8_t n = (i == IT_EGG) ? pet.eggsOwned() : pet.items[i];
    bool held = (i == IT_CHARM) && n;
    if (n) yCard(x, y, BAG_SLOT_W, BAG_SLOT_H, false);
    else gfx->drawRoundRect(x, y, BAG_SLOT_W, BAG_SLOT_H, Y_CARD_RAD, Y_CARD);
    if (held) {
      gfx->drawRoundRect(x, y, BAG_SLOT_W, BAG_SLOT_H, Y_CARD_RAD, UI_BAR_WARN);
      gfx->drawRoundRect(x + 1, y + 1, BAG_SLOT_W - 2, BAG_SLOT_H - 2, Y_CARD_RAD - 1, UI_BAR_WARN);
    }
    if (n) drawMap(ITEM_ICON[i], 16, x + (BAG_SLOT_W - 48) / 2, y + 5, 3, false);
    else drawMapTint(ITEM_ICON[i], 16, x + (BAG_SLOT_W - 48) / 2, y + 5, 3, Y_CARD_HI);
    char c[6];
    snprintf(c, sizeof(c), "x%u", n);
    gfx->setTextColor(n ? Y_TEXT : Y_TEXT_DIM);
    setSize(2);
    setCur(x + (BAG_SLOT_W - textW(c, 2)) / 2, y + 56);
    printT(c);
  }
  // una pista: el Revivir recien tocado, dormido, los huevos (v1.39), el amuleto, la mochila vacia o
  // como se usa
  bool empty = pet.itemCount() == 0 && !pet.eggsOwned();
  bool reviveHint = timeLeft(bagReviveHintUntil) > 0;
  const char *hint = reviveHint            ? T(bagHintItem == IT_BALL ? S_BALL_HINT : S_REVIVE_HINT)
                     : empty               ? T(S_BACKPACK_EMPTY)
                     : pet.homeAsleep()    ? T(S_BAG_ASLEEP)
                     : pet.eggsOwned()     ? T(S_EGG_HINT)
                     : pet.items[IT_CHARM] ? T(S_CHARM_HELD)
                                           : T(S_BAG_HINT);
  gfx->setTextColor(empty ? Y_TEXT_DIM : Y_TEXT);
  setSize(2);
  setCur(centerX(hint, 2), BAG_HINT_Y);
  printT(hint);
  if (!itemConfirm) bagPager();
  yBackBtn();

  // "usar objeto?" (mismo dialogo que "soltar?"): tarjeta encima de la rejilla, Usar / NO
  if (itemConfirm) {
    uint8_t it = itemConfirm - 1;
    yCard(BAG_ASK_X, BAG_ASK_Y, BAG_ASK_W, BAG_ASK_H, true);
    drawMap(ITEM_ICON[it], 16, CX - 32, BAG_ASK_Y + 34, 4, false);
    char q[48];
    snprintf(q, sizeof(q), T(S_USE_FMT), T((StrId)(S_IT_GOLDEN + it)));
    gfx->setTextColor(Y_TEXT);
    setSize(2);
    setCur(centerX(q, 2), BAG_ASK_Y + 120);
    printT(q);
    int xu, wu, xn, wn;
    bagAskPills(xu, wu, xn, wn);
    yPill(xu, BAG_ASK_PILL_Y, T(S_USE), Y_ACCENT_GO);
    yPill(xn, BAG_ASK_PILL_Y, T(S_NO), Y_ACCENT_BAD);
  }
  gfx->flush();
}

// v3.3: los huecos de casa (bit s) cuyo bicho puede usar ahora el objeto it de la mochila (las comidas, las
// vitaminas y el Caramelo Raro: canUseItem con el foco en cada uno)
uint8_t itemSlotsMask(uint8_t it) {
  if (foodOfItem(it) != FOOD_COUNT && it < IT_COUNT && pet.items[it] <= yardFoodReserved(it)) return 0;  // ya elegidas
  uint8_t m = 0;
  for (uint8_t s = 0; s < HOME_SLOTS; s++) {
    if (!pet.home(s).hatched()) continue;
    bool ok = false;
    pet.onSlot(s, [&] { ok = pet.canUseItem(it); });
    if (ok) m |= (uint8_t)(1u << s);
  }
  return m;
}

// v3.3: usa el objeto it de la mochila en el bicho del hueco s (sus "+N" salen sobre el). true si se uso
bool bagUseOn(uint8_t s, uint8_t it) {
  bool ok = false;
  if (!(itemSlotsMask(it) & (1u << s))) {  // (ya no puede, o la ultima esta elegida en el patio)
    Serial.printf("ITEM %u slot=%u refused\n", it, s);
    return false;
  }
  pet.onSlot(s, [&] {
    FxNeedSnap ns = fxSnap();
    ok = pet.useItem(it);
    if (ok) fxNeeds(ns);
  });
  Serial.printf("ITEM %u slot=%u %s\n", it, s, ok ? "used" : "refused");
  if (ok) sfxPlay(foodOfItem(it) != FOOD_COUNT ? SFX_EAT : SFX_LEVEL);
  return ok;
}

// toques en la mochila: una casilla pide confirmacion; el boton de volver (o fuera de la rejilla)
// la cierra
void bagTap(int16_t x, int16_t y) {
  if (yardMapPreview) {
    pickerTap(x, y);
    return;
  }
  if (bagTab == 1 && shopBuyId) {
    shopTap(x, y);
    return;
  }
  if (itemConfirm) {  // solo la pastilla de Usar usa el objeto; cualquier otro toque es NO
    uint8_t it = itemConfirm - 1;
    itemConfirm = 0;
    int xu, wu, xn, wn;
    bagAskPills(xu, wu, xn, wn);
    if (timeLeft(itemConfirmUntil) && yPillHit(x, y, xu, BAG_ASK_PILL_Y, wu)) {
      int8_t vs = pet.vacantSlot();
      if (it == IT_EGG && vs >= 0) {  // v3.0: un huevo de la mochila al hueco de casa vacio
        bool ok = false;
        pet.onSlot((uint8_t)vs, [&] { ok = pet.hatchFromBag(); });
        sfxPlay(ok ? SFX_TAP : SFX_DENY);
        if (ok) closeBag();
      } else {
        // v3.3: en el bicho de casa que puede (con los dos se abre antes el selector, petPick); a la pantalla
        // principal a verlo: comer, subir de nivel o entrenar (v1.30, "ATK +3")
        // (el que podia al tocarlo: si ya no puede, no se usa en el otro)
        if (itemConfirmSlot >= 0 && bagUseOn((uint8_t)itemConfirmSlot, it)) closeBag();
        else sfxPlay(SFX_DENY);
      }
    }
    return;
  }
  if (yBackHit(x, y)) {  // como la tecla USER
    closeBag();
    return;
  }
  if (yInRect(x, y, BAG_PAGER_X - 36, BAG_PAGER_Y - 8, 72, BAG_Y - (BAG_PAGER_Y - 8))) {  // v3.2: el "1/2" cambia de pagina
    bagPage = (uint8_t)((bagPage + 1) % BAG_PAGES);
    shopToast = 0;
    sfxPlay(SFX_TAP);
    return;
  }
  if (bagTab == 1) {
    shopTap(x, y);
    return;
  }
  for (int k = 0; k < (int)BAG_SLOTS; k++) {
    if (bagPageOf(k) != bagPage) continue;
    int sx, sy;
    bagSlotXY(bagCellOf(k), sx, sy);
    if (x < sx || x >= sx + BAG_SLOT_W || y < sy || y >= sy + BAG_SLOT_H) continue;
    uint8_t i = BAG_ORDER[k];
    uint8_t m = itemSlotsMask(i);
    if (m == 3) {  // v3.3: los dos de casa pueden usarlo: a quien se lo da (sin el "usar?": elegir ya confirma)
      openPetPick(PICK_ITEM, i);
      sfxPlay(SFX_TAP);
    } else if (m || (i == IT_EGG && pet.items[IT_EGG] && pet.vacantSlot() >= 0 && !pet.awaitingStarter())) {
      itemConfirmSlot = (m & 1) ? 0 : (m & 2) ? 1 : -1;
      // (v3.0: el huevo, con un hueco de casa vacio: "usarlo" lo pone en ese hueco)
      itemConfirm = i + 1;
      itemConfirmUntil = millis() + 10000;
      sfxPlay(SFX_TAP);
    } else if ((i == IT_REVIVE || i == IT_BALL) && pet.items[i]) {
      // el Revivir se guarda para cuando se muera (v1.59); la Poke Ball, para un salvaje vencido (v2.2)
      bagReviveHintUntil = millis() + BAG_REVIVE_HINT_MS;
      bagHintItem = i;
      sfxPlay(SFX_TAP);
    } else if (i == IT_EGG ? pet.eggsOwned() : pet.items[i]) {
      sfxPlay(SFX_DENY);  // el amuleto se lleva y el huevo va solo a casa; o dormido, o entrenamiento al maximo
    }
    return;  // un toque en una casilla nunca cierra la mochila
  }
  closeBag();
}

// puntos de pagina de la ficha en colores claros (sobre el panel oscuro)
static void cardPageDots(int y) {
  for (int i = 0; i < CARD_PAGES; i++) {
    int dx = CX - (CARD_PAGES - 1) * 13 + i * 26;
    if (i == cardPage) gfx->fillCircle(dx, y, 5, Y_TEXT);
    else gfx->fillCircle(dx, y, 4, Y_CARD_HI);
  }
}

void renderCard() {
  yPanelBg();
  if (cardPage == 0) renderCardProfile();
  else renderCardStats();

  // indicador de paginas + boton de volver (tocar en cualquier otro sitio tambien cierra)
  cardPageDots(CARD_DOTS_Y);
  yBackBtn();
  gfx->flush();
}

// ---------- la pagina del jugador (v3.4): deslizar arriba en el patio ----------
// 4 paginas (USERP_*) que se pasan deslizando a los lados, con sus puntos: las medallas del jugador (medals.h),
// el paseo (era la ultima pagina de la ficha), las insignias de los estadios (eran la 2.a pagina de la caja) y la
// coleccion de recuerdos (era una pestana de la mochila). USER, el boton de volver o deslizar a la derecha desde
// la primera vuelven al patio. Las medallas son 19: van en sub-paginas de USER_MEDAL_ROWS filas, que se pasan
// tocando el "1/4" o deslizando arriba / abajo; tocar una fila abre su tarjeta (que mide y sus tres tramos).
#define USER_DOTS_Y 384
#define UMED_Y0 98        // primera fila de medallas
#define UMED_H 44
#define UMED_STEP 50
#define UMED_DISC_R 13
#define UMED_TEXT_X (CARD_ROW_X + 50)
#define UMED_PAGER_Y Y_SUB_Y  // el "1/4" bajo el titulo (su zona de toque, toda la franja)

void openUser(uint8_t page) {
  userOpen = true;
  userPage = page < USER_PAGES ? page : 0;
  userMedalPage = 0;
  userMedalTip = -1;
  souvTip = 0;
}

// el color de un tramo (0 = sin tramo: la pista oscura)
uint16_t medalTierColor(uint8_t tier) {
  switch (tier) {
    case MTIER_BRONZE: return C565(0xcd, 0x7f, 0x32);
    case MTIER_SILVER: return C565(0xc8, 0xcc, 0xd8);
    case MTIER_GOLD: return C565(0xff, 0xc8, 0x2e);
    default: return Y_CARD_HI;
  }
}
const char *medalTierName(uint8_t tier) {
  return tier >= MTIER_BRONZE && tier <= MTIER_GOLD ? T((StrId)(S_TIER_BRONZE + tier - 1)) : "";
}

// una medalla: disco del color del tramo colgado de su cinta; sin tramo, solo el contorno
void drawMedalDisc(int cx, int cy, int r, uint8_t tier) {
  int top = cy - r;
  if (tier) {
    gfx->fillTriangle(cx - r + 2, top - r / 2 - 2, cx - 1, top - r / 2 - 2, cx - 2, top + 3, Y_BACK);
    gfx->fillTriangle(cx + 1, top - r / 2 - 2, cx + r - 2, top - r / 2 - 2, cx + 2, top + 3, UI_BAR_BAD);
    uint16_t col = medalTierColor(tier);
    gfx->fillCircle(cx, cy, r, col);
    gfx->drawCircle(cx, cy, r - 4, (uint16_t)((col >> 1) & 0x7BEF));  // un aro mas oscuro (la mitad de cada canal)
  } else {  // (el gris del texto apagado: se ve sobre la tarjeta normal y sobre la resaltada)
    gfx->drawCircle(cx, cy, r, Y_TEXT_DIM);
  }
}

// titulo a tamano 3 si cabe en la franja estrecha de arriba del circulo; si no, a 2
void yTitleFit(const char *t) {
  gfx->setTextColor(Y_TEXT);
  int ts = textW(t, 3) <= 236 ? 3 : 2;
  setSize(ts);
  setCur(centerX(t, ts), ts == 3 ? Y_TITLE_Y : Y_TITLE_Y + 4);
  printT(t);
}

// la tarjeta de una medalla: que mide, lo que lleva y sus tres tramos con lo que pagan. Tapa la lista entera
#define UMED_TIP_X (CARD_ROW_X - 4)
#define UMED_TIP_Y (UMED_Y0 - 4)
#define UMED_TIP_W (CARD_ROW_W + 8)
#define UMED_TIP_H (USER_MEDAL_ROWS * UMED_STEP - (UMED_STEP - UMED_H) + 8)
static void renderMedalTip(uint8_t i) {
  uint32_t v = pet.medalValue(i);
  uint8_t tier = pet.medalTierPaid(i);
  yCard(UMED_TIP_X, UMED_TIP_Y, UMED_TIP_W, UMED_TIP_H, true);
  drawMedalDisc(CX, UMED_TIP_Y + 46, 18, tier);
  gfx->setTextColor(Y_TEXT);
  printTight(medalName(i), CX, UMED_TIP_Y + 76, UMED_TIP_W - 24);
  char line[48];
  char num[12];
  medalShortNum(num, sizeof(num), v);
  snprintf(line, sizeof(line), "%s: %s", medalDesc(i), num);
  gfx->setTextColor(Y_TEXT_DIM);
  printTight(line, CX, UMED_TIP_Y + 102, UMED_TIP_W - 24);
  const uint32_t *t = MEDAL_T[i];
  for (uint8_t k = 0; k < MEDAL_TIERS; k++) {
    int y = UMED_TIP_Y + 144 + k * 32;
    bool got = tier > k;
    drawMedalDisc(UMED_TIP_X + 40, y + 8, 8, got ? k + 1 : 0);
    gfx->setTextColor(got ? medalTierColor(k + 1) : Y_TEXT_DIM);
    setSize(2);
    setCur(UMED_TIP_X + 58, y);
    printT(medalTierName(k + 1));
    medalShortNum(num, sizeof(num), t[k]);
    setCur(UMED_TIP_X + 170, y);
    printT(num);
    shopPrice(UMED_TIP_X + UMED_TIP_W - 58, y, medalCoins(k + 1), got ? Y_TEXT : Y_TEXT_DIM);
  }
}

static void renderUserMedals() {
  char head[32];
  snprintf(head, sizeof(head), T(S_MEDALS_FMT), (int)pet.medalTiersTotal(), (int)(MEDAL_COUNT * MEDAL_TIERS));
  yTitleFit(head);
  yPager(userMedalPage, USER_MEDAL_PAGES, UMED_PAGER_Y);
  for (int r = 0; r < USER_MEDAL_ROWS; r++) {
    int i = userMedalPage * USER_MEDAL_ROWS + r;
    if (i >= MEDAL_COUNT) break;
    int y = UMED_Y0 + r * UMED_STEP;
    uint8_t tier = pet.medalTierPaid((uint8_t)i);
    uint32_t v = pet.medalValue((uint8_t)i);
    uint32_t next = medalNext((uint8_t)i, v);
    yCard(CARD_ROW_X, y, CARD_ROW_W, UMED_H, false);
    drawMedalDisc(CARD_ROW_X + 26, y + UMED_H / 2 + 3, UMED_DISC_R, tier);
    gfx->setTextColor(Y_TEXT);
    setSize(2);
    setCur(UMED_TEXT_X, y + 5);
    printT(medalName(i));
    // lo que lleva hacia el siguiente tramo ("3/6"); con el Oro, solo el valor, dorado
    char a[12], b[12], prog[26];
    medalShortNum(a, sizeof(a), v);
    if (next) {
      medalShortNum(b, sizeof(b), next);
      snprintf(prog, sizeof(prog), "%s/%s", a, b);
    } else {
      snprintf(prog, sizeof(prog), "%s", a);
    }
    int pw = textW(prog, 2);
    int px = CARD_ROW_X + CARD_ROW_W - 12 - pw;
    gfx->setTextColor(next ? Y_TEXT_DIM : medalTierColor(MTIER_GOLD));
    setCur(px, y + 24);
    printT(prog);
    // la barra, desde el tramo anterior hasta el siguiente
    int bx = UMED_TEXT_X, bw = px - 10 - bx, by = y + 28;
    gfx->fillRoundRect(bx, by, bw, 8, 3, Y_CARD_HI);
    uint32_t from = tier ? MEDAL_T[i][tier - 1] : 0;
    int fw = next ? (int)((uint64_t)(v > from ? v - from : 0) * bw / (next - from)) : bw;
    if (fw > bw) fw = bw;
    if (fw > 2) gfx->fillRoundRect(bx, by, fw, 8, 3, medalTierColor(next ? tier + 1 : MTIER_GOLD));
  }
  if (userMedalTip >= 0) renderMedalTip((uint8_t)userMedalTip);
}

// puntos de pagina de la pagina del jugador (como los de la ficha)
static void userPageDots(int y) {
  for (int i = 0; i < USER_PAGES; i++) {
    int dx = CX - (USER_PAGES - 1) * 13 + i * 26;
    if (i == userPage) gfx->fillCircle(dx, y, 5, Y_TEXT);
    else gfx->fillCircle(dx, y, 4, Y_CARD_HI);
  }
}

void renderUser() {
  yPanelBg();
  if (userPage == USERP_MEDALS) renderUserMedals();
  else if (userPage == USERP_WALK) renderUserWalk();
  else if (userPage == USERP_BADGES) renderUserBadges();
  else renderCollection();
  userPageDots(USER_DOTS_Y);
  yBackBtn();
  gfx->flush();
}

// deslizar a los lados: izquierda avanza; a la derecha desde la primera, al patio
void userSwipe(int dir) {
  souvTip = 0;
  userMedalTip = -1;
  int p = (int)userPage + (dir > 0 ? -1 : 1);
  if (p < 0) {
    userOpen = false;
    return;
  }
  if (p < USER_PAGES) userPage = (uint8_t)p;
}

// deslizar arriba / abajo: en las medallas pasa sus sub-paginas (arriba, la siguiente); abajo desde la primera
// (o en cualquier otra pagina) cierra
void userSwipeV(int dir) {
  if (userPage == USERP_MEDALS && userMedalTip < 0) {
    int p = (int)userMedalPage + (dir < 0 ? 1 : -1);
    if (p >= 0 && p < USER_MEDAL_PAGES) {
      userMedalPage = (uint8_t)p;
      return;
    }
    if (p >= USER_MEDAL_PAGES) return;  // arriba en la ultima: nada
  }
  if (dir > 0) userOpen = false;
}

void userTap(int16_t x, int16_t y) {
  if (yBackHit(x, y)) {
    userOpen = false;
    return;
  }
  if (souvTip || userMedalTip >= 0) {  // una tarjeta abierta: cualquier toque la cierra
    souvTip = 0;
    userMedalTip = -1;
    return;
  }
  if (userPage == USERP_MEDALS) {
    if (y >= UMED_PAGER_Y - 14 && y < UMED_Y0 - 2) {  // el "1/4": la siguiente sub-pagina
      userMedalPage = (uint8_t)((userMedalPage + 1) % USER_MEDAL_PAGES);
      sfxPlay(SFX_TAP);
      return;
    }
    for (int r = 0; r < USER_MEDAL_ROWS; r++) {
      int i = userMedalPage * USER_MEDAL_ROWS + r;
      if (i >= MEDAL_COUNT) break;
      int ry = UMED_Y0 + r * UMED_STEP;
      if (yInRect(x, y, CARD_ROW_X, ry, CARD_ROW_W, UMED_H)) {
        userMedalTip = (int8_t)i;
        sfxPlay(SFX_TAP);
        return;
      }
    }
  } else if (userPage == USERP_COLLECTION) {
    collectionTap(x, y);
  }
}

// ---------- teclado para renombrar ----------

static const char KB_KEYS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ.-";  // 28 + DEL + OK = 30
#define KB_COLS 6
#define KB_W 58  // celda (y zona de toque) de cada tecla; se dibuja 6 px menor
#define KB_H 48
#define KB_X (CX - KB_COLS * KB_W / 2)
#define KB_Y 118  // panel del patio (Y1): todo sube para dejar sitio al boton de volver
#define KB_BOX_X 93
#define KB_BOX_Y 72
#define KB_BOX_W 280
#define KB_BOX_H 36

void openKeyboard() {
  kbOpen = true;
  strncpy(nameBuf, pet.nick, sizeof(nameBuf) - 1);
  nameBuf[sizeof(nameBuf) - 1] = 0;
  nameLen = strlen(nameBuf);
}

void renderKeyboard() {
  yPanelBg();
  yTitle(T(S_NAME), nullptr);
  // buffer actual
  yCard(KB_BOX_X, KB_BOX_Y, KB_BOX_W, KB_BOX_H, true);
  gfx->setTextColor(Y_TEXT);
  setSize(3);
  setCur(KB_BOX_X + 12, KB_BOX_Y + 8);
  printT(nameLen ? nameBuf : "_");

  for (int i = 0; i < 30; i++) {
    int x = KB_X + (i % KB_COLS) * KB_W, y = KB_Y + (i / KB_COLS) * KB_H;
    uint16_t bg = i == 29 ? Y_ACCENT_GO : i == 28 ? Y_CARD_HI : Y_CARD;
    gfx->fillRoundRect(x + 3, y + 3, KB_W - 6, KB_H - 6, 8, bg);
    gfx->setTextColor(Y_TEXT);
    setSize(2);
    char lab[3] = { i < 28 ? KB_KEYS[i] : 0, 0, 0 };
    if (i >= 28) strcpy(lab, i == 28 ? "<-" : "OK");
    setCur(x + (KB_W - textW(lab, 2)) / 2, y + (KB_H - 14) / 2);
    printT(lab);
  }
  yBackBtn();
  gfx->flush();
}

void keyboardTap(int16_t x, int16_t y) {
  // filtra ANTES de dividir: con x/y menores que KB_X/KB_Y la resta da negativo
  // y la division entera trunca hacia cero (no hacia -inf), asi que col/row
  // saldria 0 en vez de negativo y el filtro de abajo no lo detectaria
  if (yBackHit(x, y)) {  // como la tecla USER: descarta el nombre y vuelve a la ficha
    kbOpen = false;
    return;
  }
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

#define GAL_CELL 80  // celda de drawThumb (la usan tambien el combate y el selector de equipo)
// rejilla de la Pokedex en el panel del patio (v2.0, Y1): 4x4 celdas de 72 px, entre las flechas
// (zonas hasta x 64 y desde x 402) y por encima del contador "1/10" y del boton de volver
#define GAL_X 89
#define GAL_Y 82
#define GAL_GC 72
#define GAL_PAGER_Y 374
#define GAL_SIL C565(0x50, 0x55, 0x70)  // silueta de un no registrado, sobre la tarjeta oscura

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

// miniatura de thumbs.bin centrada en (cx, cy) a escala s; sil = silueta en el color silCol
// (en el panel oscuro la tinta INK_K de drawThumb no se ve). clip > 0: no pasa de un cuadrado de
// ese lado centrado en (cx, cy) (los mas grandes se saldrian un poco de su tarjeta)
static void galThumb(const uint8_t *b, int cx, int cy, int s, bool sil, uint16_t silCol, int clip = 0) {
  uint8_t w = b[0], h = b[1], n = b[2];
  const uint8_t *pal = b + 3;
  const uint8_t *d = pal + n * 2;
  int ox = cx - w * s / 2, oy = cy - h * s / 2;
  int lx = clip ? cx - clip / 2 : -9999, rx = clip ? cx + clip / 2 : 9999;
  int ty = clip ? cy - clip / 2 : -9999, by = clip ? cy + clip / 2 : 9999;
  for (int r = 0; r < h; r++) {
    int y0 = oy + r * s, y1 = y0 + s;
    if (y0 < ty) y0 = ty;
    if (y1 > by) y1 = by;
    if (y1 <= y0) continue;
    const uint8_t *row = d + r * w;
    forRuns(w, [&](int c) { return indexedKey(row[c], sil); },
            [&](int c0, int len, int k) {
              uint16_t col = sil ? silCol : (uint16_t)(pal[k * 2] | (pal[k * 2 + 1] << 8));
              int x0 = ox + c0 * s, x1 = x0 + len * s;
              if (x0 < lx) x0 = lx;
              if (x1 > rx) x1 = rx;
              if (x1 > x0) gfx->fillRect(x0, y0, x1 - x0, y1 - y0, col);
            });
  }
}

// silueta quieta del sprite PMD (primer cuadro de reposo) en el color col, con la misma
// escala y el mismo anclaje que drawPmdActM
static void galPmdSil(PmdMon &m, int cx, int groundY, uint8_t maxS, uint16_t col) {
  const PmdAct &a = m.acts[PMD_IDLE];
  if (!a.frames) return;
  uint8_t s = a.h ? 170 / a.h : 5;
  if (s < 2) s = 2;
  if (s > maxS) s = maxS;
  while (s > 2 && a.h * s > 250) s--;
  int x0 = cx - a.w * s / 2, y0 = groundY - (a.base ? a.base : a.h) * s;
  for (int r = 0; r < a.h; r++) {
    const uint8_t *row = a.data + r * a.w;
    forRuns(a.w, [&](int c) { return indexedKey(row[c], true); },
            [&](int c0, int len, int k) { gfx->fillRect(x0 + c0 * s, y0 + r * s, len * s, s, col); });
  }
}

void renderGallery() {
  if (galleryDetail) {  // vista detalle: se redibuja siempre (animada)
    yPanelBg();
    const DexEntry &d = DEX_TBL[galleryDetail];
    bool reg = pet.isRegistered(galleryDetail);
    char head[32];
    snprintf(head, sizeof(head), "N.%03d %s%s", galleryDetail,
             pet.isShinyRegistered(galleryDetail) ? "*" : "", reg ? dexName(galleryDetail) : "???");
    // el color del tipo, aclarado para leerse sobre el disco oscuro
    gfx->setTextColor(reg ? lerp565(d.accent, Y_TEXT, 6, 16) : Y_TEXT);
    // auto-encoge nombres largos (no caben a t3), midiendo en vez de contar
    // bytes; 13 caracteres a tamano 3 son los 234 px de antes.
    setSize(3);
    int gts = (textW(head, 3) <= 234) ? 3 : 2;
    setSize(gts);
    setCur(centerX(head, gts), gts == 3 ? Y_TITLE_Y + 8 : Y_TITLE_Y + 12);
    printT(head);
    if (galleryPmd.loaded) {
      // animado y a color si esta registrado; silueta estatica si no (estilo "?")
      if (reg) drawPmdActM(galleryPmd, PMD_IDLE, CX, 300, millis(), true, false, 6);
      else galPmdSil(galleryPmd, CX, 300, 6, GAL_SIL);
    } else {
      const uint8_t *t = thumbs.get(galleryDetail);
      if (t) galThumb(t, CX, 215, 4, !reg, GAL_SIL);
    }
    yBackBtn();  // tocar en cualquier sitio vuelve a la rejilla, como antes
    gfx->flush();
    return;
  }

  if (!galleryDirty) return;  // la rejilla es estatica
  galleryDirty = false;

  yPanelBg();
  char head[24];
  snprintf(head, sizeof(head), T(S_POKEDEX_FMT), pet.registeredCount());
  yTitle(head, nullptr);

  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      int16_t dex = galleryPage * 16 + r * 4 + c + 1;
      if (dex > 151) break;
      int x = GAL_X + c * GAL_GC, y = GAL_Y + r * GAL_GC;
      bool reg = pet.isRegistered(dex);
      yCard(x + 3, y + 3, GAL_GC - 6, GAL_GC - 6, false);
      const uint8_t *t = thumbs.get(dex);
      if (t) {
        galThumb(t, x + GAL_GC / 2, y + GAL_GC / 2, 2, !reg, GAL_SIL, GAL_GC - 8);
        if (pet.isShinyRegistered(dex)) {
          gfx->setTextColor(UI_BAR_WARN);
          setSize(2);
          setCur(x + GAL_GC - 17, y + 7);
          printT("*");
        }
      } else {
        char num[6];
        snprintf(num, sizeof(num), "%d", dex);
        gfx->setTextColor(Y_TEXT_DIM);
        setSize(2);
        setCur(x + (GAL_GC - textW(num, 2)) / 2, y + GAL_GC / 2 - 7);
        printT(num);
      }
    }
  }
  yChevrons(galleryPage > 0, galleryPage < 9);
  yPager(galleryPage, 10, GAL_PAGER_Y);
  yBackBtn();
  gfx->flush();
}

void galleryTap(int16_t x, int16_t y) {
  if (galleryDetail) {  // volver a la rejilla (tambien el boton de volver)
    galleryDetail = 0;
    galleryPmd.unload();
    galleryDirty = true;
    return;
  }
  if (y < 72 || yBackHit(x, y)) {  // la cabecera o el boton de volver = salir
    galleryOpen = false;
    galleryPmd.unload();
    return;
  }
  int ch = yChevronHit(x, y);  // las flechas: como deslizar (sin salir por la izquierda de la 1)
  if (ch) {
    int np = galleryPage + ch;
    if (np >= 0 && np <= 9 && np != galleryPage) {
      galleryPage = np;
      galleryDirty = true;
    }
    return;
  }
  // mismo motivo que en el teclado: filtrar antes de dividir
  if (x < GAL_X || y < GAL_Y) return;
  int c = (x - GAL_X) / GAL_GC, r = (y - GAL_Y) / GAL_GC;
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

// panel del patio (v2.0, Y1): rejilla de 3x2 huecos de 112x106 entre las flechas < > (sus zonas
// llegan a x 64 y empiezan en x 402), la fila "Ahora:" en su tarjeta, el contador "1/2" y el boton
// de volver abajo
#define BOX_X 65
#define BOX_Y 84
#define BOX_CW 112
#define BOX_CH 106
#define BOX_NOW_Y 330   // centro de la fila "Ahora: ..."
#define BOX_Z_INK Y_BACK  // la "z" de los que duermen
#define BOX_DLG_LINE C565(0x4a, 0x4f, 0x6a)  // borde del dialogo, sobre el disco oscuro
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

// la caja se abre desde la casa sin nada encima: ni dialogo, ni menu, ni evolucion
bool boxCanOpen() {
  return !pet.busyEnding() && !confirmUntil && !choiceKind && !panelKind && !pet.evolving();
}

void openBox() {
  boxOpen = true;
  boxAsk = 0;
  boxAskPick = false;
  boxHome = pet.focusSlot;
  boxMove = BOXMV_NONE;
  boxShakeUntil = 0;
}

void closeBox() {
  boxOpen = false;
  boxAsk = 0;
  boxAskPick = false;
}

// v3.0: el nombre del bicho del hueco de casa s (o "Huevo" / "Vacio")
const char *homeName(uint8_t s) {
  const PetLife &l = pet.home(s);
  if (l.isVacant()) return T(S_BOX_EMPTY);
  if (l.isEgg()) return T(S_EGG_HDR);
  return l.nick[0] ? l.nick : dexName(l.speciesId);
}
// el hueco de casa que ve la caja: el del movimiento en marcha, o el elegido
uint8_t boxView() { return boxMove ? boxMoveHome : boxHome; }
// a que hueco de casa va un despertar del hueco k: el primero vacio, si no uno con un huevo (que espera);
// -1 = los dos tienen un bicho (hay que preguntar con cual se cambia) o no se puede
int8_t boxWakeTarget(uint8_t k) {
  for (uint8_t s = 0; s < HOME_SLOTS; s++)
    if (pet.home(s).isVacant() && pet.canWake(k, s)) return (int8_t)s;
  for (uint8_t s = 0; s < HOME_SLOTS; s++)
    if (pet.home(s).isEgg() && pet.canWake(k, s)) return (int8_t)s;
  return -1;
}
// a la caja (hueco k vacio): el unico de casa que se puede guardar, o -1 si son los dos (o ninguno)
int8_t boxStoreSource(uint8_t k) {
  bool a = pet.canStore(k, 0), b = pet.canStore(k, 1);
  return a && !b ? 0 : (b && !a ? 1 : -1);
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
// danado; vacio, con un "+" si el activo se puede guardar. hide = su miniatura se fue con la luz.
// Panel del patio (v2.0, Y1): el ocupado es una tarjeta; el elegido (el del dialogo), la tarjeta
// clara; el vacio, solo el contorno
void drawBoxSlot(uint8_t k, uint8_t st, const PetRecord *r, bool hide, int dx) {
  int x = boxCellX(k) + 4 + dx, y = boxCellY(k) + 4, w = BOX_CW - 8, h = BOX_CH - 8;
  int cx = x + w / 2;
  bool sel = boxAsk == k + 1;
  if (st == BOX_OK && r) {
    uint16_t bg = sel ? Y_CARD_HI : Y_CARD;
    yCard(x, y, w, h, sel);
    bool away = pet.slotTravelling(k);  // de viaje (Y5b): su miniatura apagada y una maleta en vez de la "z"
    if (!hide) {
      const uint8_t *t = thumbs.get(r->speciesId);
      if (t) {
        drawThumbFit(t, cx, y + 32, w - 40, 56, away, bg);
      } else {  // sin thumbs.bin: el numero, como la Pokedex
        char num[6];
        snprintf(num, sizeof(num), "%d", r->speciesId);
        gfx->setTextColor(Y_TEXT_DIM);
        setSize(3);
        setCur(cx - textW(num, 3) / 2, y + 20);
        printT(num);
      }
    }
    if (away) {
      gfx->fillCircle(x + w - 17, y + 17, 13, Y_BACK);
      drawMap(SPR_ICON_TRAVEL, 16, x + w - 25, y + 9, 1, false);
    } else {
      gfx->setTextColor(BOX_Z_INK);  // "z": duerme
      setSize(2);
      setCur(x + w - 18, y + 6);
      printT("z");
    }
    if (r->flags & REC_SHINY) {
      gfx->setTextColor(UI_BAR_WARN);
      setCur(x + 7, y + 6);
      printT("*");
    }
    gfx->setTextColor(Y_TEXT);
    printTight(recName(*r), cx, y + 62, w - 6);
    char lv[16];
    if (away) {  // lo que falta (H:MM), o "de viaje" si no hay hora
      if (eggClockKnown(pet.lastSeenEpoch)) trvHMStr(lv, sizeof(lv), pet.travelSecondsLeft());
      else snprintf(lv, sizeof(lv), "%s", T(S_TRV_AWAY));
    } else {
      snprintf(lv, sizeof(lv), T(S_LVL_FMT), (unsigned)levelFor(r->growMin));
    }
    gfx->setTextColor(away ? Y_BACK : Y_TEXT_DIM);
    setSize(2);
    setCur(cx - textW(lv, 2) / 2, y + 80);
    printT(lv);
    return;
  }
  if (st == BOX_DAMAGED) {
    yCard(x, y, w, h, false);
    gfx->drawRoundRect(x, y, w, h, Y_CARD_RAD, Y_ACCENT_BAD);
    gfx->drawRoundRect(x + 1, y + 1, w - 2, h - 2, Y_CARD_RAD - 1, Y_ACCENT_BAD);
    gfx->setTextColor(Y_ACCENT_BAD);
    setSize(4);
    setCur(cx - textW("?", 4) / 2, y + 16);
    printT("?");
    printTight(T(S_BOX_DAMAGED), cx, y + 70, w - 4);
    return;
  }
  if (sel) yCard(x, y, w, h, true);
  gfx->drawRoundRect(x, y, w, h, Y_CARD_RAD, Y_CARD_HI);
  gfx->drawRoundRect(x + 1, y + 1, w - 2, h - 2, Y_CARD_RAD - 1, Y_CARD_HI);
  if (pet.canStore(k, 0) || pet.canStore(k, 1)) {  // aqui cabe uno de casa
    gfx->fillRoundRect(cx - 14, y + 32, 28, 6, 3, Y_TEXT_DIM);
    gfx->fillRoundRect(cx - 3, y + 21, 6, 28, 3, Y_TEXT_DIM);
  }
  gfx->setTextColor(Y_TEXT_DIM);
  printTight(T(S_BOX_EMPTY), cx, y + 70, w - 4);
}

// fila del activo: su miniatura (o el huevo) y "Ahora: <nombre>"; con un huevo esperando a su turno,
// el huevo dormido a la derecha. Deja en boxNowCx el centro de la miniatura (de ahi sale la luz).
// vacant = la casa vacia (v1.39): "Ahora: Vacio", con la silueta del huevo en gris.
// En el panel del patio la fila va en su tarjeta
void drawBoxNow(const PetRecord *r, bool hide, bool held, bool vacant) {
  char line[48];
  snprintf(line, sizeof(line), T(S_NOW_FMT), r ? recName(*r) : T(vacant ? S_BOX_EMPTY : S_EGG_HDR));
  setSize(2);
  int tw = textW(line, 2);
  int total = 48 + tw + (held ? 46 : 0);
  int x = CX - total / 2;
  boxNowCx = x + 20;
  yCard(x - 14, BOX_NOW_Y - 22, total + 28, 44, false);
  if (!hide) {
    if (r) {
      const uint8_t *t = thumbs.get(r->speciesId);
      if (t) drawThumbFit(t, boxNowCx, BOX_NOW_Y, 58, 40, false, Y_CARD);
    } else if (vacant) {
      drawMapTint(SPR_EGG, SPRITE_H, boxNowCx - 16, BOX_NOW_Y - 16, 1, Y_TEXT_DIM);
    } else {
      drawEggAt(boxNowCx, BOX_NOW_Y, 1, 0, pet.home(boxView()).eggSpecies());
    }
  }
  gfx->setTextColor(Y_TEXT);
  setCur(x + 48, BOX_NOW_Y - 7);
  printT(line);
  if (held) {  // el huevo que espera (FR-BOX.5): no se ha perdido
    int ex = x + 48 + tw + 10;
    drawMapTint(SPR_EGG, SPRITE_H, ex, BOX_NOW_Y - 16, 1, Y_TEXT_DIM);
    gfx->setTextColor(BOX_Z_INK);
    setSize(2);
    setCur(ex + 26, BOX_NOW_Y - 20);
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
  if (boxAskPick) {  // v3.0: cual de los dos de casa (guardar uno, o cambiarlo por el de la caja)
    char q[64];
    if (wake) snprintf(q, sizeof(q), T(S_WAKE_Q_FMT), recName(pet.boxRecord(k)));
    gfx->fillRoundRect(77, 166, 320, 160, 16, Y_SHADOW);
    gfx->fillRoundRect(73, 160, 320, 160, 16, Y_CARD);
    gfx->drawRoundRect(73, 160, 320, 160, 16, BOX_DLG_LINE);
    gfx->setTextColor(Y_TEXT);
    if (wake) {
      printTight(q, CX, 182, 296);
      gfx->setTextColor(Y_TEXT_DIM);
      printTight(T(S_BOX_WHICH_SWAP), CX, 214, 296);
    } else {
      printTight(T(S_BOX_WHICH_STORE), CX, 198, 296);
    }
    for (uint8_t s = 0; s < HOME_SLOTS; s++) {
      int bx = s ? 238 : 88;
      bool can = wake ? pet.canWake(k, s) : pet.canStore(k, s);
      gfx->fillRoundRect(bx, 252, 140, 52, 12, can ? Y_ACCENT_GO : Y_CARD_HI);
      gfx->setTextColor(Y_TEXT);
      printTight(homeName(s), bx + 70, 270, 128);
    }
    return;
  }
  const char *nm = homeName(boxHome);
  char q[64];
  char note[64];
  note[0] = 0;
  if (wake) {
    snprintf(q, sizeof(q), T(S_WAKE_Q_FMT), recName(pet.boxRecord(k)));
    if (!pet.home(boxHome).isEgg()) snprintf(note, sizeof(note), T(S_SWAP_NOTE_FMT), nm);
  } else {
    snprintf(q, sizeof(q), T(S_STORE_Q_FMT), nm);
    // v1.39: sin huevo que esperaba ni en la mochila, la casa se queda vacia
    // (v1.55: un huevo envuelto tambien: freeHome lo desenvuelve y llega a casa)
    if (!pet.home(boxHome).eggHeld() && !pet.items[IT_EGG] && !pet.giftEggs()) snprintf(note, sizeof(note), "%s", T(S_NO_EGG_NOTE));
  }
  // panel del patio: tarjeta oscura con borde, sombra debajo; SI verde y NO rojo en el mismo sitio
  gfx->fillRoundRect(77, 166, 320, 160, 16, Y_SHADOW);
  gfx->fillRoundRect(73, 160, 320, 160, 16, Y_CARD);
  gfx->drawRoundRect(73, 160, 320, 160, 16, BOX_DLG_LINE);
  gfx->setTextColor(Y_TEXT);
  printTight(q, CX, note[0] ? 182 : 198, 296);
  if (note[0]) {
    gfx->setTextColor(Y_TEXT_DIM);
    printTight(note, CX, 214, 296);
  }
  setSize(2);
  gfx->fillRoundRect(118, 252, 100, 52, 12, Y_ACCENT_GO);
  gfx->setTextColor(Y_TEXT);
  setCur(118 + (100 - textW(T(S_YES), 2)) / 2, 270);
  printT(T(S_YES));
  gfx->fillRoundRect(248, 252, 100, 52, 12, Y_ACCENT_BAD);
  setCur(248 + (100 - textW(T(S_NO), 2)) / 2, 270);
  printT(T(S_NO));
}

void renderBox() {
  uint32_t now = millis();
  yPanelBg();  // panel del patio (v2.0, Y1): siempre oscuro

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
  yTitle(head, nullptr);
  const PetLife &hv = pet.home(boxView());  // v3.0: el hueco de casa que se ve (o el del movimiento)
  bool hatched = !hv.isEgg();
  PetRecord act;
  memset(&act, 0, sizeof(act));
  if (hatched) act = hv.record(pet.lastSeenEpoch);
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
  bool held = hv.eggHeld();
  bool vacant = hv.isVacant();
  if (outcome) {
    nowRec = boxMove == BOXMV_STORE ? nullptr : &pet.boxRecord(mk);
    // despertar: con un huevo, el huevo espera; con la casa vacia, nada. Guardar: el huevo que
    // esperaba o uno de la mochila; si no hay, la casa queda vacia (v1.39)
    if (boxMove == BOXMV_WAKE) held = !vacant;
    else if (boxMove == BOXMV_STORE) held = false;
    if (boxMove == BOXMV_STORE) vacant = !hv.eggHeld() && !pet.items[IT_EGG] && !pet.giftEggs();
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

  if (boxAwayUntil && pet.slotTravelling(boxAwaySlot) && !boxAsk) {  // "<Nombre> esta de viaje"
    char aw[40];
    snprintf(aw, sizeof(aw), T(S_TRV_AWAY_FMT), recName(pet.boxRecord(boxAwaySlot)));
    setSize(2);
    int aw_w = textW(aw, 2) + 32;
    if (aw_w > 380) aw_w = 380;
    gfx->fillRoundRect(CX - 200, BOX_NOW_Y - 26, 400, 52, Y_CARD_RAD, Y_PANEL_BG);  // tapa la fila "Ahora"
    gfx->fillRoundRect(CX - aw_w / 2 + 2, BOX_NOW_Y - 20 + 2, aw_w, 40, Y_CARD_RAD, Y_SHADOW);
    gfx->fillRoundRect(CX - aw_w / 2, BOX_NOW_Y - 20, aw_w, 40, Y_CARD_RAD, Y_BACK);
    gfx->setTextColor(Y_TEXT);
    printTight(aw, CX, BOX_NOW_Y - 7, aw_w - 16);
  }
  yBackBtn();  // v3.4: sin "1/2" ni flechas (las insignias, en la pagina del jugador)
  if (boxAsk) drawBoxAsk();
  gfx->flush();
}

// un hueco que no se puede usar tiembla, con el sonido de "no" (como un boton, FR-FX.11)
void boxDeny(int k) {
  sfxPlay(SFX_DENY);
  boxShakeSlot = (int8_t)k;
  boxShakeUntil = millis() + FX_DENY_MS;
}

// v3.0: con el hueco de casa s
void boxStartMove(uint8_t k, uint8_t s) {
  bool toStore = pet.boxState(k) == BOX_EMPTY;
  if (toStore ? !pet.canStore(k, s) : !pet.canWake(k, s)) {
    boxDeny(k);
    return;
  }
  boxMoveHome = s;
  boxMove = toStore ? BOXMV_STORE : (pet.home(s).isEgg() ? BOXMV_WAKE : BOXMV_SWAP);
  boxMoveSlot = k;
  boxMoveStart = millis();
  boxMoveDrawnEnd = false;
  sfxPlay(toStore ? SFX_STORE : SFX_WAKE);
}

// los botones del dialogo "cual de los dos" (v3.0): el de la izquierda es el hueco 0
static int boxPickHit(int16_t x, int16_t y) {
  if (y < 252 || y > 304) return -1;
  if (x >= 88 && x <= 228) return 0;
  if (x >= 238 && x <= 378) return 1;
  return -1;
}

void boxTap(int16_t x, int16_t y) {
  if (boxMove) return;  // la luz vuela: un momento
  if (boxAsk) {         // dialogo: SI / NO (o fuera = NO); v3.0: o cual de los dos
    uint8_t k = boxAsk - 1;
    bool pick = boxAskPick;
    boxAsk = 0;
    boxAskPick = false;
    if (!timeLeft(boxAskUntil)) return;
    if (pick) {
      int s = boxPickHit(x, y);
      if (s >= 0) boxStartMove(k, (uint8_t)s);
    } else if (x >= 118 && x <= 218 && y >= 252 && y <= 304) {
      boxStartMove(k, boxHome);
    }
    return;
  }
  if (y < 72 || yBackHit(x, y)) {  // la cabecera o el boton de volver = salir, como en la Pokedex
    closeBox();
    return;
  }
  int k = boxSlotAt(x, y);
  if (k < 0) return;
  if (pet.slotTravelling((uint8_t)k)) {  // de viaje (Y5b): un aviso, no el dialogo de despertar
    boxAwaySlot = (uint8_t)k;
    boxAwayUntil = millis() + 2200;
    sfxPlay(SFX_TAP);
    return;
  }
  // v3.0: guardar uno de los de casa (si son dos, cual) o despertar al hueco vacio / con huevo (si los dos
  // tienen un bicho, con cual se cambia)
  bool store = pet.boxState(k) == BOX_EMPTY;
  bool ok = store ? (pet.canStore(k, 0) || pet.canStore(k, 1)) : (pet.canWake(k, 0) || pet.canWake(k, 1));
  if (!ok) {  // vacio sin nadie que guardar, o un hueco danado
    boxDeny(k);
    return;
  }
  int8_t s = store ? boxStoreSource((uint8_t)k) : boxWakeTarget((uint8_t)k);
  if (!store && s < 0 && !(pet.canWake(k, 0) && pet.canWake(k, 1))) s = pet.canWake(k, 0) ? 0 : 1;
  boxAskPick = s < 0;
  if (s >= 0) boxHome = (uint8_t)s;
  boxAsk = (uint8_t)(k + 1);
  boxAskUntil = millis() + 10000;
  sfxPlay(SFX_TAP);
}

// Despues de cualquier movimiento (tambien STORE / WAKE por serie): sprites del activo nuevo, su
// paseo desde el centro, y los efectos del otro bicho fuera (fx.h: la subida pendiente, "+N",
// barras). Los vigilantes miran al nuevo sin evento: si no, verian "subir" su nivel o "nacer" al
// que sustituye a un huevo. De vuelta a casa, el activo sale de una luz.
void boxAfterMove() {
  boxOpen = false;
  boxAsk = 0;
  boxMove = BOXMV_NONE;
  monReset();  // recarga aunque sea la misma especie: ensureMon tambien vuelve a centrar el paseo
  ensureMon();
  fxq.drop(FXB_LEVELUP);  // (las medallas, v3.4, son del jugador: se quedan)
  fxo.clear();
  fxArmWatches();
  if (!pet.home(boxMoveHome).isVacant()) fxo.push(FXO_BOX, 0, 0, millis(), boxMoveHome);  // hueco vacio (v1.39): nadie sale de la luz
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
  bool ok = (mv == BOXMV_STORE) ? pet.store(k, boxMoveHome) : pet.wake(k, boxMoveHome);  // ~1 s con la pantalla quieta
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
  uint8_t s = o->slot & 1;  // v3.0: el hueco de casa del movimiento
  const PetLife &l = pet.home(s);
  int cx = l.isEgg() ? CX : petFootXOf(s);
  int cy = (l.isEgg() || (!homeMon[s].loaded && petFootYOf(s) == PET_GROUND)) ? PET_CY : petFootYOf(s) - petBodyHOf(s) / 2;
  if (l.isEgg() && yardHomeNow() && yardEggX[s] >= 0) {  // el huevo del patio (Y3): donde esta
    cx = yardEggX[s];
    cy = yardEggY[s] - YARD_EGG_MID;
  }
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
  Serial.printf("eggs bag=%u held=%d/%d owned=%u next=%lu/%u vacant=%d home=%s/%s boxPets=%d\n", pet.items[IT_EGG],
                pet.home(0).eggHeld() ? 1 : 0, pet.home(1).eggHeld() ? 1 : 0, pet.eggsOwned(),
                (unsigned long)pet.eggStepsLeft(), pet.eggStepsAt(), pet.homeVacant(),
                pet.home(0).isVacant() ? "empty" : (pet.home(0).isEgg() ? "egg" : "pet"),
                pet.home(1).isVacant() ? "empty" : (pet.home(1).isEgg() ? "egg" : "pet"), pet.boxHasPets() ? 1 : 0);
}

// comando BOX: la caja por serie (uid, especie, nivel, CRC) y el huevo de la incubadora
// TRAVEL (Y5): el viaje de ahora, lo que falta y la coleccion
// v2.2: el estado del encuentro (serie ENC?)
void encReport() {
  uint32_t d = pet.dayNumber();
  uint16_t bm = encBaseMinute(d);
  uint8_t m = pet.encRolledMask();
  Serial.printf("enc: day=%lu base=%02u:%02u rolled base=%d walk=%d spawned=%d steps=%lu/%u\n", (unsigned long)d,
                bm / 60, bm % 60, (m & ENC_ROLL_BASE) ? 1 : 0, (m & ENC_ROLL_WALK) ? 1 : 0, (m & ENC_ROLL_SPAWNED) ? 1 : 0,
                (unsigned long)pet.stepsOn(d), (unsigned)ENC_WALK_STEPS);
  Serial.printf("enc: active=%d won=%d dex=#%d %s nv.%u shiny=%d excl=%d left=%u min obj=%d at %d,%d ui=%u\n",
                pet.encActive() ? 1 : 0, pet.encWonPending() ? 1 : 0, pet.encDex(),
                pet.encShown() ? dexName(pet.encDex()) : "-", pet.encLevel(), pet.encShiny() ? 1 : 0,
                pet.encExclusive() ? 1 : 0, pet.encMinutesLeft(), encMapObject(yardMapNow()), yardEncX, yardEncY, encUi);
  Serial.printf("enc: balls=%u capEggs=%u", pet.items[IT_BALL], pet.captureEggs());
  for (uint8_t i = 0; i < pet.captureEggs(); i++) Serial.printf(" #%d", pet.captureEggSpecies(i));
  Serial.printf(" homeEggCaptured=%d\n", pet.eggIsCaptured() ? 1 : 0);
}

void travelReport() {
  if (pet.travelling()) {
    Serial.printf("travel away slot=%u map=%u tier=%u start=%lu seed=%08lx left=%lus elapsed=%um\n", pet.travelSlot(),
                  pet.travelDest(), pet.travelTier(), (unsigned long)pet.travelStartEpoch(), (unsigned long)pet.travelSeed(),
                  (unsigned long)pet.travelSecondsLeft(), pet.travelMinutesElapsed());
    for (uint8_t i = 0; i < travelEventCount(pet.travelTier()); i++) {
      uint16_t off;
      uint8_t tpl, arg;
      travelEvent(pet.travelSeed(), pet.travelDest(), pet.travelTier(), i, &off, &tpl, &arg);
      Serial.printf("  event %u at %02u:%02u tpl=%u arg=%u %s\n", i, off / 60, off % 60, tpl, arg,
                    off <= pet.travelMinutesElapsed() ? "(seen)" : "");
    }
  } else {
    Serial.println("travel none");
  }
  TravelResult r;
  if (pet.peekTravelResult(&r))
    Serial.printf("  result: %s map=%u tier=%u coins=%u(dup %u) grow=+%u lv %u->%u item=%d%s souv=%d%s\n", r.nick, r.dest,
                  r.tier, r.coins, r.dupCoins, r.growMin, r.lvBefore, r.lvAfter, r.item == TRV_NONE ? -1 : r.item,
                  r.itemLost ? " LOST" : "", r.souvenir == TRV_NONE ? -1 : r.souvenir, r.dup ? " DUP" : "");
  Serial.printf("  souvenirs %u/%u mask=0x%X\n", pet.souvenirCount(), SOUV_COUNT, (unsigned)pet.souvenirMask());
}

void boxReport() {
  for (uint8_t s = 0; s < HOME_SLOTS; s++) {  // v3.0: los dos de casa ("active" = el del foco)
    const PetLife &l = pet.home(s);
    Serial.printf("%s home%u uid=%08lX spec=%d lv=%u egg=%d vacant=%d eggHeld=%d eggT=%d eggShiny=%d box=%u/%u bagEggs=%u\n",
                  s == pet.focusSlot ? "active" : "other", s, (unsigned long)l.uid, l.speciesId, l.isEgg() ? 0 : l.level(),
                  (l.isEgg() && !l.isVacant()) ? 1 : 0, l.isVacant() ? 1 : 0, l.eggHeld() ? 1 : 0, l.eggSpecies(),
                  l.eggIsShiny() ? 1 : 0, pet.boxUsed(), BOX_SLOTS, pet.items[IT_EGG]);
  }
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
    r.hygiene = 100;
    r.geneAtk = (uint8_t)(90 + random(21));
    r.geneDef = (uint8_t)(90 + random(21));
    r.geneSpe = (uint8_t)(90 + random(21));
    r.trAtk = (uint8_t)random(30);
    r.trDef = (uint8_t)random(30);
    r.trSpe = (uint8_t)random(30);
    r.bond = (uint8_t)random(100);
    // v3.4: un bicho inventado ya "conto" todo a lo que llega: al despertarlo no sube las medallas del jugador
    r.flags |= REC_V3;
    r.medals = medalPetBits(levelFor(r.growMin), r.bond, DEX_TBL[r.speciesId].evolvesTo == 0, r.weight,
                            r.careMistakes, false);
    r.storedEpoch = pet.lastSeenEpoch;
    pet.boxPut(k, r);
  }
}

// ---------- fases: seleccion, selector de equipo e insignias (v1.65) ----------
// Menus estaticos, como la caja (no repintan sin cambio de clave: mixStageKey() mezcla todo lo que
// se dibuja). El combate en si es de startStage() (la pantalla de combate). La pagina de insignias
// es la segunda de la caja (deslizar a la derecha desde sus huecos).

#define STG_X 73        // rejilla de 2x4 casillas de 156x62 (las esquinas de arriba quedan ~3 px dentro del circulo)
#define STG_Y 78
#define STG_W 156
#define STG_H 62
#define STG_GAPX 8
#define STG_GAPY 6
#define STG_TITLE_BAND 72   // tocar por encima = salir
#define STG_LEGEND_Y 366
#define STG_LOCK_INK 0x8410          // gris del texto y la silueta de una fase bloqueada
#define STG_SIL 0xAD55               // silueta de la insignia aun no ganada
#define STG_OFF_EDGE Y_CARD          // panel (v2.0): borde de una casilla bloqueada o de quien no pelea
#define TEAM_CW 84      // tarjetas de bicho: 4 en la fila de arriba y 3 debajo
#define TEAM_CH 100
#define TEAM_GAP 6
#define TEAM_ROW1_Y 96
#define TEAM_SINGLE_Y 140   // hasta 4 bichos, una sola fila
#define TEAM_BAND 90        // tocar por encima de las tarjetas = volver
#define TEAM_BTN_X 153     // pastilla verde de Empezar (se toca con 48 px de alto, 2 por encima y debajo)
#define TEAM_BTN_Y 322
#define TEAM_BTN_W 160
#define TEAM_BTN_H 44
#define TEAM_BTN_HIT_Y (TEAM_BTN_Y - (Y_HIT_MIN - TEAM_BTN_H) / 2)
#define BADGE_CELL_W 96     // pagina de insignias: 4x2 de 64 px (escala 4)
#define BADGE_ROW_Y 118
#define BADGE_ROW_H 128
#define BADGE_PLATE C565(0xe8, 0xea, 0xf2)  // placa de una insignia ganada (panel del patio, Y1)

// un bicho que puede ir al equipo: uno de casa (slot TEAM_HOME0 + su hueco, v3.0) o uno de la caja
struct TeamCand {
  uint32_t uid;
  int8_t slot;
};
static TeamCand teamCand[HOME_SLOTS + BOX_SLOTS];
static uint8_t teamCandN = 0;

static void teamBuildCands() {
  teamCandN = 0;
  for (uint8_t s = 0; s < HOME_SLOTS; s++)  // v3.0: los dos de casa
    if (pet.home(s).hatched()) teamCand[teamCandN++] = { pet.home(s).uid, (int8_t)(TEAM_HOME0 + s) };
  for (uint8_t k = 0; k < BOX_SLOTS; k++)
    if (pet.boxState(k) == BOX_OK && !pet.slotTravelling(k)) teamCand[teamCandN++] = { pet.boxRecord(k).uid, (int8_t)k };
}

static bool teamIsPicked(uint32_t uid) {
  for (uint8_t i = 0; i < teamPickN; i++)
    if (teamPick[i] == uid) return true;
  return false;
}

// quita de la eleccion a quien ya no existe o no puede pelear (p. ej. el de casa se durmio)
static void teamPrune() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < teamPickN; i++)
    if (pet.teamCanFight(teamPick[i])) teamPick[n++] = teamPick[i];
  teamPickN = n;
}

// hay alguien que pueda pelear: uno de casa despierto o cualquiera de la caja
bool stagesCanOpen() { return pet.home(0).canBattle() || pet.home(1).canBattle() || pet.boxHasPets(); }

void closeStages() {
  stagesOpen = false;
  teamOpen = false;
  teamWild = false;
  stShakeUntil = 0;
  stShakeIdx = -1;
}

void openStages() {
  stagesOpen = true;
  teamOpen = false;
  stShakeUntil = 0;
  stShakeIdx = -1;
}

// el selector: precargado con el ultimo equipo (sin los que ya no existen o no pelean) y, si no queda
// nadie, con el de casa si puede
void openTeam(uint8_t stage) {
  stageSel = stage;
  teamPickN = 0;
  const uint32_t *last = pet.teamUids();
  for (uint8_t i = 0; i < TEAM_MAX; i++)
    if (last[i] && pet.teamCanFight(last[i]) && !teamIsPicked(last[i])) teamPick[teamPickN++] = last[i];
  if (!teamPickN && pet.canBattle() && !pet.isEgg()) teamPick[teamPickN++] = pet.uid;
  teamOpen = true;
  stShakeUntil = 0;
  stShakeIdx = -1;
}

void closeTeam() {
  teamOpen = false;
  teamWild = false;
  stShakeUntil = 0;
  stShakeIdx = -1;
}

// v2.2: el selector en modo encuentro: un solo luchador, precargado con el de casa si puede pelear (si no, el
// primero del ultimo equipo que pueda, o nadie)
void openTeamWild() {
  teamWild = true;
  teamPickN = 0;
  if (pet.canBattle() && !pet.isEgg()) {
    teamPick[teamPickN++] = pet.uid;
  } else if (pet.other.canBattle()) {  // v3.0: el otro de casa
    teamPick[teamPickN++] = pet.other.uid;
  } else {
    const uint32_t *last = pet.teamUids();
    for (uint8_t i = 0; i < TEAM_MAX && !teamPickN; i++)
      if (last[i] && pet.teamCanFight(last[i])) teamPick[teamPickN++] = last[i];
  }
  teamOpen = true;
  stShakeUntil = 0;
  stShakeIdx = -1;
}

// la clave de repintado de los dos menus: todo lo que dibujan
void mixStageKey(uint32_t &h) {
  h = mixKey(h, pet.badgeMask());
  h = mixKey(h, pet.stageRewardMask());
  h = mixKey(h, gLang);
  h = mixKey(h, stageSel);
  h = mixKey(h, teamWild);
  h = mixKey(h, teamPickN);
  for (uint8_t i = 0; i < teamPickN; i++) h = mixKey(h, teamPick[i]);
  h = mixKey(h, pet.home(0).canBattle());
  h = mixKey(h, pet.home(1).canBattle());
  if (teamOpen) {
    teamBuildCands();
    for (uint8_t i = 0; i < teamCandN; i++) {
      h = mixKey(h, teamCand[i].uid);
      h = mixKey(h, pet.teamCanFight(teamCand[i].uid));
      if (teamIsHome(teamCand[i].slot)) {
        const PetLife &l = pet.home(teamHomeSlot(teamCand[i].slot));
        h = mixKey(h, l.speciesId);
        h = mixKey(h, l.growMin);
        for (const char *c = l.nick; *c; c++) h = mixKey(h, (uint8_t)*c);
      } else {
        h = mixKey(h, pet.boxRecord(teamCand[i].slot).crc);
      }
    }
  }
}

// puntos de pagina, como los de la ficha (cur = pagina actual)
void drawPageDots(int count, int cur, int y) {
  for (int i = 0; i < count; i++) {
    int dx = CX - (count - 1) * 13 + i * 26;
    if (i == cur) gfx->fillCircle(dx, y, 5, UI_INK);
    else gfx->drawCircle(dx, y, 4, UI_INK);
  }
}

static int stShakeDx(int8_t idx) {
  if (idx != stShakeIdx || !timeLeft(stShakeUntil)) return 0;
  float s = 1.0f - (float)timeLeft(stShakeUntil) / (float)FX_DENY_MS;
  return (int)(4.0f * (1.0f - s) * sinf(s * 6.0f * (float)PI));
}

static void stDeny(int8_t idx) {
  sfxPlay(SFX_DENY);
  stShakeIdx = idx;
  stShakeUntil = millis() + FX_DENY_MS;
}

static int stageTileX(uint8_t i) { return STG_X + (i % 2) * (STG_W + STG_GAPX); }
static int stageTileY(uint8_t i) { return STG_Y + (i / 2) * (STG_H + STG_GAPY); }

// marca de "premio de hoy": un circulo ambar con un punto
static void drawRewardMark(int cx, int cy) {
  gfx->fillCircle(cx, cy, 7, UI_BAR_WARN);
  gfx->fillCircle(cx, cy, 2, UI_WHITE);
}

void renderStages() {
  yPanelBg();
  yTitle(T(S_STAGES), nullptr);
  for (uint8_t i = 0; i < STAGE_COUNT; i++) {
    bool open = pet.stageUnlocked(i), got = pet.hasBadge(i);
    int x = stageTileX(i) + stShakeDx(i), y = stageTileY(i);
    if (!open) {  // bloqueada: solo el contorno, silueta y texto apagados
      gfx->drawRoundRect(x, y, STG_W, STG_H, Y_CARD_RAD, STG_OFF_EDGE);
      gfx->drawRoundRect(x + 1, y + 1, STG_W - 2, STG_H - 2, Y_CARD_RAD - 1, STG_OFF_EDGE);
    } else {
      yCard(x, y, STG_W, STG_H, false);
      if (got) {  // ganada: borde verde
        gfx->drawRoundRect(x, y, STG_W, STG_H, Y_CARD_RAD, Y_ACCENT_GO);
        gfx->drawRoundRect(x + 1, y + 1, STG_W - 2, STG_H - 2, Y_CARD_RAD - 1, Y_ACCENT_GO);
      }
    }
    if (got) drawMap(BADGE_ICON[i], 16, x + 8, y + (STG_H - 32) / 2, 2, false);
    else drawMapTint(BADGE_ICON[i], 16, x + 8, y + (STG_H - 32) / 2, 2, open ? STG_SIL : STG_LOCK_INK);
    int tcx = x + 46 + 49;
    gfx->setTextColor(open ? Y_TEXT : STG_LOCK_INK);
    printTight(stageName(i), tcx, y + 10, 98);
    char line[24];
    if (open) snprintf(line, sizeof(line), T(S_LV_RANGE_FMT), (unsigned)STAGES[i].lvMin, (unsigned)STAGES[i].lvMax);
    else snprintf(line, sizeof(line), "%s", T(S_LOCKED));
    gfx->setTextColor(open ? Y_TEXT_DIM : STG_LOCK_INK);
    printTight(line, tcx, y + 34, 98);
    if (open && pet.stageRewardReady(i)) drawRewardMark(x + STG_W - 12, y + 12);
  }
  // leyenda
  setSize(2);
  int tw = textW(T(S_REWARD_TODAY), 2), lx = CX - (tw + 22) / 2;
  drawRewardMark(lx + 7, STG_LEGEND_Y + 7);
  gfx->setTextColor(Y_TEXT_DIM);
  setCur(lx + 22, STG_LEGEND_Y);
  printT(T(S_REWARD_TODAY));
  yBackBtn();
  gfx->flush();
}

void stagesTap(int16_t x, int16_t y) {
  if (y < STG_TITLE_BAND || yBackHit(x, y)) {  // tocar la cabecera = salir, como la caja
    closeStages();
    return;
  }
  for (uint8_t i = 0; i < STAGE_COUNT; i++) {
    int tx = stageTileX(i), ty = stageTileY(i);
    if (x < tx || x >= tx + STG_W || y < ty || y >= ty + STG_H) continue;
    if (!pet.stageUnlocked(i)) {
      stDeny(i);
      return;
    }
    sfxPlay(SFX_TAP);
    openTeam(i);
    return;
  }
}

// posicion de la tarjeta i de n
static void teamCardPos(uint8_t i, uint8_t n, int &x, int &y) {
  uint8_t rows = n > 4 ? 2 : 1, row = i / 4;
  uint8_t cnt = row == 0 ? (n < 4 ? n : 4) : (uint8_t)(n - 4);
  x = CX - (cnt * TEAM_CW + (cnt - 1) * TEAM_GAP) / 2 + (i % 4) * (TEAM_CW + TEAM_GAP);
  y = rows == 2 ? TEAM_ROW1_Y + row * (TEAM_CH + 8) : TEAM_SINGLE_Y;
}

static void drawTeamCard(uint8_t idx, uint8_t n) {
  const TeamCand &c = teamCand[idx];
  int x, y;
  teamCardPos(idx, n, x, y);
  x += stShakeDx(idx);
  bool fight = pet.teamCanFight(c.uid), picked = teamIsPicked(c.uid);
  // elegido: tarjeta clara con borde verde; no pelea (dormido): solo el contorno, sobre el fondo
  uint16_t bg = !fight ? Y_CARD : picked ? Y_CARD_HI : Y_CARD;  // dormido: la miniatura se apaga hacia Y_CARD
  if (fight) yCard(x, y, TEAM_CW, TEAM_CH, picked);
  uint16_t edge = !fight ? Y_CARD_HI : picked ? Y_ACCENT_GO : 0;
  if (edge) {
    gfx->drawRoundRect(x, y, TEAM_CW, TEAM_CH, Y_CARD_RAD, edge);
    gfx->drawRoundRect(x + 1, y + 1, TEAM_CW - 2, TEAM_CH - 2, Y_CARD_RAD - 1, edge);
  }
  bool atHome = teamIsHome(c.slot);  // v3.0: uno de los dos de casa
  const PetLife &hl = pet.home(atHome ? teamHomeSlot(c.slot) : 0);
  int16_t dex = atHome ? hl.speciesId : pet.boxRecord(c.slot).speciesId;
  const char *nm = atHome ? (hl.nick[0] ? hl.nick : dexName(hl.speciesId)) : recName(pet.boxRecord(c.slot));
  uint16_t lv = atHome ? hl.level() : levelFor(pet.boxRecord(c.slot).growMin);
  int cx = x + TEAM_CW / 2;
  const uint8_t *t = thumbs.get(dex);
  if (t) drawThumbFit(t, cx, y + 32, TEAM_CW - 14, 48, !fight, bg);
  gfx->setTextColor(fight ? Y_TEXT : STG_LOCK_INK);
  printTight(nm, cx, y + 62, TEAM_CW - 4);
  char s[16];
  snprintf(s, sizeof(s), T(S_LVL_FMT), (unsigned)lv);
  gfx->setTextColor(fight ? Y_TEXT_DIM : STG_LOCK_INK);
  setSize(2);
  setCur(cx - textW(s, 2) / 2, y + 80);
  printT(s);
  if (picked && fight) {  // el numero de orden, encima de la miniatura
    int pos = 0;
    for (uint8_t i = 0; i < teamPickN; i++)
      if (teamPick[i] == c.uid) pos = i + 1;
    gfx->fillCircle(x + 14, y + 14, 12, Y_ACCENT_GO);
    gfx->setTextColor(Y_TEXT);
    setSize(2);
    char d[4];
    snprintf(d, sizeof(d), "%d", pos);
    setCur(x + 14 - textW(d, 2) / 2, y + 7);
    printT(d);
  }
}

void renderTeam() {
  teamPrune();
  teamBuildCands();
  yPanelBg();
  // el titulo va a tamano 2: "Elige tu equipo" a 3 no cabe arriba del circulo
  gfx->setTextColor(Y_TEXT);
  printTight(T(teamWild ? S_WILD_PICK : S_PICK_TEAM), CX, Y_TITLE_Y + 4, 220);
  char sub[40];
  char stg[32];
  if (teamWild) {  // v2.2: contra quien ("PIDGEY Nv.23")
    snprintf(stg, sizeof(stg), T(S_LVL_FMT), (unsigned)pet.encLevel());
    snprintf(sub, sizeof(sub), "%s %s", dexName(pet.encDex()), stg);
  } else {
    snprintf(stg, sizeof(stg), T(S_STAGE_FMT), (unsigned)stageSel + 1);
    snprintf(sub, sizeof(sub), "%s: %s", stg, stageName(stageSel));
  }
  gfx->setTextColor(Y_TEXT_DIM);
  printTight(sub, CX, Y_SUB_Y - 4, 290);
  for (uint8_t i = 0; i < teamCandN; i++) drawTeamCard(i, teamCandN);
  bool ok = teamPickN > 0;
  gfx->fillRoundRect(TEAM_BTN_X, TEAM_BTN_Y, TEAM_BTN_W, TEAM_BTN_H, TEAM_BTN_H / 2, ok ? Y_ACCENT_GO : Y_CARD);
  gfx->setTextColor(ok ? Y_TEXT : Y_TEXT_DIM);
  setSize(3);
  setCur(TEAM_BTN_X + (TEAM_BTN_W - textW(T(S_START), 3)) / 2, TEAM_BTN_Y + (TEAM_BTN_H - 21) / 2);
  printT(T(S_START));
  yBackBtn();
  gfx->flush();
}

void teamTap(int16_t x, int16_t y) {
  teamPrune();
  teamBuildCands();
  if (yInRect(x, y, TEAM_BTN_X, TEAM_BTN_HIT_Y, TEAM_BTN_W, Y_HIT_MIN)) {
    if (!teamPickN) {
      stDeny(-1);
      return;
    }
    uint32_t uids[TEAM_MAX];
    uint8_t n = teamPickN, st = stageSel;
    bool wild = teamWild;
    memcpy(uids, teamPick, sizeof(uids));
    sfxPlay(SFX_TAP);
    closeStages();
    if (wild) {
      if (pet.encActive()) startWild(uids[0]);  // v2.2 (se pudo ir mientras se elegia: 6 h)
      return;
    }
    startStage(st, uids, n);
    return;
  }
  if (y < TEAM_BAND || yBackHit(x, y)) {  // la cabecera o el boton de volver: a las fases
    closeTeam();
    return;
  }
  for (uint8_t i = 0; i < teamCandN; i++) {
    int cx, cy;
    teamCardPos(i, teamCandN, cx, cy);
    if (x < cx || x >= cx + TEAM_CW || y < cy || y >= cy + TEAM_CH) continue;
    uint32_t uid = teamCand[i].uid;
    if (teamIsPicked(uid)) {  // quitarlo: los de detras suben un puesto
      uint8_t n = 0;
      for (uint8_t j = 0; j < teamPickN; j++)
        if (teamPick[j] != uid) teamPick[n++] = teamPick[j];
      teamPickN = n;
      sfxPlay(SFX_TAP);
    } else if (!pet.teamCanFight(uid) || (!teamWild && teamPickN >= TEAM_MAX)) {
      stDeny(i);
    } else {
      if (teamWild) teamPickN = 0;  // v2.2: uno solo; tocar otro lo cambia
      teamPick[teamPickN++] = uid;
      sfxPlay(SFX_TAP);
    }
    return;
  }
}

// las 8 insignias, en color las ganadas y en silueta las que faltan (v3.4: 3.a pagina del jugador; era la
// 2.a de la caja)
void renderUserBadges() {
  char head[32];
  snprintf(head, sizeof(head), "%s %u/%u", T(S_BADGES), (unsigned)pet.badgeCount(), (unsigned)STAGE_COUNT);
  yTitleFit(head);  // el disco oscuro ya lo pinta renderUser
  int x0 = CX - (4 * BADGE_CELL_W) / 2;
  for (uint8_t i = 0; i < STAGE_COUNT; i++) {
    int cx = x0 + (i % 4) * BADGE_CELL_W + BADGE_CELL_W / 2, y = BADGE_ROW_Y + (i / 4) * BADGE_ROW_H;
    bool got = pet.hasBadge(i);
    // panel del patio (v2.0, Y1): cada insignia en su tarjeta; la ganada, en color sobre una
    // placa clara (su contorno oscuro no se leeria sobre la tarjeta); la que falta, en silueta
    yCard(cx - 40, y - 8, 80, 80, got);
    if (got) {
      gfx->fillRoundRect(cx - 35, y - 3, 70, 70, 10, BADGE_PLATE);
      drawMap(BADGE_ICON[i], 16, cx - 32, y, 4, false);
    } else {
      drawMapTint(BADGE_ICON[i], 16, cx - 32, y, 4, BOX_DLG_LINE);
    }
    gfx->setTextColor(got ? Y_TEXT : Y_TEXT_DIM);
    printTight(got ? stageName(i) : "???", cx, y + 80, BADGE_CELL_W - 8);  // 8 px entre nombres vecinos
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

// =================== kit del patio (v2.0, fase Y0; geometria en yard_ui.h) ===================
// Piezas de la interfaz nueva. Los paneles son un disco casi negro con tarjetas; el mundo
// lleva el texto con sombra para leerse sobre el mapa. Pantalla de muestra: serie YARDUI.

void yPanelBg() {
  gfx->fillScreen(0x0000);  // fuera del disco: negro (el marco redondo lo tapa igualmente)
  gfx->fillCircle(Y_CX, Y_CY, Y_PANEL_R, Y_PANEL_BG);
}

// texto con sombra de 2 px, para encima del mapa
void yShadowText(int x, int y, const char *s, uint8_t size, uint16_t col) {
  setSize(size);
  gfx->setTextColor(Y_SHADOW);
  setCur(x + 2, y + 2);
  printT(s);
  gfx->setTextColor(col);
  setCur(x, y);
  printT(s);
}

void yTitle(const char *t, const char *sub) {
  gfx->setTextColor(Y_TEXT);
  setSize(3);
  setCur(centerX(t, 3), Y_TITLE_Y);
  printT(t);
  if (sub && *sub) {
    gfx->setTextColor(Y_TEXT_DIM);
    setSize(2);
    setCur(centerX(sub, 2), Y_SUB_Y);
    printT(sub);
  }
}

void yCard(int x, int y, int w, int h, bool hi) {
  gfx->fillRoundRect(x, y, w, h, Y_CARD_RAD, hi ? Y_CARD_HI : Y_CARD);
}

int yPillWidth(const char *label) {
  setSize(2);
  return yPillW(textW(label, 2));
}

// pastilla redondeada con el texto en blanco; devuelve su ancho
int yPill(int x, int y, const char *label, uint16_t col) {
  int w = yPillWidth(label);
  gfx->fillRoundRect(x, y, w, Y_PILL_H, Y_PILL_H / 2, col);
  gfx->setTextColor(Y_TEXT);
  setSize(2);
  setCur(x + (w - textW(label, 2)) / 2, y + (Y_PILL_H - 14) / 2);
  printT(label);
  return w;
}

// moneda: disco dorado con el canto oscuro y un brillo
void yCoin(int cx, int cy, int r) {
  gfx->fillCircle(cx, cy, r, Y_COIN_DARK);
  gfx->fillCircle(cx - 1, cy - 1, r - 2, Y_COIN);
  gfx->fillRect(cx - 1, cy - r / 2, 2, r, Y_COIN_DARK);  // la raya del centro
}

// moneda + numero, centrados como grupo en cx; shadow = sobre el mapa
void yCoinBadge(int cx, int y, uint32_t n, bool shadow) {
  char s[12];
  snprintf(s, sizeof(s), "%lu", (unsigned long)n);
  setSize(2);
  int tw = textW(s, 2), w = 18 + 6 + tw, x = cx - w / 2;
  if (shadow) gfx->fillCircle(x + 9 + 2, y + 7 + 2, 9, Y_SHADOW);
  yCoin(x + 9, y + 7, 9);
  if (shadow) yShadowText(x + 24, y, s, 2, Y_TEXT);
  else {
    gfx->setTextColor(Y_TEXT);
    setCur(x + 24, y);
    printT(s);
  }
}

// lo ganado en monedas, "moneda +N", centrado en la pantalla (resultados de juegos y estadios)
void drawCoinGain(int y, uint16_t n, uint16_t col) {
  if (!n) return;
  char s[12];
  snprintf(s, sizeof(s), "+%u", (unsigned)n);
  setSize(2);
  int w = 18 + 6 + textW(s, 2), x = CX - w / 2;
  yCoin(x + 9, y + 7, 9);
  gfx->setTextColor(col);
  setCur(x + 24, y);
  printT(s);
}

static void yChevron(int cx, int cy, int dir, uint16_t col) {
  // dos trazos de 4 px: "<" (dir -1) o ">" (dir +1)
  for (int t = -2; t <= 1; t++) {
    gfx->drawLine(cx - dir * 5 + t, cy - Y_CHEV_HALF, cx + dir * 5 + t, cy, col);
    gfx->drawLine(cx + dir * 5 + t, cy, cx - dir * 5 + t, cy + Y_CHEV_HALF, col);
  }
}

void yChevrons(bool showL, bool showR) {
  if (showL) yChevron(Y_CHEV_LX, Y_CY, -1, Y_TEXT);
  if (showR) yChevron(Y_CHEV_RX, Y_CY, 1, Y_TEXT);
}

void yPager(int cur, int n, int y) {
  char s[12];
  snprintf(s, sizeof(s), "%d/%d", cur + 1, n);
  gfx->setTextColor(Y_TEXT_DIM);
  setSize(2);
  setCur(centerX(s, 2), y);
  printT(s);
}

// boton redondo de volver con una flecha curva blanca
void yBackBtn() {
  gfx->fillCircle(Y_BACK_X, Y_BACK_Y, Y_BACK_R, Y_BACK);
  // arco de 3/4 de vuelta (de las 12 en sentido horario hasta las 9) y la punta arriba a la izquierda
  for (int a = -90; a <= 180; a += 8) {
    float r = a * (float)PI / 180.0f;
    gfx->fillCircle(Y_BACK_X + (int)lroundf(cosf(r) * 8), Y_BACK_Y + 2 + (int)lroundf(sinf(r) * 8), 1, Y_TEXT);
  }
  gfx->fillTriangle(Y_BACK_X - 1, Y_BACK_Y - 12, Y_BACK_X - 1, Y_BACK_Y - 1, Y_BACK_X - 8, Y_BACK_Y - 6, Y_TEXT);
}

// dock de la casa: 3 circulos de color con su icono y una sombra debajo
void yDock(const YDockItem *items, int pressed) {
  for (int i = 0; i < Y_DOCK_N; i++) {
    int cx = yDockX(i), cy = Y_DOCK_Y + (i == pressed ? 2 : 0);
    gfx->fillCircle(cx + 2, Y_DOCK_Y + 3, Y_DOCK_R, Y_SHADOW);
    gfx->fillCircle(cx, cy, Y_DOCK_R, items[i].off ? (uint16_t)0x8410 : items[i].col);
    gfx->drawCircle(cx, cy, Y_DOCK_R, Y_TEXT);
    drawMap(items[i].icon, 16, cx - 16, cy - 16, 2, false);
  }
}

// aviso verde encima de una fila (tarjeta), con el texto centrado
void yToastC(int x, int y, int w, int h, const char *text, uint16_t col) {
  gfx->fillRoundRect(x, y, w, h, Y_CARD_RAD, col);
  gfx->setTextColor(Y_TEXT);
  setSize(2);
  setCur(x + (w - (int)textW(text, 2)) / 2, y + (h - 14) / 2);
  printT(text);
}
void yToast(int x, int y, int w, int h, const char *text) { yToastC(x, y, w, h, text, Y_ACCENT_GO); }

// globo de dialogo blanco con la cola hacia abajo; (cx, tipY) = la punta de la cola
void yBubble(int cx, int tipY, const char *text) {
  setSize(2);
  int tw = textW(text, 2), w = tw + 20, h = 30;
  int x = cx - w / 2;
  if (x < 20) x = 20;
  if (x + w > 446) x = 446 - w;
  int y = tipY - 8 - h;
  gfx->fillRoundRect(x + 2, y + 2, w, h, 10, Y_SHADOW);
  gfx->fillRoundRect(x, y, w, h, 10, Y_TEXT);
  gfx->fillTriangle(cx - 7, y + h - 1, cx + 7, y + h - 1, cx, tipY, Y_TEXT);
  gfx->setTextColor(UI_INK);
  setCur(x + 10, y + (h - 14) / 2);
  printT(text);
}

// barra de vida de 10 px con "cur/max" encima, a la izquierda
void yHpBar(int x, int y, int w, int cur, int max, uint16_t col) {
  char s[16];
  snprintf(s, sizeof(s), "%d/%d", cur, max);
  gfx->setTextColor(Y_TEXT);
  setSize(2);
  setCur(x, y - 20);
  printT(s);
  gfx->fillRoundRect(x, y, w, Y_HP_H, Y_HP_H / 2, Y_CARD_HI);
  int fw = max > 0 ? w * (cur < 0 ? 0 : cur > max ? max : cur) / max : 0;
  if (fw >= Y_HP_H) gfx->fillRoundRect(x, y, fw, Y_HP_H, Y_HP_H / 2, col);
}

// =================== el panel Jugar (v2.0, fase Y2; geometria en yard_panels.h) ===================
// Sustituye al menu emergente de JUGAR con las mismas reglas: tocar una tarjeta hace lo que hacia su hueco y
// vuelve a casa. Dos diferencias: el panel no se cierra solo a los 6 s, y una tarjeta que no se puede usar
// tiembla en el panel (que sigue abierto) en vez de cerrarlo y hacer temblar el boton de casa.
// v3.3: el panel Cuidar ya no existe (dormir, comida, el mimo y la ficha estan en el menu de cada bicho, mas
// abajo), y un minijuego lo juega el bicho que se elige (el selector, si pueden los dos de casa).

// v3.3: el bicho del hueco s puede jugar a un minijuego: nacido, despierto y sin una despedida en marcha
static bool homeCanPlay(uint8_t s) {
  const PetLife &l = pet.home(s);
  return l.hatched() && !l.sleeping && !l.busyEnding();
}
// los huecos (bit s) que pueden jugar al juego i del panel (la carrera, ademas, necesita el IMU)
static uint8_t playSlotsMask(uint8_t i) {
  if (i == PP_DASH && !imuPresent()) return 0;
  uint8_t m = 0;
  for (uint8_t s = 0; s < HOME_SLOTS; s++)
    if (homeCanPlay(s)) m |= (uint8_t)(1u << s);
  return m;
}

// Jugar: los juegos y las fases. Sin nadie despierto en casa no hay juegos, pero los de la caja si pueden ir a
// las fases (v1.65)
void openPlayPanel() {
  if (pet.ceremony) return;
  if (playSlotsMask(PP_BALL)) {
    panelKind = PANEL_PLAY;
    panelShakeIdx = -1;
    panelShakeUntil = 0;
  } else if (pet.boxHasPets()) {
    openStages();
  } else {
    sfxPlay(SFX_DENY);
  }
}

// que tarjeta se puede usar ahora (las que no, con contorno e icono apagado)
static bool panelItemOn(uint8_t kind, uint8_t i) {
  if (i == PP_BATTLE) return stagesCanOpen();
  if (i == PP_TRAVEL) return playSlotsMask(PP_BALL) != 0;  // (como antes: con alguien despierto en casa)
  return playSlotsMask(i) != 0;
}

// la tarjeta bajo el dedo (se ve pulsada), o -1
static int panelPressed() {
  if (!wasPressed || holdFired || swallowGesture) return -1;
  int i = panelHit(panelKind, tX0, tY0);
  return (i >= 0 && panelHit(panelKind, tXl, tYl) == i) ? i : -1;
}

static int panelShakeDx(int8_t idx) {
  if (idx != panelShakeIdx || !timeLeft(panelShakeUntil)) return 0;
  float s = 1.0f - (float)timeLeft(panelShakeUntil) / (float)FX_DENY_MS;
  return (int)(4.0f * (1.0f - s) * sinf(s * 6.0f * (float)PI));
}

static void panelDeny(int8_t idx) {
  sfxPlay(SFX_DENY);
  panelShakeIdx = idx;
  panelShakeUntil = millis() + FX_DENY_MS;
}

// la clave de repintado: todo lo que dibuja el panel
void mixPanelKey(uint32_t &h) {
  h = mixKey(h, 30u + panelKind);
  if (panelKind == PANEL_TRAVEL) mixTravelKey(h);
  h = mixKey(h, (uint32_t)(panelPressed() + 1));
  for (uint8_t i = 0; i < panelCount(panelKind); i++) h = mixKey(h, panelItemOn(panelKind, i));
  for (uint32_t v : { (uint32_t)pet.home(0).sleeping, (uint32_t)pet.home(1).sleeping, (uint32_t)gLang })
    h = mixKey(h, v);
}

// una tarjeta del panel: usable = tarjeta llena (clara si se pulsa); si no, solo el contorno y el icono
// apagado. Devuelve la x (con el temblor) para lo que se dibuje encima
static int panelCardDraw(uint8_t i, const char *const *icon, const char *label) {
  int x, y, w, h;
  panelCard(panelKind, i, x, y, w, h);
  x += panelShakeDx(i);
  bool on = panelItemOn(panelKind, i);
  if (on) {
    yCard(x, y, w, h, panelPressed() == i);
  } else {
    gfx->drawRoundRect(x, y, w, h, Y_CARD_RAD, STG_OFF_EDGE);
    gfx->drawRoundRect(x + 1, y + 1, w - 2, h - 2, Y_CARD_RAD - 1, STG_OFF_EDGE);
  }
  int iy = label ? y + 12 : y + (h - 48) / 2;
  if (on) drawMap(icon, 16, x + (w - 48) / 2, iy, 3, false);
  else drawMapTint(icon, 16, x + (w - 48) / 2, iy, 3, STG_LOCK_INK);
  if (label) {
    gfx->setTextColor(on ? Y_TEXT_DIM : STG_LOCK_INK);
    printTight(label, x + w / 2, y + h - 30, w - 8);
  }
  return x;
}

static void renderPlayPanel() {
  yTitle(T(S_DOCK_PLAY), nullptr);
  // bajo cada juego, la estadistica que entrena; el combate lleva a las fases
  panelCardDraw(PP_BALL, SPR_ICON_PLAY, T(S_STAT_SPE));
  panelCardDraw(PP_BATTLE, SPR_ICON_BATTLE, T(S_STAGES));
  panelCardDraw(PP_SACK, SPR_ICON_SACK, T(S_STAT_ATK));
  panelCardDraw(PP_DASH, SPR_ICON_DASH, T(S_STAT_SPE));
  panelCardDraw(PP_ZEN, SPR_ICON_ZEN, T(S_STAT_DEF));
  panelCardDraw(PP_TRAVEL, SPR_ICON_TRAVEL, T(S_TRV_TITLE));  // Y5b: los viajes
}

void renderPanel() {
  if (panelKind == PANEL_TRAVEL) {
    renderTravel();  // pinta su propio fondo (la ventana del mapa)
  } else {
    yPanelBg();
    renderPlayPanel();
  }
  yBackBtn();
  gfx->flush();
}

// v3.3: empieza el juego i del panel con el bicho del hueco s (que pasa a tener el foco)
static void startPlayFor(uint8_t s, uint8_t i) {
  panelKind = PANEL_NONE;  // el juego se abre en su lugar; al salir, a casa
  petPick = PICK_NONE;
  if (!uiFocus(s)) {
    sfxPlay(SFX_DENY);
    return;
  }
  Serial.printf("PLAY %u slot=%u\n", i, s);
  if (i == PP_BALL) startGame();
  else if (i == PP_SACK) startSack();
  else if (i == PP_DASH) startDash();
  else startZen();
}

static void playPanelTap(int i) {
  if (!panelItemOn(PANEL_PLAY, i)) {
    panelDeny(i);
    return;
  }
  sfxPlay(SFX_TAP);
  if (i == PP_TRAVEL) {  // Y5b: la pantalla de viajes, un nivel por encima (USER vuelve aqui)
    openTravel();
    return;
  }
  if (i == PP_BATTLE) {  // v1.65: las fases (antes un combate suelto); el selector de equipo elige
    panelKind = PANEL_NONE;
    openStages();
    return;
  }
  uint8_t m = playSlotsMask((uint8_t)i);
  if (m == 3) {  // v3.3: pueden los dos de casa: quien juega (el panel sigue debajo)
    openPetPick(PICK_GAME, (uint8_t)i);
    return;
  }
  startPlayFor((m & 1) ? 0 : 1, (uint8_t)i);
}

void panelTap(int16_t x, int16_t y) {
  if (panelKind == PANEL_TRAVEL) {
    travelTap(x, y);
    return;
  }
  if (yBackHit(x, y)) {  // volver: a casa, como USER
    sfxPlay(SFX_TAP);
    panelKind = PANEL_NONE;
    return;
  }
  int i = panelHit(panelKind, x, y);
  if (i < 0) return;  // fuera de las tarjetas: nada (es una pantalla, no un menu que se cierra)
  playPanelTap(i);
}

// =================== el selector de bicho (v3.3; geometria en yard_panels.h) ===================
// Una pantalla encima del panel Jugar o de la mochila, con una tarjeta por bicho de casa (miniatura, nombre y
// nivel): quien juega el minijuego, o a quien se le da el objeto. Solo sale cuando pueden los dos; volver o
// USER la cierran y se vuelve a lo de debajo.
void openPetPick(uint8_t mode, uint8_t arg) {
  petPick = mode;
  petPickArg = arg;
  Serial.printf("PICK open %s %u\n", mode == PICK_GAME ? "game" : "item", arg);
}

void closePetPick() {
  if (petPick) Serial.println("PICK close");
  petPick = PICK_NONE;
}

void renderPetPick() {
  yPanelBg();
  bool item = petPick == PICK_ITEM;
  yTitle(T(item ? S_PICK_ITEM : S_PICK_PLAY), item ? T((StrId)(S_IT_GOLDEN + petPickArg)) : nullptr);
  if (!item) {  // el juego: su icono bajo el titulo
    static const char *const *const GAME_ICON[PP_COUNT] = { SPR_ICON_PLAY, SPR_ICON_BATTLE, SPR_ICON_SACK,
                                                            SPR_ICON_DASH, SPR_ICON_ZEN, SPR_ICON_TRAVEL };
    drawMap(GAME_ICON[petPickArg < PP_COUNT ? petPickArg : 0], 16, CX - 16, Y_SUB_Y - 4, 2, false);
  }
  for (uint8_t s = 0; s < HOME_SLOTS; s++) {
    int x, y, w, h;
    petPickCard(s, x, y, w, h);
    const PetLife &l = pet.home(s);
    yCard(x, y, w, h, false);
    int cx = x + w / 2;
    // su sprite del patio (quieto, el primer cuadro), que quepa en 96 px de alto; sin el, su miniatura
    const uint8_t *t = l.hatched() ? thumbs.get(l.speciesId) : nullptr;
    if (l.hatched() && homeMon[s].loaded) {
      YardBody b = yardPmdBody(homeMon[s]);
      int h10 = b.h * 10 / YARD_PET_SCALE10;  // su alto a la escala entera b.s (b.h lleva el extra del patio)
      int s10 = h10 > 0 ? b.s * 10 * 84 / h10 / 10 * 10 : 20;  // la escala entera que lo deja en <= 84 px
      if (s10 < 10) s10 = 10;
      if (s10 > 40) s10 = 40;
      drawPmdActF(homeMon[s], PMD_IDLE, cx, y + 106, 0, true, false, (uint16_t)s10);
    } else if (t) {
      drawThumbFit(t, cx, y + 62, w - 20, 100, false, 0);
    } else if (l.hatched()) {
      yShadowText(cx - 9, y + 48, "?", 4, Y_TEXT);
    }
    const char *nm = l.nick[0] ? l.nick : dexName(l.speciesId);
    gfx->setTextColor(Y_TEXT);
    printTight(nm, cx, y + 124, w - 12);
    char lv[16];
    snprintf(lv, sizeof(lv), T(S_LVL_FMT), (unsigned)l.level());
    gfx->setTextColor(Y_TEXT_DIM);
    setSize(2);
    setCur(cx - textW(lv, 2) / 2, y + 150);
    printT(lv);
  }
  yBackBtn();
  gfx->flush();
}

void petPickTap(int16_t x, int16_t y) {
  if (yBackHit(x, y)) {
    sfxPlay(SFX_TAP);
    closePetPick();
    return;
  }
  int c = petPickHit(x, y);
  if (c < 0) return;  // fuera de las tarjetas: nada (como los paneles)
  uint8_t s = (uint8_t)c;
  Serial.printf("PICK %u\n", s);
  if (petPick == PICK_GAME) {
    if (!(playSlotsMask(petPickArg) & (1u << s))) {  // se durmio mientras tanto
      sfxPlay(SFX_DENY);
      return;
    }
    sfxPlay(SFX_TAP);
    startPlayFor(s, petPickArg);
  } else {
    uint8_t it = petPickArg;
    petPick = PICK_NONE;
    if (bagUseOn(s, it)) closeBag();  // a casa a verlo, como al usarlo con el "usar?"
    else sfxPlay(SFX_DENY);
  }
}

// =================== el menu de un bicho (v3.3; geometria en yard_panels.h) ===================
// Tocar a un bicho del patio le da el foco y abre a la derecha, encima del patio, una columna con lo que se le
// puede hacer: Dormir / Despertar, Comida, Cuidar (el mimo), Estado (su ficha) y, cuando toca, Evolucionar o
// Despedirse, resaltada (sustituyen a los botones grandes de antes; un "!" sobre el bicho avisa). El bicho se
// queda quieto con un anillo a sus pies (si estaba debajo del menu, antes se aparta andando a la izquierda). Lo
// que no puede hacer ahora sale apagado y tiembla. Comida abre en la misma columna la lista de comida: la baya
// normal y lo que se tiene, que aparece delante del bicho y se la come al momento (yardFeedFront). Tocar fuera,
// USER o deslizar lo cierran; tocar al otro bicho pasa el menu a el. Dormir, la comida y el mimo lo dejan
// abierto (se ve el resultado y se puede repetir); Estado, Evolucionar y Despedirse abren lo suyo y lo cierran.
static const char *const PM_NAMES[PM_KINDS] = { "sleep", "food", "care", "status", "evolve", "farewell" };
#define PM_TEXT_X 48  // la etiqueta, a la derecha del icono (8 + 32 + 8)

// las filas del menu del bicho del hueco s (PM_*), y cuantas
static uint8_t petMenuItems(uint8_t s, uint8_t *out) {
  const PetLife &l = pet.home(s);
  uint8_t n = 0;
  out[n++] = PM_SLEEP;
  out[n++] = PM_FOOD;
  out[n++] = PM_CARE;
  out[n++] = PM_STATUS;
  if (l.wantEvolveButton()) out[n++] = PM_EVOLVE;
  else if (l.wantFarewellButton()) out[n++] = PM_FAREWELL;
  return n;
}

// la fila k se puede usar ahora: comer y el mimo, despierto; dormir, sin una despedida en marcha
static bool petMenuItemOn(uint8_t s, uint8_t k) {
  const PetLife &l = pet.home(s);
  if (k == PM_FOOD || k == PM_CARE) return l.hatched() && !l.sleeping && !l.busyEnding();
  if (k == PM_SLEEP) return l.hatched() && !l.busyEnding();
  return true;
}

// las comidas de la lista, en el orden de FOOD_*: la baya normal (siempre) y las que se tienen
static uint8_t petFoods(uint8_t *out) {
  uint8_t n = 0;
  out[n++] = FOOD_PLAIN;
  for (uint8_t f = FOOD_BERRY0; f < FOOD_COUNT; f++)
    if (pet.items[foodItem(f)]) out[n++] = f;
  return n;
}

// la comida f se le puede dar ahora al bicho del hueco s (que tiene el foco): una a la vez, y sin contar las que
// ya estan elegidas para el otro (aun no gastadas)
static bool petFoodOn(uint8_t s, uint8_t f) {
  if (pet.focusSlot != s || !pet.canFeed(f) || yardFood[s].phase != YF_NONE) return false;
  uint8_t it = foodItem(f);
  return it == FOOD_NO_ITEM || pet.items[it] > yardFoodReserved(it);
}

bool openPetMenu(uint8_t s) {
  if (s >= HOME_SLOTS) return false;
  const PetLife &l = pet.home(s);
  if (!l.hatched() || l.busyEnding() || l.evolving() || !yardHomeNow()) return false;  // un huevo o un hueco vacio, no
  if (!uiFocus(s)) return false;
  petMenuOpen = (uint8_t)(s + 1);
  petMenuView = 0;
  petFoodPage = 0;
  pmShakeIdx = -1;
  // debajo de donde sale el menu no se verian: los que esten debajo se apartan a la izquierda en el patio (yardTick,
  // yardMenuHold; enseguida), y mientras siga abierto nadie pasea hasta alli (yardW.avoidX)
  for (YardAside &a : yardAside) a = YardAside{};
  Serial.printf("MENU %u open\n", s);
  return true;
}

void closePetMenu() {
  if (petMenuOpen) Serial.printf("MENU %u close\n", petMenuOpen - 1);
  petMenuOpen = 0;
  petMenuView = 0;
  petFoodPage = 0;
  pmShakeIdx = -1;
}

// de la lista de comida a las acciones (la fila de volver o USER)
void petMenuBackToActions() {
  petMenuView = 0;
  petFoodPage = 0;
  pmShakeIdx = -1;
  if (petMenuOpen) Serial.printf("MENU %u back\n", petMenuOpen - 1);
}

// en cada vuelta del loop: el menu se cierra si su bicho ya no lo puede usar (se puso a evolucionar, se
// despide, se muere, el hueco cambio), si se abrio otra pantalla encima, o si se dejo abierto sin tocar nada
void petMenuCheck() {
  if (!petMenuOpen) return;
  uint8_t s = petMenuOpen - 1;
  const PetLife &l = pet.home(s);
  bool other = gameOpen || sackOpen || zenOpen || dashOpen || battleOpen() || encOpen() || teamOpen || stagesOpen ||
               cardOpen || kbOpen || clockOpen || bagOpen || panelKind || boxOpen || galleryOpen || petPick ||
               choiceKind || confirmUntil || yardDemoOpen;
  if (!l.hatched() || l.busyEnding() || l.evolving() || pet.focusSlot != s || !yardHomeNow() || other ||
      millis() - lastInteract > PET_MENU_IDLE_MS)
    closePetMenu();
}

static int pmShakeDx(int8_t idx) {
  if (idx != pmShakeIdx || !timeLeft(pmShakeUntil)) return 0;
  float s = 1.0f - (float)timeLeft(pmShakeUntil) / (float)FX_DENY_MS;
  return (int)(4.0f * (1.0f - s) * sinf(s * 6.0f * (float)PI));
}

static void pmDeny(uint8_t s, int8_t row, const char *what) {
  sfxPlay(SFX_DENY);
  pmShakeIdx = row;
  pmShakeUntil = millis() + FX_DENY_MS;
  Serial.printf("MENU %u deny %s\n", s, what);
}

// la fila bajo el dedo (se ve pulsada), o -1
static int pmPressedRow(uint8_t n) {
  if (!wasPressed || holdFired || swallowGesture) return -1;
  int r = petMenuHit(n, tX0, tY0);
  return (r >= 0 && petMenuHit(n, tXl, tYl) == r) ? r : -1;
}

// una fila: sombra y tarjeta (sobre el mapa)
static void pmRow(int x, int y, int w, int h, uint16_t fill, bool pressed) {
  gfx->fillRoundRect(x + 2, y + 3, w, h, Y_CARD_RAD, Y_SHADOW);
  gfx->fillRoundRect(x, y, w, h, Y_CARD_RAD, pressed ? Y_CARD_HI : fill);
}
static void pmIcon(const char *const *icon, int x, int y, int h, bool on) {
  int iy = y + (h - 32) / 2;
  if (on) drawMap(icon, 16, x + 8, iy, 2, false);
  else drawMapTint(icon, 16, x + 8, iy, 2, STG_LOCK_INK);
}
// la etiqueta, alineada a la izquierda en maxW px (si no cabe, apretada como printTight)
static void pmLabel(const char *t, int x, int y, int maxW) {
  setSize(2);
  int w = textW(t, 2);
  if (gCjkFont || w <= maxW) {
    setCur(x, y);
    printT(t);
  } else {
    printTight(t, x + maxW / 2, y, maxW);
  }
}
// el nombre del bicho encima de la columna (n filas), alineado con su borde derecho, y debajo (v3.8) sus 3
// necesidades en barritas: COM, FEL, ENE, con los colores de la ficha
static void pmName(uint8_t s, uint8_t n) {
  const PetLife &l = pet.home(s);
  const char *nm = l.nick[0] ? l.nick : dexName(l.speciesId);
  int colX = petMenuX(n), ny = petMenuNameY(n);
  setSize(2);
  int w = textW(nm, 2), x = petMenuHeadRight(n, ny, PM_NAME_H) - w;
  if (x < colX) x = colX;
  yShadowText(x, ny, nm, 2, Y_TEXT);
  int by = petMenuNeedsY(n);
  int right = petMenuHeadRight(n, by, PM_NEED_H);
  int bw = right - colX;
  const uint8_t vals[3] = {l.fullness, l.joy, l.energy};
  for (uint8_t i = 0; i < 3; i++) {
    int y = by + i * (PM_NEED_BAR_H + PM_NEED_GAP);
    gfx->fillRoundRect(colX, y, bw, PM_NEED_BAR_H, 2, Y_CARD_HI);
    int fw = bw * vals[i] / 100;
    if (fw > 0) gfx->fillRoundRect(colX, y, fw, PM_NEED_BAR_H, 2, needBarColor(vals[i], i == 0 && l.sated()));
  }
}

static void drawFoodList(uint8_t s) {
  uint8_t foods[FOOD_COUNT], nf = petFoods(foods);
  uint8_t pages = foodPages(nf);
  if (petFoodPage >= pages) petFoodPage = 0;
  uint8_t n = foodListRows(nf, petFoodPage);
  bool more = pages > 1;
  int x, y, w, h;
  petMenuRow(0, n, x, y, w, h);
  pmName(s, n);
  int pr = pmPressedRow(n);
  for (uint8_t i = 0; i + 1 < n; i++) {
    uint8_t f = foods[petFoodPage * FOOD_PER_PAGE + i];
    petMenuRow(i, n, x, y, w, h);
    x += pmShakeDx((int8_t)i);
    bool on = petFoodOn(s, f);
    pmRow(x, y, w, h, Y_PANEL_BG, pr == i);
    pmIcon(FOOD_ICON[f], x, y, h, on);
    // la favorita, ya descubierta (v1.57): marco dorado y una estrella en la esquina del icono
    if (foodIsBerry(f) && pet.berryKnown && pet.lovesBerry(f - FOOD_BERRY0)) {
      gfx->drawRoundRect(x, y, w, h, Y_CARD_RAD, UI_BAR_WARN);
      gfx->drawRoundRect(x + 1, y + 1, w - 2, h - 2, Y_CARD_RAD - 1, UI_BAR_WARN);
      drawMap(SPR_STAR, 16, x + 2, y + 2, 1, false);
    }
    // cuantas quedan, en una chapa sobre la esquina del icono (la baya normal no se acaba)
    uint8_t it = foodItem(f);
    if (it != FOOD_NO_ITEM && pet.items[it]) {
      char cn[4];
      snprintf(cn, sizeof(cn), "%u", pet.items[it]);
      int bx = x + 38, by = y + 10;
      gfx->fillCircle(bx, by, 9, UI_BAR_BAD);
      setSize(1);
      gfx->setTextColor(UI_WHITE);
      setCur(bx - textW(cn, 1) / 2 + 1, by - 3);
      printT(cn);
    }
    gfx->setTextColor(on ? Y_TEXT : STG_LOCK_INK);
    pmLabel(T((StrId)(S_FD_PLAIN + f)), x + PM_TEXT_X, y + (h - 14) / 2, w - PM_TEXT_X - 6);
  }
  // abajo: volver (y "mas" a su derecha con varias paginas)
  int bx, by, bw, bh;
  foodNavCell(n, more, 0, bx, by, bw, bh);
  pmRow(bx, by, bw, bh, Y_PANEL_BG, false);
  int ax = more ? bx + bw / 2 : bx + 24, ay = by + bh / 2;
  gfx->fillTriangle(ax - 8, ay, ax + 6, ay - 9, ax + 6, ay + 9, Y_TEXT);
  if (!more) {
    gfx->setTextColor(Y_TEXT);
    pmLabel(T(S_PM_BACK), bx + PM_TEXT_X, by + (bh - 14) / 2, bw - PM_TEXT_X - 6);
  } else {
    foodNavCell(n, more, 1, bx, by, bw, bh);
    pmRow(bx, by, bw, bh, Y_PANEL_BG, false);
    char pg[8];
    snprintf(pg, sizeof(pg), "%u/%u", petFoodPage + 1, pages);
    setSize(2);
    int tw = textW(pg, 2), tx = bx + (bw - tw - 12) / 2;
    gfx->setTextColor(Y_TEXT);
    setCur(tx, by + (bh - 14) / 2);
    printT(pg);
    int cx = tx + tw + 6, cy = by + bh / 2;
    gfx->fillTriangle(cx + 6, cy, cx - 2, cy - 7, cx - 2, cy + 7, Y_TEXT);
  }
}

// el menu (o la lista de comida) del bicho de petMenuOpen, encima del patio
void drawPetMenu(uint32_t now) {
  uint8_t s = petMenuOpen - 1;
  if (petMenuView == 1) {
    drawFoodList(s);
    return;
  }
  uint8_t items[PM_MAX], n = petMenuItems(s, items);
  int x, y, w, h;
  petMenuRow(0, n, x, y, w, h);
  pmName(s, n);
  int pr = pmPressedRow(n);
  const PetLife &l = pet.home(s);
  for (uint8_t i = 0; i < n; i++) {
    petMenuRow(i, n, x, y, w, h);
    x += pmShakeDx((int8_t)i);
    uint8_t k = items[i];
    bool on = petMenuItemOn(s, k);
    uint16_t fill = Y_PANEL_BG, ink = on ? Y_TEXT : STG_LOCK_INK;
    if (k == PM_EVOLVE) {  // resaltadas, como los botones grandes de antes: rojo / dorado
      fill = UI_BAR_BAD;
      ink = UI_WHITE;
    } else if (k == PM_FAREWELL) {
      fill = UI_BAR_WARN;
      ink = UI_INK;
    }
    pmRow(x, y, w, h, fill, pr == i);
    if ((k == PM_EVOLVE || k == PM_FAREWELL) && (now / 400) % 2 == 0) {  // y un borde blanco que late
      gfx->drawRoundRect(x, y, w, h, Y_CARD_RAD, UI_WHITE);
      gfx->drawRoundRect(x + 1, y + 1, w - 2, h - 2, Y_CARD_RAD - 1, UI_WHITE);
    }
    const char *const *icon = k == PM_SLEEP    ? (l.sleeping ? SPR_ICON_SUN : SPR_ICON_LIGHT)
                              : k == PM_FOOD   ? SPR_ICON_BERRY
                              : k == PM_CARE   ? SPR_ICON_PET
                              : k == PM_STATUS ? SPR_ICON_CARD
                              : k == PM_EVOLVE ? SPR_STAR
                                               : SPR_ICON_LETTER;
    StrId lab = k == PM_SLEEP    ? (l.sleeping ? S_PM_WAKE : S_PM_SLEEP)
                : k == PM_FOOD   ? S_PM_FOOD
                : k == PM_CARE   ? S_DOCK_CARE
                : k == PM_STATUS ? S_PM_STATUS
                : k == PM_EVOLVE ? S_PM_EVOLVE
                                 : S_PM_FAREWELL;
    pmIcon(icon, x, y, h, on);
    gfx->setTextColor(ink);
    pmLabel(T(lab), x + PM_TEXT_X, y + (h - 14) / 2, w - PM_TEXT_X - 6);
  }
}

// una accion del menu (fila row)
static void petMenuAct(uint8_t s, uint8_t k, int8_t row) {
  if (pet.focusSlot != s) {  // (no deberia pasar: petMenuCheck lo cierra antes)
    closePetMenu();
    return;
  }
  if (!petMenuItemOn(s, k)) {  // dormido: ni comida ni mimo
    pmDeny(s, row, PM_NAMES[k]);
    return;
  }
  uint32_t now = millis();
  switch (k) {
    case PM_SLEEP:
      Serial.printf("MENU %u %s\n", s, pet.sleeping ? "wake" : "sleep");
      sfxPlay(SFX_TAP);
      pet.toggleLight();
      break;
    case PM_FOOD:
      Serial.printf("MENU %u food\n", s);
      sfxPlay(SFX_TAP);
      petMenuView = 1;
      petFoodPage = 0;
      pmShakeIdx = -1;
      break;
    case PM_CARE: {
      // el mimo (v2.3): +20 de felicidad, corazon, sonido y la pose de alegria
      Serial.printf("MENU %u care\n", s);
      FxNeedSnap ns = fxSnap();
      pet.caress();
      fxNeeds(ns);  // "+20" de felicidad
      sfxPlay(SFX_HEART);
      fxPose(pmd.has(PMD_POSE) ? PMD_POSE : PMD_HOP, now, 1200);
      break;
    }
    case PM_STATUS:
      Serial.printf("MENU %u status\n", s);
      sfxPlay(SFX_TAP);
      closePetMenu();
      cardOpen = true;
      cardPage = 0;
      break;
    case PM_EVOLVE:
    case PM_FAREWELL:  // el dialogo de siempre: evolucionar / mantener, despedirse / quedaros
      Serial.printf("MENU %u %s\n", s, PM_NAMES[k]);
      sfxPlay(SFX_TAP);
      closePetMenu();
      choiceKind = k == PM_EVOLVE ? 1 : 2;
      choiceUntil = now + 12000;
      break;
  }
}

// una comida de la lista: delante del bicho, y se la come ya (Pet la aplica, y gasta el objeto, al acabar el
// bocado: yardFoodEaten)
static void petFoodPick(uint8_t s, uint8_t f, int8_t row) {
  if (!petFoodOn(s, f)) {  // dormido, sin ese objeto o comiendose ya otra
    pmDeny(s, row, "eat");
    return;
  }
  int r = yardFeedFront(s, f);
  if (r > 0) {
    sfxPlay(SFX_TAP);
    Serial.printf("MENU %u eat %u\n", s, f);
    return;
  }
  if (r == 0) {
    pmDeny(s, row, "eat");
    return;
  }
  // -1: sin su bicho en el patio (no deberia pasar): al momento, como antes de la fase Y3
  FxNeedSnap ns = fxSnap();
  if (!pet.feedFood(f)) {
    pmDeny(s, row, "eat");
    return;
  }
  fxNeeds(ns);
  sfxPlay(SFX_EAT);
  Serial.printf("MENU %u eat %u (now)\n", s, f);
}

// un toque con el menu abierto: sus filas; fuera, el otro bicho de casa se queda con el menu y cualquier otro
// sitio lo cierra (y no hace nada mas: el dock, las cacas y los regalos esperan al siguiente toque)
void petMenuTap(int16_t x, int16_t y) {
  uint8_t s = petMenuOpen - 1;
  if (petMenuView == 1) {
    uint8_t foods[FOOD_COUNT], nf = petFoods(foods);
    uint8_t pages = foodPages(nf);
    if (petFoodPage >= pages) petFoodPage = 0;
    uint8_t n = foodListRows(nf, petFoodPage);
    int r = foodListHit(n, pages > 1, x, y);
    if (r == FL_BACK) {
      sfxPlay(SFX_TAP);
      petMenuBackToActions();
      return;
    }
    if (r == FL_MORE) {
      sfxPlay(SFX_TAP);
      petFoodPage = (uint8_t)((petFoodPage + 1) % pages);
      pmShakeIdx = -1;
      Serial.printf("MENU %u page %u\n", s, petFoodPage);
      return;
    }
    if (r >= 0) {
      petFoodPick(s, foods[petFoodPage * FOOD_PER_PAGE + r], (int8_t)r);
      return;
    }
  } else {
    uint8_t items[PM_MAX], n = petMenuItems(s, items);
    int r = petMenuHit(n, x, y);
    if (r >= 0) {
      petMenuAct(s, items[r], (int8_t)r);
      return;
    }
  }
  int who = yardPetAt(x, y);
  closePetMenu();
  if (who >= 0 && who < HOME_SLOTS && who != s && openPetMenu((uint8_t)who)) sfxPlay(SFX_TAP);
}

// serie (MENU): lo que hay abierto, cada fila y donde tocarla (TAP x y)
void petMenuReport() {
  if (!petMenuOpen) {
    Serial.println("menu=closed");
    return;
  }
  uint8_t s = petMenuOpen - 1;
  int x, y, w, h;
  if (petMenuView == 0) {
    uint8_t items[PM_MAX], n = petMenuItems(s, items);
    Serial.printf("menu=%u view=actions rows=%u\n", s, n);
    for (uint8_t i = 0; i < n; i++) {
      petMenuRow(i, n, x, y, w, h);
      Serial.printf("  row %u %s %s tap=%d,%d\n", i, PM_NAMES[items[i]], petMenuItemOn(s, items[i]) ? "on" : "off",
                    x + w / 2, y + h / 2);
    }
    return;
  }
  uint8_t foods[FOOD_COUNT], nf = petFoods(foods);
  uint8_t pages = foodPages(nf);
  if (petFoodPage >= pages) petFoodPage = 0;
  uint8_t n = foodListRows(nf, petFoodPage);
  Serial.printf("menu=%u view=food page=%u/%u rows=%u\n", s, petFoodPage + 1, pages, n);
  for (uint8_t i = 0; i + 1 < n; i++) {
    uint8_t f = foods[petFoodPage * FOOD_PER_PAGE + i], it = foodItem(f);
    petMenuRow(i, n, x, y, w, h);
    Serial.printf("  row %u food=%u have=%d %s tap=%d,%d\n", i, f, it == FOOD_NO_ITEM ? -1 : pet.items[it],
                  petFoodOn(s, f) ? "on" : "off", x + w / 2, y + h / 2);
  }
  foodNavCell(n, pages > 1, 0, x, y, w, h);
  Serial.printf("  back tap=%d,%d\n", x + w / 2, y + h / 2);
  if (pages > 1 && foodNavCell(n, true, 1, x, y, w, h)) Serial.printf("  more tap=%d,%d\n", x + w / 2, y + h / 2);
}

// =================== viajes (v2.0, Y5b; geometria y logica pura en travel_ui.h) ===================
// Una pantalla (PANEL_TRAVEL) un nivel por encima del panel Jugar. Sin viaje: ventana del destino (< >),
// los bichos de la caja que pueden salir, las tres duraciones y el boton Ir. Con un viaje en marcha:
// cuenta atras, ventana con el bicho y el registro de sucesos. Al volver, el aviso va en la casa
// (drawReturnBanner). Lo elegido solo vive en RAM; la unica escritura es startTravel / takeTravelResult.

static uint8_t trvOkMask() {  // huecos de la caja con un bicho sano
  uint8_t m = 0;
  for (uint8_t k = 0; k < BOX_SLOTS; k++)
    if (pet.boxState(k) == BOX_OK) m |= (uint8_t)(1u << k);
  return m;
}

static uint8_t trvOwnedMask() {  // mapas propios (bit n-1)
  uint8_t m = 0;
  for (uint8_t n = 1; n <= TRV_DESTS; n++)
    if (pet.ownsMap(n)) m |= (uint8_t)(1u << (n - 1));
  return m;
}

// deja la seleccion en algo valido (el bicho puede haber vuelto o haberse ido de la caja)
static uint8_t trvFix(uint8_t *cand) {
  uint8_t n = trvCandidates(trvOkMask(), cand);
  if (trvSelPet >= n) trvSelPet = 0;
  uint8_t own = trvOwnedMask();
  if (!own) own = 1;  // el prado siempre es del jugador
  if (!(own & (1u << (trvSelDest - 1)))) trvSelDest = trvStepDest(own, trvSelDest, 1);
  if (trvSelTier >= TRV_TIERS) trvSelTier = 0;
  return n;
}

void openTravel() {
  panelKind = PANEL_TRAVEL;
  trvSelPet = 0;
  trvSelTier = 0;
  trvSelDest = pet.yardMapWanted();
  panelShakeIdx = -1;
  panelShakeUntil = 0;
}

extern uint16_t *yardBg;  // (se define con el fondo del patio, mas abajo)
// una ventana con el trozo (x, y, w, h) del fondo ya expandido (yardBg), tomado desde la fila sy y con las
// esquinas redondas. Sin fondo (sin PSRAM): una tarjeta lisa
static void yardWindow(int x, int y, int w, int h, int sy, int r) {
  uint16_t *fb = (uint16_t *)gfx->getFramebuffer();
  if (!yardBg || !fb || sy < 0 || sy + h > YBG_CANVAS) {  // (sy en filas del lienzo)
    gfx->fillRoundRect(x, y, w, h, r, Y_CARD_HI);
    return;
  }
  for (int j = 0; j < h; j++) {
    int jj = j < h - 1 - j ? j : h - 1 - j;  // distancia al borde de arriba o de abajo
    int inset = 0;
    if (jj < r) {
      float dy = (float)(r - jj) - 0.5f;
      inset = r - (int)(sqrtf((float)(r * r) - dy * dy) + 0.5f);
    }
    memcpy(fb + (size_t)(y + j) * YBG_CANVAS + x + inset, yardBgCanvasRow(yardBg, sy + j) + x + inset,
           (size_t)(w - 2 * inset) * 2);
  }
  gfx->drawRoundRect(x - 1, y - 1, w + 2, h + 2, r + 1, Y_CARD_HI);
  gfx->drawRoundRect(x - 2, y - 2, w + 4, h + 4, r + 2, Y_CARD_HI);
}

// Texto a tamano 2 partido en lineas: copia en out el trozo mas largo de s que cabe en maxW (hasta una palabra
// entera si se puede) y devuelve los bytes que consumio, con el espacio de despues. Sin espacios que sirvan,
// corta por letras (sin partir un caracter UTF-8)
static int wrapLine(const char *s, int maxW, char *out, int outN) {
  int len = (int)strlen(s), fit = 0, wordEnd = 0;
  if (len >= outN) len = outN - 1;
  for (int i = 1; i <= len; i++) {
    if (i < len && ((uint8_t)s[i] & 0xC0) == 0x80) continue;  // dentro de un caracter UTF-8
    memcpy(out, s, i);
    out[i] = 0;
    if (textW(out, 2) > maxW) break;
    fit = i;
    if (i == len || s[i] == ' ') wordEnd = i;
  }
  int cut = wordEnd ? wordEnd : fit;
  if (!cut) {  // ni la primera letra cabe: se dibuja igual
    cut = 1;
    while (cut < len && ((uint8_t)s[cut] & 0xC0) == 0x80) cut++;
  }
  memcpy(out, s, cut);
  out[cut] = 0;
  while (s[cut] == ' ') cut++;
  return cut;
}

static void trvDrawArrow(int cx, int cy, int dir, bool shadow) {
  if (shadow) yChevron(cx + 2, cy + 2, dir, Y_SHADOW);
  yChevron(cx, cy, dir, Y_TEXT);
}

// "Guarda un bicho en la Caja\npara mandarlo de viaje": dos lineas centradas
static void trvTwoLines(const char *t, int cx, int y, int maxW, uint16_t col) {
  char a[64];
  const char *nl = strchr(t, '\n');
  int la = nl ? (int)(nl - t) : (int)strlen(t);
  if (la > (int)sizeof(a) - 1) la = (int)sizeof(a) - 1;
  memcpy(a, t, la);
  a[la] = 0;
  gfx->setTextColor(col);
  printTight(a, cx, y, maxW);
  if (nl) printTight(nl + 1, cx, y + 22, maxW);
}

static void renderTravelSetup() {
  uint8_t cand[8];
  uint8_t nc = trvFix(cand);
  uint8_t own = trvOwnedMask() ? trvOwnedMask() : 1;
  // el fondo: oscuro, con el destino en una ventana
  trvMapPrev = trvSelDest;
  yardEnsureBg();
  yPanelBg();
  yTitle(T(S_TRV_TITLE), nullptr);
  yardWindow(TRV_BAN_X, TRV_BAN_Y, TRV_BAN_W, TRV_BAN_H, 190, 12);
  const char *dn = T((StrId)(S_MAP_1 + trvSelDest - 1));
  yShadowText(centerX(dn, 2), TRV_BAN_Y + (TRV_BAN_H - 14) / 2, dn, 2, Y_TEXT);
  if (own & (own - 1)) {  // mas de un mapa propio: flechas
    trvDrawArrow(TRV_BAN_X + 20, TRV_BAN_Y + TRV_BAN_H / 2, -1, true);
    trvDrawArrow(TRV_BAN_X + TRV_BAN_W - 20, TRV_BAN_Y + TRV_BAN_H / 2, 1, true);
  }
  // los bichos
  if (!nc) {
    trvTwoLines(T(S_TRV_NOPETS), CX, TRV_PET_Y + 12, 330, Y_TEXT_DIM);
  } else {
    uint8_t pages = trvPetPages(nc), page = trvSelPet / TRV_PET_PAGE;
    for (uint8_t i = 0; i < TRV_PET_PAGE; i++) {
      uint8_t idx = page * TRV_PET_PAGE + i;
      if (idx >= nc) break;
      const PetRecord &r = pet.boxRecord(cand[idx]);
      int x = trvPetX(i), y = TRV_PET_Y, cx = x + TRV_PET_W / 2;
      bool sel = idx == trvSelPet;
      yCard(x, y, TRV_PET_W, TRV_PET_H, sel);
      if (sel) souvFrame(x, y, TRV_PET_W, TRV_PET_H, Y_ACCENT_GO);
      const uint8_t *t = thumbs.get(r.speciesId);
      if (t) drawThumbFit(t, cx, y + 18, TRV_PET_W - 20, 30, false, sel ? Y_CARD_HI : Y_CARD);
      gfx->setTextColor(Y_TEXT);
      printTight(recName(r), cx, y + 34, TRV_PET_W - 6);
      char lv[16];
      snprintf(lv, sizeof(lv), T(S_LVL_FMT), (unsigned)levelFor(r.growMin));
      gfx->setTextColor(Y_TEXT_DIM);
      printTight(lv, cx, y + 54, TRV_PET_W - 6);
    }
    if (pages > 1) {
      if (page > 0) trvDrawArrow(TRV_PET_ARROW_X, TRV_PET_ARROW_Y, -1, false);
      if (page + 1 < pages) trvDrawArrow(466 - TRV_PET_ARROW_X, TRV_PET_ARROW_Y, 1, false);
    }
  }
  // las duraciones, con lo que pagan
  for (uint8_t t = 0; t < TRV_TIERS; t++) {
    int x = trvTierX(t), y = TRV_TIER_Y;
    bool sel = t == trvSelTier;
    yCard(x, y, TRV_TIER_W, TRV_TIER_H, sel);
    if (sel) souvFrame(x, y, TRV_TIER_W, TRV_TIER_H, Y_ACCENT_GO);
    char h[8];
    snprintf(h, sizeof(h), "%u h", (unsigned)trvTierHours(t));
    gfx->setTextColor(Y_TEXT);
    setSize(2);
    setCur(x + (TRV_TIER_W - textW(h, 2)) / 2, y + 10);
    printT(h);
    shopPrice(x + TRV_TIER_W / 2, y + 37, TRV_COINS[t], Y_TEXT_DIM);
  }
  // Ir
  bool clock = eggClockKnown(pet.lastSeenEpoch);
  bool can = nc && pet.canTravel(cand[trvSelPet], trvSelDest, trvSelTier);
  int gx = trvGoX();
  gfx->fillRoundRect(gx + 2, TRV_GO_Y + 2, TRV_GO_W, Y_PILL_H, Y_PILL_H / 2, Y_SHADOW);
  gfx->fillRoundRect(gx, TRV_GO_Y, TRV_GO_W, Y_PILL_H, Y_PILL_H / 2, can ? Y_ACCENT_GO : Y_CARD_HI);
  gfx->setTextColor(can ? Y_TEXT : SHOP_PRICE_OFF);
  setSize(2);
  setCur(gx + (TRV_GO_W - textW(T(S_TRV_GO), 2)) / 2, TRV_GO_Y + (Y_PILL_H - 14) / 2);
  printT(T(S_TRV_GO));
  if (nc && !clock) {  // sin hora del RTC no se sabe cuando vuelve
    gfx->setTextColor(Y_ACCENT_BAD);
    printTight(T(S_TRV_NOCLOCK), CX, TRV_HINT_Y, 330);
  }
}

static void renderTravelTrip() {
  uint8_t dest = pet.travelDest();
  trvMapPrev = dest;
  yardEnsureBg();
  yPanelBg();
  yTitle(T(S_TRV_TRAVELLING), nullptr);
  char c[12];
  trvClockStr(c, sizeof(c), pet.travelSecondsLeft());
  gfx->setTextColor(Y_TEXT);
  setSize(4);
  setCur(CX - textW(c, 4) / 2, TRV_CLOCK_Y);
  printT(c);
  // la ventana del destino con el bicho
  yardWindow(TRV_WIN_X, TRV_WIN_Y, TRV_WIN_W, TRV_WIN_H, 170, 14);
  const char *dn = T((StrId)(S_MAP_1 + dest - 1));
  yShadowText(TRV_WIN_X + 16, TRV_WIN_Y + 10, dn, 2, Y_TEXT);
  uint8_t sl = pet.travelSlot();
  if (sl < BOX_SLOTS && pet.boxState(sl) == BOX_OK) {
    const PetRecord &r = pet.boxRecord(sl);
    const uint8_t *t = thumbs.get(r.speciesId);
    if (t) drawThumbFit(t, TRV_WIN_X + TRV_WIN_W / 2, TRV_WIN_Y + 58, 120, 64, false, Y_CARD);
    yShadowText(TRV_WIN_X + 16, TRV_WIN_Y + TRV_WIN_H - 26, recName(r), 2, Y_TEXT);
  }
  drawMap(SPR_ICON_TRAVEL, 16, TRV_WIN_X + TRV_WIN_W - 46, TRV_WIN_Y + TRV_WIN_H - 42, 2, false);
  // el registro: los sucesos que ya pasaron, "HH:MM texto"
  uint8_t idx[TRV_EVENTS_MAX];
  uint8_t n = trvVisibleEvents(pet.travelSeed(), dest, pet.travelTier(), pet.travelMinutesElapsed(), idx);
  if (!n) {
    gfx->setTextColor(Y_TEXT_DIM);
    printTight(T(S_TRV_ONWAY), CX, TRV_LOG_Y + 24, TRV_LOG_W);
    return;
  }
  int y = TRV_LOG_Y;
  for (uint8_t k = 0; k < n; k++) {
    uint16_t off;
    uint8_t tpl, arg;
    travelEvent(pet.travelSeed(), dest, pet.travelTier(), idx[k], &off, &tpl, &arg);
    char tm[8];
    char text[96];
    trvTimeStr(tm, sizeof(tm), pet.travelStartEpoch() + (uint32_t)off * 60);
    snprintf(text, sizeof(text), T((StrId)(S_TRV_EV_0 + tpl)), arg ? dexName(arg) : "");
    setSize(2);
    gfx->setTextColor(Y_TEXT_DIM);
    setCur(TRV_LOG_X, y);
    printT(tm);
    int tx = TRV_LOG_X + textW(tm, 2) + 10;
    char l1[96];
    int used = wrapLine(text, TRV_LOG_X + TRV_LOG_W - tx, l1, sizeof(l1));
    gfx->setTextColor(Y_TEXT);
    setCur(tx, y);
    printT(l1);
    if (text[used]) {  // la segunda linea; si ni asi cabe, con las letras juntas (nunca pasa de dos)
      char l2[96];
      int u2 = wrapLine(text + used, TRV_LOG_W, l2, sizeof(l2));
      if (text[used + u2]) {
        printTight(text + used, TRV_LOG_X + TRV_LOG_W / 2, y + TRV_LOG_LINE, TRV_LOG_W);
      } else {
        setCur(TRV_LOG_X, y + TRV_LOG_LINE);
        printT(l2);
      }
      y += TRV_LOG_LINE;
    }
    y += TRV_LOG_LINE + TRV_LOG_GAP;
  }
}

void renderTravel() {
  if (pet.travelling()) renderTravelTrip();
  else renderTravelSetup();
}

void travelTap(int16_t x, int16_t y) {
  if (yBackHit(x, y)) {  // volver: al panel Jugar, como USER
    sfxPlay(SFX_TAP);
    panelKind = PANEL_PLAY;
    return;
  }
  if (pet.travelling()) return;  // el viaje en marcha no se toca (ni se cancela)
  uint8_t cand[8];
  uint8_t nc = trvFix(cand);
  TrvHit h = trvHit(x, y);
  switch (h.kind) {
    case TH_DEST_L:
    case TH_DEST_R: {
      uint8_t m = trvStepDest(trvOwnedMask() ? trvOwnedMask() : 1, trvSelDest, h.kind == TH_DEST_L ? -1 : 1);
      if (m != trvSelDest) {
        trvSelDest = m;
        sfxPlay(SFX_TAP);
      }
      break;
    }
    case TH_PET_L:
    case TH_PET_R: {
      uint8_t pages = trvPetPages(nc), page = trvSelPet / TRV_PET_PAGE;
      int np = (int)page + (h.kind == TH_PET_L ? -1 : 1);
      if (np >= 0 && np < pages && nc) {
        trvSelPet = (uint8_t)(np * TRV_PET_PAGE);
        sfxPlay(SFX_TAP);
      }
      break;
    }
    case TH_PET: {
      uint8_t idx = (uint8_t)((trvSelPet / TRV_PET_PAGE) * TRV_PET_PAGE + h.i);
      if (idx < nc) {
        trvSelPet = idx;
        sfxPlay(SFX_TAP);
      }
      break;
    }
    case TH_TIER:
      trvSelTier = h.i;
      sfxPlay(SFX_TAP);
      break;
    case TH_GO:
      if (nc && pet.canTravel(cand[trvSelPet], trvSelDest, trvSelTier) &&
          pet.startTravel(cand[trvSelPet], trvSelDest, trvSelTier)) {
        sfxPlay(SFX_LEVEL);
        trvMapPrev = trvSelDest;
      } else {
        sfxPlay(SFX_DENY);
      }
      break;
    default:
      break;
  }
}

// lo que dibuja el panel de viajes y cambia: la seleccion, la cuenta atras (cada segundo), la caja y los mapas
void mixTravelKey(uint32_t &h) {
  uint8_t cand[8];
  uint8_t nc = trvFix(cand);
  for (uint32_t v : { (uint32_t)trvSelPet, (uint32_t)trvSelDest, (uint32_t)trvSelTier, (uint32_t)nc,
                      (uint32_t)pet.travelling(), (uint32_t)(pet.travelling() ? pet.travelSecondsLeft() : 0),
                      (uint32_t)pet.travelSlot(), (uint32_t)pet.travelDest(), (uint32_t)trvOwnedMask(),
                      (uint32_t)eggClockKnown(pet.lastSeenEpoch), (uint32_t)yardMapNow() })
    h = mixKey(h, v);
  for (uint8_t i = 0; i < nc; i++) h = mixKey(h, pet.boxRecord(cand[i]).crc);
}

// el hueco de la caja del que acaba de volver y aun no se vio el aviso (-1: ninguno, o ya no esta en la caja).
// El resultado no guarda el hueco: se busca por especie, shiny y mote (dos iguales: el primero, da igual)
int8_t trvBackSlot() {
  if (!pet.travelResultPending()) return -1;
  TravelResult r;
  if (!pet.peekTravelResult(&r)) return -1;
  for (uint8_t k = 0; k < BOX_SLOTS; k++) {
    if (pet.boxState(k) != BOX_OK || pet.slotTravelling(k)) continue;
    const PetRecord &rec = pet.boxRecord(k);
    if (rec.speciesId == r.speciesId && ((rec.flags & REC_SHINY) != 0) == (r.shiny != 0) &&
        strncmp(rec.nick, r.nick, sizeof(r.nick) - 1) == 0)
      return (int8_t)k;
  }
  return -1;
}

// el aviso de vuelta (en la casa): "<Nombre> ha vuelto!", el destino y lo que trajo. Un toque lo quita
static void drawReturnBanner() {
  TravelResult r;
  if (!pet.peekTravelResult(&r)) return;
  char nick[sizeof(r.nick) + 1];
  memcpy(nick, r.nick, sizeof(r.nick));
  nick[sizeof(r.nick)] = 0;
  const char *name = nick[0] ? nick : dexName(r.speciesId);
  bool lv = r.lvAfter > r.lvBefore, hasItem = r.item != TRV_NONE, hasSouv = r.souvenir != TRV_NONE && r.souvenir < SOUV_COUNT;
  const int W = 340, X = CX - W / 2, ROW = 40;
  int rows = (hasItem ? 1 : 0) + (hasSouv ? 1 : 0);
  int H = 104 + rows * ROW + 12;
  int y0 = CX - H / 2 - 10;
  gfx->fillRoundRect(X + 3, y0 + 4, W, H, 16, Y_SHADOW);
  gfx->fillRoundRect(X, y0, W, H, 16, UI_BAR_WARN);
  gfx->drawRoundRect(X, y0, W, H, 16, UI_INK);
  gfx->setTextColor(UI_INK);
  char t[48];
  snprintf(t, sizeof(t), T(S_TRV_BACK_FMT), name);
  if (textW(t, 3) <= W - 24) {
    setSize(3);
    setCur(CX - textW(t, 3) / 2, y0 + 12);
    printT(t);
  } else {
    printTight(t, CX, y0 + 16, W - 24);
  }
  // el destino y la duracion
  char d[40];
  snprintf(d, sizeof(d), "%s - %u h", T((StrId)(S_MAP_1 + (r.dest >= 1 && r.dest <= TRV_DESTS ? r.dest : 1) - 1)),
           (unsigned)trvTierHours(r.tier));
  gfx->setTextColor(BOX_NOTE_INK);
  printTight(d, CX, y0 + 44, W - 24);
  // monedas y niveles, en una linea
  char cs[12];
  char ls[16];
  snprintf(cs, sizeof(cs), "+%u", (unsigned)r.coins);
  ls[0] = 0;
  if (lv) snprintf(ls, sizeof(ls), T(S_LV_RANGE_FMT), (unsigned)r.lvBefore, (unsigned)r.lvAfter);
  setSize(2);
  int wc = 18 + 6 + textW(cs, 2), wl = lv ? 28 + textW(ls, 2) : 0;
  int gx = CX - (wc + wl) / 2, ry = y0 + 72;
  yCoin(gx + 9, ry + 7, 9);
  gfx->setTextColor(UI_INK);
  setCur(gx + 24, ry);
  printT(cs);
  if (lv) {
    setCur(gx + wc + 28, ry);
    printT(ls);
  }
  int y = y0 + 104;
  if (hasItem) {
    const char *in = T((StrId)(S_IT_GOLDEN + r.item));
    if (r.itemLost) drawMapTint(ITEM_ICON[r.item], 16, X + 18, y + 4, 2, BOX_NOTE_INK);
    else drawMap(ITEM_ICON[r.item], 16, X + 18, y + 4, 2, false);
    gfx->setTextColor(r.itemLost ? BOX_NOTE_INK : UI_INK);
    if (r.itemLost) {
      setSize(2);
      setCur(X + 62, y + 2);
      printT(in);
      gfx->setTextColor(Y_ACCENT_BAD);
      setCur(X + 62, y + 20);
      printT(T(S_TRV_BAGFULL));
    } else {
      setSize(2);
      setCur(X + 62, y + 13);
      printT(in);
    }
    y += ROW;
  }
  if (hasSouv) {
    const char *sn = T((StrId)(S_SOUV_0 + r.souvenir));
    drawMap(SOUV_ICON[r.souvenir], 16, X + 18, y + 4, 2, false);
    int tagW = r.dup ? 70 : 62;
    gfx->setTextColor(UI_INK);
    int nx = X + 62, nw = W - 62 - 18 - tagW - 8;
    setSize(2);
    if (textW(sn, 2) <= nw) {
      setCur(nx, y + 13);
      printT(sn);
    } else {
      printTight(sn, nx + nw / 2, y + 13, nw);
    }
    int tx = X + W - 14 - tagW;
    if (r.dup) {  // ya lo tenia: se cambio por monedas
      char ds[8];
      snprintf(ds, sizeof(ds), "+%u", (unsigned)r.dupCoins);
      yCoin(tx + 9, y + 20, 9);
      gfx->setTextColor(UI_INK);
      setSize(2);
      setCur(tx + 24, y + 13);
      printT(ds);
    } else {
      gfx->fillRoundRect(tx, y + 8, tagW, 24, 12, UI_BAR_BAD);
      gfx->setTextColor(UI_WHITE);
      setSize(2);
      setCur(tx + (tagW - textW(T(S_TRV_NEW), 2)) / 2, y + 13);
      printT(T(S_TRV_NEW));
    }
  }
}

// ---- pantalla de muestra del kit (serie YARDUI [pagina]): 0 panel, 1 mundo, 2 fase ----
// fila de la tienda de muestra: icono, nombre, precio y pastilla de comprar
static void yDemoShopRow(int y, const char *const *icon, const char *name, int price) {
  const int x = 70, w = 326, h = 64;
  yCard(x, y, w, h, false);
  drawMap(icon, 16, x + 12, y + 16, 2, false);
  gfx->setTextColor(Y_TEXT);
  setSize(2);
  setCur(x + 56, y + 12);
  printT(name);
  yCoin(x + 63, y + 45, 7);
  char s[8];
  snprintf(s, sizeof(s), "%d", price);
  gfx->setTextColor(Y_TEXT_DIM);
  setCur(x + 76, y + 38);
  printT(s);
  int pw = yPillWidth(T(S_BUY));
  yPill(x + w - 12 - pw, y + (h - Y_PILL_H) / 2, T(S_BUY), Y_ACCENT_GO);
}

static void yDemoShop() {
  yPanelBg();
  yTitle(T(S_SHOP), nullptr);
  yCoinBadge(Y_CX, Y_SUB_Y, 37, false);
  gfx->drawFastHLine(150, 100, 166, Y_CARD_HI);
  yDemoShopRow(118, SPR_ICON_GOLDEN, T(S_IT_GOLDEN), 10);
  yDemoShopRow(194, SPR_ICON_CAKE, T(S_IT_CAKE), 40);
  yDemoShopRow(270, SPR_ICON_REVIVE, T(S_IT_REVIVE), 80);
  yToast(70, 194, 326, 64, T(S_PURCHASED));
  yChevrons(true, true);
  yPager(1, 3, 352);
  yBackBtn();
}

static void yDemoWorld() {
  // un prado de cuadros de 32 px (lo que mide una baldosa de 16 a escala 2) en dos verdes
  for (int ty = 0; ty < 466; ty += 32)
    for (int tx = 0; tx < 466; tx += 32)
      gfx->fillRect(tx, ty, 32, 32, ((tx + ty) / 32) & 1 ? C565(0x8c, 0xc6, 0x5a) : C565(0x80, 0xbc, 0x52));
  drawBattery(Y_CX - BAT_ICON_W / 2);
  yShadowText(centerX("12:30", 5), 30, "12:30", 5, Y_TEXT);
  yCoinBadge(Y_CX, 76, 117, true);
  // el bicho de muestra: un huevo con su sombra, y su globo con el estado
  gfx->fillEllipse(Y_CX, 262, 26, 8, C565(0x5a, 0x8a, 0x3c));
  drawMap(SPR_ICON_EGG, 16, Y_CX - 32, 200, 4, false);
  yBubble(Y_CX, 192, T(S_HUNGRY));
  YDockItem dock[Y_DOCK_N] = {
    { SPR_ICON_PLAY, Y_DOCK_PLAY, false },
    { SPR_ICON_BAG, Y_DOCK_BAG, false },
    { SPR_ICON_SHOP, Y_DOCK_SHOP, false },
  };
  yDock(dock, -1);
}

static void yDemoStage() {
  yPanelBg();
  char t[24];
  snprintf(t, sizeof(t), T(S_STAGE_FMT), 1u);
  yTitle(t, stageName(0));
  yHpBar(60, 128, 156, 48, 52, Y_ACCENT_GO);
  yHpBar(250, 128, 156, 98, 100, Y_ACCENT_BAD);
  // ventana de la pelea (la fase Y1 pondra aqui el bioma y los dos bichos)
  gfx->fillRoundRect(60, 152, 346, 136, Y_CARD_RAD, C565(0x80, 0xbc, 0x52));
  gfx->drawRoundRect(60, 152, 346, 136, Y_CARD_RAD, Y_CARD_HI);
  gfx->setTextColor(Y_TEXT);
  setSize(2);
  setCur(centerX(T(S_SUPER_EFF), 2), 302);
  printT(T(S_SUPER_EFF));
  gfx->setTextColor(Y_TEXT_DIM);
  setCur(centerX(T(S_CRITICAL), 2), 326);
  printT(T(S_CRITICAL));
  yBackBtn();
}

void renderYardDemo() {
  if (yardDemoPage == 0) yDemoShop();
  else if (yardDemoPage == 1) yDemoWorld();
  else yDemoStage();
  gfx->flush();
}

void yardDemoTap(int16_t x, int16_t y) {
  if (yBackHit(x, y)) {
    yardDemoOpen = false;
    return;
  }
  int c = yChevronHit(x, y);
  if (c) yardDemoPage = (uint8_t)((yardDemoPage + YDEMO_PAGES + c) % YDEMO_PAGES);
}

// =================== el patio: la casa (v2.0, fase Y2) ===================
// La casa con un bicho nacido es un mapa de arriba abajo (/yard/NN.bin, yardbg.h) por el que
// pasean el bicho de casa y el primero de la caja (yard.h). Encima, la hora, la bateria y el dock
// (Cuidar / Jugar / Mochila); el estado sale en un globo sobre el bicho, y las cacas y los regalos
// estan en el suelo. La casa vacia, la despedida y el Revivir siguen con su pantalla.
//
// Fase Y3 (acciones en el mundo):
// - Comer: la baya o la tarta del panel Cuidar cae al suelo cerca del bicho, el bicho la ve ("!"), va
//   andando, se la come (Eat) y solo entonces Pet la aplica (con su "+N", su sonido y un corazon). Una a
//   la vez: con una en el suelo, las tarjetas de comida se apagan. Si deja el patio (otra pantalla, un
//   juego, un combate) la comida le espera en el suelo y sigue al volver; si ya no puede comer (se
//   duerme, ceremonia, se muere, otro bicho en casa) desaparece sin gastarse nada: la tarta solo se
//   descuenta al comersela.
// - El huevo esta en el patio (el de la caja pasea a su alrededor) y eclosiona solo a las 2 h reales;
//   cada sacudida (una cada 30 s) le quita 5 min, hasta 1 h (eggtimer.h). Tocarlo lo tambalea y dice lo
//   que falta. El inicial (y uno a medias del sistema de antes) tambien se ve aqui, pero se abre con 3
//   toques como siempre.

// v3.0: el patio se ve con alguien en casa (bicho o huevo) en cualquiera de los dos huecos
bool yardHomeNow() {
  return !pet.awaitingStarter() && !pet.ceremony && !pet.isDying() && pet.homeVacant() < HOME_SLOTS;
}

// el globo del huevo del patio, un rato (tras un toque, una sacudida que conto o EGGT): los dos huecos
void yardEggInfo() {
  yardEggInfoUntil[0] = yardEggInfoUntil[1] = millis() + 2600;
}

static_assert(YBG_CANVAS == UI_SIZE, "el lienzo es la ventana (YBG_CROP, YBG_CROP) del mapa");

// ---- el fondo ----
// el mapa del patio es del jugador (Pet::yardMapWanted: el elegido, o el prado del principio), igual para
// todos los bichos
#define YARD_MAP_FALLBACK 1  // si falta el del bioma; si falta este tambien, el suelo liso
#define YARD_MAP_NONE 255    // YARDBG 255: el suelo liso a proposito (para verlo)

uint8_t *yardFile = nullptr;       // el .bin entero en PSRAM (yardView.px apunta dentro)
YardBgView yardView;
uint16_t *yardBg = nullptr;        // 480x480 RGB565 ya tintado, en PSRAM (460 KB); el lienzo es la ventana (7, 7)
bool gYardFull = true;             // v2.5: el mapa tambien fuera del circulo (YARDFULL 0 lo apaga)
uint8_t yardWalk[YBG_WALK_BYTES];  // la mascara de paso (solo la zona conectada mas grande)
uint8_t yardMapForced = 0;         // YARDBG <n>; 0 = el elegido
uint8_t yardMapWant = 0;           // el pedido la ultima vez (0 = ninguno aun)
uint8_t yardMapLoaded = 0;         // el cargado; 0 = el suelo liso de respaldo
uint32_t yardMapEpoch = 0;
int32_t yardBgKey = -1;            // tinte + mapa + bioma de lo que hay en yardBg
char yardWhy[48] = "";             // por que no se cargo el pedido (YARDBG)

uint8_t yardMapNow() { return yardMapLoaded; }

static bool yardTryMap(uint8_t n) {
  size_t len = 0;
  uint8_t *d = sdReadYardMap(n, YBG_FILE_MAX, &len);
  if (!d) {
    snprintf(yardWhy, sizeof(yardWhy), "missing /yard/%02u.bin", n);
    return false;
  }
  YbgErr e = yardBgParse(d, len, &yardView);
  if (e != YBG_OK) {
    free(d);
    snprintf(yardWhy, sizeof(yardWhy), "bad /yard/%02u.bin (err %u)", n, (unsigned)e);
    return false;
  }
  if (yardFile) free(yardFile);
  yardFile = d;
  memcpy(yardWalk, yardView.walk, YBG_WALK_BYTES);
  yardMaskFrame(yardWalk);  // v3.5: el circulo y el HUD con la escala de ahora del mapa
  yardKeepLargestRegion(yardWalk);
  yardMapLoaded = n;
  return true;
}

// carga el mapa que toca (el del bioma, o el forzado); se mira cada frame, solo lee al cambiar
void yardEnsureMap() {
  // el selector de fondos (Y4b) manda sobre lo elegido, solo mientras esta abierto
  uint8_t want = yardMapForced ? yardMapForced : yardMapPreview ? yardMapPreview
                 : (panelKind == PANEL_TRAVEL && trvMapPrev) ? trvMapPrev : pet.yardMapWanted();  // (Y5b: la ventana de los viajes)
  if (want == yardMapWant && yardMapEpoch == sdEpoch) return;
  yardMapWant = want;
  yardMapEpoch = sdEpoch;
  yardBgKey = -1;
  yardWhy[0] = 0;
  if (want == YARD_MAP_NONE) {
    snprintf(yardWhy, sizeof(yardWhy), "forced");
  } else if (!sdReady) {
    snprintf(yardWhy, sizeof(yardWhy), "no SD");
  } else if (yardTryMap(want)) {
    Serial.printf("yard map %02u\n", want);
    return;
  } else if (want != YARD_MAP_FALLBACK) {
    char first[48];
    snprintf(first, sizeof(first), "%s", yardWhy);
    if (yardTryMap(YARD_MAP_FALLBACK)) {
      snprintf(yardWhy, sizeof(yardWhy), "%s, using %02u", first, YARD_MAP_FALLBACK);
      Serial.printf("yard map %02u (%s)\n", YARD_MAP_FALLBACK, yardWhy);
      return;
    }
  }
  if (yardFile) free(yardFile);
  yardFile = nullptr;
  yardMapLoaded = 0;
  yardFallbackWalk(yardWalk);
  Serial.printf("yard map fallback (%s)\n", yardWhy);
}

// un rectangulo en el mapa entero (480x480), en coordenadas del lienzo (como todo el patio)
static void yBufRect(uint16_t *b, int x, int y, int w, int h, uint16_t c) {
  x += YBG_CROP;
  y += YBG_CROP;
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > YBG_FULL) w = YBG_FULL - x;
  if (y + h > YBG_FULL) h = YBG_FULL - y;
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++) b[(y + j) * YBG_FULL + x + i] = c;
}

// sin mapa: suelo liso del color del bioma, en cuadros de una celda apenas distintos, y unas matas
void yardPaintFallback(uint16_t *b, uint8_t biome, YardTint t) {
  uint16_t soil = BIOME_SOIL[biome < BIOME_COUNT ? biome : 0];
  const uint16_t ink = C565(0x10, 0x18, 0x20);
  uint16_t base = yardTint565(soil, t), alt = yardTint565(lerp565(soil, ink, 1, 16), t);
  uint16_t dk = yardTint565(lerp565(soil, ink, 6, 16), t), lt = yardTint565(lerp565(soil, 0xFFFF, 6, 16), t);
  for (int cy = 0; cy < YBG_GRID; cy++)
    for (int cx = 0; cx < YBG_GRID; cx++)
      yBufRect(b, yardCellEdge(cx), yardCellEdge(cy), yardCellEdge(cx + 1) - yardCellEdge(cx),
               yardCellEdge(cy + 1) - yardCellEdge(cy), (cx + cy) & 1 ? alt : base);
  uint32_t seed = 0x7a4d11u;
  auto rnd = [&](int n) { seed = seed * 1664525u + 1013904223u; return (int)((seed >> 16) % (uint32_t)n); };
  for (int k = 0; k < 34; k++) {
    int x = 30 + rnd(406), y = 30 + rnd(406);
    int dx = x - 233, dy = y - 233;
    if (dx * dx + dy * dy > 205 * 205) continue;
    if ((x >= 130 && x <= 336 && y <= 116) || (x >= 140 && x <= 326 && y >= 370)) continue;  // bajo el HUD, liso
    if (k % 4 == 3) {  // una piedrecita
      yBufRect(b, x, y, 6, 4, lt);
      yBufRect(b, x + 2, y + 4, 6, 2, dk);
      continue;
    }
    yBufRect(b, x, y - 10, 2, 10, dk);  // una mata: tres briznas
    yBufRect(b, x - 4, y - 6, 2, 6, dk);
    yBufRect(b, x + 4, y - 7, 2, 7, dk);
    yBufRect(b, x - 2, y - 12, 2, 2, lt);
  }
}

// re-expande el fondo si cambio el mapa, el bioma o el tinte (una vez, no por frame)
void yardEnsureBg() {
  yardEnsureMap();
  YardTint tint = yardTintAt((uint8_t)sceneHour(), pet.sleeping);
  uint8_t biome = homeBiome();
  int32_t key = (int32_t)tint | (yardMapLoaded << 2) | (biome << 10);
  if (key == yardBgKey) return;
  if (!yardBg) yardBg = (uint16_t *)ps_malloc((size_t)YBG_FULL * YBG_FULL * 2);
  if (!yardBg) return;  // sin PSRAM: yardDrawBg pinta el suelo liso cada frame
  yardBgKey = key;
  if (yardFile) {
    static uint16_t pal[256];
    yardTintPalette(yardView.pal, pal, yardView.palCount, tint);
    yardBgExpandFull(yardView.px, pal, yardBg);
  } else {
    yardPaintFallback(yardBg, biome, tint);
  }
}

void yardDrawBg() {
  yardEnsureBg();
  if (yardBg) {
    uint16_t *fb = gfx->getFramebuffer();
    for (int y = 0; y < YBG_CANVAS; y++) memcpy(fb + (size_t)y * YBG_CANVAS, yardBgCanvasRow(yardBg, y), YBG_CANVAS * 2);
  } else gfx->fillScreen(yardTint565(BIOME_SOIL[homeBiome() % BIOME_COUNT], yardTintAt((uint8_t)sceneHour(), pet.sleeping)));
}

// v2.5: este frame, el mapa tambien en las esquinas y el borde del panel (fuera del circulo, que
// sigue cortando todo lo dibujado). Se pide en cada frame del patio; las demas pantallas, en negro.
void yardShowFull() {
  if (gYardFull && yardBg) roundCanvas->showUnder(yardBg, YBG_FULL, YBG_CROP, (uint32_t)yardBgKey);
}

void yardForceMap(uint8_t n) {
  yardMapForced = n;
  yardMapWant = 0;  // se vuelve a leer
  yardEnsureBg();
}

void yardReport() {
  Serial.printf("yardbg want=%u loaded=%u%s%s forced=%u free=%d pets=%u\n", yardMapWant, yardMapLoaded,
                yardWhy[0] ? " why=" : "", yardWhy, yardMapForced, yardFreeCount(yardWalk), yardPetsShown());
}

// ---- los bichos ----

// el alto visible del PMD en reposo (de su primera fila con algo a los pies) da la escala del patio:
// unos 60-100 px, la mitad que en la casa de lado, para que quepan dos bichos
YardBody yardPmdBody(PmdMon &m) {
  YardBody b = { YB_PMD, 2, 40, 60 };
  const PmdAct &a = m.acts[PMD_IDLE];
  if (!a.frames) return b;
  int top = a.h, x0 = a.w, x1 = -1;
  for (int r = 0; r < a.h; r++)
    for (int c = 0; c < a.w; c++)
      if (a.data[r * a.w + c] != 0xFF) {
        if (r < top) top = r;
        if (c < x0) x0 = c;
        if (c > x1) x1 = c;
      }
  int vh = (a.base ? a.base : a.h) - top, vw = x1 >= x0 ? x1 - x0 + 1 : a.w;
  if (vh < 1) vh = a.h;
  b.s = vh <= 16 ? 3 : (vh * 2 <= 104 ? 2 : 1);  // casi todos a 2; los muy pequenos a 3, los enormes a 1
  b.h = (int16_t)(vh * b.s * YARD_PET_SCALE10 / 10);
  b.w = (int16_t)(vw * b.s * YARD_PET_SCALE10 / 10);
  return b;
}

// v3.0: p[0] y p[1] son los dos huecos de casa (su sprite, homeMon[i]; el B/N de respaldo, solo el del foco)
// y p[2] el que vuelve de viaje (yardBoxPmd o su miniatura). Los de la caja ya no pasean (hibernan)
#define YARD_TRV 2  // el paseante del viajero
YardBody yardBodyOf(int i) {
  YardBody b = { YB_NONE, 2, 40, 48 };
  if (i < HOME_SLOTS) {
    if (homeMon[i].loaded) return yardPmdBody(homeMon[i]);
    if (i == pet.focusSlot && mon.loaded) {
      b.kind = YB_TPK1;
      b.s = mon.h * 2 <= 110 ? 2 : 1;
      b.w = (int16_t)(mon.w * b.s * 3 / 4);
      b.h = (int16_t)(mon.h * b.s);
    } else if (flashIdxForDex(pet.home(i).speciesId) >= 0) {
      b.kind = YB_FLASH;
      b.w = 52;
      b.h = 58;
    } else if (thumbs.get(pet.home(i).speciesId)) {
      b.kind = YB_THUMB;
      b.w = 56;
      b.h = 60;
    }
  } else {
    if (yardBoxPmd.loaded) return yardPmdBody(yardBoxPmd);
    if (thumbs.get(yardBoxDex)) {
      b.kind = YB_THUMB;
      b.w = 56;
      b.h = 60;
    }
  }
  return b;
}

int yardPetHeadY(int i) { return (int)yardW.p[i].y - yardBodyOf(i).h; }

uint8_t yardPetsShown() {
  uint8_t n = 0;
  for (int i = 0; i < yardW.n; i++) n += yardW.p[i].on;
  return n;
}

PmdMon &yardMonOf(int i) { return i < HOME_SLOTS ? homeMon[i] : yardBoxPmd; }

// gesto al azar (los de la casa de lado: Pose, Nod, Breath; Hop salta mucho y Sit mira atras)
bool yardPickGesture(uint8_t i, uint8_t *act, uint32_t *ms) {
  PmdMon &m = yardMonOf(i);
  static const uint8_t flair[] = { PMD_POSE, PMD_NOD, PMD_BREATH };
  uint8_t pick[3], n = 0;
  for (uint8_t f : flair)
    if (m.has(f)) pick[n++] = f;
  if (!n) return false;
  *act = pick[random(n)];
  *ms = pmdActTotalMs(m.acts[*act]);
  return true;
}

// quien esta en el patio: los dos de casa (los nacidos) y el que vuelve de viaje (cargando su sprite si cambio)
void yardSyncPets() {
  yardW.n = 3;
  yardW.pickGesture = yardPickGesture;
  if (yardW.rng.s == 1) yardW.rng.s = (uint32_t)random(1, 0x7fffffff);
  for (uint8_t s = 0; s < HOME_SLOTS; s++) {
    YardPet &h = yardW.p[s];
    if (pet.home(s).isEgg()) {  // el huevo (Y3) o el hueco vacio: aun no hay nadie que pasee
      h.on = false;
      continue;
    }
    if (!h.on) {
      h = YardPet{};
      h.on = true;
      // recien nacido, donde estaba su huevo; si no, abajo del centro (el segundo, a un lado)
      int c = yardEggX[s] >= 0 ? yardNearestFreeCell(yardWalk, yardEggX[s], yardEggY[s])
                               : yardNearestFreeCell(yardWalk, CX + (s ? 90 : 0), PET_GROUND);
      if (c >= 0 && yardSpotAway(yardW, s, (float)yardCellCenter(yardCellCX(c)), (float)yardCellCenter(yardCellCY(c)))) {
        h.x = (float)yardCellCenter(yardCellCX(c));
        h.y = (float)yardCellCenter(yardCellCY(c));
      } else {
        yardPlace(yardW, yardWalkNow(), s);
      }
      yardLastStep = 0;
    }
    h.walks = homeMon[s].has(PMD_WALKL) || homeMon[s].has(PMD_WALKR);
  }
  int8_t slot = trvBackSlot();  // v2.1: el que vuelve de viaje sale al patio a esperar que lo toquen
  YardPet &b = yardW.p[YARD_TRV];
  if (slot < 0) {
    b.on = false;
    if (yardBoxPmd.loaded) yardBoxPmd.unload();
    yardBoxDex = -2;
    yardBoxSlot = -1;
    return;
  }
  const PetRecord &r = pet.boxRecord((uint8_t)slot);
  bool sh = (r.flags & REC_SHINY) != 0;
  if (r.speciesId != yardBoxDex || sh != yardBoxShiny || yardBoxEpoch != sdEpoch) {
    yardBoxDex = r.speciesId;
    yardBoxShiny = sh;
    yardBoxEpoch = sdEpoch;
    yardBoxPmd.unload();
    if (r.speciesId >= 1 && r.speciesId <= DEX_COUNT) yardBoxPmd.load((uint8_t)r.speciesId, sh);
  }
  if (slot != yardBoxSlot) b.on = false;  // otro bicho: a un sitio nuevo
  yardBoxSlot = slot;
  if (!b.on) {
    b = YardPet{};
    b.on = true;
    yardPlace(yardW, yardWalkNow(), YARD_TRV);
  }
  b.walks = false;  // el viajero espera quieto
}

// la mascara de paso de ahora: la del mapa y, con huevos en el suelo (Y3), sus celdas bloqueadas (los
// bichos pasean a su alrededor, no por encima)
const uint8_t *yardWalkNow() {
  bool egg = false, enc = yardEncX >= 0;  // v2.2: lo que se mueve, tambien
  for (uint8_t s = 0; s < HOME_SLOTS; s++) egg |= pet.home(s).isEgg() && !pet.home(s).isVacant() && yardEggX[s] >= 0;
  if (!egg && !enc) return yardWalk;
  static uint8_t w[YBG_WALK_BYTES];
  memcpy(w, yardWalk, sizeof(w));
  for (uint8_t s = 0; s < HOME_SLOTS; s++) {
    if (!pet.home(s).isEgg() || pet.home(s).isVacant() || yardEggX[s] < 0) continue;
    int c = yardCellAt(yardEggX[s], yardEggY[s] - YBG_CELL / 2);
    if (c >= 0) w[c >> 3] |= (uint8_t)(1 << (c & 7));
  }
  int c = enc ? yardCellAt(yardEncX, yardEncY - YBG_CELL / 2) : -1;
  if (c >= 0) w[c >> 3] |= (uint8_t)(1 << (c & 7));
  return w;
}

// la comida (Y3): ya se la ha comido. Ahora Pet la aplica, como antes al tocar la tarjeta: sus "+N", su
// sonido y, si le encanta o es la tarta, su corazon (si no, sale uno pequeno igual). v3.0: al de su hueco
void yardFoodEaten(uint8_t kind, int owner) {
  if (owner < 0) return;
  bool ok = false;
  pet.onSlot((uint8_t)owner, [&] {
    FxNeedSnap ns = fxSnap();
    ok = pet.feedFood(kind);  // gasta el objeto aqui y solo si se aplica (sin tarta/baya ya, p. ej. gastada desde la mochila: nada)
    if (ok) fxNeeds(ns);
  });
  Serial.printf("FOOD eaten kind=%u slot=%d ok=%d\n", kind, owner, ok ? 1 : 0);
  if (!ok) return;
  sfxPlay(SFX_EAT);
  yardHeartPopAt = millis();
  yardHeartSlot = (uint8_t)owner;
}

// lo que tiene quieto al bicho del hueco s (YH_*), sin contar la comida
uint8_t yardHoldOf(uint8_t s, uint32_t now) {
  const PetLife &l = pet.home(s);
  PetMood m = l.mood();
  if (l.sleeping) return YH_SLEEP;
  if (l.evolving() || fxHatchHidesPetSlot(s, now) || l.emote() != EMOTE_NONE ||
      (m == MOOD_EATING && homeMon[s].has(PMD_EAT)))
    return YH_STAY;
  return YH_NONE;
}

// un paso del planificador: lo que pide el juego (dormir, comer, triste, el menu del bicho...) y la inclinacion
void yardTick(uint32_t now) {
  uint32_t dt = yardLastStep ? now - yardLastStep : 100;
  uint32_t cap = frameIntervalMs(powerCtx(now)) + 100;  // otra pantalla de por medio no es un paso largo
  if (dt > cap) {
    for (YardFood &f : yardFood) f.t0 += dt - cap;  // fuera del patio la comida espera, en la misma fase
    dt = cap;
  }
  yardLastStep = now;
  // la comida (Y3; v3.3 una por hueco, la del menu del bicho): si su bicho ya no puede comer, o el hueco ya es de
  // otro, desaparece sin gastarse nada (aun no se aplico)
  bool foodOn[HOME_SLOTS], foodWaits[HOME_SLOTS] = { false, false };
  for (uint8_t s = 0; s < HOME_SLOTS; s++) {
    YardFood &f = yardFood[s];
    if (f.phase != YF_NONE) {
      const PetLife &l = pet.home(s);
      if (pet.slotOfUid(f.tag) != (int8_t)s || !yardW.p[s].on || l.sleeping || l.isEgg() || l.busyEnding()) {
        Serial.printf("FOOD cancelled slot=%u\n", s);
        f.phase = YF_NONE;
      }
    }
    foodOn[s] = f.phase != YF_NONE;
  }
  const uint8_t *walk = yardWalkNow();
  int colX = petMenuX(5);  // v3.3: la columna mas a la izquierda que puede ocupar el menu del bicho
  // v3.3: con el menu de un bicho abierto, ni los paseos al azar ni la inclinacion llevan a nadie debajo de el
  yardW.avoidX = petMenuOpen ? (int16_t)(colX - 32) : (int16_t)32767;
  uint8_t hold[YARD_PETS_MAX] = { YH_NONE, YH_NONE, YH_NONE };
  for (uint8_t s = 0; s < HOME_SLOTS; s++) {
    hold[s] = yardHoldOf(s, now);
    if (yardFood[s].phase == YF_EAT) {
      foodWaits[s] = hold[s] != YH_NONE;  // otra cosa lo tiene quieto: la comida espera
      if (!foodWaits[s]) hold[s] = yardFoodHold(yardFood[s]);
    } else if (hold[s] == YH_NONE) {
      // v2.6: triste (una necesidad baja, p. ej. hambre) se queda quieto con su pose de dolor, pero eso no para la
      // comida que espera ni (v3.3) apartarse de debajo del menu: yardMenuHold decide sobre esa base
      uint8_t base = !foodOn[s] && pet.home(s).mood() == MOOD_SAD && homeMon[s].has(PMD_HURT) ? YH_STAY : YH_NONE;
      hold[s] = (petMenuOpen && yardW.p[s].on)
                    ? yardMenuHold(yardW, walk, s, base, petMenuOpen == s + 1, yardBodyOf(s).w, colX, yardAside[s], now)
                    : base;
    }
  }
  hold[YARD_TRV] = YH_NONE;  // el viajero espera quieto (no pasea)
  bool still = pet.home(0).emote() != EMOTE_NONE || pet.home(1).emote() != EMOTE_NONE;
  int8_t tilt = (imuPresent() && !still && !foodOn[0] && !foodOn[1]) ? imuTiltSide() : 0;
  yardStep(yardW, walk, now, dt, hold, tilt);
  for (uint8_t s = 0; s < HOME_SLOTS; s++) {
    YardFood &f = yardFood[s];
    if (!foodOn[s]) continue;
    if (f.phase == YF_QUEUED) {  // v3.3: la comida elegida espera a que se aparte del menu; despues, delante
      YardPet &p = yardW.p[s];
      bool under = petMenuOpen && yardUnderMenu(p.x, yardBodyOf(s).w, colX);
      if (yardFoodServe(f, p, walk, yardFoodReady(f, p, under, now), now))
        Serial.printf("FOOD front kind=%u slot=%u at %d,%d\n", f.kind, s, f.x, f.y);
      continue;  // (el bocado empieza en el paso siguiente)
    }
    if (!foodWaits[s]) {
      uint8_t kind = f.kind;
      if (yardFoodStep(f, yardW, s, now)) yardFoodEaten(kind, s);
    } else {
      f.t0 += dt;  // su fase no avanza mientras tanto
    }
  }
}

// sombra: el suelo de debajo, oscurecido (un 25 %) en una elipse
void yardShadow(int cx, int cy, int w) {
  int rx = w * 2 / 5, ry;
  if (rx < 10) rx = 10;
  ry = rx / 3 < 4 ? 4 : rx / 3;
  uint16_t *fb = gfx->getFramebuffer();
  for (int dy = -ry; dy <= ry; dy++) {
    int y = cy + dy;
    if (y < 0 || y >= YBG_CANVAS) continue;
    int span = (int)(rx * sqrtf(1.0f - (float)(dy * dy) / (ry * ry)));
    for (int x = cx - span; x <= cx + span; x++) {
      if (x < 0 || x >= YBG_CANVAS) continue;
      uint16_t &c = fb[y * YBG_CANVAS + x];
      c = (uint16_t)(((c >> 1) & 0x7BEF) + ((c >> 2) & 0x39E7));
    }
  }
}

// sprite del paseo en el patio (Y3): de espaldas / de frente si va mas en vertical, de lado si no
// (o si el .bin de la SD es viejo y no trae WalkU/WalkD). La casa de lado y el combate siguen con L/R
uint8_t yardWalkAct(const PmdMon &m, const YardPet &p) {
  float dx, dy;
  yardHeading(p, &dx, &dy);
  static const uint8_t ACT[] = { PMD_WALKL, PMD_WALKR, PMD_WALKU, PMD_WALKD };  // por YW_*
  return ACT[yardWalkDir(dx, dy, p.face, m.has(PMD_WALKU), m.has(PMD_WALKD))];
}

void yardDrawPet(int i, uint32_t now) {
  YardPet &p = yardW.p[i];
  YardBody b = yardBodyOf(i);
  bool home = i < HOME_SLOTS;
  const PetLife *l = home ? &pet.home((uint8_t)i) : nullptr;
  int x = (int)p.x + (home ? dizzyWobbleOf(*l, now) : 0), y = (int)p.y;
  yardShadow(x, y, b.w);
  if (home && petMenuOpen == i + 1) {  // v3.3: el que tiene el menu abierto, con un anillo a sus pies
    int rx = b.w / 2 + 6, ry;
    if (rx < 24) rx = 24;
    ry = rx / 3 < 8 ? 8 : rx / 3;
    gfx->drawEllipse(x, y, rx, ry, Y_TEXT);
    gfx->drawEllipse(x, y, rx + 1, ry + 1, Y_TEXT);
    gfx->drawEllipse(x, y, rx + 2, ry + 1, Y_SHADOW);
  }
  if (b.kind == YB_PMD) {
    PmdMon &m = yardMonOf(i);
    uint8_t act = PMD_IDLE;
    bool loop = true;
    if (p.mode == YM_SLEEP) act = PMD_SLEEP;
    else if (p.mode == YM_STAY && home) {
      PetMood md = l->mood();
      act = md == MOOD_EATING ? PMD_EAT : md == MOOD_SAD ? PMD_HURT : PMD_IDLE;
      if (yardFood[i].phase == YF_EAT) act = PMD_EAT;  // Y3: el bocado a su comida
    } else if (p.mode == YM_WALK) act = yardWalkAct(m, p);
    else if (p.mode == YM_GESTURE) {
      act = p.act;
      loop = false;
    }
    if (!m.has(act)) act = PMD_IDLE;
    drawPmdActF(m, act, x, y, now - p.t0, loop || act == PMD_IDLE, false, b.s * YARD_PET_SCALE10);
  } else if (b.kind == YB_TPK1) {
    int s = b.s, w = mon.w * s;
    uint16_t fm = mon.frameMs ? mon.frameMs : 100;
    uint16_t fi = pet.sleeping ? 0 : (now / fm) % mon.frames;
    const uint8_t *fr = mon.data + (uint32_t)fi * mon.w * mon.h;
    int x0 = x - w / 2, y0 = y - mon.h * s;
    for (int r = 0; r < mon.h; r++) {
      const uint8_t *row = fr + r * mon.w;
      forRuns(mon.w, [&](int c) { return indexedKey(row[c], false); },
              [&](int c0, int len, int k) { gfx->fillRect(x0 + c0 * s, y0 + r * s, len * s, s, mon.pal[k]); });
    }
  } else if (b.kind == YB_FLASH) {
    const Species &sp = SPECIES[flashIdxForDex(l ? l->speciesId : yardBoxDex)];
    drawMap(sp.sprite, SPRITE_H, x - 32, y - 62, 2, false);
  } else if (b.kind == YB_THUMB) {
    drawThumbFit(thumbs.get(home ? l->speciesId : yardBoxDex), x, y - b.h / 2, 80, 80, false, 0);
  } else {  // sin sprite ninguno
    yShadowText(x - 9, y - 40, "?", 4, Y_TEXT);
  }
  if (!home) {
    if (yardBoxSlot >= 0 && trvBackSlot() == yardBoxSlot) {  // v2.1: de vuelta del viaje: su maleta y un "!"
      drawMap(SPR_ICON_TRAVEL, 16, x + b.w / 2 - 4, y - 30, 2, false);
      drawExclaim(x + b.w / 2 + 14, y - b.h - 12 + (int)(3 * sinf(now * 0.012f)));
    }
    return;
  }
  int head = y - b.h;
  if (l->showHeart()) drawMap(SPR_HEART, 32, x + b.w / 2 - 6, head - 18, 1, false);
  // v3.3: puede evolucionar o quiere despedirse: un "!" sobre la cabeza (lo hace su menu, en una fila resaltada)
  if ((l->wantEvolveButton() || l->wantFarewellButton()) && !l->showHeart() && petMenuOpen != i + 1)  // (su menu ya lo dice)
    drawExclaim(x + b.w / 2 + 14, head - 12 + (int)(3 * sinf(now * 0.012f)));
  uint8_t e = l->emote();
  if (e == EMOTE_ANGRY) drawAngerMark(x + b.w / 2 + 6, head + 4);
  else if (e == EMOTE_DIZZY) drawDizzyStars(x, head - 8, now);
}

// ---- la comida y el huevo (fase Y3) ----
// la comida del hueco s (v3.3, la del menu del bicho): aparece delante de el y en el bocado se encoge y se va
void yardDrawFood(uint8_t slot, uint32_t now) {
  const YardFood &f = yardFood[slot];
  if (f.phase == YF_NONE) return;
  uint32_t el = now - f.t0;
  if (f.phase != YF_EAT) return;  // (la que espera a que se aparte aun no esta en el suelo)
  int s = 2;
  float t = (float)el / YARD_FOOD_EAT_MS;
  if (t > 0.75f) return;  // ya se la comio
  if (t > 0.4f) s = 1;     // a bocados: mas pequena
  yardShadow(f.x, f.y, 26);
  drawMap(FOOD_ICON[f.kind < FOOD_COUNT ? f.kind : 0], 16, f.x - 8 * s, f.y - 15 * s, s, false);
}

// lo de encima del bicho por la comida: el corazon que sale al acabarsela (sobre el que se la comio)
void yardDrawFoodFx(uint32_t now) {
  uint8_t hs = yardHeartSlot & 1;
  uint32_t pop = now - yardHeartPopAt;
  if (yardHeartPopAt && pop < 1000 && yardW.p[hs].on && !pet.home(hs).showHeart()) {  // (le encanta o la tarta: ya sale el suyo)
    YardPet &p = yardW.p[hs];
    int head = yardPetHeadY(hs), w = yardBodyOf(hs).w;
    int y = head + 4 - (int)(pop * 30 / 1000);  // al lado de la cabeza (encima va el globo "Nam nam!")
    drawMap(SPR_HEART, 32, (int)p.x + w / 2 + 2, y, 1, false);
  }
}

// el huevo del hueco s en el patio: tambalea al sacudir la placa (como en su pantalla) o al tocarlo; un
// destello si es raro. Mientras se rompe lo dibuja drawHatchFx
void yardDrawEgg(uint8_t s, uint32_t now) {
  const PetLife &l = pet.home(s);
  if (!l.isEgg() || l.isVacant() || yardEggX[s] < 0) return;
  int dx = 0;
  if (l.emote() == EMOTE_WOBBLE) dx = (int)(10.0f * (1.0f - l.emoteT()) * sinf(now * 0.035f));
  uint32_t te = now - yardEggTapAt[s];
  if (yardEggTapAt[s] && te < 600) dx += (int)(6.0f * (1.0f - te / 600.0f) * sinf(te * 0.05f));
  yardShadow(yardEggX[s], yardEggY[s], 54);
  drawEggAt(yardEggX[s] + dx, yardEggY[s] - YARD_EGG_MID, YARD_EGG_S, pet.eggCracksOf(l), l.eggSpecies());
  if (l.eggRarity() >= R_RARO && (now / 400) % 3 != 0) {  // raro o legendario: brilla
    int k = (int)((now / 1200) % 2);
    drawMap(SPR_STAR, 16, yardEggX[s] + 22 + dx - k * 50, yardEggY[s] - 80 + k * 26, 1, false);
  }
}

// el globo del huevo del hueco s: el inicial (a toques) dice lo de siempre; el del patio, lo que falta un
// rato tras un toque o una sacudida, alternando con la pista de agitarlo (mientras las sacudidas aun cuenten)
void yardDrawEggBubble(uint8_t s, uint32_t now) {
  const PetLife &l = pet.home(s);
  if (!l.isEgg() || l.isVacant() || yardEggX[s] < 0 || fxHatchHidesPetSlot(s, now)) return;
  int tip = yardEggY[s] - YARD_EGG_MID * 2 - 8;
  if (!pet.yardEggOf(l)) {
    yardBubbleAt(yardEggX[s], tip, eggMsg());
    return;
  }
  if (!timeLeft(yardEggInfoUntil[s])) return;
  char left[12];
  eggLeftText(left, sizeof(left), pet.eggMinutesLeftOf(l));
  const char *opts[3] = { left, nullptr, nullptr };
  int n = 1;
  if (l.eggSavedMinutes() < EGG_SHAKE_CAP) opts[n++] = T(S_EGG_SHAKE);
  if (l.eggIsCaptured()) opts[n++] = T(S_EGG_CAPTURED);  // v2.2: el de una captura (ya se sabe que es)
  else if (l.eggRarity() >= R_RARO) opts[n++] = T(l.eggRarity() == R_LEGENDARIO ? S_EGG_LEGEND : S_EGG_RARE);
  uint32_t since = 2600 - timeLeft(yardEggInfoUntil[s]);
  yardBubbleAt(yardEggX[s], tip, opts[yardCycleIndex(n, since, 900)]);
}

// lo del suelo que se ordena con los bichos (el huevo y la comida de cada hueco): su y, o INT32_MAX si no hay
static int32_t yardObjY(int k) {
  if (k < HOME_SLOTS) {
    const PetLife &l = pet.home(k);
    return (l.isEgg() && !l.isVacant() && yardEggX[k] >= 0) ? yardEggY[k] : INT32_MAX;
  }
  const YardFood &f = yardFood[k - HOME_SLOTS];
  return f.phase != YF_NONE ? f.y : INT32_MAX;
}
static void yardDrawObj(int k, uint32_t now) {
  if (k < HOME_SLOTS) yardDrawEgg((uint8_t)k, now);
  else yardDrawFood((uint8_t)(k - HOME_SLOTS), now);
}
#define YARD_OBJS (2 * HOME_SLOTS)

void yardDrawPets(uint32_t now) {
  uint8_t order[YARD_PETS_MAX];
  int n = yardDrawOrder(yardW, order);
  bool hatching[HOME_SLOTS];
  for (uint8_t s = 0; s < HOME_SLOTS; s++) {
    // el huevo se rompe donde estaba (Y3); sin sitio, donde su bicho
    if (yardEggX[s] >= 0) hatchAtSlot(s, yardEggX[s], yardEggY[s] - YARD_EGG_MID, YARD_EGG_S);
    else if (yardW.p[s].on) hatchAtSlot(s, (int)yardW.p[s].x, (int)yardW.p[s].y - YARD_EGG_MID, YARD_EGG_S);
    else hatchAtSlot(s, CX, PET_CY, 5);
    hatching[s] = drawHatchFxSlot(s, now);  // el huevo aun se rompe: su bicho no se ve
  }
  bool objDone[YARD_OBJS];
  int32_t objY[YARD_OBJS];
  for (int k = 0; k < YARD_OBJS; k++) {
    objY[k] = yardObjY(k);
    objDone[k] = objY[k] == INT32_MAX;
  }
  bool encDone = yardEncX < 0;  // v2.2: lo que se mueve tambien se ordena con los bichos
  for (int j = 0; j < n; j++) {
    int i = order[j];
    for (int k = 0; k < YARD_OBJS; k++)
      if (!objDone[k] && yardW.p[i].y > objY[k]) {  // lo del suelo que queda detras de este bicho
        yardDrawObj(k, now);
        objDone[k] = true;
      }
    if (!encDone && yardW.p[i].y > yardEncY) {
      yardDrawEnc(now);
      encDone = true;
    }
    if (i < HOME_SLOTS && (hatching[i] || pet.home(i).evolving())) continue;
    yardDrawPet(i, now);
  }
  for (int k = 0; k < YARD_OBJS; k++)
    if (!objDone[k]) yardDrawObj(k, now);
  if (!encDone) yardDrawEnc(now);
  if (pet.evolving()) drawEvolveFX(now);  // la evolucion (siempre la del foco), grande en el centro como siempre
}

// bicho bajo el dedo (el de encima primero): 0 / 1 los de casa, YARD_TRV el viajero, -1 ninguno
int yardPetAt(int16_t x, int16_t y) {
  uint8_t order[YARD_PETS_MAX];
  int n = yardDrawOrder(yardW, order);
  for (int k = n - 1; k >= 0; k--) {
    int i = order[k];
    YardBody b = yardBodyOf(i);
    if (yardPetHit(x, y, yardW.p[i].x, yardW.p[i].y, b.w, b.h)) return i;
  }
  return -1;
}

// ---- las cosas del suelo: cacas y regalos ----
// Las cacas guardadas siguen siendo un numero (y sus huecos, poopSlots) de cada bicho; su sitio en el
// patio solo vive en RAM: una nueva cae junto a los pies de su bicho, y al arrancar se reparten al azar
int16_t yardPoopX[HOME_SLOTS][POOP_MAX], yardPoopY[HOME_SLOTS][POOP_MAX];
uint8_t yardPoopHave[HOME_SLOTS] = { 0, 0 };  // huecos con sitio
bool yardPoopBooted = false;
#define YARD_GIFTS_SHOWN 3
int16_t yardGiftX[YARD_GIFTS_SHOWN], yardGiftY[YARD_GIFTS_SHOWN];
uint8_t yardGiftN = 0;
int8_t yardGiftTapped = -1;  // el que se toco: es el que desaparece al abrirse

// lo que ya hay en el suelo (para no amontonar); skip = la caca (hueco * POOP_MAX + i) que se esta colocando
static int yardThings(int16_t *ox, int16_t *oy, int skip) {
  int n = 0;
  for (int s = 0; s < HOME_SLOTS; s++)
    for (int i = 0; i < POOP_MAX; i++)
      if ((yardPoopHave[s] & (1 << i)) && s * POOP_MAX + i != skip) {
        ox[n] = yardPoopX[s][i];
        oy[n++] = yardPoopY[s][i];
      }
  for (int i = 0; i < yardGiftN; i++) {
    ox[n] = yardGiftX[i];
    oy[n++] = yardGiftY[i];
  }
  if (yardEncX >= 0) {  // v2.2
    ox[n] = yardEncX;
    oy[n++] = yardEncY;
  }
  return n;
}
#define YARD_THINGS_MAX (HOME_SLOTS * POOP_MAX + YARD_GIFTS_SHOWN + 1)

void yardSyncThings() {
  int16_t ox[YARD_THINGS_MAX + HOME_SLOTS], oy[YARD_THINGS_MAX + HOME_SLOTS];
  for (uint8_t s = 0; s < HOME_SLOTS; s++) {
    uint8_t m = pet.home(s).poopSlots();
    for (int i = 0; i < POOP_MAX; i++) {
      uint8_t bit = 1 << i;
      if (!(m & bit)) {
        yardPoopHave[s] &= ~bit;
        continue;
      }
      bool have = yardPoopHave[s] & bit;
      if (have && yardSpotOk(yardWalk, yardPoopX[s][i], yardPoopY[s][i])) continue;
      yardPoopHave[s] &= ~bit;
      int n = yardThings(ox, oy, s * POOP_MAX + i), x, y;
      bool ok = (yardPoopBooted && !have && yardW.p[s].on)
                    ? yardDropSpot(yardWalk, yardW.p[s].x, yardW.p[s].y, ox, oy, n, &x, &y)  // recien hecha: a sus pies
                    : yardFreeSpot(yardW, yardWalk, yardW.rng, ox, oy, n, &x, &y);        // al arrancar o con otro mapa
      if (!ok) continue;
      yardPoopX[s][i] = (int16_t)x;
      yardPoopY[s][i] = (int16_t)y;
      yardPoopHave[s] |= bit;
    }
  }
  yardPoopBooted = true;
  // el huevo de cada hueco (Y3): en una celda libre cerca del centro (donde nacera el bicho), el segundo a
  // un lado. Su sitio se queda hasta que acaba de romperse: el bicho sale de ahi
  for (uint8_t s = 0; s < HOME_SLOTS; s++) {
    const PetLife &l = pet.home(s);
    if (l.isEgg() && !l.isVacant()) {
      if (yardEggX[s] < 0 || !yardSpotOk(yardWalk, yardEggX[s], yardEggY[s] - YBG_CELL / 2)) {
        int c = yardNearestFreeCell(yardWalk, CX + (s ? 110 : 0), 250);
        int o = s ^ 1;  // que no caiga en la celda del otro huevo
        if (c >= 0 && yardEggX[o] >= 0 && yardCellAt(yardEggX[o], yardEggY[o] - 10) == c)
          c = yardNearestFreeCell(yardWalk, CX + (s ? 170 : -60), 250);
        if (c >= 0) {
          yardEggX[s] = (int16_t)yardCellCenter(yardCellCX(c));
          yardEggY[s] = (int16_t)(yardCellCenter(yardCellCY(c)) + 10);
        }
      }
    } else if (!fxo.find(FXO_HATCH, -1, s) && yardEggX[s] >= 0) {
      yardEggX[s] = -1;
    }
  }
  uint8_t want = pet.giftCount() < YARD_GIFTS_SHOWN ? pet.giftCount() : YARD_GIFTS_SHOWN;
  while (yardGiftN > want) {  // se abrio uno: se va el tocado
    int k = (yardGiftTapped >= 0 && yardGiftTapped < yardGiftN) ? yardGiftTapped : yardGiftN - 1;
    for (int j = k; j + 1 < yardGiftN; j++) {
      yardGiftX[j] = yardGiftX[j + 1];
      yardGiftY[j] = yardGiftY[j + 1];
    }
    yardGiftN--;
    yardGiftTapped = -1;
  }
  for (int i = 0; i < yardGiftN; i++)  // otro mapa: el que quede en algo bloqueado se mueve
    if (!yardSpotOk(yardWalk, yardGiftX[i], yardGiftY[i])) {
      for (int j = i; j + 1 < yardGiftN; j++) {
        yardGiftX[j] = yardGiftX[j + 1];
        yardGiftY[j] = yardGiftY[j + 1];
      }
      yardGiftN--;
      i--;
    }
  while (yardGiftN < want) {
    int n = yardThings(ox, oy, -1), x, y;
    if (!yardFreeSpot(yardW, yardWalk, yardW.rng, ox, oy, n, &x, &y)) break;
    yardGiftX[yardGiftN] = (int16_t)x;
    yardGiftY[yardGiftN++] = (int16_t)y;
  }
  // v2.2: el encuentro salvaje, en un sitio libre (lejos de los huevos, que estan cerca del centro); se va con el
  if (!pet.encShown()) {
    yardEncX = -1;
  } else if (yardEncX < 0 || !yardSpotOk(yardWalk, yardEncX, yardEncY)) {
    yardEncX = -1;
    int n = yardThings(ox, oy, -1), x, y;
    for (uint8_t s = 0; s < HOME_SLOTS; s++)
      if (pet.home(s).isEgg() && yardEggX[s] >= 0) {
        ox[n] = yardEggX[s];
        oy[n++] = yardEggY[s];
      }
    // es ancho (64 px): que tampoco pise agua ni rocas a los lados ni detras (si no hay sitio asi, donde sea)
    for (int tries = 0; tries < 16; tries++) {
      if (!yardFreeSpot(yardW, yardWalk, yardW.rng, ox, oy, n, &x, &y)) break;
      bool roomy = yardSpotOk(yardWalk, x - 30, y - 4) && yardSpotOk(yardWalk, x + 30, y - 4) &&
                   yardSpotOk(yardWalk, x, y - 44) && yardSpotOk(yardWalk, x - 24, y - 40) && yardSpotOk(yardWalk, x + 24, y - 40);
      if (roomy || yardEncX < 0) {
        yardEncX = (int16_t)x;
        yardEncY = (int16_t)y;
      }
      if (roomy) break;
    }
  }
}

// ---- el encuentro salvaje (v2.2): un arbusto, una roca o un monton de arena (segun el mapa) que se sacude
// cada poco con dos rayas rojas curvas a cada lado; el de una especie exclusiva, ademas, brilla ----
#define ENC_RUSTLE_EVERY 2400
#define ENC_RUSTLE_MS 800
#define ENC_MARK_COL C565(0xe8, 0x2c, 0x2c)

// una raya curva: un arco de circulo de radio r cuyo punto medio esta en (ax, ay), abombado hacia midDeg
static void encArc(int ax, int ay, int r, float midDeg, uint16_t col) {
  float m = midDeg * (float)PI / 180.0f;
  float cx = ax - r * cosf(m), cy = ay - r * sinf(m);
  for (int d = -32; d <= 32; d += 4) {
    float a = m + d * (float)PI / 180.0f;
    gfx->fillCircle((int)(cx + r * cosf(a)), (int)(cy + r * sinf(a)), 2, col);
  }
}

void yardDrawEnc(uint32_t now) {
  if (yardEncX < 0) return;
  uint8_t kind = encMapObject(yardMapNow());
  const char *const *spr = kind == ENC_OBJ_ROCK ? SPR_WILD_ROCK : kind == ENC_OBJ_SAND ? SPR_WILD_SAND : SPR_WILD_BUSH;
  bool live = pet.encActive();
  uint32_t ph = now % ENC_RUSTLE_EVERY;
  bool rustle = live && ph < ENC_RUSTLE_MS;
  int dx = 0, dy = 0;
  if (rustle) {
    float k = 1.0f - (float)ph / ENC_RUSTLE_MS;
    dx = (int)lroundf(5.0f * k * sinf(ph * 0.07f));
    dy = ((ph / 70) % 2) ? -2 : 0;
  }
  int x = yardEncX, y = yardEncY;
  yardShadow(x, y, 64);
  drawMap(spr, 32, x - 32 + dx, y - 62 + dy, 2, false);
  if (rustle && ph > 60) {  // ( (   ) ): las rayas, la de fuera un poco despues
    encArc(x - 44, y - 40, 14, 200, ENC_MARK_COL);
    encArc(x + 44, y - 40, 14, -20, ENC_MARK_COL);
    if (ph > 220) {
      encArc(x - 56, y - 50, 20, 205, ENC_MARK_COL);
      encArc(x + 56, y - 50, 20, -25, ENC_MARK_COL);
    }
  }
  if (pet.encExclusive()) {  // brilla: cuatro destellos dorados y blancos que giran alrededor y laten
    for (int k = 0; k < 4; k++) {
      float a = now * 0.0022f + k * 1.571f;
      int sx = x + (int)(42 * cosf(a)), sy = y - 34 + (int)(30 * sinf(a));
      int sz = 4 + (int)((now / 100 + k * 3) % 5);
      uint16_t col = (k & 1) ? UI_BAR_WARN : UI_WHITE;
      gfx->fillRect(sx - sz, sy - 1, 2 * sz + 1, 3, col);
      gfx->fillRect(sx - 1, sy - sz, 3, 2 * sz + 1, col);
      gfx->fillRect(sx - 1, sy - 1, 3, 3, UI_WHITE);
    }
    if ((now / 300) % 4 == 0) drawMap(SPR_STAR, 16, x - 8, y - 92, 1, false);  // y una estrella encima, a ratos
  }
  if (pet.encWonPending()) drawExclaim(x + 30, y - 78 + (int)(3 * sinf(now * 0.012f)));  // vencido: falta la Poke Ball
}

bool yardEncAt(int16_t x, int16_t y) {
  return yardEncX >= 0 && abs(x - yardEncX) < 46 && y > yardEncY - 80 && y < yardEncY + 16;
}

void yardDrawThings(uint32_t now) {
  // SPR_POOP (32x32) a escala 2, como en la casa de lado: la tinta ~32x40 con los pies del monton en y
  for (int s = 0; s < HOME_SLOTS; s++)
    for (int i = 0; i < POOP_MAX; i++)
      if (yardPoopHave[s] & (1 << i)) drawMap(SPR_POOP, 32, yardPoopX[s][i] - 34, yardPoopY[s][i] - 58, 2, false);
  uint32_t left = timeLeft(poopPuffUntil);
  if (left && poopPuffSlot >= 0) {  // la que se acaba de limpiar: destellos donde estaba
    int cx = yardPoopX[poopPuffHome & 1][poopPuffSlot], cy = yardPoopY[poopPuffHome & 1][poopPuffSlot] - 20;
    int r = 4 + (int)((POOP_PUFF_MS - left) / 30);
    for (int k = 0; k < 4; k++) {
      int sx = cx + ((k & 1) ? r : -r), sy = cy + ((k & 2) ? r : -r);
      uint16_t col = (k % 2) ? UI_BAR_WARN : UI_WHITE;
      gfx->fillRect(sx - 4, sy - 1, 9, 3, col);
      gfx->fillRect(sx - 1, sy - 4, 3, 9, col);
    }
  }
  for (int i = 0; i < yardGiftN; i++) {
    int bob = (int)lroundf(2.0f * sinf(now * 0.004f + i * 2.1f));
    yardShadow(yardGiftX[i], yardGiftY[i], 30);
    drawMap(SPR_GIFT, 16, yardGiftX[i] - 16, yardGiftY[i] - 30 + bob, 2, false);
  }
  if (pet.giftCount() > YARD_GIFTS_SHOWN && yardGiftN) {
    char t[6];
    snprintf(t, sizeof(t), "+%u", (unsigned)(pet.giftCount() - YARD_GIFTS_SHOWN));
    int k = yardGiftN - 1;
    yShadowText(yardGiftX[k] + 18, yardGiftY[k] - 30, t, 2, Y_TEXT);
  }
}

#define YARD_THING_HIT_R 28
// la caca bajo el dedo: hueco de casa * POOP_MAX + su sitio, o -1
int yardPoopAt(int16_t x, int16_t y) {
  for (int s = 0; s < HOME_SLOTS; s++)
    for (int i = 0; i < POOP_MAX; i++) {
      if (!(yardPoopHave[s] & (1 << i))) continue;
      int dx = x - yardPoopX[s][i], dy = y - (yardPoopY[s][i] - 20);
      if (dx * dx + dy * dy <= YARD_THING_HIT_R * YARD_THING_HIT_R) return s * POOP_MAX + i;
    }
  return -1;
}
// el huevo bajo el dedo: su hueco de casa, o -1
int yardEggAt(int16_t x, int16_t y) {
  for (int s = 0; s < HOME_SLOTS; s++) {
    const PetLife &l = pet.home(s);
    if (!l.isEgg() || l.isVacant() || yardEggX[s] < 0) continue;
    int dx = x - yardEggX[s], dy = y - (yardEggY[s] - YARD_EGG_MID);
    if (dx * dx + dy * dy <= 46 * 46) return s;
  }
  return -1;
}
int yardGiftAt(int16_t x, int16_t y) {
  for (int i = 0; i < yardGiftN; i++) {
    int dx = x - yardGiftX[i], dy = y - (yardGiftY[i] - 14);
    if (dx * dx + dy * dy <= YARD_THING_HIT_R * YARD_THING_HIT_R) return i;
  }
  return -1;
}

// v3.3: la comida de la lista del menu (kind: FOOD_*, food.h) para el bicho del hueco s: aparece delante de el
// en cuanto termina de apartarse del menu (yardTick; casi siempre en el paso siguiente) y se la come (yard.h,
// yardFoodQueue / yardFoodServe). 1 servida, 0 ya tiene una, -1 no esta en el patio
int yardFeedFront(uint8_t s, uint8_t kind) {
  if (s >= HOME_SLOTS) return -1;
  if (yardFood[s].phase != YF_NONE) return 0;
  const PetLife &l = pet.home(s);
  if (!yardW.p[s].on || !yardHomeNow() || !l.hatched()) return -1;
  yardFoodQueue(yardFood[s], kind, millis(), l.uid);
  Serial.printf("FOOD queued kind=%u slot=%u\n", kind, s);
  return 1;
}

// v3.3: cuantos objeto `it` estan ya elegidos para comer (en la lista del menu de algun bicho) y aun no se han
// gastado: no se pueden volver a dar (ni en la mochila)
uint8_t yardFoodReserved(uint8_t it) {
  uint8_t n = 0;
  for (const YardFood &f : yardFood)
    if (f.phase != YF_NONE && f.kind < FOOD_COUNT && foodItem(f.kind) == it) n++;
  return n;
}

// ---- el globo sobre cada bicho ----
// lo que pasa ahora mismo (uno solo, por orden de importancia), o nullptr. Los avisos de la UI (sin tarta,
// le gusta, revivido) son del foco
const char *yardEventMsg(uint8_t s) {
  const PetLife &l = pet.home(s);
  bool focus = s == pet.focusSlot;
  if (l.emote() == EMOTE_ANGRY) return T(S_GRUMPY);
  if (l.emote() == EMOTE_DIZZY) return T(S_DIZZY);
  if (l.sleeping) return "Zzz...";
  if (focus && timeLeft(revivedMsgUntil)) return T(S_REVIVED);
  if (l.eating()) return T(S_EATING);
  if (l.showHeart()) return T(S_LIKES);
  return nullptr;
}

// las necesidades bajas del hueco s, todas (el globo pasa de una a otra cada segundo)
int yardNeedList(uint8_t s, const char **out) {
  const PetLife &l = pet.home(s);
  int n = 0;
  if (l.neglected()) {  // abandono (v1.60): solo eso
    out[n++] = T(l.bond > 0 ? S_ABANDONED : S_WEAK);
    return n;
  }
  if (pet.sleepyOf(l)) out[n++] = T(S_SLEEPY);
  if (l.fullness < NEED_LOW) out[n++] = T(S_HUNGRY);
  if (l.energy < NEED_LOW) out[n++] = T(S_EXHAUSTED);
  if (l.joy < NEED_LOW) out[n++] = T(S_SAD);
  return n;
}

// globo blanco con la cola hacia (cx, tipY), dentro del circulo (como yBubble, que solo acota la x)
void yardBubbleAt(int cx, int tipY, const char *text) {
  setSize(2);
  int tw = textW(text, 2), w = tw + 20, h = 30;
  const int R = 214;  // radio util para el globo (el marco redondo y la orbita)
  if (tipY < 52) tipY = 52;
  int top, hc;
  for (;;) {  // baja hasta que el globo cabe a lo ancho a su altura
    top = tipY - 8 - h;
    int dy = top < Y_CY ? Y_CY - top : top + h - Y_CY;
    hc = dy < R ? (int)sqrtf((float)(R * R - dy * dy)) : 0;
    if (2 * hc >= w || tipY > 300) break;
    tipY += 4;
  }
  int x = cx - w / 2;
  if (x < Y_CX - hc) x = Y_CX - hc;
  if (x + w > Y_CX + hc) x = Y_CX + hc - w;
  if (petMenuOpen && x + w > petMenuX(5) - 6) x = petMenuX(5) - 6 - w;  // v3.3: a la izquierda del menu del bicho
  if (x < Y_CX - hc) x = Y_CX - hc;
  int tail = cx < x + 14 ? x + 14 : cx > x + w - 14 ? x + w - 14 : cx;
  gfx->fillRoundRect(x + 2, top + 2, w, h, 10, Y_SHADOW);
  gfx->fillRoundRect(x, top, w, h, 10, Y_TEXT);
  gfx->fillTriangle(tail - 7, top + h - 1, tail + 7, top + h - 1, tail, tipY, Y_TEXT);
  gfx->setTextColor(UI_INK);
  setCur(x + 10, top + (h - 14) / 2);
  printT(text);
}

const char *yardBubblePrev[HOME_SLOTS] = { nullptr, nullptr }, *yardBubbleLatched[HOME_SLOTS] = { nullptr, nullptr };
uint32_t yardBubbleUntil[HOME_SLOTS] = { 0, 0 };
// lo que dice el globo del hueco s ahora (nullptr: nada)
const char *yardBubbleText(uint8_t s, uint32_t now) {
  const char *ev = yardEventMsg(s);
  if (ev && ev != yardBubblePrev[s]) {  // un suceso nuevo: se queda al menos Y_BUBBLE_MS aunque acabe antes
    yardBubbleLatched[s] = ev;
    yardBubbleUntil[s] = now + Y_BUBBLE_MS;
  }
  yardBubblePrev[s] = ev;
  const char *txt = ev ? ev : (timeLeft(yardBubbleUntil[s]) ? yardBubbleLatched[s] : nullptr);
  if (!txt) {
    const char *needs[6];
    int n = yardNeedList(s, needs);
    int k = yardCycleIndex(n, now, Y_BUBBLE_CYCLE_MS);
    if (k >= 0) txt = needs[k];
  }
  return txt;
}

// v3.0: cada bicho (o huevo) con su globo. Si los dos tienen algo que decir y estan cerca, se turnan
void yardDrawBubble(uint32_t now) {
  const char *txt[HOME_SLOTS] = { nullptr, nullptr };
  for (uint8_t s = 0; s < HOME_SLOTS; s++) {
    const PetLife &l = pet.home(s);
    if (l.isEgg() || !yardW.p[s].on || l.evolving() || fxHatchHidesPetSlot(s, now)) continue;
    txt[s] = yardBubbleText(s, now);
  }
  bool both = txt[0] && txt[1] && fabsf(yardW.p[0].x - yardW.p[1].x) < 200 && fabsf(yardW.p[0].y - yardW.p[1].y) < 90;
  int turn = both ? (int)((now / (Y_BUBBLE_CYCLE_MS * 2)) % 2) : -1;
  for (uint8_t s = 0; s < HOME_SLOTS; s++) {
    if (pet.home(s).isEgg()) {  // Y3: el huevo en el patio
      yardDrawEggBubble(s, now);
      continue;
    }
    if (!txt[s] || (turn >= 0 && turn != s)) continue;
    yardBubbleAt((int)yardW.p[s].x, yardPetHeadY(s) - 4, txt[s]);
  }
}

// "LIM +10": lo que cambio una necesidad, subiendo sobre el bicho (el patio no tiene barras)
void yardStatPill(uint8_t slot, uint8_t a, int16_t v, float t) {
  char s[24];
  // v3.1: la barra a (0 comida, 1 felicidad, 2 energia) -> su etiqueta, con mapa explicito
  static const StrId BAR_STR[3] = { S_BAR_FOOD, S_BAR_JOY, S_BAR_ENE };
  snprintf(s, sizeof(s), v > 0 ? "%s +%d" : "%s %d", T(BAR_STR[a % 3]), v);
  setSize(2);
  int w = textW(s, 2) + 16;
  int cx = petFootXOf(slot), y = yardPetHeadY(slot & 1) - 70 - (a & 1) * 28 - (int)(t * 26);
  if (y < 44) y = 44;
  if (petMenuOpen && cx + w / 2 > petMenuX(5) - 6) cx = petMenuX(5) - 6 - w / 2;  // v3.3: sin pisar el menu
  if (cx - w / 2 < 40) cx = 40 + w / 2;
  if (cx + w / 2 > 426) cx = 426 - w / 2;
  gfx->fillRoundRect(cx - w / 2, y - 4, w, 24, 8, v > 0 ? Y_ACCENT_GO : Y_ACCENT_BAD);
  gfx->setTextColor(Y_TEXT);
  setCur(cx - w / 2 + 8, y);
  printT(s);
}

// ---- el HUD: bateria, hora y dock ----
// la bateria sobre el mapa: contorno doble claro con sombra (drawBattery es para el fondo crema)
void drawBatteryHud(int x, int y) {
  int pc = batPercent();
  if (pc < 0) return;
  const int w = 24, h = 12;
  bool charging = batCharging();
  uint16_t col = charging ? UI_BAR_OK : (pc >= 40) ? Y_TEXT : (pc >= 15) ? UI_BAR_WARN : UI_BAR_BAD;
  for (int k = 0; k < 2; k++) {  // la sombra y luego el dibujo
    int ox = k ? 0 : 2;
    uint16_t c = k ? col : Y_SHADOW;
    gfx->drawRoundRect(x + ox, y + ox, w, h, 3, c);
    gfx->drawRoundRect(x + ox + 1, y + ox + 1, w - 2, h - 2, 2, c);
    gfx->fillRect(x + ox + w, y + ox + 3, 3, h - 6, c);
  }
  if (charging) {
    uint16_t bolt = C565(0xff, 0xd9, 0x4a);
    int bx = x + w / 2;
    gfx->fillTriangle(bx + 3, y + 2, bx - 4, y + 7, bx + 1, y + 7, bolt);
    gfx->fillTriangle(bx - 1, y + 6, bx + 4, y + 6, bx - 3, y + 11, bolt);
  } else {
    int fw = (w - 6) * pc / 100;
    if (fw > 0) gfx->fillRect(x + 3, y + 3, fw, h - 6, col);
  }
}

// el dock (v3.3): Jugar, Mochila, Tienda. Cuidar se fue al menu de cada bicho (tocarlo en el patio)
enum : uint8_t { DOCK_PLAY = 0, DOCK_BAG, DOCK_SHOP };
// boton del dock -> boton de la fila vieja, para los efectos de pulsar y temblar (fx.h)
static int yardDockFx(int d) { return d == DOCK_PLAY ? 1 : d == DOCK_BAG ? BTN_BAG : BTN_SHOP; }
// v3.0: con que cualquiera de los dos de casa pueda. Jugar: alguno nacido y despierto, o bichos en la caja
// (las fases). La Mochila y la Tienda, siempre
static bool yardDockOff(int d) {
  bool anyAwake = false;
  for (uint8_t s = 0; s < HOME_SLOTS; s++) {
    const PetLife &l = pet.home(s);
    anyAwake |= l.hatched() && !l.sleeping;
  }
  return d == DOCK_PLAY && !anyAwake && !pet.boxHasPets();
}

#define YARD_COIN_Y 40   // la cartera, justo debajo de la bateria (v2.1: sin hora, chocaba con el aviso de vuelta)
#define YARD_BAT_Y 12
void yardDrawHud() {
  drawBatteryHud(Y_CX - BAT_ICON_W / 2, YARD_BAT_Y);
  yCoinBadge(Y_CX, YARD_COIN_Y, pet.coins, true);  // la cartera (Y4)
  YDockItem dock[Y_DOCK_N] = {
    { SPR_ICON_PLAY, Y_DOCK_PLAY, yardDockOff(DOCK_PLAY) },
    { SPR_ICON_BAG, Y_DOCK_BAG, yardDockOff(DOCK_BAG) },
    { SPR_ICON_SHOP, Y_DOCK_SHOP, yardDockOff(DOCK_SHOP) },
  };
  int pressed = -1;
  for (int d = 0; d < Y_DOCK_N; d++)
    if (fxo.find(FXO_PRESS, yardDockFx(d))) pressed = d;
  yDock(dock, pressed);
}

// ---- un frame de la casa (el fondo ya esta copiado) ----
void renderYardHome(uint32_t now) {
  yardSyncPets();
  yardTick(now);
  yardSyncThings();
  yardDrawThings(now);
  yardDrawPets(now);
  drawBoxAppear(now);       // recien guardado o despertado: sale de una luz
  yardDrawBubble(now);
  yardDrawFoodFx(now);     // Y3: el corazon al acabarse la comida
  yardDrawHud();
}

// ---- toques ----
// v3.3: con el menu de un bicho abierto, el toque es suyo (sus filas, el otro bicho o cerrarlo); si no: el dock,
// el huevo, lo que se mueve, las cacas, los regalos y los bichos (tocar uno abre su menu)
void yardTap(int16_t x, int16_t y) {
  if (petMenuOpen) {
    petMenuTap(x, y);
    return;
  }
  int d = yDockHit(x, y);
  if (d >= 0) {
    Serial.printf("DOCK %d\n", d);
    fxo.push(FXO_PRESS, yardDockFx(d), 0, millis());
    if (yardDockOff(d)) {
      sfxPlay(SFX_DENY);
      fxo.push(FXO_DENY, yardDockFx(d), 0, millis());
      return;
    }
    sfxPlay(SFX_TAP);
    if (d == DOCK_PLAY) openPlayPanel();
    else if (d == DOCK_BAG) openBag();
    else openShop();
    return;
  }
  int es = yardEggAt(x, y);
  if (es >= 0) {  // Y3: el huevo se tambalea; el del patio dice lo que falta, el inicial se abre a toques
    yardEggTapAt[es] = millis();
    sfxPlay(SFX_TAP);
    const PetLife &l = pet.home((uint8_t)es);
    if (pet.yardEggOf(l)) {
      yardEggInfoUntil[es] = millis() + 2600;
      Serial.printf("EGG tap slot=%d left=%u\n", es, pet.eggMinutesLeftOf(l));
    } else {
      pet.onSlot((uint8_t)es, [] { pet.eggTap(); });
    }
    return;
  }
  if (yardEncAt(x, y)) {  // v2.2: lo que se mueve: el encuentro salvaje
    Serial.println("ENC tap");
    openEncounter();
    return;
  }
  int pp = yardPoopAt(x, y);  // una caca: fuera con un toque (como en la casa de lado). v3.0: la de su bicho
  if (pp >= 0) {
    uint8_t ps = (uint8_t)(pp / POOP_MAX), pi = (uint8_t)(pp % POOP_MAX);
    bool ok = false;
    pet.onSlot(ps, [&] {
      FxNeedSnap ns = fxSnap();
      ok = pet.cleanPoop(pi);
      if (ok) fxNeeds(ns);
    });
    if (ok) {
      Serial.printf("POOP clean slot=%u %u\n", ps, pi);
      sfxPlay(SFX_TAP);
      poopPuffHome = ps;
      poopPuffSlot = (int8_t)pi;
      poopPuffUntil = millis() + POOP_PUFF_MS;
    } else {
      sfxPlay(SFX_DENY);
    }
    return;
  }
  int g = yardGiftAt(x, y);
  if (g >= 0) {
    yardGiftTapped = (int8_t)g;
    // v3.0: lo abre un bicho de casa despierto (el del foco, o si no el otro: el que salta de alegria)
    if (!pet.canOpenGift()) {
      const PetLife &o = pet.other;
      if (o.hatched() && !o.sleeping && !o.busyEnding()) uiFocus(pet.focusSlot ^ 1);
    }
    openGiftTap();
    return;
  }
  int who = yardPetAt(x, y);
  if (who != YARD_TRV && yardBoxSlot >= 0 && yardW.p[YARD_TRV].on && trvBackSlot() == yardBoxSlot) {  // el "!" del viajero tambien vale
    YardBody bb = yardBodyOf(YARD_TRV);
    int bx = (int)yardW.p[YARD_TRV].x + bb.w / 2 + 14, by = (int)yardW.p[YARD_TRV].y - bb.h - 12;
    if (abs(x - bx) < 34 && abs(y - by) < 34) who = YARD_TRV;
  }
  if (who >= 0 && who < HOME_SLOTS) {  // v3.3: uno de casa: pasa a tener el foco y se abre su menu
    if (!openPetMenu((uint8_t)who)) {
      sfxPlay(SFX_DENY);
      return;
    }
    sfxPlay(SFX_TAP);
  } else if (who == YARD_TRV && yardBoxSlot >= 0 && trvBackSlot() == yardBoxSlot) {  // v2.1: el viajero: lo que trajo
    Serial.println("PET travel result");
    returnOpen = true;
    returnOpenAt = millis();
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
  int x = CX, y = PET_GROUND;
  uint8_t act = PMD_IDLE;

  if (pet.ceremony == CER_DEATH) {
    // muerte por abandono (v1.60, en el sitio del final triste de la escapada): penumbra con lluvia,
    // tendido, se va volviendo silueta y se apaga. Si deja un huevo, aparece donde estaba
    drawSadRain(now);
    act = pmd.has(PMD_SLEEP) ? PMD_SLEEP : (pmd.has(PMD_HURT) ? PMD_HURT : PMD_IDLE);
    bool sil = t > 0.25f;
    bool gone = t > 0.85f || (t > 0.6f && (now / 160) % 2 == 0);
    if (!gone) drawPmdAct(act, x, y, now, true, sil, 5);
    if (pet.deathEggSpecies() && t > 0.7f) {
      drawEggAt(CX, PET_GROUND - 48, 3, 0, pet.deathEggSpecies());
      gfx->setTextColor(UI_WHITE);
      setSize(2);
      setCur(centerX(T(S_DIED_EGG), 2), 360);
      printT(T(S_DIED_EGG));
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

// lluvia de los finales tristes (la muerte por abandono y su dialogo, v1.60; antes la escapada)
void drawSadRain(uint32_t now) {
  for (int i = 0; i < 46; i++) {
    int rx = (i * 47 + now / 3) % 466;
    int ry = (i * 91 + now / 2) % 470;
    gfx->drawLine(rx, ry, rx - 3, ry + 12, C565(0x6a, 0x84, 0xb0));
  }
}

// ---------- muerte por abandono (v1.60) ----------
// Se cierra todo lo que tapara la pantalla principal: el dialogo del Revivir tiene que verse al volver
void closeForDying() {
  sackOpen = false;
  zenOpen = false;
  if (dashOpen) closeDash();
  if (battleOpen()) closeBattle();
  if (encOpen()) closeEncUi();
  gameOpen = false;
  if (galleryOpen) {
    galleryOpen = false;
    galleryDetail = 0;
    galleryPmd.unload();
  }
  if (boxOpen && boxMove == BOXMV_NONE) closeBox();
  closeStages();
  kbOpen = clockOpen = cardOpen = userOpen = false;
  closeBag();
  itemConfirm = 0;
  choiceKind = 0;
  confirmUntil = 0;
  panelKind = PANEL_NONE;
  closePetMenu();       // v3.3
  petPick = PICK_NONE;
  trvMapPrev = 0;
  boxAwayUntil = 0;
}

// dialogo: con Revivir, SI (revive) / NO (se va); sin ninguno, un solo boton (se va). El Revivir tiene
// una hora (v1.64): pasada, dice que murio y solo queda VALE (se va). Un toque fuera de los botones no
// hace nada: decide si el bicho sigue
// el cuadro va abajo, en el sitio de las barras y los botones: arriba se ve al bicho apagandose
#define DIE_BOX_X 94   // 278 x 120: a la altura de su borde de abajo (y 410) el circulo deja x 85..381
#define DIE_BOX_Y 290
#define DIE_BOX_W 278
#define DIE_BOX_H 120
#define DIE_YES_X 118
#define DIE_NO_X 248
#define DIE_OK_X 183
#define DIE_BTN_Y 352
#define DIE_BTN_W 100
#define DIE_BTN_H 44
void drawDyingScreen() {
  uint32_t now = millis();
  const DexEntry &d = DEX_TBL[pet.speciesId];
  const char *nm = pet.nick[0] ? pet.nick : dexName(pet.speciesId);
  uint8_t n = pet.revivesOwned();  // tambien los que esperan envueltos
  bool late = pet.reviveExpired();  // v1.64: paso la hora del Revivir
  char left[32];  // con Revivir y aun a tiempo, la cabecera dice cuanto queda; pasada la hora, nada
  if (late) left[0] = 0;
  else if (n) snprintf(left, sizeof(left), T(S_DYING_LEFT_FMT), (unsigned)pet.reviveMinutesLeft());
  else snprintf(left, sizeof(left), "%s", T(S_WEAK));
  drawHeader(nm, gNight ? UI_INK_NIGHT : d.accent, left);
  drawSadRain(now);
  if (pmd.loaded) drawPmdAct(pmd.has(PMD_SLEEP) ? PMD_SLEEP : PMD_IDLE, CX, PET_GROUND, now, true, true, 5);
  else drawPet();
  drawPoops();
  gfx->fillRect(0, 312, 466, 154, gNight ? UI_BG_NIGHT : UI_BG_DAY);

  gfx->fillRoundRect(DIE_BOX_X, DIE_BOX_Y, DIE_BOX_W, DIE_BOX_H, 16, UI_WHITE);
  gfx->drawRoundRect(DIE_BOX_X, DIE_BOX_Y, DIE_BOX_W, DIE_BOX_H, 16, UI_INK);
  char q[48];
  snprintf(q, sizeof(q), T(late ? S_DIED_FMT : S_DYING_FMT), nm);
  gfx->setTextColor(UI_INK);
  setSize(2);
  setCur(centerX(q, 2), DIE_BOX_Y + 14);
  printT(q);
  char a[48];
  if (late) snprintf(a, sizeof(a), "%s", T(S_DIED_LATE));
  else if (n) snprintf(a, sizeof(a), T(S_USE_REVIVE_FMT), (unsigned)n);
  else snprintf(a, sizeof(a), "%s", T(S_NO_REVIVES));
  gfx->setTextColor(n && !late ? UI_INK : BOX_NOTE_INK);
  setCur(centerX(a, 2), DIE_BOX_Y + 38);
  printT(a);
  if (n && !late) {
    gfx->fillRoundRect(DIE_YES_X, DIE_BTN_Y, DIE_BTN_W, DIE_BTN_H, 12, UI_BAR_OK);
    gfx->setTextColor(UI_WHITE);
    setCur(DIE_YES_X + (DIE_BTN_W - textW(T(S_YES), 2)) / 2, DIE_BTN_Y + 14);
    printT(T(S_YES));
    gfx->fillRoundRect(DIE_NO_X, DIE_BTN_Y, DIE_BTN_W, DIE_BTN_H, 12, UI_BAR_BAD);
    setCur(DIE_NO_X + (DIE_BTN_W - textW(T(S_NO), 2)) / 2, DIE_BTN_Y + 14);
    printT(T(S_NO));
  } else {
    gfx->fillRoundRect(DIE_OK_X, DIE_BTN_Y, DIE_BTN_W, DIE_BTN_H, 12, UI_TRACK);
    gfx->setTextColor(UI_INK);
    setCur(DIE_OK_X + (DIE_BTN_W - textW(T(S_OK), 2)) / 2, DIE_BTN_Y + 14);
    printT(T(S_OK));
  }
}

static bool inDieBtn(int16_t x, int16_t y, int bx) {
  return x >= bx && x <= bx + DIE_BTN_W && y >= DIE_BTN_Y && y <= DIE_BTN_Y + DIE_BTN_H;
}

void dyingTap(int16_t x, int16_t y) {
  if (pet.revivesOwned() && !pet.reviveExpired()) {
    if (inDieBtn(x, y, DIE_YES_X)) {
      FxNeedSnap ns = fxSnap();
      if (pet.revive()) {
        Serial.printf("REVIVE ok, left=%u\n", pet.items[IT_REVIVE]);
        fxNeeds(ns);  // "+50" en cada barra
        fxMistakeW[pet.focusSlot].reset(pet.careMistakes);  // el descuido que cuenta revivir no salta como "!"
        sfxPlay(SFX_HATCH);
        revivedMsgUntil = millis() + 5000;
        fxPose(PMD_POSE, millis(), 1500);
      }
    } else if (inDieBtn(x, y, DIE_NO_X)) {
      Serial.println("REVIVE no");
      pet.letGo();
    }
  } else if (inDieBtn(x, y, DIE_OK_X)) {
    Serial.println(pet.reviveExpired() ? "REVIVE late" : "REVIVE none");
    pet.letGo();
  }
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
  drawPmdActS(m, actId, cx, groundY, t, loop, sil, s);
}
// lo mismo a una escala fija s (el patio, v2.0: los bichos van mas pequenos que en la casa de lado)
void drawPmdActS(PmdMon &m, uint8_t actId, int cx, int groundY, uint32_t t, bool loop, bool sil, uint8_t s) {
  const PmdAct &a = m.acts[actId];
  if (!a.frames) return;
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
// lo mismo a una escala en decimas (22 = 2,2x; el patio): cada pixel ocupa de c*s10/10 a (c+1)*s10/10
void drawPmdActF(PmdMon &m, uint8_t actId, int cx, int groundY, uint32_t t, bool loop, bool sil, uint16_t s10) {
  if (s10 % 10 == 0) {
    drawPmdActS(m, actId, cx, groundY, t, loop, sil, (uint8_t)(s10 / 10));
    return;
  }
  const PmdAct &a = m.acts[actId];
  if (!a.frames) return;
  uint8_t fi = pmdFrameAt(a, t, loop);
  const uint8_t *fr = a.data + (uint32_t)fi * a.w * a.h;
  int x0 = cx - a.w * s10 / 20, y0 = groundY - (a.base ? a.base : a.h) * s10 / 10;
  for (int r = 0; r < a.h; r++) {
    const uint8_t *row = fr + r * a.w;
    int ya = y0 + r * s10 / 10, yh = y0 + (r + 1) * s10 / 10 - ya;
    if (yh <= 0) continue;
    forRuns(a.w, [&](int c) { return indexedKey(row[c], sil); }, [&](int c0, int len, int k) {
      int xa = x0 + c0 * s10 / 10;
      gfx->fillRect(xa, ya, x0 + (c0 + len) * s10 / 10 - xa, yh, sil ? INK_K : m.pal[k]);
    });
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
  else if (pet.sleepy()) drawSleepyBubble(cx, headY, millis());
}

// tiene sueño (v1.63, desde las 22): un globo de pensamiento con "zzz" junto a la cabeza, del lado
// con sitio (a la derecha, o a la izquierda si el bicho anda por la derecha), meciendose despacio
void drawSleepyBubble(int cx, int headY, uint32_t now) {
  int side = cx > CX + 20 ? -1 : 1;
  int bob = (int)(3.0f * sinf(now * 0.003f));
  int bx = cx + side * 92, by = headY - 34 + bob;  // centro de la nube
  if (by < 118) by = 118;                           // sin pisar la cabecera
  gfx->fillCircle(cx + side * 40, headY - 2 + bob / 2, 5, UI_WHITE);
  gfx->drawCircle(cx + side * 40, headY - 2 + bob / 2, 5, UI_INK);
  gfx->fillCircle(cx + side * 56, by + 16, 8, UI_WHITE);
  gfx->drawCircle(cx + side * 56, by + 16, 8, UI_INK);
  gfx->fillRoundRect(bx - 38, by - 20, 76, 40, 20, UI_WHITE);
  gfx->drawRoundRect(bx - 38, by - 20, 76, 40, 20, UI_INK);
  gfx->setTextColor(UI_INK);
  setSize(2);
  // una "z" mas que otra cada segundo: parece que respira
  const char *z = (now / 1000) % 2 ? "zzz" : "zz";
  setCur(bx - textW("zzz", 2) / 2, by - 7);
  printT(z);
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
int dizzyWobbleOf(const PetLife &l, uint32_t now) {
  if (l.emote() != EMOTE_DIZZY) return 0;
  return (int)(7.0f * (1.0f - l.emoteT()) * sinf(now * 0.03f));
}
int dizzyWobble(uint32_t now) { return dizzyWobbleOf(pet, now); }

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

// tocar un regalo: con el bicho despierto se abre, se alegra y sale el aviso "REGALO!" (drawWalkNews);
// si no cabe en la mochila el aviso lo dice y sigue envuelto. Dormido, espera
void openGiftTap() {
  if (!pet.canOpenGift()) {
    sfxPlay(SFX_DENY);
    return;
  }
  bool full = false;
  uint8_t it = pet.openGift(&full);
  if (it >= IT_COUNT) return;
  Serial.printf("GIFT open %u%s\n", it, full ? " (full)" : "");
  if (!full) fxPose(PMD_POSE, millis(), 1200);  // contento con lo que trae
}

void drawPoops() {
  uint8_t m = pet.poopSlots();
  for (int i = 0; i < POOP_MAX; i++) {
    if (m & (1 << i)) drawMap(SPR_POOP, 32, 36 + i * 46, 244, 2, false);
  }
  // la que se acaba de limpiar: destellos donde estaba (como al final del baño)
  uint32_t left = timeLeft(poopPuffUntil);
  if (!left || poopPuffSlot < 0) return;
  int cx = 70 + 46 * poopPuffSlot, cy = 282;
  int r = 6 + (int)((POOP_PUFF_MS - left) / 25);  // se abren hacia fuera
  for (int k = 0; k < 4; k++) {
    int sx = cx + ((k & 1) ? r : -r), sy = cy + ((k & 2) ? r : -r) - 6;
    uint16_t col = (k % 2) ? UI_BAR_WARN : UI_WHITE;
    gfx->fillRect(sx - 5, sy - 1, 11, 3, col);
    gfx->fillRect(sx - 1, sy - 5, 3, 11, col);
  }
}

// Separacion entre la etiqueta y su barra en la fila de necesidades. Estaba fija
// en 48 px, justo lo que ocupan 4 letras latinas a escala 2, sin margen. Las
// etiquetas japonesas son mas anchas (ごきげん son 4 kana, ~64 px) y se metian
// dentro de la barra. Se deriva de la etiqueta mas larga, con el 48 de suelo
// para que en los idiomas latinos no cambie nada.
// Requiere el tamano de texto 2 ya puesto, porque textW() lo necesita.
static int barLabelGap() {
  const StrId ids[] = { S_BAR_FOOD, S_BAR_JOY, S_BAR_ENE };
  int ancho = 0;
  for (StrId id : ids) {
    int w = textW(T(id), 2);
    if (w > ancho) ancho = w;
  }
  // por debajo del umbral original se deja EXACTAMENTE 48, para no mover nada en
  // los idiomas latinos; por encima se anade aire para que la etiqueta respire
  return ancho <= 48 ? 48 : ancho + 8;
}

// Casa vacia (v1.39): sin bicho ni huevo hasta encontrar uno caminando. La silueta del huevo donde
// estaria, los pasos de hoy y la Pokedex; si hay alguien en la caja, como despertarlo.
// v1.40: con huevos en la mochila ("Dejar casa vacia" al soltar), el boton "Abrir un huevo (N)" ocupa
// el sitio de los pasos y la Pokedex.
void drawVacantHome() {
  uint8_t eggs = pet.items[IT_EGG];
  drawHeader(T(S_NOEGG_HDR), inkColor(), T(eggs ? S_NOEGG_HAVE : S_NOEGG_MSG));
  int s = 5;  // donde y como drawEggSprite(), en gris y quieta
  uint16_t ghost = gNight ? C565(0x3a, 0x42, 0x5c) : lerp565(UI_TRACK, UI_BG_DAY, 1, 2);
  drawMapTint(SPR_EGG, SPRITE_H, CX - 16 * s, PET_CY - 16 * s, s, ghost);
  gfx->fillRect(0, 312, 466, 154, gNight ? UI_BG_NIGHT : UI_BG_DAY);
  gfx->setTextColor(inkColor());
  setSize(2);
  char line[40];
  if (eggs) {
    gfx->fillRoundRect(VAC_BTN_X, VAC_BTN_Y, VAC_BTN_W, VAC_BTN_H, 12, UI_BAR_OK);
    gfx->drawRoundRect(VAC_BTN_X, VAC_BTN_Y, VAC_BTN_W, VAC_BTN_H, 12, inkColor());
    snprintf(line, sizeof(line), T(S_HATCH_FMT), (unsigned)eggs);
    gfx->setTextColor(UI_WHITE);
    setCur(centerX(line, 2), VAC_BTN_Y + 15);
    printT(line);
  } else {
    snprintf(line, sizeof(line), T(S_STEPS_FMT), (unsigned long)walkStepsToday());
    setCur(centerX(line, 2), 326);
    printT(line);
    snprintf(line, sizeof(line), T(S_POKEDEX_FMT), pet.registeredCount());
    setCur(centerX(line, 2), 352);
    printT(line);
  }
  bool boxed = false;
  for (uint8_t k = 0; k < BOX_SLOTS; k++) boxed |= pet.boxState(k) == BOX_OK && !pet.slotTravelling(k);
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

// dibuja un mapa de n x n pixeles escalado; silhouette=true lo pinta en tinta
void drawMap(const char *const *map, int n, int x, int y, int s, bool silhouette) {
  for (int r = 0; r < n; r++) {
    const char *row = map[r];
    forRuns(n, [&](int c) { return charKey(row[c], silhouette); },
            [&](int c0, int len, int k) { gfx->fillRect(x + c0 * s, y + r * s, len * s, s, silhouette ? INK_K : spriteColor((char)k)); });
  }
}

// ---------- fases de combate (stage.h) ----------
// Una fase: 3 rivales seguidos (2 esbirros y el jefe) contra un equipo ordenado de 1 a 4 bichos (el
// de casa y/o los de la caja). Tarjeta VS con el jingle, entrada al campo y un ataque cada ATTACK_MS;
// tocar la pantalla carga el especial (5 toques). Tras un KO el caido se desvanece y, en el relevo,
// entra el siguiente (aliado o rival) andando desde su lado mientras el que sigue en pie se queda con
// su vida: la vida no se recupera en toda la fase. El motor puro (stage.h, battle.h) decide cada
// ataque en el momento en que empieza; aqui solo se anima y se cuenta. Leer la SD bloquea el loop:
// los sprites se cargan al empezar y en el relevo, nunca con ataques en marcha.

enum : uint8_t { BT_OFF = 0, BT_INTRO, BT_FIGHT, BT_END, BT_SWAP };  // (BT_END sigue siendo 3: comando TAP)
#define BT_VS_MS 2000      // tarjeta VS; el resto de INTRO_MS es la entrada al campo
#define BT_HIT_MS 350      // el golpe llega a mitad del avance del atacante
#define BT_HPBAR_MS 400    // la barra de vida baja en este tiempo
#define BT_FAINT_MS 900    // el derrotado se desvanece
#define BT_RECOIL_MS (BT_HIT_MS + BT_HPBAR_MS)  // muerte subita: el retroceso llega cuando acaba de bajar la barra del rival
#define BT_SWAP_MS 1800    // relevo tras un KO: el cartel y la entrada del siguiente...
#define BT_SWAP_WALK_MS 1000  // ...que entra andando en el ultimo segundo, como en la intro
#define BT_END_MS 7000     // el resultado se cierra solo (o con un toque)
// v1.38: el resultado no atiende toques durante 1,8 s. Se suele llegar al final aporreando la
// pantalla para cargar el especial, y el siguiente toque cerraba el resultado sin dejarlo ver.
// La pista "toca: volver" aparece cuando ya se puede tocar.
#define BT_END_LOCK_MS 1800
// El patio (v2.0, fase Y1): panel oscuro con "Fase N" y el nombre de la fase arriba, los nombres y
// las dos barras de vida (aliado verde a la izquierda, rival rojo a la derecha), una ventana
// redondeada con la pelea sobre el bioma de la fase, el registro de dos lineas debajo, el medidor
// del especial y la tira del equipo abajo. Toda la pantalla sigue cargando el especial.
#define BT_NAME_Y 96       // nombres; debajo "cur/max" y el nivel, y la barra
#define BT_HP_Y 138
#define BT_HP_W 160
#define BT_ME_HP_X 62
#define BT_FOE_HP_X 244
#define BT_WIN_X 44        // la ventana de la pelea
#define BT_WIN_Y 158
#define BT_WIN_W 378
#define BT_WIN_H 156
#define BT_WIN_R Y_CARD_RAD
#define BT_HORIZON (BT_WIN_Y + 52)
#define BT_ME_X 146        // tu bicho: abajo a la izquierda
#define BT_ME_GROUND 302
#define BT_FOE_X 314       // el rival: arriba a la derecha
#define BT_FOE_GROUND 238
#define BT_LOG_Y 322       // registro: dos lineas (la segunda atenuada)
#define BT_LOG_GAP 22
#define BT_PIP_Y 420       // tira del equipo: una pastilla por bicho
#define BT_PIP_W 20
#define BT_PIP_H 10
#define BT_PIP_PITCH 28
#define BT_PIP_DEAD Y_TEXT_DIM // contorno del caido
#define BT_RES_HEAD_Y 100  // resultado: el titular, la ventana con el ganador y la tarjeta del premio
#define BT_RES_WIN_Y 130
#define BT_RES_WIN_H 124
#define BT_RES_GROUND 242
#define BT_RES_CARD_Y 264
#define BT_NAME_LEN 13     // nombre de un aliado (apodo de 11 + margen)

PmdMon foePmd;   // sprite del rival en curso
PmdMon allyPmd;  // el del aliado en curso si no es el de casa (el de casa es pmd)
struct {
  uint8_t phase = BT_OFF;
  uint32_t t0 = 0;       // inicio de la fase en curso (en BT_FIGHT, del combate en curso)
  StageRun run;          // la fase: equipo, rivales, combate en curso y medidores
  BattleEvent ev;        // ultimo ataque calculado
  bool haveEv = false;
  bool evSounded = false;
  bool recoilSounded = false;
  uint32_t evAt = 0;     // instante (programado) en que empezo ese ataque
  uint16_t hpBefore[2] = { 0, 0 };
  uint8_t swap = SN_NONE;  // en el relevo: SN_NEXT_ALLY o SN_NEXT_FOE
  // el equipo, en el orden de run.team: lo que la pantalla necesita de cada uno
  uint32_t uid[TEAM_MAX] = { 0 };
  int16_t dex[TEAM_MAX] = { 0 };
  bool shiny[TEAM_MAX] = { false };
  uint8_t home[TEAM_MAX] = { 0 };  // uno de casa: su hueco + 1 (su sprite es homeMon[hueco]); 0 = de la caja
  char name[TEAM_MAX][BT_NAME_LEN] = {};
  bool cleared = false;  // resultado: fase superada
  StageReward rw = { false, false, (uint8_t)IT_COUNT, 0, 0 };  // lo que pago stageDone
  bool wild = false;     // v2.2: un encuentro salvaje (un rival, un luchador; paga encounterDone)
  EncReward erw = { false, (uint8_t)IT_COUNT, false, 0, 0, 0 };
} bt;

bool battleOpen() { return bt.phase != BT_OFF; }
// comando TAP: 0 cerrado, 1 intro, 2 combate, 3 resultado, 4 relevo
uint8_t battlePhase() { return bt.phase; }
const char *battlePhaseName() {
  return bt.phase == BT_INTRO ? "intro" : bt.phase == BT_FIGHT ? "fight" : bt.phase == BT_SWAP ? "swap"
       : bt.phase == BT_END ? "end" : "off";
}

// cierra la fase en cualquier momento (resultado, USER, PWR, el dialogo de la muerte): sin premio si
// no habia acabado (stageDone solo se llama al entrar en el resultado)
void closeBattle() {
  bool wild = bt.wild, ended = bt.phase == BT_END, won = bt.cleared;
  bt.phase = BT_OFF;
  bt.wild = false;
  allyPmd.unload();
  if (wild) {
    // v2.2: tras el resultado, la Poke Ball (ganado) o "huyo" (perdido); el sprite del salvaje sigue cargado.
    // Abandonar a mitad (USER, PWR, el dialogo de la muerte) cuenta como perder: se escapa
    if (!ended) {
      pet.encounterDone(false, 0);
      foePmd.unload();
      return;
    }
    openEncUiAfterBattle(won);
    return;
  }
  foePmd.unload();
}

// quien esta en el campo: 0 = el aliado en curso, 1 = el rival en curso
int16_t battleDex(uint8_t side) { return side ? bt.run.foeInfo().dex : bt.dex[bt.run.ally]; }
const char *battleName(uint8_t side) { return side ? dexName(battleDex(1)) : bt.name[bt.run.ally]; }
PmdMon &battleMon(uint8_t side) {
  return side ? foePmd : (bt.home[bt.run.ally] ? homeMon[(bt.home[bt.run.ally] - 1) & 1] : allyPmd);
}

// sprites (bloquean el loop: solo al empezar y en el relevo)
void battleLoadFoe() {
  foePmd.unload();
  foePmd.load((uint8_t)battleDex(1), bt.wild && encShowShiny);  // v2.2: un salvaje puede ser shiny
}
void battleLoadAlly() {
  allyPmd.unload();
  uint8_t a = bt.run.ally;
  if (!bt.home[a]) allyPmd.load((uint8_t)bt.dex[a], bt.shiny[a]);
}

// el bicho uid como el aliado m del combate (bt.dex/shiny/home/name/uid y su Fighter). false: no esta, no puede
// pelear o ya va en el equipo
static bool btAddMate(uint32_t id, uint8_t m, Fighter &out) {
  for (uint8_t j = 0; j < m; j++)
    if (bt.uid[j] == id) return false;
  int8_t k = pet.teamSlotOf(id);
  if (k < 0 || !pet.teamCanFight(id)) return false;
  if (teamIsHome(k)) {  // v3.0: uno de los dos de casa
    uint8_t hs = teamHomeSlot(k);
    const PetLife &l = pet.home(hs);
    out = makeFighter(l.speciesId, l.level(), l.geneAtk, l.geneDef, l.geneSpe, l.trAtk, l.trDef, l.trSpe);
    bt.dex[m] = l.speciesId;
    bt.shiny[m] = l.shiny;
    bt.home[m] = (uint8_t)(hs + 1);
    snprintf(bt.name[m], BT_NAME_LEN, "%.*s", (int)sizeof(l.nick), l.nick[0] ? l.nick : dexName(l.speciesId));
  } else {
    const PetRecord &r = pet.boxRecord((uint8_t)k);
    out = makeFighter(r.speciesId, levelFor(r.growMin), r.geneAtk, r.geneDef, r.geneSpe, r.trAtk, r.trDef, r.trSpe);
    bt.dex[m] = r.speciesId;
    bt.shiny[m] = (r.flags & REC_SHINY) != 0;
    bt.home[m] = 0;
    snprintf(bt.name[m], BT_NAME_LEN, "%.*s", (int)sizeof(r.nick), recName(r));
  }
  bt.uid[m] = id;
  return true;
}

// v2.2: el combate contra el salvaje del patio, con el bicho uid (el selector en modo un luchador). Las reglas
// de una fase con un solo rival (StageRun::beginWild); al acabar paga encounterDone (battleEnd)
void startWild(uint32_t id) {
  if (battleOpen() || !pet.encActive()) return;
  bt = decltype(bt){};
  Fighter mate;
  if (!btAddMate(id, 0, mate)) return;
  StageFoe w;
  w.dex = pet.encDex();
  w.level = pet.encLevel();
  w.tAtk = pet.encTraining(0);
  w.tDef = pet.encTraining(1);
  w.tSpe = pet.encTraining(2);
  w.aiTaps = 1;
  encShowDex = w.dex;
  encShowLv = w.level;
  encShowShiny = pet.encShiny();
  bt.wild = true;
  uint32_t seed = (uint32_t)random(0x7FFFFFFF) ^ (millis() << 7);
  bt.run.beginWild(mate, w, seed);
  battleLoadFoe();  // antes de la intro: leer la SD bloquea el loop
  battleLoadAlly();
  panelKind = PANEL_NONE;
  bt.phase = BT_INTRO;
  bt.t0 = millis();
  sfxPlay(SFX_BATTLE);
  wakeUI();
  Serial.printf("wild: #%d nv.%u contra #%d nv.%u ent.%u/%u/%u\n", bt.dex[0], bt.run.team[0].level, w.dex, w.level,
                w.tAtk, w.tDef, w.tSpe);
}

// Empieza la fase `stage` con el equipo `uids` en orden (el selector de equipo). Se salta los que no
// esten (ni en casa ni en la caja), no puedan combatir o esten repetidos; no hace nada si no queda
// nadie, si la fase no existe o si ya hay un combate.
void startStage(uint8_t stage, const uint32_t *uids, uint8_t n) {
  if (stage >= STAGE_COUNT || battleOpen() || !uids) return;
  bt = decltype(bt){};
  Fighter mates[TEAM_MAX];
  uint8_t m = 0;
  for (uint8_t i = 0; i < n && m < TEAM_MAX; i++)
    if (btAddMate(uids[i], m, mates[m])) m++;
  if (!m) return;
  pet.setTeam(bt.uid, m);  // el ultimo equipo (sin escribir ya en flash)
  uint32_t seed = (uint32_t)random(0x7FFFFFFF) ^ (millis() << 7);
  bt.run.begin(stage, mates, m, seed);
  battleLoadFoe();  // antes de la intro: leer la SD bloquea el loop
  battleLoadAlly();
  panelKind = PANEL_NONE;
  bt.phase = BT_INTRO;
  bt.t0 = millis();
  sfxPlay(SFX_BATTLE);
  wakeUI();
  Serial.printf("stage %u: equipo", stage + 1);
  for (uint8_t i = 0; i < m; i++) Serial.printf(" #%d nv.%u", bt.dex[i], bt.run.team[i].level);
  Serial.print(" | rivales");
  for (uint8_t f = 0; f < STAGE_FOES; f++) {
    const StageFoe &e = bt.run.foes[f];
    Serial.printf(" #%d nv.%u ent.%u/%u/%u", e.dex, e.level, e.tAtk, e.tDef, e.tSpe);
  }
  Serial.println();
}

// La fase con el ultimo equipo si alguno de sus bichos aun puede combatir; si no, el de casa (si
// puede) y detras los de la caja por orden de hueco, hasta TEAM_MAX (consola y respaldo del menu)
void startStageAuto(uint8_t stage) {
  const uint32_t *saved = pet.teamUids();
  for (uint8_t i = 0; i < TEAM_MAX; i++)
    if (saved[i] && pet.teamCanFight(saved[i])) {
      startStage(stage, saved, TEAM_MAX);
      return;
    }
  uint32_t ids[TEAM_MAX];
  uint8_t n = 0;
  if (pet.uid && pet.teamCanFight(pet.uid)) ids[n++] = pet.uid;
  for (uint8_t k = 0; k < BOX_SLOTS && n < TEAM_MAX; k++)
    if (pet.boxState(k) == BOX_OK && !pet.slotTravelling(k)) ids[n++] = pet.boxRecord(k).uid;
  startStage(stage, ids, n);
}

// la fase del boton del menu: la primera abierta con el premio de hoy sin cobrar; si no, la mas alta
uint8_t stageAutoPick() {
  uint8_t hi = 0;
  for (uint8_t s = 0; s < STAGE_COUNT; s++) {
    if (!pet.stageUnlocked(s)) continue;
    if (pet.stageRewardReady(s)) return s;
    hi = s;
  }
  return hi;
}

void battleTap(int16_t x, int16_t y) {
  (void)x;
  (void)y;
  if (bt.phase == BT_END) {
    if (millis() - bt.t0 >= BT_END_LOCK_MS) closeBattle();  // antes, el toque no hace nada
    return;
  }
  if (bt.phase != BT_FIGHT) return;  // en la intro y en el relevo los toques no cuentan
  if (bt.run.tap()) sfxPlay(SFX_TAP);  // (tampoco con el KO ya dado)
}

// fin de la fase: paga (una sola vez, y guarda) y pasa al resultado
void battleEnd(bool cleared) {
  bt.cleared = cleared;
  if (bt.wild) {  // v2.2: el salvaje (una vez, y guarda)
    bt.erw = pet.encounterDone(cleared, bt.uid[0]);
    bt.phase = BT_END;
    bt.t0 = millis();
    sfxPlay(cleared ? SFX_WIN : SFX_LOSE);
    return;
  }
  bt.rw = pet.stageDone(bt.run.stage, cleared, bt.uid, bt.run.teamN);
  bt.phase = BT_END;
  bt.t0 = millis();  // despues de la escritura en flash: el bloqueo del toque cuenta desde aqui
  sfxPlay(cleared ? SFX_WIN : SFX_LOSE);
  Serial.printf("stage %u: %s, insignia nueva %d, premio %d (%u bichos), objeto %u, +%u felicidad\n",
                bt.run.stage + 1, cleared ? "superada" : "perdida", bt.rw.newBadge ? 1 : 0, bt.rw.paid ? 1 : 0,
                bt.rw.pets, bt.rw.item, bt.rw.joy);
}

// tras el KO y el desvanecido: entra el siguiente (relevo) o se acaba la fase
void battleAfterKo() {
  StageRun &r = bt.run;
  bool allyFell = r.eng.result == BR_WIN_B;
  Serial.printf("stage: %s #%d gana a #%d en %u ataques (aliado %u/%u %u/%u PS, rival %u/%u %u/%u PS)\n",
                allyFell ? "rival" : "aliado", allyFell ? battleDex(1) : battleDex(0),
                allyFell ? battleDex(0) : battleDex(1), r.eng.attacks, r.ally + 1, r.teamN, r.eng.f[0].hp,
                r.eng.f[0].maxHp, r.foeIdx + 1, STAGE_FOES, r.eng.f[1].hp, r.eng.f[1].maxHp);
  uint8_t s = r.advance();
  if (s == SN_CLEARED || s == SN_LOST) {
    battleEnd(s == SN_CLEARED);
    return;
  }
  bt.swap = s;
  bt.haveEv = false;
  if (s == SN_NEXT_FOE) battleLoadFoe();
  else battleLoadAlly();
  bt.phase = BT_SWAP;
  bt.t0 = millis();  // despues de leer la SD: el relevo se ve entero
  sfxPlay(SFX_WAKE);
}

// avanza la fase: calcula el siguiente ataque cuando le toca y lo hace sonar
void battleStep(uint32_t now) {
  if (bt.phase == BT_OFF || bt.phase == BT_END) return;
  lastInteract = now;  // nadie tiene por que tocar: sin atenuar ni salvapantallas a mitad de fase
  if (bt.phase == BT_INTRO) {
    if ((int32_t)(now - bt.t0) < INTRO_MS) return;
    bt.phase = BT_FIGHT;
    bt.t0 += INTRO_MS;
  }
  if (bt.phase == BT_SWAP) {
    if ((int32_t)(now - bt.t0) < BT_SWAP_MS) return;
    bt.phase = BT_FIGHT;  // el reloj de ataques empieza de cero con el combate nuevo
    bt.t0 += BT_SWAP_MS;
  }
  StageRun &r = bt.run;
  if (!r.eng.over()) {
    uint32_t due = bt.t0 + (uint32_t)r.eng.attacks * ATTACK_MS;
    if ((int32_t)(now - due) >= 0) {
      bt.hpBefore[0] = r.eng.f[0].hp;
      bt.hpBefore[1] = r.eng.f[1].hp;
      bt.ev = r.attack();
      bt.haveEv = true;
      bt.evSounded = false;
      bt.recoilSounded = false;
      if (bt.ev.sudden)
        Serial.printf("battle: SD #%u dmg %u recoil %u hp %u/%u\n", (unsigned)bt.ev.index, (unsigned)bt.ev.damage,
                      (unsigned)bt.ev.recoil, (unsigned)bt.ev.hp[0], (unsigned)bt.ev.hp[1]);
      bt.evAt = due;  // el programado, no "ahora": un frame lento no desplaza el resto
    }
  }
  if (bt.haveEv && !bt.evSounded && (int32_t)(now - (bt.evAt + BT_HIT_MS)) >= 0) {
    bt.evSounded = true;
    sfxPlay(!bt.ev.eff4 ? SFX_DENY : bt.ev.special ? SFX_SPECIAL : bt.ev.crit ? SFX_CRIT : SFX_HIT);
  }
  if (bt.haveEv && bt.ev.recoil && !bt.recoilSounded && (int32_t)(now - (bt.evAt + BT_RECOIL_MS)) >= 0) {
    bt.recoilSounded = true;
    sfxPlay(SFX_HIT);
  }
  if (r.koPending() && bt.haveEv && (int32_t)(now - (bt.evAt + ATTACK_MS + BT_FAINT_MS)) >= 0) battleAfterKo();
}

// vida que ensena la barra: baja durante BT_HPBAR_MS desde que llega el golpe
uint16_t battleShownHp(uint8_t side, uint32_t now) {
  if (!bt.haveEv) return bt.run.eng.f[side].hp;
  uint32_t dt = now - bt.evAt;
  uint16_t a = bt.hpBefore[side], b = bt.ev.hp[side];
  // el retroceso del atacante llega cuando el rival ya ha terminado de perder vida
  uint32_t t0 = (side == bt.ev.attacker && bt.ev.recoil) ? BT_RECOIL_MS : BT_HIT_MS;
  if (dt < t0) return a;
  if (dt >= t0 + BT_HPBAR_MS) return b;
  return (uint16_t)(a + ((int32_t)b - a) * (int32_t)(dt - t0) / BT_HPBAR_MS);
}

// recorte de la ventana de la pelea: los bichos (que entran andando desde fuera, se lanzan y se
// hunden al caer) no salen de ella
static int btClipX0 = 0, btClipY0 = 0, btClipX1 = 466, btClipY1 = 466;
static void btSetClip(int x, int y, int w, int h) {
  btClipX0 = x;
  btClipY0 = y;
  btClipX1 = x + w;
  btClipY1 = y + h;
}
static void btFill(int x, int y, int w, int h, uint16_t c) {
  if (x < btClipX0) { w -= btClipX0 - x; x = btClipX0; }
  if (y < btClipY0) { h -= btClipY0 - y; y = btClipY0; }
  if (x + w > btClipX1) w = btClipX1 - x;
  if (y + h > btClipY1) h = btClipY1 - y;
  if (w > 0 && h > 0) gfx->fillRect(x, y, w, h, c);
}

// drawPmdActM recortado a la ventana (mismo escalado y mismo anclaje por los pies)
static void btPmd(PmdMon &m, uint8_t actId, int cx, int groundY, uint32_t t, bool loop, bool sil, uint8_t maxS) {
  const PmdAct &a = m.acts[actId];
  if (!a.frames) return;
  uint8_t s = m.acts[PMD_IDLE].h ? 170 / m.acts[PMD_IDLE].h : 5;
  if (s < 2) s = 2;
  if (s > maxS) s = maxS;
  while (s > 2 && a.h * s > 250) s--;  // acciones con frame grande (ataque)
  uint8_t fi = pmdFrameAt(a, t, loop);
  const uint8_t *fr = a.data + (uint32_t)fi * a.w * a.h;
  int x0 = cx - a.w * s / 2, y0 = groundY - (a.base ? a.base : a.h) * s;
  for (int r = 0; r < a.h; r++) {
    int ry = y0 + r * s;
    if (ry + s <= btClipY0) continue;
    if (ry >= btClipY1) break;
    const uint8_t *row = fr + r * a.w;
    forRuns(a.w, [&](int c) { return indexedKey(row[c], sil); },
            [&](int c0, int len, int k) { btFill(x0 + c0 * s, ry, len * s, s, sil ? INK_K : m.pal[k]); });
  }
}

// drawThumb recortado a la ventana
static void btThumb(const uint8_t *b, int x, int y, int s, bool sil) {
  uint8_t w = b[0], h = b[1], n = b[2];
  const uint8_t *pal = b + 3;
  const uint8_t *d = pal + n * 2;
  int ox = x + (GAL_CELL - w * s) / 2, oy = y + (GAL_CELL - h * s) / 2;
  for (int r = 0; r < h; r++) {
    const uint8_t *row = d + r * w;
    forRuns(w, [&](int c) { return indexedKey(row[c], sil); },
            [&](int c0, int len, int k) {
              uint16_t col = sil ? INK_K : (uint16_t)(pal[k * 2] | (pal[k * 2 + 1] << 8));
              btFill(ox + c0 * s, oy + r * s, len * s, s, col);
            });
  }
}

// el bioma de la fase: el del jefe (planta: bosque, agua: playa, fuego: volcan...)
uint8_t battleBiome() {
  if (bt.wild) {  // v2.2: el del mapa del patio (prado, cantera = montana, bosque, playa, ruinas)
    static const uint8_t MAP_BIOME[YARD_MAPS + 1] = { 0, 0, 4, 2, 1, 8 };
    uint8_t m = pet.yardMapWanted();
    return MAP_BIOME[m <= YARD_MAPS ? m : 0];
  }
  uint8_t st = bt.run.stage < STAGE_COUNT ? bt.run.stage : 0;
  int16_t boss = STAGES[st].bossDex;
  uint8_t bio = (boss >= 1 && boss <= DEX_COUNT) ? DEX_TBL[boss].biome : 0;
  return bio < BIOME_COUNT ? bio : 0;
}

// la ventana de la pelea (de BT_WIN_X a lo ancho, de y a y+h): cielo por la hora hasta hz, el suelo
// del bioma de la fase y las tarimas (dos en la pelea; una en el centro, en el resultado)
void drawBattleWindow(int y, int h, int hz, int shake, bool result) {
  const int X = BT_WIN_X, W = BT_WIN_W;
  int hh = sceneHour();
  bool night = hh < 6 || hh >= 20;
  uint16_t top, bot;
  if (night)       { top = C565(0x0c, 0x12, 0x24); bot = C565(0x1e, 0x26, 0x46); }
  else if (hh < 8) { top = C565(0xd1, 0x6a, 0x86); bot = C565(0xf3, 0xb8, 0x7c); }
  else if (hh < 18){ top = C565(0x8f, 0xc8, 0xea); bot = C565(0xdc, 0xee, 0xe6); }
  else             { top = C565(0xc7, 0x5a, 0x4a); bot = C565(0xf0, 0xae, 0x64); }
  uint16_t soil = BIOME_SOIL[battleBiome()];
  if (night) soil = lerp565(soil, C565(0x16, 0x1c, 0x30), 9, 16);
  gfx->fillRoundRect(X, y, W, h, BT_WIN_R, soil);              // el suelo, con las esquinas de abajo
  gfx->fillRoundRect(X, y, W, 2 * BT_WIN_R, BT_WIN_R, top);    // las de arriba, en el color del cielo
  for (int yy = y + BT_WIN_R; yy < hz; yy += 6)
    gfx->fillRect(X, yy, W, hz - yy < 6 ? hz - yy : 6, lerp565(top, bot, yy - y, hz - y));
  gfx->drawFastHLine(X, hz, W, lerp565(soil, C565(0x10, 0x18, 0x20), 3, 16));  // el horizonte
  if (night)
    for (auto &st : STARS)
      gfx->fillRect(X + 16 + st[0] * (W - 32) / 466, y + 8 + (st[1] - 90) * (hz - y - 18) / 140, 3, 3, UI_WHITE);
  uint16_t pad = lerp565(soil, C565(0x10, 0x18, 0x20), 4, 16);
  if (result) {
    gfx->fillEllipse(CX, BT_RES_GROUND - 4, 70, 12, pad);
  } else {
    gfx->fillEllipse(BT_FOE_X + shake, BT_FOE_GROUND - 3, 56, 11, pad);
    gfx->fillEllipse(BT_ME_X + shake, BT_ME_GROUND - 3, 70, 13, pad);
  }
}

// uno de los dos, recortado a la ventana: su sprite PMD; si no lo hay, la miniatura; si tampoco, "?"
// maxS: tu bicho x3 y el rival x2 en el campo (los frames de ATTACK son grandes)
void drawFighter(uint8_t side, uint8_t act, int x, int ground, uint32_t t, bool loop, bool sil, uint8_t maxS) {
  PmdMon &m = battleMon(side);
  int16_t dex = battleDex(side);
  if (m.loaded) {
    if (!m.has(act)) act = PMD_IDLE;
    btPmd(m, act, x, ground, t, loop, sil, maxS);
    return;
  }
  const uint8_t *th = thumbs.get(dex);
  if (th) {
    btThumb(th, x - GAL_CELL / 2, ground - GAL_CELL, 2, sil);
    return;
  }
  if (x < btClipX0 + 18 || x > btClipX1 - 18) return;
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
    drawEggAt(x, ground - 32, 2, 0, pet.eggSpecies());
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

// tira del equipo: una pastilla por bicho, en orden, desde x (centro de la primera) a la altura y.
// Llena y verde = en pie, hueca y gris = caido; el que esta en el campo, con un aro blanco.
void drawTeamPips(int x, int y, uint32_t now) {
  const StageRun &r = bt.run;
  for (uint8_t i = 0; i < r.teamN; i++) {
    int px = x + i * BT_PIP_PITCH - BT_PIP_W / 2, py = y - BT_PIP_H / 2;
    bool cur = i == r.ally && bt.phase != BT_END;
    bool alive = (i == r.ally && bt.phase == BT_FIGHT) ? battleShownHp(0, now) > 0 : r.allyHp(i) > 0;
    if (alive) gfx->fillRoundRect(px, py, BT_PIP_W, BT_PIP_H, BT_PIP_H / 2, Y_ACCENT_GO);
    else gfx->drawRoundRect(px, py, BT_PIP_W, BT_PIP_H, BT_PIP_H / 2, BT_PIP_DEAD);
    if (cur && alive) gfx->drawRoundRect(px - 3, py - 3, BT_PIP_W + 6, BT_PIP_H + 6, BT_PIP_H / 2 + 3, Y_TEXT);
  }
}

// fondo del panel del combate en una sola pasada (yPanelBg pinta negro y luego el disco: el doble de
// pixeles en cada frame de 85 ms). Fuera del disco no se ve: el marco redondo lo tapa.
void btPanelBg() {
  gfx->fillScreen(Y_PANEL_BG);
}

// cabecera de las pantallas del combate: "Fase N" y el nombre de la fase debajo
void drawBattleHeader() {
  if (bt.wild) {  // v2.2: "Salvaje" y su nombre
    yTitle(T(S_WILD), dexName(bt.run.foeInfo().dex));
    return;
  }
  char t[24];
  snprintf(t, sizeof(t), T(S_STAGE_FMT), (unsigned)(bt.run.stage + 1));
  yTitle(t, stageName(bt.run.stage));
}

// nombre, "cur/max", nivel y barra de uno de los dos: el aliado a la izquierda (verde), el rival a la
// derecha (rojo) con el nombre pegado a su borde derecho
void drawHpRow(uint8_t side, uint16_t hp) {
  const Fighter &f = bt.run.eng.f[side];
  int x = side == 0 ? BT_ME_HP_X : BT_FOE_HP_X;
  const char *nm = battleName(side);
  gfx->setTextColor(Y_TEXT);
  setSize(2);
  setCur(side == 0 ? x : x + BT_HP_W - textW(nm, 2), BT_NAME_Y);
  printT(nm);
  yHpBar(x, BT_HP_Y, BT_HP_W, hp, f.maxHp, side == 0 ? Y_ACCENT_GO : Y_ACCENT_BAD);
  char lv[10];
  snprintf(lv, sizeof(lv), T(S_LVL_FMT), (unsigned)f.level);
  gfx->setTextColor(Y_TEXT_DIM);
  setSize(2);
  setCur(x + BT_HP_W - textW(lv, 2), BT_HP_Y - 20);
  printT(lv);
}

// arriba a la derecha de la ventana: cual de los tres rivales es ("1/3", "2/3") o JEFE en rojo
void drawFoeTag() {
  if (bt.wild) return;  // v2.2: un solo rival
  char n[8];
  const char *s = n;
  bool boss = bt.run.isBoss();
  if (boss) s = T(S_BOSS);
  else snprintf(n, sizeof(n), "%u/%u", (unsigned)(bt.run.foeIdx + 1), (unsigned)STAGE_FOES);
  setSize(2);
  int tw = textW(s, 2), w = tw + 16, x = BT_WIN_X + BT_WIN_W - 8 - w, y = BT_WIN_Y + 8;
  gfx->fillRoundRect(x, y, w, 24, 12, boss ? Y_ACCENT_BAD : Y_PANEL_BG);
  gfx->setTextColor(Y_TEXT);
  setCur(x + 8, y + 5);
  printT(s);
}

// Una flecha ">" sin palo (v1.38): dos brazos de METER_T px de grueso que se juntan en la punta.
// x = su borde izquierdo, y = su centro. fill = 0 la deja hueca (solo el contorno).
#define METER_W 14     // ancho de una flecha
#define METER_H 12     // media altura
#define METER_T 6      // grueso de cada brazo, en horizontal
#define METER_PITCH 21
#define METER_TAP_GAP 10  // de la punta de la ultima flecha al cartel "Toca!"
#define METER_TAP_MIN 64  // ancho minimo del cartel
#define METER_Y 380    // el medidor, centrado como grupo (flechas + cartel) bajo el registro
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
// Las cargadas en ambar, laten cuando esta listo; bloqueado tras un especial, huecas y sin cartel.
// El grupo se centra siempre con el ancho del cartel, para que las flechas no se muevan.
void drawSpecialMeter(uint32_t now) {
  const SpecialMeter &mt = bt.run.meter;
  bool armed = mt.armed(), locked = mt.locked();
  const char *tap = T(S_TAP_BOX);
  setSize(2);
  // ancho minimo: con la fuente coreana textW() se queda corto (mide la tinta, no el avance)
  int tw = textW(tap, 2), bw = tw + 16 < METER_TAP_MIN ? METER_TAP_MIN : tw + 16, bh = 28;
  int arrows = (SPECIAL_TAPS - 1) * METER_PITCH + METER_W;
  int x0 = CX - (arrows + METER_TAP_GAP + bw) / 2;
  for (int i = 0; i < SPECIAL_TAPS; i++) {
    int x = x0 + i * METER_PITCH;
    if (locked) {
      drawChevron(x, METER_Y, false, 0, Y_CARD_HI);
    } else if (i < mt.pips) {
      uint16_t c = (armed && (now / 160) % 2) ? UI_WHITE : UI_BAR_WARN;
      drawChevron(x, METER_Y, true, c, c);
    } else {
      drawChevron(x, METER_Y, true, Y_CARD_HI, Y_CARD_HI);
    }
  }
  // el cartel: solo cuando un toque cuenta (ni lleno, ni bloqueado, ni con el combate decidido)
  if (locked || mt.pips >= SPECIAL_TAPS || bt.run.eng.over()) return;
  int bx = x0 + arrows + METER_TAP_GAP, by = METER_Y - bh / 2;
  gfx->fillRoundRect(bx, by, bw, bh, bh / 2, UI_BAR_WARN);
  gfx->setTextColor(UI_INK);
  setCur(bx + (bw - tw) / 2, by + (bh - 14) / 2);
  printT(tap);
}

// tarjeta VS: la fase arriba ("Fase 3" / "Roca"), el primer rival en una tarjeta del color de su tipo,
// el disco VS y tu primer aliado en la suya; debajo la tira del equipo
void drawVsCard() {
  btPanelBg();
  drawBattleHeader();
  char lv[10];
  const int cx = 70, cw = 326, ch = 82;
  for (uint8_t side = 0; side < 2; side++) {
    int y = side ? 108 : 276;
    gfx->fillRoundRect(cx, y, cw, ch, Y_CARD_RAD, lerp565(DEX_TBL[battleDex(side)].accent, Y_CARD, 5, 8));
    const char *nm = battleName(side);
    setSize(3);
    uint8_t ts = textW(nm, 3) <= cw - 24 ? 3 : 2;
    setSize(ts);
    gfx->setTextColor(Y_TEXT);
    setCur(centerX(nm, ts), y + (ts == 3 ? 14 : 18));
    printT(nm);
    snprintf(lv, sizeof(lv), T(S_LVL_FMT), (unsigned)bt.run.eng.f[side].level);
    gfx->setTextColor(Y_TEXT_DIM);
    setSize(2);
    setCur(centerX(lv, 2), y + 52);
    printT(lv);
  }
  drawTeamPips(CX - (bt.run.teamN - 1) * BT_PIP_PITCH / 2, 392, millis());
  gfx->fillCircle(CX, CY, 36, Y_ACCENT_BAD);
  gfx->drawCircle(CX, CY, 36, Y_PANEL_BG);
  gfx->drawCircle(CX, CY, 37, Y_PANEL_BG);
  gfx->setTextColor(Y_TEXT);
  setSize(4);
  setCur(CX - textW("VS", 4) / 2, CY - 15);
  printT("VS");
}

// registro de dos lineas bajo la ventana: la primera en blanco (o en su color), la segunda atenuada
// (salvo los avisos en rojo). Con una sola linea, va arriba.
void drawBattleLog(const char *l1, uint16_t c1, const char *l2, uint16_t c2) {
  if (!l1) {
    l1 = l2;
    c1 = c2 == Y_TEXT_DIM ? Y_TEXT : c2;
    l2 = nullptr;
  }
  setSize(2);
  if (l1) {
    gfx->setTextColor(c1);
    setCur(centerX(l1, 2), BT_LOG_Y);
    printT(l1);
  }
  if (l2) {
    gfx->setTextColor(c2);
    setCur(centerX(l2, 2), BT_LOG_Y + BT_LOG_GAP);
    printT(l2);
  }
}

// rotulos del ultimo golpe (desde que llega hasta el siguiente)
void drawBattleCaption(uint32_t now) {
  const char *l1 = nullptr, *l2 = nullptr;
  uint16_t c1 = Y_TEXT, c2 = Y_TEXT_DIM;
  bool hint = false;
  bool hit = bt.haveEv && now - bt.evAt >= BT_HIT_MS;
  if (!hit) {
    if (!bt.haveEv && bt.phase == BT_FIGHT) { l2 = T(S_TAP_CHARGE); hint = true; }  // pista hasta el primer golpe
  } else if (!bt.ev.eff4) {
    l1 = T(S_NO_EFFECT);
  } else {
    if (bt.ev.special) { l1 = T(S_SPECIAL); c1 = Y_HILITE; }
    else if (bt.ev.crit) l1 = T(S_CRITICAL);
    if (bt.ev.eff4 >= 8) l2 = T(S_SUPER_EFF);
    else if (bt.ev.eff4 <= 2) l2 = T(S_NOT_EFF);
  }
  // muerte subita: este ataque o el que viene ya cuentan (la partida no sale de ella)
  if (bt.haveEv && bt.run.eng.attacks > 0 && bt.run.eng.attacks >= SUDDEN_DEATH_AT) {
    if (!l1) { l1 = T(S_SUDDEN_DEATH); c1 = Y_ACCENT_BAD; }
    else { l2 = T(S_SUDDEN_DEATH); c2 = Y_ACCENT_BAD; }
  }
  if (bt.haveEv && bt.ev.recoil && now - bt.evAt >= BT_RECOIL_MS) { l2 = T(S_RECOIL); c2 = Y_ACCENT_BAD; }
  if (hint && !l1) { l1 = l2; c1 = Y_TEXT_DIM; l2 = nullptr; }  // la pista se queda atenuada
  drawBattleLog(l1, c1, l2, c2);
}

// el relevo, en el registro: "Siguiente rival" (y JEFE en rojo si lo es) o "Adelante, X!"
void drawSwapCaption() {
  char go[40];
  const char *l1, *l2 = nullptr;
  if (bt.swap == SN_NEXT_FOE) {
    l1 = T(S_NEXT_RIVAL);
    if (bt.run.isBoss()) l2 = T(S_BOSS);
  } else {
    snprintf(go, sizeof(go), T(S_GO_FMT), battleName(0));
    l1 = go;
  }
  drawBattleLog(l1, Y_TEXT, l2, Y_ACCENT_BAD);
}

// Dibuja un frame del combate. false = se acaba de cerrar (render pinta la casa).
bool renderBattle() {
  battleStep(millis());
  uint32_t now = millis();  // despues: un relevo que acaba de leer la SD empieza ahora
  if (bt.phase == BT_OFF) return false;
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
  } else if (bt.phase == BT_SWAP) {  // relevo: el caido ya no esta y el nuevo entra desde su lado
    uint8_t s = bt.swap == SN_NEXT_FOE ? 1 : 0;
    int32_t w = (int32_t)(now - bt.t0) - (BT_SWAP_MS - BT_SWAP_WALK_MS);
    if (w < 0) {
      hidden[s] = true;
    } else {
      float p = (float)w / BT_SWAP_WALK_MS;
      if (p > 1) p = 1;
      x[s] = s ? (int)(536 - (536 - BT_FOE_X) * p) : (int)(-70 + (BT_ME_X + 70) * p);
      act[s] = s ? PMD_WALKL : PMD_WALKR;
    }
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
    if (dt >= BT_HIT_MS && dt < BT_HIT_MS + 500 && bt.ev.damage) {  // encaja el golpe
      act[d] = PMD_HURT;
      at[d] = dt - BT_HIT_MS;
      x[d] += (int)(4 * sinf(dt * 0.12f));
    }
    if (bt.ev.recoil && dt >= BT_RECOIL_MS && dt < BT_RECOIL_MS + 500) {  // muerte subita: se hace dano a si mismo
      act[a] = PMD_HURT;
      at[a] = dt - BT_RECOIL_MS;
      x[a] += (int)(4 * sinf(dt * 0.12f));
    }
    // cae quien se queda a 0: el defensor, o (retroceso) el propio atacante
    for (uint8_t sd = 0; sd < 2; sd++) {
      if (bt.ev.hp[sd] != 0) continue;
      uint32_t t0 = (sd == a && bt.ev.recoil) ? BT_RECOIL_MS : BT_HIT_MS;
      if (dt < t0 + 400) continue;
      uint32_t f = dt - t0 - 400;  // se desvanece y se hunde
      if (f >= BT_FAINT_MS) hidden[sd] = true;
      sil[sd] = (f / 90) % 2;
      g[sd] += (int)(f * 40 / BT_FAINT_MS);
      act[sd] = PMD_HURT;
    }
    if (bt.ev.special && dt >= BT_HIT_MS && dt < BT_HIT_MS + 350) {  // destello y temblor
      shake = (int)(8 * sinf(dt * 0.15f));
      flash = dt < BT_HIT_MS + 90;
    }
  }

  btPanelBg();
  drawBattleHeader();
  drawBattleWindow(BT_WIN_Y, BT_WIN_H, BT_HORIZON, shake, false);
  btSetClip(BT_WIN_X, BT_WIN_Y, BT_WIN_W, BT_WIN_H);
  // el rival primero (esta mas lejos); en pleno ataque, el atacante encima
  uint8_t order[2] = { 1, 0 };
  if (bt.haveEv && bt.phase == BT_FIGHT) {
    order[0] = 1 - bt.ev.attacker;
    order[1] = bt.ev.attacker;
  }
  for (uint8_t side : order)
    if (!hidden[side])
      drawFighter(side, act[side], x[side] + shake, g[side], at[side], loop[side], sil[side], side == 0 ? 3 : 2);
  if (flash) gfx->fillRoundRect(BT_WIN_X, BT_WIN_Y, BT_WIN_W, BT_WIN_H, BT_WIN_R, UI_WHITE);
  drawFoeTag();
  gfx->drawRoundRect(BT_WIN_X, BT_WIN_Y, BT_WIN_W, BT_WIN_H, BT_WIN_R, Y_CARD_HI);
  drawHpRow(1, battleShownHp(1, now));
  drawHpRow(0, battleShownHp(0, now));
  if (bt.phase == BT_FIGHT) {
    drawSpecialMeter(now);
    drawBattleCaption(now);
  } else if (bt.phase == BT_SWAP) {
    drawSwapCaption();
  }
  drawTeamPips(CX - (bt.run.teamN - 1) * BT_PIP_PITCH / 2, BT_PIP_Y, now);
  gfx->flush();
  return true;
}

// Resultado: arriba la fase; el titular en verde (superada) o rojo (perdida); en la ventana posa el
// que quedo en pie (tu aliado si se supero, el rival si no) y debajo, en una tarjeta, lo que pago
// stageDone: el objeto del jefe, la insignia nueva, el premio del equipo y la felicidad. Un toque,
// la tecla USER o BT_END_MS lo cierran.
void drawBattleResult(uint32_t now) {
  btPanelBg();
  drawBattleHeader();
  uint32_t t = now - bt.t0;
  const char *head = bt.wild ? T(bt.cleared ? S_WILD_WON : S_WILD_LOST) : T(bt.cleared ? S_STAGE_CLEAR : S_STAGE_LOST);
  setSize(3);
  uint8_t ts = textW(head, 3) <= 330 ? 3 : 2;
  setSize(ts);
  gfx->setTextColor(bt.cleared ? Y_ACCENT_GO : Y_ACCENT_BAD);
  setCur(centerX(head, ts), BT_RES_HEAD_Y + (ts == 3 ? 0 : 4));
  printT(head);
  drawBattleWindow(BT_RES_WIN_Y, BT_RES_WIN_H, BT_RES_WIN_Y + 44, 0, true);
  btSetClip(BT_WIN_X, BT_RES_WIN_Y, BT_WIN_W, BT_RES_WIN_H);
  drawFighter(bt.cleared ? 0 : 1, PMD_POSE, CX, BT_RES_GROUND, t, true, false, 3);
  gfx->drawRoundRect(BT_WIN_X, BT_RES_WIN_Y, BT_WIN_W, BT_RES_WIN_H, BT_WIN_R, Y_CARD_HI);
  // la tarjeta: solo lo que de verdad se dio. El objeto del jefe con la cajita y "Regalo en casa!" (lo
  // que es se ve al abrirlo); con la casa vacia va a la mochila, y se ve el objeto con "MOCHILA"
  const char *ln[4];
  uint16_t lc[4];
  const char *const *icon[4] = { nullptr, nullptr, nullptr, nullptr };
  uint8_t n = 0;
  char joy[40];
  if (bt.wild) {  // v2.2: la hora de crecimiento, el objeto que solto (a la mochila) y la felicidad
    if (bt.erw.won) { ln[n] = T(S_WILD_GROW); lc[n++] = Y_ACCENT_GO; }
    if (bt.erw.item < IT_COUNT) {
      icon[n] = ITEM_ICON[bt.erw.item];
      ln[n] = T(bt.erw.itemLost ? S_BACKPACK_FULL : S_BACKPACK);
      lc[n++] = bt.erw.itemLost ? Y_ACCENT_BAD : Y_HILITE;
    }
    if (bt.erw.joy) {
      snprintf(joy, sizeof(joy), T(S_PRACTICE_JOY_FMT), (unsigned)bt.erw.joy);
      ln[n] = joy;
      lc[n++] = Y_ACCENT_GO;
    }
  } else {
    if (bt.rw.item < IT_COUNT) {
      bool bag = pet.isVacant();
      icon[n] = bag ? ITEM_ICON[bt.rw.item] : SPR_GIFT;
      ln[n] = T(bag ? S_BACKPACK : S_BATTLE_GIFT);
      lc[n++] = Y_HILITE;
    }
    if (bt.rw.newBadge) { icon[n] = BADGE_ICON[bt.run.stage]; ln[n] = T(S_NEW_BADGE); lc[n++] = Y_HILITE; }
    if (bt.rw.paid) { ln[n] = T(S_TEAM_REWARD); lc[n++] = Y_TEXT; }
    if (bt.rw.joy) {
      snprintf(joy, sizeof(joy), T(S_PRACTICE_JOY_FMT), (unsigned)bt.rw.joy);
      ln[n] = joy;
      lc[n++] = Y_ACCENT_GO;
    }
  }
  // y la pista "toca: volver" en su ultima fila, reservada desde el principio (la tarjeta no crece)
  bool hint = t >= BT_END_LOCK_MS;
  bool coinRow = !bt.wild && bt.rw.coins > 0;  // las monedas (Y4): moneda + "+N", en su fila (un salvaje no da)
  if (!n && !hint && !coinRow) return;
  setSize(2);
  int bw = textW(T(S_BACK), 2), bh = 12 + 20 + (coinRow ? 22 : 0);
  for (uint8_t i = 0; i < n; i++) {
    int w = textW(ln[i], 2) + (icon[i] ? 40 : 0);
    if (w > bw) bw = w;
    bh += icon[i] ? 32 : 20;
  }
  bw += 28;
  int y = BT_RES_CARD_Y;
  yCard(CX - bw / 2, y, bw, bh, false);
  int ly = y + 6;
  for (uint8_t i = 0; i < n; i++) {
    int w = textW(ln[i], 2) + (icon[i] ? 40 : 0), lx = CX - w / 2;
    if (icon[i]) {
      drawMap(icon[i], 16, lx, ly, 2, false);
      lx += 40;
    }
    gfx->setTextColor(lc[i]);
    setCur(lx, ly + (icon[i] ? 9 : 3));
    printT(ln[i]);
    ly += icon[i] ? 32 : 20;
  }
  if (coinRow) {
    drawCoinGain(ly + 1, bt.rw.coins, Y_TEXT);
    ly += 22;
  }
  if (hint) {  // ya se puede cerrar con un toque (v1.38)
    gfx->setTextColor(Y_TEXT_DIM);
    setCur(centerX(T(S_BACK), 2), ly + 3);
    printT(T(S_BACK));
  }
}

// ---------- encuentros salvajes (v2.2, encounter.h) ----------
// Tocar lo que se mueve en el patio abre esta pantalla: dos bandas negras (arriba y abajo) entran como en el
// cine y en la franja del medio, sobre el mapa, el salvaje en un circulo. Un toque: el selector de equipo
// (un luchador) y el combate (bt.wild). Ganado: la misma pantalla pregunta por la Poke Ball (Lanzar / NO);
// lanzarla la anima (vuela, se cierra, tres sacudidas y click) y el huevo va a la mochila. Perdido: huyo.
#define ENC_BAND_H 138       // alto de cada banda negra
#define ENC_BAND_MS 350      // las bandas entran...
#define ENC_RING_MS 650      // ...y el circulo crece hasta aqui
#define ENC_R 80             // radio del circulo
#define ENC_GROUND (CY + 58) // los pies del salvaje en el circulo
#define ENC_PILL_Y 362
#define ENC_PILL_GAP 24

void encLoadWild() {
  foePmd.unload();
  if (encShowDex >= 1 && encShowDex <= DEX_COUNT) foePmd.load((uint8_t)encShowDex, encShowShiny);
}

void closeEncUi() {
  encUi = ENCU_OFF;
  encDenyUntil = 0;
  if (!battleOpen()) foePmd.unload();
}

// desde el patio: el salvaje (o, ya vencido, la pregunta de la Poke Ball)
void openEncounter() {
  if (!pet.encShown()) return;
  encShowDex = pet.encDex();
  encShowLv = pet.encLevel();
  encShowShiny = pet.encShiny();
  encLoadWild();  // leer la SD bloquea el loop: antes de que empiece la animacion
  encUi = pet.encWonPending() ? ENCU_BALL : ENCU_INTRO;
  encUiAt = millis();
  encDenyUntil = 0;
  sfxPlay(encUi == ENCU_INTRO ? SFX_WILD : SFX_TAP);
  wakeUI();
  Serial.printf("ENC open %s #%d nv.%u\n", encUi == ENCU_INTRO ? "intro" : "ball", encShowDex, encShowLv);
}

// al cerrar el resultado de su combate (closeBattle): ganado, la Poke Ball; perdido, huyo
void openEncUiAfterBattle(bool won) {
  encUi = (won && pet.encWonPending()) ? ENCU_BALL : ENCU_DONE;
  encDoneKind = ENCD_FLED;
  encUiAt = millis();
  encDenyUntil = 0;
}

static void encDone(uint8_t kind) {
  encUi = ENCU_DONE;
  encDoneKind = kind;
  encUiAt = millis();
}

// las dos pastillas de la pregunta (Lanzar / NO), centradas como pareja
static void encBallPills(int &xT, int &wT, int &xN, int &wN) {
  wT = yPillWidth(T(S_THROW));
  wN = yPillWidth(T(S_NO));
  xT = CX - (wT + ENC_PILL_GAP + wN) / 2;
  xN = xT + wT + ENC_PILL_GAP;
}

void encTap(int16_t x, int16_t y) {
  uint32_t now = millis();
  if (now - encUiAt < ENC_TAP_LOCK_MS) return;  // el toque que la abrio (o uno de mas) no cuenta
  if (encUi == ENCU_INTRO) {
    closeEncUi();
    if (!pet.encActive()) return;  // se fue mientras se miraba (6 h)
    sfxPlay(SFX_TAP);
    openTeamWild();
    return;
  }
  if (encUi == ENCU_BALL) {
    int xT, wT, xN, wN;
    encBallPills(xT, wT, xN, wN);
    if (yPillHit(x, y, xT, ENC_PILL_Y, wT)) {
      if (pet.captureBlock() != CAP_OK || !pet.captureWild()) {
        sfxPlay(SFX_DENY);
        encDenyUntil = now + 500;
        return;
      }
      encUi = ENCU_THROW;  // ya esta guardado: la animacion solo lo cuenta
      encUiAt = now;
      encSndStep = 0;
      sfxPlay(SFX_TAP);
    } else if (yPillHit(x, y, xN, ENC_PILL_Y, wN)) {
      pet.releaseWild();
      sfxPlay(SFX_TAP);
      encDone(ENCD_LEFT);
    }
    return;
  }
  if (encUi == ENCU_DONE) {
    sfxPlay(SFX_TAP);
    closeEncUi();
  }
}

// el salvaje dentro de su circulo (recortado), o la miniatura, o "?"
static void encDrawWild(uint8_t act, int dx, uint32_t t, bool loop) {
  btSetClip(CX - ENC_R, CY - ENC_R, 2 * ENC_R, 2 * ENC_R);
  if (foePmd.loaded) {
    if (!foePmd.has(act)) act = PMD_IDLE;
    btPmd(foePmd, act, CX + dx, ENC_GROUND, t, loop, false, 3);
  } else if (const uint8_t *th = thumbs.get(encShowDex)) {
    btThumb(th, CX + dx - GAL_CELL / 2, ENC_GROUND - GAL_CELL, 2, false);
  } else {
    gfx->setTextColor(UI_INK);
    setSize(6);
    setCur(CX - 18 + dx, CY - 30);
    printT("?");
  }
  btSetClip(0, 0, 466, 466);
}

// el fondo de todos los pasos: el mapa, las bandas (entran en ENC_BAND_MS) y el circulo (crece hasta ENC_RING_MS)
static int encFrame(uint32_t t, bool animate) {
  yardDrawBg();
  int bh = ENC_BAND_H;
  if (animate && t < ENC_BAND_MS) bh = (int)(ENC_BAND_H * (float)t / ENC_BAND_MS);
  gfx->fillRect(0, 0, 466, bh, RGB565_BLACK);
  gfx->fillRect(0, 466 - bh, 466, bh, RGB565_BLACK);
  int r = ENC_R;
  if (animate) {
    if (t < ENC_BAND_MS) return 0;
    if (t < ENC_RING_MS) r = (int)(ENC_R * (float)(t - ENC_BAND_MS) / (ENC_RING_MS - ENC_BAND_MS));
  }
  uint16_t acc = (encShowDex >= 1 && encShowDex <= DEX_COUNT) ? DEX_TBL[encShowDex].accent : Y_CARD;
  gfx->fillCircle(CX, CY, r + 4, UI_WHITE);
  if (r < 1) return r;
  gfx->fillCircle(CX, CY, r, lerp565(acc, UI_WHITE, 11, 16));
  if (r >= 16) gfx->fillEllipse(CX, ENC_GROUND - 3, r * 5 / 8, r / 7 + 1, lerp565(acc, UI_WHITE, 7, 16));  // su tarima
  return r;
}

// un destello en cruz (como al limpiar una caca)
static void encSpark(int x, int y, int s, uint16_t col) {
  gfx->fillRect(x - s, y - 1, 2 * s + 1, 3, col);
  gfx->fillRect(x - 1, y - s, 3, 2 * s + 1, col);
}

// el shiny brilla dentro del circulo: tres destellos que giran
static void encShinySparks(uint32_t now) {
  if (!encShowShiny) return;
  for (int k = 0; k < 3; k++) {
    float a = now * 0.002f + k * 2.094f;
    int x = CX + (int)((ENC_R - 16) * cosf(a)), y = CY + (int)((ENC_R - 16) * sinf(a));
    encSpark(x, y, 4 + (int)((now / 120 + k) % 3), (k & 1) ? UI_BAR_WARN : UI_WHITE);
  }
}

// el nombre con el "*" del shiny delante (naranja), centrado
static void encNameLine(const char *s, int y, uint16_t col) {
  setSize(2);
  int w = textW(s, 2), sw = encShowShiny ? textW("* ", 2) : 0, x = CX - (w + sw) / 2;
  if (encShowShiny) {
    gfx->setTextColor(UI_BAR_WARN);
    setCur(x, y);
    printT("* ");
  }
  gfx->setTextColor(col);
  setCur(x + sw, y);
  printT(s);
}

void renderEncounter(uint32_t now) {
  uint32_t t = now - encUiAt;
  char line[64];
  if (encUi == ENCU_INTRO) {
    int r = encFrame(t, true);
    if (r == ENC_R) {
      bool posing = t < ENC_RING_MS + 1400;
      encDrawWild(posing ? PMD_POSE : PMD_IDLE, 0, posing ? t - ENC_RING_MS : t, !posing);
      encShinySparks(now);
    }
    if (t >= ENC_BAND_MS) {
      snprintf(line, sizeof(line), T(S_WILD_APPEARED_FMT), dexName(encShowDex));
      encNameLine(line, 98, Y_TEXT);
      char lv[12];
      snprintf(lv, sizeof(lv), T(S_LVL_FMT), (unsigned)encShowLv);
      gfx->setTextColor(Y_TEXT);
      setSize(2);
      setCur(centerX(lv, 2), 340);
      printT(lv);
      if (t >= ENC_TAP_LOCK_MS && (now / 500) % 3 != 0) {  // la pista late
        gfx->setTextColor(Y_TEXT_DIM);
        setCur(centerX(T(S_WILD_TAP), 2), 372);
        printT(T(S_WILD_TAP));
      }
    }
  } else if (encUi == ENCU_BALL) {
    encFrame(t, false);
    encDrawWild(PMD_IDLE, 0, now, true);
    encShinySparks(now);
    encNameLine(dexName(encShowDex), 76, Y_TEXT_DIM);
    gfx->setTextColor(Y_TEXT);
    printTight(T(S_THROW_BALL), CX, 102, 380);
    uint8_t why = pet.captureBlock();
    const char *note;
    char left[24];
    if (why == CAP_NO_BALL) note = T(S_NO_BALLS);
    else if (why == CAP_NO_ROOM) note = T(S_NO_ROOM);
    else {
      snprintf(left, sizeof(left), T(S_BALLS_LEFT_FMT), (unsigned)pet.items[IT_BALL]);
      note = left;
    }
    int nx = CX - (textW(note, 2) + 28) / 2;
    drawMap(SPR_ICON_PLAY, 16, nx, 332, 1, false);
    gfx->setTextColor(why == CAP_OK ? Y_TEXT : Y_ACCENT_BAD);
    setSize(2);
    setCur(nx + 28, 333);
    printT(note);
    int xT, wT, xN, wN;
    encBallPills(xT, wT, xN, wN);
    int shake = timeLeft(encDenyUntil) ? (int)(5 * sinf(now * 0.08f)) : 0;
    yPill(xT + shake, ENC_PILL_Y, T(S_THROW), why == CAP_OK ? Y_ACCENT_GO : Y_CARD_HI);
    yPill(xN, ENC_PILL_Y, T(S_NO), Y_ACCENT_BAD);
  } else if (encUi == ENCU_THROW) {
    uint8_t &sndStep = encSndStep;
    encFrame(t, false);
    gfx->setTextColor(Y_TEXT);
    printTight(dexName(encShowDex), CX, 98, 380);
    int bx = CX, by = CY + 18;  // donde se queda la bola
    if (t < 500) {  // vuela en arco desde abajo
      float p = t / 500.0f;
      encDrawWild(PMD_IDLE, 0, now, true);
      bx = CX + (int)(60 * (1 - p));
      by = (int)(430 + (CY - 10 - 430) * p - 160 * p * (1 - p));
    } else if (t < 760) {  // se cierra: destello blanco
      if (sndStep < 1) { sndStep = 1; sfxPlay(SFX_STORE); }
      uint8_t k = (uint8_t)(16 - (t - 500) * 16 / 260);
      gfx->fillCircle(CX, CY, ENC_R, lerp565(lerp565(DEX_TBL[encShowDex].accent, UI_WHITE, 11, 16), UI_WHITE, k, 16));
    } else if (t < 2100) {  // tres sacudidas
      uint32_t w = (t - 760) % 450;
      uint8_t n = (uint8_t)((t - 760) / 450);
      if (w < 260) bx += (int)(9 * sinf(w * 0.048f));
      if (w < 30 && sndStep < 2 + n) { sndStep = 2 + n; sfxPlay(SFX_TAP); }
    } else {  // click: estrellas
      if (sndStep < 10) { sndStep = 10; sfxPlay(SFX_LEVELUP); }
      int d = 20 + (int)((t - 2100) / 8);
      for (int k = 0; k < 4; k++)
        drawMap(SPR_STAR, 16, bx - 8 + ((k & 1) ? d : -d), by - 8 + ((k & 2) ? d / 2 : -d / 2) - 10, 1, false);
    }
    drawMap(SPR_ICON_PLAY, 16, bx - 24, by - 24, 3, false);
    if (t >= ENC_THROW_MS) encDone(ENCD_CAUGHT);
  } else {  // ENCU_DONE
    encFrame(t, false);
    if (encDoneKind == ENCD_CAUGHT) {
      snprintf(line, sizeof(line), T(S_CAPTURED_FMT), dexName(encShowDex));
      int bob = (int)(3 * sinf(now * 0.006f));
      drawEggAt(CX, CY + 6 + bob, 2, 0, DEX_TBL[encShowDex].base);  // el huevo de captura: su forma base
      drawMap(SPR_ICON_PLAY, 16, CX + 26, CY + 30, 2, false);
      encShinySparks(now);
      gfx->setTextColor(Y_TEXT_DIM);
      printTight(T(S_CAPTURE_EGG), CX, 340, 380);
    } else {
      snprintf(line, sizeof(line), T(encDoneKind == ENCD_FLED ? S_WILD_FLED_FMT : S_WILD_LEFT_FMT), dexName(encShowDex));
      if (t < 1200) encDrawWild(PMD_WALKR, (int)(t / 8), t, true);  // se va andando
    }
    encNameLine(line, 98, encDoneKind == ENCD_FLED ? Y_ACCENT_BAD : Y_TEXT);
    if (t >= ENC_TAP_LOCK_MS) {
      gfx->setTextColor(Y_TEXT_DIM);
      setSize(2);
      setCur(centerX(T(S_BACK), 2), 372);
      printT(T(S_BACK));
    }
  }
  gfx->flush();
}
