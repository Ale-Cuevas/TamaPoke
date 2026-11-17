#include "i18n.h"
#include "pet.h"        // MED_COUNT
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
    "RACHA %u  rec %u", "VIN", "BAYA ???", "BAYA ROJA", "BAYA AZUL", "BAYA VERDE",
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
    "BAYA DORADA", "PROTEINA", "HIERRO", "CARBURANTE", "AMULETO IRIS", "CARAMELO RARO", "HUEVO", "TARTA", "REVIVIR",
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
    "Manten la barra en el verde!", "MANTEN PULSADO", "FOCO %u.%us", "RECORD %u.%us", "DEFENSA +%u", "Limpito!",
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
    "En uso", "No esta en la SD", "Toca para comprar",
    // los viajes del patio (v2.0, Y5): 25 recuerdos (5 por mapa; el 5.o es el raro) y 13 sucesos del camino
    "Tr\202bol de cuatro hojas", "Corona de margaritas", "Pluma", "Tarro de miel", "Bellota dorada",
    "Guijarro liso", "Trozo de cuarzo", "Concha f\242sil", "Mineral de hierro", "Piedra lunar",
    "Pi\244a", "Seta", "Pluma de b\243ho", "Gota de \240mbar", "Hoja ancestral",
    "Caracola", "Estrella de mar", "Vidrio marino", "Botella con mensaje", "Perla",
    "Moneda antigua", "M\240scara de barro", "Cabo de vela", "Tablilla de runas", "Farol espiritual",
    "Conoci\242 a un %s salvaje", "Durmi\242 la siesta bajo un \240rbol", "Encontr\242 algo brillante", "Chapote\242 en el agua", "Escal\242 una roca grande",
    "Oy\242 un ruido extra\244o", "Persigui\242 una mariposa", "Cav\242 un hoyo peque\244o", "Mir\242 las estrellas", "Comparti\242 bayas con un %s salvaje",
    "Hizo un castillo de arena", "Explor\242 un pasillo oscuro", "Par\242 a merendar",
    // viajes: pantallas (v2.0, Y5b)
    "Viaje", "De viaje", "De camino...", "\255%s ha vuelto!", "fuera", "Ir", "Mochila llena", "NUEVO", "Hallado el %s", "Colecci\242n", "Guarda un bicho en la Caja\npara mandarlo de viaje", "Pon la hora para viajar", "Com\243n", "Poco com\243n", "Raro", "%s est\240 de viaje",
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
    "STREAK %u  best %u", "BOND", "BERRY ???", "RED BERRY", "BLUE BERRY", "GREEN BERRY",
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
    "GOLDEN BERRY", "PROTEIN", "IRON", "CARBOS", "SHINY CHARM", "RARE CANDY", "EGG", "CAKE", "REVIVE",
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
    "Keep the bar in the green!", "HOLD TO FOCUS", "FOCUS %u.%us", "BEST %u.%us", "DEF +%u", "Squeaky clean!",
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
    "In use", "Not on SD card", "Tap to buy",
    // los viajes del patio (v2.0, Y5): 25 recuerdos (5 por mapa; el 5.o es el raro) y 13 sucesos del camino
    "Four-leaf clover", "Daisy crown", "Feather", "Honey jar", "Golden acorn",
    "Smooth pebble", "Quartz shard", "Fossil shell", "Iron ore", "Moon stone",
    "Pine cone", "Mushroom", "Owl feather", "Amber drop", "Ancient leaf",
    "Seashell", "Starfish", "Sea glass", "Message bottle", "Pearl",
    "Old coin", "Clay mask", "Candle stub", "Rune tablet", "Spirit lantern",
    "Met a wild %s", "Took a nap under a tree", "Found something shiny", "Splashed in the water", "Climbed a big rock",
    "Heard a strange noise", "Chased a butterfly", "Dug a small hole", "Watched the stars", "Shared berries with a wild %s",
    "Built a sand castle", "Explored a dark corridor", "Stopped for a snack",
    // viajes: pantallas (v2.0, Y5b)
    "Travel", "Travelling", "On the way...", "%s is back!", "away", "Go", "Bag full", "NEW", "Found on %s", "Collection", "Store a pet in the Box\nto send it travelling", "Set the clock to travel", "Common", "Uncommon", "Rare", "%s is away",
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
    "S\220RIE %u  rec %u", "LIEN", "BAIE ???", "BAIE ROUGE", "BAIE BLEUE", "BAIE VERTE",
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
    "BAIE DOR\220E", "PROT\220INE", "FER", "CARBONE", "CHARME CHROMA", "SUPER BONBON", "OEUF", "GATEAU", "RANIMER",
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
    "Garde la barre dans le vert!", "MAINTIENS APPUYE", "FOCUS %u.%us", "RECORD %u.%us", "DEFENSE +%u", "Tout propre!",
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
    "Utilise", "Pas sur la SD", "Touche: acheter",
    // los viajes del patio (v2.0, Y5): 25 recuerdos (5 por mapa; el 5.o es el raro) y 13 sucesos del camino
    "Trefle a quatre feuilles", "Couronne de marguerites", "Plume", "Pot de miel", "Gland dore",
    "Galet lisse", "Eclat de quartz", "Coquillage fossile", "Minerai de fer", "Pierre de lune",
    "Pomme de pin", "Champignon", "Plume de hibou", "Goutte d'ambre", "Feuille ancienne",
    "Coquillage", "Etoile de mer", "Verre de mer", "Bouteille a la mer", "Perle",
    "Vieille piece", "Masque d'argile", "Bout de bougie", "Tablette runique", "Lanterne spectrale",
    "A rencontre un %s sauvage", "A fait la sieste sous un arbre", "A trouve quelque chose de brillant", "A patauge dans l'eau", "A escalade un gros rocher",
    "A entendu un bruit etrange", "A couru apres un papillon", "A creuse un petit trou", "A regarde les etoiles", "A partage des baies avec un %s sauvage",
    "A fait un chateau de sable", "A explore un couloir sombre", "S'est arrete pour un gouter",
    // viajes: pantallas (v2.0, Y5b)
    "Voyage", "En voyage", "En route...", "%s est de retour !", "absent", "Partir", "Sac plein", "NOUVEAU", "Trouve le %s", "Collection", "Range un bicho dans la Boite\npour l'envoyer en voyage", "Regle l'heure pour voyager", "Commun", "Peu commun", "Rare", "%s est parti",
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
    "SERIE %u  rek %u", "BND", "BEERE ???", "ROTE BEERE", "BLAUE BEERE", "GR\232NE BEERE",
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
    "GOLDBEERE", "PROTEIN", "EISEN", "CARBON", "SCHILLERPIN", "SONDERBONBON", "EI", "KUCHEN", "BELEBER",
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
    "Halte den Balken im Gr\201nen!", "GEDR\232CKT HALTEN", "FOKUS %u.%us", "REKORD %u.%us", "VERT +%u", "Blitzsauber!",
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
    "Aktiv", "Nicht auf SD", "Tippen: kaufen",
    // los viajes del patio (v2.0, Y5): 25 recuerdos (5 por mapa; el 5.o es el raro) y 13 sucesos del camino
    "Kleeblatt", "Gaensebluemchenkranz", "Feder", "Honigglas", "Goldene Eichel",
    "Glatter Kiesel", "Quarzsplitter", "Fossile Muschel", "Eisenerz", "Mondstein",
    "Tannenzapfen", "Pilz", "Eulenfeder", "Bernsteintropfen", "Uraltes Blatt",
    "Muschel", "Seestern", "Meerglas", "Flaschenpost", "Perle",
    "Alte Muenze", "Tonmaske", "Kerzenstummel", "Runentafel", "Geisterlaterne",
    "Traf ein wildes %s", "Machte ein Nickerchen unter einem Baum", "Fand etwas Glitzerndes", "Planschte im Wasser", "Kletterte auf einen Felsen",
    "Hoerte ein seltsames Geraeusch", "Jagte einen Schmetterling", "Grub ein kleines Loch", "Schaute die Sterne an", "Teilte Beeren mit einem wilden %s",
    "Baute eine Sandburg", "Erkundete einen dunklen Gang", "Machte Pause fuer einen Snack",
    // viajes: pantallas (v2.0, Y5b)
    "Reise", "Unterwegs", "Auf dem Weg...", "%s ist zurueck!", "weg", "Los", "Beutel voll", "NEU", "Gefunden am %s", "Sammlung", "Lege ein Tier in die Box,\num es auf Reisen zu schicken", "Stelle die Uhr zum Reisen", "Haeufig", "Selten", "Sehr selten", "%s ist unterwegs",
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
    "SERIE %u  rec %u", "LEG", "BACCA ???", "BACCA ROSSA", "BACCA BLU", "BACCA VERDE",
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
    "BACCA D'ORO", "PROTEINA", "FERRO", "CARBURANTE", "CROMAMULETO", "CARAMELLA RARA", "UOVO", "TORTA", "RIANIMA",
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
    "Tieni la barra nel verde!", "TIENI PREMUTO", "FOCUS %u.%us", "RECORD %u.%us", "DIFESA +%u", "Pulitissimo!",
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
    "In uso", "Non sulla SD", "Tocca per comprare",
    // los viajes del patio (v2.0, Y5): 25 recuerdos (5 por mapa; el 5.o es el raro) y 13 sucesos del camino
    "Quadrifoglio", "Corona di margherite", "Piuma", "Barattolo di miele", "Ghianda dorata",
    "Ciottolo liscio", "Scheggia di quarzo", "Conchiglia fossile", "Minerale di ferro", "Pietra lunare",
    "Pigna", "Fungo", "Piuma di gufo", "Goccia d'ambra", "Foglia antica",
    "Conchiglia", "Stella marina", "Vetro di mare", "Messaggio in bottiglia", "Perla",
    "Moneta antica", "Maschera d'argilla", "Moncone di candela", "Tavola runica", "Lanterna spettrale",
    "Ha incontrato un %s selvatico", "Ha fatto un pisolino sotto un albero", "Ha trovato qualcosa di luccicante", "Ha sguazzato nell'acqua", "Si e arrampicato su una roccia",
    "Ha sentito un rumore strano", "Ha inseguito una farfalla", "Ha scavato una piccola buca", "Ha guardato le stelle", "Ha diviso le bacche con un %s selvatico",
    "Ha costruito un castello di sabbia", "Ha esplorato un corridoio buio", "Si e fermato per uno spuntino",
    // viajes: pantallas (v2.0, Y5b)
    "Viaggio", "In viaggio", "In cammino...", "%s e tornato!", "via", "Vai", "Zaino pieno", "NUOVO", "Trovato il %s", "Collezione", "Metti un mostro nel Box\nper mandarlo in viaggio", "Imposta l'ora per viaggiare", "Comune", "Non comune", "Raro", "%s e in viaggio",
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
    "SEQ %u  rec %u", "LA\200O", "BAGA ???", "BAGA VERMELHA", "BAGA AZUL", "BAGA VERDE",
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
    "BAGA DOURADA", "PROTEINA", "FERRO", "CARBOS", "AMULETO BRILHANTE", "DOCE RARO", "OVO", "BOLO", "REVIVER",
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
    "Mantenha a barra no verde!", "SEGURE O TOQUE", "FOCO %u.%us", "RECORDE %u.%us", "DEFESA +%u", "Limpinho!",
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
    "Em uso", "Nao esta no SD", "Toque para comprar",
    // los viajes del patio (v2.0, Y5): 25 recuerdos (5 por mapa; el 5.o es el raro) y 13 sucesos del camino
    "Trevo de quatro folhas", "Coroa de margaridas", "Pena", "Pote de mel", "Bolota dourada",
    "Seixo liso", "Lasca de quartzo", "Concha fossil", "Minerio de ferro", "Pedra da lua",
    "Pinha", "Cogumelo", "Pena de coruja", "Gota de ambar", "Folha ancestral",
    "Concha", "Estrela-do-mar", "Vidro marinho", "Garrafa com mensagem", "Perola",
    "Moeda antiga", "Mascara de barro", "Toco de vela", "Tabuleta de runas", "Lanterna espiritual",
    "Encontrou um %s selvagem", "Tirou uma soneca debaixo de uma arvore", "Achou algo brilhante", "Chapinhou na agua", "Subiu numa pedra grande",
    "Ouviu um barulho estranho", "Perseguiu uma borboleta", "Cavou um buraquinho", "Olhou as estrelas", "Dividiu frutinhas com um %s selvagem",
    "Fez um castelo de areia", "Explorou um corredor escuro", "Parou para um lanche",
    // viajes: pantallas (v2.0, Y5b)
    "Viagem", "Em viagem", "A caminho...", "%s voltou!", "fora", "Ir", "Mochila cheia", "NOVO", "Achado em %s", "Colecao", "Guarde um bicho na Caixa\npara mandar viajar", "Acerte a hora para viajar", "Comum", "Incomum", "Raro", "%s esta viajando",
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

    "れんぞく %u  きろく %u", "なかよし", "きのみ ???", "あかいきのみ", "あおいきのみ", "みどりのきのみ",

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
    "きんのみ", "プロテイン", "アイアン", "カーボス", "ひかるおまもり", "ふしぎなアメ", "たまご", "ケーキ", "ふっかつ",
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
    "ゲージを みどりに たもとう！", "ながおしで しゅうちゅう", "しゅうちゅう %u.%uびょう", "きろく %u.%uびょう", "ぼうぎょ +%u", "ぴかぴか！",
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
    "つかいちゅう", "SDカードにない", "タップでかう",
    // los viajes del patio (v2.0, Y5): 25 recuerdos (5 por mapa; el 5.o es el raro) y 13 sucesos del camino
    "よつばのクローバー", "デイジーのかんむり", "はね", "はちみつのびん", "きんのどんぐり",
    "すべすべのいし", "すいしょうのかけら", "かいのかせき", "てつこうせき", "つきのいし",
    "まつぼっくり", "きのこ", "フクロウのはね", "こはくのしずく", "こだいのは",
    "かいがら", "ヒトデ", "シーグラス", "ボトルレター", "しんじゅ",
    "ふるいコイン", "どろのおめん", "ろうそくのかけら", "ルーンのいた", "れいのランタン",
    "やせいの%sにあった", "きのしたでひるね", "きらきらしたものをみつけた", "みずでバシャバシャ", "おおきないわにのぼった",
    "へんなおとがきこえた", "ちょうちょをおいかけた", "ちいさなあなをほった", "ほしをながめた", "やせいの%sときのみをわけた",
    "すなのおしろをつくった", "くらいろうかをたんけん", "おやつをたべた",
    // viajes: pantallas (v2.0, Y5b)
    "たび", "たびのさいちゅう", "とちゅう...", "%sがかえってきた！", "るす", "ゴー", "ふくろがいっぱい", "あたらしい", "%sにはっけん", "コレクション", "ボックスにあずけてね\nたびにだせるよ", "とけいをあわせてね", "ふつう", "ちょっとめずらしい", "レア", "%sはたびのなか",
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
    "연속 %u  최고 %u", "유대감", "열매 ???", "빨간 열매", "파란 열매", "초록 열매",
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
    "황금열매", "프로테인", "아이언", "카보스", "빛나는부적", "이상한사탕", "알", "케이크", "부활",
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
    "게이지를 초록색에 유지!", "길게 눌러 집중", "집중 %u.%u초", "최고 %u.%u초", "방어 +%u", "깨끗해졌다!",
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
    "사용 중", "SD 카드에 없음", "눌러서 구매",
    // los viajes del patio (v2.0, Y5): 25 recuerdos (5 por mapa; el 5.o es el raro) y 13 sucesos del camino
    "네잎클로버", "데이지 화관", "깃털", "꿀단지", "황금 도토리",
    "매끈한 조약돌", "석영 조각", "화석 조개", "철광석", "월석",
    "솔방울", "버섯", "부엉이 깃털", "호박 방울", "고대의 잎",
    "조개껍데기", "불가사리", "바다 유리", "유리병 편지", "진주",
    "옛 동전", "점토 가면", "양초 토막", "룬 석판", "영혼의 등불",
    "야생 %s을(를) 만났어요", "나무 아래서 낮잠을 잤어요", "반짝이는 것을 찾았어요", "물에서 첨벙거렸어요", "큰 바위를 올랐어요",
    "이상한 소리를 들었어요", "나비를 쫓았어요", "작은 구멍을 팠어요", "별을 바라봤어요", "야생 %s와(과) 열매를 나눴어요",
    "모래성을 쌓았어요", "어두운 복도를 탐험했어요", "간식을 먹으려 쉬었어요",
    // viajes: pantallas (v2.0, Y5b)
    "여행", "여행 중", "가는 중...", "%s 돌아왔어요!", "외출", "출발", "가방이 가득", "새로운", "%s에 발견", "수집", "상자에 넣어 두세요\n여행을 보낼 수 있어요", "시계를 맞춰 주세요", "흔함", "드묾", "레어", "%s 여행 중",
  },
};

// Nombres de medalla en sus tres longitudes [idioma][medalla].
static const char *const MED_NAME[LANG_COUNT][MED_COUNT] = {
  { "Nv.10", "Nv.25", "Nv.50", "BAYA", "RACHA 7", "VINCULO", "FORMA TOPE", "EN FORMA", "MARATON" },
  { "Lv.10", "Lv.25", "Lv.50", "BERRY", "7 STREAK", "BOND", "TOP FORM", "IN SHAPE", "MARATHON" },
  { "Niv.10", "Niv.25", "Niv.50", "BAIE", "SERIE 7", "LIEN", "FORME MAX", "EN FORME", "MARATHON" },
  { "Lv.10", "Lv.25", "Lv.50", "BEERE", "7 SERIE", "BINDUNG", "ENDFORM", "FIT", "MARATHON" },
  { "Lv.10", "Lv.25", "Lv.50", "BACCA", "SERIE 7", "LEGAME", "FORMA MAX", "IN FORMA", "MARATONA" },
  { "Niv.10", "Niv.25", "Niv.50", "BAGA", "SEQ 7", "LACO", "FORMA MAX", "EM FORMA", "MARATONA" },
  { "Lv.10", "Lv.25", "Lv.50", "きのみ", "7にち", "なかよし", "さいしゅう", "げんき", "マラソン" },
  { "Lv.10", "Lv.25", "Lv.50", "열매", "연속 7일", "유대", "최고 컨디션", "건강함", "마라톤" },
};
static const char *const MED_LBL[LANG_COUNT][MED_COUNT] = {
  { "Nv10", "Nv25", "Nv50", "BAYA", "7DIAS", "VINC", "TOPE", "SANO", "10K" },
  { "Lv10", "Lv25", "Lv50", "BERRY", "7DAYS", "BOND", "TOP", "FIT", "10K" },
  { "Niv10", "Niv25", "Niv50", "BAIE", "7JRS", "LIEN", "MAX", "FORME", "10K" },
  { "Lv10", "Lv25", "Lv50", "BEERE", "7TAGE", "BND", "END", "FIT", "10K" },
  { "Lv10", "Lv25", "Lv50", "BACCA", "7GG", "LEG", "MAX", "FIT", "10K" },
  { "Niv10", "Niv25", "Niv50", "BAGA", "7DIAS", "LACO", "MAX", "FIT", "10K" },
  { "Lv10", "Lv25", "Lv50", "きのみ", "7にち", "なかよし", "しんか", "げんき", "10K" },
  { "Lv10", "Lv25", "Lv50", "열매", "7일", "유대", "최고", "건강", "10K" },
};
static const char *const MED_DSC[LANG_COUNT][MED_COUNT] = {
  { "NIVEL 10", "NIVEL 25", "NIVEL 50", "BAYA HALLADA",
    "RACHA 7 DIAS", "VINCULO MAX", "FORMA FINAL", "EN FORMA", "10000 PASOS" },
  { "LEVEL 10", "LEVEL 25", "LEVEL 50", "BERRY FOUND",
    "7 DAY STREAK", "MAX BOND", "FINAL FORM", "IN SHAPE", "10000 STEPS" },
  { "NIVEAU 10", "NIVEAU 25", "NIVEAU 50", "BAIE TROUVEE",
    "SERIE 7 JOURS", "LIEN MAX", "FORME FINALE", "EN FORME", "10000 PAS" },
  { "LEVEL 10", "LEVEL 25", "LEVEL 50", "BEERE GEFUNDEN",
    "7 TAGE SERIE", "MAX BINDUNG", "ENDFORM", "FIT", "10000 SCHRITTE" },
  { "LIVELLO 10", "LIVELLO 25", "LIVELLO 50", "BACCA TROVATA",
    "SERIE 7 GIORNI", "LEGAME MAX", "FORMA FINALE", "IN FORMA", "10000 PASSI" },
  { "NIVEL 10", "NIVEL 25", "NIVEL 50", "BAGA ACHADA",
    "SEQ 7 DIAS", "LACO MAX", "FORMA FINAL", "EM FORMA", "10000 PASSOS" },
  { "レベル 10", "レベル 25", "レベル 50", "きのみ はっけん",
    "7にち れんぞく", "なかよし MAX", "さいしゅうしんか", "げんきいっぱい", "10000ぽ あるいた" },
  { "레벨 10", "레벨 25", "레벨 50", "열매 발견",
    "연속 7일", "유대감 최대", "최종 진화", "건강함", "10000걸음" },
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
const char *medalName(int i)  { return MED_NAME[gLang][i]; }
const char *medalLabel(int i) { return MED_LBL[gLang][i]; }
const char *medalDesc(int i)  { return MED_DSC[gLang][i]; }
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
