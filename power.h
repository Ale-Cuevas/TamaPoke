#pragma once
#include <stdint.h>

// Politica de consumo. Todo lo que decide cuanto trabaja la placa vive aqui como
// funciones puras (sin hardware), para poder probarlas en el PC (test/test_power.cpp).
//
// Donde se gasta la bateria: la pantalla AMOLED (brillo y contenido), la CPU mientras
// dibuja y vuelca el lienzo por QSPI (~70 ms de cada 100 a 10 fps), el amplificador del
// altavoz si esta encendido sin sonar, y los despertares del bucle principal.

// Escalones de inactividad (ms desde el ultimo toque o evento visible)
#define POWER_CALM_MS 60000UL   // pasado este tiempo, se dibuja algo menos
#define POWER_DIM1_MS 90000UL   // pantalla atenuada
#define POWER_DIM2_MS 300000UL  // pantalla casi apagada

enum DimStage : uint8_t { DIM_AWAKE = 0, DIM_LOW = 1, DIM_OFF = 2 };

// Niveles de brillo que elige el jugador (tecla BOOT o la pildora del reloj), v1.23.
// El nivel fija el brillo despierto; los escalones de atenuacion nunca lo superan.
// Hasta la v1.22 era fijo: 145 con bateria y 180 con USB.
#define BRIGHT_LEVELS 4
#define BRIGHT_DEFAULT 2
static const uint8_t BRIGHT_VALUE[BRIGHT_LEVELS] = { 50, 100, 150, 220 };
inline uint8_t brightLevelValue(uint8_t level) {
  return BRIGHT_VALUE[level < BRIGHT_LEVELS ? level : BRIGHT_DEFAULT];
}
inline uint8_t nextBrightLevel(uint8_t level) {
  return level + 1 < BRIGHT_LEVELS ? level + 1 : 0;  // cicla: el maximo vuelve al minimo
}

struct PowerCtx {
  uint32_t idleMs;    // ms sin interaccion
  bool screenOff;     // apagada con el boton PWR
  bool sleeping;      // el bicho duerme
  bool gameOpen;      // minijuego o saco: la fisica esta afinada a 85 ms por frame
  bool usb;           // alimentacion externa
  bool charging;
  int batPct;         // 0-100, -1 si no hay bateria
  uint8_t brightLevel;  // nivel elegido, 0..BRIGHT_LEVELS-1
  bool faceDown;      // boca abajo: nadie mira, la pantalla casi apagada como antes
};

// ms desde `last`, sin desbordar si `last` es posterior a `now` (un toque atendido despues de
// leer el reloj al principio del bucle): se tomaba como 49 dias de inactividad y el panel
// pasaba un instante a brillo 8 en cada toque
inline uint32_t idleSince(uint32_t now, uint32_t last) {
  int32_t d = (int32_t)(now - last);
  return d < 0 && d > -1000 ? 0 : now - last;  // solo un adelanto de unos ms cuenta como "ahora"; 25 dias sin tocar siguen siendo 25 dias
}

inline uint8_t dimStageFor(uint32_t idleMs) {
  return idleMs > POWER_DIM2_MS ? DIM_OFF : idleMs > POWER_DIM1_MS ? DIM_LOW : DIM_AWAKE;
}

// bateria baja: se ahorra mas (solo si de verdad depende de la bateria)
// Salvapantallas (v1.28, burnin.h): a los 5 min sin tocar (DIM_OFF) la pantalla ya no es la
// escena entera a brillo 8 sino un fondo negro con el bicho y la hora, que se ven mejor algo
// mas brillantes. Casi todo es negro (pixeles apagados), asi que gasta poco igualmente.
// v1.31: el mismo brillo con el bicho dormido (antes 16, y no se veia bien).
#define SAVER_BRIGHT 40
inline bool saverActive(const PowerCtx &c) {
  return !c.screenOff && !c.faceDown && dimStageFor(c.idleMs) == DIM_OFF;
}

// Boton PWR, pulsacion corta (v1.32): recorre tres vistas, juego -> salvapantallas -> apagada
// -> juego. Hasta la v1.31 solo encendia y apagaba. La vista sale del estado de siempre: apagada
// con PWR, salvapantallas (el de los 5 min o el puesto con PWR) o el juego (tambien atenuado a
// los 90 s o boca abajo, que no son el salvapantallas).
enum PwrView : uint8_t { VIEW_ON = 0, VIEW_SAVER, VIEW_OFF };
inline PwrView pwrViewOf(const PowerCtx &c) {
  return c.screenOff ? VIEW_OFF : saverActive(c) ? VIEW_SAVER : VIEW_ON;
}
inline PwrView pwrNext(PwrView v) {
  return v == VIEW_ON ? VIEW_SAVER : v == VIEW_SAVER ? VIEW_OFF : VIEW_ON;
}
// inactividad que pone el salvapantallas en el acto (como tras 5 min sin tocar)
#define POWER_SAVER_IDLE_MS (POWER_DIM2_MS + 1)

inline bool batteryLow(const PowerCtx &c) { return c.batPct >= 0 && c.batPct < 20 && !c.charging && !c.usb; }
inline bool batteryCritical(const PowerCtx &c) { return c.batPct >= 0 && c.batPct < 10 && !c.charging && !c.usb; }

// brillo del panel (0-255): el del nivel elegido, menos al atenuarse (sin pasar nunca del
// nivel), y con un tope cuando queda poca bateria. v1.31: dormido o despierto da igual; antes
// el bicho dormido bajaba la pantalla a 25 (10 atenuada, 16 el salvapantallas) y costaba verla.
inline uint8_t brightnessFor(const PowerCtx &c) {
  if (c.screenOff) return 0;
  uint8_t stage = dimStageFor(c.idleMs);
  uint8_t lv = brightLevelValue(c.brightLevel);
  uint8_t t = lv;
  if (stage == DIM_LOW) t = 60;
  else if (stage == DIM_OFF) t = saverActive(c) ? SAVER_BRIGHT : 8;
  if (t > lv) t = lv;
  if (batteryCritical(c) && t > 60) t = 60;
  else if (batteryLow(c) && t > 100) t = 100;
  return t;
}

// ms entre frames; 0 = no se dibuja (pantalla apagada). Cada frame cuesta un render
// completo y el volcado del lienzo por QSPI, y el panel solo cambia si se redibuja.
inline uint32_t frameIntervalMs(const PowerCtx &c) {
  if (c.screenOff) return 0;
  if (c.gameOpen) return 85;  // el juego manda: su fisica esta afinada a este periodo
  uint32_t ms;
  uint8_t stage = dimStageFor(c.idleMs);
  if (stage == DIM_OFF) ms = 1000;
  else if (c.sleeping && c.idleMs > POWER_CALM_MS) ms = 500;  // dormido y sin tocar: casi nada se mueve
  else if (stage == DIM_LOW) ms = 250;
  else ms = c.idleMs > POWER_CALM_MS ? 150 : 100;
  if (batteryLow(c)) ms *= 2;
  return ms;
}

// el amplificador del altavoz consume en reposo: solo se tiene encendido mientras
// alguien puede estar jugando
inline bool amplifierWanted(const PowerCtx &c) {
  return !c.screenOff && !c.sleeping && dimStageFor(c.idleMs) == DIM_AWAKE;
}

// pausa del bucle principal en un pase que no dibuja
inline uint32_t idleDelayMs(const PowerCtx &c) { return c.screenOff ? 20 : 1; }

// con la pantalla apagada, o casi apagada (5 min sin tocar), y sin USB se duerme la CPU (light
// sleep) entre frames y sondeos; con USB no: se perderia el puerto serie de la tarjeta SD
inline bool lightSleepAllowed(const PowerCtx &c) {
  return !c.usb && (c.screenOff || dimStageFor(c.idleMs) == DIM_OFF);
}
#define POWER_SLEEP_MS 200  // duracion de cada sueno; el toque despierta antes

// cada cuanto se lee la bateria por I2C
inline uint32_t powerCacheMs(const PowerCtx &c) {
  return (c.screenOff || dimStageFor(c.idleMs) == DIM_OFF) ? 10000 : 2000;
}
