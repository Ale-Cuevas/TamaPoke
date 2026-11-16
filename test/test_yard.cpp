// El patio (yard.h, v2.0 fase Y2): los bichos que pasean por el mapa. Nunca pisan una celda
// bloqueada, llegan a donde van, guardan la distancia entre ellos, se ordenan para dibujarse y
// la inclinacion los lleva al lado bajo. El azar es inyectado: todo es determinista.
#include "framework.h"
#include "../yard.h"

static void block(uint8_t *walk, int cx, int cy) {
  int k = cy * YBG_GRID + cx;
  walk[k >> 3] |= (uint8_t)(1 << (k & 7));
}

// la mascara de respaldo y un "estanque" de 3x3 celdas en el centro y un muro con un hueco
static void testWalk(uint8_t *walk) {
  yardFallbackWalk(walk);
  for (int y = 6; y <= 8; y++)
    for (int x = 6; x <= 8; x++) block(walk, x, y);
  for (int y = 3; y <= 11; y++)
    if (y != 10) block(walk, 3, y);
}

static bool gestureNod(uint8_t, uint8_t *act, uint32_t *ms) {
  *act = 9;
  *ms = 1200;
  return true;
}

TEST(yard, mascara_de_respaldo_como_pack_yard) {
  uint8_t walk[YBG_WALK_BYTES];
  yardFallbackWalk(walk);
  // el centro se pisa; las esquinas (fuera del circulo), el reloj y el dock no
  CHECK(!yardCellBlocked(walk, 7, 7));
  CHECK(yardCellBlocked(walk, 0, 0));
  CHECK(yardCellBlocked(walk, 14, 14));
  CHECK(yardCellBlocked(walk, 7, 1));   // bajo el reloj (y 57)
  CHECK(yardCellBlocked(walk, 7, 12));  // bajo el dock (y 393)
  CHECK(!yardCellBlocked(walk, 7, 4));
  // los centros de las celdas libres caen dentro del circulo util
  for (int c = 0; c < YARD_CELLS; c++) {
    if (!yardFree(walk, c)) continue;
    int dx = yardCellCenter(yardCellCX(c)) - 233, dy = yardCellCenter(yardCellCY(c)) - 233;
    CHECK(dx * dx + dy * dy <= 221 * 221);
  }
  CHECK(yardFreeCount(walk) > 100);
}

TEST(yard, camino_rodea_lo_bloqueado) {
  uint8_t walk[YBG_WALK_BYTES];
  testWalk(walk);
  uint8_t path[YARD_PATH_MAX];
  int from = yardCellIdx(2, 5), to = yardCellIdx(10, 7);
  int n = yardPath(walk, from, to, path, YARD_PATH_MAX);
  CHECK(n > 0);
  CHECK_EQ((int)path[n - 1], to);
  int prev = from;
  for (int i = 0; i < n; i++) {
    CHECK(yardFree(walk, path[i]));
    int d = abs(yardCellCX(path[i]) - yardCellCX(prev)) + abs(yardCellCY(path[i]) - yardCellCY(prev));
    CHECK_EQ(d, 1);  // vecinos de 4
    prev = path[i];
  }
  // a una celda bloqueada, o encerrada, no hay camino
  CHECK_EQ(yardPath(walk, from, yardCellIdx(7, 7), path, YARD_PATH_MAX), 0);
  uint8_t closed[YBG_WALK_BYTES];
  yardFallbackWalk(closed);
  for (int d = -1; d <= 1; d++) {
    block(closed, 5 + d, 4);
    block(closed, 5 + d, 6);
  }
  block(closed, 4, 5);
  block(closed, 6, 5);
  CHECK_EQ(yardPath(closed, yardCellIdx(5, 5), yardCellIdx(9, 9), path, YARD_PATH_MAX), 0);
}

TEST(yard, nunca_pisa_una_celda_bloqueada_y_llega) {
  uint8_t walk[YBG_WALK_BYTES];
  testWalk(walk);
  YardWorld w;
  w.rng.s = 77;
  w.n = 1;
  w.p[0].on = true;
  CHECK(yardPlace(w, walk, 0));
  int walks = 0, arrived = 0;
  uint32_t now = 0;
  for (int f = 0; f < 6000; f++) {  // 10 min a 10 fps
    now += 100;
    uint8_t before = w.p[0].mode;
    int target = before == YM_WALK && w.p[0].pathN ? w.p[0].path[w.p[0].pathN - 1] : -1;
    yardStep(w, walk, now, 100, nullptr, 0);
    CHECK(yardSpotOk(walk, w.p[0].x, w.p[0].y));
    if (before != YM_WALK && w.p[0].mode == YM_WALK) walks++;
    if (before == YM_WALK && w.p[0].mode != YM_WALK && target >= 0 && yardCellAt(w.p[0].x, w.p[0].y) == target) arrived++;
  }
  CHECK(walks > 20);
  CHECK_EQ(arrived, walks - (w.p[0].mode == YM_WALK ? 1 : 0));  // solo, siempre llega
}

TEST(yard, dos_bichos_guardan_la_distancia) {
  uint8_t walk[YBG_WALK_BYTES];
  testWalk(walk);
  YardWorld w;
  w.rng.s = 4242;
  w.n = 2;
  w.pickGesture = gestureNod;
  for (int i = 0; i < 2; i++) {
    w.p[i].on = true;
    CHECK(yardPlace(w, walk, i));
  }
  CHECK(yardDist(w.p[0].x, w.p[0].y, w.p[1].x, w.p[1].y) >= YARD_MIN_DIST);
  uint32_t now = 0;
  float closest = 1e9f;
  int gestures = 0;
  for (int f = 0; f < 12000; f++) {
    now += 100;
    uint8_t m0 = w.p[0].mode;
    yardStep(w, walk, now, 100, nullptr, 0);
    if (m0 != YM_GESTURE && w.p[0].mode == YM_GESTURE) gestures++;
    for (int i = 0; i < 2; i++) CHECK(yardSpotOk(walk, w.p[i].x, w.p[i].y));
    float d = yardDist(w.p[0].x, w.p[0].y, w.p[1].x, w.p[1].y);
    if (d < closest) closest = d;
  }
  CHECK(closest >= YARD_MIN_DIST - 0.01f);
  CHECK(gestures > 5);
  CHECK_EQ((int)w.p[0].act, 9);
}

TEST(yard, empiezan_pegados_y_se_separan) {
  uint8_t walk[YBG_WALK_BYTES];
  yardFallbackWalk(walk);
  YardWorld w;
  w.rng.s = 9;
  w.n = 2;
  w.p[0].on = w.p[1].on = true;
  w.p[0].x = 233; w.p[0].y = 233;
  w.p[1].x = 243; w.p[1].y = 233;
  uint32_t now = 0;
  for (int f = 0; f < 3000; f++) {
    now += 100;
    yardStep(w, walk, now, 100, nullptr, 0);
  }
  // nadie se queda bloqueado para siempre: alguno se alejo
  CHECK(yardDist(w.p[0].x, w.p[0].y, w.p[1].x, w.p[1].y) >= YARD_MIN_DIST);
}

TEST(yard, orden_de_dibujo_por_y) {
  YardWorld w;
  w.n = 3;
  float ys[3] = { 300, 120, 200 };
  for (int i = 0; i < 3; i++) {
    w.p[i].on = true;
    w.p[i].y = ys[i];
  }
  uint8_t o[YARD_PETS_MAX];
  CHECK_EQ(yardDrawOrder(w, o), 3);
  CHECK_EQ((int)o[0], 1);
  CHECK_EQ((int)o[1], 2);
  CHECK_EQ((int)o[2], 0);
  w.p[2].on = false;
  CHECK_EQ(yardDrawOrder(w, o), 2);
  CHECK_EQ((int)o[0], 1);
  CHECK_EQ((int)o[1], 0);
}

TEST(yard, dormido_y_quieto_no_se_mueven) {
  uint8_t walk[YBG_WALK_BYTES];
  yardFallbackWalk(walk);
  YardWorld w;
  w.n = 1;
  w.p[0].on = true;
  w.p[0].x = yardCellCenter(7);
  w.p[0].y = yardCellCenter(9);
  uint8_t hold[YARD_PETS_MAX] = { YH_SLEEP };
  uint32_t now = 0;
  for (int f = 0; f < 600; f++) {
    now += 100;
    yardStep(w, walk, now, 100, hold, 0);
    CHECK_EQ((int)w.p[0].mode, (int)YM_SLEEP);
  }
  CHECK_EQ(w.p[0].x, (float)yardCellCenter(7));
  hold[0] = YH_STAY;
  yardStep(w, walk, now += 100, 100, hold, 1);  // ni inclinandola
  CHECK_EQ((int)w.p[0].mode, (int)YM_STAY);
  CHECK_EQ(w.p[0].x, (float)yardCellCenter(7));
  hold[0] = YH_NONE;  // al soltarlo vuelve a decidir solo
  yardStep(w, walk, now += 100, 100, hold, 0);
  CHECK_EQ((int)w.p[0].mode, (int)YM_IDLE);
}

TEST(yard, inclinar_lleva_al_lado_bajo) {
  uint8_t walk[YBG_WALK_BYTES];
  testWalk(walk);
  YardWorld w;
  w.n = 1;
  w.p[0].on = true;
  w.p[0].x = yardCellCenter(10);
  w.p[0].y = yardCellCenter(9);
  uint32_t now = 0;
  for (int f = 0; f < 300; f++) {
    now += 100;
    yardStep(w, walk, now, 100, nullptr, -1);
    CHECK(yardSpotOk(walk, w.p[0].x, w.p[0].y));
  }
  // a la izquierda del todo de lo que alcanza (el muro de la columna 3 tiene un hueco en la 10)
  CHECK(yardCellOf((int)w.p[0].x) <= 2);
  CHECK_EQ((int)w.p[0].face, -1);
  for (int f = 0; f < 300; f++) {
    now += 100;
    yardStep(w, walk, now, 100, nullptr, 1);
    CHECK(yardSpotOk(walk, w.p[0].x, w.p[0].y));
  }
  CHECK(yardCellOf((int)w.p[0].x) >= 12);
  CHECK_EQ((int)w.p[0].face, 1);
  // nivelada: se para un momento
  yardStep(w, walk, now += 100, 100, nullptr, 0);
  CHECK_EQ((int)w.p[0].mode, (int)YM_IDLE);
}

TEST(yard, sitio_para_una_caca_y_un_regalo) {
  uint8_t walk[YBG_WALK_BYTES];
  testWalk(walk);
  int16_t ox[3], oy[3];
  int n = 0;
  float px = yardCellCenter(5), py = yardCellCenter(5);
  for (int k = 0; k < 3; k++) {
    int x, y;
    CHECK(yardDropSpot(walk, px, py, ox, oy, n, &x, &y));
    CHECK(yardSpotOk(walk, x, y));
    for (int j = 0; j < n; j++) CHECK(yardDist(x, y, ox[j], oy[j]) >= YARD_THING_GAP);
    ox[n] = (int16_t)x;
    oy[n++] = (int16_t)y;
  }
  YardWorld w;
  YardRng r;
  r.s = 3;
  for (int k = 0; k < 3; k++) {
    int x, y;
    CHECK(yardFreeSpot(w, walk, r, ox, oy, n, &x, &y));
    CHECK(yardSpotOk(walk, x, y));
  }
}

TEST(yard, el_bocadillo_cambia_cada_segundo) {
  CHECK_EQ(yardCycleIndex(0, 5000, 1000), -1);
  CHECK_EQ(yardCycleIndex(1, 5000, 1000), 0);
  CHECK_EQ(yardCycleIndex(3, 0, 1000), 0);
  CHECK_EQ(yardCycleIndex(3, 999, 1000), 0);
  CHECK_EQ(yardCycleIndex(3, 1000, 1000), 1);
  CHECK_EQ(yardCycleIndex(3, 2500, 1000), 2);
  CHECK_EQ(yardCycleIndex(3, 3000, 1000), 0);
}

TEST(yard, toque_sobre_el_bicho) {
  CHECK(yardPetHit(200, 280, 200, 300, 60, 80));   // el cuerpo
  CHECK(yardPetHit(200, 225, 200, 300, 60, 80));   // la cabeza
  CHECK(!yardPetHit(200, 200, 200, 300, 60, 80));  // por encima
  CHECK(!yardPetHit(260, 280, 200, 300, 60, 80));  // al lado
  CHECK(yardPetHit(225, 290, 200, 300, 10, 10));   // uno pequeno se toca con 48 px por lo menos
}

TEST(yard, celda_libre_mas_cercana) {
  uint8_t walk[YBG_WALK_BYTES];
  testWalk(walk);
  CHECK_EQ(yardNearestFreeCell(walk, yardCellCenter(9), yardCellCenter(4)), yardCellIdx(9, 4));
  int c = yardNearestFreeCell(walk, yardCellCenter(7), yardCellCenter(7));  // el estanque
  CHECK(yardFree(walk, c));
  CHECK_EQ(abs(yardCellCX(c) - 7) + abs(yardCellCY(c) - 7), 2);
}

TEST(yard, solo_la_zona_conectada_mas_grande) {
  uint8_t walk[YBG_WALK_BYTES];
  yardFallbackWalk(walk);
  int before = yardFreeCount(walk);
  // una celda suelta en (10, 4) rodeada de bloqueadas
  block(walk, 9, 4); block(walk, 11, 4); block(walk, 10, 3); block(walk, 10, 5);
  CHECK(yardFree(walk, yardCellIdx(10, 4)));
  int n = yardKeepLargestRegion(walk);
  CHECK(!yardFree(walk, yardCellIdx(10, 4)));
  CHECK_EQ(n, before - 5);
  CHECK_EQ(yardFreeCount(walk), n);
  // todo lo que queda se alcanza desde el centro
  uint8_t path[YARD_PATH_MAX];
  for (int c = 0; c < YARD_CELLS; c++)
    if (yardFree(walk, c) && c != yardCellIdx(7, 7)) CHECK(yardPath(walk, yardCellIdx(7, 7), c, path, YARD_PATH_MAX) > 0);
}

// ---------------------------------------------------------------- la comida que cae (Y3)

TEST(yard, la_comida_cae_cerca_en_su_fila_y_alcanzable) {
  uint8_t walk[YBG_WALK_BYTES];
  yardFallbackWalk(walk);
  float px = (float)yardCellCenter(7), py = (float)yardCellCenter(9);
  int c = yardFoodCell(walk, px, py, 1, nullptr, nullptr, 0);
  CHECK(c >= 0);
  CHECK(yardFree(walk, c));
  CHECK_EQ(yardCellCY(c), 9);         // en su fila...
  CHECK_EQ(yardCellCX(c), 9);         // ...a dos celdas, hacia donde mira
  CHECK_EQ(yardCellCX(yardFoodCell(walk, px, py, -1, nullptr, nullptr, 0)), 5);  // mirando a la izquierda
  // nunca debajo del bicho
  float d = yardDist((float)yardCellCenter(yardCellCX(c)), (float)yardCellCenter(yardCellCY(c)), px, py);
  CHECK(d >= YARD_FOOD_MIN_D);
}

TEST(yard, la_comida_no_cae_en_otra_zona_ni_sobre_una_cosa) {
  uint8_t walk[YBG_WALK_BYTES];
  yardFallbackWalk(walk);
  // un muro en la columna 6 de arriba abajo: la mitad izquierda no se alcanza desde la derecha
  for (int y = 0; y < YBG_GRID; y++) block(walk, 6, y);
  float px = (float)yardCellCenter(7), py = (float)yardCellCenter(7);
  int c = yardFoodCell(walk, px, py, -1, nullptr, nullptr, 0);  // mira a la izquierda (al muro)
  CHECK(c >= 0);
  CHECK(yardCellCX(c) > 6);
  // una caca en la mejor celda: elige otra
  int16_t ox[1] = { (int16_t)yardCellCenter(9) }, oy[1] = { (int16_t)yardCellCenter(7) };
  int c2 = yardFoodCell(walk, px, py, 1, ox, oy, 1);
  CHECK(c2 >= 0);
  CHECK(c2 != yardCellIdx(9, 7));
  CHECK(yardDist((float)yardCellCenter(yardCellCX(c2)), (float)yardCellCenter(yardCellCY(c2)), ox[0], oy[0]) >=
        YARD_THING_GAP);
  // sin sitio ninguno: -1
  uint8_t full[YBG_WALK_BYTES];
  memset(full, 0xFF, sizeof(full));
  CHECK_EQ(yardFoodCell(full, px, py, 1, nullptr, nullptr, 0), -1);
}

TEST(yard, alcanzar_la_comida) {
  CHECK(yardFoodReached(100, 100, 130, 100));
  CHECK(yardFoodReached(100, 100, 100 + YARD_FOOD_REACH, 100));
  CHECK(!yardFoodReached(100, 100, 100 + YARD_FOOD_REACH + 2, 100));
}

// el bicho ve la comida, va andando, se la come y solo entonces se aplica
TEST(yard, el_bicho_va_a_por_la_comida_y_se_la_come) {
  uint8_t walk[YBG_WALK_BYTES];
  testWalk(walk);
  YardWorld w;
  w.n = 1;
  w.p[0].on = true;
  w.p[0].x = (float)yardCellCenter(4);
  w.p[0].y = (float)yardCellCenter(10);
  YardFood f;
  uint32_t now = 1000;
  int cell = yardFoodCell(walk, w.p[0].x, w.p[0].y, 1, nullptr, nullptr, 0);
  CHECK(cell >= 0);
  yardFoodDrop(f, 2, cell, w.p[0].x, w.p[0].y, now, 77);
  CHECK_EQ((int)f.phase, (int)YF_FALL);
  CHECK_EQ((int)yardFoodHold(f), (int)YH_STAY);
  int eaten = 0, sawWalk = 0;
  uint32_t eatAt = 0;
  for (int k = 0; k < 400 && !eaten; k++) {
    now += 85;
    uint8_t hold[YARD_PETS_MAX] = { yardFoodHold(f), YH_NONE, YH_NONE };
    yardStep(w, walk, now, 85, hold, 0);
    if (f.phase == YF_WALK) sawWalk++;
    if (yardFoodStep(f, w, walk, 0, now)) eaten = k;
    if (f.phase == YF_EAT && !eatAt) {
      eatAt = now;
      CHECK(yardFoodReached(w.p[0].x, w.p[0].y, f.x, f.y));  // la alcanza antes de comer
    }
    CHECK(yardSpotOk(walk, w.p[0].x, w.p[0].y));
  }
  CHECK(eaten > 0);
  CHECK(sawWalk > 0);
  CHECK(eatAt > 0);
  CHECK(now - eatAt >= YARD_FOOD_EAT_MS);  // el bocado entero antes de aplicar la comida
  CHECK_EQ((int)f.phase, (int)YF_NONE);
  CHECK(w.p[0].face == (f.x >= w.p[0].x ? 1 : -1));  // mirandola
}

// sin sprite de andar (o sin celda) se la come donde esta, tras el "!"
TEST(yard, sin_andar_se_la_come_donde_esta) {
  uint8_t walk[YBG_WALK_BYTES];
  yardFallbackWalk(walk);
  YardWorld w;
  w.n = 1;
  w.p[0].on = true;
  w.p[0].walks = false;
  w.p[0].x = w.p[0].y = (float)yardCellCenter(7);
  YardFood f;
  yardFoodDrop(f, 3, -1, w.p[0].x, w.p[0].y, 0, 1);
  uint32_t now = 0;
  bool eaten = false;
  for (int k = 0; k < 60 && !eaten; k++) {
    now += 85;
    CHECK(f.phase != YF_WALK);
    eaten = yardFoodStep(f, w, walk, 0, now);
  }
  CHECK(eaten);
  CHECK(now >= YARD_FOOD_FALL_MS + YARD_FOOD_NOTICE_MS + YARD_FOOD_EAT_MS);
  CHECK(now < YARD_FOOD_FALL_MS + YARD_FOOD_NOTICE_MS + YARD_FOOD_EAT_MS + 400);
}

// otro bicho en medio para siempre: a los YARD_FOOD_WALK_MAX_MS se la come donde este (nunca se atasca)
TEST(yard, la_comida_no_espera_para_siempre) {
  uint8_t walk[YBG_WALK_BYTES];
  yardFallbackWalk(walk);
  YardWorld w;
  w.n = 2;
  w.p[0].on = w.p[1].on = true;
  w.p[0].x = (float)yardCellCenter(5);
  w.p[0].y = w.p[1].y = (float)yardCellCenter(7);
  w.p[1].x = (float)yardCellCenter(7);  // justo en medio, quieto
  w.p[1].mode = YM_STAY;
  YardFood f;
  yardFoodDrop(f, 0, yardCellIdx(9, 7), w.p[0].x, w.p[0].y, 0, 1);
  uint32_t now = 0;
  bool eaten = false;
  for (int k = 0; k < 400 && !eaten; k++) {
    now += 85;
    uint8_t hold[YARD_PETS_MAX] = { yardFoodHold(f), YH_STAY, YH_NONE };
    yardStep(w, walk, now, 85, hold, 0);
    eaten = yardFoodStep(f, w, walk, 0, now);
  }
  CHECK(eaten);
  CHECK(now <= YARD_FOOD_FALL_MS + YARD_FOOD_NOTICE_MS + YARD_FOOD_WALK_MAX_MS + YARD_FOOD_EAT_MS + 200);
}
