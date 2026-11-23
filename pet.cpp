#include "pet.h"
#include "dex.h"
#include "audio.h"
#include "zengame.h"
#include "dashgame.h"
#include "encounter.h"  // encuentros salvajes (v2.2): tiradas del dia, nivel y especie (logica pura)

// ---------- los dos huecos de casa (v3.0) ----------

void Pet::focus(uint8_t s) {
  s &= 1;
  if (s == focusSlot) return;
  PetLife tmp = *this;  // (la parte PetLife: el bicho del foco)
  static_cast<PetLife &>(*this) = other;
  other = tmp;
  focusSlot = s;
}

Pet::FocusGuard::FocusGuard(Pet &pp, uint8_t s) : p(pp), prev(pp.focusSlot), moved(false), ok(true) {
  s &= 1;
  if (s != p.focusSlot) {
    if (p.focusDepth) {
      // reentrada: la de fuera tiene el foco puesto en otro hueco. No se mueve (sus referencias quedarian
      // cambiadas) y quien lo pidio se salta lo suyo (ok = false: nunca sobre el bicho equivocado). En el PC
      // (pruebas) es un error: se para, salvo que la prueba lo busque (focusStrict = false)
      ok = false;
      p.focusErrors++;
      Serial.printf("FOCUS: reentrada (%u -> %u) ignorada\n", p.focusSlot, s);
#ifndef ARDUINO
      if (p.focusStrict) abort();
#endif
    } else {
      p.focus(s);
      moved = true;
    }
  }
  p.focusDepth++;
}

Pet::FocusGuard::~FocusGuard() {
  p.focusDepth--;
  if (moved) p.focus(prev);
}

int8_t Pet::slotOfUid(uint32_t id) const {
  if (!id) return -1;
  for (uint8_t s = 0; s < HOME_SLOTS; s++)
    if (home(s).hatched() && home(s).uid == id) return (int8_t)s;
  return -1;
}

bool Pet::homeAsleep() const {
  bool any = false;
  for (uint8_t s = 0; s < HOME_SLOTS; s++) {
    const PetLife &l = home(s);
    if (l.isVacant()) continue;
    if (l.isEgg() || !l.sleeping) return false;
    any = true;
  }
  return any;
}

void Pet::begin() {
  if (focusSlot) focus(0);  // (un begin() con el foco movido: el guardado es por huecos)
  pmMigrated = false;
  medalNewsN = 0;
  cnHave = false;
  // v3.20: las mejoras de casa empiezan sin nada (una partida nueva; load() lee "upg" si la hay)
  memset(upgTiers, 0, sizeof(upgTiers));
  memset(upgSlots, 0, sizeof(upgSlots));
  memset(gymMin, 0, sizeof(gymMin));
  homeEvts.clear();
  upgHave = false;
  prefs.begin("tamapoke", false);
  if (!prefs.getBool("init", false)) {
    other = vacantPetLife();  // v3.0: el segundo hueco de una partida nueva empieza vacio
    // partida nueva (o tras WIPE): su crecimiento ya es de la curva v2. "gcv" va antes
    // que "init", asi nunca se migra un contador que ya es de la curva nueva
    prefs.putUChar("gcv", GROW_CURVE_VER);
    prefs.putBool("init", true);
    items[IT_REVIVE] = REVIVE_START;  // v1.59: una partida nueva trae 3 Revivir (newEgg guarda la mochila)
    items[IT_BALL] = BALL_START;      // v2.2: y 3 Poke Balls
    pmClear(pm);                      // v3.4: sin medallas que contar (newEgg escribe "pm")
    pmReady = true;
    newEgg();
    prefs.putBool("rvg", true);       // despues: un corte antes deja la partida sin "rvg" y los da load()
    prefs.putBool("ballg", true);
  } else {
    load();
    migrateGrowthCurve();  // los contadores de la curva v1 (3 h) pasan a la nueva (2 h)
  }
  boxBoot();  // la caja (v1.37)
  // un viaje cuyo bicho ya no esta en su hueco (BOXCLR con la placa apagada, hueco danado): se cancela
  if (trvSlot != TRV_NONE && (trvSlot >= BOX_SLOTS || boxSt[trvSlot] != BOX_OK || boxRec[trvSlot].uid != trvUid))
    travelCancel();
  // guardado anterior a la caja: el bicho de ahora recibe su identidad (FR-BOX.11)
  if (dying && (isEgg() || vacant)) dying = false;  // v1.60: solo se muere un bicho nacido
  if (other.dying && other.isEgg()) other.dying = false;
  if (!isEgg() && !uid) {
    uid = newUid();
    prefs.putUInt("uid", uid);
  }
  // v1.40: la casa vacia con huevos en la mochila es un estado normal ("Dejar casa vacia" al soltar):
  // ya no se saca uno al arrancar. La casa vacia lo ofrece con su boton.
  // v3.4: sin "pm" (la partida de antes, o danado), las medallas del jugador se cuentan ahora. Despues de
  // boxBoot (la caja ya esta en su sitio) y del uid
  if (!pmReady) medalsMigrate();
  lastTick = millis();
}

// lo comun de empezar con un huevo y de quedarse con la casa vacia: nadie, a cero
void Pet::resetHome() {
  ceremony = CER_NONE;
  neglectMin = deathMin = dyingMin = 0;
  dying = false;
  deathEgg = 0;
  poopMask = 0;
  weight = 0;
  speciesId = -1;
  prevSpeciesId = -1;
  uid = 0;  // un huevo aun no es nadie: el uid se sortea al nacer
  fullness = 80;
  joy = 80;
  energy = 80;
  hygiene = 100;
  poops = 0;
  ageMinutes = 0;
  growMin = 0;
  careMistakes = 0;
  mistakeCooldown = 0;
  sleeping = false;
  counted = 0;  // v3.4
  wokeUp();
}

void Pet::newEgg(bool saveNow, int16_t heir, int8_t shinyForce) {
  resetHome();
  vacant = false;
  if (eggHeldFlag) {
    // el huevo que espero en la incubadora mientras otro bicho estaba despierto (caja,
    // FR-BOX.5): vuelve tal cual, con su especie, su tirada shiny y sus grietas. Asi guardar y
    // despertar no vuelven a tirar huevos: cada uno se tira una vez, y el amuleto con el.
    eggHeldFlag = false;
    starterPick = false;
  } else {
    eggTarget = pickEggSpecies();  // especie oculta segun rareza y pokedex
    if (heir > 0) eggTarget = heir;  // el huevo que deja al morir (v1.60): despues del sorteo, como FR-3.15
    starterPick = (registeredCount() == 0);  // primera partida: el jugador elige inicial
    // amuleto shiny de la mochila: lo gasta este huevo. No el inicial, ni
    // pickEggSpecies(), que tambien usa la simulacion de tiradas (comando EGGS).
    // v2.2: el huevo de una captura ya trae su tirada (la del salvaje) y no gasta el amuleto
    bool charm = items[IT_CHARM] && !starterPick && shinyForce < 0;
    if (charm) items[IT_CHARM] = 0;
    eggShiny = (random(shinyOdds(charm)) == 0);
    if (shinyForce >= 0) eggShiny = shinyForce > 0;
    eggCap = shinyForce >= 0;
    eggTaps = 0;
    eggPlaced = 0;
    eggSaved = 0;
  }
  // Y3: el huevo se pone en el patio (el inicial no: se abre con 3 toques). El que vuelve de la
  // incubadora sigue con su hora: la incubadora tambien incuba
  if (timedEggs && !starterPick && !eggPlaced) placeEgg();
  persist(saveNow);
}

void Pet::placeEgg() {
  eggPlaced = lastSeenEpoch >= EGG_EPOCH_MIN ? lastSeenEpoch : EGG_NO_CLOCK;
  eggShakeHave = false;
}

void Pet::eggClockStamp() {
  if (eggPlaced && !eggClockKnown(eggPlaced) && lastSeenEpoch >= EGG_EPOCH_MIN) {
    eggPlaced = lastSeenEpoch;
    pendingSave = true;
  }
}

void Pet::dbgEggElapsed(uint32_t min) {
  if (!isEgg() || vacant || starterPick) return;
  if (!eggPlaced) placeEgg();
  if (lastSeenEpoch >= EGG_EPOCH_MIN) eggPlaced = eggPlacedFor(lastSeenEpoch, min);
  pendingSave = true;
}

void Pet::dbgEggSpecies(int16_t dex) {
  if (isEgg() && !vacant && dex >= 1 && dex <= DEX_COUNT) eggTarget = dex;
}

void Pet::hatchNow() {
  if (!isEgg() || vacant || starterPick || busyEnding()) return;
  hatch();
}

// Guardar en la caja deja la casa libre. Huevos limitados (v1.39): vuelve el que esperaba en la
// incubadora; si no, sale uno de la mochila (y se sortea ahora, con el amuleto si lo hay); si
// tampoco, la casa se queda vacia.
void Pet::freeHome() {
  // v1.55: sin bicho no hay quien los abra, y un huevo envuelto cuenta para la casa. v3.0: solo si en el
  // otro hueco tampoco queda nadie (si no, los abre el)
  if (!other.hatched()) unwrapGifts();
  if (eggHeldFlag) {
    newEgg();
  } else if (items[IT_EGG]) {
    bagEggOut(true);
  } else {
    vacate();
  }
}

// un huevo de la mochila pasa a casa (v2.2): primero los de captura, con su especie y su tirada shiny
void Pet::bagEggOut(bool saveNow) {
  if (!items[IT_EGG]) return;
  items[IT_EGG]--;
  if (capN) {
    int16_t d = capDex[0];
    bool sh = capShiny[0];
    for (uint8_t i = 1; i < capN; i++) {
      capDex[i - 1] = capDex[i];
      capShiny[i - 1] = capShiny[i];
    }
    capN--;
    newEgg(saveNow, d, sh ? 1 : 0);
    return;
  }
  newEgg(saveNow);
}

// un huevo de captura a la cola (al principio: el de la incubadora que vuelve a la mochila)
void Pet::capPush(int16_t dex, bool sh, bool front) {
  if (dex < 1 || dex > DEX_COUNT) return;
  if (capN >= CAP_EGG_MAX) {
    if (!front) return;
    capN = CAP_EGG_MAX - 1;  // (no deberia pasar: captureBlock lo impide) se pierde el ultimo
  }
  if (front) {
    for (uint8_t i = capN; i > 0; i--) {
      capDex[i] = capDex[i - 1];
      capShiny[i] = capShiny[i - 1];
    }
    capDex[0] = (uint8_t)dex;
    capShiny[0] = sh;
  } else {
    capDex[capN] = (uint8_t)dex;
    capShiny[capN] = sh;
  }
  capN++;
}

// Fin de una crianza (soltar, despedida, escapada; v1.41): ningun huevo llega solo a casa. La casa se
// queda vacia y, con huevos, ofrece su boton "Abrir un huevo (N)" (preguntarlo al soltar, como hacia
// la v1.40, era redundante con ese boton).
// - La despedida deja un huevo de premio en la mochila (con la mochila llena se pierde y lo dice).
//   Se da aqui y no al empezar: la ceremonia no se guarda, y un corte durante ella devuelve al bicho
//   con su boton de despedida.
// - El huevo que esperaba en la incubadora pasa a la mochila (se sortea de nuevo al salir); con la
//   mochila llena no cabe y vuelve a casa. save() escribe la mochila antes que "eggH": un corte
//   entre los dos deja un huevo de mas, nunca uno de menos.
// - Sin huevos y sin nadie en la caja, un huevo de regalo a la mochila: nunca se acaba sin bicho,
//   sin huevo y sin nadie a quien despertar.
void Pet::endLife(uint8_t cer) {
  int16_t heir = (cer == CER_DEATH) ? deathEgg : 0;  // antes de que resetHome lo borre
  uint8_t eggsBefore = items[IT_EGG];  // v3.0: si un huevo pasa a la mochila, se escribe antes que el hueco
  if (cer == CER_DEATH) lastEnd = CER_DEATH;  // aqui y no al elegir: un corte a mitad vuelve al dialogo
  if (cer == CER_FAREWELL) pmBump(pm, MD_FAREWELL);  // v3.4: una despedida completa (medalla del jugador)
  // v1.55: sin bicho no hay quien los abra (un huevo envuelto evita el de regalo). v3.0: solo si en casa
  // no queda otro bicho nacido (si queda, los abre el)
  if (!other.hatched()) unwrapGifts();
  if (cer == CER_FAREWELL) giveFound(IT_EGG, true);
  if (heir > 0) {
    // muerte con huevo (v1.60): el suyo va derecho a casa. El que esperaba en la incubadora, a la
    // mochila; si esta llena, se pierde y lo dice (como el premio de la despedida)
    if (eggHeldFlag) {
      eggHeldFlag = false;
      if (items[IT_EGG] < ITEM_MAX) {
        items[IT_EGG]++;
        if (eggCap) capPush(eggTarget, eggShiny, true);  // v2.2: sigue siendo el de la captura
      } else {
        queueFound(IT_EGG, true, false);
      }
    }
    if (items[IT_EGG] != eggsBefore) saveBag();  // (el de la despedida no: aqui es una muerte)
    newEgg(true, heir);
    return;
  }
  if (eggHeldFlag) {
    if (items[IT_EGG] >= ITEM_MAX) {
      if (items[IT_EGG] != eggsBefore) saveBag();  // (el de la despedida)
      newEgg();
      return;
    }
    items[IT_EGG]++;
    if (eggCap) capPush(eggTarget, eggShiny, true);  // v2.2: sigue siendo el de la captura
    eggHeldFlag = false;
  }
  // v3.0: el de regalo, solo si el otro hueco tambien esta vacio, no hay huevos tuyos (mochila,
  // incubadoras, envueltos) y la caja esta vacia
  if (other.isVacant() && !eggsOwned() && !giftEggs() && !boxHasPets()) giveFound(IT_EGG, true);
  // el huevo que esperaba, el de la despedida o el de regalo: a la mochila en la NVS antes de que el
  // hueco quede vacio ("hm1" va antes que "bpack" en save(), y "uid" tambien): un corte, uno de mas
  if (items[IT_EGG] != eggsBefore) saveBag();
  vacate();
}

bool Pet::boxHasPets() const {
  for (uint8_t k = 0; k < BOX_SLOTS; k++)
    if (boxSt[k] == BOX_OK) return true;
  return false;
}

bool Pet::hatchFromBag() {
  if (!vacant || !items[IT_EGG] || busyEnding()) return false;
  eggToHome(true);
  return true;
}

void Pet::vacate(bool saveNow) {
  resetHome();
  vacant = true;
  starterPick = false;
  eggTaps = 0;
  if (!eggHeldFlag) {  // (el de la incubadora conserva su hora)
    eggPlaced = 0;
    eggSaved = 0;
    eggCap = false;
  }
  persist(saveNow);
}

void Pet::eggToHome(bool saveNow) {
  if (!vacant || !items[IT_EGG]) return;
  bagEggOut(saveNow);
}

void Pet::setBagEggs(uint8_t n) {
  items[IT_EGG] = n > ITEM_MAX ? ITEM_MAX : n;
  if (capN > items[IT_EGG]) capN = items[IT_EGG];  // v2.2: los de captura son de los de la mochila
  save();  // v1.40: con la casa vacia se quedan en la mochila (el boton de la casa los abre)
}

// sorteo shiny del huevo, 1 entre N: 1/48 base, mejor con despedida y con
// racha/vinculo altos, y x3 con el amuleto; nunca mejor que 1/8
int Pet::shinyOdds(bool charm) const {
  int n = (lastEnd == CER_FAREWELL ? SHINY_ONE_IN_BLESSED : SHINY_ONE_IN) - careBonus();
  if (n < SHINY_BEST) n = SHINY_BEST;
  if (charm) {
    n /= CHARM_SHINY_MULT;
    if (n < SHINY_BEST) n = SHINY_BEST;
  }
  return n;
}

// progresion offline: el tiempo paso aunque estuviera apagado, pero con
// piedad — las barras bajan con suelo en 15 (vuelve hambriento, no muerto),
// sin descuidos ni escapadas en ausencia
static uint8_t dropTo(uint8_t v, uint8_t d, uint8_t fl) {
  if (v <= fl) return v;
  return (v - fl > d) ? v - d : fl;
}

void Pet::setClock(uint32_t nowEpoch) {
  lastSeenEpoch = nowEpoch;
  if (nowEpoch) save();  // persiste ya: un corte de luz no pierde la referencia
}

void Pet::syncClock(uint32_t nowEpoch) {
  uint32_t seen = prefs.getUInt("seen", 0);
  lastSeenEpoch = nowEpoch;
  if (nowEpoch == 0) return;
  travelCheck();  // Y5: un viaje que acabo con la placa apagada vuelve ahora
  encCheck();     // v2.2: el encuentro de hoy (las tiradas que tocaban apagada, y el que ya se fue)
  uint32_t mins = (seen && nowEpoch > seen) ? (nowEpoch - seen) / 60 : 0;
  // v3.0: cada hueco de casa por su cuenta
  for (uint8_t s = 0; s < HOME_SLOTS; s++) {
    FocusGuard g(*this, s);
    if (!g.ok) continue;  // (reentrada: nunca sobre el bicho equivocado)
    syncHome(seen, mins);
  }
  save();  // tambien la primera vez o sin tiempo que aplicar
}

// el tiempo apagado de un hueco (el del foco)
void Pet::syncHome(uint32_t seen, uint32_t mins) {
  // v1.64: la hora del Revivir es real: apagada tambien corre (el resto del tiempo esta parado)
  if (dying && ceremony == CER_NONE && mins) {
    uint32_t m = dyingMin + mins;
    dyingMin = m > REVIVE_LIMIT_MIN ? REVIVE_LIMIT_MIN : (uint8_t)m;
  }
  if (mins < 2 || busyEnding() || starterPick || vacant) return;  // aun eligiendo inicial o sin nadie
  if (mins > OFFLINE_MAX_MIN) mins = OFFLINE_MAX_MIN;  // tope: 2 semanas
  if (yardEgg()) eggClockStamp();  // puesto sin hora: cuenta desde la ultima vista

  uint32_t offlineAwake = 0;  // minutos despierto de este repaso (una caca cada OFFLINE_POOP_EVERY_MIN)
  for (uint32_t i = 0; i < mins; i++) {
    bool full = sated();  // la tarta (v1.54) tambien cuenta apagado
    ageMinutes++;
    if (isEgg()) {
      // el del patio (Y3): a su hora real; el inicial o uno del sistema de antes, a los 3 min
      if (yardEgg() ? eggMinutesLeft(seen + (i + 1) * 60) == 0 : ageMinutes >= STARTER_EGG_MIN) hatch();  // eclosiona en tu ausencia
      continue;
    }
    growMin++;  // crece igual apagado que en vivo, dormido o despierto
    // la noche (v1.63) tambien pasa apagada: a las BED_HOUR se duerme y a las WAKE_HOUR se despierta
    // (sin sonido: no hay nadie mirando). La hora de este minuto sale de la ultima vista
    int h = hourAt(seen + (i + 1) * 60);
    nightMinute(h, false);
    if (sleeping) {  // descanso: baja lento y con suelo, igual que en vivo
      sleepMinute(full, h);
      continue;
    }
    // v3.20: el gimnasio y el juguete tambien apagada, con los topes del dia de ESTE minuto (no el de hoy)
    homeMinute(h, (seen + (i + 1) * 60) / 86400, false);
    if (!full) fullness = dropTo(fullness, FOOD_DRAIN, OFFLINE_FLOOR);
    if (ageMinutes % ENE_DRAIN_EVERY == 0) energy = dropTo(energy, ENE_DRAIN, OFFLINE_FLOOR);
    if (ageMinutes % JOY_DRAIN_EVERY == 0) joy = dropTo(joy, JOY_DRAIN, OFFLINE_FLOOR);
    if (fullness < JOY_HUNGRY_BELOW) joy = dropTo(joy, JOY_DRAIN_HUNGRY, OFFLINE_FLOOR);
    // v3.1: las cacas se hacen dentro del repaso (para que tengan su edad) y las viejas quitan felicidad
    // (con el mismo suelo). Durmiendo no ensucia ni pesa: el sueno ya hizo continue
    offlineAwake++;
    if (offlineAwake % OFFLINE_POOP_EVERY_MIN == 0 && poops < POOP_MAX) addPoop();
    uint8_t stale = stalePoops();
    if (stale && ageMinutes % POOP_JOY_EVERY == 0) joy = dropTo(joy, POOP_JOY_DRAIN[stale], OFFLINE_FLOOR);
  }
  if (!isEgg()) {
    // la evolucion NO se aplica offline: queda lista y la dispara el usuario
    // tocando al bicho cuando vuelve (para que vea la transformacion)
  }
  Serial.printf("offline: hueco %u, %u min aplicados (nv.%u)\n", focusSlot, mins, level());
}

void Pet::update(uint32_t nowMs) {
  // fin de ceremonia: la criatura se va y su hueco se queda vacio (v1.41, ver endLife). v3.0: el de cada hueco
  bool ended = false;
  for (uint8_t s = 0; s < HOME_SLOTS; s++) {
    if (home(s).ceremony == CER_NONE || timeLeft(home(s).ceremonyUntil)) continue;
    FocusGuard g(*this, s);
    if (!g.ok) continue;
    endLife(ceremony);
    ended = true;
  }
  if (ended) return;
  while (nowMs - lastTick >= PET_TICK_MS) {
    lastTick += PET_TICK_MS;
    tick();
  }
}

void Pet::tick() {
  travelCheck();  // Y5: el viaje de un bicho de la caja (no depende de los de casa)
  encCheck();     // v2.2: el encuentro salvaje del dia (del jugador: tampoco depende de los de casa)
  bool counts = false;
  for (uint8_t s = 0; s < HOME_SLOTS; s++) {  // v3.0: cada hueco de casa, su minuto
    FocusGuard g(*this, s);
    if (!g.ok) continue;
    counts |= tickHome();
  }
  // v3.4: las medallas del jugador (un nivel cruzado, el vinculo a 100...): se pagan en RAM y marcan el
  // guardado, sin escribir
  medalsUpdate();
  // autoguardado periodico: NO escribir a flash aqui (corre dentro del loop,
  // mientras se anima); solo marcar y dejar que el loop lo vuelque al atenuar
  if (counts && ++ticksSinceSave >= 5) pendingSave = true;
}

// el minuto de un hueco de casa (el del foco). true: cuenta para el autoguardado periodico
bool Pet::tickHome() {
  // muriendose (v1.64): solo corre la hora del Revivir; al acabarse, la UI dice que murio
  if (dying && ceremony == CER_NONE && dyingMin < REVIVE_LIMIT_MIN) {
    if (++dyingMin >= REVIVE_LIMIT_MIN) {
      diedNow = true;
      pendingSave = true;
      return false;
    }
    return true;
  }
  if (busyEnding()) return false;  // el tiempo se detiene en la despedida
  if (vacant) return false;        // hueco vacio: nadie que envejezca ni que eclosione
  if (starterPick) return false;   // la partida no empieza hasta elegir inicial: si el
                                   // tiempo corriera aqui, el huevo eclosionaria solo a
                                   // los 3 min con la especie sorteada y se perderia la
                                   // eleccion del jugador
  bool full = sated();  // la tarta (v1.54): este minuto la comida no baja
  ageMinutes++;

  if (isEgg()) {
    if (yardEgg()) {  // el del patio (Y3): a las 2 h reales menos lo que le quitaron las sacudidas
      eggClockStamp();
      if (eggMinutesLeft() == 0) hatch();
    } else if (ageMinutes >= STARTER_EGG_MIN) {
      hatch();  // si no lo tocas, eclosiona solo a los 3 min
    }
    return false;
  }
  growMin++;  // el nivel sale de aqui (growth.h), despierto o dormido
  int h = localHour();
  nightMinute(h, true);  // la noche (v1.63): se duerme o se despierta solo

  // el sueño es descanso: la energia se recupera y las necesidades bajan MUCHO
  // mas lento que despierto y con suelo (amanece pidiendo algo de mimo, no a
  // cero, sin descuidos ni escapadas). despierto: comida -FOOD_DRAIN/min, energia/joy cada *_EVERY min.
  // El peso aun se quema; la racha de buen cuidado (goodTicks) queda en pausa.
  if (sleeping) {
    sleepMinute(full, h);
    if (weight > 0 && ageMinutes % WEIGHT_BURN_SLEEP_MIN == 0) weight--;
    return true;  // (las medallas, al final de tick(): aun puede cruzar un nivel por edad mientras duerme)
  }
  homeMinute(h, today(), true);  // v3.20: el gimnasio y el juguete (con las necesidades de antes de este minuto)
  // la subida de nivel ya no suena aqui: la detecta la UI (fx.h) por cualquier via (minuto,
  // tiempo apagado, Caramelo Raro, LVL) y la anuncia con su propio sonido

  if (!full) fullness = clamp100(fullness - FOOD_DRAIN);
  if (fullness > POOP_MIN_FOOD && poops < POOP_MAX && random(100) < POOP_ODDS) addPoop();

  // la energia baja cada ENE_DRAIN_EVERY min; el sobrepeso da pereza: cae el doble (en el mismo toque)
  if (ageMinutes % ENE_DRAIN_EVERY == 0) {
    energy = clamp100(energy - ENE_DRAIN);
    if (weight > OVERWEIGHT_AT) energy = clamp100(energy - OVERWEIGHT_ENE_DRAIN);
  }
  if (weight > 0 && ageMinutes % WEIGHT_BURN_AWAKE_MIN == 0) weight--;

  // la disciplina forja la defensa: 12 h seguidas bien cuidado = +1 DEF
  if (lowestStat() >= GOOD_CARE_MIN_NEED) {
    if (++goodTicks >= GOOD_CARE_MIN) {
      goodTicks = 0;
      if (trDef < 100) trDef++;
    }
  } else {
    goodTicks = 0;
  }

  int dJoy = ageMinutes % JOY_DRAIN_EVERY == 0 ? -JOY_DRAIN : 0;
  if (fullness < JOY_HUNGRY_BELOW) dJoy -= JOY_DRAIN_HUNGRY;
  // v3.1: las cacas viejas (mas de POOP_STALE_MIN min en el suelo) quitan felicidad; mas cacas, mas rapido
  uint8_t stale = stalePoops();
  if (stale && ageMinutes % POOP_JOY_EVERY == 0) dJoy -= POOP_JOY_DRAIN[stale];
  joy = clamp100(joy + dJoy);

  // Descuido: dejar una estadistica por los suelos cuenta como error de
  // cuidado (con enfriamiento para no contar el mismo descuido cada minuto)
  if (mistakeCooldown > 0) mistakeCooldown--;
  if (lowestStat() <= SLIP_AT && mistakeCooldown == 0) {
    careMistakes++;
    mistakeCooldown = SLIP_COOLDOWN_MIN;
    bond = bond > SLIP_BOND ? bond - SLIP_BOND : (bond > 1 ? 1 : bond);  // el descuido enfria el vinculo, pero sin arrasarlo:
                           // a -3 cada 30 min se perdia mucho mas de lo que se
                           // podia ganar en todo un dia y el vinculo se atascaba
  }

  // la evolucion la dispara el usuario (canEvolveNow + tap), no el tick. Las medallas, al final de tick()

  // abandono (v1.60): con TODO a cero el vinculo se apaga (10 cada 3 min: de 100 a 0 en 30) y, ya a
  // cero, en DEATH_MIN minutos mas se muere. Un solo cuidado para la cuenta; lo perdido no vuelve
  if (fullness == 0 && joy == 0 && energy == 0) {
    if (bond > 0) {
      uint8_t d = (neglectMin % 3 == 0) ? 4 : 3;
      bond = bond > d ? bond - d : 0;
      if (neglectMin < 255) neglectMin++;
    } else if (++deathMin >= DEATH_MIN) {
      startDying();
      return false;
    }
  } else {
    neglectMin = deathMin = dyingMin = 0;
  }

  // ciclo completo (forma final + 3 dias): la despedida NO salta sola; queda
  // lista (canFarewellNow) y la dispara el usuario con el boton, para que la vea

  return true;
}

// vuelca el guardado periodico pendiente (lo llama el loop en un momento sin
// animacion para que el paron de la escritura a flash no se vea)
void Pet::flushSave() {
  if (pendingSave) save();
}

// quedan miembros sin registrar en la linea evolutiva de esta base?
bool Pet::lineHasUnregistered(int16_t base) const {
  int16_t cur = base;
  for (int guard = 0; cur >= 1 && cur <= 151 && guard < 6; guard++) {
    if (!isRegistered(cur)) return true;
    if (cur == DEX_EEVEE) {
      for (int16_t b = 134; b <= 136; b++)
        if (!isRegistered(b)) return true;
      return false;
    }
    cur = DEX_TBL[cur].evolvesTo;
  }
  return false;
}

uint8_t PetLife::eggRarity() const {
  return (eggTarget >= 1 && eggTarget <= 151) ? DEX_TBL[eggTarget].rarity : R_COMUN;
}

// elige la especie del huevo: tirada de rareza (mejorada por una despedida
// completa, castigada por una escapada) y sesgo hacia lineas incompletas
int16_t Pet::pickEggSpecies() {
  // primera partida: inicial clasico
  if (registeredCount() == 0) {
    return CLASSIC_DEX[random(NUM_CLASSIC_DEX)];
  }

  uint8_t tier = R_COMUN;
  if (lastEnd != CER_RUNAWAY && lastEnd != CER_DEATH) {  // tras una escapada o una muerte, solo comunes
    bool blessed = (lastEnd == CER_FAREWELL);
    int rare = (blessed ? EGG_RARE_PCT_BLESSED : EGG_RARE_PCT) + careBonus();
    int leg = (registeredCount() >= EGG_LEG_MIN_REG) ? (blessed ? EGG_LEG_PCT_BLESSED : EGG_LEG_PCT) + careBonus() / 3 : 0;
    int r = random(100);
    if (r < leg) tier = R_LEGENDARIO;
    else if (r < leg + rare) tier = R_RARO;
  }

  // candidatos del tier con linea incompleta; si no hay, baja de tier;
  // si la pokedex del tier esta completa, vale cualquiera del tier
  for (int pass = 0; pass < 2; pass++) {
    for (int t = tier; t >= R_COMUN; t--) {
      int16_t cand[80];
      int n = 0;
      for (int16_t d = 1; d <= 151 && n < 80; d++) {
        if (DEX_TBL[d].rarity != t) continue;
        if (pass == 0 && !lineHasUnregistered(d)) continue;
        cand[n++] = d;
      }
      if (n > 0) return cand[random(n)];
    }
  }
  return CLASSIC_DEX[random(NUM_CLASSIC_DEX)];  // inalcanzable, por si acaso
}

void Pet::registerSpecies(int16_t dex, bool sh) {
  if (dex < 1 || dex > 151) return;
  dexReg[(dex - 1) >> 3] |= (1 << ((dex - 1) & 7));
  if (sh) dexShinyReg[(dex - 1) >> 3] |= (1 << ((dex - 1) & 7));
}

// la racha y el vinculo mejoran el sorteo del huevo (0..~14)
int Pet::careBonus() const {
  int s = streak > CARE_BONUS_STREAK_CAP ? CARE_BONUS_STREAK_CAP : streak;
  return s / CARE_BONUS_STREAK_DIV + bond / CARE_BONUS_BOND_DIV;
}

// primer cuidado del dia: avanza la racha y afianza el vinculo. saveNow=false
// para los eventos del IMU (la meta de pasos): marca el guardado y no escribe.
void Pet::registerCare(bool saveNow) {
  if (isEgg() || busyEnding()) return;
  uint32_t d = today();
  if (d == 0) return;  // sin reloj
  bool changed = false;
  // v3.0: el primer cuidado de ESTE bicho hoy: su vinculo (cada uno el suyo, aunque la racha ya contara)
  if (careDay != d) {
    careDay = d;
    if (bondDay != d) {  // el tope diario de vinculo es por dia (ver addBond)
      bondDay = d;
      bondToday = 0;
    }
    bond = clamp100(bond + BOND_CARE_DAY);  // fuera del tope diario de addBond
    changed = true;
  }
  // la racha (del jugador): el primer cuidado del dia, de quien sea
  if (d != lastCareDay) {
    if (lastCareDay == 0 || d == lastCareDay + 1) {
      streak++;
    } else {
      streak = 1;        // hubo un hueco de dias
      lastMilestone = 0;
    }
    lastCareDay = d;
    if (streak > bestStreak) bestStreak = streak;
    uint16_t ms = (streak >= 100) ? 100 : (streak >= 30) ? 30
                : (streak >= 7)   ? 7   : (streak >= 3)  ? 3 : 0;
    if (ms > lastMilestone) {
      lastMilestone = ms;
      milestoneNew = streak;  // lo celebra la UI cuando se pueda ver
    }
    changed = true;
  }
  if (!changed) return;
  persist(saveNow);
}

void Pet::addBond(uint8_t amt) {
  // el tope diario vuelve a 0 a medianoche: antes solo lo reiniciaban
  // registerCare() y hatch(), y el de ayer seguia valiendo hasta el primer cuidado
  uint32_t d = today();
  if (d && d != bondDay) {
    bondDay = d;
    bondToday = 0;
  }
  if (bondToday >= BOND_DAY_CAP) return;  // tope diario: el vinculo no se farmea
  if (amt > BOND_DAY_CAP - bondToday) amt = BOND_DAY_CAP - bondToday;  // sin pasarse del tope
  bond = clamp100(bond + amt);
  bondToday += amt;
}

// ---------- medallas del jugador (v3.4, medals.h) ----------

// a que medallas de bicho llega un individuo ahora
static uint8_t petReach(const PetLife &l) {
  if (!l.hatched() || l.speciesId > 151) return 0;
  return medalPetBits(l.level(), l.bond, DEX_TBL[l.speciesId].evolvesTo == 0, l.weight, l.careMistakes, l.berryKnown);
}
static uint8_t recReach(const PetRecord &r) {
  return medalPetBits(levelFor(r.growMin), r.bond, DEX_TBL[r.speciesId].evolvesTo == 0, r.weight, r.careMistakes,
                      (r.flags & REC_FAV6) != 0);
}

uint32_t Pet::medalValue(uint8_t i) const {
  if (i >= MEDAL_COUNT) return 0;
  switch (i) {
    case MD_DEX: return registeredCount();
    case MD_SHINY: {
      uint32_t n = 0;
      for (uint8_t b : dexShinyReg) n += (uint32_t)__builtin_popcount(b);
      return n;
    }
    case MD_STREAK: return bestStreak > streak ? bestStreak : streak;
    case MD_WALKER: return stepsTotal;
    case MD_STAGES: return badgeCount();
    case MD_SOUV: return souvenirCount();
    default: return pm.cnt[i];  // de bicho y de evento
  }
}

uint16_t Pet::medalTiersTotal() const {
  uint16_t n = 0;
  for (uint8_t i = 0; i < MEDAL_COUNT; i++) n += pm.tier[i];
  return n;
}

bool Pet::takeMedalNews(MedalNews *out) {
  if (!medalNewsN) return false;
  if (out) *out = medalNewsQ[0];
  for (uint8_t i = 1; i < medalNewsN; i++) medalNewsQ[i - 1] = medalNewsQ[i];
  medalNewsN--;
  return true;
}

void Pet::dbgMedalCount(uint8_t i, uint16_t v) {
  if (i >= MEDAL_COUNT || medalSource(i) == MSRC_DERIVED) return;
  pm.cnt[i] = v;
  pendingSave = true;
  medalsUpdate();
}

// Cuenta a los dos de casa (lo que aun no tenian contado) y paga cada tramo nuevo, todo en RAM: no escribe
// nunca (corre en el tick y al principio de save()). Un tramo pagado queda en pm.tier y no se vuelve a pagar;
// si la cartera esta llena entra lo que quepa (el banner dice lo que entro)
void Pet::medalsUpdate() {
  if (!pmReady) return;  // (hasta la migracion del arranque)
  for (uint8_t s = 0; s < HOME_SLOTS; s++) {
    PetLife &l = slotRef(s);
    if (!l.hatched() || !l.uid) continue;
    uint8_t fresh = pmCountPet(pm, l.counted, petReach(l));
    if (fresh) {
      Serial.printf("medals: hueco %u cuenta 0x%02X\n", s, fresh);
      pendingSave = true;
    }
  }
  for (uint8_t i = 0; i < MEDAL_COUNT; i++) {
    uint8_t t = medalTierOf(i, medalValue(i));
    if (t <= pm.tier[i]) continue;
    uint16_t want = medalPayout(pm.tier[i], t);
    pm.tier[i] = t;
    uint16_t got = addCoins(want, false);  // fuera del tope diario
    medalNewsPush(medalNewsQ, medalNewsN, MEDAL_COUNT, i, t, got);
    Serial.printf("medals: %u -> tramo %u, +%u monedas\n", i, t, got);
    pendingSave = true;
  }
}

// "pm" si cambio. Sin migrar aun no se escribe nada (true: no es un fallo)
bool Pet::savePm() {
  if (!pmReady) return true;
  pmSeal(pm);
  if (pmHave && memcmp(&pm, &pmSaved, sizeof(pm)) == 0) return true;
  if (prefs.putBytes("pm", &pm, sizeof(pm)) != sizeof(pm)) return false;
  pmSaved = pm;
  pmHave = true;
  return true;
}

// Primer arranque de la v3.4 (o "pm" danado): los dos de casa y cada bicho de la caja cuentan con lo que son
// ahora (nivel, vinculo, forma, en forma, favorita), las derivadas se leen del estado y las de evento empiezan
// a 0. Los de la caja apuntan sus bits en pm (pmLegacyAdd): al despertarlos no vuelven a contar. Despues, save()
// paga todos los tramos alcanzados de una vez (y deja sus banners). Un corte antes de "pm" lo repite entero
// desde cero (las monedas van detras: nunca se pagan dos veces)
void Pet::medalsMigrate() {
  pmClear(pm);
  medalNewsN = 0;
  for (uint8_t s = 0; s < HOME_SLOTS; s++) {
    PetLife &l = slotRef(s);
    if (!l.hatched() || !l.uid) continue;
    uint8_t bits = (uint8_t)((l.counted | petReach(l)) & PMB_ALL);  // (lo que ya trajera de otro intento, tambien)
    uint8_t none = 0;
    pmCountPet(pm, none, bits);
    l.counted = bits;
  }
  for (uint8_t k = 0; k < BOX_SLOTS; k++) {
    if (boxSt[k] != BOX_OK) continue;
    const PetRecord &r = boxRec[k];
    uint8_t bits = (uint8_t)((((r.flags & REC_V3) ? r.medals : 0) | recReach(r)) & PMB_ALL);
    uint8_t none = 0;
    pmCountPet(pm, none, bits);
    if (!pmLegacyAdd(pm, r.uid, bits)) Serial.printf("medals: sin sitio para el uid %08lx\n", (unsigned long)r.uid);
  }
  pmReady = true;
  pmMigrated = true;
  if (pmKeyBad) {
    // habia un "pm" pero no se pudo leer (danado o de otra version): se recuenta, pero lo ya alcanzado se da por
    // pagado (sin monedas ni banners). Solo una partida sin "pm" (la de antes de la v3.4) cobra lo atrasado
    for (uint8_t i = 0; i < MEDAL_COUNT; i++) pm.tier[i] = medalTierOf(i, medalValue(i));
    Serial.println("medals: pm danado, recontado sin pagar");
  }
  Serial.printf("medals: migracion, %u bichos de caja apuntados\n", pm.legN);
  save();  // medalsUpdate paga (sin "pm" antes); "pm" antes que "cn"
  // con "pm" ya escrito, las claves de las medallas de cada bicho (hasta la v3.3) sobran: se borran una vez
  if (pmHave) {
    pmKeyBad = false;
    if (prefs.isKey("medal")) prefs.remove("medal");
    if (prefs.isKey("tmedal")) prefs.remove("tmedal");
  }
}

void Pet::rename(const char *name) {
  strncpy(nick, name, sizeof(nick) - 1);
  nick[sizeof(nick) - 1] = 0;
  save();
}

static uint16_t calcStat(uint8_t base, uint8_t gene, uint16_t lvl, uint8_t tr) {
  return (uint16_t)base * gene / 100 + lvl + tr;
}

uint16_t PetLife::statAt(uint8_t which, uint16_t lv) const {
  if (isEgg()) return 0;
  const DexEntry &d = DEX_TBL[speciesId];
  if (which == 0) return calcStat(d.bAtk, geneAtk, lv, trAtk);
  if (which == 1) return calcStat(d.bDef, geneDef, lv, trDef);
  return calcStat(d.bSpe, geneSpe, lv, trSpe);
}

uint16_t PetLife::atkStat() const {
  return isEgg() ? 0 : calcStat(DEX_TBL[speciesId].bAtk, geneAtk, level(), trAtk);
}
uint16_t PetLife::defStat() const {
  return isEgg() ? 0 : calcStat(DEX_TBL[speciesId].bDef, geneDef, level(), trDef);
}
uint16_t PetLife::speStat() const {
  return isEgg() ? 0 : calcStat(DEX_TBL[speciesId].bSpe, geneSpe, level(), trSpe);
}

uint16_t Pet::registeredCount() const {
  uint16_t n = 0;
  for (int i = 1; i <= 151; i++)
    if (isRegistered(i)) n++;
  return n;
}

// forma final que ya cumplio su ciclo (3 dias): lista para despedirse. La
// despedida la dispara el usuario con el boton (no salta sola, para que la vea)
bool PetLife::canFarewellNow() const {
  return !isEgg() && !sleeping && !busyEnding() &&
         DEX_TBL[speciesId].evolvesTo == 0 && ageMinutes >= FAREWELL_AGE_MIN;
}

void Pet::startFarewell() {
  if (isEgg() || busyEnding()) return;
  lastEnd = CER_FAREWELL;
  ceremony = CER_FAREWELL;
  ceremonyUntil = millis() + CEREMONY_MS;
  heartUntil = ceremonyUntil;  // corazones durante toda la despedida
  sfxPlay(SFX_BYE);
  save();
}

// se muere de abandono (v1.60): el tiempo se para (tick) y la UI pregunta si usar un Revivir. El huevo
// que deja se sortea ya y se guarda: reiniciar no lo vuelve a tirar. Se guarda al momento: un corte de
// luz vuelve al dialogo, no al bicho vivo
void Pet::startDying() {
  if (isEgg() || busyEnding() || starterPick) return;
  dying = true;
  sleeping = false;
  wokeUp();
  neglectMin = deathMin = dyingMin = 0;
  deathEgg = 0;
  if (level() >= DEATH_EGG_LEVEL && random(100) < DEATH_EGG_ODDS) {
    uint8_t b = DEX_TBL[speciesId].base;
    deathEgg = b ? b : speciesId;  // la primera forma de su linea: nace bebe
  }
  sfxPlay(SFX_BYE);
  save();
}

// un Revivir lo salva: las necesidades a la mitad, el vinculo sigue a cero (el precio del abandono) y
// cuenta como un descuido
bool Pet::revive() {
  if (!dying || ceremony != CER_NONE || reviveExpired()) return false;  // v1.64: paso la hora
  if (items[IT_REVIVE]) items[IT_REVIVE]--;
  else if (!takeGift(IT_REVIVE)) return false;  // uno que aun esperaba envuelto tambien vale
  dying = false;
  deathEgg = 0;
  neglectMin = deathMin = dyingMin = 0;
  fullness = joy = energy = REVIVE_NEEDS;
  if (careMistakes < 255) careMistakes++;
  mistakeCooldown = SLIP_COOLDOWN_MIN;
  heartUntil = millis() + HEART_MS;
  save();
  return true;
}

// sin Revivir (o sin querer usarlo): la ceremonia del final. dying sigue hasta endLife: un corte de
// luz durante la ceremonia (que no se guarda) vuelve al dialogo
void Pet::letGo() {
  if (!dying || ceremony != CER_NONE) return;
  ceremony = CER_DEATH;
  ceremonyUntil = millis() + CEREMONY_MS;
  sfxPlay(SFX_BYE);
}

void Pet::dbgNeglect(uint16_t m) {
  if (isEgg() || busyEnding()) return;
  fullness = joy = energy = 0;
  if (m <= BOND_FADE_MIN) {
    neglectMin = m;
    deathMin = 0;
    uint16_t lost = (m / 3) * 10 + ((m % 3) ? 4 : 0) + ((m % 3) == 2 ? 3 : 0);
    bond = 100 > lost ? 100 - lost : 0;
  } else {
    neglectMin = BOND_FADE_MIN;
    bond = 0;
    deathMin = (m - BOND_FADE_MIN) > DEATH_MIN ? DEATH_MIN : m - BOND_FADE_MIN;
  }
  save();
}

void Pet::release() {
  if (isEgg() || busyEnding()) return;
  lastEnd = CER_RELEASE;
  ceremony = CER_RELEASE;
  ceremonyUntil = millis() + CEREMONY_MS;
  heartUntil = ceremonyUntil;
  sfxPlay(SFX_BYE);
  save();
}

void Pet::hatch() {
  speciesId = eggTarget;
  eggCap = false;  // v2.2: ya nacio
  eggPlaced = 0;  // el huevo del patio ya no esta
  eggSaved = 0;
  shiny = eggShiny;
  // genes del individuo: 90-110% por stat (cada crianza es unica)
  geneAtk = GENE_MIN + random(GENE_MAX - GENE_MIN + 1);
  geneDef = GENE_MIN + random(GENE_MAX - GENE_MIN + 1);
  geneSpe = GENE_MIN + random(GENE_MAX - GENE_MIN + 1);
  trAtk = trDef = trSpe = 0;
  berryKnown = false;
  satedUntil = 0;    // la tarta es del individuo: la edad vuelve a empezar
  bond = 0;          // vinculo, medallas y nombre son del individuo
  bondToday = 0;
  bondDay = today();
  careDay = 0;       // v3.0: su primer cuidado de hoy aun no lo tuvo
  counted = 0;       // v3.4: aun no hizo subir ninguna medalla del jugador
  nick[0] = 0;
  uid = newUid();    // identidad del individuo (caja, v1.37)
  registerSpecies(speciesId, shiny);  // criado = registrado en la pokedex
  // (las medallas del jugador, en save(): por si nace ya en forma final, un legendario)
  // sin sonido aqui: la UI ve la eclosion y suena en el destello de su animacion (fx.h), o
  // al momento si la pantalla no estaba a la vista
  save();
}

// ¿se dan ya las condiciones para evolucionar? Cada descuido retrasa la
// evolucion 1 nivel, y ademas tiene que estar bien cuidado en ese momento
// (ninguna estadistica por debajo de 40). NO evoluciona sola: la dispara el
// usuario tocando al bicho (evolve()), para que vea la transformacion.
bool PetLife::canEvolveNow() const {
  if (isEgg() || sleeping || busyEnding()) return false;
  const DexEntry &d = DEX_TBL[speciesId];
  if (d.evolvesTo == 0) return false;
  return level() >= (uint16_t)d.evolveLevel + careMistakes && lowestStat() >= EVOLVE_MIN_NEED;
}

void Pet::evolve() {
  if (!canEvolveNow()) return;
  const DexEntry &d = DEX_TBL[speciesId];
  prevSpeciesId = speciesId;
  int16_t next = d.evolvesTo;
  if (speciesId == DEX_EEVEE) {
    // rama de Eevee: prefiere la evolucion que falte en la pokedex
    int16_t opts[3];
    int n = 0;
    for (int16_t b = 134; b <= 136; b++)
      if (!isRegistered(b)) opts[n++] = b;
    next = n > 0 ? opts[random(n)] : (int16_t)(134 + random(3));
  }
  speciesId = next;  // la baya favorita sale de la forma base (v3.2): no cambia al evolucionar
  registerSpecies(speciesId, shiny);
  sfxPlay(SFX_EVOLVE);
  evolveUntil = millis() + EVOLVE_ANIM_MS;
  pmBump(pm, MD_EVOLVE);  // v3.4: save() escribe la especie nueva antes que "pm" (un corte: no cuenta)
  save();
}

void Pet::feed() {
  feedFood(FOOD_PLAIN);
}

// v3.2: ¿puede comerse esa comida ahora? Despierto, no huevo, y con su objeto en la mochila (la baya normal, no)
bool Pet::canFeed(uint8_t food) const {
  if (food >= FOOD_COUNT || busyEnding() || isEgg() || sleeping) return false;
  uint8_t it = foodItem(food);
  return it == FOOD_NO_ITEM || items[it] > 0;
}

// da de comer: gasta el objeto SOLO si se aplica (sin bicho que coma, no se gasta nada). La comida del patio
// llega aqui cuando el bicho se la come, nunca al soltarla
bool Pet::feedFood(uint8_t food) {
  if (!canFeed(food)) return false;
  uint8_t it = foodItem(food);
  if (it != FOOD_NO_ITEM) items[it]--;
  FoodEffect e = foodEffect(food, foodIsBerry(food) && lovesBerry(food - FOOD_BERRY0));
  fullness = clamp100(fullness + e.fullness);
  if (food == FOOD_CAKE) {
    fullness = 100;
    satedUntil = ageMinutes + CAKE_SATED_MIN;  // lleno 3 h (tambien apagado: syncClock)
    weight = clamp100(weight + CAKE_WEIGHT);   // como la chuche a la que sustituye, pasa factura
  }
  if (e.joy) joy = clamp100(joy + e.joy);
  if (foodIsBerry(food) && e.heart) berryKnown = true;  // la favorita: descubierta, se muestra en la ficha
  if (e.bond) addBond(e.bond);
  eatUntil = millis() + EAT_ANIM_MS;
  if (e.heart) heartUntil = millis() + HEART_MS;  // "le encanta!"
  registerCare();
  save();
  return true;
}

bool Pet::eatCake() {
  return feedFood(FOOD_CAKE);
}

uint8_t Pet::playResult(uint8_t score) {
  if (busyEnding() || isEgg()) return 0;
  uint8_t antes = trSpe;
  uint8_t v = trSpe + score / BALL_SPE_DIV;  // jugar entrena la velocidad
  trSpe = v > 100 ? 100 : v;
  joy = clamp100(joy + BALL_JOY_BASE +
                 (score > BALL_JOY_BONUS_MAX / BALL_JOY_PER_POINT ? BALL_JOY_BONUS_MAX : score * BALL_JOY_PER_POINT));
  energy = dropTo(energy, BALL_ENE_BASE + score / BALL_ENE_DIV, GAME_FLOOR);
  if (!sated()) fullness = dropTo(fullness, GAME_FOOD_COST, GAME_FLOOR);  // con tarta (v1.54) jugar no da hambre
  int burn = (int)weight - score / GAME_WEIGHT_DIV;  // el ejercicio quema peso
  weight = burn > 0 ? burn : 0;
  if (score >= 5) heartUntil = millis() + HEART_MS;
  if (score > gameHi) gameHi = score;  // nuevo record
  lastCoins = addCoins(coinsForGame(ECO_BALL, score), true);
  addBond(GAME_BOND);
  registerCare();
  save();
  return trSpe - antes;  // lo que subio VEL (tras el tope de 100), para la pantalla de resultado
}

// saco de entrenamiento: los golpes entrenan la fuerza. Devuelve la subida.
uint8_t Pet::trainStrength(uint16_t hits) {
  if (busyEnding() || isEgg()) return 0;
  uint8_t gain = hits / SACK_HITS_PER_POINT;      // ~4 golpes = 1 punto de entrenamiento
  if (gain > SACK_GAIN_MAX) gain = SACK_GAIN_MAX;  // tope por sesion: la FUE se forja a fuego lento
  uint8_t antes = trAtk;
  uint16_t v = (uint16_t)trAtk + gain;
  trAtk = v > 100 ? 100 : (uint8_t)v;
  gain = trAtk - antes;             // lo que de verdad entro: al topar en 100 la
                                    // pantalla anunciaba +18 aunque cupieran menos
  energy = dropTo(energy, SACK_ENE_COST, GAME_FLOOR);   // cansa
  if (!sated()) fullness = dropTo(fullness, GAME_FOOD_COST, GAME_FLOOR);  // con tarta (v1.54) jugar no da hambre
  int burn = (int)weight - hits / TRAIN_WEIGHT_DIV;  // tambien quema peso
  weight = burn > 0 ? burn : 0;
  joy = clamp100(joy + SACK_JOY);
  if (hits >= 20) heartUntil = millis() + HEART_MS;
  if (hits > strHi) strHi = hits;   // record de golpes
  lastCoins = addCoins(coinsForGame(ECO_SACK, hits), true);
  addBond(GAME_BOND);
  registerCare();
  save();
  return gain;
}

// cascada (ZEN, docs/NEW_GAMES.md FR-ZEN.12): solo se llama al acabar la sesion entera (abandonar
// no da nada). La concentracion entrena la defensa y el agua lava al bicho. Sentado no quema
// peso. Devuelve los puntos de DEF que de verdad entraron.
uint8_t Pet::zenResult(uint16_t focusTenths, uint16_t driftTenths) {
  if (busyEnding() || isEgg()) return 0;
  uint8_t antes = trDef;
  uint16_t v = (uint16_t)trDef + zenTotalGain(focusTenths, driftTenths);
  trDef = v > 100 ? 100 : (uint8_t)v;
  uint8_t gain = trDef - antes;
  energy = dropTo(energy, ZEN_ENE_COST, GAME_FLOOR);
  if (!sated()) fullness = dropTo(fullness, ZEN_FOOD_COST, GAME_FLOOR);
  joy = clamp100(joy + ZEN_JOY);
  clearPoops();  // v3.1: la cascada lava: sin cacas (ya no hay limpieza que subir)
  if (focusTenths >= 200) heartUntil = millis() + HEART_MS;
  if (focusTenths > zenHi) zenHi = focusTenths;
  lastCoins = addCoins(coinsForGame(ECO_ZEN, zenTotalGain(focusTenths, driftTenths)), true);
  addBond(GAME_BOND);
  registerCare();
  save();
  return gain;
}

// carrera en trineo (DASH, docs/NEW_GAMES.md FR-DASH.17): las filas esquivadas entrenan la
// velocidad (con el tope del saco) y queman algo de peso. Devuelve los puntos que de verdad entraron.
uint8_t Pet::dashResult(uint16_t dodged) {
  if (busyEnding() || isEgg()) return 0;
  uint8_t antes = trSpe;
  uint16_t v = (uint16_t)trSpe + dashGain(dodged);
  trSpe = v > 100 ? 100 : (uint8_t)v;
  uint8_t gain = trSpe - antes;
  energy = dropTo(energy, DASH_ENE_COST, GAME_FLOOR);
  if (!sated()) fullness = dropTo(fullness, GAME_FOOD_COST, GAME_FLOOR);  // con tarta (v1.54) jugar no da hambre
  int burn = (int)weight - dodged / DASH_WEIGHT_DIV;
  weight = burn > 0 ? burn : 0;
  joy = clamp100(joy + DASH_JOY_BASE + (dodged > DASH_JOY_ROWS_MAX ? DASH_JOY_ROWS_MAX : dodged) * DASH_JOY_PER_ROW);
  if (dodged >= 10) heartUntil = millis() + HEART_MS;
  if (dodged > dashHi) dashHi = dodged;
  lastCoins = addCoins(coinsForGame(ECO_SLED, dodged), true);
  addBond(GAME_BOND);
  registerCare();
  save();
  return gain;
}

void Pet::play() {
  if (busyEnding()) return;
  if (isEgg() || sleeping) return;
  joy = clamp100(joy + 25);
  energy = clamp100(energy - 10);
  if (!sated()) fullness = clamp100(fullness - 5);
  heartUntil = millis() + HEART_MS;
  addBond(2);
  registerCare();
  save();
}

void Pet::toggleLight() {
  if (busyEnding()) return;
  if (isEgg()) return;
  if (sleeping) {
    sleeping = false;
    wokeUp();
    upgSleepReset();  // v3.20: despertarlo a mano no da los buenos dias
  } else {
    goToBed();
  }
  save();
}

// --- la noche (v1.63)
// un minuto dormido: la energia vuelve y las necesidades bajan lento y con suelo. De dia (una
// siesta) la comida baja NAP_FOOD_JOY_EVERY veces mas lento que despierto; de noche, NIGHT_FOOD_JOY_EVERY, con suelos mas altos
void Pet::sleepMinute(bool full, int h) {
  energy = clamp100(energy + BED_ENE_GAIN[upgTier(UPG_BED)]);  // v3.20: la cama (nivel 0 = SLEEP_ENE_GAIN)
  // v3.20: los minutos de esta noche, para los buenos dias de la cama (con tope: pasado el minimo ya no cambia)
  if (nightSleep)
    if (UpgSlot *u = upgSlotNow())
      if (u->sleptMin < BED_MORNING_MIN_SLEEP) u->sleptMin++;
  if (nightHour(h)) {
    if (ageMinutes % NIGHT_FOOD_JOY_EVERY == 0) {
      if (!full) fullness = dropTo(fullness, 1, NIGHT_FOOD_FLOOR);
      joy = dropTo(joy, 1, NIGHT_JOY_FLOOR);
    }
    return;
  }
  if (ageMinutes % NAP_FOOD_JOY_EVERY == 0) {
    if (!full) fullness = dropTo(fullness, 1, NAP_FOOD_FLOOR);
    joy = dropTo(joy, 1, NAP_JOY_FLOOR);
  }
}

// acostarlo a mano (la luz o boca abajo). Desde EVENING_HOUR es dormir de noche: se despertara solo
// a las WAKE_HOUR. Con sueño (desde SLEEPY_HOUR), la primera vez de esa noche da BED_BOND de vinculo
void Pet::goToBed() {
  sleeping = true;
  nightAwakeMin = 0;
  upgSleepReset();  // v3.20: la noche empieza a contar ahora
  int h = localHour();
  nightSleep = h >= EVENING_HOUR || (h >= 0 && h < WAKE_HOUR);
  uint32_t n = nightId(lastSeenEpoch);
  if (nightHour(h) && n && bedNight != n) {
    bedNight = n;
    bond = clamp100(bond + BED_BOND);  // fuera del tope diario de addBond, como BOND_CARE_DAY
  }
}

// un minuto de la noche, en vivo (tick) o apagado (syncClock, live = false). h = -1 sin reloj
void Pet::nightMinute(int h, bool live) {
  int prev = lastTickHour;
  lastTickHour = (int8_t)h;
  if (!sleeping) upgSleepReset();  // v3.20: despierto (por lo que sea), la noche de la cama no sigue contando
  if (h < 0 || isEgg() || busyEnding() || starterPick) {
    nightAwakeMin = 0;
    return;
  }
  if (sleeping) {
    nightAwakeMin = 0;
    if (bedHour(h)) nightSleep = true;  // una siesta que llega a la noche tambien amanece a las WAKE_HOUR
    // por la mañana: se despierta solo (una siesta de tarde no). Solo de WAKE_HOUR a EVENING_HOUR:
    // acostado a las 20-21 ya es dormir de noche y no debe despertarse al minuto siguiente
    if (nightSleep && h >= WAKE_HOUR && h < EVENING_HOUR) {
      // v3.20: los buenos dias de la cama, tras una noche entera (BED_MORNING_MIN_SLEEP), una vez: la cuenta
      // vuelve a 0. Despertado a mano o agitandolo no llega aqui
      uint8_t bt = upgTier(UPG_BED);
      UpgSlot *u = upgSlotNow();
      if (bt && u && u->sleptMin >= BED_MORNING_MIN_SLEEP) {
        uint8_t before = joy;
        joy = clamp100(joy + BED_MORNING_JOY[bt]);
        if (live) homeEvts.push(HE_MORNING, focusSlot & 1, (uint8_t)(joy - before));
      }
      if (u) u->sleptMin = 0;
      sleeping = false;
      nightSleep = false;
      if (live) morningWake = true;
      pendingSave = true;
    }
    return;
  }
  if (!bedHour(h)) {
    nightAwakeMin = 0;
    return;
  }
  // al llegar las BED_HOUR (o al encender la placa en esa hora), ya; despertado de noche, a los
  // NIGHT_RESLEEP_MIN minutos despierto. Con un juego abierto (nightHold) espera a que acabe
  if (h == BED_HOUR && prev != BED_HOUR) nightAwakeMin = NIGHT_RESLEEP_MIN;
  else if (nightAwakeMin < NIGHT_RESLEEP_MIN) nightAwakeMin++;
  if (nightHold || nightAwakeMin < NIGHT_RESLEEP_MIN) return;
  sleeping = true;
  nightSleep = true;
  nightAwakeMin = 0;
  upgSleepReset();     // v3.20: la noche empieza a contar ahora
  pendingSave = true;  // sin vinculo: no lo acostaste tu
}

void Pet::addPoop() {
  if (poops >= POOP_MAX) return;
  uint8_t m = poopSlots();
  uint8_t i = 0;
  while (m & (1 << i)) i++;  // el primer hueco libre
  poopMask = m | (1 << i);
  poopAt[i] = ageMinutes;  // v3.1: su edad: de ella sale lo vieja que es (stalePoops)
  poops++;
}

// v1.58: una caca, de una en una. Cuenta como cuidado del dia (racha), pero sin vinculo; no escribe en flash a cada toque
bool Pet::cleanPoop(uint8_t slot) {
  if (busyEnding() || isEgg() || slot >= POOP_MAX) return false;
  uint8_t m = poopSlots();
  if (!(m & (1 << slot))) return false;
  poopMask = m & ~(1 << slot);
  poops--;
  poopAt[slot] = ageMinutes;
  registerCare(false);
  pendingSave = true;
  return true;
}

// v3.1: sin cacas (la cascada entera): cuenta, mascara y marcas de edad
void Pet::clearPoops() {
  poops = 0;
  poopMask = 0;
  for (uint8_t i = 0; i < POOP_MAX; i++) poopAt[i] = ageMinutes;
}

void Pet::caress() {
  if (busyEnding()) return;
  if (isEgg() || sleeping) return;
  joy = clamp100(joy + CARESS_JOY);
  heartUntil = millis() + HEART_MS;
  // un toque en el bicho es gratis: sin enfriamiento, 10 toques llenaban el tope del dia
  if (!timeLeft(petBondUntil)) {
    addBond(CARESS_BOND);
    petBondUntil = millis() + PET_BOND_COOLDOWN_MS;
  }
  pendingSave = true;  // la felicidad (v2.3: el mimo del panel Cuidar)
  registerCare();
}

void Pet::eggTap() {
  if (!isEgg() || vacant) return;
  if (yardEgg()) return;  // el del patio no se abre a toques (Y3): solo se tambalea (la UI)
  if (++eggTaps >= STARTER_EGG_TAPS) hatch();
  else save();
}

// ---------- pasos, hallazgos y mochila (docs/FEATURES.md, Tarea 1) ----------

// dia nuevo: los contadores de hoy vuelven a 0 (el total no)
void Pet::rollStepDay(uint32_t day) {
  if (!day || day == stepDay) return;
  stepDay = day;
  stepsToday = 0;
  findsToday = 0;
  bonusFindsToday = 0;
  stepJoyToday = 0;
  pendingSave = true;
}

static uint16_t nextFindAt() { return FIND_MIN_STEPS + random(FIND_MAX_STEPS - FIND_MIN_STEPS + 1); }
static uint16_t nextEggAt() { return EGG_MIN_STEPS + random(EGG_MAX_STEPS - EGG_MIN_STEPS + 1); }

// Pasos contados por el IMU (o el comando STEPS). Se suman siempre a hoy y al
// total; los premios (FR-1.3) y los hallazgos solo con el bicho nacido y
// despierto. Caminar no gasta energia: una bajada a 10 contaria como descuido.
// Los restos se guardan, asi que da igual en cuantos trozos lleguen los pasos.
void Pet::addSteps(uint32_t n, uint32_t day) {
  rollStepDay(day);
  if (!n) return;
  stepsToday += n;
  stepsTotal += n;
  pendingSave = true;  // nunca save(): la escritura a flash congela ~1 s

  // v3.0: lo de caminar llega a cada bicho de casa nacido y despierto (los dos con lo mismo, cada uno con
  // su tope de vinculo); el tope diario de felicidad y los hallazgos son del jugador (una vez)
  bool rewarded[HOME_SLOTS];
  bool anyRewarded = false;
  for (uint8_t s = 0; s < HOME_SLOTS; s++) {
    const PetLife &l = home(s);
    rewarded[s] = !l.isEgg() && !l.sleeping && !l.busyEnding() && !starterPick;
    anyRewarded |= rewarded[s];
  }
  if (anyRewarded) {
    uint32_t j = joyRem + n;
    joyRem = j % STEPS_PER_JOY;
    uint32_t gain = j / STEPS_PER_JOY;
    uint32_t room = stepJoyToday < STEP_JOY_DAY_CAP ? STEP_JOY_DAY_CAP - stepJoyToday : 0;
    if (gain > room) gain = room;
    stepJoyToday += gain;

    uint32_t w = weightRem + n;
    weightRem = w % STEPS_PER_WEIGHT;
    uint32_t burn = w / STEPS_PER_WEIGHT;

    uint32_t b = bondRem + n;
    bondRem = b % STEPS_PER_BOND;
    uint32_t bonds = b / STEPS_PER_BOND;

    for (uint8_t s = 0; s < HOME_SLOTS; s++) {
      if (!rewarded[s]) continue;
      FocusGuard g(*this, s);
      if (!g.ok) continue;
      joy = clamp100(joy + (int)gain);
      weight = burn >= weight ? 0 : weight - burn;
      for (uint32_t k = bonds; k > 0; k--) addBond(1);  // el tope diario lo pone addBond (el de cada uno)
    }

    if (!findAt) findAt = nextFindAt();
    uint32_t f = (uint32_t)findRem + n;
    while (f >= findAt && randomFindsToday() < FINDS_PER_DAY) {
      f -= findAt;
      findItem();
      findAt = nextFindAt();
    }
    if (randomFindsToday() >= FINDS_PER_DAY) f = 0;  // tope del dia: lo que se ande despues no cuenta para el dia siguiente
    findRem = (uint16_t)f;
  }

  // huevos (v1.39): con todos los pasos, esten como esten los bichos, y sin tope diario. v3.0: con los dos
  // huecos de casa vacios, el huevo encontrado va derecho al primero (sin escribir aun: el loop guarda con
  // la pantalla atenuada); si no, envuelto. v1.40: solo el encontrado; los de la mochila esperan al boton
  if (!eggAt) eggAt = nextEggAt();
  uint32_t e = (uint32_t)eggRem + n;
  while (e >= eggAt) {
    e -= eggAt;
    eggAt = nextEggAt();
    if (home(0).isVacant() && home(1).isVacant()) {
      queueFound(IT_EGG, false, false);
      FocusGuard g(*this, 0);
      if (!g.ok) continue;
      newEgg(false);
    } else {
      giveGift(IT_EGG);  // envuelto en casa (v1.55); con la cola llena, a la mochila con aviso
    }
  }
  eggRem = (uint16_t)e;

  // metas diarias (FR-1.4, v1.25): 3000, 6000 y 10000, cada una una vez al dia y en orden.
  // Si unos pasos cruzan varias, saltan todas y el banner muestra la mas alta.
  if (day) {
    uint8_t tier = goalTierOn(day);
    while (tier < STEP_GOAL_TIERS && stepsToday >= STEP_GOALS[tier]) {
      tier++;
      goalDay = day;
      goalTier = tier;
      goalBanner = tier;
      goalBannerCoins += addCoins(coinsForGoal(tier), false);  // fuera del tope diario; con el bicho dormido tambien
      if (tier == STEP_GOAL_TIERS) pmBump(pm, MD_MARATHON);   // v3.4: un dia de 10000 pasos (del jugador, una vez al dia)
      goalReward(tier, rewarded);
    }
    encCheck();  // v2.2: los umbrales ENC_WALK_AT de pasos del dia dan otra tirada de encuentro
  }
  medalsUpdate();  // v3.4: los pasos y los niveles de las metas (sin escribir: marca el guardado)
}

// premio de cada meta. El banner y la racha (la primera) se dan siempre; lo demas, a cada bicho
// nacido y despierto, como el resto de premios de caminar. Los regalos (una vez) no cuentan
// para el tope de hallazgos al azar, y el vinculo va fuera del tope diario de addBond.
// v1.36: cada meta sube niveles (STEP_GOAL_LEVELS: +1, +1, +2), con la barra igual de llena.
void Pet::goalReward(uint8_t tier, const bool *rewarded) {
  uint8_t bestLv = 0;
  bool any = false;
  for (uint8_t s = 0; s < HOME_SLOTS; s++) {
    if (!rewarded[s]) continue;
    any = true;
    FocusGuard g(*this, s);
    if (!g.ok) continue;
    uint16_t before = level();
    growMin = addLevels(growMin, STEP_GOAL_LEVELS[tier - 1]);
    uint16_t gained = level() - before;  // menos en el tope (999)
    if (gained > bestLv) bestLv = (uint8_t)gained;
    if (tier == 1) {
      joy = clamp100(joy + GOAL_JOY);
    } else if (tier == 2) {
      joy = clamp100(joy + GOAL2_JOY);
      bond = clamp100(bond + GOAL2_BOND);
    } else {  // 10000: +2 niveles (arriba) en vez del Caramelo Raro de la v1.25
      joy = clamp100(joy + GOAL3_JOY);
      bond = clamp100(bond + GOAL3_BOND);
    }
  }
  goalBannerLv = (uint8_t)(goalBannerLv + bestLv);  // el banner: lo del que mas subio
  if (any && tier <= 2) {  // el regalo de la meta, una vez
    bonusFindsToday++;
    findItem();
  }
  // cuenta como dia de cuidado para la racha (y como primer cuidado de cada bicho)
  if (tier == 1)
    for (uint8_t s = 0; s < HOME_SLOTS; s++) {
      FocusGuard g(*this, s);
      if (!g.ok) continue;
      registerCare(false);
    }
}

// el bicho encuentra algo por el camino (FR-1.5 / FR-1.6)
void Pet::findItem() {
  findsToday++;
  long r = random(100);
  uint8_t it;
  if (r < ODDS_GOLDEN) it = IT_GOLDEN;
  else if ((r -= ODDS_GOLDEN) < ODDS_CAKE) it = IT_CAKE;
  else if ((r -= ODDS_CAKE) < ODDS_VITAMIN) it = IT_PROTEIN + random(3);
  else if ((r -= ODDS_VITAMIN) < ODDS_CHARM) it = IT_CHARM;
  else if ((r -= ODDS_CHARM) < ODDS_REVIVE) it = IT_REVIVE;
  else it = IT_BERRY_R + random(BERRY_COLORS);  // v3.2
  if (it == IT_CHARM && items[IT_CHARM]) it = IT_GOLDEN;  // solo se lleva un amuleto
  giveGift(it);  // v1.55: llega envuelto, se abre en casa
}

// ---------- regalos (v1.55) ----------
void Pet::giveGift(uint8_t it) {
  if (!addGift(it)) {
    Serial.printf("regalo %u: cola llena, a la mochila\n", it);
    giveFound(it);
  }
}

bool Pet::addGift(uint8_t it) {
  if (it >= IT_COUNT || giftN >= GIFT_MAX) return false;
  gifts[giftN++] = it;
  pendingSave = true;  // como los pasos: el loop guarda con la pantalla atenuada
  return true;
}

uint8_t Pet::revivesOwned() const {
  uint8_t n = items[IT_REVIVE];
  for (uint8_t i = 0; i < giftN; i++) n += gifts[i] == IT_REVIVE;
  return n;
}

bool Pet::takeGift(uint8_t it) {
  for (uint8_t i = 0; i < giftN; i++) {
    if (gifts[i] != it) continue;
    for (uint8_t j = i + 1; j < giftN; j++) gifts[j - 1] = gifts[j];
    giftN--;
    return true;
  }
  return false;
}

uint8_t Pet::giftEggs() const {
  uint8_t n = 0;
  for (uint8_t i = 0; i < giftN; i++) n += gifts[i] == IT_EGG;
  return n;
}

// el primero que quepa (v1.57, revision): uno que no cabe no tapa a los de detras. Si no cabe ninguno,
// se ve el primero ("mochila llena") y todos siguen envueltos
static bool giftFits(const uint8_t *items, uint8_t it) {
  if (it == IT_CHARM && items[IT_CHARM]) it = IT_GOLDEN;  // dos amuletos: solo se lleva uno
  return items[it] < itemCap(it);
}

uint8_t Pet::openGift(bool *full) {
  if (full) *full = false;
  if (!canOpenGift()) return IT_COUNT;
  uint8_t k = 0;
  while (k < giftN && !giftFits(items, gifts[k])) k++;
  if (k == giftN) {  // no cabe ninguno: se ve lo que es el primero y se queda envuelto
    uint8_t it = gifts[0];
    if (it == IT_CHARM && items[IT_CHARM]) it = IT_GOLDEN;
    if (full) *full = true;
    queueFound(it, true, true);
    return it;
  }
  uint8_t it = gifts[k];
  if (it == IT_CHARM && items[IT_CHARM]) it = IT_GOLDEN;
  items[it]++;
  for (uint8_t i = k + 1; i < giftN; i++) gifts[i - 1] = gifts[i];
  giftN--;
  heartUntil = millis() + HEART_MS;  // se alegra (sin premio: abrir no sube nada)
  queueFound(it, false, true);
  save();  // un toque: se guarda ya. La mochila va antes que "gifts": un corte deja uno de mas, nunca de menos
  return it;
}

void Pet::unwrapGifts() {
  uint8_t k = 0;
  for (uint8_t i = 0; i < giftN; i++) {
    uint8_t it = gifts[i];
    if (it == IT_CHARM && items[IT_CHARM]) it = IT_GOLDEN;
    uint8_t cap = itemCap(it);
    if (items[it] < cap) items[it]++;
    else gifts[k++] = gifts[i];  // no cabe: sigue envuelto
  }
  giftN = k;
}

void Pet::giveFound(uint8_t it, bool gift) {
  uint8_t cap = itemCap(it);
  bool full = items[it] >= cap;
  if (!full) items[it]++;
  queueFound(it, full, gift);
}

void Pet::queueFound(uint8_t it, bool full, bool gift) {
  // cola llena (muchos sin ver con la pantalla apagada): se pierde el aviso mas antiguo, no
  // el nuevo, que puede ser el regalo de una meta. El objeto ya esta en la mochila.
  if (foundN == FOUND_QUEUE) popFound();
  foundQ[foundN] = it;
  foundFull[foundN] = full;
  foundGift[foundN] = gift;
  foundN++;
}

void Pet::popFound() {
  if (!foundN) return;
  for (uint8_t i = 1; i < foundN; i++) {
    foundQ[i - 1] = foundQ[i];
    foundFull[i - 1] = foundFull[i];
    foundGift[i - 1] = foundGift[i];
  }
  foundN--;
}

uint16_t Pet::itemCount() const {
  uint16_t n = 0;
  for (uint8_t i = 0; i < IT_COUNT; i++) n += items[i];
  return n;
}

bool Pet::canUseItem(uint8_t it) const {
  // el amuleto, el huevo y el Revivir no se usan a mano: uno se lleva, otro va a casa cuando queda
  // libre, y el Revivir se ofrece cuando el bicho se muere (v1.59)
  // (v2.2) la Poke Ball tampoco: se lanza al ganar a un bicho salvaje. (v3.14) el Cebo no se usa en un bicho: es
  // del patio (useLure)
  if (it >= IT_COUNT || it == IT_CHARM || it == IT_EGG || it == IT_REVIVE || it == IT_BALL || it == IT_LURE || !items[it])
    return false;
  if (isEgg() || sleeping || busyEnding()) return false;
  if (it == IT_PROTEIN) return trAtk < 100;
  if (it == IT_IRON) return trDef < 100;
  if (it == IT_CARBOS) return trSpe < 100;
  if (it == IT_RARECANDY) return level() < 999;
  return true;
}

bool Pet::useItem(uint8_t it) {
  if (!canUseItem(it)) return false;
  if (foodOfItem(it) != FOOD_COUNT) return feedFood(foodOfItem(it));  // tarta, baya dorada y bayas de colores (v3.2)
  items[it]--;
  if (it == IT_RARECANDY) {
    // +1 nivel (v1.35): salta el crecimiento, no la edad, y la barra conserva lo que llevaba.
    // Puede abrir la evolucion y las medallas de nivel (en el save() de abajo).
    growMin = addLevels(growMin, RARE_CANDY_LEVELS);
    heartUntil = millis() + HEART_MS;
  } else {
    uint8_t &tr = (it == IT_PROTEIN) ? trAtk : (it == IT_IRON) ? trDef : trSpe;
    tr = clamp100(tr + VITAMIN_TRAIN);
  }
  registerCare();
  save();  // un toque en la pantalla, no un evento del IMU: se guarda ya
  return true;
}

// ---------- trucos del IMU (docs/FEATURES.md, Tarea 2) ----------

// agitarlo dormido con la pantalla encendida: se despierta de mal humor. Sin
// credito de cuidado; un segundo despertar en menos de 60 s no resta mas.
void Pet::wakeGrumpy() {
  bool any = false;
  for (uint8_t s = 0; s < HOME_SLOTS; s++) {  // v3.0: los dos de casa
    FocusGuard g(*this, s);
    if (!g.ok) continue;
    if (isEgg() || !sleeping || busyEnding()) continue;
    sleeping = false;
    wokeUp();
    upgSleepReset();  // v3.20: agitarlo tampoco
    uint32_t now = millis();
    if (!grumpied || now - lastGrumpy >= GRUMPY_COOLDOWN_MS) {
      joy = clamp100(joy - GRUMPY_JOY);
      grumpied = true;
      lastGrumpy = now;
    }
    startEmote(EMOTE_ANGRY);
    any = true;
  }
  if (!any) return;
  sfxPlay(SFX_DENY);  // uno solo aunque se despierten los dos
  pendingSave = true;
}

// agitarlo despierto: se marea. 3 sacudidas en 2 min restan felicidad, como
// mucho una vez cada 10 min. El huevo solo se tambalea. v3.0: los dos de casa (los dormidos, no: eso es
// wakeGrumpy, que la UI llama despues)
bool Pet::shaken() {
  bool counted = false;
  for (uint8_t s = 0; s < HOME_SLOTS; s++) {
    FocusGuard g(*this, s);
    if (!g.ok) continue;
    if (busyEnding() || vacant) continue;
    if (isEgg()) {
      startEmote(EMOTE_WOBBLE);
      // el del patio (Y3): la sacudida le quita minutos (una cada EGG_SHAKE_COOLDOWN_MS, hasta el tope).
      // Solo se marca el guardado: lo vuelca el loop con la pantalla atenuada
      if (yardEgg() && eggShake(&eggSaved, &eggShakeMs, &eggShakeHave, millis())) {
        pendingSave = true;
        counted = true;
      }
      continue;
    }
    if (sleeping) continue;  // dormido es wakeGrumpy()
    startEmote(EMOTE_DIZZY);
    uint32_t now = millis();
    if (shakeN == DIZZY_SHAKES) {
      for (uint8_t i = 1; i < DIZZY_SHAKES; i++) shakeAt[i - 1] = shakeAt[i];
      shakeN--;
    }
    shakeAt[shakeN++] = now;
    if (shakeN == DIZZY_SHAKES && now - shakeAt[0] <= DIZZY_WINDOW_MS &&
        (!dizzyPenalized || now - lastDizzyPenalty >= DIZZY_COOLDOWN_MS)) {
      joy = clamp100(joy - DIZZY_JOY);
      dizzyPenalized = true;
      lastDizzyPenalty = now;
      shakeN = 0;
      pendingSave = true;
    }
  }
  return counted;
}

// ---------- combate: felicidad y botin ----------

bool Pet::practiceRewardReady() const {
  return !practiceRewarded || millis() - lastPracticeReward >= PRACTICE_EVERY_MS;
}

uint8_t Pet::rollBattleItem() {
  long r = random(100);
  uint8_t it;
  if (r < BDROP_CAKE) it = IT_CAKE;
  else if ((r -= BDROP_CAKE) < BDROP_GOLDEN) it = IT_GOLDEN;
  else if ((r -= BDROP_GOLDEN) < BDROP_VITAMIN) it = IT_PROTEIN + random(3);
  else if ((r -= BDROP_VITAMIN) < BDROP_RARECANDY) it = IT_RARECANDY;
  else if ((r -= BDROP_RARECANDY) < BDROP_CHARM) it = IT_CHARM;
  else if ((r -= BDROP_CHARM) < BDROP_REVIVE) it = IT_REVIVE;
  else if ((r -= BDROP_REVIVE) < BDROP_BALL) it = IT_BALL;  // v2.2
  else if ((r -= BDROP_BALL) < BDROP_COLORED) it = IT_BERRY_R + random(BERRY_COLORS);  // v3.2
  else if ((r -= BDROP_COLORED) < BDROP_LURE) it = IT_LURE;  // v3.14
  else it = IT_EGG;
  if (it == IT_CHARM && items[IT_CHARM]) it = IT_CAKE;  // solo se lleva un amuleto
  return it;
}

// v3.0: a los dos de casa (la hora es del jugador: se marca si alguno la recibio). Devuelve la mayor subida
uint8_t Pet::practiceJoy() {
  if (!practiceRewardReady()) return 0;
  uint8_t best = 0;
  bool any = false;
  for (uint8_t s = 0; s < HOME_SLOTS; s++) {
    FocusGuard g(*this, s);
    if (!g.ok) continue;
    if (isEgg() || busyEnding()) continue;
    any = true;
    uint8_t before = joy;
    joy = clamp100(joy + PRACTICE_JOY);
    if (joy - before > best) best = joy - before;
  }
  if (any) {
    practiceRewarded = true;
    lastPracticeReward = millis();
  }
  return best;
}

// ---------- estadios ----------

uint8_t Pet::badgeCount() const {
  uint8_t n = 0;
  for (uint8_t m = badges; m; m &= (uint8_t)(m - 1)) n++;
  return n;
}

void Pet::setTeam(const uint32_t *uids, uint8_t n) {
  if (n > TEAM_MAX) n = TEAM_MAX;
  for (uint8_t i = 0; i < TEAM_MAX; i++) team[i] = (uids && i < n) ? uids[i] : 0;
  pendingSave = true;
}

int8_t Pet::teamSlotOf(uint32_t id) const {
  if (!id) return -1;
  int8_t s = slotOfUid(id);  // v3.0: uno de los dos de casa
  if (s >= 0) return (int8_t)(TEAM_HOME0 + s);
  return slotOf(id);
}

bool Pet::teamCanFight(uint32_t id) const {
  int8_t k = teamSlotOf(id);
  if (teamIsHome(k)) return home(teamHomeSlot(k)).canBattle();
  return k >= 0 && !slotTravelling((uint8_t)k);  // de viaje (Y5): no
}

// ---------- monedas (Y4) ----------
static_assert(FOOD_ITEM_BERRY0 == IT_BERRY_R && FOOD_ITEM_CAKE == IT_CAKE && FOOD_ITEM_GOLDEN == IT_GOLDEN,
              "food.h: los ids de objeto son los de pet.h");
static_assert(PACK_OLD == IT_BERRY_R, "\"bpack\" guarda los objetos de antes de las bayas");
static_assert(PACK_OLD + PACK2_LEN == IT_BERRY_O + 1 && PACK3_FIRST == IT_LURE, "\"bpk2\" las bayas, \"bpk3\" desde el Cebo");
static_assert(IT_COUNT - PACK3_FIRST <= PACK3_LEN, "\"bpk3\" no se alarga: un objeto nuevo usa uno de sus huecos libres");
static_assert(IT_CHARM == 4 && IT_BERRY_R == 10 && IT_BERRY_O == 15, "economy.h: itemCap / shopPackOf usan los ids a mano");
static_assert(BERRY_MAX <= 255, "items[] es de un byte");
static_assert(IT_COUNT == ECO_ITEMS, "economy.h: un precio por objeto de pet.h");

uint16_t Pet::coinsEarnedToday() const { return coinDay == today() ? coinToday : 0; }

uint16_t Pet::addCoins(uint16_t n, bool capped) {
  if (capped) {
    uint32_t d = today();
    if (coinDay != d) {  // dia nuevo: el contador vuelve a 0
      coinDay = d;
      coinToday = 0;
    }
    uint16_t room = coinToday >= ECO_DAILY_CAP ? 0 : ECO_DAILY_CAP - coinToday;
    if (n > room) n = room;
  }
  uint16_t room2 = coins >= COIN_MAX ? 0 : COIN_MAX - coins;
  if (n > room2) n = room2;
  if (!n) return 0;
  coins += n;
  if (capped) coinToday += n;
  pendingSave = true;  // como los pasos: el que llama guarda, o el loop con la pantalla atenuada
  return n;
}

bool Pet::spendCoins(uint16_t n) {
  if (n > coins) return false;
  coins -= n;
  save();
  return true;
}

bool Pet::buyItem(uint8_t itemId) {
  uint16_t p = priceOfItem(itemId);
  uint8_t pack = shopPackOf(itemId);
  if (!p || itemId >= IT_COUNT || p > coins || items[itemId] + pack > itemCap(itemId)) return false;
  items[itemId] += pack;
  coins -= p;
  save();
  return true;
}

bool Pet::ownsMap(uint8_t n) const {
  if (n < 1 || n > YARD_MAPS) return false;
  return (yardOwned & (1u << n)) || n == YARD_MAP_START;
}

bool Pet::buyMap(uint8_t n) {
  uint16_t p = priceOfMap(n);
  if (!p || ownsMap(n) || p > coins) return false;
  coins -= p;
  yardOwned |= (uint16_t)(1u << n);
  save();
  return true;
}

// ---------- mejoras de casa (v3.20, upgrade.h, docs/HOME_AND_PALS.md) ----------
// el siguiente nivel de una linea: primero lo que no se arregla (linea mala, al maximo), luego las monedas. save()
// escribe "cn" antes que "upg": un corte entre las dos pierde el nivel, nunca lo regala
bool Pet::buyUpgrade(uint8_t line) {
  if (upgCheck(line, upgTier(line), coins) != UB_OK) return false;
  coins -= upgPriceNext(line, upgTiers[line]);
  upgTiers[line]++;
  save();
  return true;
}

bool Pet::takeHomeEvt(HomeEvt &e) {
  return homeEvts.pop(e);
}

// ---------- los dos bichos juntos (v3.22) ----------
// CONTRATO: version minima para que el patio compile; la tarea de logica (P1) pone los topes, la NVS y la medalla
uint8_t Pet::palPlayPerk(uint8_t slot) {
  (void)slot;
  return 0;
}

uint8_t Pet::palCheerPerk(uint8_t slot) {
  (void)slot;
  return 0;
}

void Pet::palSceneSeen() {}

void Pet::dbgSetUpgTier(uint8_t line, uint8_t tier) {
  if (line >= UPG_LINES) return;
  upgTiers[line] = tier > UPG_TIERS ? UPG_TIERS : tier;
  pendingSave = true;
}

// los contadores del hueco del foco. Son de un bicho (su uid): si el hueco tiene otro, empiezan de cero
UpgSlot *Pet::upgSlotNow() {
  if (isEgg() || !uid) return nullptr;
  uint8_t s = focusSlot & 1;
  UpgSlot &u = upgSlots[s];
  if (u.uid != uid) {
    memset(&u, 0, sizeof(u));
    u.uid = uid;
    gymMin[s] = 0;
  }
  return &u;
}

// la cuenta de la noche vuelve a 0: se duerme, o se despierta (sin tocar los contadores de otro bicho)
void Pet::upgSleepReset() {
  UpgSlot &u = upgSlots[focusSlot & 1];
  if (uid && u.uid == uid) u.sleptMin = 0;
}

// un minuto despierto del foco con el gimnasio y el juguete (FR-HOME.2 y .3), en vivo (tick) o apagado (syncHome,
// live = false: sin eventos). day = el dia de ese minuto (0 sin reloj: nada). No es cuidado: ni racha, ni
// vinculo, ni monedas
void Pet::homeMinute(int h, uint32_t day, bool live) {
  uint8_t g = upgTier(UPG_GYM), t = upgTier(UPG_TOY);
  if (!g && !t) return;
  if (!day || h < 0 || nightHour(h)) return;  // ni sin reloj ni de noche (de SLEEPY_HOUR a WAKE_HOUR)
  if (isEgg() || vacant || sleeping || busyEnding() || evolving() || starterPick) return;
  if (live && nightHold) return;  // un juego, combate, encuentro o movimiento de la caja abierto
  UpgSlot *u = upgSlotNow();
  if (!u) return;
  if (u->day != day) {  // dia nuevo: los topes vuelven a 0
    u->day = day;
    u->gymGains = 0;
    u->toyPlays = 0;
  }
  uint8_t s = focusSlot & 1;
  // gimnasio: cuenta los minutos con todo >= GYM_NEED_MIN; cada GYM_EVERY_MIN, +1 al mas bajo bajo el techo
  if (g && u->gymGains < GYM_DAY_MAX[g] && lowestStat() >= GYM_NEED_MIN) {
    int8_t st = gymPick(trAtk, trDef, trSpe, GYM_CAP[g]);
    if (st >= 0 && ++gymMin[s] >= GYM_EVERY_MIN[g]) {
      gymMin[s] = 0;
      uint8_t &tr = st == 0 ? trAtk : st == 1 ? trDef : trSpe;
      uint16_t v = (uint16_t)tr + 1;
      tr = v > 100 ? 100 : (uint8_t)v;
      energy = dropTo(energy, GYM_ENE_COST, GAME_FLOOR);
      u->gymGains++;
      pendingSave = true;
      if (live) homeEvts.push(HE_GYM, s, (uint8_t)st);  // (el "+1 ATK" ya lo pone fxTrainW)
    }
  }
  // juguete: con poca alegria, de vez en cuando juega solo
  if (t && u->toyPlays < TOY_DAY_MAX[t] && joy < TOY_JOY_BELOW && random(100) < TOY_PLAY_CHANCE) {
    uint8_t before = joy;
    joy = clamp100(joy + TOY_JOY[t]);
    u->toyPlays++;
    pendingSave = true;
    if (live) homeEvts.push(HE_TOY, s, (uint8_t)(joy - before));
  }
}

// "upg" si cambio (los niveles y los contadores de cada hueco)
bool Pet::saveUpg() {
  UpgSave u;
  memset(&u, 0, sizeof(u));
  memcpy(u.tier, upgTiers, sizeof(u.tier));
  memcpy(u.slot, upgSlots, sizeof(u.slot));
  upgSeal(u);
  if (upgHave && memcmp(&u, &upgSaved, sizeof(u)) == 0) return true;
  if (prefs.putBytes("upg", &u, sizeof(u)) != sizeof(u)) return false;
  upgSaved = u;
  upgHave = true;
  return true;
}

void Pet::chooseMap(uint8_t n) {
  if (n && !ownsMap(n)) return;
  yardChosen = n;
  save();
}

// ---------- viajes y coleccion (Y5) ----------
static_assert(TRV_DESTS == YARD_MAPS, "travel.h: un destino por mapa del patio");
static_assert(TRV_POOL_ID[0] == IT_GOLDEN && TRV_POOL_ID[1] == IT_CAKE && TRV_POOL_ID[2] == IT_PROTEIN &&
                TRV_POOL_ID[3] == IT_IRON && TRV_POOL_ID[4] == IT_CARBOS && TRV_POOL_ID[5] == IT_RARECANDY &&
                TRV_POOL_ID[6] == IT_REVIVE && TRV_POOL_ID[7] == IT_BALL && TRV_POOL_ID[8] == IT_BERRY_R &&
                TRV_POOL_ID[9] == IT_LURE && TRV_POOL_N == 10 &&
                TRV_POOL_BERRY == IT_BERRY_R && TRV_BERRY_COLORS == 6 && IT_BERRY_O == IT_BERRY_R + 5,
              "travel.h: los ids de objeto son los de pet.h");

uint8_t Pet::souvenirCount() const {
  uint8_t n = 0;
  for (uint8_t i = 0; i < SOUV_COUNT; i++) n += (uint8_t)((souvOwned >> i) & 1);
  return n;
}

void Pet::setSouvenirMask(uint64_t mask) {
  mask &= SOUV_ALL;
  for (uint8_t i = 0; i < SOUV_COUNT; i++) {
    bool had = (souvOwned >> i) & 1, want = (mask >> i) & 1;
    if (want && !had) souvDate[i] = lastSeenEpoch;
    if (!want) souvDate[i] = 0;
  }
  souvOwned = mask;
  save();
}

bool Pet::canTravel(uint8_t slot, uint8_t dest, uint8_t tier) const {
  return slot < BOX_SLOTS && boxSt[slot] == BOX_OK && trvSlot == TRV_NONE && tier < TRV_TIERS && ownsMap(dest) &&
         eggClockKnown(lastSeenEpoch);  // sin hora no se sabe cuando vuelve
}

// la entrada "trv" entera, de una vez (un solo putBytes: o esta o no esta)
bool Pet::travelPersist() {
  uint8_t t[16] = { trvSlot, trvDest, trvTier, 1 };
  memcpy(t + 4, &trvUid, 4);
  memcpy(t + 8, &trvStart, 4);
  memcpy(t + 12, &trvSeed, 4);
  return prefs.putBytes("trv", t, sizeof(t)) == sizeof(t);
}

bool Pet::startTravel(uint8_t slot, uint8_t dest, uint8_t tier) {
  if (!canTravel(slot, dest, tier)) return false;
  trvSlot = slot;
  trvDest = dest;
  trvTier = tier;
  trvUid = boxRec[slot].uid;
  trvStart = lastSeenEpoch;
  trvSeed = ((uint32_t)random(0x10000) << 16) | (uint32_t)random(0x10000);
  if (!travelPersist()) {
    trvSlot = TRV_NONE;
    return false;
  }
  Serial.printf("travel: slot %u -> map %u tier %u seed %08lx\n", slot, dest, tier, (unsigned long)trvSeed);
  return true;
}

void Pet::travelCancel() {
  trvSlot = TRV_NONE;
  prefs.remove("trv");
}

void Pet::travelCheck() {
  if (trvSlot == TRV_NONE || travelTimeLeft(trvStart, trvTier, lastSeenEpoch) > 0) return;
  travelComplete();
}

// Volver. Orden de escritura para un corte de luz (nunca se duplican monedas):
//   1. el registro del bicho con su crecimiento (writeSlot, sellado);
//   2. se borra "trv" (el viaje deja de existir);
//   3. save(): monedas, mochila, coleccion y, al final, el resultado sin ver ("trvR").
// Corte entre 1 y 2: al arrancar el viaje sigue y se completa otra vez: el bicho gana el crecimiento DOS
// veces (peor caso aceptado; el sorteo es el mismo y el premio del jugador solo se cobra una vez).
// Corte entre 2 y 3: el jugador pierde el premio de ese viaje (monedas, objeto, recuerdo), nunca lo cobra
// dos veces. Corte dentro de 3: parte del premio (cada clave es atomica) y quiza sin aviso.
void Pet::travelComplete() {
  uint8_t k = trvSlot;
  if (k >= BOX_SLOTS || boxSt[k] != BOX_OK || boxRec[k].uid != trvUid) {  // el bicho ya no esta (BOXCLR)
    travelCancel();
    return;
  }
  TravelResult r = travelRoll(trvSeed, trvDest, trvTier, souvOwned);
  PetRecord rec = boxRec[k];
  r.valid = 1;
  r.speciesId = rec.speciesId;
  r.shiny = (rec.flags & REC_SHINY) ? 1 : 0;
  memcpy(r.nick, rec.nick, sizeof(r.nick));
  r.nick[sizeof(r.nick) - 1] = 0;
  r.lvBefore = levelFor(rec.growMin);
  rec.growMin += r.growMin;  // la evolucion no se mira: hiberna y se mira al despertarlo (como cualquiera de la caja)
  r.lvAfter = levelFor(rec.growMin);
  Serial.printf("travel: back slot %u map %u tier %u seed %08lx rolls item=%u souv=%u -> item=%d souv=%d dup=%d coins=%u grow=+%u lv %u->%u\n",
                k, r.dest, r.tier, (unsigned long)r.seed, r.rollItem, r.rollSouv,
                r.item == TRV_NONE ? -1 : r.item, r.souvenir == TRV_NONE ? -1 : r.souvenir, r.dup, r.coins, r.growMin,
                r.lvBefore, r.lvAfter);
  if (!writeSlot(k, rec)) return;  // 1. (no se pudo: el viaje sigue y se reintenta en el siguiente minuto)
  travelCancel();                  // 2.
  r.coins = addCoins(r.coins, false);
  if (r.dupCoins > r.coins) r.dupCoins = r.coins;
  if (r.item != TRV_NONE) {
    if (items[r.item] >= itemCap(r.item)) r.itemLost = 1;
    else items[r.item]++;
  }
  if (r.souvenir != TRV_NONE && !r.dup) {
    souvOwned |= souvBit(r.souvenir);
    souvDate[r.souvenir] = lastSeenEpoch;
  }
  trvResult = r;
  trvResPending = true;
  pmBump(pm, MD_TRIPS);  // v3.4 ("trv" ya se borro en 2: un corte no lo repite)
  save();  // 3.
  encTravelRoll(TRV_MIN[trvTier < TRV_TIERS ? trvTier : 0] / 60);  // v2.2: viajar tambien trae encuentros
}

bool Pet::takeTravelResult(TravelResult *out) {
  if (!trvResPending) return false;
  if (out) *out = trvResult;
  trvResPending = false;
  trvResult.valid = 0;
  pendingSave = true;  // el loop lo borra de la NVS con la pantalla atenuada (un corte antes: el aviso vuelve)
  return true;
}

void Pet::dbgTravelElapsed(uint32_t min) {
  if (trvSlot == TRV_NONE) return;
  uint32_t back = min * 60;
  trvStart = trvStart > back + EGG_EPOCH_MIN ? trvStart - back : EGG_EPOCH_MIN;
  travelPersist();
  travelCheck();
}

void Pet::travelFinishNow() {
  if (trvSlot == TRV_NONE) return;
  dbgTravelElapsed(TRV_MIN[trvTier]);
}

// el premio del jefe de hoy, a la NVS antes que nada de lo que paga. Un dia nuevo empieza con la
// mascara a 0 y despues el dia ("stD" antes que "stM"): un corte entre los dos deja el dia de hoy con
// la mascara de ayer (como mucho se pierde un premio de hoy), nunca la mascara de hoy con el dia de
// ayer (que se volveria a pagar). Si la escritura no llega, no se paga.
bool Pet::payStageDay(uint8_t stage) {
  uint32_t d = today();
  uint8_t mask = stageRewardMask();
  if (stageDay != d) {
    if (stageMask && !prefs.putUChar("stM", 0)) return false;
    if (!prefs.putUInt("stD", d)) return false;
    stageDay = d;
    stageMask = 0;
  }
  mask |= (uint8_t)(1u << stage);
  if (!prefs.putUChar("stM", mask)) return false;
  stageMask = mask;
  return true;
}

// Fin de un estadio. Orden de escritura (corte de luz): 1. la medalla ("bdg"), 2. el premio de hoy
// marcado ("stD"/"stM"), 3. cada bicho de la caja del equipo, sellado en su hueco (writeSlot: la copia
// en RAM solo cambia si la escritura llego), 4. save(): el de casa y el objeto envuelto. Un corte
// puede dejar parte del premio sin dar (un bicho de la caja si y el siguiente no), nunca pagarlo dos
// veces el mismo dia ni dejar un hueco danado.
StageReward Pet::stageDone(uint8_t stage, bool cleared, const uint32_t *uids, uint8_t n) {
  StageReward res = { false, false, (uint8_t)IT_COUNT, 0, 0, 0 };
  res.joy = practiceJoy();  // gane o pierda, una vez por hora (0 con huevo, casa vacia o final)
  if (cleared && stage < STAGE_COUNT) {
    uint8_t bit = (uint8_t)(1u << stage);
    if (!(badges & bit)) {
      badges |= bit;
      res.newBadge = true;
      prefs.putUChar("bdg", badges);  // 1. la medalla: abre el siguiente estadio
    }
    // monedas (Y4): la primera vez, fuera del tope; las repeticiones, una por estadio y dia (la marca
    // del premio del jefe) y dentro del tope. Si la marca no se pudo escribir, no se paga
    bool repeat = !res.newBadge && stageRewardReady(stage);
    if (res.newBadge) res.coins = addCoins(ECO_STAGE_FIRST, false);
    if (stageRewardReady(stage) && payStageDay(stage)) {  // 2.
      res.paid = true;
      if (repeat) res.coins = addCoins(ECO_STAGE_REPEAT, true);
      if (n > TEAM_MAX) n = TEAM_MAX;
      for (uint8_t i = 0; uids && i < n; i++) {
        uint32_t id = uids[i];
        bool dup = false;
        for (uint8_t j = 0; j < i; j++) dup |= (uids[j] == id);
        if (!id || dup) continue;
        int8_t k = teamSlotOf(id);
        if (teamIsHome(k)) {  // v3.0: el de ese hueco de casa
          FocusGuard g(*this, teamHomeSlot(k));
          if (!g.ok) continue;
          if (busyEnding()) continue;  // se esta muriendo o despidiendo: nada que crecer
          growMin += STAGE_GROW_MIN;   // las subidas de nivel las ve la UI (fx.h); la evolucion, el boton
          bond = clamp100(bond + STAGE_BOND);  // fuera del tope diario de addBond (las medallas, en el save())
          res.pets++;
        } else if (k >= 0) {
          // 3. hiberna, pero el premio lo recibe igual: crece y su nivel puede pasar el de su
          // evolucion. Evoluciona al despertar (canEvolveNow mira el nivel; fromRecord borra el
          // "mantener forma") y cuenta para las medallas del jugador al despertar (medalsUpdate)
          PetRecord r = boxRec[k];
          r.growMin += STAGE_GROW_MIN;
          r.bond = clamp100(r.bond + STAGE_BOND);
          if (writeSlot((uint8_t)k, r)) res.pets++;
        }
      }
      res.item = rollBattleItem();  // seguro: sale siempre
      // con los huecos de casa vacios no hay quien abra un regalo: a la mochila con su aviso (como
      // unwrapGifts). Con un huevo, espera envuelto a que nazca
      if (homeVacant() == HOME_SLOTS) giveFound(res.item);
      else giveGift(res.item);
    }
  }
  save();              // 4. fin de una pantalla de juego: se guarda ya
  return res;
}

// ---------- encuentros salvajes (v2.2, encounter.h) ----------
//
// "enc" (16 bytes, una entrada: o esta o no): dia de las tiradas, cuando salio, el salvaje (especie, nivel,
// entrenamiento), la mascara de tiradas del dia y sus flags (shiny, exclusivo, ganado). Las tiradas solo
// marcan el guardado (pasan en el tick o al contar pasos: save() congela ~1 s); pelear y lanzar la Poke Ball
// son toques y guardan al momento.

void Pet::encPersist() {
  uint8_t e[16] = { 0 };
  memcpy(e, &encDay, 4);
  memcpy(e + 4, &encSpawnAt, 4);
  memcpy(e + 8, &encDexV, 2);
  e[10] = encLv;
  e[11] = encRolled;
  e[12] = encFlags;
  memcpy(e + 13, encTr, 3);
  prefs.putBytes("enc", e, sizeof(e));
}

bool Pet::encActive() const { return encSpawnAt && !(encFlags & ENCF_WON) && encAlive(encSpawnAt, lastSeenEpoch); }

uint16_t Pet::encMinutesLeft() const { return encActive() ? ::encMinutesLeft(encSpawnAt, lastSeenEpoch) : 0; }

// alguien con quien pelear: el de casa nacido o alguien en la caja (dormido o de viaje tambien: el encuentro
// espera 6 h). Sin eso no se tira (la tirada se queda para luego)
bool Pet::encCanRoll() const { return !starterPick && (homeHatched() || boxHasPets()); }  // v3.0: cualquiera de casa

// countsAsDaily: la base, el viaje y ENC cierran esas tiradas del dia; los de pasear no (v3.11)
void Pet::encStart(int16_t dex, uint32_t rng, bool countsAsDaily) {
  EncWild w = encRollWild(yardMapWanted(), badges, rng);
  if (dex >= 1 && dex <= DEX_COUNT) {  // serie ENC <dex>: esa especie, al nivel del sorteo
    w.dex = dex;
    w.exclusive = DEX_TBL[DEX_TBL[dex].base].rarity == R_ENCUENTRO;
  }
  encDexV = w.dex;
  encLv = w.level;
  encTr[0] = w.tAtk;
  encTr[1] = w.tDef;
  encTr[2] = w.tSpe;
  encFlags = (w.exclusive ? ENCF_EXCL : 0) | (random(shinyOdds(false)) == 0 ? ENCF_SHINY : 0);
  // la medalla de "vencido"/"capturado" se guarda por su hora de salida: dos salvajes en el mismo reloj no la repiten
  encSpawnAt = lastSeenEpoch;
  uint32_t lastId = pm.wildId > pm.catchId ? pm.wildId : pm.catchId;
  if (encSpawnAt <= lastId) encSpawnAt = lastId + 1;
  if (countsAsDaily) encRolled |= ENC_ROLL_SPAWNED;
  encSound = true;
  pendingSave = true;
  Serial.printf("enc: sale #%d nv.%u%s%s ent.%u/%u/%u\n", encDexV, encLv, encShiny() ? " shiny" : "",
                encExclusive() ? " exclusivo" : "", encTr[0], encTr[1], encTr[2]);
}

void Pet::encEnd() {
  encSpawnAt = 0;
  encFlags = 0;
  encSound = false;
  pendingSave = true;
}

void Pet::encCheck() {
  uint32_t now = lastSeenEpoch;
  if (!eggClockKnown(now)) return;  // sin hora no hay dias ni 6 h
  if (encSpawnAt && !(encFlags & ENCF_WON) && !encAlive(encSpawnAt, now)) {
    Serial.println("enc: se fue (6 h)");
    encEnd();
  }
  uint32_t d = today();
  if (d != encDay) {
    encDay = d;
    encRolled = 0;
    pendingSave = true;
  }
  if (encSpawnAt || !encCanRoll()) return;  // solo uno a la vez (tambien el ganado con la bola pendiente)
  uint32_t rng = ((uint32_t)random(0x10000) << 16) | (uint32_t)random(0x10000);
  if (!(encRolled & (ENC_ROLL_BASE | ENC_ROLL_SPAWNED)) && (now / 60) % 1440 >= encBaseMinute(d)) {
    encRolled |= ENC_ROLL_BASE;
    pendingSave = true;
    bool hit = encHit(ENC_BASE_PERMIL, rng);
    Serial.printf("enc: tirada base %s\n", hit ? "SI" : "no");
    if (hit) {
      encStart(0, rng);
      return;
    }
  }
  // v3.11: una tirada por umbral de pasos (el bit solo se marca al tirar de verdad)
  for (uint8_t i = 0; i < ENC_WALK_TIERS; i++) {
    uint8_t bit = (uint8_t)(ENC_ROLL_WALK0 << i);
    if ((encRolled & bit) || stepsOn(d) < ENC_WALK_AT[i]) continue;
    encRolled |= bit;
    pendingSave = true;
    bool hit = encHit(ENC_WALK_PERMIL, rng);
    Serial.printf("enc: tirada de pasear (%u pasos) %s\n", (unsigned)ENC_WALK_AT[i], hit ? "SI" : "no");
    if (hit) {
      encStart(0, rng, false);
      return;
    }
    rng = ((uint32_t)random(0x10000) << 16) | (uint32_t)random(0x10000);
  }
}

void Pet::encTravelRoll(uint16_t hours) {
  encCheck();  // el dia al dia (y el que ya se fue, fuera) antes de tirar
  if (!eggClockKnown(lastSeenEpoch) || encSpawnAt || (encRolled & ENC_ROLL_SPAWNED) || !encCanRoll()) return;
  uint32_t rng = ((uint32_t)random(0x10000) << 16) | (uint32_t)random(0x10000);
  bool hit = encHit(encTravelPermil(hours), rng);
  Serial.printf("enc: tirada del viaje (%u h) %s\n", hours, hit ? "SI" : "no");
  if (hit) encStart(0, rng);
}

bool Pet::encSpawnNow(int16_t dex) {
  if (!eggClockKnown(lastSeenEpoch)) return false;
  encCheck();
  if (encDay != today()) {
    encDay = today();
    encRolled = 0;
  }
  uint32_t rng = ((uint32_t)random(0x10000) << 16) | (uint32_t)random(0x10000);
  encStart(dex, rng);
  return true;
}

void Pet::encClear() {
  encEnd();
  save();
}

// el Cebo (v3.14). Alguien que pueda pelear YA (no basta encCanRoll: la pantalla del encuentro se abre al momento y el
// selector lleva un solo luchador): uno de casa (despierto o dormido, v3.12) o uno de la caja que no este de viaje
uint8_t Pet::lureBlock() const {
  if (!items[IT_LURE]) return LURE_NONE;
  if (starterPick || home(0).busyEnding() || home(1).busyEnding()) return LURE_BUSY;
  if (!eggClockKnown(lastSeenEpoch)) return LURE_NO_CLOCK;
  bool fighter = home(0).canBattle() || home(1).canBattle();
  for (uint8_t k = 0; k < BOX_SLOTS && !fighter; k++) fighter = boxSt[k] == BOX_OK && !slotTravelling(k);
  if (!encCanRoll() || !fighter) return LURE_NO_FIGHTER;
  if (encShown()) return LURE_ENC;  // uno a la vez (tambien el vencido con la Poke Ball pendiente)
  return LURE_OK;
}

// Orden de escritura (corte de luz): "enc" primero (el salvaje), luego save() con la mochila. Un corte entre los dos
// deja el salvaje y el Cebo sin gastar (uno de mas, nunca uno gastado sin salvaje)
bool Pet::useLure() {
  uint8_t b = lureBlock();
  if (b != LURE_OK) {
    Serial.printf("enc: cebo no (%u)\n", b);
    return false;
  }
  uint32_t rng = ((uint32_t)random(0x10000) << 16) | (uint32_t)random(0x10000);
  encStart(0, rng, false);  // como los de pasear: no cierra las tiradas del dia
  encSound = false;         // la UI abre su pantalla ya, con su sonido
  encPersist();
  items[IT_LURE]--;
  save();
  Serial.printf("enc: cebo -> sale #%d (quedan %u)\n", encDexV, items[IT_LURE]);
  return true;
}

uint8_t Pet::rollWildItem() {
  long r = random(100);
  uint8_t it;
  if (r < WDROP_CAKE) it = IT_CAKE;
  else if ((r -= WDROP_CAKE) < WDROP_GOLDEN) it = IT_GOLDEN;
  else if ((r -= WDROP_GOLDEN) < WDROP_VITAMIN) it = IT_PROTEIN + random(3);
  else if ((r -= WDROP_VITAMIN) < WDROP_RARECANDY) it = IT_RARECANDY;
  else if ((r -= WDROP_RARECANDY) < WDROP_REVIVE) it = IT_REVIVE;
  else if ((r -= WDROP_REVIVE) < WDROP_CHARM) it = IT_CHARM;
  else it = IT_BERRY_R + random(BERRY_COLORS);  // v3.2
  if (it == IT_CHARM && items[IT_CHARM]) it = IT_CAKE;  // solo se lleva un amuleto
  return it;
}

// Fin del combate. Orden de escritura (corte de luz): 1. el que peleo, si es de la caja (writeSlot); 2. save():
// el de casa, el objeto y "enc" ganado. Un corte entre 1 y 2 deja el encuentro sin pelear y el de la caja con
// su hora: se puede volver a pelear (peor caso aceptado: una hora de mas), nunca se pierde el salvaje
EncReward Pet::encounterDone(bool won, uint32_t fighter) {
  EncReward r = { false, (uint8_t)IT_COUNT, false, 0, 0, 0 };
  if (!encActive()) return r;
  if (!won) {
    Serial.printf("enc: #%d se escapa\n", encDexV);
    encEnd();
    save();
    return r;
  }
  r.won = true;
  int8_t k = teamSlotOf(fighter);
  if (teamIsHome(k)) {  // v3.0: el de ese hueco de casa (crece y se alegra)
    FocusGuard g(*this, teamHomeSlot(k));
    if (g.ok && !isEgg() && !busyEnding()) {
      r.lvBefore = level();
      growMin += ENC_WIN_GROW_MIN;  // las subidas de nivel las ve la UI (fx.h); la evolucion, el boton
      r.lvAfter = level();
      uint8_t before = joy;
      joy = clamp100(joy + ENC_WIN_JOY);
      r.joy = joy - before;
    }
  } else if (k >= 0 && k < BOX_SLOTS) {
    PetRecord rec = boxRec[k];
    r.lvBefore = levelFor(rec.growMin);
    rec.growMin += ENC_WIN_GROW_MIN;  // hiberna: evoluciona al despertar (como el premio de un estadio)
    r.lvAfter = levelFor(rec.growMin);
    writeSlot((uint8_t)k, rec);  // 1.
  }
  if (random(100) < ENC_DROP_PCT) {
    r.item = rollWildItem();
    uint8_t cap = itemCap(r.item);
    if (items[r.item] < cap) items[r.item]++;
    else r.itemLost = true;
  }
  encFlags |= ENCF_WON;
  // v3.4: una vez por encuentro (su hora de salida va en "pm"): un corte antes de "enc" deja volver a pelearlo y
  // repetir el objeto, pero no cuenta otra victoria
  if (!encSpawnAt || pm.wildId != encSpawnAt) {
    pmBump(pm, MD_WILD);
    pm.wildId = encSpawnAt;
  }
  save();  // 2.
  Serial.printf("enc: #%d vencido, nv %u->%u, objeto %d%s\n", encDexV, r.lvBefore, r.lvAfter,
                r.item == IT_COUNT ? -1 : r.item, r.itemLost ? " (perdido)" : "");
  return r;
}

uint8_t Pet::captureBlock() const {
  if (!encWonPending()) return CAP_NONE;
  if (!items[IT_BALL]) return CAP_NO_BALL;
  if (items[IT_EGG] >= ITEM_MAX || capN >= CAP_EGG_MAX) return CAP_NO_ROOM;
  return CAP_OK;
}

// la Poke Ball: el salvaje (en su forma base) a la cola de los huevos de captura y un huevo mas en la mochila.
// Todo en un save(): la mochila (bola gastada, huevo nuevo) antes que "cegg" y "enc"
bool Pet::captureWild() {
  if (captureBlock() != CAP_OK) return false;
  int16_t base = DEX_TBL[encDexV].base;
  items[IT_BALL]--;
  items[IT_EGG]++;
  capPush(base, encShiny(), false);
  if (!encSpawnAt || pm.catchId != encSpawnAt) {  // v3.4: una vez por encuentro (como la victoria)
    pmBump(pm, MD_CATCH);
    pm.catchId = encSpawnAt;
  }
  Serial.printf("enc: #%d capturado -> huevo de #%d%s (%u en cola)\n", encDexV, base, encShiny() ? " shiny" : "", capN);
  encEnd();
  save();
  return true;
}

void Pet::releaseWild() {
  if (!encWonPending()) return;
  Serial.printf("enc: #%d vuelve a la hierba\n", encDexV);
  encEnd();
  save();
}

// boca abajo de noche: a dormir, como el boton de luz pero sin escribir ya en flash (v3.0: los dos)
void Pet::sleepDeferred() {
  for (uint8_t s = 0; s < HOME_SLOTS; s++) {
    FocusGuard g(*this, s);
    if (!g.ok) continue;
    if (busyEnding() || isEgg() || sleeping) continue;
    goToBed();
    pendingSave = true;
  }
}

// v3.0: el hueco 0 va a las claves de siempre y el 1 a "hm1", tenga quien tenga el foco
void Pet::save() {
  medalsUpdate();  // v3.4: cuenta y paga en RAM lo nuevo; "pm" va mas abajo, antes que "cn"
  ticksSinceSave = 0;
  pendingSave = false;
  const PetLife &h = home(0);
  prefs.putUChar("full", h.fullness);
  prefs.putUChar("joy", h.joy);
  prefs.putUChar("ene", h.energy);
  prefs.putUChar("hyg", 100);  // v3.1: la limpieza ya no existe; la clave se queda por compatibilidad
  prefs.putUChar("poop", h.poops);
  {  // v3.1: la edad de cada caca (seguidas desde el primer hueco: al cargar se ocupan los primeros)
    uint32_t st[POOP_MAX];
    h.poopStamps(st);
    prefs.putBytes("poopA", st, sizeof(st));
  }
  prefs.putUChar("wgt", h.weight);
  prefs.putUInt("cake", h.satedUntil);
  prefs.putUChar("gatk", h.geneAtk);
  prefs.putUChar("gdef", h.geneDef);
  prefs.putUChar("gspe", h.geneSpe);
  prefs.putUChar("tatk", h.trAtk);
  prefs.putUChar("tdef", h.trDef);
  prefs.putUChar("tspe", h.trSpe);
  prefs.putBool("fk", h.berryKnown);  // v3.2: la favorita nueva (el "bk" de antes se ignora)
  prefs.putBool("shy", h.shiny);
  prefs.putBool("eshy", h.eggShiny);
  prefs.putBool("stpk", starterPick);
  prefs.putBytes("dexsh", dexShinyReg, sizeof(dexShinyReg));
  prefs.putUInt("age", h.ageMinutes);
  prefs.putUInt("grow", h.growMin);
  prefs.putShort("dexn", h.speciesId);
  prefs.putShort("eggT2", h.eggTarget);
  prefs.putUChar("crack", h.eggTaps);
  prefs.putUInt("yegg", h.eggPlaced);  // el huevo del patio (Y3)
  prefs.putUChar("ysav", h.eggSaved);
  prefs.putUChar("mist", h.careMistakes);
  prefs.putBool("sleep", h.sleeping);
  prefs.putBool("nslp", h.nightSleep);  // la noche (v1.63)
  prefs.putUInt("bedn", h.bedNight);
  prefs.putUChar("lend", lastEnd);
  // abandono (v1.60): la cuenta y el dialogo de la muerte sobreviven a un reinicio
  prefs.putUChar("ngl", h.neglectMin);
  prefs.putUChar("dth", h.deathMin);
  prefs.putBool("dying", h.dying);
  prefs.putUChar("dym", h.dyingMin);  // v1.64: la hora del Revivir
  prefs.putShort("degg", h.deathEgg);
  if (lastSeenEpoch) prefs.putUInt("seen", lastSeenEpoch);
  prefs.putBytes("dexreg", dexReg, sizeof(dexReg));
  prefs.putUShort("strk", streak);
  prefs.putUShort("bstrk", bestStreak);
  prefs.putUInt("cday", lastCareDay);
  prefs.putUChar("bond", h.bond);
  {  // v3.4: lo que el bicho del hueco 0 ya conto para las medallas del jugador, con su uid (antes que "pm").
     // ("medal" y "tmedal", las medallas de antes, ya no se escriben)
    uint32_t pc[2] = { h.uid, h.counted };
    prefs.putBytes("pmc", pc, sizeof(pc));
  }
  prefs.putUShort("mstone", lastMilestone);
  prefs.putUShort("ghi", gameHi);
  prefs.putUShort("shi", strHi);
  prefs.putUShort("zhi", zenHi);
  prefs.putUShort("dhi", dashHi);
  prefs.putString("nick", h.nick);
  // pasos y mochila (v1.20)
  prefs.putUInt("stpT", stepsToday);
  prefs.putUInt("stpA", stepsTotal);
  prefs.putUInt("stpD", stepDay);
  prefs.putUChar("fndT", findsToday);
  prefs.putUChar("sjoy", stepJoyToday);
  prefs.putUInt("goalD", goalDay);
  prefs.putUChar("goalN", goalTier);
  prefs.putUChar("fndB", bonusFindsToday);
  prefs.putUShort("jRem", joyRem);
  prefs.putUShort("wRem", weightRem);
  prefs.putUShort("bRem", bondRem);
  prefs.putUShort("fRem", findRem);
  prefs.putUShort("fAt", findAt);
  prefs.putUShort("eRem", eggRem);  // huevos (v1.39)
  prefs.putUShort("eAt", eggAt);
  prefs.putUInt("bondD", h.bondDay);
  prefs.putUChar("bondT", h.bondToday);
  prefs.putBool("vac", h.vacant);
  prefs.putUInt("pcd", h.careDay);  // v3.0: el primer cuidado de hoy del hueco 0 (su vinculo)
  // estadios (del jugador). Antes del bloque del uid: no tocan nada de los movimientos de la caja, y
  // asi la ventana entre el uid y "eggH" no se alarga. stageDone ya los escribe por su cuenta antes de
  // pagar (aqui solo repiten el mismo valor). "stD" antes que "stM", como en payStageDay
  // la coleccion (Y5). v3.15: 64 bits en "souv2"; la vieja "souv" (u32) se borra una vez escrita la nueva (un
  // corte entre las dos deja ambas, y la carga prefiere "souv2"). "souvD" crece de 25 a 45 fechas con la misma clave
  if (prefs.putULong64("souv2", souvOwned) == sizeof(uint64_t) && prefs.isKey("souv")) prefs.remove("souv");
  prefs.putBytes("souvD", souvDate, sizeof(souvDate));
  prefs.putUChar("bdg", badges);
  prefs.putUInt("stD", stageDay);
  prefs.putUChar("stM", stageMask);
  prefs.putBytes("team", team, sizeof(team));
  // caja (v1.37). El uid (del hueco 0; "hm1" es el del hueco 1) va el ULTIMO: la NVS escribe en orden,
  // asi que si un corte de luz deja el uid nuevo, todo lo demas del bicho ya esta escrito. boxBoot() se fia de eso para
  // saber si un movimiento llego a cambiar el bicho activo. "eggH" va despues: un corte entre
  // los dos deja un huevo activo con eggH puesto, y boxBoot() lo quita.
  // La mochila va justo despues del uid (v1.39): guardar en la caja puede sacar un huevo de ella, y
  // un corte antes del uid devuelve al bicho desde su hueco con la mochila sin tocar. Entre el uid y
  // la mochila un corte deja el huevo en casa y el de la mochila aun contado: como mucho uno de mas,
  // nunca uno perdido.
  prefs.putUInt("uid", h.uid);
  // v3.0: el hueco 1, entero en una entrada (si cambio). Despues del uid del 0 y antes de la mochila: un
  // movimiento de la caja con el hueco 1 se decide aqui (como con el uid para el 0), y un huevo que sale de
  // la mochila hacia el se cuenta como mucho dos veces, nunca ninguna
  saveOk = saveHome1();
  if (!saveOk) {
    // no se pudo escribir "hm1": lo de despues (la mochila sobre todo) no se escribe, para no apuntar un
    // huevo que salio de ella hacia el hueco 1 sin que el hueco lo tenga. Se reintenta en el siguiente
    Serial.println("save: hm1 no se pudo escribir");
    pendingSave = true;
    return;
  }
  // v3.4: las medallas del jugador ("pm", una entrada) despues de todo lo que cuentan (los dos huecos de casa,
  // la Pokedex, la racha, los pasos, las insignias, los recuerdos) y ANTES que las monedas: un corte entre las
  // dos pierde el pago, nunca lo repite. Si "pm" no llega, tampoco se escriben las monedas (ni lo de despues)
  if (!savePm()) {
    Serial.println("save: pm no se pudo escribir");
    pendingSave = true;
    return;
  }
  // monedas (Y4): aqui desde la v3.4 (antes, entre los del jugador), y solo si cambiaron (asi la ventana entre
  // el uid / "hm1" y la mochila no se alarga cuando no hay nada que pagar). Antes que la mochila y los mapas:
  // comprar nunca sale gratis
  if (!cnHave || coins != cnSaved || coinDay != cnDSaved || coinToday != cnTSaved) {
    // si alguna no llega, no se apunta como escrita y no se escribe la mochila (comprar nunca sale gratis)
    if (prefs.putUShort("cn", coins) != 2 || prefs.putUInt("cnD", coinDay) != 4 || prefs.putUShort("cnT", coinToday) != 2) {
      Serial.println("save: cn no se pudo escribir");
      cnHave = false;
      pendingSave = true;
      return;
    }
    cnSaved = coins;
    cnDSaved = coinDay;
    cnTSaved = coinToday;
    cnHave = true;
  }
  putPack();
  prefs.putUShort("ybgO", yardOwned);  // los mapas comprados (despues que "cn": un corte pierde el mapa, no las monedas)
  prefs.putUChar("ybg", yardChosen);
  // v3.20: las mejoras de casa, despues de las monedas (comprar un nivel nunca sale gratis) y de la mochila (la
  // ventana uid / "hm1" -> mochila no se alarga cada minuto de noche). Si no llega, se reintenta pero lo de
  // despues se guarda igual
  if (!saveUpg()) {
    Serial.println("save: upg no se pudo escribir");
    pendingSave = true;
  }
  {  // regalos (v1.55), despues de la mochila: [cuantos, objetos...]
    uint8_t g[GIFT_MAX + 1] = { giftN };
    memcpy(g + 1, gifts, giftN);
    prefs.putBytes("gifts", g, sizeof(g));
  }
  prefs.putBool("eggH", h.eggHeldFlag);
  // encuentros (v2.2), despues de la mochila: los huevos de captura son de los de la mochila (un corte
  // entre las dos deja un huevo de mas sin su especie, nunca una especie sin huevo: load los ajusta)
  saveCapEggs();
  prefs.putBool("ecap", h.eggCap);
  encPersist();
  // el resultado de un viaje sin ver (Y5), lo ultimo: un corte antes lo deja sin aviso, pero con el premio
  if (trvResPending) prefs.putBytes("trvR", &trvResult, sizeof(trvResult));
  else if (prefs.isKey("trvR")) prefs.remove("trvR");
}

// "cegg": los huevos de captura ([n, dex, shiny] x CAP_EGG_MAX)
void Pet::saveCapEggs() {
  uint8_t c[1 + 2 * CAP_EGG_MAX] = { capN };
  for (uint8_t i = 0; i < capN; i++) {
    c[1 + 2 * i] = capDex[i];
    c[2 + 2 * i] = capShiny[i] ? 1 : 0;
  }
  prefs.putBytes("cegg", c, sizeof(c));
}

// la mochila (y sus huevos de captura) ya, antes que el hueco: un huevo que pasa de casa a la mochila
// (endLife) queda escrito antes de que el hueco deje de tenerlo. Un corte entre los dos: un huevo de mas
// la mochila a la NVS: "bpack" (10), justo despues "bpk2" (las bayas) y "bpk3" (v3.14: el Cebo y los que vengan,
// 8 bytes fijos). Un corte entre ellas solo desfasa lo de los blobs de detras (lo de antes del corte ya esta)
void Pet::putPack() {
  prefs.putBytes("bpack", items, PACK_OLD);
  prefs.putBytes("bpk2", items + PACK_OLD, PACK2_LEN);
  memcpy(pack3Raw, items + PACK3_FIRST, IT_COUNT - PACK3_FIRST);  // los de despues (de otro firmware), tal cual
  prefs.putBytes("bpk3", pack3Raw, PACK3_LEN);
}

void Pet::saveBag() {
  putPack();
  saveCapEggs();
}

void Pet::load() {
  PetLife &h = slotRef(0);  // v3.0: las claves de siempre son el hueco 0
  h.fullness = prefs.getUChar("full", 80);
  h.joy = prefs.getUChar("joy", 80);
  h.energy = prefs.getUChar("ene", 80);
  h.hygiene = 100;  // v3.1: ya no se usa (la clave "hyg" se ignora)
  h.poops = prefs.getUChar("poop", 0);
  h.weight = prefs.getUChar("wgt", 0);
  h.satedUntil = prefs.getUInt("cake", 0);
  h.geneAtk = prefs.getUChar("gatk", 0);
  h.geneDef = prefs.getUChar("gdef", 0);
  h.geneSpe = prefs.getUChar("gspe", 0);
  if (h.geneAtk == 0) {  // mascota anterior a los genes: tirada unica ahora
    h.geneAtk = 90 + random(21);
    h.geneDef = 90 + random(21);
    h.geneSpe = 90 + random(21);
  }
  h.trAtk = prefs.getUChar("tatk", 0);
  h.trDef = prefs.getUChar("tdef", 0);
  h.trSpe = prefs.getUChar("tspe", 0);
  h.berryKnown = prefs.getBool("fk", false);
  h.shiny = prefs.getBool("shy", false);
  h.eggShiny = prefs.getBool("eshy", false);
  starterPick = prefs.getBool("stpk", false);
  prefs.getBytes("dexsh", dexShinyReg, sizeof(dexShinyReg));
  h.ageMinutes = prefs.getUInt("age", 0);
  // v3.1: la edad de las cacas. Sin la clave (un guardado anterior) o mal: recien hechas
  h.poopMask = (1 << (h.poops > POOP_MAX ? POOP_MAX : h.poops)) - 1;
  {
    uint32_t st[POOP_MAX];
    bool ok = prefs.getBytes("poopA", st, sizeof(st)) == sizeof(st);
    for (uint8_t i = 0; i < POOP_MAX; i++) h.poopAt[i] = (ok && st[i] <= h.ageMinutes) ? st[i] : h.ageMinutes;
  }
  if (prefs.isKey("dexn")) {
    h.speciesId = prefs.getShort("dexn", -1);
    h.eggTarget = prefs.getShort("eggT2", 4);
  } else {
    // migracion desde la version con indices de flash (0-8)
    static const uint8_t OLD2DEX[9] = { 4, 5, 6, 1, 2, 3, 7, 8, 9 };
    int8_t old = prefs.getChar("spec", -1);
    h.speciesId = (old >= 0 && old < 9) ? OLD2DEX[old] : -1;
    int8_t oldT = prefs.getChar("eggT", 0);
    h.eggTarget = (oldT >= 0 && oldT < 9) ? OLD2DEX[oldT] : 4;
  }
  h.eggTaps = prefs.getUChar("crack", 0);
  // el huevo del patio (Y3). Sin la clave (un guardado anterior) no lo es: un huevo a medias del sistema
  // de toques lo termina asi (3 toques, o solo a los 3 min de edad)
  h.eggPlaced = prefs.isKey("yegg") ? prefs.getUInt("yegg", 0) : 0;
  h.eggSaved = prefs.isKey("ysav") ? prefs.getUChar("ysav", 0) : 0;
  if (h.eggSaved > EGG_SHAKE_CAP) h.eggSaved = EGG_SHAKE_CAP;
  h.careMistakes = prefs.getUChar("mist", 0);
  h.sleeping = prefs.getBool("sleep", false);
  h.nightSleep = h.sleeping && prefs.getBool("nslp", false);
  h.bedNight = prefs.getUInt("bedn", 0);
  lastEnd = prefs.getUChar("lend", CER_NONE);
  h.neglectMin = prefs.getUChar("ngl", 0);
  h.deathMin = prefs.getUChar("dth", 0);
  if (h.deathMin > DEATH_MIN) h.deathMin = DEATH_MIN;
  h.dying = prefs.getBool("dying", false);
  h.dyingMin = prefs.getUChar("dym", 0);
  if (h.dyingMin > REVIVE_LIMIT_MIN) h.dyingMin = REVIVE_LIMIT_MIN;
  h.deathEgg = prefs.getShort("degg", 0);
  if (h.deathEgg < 0 || h.deathEgg > 151) h.deathEgg = 0;
  prefs.getBytes("dexreg", dexReg, sizeof(dexReg));
  streak = prefs.getUShort("strk", 0);
  bestStreak = prefs.getUShort("bstrk", 0);
  lastCareDay = prefs.getUInt("cday", 0);
  h.bond = prefs.getUChar("bond", 0);
  lastMilestone = prefs.getUShort("mstone", 0);
  gameHi = prefs.getUShort("ghi", 0);
  strHi = prefs.getUShort("shi", 0);
  zenHi = prefs.getUShort("zhi", 0);
  dashHi = prefs.getUShort("dhi", 0);
  prefs.getString("nick", h.nick, sizeof(h.nick));
  // pasos y mochila (v1.20): un guardado anterior no trae las claves y quedan a 0
  stepsToday = prefs.getUInt("stpT", 0);
  stepsTotal = prefs.getUInt("stpA", 0);
  stepDay = prefs.getUInt("stpD", 0);
  findsToday = prefs.getUChar("fndT", 0);
  stepJoyToday = prefs.getUChar("sjoy", 0);
  goalDay = prefs.getUInt("goalD", 0);
  // v1.25: hasta entonces habia una sola meta; un guardado con goalD la tiene alcanzada
  goalTier = prefs.getUChar("goalN", goalDay ? 1 : 0);
  if (goalTier > STEP_GOAL_TIERS) goalTier = STEP_GOAL_TIERS;
  bonusFindsToday = prefs.getUChar("fndB", 0);
  if (bonusFindsToday > findsToday) bonusFindsToday = findsToday;
  joyRem = prefs.getUShort("jRem", 0) % STEPS_PER_JOY;
  weightRem = prefs.getUShort("wRem", 0) % STEPS_PER_WEIGHT;
  bondRem = prefs.getUShort("bRem", 0) % STEPS_PER_BOND;
  findAt = prefs.getUShort("fAt", 0);
  if (findAt && (findAt < FIND_MIN_STEPS || findAt > FIND_MAX_STEPS)) findAt = 0;
  findRem = prefs.getUShort("fRem", 0);
  if (findAt && findRem >= findAt) findRem = findAt - 1;
  eggAt = prefs.getUShort("eAt", 0);  // huevos (v1.39): un guardado anterior empieza a contar ahora
  if (eggAt && (eggAt < EGG_MIN_STEPS || eggAt > EGG_MAX_STEPS)) eggAt = 0;
  eggRem = prefs.getUShort("eRem", 0);
  if (eggAt && eggRem >= eggAt) eggRem = eggAt - 1;
  memset(items, 0, sizeof(items));
  // v3.2: "bpack" sigue con los 10 primeros objetos (un firmware anterior lo lee en un buffer de 10 y un blob mas
  // largo le daria una mochila vacia); las 6 bayas de colores van en "bpk2". Sin "bpk2": 0 bayas
  prefs.getBytes("bpack", items, PACK_OLD);
  if (prefs.isKey("bpk2")) prefs.getBytes("bpk2", items + PACK_OLD, PACK2_LEN);
  // v3.14: "bpk3" (8 bytes) desde IT_LURE. Sin ella (un guardado anterior): 0 Cebos
  memset(pack3Raw, 0, sizeof(pack3Raw));
  if (prefs.isKey("bpk3") && prefs.getBytes("bpk3", pack3Raw, PACK3_LEN) == PACK3_LEN)
    memcpy(items + PACK3_FIRST, pack3Raw, IT_COUNT - PACK3_FIRST);
  else memset(pack3Raw, 0, sizeof(pack3Raw));
  {
    uint8_t g[GIFT_MAX + 1] = { 0 };
    giftN = 0;
    // isKey: un guardado anterior a la v1.55 no lo tiene, y getBytes lo anotaria como error en el log
    if (prefs.isKey("gifts") && prefs.getBytes("gifts", g, sizeof(g)) == sizeof(g))
      for (uint8_t i = 0; i < g[0] && i < GIFT_MAX; i++)
        if (g[1 + i] < IT_COUNT) gifts[giftN++] = g[1 + i];
  }
  for (uint8_t i = 0; i < IT_COUNT; i++) {
    uint8_t cap = itemCap(i);
    if (items[i] > cap) items[i] = cap;
  }
  // v1.59: una partida de antes del Revivir recibe los 3 de una partida nueva, una sola vez. La mochila
  // se escribe antes que "rvg": un corte entre las dos los da otra vez, nunca se quedan sin darse
  if (!prefs.isKey("rvg")) {
    uint16_t n = items[IT_REVIVE] + REVIVE_START;
    items[IT_REVIVE] = n > ITEM_MAX ? ITEM_MAX : n;
    putPack();
    prefs.putBool("rvg", true);
  }
  // v2.2: una partida de antes de la Poke Ball recibe las 3 de una partida nueva, una vez (como "rvg")
  if (!prefs.isKey("ballg")) {
    uint16_t n = items[IT_BALL] + BALL_START;
    items[IT_BALL] = n > ITEM_MAX ? ITEM_MAX : n;
    putPack();
    prefs.putBool("ballg", true);
  }
  // encuentros (v2.2): un guardado anterior no trae las claves (sin encuentro, sin huevos de captura)
  capN = 0;
  if (prefs.isKey("cegg")) {
    uint8_t c[1 + 2 * CAP_EGG_MAX] = { 0 };
    if (prefs.getBytes("cegg", c, sizeof(c)) == sizeof(c))
      for (uint8_t i = 0; i < c[0] && i < CAP_EGG_MAX; i++) {
        uint8_t d = c[1 + 2 * i];
        if (d < 1 || d > DEX_COUNT) continue;
        capDex[capN] = d;
        capShiny[capN++] = c[2 + 2 * i] != 0;
      }
  }
  if (capN > items[IT_EGG]) capN = items[IT_EGG];
  h.eggCap = prefs.getBool("ecap", false);
  encDay = 0;
  encRolled = 0;
  encSpawnAt = 0;
  encFlags = 0;
  if (prefs.isKey("enc")) {
    uint8_t e[16] = { 0 };
    if (prefs.getBytes("enc", e, sizeof(e)) == sizeof(e)) {
      memcpy(&encDay, e, 4);
      memcpy(&encSpawnAt, e + 4, 4);
      memcpy(&encDexV, e + 8, 2);
      encLv = e[10];
      encRolled = e[11];
      encFlags = e[12];
      memcpy(encTr, e + 13, 3);
      if (encDexV < 1 || encDexV > DEX_COUNT || !encLv) {
        encSpawnAt = 0;
        encFlags = 0;
      }
    }
  }
  coins = prefs.getUShort("cn", 0);  // un guardado anterior no trae las claves: 0
  if (coins > COIN_MAX) coins = COIN_MAX;
  coinDay = prefs.getUInt("cnD", 0);
  coinToday = prefs.getUShort("cnT", 0);
  cnHave = prefs.isKey("cn");  // v3.4: lo que hay en la NVS (save() no lo repite)
  cnSaved = coins;
  cnDSaved = coinDay;
  cnTSaved = coinToday;
  // v3.20: las mejoras de casa. Sin "upg", o con uno de otra version o danado: nada comprado (sin migraciones)
  memset(upgTiers, 0, sizeof(upgTiers));
  memset(upgSlots, 0, sizeof(upgSlots));
  upgHave = false;
  if (prefs.isKey("upg")) {
    UpgSave u;
    memset(&u, 0, sizeof(u));
    if (prefs.getBytes("upg", &u, sizeof(u)) == sizeof(u) && upgValid(u)) {
      memcpy(upgTiers, u.tier, sizeof(upgTiers));
      memcpy(upgSlots, u.slot, sizeof(upgSlots));
      upgSaved = u;
      upgHave = true;
    } else {
      Serial.println("upg: danado, sin mejoras de casa");
    }
  }
  // la coleccion. v3.15 (migracion): sin "souv2" se lee la vieja "souv" (u32, los 25 primeros) en los bits bajos;
  // sin ninguna (un guardado de antes del Y5), sin recuerdos
  souvOwned = souvLoadMask(prefs.isKey("souv2"), prefs.getULong64("souv2", 0), prefs.getUInt("souv", 0));
  // "souvD": 100 bytes (25 fechas) antes de v3.15, 180 ahora. Se lee lo que haya hasta el tamano del buffer y el
  // resto queda a 0 (un blob mas largo, de un firmware futuro, se lee por un temporal y se recorta)
  memset(souvDate, 0, sizeof(souvDate));
  if (prefs.isKey("souvD")) {
    size_t n = prefs.getBytesLength("souvD");
    if (n && n <= sizeof(souvDate)) {
      prefs.getBytes("souvD", souvDate, n);
    } else if (n > sizeof(souvDate) && n <= 64 * sizeof(uint32_t)) {
      uint32_t tmp[64] = { 0 };
      if (prefs.getBytes("souvD", tmp, n) == n) memcpy(souvDate, tmp, sizeof(souvDate));
    }
  }
  trvSlot = TRV_NONE;
  trvResPending = false;
  if (prefs.isKey("trv")) {  // sin la clave no hay viaje
    uint8_t t[16] = { 0 };
    if (prefs.getBytes("trv", t, sizeof(t)) == sizeof(t) && t[0] < BOX_SLOTS && t[1] >= 1 && t[1] <= TRV_DESTS &&
        t[2] < TRV_TIERS) {
      trvSlot = t[0];
      trvDest = t[1];
      trvTier = t[2];
      memcpy(&trvUid, t + 4, 4);
      memcpy(&trvStart, t + 8, 4);
      memcpy(&trvSeed, t + 12, 4);
    }
  }
  if (prefs.isKey("trvR") && prefs.getBytes("trvR", &trvResult, sizeof(trvResult)) == sizeof(trvResult) && trvResult.valid) {
    trvResPending = true;
    if (trvResult.item != TRV_NONE && trvResult.item >= IT_COUNT) trvResult.item = TRV_NONE;  // id desconocido: se descarta
  }
  yardOwned = prefs.getUShort("ybgO", 0) & (uint16_t)(((1u << (YARD_MAPS + 1)) - 1) & ~1u);
  yardChosen = prefs.getUChar("ybg", 0);
  if (yardChosen > YARD_MAPS) yardChosen = 0;
  h.bondDay = prefs.getUInt("bondD", 0);
  h.bondToday = prefs.getUChar("bondT", 0);
  // estadios: un guardado anterior no trae las claves (sin medallas, nada pagado, sin equipo)
  badges = prefs.getUChar("bdg", 0);
  stageDay = prefs.getUInt("stD", 0);
  stageMask = prefs.getUChar("stM", 0);
  memset(team, 0, sizeof(team));
  if (prefs.isKey("team") && prefs.getBytes("team", team, sizeof(team)) != sizeof(team)) memset(team, 0, sizeof(team));
  // crecimiento (v1.35). Un guardado anterior no trae "grow": el nivel salia de la edad
  // (1 nivel por hora). Se traduce a la curva nueva con el mismo nivel y la barra igual de
  // llena, para que nadie suba ni baje de nivel al actualizar. Va despues de speciesId.
  if (prefs.isKey("grow")) h.growMin = prefs.getUInt("grow", 0);
  else h.growMin = h.isEgg() ? 0 : growthFromLegacyAge(h.ageMinutes);
  // siembra: la mascota actual cuenta como criada (guardados antiguos)
  if (h.speciesId >= 1) registerSpecies(h.speciesId, h.shiny);
  // caja (v1.37). Sin la clave "uid" (guardado anterior) queda a 0 y begin() le da una despues
  // de boxBoot(); un 0 guardado es un huevo, y boxBoot() lo necesita tal cual
  h.uid = prefs.getUInt("uid", 0);
  h.eggHeldFlag = prefs.getBool("eggH", false);
  // casa vacia (v1.39): solo sin bicho nacido (un corte a mitad de un despertar puede dejar "vac"
  // a medias; boxBoot decide por "boxT" y "eggH")
  h.vacant = prefs.getBool("vac", false) && h.speciesId < 0 && !h.uid;
  // v3.0: el primer cuidado del hueco 0 (sin la clave, el de la racha: hoy ya le dio su vinculo)
  h.careDay = prefs.isKey("pcd") ? prefs.getUInt("pcd", 0) : lastCareDay;
  // v3.4: lo que el hueco 0 ya conto para las medallas (de su uid; si no, nada: lo cuenta la migracion o
  // medalsUpdate) y las medallas del jugador. Sin "pm" valido, begin() migra
  h.counted = 0;
  if (prefs.isKey("pmc")) {
    uint32_t pc[2] = { 0, 0 };
    if (prefs.getBytes("pmc", pc, sizeof(pc)) == sizeof(pc) && pc[0] && pc[0] == h.uid) h.counted = (uint8_t)(pc[1] & PMB_ALL);
  }
  pmReady = pmHave = false;
  pmClear(pm);
  pmKeyBad = false;
  if (prefs.isKey("pm")) {
    pmKeyBad = true;  // (hasta ver que se puede cargar)
    PlayerMedals m;
    memset(&m, 0, sizeof(m));
    if (prefs.getBytes("pm", &m, sizeof(m)) == sizeof(m) && pmValid(m)) {
      pm = m;
      pmSaved = m;
      pmReady = pmHave = true;
      pmKeyBad = false;
    } else {
      Serial.println("pm: danado, se vuelven a contar las medallas");
    }
  }
  // v3.0: el hueco 1. Sin "hm1" (un guardado de la v2.x) esta vacio
  PetLife &h1 = slotRef(1);
  h1 = vacantPetLife();
  hm1Have = false;
  if (prefs.isKey("hm1")) {
    HomeRec r;
    memset(&r, 0, sizeof(r));
    if (prefs.getBytes("hm1", &r, sizeof(r)) == sizeof(r) && homeRecValid(r)) {
      homeRecApply(r, h1);
      hm1Saved = r;
      hm1Have = true;
      if (h1.speciesId >= 1) registerSpecies(h1.speciesId, h1.shiny);
    } else {
      Serial.println("hm1: danado, el hueco 2 queda vacio");
    }
  }
}

// "hm1": el hueco 1 entero, solo si cambio (no se escribe nada mientras siga vacio sin haberse usado)
bool Pet::saveHome1() {
  HomeRec r = homeRecOf(home(1), 0);  // (sin hora: asi un hueco que no cambia no se reescribe)
  if (hm1Have ? memcmp(&r, &hm1Saved, sizeof(r)) == 0 : (r.kind == HOME_VACANT && !r.flags)) return true;
  if (prefs.putBytes("hm1", &r, sizeof(r)) != sizeof(r)) return false;
  hm1Saved = r;
  hm1Have = true;
  return true;
}


// ---------- la caja (docs/NEXT_FEATURES.md, Tarea BOX, v1.37) ----------
//
// Cada movimiento escribe primero la copia entera del bicho que se mueve y borra la de sobra al
// final, y el uid es lo ultimo que escribe save(). Asi, corte la luz donde corte, el siguiente
// arranque (boxBoot) encuentra al bicho entero en algun sitio y, como mucho, repetido:
//   guardar A en k:        1. box k = A                2. huevo activo, save() (uid 0 al final)
//   despertar B con A:     1. boxT = B   2. box k = A   3. B activo, save()   4. borrar boxT
//   despertar B con huevo: 1. boxT = B   2. eggH        3. B activo, save()   4. vaciar k   5. borrar boxT

static_assert(sizeof(((PetRecord *)0)->nick) == sizeof(((PetLife *)0)->nick), "el apodo cabe entero en el registro");

static void slotKey(char *key, uint8_t k) {
  key[0] = 'b';
  key[1] = 'o';
  key[2] = 'x';
  key[3] = (char)('0' + k);
  key[4] = 0;
}

// ---------- curva de niveles v2 (growth.h, tras la v1.49) ----------
//
// Un guardado sin "gcv" cuenta el crecimiento con la curva v1 (v1.35-v1.49, hasta 3 h por nivel).
// Cada contador (el del activo, el de cada hueco de la caja y el de un despertar a medias en
// "boxT") pasa a la curva nueva con el mismo nivel y la barra igual de llena (growthFromCurveV1).
// Convertir dos veces subiria el nivel, asi que primero se calculan todos los valores nuevos y se
// escriben juntos en "gmig"; despues se aplican y al final "gcv". Un corte de luz a mitad repite
// la aplicacion desde "gmig" con los mismos valores (absolutos), nunca convierte otra vez.
// Un guardado sin "grow" (anterior a la v1.35) ya sale de load() en la curva nueva.

struct __attribute__((packed)) GrowMig {
  uint32_t grow;                 // el activo (si hasGrow)
  uint32_t slotUid[BOX_SLOTS];   // 0 = hueco sin migrar (vacio o danado)
  uint32_t slotGrow[BOX_SLOTS];
  uint32_t tUid, tGrow;          // "boxT" (tUid 0 = no habia)
  uint8_t hasGrow;
};

void Pet::migrateGrowthCurve() {
  if (prefs.isKey("gcv")) {
    if (prefs.isKey("gmig")) prefs.remove("gmig");  // un corte justo despues de "gcv"
    return;
  }
  char key[6];
  PetRecord r;
  GrowMig m;
  memset(&m, 0, sizeof(m));
  if (!(prefs.isKey("gmig") && prefs.getBytes("gmig", &m, sizeof(m)) == sizeof(m))) {
    // 1. los valores nuevos, todos a la vez
    memset(&m, 0, sizeof(m));
    bool any = false;
    if (prefs.isKey("grow")) {
      m.hasGrow = 1;
      m.grow = growthFromCurveV1(prefs.getUInt("grow", 0));
      any = true;
    }
    for (uint8_t k = 0; k < BOX_SLOTS; k++) {
      slotKey(key, k);
      if (prefs.isKey(key) && prefs.getBytes(key, &r, sizeof(r)) == sizeof(r) && recordValid(r)) {
        m.slotUid[k] = r.uid;
        m.slotGrow[k] = growthFromCurveV1(r.growMin);
        any = true;
      }
    }
    if (prefs.isKey("boxT") && prefs.getBytes("boxT", &r, sizeof(r)) == sizeof(r) && recordValid(r)) {
      m.tUid = r.uid;
      m.tGrow = growthFromCurveV1(r.growMin);
      any = true;
    }
    // sin sitio para "gmig" se aplica igual: solo se pierde la proteccion frente a un corte
    if (any) prefs.putBytes("gmig", &m, sizeof(m));
  }
  // 2. aplicarlos (repetible)
  if (m.hasGrow) {
    growMin = m.grow;
    prefs.putUInt("grow", growMin);
  }
  for (uint8_t k = 0; k < BOX_SLOTS; k++) {
    if (!m.slotUid[k]) continue;
    slotKey(key, k);
    if (prefs.getBytes(key, &r, sizeof(r)) == sizeof(r) && recordValid(r) && r.uid == m.slotUid[k] &&
        r.growMin != m.slotGrow[k]) {
      r.growMin = m.slotGrow[k];
      recordSeal(r);
      prefs.putBytes(key, &r, sizeof(r));
    }
  }
  if (m.tUid && prefs.isKey("boxT") && prefs.getBytes("boxT", &r, sizeof(r)) == sizeof(r) && recordValid(r) &&
      r.uid == m.tUid && r.growMin != m.tGrow) {
    r.growMin = m.tGrow;
    recordSeal(r);
    prefs.putBytes("boxT", &r, sizeof(r));
  }
  // 3. hecho
  prefs.putUChar("gcv", GROW_CURVE_VER);
  if (prefs.isKey("gmig")) prefs.remove("gmig");
}

PetRecord PetLife::record(uint32_t epoch) const {
  PetRecord r;
  memset(&r, 0, sizeof(r));
  r.flags = (shiny ? REC_SHINY : 0) | (berryKnown ? REC_FAV6 : 0) | (sleeping ? REC_SLEEP : 0) |
            (careDay && careDay == bondDay ? REC_CARED : 0) |  // v3.0: su primer cuidado de ese dia, ya dado
            (sleeping && nightSleep ? REC_NIGHT : 0) |         // v3.19: dormia de noche, no una siesta
            REC_V3;  // v3.4: medals = lo que ya conto para las medallas del jugador
  r.uid = uid;
  r.speciesId = speciesId;
  memcpy(r.nick, nick, sizeof(r.nick));
  r.nick[sizeof(r.nick) - 1] = 0;
  r.ageMinutes = ageMinutes;
  r.growMin = growMin;
  r.fullness = fullness;
  r.joy = joy;
  r.energy = energy;
  r.hygiene = 100;  // v3.1: sin uso (formato del registro)
  r.poops = poops;
  r.weight = weight;
  r.geneAtk = geneAtk;
  r.geneDef = geneDef;
  r.geneSpe = geneSpe;
  r.trAtk = trAtk;
  r.trDef = trDef;
  r.trSpe = trSpe;
  r.careMistakes = careMistakes;
  r.bond = bond;
  r.bondToday = bondToday;
  r.bondDay = bondDay;
  r.medals = counted;
  r.storedEpoch = epoch;
  recordSeal(r);
  return r;
}

// Pasa a ser el bicho de r. Solo cambia lo del individuo: racha, pasos, mochila, records y Pokedex
// son del jugador, y la incubadora del hueco de casa (los campos del huevo), y se quedan. Lo que solo vive en RAM (enfriamientos,
// "mantener forma", animaciones) empieza de cero, como tras un reinicio.
void PetLife::loadRecord(const PetRecord &r) {
  vacant = false;
  uid = r.uid;
  speciesId = r.speciesId;
  prevSpeciesId = r.speciesId;
  shiny = (r.flags & REC_SHINY) != 0;
  berryKnown = (r.flags & REC_FAV6) != 0;  // v3.2: el REC_BERRY de antes (otra baya favorita) se ignora
  sleeping = (r.flags & REC_SLEEP) != 0;
  // v3.19: de noche (REC_NIGHT) se despierta solo por la mañana; una siesta no (antes todo dormido era de noche, y
  // una siesta despertada de la caja de dia se despertaba al minuto). Un registro de antes carga como siesta
  nightSleep = sleeping && (r.flags & REC_NIGHT) != 0;
  nightAwakeMin = 0;
  memcpy(nick, r.nick, sizeof(nick));
  nick[sizeof(nick) - 1] = 0;
  ageMinutes = r.ageMinutes;
  growMin = r.growMin;
  fullness = r.fullness;
  joy = r.joy;
  energy = r.energy;
  hygiene = 100;
  poops = r.poops > 3 ? 3 : r.poops;
  poopMask = (1 << poops) - 1;  // sus cacas, en los primeros huecos
  for (uint8_t i = 0; i < POOP_MAX; i++) poopAt[i] = r.ageMinutes;  // recien hechas (no lleva su edad)
  weight = r.weight;
  satedUntil = 0;  // la tarta no viaja en la caja (v1.54): quien sale de ella no esta lleno
  geneAtk = r.geneAtk;
  geneDef = r.geneDef;
  geneSpe = r.geneSpe;
  trAtk = r.trAtk;
  trDef = r.trDef;
  trSpe = r.trSpe;
  careMistakes = r.careMistakes;
  bond = r.bond;
  bondToday = r.bondToday;
  bondDay = r.bondDay;
  careDay = (r.flags & REC_CARED) ? r.bondDay : 0;  // v3.0 (un registro de antes: puede darle el suyo hoy)
  // v3.4: lo que ya conto para las medallas del jugador; un registro de antes no lo sabe (Pet::fromRecord mira
  // lo que se apunto al migrar, y si no, cuenta al despertarlo)
  counted = (r.flags & REC_V3) ? (uint8_t)(r.medals & PMB_ALL) : 0;
  ceremony = CER_NONE;
  mistakeCooldown = 0;
  neglectMin = deathMin = dyingMin = 0;
  dying = false;
  deathEgg = 0;
  goodTicks = 0;
  petBondUntil = 0;
  evoDeclinedLv = 0;
  farDeclinedAge = 0;
  eatUntil = heartUntil = evolveUntil = 0;
  emoteUntil = 0;
  diedNow = false;
  lastTickHour = -1;
  grumpied = dizzyPenalized = false;
  shakeN = 0;
}

void Pet::readSlot(uint8_t k) {
  char key[6];
  slotKey(key, k);
  memset(&boxRec[k], 0, sizeof(PetRecord));
  if (!prefs.isKey(key)) {
    boxSt[k] = BOX_EMPTY;
    return;
  }
  size_t n = prefs.getBytes(key, &boxRec[k], sizeof(PetRecord));
  boxSt[k] = (n == sizeof(PetRecord) && recordValid(boxRec[k])) ? BOX_OK : BOX_DAMAGED;
}

bool Pet::writeSlot(uint8_t k, PetRecord r) {
  char key[6];
  slotKey(key, k);
  recordSeal(r);
  if (prefs.putBytes(key, &r, sizeof(r)) != sizeof(r)) return false;
  boxRec[k] = r;
  boxSt[k] = BOX_OK;
  return true;
}

bool Pet::clearSlot(uint8_t k) {
  char key[6];
  slotKey(key, k);
  prefs.remove(key);
  if (prefs.isKey(key)) return false;  // no se pudo borrar: sigue donde estaba
  memset(&boxRec[k], 0, sizeof(PetRecord));
  boxSt[k] = BOX_EMPTY;
  return true;
}

int8_t Pet::slotOf(uint32_t id) const {
  if (!id) return -1;
  for (uint8_t k = 0; k < BOX_SLOTS; k++)
    if (boxSt[k] == BOX_OK && boxRec[k].uid == id) return (int8_t)k;
  return -1;
}

uint32_t Pet::newUid() const {
  for (;;) {
    uint32_t id = ((uint32_t)random(0x10000) << 16) | (uint32_t)random(0x10000);
    if (id && id != home(0).uid && id != home(1).uid && slotOf(id) < 0) return id;  // v3.0: ni los dos de casa
  }
}

uint8_t Pet::boxUsed() const {
  uint8_t n = 0;
  for (uint8_t k = 0; k < BOX_SLOTS; k++)
    if (boxSt[k] != BOX_EMPTY) n++;
  return n;
}

bool Pet::store(uint8_t k, uint8_t s) {
  if (!canStore(k, s)) return false;
  FocusGuard g(*this, s);  // v3.0: el del hueco de casa s
  if (!g.ok) return false;
  // (v3.0, hueco 1) lo que freeHome puede cambiar, por si "hm1" no llega a escribirse
  medalsUpdate();  // v3.4: lo pendiente, contado antes de la foto (asi deshacer no pierde nada del otro hueco)
  PetLife before = *this;
  MedalSnap medB = medalSnap();  // v3.4: y lo que contaron las medallas en ese save()
  uint8_t itemsB[IT_COUNT], giftsB[GIFT_MAX], giftNB = giftN, capNB = capN, capDexB[CAP_EGG_MAX];
  bool capShinyB[CAP_EGG_MAX];
  memcpy(itemsB, items, sizeof(items));
  memcpy(giftsB, gifts, sizeof(gifts));
  memcpy(capDexB, capDex, sizeof(capDex));
  memcpy(capShinyB, capShiny, sizeof(capShiny));
  if (!writeSlot(k, toRecord())) return false;  // 1. la copia entera, a su hueco
  // 2. el huevo que esperaba, uno de la mochila o nadie; save() deja el uid (0) casi al final (hueco 0) o
  // "hm1" (hueco 1)
  freeHome();
  if (s == 1 && !saveOk) {
    // "hm1" no se escribio: el bicho sigue en casa (y en el hueco k, que el arranque resuelve: gana la
    // copia de la caja, que es el mismo). Se deshace en RAM y se intenta vaciar k
    static_cast<PetLife &>(*this) = before;
    medalRestore(medB);
    memcpy(items, itemsB, sizeof(items));
    memcpy(gifts, giftsB, sizeof(gifts));
    giftN = giftNB;
    capN = capNB;
    memcpy(capDex, capDexB, sizeof(capDex));
    memcpy(capShiny, capShinyB, sizeof(capShiny));
    clearSlot(k);
    return false;
  }
  return true;
}

bool Pet::wake(uint8_t k, uint8_t s) {
  if (!canWake(k, s)) return false;
  FocusGuard g(*this, s);  // v3.0: al hueco de casa s
  if (!g.ok) return false;
  PetRecord b = boxRec[k];
  medalsUpdate();  // v3.4: lo pendiente, contado antes de la foto (asi deshacer no pierde nada del otro hueco)
  PetLife before = *this;
  MedalSnap medB = medalSnap();  // v3.4: si se deshace, B tampoco cuenta para las medallas
  bool egg = isEgg() && !vacant;
  // 0. (v3.0) a que hueco de casa va, antes que nada: sin "boxTs" el arranque entiende el 0 (asi un
  // despertar al 0 escribe lo mismo que antes; uno que quedara de otro se borra primero)
  if (s) {
    if (prefs.putUChar("boxTs", s) != 1) return false;
  } else if (prefs.isKey("boxTs")) {
    prefs.remove("boxTs");
    if (prefs.isKey("boxTs")) return false;
  }
  // 1. con un huevo, el huevo espera en la incubadora, tal cual. Va ANTES que boxT (v1.39): asi un
  // despertar cortado sin el uid de B se sabe si venia de un huevo (eggH) o del hueco vacio. Hueco 1: en "hm1"
  if (egg) {
    eggHeldFlag = true;
    bool ok = s == 0 ? prefs.putBool("eggH", true) == 1 : saveHome1();
    if (s && !ok) {
      eggHeldFlag = false;
      prefs.remove("boxTs");
      return false;
    }
  }
  // 2. la copia de transferencia: mientras exista, el arranque sabe que hay un despertar a medias
  if (prefs.putBytes("boxT", &b, sizeof(b)) != sizeof(b)) {
    if (egg) {
      eggHeldFlag = false;
      if (s == 0) prefs.putBool("eggH", false);
      else saveHome1();
    }
    if (s) prefs.remove("boxTs");
    return false;
  }
  if (isEgg()) {  // un huevo (que espera) o el hueco vacio: B pasa a casa y su hueco queda libre
    fromRecord(b);  // 3. B activo (save() escribe su uid casi al final, o "hm1")
    save();
    if (s == 1 && !saveOk) {
      // "hm1" no llego: en la NVS B sigue en su hueco y "boxT" lo dice. Se deshace en RAM; boxT y boxTs se
      // quedan (el arranque rehace el huevo o deja el hueco vacio)
      static_cast<PetLife &>(*this) = before;
      medalRestore(medB);
      return false;
    }
    if (!clearSlot(k)) return true;  // 4. su hueco, libre (si falla, boxT se queda y lo hace el arranque)
  } else {
    if (!writeSlot(k, toRecord())) {  // 2. el activo, al hueco de B
      prefs.remove("boxT");
      if (s) prefs.remove("boxTs");
      return false;
    }
    fromRecord(b);  // 3.
    save();
    // hueco 1 sin "hm1" escrito: A ya esta en el hueco k y B solo en boxT. Se quedan boxT y boxTs: un
    // guardado que llegue, o el arranque, termina el cambio (B en casa)
    if (s == 1 && !saveOk) return false;
  }
  prefs.remove("boxT");  // hecho
  if (s) prefs.remove("boxTs");
  return true;
}

bool Pet::boxClear(uint8_t k) {
  if (k >= BOX_SLOTS) return false;
  if (slotTravelling(k)) travelCancel();  // Y5: forzar el borrado de un viajero cancela el viaje (antes que el hueco)
  return boxSt[k] == BOX_EMPTY || clearSlot(k);
}

bool Pet::boxPut(uint8_t k, PetRecord r) {
  if (k >= BOX_SLOTS || boxSt[k] != BOX_EMPTY || r.speciesId < 1 || r.speciesId > 151) return false;
  r.uid = newUid();
  return writeSlot(k, r);
}

// Al arrancar, antes que nada: lee los huecos y, si un corte de luz pillo un movimiento a medias,
// lo termina o lo deshace. El uid guardado del activo dice hasta donde llego save() (va el ultimo); el
// del hueco 1 (v3.0), "hm1". "boxTs" dice a que hueco de casa iba un despertar cortado (sin el, al 0).
void Pet::boxBoot() {
  for (uint8_t k = 0; k < BOX_SLOTS; k++) readSlot(k);
  bool hasTs = prefs.isKey("boxTs");
  uint8_t ts = hasTs ? (prefs.getUChar("boxTs", 0) & 1) : 0;
  PetRecord t;
  memset(&t, 0, sizeof(t));
  bool hasT = prefs.isKey("boxT");
  bool tOk = hasT && prefs.getBytes("boxT", &t, sizeof(t)) == sizeof(t) && recordValid(t);
  for (uint8_t s = 0; s < HOME_SLOTS; s++) {
    FocusGuard g(*this, s);
    if (!g.ok) continue;
    boxBootSlot(s, hasT && s == ts, t, tOk);
  }
  if (hasT) prefs.remove("boxT");
  if (hasTs) prefs.remove("boxTs");
  // v3.0: el mismo bicho en los dos huecos de casa (no deberia pasar): se queda en el 0
  if (home(1).hatched() && home(1).uid && home(1).uid == home(0).uid) {
    Serial.println("box: el hueco 2 repetia al 1, se vacia");
    FocusGuard g(*this, 1);
    if (g.ok) {
      if (eggHeldFlag) newEgg();  // el huevo que esperaba en su incubadora vuelve ya (no al otro arranque)
      else vacate();
    }
  }
}

// lo de un hueco de casa (el del foco). wakeHere: el despertar cortado ("boxT" = t) iba a este hueco
void Pet::boxBootSlot(uint8_t s, bool wakeHere, const PetRecord &t, bool tOk) {
  if (wakeHere) {
    if (tOk && uid == t.uid) {
      // el despertar llego a guardar a B: solo puede quedar su copia en el hueco (si habia huevo)
      for (uint8_t k = 0; k < BOX_SLOTS; k++)
        if (boxSt[k] == BOX_OK && boxRec[k].uid == uid) clearSlot(k);
    } else if (tOk && uid && slotOf(uid) >= 0) {
      // intercambio cortado con el activo ya en el hueco de B: B solo esta en boxT. Se termina.
      fromRecord(t);
      save();
    } else if (!uid) {
      // despertar sin bicho nacido activo que no llego a guardar a B (B sigue en su hueco). El
      // save() de B pudo empezar a pisar la casa. Con un huevo, "eggH" ya estaba escrito (va antes
      // que boxT): el huevo se rehace desde la incubadora (especie, tirada shiny y grietas; solo
      // pierde los minutos que llevaba). Sin "eggH" la casa estaba vacia (v1.39), y vuelve a estarlo
      if (eggHeldFlag) newEgg();
      else vacate();
    }
  }
  // un guardado cortado antes de que el huevo quedara escrito: la copia del hueco esta entera y
  // la del activo puede estar a medias. Gana la del hueco y el hueco se vacia (se guarda otra vez)
  int8_t k = slotOf(uid);
  if (k >= 0) {
    fromRecord(boxRec[k]);
    save();
    clearSlot((uint8_t)k);
  }
  // el huevo solo espera mientras hay un bicho nacido despierto (un corte entre el uid y "eggH").
  // Con el hueco vacio no deberia pasar nunca; si pasa, el huevo que esperaba vuelve a casa
  if (isEgg() && eggHeldFlag) {
    if (vacant) {
      newEgg();
    } else {
      eggHeldFlag = false;
      if (s == 0) prefs.putBool("eggH", false);
      else saveHome1();
    }
  }
}
