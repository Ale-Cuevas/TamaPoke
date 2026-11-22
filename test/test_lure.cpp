// El Cebo (v3.14, Pet::useLure): un objeto de la mochila que hace salir ya un salvaje al azar. No gasta las tiradas
// del dia, se niega (sin gastarse) sin hora, sin nadie que pueda pelear, con otro salvaje en el patio o con una
// despedida en casa; sale del jefe y de los viajes (no se vende) y se guarda en "bpk3" sin tocar "bpack" ni "bpk2".
#include "framework.h"
#include "shim/Arduino.h"
#include "shim/Preferences.h"
#include "../pet.h"
#include "../encounter.h"
#include "../shop.h"

static const uint32_t LDAY = 20000;  // un dia cualquiera (numero de dia unix)
static uint32_t lat(uint32_t day, int h, int m = 0) { return day * 86400 + h * 3600 + m * 60; }

// bicho recien nacido (Charmander), con reloj a las 8:00 (antes de la tirada base: 09:00-20:00) y la NVS limpia
static void lBorn(Pet &p) {
  mockClearForcedRandom();
  mockNvsReset();
  mockSetMillis(0);
  mockSerialSilence(true);
  p.begin();
  if (p.awaitingStarter()) p.chooseStarter(4);
  p.eggTap();
  p.eggTap();
  p.eggTap();
  p.setClock(lat(LDAY, 8));
}

// ------------------------------------------------------------------ usarlo
TEST(lure, sale_un_salvaje_ya_y_se_gasta_uno) {
  Pet p;
  lBorn(p);
  p.items[IT_LURE] = 2;
  CHECK(!p.encShown());
  CHECK_EQ(p.lureBlock(), (uint8_t)LURE_OK);
  CHECK(p.useLure());
  CHECK(p.encActive());
  CHECK(p.encDex() >= 1 && p.encDex() <= DEX_COUNT);
  CHECK_EQ(p.items[IT_LURE], (uint8_t)1);
  CHECK_EQ(p.encMinutesLeft(), (uint16_t)ENC_LIFETIME_MIN);  // y espera en el patio lo de siempre
  CHECK(!p.encSoundPending());  // la UI abre su pantalla ya (con su sonido): el patio no vuelve a sonar
  // guardado ya (es un toque): el salvaje y el Cebo gastado
  Pet b;
  b.begin();
  b.syncClock(lat(LDAY, 8, 30));
  CHECK(b.encActive());
  CHECK_EQ(b.encDex(), p.encDex());
  CHECK_EQ(b.items[IT_LURE], (uint8_t)1);
}

TEST(lure, no_gasta_las_tiradas_del_dia) {
  Pet p;
  lBorn(p);
  p.items[IT_LURE] = 1;
  CHECK(p.useLure());
  CHECK_EQ(p.encRolledMask() & (ENC_ROLL_BASE | ENC_ROLL_SPAWNED), 0);
  p.encounterDone(false, p.uid);  // se escapa
  CHECK(!p.encShown());
  // la tirada base del dia sigue ahi: a las 21:00 ya ha pasado su hora, y con todo a favor sale
  for (uint32_t v = 1; v < 0x10000; v++)
    if (uint32_t rng = (v << 16) | v; encHit(ENC_BASE_PERMIL, rng)) {
      mockForceRandom((long)v);
      break;
    }
  p.lastSeenEpoch = lat(LDAY, 21);
  p.encCheck();
  mockClearForcedRandom();
  CHECK(p.encActive());
  CHECK(p.encRolledMask() & ENC_ROLL_SPAWNED);
}

TEST(lure, dos_salvajes_con_el_mismo_reloj_tienen_su_propia_medalla) {
  Pet p;
  lBorn(p);
  p.items[IT_LURE] = 2;
  CHECK(p.useLure());
  uint32_t a = p.encSpawnEpoch();
  p.encounterDone(true, p.uid);
  p.releaseWild();
  CHECK(p.useLure());  // el reloj no se ha movido
  CHECK(p.encSpawnEpoch() > a);
  p.encounterDone(true, p.uid);
  CHECK_EQ(p.medalValue(MD_WILD), (uint32_t)2);
}

TEST(lure, funciona_con_el_de_casa_dormido) {
  // v3.12: un bicho dormido puede pelear, asi que el Cebo tambien vale de noche
  Pet p;
  lBorn(p);
  p.items[IT_LURE] = 1;
  p.sleeping = true;
  CHECK(p.home(0).canBattle());
  CHECK(p.useLure());
  CHECK(p.encActive());
}

// ------------------------------------------------------------------ cuando no se puede (y no se gasta)
TEST(lure, sin_cebos_o_con_otro_salvaje_no_se_usa) {
  Pet p;
  lBorn(p);
  CHECK_EQ(p.lureBlock(), (uint8_t)LURE_NONE);
  CHECK(!p.useLure());
  p.items[IT_LURE] = 3;
  CHECK(p.encSpawnNow(16));  // uno en el patio
  int16_t dex = p.encDex();
  CHECK_EQ(p.lureBlock(), (uint8_t)LURE_ENC);
  CHECK(!p.useLure());
  CHECK_EQ(p.items[IT_LURE], (uint8_t)3);
  CHECK_EQ(p.encDex(), dex);  // el de antes sigue ahi
  // vencido con la Poke Ball pendiente: tambien ocupa el patio
  p.encounterDone(true, p.uid);
  CHECK(p.encWonPending());
  CHECK_EQ(p.lureBlock(), (uint8_t)LURE_ENC);
  CHECK(!p.useLure());
  CHECK_EQ(p.items[IT_LURE], (uint8_t)3);
  p.releaseWild();
  CHECK_EQ(p.lureBlock(), (uint8_t)LURE_OK);
}

TEST(lure, uno_que_ya_se_fue_no_estorba) {
  Pet p;
  lBorn(p);
  p.items[IT_LURE] = 1;
  p.lastSeenEpoch = lat(LDAY, 9);
  CHECK(p.encSpawnNow(16));
  p.lastSeenEpoch = lat(LDAY, 9) + ENC_LIFETIME_MIN * 60;  // se fue (aun sin pasar por encCheck)
  CHECK(!p.encShown());
  CHECK_EQ(p.lureBlock(), (uint8_t)LURE_OK);
  CHECK(p.useLure());
  CHECK(p.encActive());
  CHECK_EQ(p.encSpawnEpoch(), p.lastSeenEpoch);
}

TEST(lure, sin_hora_no_se_usa) {
  Pet p;
  lBorn(p);
  p.items[IT_LURE] = 1;
  p.lastSeenEpoch = 0;
  CHECK_EQ(p.lureBlock(), (uint8_t)LURE_NO_CLOCK);
  CHECK(!p.useLure());
  CHECK_EQ(p.items[IT_LURE], (uint8_t)1);
  CHECK(!p.encShown());
}

TEST(lure, con_una_despedida_o_sin_nadie_que_pelee_no_se_usa) {
  Pet p;
  lBorn(p);
  p.items[IT_LURE] = 1;
  p.release();  // la ceremonia de soltarlo
  CHECK_EQ(p.lureBlock(), (uint8_t)LURE_BUSY);
  CHECK(!p.useLure());
  mockAdvanceMillis(CEREMONY_MS + 1);
  p.update(millis());
  p.setBagEggs(0);
  CHECK(p.isVacant());  // casa vacia y caja vacia: nadie
  CHECK_EQ(p.lureBlock(), (uint8_t)LURE_NO_FIGHTER);
  CHECK(!p.useLure());
  CHECK_EQ(p.items[IT_LURE], (uint8_t)1);
  CHECK(!p.encShown());
}

// ------------------------------------------------------------------ de donde sale y que no es
TEST(lure, no_se_vende_ni_se_usa_en_un_bicho) {
  CHECK_EQ(priceOfItem(IT_LURE), (uint16_t)0);
  for (int k = 0; k < SHOP_ITEMS; k++) CHECK(SHOP_ORDER[k] != IT_LURE);
  CHECK_EQ(shopCheckItem(IT_LURE, 9999, 0, itemCap(IT_LURE)), SB_NOT_SOLD);
  CHECK_EQ(itemCap(IT_LURE), (uint8_t)ITEM_MAX);
  CHECK(!itemIsColorBerry(IT_LURE));
  Pet p;
  lBorn(p);
  p.items[IT_LURE] = 1;
  CHECK(!p.canUseItem(IT_LURE));
  CHECK(!p.useItem(IT_LURE));
  CHECK_EQ(p.items[IT_LURE], (uint8_t)1);
  CHECK(!p.buyItem(IT_LURE));
}

TEST(lure, el_del_jefe_llega_envuelto_y_se_abre_a_la_mochila) {
  Pet p;
  lBorn(p);
  CHECK(p.addGift(IT_LURE));
  CHECK_EQ(p.openGift(), (uint8_t)IT_LURE);
  CHECK_EQ(p.items[IT_LURE], (uint8_t)1);
  // con la mochila llena se queda envuelto
  p.items[IT_LURE] = ITEM_MAX;
  CHECK(p.addGift(IT_LURE));
  bool full = false;
  CHECK_EQ(p.openGift(&full), (uint8_t)IT_LURE);
  CHECK(full);
  CHECK_EQ(p.giftCount(), (uint8_t)1);
  CHECK_EQ(p.items[IT_LURE], (uint8_t)ITEM_MAX);
}

TEST(lure, sale_de_los_viajes) {
  // con su peso de TRV_POOL_W (tuning.h): los viajes de 8 h siempre traen objeto
  int lure = 0, w = -1;
  for (int i = 0; i < TRV_POOL_N; i++)
    if (TRV_POOL_ID[i] == IT_LURE) w = TRV_POOL_W[i];
  CHECK(w > 0);
  const int N = 20000;
  for (int i = 0; i < N; i++) lure += travelRoll((uint32_t)i * 2654435761u + 9, 1 + i % TRV_DESTS, 2, 0).item == IT_LURE;
  CHECK_RANGE(lure * 100.0 / N, w - 2.0, w + 2.0);
}

// ------------------------------------------------------------------ guardado ("bpk3")
TEST(lure, se_guarda_en_bpk3_sin_alargar_bpack_ni_bpk2) {
  Pet p;
  lBorn(p);
  p.items[IT_BALL] = 4;
  p.items[IT_BERRY_G] = 12;
  p.items[IT_LURE] = 5;
  p.rename("X");  // guarda
  {
    Preferences raw;
    raw.begin("tamapoke");
    uint8_t buf[32];
    // un firmware anterior lee "bpack" en un buffer de 10 y "bpk2" en uno de 6: no pueden crecer
    CHECK_EQ(raw.getBytes("bpack", buf, sizeof(buf)), (size_t)PACK_OLD);
    CHECK_EQ(raw.getBytes("bpk2", buf, sizeof(buf)), (size_t)6);
    CHECK_EQ(buf[IT_BERRY_G - IT_BERRY_R], (uint8_t)12);
    CHECK_EQ(raw.getBytes("bpk3", buf, sizeof(buf)), (size_t)PACK3_LEN);
    CHECK_EQ(buf[IT_LURE - PACK3_FIRST], (uint8_t)5);
  }
  Pet q;
  q.begin();
  CHECK_EQ(q.items[IT_LURE], (uint8_t)5);
  CHECK_EQ(q.items[IT_BALL], (uint8_t)4);
  CHECK_EQ(q.items[IT_BERRY_G], (uint8_t)12);
}

TEST(lure, un_guardado_sin_bpk3_carga_0_cebos_y_conserva_las_bayas) {
  Pet p;
  lBorn(p);
  p.items[IT_GOLDEN] = 3;
  p.items[IT_BERRY_O] = 20;
  p.items[IT_LURE] = 2;
  p.rename("X");
  {
    Preferences raw;
    raw.begin("tamapoke");
    raw.remove("bpk3");  // como lo dejaba la v3.13
  }
  Pet q;
  q.begin();
  CHECK_EQ(q.items[IT_LURE], (uint8_t)0);
  CHECK_EQ(q.items[IT_GOLDEN], (uint8_t)3);
  CHECK_EQ(q.items[IT_BERRY_O], (uint8_t)20);
  q.rename("Y");  // al guardar de nuevo aparece "bpk3", y "bpack" / "bpk2" siguen igual de largos
  Preferences raw;
  raw.begin("tamapoke");
  uint8_t buf[32];
  CHECK_EQ(raw.getBytes("bpack", buf, sizeof(buf)), (size_t)PACK_OLD);
  CHECK_EQ(raw.getBytes("bpk2", buf, sizeof(buf)), (size_t)6);
  CHECK_EQ(raw.getBytes("bpk3", buf, sizeof(buf)), (size_t)PACK3_LEN);
}

TEST(lure, bpk3_se_recorta_al_tope_y_guarda_los_huecos_de_un_firmware_mas_nuevo) {
  Pet p;
  lBorn(p);
  p.rename("X");
  {
    Preferences raw;
    raw.begin("tamapoke");
    uint8_t b3[PACK3_LEN] = { 0 };
    b3[IT_LURE - PACK3_FIRST] = 200;  // fuera del tope
    b3[PACK3_LEN - 1] = 7;            // un objeto que este firmware no conoce
    raw.putBytes("bpk3", b3, sizeof(b3));
  }
  Pet q;
  q.begin();
  CHECK_EQ(q.items[IT_LURE], (uint8_t)ITEM_MAX);
  q.rename("Y");
  Preferences raw;
  raw.begin("tamapoke");
  uint8_t b3[PACK3_LEN] = { 0 };
  CHECK_EQ(raw.getBytes("bpk3", b3, sizeof(b3)), (size_t)PACK3_LEN);
  CHECK_EQ(b3[IT_LURE - PACK3_FIRST], (uint8_t)ITEM_MAX);
  CHECK_EQ(b3[PACK3_LEN - 1], (uint8_t)7);  // volver a una version anterior y luego a la nueva no lo pierde
}

TEST(lure, un_corte_de_luz_nunca_gasta_el_cebo_sin_salvaje) {
  // useLure escribe "enc" antes que la mochila: se corta la luz tras cada escritura posible
  bool sawBoth = false, sawNone = false, sawExtra = false;
  for (long n = 0; n < 200; n++) {
    Pet p;
    lBorn(p);
    p.items[IT_LURE] = 2;
    p.rename("X");
    long before = mockNvsWrites();
    mockNvsCutAfter(n);
    CHECK(p.useLure());
    bool all = mockNvsWrites() - before < n;  // llegaron todas: ya no hay mas cortes que probar
    mockNvsCutAfter(-1);
    Pet q;
    q.begin();
    q.syncClock(lat(LDAY, 8, 5));
    if (q.encActive()) {
      CHECK(q.items[IT_LURE] == 1 || q.items[IT_LURE] == 2);  // gastado, o uno de mas
      if (q.items[IT_LURE] == 1) sawBoth = true;
      else sawExtra = true;
    } else {
      CHECK_EQ(q.items[IT_LURE], (uint8_t)2);  // sin salvaje, el Cebo sigue ahi
      sawNone = true;
    }
    if (all) break;
  }
  CHECK(sawBoth && sawNone && sawExtra);
}
