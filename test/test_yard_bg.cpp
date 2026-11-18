// Fondos del patio (yardbg.h, v2.0 fase Y1b): el lector de TPY1, la expansion al
// lienzo, la mascara de paso y el tinte del dia. El fichero se fabrica aqui igual
// que lo escribe tools/pack_yard.py (test/test_tools.py prueba el otro lado).
#include "framework.h"
#include "../yardbg.h"
#include "../yard.h"
#include <vector>

// fichero TPY1 sintetico: pc colores (0 = 256); el pixel (x,y) usa el indice (x + 3*y) % pc
static std::vector<uint8_t> tpy1(int pc, const std::vector<int> &blocked = {}) {
  int n = pc ? pc : 256;
  std::vector<uint8_t> d = { 'T', 'P', 'Y', '1', 1, YBG_SRC, YBG_SRC, (uint8_t)pc };
  for (int i = 0; i < n; i++) {
    uint16_t c = (uint16_t)(i * 257 + 1);
    d.push_back(c & 0xFF);
    d.push_back(c >> 8);
  }
  uint8_t walk[YBG_WALK_BYTES] = {};
  for (int k : blocked) walk[k / 8] |= 1 << (k % 8);
  d.insert(d.end(), walk, walk + YBG_WALK_BYTES);
  for (int y = 0; y < YBG_SRC; y++)
    for (int x = 0; x < YBG_SRC; x++) d.push_back((uint8_t)((x + 3 * y) % n));
  return d;
}

static YardBgView view;  // 512 B de paleta: mejor fuera de la pila

TEST(yard_bg, lee_un_fichero_bueno) {
  auto d = tpy1(63);
  CHECK_EQ((int)d.size(), 8 + 2 * 63 + 29 + 240 * 240);
  CHECK_EQ(yardBgParse(d.data(), d.size(), &view), YBG_OK);
  CHECK_EQ(view.palCount, (uint16_t)63);
  CHECK_EQ(view.pal[0], (uint16_t)1);
  CHECK_EQ(view.pal[62], (uint16_t)(62 * 257 + 1));  // little-endian
  CHECK_EQ(view.pal[63], (uint16_t)0);
  CHECK(view.px == d.data() + 8 + 2 * 63 + 29);
}

TEST(yard_bg, paleta_de_256_se_guarda_como_0) {
  auto d = tpy1(0);
  CHECK_EQ(yardBgParse(d.data(), d.size(), &view), YBG_OK);
  CHECK_EQ(view.palCount, (uint16_t)256);
  CHECK_EQ(view.pal[255], (uint16_t)(255 * 257 + 1));
}

TEST(yard_bg, rechaza_ficheros_malos) {
  auto d = tpy1(63);
  CHECK_EQ(yardBgParse(d.data(), 5, &view), YBG_SHORT);
  CHECK_EQ(yardBgParse(nullptr, 0, &view), YBG_SHORT);
  CHECK_EQ(yardBgParse(d.data(), d.size() - 1, &view), YBG_SHORT);  // cortado (SD a medias)
  auto largo = d;
  largo.push_back(0);
  CHECK_EQ(yardBgParse(largo.data(), largo.size(), &view), YBG_LENGTH);
  auto magia = d;
  magia[3] = '2';
  CHECK_EQ(yardBgParse(magia.data(), magia.size(), &view), YBG_MAGIC);
  auto ver = d;
  ver[4] = 2;
  CHECK_EQ(yardBgParse(ver.data(), ver.size(), &view), YBG_VERSION);
  auto lado = d;
  lado[5] = 233;
  CHECK_EQ(yardBgParse(lado.data(), lado.size(), &view), YBG_SIZE);
  auto idx = d;
  idx.back() = 63;  // fuera de una paleta de 63
  CHECK_EQ(yardBgParse(idx.data(), idx.size(), &view), YBG_INDEX);
}

// v3.5: el mapa a escala 2,4 (antes 2): el pixel X del mapa entero sale del 20 + X * 5 / 12 de la fuente
// (se ve el centro de 200x200). Fija la promesa de diseno de la escala (la misma que los bichos del patio)
TEST(yard_bg, expande_a_480_a_escala_2_4) {
  CHECK_EQ(YBG_ZOOM10, 24);
  // promesa de diseno (v3.8): los bichos a escala entera 2 quedan a 2,6, algo mas grandes que el mapa (2,4)
  CHECK_EQ(2 * YARD_PET_SCALE10, 26);
  auto d = tpy1(63);
  CHECK_EQ(yardBgParse(d.data(), d.size(), &view), YBG_OK);
  std::vector<uint16_t> full(YBG_FULL * YBG_FULL + 1, 0xBEEF);
  yardBgExpandFull(view.px, view.pal, full.data());
  CHECK_EQ(full.back(), (uint16_t)0xBEEF);  // no escribe fuera
  int malos = 0;
  for (int y = 0; y < YBG_FULL; y++)
    for (int x = 0; x < YBG_FULL; x++)
      if (full[y * YBG_FULL + x] != view.pal[view.px[(20 + y * 5 / 12) * YBG_SRC + 20 + x * 5 / 12]]) malos++;
  CHECK_EQ(malos, 0);
}

// el lienzo es la ventana (7, 7) del mapa entero: el pixel (x, y) del lienzo sale del 20 + (x+7)*5/12 de la fuente
TEST(yard_bg, el_lienzo_es_la_ventana_con_recorte_de_7) {
  auto d = tpy1(63);
  CHECK_EQ(yardBgParse(d.data(), d.size(), &view), YBG_OK);
  std::vector<uint16_t> full(YBG_FULL * YBG_FULL);
  yardBgExpandFull(view.px, view.pal, full.data());
  CHECK_EQ(YBG_CANVAS + 2 * YBG_CROP, YBG_FULL);
  int malos = 0;
  for (int y = 0; y < YBG_CANVAS; y++) {
    const uint16_t *row = yardBgCanvasRow(full.data(), y);
    for (int x = 0; x < YBG_CANVAS; x++) {
      int sx = 20 + (x + YBG_CROP) * 5 / 12, sy = 20 + (y + YBG_CROP) * 5 / 12;
      if (row[x] != view.pal[view.px[sy * YBG_SRC + sx]]) malos++;
    }
  }
  CHECK_EQ(malos, 0);
  // el pixel de la fuente (120,120) cubre el centro del lienzo (233,233)
  CHECK_EQ(yardBgCanvasRow(full.data(), 233)[233], view.pal[view.px[120 * YBG_SRC + 120]]);
}

TEST(yard_bg, la_mascara_de_paso) {
  auto d = tpy1(63, { 0, 7, 8, 7 * 15 + 7, 224 });
  CHECK_EQ(yardBgParse(d.data(), d.size(), &view), YBG_OK);
  CHECK(yardCellBlocked(view.walk, 0, 0));
  CHECK(yardCellBlocked(view.walk, 7, 0));
  CHECK(yardCellBlocked(view.walk, 8, 0));   // bit 0 del segundo byte
  CHECK(!yardCellBlocked(view.walk, 9, 0));
  CHECK(yardCellBlocked(view.walk, 7, 7));   // el centro
  CHECK(yardCellBlocked(view.walk, 14, 14)); // la ultima (bit 224, byte 28)
  CHECK(!yardCellBlocked(view.walk, 13, 14));
  CHECK(yardCellBlocked(view.walk, -1, 3));  // fuera de la rejilla: no se pisa
  CHECK(yardCellBlocked(view.walk, 3, 15));
  // del lienzo a la celda: el centro (233) cae en la 7, y el centro de la 7 vuelve a ser 233
  CHECK_EQ(yardCellOf(233), 7);
  CHECK_EQ(yardCellCenter(7), 233);
  // v3.5: celdas de 38,4 px; la 0 y la 14 quedan fuera del lienzo
  CHECK_EQ(yardCellOf(0), 1);
  CHECK_EQ(yardCellOf(21), 1);
  CHECK_EQ(yardCellOf(22), 2);
  CHECK_EQ(yardCellEdge(2), 21);
  CHECK_EQ(yardCellOf(465), 13);
  CHECK(yardCellCenter(0) < 0);
  CHECK(yardCellCenter(14) > 465);
  for (int c = 1; c < 14; c++) CHECK_EQ(yardCellOf(yardCellCenter(c)), c);
  CHECK(yardPointBlocked(view.walk, 233, 233));
  CHECK(!yardPointBlocked(view.walk, yardCellCenter(9), yardCellCenter(1)));
}

TEST(yard_bg, el_tinte) {
  for (uint32_t c : { 0x0000u, 0xFFFFu, 0x5DCDu, 0xF800u })
    CHECK_EQ(yardTint565((uint16_t)c, YTINT_DAY), (uint16_t)c);
  // de noche la hierba sale mas oscura y mas azul (en proporcion)
  uint16_t grass = 0x8E2B, n = yardTint565(grass, YTINT_NIGHT);
  auto R = [](uint16_t c) { return ((c >> 11) & 31) << 3; };
  auto G = [](uint16_t c) { return ((c >> 5) & 63) << 2; };
  auto B = [](uint16_t c) { return (c & 31) << 3; };
  CHECK(R(n) + G(n) + B(n) < R(grass) + G(grass) + B(grass));
  CHECK(B(n) * (R(grass) + G(grass) + B(grass)) > B(grass) * (R(n) + G(n) + B(n)));
  // el blanco de noche no se queda en blanco
  CHECK(yardTint565(0xFFFF, YTINT_NIGHT) != 0xFFFF);
  uint16_t src[3] = { 0xFFFF, 0x8E2B, 0x0000 }, dst[3];
  yardTintPalette(src, dst, 3, YTINT_DUSK);
  for (int i = 0; i < 3; i++) CHECK_EQ(dst[i], yardTint565(src[i], YTINT_DUSK));
}

TEST(yard_bg, el_tinte_por_horas) {
  CHECK_EQ(yardTintAt(3, false), YTINT_NIGHT);
  CHECK_EQ(yardTintAt(6, false), YTINT_DAWN);
  CHECK_EQ(yardTintAt(7, false), YTINT_DAWN);
  CHECK_EQ(yardTintAt(8, false), YTINT_DAY);
  CHECK_EQ(yardTintAt(17, false), YTINT_DAY);
  CHECK_EQ(yardTintAt(18, false), YTINT_DUSK);
  CHECK_EQ(yardTintAt(19, false), YTINT_DUSK);
  CHECK_EQ(yardTintAt(20, false), YTINT_NIGHT);
  CHECK_EQ(yardTintAt(13, true), YTINT_NIGHT);  // dormido: siempre de noche
}
