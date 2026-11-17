// Huevos con el color del tipo (v2.4, eggcolor.h): un tipo pinta todo el huevo; dos tipos, el primero
// arriba a la izquierda y el segundo abajo a la derecha, fundidos en diagonal ( / ) por el centro.
#include "framework.h"
#include "../eggcolor.h"

static uint16_t cell(int16_t dex, char ch, int col, int row) {
  uint8_t t1, t2;
  eggTypesOf(dex, t1, t2);
  uint16_t c = 0;
  CHECK(eggCellColor(ch, t1, t2, col, row, c));
  return c;
}

static int lum(uint16_t c) { return (c >> 11) * 2 + ((c >> 5) & 63) + (c & 31) * 2; }

TEST(eggcolor, un_tipo_pinta_todo_el_huevo) {
  const int16_t TANGELA = 114;  // solo planta
  CHECK_EQ(DEX_TBL[TANGELA].type1, (uint8_t)TY_PLANTA);
  CHECK_EQ(DEX_TBL[TANGELA].type2, (uint8_t)TY_NONE);
  for (int r = 6; r <= 28; r++)
    for (int c = 8; c <= 24; c++) {
      CHECK_EQ(cell(TANGELA, 'y', c, r), TYPE_EGG_BASE[TY_PLANTA]);
      CHECK_EQ(cell(TANGELA, 'Y', c, r), TYPE_EGG_SHADE[TY_PLANTA]);
    }
}

TEST(eggcolor, dos_tipos_en_diagonal) {
  const int16_t ZAPDOS = 145;  // electrico / volador
  CHECK_EQ(DEX_TBL[ZAPDOS].type1, (uint8_t)TY_ELECTRICO);
  CHECK_EQ(DEX_TBL[ZAPDOS].type2, (uint8_t)TY_VOLADOR);
  CHECK_EQ(cell(ZAPDOS, 'y', 10, 9), TYPE_EGG_BASE[TY_ELECTRICO]);   // arriba a la izquierda
  CHECK_EQ(cell(ZAPDOS, 'y', 21, 24), TYPE_EGG_BASE[TY_VOLADOR]);    // abajo a la derecha
  uint16_t mid = cell(ZAPDOS, 'y', 16, EGG_DIAG_MID - 16);           // en la diagonal: mezcla
  CHECK(mid != TYPE_EGG_BASE[TY_ELECTRICO] && mid != TYPE_EGG_BASE[TY_VOLADOR]);
  // la frontera es una ( / ): toda la linea col + fila = cte tiene el mismo color
  for (int k = EGG_DIAG_MID - EGG_BLEND_HALF; k <= EGG_DIAG_MID + EGG_BLEND_HALF; k++)
    for (int c = 10; c <= 22; c++) CHECK_EQ(cell(ZAPDOS, 'y', c, k - c), cell(ZAPDOS, 'y', 10, k - 10));
  // y la sombra sigue el mismo reparto
  CHECK_EQ(cell(ZAPDOS, 'Y', 10, 9), TYPE_EGG_SHADE[TY_ELECTRICO]);
  CHECK_EQ(cell(ZAPDOS, 'Y', 23, 22), TYPE_EGG_SHADE[TY_VOLADOR]);
}

TEST(eggcolor, las_motas_son_mas_claras_que_lo_de_debajo) {
  for (int16_t dex : { 114, 145, 4, 7, 92 })
    for (int c = 9; c <= 23; c += 2) {
      int r = 6 + (c * 7) % 22;
      CHECK(lum(cell(dex, 'g', c, r)) > lum(cell(dex, 'y', c, r)));
    }
}

TEST(eggcolor, el_contorno_no_cambia) {
  uint16_t c;
  CHECK(!eggCellColor('k', TY_PLANTA, TY_NONE, 16, 16, c));
  CHECK(!eggCellColor('w', TY_FUEGO, TY_AGUA, 16, 16, c));
}

TEST(eggcolor, sin_especie_es_el_huevo_clasico) {
  uint8_t t1, t2;
  CHECK(!eggTypesOf(0, t1, t2));
  CHECK(!eggTypesOf(-1, t1, t2));
  CHECK(!eggTypesOf(DEX_COUNT + 1, t1, t2));
  CHECK(eggTypesOf(1, t1, t2));
  // normal: el crema de SPR_EGG (spriteColor 'y' / 'Y' en species.h)
  CHECK_EQ(TYPE_EGG_BASE[TY_NORMAL], (uint16_t)0xFED2);
  CHECK_EQ(TYPE_EGG_SHADE[TY_NORMAL], (uint16_t)0xE5CC);
}

TEST(eggcolor, cada_tipo_tiene_su_color) {
  for (int a = 0; a < TY_COUNT; a++) {
    CHECK(lum(TYPE_EGG_SHADE[a]) < lum(TYPE_EGG_BASE[a]));
    for (int b = a + 1; b < TY_COUNT; b++) CHECK(TYPE_EGG_BASE[a] != TYPE_EGG_BASE[b]);
  }
}
