// Pantallas de los viajes y la coleccion (travel_ui.h, v2.0 Y5b): geometria tocable (>= 48 px, dentro del
// circulo util, sin solaparse), el texto de la cuenta atras, de la hora y de la fecha, el registro de
// sucesos y la seleccion de bicho / mapa.
#include "framework.h"
#include "../travel_ui.h"

static std::string s(void (*f)(char *, int, uint32_t), uint32_t v) {
  char b[16];
  f(b, sizeof(b), v);
  return b;
}

TEST(travel_ui, cuenta_atras_con_y_sin_horas) {
  CHECK_EQ(s(trvClockStr, 8 * 3600), std::string("8:00:00"));
  CHECK_EQ(s(trvClockStr, 3600), std::string("1:00:00"));
  CHECK_EQ(s(trvClockStr, 3599), std::string("59:59"));
  CHECK_EQ(s(trvClockStr, 65), std::string("1:05"));
  CHECK_EQ(s(trvClockStr, 0), std::string("0:00"));
  CHECK_EQ(s(trvClockStr, 2 * 3600 + 5 * 60 + 9), std::string("2:05:09"));
}

TEST(travel_ui, lo_que_falta_en_la_caja_sube_los_minutos) {
  CHECK_EQ(s(trvHMStr, 3 * 3600), std::string("3:00"));
  CHECK_EQ(s(trvHMStr, 3 * 3600 - 1), std::string("3:00"));
  CHECK_EQ(s(trvHMStr, 61), std::string("0:02"));
  CHECK_EQ(s(trvHMStr, 1), std::string("0:01"));  // con el viaje en marcha nunca "0:00"
}

TEST(travel_ui, hora_y_fecha_del_epoch) {
  // 2026-06-15 10:05:00 (el reloj de las capturas)
  uint32_t e = 1781481600UL + 10 * 3600 + 5 * 60;
  CHECK_EQ(s(trvTimeStr, e), std::string("10:05"));
  CHECK_EQ(s(trvDateStr, e), std::string("15/06"));
  CHECK_EQ(s(trvDateStr, 1781481600UL + 86400 * 20), std::string("05/07"));
  CHECK_EQ(s(trvDateStr, 1767225600UL), std::string("01/01"));  // 2026-01-01
  CHECK_EQ(s(trvDateStr, 1709164800UL), std::string("29/02"));  // 2024-02-29: bisiesto
  CHECK_EQ(s(trvTimeStr, 86399), std::string("23:59"));
  CHECK_EQ(trvTierHours(0), (uint8_t)1);
  CHECK_EQ(trvTierHours(1), (uint8_t)3);
  CHECK_EQ(trvTierHours(2), (uint8_t)8);
}

TEST(travel_ui, sucesos_visibles_segun_los_minutos) {
  uint8_t idx[TRV_EVENTS_MAX];
  for (uint8_t tier = 0; tier < TRV_TIERS; tier++) {
    for (uint32_t seed : { 1u, 77u, 0xDEADBEEFu }) {
      CHECK_EQ(trvVisibleEvents(seed, 1, tier, 0, idx), (uint8_t)0);  // al salir no hay nada
      CHECK_EQ(trvVisibleEvents(seed, 1, tier, TRV_MIN[tier], idx), travelEventCount(tier));  // al volver, todos
      uint8_t prev = 0;
      for (uint16_t m = 0; m <= TRV_MIN[tier]; m++) {
        uint8_t c = trvVisibleEvents(seed, 1, tier, m, idx);
        CHECK(c >= prev);  // solo van apareciendo
        prev = c;
        for (uint8_t k = 0; k < c; k++) CHECK_EQ(idx[k], k);  // y siempre en orden
      }
    }
  }
}

TEST(travel_ui, candidatos_y_mapas_propios) {
  uint8_t out[8];
  CHECK_EQ(trvCandidates(0, out), (uint8_t)0);
  CHECK_EQ(trvCandidates(0b101010, out), (uint8_t)3);
  CHECK_EQ(out[0], (uint8_t)1);
  CHECK_EQ(out[1], (uint8_t)3);
  CHECK_EQ(out[2], (uint8_t)5);
  CHECK_EQ(trvPetPages(0), (uint8_t)1);
  CHECK_EQ(trvPetPages(3), (uint8_t)1);
  CHECK_EQ(trvPetPages(4), (uint8_t)2);
  CHECK_EQ(trvPetPages(6), (uint8_t)2);
  // mapas: solo el prado (bit 0): no hay otro
  CHECK_EQ(trvStepDest(0b00001, 1, 1), (uint8_t)1);
  CHECK_EQ(trvStepDest(0b00001, 1, -1), (uint8_t)1);
  // prado + playa (bits 0 y 3): da la vuelta en los dos sentidos
  CHECK_EQ(trvStepDest(0b01001, 1, 1), (uint8_t)4);
  CHECK_EQ(trvStepDest(0b01001, 4, 1), (uint8_t)1);
  CHECK_EQ(trvStepDest(0b01001, 1, -1), (uint8_t)4);
  CHECK_EQ(trvStepDest(0b01001, 4, -1), (uint8_t)1);
  CHECK_EQ(trvStepDest(0b11111, 5, 1), (uint8_t)1);
  CHECK_EQ(trvStepDest(0b11111, 1, -1), (uint8_t)5);
  // v3.15: 9 mapas, la mascara es de 16 bits (el mapa 9 es el bit 8, que no cabia en 8)
  CHECK_EQ(trvStepDest((uint16_t)((1u << 8) | 1u), 1, 1), (uint8_t)9);
  CHECK_EQ(trvStepDest((uint16_t)((1u << 8) | 1u), 9, 1), (uint8_t)1);
  CHECK_EQ(trvStepDest((uint16_t)((1u << 8) | 1u), 1, -1), (uint8_t)9);
  CHECK_EQ(trvStepDest((uint16_t)((1u << TRV_DESTS) - 1), TRV_DESTS, 1), (uint8_t)1);
  CHECK_EQ(trvStepDest((uint16_t)((1u << TRV_DESTS) - 1), 5, 1), (uint8_t)6);
  CHECK_EQ(trvStepDest((uint16_t)((1u << 6) | (1u << 1)), 2, 1), (uint8_t)7);  // cantera -> central
}

struct R {
  int x, y, w, h;
};
static bool safe(const R &r) {
  return yInsideSafe(r.x, r.y, 0) && yInsideSafe(r.x + r.w, r.y, 0) && yInsideSafe(r.x, r.y + r.h, 0) &&
         yInsideSafe(r.x + r.w, r.y + r.h, 0);
}

TEST(travel_ui, pantalla_de_salida_tocable_y_dentro_del_circulo) {
  std::vector<R> hits;
  hits.push_back({ TRV_DEST_HIT_LX, TRV_DEST_HIT_Y, TRV_DEST_HIT_W, TRV_DEST_HIT_H });
  hits.push_back({ TRV_DEST_HIT_RX, TRV_DEST_HIT_Y, TRV_DEST_HIT_W, TRV_DEST_HIT_H });
  hits.push_back({ TRV_PET_ARROW_X - TRV_PET_ARROW_W / 2, TRV_PET_Y, TRV_PET_ARROW_W, TRV_PET_H });
  hits.push_back({ 466 - TRV_PET_ARROW_X - TRV_PET_ARROW_W / 2, TRV_PET_Y, TRV_PET_ARROW_W, TRV_PET_H });
  for (int i = 0; i < TRV_PET_PAGE; i++) hits.push_back({ trvPetX(i), TRV_PET_Y, TRV_PET_W, TRV_PET_H });
  for (int i = 0; i < TRV_TIERS; i++) hits.push_back({ trvTierX(i), TRV_TIER_Y, TRV_TIER_W, TRV_TIER_H });
  int ex = (Y_HIT_MIN - Y_PILL_H) / 2;
  hits.push_back({ trvGoX(), TRV_GO_Y - ex, TRV_GO_W, Y_PILL_H + 2 * ex });
  for (const R &r : hits) CHECK(r.w >= Y_HIT_MIN && r.h >= Y_HIT_MIN);
  // lo dibujado tambien dentro: la ventana, las tarjetas y el boton
  CHECK(safe({ TRV_BAN_X, TRV_BAN_Y, TRV_BAN_W, TRV_BAN_H }));
  CHECK(safe({ trvPetX(0), TRV_PET_Y, TRV_PET_PAGE * TRV_PET_W + (TRV_PET_PAGE - 1) * TRV_PET_GAP, TRV_PET_H }));
  CHECK(safe({ trvTierX(0), TRV_TIER_Y, TRV_TIERS * TRV_TIER_W + (TRV_TIERS - 1) * TRV_TIER_GAP, TRV_TIER_H }));
  CHECK(safe({ trvGoX(), TRV_GO_Y, TRV_GO_W, Y_PILL_H }));
  for (const R &r : hits) CHECK(safe(r));
  // ninguna zona pisa a otra ni al boton de volver, y todas por debajo del titulo
  for (size_t a = 0; a < hits.size(); a++) {
    CHECK(hits[a].y >= Y_TITLE_Y);
    CHECK(hits[a].y + hits[a].h < Y_BACK_Y - Y_BACK_HIT_R);
    for (size_t b = a + 1; b < hits.size(); b++) {
      const R &p = hits[a], &q = hits[b];
      bool apart = p.x + p.w <= q.x || q.x + q.w <= p.x || p.y + p.h <= q.y || q.y + q.h <= p.y;
      CHECK(apart);
    }
  }
  // el centro de cada cosa devuelve su zona
  CHECK_EQ(trvHit(TRV_BAN_X + 10, TRV_BAN_Y + 30).kind, (uint8_t)TH_DEST_L);
  CHECK_EQ(trvHit(TRV_BAN_X + TRV_BAN_W - 10, TRV_BAN_Y + 30).kind, (uint8_t)TH_DEST_R);
  CHECK_EQ(trvHit(TRV_PET_ARROW_X, TRV_PET_Y + 30).kind, (uint8_t)TH_PET_L);
  CHECK_EQ(trvHit(466 - TRV_PET_ARROW_X, TRV_PET_Y + 30).kind, (uint8_t)TH_PET_R);
  for (int i = 0; i < TRV_PET_PAGE; i++) {
    TrvHit h = trvHit(trvPetX(i) + TRV_PET_W / 2, TRV_PET_Y + TRV_PET_H / 2);
    CHECK_EQ(h.kind, (uint8_t)TH_PET);
    CHECK_EQ(h.i, (uint8_t)i);
  }
  for (int i = 0; i < TRV_TIERS; i++) {
    TrvHit h = trvHit(trvTierX(i) + TRV_TIER_W / 2, TRV_TIER_Y + TRV_TIER_H / 2);
    CHECK_EQ(h.kind, (uint8_t)TH_TIER);
    CHECK_EQ(h.i, (uint8_t)i);
  }
  CHECK_EQ(trvHit(Y_CX, TRV_GO_Y + Y_PILL_H / 2).kind, (uint8_t)TH_GO);
  CHECK_EQ(trvHit(Y_CX, TRV_GO_Y - 8).kind, (uint8_t)TH_GO);  // la zona estirada a 48
  CHECK_EQ(trvHit(Y_BACK_X, Y_BACK_Y).kind, (uint8_t)TH_NONE);
}

TEST(travel_ui, viaje_en_marcha_dentro_del_circulo) {
  CHECK(safe({ TRV_WIN_X, TRV_WIN_Y, TRV_WIN_W, TRV_WIN_H }));
  CHECK(TRV_CLOCK_Y + 32 <= TRV_WIN_Y);                 // la cuenta atras (tamano 4) no pisa la ventana
  CHECK(TRV_WIN_Y + TRV_WIN_H < TRV_LOG_Y);
  // 3 sucesos de 2 lineas (+ separacion) caben entre el registro y el boton de volver
  int logH = TRV_EVENTS_MAX * (2 * TRV_LOG_LINE + TRV_LOG_GAP);
  CHECK(TRV_LOG_Y + logH < Y_BACK_Y - Y_BACK_R - 8);
  CHECK(safe({ TRV_LOG_X, TRV_LOG_Y, TRV_LOG_W, logH }));
}

TEST(travel_ui, coleccion_paginas_casillas_tocables_y_dentro) {
  // v3.15: 45 recuerdos en paginas de 5 mapas (5 x 5 casillas): la 1.a mapas 1-5, la 2.a mapas 6-9
  CHECK_EQ(SOUV_PAGES, 2);
  CHECK_EQ(souvPageCount(0), 25);
  CHECK_EQ(souvPageCount(1), SOUV_COUNT - 25);
  CHECK_EQ(souvPageCount(SOUV_PAGES), 0);
  int total = 0;
  for (int pg = 0; pg < SOUV_PAGES; pg++) total += souvPageCount(pg);
  CHECK_EQ(total, SOUV_COUNT);
  for (int id = 0; id < SOUV_COUNT; id++) {
    int pg = souvPageOf(id);
    CHECK(id >= souvPageFirst(pg) && id < souvPageFirst(pg) + souvPageCount(pg));
    R r = { souvCellX(id), souvCellY(id), SOUV_CELL_W, SOUV_CELL_H };
    CHECK(r.w >= Y_HIT_MIN && r.h >= Y_HIT_MIN);
    CHECK(safe(r));
    CHECK(r.y >= SOUV_TITLE_Y + 16);              // bajo "Coleccion N/45"
    CHECK(r.y >= SOUV_PAGER_Y + 14 + 4);          // y bajo el "1/2"
    CHECK(r.y + r.h < SOUV_HINT_Y);               // sobre los puntos de la pagina del jugador
    CHECK_EQ(souvAt(pg, r.x + r.w / 2, r.y + r.h / 2), id);
    CHECK_EQ(souvAt(pg, r.x, r.y), id);
    CHECK_EQ(souvAt(pg, r.x + r.w - 1, r.y + r.h - 1), id);
    for (int other = 0; other < SOUV_PAGES; other++)  // en otra pagina, esa casilla es otro recuerdo (o nada)
      if (other != pg) CHECK(souvAt(other, r.x + r.w / 2, r.y + r.h / 2) != id);
    // una fila por mapa: el id lo dice
    CHECK_EQ(id / SOUV_COLS, (int)souvenirDest((uint8_t)id) - 1);
    // las flechas de pagina (las del selector, a los lados) no pisan ninguna casilla
    CHECK_EQ(yChevronHit(r.x, r.y + r.h / 2), 0);
    CHECK_EQ(yChevronHit(r.x + r.w - 1, r.y + r.h / 2), 0);
  }
  // la 2.a pagina tiene 4 filas: la 5.a, vacia
  CHECK_EQ(souvAt(1, souvCellX(0) + 5, souvCellY(20) + 5), -1);
  CHECK(souvCellY(24) + SOUV_CELL_H < SOUV_HINT_Y);
  CHECK(SOUV_HINT_Y + 14 < Y_BACK_Y - Y_BACK_R);
  CHECK_EQ(souvAt(0, Y_CX, 20), -1);
  CHECK_EQ(souvAt(0, souvCellX(0) + SOUV_CELL_W + 1, souvCellY(0) + 5), -1);  // el hueco entre casillas
  CHECK_EQ(souvAt(SOUV_PAGES, souvCellX(0) + 5, souvCellY(0) + 5), -1);
  // las flechas, dentro del circulo util
  CHECK(yInsideSafe(Y_CHEV_LX - Y_CHEV_HALF, Y_CY, 0) && yInsideSafe(Y_CHEV_RX + Y_CHEV_HALF, Y_CY, 0));
  // pasar de pagina: las flechas no salen de las paginas; el "1/2" da la vuelta
  CHECK_EQ(souvPageStep(0, 1), 1);
  CHECK_EQ(souvPageStep(1, -1), 0);
  CHECK_EQ(souvPageStep(0, -1), -1);
  CHECK_EQ(souvPageStep(SOUV_PAGES - 1, 1), -1);
  CHECK_EQ(souvPageStep(0, 0), 1);
  CHECK_EQ(souvPageStep(SOUV_PAGES - 1, 0), 0);
  // la tarjeta del recuerdo, dentro del circulo
  CHECK(safe({ SOUV_TIP_X, SOUV_TIP_Y, SOUV_TIP_W, SOUV_TIP_H }));
}

TEST(travel_ui, pestanas_de_la_mochila) {
  int ex = (Y_HIT_MIN - BAG_TAB_H) / 2;
  for (int t = 0; t < BAG_TABS; t++) {
    CHECK(BAG_TAB_W >= Y_HIT_MIN);
    CHECK(BAG_TAB_H + 2 * ex >= Y_HIT_MIN);
    CHECK(safe({ bagTabX(t), BAG_TAB_TOP, BAG_TAB_W, BAG_TAB_H }));
    CHECK_EQ(bagTabHit(bagTabX(t) + BAG_TAB_W / 2, BAG_TAB_TOP + BAG_TAB_H / 2), t);
    CHECK_EQ(bagTabHit(bagTabX(t), BAG_TAB_TOP - ex), t);
  }
  CHECK_EQ(bagTabHit(bagTabX(0) - 3, BAG_TAB_TOP + 10), -1);
  CHECK_EQ(bagTabHit(Y_CX, 120), -1);
}
