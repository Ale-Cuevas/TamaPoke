#pragma once
// Los viajes y la coleccion del patio (v2.0, fase Y5 parte B): la geometria y la logica pura de las
// pantallas (comprobadas en test/test_travel_ui.cpp: lo tocable mide >= 48 px y cae dentro del
// circulo). El dibujo y las acciones estan en TamaPoke.ino ("viajes").
//
// Pantalla de salida (panel PANEL_TRAVEL sin viaje): arriba una ventana con el destino (< >), debajo
// los bichos de la caja que pueden viajar (3 por pagina), las 3 duraciones con su recompensa y el boton
// Ir. Con un viaje en marcha: cuenta atras, ventana con el bicho y el registro de sucesos.
// Coleccion (3.a pestana de la mochila): rejilla de 5 x 5 (una fila por mapa, el raro a la derecha).
#include <stdint.h>
#include <stdio.h>
#include "yard_ui.h"
#include "travel.h"

// ---------- texto ----------
// cuenta atras: H:MM:SS, o M:SS por debajo de la hora
inline void trvClockStr(char *out, int n, uint32_t secs) {
  if (secs >= 3600) snprintf(out, n, "%u:%02u:%02u", (unsigned)(secs / 3600), (unsigned)(secs / 60 % 60), (unsigned)(secs % 60));
  else snprintf(out, n, "%u:%02u", (unsigned)(secs / 60), (unsigned)(secs % 60));
}
// lo que falta en la caja: H:MM (los minutos, hacia arriba: nunca "0:00" con el viaje en marcha)
inline void trvHMStr(char *out, int n, uint32_t secs) {
  uint32_t m = (secs + 59) / 60;
  snprintf(out, n, "%u:%02u", (unsigned)(m / 60), (unsigned)(m % 60));
}
// la hora del RTC (epoch "local como UTC", como el resto del firmware) como HH:MM
inline void trvTimeStr(char *out, int n, uint32_t epoch) {
  uint32_t s = epoch % 86400;
  snprintf(out, n, "%02u:%02u", (unsigned)(s / 3600), (unsigned)(s / 60 % 60));
}
// el dia del epoch como DD/MM (algoritmo civil_from_days de H. Hinnant)
inline void trvDateStr(char *out, int n, uint32_t epoch) {
  int64_t z = (int64_t)(epoch / 86400) + 719468;
  int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  uint32_t doe = (uint32_t)(z - era * 146097);
  uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  uint32_t mp = (5 * doy + 2) / 153;
  uint32_t d = doy - (153 * mp + 2) / 5 + 1;
  uint32_t m = mp < 10 ? mp + 3 : mp - 9;
  snprintf(out, n, "%02u/%02u", (unsigned)d, (unsigned)m);
}
// horas de una duracion (1, 3, 8)
inline uint8_t trvTierHours(uint8_t tier) { return (uint8_t)(TRV_MIN[tier < TRV_TIERS ? tier : TRV_TIERS - 1] / 60); }

// ---------- el registro del viaje ----------
// sucesos ya ocurridos (su minuto <= los cumplidos), en orden; devuelve cuantos (<= TRV_EVENTS_MAX)
inline uint8_t trvVisibleEvents(uint32_t seed, uint8_t dest, uint8_t tier, uint16_t elapsedMin, uint8_t *idx) {
  uint8_t n = travelEventCount(tier), c = 0;
  for (uint8_t i = 0; i < n && c < TRV_EVENTS_MAX; i++) {
    uint16_t off;
    uint8_t tpl, arg;
    travelEvent(seed, dest, tier, i, &off, &tpl, &arg);
    if (off <= elapsedMin) idx[c++] = i;
  }
  return c;
}

// ---------- la seleccion ----------
// bichos de la caja que pueden salir: los huecos con bit en okMask (bit k = hueco k en estado BOX_OK)
inline uint8_t trvCandidates(uint8_t okMask, uint8_t *out) {
  uint8_t n = 0;
  for (uint8_t k = 0; k < 8; k++)
    if (okMask & (1u << k)) out[n++] = k;
  return n;
}
// el siguiente mapa propio (ownedMask: bit n-1 = mapa n) en la direccion dir (+1 / -1), dando la vuelta;
// cur si no hay otro
inline uint8_t trvStepDest(uint8_t ownedMask, uint8_t cur, int dir) {
  for (int i = 1; i <= TRV_DESTS; i++) {
    int m = ((int)cur - 1 + dir * i) % TRV_DESTS;
    if (m < 0) m += TRV_DESTS;
    if (ownedMask & (1u << m)) return (uint8_t)(m + 1);
  }
  return cur;
}
#define TRV_PET_PAGE 3
inline uint8_t trvPetPages(uint8_t n) { return n ? (uint8_t)((n + TRV_PET_PAGE - 1) / TRV_PET_PAGE) : 1; }

// ---------- geometria de la pantalla de salida ----------
#define TRV_BAN_X 103      // la ventana del destino
#define TRV_BAN_Y 76
#define TRV_BAN_W 260
#define TRV_BAN_H 60
#define TRV_DEST_HIT_W 68  // zonas de las flechas del destino, en los extremos de la ventana (dentro del circulo util)
#define TRV_DEST_HIT_Y 82
#define TRV_DEST_HIT_H 48
#define TRV_DEST_HIT_LX (TRV_BAN_X - 23)
#define TRV_DEST_HIT_RX (TRV_BAN_X + TRV_BAN_W - 45)
#define TRV_PET_W 92       // tarjetas de bicho
#define TRV_PET_H 74
#define TRV_PET_GAP 6
#define TRV_PET_Y 148
#define TRV_PET_ARROW_X 58 // flechas de las paginas de bichos
#define TRV_PET_ARROW_Y 185
#define TRV_PET_ARROW_W 56
#define TRV_TIER_W 84      // tarjetas de duracion
#define TRV_TIER_H 62
#define TRV_TIER_GAP 8
#define TRV_TIER_Y 238
#define TRV_GO_W 120       // pastilla Ir
#define TRV_GO_Y 320
#define TRV_HINT_Y 358

inline int trvPetX(int i) { return Y_CX - (TRV_PET_PAGE * TRV_PET_W + (TRV_PET_PAGE - 1) * TRV_PET_GAP) / 2 + i * (TRV_PET_W + TRV_PET_GAP); }
inline int trvTierX(int i) { return Y_CX - (TRV_TIERS * TRV_TIER_W + (TRV_TIERS - 1) * TRV_TIER_GAP) / 2 + i * (TRV_TIER_W + TRV_TIER_GAP); }
inline int trvGoX() { return Y_CX - TRV_GO_W / 2; }

enum : uint8_t { TH_NONE = 0, TH_DEST_L, TH_DEST_R, TH_PET_L, TH_PET_R, TH_PET, TH_TIER, TH_GO };
struct TrvHit {
  uint8_t kind = TH_NONE;
  uint8_t i = 0;  // TH_PET: 0..2 en la pagina; TH_TIER: 0..2
};
inline TrvHit trvHit(int x, int y) {
  TrvHit h;
  if (yInRect(x, y, TRV_DEST_HIT_LX, TRV_DEST_HIT_Y, TRV_DEST_HIT_W, TRV_DEST_HIT_H)) { h.kind = TH_DEST_L; return h; }
  if (yInRect(x, y, TRV_DEST_HIT_RX, TRV_DEST_HIT_Y, TRV_DEST_HIT_W, TRV_DEST_HIT_H)) { h.kind = TH_DEST_R; return h; }
  if (yInRect(x, y, TRV_PET_ARROW_X - TRV_PET_ARROW_W / 2, TRV_PET_Y, TRV_PET_ARROW_W, TRV_PET_H)) { h.kind = TH_PET_L; return h; }
  if (yInRect(x, y, 466 - TRV_PET_ARROW_X - TRV_PET_ARROW_W / 2, TRV_PET_Y, TRV_PET_ARROW_W, TRV_PET_H)) { h.kind = TH_PET_R; return h; }
  for (int i = 0; i < TRV_PET_PAGE; i++)
    if (yInRect(x, y, trvPetX(i), TRV_PET_Y, TRV_PET_W, TRV_PET_H)) { h.kind = TH_PET; h.i = (uint8_t)i; return h; }
  for (int i = 0; i < TRV_TIERS; i++)
    if (yInRect(x, y, trvTierX(i), TRV_TIER_Y, TRV_TIER_W, TRV_TIER_H)) { h.kind = TH_TIER; h.i = (uint8_t)i; return h; }
  if (yPillHit(x, y, trvGoX(), TRV_GO_Y, TRV_GO_W)) h.kind = TH_GO;
  return h;
}

// ---------- geometria del viaje en marcha ----------
#define TRV_CLOCK_Y 76     // la cuenta atras (tamano 4)
#define TRV_WIN_X 83       // la ventana del destino con el bicho
#define TRV_WIN_Y 118
#define TRV_WIN_W 300
#define TRV_WIN_H 108
#define TRV_LOG_Y 240      // el registro: filas de 2 lineas
#define TRV_LOG_X 78
#define TRV_LOG_W 310
#define TRV_LOG_LINE 20
#define TRV_LOG_GAP 6

// ---------- la coleccion ----------
#define SOUV_COLS SOUV_PER_DEST
#define SOUV_CELL_W 52
#define SOUV_CELL_H 48
#define SOUV_GAP 4
#define SOUV_GRID_Y 106
#define SOUV_TITLE_Y 80
#define SOUV_HINT_Y 372
#define SOUV_TIP_X 73      // la tarjeta de un recuerdo
#define SOUV_TIP_Y 112
#define SOUV_TIP_W 320
#define SOUV_TIP_H 212
inline int souvCellX(int id) { return Y_CX - (SOUV_COLS * SOUV_CELL_W + (SOUV_COLS - 1) * SOUV_GAP) / 2 + (id % SOUV_COLS) * (SOUV_CELL_W + SOUV_GAP); }
inline int souvCellY(int id) { return SOUV_GRID_Y + (id / SOUV_COLS) * (SOUV_CELL_H + SOUV_GAP); }
// recuerdo bajo el toque (0..24) o -1
inline int souvAt(int x, int y) {
  for (int id = 0; id < SOUV_COUNT; id++)
    if (yInRect(x, y, souvCellX(id), souvCellY(id), SOUV_CELL_W, SOUV_CELL_H)) return id;
  return -1;
}
// en que casilla del dock de la mochila va cada pestana: pastillas con icono (no caben las etiquetas: en
// espanol "OBJETOS / TIENDA / COLECCION" eran 330 px y a y 44 el disco deja 229). v3.3: la Tienda es su propia
// pantalla (su boton del dock): quedan dos, Objetos y Coleccion (la Coleccion, hasta la pagina del jugador)
#define BAG_TABS 2
#define BAG_TAB_W 64
#define BAG_TAB_H 34
#define BAG_TAB_GAP2 8
#define BAG_TAB_TOP 40
inline int bagTabX(int t) { return Y_CX - (BAG_TABS * BAG_TAB_W + (BAG_TABS - 1) * BAG_TAB_GAP2) / 2 + t * (BAG_TAB_W + BAG_TAB_GAP2); }
// pestana tocada (0..BAG_TABS-1) o -1; la zona se estira a 48 de alto, centrada en la pastilla
inline int bagTabHit(int x, int y) {
  int extra = (Y_HIT_MIN - BAG_TAB_H) / 2;
  for (int t = 0; t < BAG_TABS; t++)
    if (yInRect(x, y, bagTabX(t), BAG_TAB_TOP - extra, BAG_TAB_W, BAG_TAB_H + 2 * extra)) return t;
  return -1;
}
