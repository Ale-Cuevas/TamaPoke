#pragma once
#include <Arduino.h>
#include <type_traits>
#include "growth.h"  // el nivel sale de growMin
#include "box.h"     // PetRecord: lo que va a la caja
#include "food.h"    // favBerryOf: la baya favorita de la linea (v3.2)
#include "tuning.h"  // POOP_MAX, DIZZY_SHAKES, REVIVE_LIMIT_MIN...

// La vida de un bicho de casa (v3.0, dos bichos en casa). Todo lo que es de un individuo (o del hueco de
// casa donde vive: su huevo, su incubadora, su muerte) esta aqui; lo del jugador (racha, pasos, mochila,
// monedas, caja, Pokedex...) se queda en Pet. Pet hereda de PetLife: `pet.fullness` es el bicho que tiene
// el foco (Pet::focus), y el otro hueco vive en Pet::other. Cambiar el foco intercambia los dos bloques
// enteros, asi que tiene que poder copiarse con un memcpy (trivially copyable): nada de punteros a si
// mismo ni de clases con constructor de copia.
//
// Las consultas que solo miran al individuo viven aqui (el patio las usa sobre Pet::home(s) para dibujar
// los dos bichos sin mover el foco). Las que necesitan algo del jugador (la hora, el huevo del patio, la
// eleccion del inicial) son de Pet.

#define EVOLVE_ANIM_MS 5200UL              // animacion de evolucion (mas larga = mas epica)
#define CEREMONY_MS 10000UL                // duracion de la despedida en pantalla
#define EMOTE_MS 3000UL                    // enfado, mareo, huevo que se tambalea

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
// CER_RUNAWAY ya no se usa (v1.60, la muerte la sustituye), pero un guardado puede traerlo en "lend"
enum : uint8_t { CER_NONE = 0, CER_FAREWELL, CER_RUNAWAY, CER_RELEASE, CER_DEATH };

enum PetMood : uint8_t { MOOD_HAPPY, MOOD_SAD, MOOD_EATING, MOOD_SLEEPING };

enum : uint8_t { EMOTE_NONE = 0, EMOTE_ANGRY, EMOTE_DIZZY, EMOTE_WOBBLE };

#define HOME_SLOTS 2  // huecos de casa (v3.0): dos bichos vivos a la vez

struct PetLife {
  // Estadisticas 0..100
  uint8_t fullness = 80;  // comida
  uint8_t joy = 80;       // felicidad
  uint8_t energy = 80;    // energia
  uint8_t hygiene = 100;  // SIN USO desde v3.1 (se guarda por compatibilidad: siempre 100)
  uint8_t poops = 0;      // cacas en el suelo (max POOP_MAX)
  uint8_t weight = 0;     // 0-100: las chuches engordan, el minijuego quema
  // cacas (v1.58): huecos ocupados (bit i = la caca i en el suelo). Solo en RAM: el guardado trae la
  // cuenta (poops) y al cargar se ocupan los primeros huecos (ver poopSlots)
  uint8_t poopMask = 0;
  // la edad (ageMinutes) a la que salio cada caca (v3.1: las viejas drenan felicidad, ver stalePoops)
  uint32_t poopAt[POOP_MAX] = { 0 };
  // genes (90-110%, se tiran al eclosionar) y entrenamiento (0-100)
  uint8_t geneAtk = 100, geneDef = 100, geneSpe = 100;
  uint8_t trAtk = 0, trDef = 0, trSpe = 0;
  bool berryKnown = false;  // ya descubrio su baya favorita (v3.2: se guarda con REC_FAV6 / "fk"; el "bk" de antes se ignora)
  bool shiny = false;       // variante de color rara (se sortea en el huevo)
  uint32_t ageMinutes = 0;  // edad real en minutos (huevo incluido): dias, despedida
  // minutos de crecimiento (v1.35, growth.h): de ellos sale el nivel. Avanzan con la edad
  // desde que nace, y los saltan el Caramelo Raro y las metas de pasos sin envejecerlo.
  uint32_t growMin = 0;
  int16_t speciesId = -1;   // numero de Pokedex (1-151), -1 = huevo (o hueco vacio, ver vacant)
  // hueco vacio (v1.39): ni bicho ni huevo. Va con speciesId = -1, asi que isEgg() tambien es true:
  // todo lo que exige un bicho nacido ya lo descarta. Lo propio del huevo (eclosionar, grietas,
  // tambalearse) mira vacant.
  bool vacant = false;
  int16_t prevSpeciesId = -1;  // para la animacion de evolucion
  uint8_t careMistakes = 0;    // descuidos: cada uno retrasa la evolucion 1 nivel
  uint8_t mistakeCooldown = 0;
  uint16_t goodTicks = 0;      // racha bien cuidado: forja la DEF
  bool sleeping = false;
  uint8_t ceremony = CER_NONE; // despedida/liberacion/muerte en curso
  uint32_t ceremonyUntil = 0;
  // vinculo (del bicho: sube lento con cuidado, se resetea al nacer otro), con su tope diario
  uint8_t bond = 0;
  uint8_t bondToday = 0;
  uint32_t bondDay = 0;     // dia de bondToday: el tope vuelve a 0 a medianoche
  // v3.0: el dia del primer cuidado de ESTE bicho (su +BOND_CARE_DAY); la racha es del jugador
  uint32_t careDay = 0;
  char nick[12] = "";       // apodo (vacio = nombre de especie)
  // v3.4: medallas del jugador que este bicho ya hizo subir (bits PMB_*, medals.h): cada uno cuenta una vez
  // aunque vaya y vuelva de la caja (va en PetRecord::medals con REC_V3, y en "pmc" el del hueco 0)
  uint8_t counted = 0;
  uint32_t satedUntil = 0;  // tarta (v1.54): lleno hasta esta edad en minutos
  // identidad del individuo (v1.37): se sortea al nacer, nunca 0; el huevo tiene 0
  uint32_t uid = 0;
  // abandono y muerte (v1.60, v1.64)
  bool dying = false;       // se esta muriendo, el dialogo espera
  uint8_t dyingMin = 0;     // minutos muriendose, hacia REVIVE_LIMIT_MIN
  uint8_t neglectMin = 0, deathMin = 0;  // con el vinculo apagandose, y ya sin el
  int16_t deathEgg = 0;     // sorteado al empezar a morirse (un reinicio no lo repite)
  bool diedNow = false;     // acaba de pasar la hora con la placa encendida: la UI lo lee una vez
  // la noche (v1.63): se durmio de noche (se despierta solo a las WAKE_HOUR), la ultima noche en que
  // acostarlo dio vinculo, minutos despierto de noche y la hora del minuto anterior
  bool nightSleep = false;
  uint32_t bedNight = 0;
  uint8_t nightAwakeMin = 0;
  int8_t lastTickHour = -1;
  // el huevo de este hueco y su incubadora (FR-BOX.5): especie oculta, tirada shiny, grietas, el huevo
  // del patio (cuando se puso y lo que le quitaron las sacudidas), si es de una captura y si espera
  int16_t eggTarget = 1;
  bool eggShiny = false;
  uint8_t eggTaps = 0;
  uint32_t eggPlaced = 0;
  uint8_t eggSaved = 0;
  uint32_t eggShakeMs = 0;  // la ultima sacudida que conto (solo RAM)
  bool eggShakeHave = false;
  bool eggCap = false;
  bool eggHeldFlag = false;  // el huevo espera en la incubadora mientras otro bicho esta despierto
  // solo RAM: animaciones, enfriamientos y "mantener forma"
  uint32_t eatUntil = 0;
  uint32_t heartUntil = 0;
  uint32_t petBondUntil = 0;   // enfriamiento del vinculo por mimos
  uint32_t evolveUntil = 0;
  uint16_t evoDeclinedLv = 0;  // "mantener forma": no ofrecer evolucion hasta subir de nivel
  uint32_t farDeclinedAge = 0; // "quedaros juntos": no ofrecer despedida hasta esta edad
  uint8_t emoteKind = EMOTE_NONE;
  uint32_t emoteUntil = 0;
  uint32_t lastGrumpy = 0;     // millis del ultimo despertar que resto felicidad...
  bool grumpied = false;       // ...si lo hubo
  uint32_t shakeAt[DIZZY_SHAKES] = { 0 };  // ultimas sacudidas con el bicho despierto
  uint8_t shakeN = 0;
  uint32_t lastDizzyPenalty = 0;
  bool dizzyPenalized = false;

  // --- consultas del individuo
  // sin bicho nacido: un huevo O el hueco vacio (v1.39; isVacant() distingue)
  bool isEgg() const { return speciesId < 0; }
  bool isVacant() const { return vacant; }
  bool hatched() const { return speciesId >= 0; }
  // ni ceremonia ni muriendose: lo que exige un bicho que pueda hacer cosas
  bool busyEnding() const { return ceremony != CER_NONE || dying; }
  bool isDying() const { return dying; }
  // v1.64: paso la hora sin Revivir; ya solo queda el final
  bool reviveExpired() const { return dying && dyingMin >= REVIVE_LIMIT_MIN; }
  uint8_t reviveMinutesLeft() const { return dying && dyingMin < REVIVE_LIMIT_MIN ? REVIVE_LIMIT_MIN - dyingMin : 0; }
  uint8_t dyingMinutes() const { return dyingMin; }
  // las tres necesidades a cero, despierto: el vinculo se apaga (y luego se muere)
  bool neglected() const {
    return !isEgg() && !sleeping && !busyEnding() && fullness == 0 && joy == 0 && energy == 0;
  }
  uint8_t neglectMinutes() const { return neglectMin; }
  uint8_t deathMinutes() const { return deathMin; }
  int16_t deathEggSpecies() const { return deathEgg; }  // 0 = no deja huevo
  // la tarta lo tiene lleno: la comida no baja (ni con el tiempo ni jugando) hasta satedUntil
  bool sated() const { return !isEgg() && ageMinutes < satedUntil; }
  uint32_t satedMinutesLeft() const { return sated() ? satedUntil - ageMinutes : 0; }
  // v3.2: la baya favorita sale de la forma base de su linea (food.h), asi no cambia al evolucionar; color 0..5
  bool lovesBerry(uint8_t color) const { return !isEgg() && favBerryOf(speciesId) == color; }
  bool eating() const { return timeLeft(eatUntil) > 0; }
  bool showHeart() const { return timeLeft(heartUntil) > 0; }
  bool evolving() const { return timeLeft(evolveUntil) > 0; }
  float evolveT() const { return 1.0f - (float)timeLeft(evolveUntil) / (float)EVOLVE_ANIM_MS; }
  // progreso de la ceremonia de despedida, 0..1 (para animarla)
  float ceremonyT() const {
    if (ceremony == CER_NONE) return 0.0f;
    return 1.0f - (float)timeLeft(ceremonyUntil) / (float)CEREMONY_MS;
  }
  uint8_t emote() const { return timeLeft(emoteUntil) ? emoteKind : (uint8_t)EMOTE_NONE; }
  float emoteT() const { return 1.0f - (float)timeLeft(emoteUntil) / (float)EMOTE_MS; }
  void startEmote(uint8_t kind) { emoteKind = kind; emoteUntil = millis() + EMOTE_MS; }
  void clearEmote() { emoteUntil = 0; }
  // uint16_t, no uint8_t: un uint8_t daba la vuelta a 0 en el nivel 256 (ver growth.h)
  uint16_t level() const { return levelFor(growMin); }
  uint16_t levelMinutesInto() const { return levelInto(growMin); }  // de levelCost(level())
  uint16_t levelMinutesCost() const { return levelCost(level()); }
  void setLevel(uint16_t lv) { growMin = levelStart(lv); }  // comando LVL y pruebas
  uint8_t lowestStat() const {
    uint8_t a = fullness < joy ? fullness : joy;
    return a < energy ? a : energy;
  }
  PetMood mood() const {
    if (sleeping) return MOOD_SLEEPING;
    if (eating()) return MOOD_EATING;
    if (lowestStat() < NEED_LOW) return MOOD_SAD;
    return MOOD_HAPPY;
  }
  uint8_t bondGainedToday() const { return bondToday; }  // diagnostico (STATS)
  bool nightSlept() const { return nightSleep; }        // se durmio de noche: se despierta a las WAKE_HOUR
  uint8_t nightAwakeMinutes() const { return nightAwakeMin; }
  uint32_t bedNightId() const { return bedNight; }
  void wokeUp() { nightSleep = false; nightAwakeMin = 0; }
  uint8_t poopSlots() const {
    uint8_t n = poops > POOP_MAX ? POOP_MAX : poops;
    uint8_t m = poopMask & ((1 << POOP_MAX) - 1);
    return __builtin_popcount(m) == n ? m : (uint8_t)((1 << n) - 1);
  }
  // v3.1: cuantas cacas llevan en el suelo mas de POOP_STALE_MIN minutos (de edad del bicho)
  uint8_t stalePoops() const {
    uint8_t m = poopSlots(), n = 0;
    for (uint8_t i = 0; i < POOP_MAX; i++)
      if ((m & (1 << i)) && ageMinutes > poopAt[i] && ageMinutes - poopAt[i] > POOP_STALE_MIN) n++;
    return n;
  }
  // las marcas de edad de las cacas puestas, seguidas desde el primer hueco (como las recompone poopSlots
  // al cargar); las demas a 0 (asi el blob "poopA" solo cambia cuando cambian las cacas)
  void poopStamps(uint32_t out[POOP_MAX]) const {
    uint8_t m = poopSlots(), k = 0;
    for (uint8_t i = 0; i < POOP_MAX; i++)
      if (m & (1 << i)) out[k++] = poopAt[i];
    while (k < POOP_MAX) out[k++] = 0;
  }
  // la incubadora de este hueco (serie BOX y pruebas)
  bool eggHeld() const { return eggHeldFlag; }
  int16_t eggSpecies() const { return eggTarget; }
  bool eggIsShiny() const { return eggShiny; }
  bool eggIsCaptured() const { return eggCap; }  // el huevo de este hueco es de una captura
  uint8_t eggSavedMinutes() const { return eggSaved; }
  uint32_t eggPlacedEpoch() const { return eggPlaced; }
  uint8_t eggRarity() const;  // rareza del huevo (sin revelar especie)
  // puede combatir: nacido, despierto, sin ceremonia ni evolucion en curso (con un bicho nacido nunca se
  // esta eligiendo inicial)
  bool canBattle() const { return !isEgg() && !sleeping && !busyEnding() && !evolving(); }
  // stats de combate: base real de gen 1 x genes + nivel + entrenamiento (pet.cpp)
  uint16_t statAt(uint8_t which, uint16_t lv) const;  // ATK (0), DEF (1) o VEL (2) a un nivel dado
  uint16_t atkStat() const;
  uint16_t defStat() const;
  uint16_t speStat() const;
  bool canEvolveNow() const;    // condiciones de evolucion cumplidas (lista)
  bool canFarewellNow() const;  // forma final + 3 dias: lista para despedirse (boton)
  // el usuario decide en un dialogo; "mantener/quedaros" pospone y re-ofrece luego
  bool wantEvolveButton() const { return canEvolveNow() && level() > evoDeclinedLv; }
  bool wantFarewellButton() const { return canFarewellNow() && ageMinutes >= farDeclinedAge; }
  void declineEvolve() { evoDeclinedLv = level(); }                                  // re-ofrece al subir de nivel
  void declineFarewell() { farDeclinedAge = ageMinutes + FAREWELL_POSTPONE_MIN; }  // re-ofrece dentro de 1 dia
  // el individuo como registro de la caja (sellado; storedEpoch = epoch) y al reves: pasa a ser el bicho de r
  // (solo lo del individuo: lo que vive en RAM empieza de cero, como tras un reinicio)
  PetRecord record(uint32_t epoch) const;
  void loadRecord(const PetRecord &r);

  // renderizador y serie
  void dbgClearAnims() { eatUntil = heartUntil = evolveUntil = 0; emoteKind = EMOTE_NONE; }
  void dbgEggSaved(uint8_t m) { eggSaved = m > EGG_SHAKE_CAP ? EGG_SHAKE_CAP : m; }
  void dbgEggClear() { eggPlaced = 0; eggSaved = 0; eggTaps = 0; }  // cada escena, sin huevo del patio ni grietas
  void dbgSetPoops(uint8_t n) {  // v3.1: recien hechas (sin edad)
    poops = n > POOP_MAX ? POOP_MAX : n;
    poopMask = (1 << poops) - 1;
    for (uint8_t i = 0; i < POOP_MAX; i++) poopAt[i] = ageMinutes;
  }
  void dbgAbandon() { fullness = joy = energy = 0; }  // serie ABANDON: empieza la cuenta
  void dbgClearDying() { dying = false; deathEgg = 0; neglectMin = deathMin = dyingMin = 0; }
};
static_assert(std::is_trivially_copyable<PetLife>::value, "el foco intercambia PetLife enteros");

// un hueco de casa vacio (el segundo de una partida nueva o de un guardado de la v2.x)
inline PetLife vacantPetLife() {
  PetLife l;
  l.vacant = true;
  return l;
}
