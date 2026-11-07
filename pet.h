#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "growth.h"  // ritmo de los niveles (v1.35): cuanto cuesta cada uno
#include "box.h"     // la caja (v1.37): el registro de un bicho guardado

// 1 tick = 1 minuto de juego. Baja este valor para probar mas rapido
// (p. ej. 5000UL = las estadisticas caen 12x mas rapido).
#define PET_TICK_MS 60000UL
// Los niveles ya no son las horas de edad (v1.35): ver growth.h. Con la curva nueva
// CHARMANDER llega al nivel 16 a las ~8 h de juego con cuidado perfecto.
#define EAT_ANIM_MS 2500UL
#define HEART_MS 1500UL
#define EVOLVE_ANIM_MS 5200UL              // animacion de evolucion (mas larga = mas epica)
#define CEREMONY_MS 10000UL                // duracion de la despedida en pantalla
#define FAREWELL_AGE_MIN (3UL * 24 * 60)   // se despide a los 3 dias de juego (en forma final)
#define RUNAWAY_TICKS 60                   // se escapa tras 1 h con TODO a cero

// --- ritmo del peso y del vinculo (v1.22). El peso es grasa de mas: 0 = en forma.
// Antes se quemaba -1 cada 3 min (20 a la hora): una chuche (+12) desaparecia en
// 36 min y el peso se quedaba en 0 todo el dia. Ahora una chuche dura ~3 h despierto.
#define WEIGHT_BURN_AWAKE_MIN 15           // -1 de peso cada 15 min despierto
#define WEIGHT_BURN_SLEEP_MIN 30           // -1 cada 30 min durmiendo
#define GAME_WEIGHT_DIV 2                  // minijuego: -puntos/2 de peso
#define TRAIN_WEIGHT_DIV 10                // saco: -golpes/10 de peso
// El vinculo maximo (100) debe costar 5-8 dias: 10 al dia por cuidados (tope de
// addBond) + 4 del primer cuidado del dia = 14, el dia 8 cuidandolo al maximo.
#define BOND_DAY_CAP 10                    // tope diario de addBond (era 20)
#define BOND_CARE_DAY 4                    // primer cuidado del dia, fuera del tope
#define PET_BOND_COOLDOWN_MS (15UL * 60 * 1000)  // mimar da vinculo como mucho cada 15 min

// milisegundos que faltan hasta `deadline` (0 si ya paso, o si deadline==0:
// "temporizador inactivo"). Todos los temporizadores del juego (dialogos,
// animaciones) se guardan como un instante absoluto "deadline = millis() +
// duracion" y se comparaban con millis() < deadline / millis() > deadline;
// eso se rompe cuando millis() da la vuelta a los ~49,7 dias de uptime. La
// resta sin signo, reinterpretada como con signo, es segura frente a esa
// vuelta mientras el intervalo real sea muy inferior a ~24,8 dias (aqui el
// temporizador mas largo son ~12 s), sin cambiar como se guarda el deadline.
static inline uint32_t timeLeft(uint32_t deadline) {
  if (!deadline) return 0;
  int32_t left = (int32_t)(deadline - millis());
  return left > 0 ? (uint32_t)left : 0;
}

// ceremonias de fin de ciclo
enum : uint8_t { CER_NONE = 0, CER_FAREWELL, CER_RUNAWAY, CER_RELEASE };

enum PetMood : uint8_t { MOOD_HAPPY, MOOD_SAD, MOOD_EATING, MOOD_SLEEPING };

// --- pasos (docs/FEATURES.md, Tarea 1). Valores de partida, para afinar jugando.
// metas diarias de pasos (v1.25): 3000, 6000 y 10000, cada una una vez al dia y en orden.
// La de 10000 no es para todos los dias: premia un paseo largo.
#define STEP_GOAL_TIERS 3
static const uint32_t STEP_GOALS[STEP_GOAL_TIERS] = { 3000, 6000, 10000 };
// niveles de cada meta (v1.36, docs/GAMEPLAY_TUNING.md #6): pasear acelera la crianza. Hasta 4
// al dia, con el bicho despierto (como el resto de premios de caminar). La de 10000 daba un
// Caramelo Raro (+1); ahora da +2 directamente y ya no da caramelo.
static const uint8_t STEP_GOAL_LEVELS[STEP_GOAL_TIERS] = { 1, 1, 2 };
#define STEP_GOAL 3000            // la primera meta (la de la racha), = STEP_GOALS[0]
#define GOAL2_JOY 10              // premio de la meta de 6000...
#define GOAL2_BOND 2              // ...con vinculo fuera del tope diario
#define GOAL3_JOY 15              // premio de la de 10000: +2 niveles y medalla MARATON
#define GOAL3_BOND 3
#define RARE_CANDY_LEVELS 1       // Caramelo Raro: +1 nivel (addLevels: la barra conserva lo que llevaba)
#define STEPS_PER_JOY 100         // +1 felicidad cada 100 pasos...
#define STEP_JOY_DAY_CAP 30       // ...como mucho +30 al dia
#define STEPS_PER_WEIGHT 1000     // -1 de peso cada 1000 pasos (v1.22: eran 250, 3000 pasos quemaban una chuche)
#define STEPS_PER_BOND 1000       // +1 de vinculo cada 1000 (con el tope diario de addBond)
#define GOAL_JOY 10               // premio de la meta
#define FIND_MIN_STEPS 800        // pasos premiados entre dos hallazgos (al azar entre MIN y MAX)
#define FIND_MAX_STEPS 1200
#define FINDS_PER_DAY 5           // tope de hallazgos al azar al dia (los regalos de las metas van aparte)
#define ODDS_BERRY 55             // probabilidad de cada objeto (%)
#define ODDS_VITAMIN 35
#define ODDS_CHARM 10
#define ITEM_MAX 9                // tope de cada objeto en la mochila
#define GOLDEN_FOOD 50            // baya dorada
#define GOLDEN_JOY 15
#define VITAMIN_TRAIN 3           // vitaminas: +3 de entrenamiento
#define CHARM_SHINY_MULT 3        // amuleto: el siguiente huevo, x3 de probabilidad shiny
#define FOUND_QUEUE (FINDS_PER_DAY + STEP_GOAL_TIERS)  // hallazgos que esperan a que se encienda la pantalla: los de un dia entero (8)
// huevos (v1.39): ya no son gratis. Solo se encuentran caminando, uno cada EGG_MIN..EGG_MAX pasos
// sorteados (media 11500): quien camina 5000 pasos al dia encuentra ~3 a la semana (35000 / 11500),
// nunca menos de 2 ni mas de 4-5. Cuentan TODOS los pasos (dormido, con un huevo, con la casa
// vacia), aparte de los hallazgos y su tope diario. Van a la mochila (IT_EGG, hasta ITEM_MAX) y
// pasan a casa en cuanto queda libre: una despedida, escapada o liberacion, o guardar al bicho en la
// caja. Sin ninguno, la casa se queda vacia hasta encontrar uno (o despertar a alguien de la caja).
#define EGG_MIN_STEPS 8000
#define EGG_MAX_STEPS 15000

// objetos de la mochila (del jugador). El amuleto no se usa a mano: se lleva.
// IT_RARECANDY y IT_EGG van al final: la mochila se guarda por posicion ("bpack") y un guardado
// anterior trae solo las 5 (o 6) primeras. El huevo (v1.39) tampoco se usa a mano: va a casa solo.
enum : uint8_t { IT_GOLDEN = 0, IT_PROTEIN, IT_IRON, IT_CARBOS, IT_CHARM, IT_RARECANDY, IT_EGG, IT_COUNT };

// --- trucos del IMU (Tarea 2)
#define GRUMPY_JOY 5                 // despertarlo agitando: -5 felicidad...
#define GRUMPY_COOLDOWN_MS 60000UL   // ...y otro despertar en 60 s no resta mas
#define EMOTE_MS 3000UL              // enfado, mareo, huevo que se tambalea
#define DIZZY_SHAKES 3               // 3 sacudidas en 2 minutos...
#define DIZZY_WINDOW_MS 120000UL
#define DIZZY_JOY 3                  // ...restan 3 de felicidad...
#define DIZZY_COOLDOWN_MS 600000UL   // ...como mucho cada 10 minutos

enum : uint8_t { EMOTE_NONE = 0, EMOTE_ANGRY, EMOTE_DIZZY, EMOTE_WOBBLE };

// --- combate de practica (Tarea 3a)
#define PRACTICE_JOY 3                 // premio del combate de practica...
#define PRACTICE_EVERY_MS 3600000UL    // ...como mucho una vez por hora

// medallas del individuo (bitmask)
enum : uint16_t {
  MED_LV10 = 1 << 0, MED_LV25 = 1 << 1, MED_LV50 = 1 << 2,
  MED_BERRY = 1 << 3, MED_STREAK7 = 1 << 4, MED_BOND = 1 << 5,
  MED_FINAL = 1 << 6, MED_FIT = 1 << 7,
  MED_MARATHON = 1 << 8,  // 10000 pasos en un dia, con el bicho despierto (v1.25)
};
#define MED_COUNT 9

class Pet {
public:
  // Estadisticas 0..100
  uint8_t fullness = 80;  // comida
  uint8_t joy = 80;       // felicidad
  uint8_t energy = 80;    // energia
  uint8_t hygiene = 100;  // limpieza
  uint8_t poops = 0;      // cacas en pantalla (max 3)
  uint8_t weight = 0;     // 0-100: las chuches engordan, el minijuego quema
  // genes (90-110%, se tiran al eclosionar) y entrenamiento (0-100)
  uint8_t geneAtk = 100, geneDef = 100, geneSpe = 100;
  uint8_t trAtk = 0, trDef = 0, trSpe = 0;
  bool berryKnown = false;  // ya descubrio su baya favorita
  bool shiny = false;       // variante de color rara (se sortea en el huevo)
  uint32_t ageMinutes = 0;     // edad real en minutos (huevo incluido): dias, despedida
  // minutos de crecimiento (v1.35, growth.h): de ellos sale el nivel. Avanzan con la edad
  // desde que nace, y los saltan el Caramelo Raro y las metas de pasos sin envejecerlo.
  uint32_t growMin = 0;
  int16_t speciesId = -1;      // numero de Pokedex (1-151), -1 = huevo (o casa vacia, ver vacant)
  // casa vacia (v1.39): ni bicho ni huevo, hasta encontrar un huevo caminando o despertar a uno de
  // la caja. Va con speciesId = -1, asi que isEgg() tambien es true: todo lo que exige un bicho
  // nacido ya lo descarta. Lo propio del huevo (eclosionar, grietas, tambalearse) mira vacant.
  bool vacant = false;
  int16_t prevSpeciesId = -1;  // para la animacion de evolucion
  uint8_t careMistakes = 0;   // descuidos: cada uno retrasa la evolucion 1 nivel
  bool sleeping = false;
  uint32_t lastSeenEpoch = 0;   // ultima hora RTC vista (para progresion offline)
  uint8_t ceremony = CER_NONE;  // despedida/escapada/liberacion en curso
  uint8_t lastEnd = CER_NONE;   // como acabo la anterior (afecta al huevo)
  uint8_t dexReg[19] = { 0 };       // pokedex de criados (bitmap 151 bits)
  uint8_t dexShinyReg[19] = { 0 };  // criados en version shiny
  // racha de cuidado diario (del jugador: persiste entre crianzas)
  uint16_t streak = 0, bestStreak = 0;
  uint32_t lastCareDay = 0;
  // vinculo (del bicho: sube lento con cuidado, se resetea al nacer otro)
  uint8_t bond = 0;
  char nick[12] = "";    // apodo (vacio = nombre de especie)
  // medallas: del individuo + contador acumulado entre todas las crianzas
  uint16_t medals = 0, totalMedals = 0;
  uint16_t newMedal = 0;   // recien conseguida(s), sin celebrar aun (takeMedal)
  uint16_t lastMilestone = 0;  // hito de racha ya celebrado
  uint16_t gameHi = 0;     // record del minijuego (del jugador)
  uint16_t strHi = 0;      // record de golpes al saco
  uint16_t zenHi = 0;      // record de la cascada: decimas de segundo en la franja (v1.43)
  uint16_t dashHi = 0;     // record de la carrera: filas esquivadas (v1.44)
  // pasos (del jugador: sobreviven a un huevo nuevo y a un reinicio)
  uint32_t stepsToday = 0;   // desde la medianoche local (el mismo dia que la racha)
  uint32_t stepsTotal = 0;   // de toda la vida
  uint32_t stepDay = 0;      // dia al que pertenecen stepsToday y los topes diarios
  uint8_t findsToday = 0;    // hallazgos de hoy, al azar y regalos de las metas
  uint8_t bonusFindsToday = 0;  // de ellos, regalos de las metas (fuera de FINDS_PER_DAY)
  uint8_t stepJoyToday = 0;  // felicidad ganada hoy caminando (STEP_JOY_DAY_CAP)
  uint32_t goalDay = 0;      // ultimo dia en que se alcanzo una meta
  uint8_t goalTier = 0;      // metas alcanzadas ese dia (0..STEP_GOAL_TIERS)
  uint8_t items[IT_COUNT] = { 0 };  // mochila; items[IT_CHARM] es 0 o 1
  // identidad del individuo (v1.37): se sortea al nacer, nunca 0; el huevo tiene 0. Con ella se
  // sabe, tras un corte de luz a mitad de un movimiento de la caja, quien es quien (y la podra
  // usar la lista de amigos del Link, docs/FEATURES.md FR-3.11)
  uint32_t uid = 0;

  void begin();                 // carga estado de NVS (o crea el primer huevo)
  void update(uint32_t nowMs);  // llamar en cada loop()

  // Acciones (botones tactiles)
  void feed();              // baya roja (compatibilidad)
  void feedBerry(uint8_t color);  // 0 roja, 1 azul, 2 verde
  void feedCandy();
  bool lovesBerry(uint8_t color) const {
    return !isEgg() && (speciesId % 3) == color;  // gusto oculto por especie
  }
  void playResult(uint8_t score);  // recompensa del minijuego (entrena VEL)
  uint8_t trainStrength(uint16_t hits);  // saco de entrenamiento (entrena FUE)
  uint8_t zenResult(uint16_t focusTenths);  // cascada completa (entrena DEF y lava), v1.43
  uint8_t dashResult(uint16_t dodged);      // carrera en trineo (entrena VEL), v1.44

  // stats de combate: base real de gen 1 x genes + nivel + entrenamiento
  uint16_t atkStat() const;
  uint16_t defStat() const;
  uint16_t speStat() const;
  void play();
  void toggleLight();  // dormir / despertar
  void clean();
  void caress();  // tocar al bicho
  void eggTap();  // tocar el huevo: 3 toques y eclosiona
  // un huevo en casa: el que esperaba en la incubadora, o uno nuevo (que se sortea ahora). No mira
  // la mochila: quien libera la casa usa freeHome(). saveNow=false solo marca el guardado (pasos)
  void newEgg(bool saveNow = true);
  void release();  // soltar (pulsacion larga + confirmar)
  void syncClock(uint32_t nowEpoch);  // aplica el tiempo transcurrido apagado
  void setClock(uint32_t nowEpoch);   // fija la hora sin aplicar progresion
  void startFarewell();  // tambien usable desde la consola serie (BYE)
  void startRunaway();   // tambien usable desde la consola serie (RUN)

  // sin bicho nacido: un huevo O la casa vacia (v1.39; isVacant() distingue)
  bool isEgg() const { return speciesId < 0; }
  bool isVacant() const { return vacant; }
  // huevos limitados (v1.40): los tuyos son los de la mochila mas el que espera en la incubadora
  uint8_t eggsOwned() const { return items[IT_EGG] + (eggHeldFlag ? 1 : 0); }
  bool boxHasPets() const;  // algun hueco con un bicho que se pueda despertar
  // al soltar sin huevos: false = solo la casa vacia, true = ademas un huevo de regalo a la mochila
  bool releaseGivesEgg() const { return !eggsOwned() && !boxHasPets(); }
  bool hatchFromBag();  // casa vacia: el boton "Abrir un huevo" (uno de la mochila a casa)
  uint8_t eggCracks() const { return eggTaps; }
  bool eating() const { return timeLeft(eatUntil) > 0; }
  bool showHeart() const { return timeLeft(heartUntil) > 0; }
  bool evolving() const { return timeLeft(evolveUntil) > 0; }
  float evolveT() const {     // progreso de la animacion de evolucion 0..1
    return 1.0f - (float)timeLeft(evolveUntil) / (float)EVOLVE_ANIM_MS;
  }
  bool canEvolveNow() const;  // condiciones de evolucion cumplidas (lista)
  void evolve();              // dispara la transformacion (la llama un toque del usuario)
  bool canFarewellNow() const;  // forma final + 7 dias: lista para despedirse (boton)
  bool canRunawayNow() const;   // abandono total 1h: lista para escaparse (boton triste)
  // el usuario decide en un dialogo; "mantener/quedaros" pospone y re-ofrece luego
  bool wantEvolveButton() const { return canEvolveNow() && level() > evoDeclinedLv; }
  bool wantFarewellButton() const { return canFarewellNow() && ageMinutes >= farDeclinedAge; }
  void declineEvolve() { evoDeclinedLv = level(); }              // re-ofrece al subir de nivel
  void declineFarewell() { farDeclinedAge = ageMinutes + 1440; } // re-ofrece dentro de 1 dia
  // primera partida: el jugador elige inicial (Bulbasaur/Charmander/Squirtle)
  bool awaitingStarter() const { return starterPick; }
  void chooseStarter(int16_t dex) { eggTarget = dex; starterPick = false; save(); }
  void factoryReset() { prefs.clear(); }  // borra la NVS (test: comando serie WIPE)
  void dbgSetAge(uint32_t m) { ageMinutes = m; save(); }  // serie AGE (pruebas)
  void dbgRunawayReady() { fullness = joy = energy = hygiene = 0; neglectTicks = RUNAWAY_TICKS; }  // test
  // uint16_t, no uint8_t: un uint8_t daba la vuelta a 0 en el nivel 256, y eso se
  // alcanza jugando, porque "quedaros juntos" permite posponer la despedida
  // indefinidamente. Se topa en LEVEL_MAX (999) en vez de desbordar.
  uint16_t level() const { return levelFor(growMin); }
  uint16_t levelMinutesInto() const { return levelInto(growMin); }      // de levelCost(level())
  uint16_t levelMinutesCost() const { return levelCost(level()); }
  void setLevel(uint16_t lv) { growMin = levelStart(lv); }  // comando LVL y pruebas
  bool isRegistered(int16_t dex) const {
    return dex >= 1 && dex <= 151 && (dexReg[(dex - 1) >> 3] & (1 << ((dex - 1) & 7)));
  }
  bool isShinyRegistered(int16_t dex) const {
    return dex >= 1 && dex <= 151 && (dexShinyReg[(dex - 1) >> 3] & (1 << ((dex - 1) & 7)));
  }
  uint16_t registeredCount() const;
  bool lineHasUnregistered(int16_t base) const;
  uint8_t eggRarity() const;       // rareza del huevo actual (sin revelar especie)
  int16_t pickEggSpecies();        // publica para poder simular tiradas (EGGS)
  uint8_t lowestStat() const { return min(min(fullness, joy), min(energy, hygiene)); }
  PetMood mood() const;
  // progreso de la ceremonia de despedida/escapada, 0..1 (para animarla)
  float ceremonyT() const {
    if (ceremony == CER_NONE) return 0.0f;
    return 1.0f - (float)timeLeft(ceremonyUntil) / (float)CEREMONY_MS;
  }

  // racha / vinculo / medallas / nombre
  void rename(const char *name);
  bool hasMedal(uint16_t m) const { return medals & m; }
  uint8_t bondGainedToday() const { return bondToday; }  // diagnostico (STATS)
  // celebraciones pendientes (v1.30, fx.h): las recoge la UI, que decide cuando se ven y
  // cuanto duran. Antes el bicho fijaba el plazo al ganarlas y caducaban sin verse con la
  // pantalla apagada. takeMedal = bits de las medallas nuevas; takeMilestone = dias del hito.
  uint16_t takeMedal() { uint16_t m = newMedal; newMedal = 0; return m; }
  uint16_t takeMilestone() { uint16_t d = milestoneNew; milestoneNew = 0; return d; }
  // ATK (0), DEF (1) o VEL (2) a un nivel dado: la subida de nivel ensena lo que gano
  uint16_t statAt(uint8_t which, uint16_t lv) const;
  int careBonus() const;  // mejora del huevo por racha + vinculo
  int shinyOdds(bool charm) const;  // denominador de la tirada shiny del huevo

  // --- pasos y mochila (Tarea 1). addSteps() nunca escribe en flash: marca el
  // guardado pendiente y el loop lo vuelca con la pantalla atenuada.
  void addSteps(uint32_t n, uint32_t day);  // n pasos contados; day = dayNumber() (0 = sin reloj)
  uint32_t dayNumber() const { return today(); }
  uint32_t stepsOn(uint32_t day) const { return day == stepDay ? stepsToday : 0; }  // "hoy" visto desde day
  uint8_t goalTierOn(uint32_t day) const { return day && goalDay == day ? goalTier : 0; }
  bool goalDoneOn(uint32_t day) const { return goalTierOn(day) > 0; }  // la primera (3000)
  uint8_t randomFindsToday() const { return findsToday - bonusFindsToday; }
  // huevos (v1.39): pasos que faltan para el siguiente (0 = aun sin sortear) y el sorteo
  uint32_t eggStepsLeft() const { return eggAt > eggRem ? eggAt - eggRem : 0; }
  uint16_t eggStepsAt() const { return eggAt; }
  // EGG <n> por serie (pruebas): n huevos en la mochila; con la casa vacia, uno pasa a casa
  void setBagEggs(uint8_t n);
  // efecto de usar un objeto de la mochila (false = no se pudo: dormido, huevo, no hay)
  bool canUseItem(uint8_t it) const;
  bool useItem(uint8_t it);
  uint8_t itemCount() const;  // objetos en la mochila, amuleto incluido
  // hallazgos pendientes de mostrar (cola en RAM, se pierde al reiniciar; el
  // objeto ya esta en la mochila). full = la mochila estaba llena.
  bool hasFound() const { return foundN > 0; }
  // gift = un regalo (v1.40: el huevo de la despedida o el que llega sin huevos ni caja)
  uint8_t peekFound(bool *full = nullptr, bool *gift = nullptr) const {
    if (full) *full = foundN && foundFull[0];
    if (gift) *gift = foundN && foundGift[0];
    return foundN ? foundQ[0] : (uint8_t)IT_COUNT;
  }
  void popFound();
  // banner de la meta pendiente de mostrar (lo recoge la UI con la pantalla encendida)
  // devuelve la meta (1..STEP_GOAL_TIERS) del banner, la mas alta si se cruzaron varias; 0 = ninguno
  // levels (opcional): niveles que dieron las metas desde el ultimo banner (0 si se cruzaron
  // dormido); el banner los ensena como "+N Lv"
  uint8_t takeGoalBanner(uint8_t *levels = nullptr) {
    uint8_t g = goalBanner;
    if (levels) *levels = goalBannerLv;
    goalBanner = 0;
    goalBannerLv = 0;
    return g;
  }

  // --- combate de practica (Tarea 3a)
  // puede combatir: nacido, despierto, sin ceremonia ni evolucion en curso
  bool canBattle() const { return !isEgg() && !sleeping && ceremony == CER_NONE && !evolving() && !starterPick; }
  // fin de un combate de practica (gane quien gane): +PRACTICE_JOY de felicidad
  // como mucho una vez por hora. Devuelve la felicidad dada (0 si no tocaba).
  // No cuenta en ningun historial de victorias.
  uint8_t practiceDone();
  bool practiceRewardReady() const;  // el proximo combate de practica dara premio

  // --- trucos del IMU (Tarea 2): sin guardado inmediato
  void wakeGrumpy();     // agitar con el bicho dormido: se despierta de mal humor
  void shaken();         // agitar con el bicho despierto: se marea (o el huevo se tambalea)
  void sleepDeferred();  // boca abajo de noche: a dormir (toggleLight sin guardar al momento)
  uint8_t emote() const { return timeLeft(emoteUntil) ? emoteKind : (uint8_t)EMOTE_NONE; }
  float emoteT() const { return 1.0f - (float)timeLeft(emoteUntil) / (float)EMOTE_MS; }
  void clearEmote() { emoteUntil = 0; }  // (renderizador del PC: cada escena empieza limpia)

  // guardado periodico diferido: tick() marca pendiente y el loop lo vuelca
  // cuando la pantalla esta atenuada/apagada (la escritura a flash congela
  // ~1s ambos cores: asi no se ve ni corta el tactil)
  bool savePending() const { return pendingSave; }
  // ultima hora real persistida; sirve para resembrar un RTC que perdio la hora
  uint32_t savedEpoch() { return prefs.getUInt("seen", 0); }
  void flushSave();

  // --- la caja (docs/NEXT_FEATURES.md, Tarea BOX, v1.37): BOX_SLOTS huecos en la NVS (box0..5).
  // Guardar y despertar escriben en flash al momento (es un toque con la pantalla encendida,
  // como comer). Ningun orden de corte de luz pierde un bicho: el siguiente arranque termina o
  // deshace el movimiento (boxBoot).
  uint8_t boxState(uint8_t k) const { return k < BOX_SLOTS ? boxSt[k] : (uint8_t)BOX_EMPTY; }
  const PetRecord &boxRecord(uint8_t k) const { return boxRec[k < BOX_SLOTS ? k : 0]; }  // solo si BOX_OK
  uint8_t boxUsed() const;   // huecos ocupados (danados incluidos)
  // se puede mover algo: sin despedida, evolucion ni eleccion de inicial
  bool canBoxMove() const { return ceremony == CER_NONE && !evolving() && !starterPick; }
  bool canStore(uint8_t k) const { return boxState(k) == BOX_EMPTY && k < BOX_SLOTS && !isEgg() && canBoxMove(); }
  bool canWake(uint8_t k) const { return boxState(k) == BOX_OK && canBoxMove(); }
  // guarda al bicho en el hueco k (vacio) y deja en su sitio el huevo guardado, o uno nuevo
  bool store(uint8_t k);
  // despierta al del hueco k. Con un bicho nacido, los dos se cambian el sitio; con un huevo,
  // el huevo se queda tal cual (especie, tirada shiny y grietas) y vuelve tras el proximo
  // guardado o final
  bool wake(uint8_t k);
  bool boxClear(uint8_t k);                // BOXCLR (tambien un hueco danado)
  bool boxPut(uint8_t k, PetRecord r);     // BOXFILL: un bicho inventado en un hueco vacio (uid nuevo)
  void boxReload() { for (uint8_t k = 0; k < BOX_SLOTS; k++) readSlot(k); }  // relee la NVS (renderizador)
  bool eggHeld() const { return eggHeldFlag; }  // hay un huevo esperando (FR-BOX.5)
  int16_t eggSpecies() const { return eggTarget; }  // la incubadora (serie BOX y pruebas)
  bool eggIsShiny() const { return eggShiny; }
  PetRecord toRecord() const;              // el bicho activo como registro sellado (storedEpoch = ahora)
  void fromRecord(const PetRecord &r);     // pasa a ser el bicho de r (solo lo del individuo)

private:
  Preferences prefs;
  uint32_t lastTick = 0;
  uint32_t eatUntil = 0;
  uint32_t heartUntil = 0;
  uint32_t petBondUntil = 0;   // enfriamiento del vinculo por mimos (solo RAM)
  uint32_t evolveUntil = 0;
  int16_t eggTarget = 1;       // dex oculto que saldra del huevo
  bool eggShiny = false;       // sorpresa sorteada al crear el huevo
  uint8_t eggTaps = 0;
  uint8_t mistakeCooldown = 0;
  uint8_t ticksSinceSave = 0;
  bool pendingSave = false;     // guardado periodico pendiente de volcar
  uint16_t evoDeclinedLv = 0;   // "mantener forma": no ofrecer evolucion hasta subir de nivel
  uint32_t farDeclinedAge = 0;  // "quedaros juntos": no ofrecer despedida hasta esta edad
  bool starterPick = false;     // primera partida: esperando que el jugador elija inicial
  uint8_t neglectTicks = 0;
  uint16_t goodTicks = 0;  // racha bien cuidado: forja la DEF
  uint32_t ceremonyUntil = 0;
  uint8_t bondToday = 0;       // tope diario de subida de vinculo
  uint32_t bondDay = 0;        // dia de bondToday: el tope vuelve a 0 a medianoche
  uint16_t milestoneNew = 0;   // hito de racha sin celebrar (dias; takeMilestone)
  // pasos: restos hacia el siguiente punto de cada premio (nada se pierde entre lecturas)
  uint16_t joyRem = 0, weightRem = 0, bondRem = 0;
  uint16_t findRem = 0;        // pasos premiados hacia el siguiente hallazgo
  uint16_t findAt = 0;         // ...y a cuantos toca (FIND_MIN..FIND_MAX; 0 = sin sortear)
  uint8_t foundQ[FOUND_QUEUE];
  bool foundFull[FOUND_QUEUE];
  bool foundGift[FOUND_QUEUE];
  uint8_t foundN = 0;
  uint16_t eggRem = 0;         // pasos hacia el siguiente huevo (v1.39)...
  uint16_t eggAt = 0;          // ...y a cuantos toca (EGG_MIN..EGG_MAX; 0 = sin sortear)
  uint8_t goalBanner = 0;     // meta del banner pendiente (0 = ninguno)
  uint8_t goalBannerLv = 0;   // niveles ganados con las metas desde el ultimo banner
  // trucos del IMU
  uint8_t emoteKind = EMOTE_NONE;
  uint32_t emoteUntil = 0;
  uint32_t lastGrumpy = 0;     // millis del ultimo despertar que resto felicidad...
  bool grumpied = false;       // ...si lo hubo
  uint32_t shakeAt[DIZZY_SHAKES] = { 0 };  // ultimas sacudidas con el bicho despierto
  uint8_t shakeN = 0;
  uint32_t lastDizzyPenalty = 0;
  bool dizzyPenalized = false;
  // combate de practica
  uint32_t lastPracticeReward = 0;  // millis del ultimo premio...
  bool practiceRewarded = false;    // ...si lo hubo

  uint32_t today() const { return lastSeenEpoch ? lastSeenEpoch / 86400 : 0; }
  void registerCare(bool saveNow = true);   // primer cuidado del dia: racha + vinculo
  void addBond(uint8_t amt);
  void checkMedals(bool saveNow = true);
  void persist(bool now) { if (now) save(); else pendingSave = true; }
  void rollStepDay(uint32_t day);
  void findItem();
  void giveFound(uint8_t it, bool gift = false);  // a la mochila y a la cola de avisos
  void queueFound(uint8_t it, bool full, bool gift);  // solo el aviso
  void goalReward(uint8_t tier, bool rewarded);
  uint16_t pendingMedals = 0;      // medallas ganadas por un evento (MARATON) que checkMedals celebra
  void startEmote(uint8_t kind) { emoteKind = kind; emoteUntil = millis() + EMOTE_MS; }
  void tick();
  void hatch();
  void resetHome();                 // lo comun de un huevo nuevo y de la casa vacia
  void freeHome();                  // guardar en la caja: huevo que esperaba, de la mochila, o vacia
  void endLife(uint8_t cer);        // fin de una crianza (v1.41): la casa, vacia; los huevos, a la mochila
  void vacate(bool saveNow = true); // la casa, vacia (v1.39)
  void eggToHome(bool saveNow);     // con la casa vacia: un huevo de la mochila pasa a casa
  // caja (v1.37)
  PetRecord boxRec[BOX_SLOTS];      // copia en RAM de cada hueco (la UI la dibuja sin leer flash)
  uint8_t boxSt[BOX_SLOTS] = { 0 };
  bool eggHeldFlag = false;         // "eggH": el huevo de la incubadora espera a que vuelva su turno
  void boxBoot();                   // al arrancar: lee la caja y arregla un movimiento cortado
  void readSlot(uint8_t k);
  bool writeSlot(uint8_t k, PetRecord r);
  bool clearSlot(uint8_t k);
  int8_t slotOf(uint32_t id) const; // hueco (BOX_OK) con ese uid, o -1
  uint32_t newUid() const;          // al azar, distinto de 0 y de los de la caja
  void registerSpecies(int16_t dex);
  void save();
  void load();
  static uint8_t clamp100(int v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }
};
