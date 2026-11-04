// Marco redondo de la AMOLED-2.16 (roundmask.h + ui_frame.h): la UI de 466x466
// pensada para la pantalla redonda se ve en el panel cuadrado de 480x480 con
// las esquinas en negro y el borde del circulo suavizado.
#include "framework.h"
#include "../roundmask.h"
#include "../ui_frame.h"
#include <cmath>
#include <vector>

static const uint16_t BLANCO = 0xFFFF;

// lienzo de 466x466 relleno de un color y con el marco aplicado
static std::vector<uint16_t> enmarcado(uint16_t color) {
  static RoundMask mask;
  if (!mask.ready()) mask.build(UI_SIZE, UI_FRAME_CENTER, UI_FRAME_CENTER, UI_FRAME_RADIUS);
  std::vector<uint16_t> fb((size_t)UI_SIZE * UI_SIZE, color);
  mask.apply(fb.data());
  return fb;
}
static uint16_t px(const std::vector<uint16_t> &fb, int x, int y) { return fb[(size_t)y * UI_SIZE + x]; }
static float distCentro(int x, int y) {
  float dx = x + 0.5f - UI_FRAME_CENTER, dy = y + 0.5f - UI_FRAME_CENTER;
  return std::sqrt(dx * dx + dy * dy);
}

TEST(roundmask, esquinas_negras_y_centro_intacto) {
  auto fb = enmarcado(BLANCO);
  CHECK_EQ(px(fb, 0, 0), 0);
  CHECK_EQ(px(fb, UI_SIZE - 1, 0), 0);
  CHECK_EQ(px(fb, 0, UI_SIZE - 1), 0);
  CHECK_EQ(px(fb, UI_SIZE - 1, UI_SIZE - 1), 0);
  CHECK_EQ(px(fb, 233, 233), BLANCO);
  // los extremos del circulo llegan al borde del lienzo: [1, 466]
  CHECK_EQ(px(fb, 1, 233), BLANCO);
  CHECK_EQ(px(fb, UI_SIZE - 1, 233), BLANCO);
  CHECK_EQ(px(fb, 233, 1), BLANCO);
  CHECK_EQ(px(fb, 233, UI_SIZE - 1), BLANCO);
  CHECK_EQ(px(fb, 0, 233), 0);
  CHECK_EQ(px(fb, 233, 0), 0);
}

// el marco es simetrico respecto a su centro (233.5): el pixel x se refleja en 466 - x
TEST(roundmask, simetrico_en_los_dos_ejes_y_la_diagonal) {
  auto fb = enmarcado(BLANCO);
  int malos = 0;
  for (int y = 1; y < UI_SIZE; y++)
    for (int x = 1; x < UI_SIZE; x++) {
      uint16_t v = px(fb, x, y);
      if (v != px(fb, UI_SIZE - x, y) || v != px(fb, x, UI_SIZE - y) || v != px(fb, y, x)) malos++;
    }
  CHECK_EQ(malos, 0);
}

TEST(roundmask, dentro_intacto_fuera_negro_y_solo_el_borde_a_medias) {
  auto fb = enmarcado(BLANCO);
  int fueraConLuz = 0, dentroTocado = 0, aMedias = 0;
  for (int y = 0; y < UI_SIZE; y++)
    for (int x = 0; x < UI_SIZE; x++) {
      float d = distCentro(x, y);
      uint16_t v = px(fb, x, y);
      if (d >= UI_FRAME_RADIUS + 0.5f && v != 0) fueraConLuz++;
      if (d <= UI_FRAME_RADIUS - 0.5f && v != BLANCO) dentroTocado++;
      if (v != 0 && v != BLANCO) {
        aMedias++;
        CHECK(std::fabs(d - UI_FRAME_RADIUS) < 0.5f);  // solo en el anillo del borde
      }
    }
  CHECK_EQ(fueraConLuz, 0);
  CHECK_EQ(dentroTocado, 0);
  // un anillo de ~1 px: del orden de la circunferencia (2*pi*232.5 = 1461)
  CHECK_MSG(aMedias > 1000 && aMedias < 2200, std::to_string(aMedias));
}

// los fondos redondos de la UI (fillCircle(233, 233, 231) = pixeles 2..464) no
// los toca el marco: entre ellos y el borde suavizado queda un anillo negro
TEST(roundmask, el_disco_de_fondo_de_la_ui_queda_entero) {
  auto fb = enmarcado(BLANCO);
  int tocados = 0;
  for (int y = 0; y < UI_SIZE; y++)
    for (int x = 0; x < UI_SIZE; x++) {
      int dx = x - 233, dy = y - 233;
      if (dx * dx + dy * dy <= 231 * 231 && px(fb, x, y) != BLANCO) tocados++;
    }
  CHECK_EQ(tocados, 0);
}

TEST(roundmask, el_borde_oscurece_cada_canal_sin_mezclarlos) {
  CHECK_EQ(scale565(0xFFFF, 255), 0xFFFF);
  CHECK_EQ(scale565(0xFFFF, 0), 0);
  CHECK_EQ(scale565(0xF800, 128), (uint16_t)(15 << 11));  // rojo puro sigue siendo rojo
  CHECK_EQ(scale565(0x07E0, 128), (uint16_t)(31 << 5));   // verde puro
  CHECK_EQ(scale565(0x001F, 128), (uint16_t)15);           // azul puro
  auto fb = enmarcado(0xF800);
  for (uint16_t v : fb) CHECK((v & 0x07FF) == 0);  // ningun pixel rojo gana verde o azul
}

TEST(roundmask, cada_fila_cubre_un_tramo_continuo) {
  RoundMask m;
  CHECK(m.build(UI_SIZE, UI_FRAME_CENTER, UI_FRAME_CENTER, UI_FRAME_RADIUS));
  CHECK_EQ(m.rowStart(0), UI_SIZE);  // la fila 0 queda entera fuera
  CHECK_EQ(m.rowEnd(0), UI_SIZE);
  CHECK_EQ(m.rowStart(233), 1);
  CHECK_EQ(m.rowEnd(233), UI_SIZE);
  for (int y = 1; y < UI_SIZE; y++) CHECK(m.rowStart(y) < m.rowEnd(y));
}

TEST(roundmask, el_lienzo_cabe_en_el_panel_y_alineado_a_2px) {
  CHECK_EQ(UI_ORIGIN % 2, 0);
  CHECK(UI_ORIGIN + UI_SIZE <= BOARD_LCD_WIDTH);
  // margen negro por cada lado: 6 a la izquierda/arriba, 8 a la derecha/abajo
  CHECK_EQ(BOARD_LCD_WIDTH - UI_ORIGIN - UI_SIZE, 8);
  CHECK(UI_FRAME_CENTER - UI_FRAME_RADIUS >= 0.0f);
  CHECK(UI_FRAME_CENTER + UI_FRAME_RADIUS <= (float)UI_SIZE);
}

TEST(roundmask, el_tactil_pasa_del_panel_al_lienzo) {
  CHECK_EQ(panelToUi(UI_ORIGIN), 0);
  CHECK_EQ(panelToUi(UI_ORIGIN + 233), 233);
  CHECK_EQ(panelToUi(0), -UI_ORIGIN);                      // borde negro: fuera de todo boton
  CHECK_EQ(panelToUi(BOARD_LCD_WIDTH - 1), 479 - UI_ORIGIN);
}
