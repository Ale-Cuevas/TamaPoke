// Efectos de respuesta (fx.h, v1.30): la cola de celebraciones, los efectos pequenos y los
// vigilantes que detectan una subida de nivel, vinculo o descuidos.
#include "framework.h"
#include "../fx.h"

// ---------------------------------------------------------------- cola de celebraciones
TEST(fx, dos_subidas_pendientes_son_una_sola) {
  FxQueue q;
  CHECK(q.push(FXB_LEVELUP, 12, 13, true));
  CHECK(q.push(FXB_LEVELUP, 13, 14, true));
  CHECK(q.push(FXB_LEVELUP, 14, 18, true));
  CHECK_EQ((int)q.n, 1);
  CHECK_EQ((int)q.q[0].a, 12);
  CHECK_EQ((int)q.q[0].b, 18);
  CHECK(q.q[0].chimed);
}

TEST(fx, si_una_parte_no_sono_la_subida_suena_al_verse) {
  FxQueue q;
  q.push(FXB_LEVELUP, 3, 4, true);    // con el bicho despierto: ya sono
  q.push(FXB_LEVELUP, 4, 5, false);   // dormido: no sono
  CHECK(!q.q[0].chimed);
}

TEST(fx, sale_por_prioridad_y_en_orden_de_llegada) {
  FxQueue q;
  q.push(FXB_GOAL, 1);
  q.push(FXB_MEDAL, 1);
  q.push(FXB_MILESTONE, 7);
  q.push(FXB_MEDAL, 2);
  q.push(FXB_LEVELUP, 9, 10);
  const uint8_t want[] = { FXB_LEVELUP, FXB_MEDAL, FXB_MEDAL, FXB_MILESTONE, FXB_GOAL };
  uint32_t now = 1000;
  for (uint8_t k : want) {
    const FxEvent *e = q.step(now, false);
    CHECK(e != nullptr);
    if (e) CHECK_EQ((int)e->kind, (int)k);
    now += e ? e->dur : 1;
  }
  // las dos medallas, en el orden en que llegaron
  FxQueue r;
  r.push(FXB_MEDAL, 1);
  r.push(FXB_MEDAL, 2);
  CHECK_EQ((int)r.step(0, false)->a, 1);
}

TEST(fx, una_repetida_exacta_se_descarta) {
  FxQueue q;
  q.push(FXB_MEDAL, 4);
  q.push(FXB_MEDAL, 4);
  q.push(FXB_GOAL, 2);
  q.push(FXB_GOAL, 2);
  CHECK_EQ((int)q.n, 2);
}

TEST(fx, la_cola_llena_no_desborda) {
  FxQueue q;
  for (int i = 0; i < FX_QUEUE; i++) CHECK(q.push(FXB_MEDAL, (uint16_t)(1 << i)));
  CHECK(!q.push(FXB_MEDAL, 0x4000));
  CHECK_EQ((int)q.n, FX_QUEUE);
  CHECK(q.push(FXB_LEVELUP, 1, 2) == false);  // llena tambien para una subida nueva
}

TEST(fx, el_reloj_empieza_al_mostrarse_no_al_ganarse) {
  FxQueue q;
  q.push(FXB_MEDAL, 1);  // ganada en t=0 con la pantalla apagada
  for (uint32_t t = 0; t < 60000; t += 500) CHECK(q.step(t, true) == nullptr);  // ocupada: espera
  bool started = false;
  const FxEvent *e = q.step(60000, false, &started);
  CHECK(e != nullptr);
  CHECK(started);
  CHECK(q.step(60000 + FX_MEDAL_MS - 1, false, &started) != nullptr);
  CHECK(!started);  // solo el primer paso dice "empieza"
  CHECK(q.step(60000 + FX_MEDAL_MS, false) == nullptr);  // y se va a su hora
  CHECK(!q.busy());
}

TEST(fx, la_que_ya_se_ve_no_la_corta_otra_de_mas_prioridad) {
  FxQueue q;
  q.push(FXB_GOAL, 1);
  const FxEvent *e = q.step(0, false);
  CHECK_EQ((int)e->kind, (int)FXB_GOAL);
  q.push(FXB_LEVELUP, 4, 5);
  CHECK_EQ((int)q.step(100, false)->kind, (int)FXB_GOAL);
  CHECK_EQ((int)q.step(FX_GOAL_MS, false)->kind, (int)FXB_LEVELUP);
}

// caja (v1.37): al cambiar de bicho se quitan su subida y su medalla; lo del jugador sigue
TEST(fx, drop_quita_un_tipo_tambien_el_que_se_ve) {
  FxQueue q;
  q.push(FXB_MEDAL, 1);
  q.push(FXB_GOAL, 2);
  q.push(FXB_LEVELUP, 4, 6);
  q.push(FXB_MEDAL, 2);
  q.push(FXB_MILESTONE, 7);
  CHECK_EQ((int)q.step(0, false)->kind, (int)FXB_LEVELUP);  // la subida, en pantalla
  q.drop(FXB_LEVELUP);
  q.drop(FXB_MEDAL);
  CHECK(!q.active);
  CHECK_EQ((int)q.n, 2);
  const FxEvent *e = q.step(10, false);
  CHECK_EQ((int)e->kind, (int)FXB_MILESTONE);
  CHECK_EQ((int)q.step(10 + e->dur, false)->kind, (int)FXB_GOAL);
  q.drop(FXB_MEDAL);  // sin ninguna: nada cambia
  CHECK(q.active);
}

TEST(fx, la_meta_de_10000_dura_mas) {
  CHECK_EQ(fxBannerMs(FXB_GOAL, 1), FX_GOAL_MS);
  CHECK_EQ(fxBannerMs(FXB_GOAL, FX_GOAL_TOP_TIER), FX_GOAL_TOP_MS);
  CHECK_EQ(fxBannerMs(FXB_LEVELUP, 0), FX_LEVELUP_MS + FX_GAINS_MS);
}

TEST(fx, el_progreso_va_de_0_a_1) {
  FxQueue q;
  CHECK(q.t(5) == 0.0f);
  q.push(FXB_MILESTONE, 3);
  q.step(1000, false);
  CHECK(q.t(1000) == 0.0f);
  float mid = q.t(1000 + FX_MILESTONE_MS / 2);
  CHECK(mid > 0.49f && mid < 0.51f);
  CHECK(q.t(1000 + FX_MILESTONE_MS + 50) == 1.0f);
}

TEST(fx, los_relojes_aguantan_la_vuelta_de_millis) {
  FxQueue q;
  q.push(FXB_MEDAL, 2);
  uint32_t t0 = 0xFFFFFF00u;  // a 256 ms de dar la vuelta
  CHECK(q.step(t0, false) != nullptr);
  CHECK(q.step(t0 + 1000, false) != nullptr);          // ya dio la vuelta: sigue en pantalla
  CHECK(q.step(t0 + FX_MEDAL_MS, false) == nullptr);   // y acaba a su hora

  FxOverlays o;
  o.push(FXO_STAT, 0, 5, t0);
  o.step(t0 + 10, true);
  CHECK(o.find(FXO_STAT) != nullptr);
  o.step(t0 + 10 + FX_FLOAT_MS, true);
  CHECK(o.find(FXO_STAT) == nullptr);
}

// ---------------------------------------------------------------- efectos pequenos
TEST(fx, un_efecto_espera_a_verse_y_luego_corre_su_tiempo) {
  FxOverlays o;
  o.push(FXO_STAT, 1, 25, 0);  // al acabar el minijuego: la casa aun no se ve
  o.step(3000, false);
  CHECK(o.find(FXO_STAT) == nullptr);
  CHECK(o.waiting(FXO_STAT));
  o.step(4000, true);  // se cierra la pantalla de fin: ahora si
  const FxOverlay *x = o.find(FXO_STAT, 1);
  CHECK(x != nullptr);
  if (x) CHECK_EQ((int)x->v, 25);
  o.step(4000 + FX_FLOAT_MS - 1, true);
  CHECK(o.find(FXO_STAT) != nullptr);
  o.step(4000 + FX_FLOAT_MS, true);
  CHECK(o.find(FXO_STAT) == nullptr);
}

TEST(fx, un_efecto_que_no_se_ve_a_tiempo_se_descarta) {
  FxOverlays o;
  o.push(FXO_BOND, 0, 1, 0);
  o.step(FX_WAIT_MS, false);
  CHECK(o.waiting(FXO_BOND));
  o.step(FX_WAIT_MS + 1, false);
  CHECK(!o.waiting(FXO_BOND));
  o.step(FX_WAIT_MS + 2, true);
  CHECK(o.find(FXO_BOND) == nullptr);  // no reaparece al volver
}

TEST(fx, cada_efecto_lleva_su_reloj) {
  FxOverlays o;
  o.push(FXO_PRESS, 2, 0, 0);
  o.push(FXO_OOPS, 0, 0, 0);
  o.step(0, true);
  o.step(FX_PRESS_MS, true);
  CHECK(o.find(FXO_PRESS) == nullptr);  // el boton ya subio...
  CHECK(o.find(FXO_OOPS) != nullptr);   // ...y el "!" sigue
  o.step(FX_OOPS_MS, true);
  CHECK(o.find(FXO_OOPS) == nullptr);
}

TEST(fx, el_mismo_efecto_sobre_lo_mismo_vuelve_a_empezar) {
  FxOverlays o;
  o.push(FXO_STAT, 0, 10, 0);
  o.step(0, true);
  o.push(FXO_STAT, 0, 25, 500);  // otra comida antes de que se acabe el primer "+"
  o.push(FXO_STAT, 1, 5, 500);   // otra barra: efecto aparte
  int n = 0;
  for (auto &x : o.o) n += x.on;
  CHECK_EQ(n, 2);
  o.step(500, true);
  const FxOverlay *x = o.find(FXO_STAT, 0);
  CHECK(x != nullptr);
  if (x) {
    CHECK_EQ((int)x->v, 25);
    CHECK_EQ(x->start, 500u);
  }
}

TEST(fx, sin_hueco_se_reemplaza_el_mas_antiguo) {
  FxOverlays o;
  for (int i = 0; i < FX_OVERLAYS; i++) o.push(FXO_DENY, (uint8_t)i, 0, (uint32_t)(100 + i));
  o.push(FXO_OOPS, 0, 0, 500);
  bool oldest = false, oops = false;
  for (auto &x : o.o) {
    if (x.on && x.kind == FXO_DENY && x.a == 0) oldest = true;
    if (x.on && x.kind == FXO_OOPS) oops = true;
  }
  CHECK(!oldest);
  CHECK(oops);
}

// ---------------------------------------------------------------- vigilantes
TEST(fx, el_vigilante_solo_avisa_al_subir) {
  FxWatch w;
  uint32_t from = 0;
  CHECK(!w.rose(5));       // la primera lectura solo se apunta
  CHECK(!w.rose(5));
  CHECK(w.rose(7, &from));
  CHECK_EQ(from, 5u);
  CHECK(!w.rose(3));       // bajar no es un evento...
  CHECK(w.rose(4, &from)); // ...pero cuenta desde ahi
  CHECK_EQ(from, 3u);
}

TEST(fx, un_salto_offline_es_un_solo_evento) {
  // el vigilante se arma antes de aplicar el tiempo apagado (setup, antes de syncClock)
  FxWatch lv;
  lv.reset(12);
  uint32_t from = 0;
  CHECK(lv.rose(18, &from));  // 6 horas despues, en la primera vuelta del loop
  CHECK_EQ(from, 12u);
  CHECK(!lv.rose(18));
  FxQueue q;
  q.push(FXB_LEVELUP, (uint16_t)from, 18, true);
  CHECK_EQ((int)q.n, 1);
}

TEST(fx, desarmado_no_confunde_un_huevo_que_eclosiona_con_un_nivel) {
  FxWatch lv;
  lv.reset(30);   // el bicho anterior
  lv.disarm();    // ahora es un huevo
  CHECK(!lv.rose(1));  // recien nacido, nivel 1: sin evento
  CHECK(lv.rose(2));
}

TEST(fx, la_puerta_deja_pasar_uno_cada_tanto) {
  FxGate g;
  CHECK(g.allow(1000, FX_BOND_GAP_MS));
  CHECK(!g.allow(1000 + FX_BOND_GAP_MS - 1, FX_BOND_GAP_MS));
  CHECK(g.allow(1000 + FX_BOND_GAP_MS, FX_BOND_GAP_MS));
}

// ---------------------------------------------------------------- barras
TEST(fx, la_barra_se_desliza_al_valor_nuevo) {
  FxTween b;
  b.target(40, 0);  // primera vez: sin animacion
  CHECK_EQ(b.value(0), 40);
  b.target(80, 1000);
  CHECK_EQ(b.value(1000), 40);
  CHECK_EQ(b.value(1000 + FX_TWEEN_MS / 2), 60);
  CHECK_EQ(b.value(1000 + FX_TWEEN_MS), 80);
  // a medias, otro cambio: sale de donde se ve
  b.target(20, 2000);
  b.target(0, 2000 + FX_TWEEN_MS / 2);
  CHECK_EQ(b.value(2000 + FX_TWEEN_MS / 2), 50);
  CHECK_EQ(b.value(2000 + FX_TWEEN_MS * 2), 0);
}

TEST(fx, el_azar_de_los_efectos_es_repetible) {
  CHECK_EQ(fxHash(7), fxHash(7));
  CHECK(fxHash(7) != fxHash(8));
}
