#pragma once
#include <stdint.h>

// Efectos de respuesta (docs/NEXT_FEATURES.md, Tarea FX, v1.30). Logica pura, sin hardware,
// probada en el PC (test/test_fx.cpp). El dibujo y los sonidos viven en TamaPoke.ino.
//
// Dos familias:
// - Celebraciones (FxQueue): subida de nivel, medalla, hito de racha y meta de pasos. Salen
//   de una en una y por prioridad, y el temporizador de cada una empieza AL MOSTRARSE, no
//   al ganarse: una medalla ganada con la pantalla apagada ya no caduca sin verse, y la de
//   Nv.10 (que llega en el mismo minuto que el nivel 10) espera a que acabe la subida.
// - Efectos pequenos (FxOverlays): "+N" de las barras, corazon de vinculo, entrenamiento,
//   boton que tiembla o se hunde, "!" del descuido y la eclosion. Cada uno con su reloj;
//   tambien esperan a que se vea la pantalla principal (p. ej. al cerrar el minijuego).
//
// Los relojes son millis() de 32 bits y se comparan siempre por resta sin signo: siguen
// funcionando cuando millis() da la vuelta a los ~49,7 dias.

// ---- celebraciones, en orden de prioridad (menor = antes)
enum : uint8_t { FXB_LEVELUP = 0, FXB_MEDAL, FXB_MILESTONE, FXB_GOAL, FXB_KINDS };

#define FX_LEVELUP_MS 2400UL    // animacion de subida de nivel...
#define FX_GAINS_MS 1500UL      // ...y despues la linea de estadisticas ganadas
#define FX_MEDAL_MS 4000UL
#define FX_MILESTONE_MS 4500UL
#define FX_GOAL_MS 4500UL
#define FX_GOAL_TOP_MS 6000UL   // la meta de 10000 pasos, mas larga (tier 3)
#define FX_GOAL_TOP_TIER 3
#define FX_BANNER_IN_MS 200UL   // los banners entran deslizandose desde arriba
#define FX_QUEUE 8

struct FxEvent {
  uint8_t kind = FXB_LEVELUP;
  uint16_t a = 0, b = 0;  // subida: a = nivel de antes, b = nivel nuevo. Medalla: a = bit.
                          // Hito: a = dias de racha. Meta: a = tier (1..3)
  bool chimed = false;    // subida: el sonido ya sono al llegar al nivel (no repetirlo)
  uint32_t start = 0;     // millis() en que empezo a verse
  uint32_t dur = 0;       // ms en pantalla
};

inline uint32_t fxBannerMs(uint8_t kind, uint16_t a) {
  switch (kind) {
    case FXB_LEVELUP: return FX_LEVELUP_MS + FX_GAINS_MS;
    case FXB_MEDAL: return FX_MEDAL_MS;
    case FXB_MILESTONE: return FX_MILESTONE_MS;
    default: return a >= FX_GOAL_TOP_TIER ? FX_GOAL_TOP_MS : FX_GOAL_MS;
  }
}

struct FxQueue {
  FxEvent q[FX_QUEUE];  // pendientes, ordenadas por prioridad (estable: misma prioridad, en orden)
  uint8_t n = 0;
  FxEvent cur;          // la que se ve ahora (si active)
  bool active = false;

  void clear() { n = 0; active = false; }
  bool busy() const { return active || n; }  // hay algo en pantalla o esperando

  // Encola una celebracion. Una subida pendiente se funde con la nueva (12->13 y 13->14 son
  // una sola 12->14, como tras horas apagada o un Caramelo Raro), y una repetida exacta
  // (misma medalla, mismo hito, misma meta) se descarta. false = cola llena.
  bool push(uint8_t kind, uint16_t a, uint16_t b = 0, bool chimed = false) {
    if (kind >= FXB_KINDS) return false;
    for (uint8_t i = 0; i < n; i++) {
      if (q[i].kind != kind) continue;
      if (kind == FXB_LEVELUP) {
        if (b > q[i].b) q[i].b = b;
        q[i].chimed = q[i].chimed && chimed;  // si una parte no sono, suena al mostrarse
        return true;
      }
      if (q[i].a == a) return true;
    }
    if (n >= FX_QUEUE) return false;
    uint8_t at = n;
    while (at > 0 && q[at - 1].kind > kind) {
      q[at] = q[at - 1];
      at--;
    }
    FxEvent e;
    e.kind = kind;
    e.a = a;
    e.b = b;
    e.chimed = chimed;
    e.dur = fxBannerMs(kind, a);
    q[at] = e;
    n++;
    return true;
  }

  // Avanza la cola: termina la actual si ya cumplio su tiempo y, si la pantalla esta libre
  // (busyScreen = false), empieza la siguiente en `now`. *started = true en el paso en que una
  // empieza (para lanzar su sonido o su pose). Devuelve la que esta en pantalla o nullptr.
  const FxEvent *step(uint32_t now, bool busyScreen, bool *started = nullptr) {
    if (started) *started = false;
    if (active && now - cur.start >= cur.dur) active = false;
    if (!active && !busyScreen && n) {
      cur = q[0];
      for (uint8_t i = 1; i < n; i++) q[i - 1] = q[i];
      n--;
      cur.start = now;
      active = true;
      if (started) *started = true;
    }
    return active ? &cur : nullptr;
  }

  // Quita las de un tipo, tambien la que se ve. Tras un cambio de bicho en la caja (v1.37), la
  // subida de nivel y la medalla que esperaban eran del otro; el hito y la meta son del jugador.
  void drop(uint8_t kind) {
    uint8_t j = 0;
    for (uint8_t i = 0; i < n; i++)
      if (q[i].kind != kind) q[j++] = q[i];
    n = j;
    if (active && cur.kind == kind) active = false;
  }

  // progreso de la actual, 0..1 (0 si no hay)
  float t(uint32_t now) const {
    if (!active || !cur.dur) return 0.0f;
    uint32_t e = now - cur.start;
    return e >= cur.dur ? 1.0f : (float)e / (float)cur.dur;
  }
  uint32_t elapsed(uint32_t now) const { return active ? now - cur.start : 0; }
};

// ---- efectos pequenos
enum : uint8_t {
  FXO_STAT = 0,  // "+N"/"-N" sobre una barra (a = barra 0..3, v = cambio)
  FXO_BOND,      // corazon con "+N" de vinculo (v = subida)
  FXO_TRAIN,     // "ATK +3" y un ataque (a = 0 ATK, 1 DEF, 2 SPE; v = subida)
  FXO_DENY,      // el boton tocado tiembla (a = boton)
  FXO_PRESS,     // el boton tocado se hunde (a = boton)
  FXO_OOPS,      // "!" del descuido
  FXO_HATCH,     // eclosion: el huevo tiembla, salta la cascara, destello
  FXO_BOX,       // caja (v1.37): el bicho (o el huevo) que queda activo sale de una luz
  FXO_KINDS
};

#define FX_FLOAT_MS 900UL      // "+N" de barras y vinculo
#define FX_TRAIN_MS 1500UL
#define FX_DENY_MS 300UL
#define FX_PRESS_MS 120UL
#define FX_OOPS_MS 2000UL
#define FX_HATCH_MS 1200UL
#define FX_APPEAR_MS 600UL     // caja: la luz se encoge y deja ver al que queda activo
#define FX_WAIT_MS 8000UL      // un efecto que no llega a verse en este tiempo se descarta
#define FX_OVERLAYS 10

inline uint32_t fxOverlayMs(uint8_t kind) {
  switch (kind) {
    case FXO_STAT: case FXO_BOND: return FX_FLOAT_MS;
    case FXO_TRAIN: return FX_TRAIN_MS;
    case FXO_DENY: return FX_DENY_MS;
    case FXO_PRESS: return FX_PRESS_MS;
    case FXO_OOPS: return FX_OOPS_MS;
    case FXO_BOX: return FX_APPEAR_MS;
    default: return FX_HATCH_MS;
  }
}

struct FxOverlay {
  bool on = false;
  bool started = false;
  uint8_t kind = 0, a = 0;
  int16_t v = 0;
  uint32_t queued = 0, start = 0;
};

struct FxOverlays {
  FxOverlay o[FX_OVERLAYS];

  void clear() {
    for (auto &x : o) x.on = false;
  }

  // Lanza un efecto. El mismo efecto sobre lo mismo (misma barra, mismo boton) vuelve a
  // empezar en vez de duplicarse; sin hueco libre se reemplaza el mas antiguo.
  void push(uint8_t kind, uint8_t a, int16_t v, uint32_t now) {
    FxOverlay *slot = nullptr;
    for (auto &x : o)
      if (x.on && x.kind == kind && x.a == a) { slot = &x; break; }
    if (!slot)
      for (auto &x : o)
        if (!x.on) { slot = &x; break; }
    if (!slot) {
      slot = &o[0];
      for (auto &x : o)
        if (now - x.queued > now - slot->queued) slot = &x;
    }
    slot->on = true;
    slot->started = false;
    slot->kind = kind;
    slot->a = a;
    slot->v = v;
    slot->queued = now;
    slot->start = 0;
  }

  // shown = la pantalla principal se esta dibujando ahora y a la vista. Los que esperan
  // empiezan en cuanto se ven; los que no llegan a verse en FX_WAIT_MS, o ya acabaron, se van.
  void step(uint32_t now, bool shown) {
    for (auto &x : o) {
      if (!x.on) continue;
      if (!x.started) {
        if (shown) {
          x.started = true;
          x.start = now;
        } else if (now - x.queued > FX_WAIT_MS) {
          x.on = false;
        }
        continue;
      }
      if (now - x.start >= fxOverlayMs(x.kind)) x.on = false;
    }
  }

  // el efecto en marcha de ese tipo (y objetivo, si a >= 0), o nullptr
  const FxOverlay *find(uint8_t kind, int a = -1) const {
    for (auto &x : o)
      if (x.on && x.started && x.kind == kind && (a < 0 || x.a == a)) return &x;
    return nullptr;
  }
  bool waiting(uint8_t kind) const {
    for (auto &x : o)
      if (x.on && !x.started && x.kind == kind) return true;
    return false;
  }

  static float t(const FxOverlay &x, uint32_t now) {
    uint32_t d = fxOverlayMs(x.kind), e = now - x.start;
    return e >= d ? 1.0f : (float)e / (float)d;
  }
};

// ---- vigilantes: detectan una subida de un contador del bicho (nivel, vinculo, descuidos,
// entrenamiento) sin tocar cada sitio que lo cambia. disarm() cuando el contador deja de
// tener sentido (un huevo no tiene nivel): la siguiente lectura solo se apunta, sin evento.
struct FxWatch {
  uint32_t last = 0;
  bool armed = false;

  void disarm() { armed = false; }
  void reset(uint32_t v) {
    last = v;
    armed = true;
  }
  // true si v subio desde la ultima lectura; *from = el valor de antes
  bool rose(uint32_t v, uint32_t *from = nullptr) {
    if (!armed) {
      reset(v);
      return false;
    }
    bool up = v > last;
    if (from) *from = last;
    last = v;  // tambien al bajar: un huevo nuevo, un descuido que resta vinculo
    return up;
  }
};

// vigilante de una necesidad (comida, higiene, felicidad, energia) que se vacia (v1.50): true solo
// en la lectura en que pasa de >0 a 0. Quedarse en 0 no repite el aviso; tiene que volver a subir
// y caer otra vez. Como FxWatch, la primera lectura (o tras disarm) solo se apunta: un bicho que
// arranca ya a 0, o el que despierta de la caja, no avisa.
struct FxZeroWatch {
  uint32_t last = 0;
  bool armed = false;

  void disarm() { armed = false; }
  void reset(uint32_t v) {
    last = v;
    armed = true;
  }
  bool hitZero(uint32_t v) {
    if (!armed) {
      reset(v);
      return false;
    }
    bool hit = last > 0 && v == 0;
    last = v;
    return hit;
  }
};

// como mucho un evento cada `gap` ms (el corazon de vinculo, al caminar con la pantalla encendida)
struct FxGate {
  uint32_t last = 0;
  bool any = false;
  bool allow(uint32_t now, uint32_t gap) {
    if (any && now - last < gap) return false;
    any = true;
    last = now;
    return true;
  }
};

// barra que se desliza de su valor anterior al nuevo en FX_TWEEN_MS
#define FX_TWEEN_MS 400UL
struct FxTween {
  int16_t from = 0, to = 0;
  uint32_t t0 = 0;
  bool init = false;

  int value(uint32_t now) const {
    if (!init) return to;
    uint32_t e = now - t0;
    if (e >= FX_TWEEN_MS) return to;
    return from + (int)((to - from) * (int32_t)e / (int32_t)FX_TWEEN_MS);
  }
  // nuevo destino: se desliza desde donde se ve ahora (si ya iba a medias, desde ahi)
  void target(int v, uint32_t now) {
    if (!init) {
      from = to = (int16_t)v;
      init = true;
      return;
    }
    if (v == to) return;
    from = (int16_t)value(now);
    to = (int16_t)v;
    t0 = now;
  }
  void snap(int v) {
    from = to = (int16_t)v;
    init = true;
  }
};

#define FX_STAT_MIN 3           // cambio minimo de una barra que saca un "+N"
#define FX_BOND_GAP_MS 10000UL  // como mucho un corazon de vinculo cada 10 s
#define FX_CONFETTI 32          // particulas de confeti (medallas y meta de 10000)
#define FX_CONFETTI_MS 1500UL
#define FX_SPARKS 12            // chispas de la subida de nivel

// pequeno pseudoaleatorio determinista (el mismo frame se dibuja igual: renders repetibles)
inline uint32_t fxHash(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352dU;
  x ^= x >> 15;
  x *= 0x846ca68bU;
  x ^= x >> 16;
  return x;
}

// necesidades antes de una accion (comer, banarse, el minijuego...): la diferencia de despues
// saca los "+N" de las barras. Va aqui y no en el sketch: el conversor .ino de PlatformIO pone
// los prototipos antes de las variables globales, y un tipo del sketch no existiria aun.
struct FxNeedSnap {
  uint8_t v[4];
};
