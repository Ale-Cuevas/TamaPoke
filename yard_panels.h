#pragma once
// El patio (v2.0, fase Y2; v3.3): el panel Jugar del dock, el menu de un bicho (a la derecha del patio), su
// lista de comida y el selector de bicho (los minijuegos y los objetos de la mochila). Aqui solo la geometria
// y las pruebas de toque (logica pura, comprobada en el test del host); el dibujo y las acciones estan en
// TamaPoke.ino.
//
// Jugar: pelota | fases | saco arriba, carrera | cascada | viajes debajo, cada uno con la estadistica que
// entrena (o "Fases") en gris bajo el icono.
// v3.3: el panel Cuidar ya no existe: dormir, comer, el mimo y la ficha estan en el menu de cada bicho.
#include "yard_ui.h"

// PANEL_TRAVEL (Y5b): la pantalla de los viajes, un nivel por encima de Jugar (no usa las tarjetas de aqui)
enum : uint8_t { PANEL_NONE = 0, PANEL_PLAY, PANEL_TRAVEL };

// ---- Jugar ----
enum : uint8_t { PP_BALL = 0, PP_BATTLE, PP_SACK, PP_DASH, PP_ZEN, PP_TRAVEL, PP_COUNT };
#define PP_W 100  // icono a 3x arriba y la etiqueta debajo, dentro de la tarjeta
#define PP_H 100
#define PP_GAP 12
#define PP_ROW1_Y 116
#define PP_ROW2_Y 240

inline void playPanelCard(uint8_t i, int &x, int &y, int &w, int &h) {
  w = PP_W;
  h = PP_H;
  // dos filas de tres (la ultima, el viaje, Y5b)
  x = Y_CX - (3 * PP_W + 2 * PP_GAP) / 2 + (i % 3) * (PP_W + PP_GAP);
  y = i < 3 ? PP_ROW1_Y : PP_ROW2_Y;
}

// tarjeta i del panel kind, o false si no existe
inline bool panelCard(uint8_t kind, uint8_t i, int &x, int &y, int &w, int &h) {
  if (kind == PANEL_PLAY && i < PP_COUNT) { playPanelCard(i, x, y, w, h); return true; }
  return false;
}

inline uint8_t panelCount(uint8_t kind) { return kind == PANEL_PLAY ? PP_COUNT : 0; }

// tarjeta tocada (0..n-1) o -1
inline int panelHit(uint8_t kind, int px, int py) {
  for (uint8_t i = 0; i < panelCount(kind); i++) {
    int x, y, w, h;
    panelCard(kind, i, x, y, w, h);
    if (yInRect(px, py, x, y, w, h)) return i;
  }
  return -1;
}

// ---- el menu de un bicho (v3.3) ----
// Tocar a un bicho del patio abre, encima del patio y a la derecha, una columna de filas: Dormir (o
// Despertar), Comida, Mimar, Estado y, cuando toca, Evolucionar o Despedirse (resaltada). Filas de 48 px
// (lo minimo tocable), PM_W de ancho, la columna centrada en Y_CY; su borde derecho, lo mas a la derecha que
// deja el circulo util a la altura de su esquina mas alejada del centro (menos PM_EDGE). Va entre la cartera
// del HUD (arriba) y las zonas de toque del dock (abajo): con 6 filas el hueco entre ellas se encoge y la
// columna sube (la interfaz usa 5 como mucho: el menu con Evolucionar, o la comida con su fila de volver).
enum : uint8_t { PM_SLEEP = 0, PM_FOOD, PM_CARE, PM_STATUS, PM_EVOLVE, PM_FAREWELL, PM_KINDS };
#define PM_ROW_H Y_HIT_MIN  // 48
#define PM_GAP 6
#define PM_W 150
#define PM_EDGE 4    // px por dentro del circulo util
#define PM_MAX 6
#define PM_TOP 64    // la cartera del HUD (y 40: 14 px de texto, la moneda de r 9 y su sombra) acaba en ~58
#define PM_BOTTOM (Y_DOCK_Y - Y_DOCK_HIT_R - 2)  // 374: encima de los circulos tocables del dock

// el hueco entre filas: PM_GAP, o menos si n filas no caben entre PM_TOP y PM_BOTTOM
inline int petMenuGap(uint8_t n) {
  if (n < 2) return PM_GAP;
  int g = (PM_BOTTOM - PM_TOP - n * PM_ROW_H) / (n - 1);
  return g < 0 ? 0 : g > PM_GAP ? PM_GAP : g;
}
inline int petMenuHeight(uint8_t n) { return n ? n * PM_ROW_H + (n - 1) * petMenuGap(n) : 0; }
// borde de arriba de la columna: centrada en Y_CY, subida si pisaria el dock
inline int petMenuTop(uint8_t n) {
  int h = petMenuHeight(n), t = Y_CY - h / 2;
  if (t + h > PM_BOTTOM) t = PM_BOTTOM - h;
  if (t < PM_TOP) t = PM_TOP;
  return t;
}
// x de la columna: el borde derecho en el circulo util a la altura de la esquina mas lejana (menos PM_EDGE)
inline int petMenuX(uint8_t n) {
  int t = petMenuTop(n), b = t + petMenuHeight(n);
  int d = Y_CY - t > b - Y_CY ? Y_CY - t : b - Y_CY;
  int half = 0;
  while ((half + 1) * (half + 1) + d * d <= Y_SAFE_R * Y_SAFE_R) half++;
  return Y_CX + half - PM_EDGE - PM_W;
}

// la fila i de un menu de n filas; false si no existe
inline bool petMenuRow(uint8_t i, uint8_t n, int &x, int &y, int &w, int &h) {
  if (n == 0 || n > PM_MAX || i >= n) return false;
  w = PM_W;
  h = PM_ROW_H;
  x = petMenuX(n);
  y = petMenuTop(n) + i * (PM_ROW_H + petMenuGap(n));
  return true;
}

// fila tocada (0..n-1) o -1
inline int petMenuHit(uint8_t n, int px, int py) {
  for (uint8_t i = 0; i < n; i++) {
    int x, y, w, h;
    if (petMenuRow(i, n, x, y, w, h) && yInRect(px, py, x, y, w, h)) return i;
  }
  return -1;
}

// ---- la lista de comida del menu (v3.3) ----
// En la misma columna: FOOD_PER_PAGE comidas por pagina (la baya normal, las bayas de colores que se tienen,
// la tarta y la dorada) y debajo una fila para volver al menu; con mas de una pagina esa fila se parte en dos:
// volver (izquierda) y "mas" (derecha, pasa de pagina). Asi nunca hay mas de 5 filas: la columna del menu.
#define FOOD_PER_PAGE 4
#define FL_NAV_GAP 6
enum { FL_BACK = -2, FL_MORE = -3 };

inline uint8_t foodPages(uint8_t foods) { return foods ? (uint8_t)((foods + FOOD_PER_PAGE - 1) / FOOD_PER_PAGE) : 1; }
// cuantas comidas hay en la pagina page
inline uint8_t foodOnPage(uint8_t foods, uint8_t page) {
  int left = (int)foods - (int)page * FOOD_PER_PAGE;
  return (uint8_t)(left <= 0 ? 0 : left > FOOD_PER_PAGE ? FOOD_PER_PAGE : left);
}
// filas de la lista en esa pagina: sus comidas y la de volver
inline uint8_t foodListRows(uint8_t foods, uint8_t page) { return (uint8_t)(foodOnPage(foods, page) + 1); }

// la fila de abajo (n filas): which 0 = volver, 1 = "mas" (solo con more). false si no existe
inline bool foodNavCell(uint8_t n, bool more, uint8_t which, int &x, int &y, int &w, int &h) {
  if (n == 0 || !petMenuRow((uint8_t)(n - 1), n, x, y, w, h)) return false;
  if (!more) return which == 0;
  if (which > 1) return false;
  int half = (w - FL_NAV_GAP) / 2;
  if (which == 1) x += w - half;
  w = half;
  return true;
}

// lo tocado en una lista de n filas: la comida 0..n-2 de la pagina, FL_BACK, FL_MORE o -1
inline int foodListHit(uint8_t n, bool more, int px, int py) {
  int r = petMenuHit(n, px, py);
  if (r < 0) return -1;
  if (r < n - 1) return r;
  int x, y, w, h;
  if (foodNavCell(n, more, 0, x, y, w, h) && yInRect(px, py, x, y, w, h)) return FL_BACK;
  if (more && foodNavCell(n, more, 1, x, y, w, h) && yInRect(px, py, x, y, w, h)) return FL_MORE;
  return -1;
}

// ---- el selector de bicho (v3.3) ----
// Un panel con dos tarjetas, una por bicho de casa (su miniatura, nombre y nivel): elige quien juega un
// minijuego o a quien se le da un objeto de la mochila, cuando los dos pueden.
#define PK_W 150
#define PK_H 180
#define PK_GAP 16
#define PK_Y 118

inline void petPickCard(uint8_t i, int &x, int &y, int &w, int &h) {
  w = PK_W;
  h = PK_H;
  x = Y_CX - (2 * PK_W + PK_GAP) / 2 + (i & 1) * (PK_W + PK_GAP);
  y = PK_Y;
}

// tarjeta tocada (0 o 1) o -1
inline int petPickHit(int px, int py) {
  for (uint8_t i = 0; i < 2; i++) {
    int x, y, w, h;
    petPickCard(i, x, y, w, h);
    if (yInRect(px, py, x, y, w, h)) return i;
  }
  return -1;
}
