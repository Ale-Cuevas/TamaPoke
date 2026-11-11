#pragma once
#include <stdint.h>

// Ritmo de los niveles (v1.35, docs/GAMEPLAY_TUNING.md #5 CURVE; curva v2, mas rapida, tras la v1.49).
// Logica pura, sin hardware, para probarla en el PC (test/test_growth.cpp).
//
// Hasta la v1.34 el nivel eran las horas de edad: 1 + ageMinutes / 60, siempre igual. Ahora
// cada nivel cuesta mas que el anterior, como cuesta mas criar a un bicho ya fuerte:
//   - Lv 1-10: 20 min por nivel;
//   - Lv 11-89: una curva logaritmica, cost(L) = 20 + 100 * ln(L - 10) / ln(80), redondeada al
//     minuto: 20 en el 11, 120 en el 89-90, sin saltos en los extremos;
//   - Lv 90 en adelante: 120 min (2 h) por nivel.
// La curva de la v1.35 a la v1.49 (la "v1") llegaba a 180 min (3 h): 20 + 160 * ln(L - 10) / ln(80).
// Se queda abajo (LV_COST_V1) para migrar los guardados: el bicho conserva su nivel y lo lleno
// de la barra (growthFromCurveV1, clave "gcv" en pet.cpp).
//
// El nivel se cuenta con su propio contador, growMin (minutos de crecimiento), no con la edad.
// Asi la edad sigue siendo tiempo real (dias en la ficha, despedida a los 3 dias) y los
// niveles que dan el Caramelo Raro y los paseos no envejecen al bicho.

#define GROW_FAST_MIN 20      // Lv 1-10
#define GROW_SLOW_MIN 120     // Lv 90+ (2 h; 180 hasta la v1.49)
#define GROW_CURVE_FIRST 11   // primer nivel de la curva (LV_COST[0])
#define GROW_CURVE_LAST 90    // ultimo nivel de la tabla (cost = GROW_SLOW_MIN)
#define LEVEL_MAX 999         // tope del nivel (como antes: ya no da la vuelta)
#define LEGACY_MINUTES_PER_LEVEL 60  // hasta la v1.34: 1 nivel por hora de edad (migracion)
#define GROW_SLOW_MIN_V1 180  // Lv 90+ en la curva v1 (v1.35-v1.49; migracion)
#define GROW_CURVE_VER 2      // version de la curva (clave NVS "gcv"); sin la clave = v1

// minutos de cada nivel L -> L+1 para L = 11..90. La tabla se comprueba contra la formula en
// test/test_growth.cpp: asi la placa y el PC nunca discrepan por redondeos de coma flotante.
static const uint8_t LV_COST[GROW_CURVE_LAST - GROW_CURVE_FIRST + 1] = {
  20, 36, 45, 52, 57, 61, 64, 67, 70, 73,             // Lv 11-20
  75, 77, 79, 80, 82, 83, 85, 86, 87, 88,             // Lv 21-30
  89, 91, 92, 93, 93, 94, 95, 96, 97, 98,             // Lv 31-40
  98, 99, 100, 100, 101, 102, 102, 103, 104, 104,     // Lv 41-50
  105, 105, 106, 106, 107, 107, 108, 108, 109, 109,   // Lv 51-60
  110, 110, 111, 111, 111, 112, 112, 113, 113, 113,   // Lv 61-70
  114, 114, 115, 115, 115, 116, 116, 116, 117, 117,   // Lv 71-80
  117, 118, 118, 118, 119, 119, 119, 119, 120, 120,   // Lv 81-90
};

// la curva v1 (v1.35-v1.49): solo para migrar guardados (growthFromCurveV1)
static const uint8_t LV_COST_V1[GROW_CURVE_LAST - GROW_CURVE_FIRST + 1] = {
  20, 45, 60, 71, 79, 85, 91, 96, 100, 104,           // Lv 11-20
  108, 111, 114, 116, 119, 121, 123, 126, 128, 129,   // Lv 21-30
  131, 133, 134, 136, 138, 139, 140, 142, 143, 144,   // Lv 31-40
  145, 147, 148, 149, 150, 151, 152, 153, 154, 155,   // Lv 41-50
  156, 156, 157, 158, 159, 160, 161, 161, 162, 163,   // Lv 51-60
  164, 164, 165, 166, 166, 167, 168, 168, 169, 169,   // Lv 61-70
  170, 171, 171, 172, 172, 173, 174, 174, 175, 175,   // Lv 71-80
  176, 176, 177, 177, 178, 178, 179, 179, 180, 180,   // Lv 81-90
};

// minutos de crecimiento para pasar del nivel L al L+1
inline uint16_t levelCost(uint16_t lv) {
  if (lv < GROW_CURVE_FIRST) return GROW_FAST_MIN;
  if (lv > GROW_CURVE_LAST) return GROW_SLOW_MIN;
  return LV_COST[lv - GROW_CURVE_FIRST];
}

// minutos de crecimiento con los que empieza el nivel L (L = 1 empieza en 0)
inline uint32_t levelStart(uint16_t lv) {
  if (lv <= 1) return 0;
  if (lv > LEVEL_MAX) lv = LEVEL_MAX;
  uint16_t top = lv < GROW_CURVE_LAST ? lv : GROW_CURVE_LAST;
  uint32_t m = 0;
  for (uint16_t l = 1; l < top; l++) m += levelCost(l);
  if (lv > GROW_CURVE_LAST) m += (uint32_t)(lv - GROW_CURVE_LAST) * GROW_SLOW_MIN;
  return m;
}

// nivel con g minutos de crecimiento (1..LEVEL_MAX)
inline uint16_t levelFor(uint32_t g) {
  uint16_t lv = 1;
  while (lv < GROW_CURVE_LAST) {
    uint16_t c = levelCost(lv);
    if (g < c) return lv;
    g -= c;
    lv++;
  }
  uint32_t l = GROW_CURVE_LAST + g / GROW_SLOW_MIN;
  return l > LEVEL_MAX ? LEVEL_MAX : (uint16_t)l;
}

// minutos ya hechos del nivel actual (0..levelCost-1). En el tope se queda en el ultimo minuto:
// la barra llena y sin "siguiente nivel".
inline uint16_t levelInto(uint32_t g) {
  uint16_t lv = levelFor(g);
  uint32_t into = g - levelStart(lv);
  uint16_t c = levelCost(lv);
  return into >= c ? (uint16_t)(c - 1) : (uint16_t)into;
}

// sube n niveles de golpe (Caramelo Raro, metas de pasos) y conserva lo lleno que estaba la
// barra: into * coste nuevo / coste viejo. Asi un paseo nunca tira el progreso. Con el tope,
// se queda en LEVEL_MAX.
inline uint32_t addLevels(uint32_t g, uint16_t n) {
  uint16_t lv = levelFor(g);
  if (!n || lv >= LEVEL_MAX) return g;
  uint32_t to = (uint32_t)lv + n;
  if (to > LEVEL_MAX) to = LEVEL_MAX;
  uint32_t into = g - levelStart(lv);  // < levelCost(lv): lv no es el tope
  uint16_t oldC = levelCost(lv), newC = levelCost((uint16_t)to);
  uint32_t inNew = into * newC / oldC;
  if (inNew >= newC) inNew = newC - 1;
  return levelStart((uint16_t)to) + inNew;
}

// migracion de un guardado anterior a la v1.35 (sin la clave "grow"): mismo nivel que daba la
// edad (1 + edad / 60, con el tope de entonces) y la barra igual de llena
// (minutos dentro de la hora * coste nuevo / 60)
inline uint32_t growthFromLegacyAge(uint32_t ageMin) {
  uint32_t lv = 1 + ageMin / LEGACY_MINUTES_PER_LEVEL;
  uint32_t into = ageMin % LEGACY_MINUTES_PER_LEVEL;
  if (lv >= LEVEL_MAX) {
    lv = LEVEL_MAX;
    into = 0;
  }
  return levelStart((uint16_t)lv) + into * levelCost((uint16_t)lv) / LEGACY_MINUTES_PER_LEVEL;
}

// ---------- la curva v1 (v1.35-v1.49), para la migracion ----------

inline uint16_t levelCostV1(uint16_t lv) {
  if (lv < GROW_CURVE_FIRST) return GROW_FAST_MIN;
  if (lv > GROW_CURVE_LAST) return GROW_SLOW_MIN_V1;
  return LV_COST_V1[lv - GROW_CURVE_FIRST];
}

inline uint32_t levelStartV1(uint16_t lv) {
  if (lv <= 1) return 0;
  if (lv > LEVEL_MAX) lv = LEVEL_MAX;
  uint16_t top = lv < GROW_CURVE_LAST ? lv : GROW_CURVE_LAST;
  uint32_t m = 0;
  for (uint16_t l = 1; l < top; l++) m += levelCostV1(l);
  if (lv > GROW_CURVE_LAST) m += (uint32_t)(lv - GROW_CURVE_LAST) * GROW_SLOW_MIN_V1;
  return m;
}

inline uint16_t levelForV1(uint32_t g) {
  uint16_t lv = 1;
  while (lv < GROW_CURVE_LAST) {
    uint16_t c = levelCostV1(lv);
    if (g < c) return lv;
    g -= c;
    lv++;
  }
  uint32_t l = GROW_CURVE_LAST + g / GROW_SLOW_MIN_V1;
  return l > LEVEL_MAX ? LEVEL_MAX : (uint16_t)l;
}

// growMin de la curva v1 -> curva v2: el mismo nivel y la barra igual de llena
// (minutos dentro del nivel * coste nuevo / coste viejo, siempre por debajo del coste nuevo). En
// el tope (999) se queda al principio de LEVEL_MAX, como growthFromLegacyAge.
inline uint32_t growthFromCurveV1(uint32_t g) {
  uint16_t lv = levelForV1(g);
  if (lv >= LEVEL_MAX) return levelStart(LEVEL_MAX);
  uint32_t into = g - levelStartV1(lv);  // < levelCostV1(lv): lv no es el tope
  uint16_t oldC = levelCostV1(lv), newC = levelCost(lv);
  uint32_t inNew = into * newC / oldC;
  if (inNew >= newC) inNew = newC - 1;
  return levelStart(lv) + inNew;
}
