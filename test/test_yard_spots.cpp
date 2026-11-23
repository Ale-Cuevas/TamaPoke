// Los muebles del patio (yard_spots.h, v3.20 FR-HOME.5): cada sitio de la tabla cae en una celda por la que se
// pasea, tiene al lado una celda libre (para llegar a el o salir de la cama) y, con cualquier combinacion de
// muebles comprados, la zona por la que se pasea sigue siendo una sola (yardKeepLargestRegion solo corre al
// cargar el mapa) y queda sitio para los dos bichos. Sobre el suelo liso de respaldo y sobre los mapas de
// verdad (tools/yard/out/NN-name.bin, que no estan en git: si faltan, esa parte se salta con un aviso).
#include "framework.h"
#include "../yard_spots.h"
#include "../yard.h"
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

// diseno: sitio de sobra para los dos bichos (y las cacas, los regalos y el huevo) con todo comprado
#define SPOTS_MIN_FREE 30

static int regionsOf(const uint8_t *walk) {
  bool seen[YARD_CELLS] = {};
  int n = 0;
  for (int c = 0; c < YARD_CELLS; c++) {
    if (seen[c] || !yardFree(walk, c)) continue;
    n++;
    bool r[YARD_CELLS];
    yardReachable(walk, c, r);
    for (int k = 0; k < YARD_CELLS; k++) seen[k] |= r[k];
  }
  return n;
}

static bool hasFreeNeighbour(const uint8_t *walk, int cx, int cy) {
  static const int D[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
  for (auto &d : D)
    if (!yardCellBlocked(walk, cx + d[0], cy + d[1])) return true;
  return false;
}

// todo lo de arriba para el mapa `map` con su mascara ya preparada como en el firmware (marco + zona grande)
static void checkSpots(uint8_t map, const uint8_t *base, const std::string &what) {
  CHECK_MSG(regionsOf(base) == 1, what + ": la mascara de partida ya esta partida");
  for (uint8_t k = 0; k < YOBJ_N; k++) {
    int cx, cy;
    yardObjCell(map, k, &cx, &cy);
    CHECK_MSG(cx >= 1 && cx <= 13 && cy >= 1 && cy <= 13, what + ": celda fuera del lienzo");
    CHECK_MSG(!yardCellBlocked(base, cx, cy), what + ": mueble " + std::to_string(k) + " en una celda bloqueada (" +
                                                  std::to_string(cx) + "," + std::to_string(cy) + ")");
  }
  for (int combo = 1; combo < 8; combo++) {
    bool bed = combo & 1, gym = combo & 2, toy = combo & 4;
    uint8_t w[YBG_WALK_BYTES];
    memcpy(w, base, sizeof(w));
    yardBlockObjects(w, map, bed, gym, toy);
    std::string tag = what + " combo " + std::to_string(combo);
    CHECK_MSG(regionsOf(w) == 1, tag + ": los muebles parten la zona");
    CHECK_MSG(yardFreeCount(w) >= SPOTS_MIN_FREE, tag + ": poco sitio (" + std::to_string(yardFreeCount(w)) + ")");
    for (uint8_t k = 0; k < YOBJ_N; k++) {
      bool on = k <= YOBJ_BED1 ? bed : k == YOBJ_GYM ? gym : toy;
      if (!on) continue;
      int cx, cy;
      yardObjCell(map, k, &cx, &cy);
      CHECK_MSG(yardCellBlocked(w, cx, cy), tag + ": el mueble no bloquea su celda");
      CHECK_MSG(hasFreeNeighbour(w, cx, cy), tag + ": mueble " + std::to_string(k) + " sin celda libre al lado");
    }
  }
}

TEST(yard_spots, suelo_liso_de_respaldo) {
  uint8_t w[YBG_WALK_BYTES];
  yardFallbackWalk(w);
  yardKeepLargestRegion(w);
  checkSpots(0, w, "respaldo");
}

TEST(yard_spots, camas_juntas_y_a_la_distancia_de_dos_bichos) {
  for (uint8_t m = 0; m <= YARD_MAPS; m++) {
    int x0, y0, x1, y1;
    yardObjFoot(m, YOBJ_BED0, &x0, &y0);
    yardObjFoot(m, YOBJ_BED1, &x1, &y1);
    CHECK_EQ(y0, y1);
    CHECK(x1 > x0);
    // los pies de cada dormido siguen dentro de la celda de su cama (la que el juego le deja libre)
    CHECK_EQ(yardCellAt((float)x0, (float)y0), yardCellIdx(yardSpotsOf(m).bedX, yardSpotsOf(m).bedY));
    CHECK_EQ(yardCellAt((float)x1, (float)y1), yardCellIdx(yardSpotsOf(m).bedX + 1, yardSpotsOf(m).bedY));
    CHECK(x1 - x0 >= YARD_MIN_DIST);  // tumbados no se pisan
  }
  // un mapa fuera de la tabla usa el del suelo liso
  CHECK_EQ(&yardSpotsOf(YARD_MAPS + 1), &YARD_SPOTS[0]);
}

// la carpeta con los .bin de verdad: TAMAPOKE_YARD_OUT, o tools/yard/out del checkout
static std::string yardOutDir() {
  const char *e = getenv("TAMAPOKE_YARD_OUT");
  return e && *e ? e : "../tools/yard/out";
}

TEST(yard_spots, mapas_de_verdad_con_cada_combinacion) {
  std::string dir = yardOutDir();
  DIR *d = opendir(dir.c_str());
  if (!d) {
    printf("  (aviso: sin %s, los mapas de verdad no se comprueban)\n", dir.c_str());
    return;
  }
  std::vector<std::string> names;
  while (dirent *e = readdir(d)) {
    std::string n = e->d_name;
    if (n.size() > 7 && isdigit((unsigned char)n[0]) && isdigit((unsigned char)n[1]) && n[2] == '-' &&
        n.compare(n.size() - 4, 4, ".bin") == 0)
      names.push_back(n);
  }
  closedir(d);
  if (names.empty()) printf("  (aviso: %s no tiene mapas NN-name.bin)\n", dir.c_str());
  for (const std::string &n : names) {
    int map = atoi(n.substr(0, 2).c_str());
    CHECK_MSG(map >= 1 && map <= YARD_MAPS, n + ": numero de mapa fuera de YARD_MAPS");
    if (map < 1 || map > YARD_MAPS) continue;
    FILE *f = fopen((dir + "/" + n).c_str(), "rb");
    CHECK(f != nullptr);
    if (!f) continue;
    std::vector<uint8_t> buf(YBG_FILE_MAX + 1);
    size_t len = fread(buf.data(), 1, buf.size(), f);
    fclose(f);
    YardBgView v;
    CHECK_MSG(yardBgParse(buf.data(), len, &v) == YBG_OK, n + ": no es un TPY1 valido");
    uint8_t w[YBG_WALK_BYTES];
    memcpy(w, v.walk, sizeof(w));
    yardMaskFrame(w);  // como yardTryMap
    yardKeepLargestRegion(w);
    checkSpots((uint8_t)map, w, n);
  }
}

// v3.20 (revision): las cosas (cacas, regalos, huevo, encuentro) no caen en las celdas que tapan los sprites de
// los muebles (64 px sobre celdas de 38): encima del gimnasio y del juguete, encima de las dos camas y a los
// lados de la pareja; y sobre cada mapa de verdad siguen quedando celdas para ellas
TEST(yard_spots, las_cosas_evitan_las_celdas_tapadas_por_los_muebles) {
  for (uint8_t m = 0; m <= YARD_MAPS; m++) {
    const YardSpots &s = yardSpotsOf(m);
    uint8_t w[YBG_WALK_BYTES];
    memset(w, 0, sizeof(w));
    yardBlockAboveObjects(w, m, false, false, false);
    bool any = false;
    for (int i = 0; i < YBG_WALK_BYTES; i++) any |= w[i] != 0;
    CHECK(!any);
    yardBlockAboveObjects(w, m, true, true, true);
    CHECK(yardCellBlocked(w, s.bedX, s.bedY - 1));
    CHECK(yardCellBlocked(w, s.bedX + 1, s.bedY - 1));
    CHECK(yardCellBlocked(w, s.bedX - 1, s.bedY));
    CHECK(yardCellBlocked(w, s.bedX + 2, s.bedY));
    CHECK(yardCellBlocked(w, s.gymX, s.gymY - 1));
    CHECK(yardCellBlocked(w, s.toyX, s.toyY - 1));
  }
  // al borde de la rejilla no se sale de ella
  uint8_t w[YBG_WALK_BYTES];
  memset(w, 0, sizeof(w));
  yardBlockCell(w, -1, 3);
  yardBlockCell(w, 3, YBG_GRID);
  for (int i = 0; i < YBG_WALK_BYTES; i++) CHECK_EQ(w[i], (uint8_t)0);
}
