#pragma once
#include <stdint.h>
#include "i18n.h"  // gLang

// GENERADO por tools/gen_dex.py desde tools/dex_data.py - no editar

#define DEX_COUNT 151
#define DEX_EEVEE 133  // rama al azar: 134/135/136

// los 15 tipos de gen 1, en el orden de la tabla de eficacias de battle.h
enum : uint8_t {
  TY_NORMAL = 0,
  TY_FUEGO,
  TY_AGUA,
  TY_ELECTRICO,
  TY_PLANTA,
  TY_HIELO,
  TY_LUCHA,
  TY_VENENO,
  TY_TIERRA,
  TY_VOLADOR,
  TY_PSIQUICO,
  TY_BICHO,
  TY_ROCA,
  TY_FANTASMA,
  TY_DRAGON,
  TY_COUNT,
  TY_NONE = 0xFF
};

// rareza: 0 = solo por evolucion, 1 = comun, 2 = raro, 3 = legendario
enum : uint8_t { R_EVO = 0, R_COMUN, R_RARO, R_LEGENDARIO };

struct DexEntry {
  const char *name;
  uint8_t evolvesTo;    // numero de dex, 0 = forma final
  uint8_t evolveLevel;
  uint8_t rarity;       // sale de huevo si > 0
  uint16_t accent;      // color RGB565 del tipo para la UI
  uint8_t bHp, bAtk, bDef, bSpe;  // base stats reales de gen 1
  uint8_t biome;        // 0 pradera 1 playa 2 bosque 3 volcan 4 montana 5 nieve
  uint8_t type1, type2; // TY_*; type2 = TY_NONE si solo tiene uno (combate)
  uint8_t base;         // primera forma de su linea evolutiva
};

static const DexEntry DEX_TBL[DEX_COUNT + 1] = {
  { "?", 0, 0, 0, 0x2946, 50, 50, 50, 50, 0, TY_NORMAL, TY_NONE, 0 },  // 0: sin usar
  { "BULBASAUR", 2, 16, R_COMUN, 0x3C49, 45, 49, 49, 45, 2, TY_PLANTA, TY_VENENO, 1 },  // 1 planta/veneno
  { "IVYSAUR", 3, 32, R_EVO, 0x3C49, 60, 62, 63, 60, 2, TY_PLANTA, TY_VENENO, 1 },  // 2 planta/veneno
  { "VENUSAUR", 0, 0, R_EVO, 0x3C49, 80, 82, 83, 80, 2, TY_PLANTA, TY_VENENO, 1 },  // 3 planta/veneno
  { "CHARMANDER", 5, 16, R_COMUN, 0xEA87, 39, 52, 43, 65, 3, TY_FUEGO, TY_NONE, 4 },  // 4 fuego
  { "CHARMELEON", 6, 36, R_EVO, 0xEA87, 58, 64, 58, 80, 3, TY_FUEGO, TY_NONE, 4 },  // 5 fuego
  { "CHARIZARD", 0, 0, R_EVO, 0xEA87, 78, 84, 78, 100, 3, TY_FUEGO, TY_VOLADOR, 4 },  // 6 fuego/volador
  { "SQUIRTLE", 8, 16, R_COMUN, 0x4C98, 44, 48, 65, 43, 1, TY_AGUA, TY_NONE, 7 },  // 7 agua
  { "WARTORTLE", 9, 36, R_EVO, 0x4C98, 59, 63, 80, 58, 1, TY_AGUA, TY_NONE, 7 },  // 8 agua
  { "BLASTOISE", 0, 0, R_EVO, 0x4C98, 79, 83, 100, 78, 1, TY_AGUA, TY_NONE, 7 },  // 9 agua
  { "CATERPIE", 11, 7, R_COMUN, 0x7CC4, 45, 30, 35, 45, 2, TY_BICHO, TY_NONE, 10 },  // 10 bicho
  { "METAPOD", 12, 10, R_EVO, 0x7CC4, 50, 20, 55, 30, 2, TY_BICHO, TY_NONE, 10 },  // 11 bicho
  { "BUTTERFREE", 0, 0, R_EVO, 0x7CC4, 60, 45, 50, 70, 2, TY_BICHO, TY_VOLADOR, 10 },  // 12 bicho/volador
  { "WEEDLE", 14, 7, R_COMUN, 0x7CC4, 40, 35, 30, 50, 2, TY_BICHO, TY_VENENO, 13 },  // 13 bicho/veneno
  { "KAKUNA", 15, 10, R_EVO, 0x7CC4, 45, 25, 50, 35, 2, TY_BICHO, TY_VENENO, 13 },  // 14 bicho/veneno
  { "BEEDRILL", 0, 0, R_EVO, 0x7CC4, 65, 90, 40, 75, 2, TY_BICHO, TY_VENENO, 13 },  // 15 bicho/veneno
  { "PIDGEY", 17, 18, R_COMUN, 0x8C4D, 40, 45, 40, 56, 0, TY_NORMAL, TY_VOLADOR, 16 },  // 16 normal/volador
  { "PIDGEOTTO", 18, 36, R_EVO, 0x8C4D, 63, 60, 55, 71, 0, TY_NORMAL, TY_VOLADOR, 16 },  // 17 normal/volador
  { "PIDGEOT", 0, 0, R_EVO, 0x8C4D, 83, 80, 75, 101, 0, TY_NORMAL, TY_VOLADOR, 16 },  // 18 normal/volador
  { "RATTATA", 20, 20, R_COMUN, 0x8C4D, 30, 56, 35, 72, 0, TY_NORMAL, TY_NONE, 19 },  // 19 normal
  { "RATICATE", 0, 0, R_EVO, 0x8C4D, 55, 81, 60, 97, 0, TY_NORMAL, TY_NONE, 19 },  // 20 normal
  { "SPEAROW", 22, 20, R_COMUN, 0x8C4D, 40, 60, 30, 70, 0, TY_NORMAL, TY_VOLADOR, 21 },  // 21 normal/volador
  { "FEAROW", 0, 0, R_EVO, 0x8C4D, 65, 90, 65, 100, 0, TY_NORMAL, TY_VOLADOR, 21 },  // 22 normal/volador
  { "EKANS", 24, 22, R_COMUN, 0x8A73, 35, 60, 44, 55, 0, TY_VENENO, TY_NONE, 23 },  // 23 veneno
  { "ARBOK", 0, 0, R_EVO, 0x8A73, 60, 95, 69, 80, 0, TY_VENENO, TY_NONE, 23 },  // 24 veneno
  { "PIKACHU", 26, 30, R_COMUN, 0xBCA1, 35, 55, 40, 90, 0, TY_ELECTRICO, TY_NONE, 25 },  // 25 electrico
  { "RAICHU", 0, 0, R_EVO, 0xBCA1, 60, 90, 55, 110, 0, TY_ELECTRICO, TY_NONE, 25 },  // 26 electrico
  { "SANDSHREW", 28, 22, R_COMUN, 0xB447, 50, 75, 85, 40, 4, TY_TIERRA, TY_NONE, 27 },  // 27 tierra
  { "SANDSLASH", 0, 0, R_EVO, 0xB447, 75, 100, 110, 65, 4, TY_TIERRA, TY_NONE, 27 },  // 28 tierra
  { "NIDORAN H", 30, 16, R_COMUN, 0x8A73, 55, 47, 52, 41, 0, TY_VENENO, TY_NONE, 29 },  // 29 veneno
  { "NIDORINA", 31, 30, R_EVO, 0x8A73, 70, 62, 67, 56, 0, TY_VENENO, TY_NONE, 29 },  // 30 veneno
  { "NIDOQUEEN", 0, 0, R_EVO, 0x8A73, 90, 92, 87, 76, 0, TY_VENENO, TY_TIERRA, 29 },  // 31 veneno/tierra
  { "NIDORAN M", 33, 16, R_COMUN, 0x8A73, 46, 57, 40, 50, 0, TY_VENENO, TY_NONE, 32 },  // 32 veneno
  { "NIDORINO", 34, 30, R_EVO, 0x8A73, 61, 72, 57, 65, 0, TY_VENENO, TY_NONE, 32 },  // 33 veneno
  { "NIDOKING", 0, 0, R_EVO, 0x8A73, 81, 102, 77, 85, 0, TY_VENENO, TY_TIERRA, 32 },  // 34 veneno/tierra
  { "CLEFAIRY", 36, 30, R_COMUN, 0x8C4D, 70, 45, 48, 35, 0, TY_NORMAL, TY_NONE, 35 },  // 35 normal
  { "CLEFABLE", 0, 0, R_EVO, 0x8C4D, 95, 70, 73, 60, 0, TY_NORMAL, TY_NONE, 35 },  // 36 normal
  { "VULPIX", 38, 30, R_COMUN, 0xEA87, 38, 41, 40, 65, 3, TY_FUEGO, TY_NONE, 37 },  // 37 fuego
  { "NINETALES", 0, 0, R_EVO, 0xEA87, 73, 76, 75, 100, 3, TY_FUEGO, TY_NONE, 37 },  // 38 fuego
  { "JIGGLYPUFF", 40, 30, R_COMUN, 0x8C4D, 115, 45, 20, 20, 0, TY_NORMAL, TY_NONE, 39 },  // 39 normal
  { "WIGGLYTUFF", 0, 0, R_EVO, 0x8C4D, 140, 70, 45, 45, 0, TY_NORMAL, TY_NONE, 39 },  // 40 normal
  { "ZUBAT", 42, 22, R_COMUN, 0x8A73, 40, 45, 35, 55, 0, TY_VENENO, TY_VOLADOR, 41 },  // 41 veneno/volador
  { "GOLBAT", 0, 0, R_EVO, 0x8A73, 75, 80, 70, 90, 0, TY_VENENO, TY_VOLADOR, 41 },  // 42 veneno/volador
  { "ODDISH", 44, 21, R_COMUN, 0x3C49, 45, 50, 55, 30, 2, TY_PLANTA, TY_VENENO, 43 },  // 43 planta/veneno
  { "GLOOM", 45, 36, R_EVO, 0x3C49, 60, 65, 70, 40, 2, TY_PLANTA, TY_VENENO, 43 },  // 44 planta/veneno
  { "VILEPLUME", 0, 0, R_EVO, 0x3C49, 75, 80, 85, 50, 2, TY_PLANTA, TY_VENENO, 43 },  // 45 planta/veneno
  { "PARAS", 47, 24, R_COMUN, 0x7CC4, 35, 70, 55, 25, 2, TY_BICHO, TY_PLANTA, 46 },  // 46 bicho/planta
  { "PARASECT", 0, 0, R_EVO, 0x7CC4, 60, 95, 80, 30, 2, TY_BICHO, TY_PLANTA, 46 },  // 47 bicho/planta
  { "VENONAT", 49, 31, R_COMUN, 0x7CC4, 60, 55, 50, 45, 2, TY_BICHO, TY_VENENO, 48 },  // 48 bicho/veneno
  { "VENOMOTH", 0, 0, R_EVO, 0x7CC4, 70, 65, 60, 90, 2, TY_BICHO, TY_VENENO, 48 },  // 49 bicho/veneno
  { "DIGLETT", 51, 26, R_COMUN, 0xB447, 10, 55, 25, 95, 4, TY_TIERRA, TY_NONE, 50 },  // 50 tierra
  { "DUGTRIO", 0, 0, R_EVO, 0xB447, 35, 100, 50, 120, 4, TY_TIERRA, TY_NONE, 50 },  // 51 tierra
  { "MEOWTH", 53, 28, R_COMUN, 0x8C4D, 40, 45, 35, 90, 0, TY_NORMAL, TY_NONE, 52 },  // 52 normal
  { "PERSIAN", 0, 0, R_EVO, 0x8C4D, 65, 70, 60, 115, 0, TY_NORMAL, TY_NONE, 52 },  // 53 normal
  { "PSYDUCK", 55, 33, R_COMUN, 0x4C98, 50, 52, 48, 55, 1, TY_AGUA, TY_NONE, 54 },  // 54 agua
  { "GOLDUCK", 0, 0, R_EVO, 0x4C98, 80, 82, 78, 85, 1, TY_AGUA, TY_NONE, 54 },  // 55 agua
  { "MANKEY", 57, 28, R_COMUN, 0xA2A5, 40, 80, 35, 70, 0, TY_LUCHA, TY_NONE, 56 },  // 56 lucha
  { "PRIMEAPE", 0, 0, R_EVO, 0xA2A5, 65, 105, 60, 95, 0, TY_LUCHA, TY_NONE, 56 },  // 57 lucha
  { "GROWLITHE", 59, 30, R_RARO, 0xEA87, 55, 70, 45, 60, 3, TY_FUEGO, TY_NONE, 58 },  // 58 fuego
  { "ARCANINE", 0, 0, R_EVO, 0xEA87, 90, 110, 80, 95, 3, TY_FUEGO, TY_NONE, 58 },  // 59 fuego
  { "POLIWAG", 61, 25, R_COMUN, 0x4C98, 40, 50, 40, 90, 1, TY_AGUA, TY_NONE, 60 },  // 60 agua
  { "POLIWHIRL", 62, 36, R_EVO, 0x4C98, 65, 65, 65, 90, 1, TY_AGUA, TY_NONE, 60 },  // 61 agua
  { "POLIWRATH", 0, 0, R_EVO, 0x4C98, 90, 95, 95, 70, 1, TY_AGUA, TY_LUCHA, 60 },  // 62 agua/lucha
  { "ABRA", 64, 16, R_COMUN, 0xD28F, 25, 20, 15, 90, 0, TY_PSIQUICO, TY_NONE, 63 },  // 63 psiquico
  { "KADABRA", 65, 40, R_EVO, 0xD28F, 40, 35, 30, 105, 0, TY_PSIQUICO, TY_NONE, 63 },  // 64 psiquico
  { "ALAKAZAM", 0, 0, R_EVO, 0xD28F, 55, 50, 45, 120, 0, TY_PSIQUICO, TY_NONE, 63 },  // 65 psiquico
  { "MACHOP", 67, 28, R_COMUN, 0xA2A5, 70, 80, 50, 35, 0, TY_LUCHA, TY_NONE, 66 },  // 66 lucha
  { "MACHOKE", 68, 40, R_EVO, 0xA2A5, 80, 100, 70, 45, 0, TY_LUCHA, TY_NONE, 66 },  // 67 lucha
  { "MACHAMP", 0, 0, R_EVO, 0xA2A5, 90, 130, 80, 55, 0, TY_LUCHA, TY_NONE, 66 },  // 68 lucha
  { "BELLSPROUT", 70, 21, R_COMUN, 0x3C49, 50, 75, 35, 40, 2, TY_PLANTA, TY_VENENO, 69 },  // 69 planta/veneno
  { "WEEPINBELL", 71, 36, R_EVO, 0x3C49, 65, 90, 50, 55, 2, TY_PLANTA, TY_VENENO, 69 },  // 70 planta/veneno
  { "VICTREEBEL", 0, 0, R_EVO, 0x3C49, 80, 105, 65, 70, 2, TY_PLANTA, TY_VENENO, 69 },  // 71 planta/veneno
  { "TENTACOOL", 73, 30, R_COMUN, 0x4C98, 40, 40, 35, 70, 1, TY_AGUA, TY_VENENO, 72 },  // 72 agua/veneno
  { "TENTACRUEL", 0, 0, R_EVO, 0x4C98, 80, 70, 65, 100, 1, TY_AGUA, TY_VENENO, 72 },  // 73 agua/veneno
  { "GEODUDE", 75, 25, R_COMUN, 0x9407, 40, 80, 100, 20, 4, TY_ROCA, TY_TIERRA, 74 },  // 74 roca/tierra
  { "GRAVELER", 76, 40, R_EVO, 0x9407, 55, 95, 115, 35, 4, TY_ROCA, TY_TIERRA, 74 },  // 75 roca/tierra
  { "GOLEM", 0, 0, R_EVO, 0x9407, 80, 120, 130, 45, 4, TY_ROCA, TY_TIERRA, 74 },  // 76 roca/tierra
  { "PONYTA", 78, 40, R_RARO, 0xEA87, 50, 85, 55, 90, 3, TY_FUEGO, TY_NONE, 77 },  // 77 fuego
  { "RAPIDASH", 0, 0, R_EVO, 0xEA87, 65, 100, 70, 105, 3, TY_FUEGO, TY_NONE, 77 },  // 78 fuego
  { "SLOWPOKE", 80, 37, R_COMUN, 0x4C98, 90, 65, 65, 15, 1, TY_AGUA, TY_PSIQUICO, 79 },  // 79 agua/psiquico
  { "SLOWBRO", 0, 0, R_EVO, 0x4C98, 95, 75, 110, 30, 1, TY_AGUA, TY_PSIQUICO, 79 },  // 80 agua/psiquico
  { "MAGNEMITE", 82, 30, R_COMUN, 0xBCA1, 25, 35, 70, 45, 0, TY_ELECTRICO, TY_NONE, 81 },  // 81 electrico
  { "MAGNETON", 0, 0, R_EVO, 0xBCA1, 50, 60, 95, 70, 0, TY_ELECTRICO, TY_NONE, 81 },  // 82 electrico
  { "FARFETCHD", 0, 0, R_RARO, 0x8C4D, 52, 90, 55, 60, 0, TY_NORMAL, TY_VOLADOR, 83 },  // 83 normal/volador
  { "DODUO", 85, 31, R_COMUN, 0x8C4D, 35, 85, 45, 75, 0, TY_NORMAL, TY_VOLADOR, 84 },  // 84 normal/volador
  { "DODRIO", 0, 0, R_EVO, 0x8C4D, 60, 110, 70, 110, 0, TY_NORMAL, TY_VOLADOR, 84 },  // 85 normal/volador
  { "SEEL", 87, 34, R_COMUN, 0x4C98, 65, 45, 55, 45, 1, TY_AGUA, TY_NONE, 86 },  // 86 agua
  { "DEWGONG", 0, 0, R_EVO, 0x4C98, 90, 70, 80, 70, 1, TY_AGUA, TY_HIELO, 86 },  // 87 agua/hielo
  { "GRIMER", 89, 38, R_RARO, 0x8A73, 80, 80, 50, 25, 0, TY_VENENO, TY_NONE, 88 },  // 88 veneno
  { "MUK", 0, 0, R_EVO, 0x8A73, 105, 105, 75, 50, 0, TY_VENENO, TY_NONE, 88 },  // 89 veneno
  { "SHELLDER", 91, 30, R_COMUN, 0x4C98, 30, 65, 100, 40, 1, TY_AGUA, TY_NONE, 90 },  // 90 agua
  { "CLOYSTER", 0, 0, R_EVO, 0x4C98, 50, 95, 180, 70, 1, TY_AGUA, TY_HIELO, 90 },  // 91 agua/hielo
  { "GASTLY", 93, 25, R_COMUN, 0x6AD3, 30, 35, 30, 80, 0, TY_FANTASMA, TY_VENENO, 92 },  // 92 fantasma/veneno
  { "HAUNTER", 94, 40, R_EVO, 0x6AD3, 45, 50, 45, 95, 0, TY_FANTASMA, TY_VENENO, 92 },  // 93 fantasma/veneno
  { "GENGAR", 0, 0, R_EVO, 0x6AD3, 60, 65, 60, 110, 0, TY_FANTASMA, TY_VENENO, 92 },  // 94 fantasma/veneno
  { "ONIX", 0, 0, R_RARO, 0x9407, 35, 45, 160, 70, 4, TY_ROCA, TY_TIERRA, 95 },  // 95 roca/tierra
  { "DROWZEE", 97, 26, R_COMUN, 0xD28F, 60, 48, 45, 42, 0, TY_PSIQUICO, TY_NONE, 96 },  // 96 psiquico
  { "HYPNO", 0, 0, R_EVO, 0xD28F, 85, 73, 70, 67, 0, TY_PSIQUICO, TY_NONE, 96 },  // 97 psiquico
  { "KRABBY", 99, 28, R_COMUN, 0x4C98, 30, 105, 90, 50, 1, TY_AGUA, TY_NONE, 98 },  // 98 agua
  { "KINGLER", 0, 0, R_EVO, 0x4C98, 55, 130, 115, 75, 1, TY_AGUA, TY_NONE, 98 },  // 99 agua
  { "VOLTORB", 101, 30, R_COMUN, 0xBCA1, 40, 30, 50, 100, 0, TY_ELECTRICO, TY_NONE, 100 },  // 100 electrico
  { "ELECTRODE", 0, 0, R_EVO, 0xBCA1, 60, 50, 70, 150, 0, TY_ELECTRICO, TY_NONE, 100 },  // 101 electrico
  { "EXEGGCUTE", 103, 30, R_COMUN, 0x3C49, 60, 40, 80, 40, 2, TY_PLANTA, TY_PSIQUICO, 102 },  // 102 planta/psiquico
  { "EXEGGUTOR", 0, 0, R_EVO, 0x3C49, 95, 95, 85, 55, 2, TY_PLANTA, TY_PSIQUICO, 102 },  // 103 planta/psiquico
  { "CUBONE", 105, 28, R_COMUN, 0xB447, 50, 50, 95, 35, 4, TY_TIERRA, TY_NONE, 104 },  // 104 tierra
  { "MAROWAK", 0, 0, R_EVO, 0xB447, 60, 80, 110, 45, 4, TY_TIERRA, TY_NONE, 104 },  // 105 tierra
  { "HITMONLEE", 0, 0, R_RARO, 0xA2A5, 50, 120, 53, 87, 0, TY_LUCHA, TY_NONE, 106 },  // 106 lucha
  { "HITMONCHAN", 0, 0, R_RARO, 0xA2A5, 50, 105, 79, 76, 0, TY_LUCHA, TY_NONE, 107 },  // 107 lucha
  { "LICKITUNG", 0, 0, R_RARO, 0x8C4D, 90, 55, 75, 30, 0, TY_NORMAL, TY_NONE, 108 },  // 108 normal
  { "KOFFING", 110, 35, R_COMUN, 0x8A73, 40, 65, 95, 35, 0, TY_VENENO, TY_NONE, 109 },  // 109 veneno
  { "WEEZING", 0, 0, R_EVO, 0x8A73, 65, 90, 120, 60, 0, TY_VENENO, TY_NONE, 109 },  // 110 veneno
  { "RHYHORN", 112, 42, R_RARO, 0xB447, 80, 85, 95, 25, 4, TY_TIERRA, TY_ROCA, 111 },  // 111 tierra/roca
  { "RHYDON", 0, 0, R_EVO, 0xB447, 105, 130, 120, 40, 4, TY_TIERRA, TY_ROCA, 111 },  // 112 tierra/roca
  { "CHANSEY", 0, 0, R_RARO, 0x8C4D, 250, 5, 5, 50, 0, TY_NORMAL, TY_NONE, 113 },  // 113 normal
  { "TANGELA", 0, 0, R_RARO, 0x3C49, 65, 55, 115, 60, 2, TY_PLANTA, TY_NONE, 114 },  // 114 planta
  { "KANGASKHAN", 0, 0, R_RARO, 0x8C4D, 105, 95, 80, 90, 0, TY_NORMAL, TY_NONE, 115 },  // 115 normal
  { "HORSEA", 117, 32, R_COMUN, 0x4C98, 30, 40, 70, 60, 1, TY_AGUA, TY_NONE, 116 },  // 116 agua
  { "SEADRA", 0, 0, R_EVO, 0x4C98, 55, 65, 95, 85, 1, TY_AGUA, TY_NONE, 116 },  // 117 agua
  { "GOLDEEN", 119, 33, R_COMUN, 0x4C98, 45, 67, 60, 63, 1, TY_AGUA, TY_NONE, 118 },  // 118 agua
  { "SEAKING", 0, 0, R_EVO, 0x4C98, 80, 92, 65, 68, 1, TY_AGUA, TY_NONE, 118 },  // 119 agua
  { "STARYU", 121, 30, R_COMUN, 0x4C98, 30, 45, 55, 85, 1, TY_AGUA, TY_NONE, 120 },  // 120 agua
  { "STARMIE", 0, 0, R_EVO, 0x4C98, 60, 75, 85, 115, 1, TY_AGUA, TY_PSIQUICO, 120 },  // 121 agua/psiquico
  { "MR. MIME", 0, 0, R_RARO, 0xD28F, 40, 45, 65, 90, 0, TY_PSIQUICO, TY_NONE, 122 },  // 122 psiquico
  { "SCYTHER", 0, 0, R_RARO, 0x7CC4, 70, 110, 80, 105, 2, TY_BICHO, TY_VOLADOR, 123 },  // 123 bicho/volador
  { "JYNX", 0, 0, R_RARO, 0x4DB8, 65, 50, 35, 95, 5, TY_HIELO, TY_PSIQUICO, 124 },  // 124 hielo/psiquico
  { "ELECTABUZZ", 0, 0, R_RARO, 0xBCA1, 65, 83, 57, 105, 0, TY_ELECTRICO, TY_NONE, 125 },  // 125 electrico
  { "MAGMAR", 0, 0, R_RARO, 0xEA87, 65, 95, 57, 93, 3, TY_FUEGO, TY_NONE, 126 },  // 126 fuego
  { "PINSIR", 0, 0, R_RARO, 0x7CC4, 65, 125, 100, 85, 2, TY_BICHO, TY_NONE, 127 },  // 127 bicho
  { "TAUROS", 0, 0, R_RARO, 0x8C4D, 75, 100, 95, 110, 0, TY_NORMAL, TY_NONE, 128 },  // 128 normal
  { "MAGIKARP", 130, 20, R_COMUN, 0x4C98, 20, 10, 55, 80, 1, TY_AGUA, TY_NONE, 129 },  // 129 agua
  { "GYARADOS", 0, 0, R_EVO, 0x4C98, 95, 125, 79, 81, 1, TY_AGUA, TY_VOLADOR, 129 },  // 130 agua/volador
  { "LAPRAS", 0, 0, R_RARO, 0x4C98, 130, 85, 80, 60, 1, TY_AGUA, TY_HIELO, 131 },  // 131 agua/hielo
  { "DITTO", 0, 0, R_RARO, 0x8C4D, 48, 48, 48, 48, 0, TY_NORMAL, TY_NONE, 132 },  // 132 normal
  { "EEVEE", 134, 30, R_COMUN, 0x8C4D, 55, 55, 50, 55, 0, TY_NORMAL, TY_NONE, 133 },  // 133 normal
  { "VAPOREON", 0, 0, R_EVO, 0x4C98, 130, 65, 60, 65, 1, TY_AGUA, TY_NONE, 133 },  // 134 agua
  { "JOLTEON", 0, 0, R_EVO, 0xBCA1, 65, 65, 60, 130, 0, TY_ELECTRICO, TY_NONE, 133 },  // 135 electrico
  { "FLAREON", 0, 0, R_EVO, 0xEA87, 65, 130, 60, 65, 3, TY_FUEGO, TY_NONE, 133 },  // 136 fuego
  { "PORYGON", 0, 0, R_RARO, 0x8C4D, 65, 60, 70, 40, 0, TY_NORMAL, TY_NONE, 137 },  // 137 normal
  { "OMANYTE", 139, 40, R_RARO, 0x9407, 35, 40, 100, 35, 1, TY_ROCA, TY_AGUA, 138 },  // 138 roca/agua
  { "OMASTAR", 0, 0, R_EVO, 0x9407, 70, 60, 125, 55, 1, TY_ROCA, TY_AGUA, 138 },  // 139 roca/agua
  { "KABUTO", 141, 40, R_RARO, 0x9407, 30, 80, 90, 55, 1, TY_ROCA, TY_AGUA, 140 },  // 140 roca/agua
  { "KABUTOPS", 0, 0, R_EVO, 0x9407, 60, 115, 105, 80, 1, TY_ROCA, TY_AGUA, 140 },  // 141 roca/agua
  { "AERODACTYL", 0, 0, R_RARO, 0x9407, 80, 105, 65, 130, 4, TY_ROCA, TY_VOLADOR, 142 },  // 142 roca/volador
  { "SNORLAX", 0, 0, R_RARO, 0x8C4D, 160, 110, 65, 30, 0, TY_NORMAL, TY_NONE, 143 },  // 143 normal
  { "ARTICUNO", 0, 0, R_LEGENDARIO, 0x4DB8, 90, 85, 100, 85, 5, TY_HIELO, TY_VOLADOR, 144 },  // 144 hielo/volador
  { "ZAPDOS", 0, 0, R_LEGENDARIO, 0xBCA1, 90, 90, 85, 100, 0, TY_ELECTRICO, TY_VOLADOR, 145 },  // 145 electrico/volador
  { "MOLTRES", 0, 0, R_LEGENDARIO, 0xEA87, 90, 100, 90, 90, 3, TY_FUEGO, TY_VOLADOR, 146 },  // 146 fuego/volador
  { "DRATINI", 148, 30, R_RARO, 0x5A98, 41, 64, 45, 50, 1, TY_DRAGON, TY_NONE, 147 },  // 147 dragon
  { "DRAGONAIR", 149, 55, R_EVO, 0x5A98, 61, 84, 65, 70, 1, TY_DRAGON, TY_NONE, 147 },  // 148 dragon
  { "DRAGONITE", 0, 0, R_EVO, 0x5A98, 91, 134, 95, 80, 1, TY_DRAGON, TY_VOLADOR, 147 },  // 149 dragon/volador
  { "MEWTWO", 0, 0, R_LEGENDARIO, 0xD28F, 106, 110, 90, 130, 0, TY_PSIQUICO, TY_NONE, 150 },  // 150 psiquico
  { "MEW", 0, 0, R_LEGENDARIO, 0xD28F, 100, 100, 100, 100, 0, TY_PSIQUICO, TY_NONE, 151 },  // 151 psiquico
};

// Nombres oficiales por idioma. FR y DE son los unicos latinos que difieren
// del ingles en gen 1 (ES/IT/PT usan el de DEX_TBL); JA y KO van en UTF-8,
// que se pinta con la fuente U8g2. nullptr = sin nombre propio.
static const char *const DEX_NAME_FR[DEX_COUNT + 1] = {
  nullptr, "BULBIZARRE", "HERBIZARRE", "FLORIZARRE",
  "SALAMECHE", "REPTINCEL", "DRACAUFEU", "CARAPUCE",
  "CARABAFFE", "TORTANK", "CHENIPAN", "CHRYSACIER",
  "PAPILUSION", "ASPICOT", "COCONFORT", "DARDARGNAN",
  "ROUCOOL", "ROUCOUPS", "ROUCARNAGE", nullptr,
  "RATTATAC", "PIAFABEC", "RAPASDEPIC", "ABO",
  nullptr, nullptr, nullptr, "SABELETTE",
  "SABLAIREAU", nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, "MELOFEE",
  "MELODELFE", "GOUPIX", "FEUNARD", "RONDOUDOU",
  "GRODOUDOU", "NOSFERAPTI", "NOSFERALTO", "MYSTHERBE",
  "ORTIDE", "RAFFLESIA", nullptr, nullptr,
  "MIMITOSS", "AEROMITE", "TAUPIQUEUR", "TRIOPIKEUR",
  "MIAOUSS", nullptr, "PSYKOKWAK", "AKWAKWAK",
  "FEROSINGE", "COLOSSINGE", "CANINOS", "ARCANIN",
  "PTITARD", "TETARTE", "TARTARD", nullptr,
  nullptr, nullptr, "MACHOC", "MACHOPEUR",
  "MACKOGNEUR", "CHETIFLOR", "BOUSTIFLOR", "EMPIFLOR",
  nullptr, nullptr, "RACAILLOU", "GRAVALANCH",
  "GROLEM", nullptr, "GALOPA", "RAMOLOSS",
  "FLAGADOSS", "MAGNETI", nullptr, "CANARTICHO",
  nullptr, nullptr, "OTARIA", "LAMANTINE",
  "TADMORV", "GROTADMORV", "KOKIYAS", "CRUSTABRI",
  "FANTOMINUS", "SPECTRUM", "ECTOPLASMA", nullptr,
  "SOPORIFIK", "HYPNOMADE", nullptr, "KRABBOSS",
  "VOLTORBE", nullptr, "NOEUNOEUF", "NOADKOKO",
  "OSSELAIT", "OSSATUEUR", "KICKLEE", "TYGNON",
  "EXCELANGUE", "SMOGO", "SMOGOGO", "RHINOCORNE",
  "RHINOFEROS", "LEVEINARD", "SAQUEDENEU", "KANGOUREX",
  "HYPOTREMPE", "HYPOCEAN", "POISSIRENE", "POISSOROY",
  "STARI", "STAROSS", "M. MIME", "INSECATEUR",
  "LIPPOUTOU", "ELEKTEK", nullptr, "SCARABRUTE",
  nullptr, "MAGICARPE", "LEVIATOR", "LOKHLASS",
  "METAMORPH", "EVOLI", "AQUALI", "VOLTALI",
  "PYROLI", nullptr, "AMONITA", "AMONISTAR",
  nullptr, nullptr, "PTERA", "RONFLEX",
  "ARTIKODIN", "ELECTHOR", "SULFURA", "MINIDRACO",
  "DRACO", "DRACOLOSSE", nullptr, nullptr,
};

static const char *const DEX_NAME_DE[DEX_COUNT + 1] = {
  nullptr, "BISASAM", "BISAKNOSP", "BISAFLOR",
  "GLUMANDA", "GLUTEXO", "GLURAK", "SCHIGGY",
  "SCHILLOK", "TURTOK", "RAUPY", "SAFCON",
  "SMETTBO", "HORNLIU", "KOKUNA", "BIBOR",
  "TAUBSI", "TAUBOGA", "TAUBOSS", "RATTFRATZ",
  "RATTIKARL", "HABITAK", "IBITAK", "RETTAN",
  nullptr, nullptr, nullptr, "SANDAN",
  "SANDAMER", nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, "PIEPI",
  "PIXI", nullptr, "VULNONA", "PUMMELUFF",
  "KNUDDELUFF", nullptr, nullptr, "MYRAPLA",
  "DUFLOR", "GIFLOR", nullptr, "PARASEK",
  "BLUZUK", "OMOT", "DIGDA", "DIGDRI",
  "MAUZI", "SNOBILIKAT", "ENTON", "ENTORON",
  "MENKI", "RASAFF", "FUKANO", "ARKANI",
  "QUAPSEL", "QUAPUTZI", "QUAPPO", nullptr,
  nullptr, "SIMSALA", "MACHOLLO", "MASCHOCK",
  "MACHOMEI", "KNOFENSA", "ULTRIGARIA", "SARZENIA",
  "TENTACHA", "TENTOXA", "KLEINSTEIN", "GEOROK",
  "GEOWAZ", "PONITA", "GALLOPA", "FLEGMON",
  "LAHMUS", "MAGNETILO", nullptr, "PORENTA",
  "DODU", "DODRI", "JUROB", "JUGONG",
  "SLEIMA", "SLEIMOK", "MUSCHAS", "AUSTOS",
  "NEBULAK", "ALPOLLO", nullptr, nullptr,
  "TRAUMATO", nullptr, nullptr, nullptr,
  "VOLTOBAL", "LEKTROBAL", "OWEI", "KOKOWEI",
  "TRAGOSSO", "KNOGGA", "KICKLEE", "NOCKCHAN",
  "SCHLURP", "SMOGON", "SMOGMOG", "RIHORN",
  "RIZEROS", "CHANEIRA", nullptr, "KANGAMA",
  "SEEPER", "SEEMON", "GOLDINI", "GOLKING",
  "STERNDU", nullptr, "PANTIMOS", "SICHLOR",
  "ROSSANA", "ELEKTEK", nullptr, nullptr,
  nullptr, "KARPADOR", "GARADOS", nullptr,
  nullptr, "EVOLI", "AQUANA", "BLITZA",
  "FLAMARA", nullptr, "AMONITAS", "AMOROSO",
  nullptr, nullptr, nullptr, "RELAXO",
  "ARKTOS", nullptr, "LAVADOS", nullptr,
  "DRAGONIR", "DRAGORAN", "MEWTU", nullptr,
};

static const char *const DEX_NAME_JA[DEX_COUNT + 1] = {
  nullptr, "フシギダネ", "フシギソウ", "フシギバナ",
  "ヒトカゲ", "リザード", "リザードン", "ゼニガメ",
  "カメール", "カメックス", "キャタピー", "トランセル",
  "バタフリー", "ビードル", "コクーン", "スピアー",
  "ポッポ", "ピジョン", "ピジョット", "コラッタ",
  "ラッタ", "オニスズメ", "オニドリル", "アーボ",
  "アーボック", "ピカチュウ", "ライチュウ", "サンド",
  "サンドパン", "ニドランF", "ニドリーナ", "ニドクイン",
  "ニドランM", "ニドリーノ", "ニドキング", "ピッピ",
  "ピクシー", "ロコン", "キュウコン", "プリン",
  "プクリン", "ズバット", "ゴルバット", "ナゾノクサ",
  "クサイハナ", "ラフレシア", "パラス", "パラセクト",
  "コンパン", "モルフォン", "ディグダ", "ダグトリオ",
  "ニャース", "ペルシアン", "コダック", "ゴルダック",
  "マンキー", "オコリザル", "ガーディ", "ウインディ",
  "ニョロモ", "ニョロゾ", "ニョロボン", "ケーシィ",
  "ユンゲラー", "フーディン", "ワンリキー", "ゴーリキー",
  "カイリキー", "マダツボミ", "ウツドン", "ウツボット",
  "メノクラゲ", "ドククラゲ", "イシツブテ", "ゴローン",
  "ゴローニャ", "ポニータ", "ギャロップ", "ヤドン",
  "ヤドラン", "コイル", "レアコイル", "カモネギ",
  "ドードー", "ドードリオ", "パウワウ", "ジュゴン",
  "ベトベター", "ベトベトン", "シェルダー", "パルシェン",
  "ゴース", "ゴースト", "ゲンガー", "イワーク",
  "スリープ", "スリーパー", "クラブ", "キングラー",
  "ビリリダマ", "マルマイン", "タマタマ", "ナッシー",
  "カラカラ", "ガラガラ", "サワムラー", "エビワラー",
  "ベロリンガ", "ドガース", "マタドガス", "サイホーン",
  "サイドン", "ラッキー", "モンジャラ", "ガルーラ",
  "タッツー", "シードラ", "トサキント", "アズマオウ",
  "ヒトデマン", "スターミー", "バリヤード", "ストライク",
  "ルージュラ", "エレブー", "ブーバー", "カイロス",
  "ケンタロス", "コイキング", "ギャラドス", "ラプラス",
  "メタモン", "イーブイ", "シャワーズ", "サンダース",
  "ブースター", "ポリゴン", "オムナイト", "オムスター",
  "カブト", "カブトプス", "プテラ", "カビゴン",
  "フリーザー", "サンダー", "ファイヤー", "ミニリュウ",
  "ハクリュー", "カイリュー", "ミュウツー", "ミュウ",
};

static const char *const DEX_NAME_KO[DEX_COUNT + 1] = {
  nullptr, "이상해씨", "이상해풀", "이상해꽃",
  "파이리", "리자드", "리자몽", "꼬부기",
  "어니부기", "거북왕", "캐터피", "단데기",
  "버터플", "뿔충이", "딱충이", "독침붕",
  "구구", "피죤", "피죤투", "꼬렛",
  "레트라", "깨비참", "깨비드릴조", "아보",
  "아보크", "피카츄", "라이츄", "모래두지",
  "고지", "니드런 암", "니드리나", "니드퀸",
  "니드런 수", "니드리노", "니드킹", "삐삐",
  "픽시", "식스테일", "나인테일", "푸린",
  "푸크린", "주뱃", "골뱃", "뚜벅쵸",
  "냄새꼬", "라플레시아", "파라스", "파라섹트",
  "콘팡", "도나리", "디그다", "닥트리오",
  "나옹", "페르시온", "고라파덕", "골덕",
  "망키", "성원숭", "가디", "윈디",
  "발챙이", "슈륙챙이", "강챙이", "캐이시",
  "윤겔라", "후딘", "알통몬", "근육몬",
  "괴력몬", "모다피", "우츠동", "우츠보트",
  "왕눈해", "독파리", "꼬마돌", "데구리",
  "딱구리", "포니타", "날쌩마", "야돈",
  "야도란", "코일", "레어코일", "파오리",
  "두두", "두트리오", "쥬쥬", "쥬레곤",
  "질퍽이", "질뻐기", "셀러", "파르셀",
  "고오스", "고우스트", "팬텀", "롱스톤",
  "슬리프", "슬리퍼", "크랩", "킹크랩",
  "찌리리공", "붐볼", "아라리", "나시",
  "탕구리", "텅구리", "시라소몬", "홍수몬",
  "내루미", "또가스", "또도가스", "뿔카노",
  "코뿌리", "럭키", "덩쿠리", "캥카",
  "쏘드라", "시드라", "콘치", "왕콘치",
  "별가사리", "아쿠스타", "마임맨", "스라크",
  "루주라", "에레브", "마그마", "쁘사이저",
  "켄타로스", "잉어킹", "갸라도스", "라프라스",
  "메타몽", "이브이", "샤미드", "쥬피썬더",
  "부스터", "폴리곤", "암나이트", "암스타",
  "투구", "투구푸스", "프테라", "잠만보",
  "프리져", "썬더", "파이어", "미뇽",
  "신뇽", "망나뇽", "뮤츠", "뮤",
};

// Nombre de la especie en el idioma activo (cae al de DEX_TBL si ese
// idioma no tiene nombre propio para ella).
static inline const char *dexName(int16_t dex) {
  if (dex < 1 || dex > DEX_COUNT) return DEX_TBL[0].name;
  const char *n = (gLang == LANG_FR)   ? DEX_NAME_FR[dex]
                  : (gLang == LANG_DE) ? DEX_NAME_DE[dex]
                  : (gLang == LANG_JA) ? DEX_NAME_JA[dex]
                  : (gLang == LANG_KO) ? DEX_NAME_KO[dex]
                                       : nullptr;
  return n ? n : DEX_TBL[dex].name;
}

// el primer huevo de la partida: iniciales clasicos
static const int16_t CLASSIC_DEX[] = { 1, 4, 7, 25, 133 };
#define NUM_CLASSIC_DEX 5
