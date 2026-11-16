#pragma once
// El patio (v2.0, fase Y2): los paneles Cuidar y Jugar del dock, que sustituyen a los menus
// emergentes de COMER / JUGAR / CUIDADOS. Aqui solo la geometria y las pruebas de toque (logica
// pura, comprobada en el test del host); el dibujo y las acciones estan en TamaPoke.ino.
//
// Cuidar: fila de comida (baya roja | azul | verde | tarta) y debajo los cuidados (luz | bano).
// Jugar: pelota | fases | saco arriba, carrera | cascada debajo, cada uno con la estadistica que
// entrena (o "Fases") en gris bajo el icono.
#include "yard_ui.h"

enum : uint8_t { PANEL_NONE = 0, PANEL_CARE, PANEL_PLAY };

// ---- Cuidar ----
enum : uint8_t { CP_RED = 0, CP_BLUE, CP_GREEN, CP_CAKE, CP_LIGHT, CP_BATH, CP_COUNT };
#define CP_FOOD_W 80   // tarjetas de comida, cuadradas, icono a 3x (48 px)
#define CP_FOOD_GAP 10
#define CP_FOOD_Y 128
#define CP_CARE_W 120  // las dos de cuidados, mas anchas
#define CP_CARE_H 80
#define CP_CARE_GAP 14
#define CP_CARE_Y 252

inline void carePanelCard(uint8_t i, int &x, int &y, int &w, int &h) {
  if (i < CP_LIGHT) {
    w = h = CP_FOOD_W;
    x = Y_CX - (4 * CP_FOOD_W + 3 * CP_FOOD_GAP) / 2 + i * (CP_FOOD_W + CP_FOOD_GAP);
    y = CP_FOOD_Y;
  } else {
    w = CP_CARE_W;
    h = CP_CARE_H;
    x = Y_CX - (2 * CP_CARE_W + CP_CARE_GAP) / 2 + (i - CP_LIGHT) * (CP_CARE_W + CP_CARE_GAP);
    y = CP_CARE_Y;
  }
}

// ---- Jugar ----
enum : uint8_t { PP_BALL = 0, PP_BATTLE, PP_SACK, PP_DASH, PP_ZEN, PP_COUNT };
#define PP_W 100  // icono a 3x arriba y la etiqueta debajo, dentro de la tarjeta
#define PP_H 100
#define PP_GAP 12
#define PP_ROW1_Y 116
#define PP_ROW2_Y 240

inline void playPanelCard(uint8_t i, int &x, int &y, int &w, int &h) {
  w = PP_W;
  h = PP_H;
  if (i < 3) {
    x = Y_CX - (3 * PP_W + 2 * PP_GAP) / 2 + i * (PP_W + PP_GAP);
    y = PP_ROW1_Y;
  } else {
    x = Y_CX - (2 * PP_W + PP_GAP) / 2 + (i - 3) * (PP_W + PP_GAP);
    y = PP_ROW2_Y;
  }
}

// tarjeta i del panel kind, o false si no existe
inline bool panelCard(uint8_t kind, uint8_t i, int &x, int &y, int &w, int &h) {
  if (kind == PANEL_CARE && i < CP_COUNT) { carePanelCard(i, x, y, w, h); return true; }
  if (kind == PANEL_PLAY && i < PP_COUNT) { playPanelCard(i, x, y, w, h); return true; }
  return false;
}

inline uint8_t panelCount(uint8_t kind) { return kind == PANEL_CARE ? CP_COUNT : kind == PANEL_PLAY ? PP_COUNT : 0; }

// tarjeta tocada (0..n-1) o -1
inline int panelHit(uint8_t kind, int px, int py) {
  for (uint8_t i = 0; i < panelCount(kind); i++) {
    int x, y, w, h;
    panelCard(kind, i, x, y, w, h);
    if (yInRect(px, py, x, y, w, h)) return i;
  }
  return -1;
}
