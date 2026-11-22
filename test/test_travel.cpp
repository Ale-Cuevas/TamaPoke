// Los viajes y la coleccion del patio (v2.0, Y5 parte A; travel.h + Pet): el sorteo desde la semilla, las
// reglas de cada duracion, salir (y quien no puede), volver con la placa apagada, el crecimiento en el hueco,
// monedas / objeto / recuerdo, el resultado sin ver tras un reinicio, un guardado de antes y los sucesos.
#include "framework.h"
#include "shim/Arduino.h"
#include "shim/Preferences.h"
#include "../pet.h"

static const uint32_t TDAY = 20000;
static const uint32_t TT = TDAY * 86400 + 10 * 3600;

// un bicho nacido (el de casa) con la hora puesta y la caja con un bicho en el hueco 0 y otro en el 2
static void tBorn(Pet &p) {
  mockClearForcedRandom();
  mockNvsReset();
  mockSetMillis(1000);
  p.begin();
  if (p.awaitingStarter()) p.chooseStarter(4);
  p.eggTap();
  p.eggTap();
  p.eggTap();
  // v3.4: el de casa ya conto todo para las medallas del jugador (crecer apagado no paga monedas: estos tests
  // miden las del viaje)
  p.counted = PMB_ALL;
  p.setClock(TT);
  p.growMin = 100;
  PetRecord r = p.toRecord();
  r.speciesId = 25;
  r.growMin = 400;
  p.boxPut(0, r);
  r.speciesId = 133;
  r.growMin = 900;
  p.boxPut(2, r);
}

// la placa "se apaga" y vuelve a las hora now: un Pet nuevo sobre la misma NVS
static void tReboot(Pet &p, uint32_t now) {
  p.begin();
  p.syncClock(now);
}

// ---------------------------------------------------------------- logica pura
TEST(travel, tablas_coherentes) {
  CHECK_EQ(TRV_DESTS, YARD_MAPS);
  CHECK_EQ(SOUV_COUNT, 45);  // v3.15: 9 mapas x 5
  CHECK(SOUV_COUNT <= 64);   // caben en la mascara de 64 bits ("souv2")
  CHECK_EQ(SOUV_ALL, (1ull << SOUV_COUNT) - 1);
  CHECK_EQ(souvBit(44), 1ull << 44);
  CHECK_EQ(souvBit(SOUV_COUNT), 0ull);  // fuera de la coleccion: ningun bit
  int sum = 0;
  for (int i = 0; i < TRV_POOL_N; i++) sum += TRV_POOL_W[i];
  CHECK_EQ(sum, 100);
  for (int t = 0; t < TRV_TIERS; t++) CHECK_EQ(TRV_RARITY_W[t][0] + TRV_RARITY_W[t][1] + TRV_RARITY_W[t][2], 100);
  // promesa de diseno (v3.11, a proposito): un viaje paga unas 50 monedas por hora de viaje
  for (int t = 0; t < TRV_TIERS; t++) {
    int perHour = TRV_COINS[t] * 60 / TRV_MIN[t];
    CHECK(perHour >= 45 && perHour <= 55);
  }
  CHECK_EQ(TRV_GROW[2], (uint16_t)240);
  for (uint8_t id = 0; id < SOUV_COUNT; id++) {
    CHECK_EQ(souvenirDest(id), (uint8_t)(id / 5 + 1));
    CHECK_EQ(souvenirRarity(id), id % 5 <= 1 ? (uint8_t)SOUV_COMMON : id % 5 <= 3 ? (uint8_t)SOUV_UNCOMMON : (uint8_t)SOUV_RARE);
  }
  CHECK(sizeof(TravelResult) <= 64);
}

TEST(travel, el_sorteo_es_determinista) {
  for (uint32_t s = 1; s < 200; s++)
    for (uint8_t d = 1; d <= TRV_DESTS; d++)
      for (uint8_t t = 0; t < 3; t++) {
        TravelResult a = travelRoll(s * 7919u, d, t, 0), b = travelRoll(s * 7919u, d, t, 0);
        CHECK(memcmp(&a, &b, sizeof(a)) == 0);
        CHECK_EQ(a.coins, TRV_COINS[t]);
        CHECK_EQ(a.growMin, TRV_GROW[t]);
      }
  int diff = 0;  // y la semilla cambia el resultado
  TravelResult base = travelRoll(1, 3, 1, 0);
  for (uint32_t s = 2; s < 60; s++) {
    TravelResult r = travelRoll(s, 3, 1, 0);
    diff += r.item != base.item || r.souvenir != base.souvenir;
  }
  CHECK(diff > 20);
}

TEST(travel, sin_raros_en_1h_y_el_resto_de_reglas) {
  const int N = 20000;
  int rare[3] = { 0, 0, 0 }, souv[3] = { 0, 0, 0 }, item[3] = { 0, 0, 0 };
  int itemId[IT_COUNT] = { 0 };
  for (uint8_t t = 0; t < 3; t++)
    for (int i = 0; i < N; i++) {
      uint8_t d = (uint8_t)(1 + i % TRV_DESTS);
      TravelResult r = travelRoll((uint32_t)i * 2654435761u + t, d, t, 0);
      if (r.souvenir != TRV_NONE) {
        souv[t]++;
        CHECK_EQ(souvenirDest(r.souvenir), d);  // del mapa al que fue
        rare[t] += souvenirRarity(r.souvenir) == SOUV_RARE;
        CHECK(!r.dup);
      }
      if (r.item != TRV_NONE) {
        item[t]++;
        if (t == 1) itemId[r.item]++;
      }
    }
  CHECK_EQ(rare[0], 0);  // 1 h: ni un raro
  CHECK(rare[1] > 0);
  CHECK(rare[2] > rare[1]);  // 8 h: mas raros que 3 h
  // las probabilidades, con margen
  CHECK_RANGE(item[0] * 100.0 / N, 23.0, 27.0);
  CHECK_RANGE(item[1] * 100.0 / N, 48.0, 52.0);
  CHECK_EQ(item[2], N);
  CHECK_RANGE(souv[0] * 100.0 / N, 38.0, 42.0);
  CHECK_RANGE(souv[1] * 100.0 / N, 73.0, 77.0);
  CHECK_EQ(souv[2], N);
  CHECK_RANGE(rare[1] * 100.0 / souv[1], 6.0, 10.0);   // 8 % de los recuerdos de 3 h
  CHECK_RANGE(rare[2] * 100.0 / souv[2], 22.0, 28.0);  // 25 % de los de 8 h
  // el reparto de objetos de 3 h: el peso de cada uno sale de TRV_POOL_W (tuning.h; la baya de color, repartida en 6)
  for (int i = 0; i < TRV_POOL_N; i++)
    if (TRV_POOL_ID[i] != TRV_POOL_BERRY)
      CHECK_RANGE(itemId[TRV_POOL_ID[i]] * 100.0 / item[1], TRV_POOL_W[i] - 3.0, TRV_POOL_W[i] + 3.0);
  int colored = 0, berryW = -1;
  for (int i = 0; i < TRV_POOL_N; i++)
    if (TRV_POOL_ID[i] == TRV_POOL_BERRY) berryW = TRV_POOL_W[i];
  CHECK(berryW >= 0);
  for (int c = 0; c < TRV_BERRY_COLORS; c++) {
    colored += itemId[IT_BERRY_R + c];
    CHECK(itemId[IT_BERRY_R + c] > 0);  // los seis colores salen
  }
  CHECK_RANGE(colored * 100.0 / item[1], berryW - 3.0, berryW + 3.0);
  CHECK(itemId[IT_LURE] > 0);  // v3.14: el Cebo tambien sale de los viajes
  CHECK_EQ(itemId[IT_CHARM] + itemId[IT_EGG], 0);
}

// v3.2: la baya de color que trae un viaje sale de la semilla: siempre una de las seis, y la misma semilla, la misma
TEST(travel, la_baya_de_color_sale_de_la_semilla) {
  bool seen[TRV_BERRY_COLORS] = { false };
  for (uint32_t i = 0; i < 20000; i++) {
    uint32_t seed = i * 2654435761u + 5;
    TravelResult r = travelRoll(seed, 1 + i % TRV_DESTS, 2, 0);  // 8 h: siempre trae objeto
    CHECK(r.item != TRV_NONE);
    CHECK(r.item < IT_COUNT);
    if (r.item >= IT_BERRY_R && r.item <= IT_BERRY_O) seen[r.item - IT_BERRY_R] = true;
    TravelResult again = travelRoll(seed, 1 + i % TRV_DESTS, 2, 0);
    CHECK_EQ(again.item, r.item);
  }
  for (int c = 0; c < TRV_BERRY_COLORS; c++) CHECK(seen[c]);
}

TEST(travel, prefiere_los_que_no_tiene_y_el_repetido_da_monedas) {
  // tiene el id 0 (comun del prado): la segunda tirada lo esquiva casi siempre
  int souv = 0, dup = 0;
  for (int i = 0; i < 20000; i++) {
    TravelResult r = travelRoll((uint32_t)i * 40503u + 7, 1, 0, 1u);
    if (r.souvenir == TRV_NONE) continue;
    souv++;
    if (r.dup) {
      dup++;
      CHECK_EQ(r.souvenir, (uint8_t)0);
      CHECK_EQ(r.dupCoins, (uint16_t)TRV_DUP_COINS);
      CHECK_EQ(r.coins, (uint16_t)(TRV_COINS[0] + TRV_DUP_COINS));
    } else {
      CHECK_EQ(r.dupCoins, (uint16_t)0);
      CHECK_EQ(r.coins, TRV_COINS[0]);
    }
  }
  // sin segunda tirada seria ~35 % (comun 70 % x 1/2); con ella ~12 %
  CHECK_RANGE(dup * 100.0 / souv, 8.0, 17.0);
  // todo el mapa ya conseguido: cada recuerdo es repetido -> +10 monedas
  uint32_t all = 0x1Fu;  // los 5 del prado
  for (int i = 0; i < 500; i++) {
    TravelResult r = travelRoll((uint32_t)i * 977u, 1, 2, all);
    CHECK(r.souvenir != TRV_NONE);  // 8 h: siempre trae
    CHECK(r.dup);
    CHECK_EQ(r.coins, (uint16_t)(TRV_COINS[2] + TRV_DUP_COINS));
  }
  // los de otro mapa no cuentan como repetidos
  TravelResult r = travelRoll(5, 2, 2, 0x1Fu);
  CHECK(!r.dup);
}

TEST(travel, el_tiempo_que_falta) {
  const uint32_t S = 1700000000;
  CHECK_EQ(travelTimeLeft(S, 0, S), (uint32_t)3600);
  CHECK_EQ(travelTimeLeft(S, 0, S + 600), (uint32_t)3000);
  CHECK_EQ(travelTimeLeft(S, 0, S + 3600), (uint32_t)0);
  CHECK_EQ(travelTimeLeft(S, 1, S + 3600), (uint32_t)(10800 - 3600));
  CHECK_EQ(travelTimeLeft(S, 2, S + 86400), (uint32_t)0);
  CHECK_EQ(travelTimeLeft(S, 2, S - 100), (uint32_t)28800);  // el reloj fue hacia atras: no termina de golpe
  CHECK_EQ(travelTimeLeft(S, 0, 0), (uint32_t)3600);         // sin hora ahora
  CHECK_EQ(travelTimeLeft(0, 0, S), (uint32_t)3600);         // sin hora al salir
  CHECK_EQ(travelElapsedMin(S, 1, S + 90 * 60), (uint16_t)90);
  CHECK_EQ(travelElapsedMin(S, 1, S + 99999), (uint16_t)180);
}

TEST(travel, los_sucesos) {
  CHECK_EQ(travelEventCount(0), (uint8_t)1);
  CHECK_EQ(travelEventCount(1), (uint8_t)2);
  CHECK_EQ(travelEventCount(2), (uint8_t)3);
  for (uint32_t s = 1; s < 400; s++)
    for (uint8_t d = 1; d <= TRV_DESTS; d++)
      for (uint8_t t = 0; t < 3; t++) {
        uint16_t prev = 0;
        uint8_t used[3] = { 255, 255, 255 };
        for (uint8_t i = 0; i < travelEventCount(t); i++) {
          uint16_t off, off2;
          uint8_t tpl, arg, tpl2, arg2;
          travelEvent(s * 31u, d, t, i, &off, &tpl, &arg);
          travelEvent(s * 31u, d, t, i, &off2, &tpl2, &arg2);
          CHECK(off == off2 && tpl == tpl2 && arg == arg2);  // determinista
          CHECK(off > 0 && off < TRV_MIN[t]);                 // dentro del viaje
          CHECK(i == 0 || off > prev);                        // en orden
          prev = off;
          CHECK(tpl < TRV_TEMPLATES);
          CHECK(travelTplFits(tpl, d));  // vale para el mapa
          for (uint8_t j = 0; j < i; j++) CHECK(used[j] != tpl);  // sin repetir
          used[i] = tpl;
          if (travelTplHasArg(tpl)) {
            CHECK(arg >= 1 && arg <= 151);
            bool inBiome = false;
            for (int k = 0; k < 5; k++) inBiome |= TRV_SPECIES[d - 1][k] == arg;
            CHECK(inBiome);
          } else {
            CHECK_EQ(arg, (uint8_t)0);
          }
        }
      }
  // cada mapa tiene plantillas de sobra para 3 sucesos distintos y las de un solo mapa solo salen en el suyo
  for (uint8_t d = 1; d <= TRV_DESTS; d++) {
    int fit = 0;
    for (uint8_t k = 0; k < TRV_TEMPLATES; k++) fit += travelTplFits(k, d);
    CHECK(fit >= 3);
  }
  CHECK(!travelTplFits(3, 1));  // "chapoteo": solo la playa
  CHECK(travelTplFits(3, 4));
  // v3.15: la mascara es de 16 bits; el mapa 9 (bit 8) no se confunde con TRV_ARG (bit 15)
  CHECK(travelTplFits(16, 9) && !travelTplFits(16, 1) && !travelTplHasArg(16));
  CHECK(travelTplFits(13, 6) && travelTplFits(14, 7) && travelTplFits(15, 8));
  CHECK(travelTplFits(0, 9) && travelTplHasArg(0));
  CHECK(!travelTplFits(0, TRV_DESTS + 1));
  // los mapas 1..5 valen para las mismas plantillas que antes de v3.15 (un viaje en marcha no cambia de registro)
  static const uint16_t OLD_MASK[13] = { 0x1F, 0x05, 0x1F, 0x08, 0x02, 0x14, 0x01, 0x1A, 0x1F, 0x1F, 0x08, 0x12, 0x1F };
  for (uint8_t k = 0; k < TRV_TEMPLATES; k++)
    for (uint8_t d = 1; d <= 5; d++) CHECK_EQ(travelTplFits(k, d), k < 13 && ((OLD_MASK[k] >> (d - 1)) & 1));
}

// ---------------------------------------------------------------- salir
TEST(travel, quien_puede_salir) {
  Pet p;
  tBorn(p);
  CHECK(p.canTravel(0, 1, 0));
  CHECK(!p.canTravel(1, 1, 0));   // hueco vacio
  CHECK(!p.canTravel(6, 1, 0));   // no existe
  CHECK(!p.canTravel(0, 2, 0));   // la cantera no es suya
  CHECK(!p.canTravel(0, 0, 0));
  CHECK(!p.canTravel(0, 6, 0));
  CHECK(!p.canTravel(0, 1, 3));   // duracion que no existe
  CHECK(!p.startTravel(1, 1, 0));
  CHECK(!p.travelling());
  // el de casa nunca: su "hueco" no existe en la caja
  CHECK(!p.canTravel(TEAM_HOME, 1, 0));
  p.setCoins(500);
  CHECK(p.buyMap(2));
  CHECK(p.canTravel(0, 2, 1));
  // sin hora (RTC sin poner) no se sale: no se sabria cuando vuelve
  p.lastSeenEpoch = 0;
  CHECK(!p.canTravel(0, 1, 0));
  p.lastSeenEpoch = TT;
  CHECK(p.startTravel(0, 2, 1));
  CHECK(p.travelling());
  CHECK(p.slotTravelling(0));
  CHECK(!p.slotTravelling(2));
  CHECK_EQ(p.travelSlot(), (uint8_t)0);
  CHECK_EQ(p.travelDest(), (uint8_t)2);
  CHECK_EQ(p.travelTier(), (uint8_t)1);
  CHECK_EQ(p.travelStartEpoch(), TT);
  CHECK_EQ(p.travelSecondsLeft(), (uint32_t)10800);
  // uno a la vez para toda la partida
  CHECK(!p.canTravel(2, 1, 0));
  CHECK(!p.startTravel(2, 1, 0));
  CHECK_EQ(p.travelSlot(), (uint8_t)0);
}

TEST(travel, mientras_esta_fuera) {
  Pet p;
  tBorn(p);
  uint32_t uid0 = p.boxRecord(0).uid, uid2 = p.boxRecord(2).uid;
  CHECK(p.canWake(0));
  CHECK(p.teamCanFight(uid0));
  CHECK(p.startTravel(0, 1, 0));
  CHECK(!p.canWake(0));
  CHECK(!p.wake(0));
  CHECK(!p.canStore(0));
  CHECK(!p.teamCanFight(uid0));
  CHECK(p.canWake(2));  // el otro, normal
  CHECK(p.teamCanFight(uid2));
  CHECK_EQ(p.boxState(0), (uint8_t)BOX_OK);
  // BOXCLR lo fuerza y cancela el viaje (sin premio)
  uint16_t c = p.coins;
  CHECK(p.boxClear(0));
  CHECK(!p.travelling());
  CHECK_EQ(p.boxState(0), (uint8_t)BOX_EMPTY);
  CHECK_EQ(p.coins, c);
  CHECK(!p.travelResultPending());
  // y ya no hay nada pendiente tras un reinicio
  Pet q;
  tReboot(q, TT + 200 * 60);
  CHECK(!q.travelling());
  CHECK(!q.travelResultPending());
}

TEST(travel, el_viaje_sobrevive_a_un_reinicio_sin_terminar) {
  Pet p;
  tBorn(p);
  CHECK(p.startTravel(2, 1, 1));
  uint32_t seed = p.travelSeed();
  Pet q;
  tReboot(q, TT + 100 * 60);  // 100 min de 180
  CHECK(q.travelling());
  CHECK_EQ(q.travelSlot(), (uint8_t)2);
  CHECK_EQ(q.travelSeed(), seed);
  CHECK_EQ(q.travelStartEpoch(), TT);
  CHECK_EQ(q.travelSecondsLeft(), (uint32_t)80 * 60);
  CHECK_EQ(q.travelMinutesElapsed(), (uint16_t)100);
  CHECK(!q.canWake(2));
  CHECK(!q.travelResultPending());
}

// ---------------------------------------------------------------- volver
TEST(travel, vuelve_con_la_placa_apagada) {
  Pet p;
  tBorn(p);
  p.setCoins(100);
  CHECK(p.startTravel(0, 1, 2));  // 8 h
  uint32_t seed = p.travelSeed();
  uint16_t grow0 = p.boxRecord(0).growMin;
  uint32_t uid0 = p.boxRecord(0).uid;
  TravelResult want = travelRoll(seed, 1, 2, 0);
  Pet q;
  tReboot(q, TT + 3 * 86400);  // 3 dias despues
  CHECK(!q.travelling());
  CHECK(q.canWake(0));
  // el crecimiento esta en el registro del hueco, sellado, y sigue siendo el mismo bicho
  CHECK_EQ(q.boxState(0), (uint8_t)BOX_OK);
  CHECK_EQ(q.boxRecord(0).growMin, (uint32_t)(grow0 + 240));
  CHECK_EQ(q.boxRecord(0).uid, uid0);
  CHECK_EQ(q.boxRecord(0).speciesId, (int16_t)25);
  CHECK(recordValid(q.boxRecord(0)));
  Pet r;  // y esta en la NVS, no solo en la RAM
  r.begin();
  CHECK_EQ(r.boxRecord(0).growMin, (uint32_t)(grow0 + 240));
  // las monedas fuera del tope diario
  CHECK_EQ(q.coins, (uint16_t)(100 + want.coins));
  CHECK_EQ(q.coinsEarnedToday(), (uint16_t)0);
  // el recuerdo (8 h: siempre) con su fecha, y el objeto (8 h: siempre)
  CHECK(want.souvenir != TRV_NONE && want.item != TRV_NONE);
  CHECK(q.hasSouvenir(want.souvenir));
  CHECK_EQ(q.souvenirCount(), (uint8_t)1);
  CHECK_EQ(q.souvenirFoundEpoch(want.souvenir), TT + 3 * 86400);
  CHECK_EQ(q.items[want.item], (uint8_t)((want.item == IT_REVIVE ? REVIVE_START : want.item == IT_BALL ? BALL_START : 0) + 1));
  // el aviso sin ver
  TravelResult res;
  CHECK(q.travelResultPending());
  CHECK(q.takeTravelResult(&res));
  CHECK_EQ(res.dest, (uint8_t)1);
  CHECK_EQ(res.tier, (uint8_t)2);
  CHECK_EQ(res.coins, want.coins);
  CHECK_EQ(res.growMin, (uint16_t)240);
  CHECK_EQ(res.item, want.item);
  CHECK_EQ(res.souvenir, want.souvenir);
  CHECK_EQ(res.speciesId, (int16_t)25);
  CHECK_EQ(res.lvBefore, levelFor(grow0));
  CHECK_EQ(res.lvAfter, levelFor(grow0 + 240));
  CHECK(res.lvAfter > res.lvBefore);
  CHECK(!q.takeTravelResult(&res));
}

TEST(travel, vuelve_en_vivo_con_el_tick) {
  Pet p;
  tBorn(p);
  CHECK(p.startTravel(0, 1, 0));  // 1 h
  uint32_t now = TT;
  for (int m = 0; m < 59; m++) {
    now += 60;
    p.lastSeenEpoch = now;
    mockAdvanceMillis(PET_TICK_MS);
    p.update(millis());
  }
  CHECK(p.travelling());  // 59 min: aun no
  CHECK(!p.travelResultPending());
  now += 60;
  p.lastSeenEpoch = now;
  mockAdvanceMillis(PET_TICK_MS);
  p.update(millis());
  CHECK(!p.travelling());
  CHECK(p.travelResultPending());
  CHECK(p.canWake(0));
  TravelResult res;
  CHECK(p.takeTravelResult(&res));
  CHECK_EQ(res.coins, (uint16_t)(TRV_COINS[0] + res.dupCoins));
}

TEST(travel, la_hora_hacia_atras_o_sin_hora_no_termina) {
  Pet p;
  tBorn(p);
  CHECK(p.startTravel(0, 1, 0));
  p.lastSeenEpoch = TT - 7200;  // el reloj fue hacia atras
  p.travelCheck();
  CHECK(p.travelling());
  p.lastSeenEpoch = 0;
  p.travelCheck();
  CHECK(p.travelling());
  p.lastSeenEpoch = TT + 3600;
  p.travelCheck();
  CHECK(!p.travelling());
}

TEST(travel, objeto_con_la_mochila_llena_se_pierde) {
  // se busca un viaje que traiga un objeto concreto: la semilla sale de random() al salir
  for (long guess = 1; guess < 400; guess++) {
    Pet p;
    tBorn(p);
    mockForceRandom(guess);
    CHECK(p.startTravel(0, 1, 2));
    mockClearForcedRandom();
    TravelResult want = travelRoll(p.travelSeed(), 1, 2, 0);
    if (want.item != IT_CAKE) continue;
    p.items[IT_CAKE] = ITEM_MAX;
    p.travelFinishNow();
    TravelResult res;
    CHECK(p.takeTravelResult(&res));
    CHECK(res.itemLost);
    CHECK_EQ(res.item, (uint8_t)IT_CAKE);
    CHECK_EQ(p.items[IT_CAKE], (uint8_t)ITEM_MAX);
    CHECK_EQ(p.coins, (uint16_t)want.coins);  // lo demas se cobra igual
    return;
  }
  CHECK_MSG(false, "ninguna semilla probada traia una tarta");
}

TEST(travel, un_recuerdo_repetido_da_monedas) {
  Pet p;
  tBorn(p);
  p.setSouvenirMask(0x1Fu);  // todo el prado
  p.coins = 0;               // v3.4: setSouvenirMask guarda y paga el Bronce de Coleccionista (5 recuerdos): aqui
                             // solo cuentan las monedas del viaje
  CHECK(p.startTravel(0, 1, 2));
  p.travelFinishNow();
  TravelResult res;
  CHECK(p.takeTravelResult(&res));
  CHECK(res.dup);
  CHECK_EQ(res.dupCoins, (uint16_t)TRV_DUP_COINS);
  CHECK_EQ(res.coins, (uint16_t)(TRV_COINS[2] + TRV_DUP_COINS));
  CHECK_EQ(p.coins, res.coins);
  CHECK_EQ(p.souvenirCount(), (uint8_t)5);  // no se cuenta dos veces
  // y la fecha del que ya tenia no cambia: setSouvenirMask los fecho en TT
  CHECK_EQ(p.souvenirFoundEpoch(res.souvenir), TT);
}

TEST(travel, las_monedas_respetan_el_tope_de_la_cartera) {
  Pet p;
  tBorn(p);
  p.setCoins(COIN_MAX - 5);
  CHECK(p.startTravel(0, 1, 0));
  p.travelFinishNow();
  TravelResult res;
  CHECK(p.takeTravelResult(&res));
  CHECK_EQ(p.coins, (uint16_t)COIN_MAX);
  CHECK_EQ(res.coins, (uint16_t)5);  // lo que de verdad entro
}

// ---------------------------------------------------------------- el aviso y los guardados
TEST(travel, el_resultado_sin_ver_sobrevive_a_un_reinicio) {
  Pet p;
  tBorn(p);
  CHECK(p.startTravel(0, 1, 1));
  TravelResult want = travelRoll(p.travelSeed(), 1, 1, 0);
  Pet q;
  tReboot(q, TT + 4 * 3600);  // volvio con la placa apagada
  CHECK(q.travelResultPending());
  Pet r;  // y otra vez: nadie lo ha mirado
  r.begin();
  CHECK(r.travelResultPending());
  TravelResult a, b;
  CHECK(r.peekTravelResult(&a));
  CHECK(r.travelResultPending());  // mirar no lo gasta
  CHECK_EQ(a.coins, want.coins);
  CHECK(r.takeTravelResult(&b));
  CHECK(memcmp(&a, &b, sizeof(a)) == 0);
  CHECK(!r.travelResultPending());
  r.flushSave();  // el loop lo vuelca con la pantalla atenuada
  Pet s;
  s.begin();
  CHECK(!s.travelResultPending());  // visto: no vuelve
  CHECK_EQ(s.coins, r.coins);       // y la coleccion y las monedas se quedan
  CHECK_EQ(s.souvenirMask(), r.souvenirMask());
  CHECK_EQ(s.souvenirFoundEpoch(want.souvenir == TRV_NONE ? 0 : want.souvenir),
           want.souvenir == TRV_NONE ? 0u : r.souvenirFoundEpoch(want.souvenir));
}

TEST(travel, un_guardado_de_antes_sin_las_claves) {
  Pet p;
  tBorn(p);
  p.setSouvenirMask(0x3u);
  CHECK(p.startTravel(0, 1, 0));
  Preferences pr;  // como una partida de antes del Y5: sin ninguna de las claves
  pr.begin("tamapoke", false);
  pr.remove("trv");
  pr.remove("trvR");
  pr.remove("souv");
  pr.remove("souv2");
  pr.remove("souvD");
  Pet q;
  q.begin();
  CHECK(!q.travelling());
  CHECK(!q.travelResultPending());
  CHECK_EQ(q.souvenirMask(), 0ull);
  CHECK_EQ(q.souvenirCount(), (uint8_t)0);
  CHECK_EQ(q.souvenirFoundEpoch(0), (uint32_t)0);
  CHECK_EQ(q.travelSlot(), (uint8_t)TRV_NONE);
  CHECK_EQ(q.travelSecondsLeft(), (uint32_t)0);
  CHECK(q.canWake(0));
  q.setClock(TT);
  CHECK(q.canTravel(0, 1, 0));
  // y una partida nueva igual
  Pet n;
  mockNvsReset();
  n.begin();
  CHECK(!n.travelling());
  CHECK_EQ(n.souvenirMask(), 0ull);
}

TEST(travel, serie_souv_y_dbg) {
  Pet p;
  tBorn(p);
  p.setSouvenirMask(0x1000001ull | (1ull << 44));
  CHECK_EQ(p.souvenirCount(), (uint8_t)3);
  CHECK(p.hasSouvenir(0) && p.hasSouvenir(24) && p.hasSouvenir(44) && !p.hasSouvenir(1) && !p.hasSouvenir(25) &&
        !p.hasSouvenir(45));
  CHECK_EQ(p.souvenirFoundEpoch(24), TT);
  CHECK_EQ(p.souvenirFoundEpoch(44), TT);
  CHECK_EQ(p.souvenirFoundEpoch(1), (uint32_t)0);
  p.setSouvenirMask(~0ull);  // los bits de mas se ignoran
  CHECK_EQ(p.souvenirCount(), (uint8_t)SOUV_COUNT);
  Pet q;
  q.begin();
  q.setClock(TT);
  CHECK_EQ(q.souvenirMask(), (1ull << SOUV_COUNT) - 1);
  // TRAVELT: salio 30 min antes
  CHECK(q.startTravel(0, 1, 0));
  q.dbgTravelElapsed(30);
  CHECK_EQ(q.travelSecondsLeft(), (uint32_t)1800);
  q.dbgTravelElapsed(29);
  CHECK(q.travelling());
  q.dbgTravelElapsed(1);
  CHECK(!q.travelling());
}

// Un corte de luz en cualquier escritura de la vuelta: el jugador nunca cobra dos veces (monedas), y el
// bicho no pierde ni su identidad ni su registro. Peor caso aceptado: el bicho gana el crecimiento dos
// veces, o el jugador pierde el premio de ese viaje.
TEST(travel, corte_de_luz_al_volver_nunca_duplica_monedas) {
  Pet p;
  tBorn(p);
  p.setCoins(100);
  CHECK(p.startTravel(0, 1, 1));
  TravelResult want = travelRoll(p.travelSeed(), 1, 1, 0);
  uint16_t gain = want.coins;
  uint32_t uid0 = p.boxRecord(0).uid;
  uint32_t g0 = p.boxRecord(0).growMin;
  mockNvsSnapshot();
  int sawOnce = 0, sawLost = 0;
  for (long cut = 0; cut < 60; cut++) {
    mockNvsRestore();
    Pet q;
    q.begin();  // arranca con la NVS entera: el corte empieza al volver
    mockNvsCutAfter(cut);
    q.syncClock(TT + 4 * 3600);
    mockNvsCutAfter(-1);
    Pet r;  // la placa vuelve a arrancar tras el corte
    r.begin();
    r.syncClock(TT + 4 * 3600 + 60);
    int dc = (int)r.coins - 100;
    CHECK(dc == 0 || dc == gain);  // nunca el doble
    sawOnce += dc == gain;
    sawLost += dc == 0;
    CHECK_EQ(r.boxState(0), (uint8_t)BOX_OK);
    CHECK_EQ(r.boxRecord(0).uid, uid0);
    CHECK(recordValid(r.boxRecord(0)));
    uint32_t g = r.boxRecord(0).growMin;
    CHECK(g == g0 + 90 || g == g0 + 180 || g == g0);  // 1 vez, 2 veces (peor caso), o aun sin volver
    CHECK(!r.travelling() || r.slotTravelling(0));
  }
  CHECK(sawOnce > 0);  // sin corte (o tarde) se cobra una vez
  CHECK(sawLost > 0);  // y alguno de los cortes pierde el premio
}

// ---------------------------------------------------------------- v3.15: 9 mapas, 45 recuerdos
TEST(travel, los_mapas_nuevos_traen_sus_recuerdos_25_a_44) {
  bool seen[SOUV_COUNT] = { false };
  for (uint8_t d = 6; d <= TRV_DESTS; d++)
    for (int i = 0; i < 4000; i++) {
      TravelResult r = travelRoll((uint32_t)i * 2654435761u + d, d, 2, 0);  // 8 h: siempre trae recuerdo
      CHECK_EQ(r.dest, d);
      CHECK(r.souvenir != TRV_NONE);
      CHECK(r.souvenir >= 25 && r.souvenir < SOUV_COUNT);
      CHECK_EQ(souvenirDest(r.souvenir), d);
      CHECK_EQ((int)r.souvenir, (d - 1) * SOUV_PER_DEST + r.souvenir % SOUV_PER_DEST);
      seen[r.souvenir] = true;
    }
  for (int id = 25; id < SOUV_COUNT; id++) CHECK_MSG(seen[id], "el recuerdo " + std::to_string(id) + " no sale nunca");
  // la mascara de 64 bits: tener los 5 de la torre (35..39) hace repetido todo lo que trae de alli
  uint64_t tower = 0x1Full << 35;
  for (int i = 0; i < 500; i++) {
    TravelResult r = travelRoll((uint32_t)i * 977u, 8, 2, tower);
    CHECK(r.dup);
    CHECK(r.souvenir >= 35 && r.souvenir <= 39);
    CHECK_EQ(r.coins, (uint16_t)(TRV_COINS[2] + TRV_DUP_COINS));
  }
  // ...y no los de otro mapa con los mismos bits bajos (35 - 32 = 3: un desplazamiento de 32 bits los confundiria)
  TravelResult r = travelRoll(5, 1, 2, tower);
  CHECK(!r.dup);
  // un mapa fuera de rango se queda en el ultimo
  CHECK_EQ(travelRoll(5, TRV_DESTS + 3, 2, 0).dest, (uint8_t)TRV_DESTS);
}

TEST(travel, un_viaje_a_un_mapa_nuevo_paga_su_recuerdo_y_se_guarda) {
  Pet p;
  tBorn(p);
  p.coins = 9999;
  CHECK(p.buyMap(9));
  CHECK(p.startTravel(0, 9, 2));
  TravelResult want = travelRoll(p.travelSeed(), 9, 2, p.souvenirMask());
  p.travelFinishNow();
  TravelResult res;
  CHECK(p.takeTravelResult(&res));
  CHECK_EQ(res.souvenir, want.souvenir);
  CHECK(res.souvenir >= 40 && res.souvenir <= 44);
  CHECK(p.hasSouvenir(res.souvenir));
  CHECK_EQ(p.souvenirFoundEpoch(res.souvenir), TT);
  p.flushSave();
  Pet q;
  q.begin();
  CHECK_EQ(q.souvenirMask(), p.souvenirMask());
  CHECK_EQ(q.souvenirFoundEpoch(res.souvenir), TT);
  Preferences pr;
  pr.begin("tamapoke", true);
  CHECK(pr.isKey("souv2"));
  CHECK(!pr.isKey("souv"));
  CHECK_EQ(pr.getBytesLength("souvD"), sizeof(uint32_t) * SOUV_COUNT);
}

// la mascara guardada: "souv2" manda; sin ella, la vieja "souv" de 32 bits en los bits bajos (solo sus 25)
TEST(travel, souvLoadMask_migra_la_mascara_de_32_bits) {
  CHECK_EQ(souvLoadMask(false, 0, 0), 0ull);
  CHECK_EQ(souvLoadMask(false, 0, 0x0A5B6D3u), 0x0A5B6D3ull);
  CHECK_EQ(souvLoadMask(false, 0, 0xFFFFFFFFu), (1ull << 25) - 1);  // bits de mas en la vieja: fuera
  CHECK_EQ(souvLoadMask(true, 1ull << 44, 0x1Fu), 1ull << 44);       // con las dos (un corte al guardar): la nueva
  CHECK_EQ(souvLoadMask(true, 0, 0x1Fu), 0ull);
  CHECK_EQ(souvLoadMask(true, ~0ull, 0), SOUV_ALL);
}

// una partida de v3.14: "souv" (u32) y "souvD" de 25 fechas (100 bytes). Al arrancar con v3.15 conserva los mismos
// recuerdos y fechas, los 20 nuevos vacios; al guardar escribe "souv2", borra "souv" y "souvD" pasa a 180 bytes
TEST(travel, migracion_de_la_coleccion_de_v3_14) {
  Pet p;
  tBorn(p);
  p.flushSave();
  const uint32_t oldMask = 0x1A5B6D3u;  // 15 recuerdos, entre ellos el 24 (el raro de las ruinas)
  uint32_t oldDates[25] = { 0 };
  for (int i = 0; i < 25; i++)
    if (oldMask & (1u << i)) oldDates[i] = TT - 86400u * (uint32_t)(i + 1);
  Preferences pr;
  pr.begin("tamapoke", false);
  pr.remove("souv2");
  pr.putUInt("souv", oldMask);
  pr.putBytes("souvD", oldDates, sizeof(oldDates));
  CHECK_EQ(pr.getBytesLength("souvD"), (size_t)100);
  Pet q;
  q.begin();
  CHECK_EQ(q.souvenirMask(), (uint64_t)oldMask);
  CHECK_EQ(q.souvenirCount(), (uint8_t)15);
  for (uint8_t i = 0; i < 25; i++) CHECK_EQ(q.souvenirFoundEpoch(i), oldDates[i]);
  for (uint8_t i = 25; i < SOUV_COUNT; i++) {
    CHECK(!q.hasSouvenir(i));
    CHECK_EQ(q.souvenirFoundEpoch(i), (uint32_t)0);
  }
  // guarda: la clave nueva, sin la vieja, y las 45 fechas
  q.setClock(TT);
  q.setSouvenirMask(q.souvenirMask() | (1ull << 30));  // (guarda) uno de la central, fechado ahora
  CHECK(pr.isKey("souv2"));
  CHECK(!pr.isKey("souv"));
  CHECK_EQ(pr.getULong64("souv2", 0), (uint64_t)oldMask | (1ull << 30));
  CHECK_EQ(pr.getBytesLength("souvD"), sizeof(uint32_t) * SOUV_COUNT);
  Pet r;
  r.begin();
  CHECK_EQ(r.souvenirMask(), (uint64_t)oldMask | (1ull << 30));
  for (uint8_t i = 0; i < 25; i++) CHECK_EQ(r.souvenirFoundEpoch(i), oldDates[i]);
  CHECK(r.souvenirFoundEpoch(30) != 0);
  // un corte entre "souv2" y el borrado de "souv" deja las dos: manda "souv2"
  pr.putUInt("souv", 0x3u);
  Pet s;
  s.begin();
  CHECK_EQ(s.souvenirMask(), (uint64_t)oldMask | (1ull << 30));
  // un "souvD" mas largo (un firmware futuro) se lee recortado; uno raro, mas corto y sin fechas enteras, tambien
  uint32_t longer[50];
  for (int i = 0; i < 50; i++) longer[i] = 1000u + (uint32_t)i;
  pr.putBytes("souvD", longer, sizeof(longer));
  Pet t;
  t.begin();
  CHECK_EQ(t.souvenirFoundEpoch(0), 1000u);
  CHECK_EQ(t.souvenirFoundEpoch(30), 1030u);
  pr.putBytes("souvD", longer, 6);  // 1 fecha y media: la 1.a entera, lo demas como venga, desde la 3.a a 0
  Pet u;
  u.begin();
  CHECK_EQ(u.souvenirFoundEpoch(0), 1000u);
  CHECK_EQ(u.souvenirFoundEpoch(4), (uint32_t)0);
}
