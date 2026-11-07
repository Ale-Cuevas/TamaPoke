#pragma once
#include <Arduino.h>

// Idiomas soportados. La fuente 5x7 de Arduino_GFX es una tabla CP437 completa
// (256 glifos), asi que SI hay acentos, dieresis y enes: van como byte suelto en
// octal (\240 a, \244 n~, \202 e, \204 a"...), nunca en UTF-8, porque la UI centra
// con strlen(t)*6 y un caracter de dos bytes descuadraria las etiquetas.
// No estan en CP437: las mayusculas acentuadas salvo E (\220), ni a~/o~ del portugues.
enum Lang : uint8_t { LANG_ES = 0, LANG_EN, LANG_FR, LANG_DE, LANG_IT, LANG_PT,
                     LANG_JA, LANG_KO, LANG_COUNT };
// idiomas que necesitan una fuente U8g2 y texto UTF-8 en vez de la tabla CP437
#define LANG_IS_CJK(l) ((l) == LANG_JA || (l) == LANG_KO)
#define LANG_DEFAULT LANG_EN  // idioma por defecto: ingles

extern Lang gLang;  // idioma activo (definido en i18n.cpp)

// IDs de cadena. El orden debe coincidir con la tabla STRINGS de i18n.cpp.
enum StrId : uint8_t {
  // estado del bicho (statusMsg)
  S_EVOLVING, S_EATING, S_LIKES, S_HUNGRY, S_NEEDS_BATH,
  S_EXHAUSTED, S_SAD, S_CHUBBY, S_IS_SHINY, S_HAPPY,
  // ceremonias de despedida
  S_FAREWELL, S_RUNAWAY, S_GOODBYE,
  // huevo
  S_EGG_HDR, S_EGG_LEGEND, S_EGG_RARE, S_EGG_TOUCH, S_EGG_MOVES, S_EGG_ALMOST,
  // formatos compartidos
  S_POKEDEX_FMT,   // "POKEDEX %u/151"
  S_NAME_FMT,      // "%s%s Nv.%u"
  // dialogo soltar
  S_RELEASE_FMT, S_YES, S_NO,
  // minijuego y saco
  S_HITS_FMT, S_STR_GAIN_FMT, S_NEW_RECORD, S_RECORD_FMT, S_HIT_FAST,
  S_SCORE_FMT, S_GREAT_JOY, S_PLUS_JOY,
  // reloj / ajustes
  S_SET_TIME, S_HOUR, S_MIN, S_CLOCK_CANCEL, S_LANG_LABEL,
  // celebracion
  S_MEDAL_BANNER, S_GREAT, S_STREAK_DAYS_FMT,
  // ficha: perfil
  S_STREAK_FMT, S_VIN, S_BERRY_UNK, S_BERRY_RED, S_BERRY_BLUE, S_BERRY_GREEN,
  S_INFO_FMT, S_RENAME_HINT,
  // ficha: combate
  S_BATTLE, S_STAT_ATK, S_STAT_DEF, S_STAT_SPE, S_STAT_WGT,
  // ficha: medallas
  S_MEDALS_FMT, S_BACK,
  // teclado y galeria
  S_NAME, S_DETAIL_BACK,
  // barras
  S_BAR_FOOD, S_BAR_JOY, S_BAR_ENE, S_BAR_HYG,
  // marcador en vivo del minijuego
  S_REC_FMT,
  // ficha: pagina de progreso
  S_PROGRESS, S_LVL_FMT, S_NEXT_LVL_FMT, S_EVO_LABEL, S_FINAL_FORM,
  S_EVO_READY, S_EVO_BLOCKED, S_EVO_IN_FMT, S_MISTAKES_FMT,
  // interruptor de sonido (ajustes)
  S_SND_ON, S_SND_OFF,
  S_EVO_TAP,        // texto del boton de evolucion
  S_FAREWELL_BTN,   // texto del boton de despedida (lleva el nombre: "%s ...")
  S_RUNAWAY_BTN,    // texto del boton de escapada por abandono (final triste)
  // dialogos de decision (evolucionar/mantener, despedirse/quedaros)
  S_EVO_Q, S_EVO_KEEP, S_FAR_Q, S_FAR_GO, S_FAR_STAY,
  S_CHOOSE_STARTER,  // titulo de la eleccion del inicial (primera vez)
  S_NO_SPRITES, S_LOAD_SPRITES,  // aviso cuando falta el sprite en la SD
  // pasos y mochila (v1.20): ficha WALK, hallazgos, meta, trucos del IMU
  S_WALK, S_STEPS_FMT, S_STEPS_TOTAL_FMT, S_FINDS_FMT, S_BACKPACK, S_NO_IMU,
  S_GOAL_DONE, S_FOUND, S_BACKPACK_FULL, S_USE_FMT, S_CHARM_HELD, S_BACKPACK_EMPTY,
  S_GRUMPY, S_DIZZY,
  // nombres de los objetos, en el orden de IT_* (pet.h)
  S_IT_GOLDEN, S_IT_PROTEIN, S_IT_IRON, S_IT_CARBOS, S_IT_CHARM, S_IT_RARECANDY, S_IT_EGG,
  // combate de practica (v1.24): tarjeta VS, rotulos de cada golpe, resultado
  S_PRACTICE, S_SUPER_EFF, S_NOT_EFF, S_NO_EFFECT, S_CRITICAL, S_SPECIAL, S_TAP_CHARGE,
  S_WIN, S_LOSE, S_DRAW, S_PRACTICE_JOY_FMT, S_PRACTICE_NONE,
  // efectos de respuesta (v1.30): pildora de la subida de nivel
  S_LEVEL_UP,
  // la caja (v1.37): titulo, dialogos de guardar / despertar, el activo y los huecos sin bicho
  S_BOX_FMT, S_STORE_Q_FMT, S_WAKE_Q_FMT, S_SWAP_NOTE_FMT, S_NOW_FMT, S_BOX_EMPTY, S_BOX_DAMAGED,
  // combate (v1.38): el cartel que pide tocar, al lado de las flechas del especial
  S_TAP_BOX,
  // huevos limitados (v1.39): la casa vacia (titulo, mensaje, pista de la caja), la nota de los
  // dialogos que la dejarian vacia y la pista de la mochila
  S_NOEGG_HDR, S_NOEGG_MSG, S_NOEGG_BOX, S_NO_EGG_NOTE, S_EGG_HINT,
  // huevos al acabar una crianza (v1.40/v1.41): el boton "abrir un huevo (N)" de la casa vacia, la
  // nota del regalo, el mensaje de la casa vacia con huevos y el titulo del aviso de un regalo
  S_HATCH_FMT, S_GIFT_EGG_NOTE, S_NOEGG_HAVE, S_GIFT,
  // la cascada (v1.43): intro, pista, concentracion en vivo, record, resultado y el lavado
  S_ZEN_INTRO, S_ZEN_HOLD, S_FOCUS_FMT, S_ZEN_BEST_FMT, S_DEF_GAIN_FMT, S_CLEAN,
  // la carrera en trineo (v1.44): calibracion, pista, resultado
  S_DASH_HOLD, S_DASH_STILL, S_DASH_TILT, S_DODGED_FMT, S_SPD_GAIN_FMT,
  // v1.47: bonificacion de la cascada y el entrenamiento ya al tope (en vez de "+0")
  S_ZEN_BONUS_FMT, S_ATK_MAX, S_DEF_MAX, S_SPE_MAX,
  STR_COUNT
};

const char *T(StrId id);       // texto en el idioma activo
const char *medalName(int i);  // banner de medalla (MED_COUNT)
const char *medalLabel(int i); // etiqueta corta de medalla
const char *medalDesc(int i);  // descripcion larga de medalla

void loadLang();             // lee el idioma de NVS (llamar en setup)
void setLang(Lang l);        // cambia y persiste el idioma
