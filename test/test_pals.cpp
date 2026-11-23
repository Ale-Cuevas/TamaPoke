// Los dos bichos juntos (pals.h, v3.22 PALS, FR-PALS.1-7): el director de escenas del patio. Cuando empieza cada
// escena (los huecos de patio a la vista, el saludo con su espera), lo que la impide o la corta, grande y pequeno,
// que el animo va antes que el juego, que el juguete hace falta para sus escenas, que las rinas no pagan nada, que
// al reabrir el patio no queda nada a medias y que los bichos siempre quedan sueltos al acabar. El azar es el del
// patio (YardWorld.rng): todo es determinista.
#include "framework.h"
#include "../pals.h"

namespace {

struct Sim {
  uint8_t walk[YBG_WALK_BYTES];
  YardWorld w;
  PalsState st;
  PalsCtx c;
  uint32_t now = 1000;
  // lo que salio en los pasos
  int started[PS_COUNT] = {};
  int ended[PS_COUNT] = {};
  int aborted[PS_COUNT] = {};
  int play[2] = { 0, 0 }, cheer[2] = { 0, 0 }, seen = 0;
  PalsOut last;

  Sim(uint32_t seed = 7) {
    yardFallbackWalk(walk);
    w = YardWorld{};
    w.n = 2;
    w.rng.s = seed;
    for (int i = 0; i < 2; i++) {
      w.p[i].on = true;
      w.p[i].walks = true;
      PalsSlot &s = c.s[i];
      s.pet = true;
      s.joy = 70;
      s.level = 10;
      s.actHop = 8;
      s.actBump = 6;
      s.actSit = 11;
      s.actJoy = 7;
    }
    place(0, 5, 7);
    place(1, 9, 7);
  }
  void place(int i, int cx, int cy) {
    w.p[i].x = (float)yardCellCenter(cx);
    w.p[i].y = (float)yardCellCenter(cy);
    w.p[i].mode = YM_IDLE;
    w.p[i].pathN = w.p[i].pathI = 0;
  }
  // un paso como el de yardTick: el director y despues el patio
  void step(uint32_t dt = 85) {
    now += dt;
    for (int i = 0; i < 2; i++)
      if (c.s[i].asleep && c.s[i].hold == YH_NONE) c.s[i].hold = YH_SLEEP;
    palsStep(st, w, walk, now, dt, c, last);
    uint8_t hold[YARD_PETS_MAX] = { c.s[0].hold, c.s[1].hold, YH_NONE };
    for (int i = 0; i < 2; i++)
      if (last.holdFree[i]) hold[i] = YH_NONE;
    yardStep(w, walk, now, dt, hold, c.tilt);
    w.bedSnap = false;
    if (last.started) started[last.started]++;
    if (last.ended) ended[last.ended]++;
    if (last.aborted) aborted[last.aborted]++;
    for (int i = 0; i < 2; i++) play[i] += last.play[i];
    if (last.cheer >= 0) cheer[last.cheer]++;
    seen += last.seen;
  }
  void run(uint32_t ms, uint32_t dt = 85) {
    for (uint32_t t = 0; t < ms; t += dt) step(dt);
  }
  // corre hasta que no haya escena (o pase el tiempo)
  bool runScene(uint32_t maxMs = 30000) {
    for (uint32_t t = 0; t < maxMs && st.scene != PS_NONE; t += 85) step();
    return st.scene == PS_NONE;
  }
  // fuerza la escena en el proximo paso: el motivo si no empezo (nullptr si empezo)
  const char *force(uint8_t sc) {
    palsForce(st, sc);
    step();
    if (last.refused == sc) return last.why;
    return last.started == sc ? nullptr : "not started";
  }
  bool released() const {
    for (int i = 0; i < 2; i++)
      if (w.p[i].task == YT_SCENE || w.p[i].sceneDist || w.p[i].scenePct) return false;
    return true;
  }
  int playStarted() const {
    int n = 0;
    for (int k = PS_CHASE; k <= PS_ESCORT; k++) n += started[k];
    return n;
  }
};

}  // namespace

// ---------- cuando ----------

TEST(pals, la_primera_escena_de_juego_espera_su_hueco) {
  Sim s;
  s.step();
  CHECK(s.st.playAt >= PAL_SCENE_GAP_MIN_S * 1000u);
  CHECK(s.st.playAt <= PAL_SCENE_GAP_MAX_S * 1000u);
  // hasta el hueco, ninguna (el saludo si puede)
  while (s.st.watch + 200 < s.st.playAt) s.step(200);
  CHECK_EQ(s.playStarted(), 0);
  CHECK_EQ(s.started[PS_CHEER], 0);
}

TEST(pals, unas_tres_escenas_por_hora_de_patio) {
  Sim s(12345);
  const uint32_t hours = 10;
  s.run(hours * 3600u * 1000u, 200);
  int n = s.playStarted();
  // cada hueco es de MIN a MAX s de patio (mas lo que dura la escena y el respiro)
  int lo = (int)(hours * 3600 / (PAL_SCENE_GAP_MAX_S + 60)), hi = (int)(hours * 3600 / PAL_SCENE_GAP_MIN_S) + 1;
  CHECK(n >= lo);
  CHECK(n <= hi);
  // y la mayoria acaba bien, pagando a los dos
  int done = 0;
  for (int k = PS_CHASE; k <= PS_ESCORT; k++) done += s.ended[k];
  CHECK(done * 10 >= n * 7);
  CHECK_EQ(s.play[0], done);
  CHECK_EQ(s.play[1], done);
  CHECK(s.released() || s.st.scene != PS_NONE);
  // sin juguete ni grande y pequeno, esas dos no salen
  CHECK_EQ(s.started[PS_TOYPASS], 0);
  CHECK_EQ(s.started[PS_ESCORT], 0);
}

TEST(pals, el_saludo_tiene_su_espera) {
  Sim s(99);
  uint32_t lastGreet = 0;
  bool any = false;
  for (uint32_t t = 0; t < 3600u * 1000u; t += 200) {
    s.step(200);
    if (s.last.started == PS_GREET) {
      if (any) CHECK(s.st.watch - lastGreet >= PAL_GREET_COOLDOWN_S * 1000u);
      lastGreet = s.st.watch;
      any = true;
    }
  }
  CHECK(any);  // se cruzan a menudo en una hora
  CHECK(s.started[PS_GREET] <= (int)(3600 / PAL_GREET_COOLDOWN_S) + 1);
  // el saludo no paga ni cuenta para la medalla
  CHECK_EQ(s.ended[PS_GREET] > 0, true);
}

TEST(pals, el_saludo_solo_de_cerca_y_andando) {
  Sim s;
  s.step();
  s.place(0, 5, 7);
  s.place(1, 7, 7);  // 77 px: cerca, pero quietos
  s.w.p[0].until = s.w.p[1].until = s.now + 60000;
  s.step();
  CHECK_EQ((int)s.last.started, (int)PS_NONE);
  CHECK(s.force(PS_GREET) == nullptr);  // forzado si
  CHECK(s.runScene());
  CHECK_EQ(s.ended[PS_GREET], 1);
  CHECK(s.released());
}

// ---------- lo que la impide ----------

TEST(pals, cada_bloqueo_impide_empezar) {
  struct Case {
    const char *why;
    void (*set)(PalsCtx &);
  };
  static const Case CASES[] = {
    { "modal", [](PalsCtx &c) { c.modal = true; } },
    { "tilt", [](PalsCtx &c) { c.tilt = 1; } },
    { "busy", [](PalsCtx &c) { c.s[1].busy = true; } },  // evoluciona, eclosiona, ceremonia o se muere
    { "menu", [](PalsCtx &c) { c.menuSlot = 0; } },
    { "food", [](PalsCtx &c) { c.s[0].food = YF_EAT; } },
  };
  for (const Case &k : CASES) {
    Sim s;
    s.step();
    k.set(s.c);
    const char *why = s.force(PS_NUZZLE);
    CHECK(why != nullptr);
    if (why) CHECK(!strcmp(why, k.why));
    CHECK_EQ((int)s.st.scene, (int)PS_NONE);
    // y en su hora tampoco
    s.st.playAt = s.st.watch;
    s.run(2000);
    CHECK_EQ(s.playStarted(), 0);
    CHECK(s.released());
  }
}

TEST(pals, un_bicho_retenido_o_dormido_no_juega) {
  Sim s;
  s.step();
  s.c.s[1].hold = YH_STAY;  // una emocion, comiendo, la pose triste...
  CHECK(s.force(PS_CHASE) != nullptr);
  s.c.s[1].hold = YH_NONE;
  s.c.s[1].asleep = true;
  CHECK(s.force(PS_HOP) != nullptr);
  s.c.s[1].asleep = false;
  s.c.s[1].hold = YH_NONE;
  s.c.s[1].pet = false;  // hueco vacio
  CHECK(s.force(PS_NUZZLE) != nullptr);
  CHECK(s.force(PS_EGGSIT) != nullptr);  // sin huevo tampoco
}

TEST(pals, cada_bloqueo_corta_la_escena_y_suelta_a_los_dos) {
  struct Case {
    void (*set)(PalsCtx &);
  };
  static const Case CASES[] = {
    { [](PalsCtx &c) { c.modal = true; } },  { [](PalsCtx &c) { c.tilt = -1; } },
    { [](PalsCtx &c) { c.s[0].busy = true; } }, { [](PalsCtx &c) { c.menuSlot = 1; } },  // un toque abre su menu
    { [](PalsCtx &c) { c.s[1].food = YF_QUEUED; } }, { [](PalsCtx &c) { c.s[0].hold = YH_STAY; } },
    { [](PalsCtx &c) { c.s[1].asleep = true; } },
  };
  for (const Case &k : CASES) {
    Sim s;
    s.step();
    CHECK(s.force(PS_HOP) == nullptr);
    s.run(600);
    CHECK_EQ((int)s.st.scene, (int)PS_HOP);
    k.set(s.c);
    s.step();
    CHECK_EQ((int)s.st.scene, (int)PS_NONE);
    CHECK_EQ(s.aborted[PS_HOP], 1);
    CHECK_EQ(s.play[0] + s.play[1] + s.seen, 0);  // cortada: no paga
    CHECK(s.released());
    CHECK_EQ((int)s.st.emo[0].kind, (int)PE_NONE);
  }
}

TEST(pals, un_gesto_del_juego_corta_la_escena) {
  Sim s;
  s.step();
  CHECK(s.force(PS_HOP) == nullptr);
  s.step();
  yardGesture(s.w, 0, 7, 1500, s.now);  // la subida de nivel, p. ej.
  CHECK_EQ((int)s.w.p[0].task, (int)YT_NONE);
  s.step();
  CHECK_EQ((int)s.st.scene, (int)PS_NONE);
  CHECK(s.released());
}

TEST(pals, al_reabrir_el_patio_no_queda_nada) {
  Sim s;
  s.step();
  CHECK(s.force(PS_NUZZLE) == nullptr);
  s.run(500);
  CHECK(s.st.scene != PS_NONE);
  s.w.bedSnap = true;
  s.step();
  CHECK_EQ((int)s.st.scene, (int)PS_NONE);
  CHECK(s.released());
  CHECK_EQ((int)s.st.emo[0].kind, (int)PE_NONE);
  CHECK_EQ((int)s.st.emo[1].kind, (int)PE_NONE);
  CHECK_EQ(s.play[0] + s.seen, 0);
}

TEST(pals, el_reloj_solo_cuenta_patio_a_la_vista) {
  Sim s;
  s.step();
  uint32_t w0 = s.st.watch;
  s.w.bedSnap = true;  // vuelve de otra pantalla tras mucho rato: ese rato no cuenta
  s.now += 3600u * 1000u;
  s.step();
  CHECK_EQ(s.st.watch, w0);
}

// ---------- grande y pequeno ----------

TEST(pals, etapa_de_evolucion) {
  CHECK_EQ((int)palsStageOf(4), 0);    // Charmander
  CHECK_EQ((int)palsStageOf(5), 1);    // Charmeleon
  CHECK_EQ((int)palsStageOf(6), 2);    // Charizard
  CHECK_EQ((int)palsStageOf(25), 0);   // Pikachu (en gen 1 su linea empieza en el)
  CHECK_EQ((int)palsStageOf(26), 1);   // Raichu
  CHECK_EQ((int)palsStageOf(133), 0);  // Eevee
  CHECK_EQ((int)palsStageOf(134), 1);  // Vaporeon (una rama)
  CHECK_EQ((int)palsStageOf(136), 1);  // Flareon (otra rama)
}

TEST(pals, grande_y_pequeno) {
  PalsCtx c;
  c.s[0].pet = c.s[1].pet = true;
  c.s[0].level = c.s[1].level = 10;
  CHECK_EQ(palsBigOf(c), -1);
  c.s[1].stage = 1;  // una etapa mas, aunque sea de menos nivel
  c.s[0].level = 30;
  CHECK_EQ(palsBigOf(c), 1);
  c.s[1].stage = 0;
  c.s[0].level = 10 + PAL_BIG_LEVEL_GAP;
  CHECK_EQ(palsBigOf(c), 0);
  c.s[0].level = 10 + PAL_BIG_LEVEL_GAP - 1;
  CHECK_EQ(palsBigOf(c), -1);
  c.s[1].pet = false;
  c.s[0].stage = 2;
  CHECK_EQ(palsBigOf(c), -1);
}

TEST(pals, el_paseo_solo_con_grande_y_pequeno) {
  Sim s;
  s.step();
  CHECK(s.force(PS_ESCORT) != nullptr);
  s.c.s[0].stage = 1;  // el 0 es el grande
  CHECK(s.force(PS_ESCORT) == nullptr);
  CHECK_EQ((int)s.st.b, 0);  // acompana el grande
  CHECK_EQ((int)s.st.a, 1);
  CHECK(s.runScene());
  CHECK_EQ(s.ended[PS_ESCORT], 1);
  CHECK_EQ(s.play[0], 1);
  CHECK_EQ(s.play[1], 1);
  CHECK(s.released());
}

TEST(pals, el_grande_espera_junto_a_la_comida_del_pequeno) {
  Sim s;
  s.step();
  s.c.s[0].stage = 1;  // grande
  s.c.s[1].food = YF_EAT;
  s.c.s[1].foodX = (int16_t)(s.w.p[1].x + 28);
  s.c.s[1].foodY = (int16_t)s.w.p[1].y;
  s.c.menuSlot = 1;  // la comida se elige en su menu: eso no la para
  s.step();
  CHECK_EQ((int)s.last.started, (int)PS_WAIT);
  CHECK_EQ((int)s.st.a, 0);
  s.run(900);
  s.c.s[1].food = YF_NONE;  // se la comio (yardFoodEaten, sin tocar nada de aqui)
  CHECK(s.runScene());
  CHECK_EQ(s.ended[PS_WAIT], 1);
  CHECK_EQ(s.play[0] + s.play[1] + s.seen, 0);
  CHECK(s.released());
}

// ---------- animar ----------

TEST(pals, animar_va_antes_que_el_juego) {
  Sim s;
  s.step();
  s.c.s[1].joy = PAL_CHEER_BELOW - 1;
  s.c.s[0].joy = PAL_CHEER_FROM;
  s.c.s[1].hold = YH_STAY;  // con la pose triste
  s.c.s[1].sadPose = true;
  s.st.playAt = s.st.watch;
  s.step();
  CHECK_EQ((int)s.last.started, (int)PS_CHEER);
  CHECK_EQ((int)s.st.a, 0);  // va el contento
  CHECK_EQ((int)s.st.b, 1);
  CHECK(!s.st.own[1]);       // al triste con su pose no se le mueve
  CHECK(s.runScene());
  CHECK_EQ(s.ended[PS_CHEER], 1);
  CHECK_EQ(s.cheer[1], 1);
  CHECK_EQ(s.cheer[0], 0);
  CHECK_EQ(s.play[0] + s.play[1], 0);
  CHECK_EQ(s.seen, 1);
  CHECK(s.released());
  CHECK(s.st.playAt > s.st.watch + PAL_SCENE_GAP_MIN_S * 1000u - 30000);  // gasto el hueco de la hora
}

TEST(pals, animar_necesita_uno_triste_y_otro_contento) {
  Sim s;
  s.step();
  CHECK(s.force(PS_CHEER) != nullptr);
  s.c.s[1].joy = PAL_CHEER_BELOW - 1;
  s.c.s[0].joy = PAL_CHEER_FROM - 1;
  CHECK(s.force(PS_CHEER) != nullptr);
  s.c.s[0].joy = PAL_CHEER_FROM;
  CHECK(s.force(PS_CHEER) == nullptr);
}

// ---------- el juguete ----------

TEST(pals, las_escenas_del_juguete_solo_con_juguete) {
  Sim s;
  s.step();
  CHECK(s.force(PS_TOYPASS) != nullptr);
  CHECK(s.force(PS_TUG) != nullptr);
  s.c.toy = true;
  s.c.toyX = (float)yardCellCenter(7);
  s.c.toyY = (float)yardCellCenter(5);
  s.walk[yardCellIdx(7, 5) >> 3] |= (uint8_t)(1 << (yardCellIdx(7, 5) & 7));  // su celda, bloqueada (yardBlockObjects)
  CHECK(s.force(PS_TOYPASS) == nullptr);
  CHECK(s.runScene());
  CHECK_EQ(s.ended[PS_TOYPASS], 1);
  CHECK_EQ(s.play[0], 1);
  CHECK(s.released());
  s.st.restAt = s.st.watch;
  CHECK(s.force(PS_TUG) == nullptr);
  CHECK(s.runScene());
  CHECK_EQ(s.ended[PS_TUG], 1);
  CHECK_EQ(s.play[0], 1);  // la rina no paga
  CHECK(s.released());
}

// ---------- rinas ----------

// promesa de diseno (FR-PALS.7): una rina nunca cambia nada del bicho. El director no ve a Pet (solo PalsCtx, de
// lectura): lo que queda por comprobar es que no pide premios y que no despierta ni mueve al dormido
TEST(pals, las_rinas_no_pagan_ni_despiertan) {
  // la siesta
  Sim s;
  s.step();
  s.c.hour = 15;
  s.c.s[1].asleep = s.c.s[1].nap = true;
  s.run(300);
  float x1 = s.w.p[1].x, y1 = s.w.p[1].y;
  CHECK(s.force(PS_BUMP) == nullptr);
  bool grumpy = false;
  while (s.st.scene != PS_NONE) {
    s.step();
    grumpy |= s.st.emo[1].kind == PE_GRUMPY;
    CHECK_EQ((int)s.w.p[1].mode, (int)YM_SLEEP);  // sigue dormido
  }
  CHECK(grumpy);
  CHECK_EQ(s.ended[PS_BUMP], 1);
  CHECK_EQ(s.w.p[1].x, x1);
  CHECK_EQ(s.w.p[1].y, y1);
  CHECK_EQ(s.play[0] + s.play[1] + s.cheer[0] + s.cheer[1] + s.seen, 0);
  CHECK(s.released());
  // la misma baya
  Sim b;
  b.step();
  b.c.s[0].food = YF_EAT;
  b.c.s[0].foodX = (int16_t)(b.w.p[0].x + 28);
  b.c.s[0].foodY = (int16_t)b.w.p[0].y;
  b.c.menuSlot = 0;
  CHECK(b.force(PS_BERRY) == nullptr);
  CHECK_EQ((int)b.st.a, 1);
  b.run(900);
  b.c.s[0].food = YF_NONE;
  uint32_t gone = b.now, heart = 0;
  while (b.st.scene != PS_NONE) {
    b.step();
    if (!heart && b.st.emo[1].kind == PE_HEART) heart = b.now;
  }
  CHECK(heart > 0);
  CHECK(heart - gone <= PAL_MAKEUP_S * 1000u);  // las paces enseguida
  CHECK_EQ(b.ended[PS_BERRY], 1);
  CHECK_EQ(b.play[0] + b.play[1] + b.seen, 0);
  CHECK(b.released());
}

TEST(pals, despertar_solo_en_la_siesta_de_dia) {
  Sim s;
  s.step();
  s.c.s[1].asleep = true;
  s.c.hour = 15;
  CHECK(s.force(PS_BUMP) != nullptr);  // dormido de noche (nightSlept)
  s.c.s[1].nap = true;
  s.c.hour = EVENING_HOUR;
  CHECK(s.force(PS_BUMP) != nullptr);  // por la tarde, nunca
  s.c.hour = WAKE_HOUR;
  CHECK(s.force(PS_BUMP) == nullptr);
}

TEST(pals, como_mucho_una_rina_por_hora) {
  Sim s(4242);
  s.c.toy = true;
  s.c.toyX = (float)yardCellCenter(7);
  s.c.toyY = (float)yardCellCenter(5);
  s.walk[yardCellIdx(7, 5) >> 3] |= (uint8_t)(1 << (yardCellIdx(7, 5) & 7));
  const uint32_t hours = 6;
  s.run(hours * 3600u * 1000u, 200);
  int n = s.started[PS_TUG] + s.started[PS_BUMP] + s.started[PS_BERRY];
  CHECK(n >= 1);
  CHECK(n <= (int)(hours * 3600 / PAL_SQUABBLE_GAP_S));
}

TEST(pals, la_baya_de_otro_sin_rina_si_no_toca) {
  Sim s;
  s.step();
  s.c.s[0].food = YF_EAT;  // no es grande y pequeno y la rina aun no toca: nada
  s.step();
  CHECK_EQ((int)s.last.started, (int)PS_NONE);
  s.c.s[0].food = YF_NONE;
  s.step();
  s.st.squabbleAt = s.st.watch;
  s.c.s[0].food = YF_QUEUED;
  s.c.s[0].foodX = (int16_t)s.w.p[0].x;
  s.c.s[0].foodY = (int16_t)s.w.p[0].y;
  s.step();
  CHECK_EQ((int)s.last.started, (int)PS_BERRY);
}

// ---------- el huevo, la tarde y dormir juntos ----------

TEST(pals, junto_al_huevo) {
  Sim s;
  s.step();
  s.c.s[1].pet = false;
  s.w.p[1].on = false;
  s.c.s[1].egg = true;
  s.c.s[1].eggX = (float)yardCellCenter(9);
  s.c.s[1].eggY = (float)yardCellCenter(8);
  s.walk[yardCellIdx(9, 8) >> 3] |= (uint8_t)(1 << (yardCellIdx(9, 8) & 7));
  CHECK(s.force(PS_NUZZLE) != nullptr);  // con un huevo, solo esto
  s.st.playAt = s.st.watch;
  s.step();
  CHECK_EQ((int)s.last.started, (int)PS_EGGSIT);
  CHECK(s.runScene());
  CHECK_EQ(s.ended[PS_EGGSIT], 1);
  float d = yardDist(s.w.p[0].x, s.w.p[0].y, s.c.s[1].eggX, s.c.s[1].eggY);
  CHECK(d >= PAL_SIDE_PX - 1);  // al lado, sin taparlo
  CHECK(d <= palsBesideMax(PAL_SIDE_PX) + 8);
  CHECK_EQ(s.play[0] + s.seen, 0);
  CHECK(s.released());
}

TEST(pals, sentarse_junto_al_que_duerme_por_la_tarde) {
  Sim s;
  s.step();
  s.c.s[1].asleep = true;
  s.c.hour = EVENING_HOUR - 1;
  CHECK(s.force(PS_SIT) != nullptr);
  s.c.hour = EVENING_HOUR;
  CHECK(s.force(PS_SIT) == nullptr);
  CHECK(s.runScene());
  CHECK_EQ(s.ended[PS_SIT], 1);
  CHECK(s.released());
}

TEST(pals, dormir_juntos_sin_camas) {
  Sim s;
  s.step();
  s.c.s[0].asleep = true;
  s.run(500);
  s.c.s[1].asleep = true;  // el segundo se duerme con el patio a la vista
  s.step();
  CHECK_EQ((int)s.last.started, (int)PS_SLEEP);
  CHECK_EQ((int)s.st.b, 1);
  CHECK(s.last.holdFree[1]);
  CHECK(s.runScene());
  CHECK_EQ(s.ended[PS_SLEEP], 1);
  CHECK(yardDist(s.w.p[0].x, s.w.p[0].y, s.w.p[1].x, s.w.p[1].y) <= PAL_BESIDE_PX);
  s.step();
  CHECK_EQ((int)s.w.p[1].mode, (int)YM_SLEEP);  // y duerme alli
  CHECK(s.released());
  CHECK(palsZzzTogether(s.w));
}

TEST(pals, con_camas_no_anda_dormido) {
  Sim s;
  s.step();
  s.c.beds = true;
  s.c.s[0].asleep = true;
  s.run(500);
  s.c.s[1].asleep = true;
  s.step();
  CHECK_EQ((int)s.last.started, (int)PS_NONE);
  CHECK(s.force(PS_SLEEP) != nullptr);
}

// ---------- siempre sueltos ----------

TEST(pals, cada_escena_suelta_a_los_dos_al_acabar) {
  static const uint8_t ALL[] = { PS_GREET, PS_CHASE, PS_HOP, PS_NUZZLE, PS_TOYPASS, PS_ESCORT, PS_CHEER, PS_TUG };
  for (uint8_t sc : ALL) {
    Sim s(31 + sc);
    s.step();
    s.c.toy = true;
    s.c.toyX = (float)yardCellCenter(7);
    s.c.toyY = (float)yardCellCenter(5);
    s.walk[yardCellIdx(7, 5) >> 3] |= (uint8_t)(1 << (yardCellIdx(7, 5) & 7));
    s.c.s[0].stage = 1;
    if (sc == PS_CHEER) s.c.s[1].joy = 5;
    CHECK(s.force(sc) == nullptr);
    CHECK(s.runScene());
    CHECK_EQ(s.ended[sc] + s.aborted[sc], 1);
    CHECK(s.released());
    // y vuelven a pasear solos (yardDecide ya no los salta)
    s.st.restAt = s.st.greetAt = s.st.playAt = s.st.squabbleAt = s.st.watch + 3600000u;
    bool moved = false;
    for (int k = 0; k < 120; k++) {
      s.step();
      moved |= s.w.p[0].mode == YM_WALK || s.w.p[1].mode == YM_WALK || s.w.p[0].mode == YM_GESTURE;
    }
    CHECK(moved);
    CHECK(s.w.p[0].task == YT_NONE && s.w.p[1].task == YT_NONE);
  }
}

// ---------- la distancia minima (yard.h) ----------

TEST(pals, solo_en_escena_se_acercan_mas) {
  for (int scene = 0; scene < 2; scene++) {
    uint8_t walk[YBG_WALK_BYTES];
    yardFallbackWalk(walk);
    YardWorld w;
    w.n = 2;
    w.p[0].on = w.p[1].on = true;
    w.p[0].x = (float)yardCellCenter(5);
    w.p[1].x = (float)yardCellCenter(8);
    w.p[0].y = w.p[1].y = (float)yardCellCenter(7);
    w.p[1].until = 1u << 30;
    w.p[0].sceneDist = PAL_NEAR_PX;  // sin la escena no sirve de nada
    if (scene) w.p[0].task = w.p[1].task = YT_SCENE;
    CHECK(yardWalkTo(w, walk, 0, yardCellIdx(7, 7), 0));
    uint8_t hold[YARD_PETS_MAX] = {};
    for (uint32_t t = 85; t < 6000; t += 85) yardStep(w, walk, t, 85, hold, 0);
    float d = yardDist(w.p[0].x, w.p[0].y, w.p[1].x, w.p[1].y);
    if (scene) CHECK(d < YARD_MIN_DIST);
    else CHECK(d >= YARD_MIN_DIST - 1);
  }
}
