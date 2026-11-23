#pragma once
// ============================================================================================
//  tuning.h: los numeros del equilibrio del juego, todos en un sitio
// ============================================================================================
// Para afinar el juego se cambia un valor aqui, se compila y se flashea (pio run -e tamapoke -t upload).
// Es logica pura (solo <stdint.h>): la usan el firmware, los tests del PC (test/) y el render (tools/render).
//
// Lo que NO esta aqui, a proposito:
//   - la curva de niveles (growth.h, LV_COST): cambiarla sube o baja de nivel a los bichos ya criados;
//     la de verdad se cambia con una migracion (clave "gcv", migrateGrowthCurve en pet.cpp)
//   - la fisica y la dificultad de los minijuegos (ballgame.h, zengame.h, dashgame.h, arriba del todo)
//   - los 8 estadios: esbirros, niveles, jefes (stage.h, STAGES) y los exclusivos salvajes (encounter.h)
//   - lo que fija el tamano de algo guardado en flash (GIFT_MAX, CAP_EGG_MAX, ITEM ids, la caja): pet.h, box.h
//   - tiempos de animacion y de pantalla (pet.h, power.h, TamaPoke.ino)
//
// Al cambiar un valor:
//   1. ./test/run_tests.sh: los tests que fallen fijan a proposito ese numero (un balance que se decidio);
//      si el cambio es el que quieres, actualiza el test.
//   2. Si el numero sale en el README ("Game manual"), cambialo alli tambien.
//   3. docs/GAME_LOGIC.md cita algunos valores: revisa la seccion del sistema que tocaste.
// Las unidades van en cada comentario. "min" = minutos de juego (1 min de juego = 1 min real).
#include <stdint.h>
#include "upg_tiers.h"

// --------------------------------------------------------------------------------------------
// Tiempo de juego
// --------------------------------------------------------------------------------------------
// 1 tick = 1 minuto de juego. Baja este valor para probar mas rapido
// (p. ej. 5000UL = las estadisticas caen 12x mas rapido).
#define PET_TICK_MS 60000UL

// --------------------------------------------------------------------------------------------
// Necesidades despierto: lo que baja cada minuto (Pet::tick, pet.cpp). Barras de 0 a 100.
// --------------------------------------------------------------------------------------------
// (v3.11: el doble de lento que antes: comida -2/min, energia y felicidad -1/min, hambre -2 mas)
#define FOOD_DRAIN 1            // comida -1/min (nada mientras dura la tarta)
#define ENE_DRAIN 1             // energia -1 cada ENE_DRAIN_EVERY min...
#define ENE_DRAIN_EVERY 2
#define OVERWEIGHT_AT 50        // ...y -OVERWEIGHT_ENE_DRAIN mas (en el mismo toque) con el peso por encima de esto
#define OVERWEIGHT_ENE_DRAIN 1
#define JOY_DRAIN 1             // felicidad -1 cada JOY_DRAIN_EVERY min...
#define JOY_DRAIN_EVERY 2
#define JOY_DRAIN_HUNGRY 1      // ...y -1 mas cada minuto con la comida por debajo de JOY_HUNGRY_BELOW
#define JOY_HUNGRY_BELOW 30
#define NEED_LOW 25             // por debajo: esta triste (mood) y lo pide en un globo del patio
// alarma de necesidad critica: con un bicho de casa despierto a NEED_ALARM_AT o menos en su necesidad mas baja, la
// placa suena dos pitidos cada NEED_ALARM_EVERY_MS hasta que se arregla (por debajo de NEED_LOW, justo encima de
// SLIP_AT: avisa antes del descuido). Volumen 0 = en silencio
#define NEED_ALARM_AT 15
#define NEED_ALARM_EVERY_MS 2000UL

// cacas (v1.58): probabilidad (%) por minuto despierto con la comida por encima de POOP_MIN_FOOD. Era 15 (una
// cada ~7 min); con 5, una cada ~20 min. Solo se quitan tocandolas (o con una sesion entera de cascada).
// POOP_MAX tambien es cuantas caben en el patio.
#define POOP_ODDS 5
#define POOP_MIN_FOOD 40
#define POOP_MAX 3
// v3.1 (sin limpieza): una caca con mas de POOP_STALE_MIN min en el suelo es "vieja" y, despierto, cada
// POOP_JOY_EVERY min quita felicidad: POOP_JOY_DRAIN[n] con n = cuantas viejas hay (0..POOP_MAX). Dormido no
#define POOP_STALE_MIN 5
#define POOP_JOY_EVERY 3
static const uint8_t POOP_JOY_DRAIN[POOP_MAX + 1] = { 0, 1, 2, 4 };

// el peso es grasa de mas: 0 = en forma. Una tarta (+12) dura ~3 h despierto (v1.22)
#define WEIGHT_BURN_AWAKE_MIN 15   // -1 de peso cada 15 min despierto
#define WEIGHT_BURN_SLEEP_MIN 30   // -1 cada 30 min durmiendo

// la disciplina forja la defensa: GOOD_CARE_MIN minutos seguidos despierto con todas las necesidades a
// GOOD_CARE_MIN_NEED o mas = +1 DEF
#define GOOD_CARE_MIN_NEED 40
#define GOOD_CARE_MIN 720          // 12 h

// descuido: una necesidad a SLIP_AT o menos cuenta como error de cuidado (retrasa la evolucion 1 nivel y
// quita SLIP_BOND de vinculo); luego hacen falta SLIP_COOLDOWN_MIN minutos despierto para contar otro
#define SLIP_AT 10
#define SLIP_COOLDOWN_MIN 60
#define SLIP_BOND 1

// --------------------------------------------------------------------------------------------
// Dormido (Pet::sleepMinute) y la noche (Pet::nightMinute). Necesita el reloj.
// --------------------------------------------------------------------------------------------
#define SLEEP_ENE_GAIN 6           // energia +6/min dormido
// siesta (de dia): comida y felicidad -1 cada NAP_FOOD_JOY_EVERY min, sin bajar de sus suelos
#define NAP_FOOD_JOY_EVERY 4
#define NAP_FOOD_FLOOR 30
#define NAP_JOY_FLOOR 35
// de noche (de SLEEPY_HOUR a WAKE_HOUR) baja la cuarta parte que en una siesta (v3.13: 8 -> 16 min; comida 16x mas
// lenta que despierto), con suelos mas altos: acostado lleno a las 23, amanece a las 9 con ~62
#define NIGHT_FOOD_JOY_EVERY 16
#define NIGHT_FOOD_FLOOR 50
#define NIGHT_JOY_FLOOR 50
// Desde SLEEPY_HOUR tiene sueno (globo con "zzz"); a las BED_HOUR se duerme solo y, despertado antes de
// WAKE_HOUR, vuelve a dormirse a los NIGHT_RESLEEP_MIN minutos despierto. A las WAKE_HOUR se despierta solo,
// si se durmio de noche (desde EVENING_HOUR: una siesta de tarde no). Acostarlo tu con sueno da BED_BOND de
// vinculo, una vez por noche y fuera del tope diario.
#define EVENING_HOUR 20
#define SLEEPY_HOUR 22
#define BED_HOUR 23
#define WAKE_HOUR 9
#define NIGHT_RESLEEP_MIN 30
#define BED_BOND 3

// --------------------------------------------------------------------------------------------
// Placa apagada (Pet::syncClock): el tiempo pasa igual, pero con piedad
// --------------------------------------------------------------------------------------------
// apagada baja al ritmo de despierto (FOOD_DRAIN, ENE_DRAIN, JOY_DRAIN, JOY_DRAIN_HUNGRY con hambre, y las cacas
// viejas) pero sin bajar de OFFLINE_FLOOR (por encima de SLIP_AT: sin descuidos, abandono ni muerte en ausencia; y
// por encima de NEED_ALARM_AT: al encender no suena la alarma de necesidad)
#define OFFLINE_FLOOR 16
static_assert(OFFLINE_FLOOR > NEED_ALARM_AT && OFFLINE_FLOOR > SLIP_AT, "tuning.h: el suelo de ausencia queda por encima de la alarma y del descuido");
static_assert(ENE_DRAIN_EVERY >= 1 && JOY_DRAIN_EVERY >= 1 && NAP_FOOD_JOY_EVERY >= 1 && NIGHT_FOOD_JOY_EVERY >= 1,
              "tuning.h: los *_EVERY son divisores (un 0 dividiria por cero)");
#define OFFLINE_POOP_EVERY_MIN 240   // una caca cada 4 h despierto apagada
#define OFFLINE_MAX_MIN (14UL * 24 * 60)  // como mucho 2 semanas de golpe

// --------------------------------------------------------------------------------------------
// Cuidados (pet.cpp: feedFood, eatCake, caress, useItem)
// --------------------------------------------------------------------------------------------
// comida (v3.2, food.h): la baya normal es gratis y sin limite; las de colores se consiguen (tienda, botin, paseos)
#define PLAIN_BERRY_FOOD 20        // baya normal: solo comida
#define BERRY_FOOD 30              // baya de color: mas comida y algo de felicidad
#define BERRY_JOY 3
#define FAV_BERRY_FOOD 35          // la favorita de la linea (su forma base)...
#define FAV_BERRY_JOY 12
#define FAV_BERRY_BOND 2
// tarta (v1.54): limitada. Llena la comida y la deja llena CAKE_SATED_MIN minutos; engorda.
#define CAKE_SATED_MIN 180
#define CAKE_JOY 12
#define CAKE_WEIGHT 12
#define CARESS_JOY 20              // mimarlo (el corazon del panel Cuidar, v2.3; v3.1: 5 -> 20)...
#define CARESS_BOND 1              // ...y vinculo como mucho cada PET_BOND_COOLDOWN_MS
#define PET_BOND_COOLDOWN_MS (15UL * 60 * 1000)
#define GOLDEN_FOOD 50             // baya dorada
#define GOLDEN_JOY 15
#define VITAMIN_TRAIN 3            // proteina / hierro / carburante: +3 de entrenamiento
#define RARE_CANDY_LEVELS 1        // Caramelo Raro: +1 nivel (la barra conserva lo que llevaba)
#define CHARM_SHINY_MULT 3         // amuleto: el siguiente huevo, x3 de probabilidad shiny
#define REVIVE_START 3             // Revivir con los que empieza una partida (y una vez al actualizar)
#define ITEM_MAX 9                 // tope de cada objeto en la mochila...
#define BERRY_MAX 30               // ...salvo las bayas de colores, que se venden en paquetes y caben mas
#define BERRY_PACK 5               // bayas de colores que da una compra (al precio de PRICE_BERRY)

// --------------------------------------------------------------------------------------------
// Vinculo y racha
// --------------------------------------------------------------------------------------------
// El vinculo maximo (100) debe costar 5-8 dias: 10 al dia por cuidados (tope de addBond) + 4 del primer
// cuidado del dia = 14, el dia 8 cuidandolo al maximo.
#define BOND_DAY_CAP 10            // tope diario de addBond
#define BOND_CARE_DAY 4            // primer cuidado del dia, fuera del tope
// la racha y el vinculo mejoran huevos y shinies: careBonus = min(racha, CAP) / STREAK_DIV + vinculo / BOND_DIV
// (0..14 con los valores de hoy)
#define CARE_BONUS_STREAK_CAP 30
#define CARE_BONUS_STREAK_DIV 3
#define CARE_BONUS_BOND_DIV 25

// --------------------------------------------------------------------------------------------
// Huevos: eclosion, rareza, shiny, genes
// --------------------------------------------------------------------------------------------
#define EGG_HATCH_MIN 120              // el huevo del patio eclosiona a las 2 h de tiempo real
#define EGG_SHAKE_MIN 5                // cada sacudida le quita 5 min...
#define EGG_SHAKE_CAP 60               // ...hasta 1 h en total (lo mas rapido, 1 h)
#define EGG_SHAKE_COOLDOWN_MS 30000    // entre dos sacudidas que cuentan
#define STARTER_EGG_TAPS 3             // el huevo del inicial se abre con 3 toques...
#define STARTER_EGG_MIN 3              // ...o solo a los 3 min (tambien un huevo de antes de v2.0)
#define EGG_MIN_STEPS 8000             // un huevo cada 8000-15000 pasos (al azar)
#define EGG_MAX_STEPS 15000
// rareza de un huevo al azar (%): raro = RARE + careBonus; legendario = LEG + careBonus / 3, y solo con
// EGG_LEG_MIN_REG especies registradas. "_BLESSED": tras una despedida. Tras una muerte, solo comunes.
#define EGG_RARE_PCT 27
#define EGG_RARE_PCT_BLESSED 45
#define EGG_LEG_PCT 3
#define EGG_LEG_PCT_BLESSED 10
#define EGG_LEG_MIN_REG 25
// shiny: 1 entre (SHINY_ONE_IN - careBonus), 1 entre SHINY_ONE_IN_BLESSED tras una despedida, nunca mejor que
// 1 entre SHINY_BEST (tampoco con el amuleto)
#define SHINY_ONE_IN 48
#define SHINY_ONE_IN_BLESSED 24
#define SHINY_BEST 8
// genes del individuo: GENE_MIN..GENE_MAX % de cada stat base
#define GENE_MIN 90
#define GENE_MAX 110

// --------------------------------------------------------------------------------------------
// Evolucion y finales
// --------------------------------------------------------------------------------------------
#define EVOLVE_MIN_NEED 40             // para evolucionar, todas las necesidades a 40 o mas en ese momento
#define FAREWELL_AGE_MIN (3UL * 24 * 60)  // forma final + 3 dias: puede despedirse
#define FAREWELL_POSTPONE_MIN 1440     // "quedaros juntos": se vuelve a ofrecer en 1 dia
// abandono (v1.60): con las cuatro necesidades a cero (despierto y con la placa encendida), el vinculo se
// apaga del todo en BOND_FADE_MIN minutos (-4, -3, -3...: 10 cada 3; pet.cpp tick y dbgNeglect). Ya a cero,
// DEATH_MIN minutos mas y se muere: un dialogo ofrece un Revivir durante REVIVE_LIMIT_MIN minutos reales
// (tambien apagada). Desde el nivel DEATH_EGG_LEVEL deja un huevo de su linea con un DEATH_EGG_ODDS %.
#define BOND_FADE_MIN 30
#define DEATH_MIN 30
#define REVIVE_LIMIT_MIN 60
#define REVIVE_NEEDS 50                // revivido: las tres necesidades a 50
#define DEATH_EGG_LEVEL 50
#define DEATH_EGG_ODDS 60

// --------------------------------------------------------------------------------------------
// Pasos (Pet::addSteps, goalReward). Los premios de caminar piden al bicho despierto.
// --------------------------------------------------------------------------------------------
#define STEPS_PER_JOY 100          // +1 felicidad cada 100 pasos...
#define STEP_JOY_DAY_CAP 30        // ...como mucho +30 al dia
#define STEPS_PER_WEIGHT 1000      // -1 de peso cada 1000 pasos
#define STEPS_PER_BOND 1000        // +1 de vinculo cada 1000 (con el tope diario de addBond)
#define FIND_MIN_STEPS 800         // un hallazgo cada 800-1200 pasos premiados (al azar)...
#define FIND_MAX_STEPS 1200
#define FINDS_PER_DAY 5            // ...como mucho 5 al dia (los regalos de las metas van aparte)
// lo que se encuentra (%), suman 100
#define ODDS_GOLDEN 28
#define ODDS_CAKE 17
#define ODDS_VITAMIN 30
#define ODDS_CHARM 10
#define ODDS_REVIVE 5
#define ODDS_COLORED 10            // v3.2: una baya de color (la tirada de cual, al azar entre las 6)
static_assert(ODDS_GOLDEN + ODDS_CAKE + ODDS_VITAMIN + ODDS_CHARM + ODDS_REVIVE + ODDS_COLORED == 100,
              "los hallazgos suman 100");
// metas diarias: 3000, 6000 y 10000 pasos, cada una una vez al dia y en orden
#define STEP_GOAL_TIERS 3          // (estructura: no cambiar)
static const uint32_t STEP_GOALS[STEP_GOAL_TIERS] = { 3000, 6000, 10000 };
#define STEP_GOAL 3000             // la primera (la de la racha): = STEP_GOALS[0]
static const uint8_t STEP_GOAL_LEVELS[STEP_GOAL_TIERS] = { 1, 1, 2 };  // niveles que da cada meta
#define GOAL_JOY 10                // felicidad de la meta 1...
#define GOAL2_JOY 10               // ...de la 2, con GOAL2_BOND de vinculo fuera del tope...
#define GOAL2_BOND 2
#define GOAL3_JOY 15               // ...y de la 3 (con la medalla MARATON)
#define GOAL3_BOND 3

// --------------------------------------------------------------------------------------------
// Sacudir la placa (motion.h detecta; pet.cpp castiga)
// --------------------------------------------------------------------------------------------
#define GRUMPY_JOY 5                 // despertarlo agitando: -5 felicidad...
#define GRUMPY_COOLDOWN_MS 60000UL   // ...y otro despertar en 60 s no resta mas
#define DIZZY_SHAKES 3               // 3 sacudidas despierto en DIZZY_WINDOW_MS...
#define DIZZY_WINDOW_MS 120000UL
#define DIZZY_JOY 3                  // ...restan 3 de felicidad...
#define DIZZY_COOLDOWN_MS 600000UL   // ...como mucho cada 10 minutos

// --------------------------------------------------------------------------------------------
// Minijuegos: lo que dan (la fisica y la dificultad estan en ballgame.h, zengame.h y dashgame.h)
// --------------------------------------------------------------------------------------------
#define GAME_BOND 2                // cualquier partida terminada: vinculo
#define GAME_FOOD_COST 5           // comida que cuesta jugar (nada con tarta)...
#define GAME_FLOOR 5               // ...sin bajar de aqui (tambien la energia)
// pelota (VEL): entrenamiento = puntos / BALL_SPE_DIV (sin tope por partida); felicidad = BASE + puntos *
// PER_POINT, con la parte de los puntos hasta BALL_JOY_BONUS_MAX; energia = ENE_BASE + puntos / ENE_DIV
#define BALL_SPE_DIV 5
#define BALL_JOY_BASE 5
#define BALL_JOY_PER_POINT 2
#define BALL_JOY_BONUS_MAX 30
#define BALL_ENE_BASE 10
#define BALL_ENE_DIV 2
#define GAME_WEIGHT_DIV 2          // pelota: -puntos/2 de peso
// saco (FUE): SACK_MS de golpes; 1 punto cada SACK_HITS_PER_POINT golpes, como mucho SACK_GAIN_MAX
#define SACK_MS 10000UL
#define SACK_HITS_PER_POINT 4
#define SACK_GAIN_MAX 18
#define SACK_ENE_COST 12
#define SACK_JOY 6
#define TRAIN_WEIGHT_DIV 10        // saco: -golpes/10 de peso
// cascada (DEF): solo paga la sesion entera
#define ZEN_SESSION_MS 45000u      // lo que dura (sin la intro de 1,5 s)
#define ZEN_TENTHS_PER_POINT 35    // 1 punto cada 3,5 s dentro de la franja...
#define ZEN_GAIN_MAX 8             // ...hasta 8
#define ZEN_BONUS_TENTHS_PER_POINT 40  // bonificacion: 1 DEF cada 4 s en la franja mientras se mueve...
#define ZEN_BONUS_MAX 3            // ...hasta 3
#define ZEN_ENE_COST 8
#define ZEN_FOOD_COST 3
#define ZEN_JOY 6
// trineo (VEL)
#define DASH_LIVES 3               // golpes que acaban la carrera
#define DASH_PER_POINT 4           // 1 punto cada 6 filas esquivadas (50 filas = 8)...
#define DASH_GAIN_MAX 20           // ...con el tope del saco
#define DASH_WEIGHT_DIV 8          // peso: -filas/8
#define DASH_ENE_COST 12
#define DASH_JOY_BASE 5            // felicidad = BASE + min(filas, ROWS_MAX) * PER_ROW
#define DASH_JOY_PER_ROW 2
#define DASH_JOY_ROWS_MAX 15

// --------------------------------------------------------------------------------------------
// Combate (battle.h) y estadios (pet.cpp stageDone). Los estadios en si: stage.h, STAGES.
// --------------------------------------------------------------------------------------------
#define BATTLE_LEVEL_CAP 100   // nivel que entra en las cuentas (a nivel 999 decidiria todo)
#define ATK_POWER 40           // potencia de un ataque normal
#define SPECIAL_POWER_PCT 130  // el especial: +30 %
#define SPECIAL_TAPS 5         // toques para cargarlo
#define SPECIAL_COOLDOWN 2     // ataques normales propios antes de volver a cargar
#define HIT_MIN_PCT 2          // cada golpe, entre el 2 y el 60 % de la vida maxima del rival...
#define HIT_MAX_PCT 60
#define SPECIAL_MAX_PCT 70     // ...o el 70 % si es especial: un KO pide al menos 2 golpes
#define CRIT_ONE_IN 16         // golpe critico (x1,5)
#define SUDDEN_DEATH_AT 20     // desde el ataque 21 (index 20) de un combate de fase: sin DEF y con retroceso
#define RECOIL_PCT 10          // retroceso: % de la vida maxima del propio atacante (minimo 1)
#define MAX_ATTACKS 15         // tope fuera de las fases: luego gana quien conserve mas % de vida
#define ATTACK_MS 1300         // tiempo por ataque en pantalla
#define PRACTICE_AI_TAPS 2     // el jefe de un estadio "toca" 2 veces por ataque suyo
#define PRACTICE_JOY 3                 // felicidad del final de un estadio...
#define PRACTICE_EVERY_MS 3600000UL    // ...como mucho una vez por hora
// premio del jefe, una vez al dia por estadio: a cada bicho del equipo
#define STAGE_GROW_MIN 60              // minutos de crecimiento
#define STAGE_BOND 5                   // vinculo, fuera del tope diario
// el objeto del jefe (%), suman 100
#define BDROP_CAKE 19
#define BDROP_GOLDEN 18
#define BDROP_VITAMIN 19
#define BDROP_RARECANDY 7
#define BDROP_CHARM 4
#define BDROP_REVIVE 5
#define BDROP_EGG 3
#define BDROP_BALL 7
#define BDROP_COLORED 8            // v3.2: una baya de color al azar
#define BDROP_LURE 10              // v3.14: el Cebo (solo sale del jefe y de los viajes; no se vende)
static_assert(BDROP_CAKE + BDROP_GOLDEN + BDROP_VITAMIN + BDROP_RARECANDY + BDROP_CHARM + BDROP_REVIVE + BDROP_EGG +
                      BDROP_BALL + BDROP_COLORED + BDROP_LURE == 100,
              "el botin suma 100");

// --------------------------------------------------------------------------------------------
// Encuentros salvajes (encounter.h, pet.cpp encCheck / encounterDone). Probabilidades en tantos por mil.
// --------------------------------------------------------------------------------------------
#define ENC_BASE_PERMIL 600       // la tirada base del dia: 60 %
#define ENC_WALK_TIERS 3          // v3.11: tiradas de pasear, una por cada umbral de pasos del dia...
static const uint16_t ENC_WALK_AT[ENC_WALK_TIERS] = { 3000, 6000, 10000 };
#define ENC_WALK_PERMIL 500       // ...cada una al 50 %
#define ENC_TRAVEL_PERMIL_H 30    // cada viaje: 3 % por hora
#define ENC_FIRST_MIN (9 * 60)    // la hora de la tirada base: de las 9:00...
#define ENC_LAST_MIN (20 * 60)    // ...a las 20:00
#define ENC_LIFETIME_MIN 360      // 6 h en el patio
#define ENC_EXCL_PCT 5            // de cada 100 encuentros, 5 son de una especie exclusiva (encounter.h, ENC_EXCL)
#define ENC_MAP_WEIGHT 3          // las lineas del tipo del mapa salen 3 veces mas
#define ENC_LV_CENTER_NONE 10     // sin ningun estadio vencido, el nivel ronda el 10
#define ENC_LV_SPREAD 7           // nivel = centro +-SPREAD +-SPREAD +-1 (dos tiradas: casi siempre cerca)
#define ENC_LV_MIN 2
#define ENC_LV_MAX 100
#define ENC_TR_NONE_MAX 10        // sin estadios, entrenamiento 0..10
// ganar: al que peleo, crecimiento; al de casa, felicidad; con un ENC_DROP_PCT % un objeto (%, suman 100)
#define ENC_WIN_GROW_MIN 60
#define ENC_WIN_JOY 3
#define ENC_DROP_PCT 50
#define WDROP_CAKE 25
#define WDROP_GOLDEN 25
#define WDROP_VITAMIN 21
#define WDROP_RARECANDY 8
#define WDROP_REVIVE 7
#define WDROP_CHARM 4
#define WDROP_COLORED 10           // v3.2: una baya de color al azar
static_assert(WDROP_CAKE + WDROP_GOLDEN + WDROP_VITAMIN + WDROP_RARECANDY + WDROP_REVIVE + WDROP_CHARM + WDROP_COLORED ==
                100,
              "lo que suelta un salvaje suma 100");
#define BALL_START 3              // Poke Balls de una partida nueva (y una vez al actualizar)

// --------------------------------------------------------------------------------------------
// Monedas (economy.h)
// --------------------------------------------------------------------------------------------
#define COIN_MAX 9999             // tope de la cartera
// minijuegos: 0 si no se gano nada; si no, ECO_GAME_MIN + metrica / su divisor, como mucho ECO_GAME_MAX
#define ECO_GAME_MIN 3
#define ECO_GAME_MAX 8
#define ECO_DIV_BALL 10           // pelota: +1 cada 10 puntos (50 puntos -> 8)
#define ECO_DIV_SACK 8            // saco: +1 cada 8 golpes (30 -> 6, 40 -> 8)
#define ECO_DIV_SLED 10           // trineo: +1 cada 10 filas (50 -> 8)
#define ECO_DIV_ZEN 2             // cascada: +1 cada 2 puntos de DEF (11 -> 8)
#define ECO_STAGE_FIRST 50        // vencer un estadio la primera vez (fuera del tope diario)
#define ECO_STAGE_REPEAT 10       // repetirlo, una vez por estadio y dia (dentro del tope)
#define ECO_GOAL_TIERS 3          // (estructura: = STEP_GOAL_TIERS)
static const uint8_t ECO_GOAL_COINS[ECO_GOAL_TIERS] = { 10, 20, 40 };  // metas de pasos, fuera del tope
#define ECO_DAILY_CAP 150         // tope diario de minijuegos y estadios repetidos
// precios de la tienda (0 = no se vende)
#define PRICE_GOLDEN 30
#define PRICE_PROTEIN 60
#define PRICE_IRON 60
#define PRICE_CARBOS 60
#define PRICE_RARECANDY 150
#define PRICE_CAKE 40
#define PRICE_REVIVE 80
#define PRICE_BALL 100
#define PRICE_BERRY 25            // v3.2: cada compra de baya de color (paquete de BERRY_PACK)
// mapas del patio 1..9 (prado, cantera, bosque, playa, ruinas, volcan, central, torre del cielo, distrito
// toxico): el prado es gratis y de salida
#define YARD_MAPS 9               // (estructura: los .bin de la SD; 9 cabe en "ybgO", u16, y en "trv" / "trvR")
static const uint16_t ECO_MAP_PRICE[] = { 0, 150, 150, 200, 250, 250, 300, 350, 400 };
static_assert(sizeof(ECO_MAP_PRICE) / sizeof(ECO_MAP_PRICE[0]) == YARD_MAPS, "ECO_MAP_PRICE: un precio por mapa");

// --------------------------------------------------------------------------------------------
// Mejoras de casa (v3.20, docs/HOME_AND_PALS.md, tarea HOME): camas, gimnasio y juguete, 3 niveles cada una.
// Se compran una vez y son del jugador. Los arrays por nivel van de 0 (sin comprar) a UPG_TIERS.
// --------------------------------------------------------------------------------------------
// UPG_TIERS (niveles por linea) dimensiona "upg": vive en upg_tiers.h, fuera de este fichero
// precio de cada nivel [linea][nivel - 1]: camas, gimnasio, juguete (~una semana de monedas por nivel)
static const uint16_t UPG_PRICE[3][UPG_TIERS] = { { 600, 1000, 1500 }, { 800, 1200, 1800 }, { 500, 800, 1200 } };
// camas (FR-HOME.1): ENE por minuto dormido y JOY de buenos dias tras una noche entera
static const uint8_t BED_ENE_GAIN[UPG_TIERS + 1] = { SLEEP_ENE_GAIN, 7, 8, 10 };
static const uint8_t BED_MORNING_JOY[UPG_TIERS + 1] = { 0, 5, 10, 15 };
#define BED_MORNING_MIN_SLEEP 360 // minutos dormidos de noche para el bonus (lo despierta el de las 9:00)
#define BED_WALK_MAX_MS 4000      // el paseo hasta la cama (solo se ve): si tarda mas, aparece en ella
// gimnasio (FR-HOME.2): +1 de entrenamiento (el mas bajo) cada N minutos despierto con todo >= GYM_NEED_MIN
static const uint8_t GYM_EVERY_MIN[UPG_TIERS + 1] = { 0, 60, 45, 30 };
static const uint8_t GYM_CAP[UPG_TIERS + 1] = { 0, 25, 40, 60 };      // techo: no sube mas alla solo
static const uint8_t GYM_DAY_MAX[UPG_TIERS + 1] = { 0, 4, 6, 8 };     // puntos al dia por bicho
#define GYM_ENE_COST 3            // ENE por cada punto (dropTo, suelo GAME_FLOOR)
#define GYM_NEED_MIN 40           // las tres necesidades al menos asi para entrenar
// juguete (FR-HOME.3): juega solo de vez en cuando si le falta alegria
#define TOY_JOY_BELOW 70          // solo si JOY < 70
#define TOY_PLAY_CHANCE 2         // % por minuto despierto
static const uint8_t TOY_JOY[UPG_TIERS + 1] = { 0, 3, 4, 5 };
static const uint8_t TOY_DAY_MAX[UPG_TIERS + 1] = { 0, 3, 5, 7 };      // veces al dia por bicho
// lo que se ve en el patio (solo con el patio en pantalla; la logica ya lo aplico)
#define HOME_WORKOUT_MS 3000      // el ejercicio junto al gimnasio (su gesto, en bucle)
#define HOME_TOY_PLAY_MS 3000     // el juego junto al juguete
#define HOME_MORNING_MIN_MS 1500  // el estiron de buenos dias al salir de la cama, como poco
#define HOME_VISIT_WAIT_MS 8000   // si el gimnasio o el juguete estan ocupados por el otro, espera hasta esto

// --------------------------------------------------------------------------------------------
// Los dos bichos juntos (v3.22, docs/HOME_AND_PALS.md, tarea PALS): escenas solas en el patio, solo con el patio
// en pantalla. Las riñas nunca cambian nada del bicho.
// --------------------------------------------------------------------------------------------
#define PAL_GREET_DIST 90         // px: a esta distancia se saludan (pies a pies)
#define PAL_GREET_COOLDOWN_S 180  // s entre dos saludos de la pareja
#define PAL_SCENE_GAP_MIN_S 720   // s de patio en pantalla entre dos escenas de juego (al azar entre min y max:
#define PAL_SCENE_GAP_MAX_S 1680  //   ~3 por hora)
#define PAL_PLAY_JOY 3            // JOY a cada uno tras una escena de juego
#define PAL_PLAY_PAY_DAY 4        // escenas que pagan al dia por bicho (las demas se ven igual, sin premio)
#define PAL_CHEER_BELOW 25        // animar: el triste tiene JOY < esto
#define PAL_CHEER_FROM 50         //   y el contento JOY >= esto
#define PAL_CHEER_JOY 8           // JOY para el triste
#define PAL_CHEER_DAY 2           // veces que paga al dia por bicho
#define PAL_BIG_LEVEL_GAP 20      // "grande y pequeno": una etapa de evolucion mas, o esta diferencia de nivel
#define PAL_SQUABBLE_GAP_S 3600   // s de patio en pantalla entre dos riñas (como mucho 1 por hora)
#define PAL_MAKEUP_S 3            // s hasta el corazon de las paces tras una riña
// lo que se ve (pals.h): duraciones y distancias de cada escena
#define PAL_REST_S 30             // s de patio tras una escena antes de otra (y del saludo)
#define PAL_GREET_MS 1500         // el saludo: quietos, mirandose, con su icono
#define PAL_APPROACH_MS 7000      // lo que puede tardar en llegar junto al otro (si no, la escena se deja sin premio)
#define PAL_NEAR_PX 30            // en una escena se acercan hasta esto (fuera de ellas, YARD_MIN_DIST 64)
#define PAL_BESIDE_PX 70          // "al lado" para empezar lo de juntos (pies a pies)
#define PAL_SIDE_PX 56            // junto al huevo o al que duerme por la tarde: no mas cerca (sin taparlo)
#define PAL_CHASE_MS 7000         // la persecucion: dos carreras, cambiando de papel a la mitad
#define PAL_CHASE_PCT 150         //   a esta velocidad (% del paseo)
#define PAL_HOP_MS 5000           // saltos a la vez
#define PAL_NUZZLE_MS 5000        // juntos: el toque de cabezas y los corazones
#define PAL_TOY_MS 6000           // pasarse el juguete, por turnos
#define PAL_ESCORT_MS 8000        // el grande acompana al pequeno, despacio
#define PAL_ESCORT_PCT 60         //   a esta velocidad (% del paseo)
#define PAL_CHEER_MS 5000         // animar: el carino junto al triste
#define PAL_EGGSIT_MS 6000        // sentado junto al huevo
#define PAL_SIT_MS 9000           // sentado junto al que duerme por la tarde
#define PAL_TUG_MS 2000           // el tira y afloja del juguete (rina)
#define PAL_TURN_MS 2000          //   y despues, el turno de cada uno
#define PAL_BUMP_MS 1500          // la nube gris del que despertaron de la siesta (sigue dormido)
#define PAL_ROLL_EVERY_S 45       // con camas y los dos dormidos: cada tanto uno se gira hacia el otro
#define PAL_ROLL_MS 2400
#define PAL_ROLL_PX 10
// peso de cada escena de juego al sortearla: persecucion, saltos, carantonas, juguete (con juguete), paseo
// (grande y pequeno)
static const uint8_t PAL_PLAY_W[5] = { 3, 2, 3, 3, 3 };

// --------------------------------------------------------------------------------------------
// Viajes (travel.h): 1, 3 u 8 h de tiempo real
// --------------------------------------------------------------------------------------------
#define TRV_TIERS 3               // (estructura)
static const uint16_t TRV_MIN[TRV_TIERS] = { 60, 180, 480 };         // duracion (minutos reales)
static const uint16_t TRV_COINS[TRV_TIERS] = { 50, 150, 400 };       // monedas (fuera del tope diario): ~50 por hora de viaje
static const uint16_t TRV_GROW[TRV_TIERS] = { 30, 90, 240 };         // minutos de crecimiento
static const uint8_t TRV_ITEM_PCT[TRV_TIERS] = { 25, 50, 100 };      // probabilidad de un objeto
static const uint8_t TRV_SOUV_PCT[TRV_TIERS] = { 40, 75, 100 };      // probabilidad de un recuerdo
#define TRV_DUP_COINS 10          // un recuerdo repetido se cambia por monedas
// peso de cada objeto que puede traer (suman 100), en el orden de TRV_POOL_ID (travel.h): baya dorada, tarta,
// proteina, hierro, carburante, caramelo raro, revivir, Poke Ball, (v3.2) una baya de color (cual, al azar de la
// semilla) y (v3.14) el Cebo
#define TRV_POOL_N 10             // (estructura)
static const uint8_t TRV_POOL_W[TRV_POOL_N] = { 20, 10, 9, 9, 9, 9, 5, 9, 10, 10 };
// peso (%) de recuerdo comun / poco comun / raro por duracion: 1 h sin raros; 3 h pocos; 8 h muchos
static const uint8_t TRV_RARITY_W[TRV_TIERS][3] = { { 70, 30, 0 }, { 60, 32, 8 }, { 40, 35, 25 } };

// --------------------------------------------------------------------------------------------
// Medallas del jugador (v3.4, medals.h): 19 medallas, cada una con Bronce / Plata / Oro
// --------------------------------------------------------------------------------------------
// Cada tramo paga sus monedas una sola vez (fuera del tope diario). Los umbrales van en el orden de
// MD_* de medals.h (ese orden es del guardado "pm": no se cambia aqui, solo los numeros).
static const uint16_t MEDAL_COINS[3] = { 25, 75, 200 };  // Bronce, Plata, Oro
// lo que cuenta un bicho para las medallas de bicho (cada uno, una vez)
static const uint16_t MEDAL_PET_LV[3] = { 10, 25, 50 };  // Creciendo, Veterano, Gran criador: el nivel
#define MEDAL_BOND 100            // Mejores amigos: el vinculo
#define MEDAL_FIT_LV 10           // En forma: desde este nivel...
#define MEDAL_FIT_WEIGHT 0        // ...con este peso como mucho...
#define MEDAL_FIT_MISTAKES 0      // ...y estos descuidos como mucho
static const uint32_t MEDAL_T[][3] = {
  { 2, 6, 15 },             // Creciendo: bichos que llegan al Nv.10
  { 1, 4, 10 },             // Veterano: al Nv.25
  { 1, 4, 8 },              // Gran criador: al Nv.50
  { 1, 3, 8 },              // Mejores amigos: con el vinculo a 100
  { 2, 5, 12 },             // Adulto: en una forma final
  { 1, 3, 8 },              // En forma: Nv.10, peso 0 y sin descuidos
  { 2, 6, 15 },             // Gourmet: baya favorita descubierta
  { 3, 10, 30 },            // Evolucion: evoluciones
  { 1, 3, 8 },              // Feliz adios: despedidas
  { 15, 50, 120 },          // Pokedex: especies registradas
  { 1, 3, 8 },              // Shiny: especies shiny registradas
  { 7, 30, 100 },           // Cuidado diario: la mejor racha (dias)
  { 50000, 250000, 1000000 },  // Caminante: pasos en total
  { 1, 10, 30 },            // Maraton: dias de 10000 pasos
  { 3, 6, 8 },              // Campeon: estadios vencidos
  { 3, 15, 40 },            // Domador: salvajes vencidos
  { 1, 5, 15 },             // Cazador: capturas
  { 3, 15, 50 },            // Trotamundos: viajes completados
  { 5, 20, 45 },            // Coleccionista: recuerdos (el Oro, todos: SOUV_COUNT)
};
