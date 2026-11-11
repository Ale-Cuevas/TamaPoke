// Volumen del altavoz (volume.h).
#include "framework.h"
#include "../volume.h"

TEST(volume, cicla_los_cuatro_niveles_con_el_apagado) {
  CHECK_EQ((int)volNext(0), 1);
  CHECK_EQ((int)volNext(1), 2);
  CHECK_EQ((int)volNext(2), 3);
  CHECK_EQ((int)volNext(3), 0);
  uint8_t l = VOL_DEFAULT;  // cuatro toques vuelven al de partida
  for (int i = 0; i < VOL_LEVELS; i++) l = volNext(l);
  CHECK_EQ((int)l, VOL_DEFAULT);
}

TEST(volume, el_nivel_por_defecto_suena_como_antes) {
  CHECK_EQ(VOL_LEVELS, 4);
  CHECK_EQ(VOL_DEFAULT, 2);
  CHECK_EQ((int)volDacReg(VOL_DEFAULT), 0xAB);  // -10 dB: el valor fijo hasta la v1.49
}

TEST(volume, registros_del_dac_suben_con_el_nivel) {
  CHECK_EQ((int)volDacReg(0), 0x00);
  CHECK_EQ((int)volDacReg(1), 0x9B);
  CHECK_EQ((int)volDacReg(2), 0xAB);
  CHECK_EQ((int)volDacReg(3), 0xBB);
  for (uint8_t l = 2; l < VOL_LEVELS; l++) CHECK(volDacReg(l) > volDacReg(l - 1));
  CHECK(volDacReg(VOL_LEVELS - 1) <= 0xBF);  // nunca por encima de 0 dB
}

TEST(volume, el_ajuste_viejo_de_sonido_se_traduce) {
  CHECK_EQ((int)volFromLegacy(false), 0);
  CHECK_EQ((int)volFromLegacy(true), VOL_DEFAULT);
  CHECK_EQ((int)volFromStored(false, 0, true), VOL_DEFAULT);  // partida de la v1.49 con sonido
  CHECK_EQ((int)volFromStored(false, 0, false), 0);           // ... y silenciada
  CHECK_EQ((int)volFromStored(true, 3, false), 3);            // "vol" manda sobre "snd"
  CHECK_EQ((int)volFromStored(true, 0, true), 0);
}

TEST(volume, valores_guardados_invalidos_vuelven_al_de_por_defecto) {
  CHECK_EQ((int)volClamp(4), VOL_DEFAULT);
  CHECK_EQ((int)volClamp(255), VOL_DEFAULT);
  CHECK_EQ((int)volFromStored(true, 9, false), VOL_DEFAULT);
  CHECK_EQ((int)volDacReg(200), 0xAB);
  CHECK_EQ((int)volNext(77), (VOL_DEFAULT + 1) % VOL_LEVELS);
}
