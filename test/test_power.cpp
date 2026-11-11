// Politica de consumo (power.h).
#include "framework.h"
#include "../power.h"

static PowerCtx ctx(uint32_t idle) {
  PowerCtx c = {};
  c.idleMs = idle;
  c.usb = false;
  c.batPct = 80;
  c.brightLevel = BRIGHT_DEFAULT;
  return c;
}

TEST(power, idleSince_no_desborda_si_el_toque_es_posterior_al_reloj_leido) {
  CHECK_EQ((int)idleSince(1000, 400), 600);
  CHECK_EQ((int)idleSince(1000, 1000), 0);
  CHECK_EQ((int)idleSince(1000, 1002), 0);               // el toque se atendio 2 ms despues de leer el reloj
  CHECK_EQ((int)dimStageFor(idleSince(1000, 1002)), (int)DIM_AWAKE);
  CHECK_EQ((int)idleSince(5, 0xFFFFFFF0u), 21);
  CHECK(idleSince(0x90000000u, 0) == 0x90000000u);       // 28 dias sin tocar: no se confunde con un toque futuro          // vuelta del reloj de 49 dias
}

// atenuada a los 90 s; salvapantallas a los 2 min (hasta la v1.49 a los 5)
TEST(power, los_escalones_de_atenuacion_son_90_s_y_2_min) {
  CHECK_EQ((int)dimStageFor(0), (int)DIM_AWAKE);
  CHECK_EQ((int)dimStageFor(90000), (int)DIM_AWAKE);
  CHECK_EQ((int)dimStageFor(90001), (int)DIM_LOW);
  CHECK_EQ((int)dimStageFor(119999), (int)DIM_LOW);
  CHECK_EQ((int)dimStageFor(120000), (int)DIM_LOW);
  CHECK_EQ((int)dimStageFor(120001), (int)DIM_OFF);
  CHECK_EQ((int)dimStageFor(300000), (int)DIM_OFF);  // antes aun atenuada
}

TEST(power, el_brillo_no_cambia_con_bateria_normal) {
  PowerCtx c = ctx(0);
  CHECK_EQ((int)brightnessFor(c), 150);       // nivel por defecto
  c.usb = true;
  CHECK_EQ((int)brightnessFor(c), 150);       // con USB igual: manda el nivel (v1.23)
  c = ctx(100000);
  CHECK_EQ((int)brightnessFor(c), 60);        // atenuada
  c.sleeping = true;
  CHECK_EQ((int)brightnessFor(c), 60);        // dormido, igual (v1.31: antes 10)
  c = ctx(400000);
  CHECK_EQ((int)brightnessFor(c), 150);           // salvapantallas: el nivel elegido (antes 40)
  c.faceDown = true;
  CHECK_EQ((int)brightnessFor(c), 8);             // boca abajo: casi apagada, como antes
  c = ctx(0);
  c.screenOff = true;
  CHECK_EQ((int)brightnessFor(c), 0);
}

TEST(power, con_poca_bateria_el_brillo_tiene_tope_y_no_sube_nunca) {
  PowerCtx c = ctx(0);
  c.batPct = 15;
  CHECK_EQ((int)brightnessFor(c), 100);
  c.batPct = 5;
  CHECK_EQ((int)brightnessFor(c), 60);
  c.charging = true;                           // cargando: sin restricciones
  CHECK_EQ((int)brightnessFor(c), 150);
  c = ctx(100000);
  c.batPct = 5;
  CHECK_EQ((int)brightnessFor(c), 60);         // el tope no sube un brillo ya menor
  c = ctx(400000);
  c.batPct = 15;
  CHECK_EQ((int)brightnessFor(c), 100);        // salvapantallas con bateria baja: el tope
  c.batPct = 5;
  CHECK_EQ((int)brightnessFor(c), 60);         // y con bateria critica
  c.charging = true;
  CHECK_EQ((int)brightnessFor(c), 150);        // cargando: el nivel tal cual
}

TEST(power, el_periodo_de_frame_crece_con_la_inactividad_y_nunca_es_cero_con_pantalla_encendida) {
  uint32_t prev = 0;
  for (uint32_t idle = 0; idle <= 600000; idle += 1000) {
    PowerCtx c = ctx(idle);
    uint32_t ms = frameIntervalMs(c);
    CHECK(ms >= 100);
    CHECK(ms >= prev);
    prev = ms;
  }
  CHECK_EQ((int)frameIntervalMs(ctx(0)), 100);        // activo: 10 fps como siempre
  CHECK_EQ((int)frameIntervalMs(ctx(70000)), 150);
  CHECK_EQ((int)frameIntervalMs(ctx(100000)), 250);
  CHECK_EQ((int)frameIntervalMs(ctx(400000)), 1000);
}

TEST(power, el_juego_y_la_pantalla_apagada_mandan_sobre_el_resto) {
  PowerCtx g = ctx(200000);
  g.gameOpen = true;
  CHECK_EQ((int)frameIntervalMs(g), 85);               // la fisica esta afinada a 85 ms
  g.batPct = 5;
  CHECK_EQ((int)frameIntervalMs(g), 85);
  PowerCtx off = ctx(0);
  off.screenOff = true;
  CHECK_EQ((int)frameIntervalMs(off), 0);
}

TEST(power, dormido_y_con_poca_bateria_se_dibuja_menos) {
  PowerCtx c = ctx(0);
  c.sleeping = true;
  CHECK_EQ((int)frameIntervalMs(c), 100);              // dormido pero usandolo: fluido
  c.idleMs = 70000;
  CHECK_EQ((int)frameIntervalMs(c), 500);              // dormido y sin tocar: casi nada se mueve
  c = ctx(0);
  c.batPct = 15;
  CHECK_EQ((int)frameIntervalMs(c), 200);
  c.usb = true;                                        // con USB no hay ahorro por bateria
  CHECK_EQ((int)frameIntervalMs(c), 100);
}

TEST(power, el_amplificador_solo_esta_encendido_con_el_bicho_despierto_y_activo) {
  PowerCtx c = ctx(1000);
  CHECK(amplifierWanted(c));
  c.sleeping = true;
  CHECK(!amplifierWanted(c));
  c = ctx(100000);
  CHECK(!amplifierWanted(c));
  c = ctx(0);
  c.screenOff = true;
  CHECK(!amplifierWanted(c));
}

TEST(power, light_sleep_con_la_pantalla_apagada_o_casi_apagada_y_sin_usb) {
  PowerCtx c = ctx(0);
  CHECK(!lightSleepAllowed(c));                        // en uso: no
  c.idleMs = 100000;
  CHECK(!lightSleepAllowed(c));                        // atenuada: aun no
  c.idleMs = 400000;
  CHECK(lightSleepAllowed(c));                         // casi apagada: si
  c.usb = true;
  CHECK(!lightSleepAllowed(c));                        // con USB nunca
  c = ctx(0);
  c.screenOff = true;
  CHECK(lightSleepAllowed(c));
  c.usb = true;
  CHECK(!lightSleepAllowed(c));
  CHECK_EQ((int)idleDelayMs(c), 20);
  c.screenOff = false;
  CHECK_EQ((int)idleDelayMs(c), 1);
}

TEST(power, la_bateria_se_lee_menos_veces_con_la_pantalla_apagada) {
  CHECK_EQ((int)powerCacheMs(ctx(0)), 2000);
  CHECK_EQ((int)powerCacheMs(ctx(400000)), 10000);
  PowerCtx off = ctx(0);
  off.screenOff = true;
  CHECK_EQ((int)powerCacheMs(off), 10000);
}

// Escenario: se toca el bicho y se deja 10 minutos. Se cuentan los frames volcados (cada uno
// son 466*466*2 bytes por QSPI) y los despertares del bucle con la politica de antes
// (10 fps siempre, 1 ms de pausa) y con la nueva.
TEST(power, diez_minutos_sin_tocar_vuelcan_mucho_menos_que_antes) {
  const uint32_t frameBytes = 466u * 466u * 2u;
  uint64_t before = 0, after = 0;
  uint32_t lastAfter = 0;
  for (uint32_t t = 0; t < 600000; t += 10) {
    if (t % 100 == 0) before += frameBytes;           // antes: un frame cada 100 ms
    uint32_t iv = frameIntervalMs(ctx(t));
    if (iv && t - lastAfter >= iv) { after += frameBytes; lastAfter = t; }
  }
  double ratio = (double)after / (double)before;
  CHECK(ratio < 0.35);                                 // al menos un 65 % menos de datos por QSPI (sale ~0,32)
}

TEST(power, con_la_pantalla_apagada_y_sin_usb_el_bucle_despierta_cinco_veces_por_segundo) {
  PowerCtx c = ctx(0);
  c.screenOff = true;
  CHECK(lightSleepAllowed(c));
  CHECK_EQ(1000 / POWER_SLEEP_MS, 5);                  // antes: 1000 despertares por segundo
}

// v1.23: el nivel elegido manda en el brillo despierto; atenuada o dormido nunca lo pasa
TEST(power, el_nivel_de_brillo_fija_el_brillo_despierto) {
  for (uint8_t lv = 0; lv < BRIGHT_LEVELS; lv++) {
    PowerCtx c = ctx(0);
    c.brightLevel = lv;
    CHECK_EQ((int)brightnessFor(c), (int)BRIGHT_VALUE[lv]);
  }
  PowerCtx c = ctx(0);
  c.brightLevel = 3;
  CHECK_EQ((int)brightnessFor(c), 220);
  c.batPct = 15;
  CHECK_EQ((int)brightnessFor(c), 100);  // bateria baja: el tope sigue mandando
  c.batPct = 5;
  CHECK_EQ((int)brightnessFor(c), 60);
  c = ctx(0);
  c.brightLevel = 0;  // el minimo (50)
  CHECK_EQ((int)brightnessFor(c), 50);
  c.sleeping = true;
  CHECK_EQ((int)brightnessFor(c), 50);  // dormido, el mismo (v1.31: antes 25)
  c = ctx(100000);    // atenuada: 60, pero nunca mas que el nivel
  c.brightLevel = 0;
  CHECK_EQ((int)brightnessFor(c), 50);
  c.brightLevel = 1;
  CHECK_EQ((int)brightnessFor(c), 60);
  c = ctx(400000);
  c.brightLevel = 0;
  CHECK_EQ((int)brightnessFor(c), 50);  // salvapantallas: el nivel, tambien el minimo
  c = ctx(0);
  c.brightLevel = 0;
  c.screenOff = true;
  CHECK_EQ((int)brightnessFor(c), 0);
}

TEST(power, el_nivel_de_brillo_cicla_y_un_valor_raro_cae_al_de_por_defecto) {
  CHECK_EQ((int)nextBrightLevel(0), 1);
  CHECK_EQ((int)nextBrightLevel(2), 3);
  CHECK_EQ((int)nextBrightLevel(BRIGHT_LEVELS - 1), 0);
  CHECK_EQ((int)nextBrightLevel(200), 0);  // un guardado corrupto vuelve al principio
  CHECK_EQ((int)brightLevelValue(200), (int)BRIGHT_VALUE[BRIGHT_DEFAULT]);
  for (uint8_t lv = 1; lv < BRIGHT_LEVELS; lv++) CHECK(BRIGHT_VALUE[lv] > BRIGHT_VALUE[lv - 1]);
}

// v1.28: el salvapantallas (fondo negro, el bicho y la hora); a los 2 min sin tocar (antes 5)
TEST(power, el_salvapantallas_sale_a_los_2_minutos_y_no_boca_abajo_ni_apagada) {
  PowerCtx c = ctx(119999);
  CHECK(!saverActive(c));
  c = ctx(120000);
  CHECK(!saverActive(c));
  c = ctx(120001);
  CHECK(saverActive(c));
  c.sleeping = true;
  CHECK_EQ((int)brightnessFor(c), 150);  // dormido, el mismo nivel (v1.31: antes 16)
  c.faceDown = true;
  CHECK(!saverActive(c));
  c = ctx(400000);
  c.screenOff = true;  // apagada con PWR: nada
  CHECK(!saverActive(c));
  CHECK_EQ((int)brightnessFor(c), 0);
  CHECK_EQ((int)frameIntervalMs(c), 0);
  c = ctx(400000);
  CHECK_EQ((int)frameIntervalMs(c), 1000);  // un frame por segundo, como antes
  CHECK(lightSleepAllowed(c));
  // el ritmo de 1 fps, el light sleep y la bateria cada 10 s llegan tambien a los 2 min
  c = ctx(119999);
  CHECK_EQ((int)frameIntervalMs(c), 250);
  CHECK(!lightSleepAllowed(c));
  CHECK_EQ((int)powerCacheMs(c), 2000);
  c = ctx(120001);
  CHECK_EQ((int)frameIntervalMs(c), 1000);
  CHECK(lightSleepAllowed(c));
  CHECK_EQ((int)powerCacheMs(c), 10000);
}

// el salvapantallas usa el brillo del nivel elegido (antes fijo a 40), con los topes de bateria;
// boca abajo sigue a 8
TEST(power, el_salvapantallas_tiene_el_brillo_del_nivel) {
  for (uint8_t lv = 0; lv < BRIGHT_LEVELS; lv++) {
    PowerCtx c = ctx(POWER_SAVER_IDLE_MS);
    c.brightLevel = lv;
    int v = brightLevelValue(lv);
    CHECK_EQ((int)brightnessFor(c), v);
    c.usb = true;
    CHECK_EQ((int)brightnessFor(c), v);
    c.usb = false;
    c.batPct = 15;
    CHECK_EQ((int)brightnessFor(c), v < 100 ? v : 100);
    c.batPct = 5;
    CHECK_EQ((int)brightnessFor(c), v < 60 ? v : 60);
    c.batPct = -1;  // sin bateria: sin topes
    CHECK_EQ((int)brightnessFor(c), v);
    c.batPct = 80;
    c.faceDown = true;
    CHECK_EQ((int)brightnessFor(c), 8);
    c = ctx(100000);  // atenuada: 60 como mucho, sin pasar del nivel
    c.brightLevel = lv;
    CHECK_EQ((int)brightnessFor(c), v < 60 ? v : 60);
  }
}

// v1.31: con el bicho dormido la pantalla se ve igual de bien que despierto, en todos los
// escalones (activa, atenuada, salvapantallas, boca abajo, apagada) y con cualquier nivel o bateria
TEST(power, dormido_el_brillo_es_el_mismo_que_despierto) {
  const uint32_t idles[] = { 0, 70000, 100000, 120001, 300001, 400000 };
  const int bats[] = { 80, 15, 5, -1 };
  for (uint8_t lv = 0; lv < BRIGHT_LEVELS; lv++)
    for (uint32_t idle : idles)
      for (int bat : bats)
        for (int fd = 0; fd < 2; fd++)
          for (int off = 0; off < 2; off++) {
            PowerCtx awake = ctx(idle);
            awake.brightLevel = lv;
            awake.batPct = bat;
            awake.faceDown = fd;
            awake.screenOff = off;
            PowerCtx asleep = awake;
            asleep.sleeping = true;
            CHECK_EQ((int)brightnessFor(asleep), (int)brightnessFor(awake));
          }
}

// v1.32: la pulsacion corta del PWR recorre juego -> salvapantallas -> apagada -> juego
TEST(power, el_pwr_recorre_juego_salvapantallas_apagada) {
  CHECK_EQ((int)pwrNext(VIEW_ON), (int)VIEW_SAVER);
  CHECK_EQ((int)pwrNext(VIEW_SAVER), (int)VIEW_OFF);
  CHECK_EQ((int)pwrNext(VIEW_OFF), (int)VIEW_ON);
  PwrView v = VIEW_ON;
  for (int i = 0; i < 3; i++) v = pwrNext(v);
  CHECK_EQ((int)v, (int)VIEW_ON);  // tres pulsaciones: de vuelta al juego
}

TEST(power, la_vista_del_pwr_sale_del_estado_de_la_pantalla) {
  CHECK_EQ((int)pwrViewOf(ctx(0)), (int)VIEW_ON);
  CHECK_EQ((int)pwrViewOf(ctx(100000)), (int)VIEW_ON);          // atenuada a los 90 s: sigue siendo el juego
  CHECK_EQ((int)pwrViewOf(ctx(119999)), (int)VIEW_ON);          // aun atenuada
  CHECK_EQ((int)pwrViewOf(ctx(120001)), (int)VIEW_SAVER);       // el de los 2 min: el PWR la apaga
  PowerCtx c = ctx(400000);
  c.faceDown = true;
  CHECK_EQ((int)pwrViewOf(c), (int)VIEW_ON);                    // boca abajo no es el salvapantallas
  c = ctx(0);
  c.screenOff = true;
  CHECK_EQ((int)pwrViewOf(c), (int)VIEW_OFF);
  c.idleMs = 400000;
  CHECK_EQ((int)pwrViewOf(c), (int)VIEW_OFF);                   // apagada manda sobre la inactividad
}

// el salvapantallas puesto con PWR es el mismo que el de los 2 min: su brillo, su ritmo, el light
// sleep sin USB y el amplificador apagado, dormido o despierto
TEST(power, el_salvapantallas_del_pwr_es_el_de_los_dos_minutos) {
  for (int sleeping = 0; sleeping < 2; sleeping++) {
    PowerCtx c = ctx(POWER_SAVER_IDLE_MS);
    c.sleeping = sleeping;
    CHECK(saverActive(c));
    CHECK_EQ((int)pwrViewOf(c), (int)VIEW_SAVER);
    CHECK_EQ((int)brightnessFor(c), (int)brightLevelValue(BRIGHT_DEFAULT));
    CHECK_EQ((int)frameIntervalMs(c), 1000);
    CHECK(lightSleepAllowed(c));
    CHECK(!amplifierWanted(c));
  }
  CHECK(!saverActive(ctx(POWER_SAVER_IDLE_MS - 1)));  // un ms antes, aun atenuada
}
