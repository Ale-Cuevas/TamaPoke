#pragma once
// El patio (v2.0, fase Y0): paleta y geometria del kit de interfaz nuevo.
//
// Dos superficies (docs/yard-prototype/DESIGN_YARD_UI.md, seccion 2):
//  - el MUNDO (la casa): el mapa de pixel art de borde a borde, con la hora, las
//    monedas, la bateria y el dock de 3 botones flotando encima;
//  - el PANEL (mochila, tienda, caja, ficha, ajustes, fases): un disco casi negro
//    (en AMOLED el negro no gasta), titulo blanco arriba, tarjetas redondeadas y un
//    unico boton redondo de volver abajo en el centro.
//
// Aqui solo hay logica pura (colores, posiciones y pruebas de toque), para que el
// test del host compruebe que todo lo tocable mide al menos 48 px y cae dentro del
// circulo. El dibujo esta en TamaPoke.ino ("kit del patio").
#include <stdint.h>

// ---------- paleta (tomada a ojo del video: una direccion, no valores exactos) ----------
constexpr uint16_t yRgb(uint32_t hex) {
  return (uint16_t)((((hex >> 16) & 0xFF) >> 3) << 11 | (((hex >> 8) & 0xFF) >> 2) << 5 | ((hex & 0xFF) >> 3));
}
#define Y_PANEL_BG   yRgb(0x14161E)  // fondo del disco de los paneles
#define Y_CARD       yRgb(0x252838)  // tarjetas de fila y de aviso
#define Y_CARD_HI    yRgb(0x30344A)  // tarjeta pulsada o elegida
#define Y_TEXT       yRgb(0xFFFFFF)  // titulos y nombres
#define Y_TEXT_DIM   yRgb(0x8C90A6)  // precios, subtitulos, contador de pagina
#define Y_ACCENT_GO  yRgb(0x2EC48A)  // Comprar / Usar / OK, avisos, vida propia
#define Y_ACCENT_BAD yRgb(0xD8344A)  // vida del rival, avisos de peligro
#define Y_BACK       yRgb(0x5E8FE0)  // boton redondo de volver
#define Y_COIN       yRgb(0xF2C230)  // la moneda
#define Y_COIN_DARK  yRgb(0xB8860B)  // su canto
#define Y_HILITE     yRgb(0xFF6FA8)  // valores positivos en los registros ("+2 felicidad")
#define Y_SHADOW     yRgb(0x10121A)  // sombra del texto sobre el mapa
// los tres botones del dock (v3.3: Jugar, Mochila, Tienda; el de Cuidar se fue al menu de cada bicho)
#define Y_DOCK_PLAY  yRgb(0xF0A830)  // ambar
#define Y_DOCK_BAG   yRgb(0x5E8FE0)  // azul
#define Y_DOCK_SHOP  yRgb(0xD8609A)  // rosa (la moneda dorada encima se ve bien)

// ---------- geometria comun (lienzo de 466x466, centro 233,233) ----------
#define Y_CX 233
#define Y_CY 233
#define Y_PANEL_R 231  // radio del disco del panel
#define Y_SAFE_R 221   // radio util: nada tocable ni texto mas alla (233 - 12)
#define Y_HIT_MIN 48   // lado minimo de una zona de toque, aunque lo dibujado sea menor

#define Y_TITLE_Y 40   // borde superior del titulo (tamano 3)
#define Y_SUB_Y 74     // subtitulo atenuado (tamano 2)

// boton de volver: circulo de r 20 abajo en el centro; se toca en r 28 (56 px)
#define Y_BACK_X Y_CX
#define Y_BACK_Y 418
#define Y_BACK_R 20
#define Y_BACK_HIT_R 28

// dock de la casa: 3 circulos de r 22 en y 404, cada 72 px; se tocan en r 28.
// Sin etiquetas: a tamano 1 no se leen y a tamano 2 "Mochila" (84 px) pisa al vecino;
// los nombres (S_DOCK_*) son el titulo del panel que abre cada uno.
#define Y_DOCK_N 3
#define Y_DOCK_Y 404
#define Y_DOCK_R 22
#define Y_DOCK_HIT_R 28
#define Y_DOCK_STEP 72  // v2.0: 56 -> 72, 16 px muertos entre zonas de toque (se tocaba el vecino)
inline int yDockX(int i) { return Y_CX + (i - 1) * Y_DOCK_STEP; }

// flechas < > a los lados, a media altura; zonas de 60x120
#define Y_CHEV_LX 34
#define Y_CHEV_RX (466 - 34)
#define Y_CHEV_HALF 10    // medio tamano del dibujo
#define Y_CHEV_HIT_W 60
#define Y_CHEV_HIT_H 120

// pastilla (Comprar / Usar): 28 de alto dibujada, 48 tocable; ancho minimo 64
#define Y_PILL_H 28
#define Y_PILL_MIN_W 64
#define Y_PILL_PAD 12  // margen del texto a cada lado

// tarjeta: esquinas de radio 12
#define Y_CARD_RAD 12

// globo de dialogo: maximo uno en pantalla, 3 s; con varias necesidades, 1 s cada una
#define Y_BUBBLE_MS 3000
#define Y_BUBBLE_CYCLE_MS 1000
#define Y_TOAST_MS 900

// barra de vida: 10 px de alto, con "cur/max" encima
#define Y_HP_H 10

// ---------- pruebas de toque ----------
inline bool yInCircle(int x, int y, int cx, int cy, int r) {
  int dx = x - cx, dy = y - cy;
  return dx * dx + dy * dy <= r * r;
}
inline bool yInRect(int x, int y, int rx, int ry, int rw, int rh) {
  return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}
// el punto (x,y) mas un margen r cae dentro del radio util
inline bool yInsideSafe(int x, int y, int r) {
  int dx = x - Y_CX, dy = y - Y_CY;
  int lim = Y_SAFE_R - r;
  return lim >= 0 && dx * dx + dy * dy <= lim * lim;
}

inline bool yBackHit(int x, int y) { return yInCircle(x, y, Y_BACK_X, Y_BACK_Y, Y_BACK_HIT_R); }

// boton del dock tocado (0..2) o -1
inline int yDockHit(int x, int y) {
  for (int i = 0; i < Y_DOCK_N; i++)
    if (yInCircle(x, y, yDockX(i), Y_DOCK_Y, Y_DOCK_HIT_R)) return i;
  return -1;
}

// -1 = flecha izquierda, +1 = derecha, 0 = ninguna
inline int yChevronHit(int x, int y) {
  int top = Y_CY - Y_CHEV_HIT_H / 2;
  if (yInRect(x, y, Y_CHEV_LX - Y_CHEV_HIT_W / 2, top, Y_CHEV_HIT_W, Y_CHEV_HIT_H)) return -1;
  if (yInRect(x, y, Y_CHEV_RX - Y_CHEV_HIT_W / 2, top, Y_CHEV_HIT_W, Y_CHEV_HIT_H)) return 1;
  return 0;
}

// ancho real de una pastilla para un texto de tw px
inline int yPillW(int tw) {
  int w = tw + 2 * Y_PILL_PAD;
  return w < Y_PILL_MIN_W ? Y_PILL_MIN_W : w;
}
// pastilla dibujada en (px,py) con ancho pw: se toca con 48 px de alto, centrada en ella
inline bool yPillHit(int x, int y, int px, int py, int pw) {
  int extra = (Y_HIT_MIN - Y_PILL_H) / 2;
  return yInRect(x, y, px, py - extra, pw, Y_PILL_H + 2 * extra);
}

// boton del dock: icono 16x16 (a escala 2), color y si esta apagado (gris)
struct YDockItem {
  const char *const *icon;
  uint16_t col;
  bool off;
};
