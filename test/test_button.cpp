// Antirrebote de la tecla USER (button.h).
#include "framework.h"
#include "../button.h"

TEST(button, sin_pulsar_no_dispara) {
  KeyDebounce k;
  for (uint32_t t = 1000; t < 3000; t += 10) CHECK(!k.update(false, t));
}

TEST(button, mantenerla_pulsada_dispara_una_sola_vez) {
  KeyDebounce k;
  CHECK(k.update(true, 1000));
  for (uint32_t t = 1010; t < 4000; t += 10) CHECK(!k.update(true, t));
}

TEST(button, los_rebotes_al_pulsar_y_al_soltar_no_disparan) {
  KeyDebounce k;
  CHECK(k.update(true, 1000));
  CHECK(!k.update(false, 1003));  // rebote al pulsar
  CHECK(!k.update(true, 1006));
  CHECK(!k.update(false, 1009));
  CHECK(!k.update(true, 1040));
  CHECK(!k.update(false, 1200));  // se suelta
  CHECK(!k.update(true, 1203));   // rebote al soltar
  CHECK(!k.update(false, 1206));
  CHECK(!k.update(true, 1215));
  CHECK(!k.update(false, 1240));
  CHECK(!k.stable);
}

TEST(button, dos_pulsaciones_disparan_dos_veces) {
  KeyDebounce k;
  CHECK(k.update(true, 1000));
  CHECK(!k.update(false, 1150));
  CHECK(k.update(true, 1300));
  CHECK(!k.update(false, 1450));
}

TEST(button, basta_una_lectura_entre_dos_frames) {
  // el loop lee una vez por pase: una pulsacion corta puede verse en una sola lectura
  KeyDebounce k;
  CHECK(!k.update(false, 1000));
  CHECK(k.update(true, 1060));
  CHECK(!k.update(false, 1120));
  CHECK(!k.update(false, 1180));
  CHECK(k.update(true, 1240));
}

TEST(button, una_pulsacion_mas_corta_que_la_ventana_se_cuenta_una_vez) {
  KeyDebounce k;
  CHECK(k.update(true, 1000));
  CHECK(!k.update(false, 1020));  // se suelta antes de 30 ms: aun dentro de la ventana
  CHECK(!k.update(false, 1040));  // ahora si se acepta la suelta
  CHECK(!k.stable);
  CHECK(k.update(true, 1100));
}

TEST(button, sobrevive_a_la_vuelta_de_millis) {
  KeyDebounce k;
  CHECK(k.update(true, 0xFFFFFFF0u));
  CHECK(!k.update(false, 0xFFFFFFF8u));  // 8 ms: rebote
  CHECK(!k.update(false, 20));           // 36 ms tras la vuelta: suelta aceptada
  CHECK(!k.stable);
  CHECK(!k.update(true, 30));            // 10 ms tras la suelta: rebote
  CHECK(k.update(true, 60));
}
