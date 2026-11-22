// La comida (v3.2, food.h): la baya normal gratis, las seis bayas de colores, la baya favorita de cada linea
// (por su forma base), lo que da cada comida (tuning.h) y que el objeto se gasta solo al aplicarse.
#include "framework.h"
#include "shim/Arduino.h"
#include "shim/Preferences.h"
#include "../pet.h"
#include "../food.h"
#include "../shop.h"

// las tablas de lo que se encuentra / suelta suman 100 (tambien lo comprueba tuning.h al compilar)
static_assert(ODDS_GOLDEN + ODDS_CAKE + ODDS_VITAMIN + ODDS_CHARM + ODDS_REVIVE + ODDS_COLORED == 100, "hallazgos");
static_assert(BDROP_CAKE + BDROP_GOLDEN + BDROP_VITAMIN + BDROP_RARECANDY + BDROP_CHARM + BDROP_REVIVE + BDROP_EGG +
                      BDROP_BALL + BDROP_COLORED + BDROP_LURE == 100, "botin");
static_assert(WDROP_CAKE + WDROP_GOLDEN + WDROP_VITAMIN + WDROP_RARECANDY + WDROP_REVIVE + WDROP_CHARM + WDROP_COLORED ==
                100, "salvajes");

static void fBorn(Pet &p, int16_t dex = 4) {
  mockClearForcedRandom();
  mockNvsReset();
  mockSetMillis(1000);
  p.begin();
  if (p.awaitingStarter()) p.chooseStarter(dex);
  p.eggTap();
  p.eggTap();
  p.eggTap();
  p.speciesId = dex;
}

// ---------------------------------------------------------------- la tabla de comidas
TEST(food, los_objetos_de_las_bayas_van_al_final_de_la_mochila) {
  // la mochila se guarda por posicion ("bpack"): los ids de antes no se mueven
  CHECK_EQ((int)IT_BALL, 9);
  CHECK_EQ((int)IT_BERRY_R, 10);
  CHECK_EQ((int)IT_BERRY_O, 15);
  CHECK_EQ((int)IT_LURE, 16);  // v3.14: el Cebo, detras de las bayas
  CHECK_EQ((int)IT_COUNT, 17);
  for (uint8_t c = 0; c < BERRY_COLORS; c++) {
    CHECK_EQ(foodItem(FOOD_BERRY0 + c), (uint8_t)(IT_BERRY_R + c));
    CHECK_EQ(foodOfItem(IT_BERRY_R + c), (uint8_t)(FOOD_BERRY0 + c));
  }
  CHECK_EQ(foodItem(FOOD_PLAIN), (uint8_t)FOOD_NO_ITEM);
  CHECK_EQ(foodItem(FOOD_CAKE), (uint8_t)IT_CAKE);
  CHECK_EQ(foodItem(FOOD_GOLDEN), (uint8_t)IT_GOLDEN);
  CHECK_EQ(foodOfItem(IT_PROTEIN), (uint8_t)FOOD_COUNT);
  CHECK_EQ(foodOfItem(IT_BALL), (uint8_t)FOOD_COUNT);
}

TEST(food, la_favorita_sale_de_la_forma_base_y_no_cambia_al_evolucionar) {
  bool seen[BERRY_COLORS] = { false };
  for (int d = 1; d <= DEX_COUNT; d++) {
    uint8_t f = favBerryOf(d);
    CHECK(f < BERRY_COLORS);
    CHECK_EQ(f, favBerryOf(DEX_TBL[d].base));  // toda la linea, la misma
    if (DEX_TBL[d].evolvesTo) CHECK_EQ(f, favBerryOf(DEX_TBL[d].evolvesTo));
    seen[f] = true;
  }
  for (int c = 0; c < BERRY_COLORS; c++) CHECK(seen[c]);  // los seis colores le tocan a alguna linea
  CHECK_EQ(favBerryOf(0), favBerryOf(0));  // fuera de la dex no revienta
  CHECK(favBerryOf(999) < BERRY_COLORS);
}

TEST(food, lo_que_da_cada_comida_sale_de_tuning) {
  FoodEffect p = foodEffect(FOOD_PLAIN, false);
  CHECK_EQ((int)p.fullness, PLAIN_BERRY_FOOD);
  CHECK_EQ((int)p.joy, 0);
  FoodEffect c = foodEffect(FOOD_BERRY0, false);
  CHECK_EQ((int)c.fullness, BERRY_FOOD);
  CHECK_EQ((int)c.joy, BERRY_JOY);
  CHECK_EQ((int)c.bond, 0);
  FoodEffect f = foodEffect(FOOD_BERRY0 + 3, true);
  CHECK_EQ((int)f.fullness, FAV_BERRY_FOOD);
  CHECK_EQ((int)f.joy, FAV_BERRY_JOY);
  CHECK_EQ((int)f.bond, FAV_BERRY_BOND);
  CHECK(f.heart && !c.heart && !p.heart);
  FoodEffect g = foodEffect(FOOD_GOLDEN, false);
  CHECK_EQ((int)g.fullness, GOLDEN_FOOD);
  CHECK_EQ((int)g.joy, GOLDEN_JOY);
  FoodEffect k = foodEffect(FOOD_CAKE, false);
  CHECK_EQ((int)k.joy, CAKE_JOY);
  // cualquier baya de color es mejor que la normal; la favorita, mejor que la de color
  CHECK(c.fullness > p.fullness && c.joy > p.joy);
  CHECK(f.fullness > c.fullness && f.joy > c.joy);
}

// ---------------------------------------------------------------- dar de comer
TEST(food, la_baya_normal_es_gratis_y_sin_limite) {
  Pet p;
  fBorn(p);
  uint8_t before[IT_COUNT];
  memcpy(before, p.items, sizeof(before));
  for (int i = 0; i < 20; i++) {
    p.fullness = 10;
    CHECK(p.canFeed(FOOD_PLAIN));
    CHECK(p.feedFood(FOOD_PLAIN));
    CHECK_EQ((int)p.fullness, 10 + PLAIN_BERRY_FOOD);
  }
  CHECK(memcmp(before, p.items, sizeof(before)) == 0);  // la mochila, intacta
}

TEST(food, una_baya_de_color_se_gasta_una_vez_al_comerla) {
  Pet p;
  fBorn(p);
  uint8_t fav = favBerryOf(p.speciesId), other = (fav + 1) % BERRY_COLORS;
  p.items[IT_BERRY_R + other] = 2;
  p.fullness = 20;
  p.joy = 20;
  CHECK(p.feedFood(FOOD_BERRY0 + other));
  CHECK_EQ(p.items[IT_BERRY_R + other], (uint8_t)1);  // una, no dos
  CHECK_EQ((int)p.fullness, 20 + BERRY_FOOD);
  CHECK_EQ((int)p.joy, 20 + BERRY_JOY);
  CHECK(!p.berryKnown);  // una que no es la favorita no la revela
  CHECK(p.feedFood(FOOD_BERRY0 + other));
  CHECK_EQ(p.items[IT_BERRY_R + other], (uint8_t)0);
  uint8_t full = p.fullness;
  CHECK(!p.canFeed(FOOD_BERRY0 + other));
  CHECK(!p.feedFood(FOOD_BERRY0 + other));  // ya no hay: nada se gasta ni se da
  CHECK_EQ(p.items[IT_BERRY_R + other], (uint8_t)0);
  CHECK_EQ(p.fullness, full);
}

TEST(food, si_no_puede_comer_no_se_gasta_nada) {
  Pet p;
  fBorn(p);
  for (uint8_t i = IT_BERRY_R; i <= IT_BERRY_O; i++) p.items[i] = 3;
  p.items[IT_CAKE] = 3;
  p.items[IT_GOLDEN] = 3;
  p.fullness = 30;
  p.toggleLight();  // dormido
  CHECK(p.sleeping);
  for (uint8_t f = FOOD_PLAIN; f < FOOD_COUNT; f++) {
    CHECK(!p.canFeed(f));
    CHECK(!p.feedFood(f));
  }
  for (uint8_t i = IT_BERRY_R; i <= IT_BERRY_O; i++) CHECK_EQ(p.items[i], (uint8_t)3);
  CHECK_EQ(p.items[IT_CAKE], (uint8_t)3);
  CHECK_EQ(p.items[IT_GOLDEN], (uint8_t)3);
  CHECK_EQ(p.fullness, (uint8_t)30);
  // y la comida del patio que cae mientras duerme: tampoco (Pet::feedFood es lo unico que gasta)
  Pet e;
  mockNvsReset();
  e.begin();
  e.chooseStarter(4);  // sigue en el huevo
  e.items[IT_BERRY_R] = 2;
  CHECK(!e.feedFood(FOOD_BERRY0));
  CHECK_EQ(e.items[IT_BERRY_R], (uint8_t)2);
}

TEST(food, la_favorita_se_descubre_y_da_mas) {
  Pet p;
  fBorn(p);
  uint8_t fav = favBerryOf(p.speciesId);
  p.items[IT_BERRY_R + fav] = 1;
  p.fullness = 10;
  p.joy = 10;
  uint8_t bond = p.bond;
  CHECK(!p.berryKnown);
  CHECK(p.feedFood(FOOD_BERRY0 + fav));
  CHECK_EQ((int)p.fullness, 10 + FAV_BERRY_FOOD);
  CHECK_EQ((int)p.joy, 10 + FAV_BERRY_JOY);
  CHECK(p.bond >= bond + 1 || p.bond == bond + FAV_BERRY_BOND || p.bond > bond);  // el tope diario del vinculo manda
  CHECK(p.berryKnown);
  CHECK(p.showHeart());
}

TEST(food, la_tarta_y_la_dorada_siguen_igual) {
  Pet p;
  fBorn(p);
  p.items[IT_CAKE] = 1;
  p.items[IT_GOLDEN] = 1;
  p.fullness = 10;
  p.joy = 10;
  p.weight = 20;
  CHECK(p.feedFood(FOOD_GOLDEN));
  CHECK_EQ((int)p.fullness, 10 + GOLDEN_FOOD);
  CHECK_EQ((int)p.joy, 10 + GOLDEN_JOY);
  CHECK_EQ(p.items[IT_GOLDEN], (uint8_t)0);
  CHECK(!p.berryKnown);  // la dorada no revela favorita
  CHECK(p.feedFood(FOOD_CAKE));
  CHECK_EQ(p.fullness, (uint8_t)100);
  CHECK(p.sated());
  CHECK_EQ((int)p.weight, 20 + CAKE_WEIGHT);
  CHECK_EQ(p.items[IT_CAKE], (uint8_t)0);
  CHECK(!p.eatCake());  // sin tartas
  // y desde la mochila (useItem) llega por el mismo camino, tambien con una baya de color
  p.items[IT_BERRY_G] = 1;
  p.fullness = 10;
  CHECK(p.useItem(IT_BERRY_G));
  CHECK_EQ(p.items[IT_BERRY_G], (uint8_t)0);
  CHECK(p.fullness > 10);
  CHECK(!p.useItem(IT_BERRY_G));
}

// ---------------------------------------------------------------- guardado
TEST(food, un_guardado_con_la_mochila_de_10_carga_con_bayas_a_cero) {
  Pet p;
  fBorn(p);
  p.items[IT_GOLDEN] = 4;
  p.items[IT_BALL] = 5;
  p.items[IT_BERRY_B] = 7;
  p.rename("X");  // guarda (la mochila va en el mismo save)
  {
    Preferences raw;
    raw.begin("tamapoke");
    uint8_t buf[IT_COUNT];
    // "bpack" sigue midiendo 10 (un firmware anterior lo lee en un buffer de 10), las bayas en "bpk2"
    CHECK_EQ(raw.getBytes("bpack", buf, sizeof(buf)), (size_t)10);
    CHECK_EQ(raw.getBytes("bpk2", buf, sizeof(buf)), (size_t)6);
    CHECK_EQ(buf[IT_BERRY_B - IT_BERRY_R], (uint8_t)7);
    raw.remove("bpk2");  // como lo dejaba la v3.1
  }
  Pet q;
  q.begin();
  CHECK_EQ(q.items[IT_GOLDEN], (uint8_t)4);
  CHECK_EQ(q.items[IT_BALL], (uint8_t)5);
  for (uint8_t i = IT_BERRY_R; i <= IT_BERRY_O; i++) CHECK_EQ(q.items[i], (uint8_t)0);
  q.rename("Y");  // y al guardar de nuevo, "bpack" sigue en 10
  Preferences raw;
  raw.begin("tamapoke");
  uint8_t buf[IT_COUNT];
  CHECK_EQ(raw.getBytes("bpack", buf, sizeof(buf)), (size_t)10);
}

TEST(food, las_bayas_sobreviven_a_un_reinicio) {
  Pet p;
  fBorn(p);
  p.items[IT_BERRY_Y] = 3;
  p.items[IT_BERRY_O] = 9;
  p.rename("X");
  Pet q;
  q.begin();
  CHECK_EQ(q.items[IT_BERRY_Y], (uint8_t)3);
  CHECK_EQ(q.items[IT_BERRY_O], (uint8_t)9);
}

// un resultado de viaje guardado con un id de objeto desconocido se descarta, no indexa fuera de la mochila
TEST(food, un_resultado_de_viaje_con_un_objeto_desconocido_se_descarta) {
  Pet p;
  fBorn(p);
  p.setClock(20000u * 86400 + 10 * 3600);
  TravelResult r = travelRoll(5, 1, 2, 0);
  r.valid = 1;
  r.item = IT_COUNT + 3;
  p.rename("X");
  {
    Preferences raw;
    raw.begin("tamapoke");
    raw.putBytes("trvR", &r, sizeof(r));
  }
  Pet q;
  q.begin();
  TravelResult out;
  if (q.takeTravelResult(&out)) CHECK_EQ(out.item, (uint8_t)TRV_NONE);
}

TEST(food, el_berryKnown_viejo_se_ignora) {
  Pet p;
  fBorn(p);
  p.flushSave();
  {
    Preferences raw;  // la v3.1 guardaba "bk": el gusto de entonces (% 3) ya no vale
    raw.begin("tamapoke");
    raw.putBool("bk", true);
    raw.remove("fk");
  }
  Pet q;
  q.begin();
  CHECK(!q.berryKnown);
  // el registro de la caja / hueco 2 con el REC_BERRY de antes: tambien se ignora
  PetLife l = p;
  l.berryKnown = true;
  PetRecord r = l.record(0);
  CHECK((r.flags & REC_FAV6) != 0);
  CHECK((r.flags & REC_BERRY) == 0);
  r.flags = (uint8_t)((r.flags & ~REC_FAV6) | REC_BERRY);
  recordSeal(r);
  Pet z;
  fBorn(z);
  z.fromRecord(r);
  CHECK(!z.berryKnown);
  r.flags = (uint8_t)((r.flags & ~REC_BERRY) | REC_FAV6);
  recordSeal(r);
  z.fromRecord(r);
  CHECK(z.berryKnown);
}

TEST(food, la_favorita_descubierta_se_guarda) {
  Pet p;
  fBorn(p);
  uint8_t fav = favBerryOf(p.speciesId);
  p.items[IT_BERRY_R + fav] = 1;
  CHECK(p.feedFood(FOOD_BERRY0 + fav));
  p.flushSave();
  Pet q;
  q.begin();
  CHECK(q.berryKnown);
}

// ---------------------------------------------------------------- dos bichos de casa
TEST(food, el_hueco_2_come_con_su_favorita_y_la_descubre_solo_el) {
  Pet p;
  fBorn(p, 4);
  // dos de casa: B en el 0 y A en el 1 con favoritas distintas
  int16_t dexA = 4, dexB = 0;
  for (int16_t d = 1; d <= 151 && !dexB; d++)
    if (favBerryOf(d) != favBerryOf(dexA) && DEX_TBL[d].evolvesTo) dexB = d;
  CHECK(dexB != 0);
  p.items[IT_EGG] = ITEM_MAX;
  p.setClock(20000u * 86400 + 10 * 3600);
  CHECK(p.store(0, 0));  // A a la caja; en el 0, un huevo de la mochila
  p.eggTap();
  p.eggTap();
  p.eggTap();
  p.speciesId = dexB;
  CHECK(p.wake(0, 1));  // A al hueco 1
  CHECK(p.home(1).hatched());
  CHECK_EQ(p.home(1).speciesId, dexA);
  uint8_t favA = favBerryOf(dexA), favB = favBerryOf(dexB);
  p.items[IT_BERRY_R + favA] = 2;
  p.items[IT_BERRY_R + favB] = 2;
  p.slotRef(1).fullness = 10;
  p.slotRef(1).joy = 10;
  bool ok = false;
  p.onSlot(1, [&] { ok = p.feedFood(FOOD_BERRY0 + favA); });  // la de A, comida por A
  CHECK(ok);
  CHECK(p.home(1).berryKnown);
  CHECK(!p.home(0).berryKnown);
  CHECK_EQ((int)p.home(1).fullness, 10 + FAV_BERRY_FOOD);
  CHECK_EQ((int)p.home(1).joy, 10 + FAV_BERRY_JOY);
  p.onSlot(1, [&] { ok = p.feedFood(FOOD_BERRY0 + favB); });  // la de B, comida por A: solo una de color
  CHECK(ok);
  CHECK_EQ((int)p.home(1).fullness, 10 + FAV_BERRY_FOOD + BERRY_FOOD);
  CHECK(!p.home(0).berryKnown);
  CHECK_EQ(p.items[IT_BERRY_R + favA], (uint8_t)1);
  CHECK_EQ(p.items[IT_BERRY_R + favB], (uint8_t)1);
  // y el descubrimiento del hueco 2 sobrevive a un reinicio (en "hm1", con REC_FAV6)
  p.flushSave();
  Pet q;
  q.begin();
  CHECK(q.home(1).berryKnown);
  CHECK(!q.home(0).berryKnown);
}
