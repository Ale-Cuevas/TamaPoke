#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "growth.h"  // ritmo de los niveles (v1.35): cuanto cuesta cada uno
#include "box.h"     // la caja (v1.37): el registro de un bicho guardado
#include "economy.h" // las monedas del patio (Y4): lo que se gana y lo que cuesta
#include "eggtimer.h" // el huevo del patio (v2.0, Y3): 2 h de tiempo real, las sacudidas lo adelantan
#include "travel.h"  // los viajes y la coleccion (v2.0, Y5): lo que trae un viaje
#include "tuning.h"  // los numeros del equilibrio (necesidades, huevos, premios...): se afinan alli
#include "petlife.h" // v3.0: la vida de cada bicho de casa (dos huecos); Pet es el jugador + el del foco
#include "homerec.h" // v3.0: el segundo hueco de casa en la NVS ("hm1")

// Los niveles ya no son las horas de edad (v1.35): ver growth.h. Con la curva nueva
// CHARMANDER llega al nivel 16 a las ~8 h de juego con cuidado perfecto.
#define EAT_ANIM_MS 2500UL
#define HEART_MS 1500UL
// El abandono, la muerte y el Revivir (v1.60, v1.64), la noche (v1.63), las cacas, el peso y el vinculo:
// sus numeros estan en tuning.h.

// (timeLeft, CER_*, PetMood y EMOTE_* estan en petlife.h)

// --- pasos (docs/FEATURES.md, Tarea 1): metas, hallazgos y sus premios en tuning.h
#define FOUND_QUEUE (FINDS_PER_DAY + STEP_GOAL_TIERS)  // hallazgos que esperan a que se encienda la pantalla: los de un dia entero (8)
// huevos (v1.39): ya no son gratis. Solo se encuentran caminando, uno cada EGG_MIN..EGG_MAX pasos
// sorteados (media 11500): quien camina 5000 pasos al dia encuentra ~3 a la semana (35000 / 11500),
// nunca menos de 2 ni mas de 4-5. Cuentan TODOS los pasos (dormido, con un huevo, con la casa
// vacia), aparte de los hallazgos y su tope diario. Van a la mochila (IT_EGG, hasta ITEM_MAX) y
// pasan a casa en cuanto queda libre: una despedida, escapada o liberacion, o guardar al bicho en la
// caja. Sin ninguno, la casa se queda vacia hasta encontrar uno (o despertar a alguien de la caja).
// regalos (v1.55): lo que se encuentra caminando ya no salta en un aviso ni va derecho a la mochila.
// Llega envuelto, como una cajita en el suelo de casa, y entra en la mochila al abrirlo (un toque con
// el bicho despierto, que se alegra). Se guardan en la NVS ("gifts"), hasta GIFT_MAX sin abrir; con
// la cola llena, lo nuevo va derecho a la mochila con el aviso de siempre (nada se pierde).
#define GIFT_MAX 8  // tamano del blob "gifts" en la NVS: no es un numero de equilibrio
// (los pasos entre dos huevos, EGG_MIN_STEPS..EGG_MAX_STEPS, estan en tuning.h)

// objetos de la mochila (del jugador). El amuleto no se usa a mano: se lleva.
// IT_RARECANDY, IT_EGG, IT_CAKE e IT_REVIVE van al final: la mochila se guarda por posicion ("bpack") y
// un guardado anterior trae solo las 5 (6, 7, 8) primeras. El huevo (v1.39) tampoco se usa a mano: va a
// casa solo. La tarta (v1.54) se come desde el menu de comida o desde la mochila. El Revivir (v1.59)
// tampoco se usa a mano: se ofrece cuando el bicho se muere de abandono. La Poke Ball (v2.2) tampoco: se
// lanza al ganar a un bicho salvaje (encounter.h).
enum : uint8_t { IT_GOLDEN = 0, IT_PROTEIN, IT_IRON, IT_CARBOS, IT_CHARM, IT_RARECANDY, IT_EGG, IT_CAKE, IT_REVIVE, IT_BALL, IT_COUNT };

// --- trucos del IMU (Tarea 2): lo que restan el enfado y el mareo, en tuning.h

// --- combate: la felicidad del final de un estadio y el botin del jefe (v1.56), en tuning.h

// --- encuentros salvajes (v2.2, encounter.h). Ganar da al que peleo crecimiento, al de casa felicidad y a
// veces un objeto (nunca monedas ni Poke Balls): los numeros en tuning.h. Despues, una Poke Ball lo captura
// seguro: un huevo de su forma base a la mochila, que nace a nivel 1. Perder: se escapa.
#define CAP_EGG_MAX 4   // huevos de captura esperando en la mochila (tamano del blob "cegg" en la NVS)
// el estado del encuentro de hoy (flags de "enc")
enum : uint8_t { ENCF_SHINY = 1, ENCF_EXCL = 2, ENCF_WON = 4 };
// por que no se puede lanzar la Poke Ball
enum : uint8_t { CAP_OK = 0, CAP_NO_BALL, CAP_NO_ROOM, CAP_NONE };

// lo que dio ganar (o perder) a un bicho salvaje (encounterDone)
struct EncReward {
  bool won;
  uint8_t item;      // el objeto que solto (a la mochila), o IT_COUNT
  bool itemLost;     // no cabia en la mochila
  uint16_t lvBefore, lvAfter;  // el nivel del que peleo
  uint8_t joy;       // felicidad del de casa
};

// --- estadios: 8 estadios tematicos, cada uno con su jefe, que combate un equipo ordenado de 1 a 4
// bichos (el de casa y/o los de la caja). stage.h define los mismos valores: de ahi los #ifndef.
// Vencer al jefe da la medalla del estadio (del jugador, para siempre) y, una vez al dia por estadio,
// el premio del jefe: a cada bicho del equipo STAGE_GROW_MIN minutos de crecimiento y STAGE_BOND de
// vinculo (fuera del tope diario), y un objeto de la tabla del botin, envuelto en casa (tuning.h)
#ifndef STAGE_COUNT
#define STAGE_COUNT 8
#endif
#ifndef TEAM_MAX
#define TEAM_MAX 4
#endif
// teamSlotOf (v3.0): los de casa son TEAM_HOME0 + su hueco (0..BOX_SLOTS-1 = su hueco de la caja;
// -1 = nadie). TEAM_HOME (el de antes) es el hueco 0. No se guarda: "team" son uids
#define TEAM_HOME0 100
#define TEAM_HOME1 101
#define TEAM_HOME TEAM_HOME0
static inline bool teamIsHome(int8_t k) { return k >= TEAM_HOME0 && k < TEAM_HOME0 + HOME_SLOTS; }
static inline uint8_t teamHomeSlot(int8_t k) { return (uint8_t)(k - TEAM_HOME0); }
static_assert(STAGE_COUNT <= 8, "las medallas de estadio caben en un byte");
static_assert(TEAM_HOME0 >= BOX_SLOTS, "TEAM_HOME0 no es un hueco de la caja");

// lo que dio el final de un estadio (stageDone)
struct StageReward {
  bool paid;      // se pago el premio del jefe (vencido y aun no pagado hoy)
  bool newBadge;  // la medalla del estadio es nueva
  uint8_t item;   // el objeto del jefe (envuelto en casa), o IT_COUNT
  uint8_t joy;    // felicidad dada al bicho de casa (la del combate de practica: una vez por hora)
  uint8_t pets;   // bichos del equipo que recibieron el premio
  uint16_t coins; // monedas que entraron (50 la primera vez, 10 las repeticiones del dia; 0 si no hubo)
};

// medallas del individuo (bitmask)
enum : uint16_t {
  MED_LV10 = 1 << 0, MED_LV25 = 1 << 1, MED_LV50 = 1 << 2,
  MED_BERRY = 1 << 3, MED_STREAK7 = 1 << 4, MED_BOND = 1 << 5,
  MED_FINAL = 1 << 6, MED_FIT = 1 << 7,
  MED_MARATHON = 1 << 8,  // 10000 pasos en un dia, con el bicho despierto (v1.25)
};
#define MED_COUNT 9

// Pet (v3.0) = el jugador + el bicho de casa que tiene el FOCO. Lo heredado de PetLife (pet.fullness,
// pet.uid...) es siempre el hueco focusSlot; el otro hueco esta en `other`. focus(s) intercambia los dos
// bloques enteros. Reglas:
// - save() y load() no dependen del foco: el hueco 0 va a las claves de siempre y el 1 a "hm1".
// - Solo las entradas de arriba cambian el foco por dentro (tick, update, syncClock, addSteps, store,
//   wake, boxBoot, stageDone, encounterDone, shaken, wakeGrumpy, sleepDeferred), siempre con FocusGuard,
//   que lo deja como estaba. La UI lo cambia con focus() solo cuando no hay nada abierto (uiFocus).
// - Nada guarda una referencia a un campo de PetLife a traves de un cambio de foco.
class Pet : public PetLife {
public:
  // --- los dos huecos de casa
  PetLife other = vacantPetLife();  // el hueco que no tiene el foco (vacio hasta que se carga)
  uint8_t focusSlot = 0;      // el hueco de casa que es "pet" ahora (0 o 1)
  void focus(uint8_t s);      // intercambia el foco (no guarda nada)
  // los dos huecos sin mover el foco (el patio dibuja los dos asi)
  const PetLife &home(uint8_t s) const { return (s & 1) == focusSlot ? (const PetLife &)*this : other; }
  PetLife &slotRef(uint8_t s) { return (s & 1) == focusSlot ? (PetLife &)*this : other; }
  // f() con el foco en el hueco s, y despues el foco vuelve a donde estaba
  template <typename F> void onSlot(uint8_t s, F f) {
    FocusGuard g(*this, s);
    if (g.ok) f();  // (una reentrada a otro hueco no hace nada: nunca sobre el bicho equivocado)
  }
  // el foco en el hueco s mientras vive (RAII). Anidar uno con OTRO hueco es un error (las referencias
  // de la de fuera quedarian cambiadas): se avisa, no se cambia el foco y ok queda a false (quien lo usa
  // se salta lo suyo). En el PC, ademas, abort() (salvo focusStrict = false)
  struct FocusGuard {
    Pet &p;
    uint8_t prev;
    bool moved;
    bool ok;
    FocusGuard(Pet &pp, uint8_t s);
    ~FocusGuard();
    FocusGuard(const FocusGuard &) = delete;
    FocusGuard &operator=(const FocusGuard &) = delete;
  };
  uint8_t focusDepth = 0;     // FocusGuard activos (el aviso de reentrada)
  uint16_t focusErrors = 0;   // reentradas detectadas (pruebas y STATS)
  bool focusStrict = true;    // en el PC una reentrada para las pruebas (abort); la que lo prueba lo quita
  // cuantos huecos de casa tienen un bicho nacido / estan vacios
  uint8_t homeHatched() const { return (uint8_t)(home(0).hatched() + home(1).hatched()); }
  uint8_t homeVacant() const { return (uint8_t)(home(0).isVacant() + home(1).isVacant()); }
  int8_t vacantSlot() const { return home(0).isVacant() ? 0 : home(1).isVacant() ? 1 : -1; }  // el primero, o -1
  int8_t slotOfUid(uint32_t id) const;  // hueco de casa con ese uid (nacido), o -1
  // la casa duerme: algun bicho nacido y todo lo que no es un hueco vacio, dormido (el amplificador, el
  // autoguardado y el ritmo de dibujo; un huevo cuenta como despierto, como antes)
  bool homeAsleep() const;

  uint32_t lastSeenEpoch = 0;   // ultima hora RTC vista (para progresion offline)
  uint8_t lastEnd = CER_NONE;   // como acabo la anterior (afecta al huevo)
  uint8_t dexReg[19] = { 0 };       // pokedex de criados (bitmap 151 bits)
  uint8_t dexShinyReg[19] = { 0 };  // criados en version shiny
  // racha de cuidado diario (del jugador: persiste entre crianzas)
  uint16_t streak = 0, bestStreak = 0;
  uint32_t lastCareDay = 0;
  // medallas: las del individuo estan en PetLife; aqui el contador acumulado entre todas las crianzas
  uint16_t totalMedals = 0;
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

  void begin();                 // carga estado de NVS (o crea el primer huevo)
  void update(uint32_t nowMs);  // llamar en cada loop()

  // Acciones (botones tactiles): el bicho del foco
  void feed();              // baya roja (compatibilidad)
  void feedBerry(uint8_t color);  // 0 roja, 1 azul, 2 verde
  bool eatCake();  // una tarta de la mochila (v1.54): false si no hay o no puede comer
  void playResult(uint8_t score);  // recompensa del minijuego (entrena VEL)
  uint8_t trainStrength(uint16_t hits);  // saco de entrenamiento (entrena FUE)
  // cascada completa (entrena DEF y lava), v1.43; driftTenths: la parte con la franja moviendose,
  // que da DEF de bonificacion (v1.47)
  uint8_t zenResult(uint16_t focusTenths, uint16_t driftTenths = 0);
  uint8_t dashResult(uint16_t dodged);      // carrera en trineo (entrena VEL), v1.44

  void evolve();              // dispara la transformacion del del foco (la llama un toque del usuario)
  void play();
  void toggleLight();  // dormir / despertar
  // --- la noche (v1.63). Hora local 0-23 de lastSeenEpoch, -1 sin reloj
  static int hourAt(uint32_t epoch) { return epoch ? (int)((epoch / 3600) % 24) : -1; }
  int localHour() const { return hourAt(lastSeenEpoch); }
  // de SLEEPY_HOUR a WAKE_HOUR: tiene sueño despierto, dormido baja lento, acostarlo da vinculo
  static bool nightHour(int h) { return h >= SLEEPY_HOUR || (h >= 0 && h < WAKE_HOUR); }
  static bool bedHour(int h) { return h >= BED_HOUR || (h >= 0 && h < WAKE_HOUR); }  // se duerme solo
  // despierto con sueño: la UI pone el globo con "zzz" y "Tiene sueño..."
  bool sleepyOf(const PetLife &l) const { return !l.sleeping && !l.isEgg() && !l.busyEnding() && !starterPick && nightHour(localHour()); }
  bool sleepy() const { return sleepyOf(*this); }
  // noche de una hora: de WAKE_HOUR a WAKE_HOUR del dia siguiente (0 sin reloj)
  static uint32_t nightId(uint32_t epoch) { return epoch > WAKE_HOUR * 3600UL ? (epoch - WAKE_HOUR * 3600UL) / 86400 + 1 : 0; }
  // la UI lo pone mientras hay un juego, combate, evolucion o movimiento de la caja: no se duerme solo
  // en mitad (lo hara al acabar)
  bool nightHold = false;
  // alguno se desperto solo por la mañana con la placa encendida: la UI lo lee una vez y suena (SFX_MORNING)
  bool morningWake = false;
  void clean();
  bool cleanPoop(uint8_t slot);  // tocar una caca: false si no hay o no se puede (ceremonia, huevo)
  void caress();  // tocar al bicho
  void eggTap();  // tocar el huevo: 3 toques y eclosiona
  // un huevo en el hueco del foco: el que esperaba en su incubadora, o uno nuevo (que se sortea ahora).
  // No mira la mochila: quien libera el hueco usa freeHome(). saveNow=false solo marca el guardado (pasos)
  // heir (v1.60): la especie del huevo que deja un bicho al morir (0 = la que salga en el sorteo)
  // shinyForce (v2.2): el huevo de una captura, con su especie (heir) y su tirada shiny (0/1; -1 = sortear)
  void newEgg(bool saveNow = true, int16_t heir = 0, int8_t shinyForce = -1);
  void release();  // soltar (pulsacion larga + confirmar)
  void syncClock(uint32_t nowEpoch);  // aplica el tiempo transcurrido apagado (a los dos huecos)
  void setClock(uint32_t nowEpoch);   // fija la hora sin aplicar progresion
  void startFarewell();  // tambien usable desde la consola serie (BYE)
  // muerte por abandono (v1.60). startDying: se esta muriendo (tambien serie DIE); el tiempo se
  // para y la UI pregunta. revive gasta un Revivir; letGo empieza la ceremonia del final
  void startDying();
  bool revive();
  void letGo();
  // Revivir a mano: los de la mochila y los que aun esperan envueltos (muriendose no se abren regalos)
  uint8_t revivesOwned() const;
  // huevos limitados (v1.40): los tuyos son los de la mochila mas los que esperan en las incubadoras
  uint8_t eggsOwned() const { return items[IT_EGG] + (home(0).eggHeldFlag ? 1 : 0) + (home(1).eggHeldFlag ? 1 : 0); }
  bool boxHasPets() const;  // algun hueco con un bicho que se pueda despertar
  // al soltar sin huevos: false = solo el hueco vacio, true = ademas un huevo de regalo a la mochila
  // (v1.55: un huevo envuelto tambien cuenta; v3.0: y solo si el otro hueco de casa esta vacio)
  bool releaseGivesEgg() const { return !eggsOwned() && !giftEggs() && !boxHasPets() && other.isVacant(); }
  // regalos sin abrir (v1.55), el primero es el que se abre
  uint8_t giftCount() const { return giftN; }
  uint8_t giftItem(uint8_t i) const { return i < giftN ? gifts[i] : (uint8_t)IT_COUNT; }
  uint8_t giftEggs() const;
  bool canOpenGift() const { return giftN && !isEgg() && !sleeping && !busyEnding() && !starterPick; }
  // abre el primero: a la mochila, el bicho se alegra (corazon) y sale el aviso "REGALO!". Devuelve el
  // objeto (IT_COUNT si no se pudo abrir). *full: no cabia en la mochila y el regalo sigue ahi
  uint8_t openGift(bool *full = nullptr);
  bool addGift(uint8_t it);  // un regalo nuevo (serie GIFT y pruebas); false = cola llena
  bool hatchFromBag();  // hueco vacio: el boton "Abrir un huevo" (uno de la mochila al hueco del foco)
  uint8_t eggCracksOf(const PetLife &l) const { return yardEggOf(l) ? eggCracksFor(eggMinutesLeftOf(l)) : l.eggTaps; }
  uint8_t eggCracks() const { return eggCracksOf(*this); }
  // el huevo del patio (v2.0, Y3; eggtimer.h). La UI pone timedEggs antes de begin(): cada huevo
  // que llega a casa se pone en el patio y eclosiona solo a las 2 h reales (las sacudidas lo adelantan,
  // hasta 2 h). El inicial y uno a medias del sistema de antes (sin "yegg") siguen con los 3 toques.
  bool timedEggs = false;
  bool yardEggOf(const PetLife &l) const { return timedEggs && l.isEgg() && !l.vacant && !starterPick && l.eggPlaced; }
  bool yardEgg() const { return yardEggOf(*this); }
  uint16_t eggMinutesLeftOf(const PetLife &l) const { return ::eggMinutesLeft(l.eggPlaced, lastSeenEpoch, l.eggSaved); }
  uint16_t eggMinutesLeft() const { return eggMinutesLeft(lastSeenEpoch); }
  uint16_t eggMinutesLeft(uint32_t epoch) const { return ::eggMinutesLeft(eggPlaced, epoch, eggSaved); }
  void dbgEggElapsed(uint32_t min);  // serie EGGT <min>: como si se hubiera puesto hace min minutos
  void hatchNow();                   // serie HATCH: el huevo de casa eclosiona ya (tambien el del patio)
  // serie EGGD <dex> y renderizador (v2.4): el huevo de casa lleva a dex (para ver su color); no guarda
  void dbgEggSpecies(int16_t dex);
  // primera partida: el jugador elige inicial (Bulbasaur/Charmander/Squirtle)
  bool awaitingStarter() const { return starterPick; }
  void chooseStarter(int16_t dex) { eggTarget = dex; starterPick = false; save(); }
  void factoryReset() { prefs.clear(); }  // borra la NVS (test: comando serie WIPE)
  void dbgSetAge(uint32_t m) { ageMinutes = m; save(); }  // serie AGE (pruebas)
  void dbgClearGifts() { giftN = 0; }  // renderizador: cada escena empieza sin regalos
  void dbgAddPoop() { addPoop(); }  // serie POOP y renderizador
  void dbgDyingMin(uint8_t m) {  // serie DYING <min> (v1.64): minutos ya pasados muriendose
    if (!dying) return;
    dyingMin = m > REVIVE_LIMIT_MIN ? REVIVE_LIMIT_MIN : m;
    save();
  }
  // serie NEGLECT <min>: adelanta la cuenta. Hasta BOND_FADE_MIN, el vinculo como si llevara m minutos
  // apagandose; por encima, vinculo 0 y m - BOND_FADE_MIN minutos hacia la muerte
  void dbgNeglect(uint16_t m);
  bool isRegistered(int16_t dex) const {
    return dex >= 1 && dex <= 151 && (dexReg[(dex - 1) >> 3] & (1 << ((dex - 1) & 7)));
  }
  bool isShinyRegistered(int16_t dex) const {
    return dex >= 1 && dex <= 151 && (dexShinyReg[(dex - 1) >> 3] & (1 << ((dex - 1) & 7)));
  }
  uint16_t registeredCount() const;
  bool lineHasUnregistered(int16_t base) const;
  int16_t pickEggSpecies();        // publica para poder simular tiradas (EGGS)

  // racha / vinculo / medallas / nombre
  void rename(const char *name);
  // celebraciones pendientes (v1.30, fx.h): las recoge la UI, que decide cuando se ven y
  // cuanto duran. takeMedal = bits de las medallas nuevas (del hueco s); takeMilestone = dias del hito.
  uint16_t takeMedal(uint8_t s) { PetLife &l = slotRef(s); uint16_t m = l.newMedal; l.newMedal = 0; return m; }
  uint16_t takeMedal() { return takeMedal(focusSlot); }
  uint16_t takeMilestone() { uint16_t d = milestoneNew; milestoneNew = 0; return d; }
  bool takeDiedNow(uint8_t s) { PetLife &l = slotRef(s); bool d = l.diedNow; l.diedNow = false; return d; }
  int careBonus() const;  // mejora del huevo por racha + vinculo
  int shinyOdds(bool charm) const;  // denominador de la tirada shiny del huevo

  // --- monedas (v2.0, Y4; economy.h). Las claves de la NVS que faltan valen 0 (partidas de antes)
  uint16_t coins = 0;          // "cn": la cartera, 0..COIN_MAX
  uint16_t yardOwned = 0;      // "ybgO": bit n = mapa n comprado (del jugador, sirve para todos los bichos)
  uint8_t yardChosen = 0;      // "ybg": 0 = el inicial (YARD_MAP_START); si no, el mapa n (solo uno que se tenga)
  // suma n; capped = cuenta para el tope diario (ECO_DAILY_CAP; minijuegos y estadios repetidos), si no
  // (primeras veces, metas de pasos) entra entera. Devuelve lo que de verdad entro (tope diario y COIN_MAX)
  uint16_t addCoins(uint16_t n, bool capped);
  void setCoins(uint16_t n) { coins = n > COIN_MAX ? COIN_MAX : n; save(); }  // serie COINS (pruebas)
  bool spendCoins(uint16_t n);  // false si no hay tantas (no toca nada); guarda
  // comprar un objeto de la mochila: false si no se vende, no hay monedas o no cabe (ITEM_MAX); guarda
  bool buyItem(uint8_t itemId);
  bool ownsMap(uint8_t n) const;  // 1..YARD_MAPS: comprado, o el inicial (el prado)
  bool buyMap(uint8_t n);         // false si ya es suyo, no existe o no hay monedas; guarda
  void chooseMap(uint8_t n);      // 0 = el inicial; un mapa que no es suyo se ignora; guarda
  // el mapa que toca de verdad: el elegido si es suyo, si no el del bioma
  uint8_t yardMapWanted() const { return yardChosen && ownsMap(yardChosen) ? yardChosen : YARD_MAP_START; }
  uint16_t coinsEarnedToday() const;  // lo contado hoy para el tope diario

  // --- viajes y coleccion (v2.0, Y5; travel.h). Viaja un bicho de la CAJA (los de casa nunca), de uno en
  // uno para toda la partida, a un mapa que se tenga, 1 / 3 / 8 h de tiempo real (tier 0..2). Mientras
  // esta fuera su hueco no se puede despertar, ni guardar encima, ni llevar al equipo. Al cumplirse (en
  // el tick, o al arrancar si paso con la placa apagada) vuelve solo: monedas, crecimiento, objeto,
  // recuerdo y un resultado sin ver (takeTravelResult). Sin hora (RTC sin poner) no se puede salir.
  bool canTravel(uint8_t slot, uint8_t dest, uint8_t tier) const;
  bool startTravel(uint8_t slot, uint8_t dest, uint8_t tier);  // guarda al momento (un toque)
  bool travelling() const { return trvSlot != TRV_NONE; }
  bool slotTravelling(uint8_t k) const { return trvSlot != TRV_NONE && trvSlot == k; }
  uint8_t travelSlot() const { return trvSlot; }  // TRV_NONE si no hay viaje
  uint8_t travelDest() const { return trvDest; }  // mapa 1..5
  uint8_t travelTier() const { return trvTier; }  // 0..2
  uint32_t travelStartEpoch() const { return trvStart; }
  uint32_t travelSeed() const { return trvSeed; }
  uint32_t travelSecondsLeft() const { return travelling() ? travelTimeLeft(trvStart, trvTier, lastSeenEpoch) : 0; }
  uint16_t travelMinutesElapsed() const { return travelling() ? travelElapsedMin(trvStart, trvTier, lastSeenEpoch) : 0; }
  void travelCheck();  // lo llama el tick y syncClock: si ya paso el tiempo, termina el viaje
  // el resultado del ultimo viaje sin ver (sobrevive a un reinicio); lo recoge la UI y se olvida
  bool travelResultPending() const { return trvResPending; }
  bool takeTravelResult(TravelResult *out);
  bool peekTravelResult(TravelResult *out) const { if (out && trvResPending) *out = trvResult; return trvResPending; }  // sin olvidarlo
  // coleccion (del jugador): bit id = recuerdo id (0..SOUV_COUNT-1) conseguido
  uint32_t souvenirMask() const { return souvOwned; }
  bool hasSouvenir(uint8_t id) const { return id < SOUV_COUNT && (souvOwned & (1u << id)); }
  uint8_t souvenirCount() const;
  uint32_t souvenirFoundEpoch(uint8_t id) const { return id < SOUV_COUNT && hasSouvenir(id) ? souvDate[id] : 0; }  // 0 = sin hora
  void setSouvenirMask(uint32_t mask);  // serie SOUV (pruebas): los nuevos se fechan ahora
  void dbgTravelElapsed(uint32_t min);  // serie TRAVELT: el viaje salio hace min minutos MAS
  void travelFinishNow();               // serie TRAVELEND
  uint16_t lastCoins = 0;        // lo ganado en el ultimo minijuego (la UI lo ensena; no se guarda)
  uint16_t goalBannerCoins = 0;  // monedas de las metas cruzadas desde el ultimo banner (takeGoalBanner)

  // --- pasos y mochila (Tarea 1). addSteps() nunca escribe en flash: marca el
  // guardado pendiente y el loop lo vuelca con la pantalla atenuada.
  // v3.0: lo de caminar (felicidad, peso, vinculo, niveles de las metas) llega a cada bicho de casa
  // nacido y despierto, con su propio tope de vinculo; hallazgos, huevos y monedas, una sola vez
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
  void setRevives(uint8_t n) { items[IT_REVIVE] = n > ITEM_MAX ? ITEM_MAX : n; save(); }  // serie REVIVES (v1.59)
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
  // dormido; con dos bichos, los del que mas subio); el banner los ensena como "+N Lv"
  uint8_t takeGoalBanner(uint8_t *levels = nullptr, uint16_t *coinsOut = nullptr) {
    uint8_t g = goalBanner;
    if (levels) *levels = goalBannerLv;
    if (coinsOut) *coinsOut = goalBannerCoins;
    goalBannerCoins = 0;
    goalBanner = 0;
    goalBannerLv = 0;
    return g;
  }

  // --- combate
  // la felicidad del final de un estadio (+PRACTICE_JOY, como mucho una vez por hora) toca ya
  bool practiceRewardReady() const;
  uint8_t rollBattleItem();  // la tabla del botin: el objeto del jefe (publica para las pruebas)

  // --- estadios. Medallas, dia del premio y equipo son del jugador: sobreviven a eclosiones,
  // finales y movimientos de la caja (solo WIPE los borra)
  bool hasBadge(uint8_t s) const { return s < STAGE_COUNT && (badges & (1u << s)); }
  uint8_t badgeMask() const { return badges; }
  uint8_t badgeCount() const;
  // el 0 siempre; el s con la medalla del s - 1
  bool stageUnlocked(uint8_t s) const { return s < STAGE_COUNT && (s == 0 || hasBadge(s - 1)); }
  // estadios cuyo premio del jefe ya se pago hoy (sin reloj, el dia 0 dura hasta tenerlo)
  uint8_t stageRewardMask() const { return stageDay == today() ? stageMask : 0; }
  bool stageRewardReady(uint8_t s) const { return s < STAGE_COUNT && !(stageRewardMask() & (1u << s)); }
  // el ultimo equipo, en orden (TEAM_MAX uids, 0 = vacio). setTeam no escribe en flash: marca el
  // guardado, como addSteps
  const uint32_t *teamUids() const { return team; }
  void setTeam(const uint32_t *uids, uint8_t n);
  // donde esta el bicho de ese uid: TEAM_HOME0/1 (en casa, ese hueco), su hueco de la caja (BOX_OK) o -1
  int8_t teamSlotOf(uint32_t id) const;
  // puede ir en el equipo: uno de casa si canBattle(); uno de la caja, siempre (hiberna entero)
  bool teamCanFight(uint32_t id) const;
  // fin de un estadio (vencido o no), una vez al acabar: la felicidad del combate de practica a los de
  // casa (una vez por hora); vencido, la medalla y, si hoy no se pago, el premio del jefe a cada uid
  // que siga existiendo y un objeto envuelto. Guarda antes de volver. Corte de luz: la marca de
  // "pagado hoy" y la medalla se escriben antes que los premios (se puede perder parte del premio,
  // nunca cobrarlo dos veces)
  StageReward stageDone(uint8_t stage, bool cleared, const uint32_t *uids, uint8_t n);

  // --- encuentros salvajes (v2.2, encounter.h). Del jugador: sobreviven a eclosiones y finales. "enc" guarda
  // el dia, las tiradas hechas, el salvaje y cuando salio; "cegg" los huevos de captura de la mochila
  void encCheck();           // el tick, syncClock y los pasos: dia nuevo, tiradas, y se va a las 6 h
  bool encCanRoll() const;   // hay alguien con quien pelear (y la partida empezo): de casa o de la caja
  void encTravelRoll(uint16_t hours);  // un viaje acaba de volver: su tirada (3 % por hora)
  bool encActive() const;    // hay uno en el patio sin pelear (vivo)
  bool encWonPending() const { return encSpawnAt && (encFlags & ENCF_WON); }  // ganado: falta la Poke Ball
  bool encShown() const { return encActive() || encWonPending(); }  // se ve en el patio
  int16_t encDex() const { return encDexV; }
  uint8_t encLevel() const { return encLv; }
  bool encShiny() const { return encFlags & ENCF_SHINY; }
  bool encExclusive() const { return encFlags & ENCF_EXCL; }
  uint8_t encTraining(uint8_t i) const { return i < 3 ? encTr[i] : 0; }
  uint32_t encSpawnEpoch() const { return encSpawnAt; }
  uint16_t encMinutesLeft() const;
  uint8_t encRolledMask() const { return encDay == today() ? encRolled : 0; }  // serie ENC?
  // acaba de salir: la UI lo lee (despierto) y suena SFX_WILD una vez
  bool encSoundPending() const { return encSound && encShown(); }
  void takeEncSound() { encSound = false; }
  bool encSpawnNow(int16_t dex = 0);   // serie ENC [dex] y renderizador: sale ya (dex 0 = sorteo)
  void encClear();                     // serie ENCEND: se va sin mas
  void dbgEncSet(uint8_t lv, bool sh) {  // renderizador: el nivel y el shiny del que hay
    encLv = lv ? lv : encLv;
    encFlags = sh ? (encFlags | ENCF_SHINY) : (encFlags & ~ENCF_SHINY);
  }
  // fin del combate contra el salvaje (una vez; guarda). Perder: se escapa. Ganar: el premio al uid que
  // peleo y queda pendiente de la Poke Ball (encWonPending)
  EncReward encounterDone(bool won, uint32_t fighter);
  uint8_t captureBlock() const;        // CAP_OK o por que no
  bool captureWild();                  // la Poke Ball: un huevo de captura a la mochila; guarda
  void releaseWild();                  // sin Poke Ball (o "no"): vuelve a la hierba; guarda
  uint8_t rollWildItem();              // lo que suelta (publica para las pruebas)
  uint8_t captureEggs() const { return capN; }
  int16_t captureEggSpecies(uint8_t i) const { return i < capN ? capDex[i] : 0; }
  void setBalls(uint8_t n) { items[IT_BALL] = n > ITEM_MAX ? ITEM_MAX : n; save(); }  // serie BALLS
  void setBadges(uint8_t mask) { badges = mask; pendingSave = true; }    // serie y renderizador
  void clearStageDay() { stageDay = 0; stageMask = 0; pendingSave = true; }  // premios de hoy otra vez

  // --- trucos del IMU (Tarea 2): sin guardado inmediato. v3.0: a los dos bichos de casa
  void wakeGrumpy();     // agitar con alguno dormido: se despiertan de mal humor
  // agitar con alguno despierto: se marean (o los huevos se tambalean). true: un huevo del patio
  // conto la sacudida (le quito EGG_SHAKE_MIN minutos)
  bool shaken();
  void sleepDeferred();  // boca abajo de noche: a dormir (toggleLight sin guardar al momento)

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
  // deshace el movimiento (boxBoot). v3.0: s = el hueco de casa (0 o 1)
  uint8_t boxState(uint8_t k) const { return k < BOX_SLOTS ? boxSt[k] : (uint8_t)BOX_EMPTY; }
  const PetRecord &boxRecord(uint8_t k) const { return boxRec[k < BOX_SLOTS ? k : 0]; }  // solo si BOX_OK
  uint8_t boxUsed() const;   // huecos ocupados (danados incluidos)
  // se puede mover algo con el hueco de casa s: sin despedida, evolucion ni eleccion de inicial
  bool canBoxMoveSlot(uint8_t s) const { const PetLife &l = home(s); return !l.busyEnding() && !l.evolving() && !starterPick; }
  bool canBoxMove() const { return canBoxMoveSlot(focusSlot); }
  bool canStore(uint8_t k, uint8_t s) const {
    return k < BOX_SLOTS && s < HOME_SLOTS && boxState(k) == BOX_EMPTY && !slotTravelling(k) && !home(s).isEgg() && canBoxMoveSlot(s);
  }
  bool canStore(uint8_t k) const { return canStore(k, focusSlot); }
  bool canWake(uint8_t k, uint8_t s) const {
    return s < HOME_SLOTS && boxState(k) == BOX_OK && !slotTravelling(k) && canBoxMoveSlot(s);  // de viaje (Y5): no
  }
  bool canWake(uint8_t k) const { return canWake(k, focusSlot); }
  // guarda al bicho del hueco de casa s en el hueco k (vacio) y deja en su sitio el huevo guardado, o uno nuevo
  bool store(uint8_t k, uint8_t s);
  bool store(uint8_t k) { return store(k, focusSlot); }
  // despierta al del hueco k en el hueco de casa s. Con un bicho nacido, los dos se cambian el sitio;
  // con un huevo, el huevo se queda tal cual (especie, tirada shiny y grietas) en la incubadora de ese
  // hueco y vuelve tras el proximo guardado o final
  bool wake(uint8_t k, uint8_t s);
  bool wake(uint8_t k) { return wake(k, focusSlot); }
  bool boxClear(uint8_t k);                // BOXCLR (tambien un hueco danado)
  bool boxPut(uint8_t k, PetRecord r);     // BOXFILL: un bicho inventado en un hueco vacio (uid nuevo)
  void boxReload() { for (uint8_t k = 0; k < BOX_SLOTS; k++) readSlot(k); }  // relee la NVS (renderizador)
  PetRecord toRecord() const { return record(lastSeenEpoch); }  // el del foco, sellado (storedEpoch = ahora)
  void fromRecord(const PetRecord &r) { loadRecord(r); }       // el del foco pasa a ser el bicho de r

private:
  Preferences prefs;
  uint32_t lastTick = 0;
  void placeEgg();          // el huevo del foco pasa a ser del patio, puesto ahora
  void eggClockStamp();     // puesto sin hora: se fecha en cuanto se sabe
  // viajes (Y5): "trv" (hueco, mapa, duracion, uid del viajero, salida, semilla) en UNA entrada; sin ella no
  // hay viaje. "souv" la mascara y "souvD" las 25 fechas; "trvR" el resultado sin ver
  uint8_t trvSlot = TRV_NONE, trvDest = 0, trvTier = 0;
  uint32_t trvUid = 0, trvStart = 0, trvSeed = 0;
  uint32_t souvOwned = 0;
  uint32_t souvDate[SOUV_COUNT] = { 0 };
  TravelResult trvResult = {};
  bool trvResPending = false;
  void travelComplete();
  void travelCancel();
  bool travelPersist();
  uint8_t ticksSinceSave = 0;
  bool pendingSave = false;     // guardado periodico pendiente de volcar
  bool starterPick = false;     // primera partida: esperando que el jugador elija inicial
  uint32_t coinDay = 0;        // "cnD": dia del contador del tope diario
  uint16_t coinToday = 0;      // "cnT": monedas con tope ganadas ese dia
  void nightMinute(int h, bool live);  // un minuto de la noche del foco: dormirse o despertarse solo
  void sleepMinute(bool full, int h);  // un minuto dormido: energia y necesidades (en vivo y apagado)
  void goToBed();                      // acostarlo (luz o boca abajo): de noche, vinculo y despertar solo
  void addPoop();
  uint16_t milestoneNew = 0;   // hito de racha sin celebrar (dias; takeMilestone)
  // pasos: restos hacia el siguiente punto de cada premio (nada se pierde entre lecturas)
  uint16_t joyRem = 0, weightRem = 0, bondRem = 0;
  uint16_t findRem = 0;        // pasos premiados hacia el siguiente hallazgo
  uint16_t findAt = 0;         // ...y a cuantos toca (FIND_MIN..FIND_MAX; 0 = sin sortear)
  uint8_t foundQ[FOUND_QUEUE];
  bool foundFull[FOUND_QUEUE];
  bool foundGift[FOUND_QUEUE];
  uint8_t foundN = 0;
  uint8_t gifts[GIFT_MAX] = { 0 };  // regalos sin abrir (v1.55), en orden de llegada
  uint8_t giftN = 0;
  bool takeGift(uint8_t it);   // quita un regalo de ese objeto sin abrirlo (el Revivir, v1.60)
  void giveGift(uint8_t it);   // envuelto; con la cola llena, a la mochila con aviso (giveFound)
  void unwrapGifts();          // la casa queda libre: los regalos entran solos en la mochila
  uint16_t eggRem = 0;         // pasos hacia el siguiente huevo (v1.39)...
  uint16_t eggAt = 0;          // ...y a cuantos toca (EGG_MIN..EGG_MAX; 0 = sin sortear)
  uint8_t goalBanner = 0;     // meta del banner pendiente (0 = ninguno)
  uint8_t goalBannerLv = 0;   // niveles ganados con las metas desde el ultimo banner
  // la felicidad del combate (stageDone), una vez por hora
  uint32_t lastPracticeReward = 0;  // millis del ultimo premio...
  bool practiceRewarded = false;    // ...si lo hubo
  uint8_t practiceJoy();            // la felicidad al del foco, sin guardar (stageDone)
  // estadios (del jugador): "bdg" medallas (bit s = estadio s vencido alguna vez), "stD" + "stM" el
  // dia y los estadios cuyo premio del jefe ya se pago ese dia, "team" el ultimo equipo
  uint8_t badges = 0;
  uint32_t stageDay = 0;
  uint8_t stageMask = 0;
  uint32_t team[TEAM_MAX] = { 0 };
  bool payStageDay(uint8_t stage);  // marca el premio de hoy en la NVS; false = no se pudo (no se paga)
  // encuentros salvajes (v2.2): "enc" (16 bytes) y "cegg" ([n, dex, shiny] x CAP_EGG_MAX)
  uint32_t encDay = 0;       // dia de encRolled
  uint8_t encRolled = 0;     // tiradas hechas ese dia (ENC_ROLL_*) y si ya salio uno
  uint32_t encSpawnAt = 0;   // cuando salio (epoch); 0 = no hay
  int16_t encDexV = 0;
  uint8_t encLv = 0;
  uint8_t encFlags = 0;      // ENCF_*
  uint8_t encTr[3] = { 0, 0, 0 };
  bool encSound = false;     // solo RAM: acaba de salir y aun no sono
  uint8_t capN = 0;
  uint8_t capDex[CAP_EGG_MAX] = { 0 };
  bool capShiny[CAP_EGG_MAX] = { false };
  void encStart(int16_t dex, uint32_t rng);
  void encEnd();
  void capPush(int16_t dex, bool sh, bool front);
  void bagEggOut(bool saveNow);  // un huevo de la mochila al hueco del foco: primero los de captura
  void encPersist();

  uint32_t today() const { return lastSeenEpoch ? lastSeenEpoch / 86400 : 0; }
  // el cuidado del dia (v3.0, partido en dos): la racha, una vez al dia para el jugador, y el primer
  // cuidado de ESTE bicho (careDay), que le da BOND_CARE_DAY de vinculo
  void registerCare(bool saveNow = true);
  void addBond(uint8_t amt);
  void checkMedals(bool saveNow = true);
  void persist(bool now) { if (now) save(); else pendingSave = true; }
  void rollStepDay(uint32_t day);
  void findItem();
  void giveFound(uint8_t it, bool gift = false);  // a la mochila y a la cola de avisos
  void queueFound(uint8_t it, bool full, bool gift);  // solo el aviso
  void goalReward(uint8_t tier, const bool *rewarded);  // rewarded[s]: el hueco s recibe lo de caminar
  void tick();
  bool tickHome();                  // el minuto del hueco del foco; true = cuenta para el autoguardado
  void syncHome(uint32_t seen, uint32_t mins);  // el tiempo apagado del hueco del foco
  void hatch();
  void resetHome();                 // lo comun de un huevo nuevo y del hueco vacio
  void freeHome();                  // guardar en la caja: huevo que esperaba, de la mochila, o vacio
  void endLife(uint8_t cer);        // fin de una crianza (v1.41): el hueco, vacio; los huevos, a la mochila
  void vacate(bool saveNow = true); // el hueco del foco, vacio (v1.39)
  void eggToHome(bool saveNow);     // con el hueco vacio: un huevo de la mochila pasa a el
  // caja (v1.37)
  PetRecord boxRec[BOX_SLOTS];      // copia en RAM de cada hueco (la UI la dibuja sin leer flash)
  uint8_t boxSt[BOX_SLOTS] = { 0 };
  void boxBoot();                   // al arrancar: lee la caja y arregla un movimiento cortado
  void boxBootSlot(uint8_t s, bool hasT, const PetRecord &t, bool tOk);  // lo de un hueco de casa
  void migrateGrowthCurve();        // crecimiento de la curva v1 a la v2 (clave "gcv")
  void readSlot(uint8_t k);
  bool writeSlot(uint8_t k, PetRecord r);
  bool clearSlot(uint8_t k);
  int8_t slotOf(uint32_t id) const; // hueco (BOX_OK) con ese uid, o -1
  uint32_t newUid() const;          // al azar, distinto de 0, de los dos de casa y de los de la caja
  void registerSpecies(int16_t dex, bool sh);  // sh: tambien en la de shinies
  bool saveHome1();                 // "hm1" (si cambio); false si no se pudo escribir
  bool saveOk = true;               // el ultimo save() llego a escribir "hm1" (y lo de despues)
  void saveBag();                   // "bpack" y "cegg" ya (endLife: un huevo de casa a la mochila)
  void saveCapEggs();               // "cegg"
  HomeRec hm1Saved;                 // lo ultimo escrito en "hm1" (para no repetirlo)
  bool hm1Have = false;             // ...si lo hay
  void save();
  void load();
  static uint8_t clamp100(int v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }
};
