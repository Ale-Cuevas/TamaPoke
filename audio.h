#pragma once
#include <stdint.h>

// Efectos de sonido del juego (cola, no bloqueante). El orden coincide con la
// tabla SFX de audio.cpp.
enum Sfx : uint8_t {
  SFX_TAP = 0,  // tocar / boton
  SFX_EAT,      // comer
  SFX_PLAY,     // punto del minijuego / golpe
  SFX_HEART,    // le gusta / mimo
  SFX_HATCH,    // eclosion
  SFX_EVOLVE,   // evolucion
  SFX_MEDAL,    // medalla / hito
  SFX_DENY,     // accion no permitida
  SFX_BYE,      // despedida
  SFX_LEVEL,    // "+" pequeno: objeto usado, fin del minijuego (la subida de nivel es SFX_LEVELUP)
  SFX_BATTLE,   // jingle de entrada al combate (~2,5 s)
  SFX_HIT,      // golpe normal
  SFX_CRIT,     // golpe critico
  SFX_SPECIAL,  // ataque especial
  SFX_WIN,      // victoria
  SFX_LOSE,     // derrota
  SFX_LEVELUP,  // subida de nivel (v1.30): arpegio que sube, mas agudo que eclosion y evolucion
  SFX_OOPS,     // descuido (v1.30): dos notas que caen, mas suave que SFX_DENY
  SFX_STORE,    // caja (v1.37): la luz entra en su hueco (destello que baja)
  SFX_WAKE,     // caja: sale de el (destello que sube)
  SFX_COUNT
};

void audioBegin();          // init ES8311 + I2S + amplificador + tarea de audio
void sfxPlay(uint8_t id);   // encola un efecto (no bloquea el loop)
void audioSetLevel(uint8_t level);  // volumen 0..VOL_LEVELS-1 (volume.h; 0 = apagado): lo aplica y lo guarda
uint8_t audioLevel();
bool audioEnabled();                // audioLevel() > 0
void audioSetSleeping(bool sleeping);  // dormida: amplificador apagado
bool audioBusy();                      // un efecto pedido en reposo aun puede estar sonando: no dormir la CPU
void audioSetIdle(bool idle);          // sin uso reciente: amplificador apagado (un efecto lo enciende 4 s)
