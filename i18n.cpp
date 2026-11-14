#include "i18n.h"
#include "pet.h"        // MED_COUNT
#include <Preferences.h>

Lang gLang = LANG_DEFAULT;

// Tabla de cadenas [idioma][id]. Sin acentos ni enes: la fuente bitmap del
// firmware no los tiene (por eso el espanol ya iba "Esta", "bano", etc.).
static const char *const STRINGS[LANG_COUNT][STR_COUNT] = {
  // ---------------- ES ----------------
  {
    "Est\240 evolucionando!", "Nam nam!", "Le gusta!", "Tiene hambre!", "Necesita un ba\244o!",
    "Est\240 agotado...", "Est\240 triste...", "Est\240 rellenito...", "Es SHINY!!", "Est\240 feliz",
    "GRACIAS! Hasta siempre", "Se ha escapado...", "Adi\242s! Se despide...",
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
    "EVOLUCIONAR", "%s quiere decirte algo...", "%s se siente abandonado...",
    "Evolucionar?", "Mantener forma", "Despedirse?", "Despedirse", "Quedaros juntos",
    "Elige tu inicial",
    "Sin sprites", "C\240rgalos en la SD",
    // pasos y mochila (v1.20)
    "PASEO", "%lu PASOS", "TOTAL %lu", "HALLAZGOS %u/%u", "MOCHILA", "Sin sensor de pasos",
    "META!", "HALLAZGO!", "Mochila llena", "Usar %s?", "Amuleto: huevo shiny", "Vac\241a: sal a pasear!",
    "Est\240 enfadado!", "Qu\202 mareo...",
    "BAYA DORADA", "PROTEINA", "HIERRO", "CARBURANTE", "AMULETO IRIS", "CARAMELO RARO", "HUEVO", "TARTA",
    // combate (v1.24)
    "ENTRENAMIENTO", "Es muy eficaz!", "No es muy eficaz...", "No le afecta...", "Golpe cr\241tico!", "Especial!",
    "Toca para cargar!", "GANASTE!", "PERDISTE...", "EMPATE", "Felicidad +%u", "Premio: una vez por hora",
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
    "Est\240 lleno!", "Sin tarta: camina o combate",
    // v1.56: el botin de un combate ganado, que espera envuelto en casa
    "Regalo en casa!",
  },
  // ---------------- EN ----------------
  {
    "Evolving!", "Yum yum!", "It likes it!", "It's hungry!", "Needs a bath!",
    "Worn out...", "Feeling sad...", "A bit chubby...", "It's SHINY!!", "It's happy",
    "THANKS! Farewell", "It ran away...", "Bye! Waving goodbye...",
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
    "EVOLVE!", "%s wants to tell you...", "%s feels abandoned...",
    "Evolve?", "Keep form", "Say goodbye?", "Goodbye", "Stay together",
    "Choose your starter",
    "No sprites", "Load them onto the SD",
    // pasos y mochila (v1.20)
    "WALK", "%lu STEPS", "TOTAL %lu", "FINDS %u/%u", "BACKPACK", "No step sensor",
    "GOAL!", "FOUND!", "Backpack full", "Use %s?", "Charm: shinier egg", "Empty: go for a walk!",
    "Grumpy!", "So dizzy...",
    "GOLDEN BERRY", "PROTEIN", "IRON", "CARBOS", "SHINY CHARM", "RARE CANDY", "EGG", "CAKE",
    // combate (v1.24)
    "PRACTICE", "Super effective!", "Not very effective...", "No effect...", "Critical hit!", "Special!",
    "Tap to charge!", "YOU WIN!", "YOU LOST...", "DRAW", "Joy +%u", "Reward: once an hour",
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
    "So full!", "No cake: walk or battle",
    // v1.56: el botin de un combate ganado, que espera envuelto en casa
    "A gift at home!",
  },
  // ---------------- FR ----------------
  {
    "Il \202volue!", "Miam miam!", "Il aime \207a!", "Il a faim!", "Besoin d'un bain!",
    "\220puis\202...", "Triste...", "Un peu rond...", "C'est SHINY!!", "Il est content",
    "MERCI! Adieu", "Il s'est enfui...", "Au revoir!",
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
    "\220VOLUER", "%s veut te parler...", "%s se sent abandonn\202...",
    "\220voluer?", "Garder forme", "Dire adieu?", "Adieu", "Rester ensemble",
    "Choisis ton starter",
    "Pas de sprites", "Charge-les sur la SD",
    // pasos y mochila (v1.20)
    "BALADE", "%lu PAS", "TOTAL %lu", "TROUV\220S %u/%u", "SAC \205 DOS", "Pas de capteur",
    "OBJECTIF!", "TROUV\220!", "Sac plein", "Utiliser %s?", "Charme: oeuf chroma", "Vide: va te balader!",
    "Grognon!", "Tout \202tourdi...",
    "BAIE DOR\220E", "PROT\220INE", "FER", "CARBONE", "CHARME CHROMA", "SUPER BONBON", "OEUF", "GATEAU",
    // combate (v1.24)
    "ENTRA\214NEMENT", "Super efficace!", "Pas tr\212s efficace...", "Aucun effet...", "Coup critique!", "Sp\202cial!",
    "Tape pour charger!", "GAGN\220!", "PERDU...", "\220GALIT\220", "Bonheur +%u", "Prix: une fois par heure",
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
    "Bien rempli !", "Pas de g\203teau: marche",
    // v1.56: el botin de un combate ganado, que espera envuelto en casa
    "Cadeau \205 la maison!",
  },
  // ---------------- DE ----------------
  // El aleman si lleva dieresis: la fuente 5x7 del firmware es una tabla CP437
  // completa (256 glifos), asi que basta con el byte suelto de cada caracter.
  // Van en octal para no depender de la codificacion del fichero fuente:
  //   \204 a   \224 o   \201 u   \216 A   \231 O   \232 U   \341 ss
  // Un byte por caracter: strlen() sigue midiendo bien y el centrado no cambia.
  {
    "Entwickelt sich!", "Mampf mampf!", "Gef\204llt ihm!", "Hat Hunger!", "Braucht ein Bad!",
    "Ersch\224pft...", "Traurig...", "Etwas rundlich...", "Es ist SHINY!!", "Es ist froh",
    "DANKE! Leb wohl", "Es ist weg...", "Tsch\201ss! Winkt",
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
    "ENTWICKELN", "%s will dir etwas sagen...", "%s f\201hlt sich verlassen...",
    "Entwickeln?", "Form behalten", "Abschied?", "Leb wohl", "Zusammen bleiben",
    "W\204hle deinen Starter",
    "Keine Sprites", "Auf die SD laden",
    // pasos y mochila (v1.20)
    "SPAZIERGANG", "%lu SCHRITTE", "GESAMT %lu", "FUNDE %u/%u", "RUCKSACK", "Kein Sensor",
    "ZIEL!", "GEFUNDEN!", "Rucksack voll", "%s nutzen?", "Pin: Ei schillert", "Leer: geh spazieren!",
    "Grummelig!", "Schwindelig...",
    "GOLDBEERE", "PROTEIN", "EISEN", "CARBON", "SCHILLERPIN", "SONDERBONBON", "EI", "KUCHEN",
    // combate (v1.24)
    "TRAINING", "Sehr effektiv!", "Nicht sehr effektiv...", "Keine Wirkung...", "Volltreffer!", "Spezial!",
    "Tippen zum Laden!", "GEWONNEN!", "VERLOREN...", "UNENTSCHIEDEN", "Laune +%u", "Belohnung: 1x pro Stunde",
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
    "Pappsatt!", "Kein Kuchen: lauf oder k\204mpf",
    // v1.56: el botin de un combate ganado, que espera envuelto en casa
    "Geschenk daheim!",
  },
  // ---------------- IT ----------------
  {
    "Si evolve!", "Gnam gnam!", "Gli piace!", "Ha fame!", "Vuole un bagno!",
    "Esausto...", "Triste...", "Un po' cicciotto...", "E' SHINY!!", "E' felice",
    "GRAZIE! Addio", "E' scappato...", "Ciao! Saluta",
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
    "EVOLVI", "%s vuole dirti qualcosa...", "%s si sente abbandonato...",
    "Evolvere?", "Mantieni forma", "Salutare?", "Addio", "Restare insieme",
    "Scegli l'iniziale",
    "Senza sprite", "Caricali sulla SD",
    // pasos y mochila (v1.20)
    "PASSEGGIATA", "%lu PASSI", "TOTALE %lu", "TROVATI %u/%u", "ZAINO", "Nessun sensore",
    "OBIETTIVO!", "TROVATO!", "Zaino pieno", "Usare %s?", "Amuleto: uovo shiny", "Vuoto: fai due passi!",
    "Arrabbiato!", "Che capogiro...",
    "BACCA D'ORO", "PROTEINA", "FERRO", "CARBURANTE", "CROMAMULETO", "CARAMELLA RARA", "UOVO", "TORTA",
    // combate (v1.24)
    "ALLENAMENTO", "Superefficace!", "Non \212 molto efficace...", "Nessun effetto...", "Brutto colpo!", "Speciale!",
    "Tocca per caricare!", "HAI VINTO!", "HAI PERSO...", "PAREGGIO", "Felicit\205 +%u", "Premio: una volta l'ora",
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
    "Che sazio!", "Niente torta: cammina",
    // v1.56: el botin de un combate ganado, que espera envuelto en casa
    "Regalo a casa!",
  },
  // ---------------- PT ----------------
  {
    "Evoluindo!", "Nham nham!", "Ele gosta!", "Est\240 com fome!", "Precisa de banho!",
    "Exausto...", "Triste...", "Um pouco gordinho...", "\220 SHINY!!", "Est\240 feliz",
    "OBRIGADO! Adeus", "Fugiu...", "Tchau! Acena",
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
    "EVOLUIR", "%s quer dizer-te algo...", "%s sente-se abandonado...",
    "Evoluir?", "Manter forma", "Despedir?", "Adeus", "Ficar juntos",
    "Escolhe o inicial",
    "Sem sprites", "Carrega-os no SD",
    // pasos y mochila (v1.20)
    "PASSEIO", "%lu PASSOS", "TOTAL %lu", "ACHADOS %u/%u", "MOCHILA", "Sem sensor",
    "META!", "ACHOU!", "Mochila cheia", "Usar %s?", "Amuleto: ovo shiny", "Vazia: vai passear!",
    "Zangado!", "Que tontura...",
    "BAGA DOURADA", "PROTEINA", "FERRO", "CARBOS", "AMULETO BRILHANTE", "DOCE RARO", "OVO", "BOLO",
    // combate (v1.24)
    "TREINO", "Super efetivo!", "Pouco efetivo...", "Sem efeito...", "Golpe cr\241tico!", "Especial!",
    "Toque para carregar!", "VENCEU!", "PERDEU...", "EMPATE", "Alegria +%u", "Pr\210mio: 1 vez por hora",
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
    "T\240 cheio!", "Sem bolo: ande ou lute",
    // v1.56: el botin de un combate ganado, que espera envuelto en casa
    "Presente em casa!",
  },
  // ---------------- JA ----------------
  {
    "しんかした！", "おいしい！", "うれしそう！", "おなかすいた！", "おふろにいれて！",
    "つかれた...", "かなしい...", "ちょっとふとった...", "いろちがい！！", "ごきげん！",
    "ありがとう！ またね", "にげちゃった...", "バイバイ！",

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

    "しんか", "%sがなにかいいたそう...", "%sがさびしがっている...",

    "しんかする？", "このまま", "おわかれする？", "さようなら", "いっしょにいる",

    "さいしょのポケモンをえらぶ",

    "スプライトなし", "SDにいれてください",
    // pasos y mochila (v1.20)
    "さんぽ", "%lu ほ", "ごうけい %lu", "みつけた %u/%u", "リュック", "センサーなし",
    "もくひょう！", "みつけた！", "リュックがいっぱい", "%sをつかう？", "おまもり: いろちがい", "からっぽ: さんぽしよう！",
    "ぷんぷん！", "くらくら...",
    "きんのみ", "プロテイン", "アイアン", "カーボス", "ひかるおまもり", "ふしぎなアメ", "たまご", "ケーキ",
    // combate (v1.24)
    "れんしゅう", "こうかは ばつぐんだ！", "こうかは いまひとつ...", "こうかが ない...", "きゅうしょに あたった！", "ひっさつ！",
    "タッチで ためる！", "かった！", "まけた...", "ひきわけ", "ごきげん +%u", "ごほうびは 1じかんに 1かい",
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
    "おなか いっぱい!", "ケーキなし: あるこう",
    // v1.56: el botin de un combate ganado, que espera envuelto en casa
    "いえに プレゼント!",
  },
  // ---------------- KO ----------------
  // Los especificadores van en el MISMO ORDEN que en la fila inglesa aunque
  // el coreano natural los colocaria de otra manera (p. ej. S_NEXT_LVL_FMT):
  // T(id) se rellena con varargs en un orden fijo en cada llamada, asi que
  // reordenar %u/%s aqui sin tocar el punto de llamada intercambiaria en
  // silencio que valor cae en que hueco.
  {
    "진화 중!", "냠냠!", "마음에 들어해요!", "배고파요!", "목욕이 필요해요!",
    "지쳤어요...", "슬퍼해요...", "조금 통통해요...", "이로치예요!!", "행복해요",
    "고마워요! 안녕", "가출했어요...", "잘 가! 손을 흔들며...",
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
    "진화!", "%s가 할 말이 있대요...", "%s가 버려진 기분이래요...",
    "진화할까요?", "형태 유지", "작별할까요?", "작별", "계속 함께",
    "스타터를 선택하세요",
    "스프라이트 없음", "SD 카드에 넣어주세요",
    // pasos y mochila (v1.20)
    "산책", "%lu 걸음", "누적 %lu", "발견 %u/%u", "배낭", "센서 없음",
    "목표 달성!", "발견!", "배낭이 가득", "%s 사용?", "부적: 이로치 알", "비었어요: 산책하자!",
    "화났어!", "어지러워...",
    "황금열매", "프로테인", "아이언", "카보스", "빛나는부적", "이상한사탕", "알", "케이크",
    // combate (v1.24)
    "연습", "효과가 굉장했다!", "효과가 별로인 듯...", "효과가 없다...", "급소에 맞았다!", "필살기!",
    "탭해서 모으기!", "이겼다!", "졌다...", "무승부", "기분 +%u", "보상은 1시간에 1번",
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
    "배불러!", "케이크 없음: 걷기",
    // v1.56: el botin de un combate ganado, que espera envuelto en casa
    "집에 선물!",
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

const char *T(StrId id) { return STRINGS[gLang][id]; }
const char *medalName(int i)  { return MED_NAME[gLang][i]; }
const char *medalLabel(int i) { return MED_LBL[gLang][i]; }
const char *medalDesc(int i)  { return MED_DSC[gLang][i]; }

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
