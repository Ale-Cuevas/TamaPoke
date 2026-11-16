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
  CHECK_EQ(SOUV_COUNT, 25);
  int sum = 0;
  for (int i = 0; i < TRV_POOL_N; i++) sum += TRV_POOL_W[i];
  CHECK_EQ(sum, 100);
  for (int t = 0; t < TRV_TIERS; t++) CHECK_EQ(TRV_RARITY_W[t][0] + TRV_RARITY_W[t][1] + TRV_RARITY_W[t][2], 100);
  CHECK_EQ(TRV_COINS[0], (uint16_t)15);
  CHECK_EQ(TRV_COINS[1], (uint16_t)40);
  CHECK_EQ(TRV_COINS[2], (uint16_t)90);
  CHECK_EQ(TRV_GROW[2], (uint16_t)240);
  for (uint8_t id = 0; id < SOUV_COUNT; id++) {
    CHECK_EQ(souvenirDest(id), (uint8_t)(id / 5 + 1));
    CHECK_EQ(souvenirRarity(id), id % 5 <= 1 ? (uint8_t)SOUV_COMMON : id % 5 <= 3 ? (uint8_t)SOUV_UNCOMMON : (uint8_t)SOUV_RARE);
  }
  CHECK(sizeof(TravelResult) <= 64);
}

TEST(travel, el_sorteo_es_determinista) {
  for (uint32_t s = 1; s < 200; s++)
    for (uint8_t d = 1; d <= 5; d++)
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
  int itemId[16] = { 0 };
  for (uint8_t t = 0; t < 3; t++)
    for (int i = 0; i < N; i++) {
      uint8_t d = (uint8_t)(1 + i % 5);
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
  // el reparto de objetos de 3 h: baya 40, tarta 15, vitaminas 10, caramelo 10, revivir 5 (de los que traen)
  CHECK_RANGE(itemId[IT_GOLDEN] * 100.0 / item[1], 37.0, 43.0);
  CHECK_RANGE(itemId[IT_CAKE] * 100.0 / item[1], 12.0, 18.0);
  CHECK_RANGE(itemId[IT_PROTEIN] * 100.0 / item[1], 8.0, 12.0);
  CHECK_RANGE(itemId[IT_RARECANDY] * 100.0 / item[1], 8.0, 12.0);
  CHECK_RANGE(itemId[IT_REVIVE] * 100.0 / item[1], 3.0, 7.0);
  CHECK_EQ(itemId[IT_CHARM] + itemId[IT_EGG], 0);
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
    for (uint8_t d = 1; d <= 5; d++)
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
  for (uint8_t d = 1; d <= 5; d++) {
    int fit = 0;
    for (uint8_t k = 0; k < TRV_TEMPLATES; k++) fit += travelTplFits(k, d);
    CHECK(fit >= 3);
  }
  CHECK(!travelTplFits(3, 1));  // "chapoteo": solo la playa
  CHECK(travelTplFits(3, 4));
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
  CHECK_EQ(q.items[want.item], (uint8_t)((want.item == IT_REVIVE ? REVIVE_START : 0) + 1));
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
  pr.remove("souvD");
  Pet q;
  q.begin();
  CHECK(!q.travelling());
  CHECK(!q.travelResultPending());
  CHECK_EQ(q.souvenirMask(), 0u);
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
  CHECK_EQ(n.souvenirMask(), 0u);
}

TEST(travel, serie_souv_y_dbg) {
  Pet p;
  tBorn(p);
  p.setSouvenirMask(0x1000001u);
  CHECK_EQ(p.souvenirCount(), (uint8_t)2);
  CHECK(p.hasSouvenir(0) && p.hasSouvenir(24) && !p.hasSouvenir(1) && !p.hasSouvenir(25));
  CHECK_EQ(p.souvenirFoundEpoch(24), TT);
  CHECK_EQ(p.souvenirFoundEpoch(1), (uint32_t)0);
  p.setSouvenirMask(0xFFFFFFFFu);  // los bits de mas se ignoran
  CHECK_EQ(p.souvenirCount(), (uint8_t)25);
  Pet q;
  q.begin();
  q.setClock(TT);
  CHECK_EQ(q.souvenirMask(), (1u << 25) - 1);
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
