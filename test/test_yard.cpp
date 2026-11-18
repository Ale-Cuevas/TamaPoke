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

TEST(yard, sprite_de_andar_segun_la_direccion) {
  // mas vertical que horizontal: de espaldas subiendo, de frente bajando
  CHECK_EQ(yardWalkDir(3, -10, 1, true, true), YW_UP);
  CHECK_EQ(yardWalkDir(-3, 10, 1, true, true), YW_DOWN);
  // mas horizontal (o a 45 grados): de lado, hacia donde mira
  CHECK_EQ(yardWalkDir(10, -3, 1, true, true), YW_RIGHT);
  CHECK_EQ(yardWalkDir(-10, 3, -1, true, true), YW_LEFT);
  CHECK_EQ(yardWalkDir(5, 5, 1, true, true), YW_RIGHT);
  // sin moverse: de lado
  CHECK_EQ(yardWalkDir(0, 0, -1, true, true), YW_LEFT);
  // sprite viejo sin arriba/abajo: de lado segun el ultimo lado al que miraba
  CHECK_EQ(yardWalkDir(0, -10, -1, false, false), YW_LEFT);
  CHECK_EQ(yardWalkDir(0, 10, 1, false, false), YW_RIGHT);
  CHECK_EQ(yardWalkDir(0, -10, 1, false, true), YW_RIGHT);  // solo tiene de frente
  CHECK_EQ(yardWalkDir(0, 10, 1, false, true), YW_DOWN);
}

TEST(yard, paseo_vertical_mira_arriba_o_abajo) {
  uint8_t walk[YBG_WALK_BYTES];
  yardFallbackWalk(walk);
  YardWorld w;
  w.n = 1;
  YardPet &p = w.p[0];
  p.on = true;
  p.x = (float)yardCellCenter(7);
  p.y = (float)yardCellCenter(9);
  float dx, dy;
  yardHeading(p, &dx, &dy);
  CHECK(dx == 0 && dy == 0);  // sin camino
  CHECK(yardWalkTo(w, walk, 0, yardCellIdx(7, 4), 0));  // recto hacia arriba
  yardAdvance(w, walk, 0, 100, YARD_SPEED);
  yardHeading(p, &dx, &dy);
  CHECK_EQ(yardWalkDir(dx, dy, p.face, true, true), YW_UP);
  CHECK(yardWalkTo(w, walk, 0, yardCellIdx(7, 11), 0));  // y hacia abajo
  yardAdvance(w, walk, 0, 100, YARD_SPEED);
  yardHeading(p, &dx, &dy);
  CHECK_EQ(yardWalkDir(dx, dy, p.face, true, true), YW_DOWN);
}

// ---------------------------------------------------------------- la comida que cae (Y3)

// v3.3: la comida del menu del bicho, delante de el: empieza ya en el bocado (ni cae, ni "!", ni anda), a
// YARD_FOOD_FRONT_DX px hacia donde mira, y se aplica al acabar el bocado
TEST(yard, comida_delante_se_la_come_ya) {
  uint8_t walk[YBG_WALK_BYTES];
  yardFallbackWalk(walk);
  YardWorld w;
  w.n = 1;
  YardPet &p = w.p[0];
  p.on = true;
  p.x = p.y = (float)yardCellCenter(7);
  p.face = -1;
  p.mode = YM_WALK;
  p.pathN = 3;
  YardFood f;
  uint32_t now = 5000;
  yardFoodFront(f, p, walk, 4, now, 99);
  CHECK_EQ((int)f.phase, (int)YF_EAT);
  CHECK_EQ((int)f.kind, 4);
  CHECK_EQ(f.tag, (uint32_t)99);
  CHECK_EQ((int)f.x, (int)p.x - YARD_FOOD_FRONT_DX);  // mira a la izquierda: a su izquierda
  CHECK(f.y >= (int)p.y);                              // a sus pies (se dibuja delante)
  CHECK_EQ((int)p.face, -1);
  CHECK_EQ((int)p.mode, (int)YM_STAY);                 // deja de andar
  CHECK_EQ((int)p.pathN, 0);
  CHECK_EQ((int)yardFoodHold(f), (int)YH_STAY);
  float x0 = p.x, y0 = p.y;
  bool eaten = false;
  for (int k = 0; k < 40 && !eaten; k++) {
    now += 85;
    uint8_t hold[YARD_PETS_MAX] = { yardFoodHold(f), YH_NONE, YH_NONE };
    yardStep(w, walk, now, 85, hold, 0);
    CHECK(f.phase == YF_EAT || f.phase == YF_NONE);   // nunca espera ni anda
    eaten = yardFoodStep(f, w, 0, now);
  }
  CHECK(eaten);
  CHECK(now - 5000 >= YARD_FOOD_EAT_MS);
  CHECK(now - 5000 < YARD_FOOD_EAT_MS + 100);
  CHECK_EQ(p.x, x0);  // quieto todo el bocado
  CHECK_EQ(p.y, y0);
  CHECK_EQ((int)f.phase, (int)YF_NONE);
}

// delante no se pisa (un muro): al otro lado, y mirandola; sin sitio a ningun lado, a sus pies
TEST(yard, comida_delante_sobre_suelo_que_se_pisa) {
  uint8_t walk[YBG_WALK_BYTES];
  yardFallbackWalk(walk);
  block(walk, 8, 7);  // a la derecha del bicho
  YardPet p;
  p.on = true;
  p.x = (float)yardCellCenter(7) + 10;  // cerca del borde derecho de su celda: x + 28 cae en la 8
  p.y = (float)yardCellCenter(7);
  p.face = 1;
  YardFood f;
  yardFoodFront(f, p, walk, 0, 0, 1);
  CHECK(yardSpotOk(walk, f.x, p.y));
  CHECK_EQ((int)f.x, (int)(p.x - YARD_FOOD_FRONT_DX));
  CHECK_EQ((int)p.face, -1);  // se gira hacia ella
  block(walk, 6, 7);          // y a la izquierda tambien
  YardPet q = p;
  q.face = 1;
  yardFoodFront(f, q, walk, 0, 0, 1);
  CHECK_EQ((int)f.x, (int)q.x);
  CHECK_EQ((int)f.phase, (int)YF_EAT);
}

// v3.3: apartarse de debajo del menu de la derecha: la celda alcanzable mas cercana con el centro a la
// izquierda de maxX
TEST(yard, celda_a_la_izquierda_para_apartarse) {
  uint8_t walk[YBG_WALK_BYTES];
  testWalk(walk);
  float px = (float)yardCellCenter(11), py = (float)yardCellCenter(9);
  int c = yardCellLeftOf(walk, px, py, 220);
  CHECK(c >= 0);
  CHECK(yardFree(walk, c));
  CHECK(yardCellCenter(yardCellCX(c)) <= 220);
  // la mas cercana: ninguna otra alcanzable a la izquierda de 220 esta mas cerca
  float d = yardDist((float)yardCellCenter(yardCellCX(c)), (float)yardCellCenter(yardCellCY(c)), px, py);
  for (int k = 0; k < YARD_CELLS; k++)
    if (yardFree(walk, k) && yardCellCenter(yardCellCX(k)) <= 220)
      CHECK(yardDist((float)yardCellCenter(yardCellCX(k)), (float)yardCellCenter(yardCellCY(k)), px, py) >= d - 0.01f);
  // ya a la izquierda: su propia celda
  CHECK_EQ(yardCellLeftOf(walk, (float)yardCellCenter(4), (float)yardCellCenter(10), 220), yardCellIdx(4, 10));
  // un muro de arriba abajo: la otra mitad no se alcanza
  uint8_t wall[YBG_WALK_BYTES];
  yardFallbackWalk(wall);
  for (int y = 0; y < YBG_GRID; y++) block(wall, 9, y);
  CHECK_EQ(yardCellLeftOf(wall, (float)yardCellCenter(11), (float)yardCellCenter(7), 250), -1);
}

// v3.3: con el menu de un bicho abierto (a la derecha), los paseos al azar no acaban debajo de el
TEST(yard, paseos_al_azar_lejos_del_menu) {
  uint8_t walk[YBG_WALK_BYTES];
  yardFallbackWalk(walk);
  YardWorld w;
  w.n = 1;
  w.p[0].on = true;
  w.p[0].x = (float)yardCellCenter(4);
  w.p[0].y = (float)yardCellCenter(7);
  w.avoidX = 230;
  int walks = 0;
  for (int k = 0; k < 200; k++) {
    w.p[0].mode = YM_IDLE;
    if (!yardWander(w, walk, 0, (uint32_t)k * 1000)) continue;
    walks++;
    int to = w.p[0].path[w.p[0].pathN - 1];
    CHECK(yardCellCenter(yardCellCX(to)) <= 230);
  }
  CHECK(walks > 150);
  w.avoidX = 32767;  // sin menu, toda la mitad derecha vuelve a valer
  bool right = false;
  for (int k = 0; k < 200 && !right; k++)
    if (yardWander(w, walk, 0, (uint32_t)k * 1000)) right = yardCellCenter(yardCellCX(w.p[0].path[w.p[0].pathN - 1])) > 230;
  CHECK(right);
}

// v3.3: un bicho debajo del menu se aparta aunque este triste (la pose triste lo tendria quieto), llega a la
// izquierda de la columna y despues, con su menu, se queda quieto
TEST(yard, apartarse_del_menu_gana_a_la_pose_triste) {
  uint8_t walk[YBG_WALK_BYTES];
  yardFallbackWalk(walk);
  const int colX = 256, bw = 40;
  YardWorld w;
  w.n = 1;
  w.avoidX = colX - 32;
  w.p[0].on = true;
  w.p[0].x = (float)yardCellCenter(10);
  w.p[0].y = (float)yardCellCenter(7);
  CHECK(yardUnderMenu(w.p[0].x, bw, colX));
  YardAside a;
  uint32_t now = 1000;
  uint8_t h = yardMenuHold(w, walk, 0, YH_STAY, true, bw, colX, a, now);  // base: triste, quieto
  CHECK_EQ((int)h, (int)YH_NONE);
  CHECK_EQ((int)w.p[0].mode, (int)YM_WALK);
  for (int k = 0; k < 200 && yardUnderMenu(w.p[0].x, bw, colX); k++) {
    now += 85;
    uint8_t hold[YARD_PETS_MAX] = { yardMenuHold(w, walk, 0, YH_STAY, true, bw, colX, a, now), YH_NONE, YH_NONE };
    yardStep(w, walk, now, 85, hold, 0);
  }
  CHECK(!yardUnderMenu(w.p[0].x, bw, colX));
  now += 85;
  CHECK_EQ((int)yardMenuHold(w, walk, 0, YH_STAY, true, bw, colX, a, now), (int)YH_STAY);    // su menu: quieto
  CHECK_EQ((int)yardMenuHold(w, walk, 0, YH_NONE, false, bw, colX, a, now), (int)YH_NONE);   // el otro: lo suyo
  CHECK_EQ((int)yardMenuHold(w, walk, 0, YH_STAY, false, bw, colX, a, now), (int)YH_STAY);   // (triste: quieto)
}

// v3.3: si algo para el paso a un lado (un gesto, el bocado), lo vuelve a intentar: no se queda debajo
TEST(yard, apartarse_del_menu_se_reintenta) {
  uint8_t walk[YBG_WALK_BYTES];
  yardFallbackWalk(walk);
  const int colX = 256, bw = 40;
  YardWorld w;
  w.n = 1;
  w.p[0].on = true;
  w.p[0].x = (float)yardCellCenter(10);
  w.p[0].y = (float)yardCellCenter(7);
  YardAside a;
  uint32_t now = 1000;
  CHECK_EQ((int)yardMenuHold(w, walk, 0, YH_NONE, true, bw, colX, a, now), (int)YH_NONE);
  yardGesture(w, 0, 7, 600, now);  // la pose del mimo corta el paseo
  CHECK_EQ((int)yardMenuHold(w, walk, 0, YH_NONE, true, bw, colX, a, now + 100), (int)YH_NONE);  // la deja acabar
  w.p[0].mode = YM_STAY;  // y el juego lo paro (p. ej. el bocado)
  now += YARD_ASIDE_RETRY_MS;
  CHECK_EQ((int)yardMenuHold(w, walk, 0, YH_NONE, true, bw, colX, a, now), (int)YH_NONE);
  CHECK_EQ((int)w.p[0].mode, (int)YM_WALK);  // otra vez en marcha
  // sin sitio a la izquierda (o sin sprite de andar): quieto debajo, sin bucle
  w.p[0].walks = false;
  w.p[0].mode = YM_IDLE;
  CHECK_EQ((int)yardMenuHold(w, walk, 0, YH_NONE, true, bw, colX, a, now + 5000), (int)YH_STAY);
}

// v3.3: la comida elegida mientras se aparta espera (nada en el suelo, sin moverlo) y se sirve delante al llegar
TEST(yard, comida_elegida_al_apartarse_se_sirve_al_llegar) {
  uint8_t walk[YBG_WALK_BYTES];
  yardFallbackWalk(walk);
  const int colX = 256, bw = 40;
  YardWorld w;
  w.n = 1;
  w.p[0].on = true;
  w.p[0].x = (float)yardCellCenter(10);
  w.p[0].y = (float)yardCellCenter(7);
  YardAside a;
  YardFood f;
  uint32_t now = 1000;
  CHECK_EQ((int)yardMenuHold(w, walk, 0, YH_NONE, true, bw, colX, a, now), (int)YH_NONE);
  yardFoodQueue(f, 3, now, 42);
  CHECK_EQ((int)f.phase, (int)YF_QUEUED);
  CHECK_EQ((int)yardFoodHold(f), (int)YH_NONE);  // no lo para
  CHECK(!yardFoodStep(f, w, 0, now));             // ni se come nada aun
  bool served = false;
  for (int k = 0; k < 200 && !served; k++) {
    now += 85;
    uint8_t hold[YARD_PETS_MAX] = { yardMenuHold(w, walk, 0, YH_NONE, true, bw, colX, a, now), YH_NONE, YH_NONE };
    yardStep(w, walk, now, 85, hold, 0);
    bool under = yardUnderMenu(w.p[0].x, bw, colX);
    served = yardFoodServe(f, w.p[0], walk, yardFoodReady(f, w.p[0], under, now), now);
    if (!served) CHECK_EQ((int)f.phase, (int)YF_QUEUED);
  }
  CHECK(served);
  CHECK(!yardUnderMenu(w.p[0].x, bw, colX));  // servida ya fuera de la columna
  CHECK_EQ((int)f.phase, (int)YF_EAT);
  CHECK_EQ((int)f.kind, 3);
  CHECK_EQ(f.tag, (uint32_t)42);
  CHECK(abs(f.x - (int)w.p[0].x) == YARD_FOOD_FRONT_DX || f.x == (int)w.p[0].x);
  // sin poder apartarse, no espera para siempre
  YardFood g;
  yardFoodQueue(g, 0, 0, 1);
  YardPet p;
  p.mode = YM_WALK;
  CHECK(!yardFoodReady(g, p, true, YARD_FOOD_QUEUE_MAX_MS - 1));
  CHECK(yardFoodReady(g, p, true, YARD_FOOD_QUEUE_MAX_MS));
  p.mode = YM_IDLE;
  p.walks = false;
  CHECK(yardFoodReady(g, p, true, 10));
}

// v3.3: con el menu abierto, inclinar la placa tampoco lleva a nadie debajo de el
TEST(yard, inclinar_no_lleva_debajo_del_menu) {
  uint8_t walk[YBG_WALK_BYTES];
  yardFallbackWalk(walk);
  int from = yardCellIdx(4, 7);
  int free = yardTiltTargetSide(walk, from, 1);
  CHECK(yardCellCenter(yardCellCX(free)) > 300);  // sin menu: hasta el borde derecho
  int to = yardTiltTargetSide(walk, from, 1, 224);
  CHECK(to >= 0);
  CHECK(yardCellCenter(yardCellCX(to)) <= 224);
  CHECK(yardCellCX(to) > yardCellCX(from));  // pero hacia la derecha, lo que se pueda
  CHECK_EQ(yardTiltTargetSide(walk, from, -1, 224), yardTiltTargetSide(walk, from, -1));  // a la izquierda, igual
  // yardStep usa avoidX: un bicho inclinado a la derecha se para antes de la columna
  YardWorld w;
  w.n = 1;
  w.avoidX = 224;
  w.p[0].on = true;
  w.p[0].x = (float)yardCellCenter(4);
  w.p[0].y = (float)yardCellCenter(7);
  uint32_t now = 0;
  for (int k = 0; k < 200; k++) {
    now += 85;
    yardStep(w, walk, now, 85, nullptr, 1);
  }
  CHECK(w.p[0].x <= 224 + 1);
}
