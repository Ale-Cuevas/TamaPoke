#include "i18n.h"
#include "medals.h"     // MEDAL_COUNT (v3.4)
#include <Preferences.h>

Lang gLang = LANG_DEFAULT;

// Tabla de cadenas [idioma][id]. Sin acentos ni enes: la fuente bitmap del
// firmware no los tiene (por eso el espanol ya iba "Esta", "bano", etc.).
static const char *const STRINGS[LANG_COUNT][STR_COUNT] = {
  // ---------------- ES ----------------
  {
    "Nam nam!", "Le gusta!", "Tiene hambre!", "Necesita un ba\244o!",
    "Est\240 agotado...", "Est\240 triste...",
    "GRACIAS! Hasta siempre", "Ha muerto...", "Adi\242s! Se despide...",
    "HUEVO", "Huevo legendario!?", "Huevo raro!", "Toca el huevo...", "Se mueve!", "Est\240 a punto!",
    "POKEDEX %u/151",
    "%s%s Nv.%u",
    "Soltar a %s?", "SI", "NO",
    "%u GOLPES", "FUERZA +%u", "NUEVO RECORD!", "RECORD: %u", "APORREA RAPIDO!",
    "PUNTOS: %u", "Qu\202 felicidad!", "+felicidad",
    "AJUSTAR HORA", "HORA", "MIN", "desliza arriba: cancelar", "Idioma",
    "MEDALLA!", "GENIAL!", "RACHA %u DIAS!",
    "RACHA %u  rec %u", "VIN", "BAYA ???",
    "%s   EDAD %lud", "toca el nombre: renombrar",
    "COMBATE", "FUE", "DEF", "VEL", "PES",
    "MEDALLAS %d/%d", "toca: volver",
    "NOMBRE:", "toca para volver",
    "COM", "FEL", "ENE", "LIM",
    "REC %u",
    "PROGRESO", "Nv.%u", "%u min para Nv.%u", "EVOLUCION", "Forma final",
    "Listo para evolucionar!", "Sube todo a 40 para evolucionar",
    "Evoluciona en %u niv.", "Descuidos: %u",
    "EVOLUCIONAR", "%s quiere decirte algo...", "Se siente abandonado...",
    "Evolucionar?", "Mantener forma", "Despedirse?", "Despedirse", "Quedaros juntos",
    "Elige tu inicial",
    "Sin sprites", "C\240rgalos en la SD",
    // pasos y mochila (v1.20)
    "PASEO", "%lu PASOS", "TOTAL %lu", "HALLAZGOS %u/%u", "MOCHILA", "Sin sensor de pasos",
    "META!", "HALLAZGO!", "Mochila llena", "Usar %s?", "Amuleto: huevo shiny", "Vac\241a: sal a pasear!",
    "Est\240 enfadado!", "Qu\202 mareo...",
    "BAYA DORADA", "PROTEINA", "HIERRO", "CARBURANTE", "AMULETO IRIS", "CARAMELO RARO", "HUEVO", "TARTA", "REVIVIR", "POK\220 BALL",
    "BAYA ROJA", "BAYA AZUL", "BAYA VERDE", "BAYA AMARILLA", "BAYA ROSA", "BAYA NARANJA",  // v3.2
    "CEBO",  // v3.14: el Cebo
    // combate (v1.24)
    "ENTRENAMIENTO", "Es muy eficaz!", "No es muy eficaz...", "No le afecta...", "Golpe cr\241tico!", "Especial!",
    "Toca para cargar!", "Muerte s\243bita!", "Retroceso!", "GANASTE!", "PERDISTE...", "EMPATE", "Felicidad +%u", "Premio: una vez por hora",
    // efectos (v1.30)
    "SUBE DE NIVEL!",
    // la caja (v1.37)
    "CAJA %u/%u", "Guardar a %s?", "Despertar a %s?", "%s va a la caja", "Ahora: %s", "Vac\241o", "Da\244ado",
    // combate (v1.38)
    "Toca!",
    // huevos limitados (v1.39)
    "SIN HUEVO", "Camina para encontrar uno!", "Desliza a la derecha: caja", "Sin huevo: casa vac\241a", "El huevo nace con la casa libre",
    // huevos al acabar una crianza (v1.40/v1.41)
    "Abrir un huevo (%u)", "Llegar\240 un huevo nuevo", "Tus huevos esperan!", "REGALO!",
    // la cascada (v1.43)
    "Manten la barra en el verde!", "MANTEN PULSADO", "FOCO %u.%us", "RECORD %u.%us", "DEFENSA +%u", "Sin cacas!",
    // la carrera en trineo (v1.44)
    "Sujetala como vas a jugar", "Quieta un momento...", "INCLINA PARA GIRAR", "ESQUIVADOS: %u", "VELOCIDAD +%u",
    // v1.47: bonificacion de la cascada, entrenamiento al tope
    "EXTRA +%u", "FUERZA MAX", "DEFENSA MAX", "VELOCIDAD MAX",
    // v1.49: la barra fina de la ficha de combate
    "barra fina: entreno",
    // v1.53: la mochila con su boton: como se usa y dormido solo se mira
    "Toca un objeto para usarlo", "Duerme: solo mirar",
    // v1.54: la tarta: lleno durante 3 h, y tocarla sin ninguna
    "Sin tarta: camina o combate",
    // v1.56: el botin de un combate ganado, que espera envuelto en casa
    "Regalo en casa!",
    // v1.57: al evolucionar cambio la baya favorita (vuelve a ser un secreto)
    "Sus gustos cambiaron!",
    // v1.59: la pista del Revivir en la mochila (no se usa a mano)
    "Salva a un bicho moribundo",
    // v1.60: muerte por abandono: sin vinculo, el dialogo, el huevo que deja, revivido
    "Est\240 muy d\202bil!", "%s se muere...", "Usar un Revivir? (%u)", "No te quedan Revivir", "VALE", "Dej\242 un huevo...", "Revivido!",
    // v1.63: tiene sueño
    "Tiene sue\244o...",
    // v1.64: la hora del Revivir
    "Quedan %u min", "%s ha muerto...", "No lleg\242 el Revivir",
    // stages
    "Fases", "Insignias", "Elige tu equipo", "Empezar", "JEFE", "Bloqueada", "Fase superada!", "Derrotado...",
    "Siguiente rival", "Adelante, %s!", "Nueva insignia!", "Nv.%u-%u", "Equipo: +1 h, +v\241nculo",
    "Premio disponible", "Fase %u",
    // el patio (v2.0)
    "TIENDA", "Comprar", "Usar", "Comprado!", "Cuidar", "Jugar", "Mochila",
    // el huevo del patio (v2.0, Y3)
    "Agitame!",
    // la tienda de la mochila y los fondos (v2.0, Y4)
    "OBJETOS", "Fondos", "Comprar %s?", "Faltan monedas", "Mochila llena",
    "Pradera", "Cantera", "Bosque", "Playa", "Ruinas",
    "Volc\240n", "Central el\202ctrica", "Torre del cielo", "Distrito t\242xico",
    "En uso", "No esta en la SD", "Toca para comprar",
    // los viajes del patio (v2.0, Y5; v3.15 9 mapas): 45 recuerdos (5 por mapa; el 5.o es el raro) y 17 sucesos del camino
    "Tr\202bol de cuatro hojas", "Corona de margaritas", "Pluma", "Tarro de miel", "Bellota dorada",
    "Guijarro liso", "Trozo de cuarzo", "Concha f\242sil", "Mineral de hierro", "Piedra lunar",
    "Pi\244a", "Seta", "Pluma de b\243ho", "Gota de \240mbar", "Hoja ancestral",
    "Caracola", "Estrella de mar", "Vidrio marino", "Botella con mensaje", "Perla",
    "Moneda antigua", "M\240scara de barro", "Cabo de vela", "Tablilla de runas", "Farol espiritual",
    "Brasa", "Obsidiana", "Piedra de magma", "Pluma de fuego", "Ascua de f\202nix",
    "Pila", "Im\240n", "Bobina de cobre", "Piedra trueno", "Orbe de plasma",
    "Pluma de vendaval", "Copo de nube", "Campanilla de viento", "Perla del cielo", "Trozo de estrella",
    "Engranaje", "Frasco de lodo", "M\240scara de gas", "Cristal t\242xico", "Corona oxidada",
    "Conoci\242 a un %s salvaje", "Durmi\242 la siesta bajo un \240rbol", "Encontr\242 algo brillante", "Chapote\242 en el agua", "Escal\242 una roca grande",
    "Oy\242 un ruido extra\244o", "Persigui\242 una mariposa", "Cav\242 un hoyo peque\244o", "Mir\242 las estrellas", "Comparti\242 bayas con un %s salvaje",
    "Hizo un castillo de arena", "Explor\242 un pasillo oscuro", "Par\242 a merendar",
    "Se calent\242 junto a la lava", "Sinti\242 una chispa", "Vol\242 con una r\240faga de viento", "Contuvo el aliento en el humo",
    // viajes: pantallas (v2.0, Y5b)
    "Viaje", "De viaje", "De camino...", "\255%s ha vuelto!", "fuera", "Ir", "Mochila llena", "NUEVO", "Hallado el %s", "Colecci\242n", "Guarda un bicho en la Caja\npara mandarlo de viaje", "Pon la hora para viajar", "Com\243n", "Poco com\243n", "Raro", "%s est\240 de viaje",
    // encuentros salvajes (v2.2)
    "Para un salvaje vencido", "\255Un %s salvaje!", "Toca para luchar", "Elige luchador", "Salvaje", "\255Ganaste!", "Se escap\242...", "+1 h de crecimiento", "\250Lanzar una Pok\202 Ball?", "Te quedan %u", "Lanzar", "\255%s capturado!", "Su huevo, en la mochila", "%s se march\242", "\255%s huy\242!", "No te quedan Pok\202 Balls", "No caben m\240s huevos", "\255Huevo capturado!",
    // dos bichos en casa (v3.0): la caja pregunta cual
    "\250Cu\240l va a la caja?", "\250Con cu\240l lo cambias?",
    // el menu del bicho, la lista de comida y el selector (v3.3)
    "Dormir", "Despertar", "Comida", "Estado", "Evoluci\242n", "Despedida", "Volver",
    "Baya", "Roja", "Azul", "Verde", "Amarilla", "Rosa", "Naranja", "Tarta", "Dorada",
    "\250Qui\202n juega?", "\250Para qui\202n?",
    // las medallas del jugador (v3.4): los tres tramos, y la 2.a pagina de la ficha
    "Bronce", "Plata", "Oro", "ESTADISTICAS",
    // v3.14: el Cebo
    "Hace salir a un salvaje", "Ya hay un salvaje en el patio", "Ahora nadie puede pelear",
    "Toca la bola!",
    // v3.20 HOME: la pagina Casa de la tienda (titulo, 3 lineas, 9 niveles, 3 efectos, MAX, comprar)
    "Casa", "Camas", "Gimnasio", "Juguete",
    "Cama de paja", "Cama de cojin", "Cama real", "Pesas de tronco", "Mancuernas", "Gimnasio entero", "Pelota de goma", "Cuerda", "Columpio",
    "Duerme: ENE +%u/min", "Entrena hasta %u", "Juega: Alegria +%u", "MAX", "Comprar %s por %u?",
  },
  // ---------------- EN ----------------
  {
    "Yum yum!", "It likes it!", "It's hungry!", "Needs a bath!",
    "Worn out...", "Feeling sad...",
    "THANKS! Farewell", "It has passed away...", "Bye! Waving goodbye...",
    "EGG", "Legendary egg!?", "Rare egg!", "Tap the egg...", "It moves!", "Almost there!",
    "POKEDEX %u/151",
    "%s%s Lv.%u",
    "Release %s?", "YES", "NO",
    "%u HITS", "STR +%u", "NEW RECORD!", "BEST: %u", "HIT FAST!",
    "SCORE: %u", "So much fun!", "+happiness",
    "SET TIME", "HOUR", "MIN", "swipe up: cancel", "Lang",
    "MEDAL!", "AWESOME!", "%u DAY STREAK!",
    "STREAK %u  best %u", "BOND", "BERRY ???",
    "%s   AGE %lud", "tap name: rename",
    "BATTLE", "ATK", "DEF", "SPD", "WGT",
    "MEDALS %d/%d", "tap: back",
    "NAME:", "tap to go back",
    "FOOD", "JOY", "ENE", "HYG",
    "BEST %u",
    "PROGRESS", "Lv.%u", "%u min to Lv.%u", "EVOLUTION", "Final form",
    "Ready to evolve!", "All needs >=40 to evolve",
    "Evolves in %u lv.", "Slip-ups: %u",
    "EVOLVE!", "%s wants to tell you...", "It feels abandoned...",
    "Evolve?", "Keep form", "Say goodbye?", "Goodbye", "Stay together",
    "Choose your starter",
    "No sprites", "Load them onto the SD",
    // pasos y mochila (v1.20)
    "WALK", "%lu STEPS", "TOTAL %lu", "FINDS %u/%u", "BACKPACK", "No step sensor",
    "GOAL!", "FOUND!", "Backpack full", "Use %s?", "Charm: shinier egg", "Empty: go for a walk!",
    "Grumpy!", "So dizzy...",
    "GOLDEN BERRY", "PROTEIN", "IRON", "CARBOS", "SHINY CHARM", "RARE CANDY", "EGG", "CAKE", "REVIVE", "POK\220 BALL",
    "RED BERRY", "BLUE BERRY", "GREEN BERRY", "YELLOW BERRY", "PINK BERRY", "ORANGE BERRY",  // v3.2
    "LURE",  // v3.14: el Cebo
    // combate (v1.24)
    "PRACTICE", "Super effective!", "Not very effective...", "No effect...", "Critical hit!", "Special!",
    "Tap to charge!", "Sudden death!", "Recoil!", "YOU WIN!", "YOU LOST...", "DRAW", "Joy +%u", "Reward: once an hour",
    // efectos (v1.30)
    "LEVEL UP!",
    // la caja (v1.37)
    "BOX %u/%u", "Store %s?", "Wake %s?", "%s goes in the box", "Now: %s", "Empty", "Damaged",
    // combate (v1.38)
    "Tap!",
    // huevos limitados (v1.39)
    "NO EGG", "Walk to find an egg!", "Swipe right: the Box", "No egg: home stays empty", "Eggs hatch when home is free",
    // huevos al acabar una crianza (v1.40/v1.41)
    "Hatch an egg (%u)", "A new egg will come", "Your eggs are waiting!", "GIFT!",
    // la cascada (v1.43)
    "Keep the bar in the green!", "HOLD TO FOCUS", "FOCUS %u.%us", "BEST %u.%us", "DEF +%u", "No more poop!",
    // la carrera en trineo (v1.44)
    "Hold it the way you'll play", "Hold still...", "TILT TO STEER", "DODGED: %u", "SPD +%u",
    // v1.47: bonificacion de la cascada, entrenamiento al tope
    "BONUS +%u", "STR MAX", "DEF MAX", "SPD MAX",
    // v1.49: la barra fina de la ficha de combate
    "thin bar: training",
    // v1.53: la mochila con su boton: como se usa y dormido solo se mira
    "Tap an item to use it", "Asleep: just looking",
    // v1.54: la tarta: lleno durante 3 h, y tocarla sin ninguna
    "No cake: walk or battle",
    // v1.56: el botin de un combate ganado, que espera envuelto en casa
    "A gift at home!",
    // v1.57: al evolucionar cambio la baya favorita (vuelve a ser un secreto)
    "Its tastes changed!",
    // v1.59: la pista del Revivir en la mochila (no se usa a mano)
    "Saves a dying pet",
    // v1.60: muerte por abandono: sin vinculo, el dialogo, el huevo que deja, revivido
    "It's very weak!", "%s is dying...", "Use a Revive? (%u)", "No Revives left", "OK", "It left an egg...", "Revived!",
    // v1.63: tiene sueño
    "Getting sleepy...",
    // v1.64: la hora del Revivir
    "%u min left", "%s has died...", "No Revive in time",
    // stages
    "Stages", "Badges", "Pick your team", "Start", "BOSS", "Locked", "Stage clear!", "Defeated...",
    "Next rival", "Go, %s!", "New badge!", "Lv.%u-%u", "Team: +1 h, +bond",
    "Reward ready", "Stage %u",
    // el patio (v2.0)
    "SHOP", "Buy", "Use", "Purchased!", "Care", "Play", "Bag",
    // el huevo del patio (v2.0, Y3)
    "Shake me!",
    // la tienda de la mochila y los fondos (v2.0, Y4)
    "ITEMS", "Backgrounds", "Buy %s?", "Not enough coins", "Backpack full",
    "Meadow", "Quarry", "Forest", "Beach", "Ruins",
    "Volcano", "Power Plant", "Sky Tower", "Toxic District",
    "In use", "Not on SD card", "Tap to buy",
    // los viajes del patio (v2.0, Y5; v3.15 9 mapas): 45 recuerdos (5 por mapa; el 5.o es el raro) y 17 sucesos del camino
    "Four-leaf clover", "Daisy crown", "Feather", "Honey jar", "Golden acorn",
    "Smooth pebble", "Quartz shard", "Fossil shell", "Iron ore", "Moon stone",
    "Pine cone", "Mushroom", "Owl feather", "Amber drop", "Ancient leaf",
    "Seashell", "Starfish", "Sea glass", "Message bottle", "Pearl",
    "Old coin", "Clay mask", "Candle stub", "Rune tablet", "Spirit lantern",
    "Ember", "Obsidian", "Magma stone", "Fire feather", "Phoenix ember",
    "Battery", "Magnet", "Copper coil", "Thunder stone", "Plasma orb",
    "Gale feather", "Cloud puff", "Wind chime", "Sky pearl", "Star fragment",
    "Gear", "Sludge vial", "Gas mask", "Toxic crystal", "Rusted crown",
    "Met a wild %s", "Took a nap under a tree", "Found something shiny", "Splashed in the water", "Climbed a big rock",
    "Heard a strange noise", "Chased a butterfly", "Dug a small hole", "Watched the stars", "Shared berries with a wild %s",
    "Built a sand castle", "Explored a dark corridor", "Stopped for a snack",
    "Warmed up by the lava", "Felt a static spark", "Rode a gust of wind", "Held its breath in the smog",
    // viajes: pantallas (v2.0, Y5b)
    "Travel", "Travelling", "On the way...", "%s is back!", "away", "Go", "Bag full", "NEW", "Found on %s", "Collection", "Store a pet in the Box\nto send it travelling", "Set the clock to travel", "Common", "Uncommon", "Rare", "%s is away",
    // encuentros salvajes (v2.2)
    "For a beaten wild pet", "A wild %s appeared!", "Tap to fight", "Choose a fighter", "Wild", "You won!", "It got away...", "+1 h of growth", "Throw a Pok\202 Ball?", "%u left", "Throw", "%s captured!", "Its egg is in the Backpack", "%s went away", "%s fled!", "No Pok\202 Balls left", "No room for another egg", "Captured egg!",
    // dos bichos en casa (v3.0): la caja pregunta cual
    "Which one goes to the Box?", "Swap with which one?",
    // el menu del bicho, la lista de comida y el selector (v3.3)
    "Sleep", "Wake", "Food", "Status", "Evolve", "Farewell", "Back",
    "Berry", "Red", "Blue", "Green", "Yellow", "Pink", "Orange", "Cake", "Golden",
    "Who plays?", "Who gets it?",
    // las medallas del jugador (v3.4): los tres tramos, y la 2.a pagina de la ficha
    "Bronze", "Silver", "Gold", "STATS",
    // v3.14: el Cebo
    "Draws out a wild one", "A wild one is already out", "No one can fight now",
    "Tap the ball!",
    // v3.20 HOME: la pagina Casa de la tienda (titulo, 3 lineas, 9 niveles, 3 efectos, MAX, comprar)
    "Home", "Beds", "Gym set", "Toy",
    "Straw bed", "Cushion bed", "Royal bed", "Log weights", "Dumbbells", "Full gym", "Rubber ball", "Tug rope", "Swing set",
    "Energy +%u/min asleep", "Trains up to %u", "Plays: JOY +%u", "MAX", "Buy %s for %u?",
  },
  // ---------------- FR ----------------
  {
    "Miam miam!", "Il aime \207a!", "Il a faim!", "Besoin d'un bain!",
    "\220puis\202...", "Triste...",
    "MERCI! Adieu", "Il est mort...", "Au revoir!",
    "OEUF", "Oeuf l\202gendaire!?", "Oeuf rare!", "Touche l'oeuf...", "Il bouge!", "Presque l\205!",
    "POKEDEX %u/151",
    "%s%s Niv.%u",
    "Rel\203cher %s?", "OUI", "NON",
    "%u COUPS", "FORCE +%u", "NOUVEAU RECORD!", "RECORD: %u", "FRAPPE VITE!",
    "SCORE: %u", "Trop bien!", "+bonheur",
    "R\220GLER L'HEURE", "HEURE", "MIN", "glisse haut: annuler", "Langue",
    "M\220DAILLE!", "SUPER!", "S\220RIE %u JOURS!",
    "S\220RIE %u  rec %u", "LIEN", "BAIE ???",
    "%s   AGE %lud", "touche le nom: renommer",
    "COMBAT", "ATQ", "DEF", "VIT", "PDS",
    "M\220DAILLES %d/%d", "touche: retour",
    "NOM:", "touche pour revenir",
    "NOUR", "JOIE", "ENE", "HYG",
    "REC %u",
    "PROGRES", "Niv.%u", "%u min pour Niv.%u", "\220VOLUTION", "Forme finale",
    "Pr\210t \205 \202voluer!", "Tout \205 40 pour \202voluer",
    "\220volue dans %u niv.", "N\202gligences: %u",
    "\220VOLUER", "%s veut te parler...", "Se sent abandonn\202...",
    "\220voluer?", "Garder forme", "Dire adieu?", "Adieu", "Rester ensemble",
    "Choisis ton starter",
    "Pas de sprites", "Charge-les sur la SD",
    // pasos y mochila (v1.20)
    "BALADE", "%lu PAS", "TOTAL %lu", "TROUV\220S %u/%u", "SAC \205 DOS", "Pas de capteur",
    "OBJECTIF!", "TROUV\220!", "Sac plein", "Utiliser %s?", "Charme: oeuf chroma", "Vide: va te balader!",
    "Grognon!", "Tout \202tourdi...",
    "BAIE DOR\220E", "PROT\220INE", "FER", "CARBONE", "CHARME CHROMA", "SUPER BONBON", "OEUF", "GATEAU", "RANIMER", "POKE BALL",
    "BAIE ROUGE", "BAIE BLEUE", "BAIE VERTE", "BAIE JAUNE", "BAIE ROSE", "BAIE ORANGE",  // v3.2
    "LEURRE",  // v3.14: el Cebo
    // combate (v1.24)
    "ENTRA\214NEMENT", "Super efficace!", "Pas tr\212s efficace...", "Aucun effet...", "Coup critique!", "Sp\202cial!",
    "Tape pour charger!", "Mort subite !", "Recul !", "GAGN\220!", "PERDU...", "\220GALIT\220", "Bonheur +%u", "Prix: une fois par heure",
    // efectos (v1.30)
    "NOUVEAU NIVEAU!",
    // la caja (v1.37)
    "BOITE %u/%u", "Ranger %s?", "R\202veiller %s?", "%s va dans la bo\214te", "Actif: %s", "Vide", "Ab\214m\202",
    // combate (v1.38)
    "Tape!",
    // huevos limitados (v1.39)
    "PAS D'OEUF", "Marche pour en trouver un!", "Glisse \205 droite: bo\214te", "Pas d'oeuf: maison vide", "Oeuf: \202clot si maison libre",
    // huevos al acabar una crianza (v1.40/v1.41)
    "Faire \202clore (%u)", "Un oeuf va arriver", "Tes oeufs attendent!", "CADEAU!",
    // la cascada (v1.43)
    "Garde la barre dans le vert!", "MAINTIENS APPUYE", "FOCUS %u.%us", "RECORD %u.%us", "DEFENSE +%u", "Plus de crottes!",
    // la carrera en trineo (v1.44)
    "Tiens-la comme pour jouer", "Ne bouge plus...", "PENCHE POUR TOURNER", "ESQUIVES: %u", "VITESSE +%u",
    // v1.47: bonificacion de la cascada, entrenamiento al tope
    "BONUS +%u", "FORCE MAX", "DEFENSE MAX", "VITESSE MAX",
    // v1.49: la barra fina de la ficha de combate
    "barre fine: entra\214nement",
    // v1.53: la mochila con su boton: como se usa y dormido solo se mira
    "Touche un objet pour l'utiliser", "Il dort : on regarde",
    // v1.54: la tarta: lleno durante 3 h, y tocarla sin ninguna
    "Pas de g\203teau: marche",
    // v1.56: el botin de un combate ganado, que espera envuelto en casa
    "Cadeau \205 la maison!",
    // v1.57: al evolucionar cambio la baya favorita (vuelve a ser un secreto)
    "Ses go\226ts ont chang\202!",
    // v1.59: la pista del Revivir en la mochila (no se usa a mano)
    "Sauve un ami mourant",
    // v1.60: muerte por abandono: sin vinculo, el dialogo, el huevo que deja, revivido
    "Il est tr\212s faible!", "%s se meurt...", "Utiliser Ranimer? (%u)", "Plus de Ranimer", "OK", "Il a laiss\202 un oeuf...", "Ranim\202!",
    // v1.63: tiene sueño
    "Il a sommeil...",
    // v1.64: la hora del Revivir
    "Encore %u min", "%s est mort...", "Ranimer trop tard",
    // stages
    "Phases", "Insignes", "Choisis ton \202quipe", "Commencer", "BOSS", "Verrouill\202", "Phase r\202ussie!", "Battu...",
    "Prochain rival", "Allez, %s!", "Nouvel insigne!", "Niv.%u-%u", "\220quipe: +1 h, +lien",
    "R\202compense pr\202te", "Phase %u",
    // el patio (v2.0)
    "BOUTIQUE", "Acheter", "Utiliser", "Achet\202!", "Soins", "Jouer", "Sac",
    // el huevo del patio (v2.0, Y3)
    "Secoue-moi!",
    // la tienda de la mochila y los fondos (v2.0, Y4)
    "OBJETS", "Fonds", "Acheter %s?", "Pas assez de pieces", "Sac plein",
    "Prairie", "Carriere", "Foret", "Plage", "Ruines",
    "Volcan", "Centrale", "Tour celeste", "Quartier toxique",
    "Utilise", "Pas sur la SD", "Touche: acheter",
    // los viajes del patio (v2.0, Y5; v3.15 9 mapas): 45 recuerdos (5 por mapa; el 5.o es el raro) y 17 sucesos del camino
    "Trefle a quatre feuilles", "Couronne de marguerites", "Plume", "Pot de miel", "Gland dore",
    "Galet lisse", "Eclat de quartz", "Coquillage fossile", "Minerai de fer", "Pierre de lune",
    "Pomme de pin", "Champignon", "Plume de hibou", "Goutte d'ambre", "Feuille ancienne",
    "Coquillage", "Etoile de mer", "Verre de mer", "Bouteille a la mer", "Perle",
    "Vieille piece", "Masque d'argile", "Bout de bougie", "Tablette runique", "Lanterne spectrale",
    "Braise", "Obsidienne", "Pierre de magma", "Plume de feu", "Braise de phenix",
    "Pile", "Aimant", "Bobine de cuivre", "Pierre foudre", "Orbe de plasma",
    "Plume de rafale", "Bout de nuage", "Carillon a vent", "Perle du ciel", "Eclat d'etoile",
    "Engrenage", "Fiole de boue", "Masque a gaz", "Cristal toxique", "Couronne rouillee",
    "A rencontre un %s sauvage", "A fait la sieste sous un arbre", "A trouve quelque chose de brillant", "A patauge dans l'eau", "A escalade un gros rocher",
    "A entendu un bruit etrange", "A couru apres un papillon", "A creuse un petit trou", "A regarde les etoiles", "A partage des baies avec un %s sauvage",
    "A fait un chateau de sable", "A explore un couloir sombre", "S'est arrete pour un gouter",
    "A eu chaud pres de la lave", "A senti une etincelle", "A vole sur une rafale", "A retenu son souffle dans la fumee",
    // viajes: pantallas (v2.0, Y5b)
    "Voyage", "En voyage", "En route...", "%s est de retour !", "absent", "Partir", "Sac plein", "NOUVEAU", "Trouve le %s", "Collection", "Range un bicho dans la Boite\npour l'envoyer en voyage", "Regle l'heure pour voyager", "Commun", "Peu commun", "Rare", "%s est parti",
    // encuentros salvajes (v2.2)
    "Pour un sauvage vaincu", "Un %s sauvage!", "Touche pour combattre", "Choisis un combattant", "Sauvage", "Gagne!", "Il a fui...", "+1 h de croissance", "Lancer une Poke Ball?", "Il en reste %u", "Lancer", "%s capture!", "Son oeuf est dans le sac", "%s est parti", "%s a fui!", "Plus de Poke Balls", "Plus de place pour un oeuf", "Oeuf capture!",
    // dos bichos en casa (v3.0): la caja pregunta cual
    "Lequel va dans la boite?", "Echanger avec lequel?",
    // el menu del bicho, la lista de comida y el selector (v3.3)
    "Dormir", "Reveil", "Manger", "Etat", "Evoluer", "Adieu", "Retour",
    "Baie", "Rouge", "Bleue", "Verte", "Jaune", "Rose", "Orange", "Gateau", "Doree",
    "Qui joue?", "Pour qui?",
    // las medallas del jugador (v3.4): los tres tramos, y la 2.a pagina de la ficha
    "Bronze", "Argent", "Or", "STATS",
    // v3.14: el Cebo
    "Attire un sauvage", "Un sauvage est deja la", "Personne ne peut combattre",
    "Touche la balle!",
    // v3.20 HOME: la pagina Casa de la tienda (titulo, 3 lineas, 9 niveles, 3 efectos, MAX, comprar)
    "Maison", "Lits", "Gym", "Jouet",
    "Lit de paille", "Lit coussin", "Lit royal", "Buches", "Halteres", "Salle complete", "Balle", "Corde", "Balancoire",
    "Energie +%u/min", "Entraine jusqu'a %u", "Jeu: JOIE +%u", "MAX", "Acheter %s pour %u?",
  },
  // ---------------- DE ----------------
  // El aleman si lleva dieresis: la fuente 5x7 del firmware es una tabla CP437
  // completa (256 glifos), asi que basta con el byte suelto de cada caracter.
  // Van en octal para no depender de la codificacion del fichero fuente:
  //   \204 a   \224 o   \201 u   \216 A   \231 O   \232 U   \341 ss
  // Un byte por caracter: strlen() sigue midiendo bien y el centrado no cambia.
  {
    "Mampf mampf!", "Gef\204llt ihm!", "Hat Hunger!", "Braucht ein Bad!",
    "Ersch\224pft...", "Traurig...",
    "DANKE! Leb wohl", "Es ist gestorben...", "Tsch\201ss! Winkt",
    "EI", "Legend\204res Ei!?", "Seltenes Ei!", "Ber\201hre das Ei...", "Es bewegt sich!", "Fast so weit!",
    "POKEDEX %u/151",
    "%s%s Lv.%u",
    "%s freilassen?", "JA", "NEIN",
    "%u TREFFER", "KRAFT +%u", "NEUER REKORD!", "REKORD: %u", "SCHNELL HAUEN!",
    "PUNKTE: %u", "Wie sch\224n!", "+Freude",
    "ZEIT STELLEN", "STD", "MIN", "hoch wischen: Abbruch", "Sprache",
    "MEDAILLE!", "TOLL!", "%u TAGE SERIE!",
    "SERIE %u  rek %u", "BND", "BEERE ???",
    "%s   ALTER %lud", "Name tippen: umbenennen",
    "KAMPF", "ANG", "VER", "INI", "GEW",
    "MEDAILLEN %d/%d", "tippen: zur\201ck",
    "NAME:", "tippen: zur\201ck",
    "ESS", "FRO", "ENE", "HYG",
    "REK %u",
    "FORTSCHRITT", "Lv.%u", "%u min bis Lv.%u", "ENTWICKLUNG", "Endform",
    "Bereit zur Entwicklung!", "Alles >=40 zur Entwicklung",
    "Entwicklung in %u Lv.", "Patzer: %u",
    "ENTWICKELN", "%s will dir etwas sagen...", "F\201hlt sich verlassen...",
    "Entwickeln?", "Form behalten", "Abschied?", "Leb wohl", "Zusammen bleiben",
    "W\204hle deinen Starter",
    "Keine Sprites", "Auf die SD laden",
    // pasos y mochila (v1.20)
    "SPAZIERGANG", "%lu SCHRITTE", "GESAMT %lu", "FUNDE %u/%u", "RUCKSACK", "Kein Sensor",
    "ZIEL!", "GEFUNDEN!", "Rucksack voll", "%s nutzen?", "Pin: Ei schillert", "Leer: geh spazieren!",
    "Grummelig!", "Schwindelig...",
    "GOLDBEERE", "PROTEIN", "EISEN", "CARBON", "SCHILLERPIN", "SONDERBONBON", "EI", "KUCHEN", "BELEBER", "POKEBALL",
    "ROTE BEERE", "BLAUE BEERE", "GR\232NE BEERE", "GELBE BEERE", "ROSA BEERE", "ORANGE BEERE",  // v3.2
    "K\224DER",  // v3.14: el Cebo
    // combate (v1.24)
    "TRAINING", "Sehr effektiv!", "Nicht sehr effektiv...", "Keine Wirkung...", "Volltreffer!", "Spezial!",
    "Tippen zum Laden!", "Sudden Death!", "R\201cksto\341!", "GEWONNEN!", "VERLOREN...", "UNENTSCHIEDEN", "Laune +%u", "Belohnung: 1x pro Stunde",
    // efectos (v1.30)
    "AUFGESTIEGEN!",
    // la caja (v1.37)
    "BOX %u/%u", "%s ablegen?", "%s wecken?", "%s kommt in die Box", "Jetzt: %s", "Leer", "Besch\204digt",
    // combate (v1.38)
    "Tippen!",
    // huevos limitados (v1.39)
    "KEIN EI", "Lauf los und finde eins!", "Nach rechts: die Box", "Kein Ei: Zuhause leer", "Ei schl\201pft bei leerem Zuhause",
    // huevos al acabar una crianza (v1.40/v1.41)
    "Ei ausbr\201ten (%u)", "Ein neues Ei kommt", "Deine Eier warten!", "GESCHENK!",
    // la cascada (v1.43)
    "Halte den Balken im Gr\201nen!", "GEDR\232CKT HALTEN", "FOKUS %u.%us", "REKORD %u.%us", "VERT +%u", "Kein Kot mehr!",
    // la carrera en trineo (v1.44)
    "Halte es wie beim Spielen", "Still halten...", "NEIGEN ZUM LENKEN", "AUSGEWICHEN: %u", "INIT +%u",
    // v1.47: bonificacion de la cascada, entrenamiento al tope
    "BONUS +%u", "KRAFT MAX", "VERT MAX", "INIT MAX",
    // v1.49: la barra fina de la ficha de combate
    "d\201nner Balken: Training",
    // v1.53: la mochila con su boton: como se usa y dormido solo se mira
    "Tippe ein Item zum Benutzen", "Es schl\204ft: nur ansehen",
    // v1.54: la tarta: lleno durante 3 h, y tocarla sin ninguna
    "Kein Kuchen: lauf oder k\204mpf",
    // v1.56: el botin de un combate ganado, que espera envuelto en casa
    "Geschenk daheim!",
    // v1.57: al evolucionar cambio la baya favorita (vuelve a ser un secreto)
    "Neuer Geschmack!",
    // v1.59: la pista del Revivir en la mochila (no se usa a mano)
    "Rettet ein sterbendes Tier",
    // v1.60: muerte por abandono: sin vinculo, el dialogo, el huevo que deja, revivido
    "Sehr schwach!", "%s stirbt...", "Beleber nutzen? (%u)", "Keine Beleber mehr", "OK", "Es hinterl\204sst ein Ei...", "Wiederbelebt!",
    // v1.63: tiene sueño
    "Wird m\201de...",
    // v1.64: la hora del Revivir
    "Noch %u Min.", "%s ist gestorben...", "Kein Beleber rechtzeitig",
    // stages
    "Stufen", "Abzeichen", "W\204hle dein Team", "Start", "BOSS", "Gesperrt", "Stufe geschafft!", "Besiegt...",
    "N\204chster Gegner", "Los, %s!", "Neues Abzeichen!", "Lv.%u-%u", "Team: +1 Std, +Bindung",
    "Belohnung bereit", "Stufe %u",
    // el patio (v2.0)
    "LADEN", "Kaufen", "Nutzen", "Gekauft!", "Pflege", "Spielen", "Tasche",
    // el huevo del patio (v2.0, Y3)
    "Sch\201ttel mich!",
    // la tienda de la mochila y los fondos (v2.0, Y4)
    "DINGE", "Hintergruende", "%s kaufen?", "Zu wenig Muenzen", "Rucksack voll",
    "Wiese", "Steinbruch", "Wald", "Strand", "Ruinen",
    "Vulkan", "Kraftwerk", "Himmelsturm", "Giftviertel",
    "Aktiv", "Nicht auf SD", "Tippen: kaufen",
    // los viajes del patio (v2.0, Y5; v3.15 9 mapas): 45 recuerdos (5 por mapa; el 5.o es el raro) y 17 sucesos del camino
    "Kleeblatt", "Gaensebluemchenkranz", "Feder", "Honigglas", "Goldene Eichel",
    "Glatter Kiesel", "Quarzsplitter", "Fossile Muschel", "Eisenerz", "Mondstein",
    "Tannenzapfen", "Pilz", "Eulenfeder", "Bernsteintropfen", "Uraltes Blatt",
    "Muschel", "Seestern", "Meerglas", "Flaschenpost", "Perle",
    "Alte Muenze", "Tonmaske", "Kerzenstummel", "Runentafel", "Geisterlaterne",
    "Glut", "Obsidian", "Magmastein", "Feuerfeder", "Phoenixglut",
    "Batterie", "Magnet", "Kupferspule", "Donnerstein", "Plasmakugel",
    "Sturmfeder", "Wolkenbausch", "Windspiel", "Himmelsperle", "Sternsplitter",
    "Zahnrad", "Schlammphiole", "Gasmaske", "Giftkristall", "Rostige Krone",
    "Traf ein wildes %s", "Machte ein Nickerchen unter einem Baum", "Fand etwas Glitzerndes", "Planschte im Wasser", "Kletterte auf einen Felsen",
    "Hoerte ein seltsames Geraeusch", "Jagte einen Schmetterling", "Grub ein kleines Loch", "Schaute die Sterne an", "Teilte Beeren mit einem wilden %s",
    "Baute eine Sandburg", "Erkundete einen dunklen Gang", "Machte Pause fuer einen Snack",
    "Waermte sich an der Lava", "Spuerte einen Funken", "Ritt auf einem Windstoss", "Hielt im Smog die Luft an",
    // viajes: pantallas (v2.0, Y5b)
    "Reise", "Unterwegs", "Auf dem Weg...", "%s ist zurueck!", "weg", "Los", "Beutel voll", "NEU", "Gefunden am %s", "Sammlung", "Lege ein Tier in die Box,\num es auf Reisen zu schicken", "Stelle die Uhr zum Reisen", "Haeufig", "Selten", "Sehr selten", "%s ist unterwegs",
    // encuentros salvajes (v2.2)
    "Fuer ein besiegtes wildes", "Ein wildes %s!", "Tippe zum Kampf", "Waehle einen Kaempfer", "Wild", "Gewonnen!", "Entkommen...", "+1 h Wachstum", "Pokeball werfen?", "Noch %u", "Werfen", "%s gefangen!", "Sein Ei ist im Rucksack", "%s ist weg", "%s ist geflohen!", "Keine Pokebaelle mehr", "Kein Platz fuer ein Ei", "Gefangenes Ei!",
    // dos bichos en casa (v3.0): la caja pregunta cual
    "Wer kommt in die Box?", "Mit wem tauschen?",
    // el menu del bicho, la lista de comida y el selector (v3.3)
    "Schlafen", "Wecken", "Essen", "Status", "Entwickeln", "Abschied", "Zurueck",
    "Beere", "Rot", "Blau", "Gruen", "Gelb", "Rosa", "Orange", "Kuchen", "Gold",
    "Wer spielt?", "Fuer wen?",
    // las medallas del jugador (v3.4): los tres tramos, y la 2.a pagina de la ficha
    "Bronze", "Silber", "Gold", "WERTE",
    // v3.14: el Cebo
    "Lockt ein wildes an", "Schon ein wildes da", "Niemand kann kampfen",
    "Tipp den Ball an!",
    // v3.20 HOME: la pagina Casa de la tienda (titulo, 3 lineas, 9 niveles, 3 efectos, MAX, comprar)
    "Haus", "Betten", "Sportgeraet", "Spielzeug",
    "Strohbett", "Kissenbett", "Koenigsbett", "Holzgewichte", "Hanteln", "Ganzes Gym", "Gummiball", "Zugseil", "Schaukel",
    "Energie +%u/Min Schlaf", "Trainiert bis %u", "Spielt: FREUDE +%u", "MAX", "%s fuer %u kaufen?",
  },
  // ---------------- IT ----------------
  {
    "Gnam gnam!", "Gli piace!", "Ha fame!", "Vuole un bagno!",
    "Esausto...", "Triste...",
    "GRAZIE! Addio", "E' morto...", "Ciao! Saluta",
    "UOVO", "Uovo leggendario!?", "Uovo raro!", "Tocca l'uovo...", "Si muove!", "Ci siamo quasi!",
    "POKEDEX %u/151",
    "%s%s Lv.%u",
    "Liberare %s?", "SI", "NO",
    "%u COLPI", "FORZA +%u", "NUOVO RECORD!", "RECORD: %u", "COLPISCI VELOCE!",
    "PUNTI: %u", "Che gioia!", "+felicit\205",
    "IMPOSTA ORA", "ORA", "MIN", "scorri su: annulla", "Lingua",
    "MEDAGLIA!", "GRANDE!", "SERIE %u GIORNI!",
    "SERIE %u  rec %u", "LEG", "BACCA ???",
    "%s   ETA %lud", "tocca il nome: rinomina",
    "LOTTA", "ATT", "DIF", "VEL", "PES",
    "MEDAGLIE %d/%d", "tocca: indietro",
    "NOME:", "tocca per tornare",
    "CIB", "GIO", "ENE", "IGI",
    "REC %u",
    "PROGRESSI", "Lv.%u", "%u min per Lv.%u", "EVOLUZIONE", "Forma finale",
    "Pronto a evolvere!", "Tutto a 40 per evolvere",
    "Evolve tra %u liv.", "Disattenzioni: %u",
    "EVOLVI", "%s vuole dirti qualcosa...", "Si sente abbandonato...",
    "Evolvere?", "Mantieni forma", "Salutare?", "Addio", "Restare insieme",
    "Scegli l'iniziale",
    "Senza sprite", "Caricali sulla SD",
    // pasos y mochila (v1.20)
    "PASSEGGIATA", "%lu PASSI", "TOTALE %lu", "TROVATI %u/%u", "ZAINO", "Nessun sensore",
    "OBIETTIVO!", "TROVATO!", "Zaino pieno", "Usare %s?", "Amuleto: uovo shiny", "Vuoto: fai due passi!",
    "Arrabbiato!", "Che capogiro...",
    "BACCA D'ORO", "PROTEINA", "FERRO", "CARBURANTE", "CROMAMULETO", "CARAMELLA RARA", "UOVO", "TORTA", "RIANIMA", "POKE BALL",
    "BACCA ROSSA", "BACCA BLU", "BACCA VERDE", "BACCA GIALLA", "BACCA ROSA", "BACCA ARANCIO",  // v3.2
    "ESCA",  // v3.14: el Cebo
    // combate (v1.24)
    "ALLENAMENTO", "Superefficace!", "Non \212 molto efficace...", "Nessun effetto...", "Brutto colpo!", "Speciale!",
    "Tocca per caricare!", "Morte improvvisa!", "Contraccolpo!", "HAI VINTO!", "HAI PERSO...", "PAREGGIO", "Felicit\205 +%u", "Premio: una volta l'ora",
    // efectos (v1.30)
    "NUOVO LIVELLO!",
    // la caja (v1.37)
    "BOX %u/%u", "Depositare %s?", "Svegliare %s?", "%s va nel box", "Ora: %s", "Vuoto", "Danneggiato",
    // combate (v1.38)
    "Tocca!",
    // huevos limitados (v1.39)
    "NESSUN UOVO", "Cammina per trovarne uno!", "Scorri a destra: il box", "Niente uovo: casa vuota", "L'uovo nasce a casa libera",
    // huevos al acabar una crianza (v1.40/v1.41)
    "Schiudi uovo (%u)", "Arriva un uovo nuovo", "Le uova ti aspettano!", "REGALO!",
    // la cascada (v1.43)
    "Tieni la barra nel verde!", "TIENI PREMUTO", "FOCUS %u.%us", "RECORD %u.%us", "DIFESA +%u", "Niente cacca!",
    // la carrera en trineo (v1.44)
    "Tienila come per giocare", "Fermo un attimo...", "INCLINA PER STERZARE", "SCHIVATI: %u", "VELOCITA +%u",
    // v1.47: bonificacion de la cascada, entrenamiento al tope
    "BONUS +%u", "FORZA MAX", "DIFESA MAX", "VELOCITA MAX",
    // v1.49: la barra fina de la ficha de combate
    "barra sottile: allenamento",
    // v1.53: la mochila con su boton: como se usa y dormido solo se mira
    "Tocca un oggetto per usarlo", "Dorme: solo guardare",
    // v1.54: la tarta: lleno durante 3 h, y tocarla sin ninguna
    "Niente torta: cammina",
    // v1.56: el botin de un combate ganado, que espera envuelto en casa
    "Regalo a casa!",
    // v1.57: al evolucionar cambio la baya favorita (vuelve a ser un secreto)
    "Gusti cambiati!",
    // v1.59: la pista del Revivir en la mochila (no se usa a mano)
    "Salva chi sta morendo",
    // v1.60: muerte por abandono: sin vinculo, el dialogo, el huevo que deja, revivido
    "E' debolissimo!", "%s sta morendo...", "Usare Rianima? (%u)", "Niente Rianima", "OK", "Ha lasciato un uovo...", "Rianimato!",
    // v1.63: tiene sueño
    "Ha sonno...",
    // v1.64: la hora del Revivir
    "Ancora %u min", "%s e' morto...", "Rianima non arrivato",
    // stages
    "Fasi", "Distintivi", "Scegli la squadra", "Via", "BOSS", "Bloccata", "Fase superata!", "Sconfitto...",
    "Prossimo rivale", "Vai, %s!", "Nuovo distintivo!", "Lv.%u-%u", "Squadra: +1 h, +legame",
    "Premio pronto", "Fase %u",
    // el patio (v2.0)
    "NEGOZIO", "Compra", "Usa", "Comprato!", "Cura", "Gioca", "Zaino",
    // el huevo del patio (v2.0, Y3)
    "Scuotimi!",
    // la tienda de la mochila y los fondos (v2.0, Y4)
    "OGGETTI", "Sfondi", "Comprare %s?", "Monete insufficienti", "Zaino pieno",
    "Prato", "Cava", "Bosco", "Spiaggia", "Rovine",
    "Vulcano", "Centrale", "Torre del cielo", "Distretto tossico",
    "In uso", "Non sulla SD", "Tocca per comprare",
    // los viajes del patio (v2.0, Y5; v3.15 9 mapas): 45 recuerdos (5 por mapa; el 5.o es el raro) y 17 sucesos del camino
    "Quadrifoglio", "Corona di margherite", "Piuma", "Barattolo di miele", "Ghianda dorata",
    "Ciottolo liscio", "Scheggia di quarzo", "Conchiglia fossile", "Minerale di ferro", "Pietra lunare",
    "Pigna", "Fungo", "Piuma di gufo", "Goccia d'ambra", "Foglia antica",
    "Conchiglia", "Stella marina", "Vetro di mare", "Messaggio in bottiglia", "Perla",
    "Moneta antica", "Maschera d'argilla", "Moncone di candela", "Tavola runica", "Lanterna spettrale",
    "Brace", "Ossidiana", "Pietra di magma", "Piuma di fuoco", "Brace di fenice",
    "Batteria", "Magnete", "Bobina di rame", "Pietratuono", "Sfera di plasma",
    "Piuma di bufera", "Batuffolo di nuvola", "Campanella a vento", "Perla del cielo", "Frammento di stella",
    "Ingranaggio", "Fiala di melma", "Maschera antigas", "Cristallo tossico", "Corona arrugginita",
    "Ha incontrato un %s selvatico", "Ha fatto un pisolino sotto un albero", "Ha trovato qualcosa di luccicante", "Ha sguazzato nell'acqua", "Si e arrampicato su una roccia",
    "Ha sentito un rumore strano", "Ha inseguito una farfalla", "Ha scavato una piccola buca", "Ha guardato le stelle", "Ha diviso le bacche con un %s selvatico",
    "Ha costruito un castello di sabbia", "Ha esplorato un corridoio buio", "Si e fermato per uno spuntino",
    "Si e scaldato vicino alla lava", "Ha sentito una scintilla", "Ha cavalcato una raffica", "Ha trattenuto il fiato nello smog",
    // viajes: pantallas (v2.0, Y5b)
    "Viaggio", "In viaggio", "In cammino...", "%s e tornato!", "via", "Vai", "Zaino pieno", "NUOVO", "Trovato il %s", "Collezione", "Metti un mostro nel Box\nper mandarlo in viaggio", "Imposta l'ora per viaggiare", "Comune", "Non comune", "Raro", "%s e in viaggio",
    // encuentros salvajes (v2.2)
    "Per un selvatico battuto", "Un %s selvatico!", "Tocca per lottare", "Scegli chi lotta", "Selvatico", "Vinto!", "E fuggito...", "+1 h di crescita", "Lanciare una Poke Ball?", "Ne restano %u", "Lancia", "%s catturato!", "Il suo uovo e nello zaino", "%s se ne e andato", "%s e fuggito!", "Niente Poke Ball", "Non c'e posto per un uovo", "Uovo catturato!",
    // dos bichos en casa (v3.0): la caja pregunta cual
    "Chi va nel box?", "Scambiare con chi?",
    // el menu del bicho, la lista de comida y el selector (v3.3)
    "Dormi", "Sveglia", "Cibo", "Stato", "Evolvi", "Addio", "Indietro",
    "Bacca", "Rossa", "Blu", "Verde", "Gialla", "Rosa", "Arancio", "Torta", "Dorata",
    "Chi gioca?", "Per chi?",
    // las medallas del jugador (v3.4): los tres tramos, y la 2.a pagina de la ficha
    "Bronzo", "Argento", "Oro", "STATISTICHE",
    // v3.14: el Cebo
    "Attira un selvatico", "C'e gia un selvatico", "Nessuno puo lottare",
    "Tocca la palla!",
    // v3.20 HOME: la pagina Casa de la tienda (titulo, 3 lineas, 9 niveles, 3 efectos, MAX, comprar)
    "Casa", "Letti", "Palestra", "Giocattolo",
    "Letto di paglia", "Letto cuscino", "Letto reale", "Pesi di legno", "Manubri", "Palestra intera", "Palla di gomma", "Corda", "Altalena",
    "Energia +%u/min", "Allena fino a %u", "Gioca: GIOIA +%u", "MAX", "Comprare %s per %u?",
  },
  // ---------------- PT ----------------
  {
    "Nham nham!", "Ele gosta!", "Est\240 com fome!", "Precisa de banho!",
    "Exausto...", "Triste...",
    "OBRIGADO! Adeus", "Morreu...", "Tchau! Acena",
    "OVO", "Ovo lend\240rio!?", "Ovo raro!", "Toque no ovo...", "Mexe-se!", "Quase l\240!",
    "POKEDEX %u/151",
    "%s%s Niv.%u",
    "Soltar %s?", "SIM", "NAO",
    "%u GOLPES", "FOR\200A +%u", "NOVO RECORDE!", "RECORDE: %u", "BATA RAPIDO!",
    "PONTOS: %u", "Que alegria!", "+alegria",
    "AJUSTAR HORA", "HORA", "MIN", "deslize cima: cancelar", "Idioma",
    "MEDALHA!", "OTIMO!", "%u DIAS SEGUIDOS!",
    "SEQ %u  rec %u", "LA\200O", "BAGA ???",
    "%s   IDADE %lud", "toque no nome: renomear",
    "COMBATE", "ATQ", "DEF", "VEL", "PES",
    "MEDALHAS %d/%d", "toque: voltar",
    "NOME:", "toque para voltar",
    "COM", "ALE", "ENE", "HIG",
    "REC %u",
    "PROGRESSO", "Niv.%u", "%u min para Niv.%u", "EVOLUCAO", "Forma final",
    "Pronto a evoluir!", "Tudo a 40 para evoluir",
    "Evolui em %u niv.", "Descuidos: %u",
    "EVOLUIR", "%s quer dizer-te algo...", "Sente-se abandonado...",
    "Evoluir?", "Manter forma", "Despedir?", "Adeus", "Ficar juntos",
    "Escolhe o inicial",
    "Sem sprites", "Carrega-os no SD",
    // pasos y mochila (v1.20)
    "PASSEIO", "%lu PASSOS", "TOTAL %lu", "ACHADOS %u/%u", "MOCHILA", "Sem sensor",
    "META!", "ACHOU!", "Mochila cheia", "Usar %s?", "Amuleto: ovo shiny", "Vazia: vai passear!",
    "Zangado!", "Que tontura...",
    "BAGA DOURADA", "PROTEINA", "FERRO", "CARBOS", "AMULETO BRILHANTE", "DOCE RARO", "OVO", "BOLO", "REVIVER", "POKE BOLA",
    "BAGA VERMELHA", "BAGA AZUL", "BAGA VERDE", "BAGA AMARELA", "BAGA ROSA", "BAGA LARANJA",  // v3.2
    "ISCA",  // v3.14: el Cebo
    // combate (v1.24)
    "TREINO", "Super efetivo!", "Pouco efetivo...", "Sem efeito...", "Golpe cr\241tico!", "Especial!",
    "Toque para carregar!", "Morte s\243bita!", "Recuo!", "VENCEU!", "PERDEU...", "EMPATE", "Alegria +%u", "Pr\210mio: 1 vez por hora",
    // efectos (v1.30)
    "SUBIU DE NIVEL!",
    // la caja (v1.37)
    "CAIXA %u/%u", "Guardar %s?", "Acordar %s?", "%s vai para a caixa", "Agora: %s", "Vazio", "Danificado",
    // combate (v1.38)
    "Toque!",
    // huevos limitados (v1.39)
    "SEM OVO", "Caminhe para achar um!", "Deslize p/ direita: caixa", "Sem ovo: casa vazia", "O ovo nasce com a casa livre",
    // huevos al acabar una crianza (v1.40/v1.41)
    "Chocar um ovo (%u)", "Vem um ovo novo", "Seus ovos esperam!", "PRESENTE!",
    // la cascada (v1.43)
    "Mantenha a barra no verde!", "SEGURE O TOQUE", "FOCO %u.%us", "RECORDE %u.%us", "DEFESA +%u", "Sem coco!",
    // la carrera en trineo (v1.44)
    "Segure como vai jogar", "Parado um pouco...", "INCLINE PARA VIRAR", "DESVIOS: %u", "VELOCIDADE +%u",
    // v1.47: bonificacion de la cascada, entrenamiento al tope
    "EXTRA +%u", "FOR\200A MAX", "DEFESA MAX", "VELOCIDADE MAX",
    // v1.49: la barra fina de la ficha de combate
    "barra fina: treino",
    // v1.53: la mochila con su boton: como se usa y dormido solo se mira
    "Toque um item para usar", "Dormindo: s\242 olhar",
    // v1.54: la tarta: lleno durante 3 h, y tocarla sin ninguna
    "Sem bolo: ande ou lute",
    // v1.56: el botin de un combate ganado, que espera envuelto en casa
    "Presente em casa!",
    // v1.57: al evolucionar cambio la baya favorita (vuelve a ser un secreto)
    "Mudou de gosto!",
    // v1.59: la pista del Revivir en la mochila (no se usa a mano)
    "Salva quem esta morrendo",
    // v1.60: muerte por abandono: sin vinculo, el dialogo, el huevo que deja, revivido
    "Muito fraco!", "%s esta morrendo...", "Usar Reviver? (%u)", "Sem Reviver", "OK", "Deixou um ovo...", "Revivido!",
    // v1.63: tiene sueño
    "Esta com sono...",
    // v1.64: la hora del Revivir
    "Faltam %u min", "%s morreu...", "Reviver nao chegou",
    // stages
    "Fases", "Ins\241gnias", "Escolhe a tua equipa", "Iniciar", "CHEFE", "Bloqueada", "Fase conclu\241da!", "Derrotado...",
    "Pr\242ximo rival", "Vai, %s!", "Nova ins\241gnia!", "Niv.%u-%u", "Equipa: +1 h, +la\207o",
    "Pr\202mio pronto", "Fase %u",
    // el patio (v2.0)
    "LOJA", "Comprar", "Usar", "Comprado!", "Cuidar", "Jogar", "Mochila",
    // el huevo del patio (v2.0, Y3)
    "Me sacode!",
    // la tienda de la mochila y los fondos (v2.0, Y4)
    "ITENS", "Fundos", "Comprar %s?", "Faltam moedas", "Mochila cheia",
    "Prado", "Pedreira", "Floresta", "Praia", "Ruinas",
    "Vulcao", "Usina", "Torre do ceu", "Distrito toxico",
    "Em uso", "Nao esta no SD", "Toque para comprar",
    // los viajes del patio (v2.0, Y5; v3.15 9 mapas): 45 recuerdos (5 por mapa; el 5.o es el raro) y 17 sucesos del camino
    "Trevo de quatro folhas", "Coroa de margaridas", "Pena", "Pote de mel", "Bolota dourada",
    "Seixo liso", "Lasca de quartzo", "Concha fossil", "Minerio de ferro", "Pedra da lua",
    "Pinha", "Cogumelo", "Pena de coruja", "Gota de ambar", "Folha ancestral",
    "Concha", "Estrela-do-mar", "Vidro marinho", "Garrafa com mensagem", "Perola",
    "Moeda antiga", "Mascara de barro", "Toco de vela", "Tabuleta de runas", "Lanterna espiritual",
    "Brasa", "Obsidiana", "Pedra de magma", "Pena de fogo", "Brasa de fenix",
    "Pilha", "Ima", "Bobina de cobre", "Pedra trovao", "Orbe de plasma",
    "Pena de vendaval", "Floco de nuvem", "Sino de vento", "Perola do ceu", "Fragmento de estrela",
    "Engrenagem", "Frasco de lodo", "Mascara de gas", "Cristal toxico", "Coroa enferrujada",
    "Encontrou um %s selvagem", "Tirou uma soneca debaixo de uma arvore", "Achou algo brilhante", "Chapinhou na agua", "Subiu numa pedra grande",
    "Ouviu um barulho estranho", "Perseguiu uma borboleta", "Cavou um buraquinho", "Olhou as estrelas", "Dividiu frutinhas com um %s selvagem",
    "Fez um castelo de areia", "Explorou um corredor escuro", "Parou para um lanche",
    "Se aqueceu perto da lava", "Sentiu uma faisca", "Voou numa rajada de vento", "Prendeu a respiracao na fumaca",
    // viajes: pantallas (v2.0, Y5b)
    "Viagem", "Em viagem", "A caminho...", "%s voltou!", "fora", "Ir", "Mochila cheia", "NOVO", "Achado em %s", "Colecao", "Guarde um bicho na Caixa\npara mandar viajar", "Acerte a hora para viajar", "Comum", "Incomum", "Raro", "%s esta viajando",
    // encuentros salvajes (v2.2)
    "Para um selvagem vencido", "Um %s selvagem!", "Toca para lutar", "Escolhe quem luta", "Selvagem", "Ganhaste!", "Fugiu...", "+1 h de crescimento", "Lancar uma Poke Bola?", "Restam %u", "Lancar", "%s capturado!", "O ovo esta na mochila", "%s foi embora", "%s fugiu!", "Sem Poke Bolas", "Nao cabem mais ovos", "Ovo capturado!",
    // dos bichos en casa (v3.0): la caja pregunta cual
    "Qual vai para a caixa?", "Trocar com qual?",
    // el menu del bicho, la lista de comida y el selector (v3.3)
    "Dormir", "Acordar", "Comida", "Estado", "Evoluir", "Adeus", "Voltar",
    "Baga", "Vermelha", "Azul", "Verde", "Amarela", "Rosa", "Laranja", "Bolo", "Dourada",
    "Quem joga?", "Para quem?",
    // las medallas del jugador (v3.4): los tres tramos, y la 2.a pagina de la ficha
    "Bronze", "Prata", "Ouro", "ATRIBUTOS",
    // v3.14: el Cebo
    "Atrai um selvagem", "Ja ha um selvagem", "Ninguem pode lutar",
    "Toque na bola!",
    // v3.20 HOME: la pagina Casa de la tienda (titulo, 3 lineas, 9 niveles, 3 efectos, MAX, comprar)
    "Casa", "Camas", "Ginasio", "Brinquedo",
    "Cama de palha", "Cama de almofada", "Cama real", "Pesos de tronco", "Halteres", "Ginasio inteiro", "Bola de borracha", "Corda", "Balanco",
    "Energia +%u/min", "Treina ate %u", "Brinca: Alegria +%u", "MAX", "Comprar %s por %u?",
  },
  // ---------------- JA ----------------
  {
    "おいしい！", "うれしそう！", "おなかすいた！", "おふろにいれて！",
    "つかれた...", "かなしい...",
    "ありがとう！ またね", "しんでしまった...", "バイバイ！",

    "たまご", "でんせつのたまご！？", "めずらしいたまご！", "たまごをタッチ...", "うごいた！", "もうすぐ！",

    "ずかん %u/151",

    "%s%s Lv.%u",

    "%s とおわかれ？", "はい", "いいえ",

    "わざ %u", "ちから +%u", "しんきろく！", "きろく: %u", "れんだ！",

    "スコア: %u", "たのしい！", "+ごきげん",

    "じかんせってい", "じ", "ふん", "うえスワイプ: もどる", "ことば",

    "メダル！", "すごい！", "%u にちれんぞく！",

    "れんぞく %u  きろく %u", "なかよし", "きのみ ???",

    "%s   %luにち", "なまえをタッチ: へんこう",

    "バトル", "こうげき", "ぼうぎょ", "すばやさ", "たいじゅう",

    "メダル %d/%d", "タッチ: もどる",

    "なまえ:", "タッチでもどる",

    "ごはん", "ごきげん", "げんき", "きれい",

    "きろく %u",

    "せいちょう", "Lv.%u", "あと%uふんで Lv.%u", "しんか", "さいしゅうけい",

    "しんかできる！", "ぜんぶ40でしんか",

    "%u Lv.でしんか", "おせわミス: %u",

    "しんか", "%sがなにかいいたそう...", "さびしがっている...",

    "しんかする？", "このまま", "おわかれする？", "さようなら", "いっしょにいる",

    "さいしょのポケモンをえらぶ",

    "スプライトなし", "SDにいれてください",
    // pasos y mochila (v1.20)
    "さんぽ", "%lu ほ", "ごうけい %lu", "みつけた %u/%u", "リュック", "センサーなし",
    "もくひょう！", "みつけた！", "リュックがいっぱい", "%sをつかう？", "おまもり: いろちがい", "からっぽ: さんぽしよう！",
    "ぷんぷん！", "くらくら...",
    "きんのみ", "プロテイン", "アイアン", "カーボス", "ひかるおまもり", "ふしぎなアメ", "たまご", "ケーキ", "ふっかつ", "モンスターボール",
    "あかいきのみ", "あおいきのみ", "みどりのきのみ", "きいろいきのみ", "ピンクのきのみ", "オレンジのきのみ",  // v3.2
    "ルアー",  // v3.14: el Cebo
    // combate (v1.24)
    "れんしゅう", "こうかは ばつぐんだ！", "こうかは いまひとつ...", "こうかが ない...", "きゅうしょに あたった！", "ひっさつ！",
    "タッチで ためる！", "サドンデス！", "はんどう！", "かった！", "まけた...", "ひきわけ", "ごきげん +%u", "ごほうびは 1じかんに 1かい",
    // efectos (v1.30)
    "レベルアップ！",
    // la caja (v1.37)
    "ボックス %u/%u", "%sを あずける？", "%sを おこす？", "%sは ボックスへ", "いま: %s", "あき", "こわれている",
    // combate (v1.38)
    "タッチ",
    // huevos limitados (v1.39)
    "たまごなし", "あるいて たまごを さがそう！", "みぎへ スワイプ: ボックス", "たまごなし: いえが からっぽに", "いえが あくと たまごが かえる",
    // huevos al acabar una crianza (v1.40/v1.41)
    "たまごを かえす (%u)", "あたらしい たまごが くるよ", "たまごが まってるよ！", "プレゼント！",
    // la cascada (v1.43)
    "ゲージを みどりに たもとう！", "ながおしで しゅうちゅう", "しゅうちゅう %u.%uびょう", "きろく %u.%uびょう", "ぼうぎょ +%u", "うんちなし！",
    // la carrera en trineo (v1.44)
    "あそぶ ときの もちかたで", "うごかさないで...", "かたむけて よけよう", "よけた: %u", "すばやさ +%u",
    // v1.47: bonificacion de la cascada, entrenamiento al tope
    "ボーナス +%u", "ちから MAX", "ぼうぎょ MAX", "すばやさ MAX",
    // v1.49: la barra fina de la ficha de combate
    "ほそい バー: とっくん",
    // v1.53: la mochila con su boton: como se usa y dormido solo se mira
    "タッチして つかう", "ねてる: みるだけ",
    // v1.54: la tarta: lleno durante 3 h, y tocarla sin ninguna
    "ケーキなし: あるこう",
    // v1.56: el botin de un combate ganado, que espera envuelto en casa
    "いえに プレゼント!",
    // v1.57: al evolucionar cambio la baya favorita (vuelve a ser un secreto)
    "このみが かわった!",
    // v1.59: la pista del Revivir en la mochila (no se usa a mano)
    "しにかけた なかまを たすける",
    // v1.60: muerte por abandono: sin vinculo, el dialogo, el huevo que deja, revivido
    "とても よわっている!", "%sが しにそう...", "ふっかつを つかう? (%u)", "ふっかつが ない", "OK", "たまごを のこした...", "ふっかつ!",
    // v1.63: tiene sueño
    "ねむそう...",
    // v1.64: la hora del Revivir
    "のこり %u ふん", "%sは しんでしまった...", "ふっかつが まにあわなかった",
    // stages
    "ステージ", "バッジ", "チームをえらぶ", "スタート", "ボス", "ロックちゅう", "ステージクリア!", "まけた...",
    "つぎのあいて", "いけ、%s!", "あたらしいバッジ!", "Lv.%u-%u", "チーム: +1じかん、+なかよし",
    "ごほうびあり", "ステージ%u",
    // el patio (v2.0)
    "ショップ", "かう", "つかう", "かったよ!", "おせわ", "あそぶ", "バッグ",
    // el huevo del patio (v2.0, Y3)
    "ゆらして!",
    // la tienda de la mochila y los fondos (v2.0, Y4)
    "どうぐ", "はいけい", "%sをかう?", "コインがたりない", "バッグがいっぱい",
    "そうげん", "さいせきじょう", "もり", "ビーチ", "いせき",
    "かざん", "はつでんしょ", "そらのとう", "どくのまち",
    "つかいちゅう", "SDカードにない", "タップでかう",
    // los viajes del patio (v2.0, Y5; v3.15 9 mapas): 45 recuerdos (5 por mapa; el 5.o es el raro) y 17 sucesos del camino
    "よつばのクローバー", "デイジーのかんむり", "はね", "はちみつのびん", "きんのどんぐり",
    "すべすべのいし", "すいしょうのかけら", "かいのかせき", "てつこうせき", "つきのいし",
    "まつぼっくり", "きのこ", "フクロウのはね", "こはくのしずく", "こだいのは",
    "かいがら", "ヒトデ", "シーグラス", "ボトルレター", "しんじゅ",
    "ふるいコイン", "どろのおめん", "ろうそくのかけら", "ルーンのいた", "れいのランタン",
    "ひのこ", "こくようせき", "マグマのいし", "ほのおのはね", "フェニックスのひ",
    "でんち", "じしゃく", "どうのコイル", "かみなりのいし", "プラズマだま",
    "はやてのはね", "くものかけら", "ふうりん", "そらのしんじゅ", "ほしのかけら",
    "はぐるま", "ヘドロのびん", "ガスマスク", "どくのけっしょう", "さびたかんむり",
    "やせいの%sにあった", "きのしたでひるね", "きらきらしたものをみつけた", "みずでバシャバシャ", "おおきないわにのぼった",
    "へんなおとがきこえた", "ちょうちょをおいかけた", "ちいさなあなをほった", "ほしをながめた", "やせいの%sときのみをわけた",
    "すなのおしろをつくった", "くらいろうかをたんけん", "おやつをたべた",
    "ようがんであたたまった", "パチッとしびれた", "かぜにのってとんだ", "スモッグでいきをとめた",
    // viajes: pantallas (v2.0, Y5b)
    "たび", "たびのさいちゅう", "とちゅう...", "%sがかえってきた！", "るす", "ゴー", "ふくろがいっぱい", "あたらしい", "%sにはっけん", "コレクション", "ボックスにあずけてね\nたびにだせるよ", "とけいをあわせてね", "ふつう", "ちょっとめずらしい", "レア", "%sはたびのなか",
    // encuentros salvajes (v2.2)
    "かったやせいにつかう", "やせいの%sがあらわれた!", "タッチでたたかう", "たたかうポケモンをえらぶ", "やせい", "かった!", "にげられた...", "+1じかんぶんせいちょう", "ボールをなげる?", "のこり%u", "なげる", "%sをつかまえた!", "タマゴはバッグに", "%sはさっていった", "%sはにげた!", "ボールがない", "タマゴがいっぱい", "つかまえたタマゴ!",
    // dos bichos en casa (v3.0): la caja pregunta cual
    "どちらを ボックスへ?", "どちらと こうかん?",
    // el menu del bicho, la lista de comida y el selector (v3.3)
    "ねる", "おこす", "ごはん", "ようす", "しんか", "おわかれ", "もどる",
    "きのみ", "あか", "あお", "みどり", "きいろ", "ピンク", "オレンジ", "ケーキ", "きんいろ",
    "だれが あそぶ?", "だれに あげる?",
    // las medallas del jugador (v3.4): los tres tramos, y la 2.a pagina de la ficha
    "ブロンズ", "シルバー", "ゴールド", "のうりょく",
    // v3.14: el Cebo
    "やせいを よびよせる", "もう やせいが いる", "いまは だれも たたかえない",
    "タップして！",
    // v3.20 HOME: la pagina Casa de la tienda (titulo, 3 lineas, 9 niveles, 3 efectos, MAX, comprar)
    "ホーム", "ベッド", "ジム", "おもちゃ",
    "わらのベッド", "クッションベッド", "ごうかベッド", "まるたのおもり", "ダンベル", "フルジム", "ゴムボール", "ひっぱりなわ", "ブランコ",
    "ねるとエネルギー +%u/ふん", "きたえる じょうげん %u", "あそぶ: うれしさ +%u", "MAX", "%s を %u で かう？",
  },
  // ---------------- KO ----------------
  // Los especificadores van en el MISMO ORDEN que en la fila inglesa aunque
  // el coreano natural los colocaria de otra manera (p. ej. S_NEXT_LVL_FMT):
  // T(id) se rellena con varargs en un orden fijo en cada llamada, asi que
  // reordenar %u/%s aqui sin tocar el punto de llamada intercambiaria en
  // silencio que valor cae en que hueco.
  {
    "냠냠!", "마음에 들어해요!", "배고파요!", "목욕이 필요해요!",
    "지쳤어요...", "슬퍼해요...",
    "고마워요! 안녕", "세상을 떠났어요...", "잘 가! 손을 흔들며...",
    "알", "전설의 알!?", "희귀한 알!", "알을 톡톡...", "움직여요!", "거의 다 됐어요!",
    "도감 %u/151",
    "%s%s Lv.%u",
    "%s 놓아줄까요?", "예", "아니오",
    "%u회 명중", "힘 +%u", "신기록!", "최고: %u", "빠르게!",
    "점수: %u", "정말 즐거워요!", "+행복",
    "시간 설정", "시", "분", "위로 스와이프: 취소", "언어",
    "메달!", "최고예요!", "%u일 연속!",
    "연속 %u  최고 %u", "유대감", "열매 ???",
    "%s   나이 %lud", "이름 탭: 이름변경",
    "배틀", "공격", "방어", "속도", "무게",
    "메달 %d/%d", "탭: 뒤로",
    "이름:", "탭하면 뒤로",
    "포만", "기분", "기력", "청결",
    "최고 %u",
    "진행도", "Lv.%u", "%u분 후 Lv.%u", "진화", "최종 형태",
    "진화 준비 완료!", "진화하려면 전부 40 이상",
    "%u레벨 이후에 진화", "실수: %u",
    "진화!", "%s가 할 말이 있대요...", "버려진 기분이래요...",
    "진화할까요?", "형태 유지", "작별할까요?", "작별", "계속 함께",
    "스타터를 선택하세요",
    "스프라이트 없음", "SD 카드에 넣어주세요",
    // pasos y mochila (v1.20)
    "산책", "%lu 걸음", "누적 %lu", "발견 %u/%u", "배낭", "센서 없음",
    "목표 달성!", "발견!", "배낭이 가득", "%s 사용?", "부적: 이로치 알", "비었어요: 산책하자!",
    "화났어!", "어지러워...",
    "황금열매", "프로테인", "아이언", "카보스", "빛나는부적", "이상한사탕", "알", "케이크", "부활", "몬스터볼",
    "빨간 열매", "파란 열매", "초록 열매", "노란 열매", "분홍 열매", "주황 열매",  // v3.2
    "루어",  // v3.14: el Cebo
    // combate (v1.24)
    "연습", "효과가 굉장했다!", "효과가 별로인 듯...", "효과가 없다...", "급소에 맞았다!", "필살기!",
    "탭해서 모으기!", "서든데스!", "반동!", "이겼다!", "졌다...", "무승부", "기분 +%u", "보상은 1시간에 1번",
    // efectos (v1.30)
    "레벨 업!",
    // la caja (v1.37)
    "박스 %u/%u", "%s 맡길까요?", "%s 깨울까요?", "%s는 박스로", "지금: %s", "빈 칸", "손상됨",
    // combate (v1.38)
    "탭!",
    // huevos limitados (v1.39)
    "알 없음", "걸어서 알을 찾자!", "오른쪽으로 밀기: 박스", "알 없음: 집이 비어요", "집이 비면 알이 부화해요",
    // huevos al acabar una crianza (v1.40/v1.41)
    "알 부화하기 (%u)", "새 알이 와요", "알이 기다려요!", "선물!",
    // la cascada (v1.43)
    "게이지를 초록색에 유지!", "길게 눌러 집중", "집중 %u.%u초", "최고 %u.%u초", "방어 +%u", "똥 없음!",
    // la carrera en trineo (v1.44)
    "플레이할 때처럼 잡아요", "잠깐 가만히...", "기울여서 피하기", "피한 수: %u", "속도 +%u",
    // v1.47: bonificacion de la cascada, entrenamiento al tope
    "보너스 +%u", "힘 MAX", "방어 MAX", "속도 MAX",
    // v1.49: la barra fina de la ficha de combate
    "얇은 막대: 훈련",
    // v1.53: la mochila con su boton: como se usa y dormido solo se mira
    "눌러서 사용", "자는 중: 보기만",
    // v1.54: la tarta: lleno durante 3 h, y tocarla sin ninguna
    "케이크 없음: 걷기",
    // v1.56: el botin de un combate ganado, que espera envuelto en casa
    "집에 선물!",
    // v1.57: al evolucionar cambio la baya favorita (vuelve a ser un secreto)
    "입맛이 바뀌었다!",
    // v1.59: la pista del Revivir en la mochila (no se usa a mano)
    "죽어가는 친구를 구한다",
    // v1.60: muerte por abandono: sin vinculo, el dialogo, el huevo que deja, revivido
    "너무 약해요!", "%s가 죽어가요...", "부활을 쓸까요? (%u)", "부활이 없어요", "확인", "알을 남겼어요...", "부활했어요!",
    // v1.63: tiene sueño
    "졸려요...",
    // v1.64: la hora del Revivir
    "%u분 남았어요", "%s가 죽었어요...", "부활이 늦었어요",
    // stages
    "스테이지", "배지", "팀을 고르세요", "시작", "보스", "잠김", "스테이지 클리어!", "패배...",
    "다음 상대", "가자, %s!", "새 배지!", "Lv.%u-%u", "팀: +1시간, +유대",
    "보상 준비됨", "스테이지 %u",
    // el patio (v2.0)
    "상점", "구매", "사용", "구매 완료!", "돌보기", "놀기", "가방",
    // el huevo del patio (v2.0, Y3)
    "흔들어 줘!",
    // la tienda de la mochila y los fondos (v2.0, Y4)
    "물건", "배경", "%s 살까요?", "코인이 부족해요", "가방이 가득 찼어요",
    "초원", "채석장", "숲", "해변", "유적",
    "화산", "발전소", "하늘 탑", "독성 지구",
    "사용 중", "SD 카드에 없음", "눌러서 구매",
    // los viajes del patio (v2.0, Y5; v3.15 9 mapas): 45 recuerdos (5 por mapa; el 5.o es el raro) y 17 sucesos del camino
    "네잎클로버", "데이지 화관", "깃털", "꿀단지", "황금 도토리",
    "매끈한 조약돌", "석영 조각", "화석 조개", "철광석", "월석",
    "솔방울", "버섯", "부엉이 깃털", "호박 방울", "고대의 잎",
    "조개껍데기", "불가사리", "바다 유리", "유리병 편지", "진주",
    "옛 동전", "점토 가면", "양초 토막", "룬 석판", "영혼의 등불",
    "불씨", "흑요석", "마그마 돌", "불꽃 깃털", "불사조 불씨",
    "건전지", "자석", "구리 코일", "천둥의 돌", "플라스마 구슬",
    "질풍 깃털", "구름 조각", "풍경", "하늘 진주", "별 조각",
    "톱니바퀴", "오물 병", "방독면", "독 수정", "녹슨 왕관",
    "야생 %s을(를) 만났어요", "나무 아래서 낮잠을 잤어요", "반짝이는 것을 찾았어요", "물에서 첨벙거렸어요", "큰 바위를 올랐어요",
    "이상한 소리를 들었어요", "나비를 쫓았어요", "작은 구멍을 팠어요", "별을 바라봤어요", "야생 %s와(과) 열매를 나눴어요",
    "모래성을 쌓았어요", "어두운 복도를 탐험했어요", "간식을 먹으려 쉬었어요",
    "용암 옆에서 몸을 녹였어요", "찌릿한 불꽃을 느꼈어요", "돌풍을 타고 날았어요", "스모그 속에서 숨을 참았어요",
    // viajes: pantallas (v2.0, Y5b)
    "여행", "여행 중", "가는 중...", "%s 돌아왔어요!", "외출", "출발", "가방이 가득", "새로운", "%s에 발견", "수집", "상자에 넣어 두세요\n여행을 보낼 수 있어요", "시계를 맞춰 주세요", "흔함", "드묾", "레어", "%s 여행 중",
    // encuentros salvajes (v2.2)
    "이긴 야생에게 사용", "야생 %s 등장!", "터치해서 싸우기", "싸울 포켓몬 선택", "야생", "이겼다!", "도망쳤다...", "+1시간 성장", "볼을 던질까?", "남은 볼 %u", "던지기", "%s 잡았다!", "알은 가방에", "%s 떠났다", "%s 도망쳤다!", "볼이 없다", "알 자리가 없다", "잡은 알!",
    // dos bichos en casa (v3.0): la caja pregunta cual
    "어느 쪽을 박스에?", "어느 쪽과 교환?",
    // el menu del bicho, la lista de comida y el selector (v3.3)
    "재우기", "깨우기", "먹이", "상태", "진화", "작별", "뒤로",
    "열매", "빨강", "파랑", "초록", "노랑", "분홍", "주황", "케이크", "황금",
    "누가 놀까?", "누구에게?",
    // las medallas del jugador (v3.4): los tres tramos, y la 2.a pagina de la ficha
    "브론즈", "실버", "골드", "능력치",
    // v3.14: el Cebo
    "야생을 불러낸다", "이미 야생이 있다", "지금은 싸울 수 없다",
    "공을 터치!",
    // v3.20 HOME: la pagina Casa de la tienda (titulo, 3 lineas, 9 niveles, 3 efectos, MAX, comprar)
    "집", "침대", "운동기구", "장난감",
    "짚 침대", "쿠션 침대", "왕실 침대", "통나무 역기", "덤벨", "풀 헬스장", "고무공", "줄다리기 밧줄", "그네",
    "수면 에너지 +%u/분", "최대 %u까지 훈련", "놀이: 기쁨 +%u", "MAX", "%s을(를) %u에 살까요?",
  },
};

// Las medallas del jugador (v3.4, medals.h) [idioma][MD_*]: el nombre (filas de la pagina de medallas y
// el banner) y lo que mide (su tarjeta). Sin acentos (como las de antes).
static const char *const MEDAL_NAME[LANG_COUNT][MEDAL_COUNT] = {
  { "Creciendo", "Veterano", "Gran criador", "Mejores amigos", "Adulto", "En forma", "Gourmet", "Evolucion", "Feliz adios", "Pokedex", "Shiny", "Cuidado diario", "Caminante", "Maraton", "Campeon", "Domador", "Cazador", "Trotamundos", "Coleccionista" },  // ES
  { "Growing up", "Seasoned", "Master raiser", "Best friends", "Full grown", "In shape", "Gourmet", "Evolution", "Happy farewell", "Pokedex", "Shiny", "Daily care", "Walker", "Marathon", "Stage champion", "Wild tamer", "Catcher", "Globetrotter", "Collector" },  // EN
  { "Grandir", "Aguerri", "Grand eleveur", "Meilleurs amis", "Adulte", "En forme", "Gourmet", "Evolution", "Doux adieu", "Pokedex", "Chromatique", "Soins du jour", "Marcheur", "Marathon", "Champion", "Dompteur", "Attrapeur", "Globe-trotter", "Collection" },  // FR
  { "Aufwachsen", "Erfahren", "Meisterzucht", "Beste Freunde", "Ausgewachsen", "Fit", "Feinschmecker", "Entwicklung", "Abschied", "Pokedex", "Schillernd", "Pflege", "Wanderer", "Marathon", "Champion", "Wildbaendiger", "Faenger", "Weltenbummler", "Sammler" },  // DE
  { "Crescere", "Esperto", "Allevatore", "Migliori amici", "Adulto", "In forma", "Buongustaio", "Evoluzione", "Addio felice", "Pokedex", "Cromatico", "Cura diaria", "Camminatore", "Maratona", "Campione", "Domatore", "Catturatore", "Giramondo", "Collezionista" },  // IT
  { "Crescendo", "Veterano", "Grande criador", "Grandes amigos", "Adulto", "Em forma", "Gourmet", "Evolucao", "Adeus feliz", "Pokedex", "Brilhante", "Cuidado diario", "Caminhante", "Maratona", "Campeao", "Domador", "Cacador", "Viajante", "Colecionador" },  // PT
  { "せいちょう", "ベテラン", "マスター", "なかよし", "おとな", "げんき", "グルメ", "しんか", "おわかれ", "ずかん", "いろちがい", "まいにち", "さんぽ", "マラソン", "チャンピオン", "やせいハンター", "ゲット", "たびびと", "コレクター" },  // JA
  { "성장", "베테랑", "마스터", "단짝", "어른", "건강", "미식가", "진화", "작별", "도감", "이로치", "매일 돌봄", "산책", "마라톤", "챔피언", "야생 조련사", "포획", "여행가", "수집가" },  // KO
};


static const char *const MEDAL_DESC[LANG_COUNT][MEDAL_COUNT] = {
  { "Bichos al Nv.10", "Bichos al Nv.25", "Bichos al Nv.50", "Bichos con vinculo 100", "Bichos en forma final", "Nv.10 sin peso ni fallos", "Bayas favoritas halladas", "Evoluciones", "Despedidas", "Especies registradas", "Especies shiny", "Mejor racha (dias)", "Pasos en total", "Dias de 10000 pasos", "Estadios vencidos", "Salvajes vencidos", "Capturas", "Viajes completados", "Recuerdos" },  // ES
  { "Pets to Lv.10", "Pets to Lv.25", "Pets to Lv.50", "Pets at bond 100", "Pets in a final form", "Fit at Lv.10, no slips", "Favourite berries found", "Evolutions", "Farewells", "Species registered", "Shiny species", "Best streak (days)", "Total steps", "10000-step days", "Stages beaten", "Wild wins", "Captures", "Trips completed", "Souvenirs" },  // EN
  { "Pokemon au Niv.10", "Pokemon au Niv.25", "Pokemon au Niv.50", "Lien a 100", "Forme finale", "Niv.10, poids 0, 0 faute", "Baies favorites", "Evolutions", "Adieux", "Especes vues", "Especes chromatiques", "Meilleure serie (j)", "Pas au total", "Jours a 10000 pas", "Arenes battues", "Sauvages battus", "Captures", "Voyages finis", "Souvenirs" },  // FR
  { "Pokemon auf Lv.10", "Pokemon auf Lv.25", "Pokemon auf Lv.50", "Bindung 100", "Endform", "Fit auf Lv.10", "Lieblingsbeeren", "Entwicklungen", "Abschiede", "Arten registriert", "Schillernde Arten", "Beste Serie (Tage)", "Schritte gesamt", "Tage mit 10000", "Arenen besiegt", "Wilde besiegt", "Faenge", "Reisen beendet", "Andenken" },  // DE
  { "Pokemon al Lv.10", "Pokemon al Lv.25", "Pokemon al Lv.50", "Legame a 100", "Forma finale", "In forma al Lv.10", "Bacche preferite", "Evoluzioni", "Addii", "Specie registrate", "Specie cromatiche", "Serie migliore (gg)", "Passi totali", "Giorni da 10000 passi", "Palestre vinte", "Selvatici battuti", "Catture", "Viaggi completati", "Souvenir" },  // IT
  { "Pokemon ao Nv.10", "Pokemon ao Nv.25", "Pokemon ao Nv.50", "Laco a 100", "Forma final", "Em forma no Nv.10", "Bagas favoritas", "Evolucoes", "Despedidas", "Especies registradas", "Especies brilhantes", "Melhor sequencia (d)", "Passos no total", "Dias de 10000 passos", "Ginasios vencidos", "Selvagens vencidos", "Capturas", "Viagens feitas", "Lembrancas" },  // PT
  { "Lv.10 にした数", "Lv.25 にした数", "Lv.50 にした数", "なかよし 100", "さいしゅうしんか", "Lv.10 で げんき", "すきな きのみ", "しんか かいすう", "おわかれ かいすう", "とうろく した数", "いろちがい の数", "さいこう れんぞく", "ぜんぶの ほすう", "10000ぽ の日", "かった ステージ", "やせいに かった数", "ゲット した数", "りょこう かいすう", "おみやげ" },  // JA
  { "Lv.10 달성 수", "Lv.25 달성 수", "Lv.50 달성 수", "유대 100", "최종 진화", "Lv.10 건강", "좋아하는 열매", "진화 횟수", "작별 횟수", "등록한 종", "이로치 종", "최고 연속 일수", "총 걸음", "10000걸음 날", "이긴 스테이지", "이긴 야생", "포획 수", "여행 횟수", "기념품" },  // KO
};

// Nombres de las 8 fases (v1.65) [idioma][fase]: planta, agua, roca, fuego, electrico, fantasma, veneno, dragon.
static const char *const STAGE_NAME[LANG_COUNT][STAGE_NAME_COUNT] = {
  { "Planta", "Agua", "Roca", "Fuego", "El\202ctrico", "Fantasma", "Veneno", "Drag\242n" },
  { "Grass", "Water", "Rock", "Fire", "Electric", "Ghost", "Poison", "Dragon" },
  { "Plante", "Eau", "Roche", "Feu", "Electrique", "Fant\223me", "Poison", "Dragon" },
  { "Pflanze", "Wasser", "Gestein", "Feuer", "Elektro", "Geist", "Gift", "Drache" },
  { "Erba", "Acqua", "Roccia", "Fuoco", "Elettro", "Spettro", "Veleno", "Drago" },
  { "Planta", "Agua", "Pedra", "Fogo", "Eletrico", "Fantasma", "Veneno", "Dragao" },
  { "くさ", "みず", "いわ", "ほのお", "でんき", "ゴースト", "どく", "ドラゴン" },
  { "풀", "물", "바위", "불", "전기", "고스트", "독", "드래곤" },
};

const char *T(StrId id) { return STRINGS[gLang][id]; }
const char *medalName(int i) { return MEDAL_NAME[gLang][i >= 0 && i < MEDAL_COUNT ? i : 0]; }
const char *medalDesc(int i) { return MEDAL_DESC[gLang][i >= 0 && i < MEDAL_COUNT ? i : 0]; }
const char *stageName(uint8_t i) { return STAGE_NAME[gLang][i < STAGE_NAME_COUNT ? i : 0]; }

void loadLang() {
  Preferences p;
  p.begin("tamapoke", true);  // solo lectura
  uint8_t v = p.getUChar("lang", LANG_DEFAULT);
  p.end();
  gLang = (v < LANG_COUNT) ? (Lang)v : LANG_DEFAULT;
}

void setLang(Lang l) {
  if (l >= LANG_COUNT) return;
  gLang = l;
  Preferences p;
  p.begin("tamapoke", false);
  p.putUChar("lang", (uint8_t)l);
  p.end();
}
