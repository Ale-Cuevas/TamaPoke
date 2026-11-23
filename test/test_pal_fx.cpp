// Iconos y particulas de las escenas PALS (v3.22, pal_fx.h): sprites, tipo -> particula, tinte y movimiento.
#include "framework.h"
#include "../pal_fx.h"

TEST(pal_fx, cada_emote_tiene_sprite_o_marca) {
  for (int k = PE_NONE; k < PE_KINDS; k++) {
    uint8_t n = 99;
    const char *const *s = palEmoteSprite((PalEmote)k, n);
    if (k == PE_NONE || k == PE_EXCLAIM) {  // sin sprite: el "!" lo pinta el .ino
      CHECK(s == nullptr);
      CHECK_EQ((int)n, 0);
    } else {
      CHECK(s != nullptr);
      CHECK(n == 16 || n == 32);
      for (int r = 0; r < n; r++) CHECK_EQ((int)strlen(s[r]), (int)n);  // cuadrado
    }
  }
}

TEST(pal_fx, tipo_a_particula_cubre_todos_los_tipos) {
  for (uint8_t t = 0; t < TY_COUNT; t++) {
    PalParticle p = palParticleOf(t);
    CHECK(p < PP_KINDS);
    uint8_t n = 0;
    const char *const *s = palParticleSprite(p, n);
    CHECK(s != nullptr);
    CHECK_EQ((int)n, 8);
    for (int r = 0; r < n; r++) CHECK_EQ((int)strlen(s[r]), 8);
  }
  CHECK_EQ((int)palParticleOf(TY_FUEGO), (int)PP_FLAME);
  CHECK_EQ((int)palParticleOf(TY_AGUA), (int)PP_DROP);
  CHECK_EQ((int)palParticleOf(TY_HIELO), (int)PP_DROP);
  CHECK_EQ((int)palParticleOf(TY_PLANTA), (int)PP_LEAF);
  CHECK_EQ((int)palParticleOf(TY_BICHO), (int)PP_LEAF);
  CHECK_EQ((int)palParticleOf(TY_ELECTRICO), (int)PP_BOLT);
  CHECK_EQ((int)palParticleOf(TY_DRAGON), (int)PP_SPARKLE);
  CHECK_EQ((int)palParticleOf(TY_NONE), (int)PP_SPARKLE);  // tipo invalido: generica
}

TEST(pal_fx, tinte_del_primer_tipo_o_respaldo) {
  uint8_t t1, t2;
  for (int16_t d = 1; d <= DEX_COUNT; d++) {
    CHECK(eggTypesOf(d, t1, t2));
    CHECK_EQ(palTypeTint(d), TYPE_EGG_BASE[t1]);
  }
  CHECK_EQ(palTypeTint(0), (uint16_t)PAL_TINT_FALLBACK);
  CHECK_EQ(palTypeTint(-3), (uint16_t)PAL_TINT_FALLBACK);
  CHECK_EQ(palTypeTint(DEX_COUNT + 1), (uint16_t)PAL_TINT_FALLBACK);
}

TEST(pal_fx, flotar_sube_dentro_de_una_caja_y_acaba_invisible) {
  const int32_t DUR = 1600;
  bool sawVisible = false;
  int prevDy = 0;
  for (int wob = 0; wob < 2; wob++)
    for (int32_t t = 0; t < DUR; t += 10) {
      int16_t dx, dy;
      bool vis;
      palFloat(t, DUR, &dx, &dy, &vis, wob == 1);
      CHECK(dy <= 0 && dy >= -PAL_FLOAT_RISE);
      CHECK(dx >= -PAL_FLOAT_WOBBLE && dx <= PAL_FLOAT_WOBBLE);
      if (wob == 0) CHECK_EQ((int)dx, 0);
      if (wob == 0) { CHECK(dy <= prevDy || t == 0); prevDy = dy; }  // solo sube
      if (vis) sawVisible = true;
    }
  CHECK(sawVisible);
  int16_t dx, dy;
  bool vis = true;
  palFloat(DUR, DUR, &dx, &dy, &vis);
  CHECK(!vis);
  vis = true;
  palFloat(-5, DUR, &dx, &dy, &vis);
  CHECK(!vis);
  vis = true;
  palFloat(100, 0, &dx, &dy, &vis);  // duracion 0: nada
  CHECK(!vis);
  palFloat(10, DUR, &dx, &dy, &vis);  // al empezar es visible
  CHECK(vis);
}

TEST(pal_fx, abanico_de_particulas_dentro_de_una_caja_y_acaba) {
  const int32_t DUR = 700;
  const int N = 6;
  for (int i = 0; i < N; i++) {
    for (int32_t t = 0; t < DUR; t += 10) {
      int16_t dx, dy;
      CHECK(palBurst(i, N, t, DUR, &dx, &dy));
      CHECK(dx >= -PAL_BURST_R && dx <= PAL_BURST_R);
      CHECK(dy >= -(PAL_BURST_R + PAL_BURST_RISE) && dy <= PAL_BURST_R);
    }
    int16_t dx, dy;
    CHECK(!palBurst(i, N, DUR, DUR, &dx, &dy));
    CHECK(!palBurst(i, N, -1, DUR, &dx, &dy));
  }
  // se abren: particulas distintas en la mitad del recorrido, y deterministas
  int16_t ax, ay, bx, by, cx, cy;
  palBurst(0, N, 400, DUR, &ax, &ay);
  palBurst(3, N, 400, DUR, &bx, &by);
  CHECK(ax != bx || ay != by);
  palBurst(0, N, 400, DUR, &cx, &cy);
  CHECK(ax == cx && ay == cy);
  CHECK(!palBurst(0, 0, 100, DUR, &ax, &ay));  // n = 0
}
