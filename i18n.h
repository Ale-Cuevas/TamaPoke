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
enum StrId : uint16_t {
  // estado del bicho
  S_EATING, S_LIKES, S_HUNGRY, S_NEEDS_BATH,
  S_EXHAUSTED, S_SAD,
  // ceremonias de despedida
  S_FAREWELL, S_DIED, S_GOODBYE,  // la segunda: la muerte por abandono (v1.60, era la escapada)
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
  S_STREAK_FMT, S_VIN, S_BERRY_UNK,
  S_INFO_FMT, S_RENAME_HINT,
  // (v3.4: los de la antigua pagina de progreso, mas abajo, ya no se usan todos; se quedan: quitar un id corre las
  // 8 filas de i18n.cpp. test_tools.py lee los ids de este enum: aqui los comentarios no nombran ids)
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
  S_EVO_TAP,        // texto del boton de evolucion
  S_FAREWELL_BTN,   // v3.3: sin uso (era el boton de despedida; se queda: quitar un id corre las 8 filas)
  S_ABANDONED,      // estado con el vinculo apagandose por abandono (v1.60, era el boton de escapada)
  // dialogos de decision (evolucionar/mantener, despedirse/quedaros)
  S_EVO_Q, S_EVO_KEEP, S_FAR_Q, S_FAR_GO, S_FAR_STAY,
  S_CHOOSE_STARTER,  // titulo de la eleccion del inicial (primera vez)
  S_NO_SPRITES, S_LOAD_SPRITES,  // aviso cuando falta el sprite en la SD
  // pasos y mochila (v1.20): ficha WALK, hallazgos, meta, trucos del IMU
  S_WALK, S_STEPS_FMT, S_STEPS_TOTAL_FMT, S_FINDS_FMT, S_BACKPACK, S_NO_IMU,
  S_GOAL_DONE, S_FOUND, S_BACKPACK_FULL, S_USE_FMT, S_CHARM_HELD, S_BACKPACK_EMPTY,
  S_GRUMPY, S_DIZZY,
  // nombres de los objetos, en el orden de IT_* (pet.h)
  S_IT_GOLDEN, S_IT_PROTEIN, S_IT_IRON, S_IT_CARBOS, S_IT_CHARM, S_IT_RARECANDY, S_IT_EGG, S_IT_CAKE, S_IT_REVIVE,
  S_IT_BALL,  // v2.2
  S_IT_BERRY_R, S_IT_BERRY_B, S_IT_BERRY_G, S_IT_BERRY_Y, S_IT_BERRY_P, S_IT_BERRY_O,  // v3.2: bayas de colores
  S_IT_LURE,  // v3.14: el Cebo
  // combate de practica (v1.24): tarjeta VS, rotulos de cada golpe, resultado
  S_PRACTICE, S_SUPER_EFF, S_NOT_EFF, S_NO_EFFECT, S_CRITICAL, S_SPECIAL, S_TAP_CHARGE,
  S_SUDDEN_DEATH, S_RECOIL,  // v1.67: muerte subita (ataque 21 en adelante)
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
  // v1.49: leyenda de la barra fina de entrenamiento en la ficha de combate
  S_TRAIN_LEGEND,
  // v1.53: la mochila con su boton: como se usa y dormido solo se mira
  S_BAG_HINT, S_BAG_ASLEEP,
  // v1.54: tocar la tarta sin ninguna
  S_NO_CAKE,
  // v1.56: el botin de un combate ganado, que espera envuelto en casa
  S_BATTLE_GIFT,
  // v1.57: al evolucionar cambio la baya favorita (vuelve a ser un secreto)
  S_TASTES,
  // v1.59: la pista del Revivir en la mochila (no se usa a mano)
  S_REVIVE_HINT,
  // v1.60: muerte por abandono: estado sin vinculo, el dialogo (con o sin Revivir), el huevo que deja
  // y el estado al revivir
  S_WEAK, S_DYING_FMT, S_USE_REVIVE_FMT, S_NO_REVIVES, S_OK, S_DIED_EGG, S_REVIVED,
  // v1.63: la noche, tiene sueño (desde las 22)
  S_SLEEPY,
  // v1.64: una hora para el Revivir: los minutos que quedan, y el aviso de que murio
  S_DYING_LEFT_FMT, S_DIED_FMT, S_DIED_LATE,
  // stages: la lista de fases, el equipo, los carteles de la pelea y las insignias
  S_STAGES, S_BADGES, S_PICK_TEAM, S_START, S_BOSS, S_LOCKED, S_STAGE_CLEAR, S_STAGE_LOST,
  S_NEXT_RIVAL, S_GO_FMT, S_NEW_BADGE, S_LV_RANGE_FMT, S_TEAM_REWARD, S_REWARD_TODAY, S_STAGE_FMT,
  // el patio (v2.0): la tienda, sus botones y las etiquetas del dock (Cuidar / Jugar / Mochila)
  S_SHOP, S_BUY, S_USE, S_PURCHASED, S_DOCK_CARE, S_DOCK_PLAY, S_DOCK_BAG,
  // el huevo del patio (v2.0, Y3): la pista de agitarlo (en el globo, con lo que le falta)
  S_EGG_SHAKE,
  // la tienda de la mochila y los fondos (v2.0, Y4)
  S_TAB_ITEMS, S_BACKGROUNDS, S_BUY_FMT, S_NOT_ENOUGH, S_BAG_FULL,
  S_MAP_1, S_MAP_2, S_MAP_3, S_MAP_4, S_MAP_5,
  S_MAP_6, S_MAP_7, S_MAP_8, S_MAP_9,  // v3.15: volcan, central, torre del cielo, distrito toxico (el primero + mapa - 1)
  S_USING, S_NOT_ON_SD, S_SHOP_HINT,
  // los viajes del patio (v2.0, Y5): los recuerdos (el primero + su id, 0..SOUV_COUNT-1; v3.15: 45) y las plantillas de
  // sucesos (el primero + la plantilla; travel.h; v3.15: 17)
  S_SOUV_0, S_SOUV_1, S_SOUV_2, S_SOUV_3, S_SOUV_4, S_SOUV_5, S_SOUV_6, S_SOUV_7, S_SOUV_8, S_SOUV_9, S_SOUV_10, S_SOUV_11, S_SOUV_12, S_SOUV_13, S_SOUV_14, S_SOUV_15, S_SOUV_16, S_SOUV_17, S_SOUV_18, S_SOUV_19, S_SOUV_20, S_SOUV_21, S_SOUV_22, S_SOUV_23, S_SOUV_24,
  S_SOUV_25, S_SOUV_26, S_SOUV_27, S_SOUV_28, S_SOUV_29, S_SOUV_30, S_SOUV_31, S_SOUV_32, S_SOUV_33, S_SOUV_34,
  S_SOUV_35, S_SOUV_36, S_SOUV_37, S_SOUV_38, S_SOUV_39, S_SOUV_40, S_SOUV_41, S_SOUV_42, S_SOUV_43, S_SOUV_44,
  S_TRV_EV_0, S_TRV_EV_1, S_TRV_EV_2, S_TRV_EV_3, S_TRV_EV_4, S_TRV_EV_5, S_TRV_EV_6, S_TRV_EV_7, S_TRV_EV_8, S_TRV_EV_9, S_TRV_EV_10, S_TRV_EV_11, S_TRV_EV_12,
  S_TRV_EV_13, S_TRV_EV_14, S_TRV_EV_15, S_TRV_EV_16,  // v3.15: uno por mapa nuevo (volcan, central, torre, distrito)
  // los viajes, las pantallas (v2.0, Y5b)
  S_TRV_TITLE, S_TRV_TRAVELLING, S_TRV_ONWAY, S_TRV_BACK_FMT, S_TRV_AWAY, S_TRV_GO, S_TRV_BAGFULL, S_TRV_NEW,
  S_TRV_FOUND_FMT, S_COLLECTION, S_TRV_NOPETS, S_TRV_NOCLOCK, S_RAR_0, S_RAR_1, S_RAR_2, S_TRV_AWAY_FMT,
  // encuentros salvajes (v2.2): la pista de la Poke Ball, la pantalla del encuentro, el resultado y la captura
  S_BALL_HINT, S_WILD_APPEARED_FMT, S_WILD_TAP, S_WILD_PICK, S_WILD, S_WILD_WON, S_WILD_LOST,
  S_WILD_GROW, S_THROW_BALL, S_BALLS_LEFT_FMT, S_THROW, S_CAPTURED_FMT, S_CAPTURE_EGG, S_WILD_LEFT_FMT,
  S_WILD_FLED_FMT, S_NO_BALLS, S_NO_ROOM, S_EGG_CAPTURED,
  // dos bichos en casa (v3.0): la caja pregunta cual se guarda o con cual se cambia
  S_BOX_WHICH_STORE, S_BOX_WHICH_SWAP,
  // v3.3: el menu de cada bicho en el patio (la etiqueta del antiguo boton Cuidar es ahora su fila del mimo),
  // los nombres cortos de la lista de comida (en el orden de FOOD_* de food.h) y el selector de bicho
  S_PM_SLEEP, S_PM_WAKE, S_PM_FOOD, S_PM_STATUS, S_PM_EVOLVE, S_PM_FAREWELL, S_PM_BACK,
  S_FD_PLAIN, S_FD_RED, S_FD_BLUE, S_FD_GREEN, S_FD_YELLOW, S_FD_PINK, S_FD_ORANGE, S_FD_CAKE, S_FD_GOLDEN,
  S_PICK_PLAY, S_PICK_ITEM,
  // v3.4: los tramos de las medallas del jugador y la 2.a pagina de la ficha (era "Combate")
  S_TIER_BRONZE, S_TIER_SILVER, S_TIER_GOLD, S_STATS,
  // v3.14: la pista del Cebo en la mochila cuando no se puede usar (para que sirve / ya hay un salvaje)
  S_LURE_HINT, S_LURE_BUSY, S_LURE_NOFIGHT,
  // v3.17: la pista del minijuego de la bola
  S_TAP_BALL,
  // v3.20 HOME: la pagina Casa de la tienda: titulo, las 3 lineas, los 9 niveles (S_UPG_BED1 + 3 * linea + nivel - 1),
  // los 3 efectos (%u), MAX y "Comprar %s por %u?"
  S_HOME, S_UPG_BED, S_UPG_GYM, S_UPG_TOY,
  S_UPG_BED1, S_UPG_BED2, S_UPG_BED3, S_UPG_GYM1, S_UPG_GYM2, S_UPG_GYM3, S_UPG_TOY1, S_UPG_TOY2, S_UPG_TOY3,
  S_UPG_BED_FX, S_UPG_GYM_FX, S_UPG_TOY_FX, S_MAX, S_BUY_UPG_FMT,
  STR_COUNT
};

const char *T(StrId id);       // texto en el idioma activo
const char *medalName(int i);  // v3.4: nombre de la medalla del jugador i (MD_*, MEDAL_COUNT)
const char *medalDesc(int i);  // v3.4: lo que mide (su tarjeta en la pagina de medallas)
#define STAGE_NAME_COUNT 8     // las 8 fases (igual que STAGE_COUNT de stage.h)
const char *stageName(uint8_t i); // nombre del tema de la fase 0-7 (planta ... dragon)

void loadLang();             // lee el idioma de NVS (llamar en setup)
void setLang(Lang l);        // cambia y persiste el idioma
