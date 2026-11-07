#include "pet.h"
#include "dex.h"
#include "audio.h"

void Pet::begin() {
  prefs.begin("tamapoke", false);
  if (!prefs.getBool("init", false)) {
    prefs.putBool("init", true);
    newEgg();
  } else {
    load();
  }
  boxBoot();  // la caja (v1.37): la lee y termina o deshace un movimiento que corto la luz
  // guardado anterior a la caja: el bicho de ahora recibe su identidad (FR-BOX.11)
  if (!isEgg() && !uid) {
    uid = newUid();
    prefs.putUInt("uid", uid);
  }
  // v1.40: la casa vacia con huevos en la mochila es un estado normal ("Dejar casa vacia" al soltar):
  // ya no se saca uno al arrancar. La casa vacia lo ofrece con su boton.
  lastTick = millis();
}

// lo comun de empezar con un huevo y de quedarse con la casa vacia: nadie, a cero
void Pet::resetHome() {
  ceremony = CER_NONE;
  neglectTicks = 0;
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
}

void Pet::newEgg(bool saveNow, bool charmOk) {
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
    starterPick = (registeredCount() == 0);  // primera partida: el jugador elige inicial
    // amuleto shiny de la mochila: lo gasta este huevo. No el inicial, ni
    // pickEggSpecies(), que tambien usa la simulacion de tiradas (comando EGGS).
    bool charm = charmOk && items[IT_CHARM] && !starterPick;
    if (charm) items[IT_CHARM] = 0;
    eggShiny = (random(shinyOdds(charm)) == 0);
    eggTaps = 0;
  }
  persist(saveNow);
}

// La casa queda libre (fin de una crianza o guardar en la caja). Huevos limitados (v1.39): vuelve el
// que esperaba en la incubadora; si no, sale uno de la mochila (y se sortea ahora, con el amuleto si
// lo hay); si tampoco, la casa se queda vacia.
// v1.40: sin huevos y sin nadie en la caja llega un huevo de regalo (tirada normal, sin amuleto):
// nunca se acaba sin bicho, sin huevo y sin nadie a quien despertar. HOME_EMPTY ("Dejar casa vacia"
// al soltar) deja los huevos donde estan; el que esperaba pasa a la mochila (su especie y su tirada
// shiny se pierden: se sortea de nuevo al salir). save() escribe la mochila antes que "eggH", asi
// que un corte entre los dos deja un huevo de mas, nunca uno de menos.
void Pet::freeHome(uint8_t mode) {
  if (mode == HOME_EMPTY && (eggHeldFlag || items[IT_EGG])) {
    if (eggHeldFlag && items[IT_EGG] < ITEM_MAX) {
      items[IT_EGG]++;
      eggHeldFlag = false;
    }
    if (!eggHeldFlag) {
      vacate();
      return;
    }
    // mochila llena: el que esperaba no cabe y vuelve a casa (la UI no ofrece esa opcion)
  }
  if (eggHeldFlag) {
    newEgg();
  } else if (items[IT_EGG]) {
    items[IT_EGG]--;
    newEgg();
  } else if (!boxHasPets()) {
    queueFound(IT_EGG, false, true);  // "REGALO!" con el huevo, ya en casa
    newEgg(true, false);
  } else {
    vacate();
  }
}

bool Pet::boxHasPets() const {
  for (uint8_t k = 0; k < BOX_SLOTS; k++)
    if (boxSt[k] == BOX_OK) return true;
  return false;
}

bool Pet::hatchFromBag() {
  if (!vacant || !items[IT_EGG] || ceremony != CER_NONE) return false;
  eggToHome(true);
  return true;
}

void Pet::vacate(bool saveNow) {
  resetHome();
  vacant = true;
  starterPick = false;
  eggTaps = 0;
  persist(saveNow);
}

void Pet::eggToHome(bool saveNow) {
  if (!vacant || !items[IT_EGG]) return;
  items[IT_EGG]--;
  newEgg(saveNow);
}

void Pet::setBagEggs(uint8_t n) {
  items[IT_EGG] = n > ITEM_MAX ? ITEM_MAX : n;
  save();  // v1.40: con la casa vacia se quedan en la mochila (el boton de la casa los abre)
}

// sorteo shiny del huevo, 1 entre N: 1/48 base, mejor con despedida y con
// racha/vinculo altos, y x3 con el amuleto; nunca mejor que 1/8
int Pet::shinyOdds(bool charm) const {
  int n = (lastEnd == CER_FAREWELL ? 24 : 48) - careBonus();
  if (n < 8) n = 8;
  if (charm) {
    n /= CHARM_SHINY_MULT;
    if (n < 8) n = 8;
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
  uint32_t mins = (seen && nowEpoch > seen) ? (nowEpoch - seen) / 60 : 0;
  if (mins < 2 || ceremony != CER_NONE || starterPick || vacant) {
    save();  // primera vez, sin tiempo que aplicar, aun eligiendo inicial o sin nadie en casa
    return;
  }
  if (mins > 14UL * 24 * 60) mins = 14UL * 24 * 60;  // tope: 2 semanas

  for (uint32_t i = 0; i < mins; i++) {
    ageMinutes++;
    if (isEgg()) {
      if (ageMinutes >= 3) hatch();  // eclosiona en tu ausencia
      continue;
    }
    growMin++;  // crece igual apagado que en vivo, dormido o despierto
    if (sleeping) {  // descanso: baja lento y con suelo, igual que en vivo
      energy = clamp100(energy + 6);
      if (ageMinutes % 2 == 0) {
        fullness = dropTo(fullness, 1, 30);
        joy = dropTo(joy, 1, 35);
      }
      if (ageMinutes % 3 == 0) hygiene = dropTo(hygiene, 1, 45);
      continue;
    }
    fullness = dropTo(fullness, 2, 15);
    energy = dropTo(energy, 1, 15);
    hygiene = dropTo(hygiene, 1, 15);
    joy = dropTo(joy, 1, 15);
  }
  if (!isEgg()) {
    if (!sleeping) {  // durmiendo no ensucia
      uint8_t p = poops + mins / 240;
      poops = p > 3 ? 3 : p;
    }
    // la evolucion NO se aplica offline: queda lista y la dispara el usuario
    // tocando al bicho cuando vuelve (para que vea la transformacion)
  }
  Serial.printf("offline: %u min aplicados (nv.%u)\n", mins, level());
  save();
}

void Pet::update(uint32_t nowMs) {
  // fin de ceremonia: la criatura se va y llega el huevo que esperaba, uno de la mochila o nadie
  if (ceremony != CER_NONE && !timeLeft(ceremonyUntil)) {
    // v1.40: la despedida (el final bueno) deja un huevo de premio. Se da aqui y no al empezar: la
    // ceremonia no se guarda, y un corte durante ella devuelve al bicho con su boton de despedida.
    // Con la mochila llena y sin huevo esperando, cabe igual: freeHome() lo saca ahora mismo.
    if (ceremony == CER_FAREWELL) {
      bool full = items[IT_EGG] >= ITEM_MAX && eggHeldFlag;
      if (!full) items[IT_EGG]++;
      queueFound(IT_EGG, full, true);
    }
    uint8_t mode = endHome;
    endHome = HOME_AUTO;
    freeHome(mode);
    return;
  }
  while (nowMs - lastTick >= PET_TICK_MS) {
    lastTick += PET_TICK_MS;
    tick();
  }
}

void Pet::tick() {
  if (ceremony != CER_NONE) return;  // el tiempo se detiene en la despedida
  if (vacant) return;                // casa vacia: nadie que envejezca ni que eclosione
  if (starterPick) return;  // la partida no empieza hasta elegir inicial: si el
                            // tiempo corriera aqui, el huevo eclosionaria solo a
                            // los 3 min con la especie sorteada y se perderia la
                            // eleccion del jugador
  ageMinutes++;

  if (isEgg()) {
    if (ageMinutes >= 3) hatch();  // si no lo tocas, eclosiona solo a los 3 min
    return;
  }
  growMin++;  // el nivel sale de aqui (growth.h), despierto o dormido

  // el sueño es descanso: la energia se recupera y las necesidades bajan MUCHO
  // mas lento que despierto y con suelo (amanece pidiendo algo de mimo, no a
  // cero, sin descuidos ni escapadas). despierto: comida -2/min, hig/joy -1/min.
  // El peso aun se quema; la racha de buen cuidado (goodTicks) queda en pausa.
  if (sleeping) {
    energy = clamp100(energy + 6);
    if (weight > 0 && ageMinutes % WEIGHT_BURN_SLEEP_MIN == 0) weight--;
    if (ageMinutes % 2 == 0) {                 // ~4x mas lento que despierto
      fullness = dropTo(fullness, 1, 30);
      joy = dropTo(joy, 1, 35);
    }
    if (ageMinutes % 3 == 0) hygiene = dropTo(hygiene, 1, 45);
    checkMedals();  // aun puede cruzar un nivel por edad mientras duerme
    if (++ticksSinceSave >= 5) pendingSave = true;
    return;
  }

  // la subida de nivel ya no suena aqui: la detecta la UI (fx.h) por cualquier via (minuto,
  // tiempo apagado, Caramelo Raro, LVL) y la anuncia con su propio sonido

  fullness = clamp100(fullness - 2);
  energy = clamp100(energy - 1);
  if (fullness > 40 && poops < 3 && random(100) < 15) poops++;

  hygiene = clamp100(hygiene - 1 - 4 * poops);
  // el sobrepeso da pereza: la energia cae el doble
  if (weight > 50) energy = clamp100(energy - 1);
  if (weight > 0 && ageMinutes % WEIGHT_BURN_AWAKE_MIN == 0) weight--;

  // la disciplina forja la defensa: 12 h seguidas bien cuidado = +1 DEF
  if (lowestStat() >= 40) {
    if (++goodTicks >= 720) {
      goodTicks = 0;
      if (trDef < 100) trDef++;
    }
  } else {
    goodTicks = 0;
  }

  int dJoy = -1;
  if (fullness < 30) dJoy -= 2;
  if (hygiene < 30) dJoy -= 2;
  joy = clamp100(joy + dJoy);

  // Descuido: dejar una estadistica por los suelos cuenta como error de
  // cuidado (con enfriamiento para no contar el mismo descuido cada minuto)
  if (mistakeCooldown > 0) mistakeCooldown--;
  if (lowestStat() <= 10 && mistakeCooldown == 0) {
    careMistakes++;
    mistakeCooldown = 60;
    if (bond > 1) bond--;  // el descuido enfria el vinculo, pero sin arrasarlo:
                           // a -3 cada 30 min se perdia mucho mas de lo que se
                           // podia ganar en todo un dia y el vinculo se atascaba
  }

  checkMedals();  // la evolucion la dispara el usuario (canEvolveNow + tap), no el tick

  // abandono total: con TODO a cero durante una hora queda lista para escaparse;
  // NO se va sola, la dispara el usuario con el boton (final triste, lo presencia)
  if (fullness == 0 && joy == 0 && energy == 0 && hygiene == 0) {
    if (neglectTicks < RUNAWAY_TICKS) neglectTicks++;
  } else {
    neglectTicks = 0;  // un solo cuidado la salva
  }

  // ciclo completo (forma final + 7 dias): la despedida NO salta sola; queda
  // lista (canFarewellNow) y la dispara el usuario con el boton, para que la vea

  // autoguardado periodico: NO escribir a flash aqui (corre dentro del loop,
  // mientras se anima); solo marcar y dejar que el loop lo vuelque al atenuar
  if (++ticksSinceSave >= 5) pendingSave = true;
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

uint8_t Pet::eggRarity() const {
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
  if (lastEnd != CER_RUNAWAY) {
    bool blessed = (lastEnd == CER_FAREWELL);
    int rare = (blessed ? 45 : 27) + careBonus();
    int leg = (registeredCount() >= 25) ? (blessed ? 10 : 3) + careBonus() / 3 : 0;
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

void Pet::registerSpecies(int16_t dex) {
  if (dex < 1 || dex > 151) return;
  dexReg[(dex - 1) >> 3] |= (1 << ((dex - 1) & 7));
  if (shiny) dexShinyReg[(dex - 1) >> 3] |= (1 << ((dex - 1) & 7));
}

// la racha y el vinculo mejoran el sorteo del huevo (0..~14)
int Pet::careBonus() const {
  int s = streak > 30 ? 30 : streak;
  return s / 3 + bond / 25;
}

// primer cuidado del dia: avanza la racha y afianza el vinculo. saveNow=false
// para los eventos del IMU (la meta de pasos): marca el guardado y no escribe.
void Pet::registerCare(bool saveNow) {
  if (isEgg() || ceremony != CER_NONE) return;
  uint32_t d = today();
  if (d == 0 || d == lastCareDay) return;  // sin reloj, o ya conto hoy
  if (lastCareDay == 0 || d == lastCareDay + 1) {
    streak++;
  } else {
    streak = 1;        // hubo un hueco de dias
    lastMilestone = 0;
  }
  lastCareDay = d;
  if (bondDay != d) {  // el tope diario de vinculo es por dia (ver addBond)
    bondDay = d;
    bondToday = 0;
  }
  if (streak > bestStreak) bestStreak = streak;
  bond = clamp100(bond + BOND_CARE_DAY);  // fuera del tope diario de addBond
  uint16_t ms = (streak >= 100) ? 100 : (streak >= 30) ? 30
              : (streak >= 7)   ? 7   : (streak >= 3)  ? 3 : 0;
  if (ms > lastMilestone) {
    lastMilestone = ms;
    milestoneNew = streak;  // lo celebra la UI cuando se pueda ver
  }
  checkMedals(saveNow);
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

void Pet::checkMedals(bool saveNow) {
  if (isEgg()) return;
  uint16_t before = medals;
  medals |= pendingMedals;  // las que da un evento, no un estado (MARATON)
  pendingMedals = 0;
  if (level() >= 10) medals |= MED_LV10;
  if (level() >= 25) medals |= MED_LV25;
  if (level() >= 50) medals |= MED_LV50;
  if (berryKnown) medals |= MED_BERRY;
  if (streak >= 7) medals |= MED_STREAK7;
  if (bond >= 100) medals |= MED_BOND;
  if (DEX_TBL[speciesId].evolvesTo == 0) medals |= MED_FINAL;
  if (weight == 0 && level() >= 5 && careMistakes == 0) medals |= MED_FIT;
  uint16_t gained = medals & ~before;
  if (gained) {
    for (uint16_t m = gained; m; m &= (m - 1)) totalMedals++;
    newMedal |= gained;  // la UI la celebra (y suena) cuando se pueda ver: fx.h
    persist(saveNow);
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

uint16_t Pet::statAt(uint8_t which, uint16_t lv) const {
  if (isEgg()) return 0;
  const DexEntry &d = DEX_TBL[speciesId];
  if (which == 0) return calcStat(d.bAtk, geneAtk, lv, trAtk);
  if (which == 1) return calcStat(d.bDef, geneDef, lv, trDef);
  return calcStat(d.bSpe, geneSpe, lv, trSpe);
}

uint16_t Pet::atkStat() const {
  return isEgg() ? 0 : calcStat(DEX_TBL[speciesId].bAtk, geneAtk, level(), trAtk);
}
uint16_t Pet::defStat() const {
  return isEgg() ? 0 : calcStat(DEX_TBL[speciesId].bDef, geneDef, level(), trDef);
}
uint16_t Pet::speStat() const {
  return isEgg() ? 0 : calcStat(DEX_TBL[speciesId].bSpe, geneSpe, level(), trSpe);
}

uint16_t Pet::registeredCount() const {
  uint16_t n = 0;
  for (int i = 1; i <= 151; i++)
    if (isRegistered(i)) n++;
  return n;
}

// forma final que ya cumplio su ciclo (7 dias): lista para despedirse. La
// despedida la dispara el usuario con el boton (no salta sola, para que la vea)
bool Pet::canFarewellNow() const {
  return !isEgg() && !sleeping && ceremony == CER_NONE &&
         DEX_TBL[speciesId].evolvesTo == 0 && ageMinutes >= FAREWELL_AGE_MIN;
}

// abandono total durante 1h: lista para escaparse. La dispara el usuario con el
// boton (final triste); cuidarla un solo tick la salva (neglectTicks se resetea)
bool Pet::canRunawayNow() const {
  return !isEgg() && !sleeping && ceremony == CER_NONE && neglectTicks >= RUNAWAY_TICKS;
}

void Pet::startFarewell() {
  if (isEgg() || ceremony != CER_NONE) return;
  endHome = HOME_AUTO;
  lastEnd = CER_FAREWELL;
  ceremony = CER_FAREWELL;
  ceremonyUntil = millis() + CEREMONY_MS;
  heartUntil = ceremonyUntil;  // corazones durante toda la despedida
  sfxPlay(SFX_BYE);
  save();
}

void Pet::startRunaway() {
  if (isEgg() || ceremony != CER_NONE) return;
  endHome = HOME_AUTO;
  lastEnd = CER_RUNAWAY;
  ceremony = CER_RUNAWAY;
  ceremonyUntil = millis() + CEREMONY_MS;
  sfxPlay(SFX_BYE);
  save();
}

void Pet::release(uint8_t home) {
  if (isEgg() || ceremony != CER_NONE) return;
  endHome = home;
  lastEnd = CER_RELEASE;
  ceremony = CER_RELEASE;
  ceremonyUntil = millis() + CEREMONY_MS;
  heartUntil = ceremonyUntil;
  sfxPlay(SFX_BYE);
  save();
}

void Pet::hatch() {
  speciesId = eggTarget;
  shiny = eggShiny;
  // genes del individuo: 90-110% por stat (cada crianza es unica)
  geneAtk = 90 + random(21);
  geneDef = 90 + random(21);
  geneSpe = 90 + random(21);
  trAtk = trDef = trSpe = 0;
  berryKnown = false;
  bond = 0;          // vinculo, medallas y nombre son del individuo
  bondToday = 0;
  bondDay = today();
  medals = 0;
  newMedal = 0;
  nick[0] = 0;
  uid = newUid();    // identidad del individuo (caja, v1.37)
  registerSpecies(speciesId);  // criado = registrado en la pokedex
  checkMedals();     // por si nace ya en forma final (legendario)
  // sin sonido aqui: la UI ve la eclosion y suena en el destello de su animacion (fx.h), o
  // al momento si la pantalla no estaba a la vista
  save();
}

// ¿se dan ya las condiciones para evolucionar? Cada descuido retrasa la
// evolucion 1 nivel, y ademas tiene que estar bien cuidado en ese momento
// (ninguna estadistica por debajo de 40). NO evoluciona sola: la dispara el
// usuario tocando al bicho (evolve()), para que vea la transformacion.
bool Pet::canEvolveNow() const {
  if (isEgg() || sleeping || ceremony != CER_NONE) return false;
  const DexEntry &d = DEX_TBL[speciesId];
  if (d.evolvesTo == 0) return false;
  return level() >= (uint16_t)d.evolveLevel + careMistakes && lowestStat() >= 40;
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
  speciesId = next;
  registerSpecies(speciesId);
  sfxPlay(SFX_EVOLVE);
  evolveUntil = millis() + EVOLVE_ANIM_MS;
  save();
}

void Pet::feed() {
  feedBerry(0);
}

void Pet::feedBerry(uint8_t color) {
  if (ceremony != CER_NONE) return;
  if (isEgg() || sleeping) return;
  if (lovesBerry(color)) {
    fullness = clamp100(fullness + 35);
    joy = clamp100(joy + 10);
    heartUntil = millis() + HEART_MS;  // "le encanta!"
    berryKnown = true;                 // descubierto: se muestra en la ficha
    addBond(2);
  } else {
    fullness = clamp100(fullness + 25);
  }
  eatUntil = millis() + EAT_ANIM_MS;
  registerCare();
  save();
}

void Pet::feedCandy() {
  if (ceremony != CER_NONE) return;
  if (isEgg() || sleeping) return;
  fullness = clamp100(fullness + 10);
  joy = clamp100(joy + 12);
  weight = clamp100(weight + 12);  // las chuches pasan factura
  eatUntil = millis() + EAT_ANIM_MS;
  registerCare();
  save();
}

void Pet::playResult(uint8_t score) {
  if (ceremony != CER_NONE || isEgg()) return;
  uint8_t v = trSpe + score / 5;  // jugar entrena la velocidad
  trSpe = v > 100 ? 100 : v;
  joy = clamp100(joy + 5 + (score > 15 ? 30 : score * 2));
  energy = dropTo(energy, 10 + score / 2, 5);
  fullness = dropTo(fullness, 5, 5);
  int burn = (int)weight - score / GAME_WEIGHT_DIV;  // el ejercicio quema peso
  weight = burn > 0 ? burn : 0;
  if (score >= 5) heartUntil = millis() + HEART_MS;
  if (score > gameHi) gameHi = score;  // nuevo record
  addBond(2);
  registerCare();
  save();
}

// saco de entrenamiento: los golpes entrenan la fuerza. Devuelve la subida.
uint8_t Pet::trainStrength(uint16_t hits) {
  if (ceremony != CER_NONE || isEgg()) return 0;
  uint8_t gain = hits / 4;          // ~4 golpes = 1 punto de entrenamiento
  if (gain > 18) gain = 18;         // tope por sesion: la FUE se forja a fuego lento
  uint8_t antes = trAtk;
  uint16_t v = (uint16_t)trAtk + gain;
  trAtk = v > 100 ? 100 : (uint8_t)v;
  gain = trAtk - antes;             // lo que de verdad entro: al topar en 100 la
                                    // pantalla anunciaba +18 aunque cupieran menos
  energy = dropTo(energy, 12, 5);   // cansa
  fullness = dropTo(fullness, 5, 5);
  int burn = (int)weight - hits / TRAIN_WEIGHT_DIV;  // tambien quema peso
  weight = burn > 0 ? burn : 0;
  joy = clamp100(joy + 6);
  if (hits >= 20) heartUntil = millis() + HEART_MS;
  if (hits > strHi) strHi = hits;   // record de golpes
  addBond(2);
  registerCare();
  save();
  return gain;
}

void Pet::play() {
  if (ceremony != CER_NONE) return;
  if (isEgg() || sleeping) return;
  joy = clamp100(joy + 25);
  energy = clamp100(energy - 10);
  fullness = clamp100(fullness - 5);
  heartUntil = millis() + HEART_MS;
  addBond(2);
  registerCare();
  save();
}

void Pet::toggleLight() {
  if (ceremony != CER_NONE) return;
  if (isEgg()) return;
  sleeping = !sleeping;
  save();
}

void Pet::clean() {
  if (ceremony != CER_NONE) return;
  poops = 0;
  hygiene = 100;
  addBond(1);
  registerCare();
  save();
}

void Pet::caress() {
  if (ceremony != CER_NONE) return;
  if (isEgg() || sleeping) return;
  joy = clamp100(joy + 5);
  heartUntil = millis() + HEART_MS;
  // un toque en el bicho es gratis: sin enfriamiento, 10 toques llenaban el tope del dia
  if (!timeLeft(petBondUntil)) {
    addBond(1);
    petBondUntil = millis() + PET_BOND_COOLDOWN_MS;
  }
  registerCare();
}

void Pet::eggTap() {
  if (!isEgg() || vacant) return;
  if (++eggTaps >= 3) hatch();
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

  bool rewarded = !isEgg() && !sleeping && ceremony == CER_NONE && !starterPick;
  if (rewarded) {
    uint32_t j = joyRem + n;
    joyRem = j % STEPS_PER_JOY;
    uint32_t gain = j / STEPS_PER_JOY;
    uint32_t room = stepJoyToday < STEP_JOY_DAY_CAP ? STEP_JOY_DAY_CAP - stepJoyToday : 0;
    if (gain > room) gain = room;
    joy = clamp100(joy + (int)gain);
    stepJoyToday += gain;

    uint32_t w = weightRem + n;
    weightRem = w % STEPS_PER_WEIGHT;
    uint32_t burn = w / STEPS_PER_WEIGHT;
    weight = burn >= weight ? 0 : weight - burn;

    uint32_t b = bondRem + n;
    bondRem = b % STEPS_PER_BOND;
    for (uint32_t i = b / STEPS_PER_BOND; i > 0; i--) addBond(1);  // el tope diario lo pone addBond

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

  // huevos (v1.39): con todos los pasos, este como este el bicho, y sin tope diario. Con la casa
  // vacia, el huevo encontrado va derecho a casa (sin escribir aun: el loop guarda con la pantalla
  // atenuada). v1.40: solo el encontrado; los de la mochila esperan al boton de la casa vacia.
  if (!eggAt) eggAt = nextEggAt();
  uint32_t e = (uint32_t)eggRem + n;
  while (e >= eggAt) {
    e -= eggAt;
    eggAt = nextEggAt();
    if (vacant) {
      queueFound(IT_EGG, false, false);
      newEgg(false);
    } else {
      giveFound(IT_EGG);  // a la mochila (o "mochila llena") y al aviso de hallazgo
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
      goalReward(tier, rewarded);
    }
  }
}

// premio de cada meta. El banner y la racha (la primera) se dan siempre; lo demas, con el
// bicho nacido y despierto, como el resto de premios de caminar. Los regalos no cuentan
// para el tope de hallazgos al azar, y el vinculo va fuera del tope diario de addBond.
// v1.36: cada meta sube niveles (STEP_GOAL_LEVELS: +1, +1, +2), con la barra igual de llena.
void Pet::goalReward(uint8_t tier, bool rewarded) {
  if (rewarded) {
    uint16_t before = level();
    growMin = addLevels(growMin, STEP_GOAL_LEVELS[tier - 1]);
    uint16_t gained = level() - before;  // menos en el tope (999)
    goalBannerLv = (uint8_t)(goalBannerLv + gained);
    if (tier == 1) {
      joy = clamp100(joy + GOAL_JOY);
      bonusFindsToday++;
      findItem();
    } else if (tier == 2) {
      joy = clamp100(joy + GOAL2_JOY);
      bond = clamp100(bond + GOAL2_BOND);
      bonusFindsToday++;
      findItem();
    } else {  // 10000: +2 niveles (arriba) en vez del Caramelo Raro de la v1.25
      joy = clamp100(joy + GOAL3_JOY);
      bond = clamp100(bond + GOAL3_BOND);
      pendingMedals |= MED_MARATHON;
    }
    checkMedals(false);  // las de nivel (10, 25, 50) que abran los niveles nuevos, y MARATON
  }
  if (tier == 1) registerCare(false);  // cuenta como dia de cuidado para la racha
}

// el bicho encuentra algo por el camino (FR-1.5 / FR-1.6)
void Pet::findItem() {
  findsToday++;
  long r = random(100);
  uint8_t it;
  if (r < ODDS_BERRY) it = IT_GOLDEN;
  else if (r < ODDS_BERRY + ODDS_VITAMIN) it = IT_PROTEIN + random(3);
  else it = IT_CHARM;
  if (it == IT_CHARM && items[IT_CHARM]) it = IT_GOLDEN;  // solo se lleva un amuleto
  giveFound(it);
}

void Pet::giveFound(uint8_t it, bool gift) {
  uint8_t cap = (it == IT_CHARM) ? 1 : ITEM_MAX;
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

uint8_t Pet::itemCount() const {
  uint8_t n = 0;
  for (uint8_t i = 0; i < IT_COUNT; i++) n += items[i];
  return n;
}

bool Pet::canUseItem(uint8_t it) const {
  // el amuleto y el huevo no se usan a mano: uno se lleva, el otro va a casa cuando queda libre
  if (it >= IT_COUNT || it == IT_CHARM || it == IT_EGG || !items[it]) return false;
  if (isEgg() || sleeping || ceremony != CER_NONE) return false;
  if (it == IT_PROTEIN) return trAtk < 100;
  if (it == IT_IRON) return trDef < 100;
  if (it == IT_CARBOS) return trSpe < 100;
  if (it == IT_RARECANDY) return level() < 999;
  return true;
}

bool Pet::useItem(uint8_t it) {
  if (!canUseItem(it)) return false;
  items[it]--;
  if (it == IT_GOLDEN) {
    fullness = clamp100(fullness + GOLDEN_FOOD);
    joy = clamp100(joy + GOLDEN_JOY);
    eatUntil = millis() + EAT_ANIM_MS;
    heartUntil = millis() + HEART_MS;
  } else if (it == IT_RARECANDY) {
    // +1 nivel (v1.35): salta el crecimiento, no la edad, y la barra conserva lo que llevaba.
    // Puede abrir la evolucion y las medallas de nivel.
    growMin = addLevels(growMin, RARE_CANDY_LEVELS);
    heartUntil = millis() + HEART_MS;
    checkMedals(false);
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
  if (isEgg() || !sleeping || ceremony != CER_NONE) return;
  sleeping = false;
  uint32_t now = millis();
  if (!grumpied || now - lastGrumpy >= GRUMPY_COOLDOWN_MS) {
    joy = clamp100(joy - GRUMPY_JOY);
    grumpied = true;
    lastGrumpy = now;
  }
  startEmote(EMOTE_ANGRY);
  sfxPlay(SFX_DENY);
  pendingSave = true;
}

// agitarlo despierto: se marea. 3 sacudidas en 2 min restan felicidad, como
// mucho una vez cada 10 min. El huevo solo se tambalea.
void Pet::shaken() {
  if (ceremony != CER_NONE) return;
  if (isEgg()) {
    if (!vacant) startEmote(EMOTE_WOBBLE);
    return;
  }
  if (sleeping) return;  // dormido es wakeGrumpy()
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

// ---------- combate de practica (docs/FEATURES.md, Tarea 3a) ----------

bool Pet::practiceRewardReady() const {
  return !practiceRewarded || millis() - lastPracticeReward >= PRACTICE_EVERY_MS;
}

uint8_t Pet::practiceDone() {
  if (isEgg() || ceremony != CER_NONE || !practiceRewardReady()) return 0;
  practiceRewarded = true;
  lastPracticeReward = millis();
  uint8_t before = joy;
  joy = clamp100(joy + PRACTICE_JOY);
  save();  // fin de una pantalla de juego, como el minijuego: se guarda ya
  return joy - before;
}

// boca abajo de noche: a dormir, como el boton de luz pero sin escribir ya en flash
void Pet::sleepDeferred() {
  if (ceremony != CER_NONE || isEgg() || sleeping) return;
  sleeping = true;
  pendingSave = true;
}

PetMood Pet::mood() const {
  if (sleeping) return MOOD_SLEEPING;
  if (eating()) return MOOD_EATING;
  if (lowestStat() < 25) return MOOD_SAD;
  return MOOD_HAPPY;
}

void Pet::save() {
  ticksSinceSave = 0;
  pendingSave = false;
  prefs.putUChar("full", fullness);
  prefs.putUChar("joy", joy);
  prefs.putUChar("ene", energy);
  prefs.putUChar("hyg", hygiene);
  prefs.putUChar("poop", poops);
  prefs.putUChar("wgt", weight);
  prefs.putUChar("gatk", geneAtk);
  prefs.putUChar("gdef", geneDef);
  prefs.putUChar("gspe", geneSpe);
  prefs.putUChar("tatk", trAtk);
  prefs.putUChar("tdef", trDef);
  prefs.putUChar("tspe", trSpe);
  prefs.putBool("bk", berryKnown);
  prefs.putBool("shy", shiny);
  prefs.putBool("eshy", eggShiny);
  prefs.putBool("stpk", starterPick);
  prefs.putBytes("dexsh", dexShinyReg, sizeof(dexShinyReg));
  prefs.putUInt("age", ageMinutes);
  prefs.putUInt("grow", growMin);
  prefs.putShort("dexn", speciesId);
  prefs.putShort("eggT2", eggTarget);
  prefs.putUChar("crack", eggTaps);
  prefs.putUChar("mist", careMistakes);
  prefs.putBool("sleep", sleeping);
  prefs.putUChar("lend", lastEnd);
  if (lastSeenEpoch) prefs.putUInt("seen", lastSeenEpoch);
  prefs.putBytes("dexreg", dexReg, sizeof(dexReg));
  prefs.putUShort("strk", streak);
  prefs.putUShort("bstrk", bestStreak);
  prefs.putUInt("cday", lastCareDay);
  prefs.putUChar("bond", bond);
  prefs.putUShort("medal", medals);
  prefs.putUShort("tmedal", totalMedals);
  prefs.putUShort("mstone", lastMilestone);
  prefs.putUShort("ghi", gameHi);
  prefs.putUShort("shi", strHi);
  prefs.putString("nick", nick);
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
  prefs.putUInt("bondD", bondDay);
  prefs.putUChar("bondT", bondToday);
  prefs.putBool("vac", vacant);
  // caja (v1.37). El uid va el ULTIMO: la NVS escribe en orden, asi que si un corte de luz
  // deja el uid nuevo, todo lo demas del bicho ya esta escrito. boxBoot() se fia de eso para
  // saber si un movimiento llego a cambiar el bicho activo. "eggH" va despues: un corte entre
  // los dos deja un huevo activo con eggH puesto, y boxBoot() lo quita.
  // La mochila va justo despues del uid (v1.39): guardar en la caja puede sacar un huevo de ella, y
  // un corte antes del uid devuelve al bicho desde su hueco con la mochila sin tocar. Entre el uid y
  // la mochila un corte deja el huevo en casa y el de la mochila aun contado: como mucho uno de mas,
  // nunca uno perdido.
  prefs.putUInt("uid", uid);
  prefs.putBytes("bpack", items, sizeof(items));
  prefs.putBool("eggH", eggHeldFlag);
}

void Pet::load() {
  fullness = prefs.getUChar("full", 80);
  joy = prefs.getUChar("joy", 80);
  energy = prefs.getUChar("ene", 80);
  hygiene = prefs.getUChar("hyg", 100);
  poops = prefs.getUChar("poop", 0);
  weight = prefs.getUChar("wgt", 0);
  geneAtk = prefs.getUChar("gatk", 0);
  geneDef = prefs.getUChar("gdef", 0);
  geneSpe = prefs.getUChar("gspe", 0);
  if (geneAtk == 0) {  // mascota anterior a los genes: tirada unica ahora
    geneAtk = 90 + random(21);
    geneDef = 90 + random(21);
    geneSpe = 90 + random(21);
  }
  trAtk = prefs.getUChar("tatk", 0);
  trDef = prefs.getUChar("tdef", 0);
  trSpe = prefs.getUChar("tspe", 0);
  berryKnown = prefs.getBool("bk", false);
  shiny = prefs.getBool("shy", false);
  eggShiny = prefs.getBool("eshy", false);
  starterPick = prefs.getBool("stpk", false);
  prefs.getBytes("dexsh", dexShinyReg, sizeof(dexShinyReg));
  ageMinutes = prefs.getUInt("age", 0);
  if (prefs.isKey("dexn")) {
    speciesId = prefs.getShort("dexn", -1);
    eggTarget = prefs.getShort("eggT2", 4);
  } else {
    // migracion desde la version con indices de flash (0-8)
    static const uint8_t OLD2DEX[9] = { 4, 5, 6, 1, 2, 3, 7, 8, 9 };
    int8_t old = prefs.getChar("spec", -1);
    speciesId = (old >= 0 && old < 9) ? OLD2DEX[old] : -1;
    int8_t oldT = prefs.getChar("eggT", 0);
    eggTarget = (oldT >= 0 && oldT < 9) ? OLD2DEX[oldT] : 4;
  }
  eggTaps = prefs.getUChar("crack", 0);
  careMistakes = prefs.getUChar("mist", 0);
  sleeping = prefs.getBool("sleep", false);
  lastEnd = prefs.getUChar("lend", CER_NONE);
  prefs.getBytes("dexreg", dexReg, sizeof(dexReg));
  streak = prefs.getUShort("strk", 0);
  bestStreak = prefs.getUShort("bstrk", 0);
  lastCareDay = prefs.getUInt("cday", 0);
  bond = prefs.getUChar("bond", 0);
  medals = prefs.getUShort("medal", 0);
  totalMedals = prefs.getUShort("tmedal", 0);
  lastMilestone = prefs.getUShort("mstone", 0);
  gameHi = prefs.getUShort("ghi", 0);
  strHi = prefs.getUShort("shi", 0);
  prefs.getString("nick", nick, sizeof(nick));
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
  prefs.getBytes("bpack", items, sizeof(items));
  for (uint8_t i = 0; i < IT_COUNT; i++) {
    uint8_t cap = (i == IT_CHARM) ? 1 : ITEM_MAX;
    if (items[i] > cap) items[i] = cap;
  }
  bondDay = prefs.getUInt("bondD", 0);
  bondToday = prefs.getUChar("bondT", 0);
  // crecimiento (v1.35). Un guardado anterior no trae "grow": el nivel salia de la edad
  // (1 nivel por hora). Se traduce a la curva nueva con el mismo nivel y la barra igual de
  // llena, para que nadie suba ni baje de nivel al actualizar. Va despues de speciesId.
  if (prefs.isKey("grow")) growMin = prefs.getUInt("grow", 0);
  else growMin = isEgg() ? 0 : growthFromLegacyAge(ageMinutes);
  // siembra: la mascota actual cuenta como criada (guardados antiguos)
  if (speciesId >= 1) registerSpecies(speciesId);
  // caja (v1.37). Sin la clave "uid" (guardado anterior) queda a 0 y begin() le da una despues
  // de boxBoot(); un 0 guardado es un huevo, y boxBoot() lo necesita tal cual
  uid = prefs.getUInt("uid", 0);
  eggHeldFlag = prefs.getBool("eggH", false);
  // casa vacia (v1.39): solo sin bicho nacido (un corte a mitad de un despertar puede dejar "vac"
  // a medias; boxBoot decide por "boxT" y "eggH")
  vacant = prefs.getBool("vac", false) && speciesId < 0 && !uid;
}

// ---------- la caja (docs/NEXT_FEATURES.md, Tarea BOX, v1.37) ----------
//
// Cada movimiento escribe primero la copia entera del bicho que se mueve y borra la de sobra al
// final, y el uid es lo ultimo que escribe save(). Asi, corte la luz donde corte, el siguiente
// arranque (boxBoot) encuentra al bicho entero en algun sitio y, como mucho, repetido:
//   guardar A en k:        1. box k = A                2. huevo activo, save() (uid 0 al final)
//   despertar B con A:     1. boxT = B   2. box k = A   3. B activo, save()   4. borrar boxT
//   despertar B con huevo: 1. boxT = B   2. eggH        3. B activo, save()   4. vaciar k   5. borrar boxT

static_assert(sizeof(((PetRecord *)0)->nick) == sizeof(((Pet *)0)->nick), "el apodo cabe entero en el registro");

static void slotKey(char *key, uint8_t k) {
  key[0] = 'b';
  key[1] = 'o';
  key[2] = 'x';
  key[3] = (char)('0' + k);
  key[4] = 0;
}

PetRecord Pet::toRecord() const {
  PetRecord r;
  memset(&r, 0, sizeof(r));
  r.flags = (shiny ? REC_SHINY : 0) | (berryKnown ? REC_BERRY : 0) | (sleeping ? REC_SLEEP : 0);
  r.uid = uid;
  r.speciesId = speciesId;
  memcpy(r.nick, nick, sizeof(r.nick));
  r.nick[sizeof(r.nick) - 1] = 0;
  r.ageMinutes = ageMinutes;
  r.growMin = growMin;
  r.fullness = fullness;
  r.joy = joy;
  r.energy = energy;
  r.hygiene = hygiene;
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
  r.medals = medals;
  r.storedEpoch = lastSeenEpoch;
  recordSeal(r);
  return r;
}

// Pasa a ser el bicho de r. Solo cambia lo del individuo: racha, pasos, mochila, records,
// Pokedex y la incubadora son del jugador y se quedan. Lo que solo vive en RAM (enfriamientos,
// "mantener forma", animaciones) empieza de cero, como tras un reinicio.
void Pet::fromRecord(const PetRecord &r) {
  vacant = false;
  uid = r.uid;
  speciesId = r.speciesId;
  prevSpeciesId = r.speciesId;
  shiny = (r.flags & REC_SHINY) != 0;
  berryKnown = (r.flags & REC_BERRY) != 0;
  sleeping = (r.flags & REC_SLEEP) != 0;
  memcpy(nick, r.nick, sizeof(nick));
  nick[sizeof(nick) - 1] = 0;
  ageMinutes = r.ageMinutes;
  growMin = r.growMin;
  fullness = r.fullness;
  joy = r.joy;
  energy = r.energy;
  hygiene = r.hygiene;
  poops = r.poops > 3 ? 3 : r.poops;
  weight = r.weight;
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
  medals = r.medals;
  ceremony = CER_NONE;
  mistakeCooldown = 0;
  neglectTicks = 0;
  goodTicks = 0;
  petBondUntil = 0;
  evoDeclinedLv = 0;
  farDeclinedAge = 0;
  eatUntil = heartUntil = evolveUntil = 0;
  emoteUntil = 0;
  newMedal = 0;
  pendingMedals = 0;
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
    if (id && id != uid && slotOf(id) < 0) return id;
  }
}

uint8_t Pet::boxUsed() const {
  uint8_t n = 0;
  for (uint8_t k = 0; k < BOX_SLOTS; k++)
    if (boxSt[k] != BOX_EMPTY) n++;
  return n;
}

bool Pet::store(uint8_t k) {
  if (!canStore(k)) return false;
  if (!writeSlot(k, toRecord())) return false;  // 1. la copia entera, a su hueco
  freeHome();  // 2. el huevo que esperaba, uno de la mochila o nadie; save() deja el uid (0) casi al final
  return true;
}

bool Pet::wake(uint8_t k) {
  if (!canWake(k)) return false;
  PetRecord b = boxRec[k];
  bool egg = isEgg() && !vacant;
  // 1. con un huevo, el huevo espera en la incubadora, tal cual. Va ANTES que boxT (v1.39): asi un
  // despertar cortado sin el uid de B se sabe si venia de un huevo (eggH) o de la casa vacia
  if (egg) {
    eggHeldFlag = true;
    prefs.putBool("eggH", true);
  }
  // 2. la copia de transferencia: mientras exista, el arranque sabe que hay un despertar a medias
  if (prefs.putBytes("boxT", &b, sizeof(b)) != sizeof(b)) {
    if (egg) {
      eggHeldFlag = false;
      prefs.putBool("eggH", false);
    }
    return false;
  }
  if (isEgg()) {  // un huevo (que espera) o la casa vacia: B pasa a casa y su hueco queda libre
    fromRecord(b);  // 3. B activo (save() escribe su uid casi al final)
    save();
    if (!clearSlot(k)) return true;  // 4. su hueco, libre (si falla, boxT se queda y lo hace el arranque)
  } else {
    if (!writeSlot(k, toRecord())) {  // 2. el activo, al hueco de B
      prefs.remove("boxT");
      return false;
    }
    fromRecord(b);  // 3.
    save();
  }
  prefs.remove("boxT");  // hecho
  return true;
}

bool Pet::boxClear(uint8_t k) {
  if (k >= BOX_SLOTS) return false;
  return boxSt[k] == BOX_EMPTY || clearSlot(k);
}

bool Pet::boxPut(uint8_t k, PetRecord r) {
  if (k >= BOX_SLOTS || boxSt[k] != BOX_EMPTY || r.speciesId < 1 || r.speciesId > 151) return false;
  r.uid = newUid();
  return writeSlot(k, r);
}

// Al arrancar, antes que nada: lee los huecos y, si un corte de luz pillo un movimiento a medias,
// lo termina o lo deshace. El uid guardado del activo dice hasta donde llego save() (va el ultimo).
void Pet::boxBoot() {
  for (uint8_t k = 0; k < BOX_SLOTS; k++) readSlot(k);
  if (prefs.isKey("boxT")) {
    PetRecord t;
    memset(&t, 0, sizeof(t));
    bool ok = prefs.getBytes("boxT", &t, sizeof(t)) == sizeof(t) && recordValid(t);
    if (ok && uid == t.uid) {
      // el despertar llego a guardar a B: solo puede quedar su copia en el hueco (si habia huevo)
      for (uint8_t k = 0; k < BOX_SLOTS; k++)
        if (boxSt[k] == BOX_OK && boxRec[k].uid == uid) clearSlot(k);
    } else if (ok && uid && slotOf(uid) >= 0) {
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
    prefs.remove("boxT");
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
  // Con la casa vacia no deberia pasar nunca; si pasa, el huevo que esperaba vuelve a casa
  if (isEgg() && eggHeldFlag) {
    if (vacant) {
      newEgg();
    } else {
      eggHeldFlag = false;
      prefs.putBool("eggH", false);
    }
  }
}
