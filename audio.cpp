#include "audio.h"
#include "board_amoled_216.h"
#include <Arduino.h>
#include <Wire.h>
#include <ESP_I2S.h>
#include <Preferences.h>

// ---------------------------------------------------------------------------
// Audio del TamaPoke: códec ES8311 (DAC -> amplificador PA -> altavoz) por I2S.
// Init del ES8311 portado del driver oficial de Espressif (esp-bsp), fijado a
// MCLK=4.096MHz (256*fs), 16kHz, 16-bit, esclavo I2S. Los efectos son tonos
// cuadrados (estilo Game Boy) sintetizados en una tarea aparte para no
// bloquear el loop de juego.
// ---------------------------------------------------------------------------

#define ES8311_ADDR BOARD_I2C_ADDR_ES8311  // 0x18
#define SAMPLE_RATE 16000

static I2SClass i2s;
static bool gReady = false;
static bool gOn = true;
static bool gSleeping = false;
static bool gIdle = false;          // sin interaccion reciente (ver power.h): el amplificador consume en reposo
static uint32_t gAmpHoldUntil = 0;  // un efecto pedido en reposo mantiene el amplificador encendido hasta aqui
static bool gAmpHold = false;       // (indicador aparte: asi la comparacion no falla al dar la vuelta millis())
static uint32_t gAmpOnAt = 0;       // cuando se encendio el amplificador
static QueueHandle_t gQ = nullptr;

// El NS4150B tarda bastante mas de 8 ms en estabilizarse tras cada apagado, asi
// que encenderlo justo antes de cada efecto se comia los cortos: el jingle de
// arranque (440 ms) se oia y los pitidos de 35 ms no. Se deja encendido mientras
// haya sonido y la mascota este despierta, como en el ejemplo verificado de
// Waveshare, y solo se apaga al dormir o al silenciar.
static void updateAmplifierPower() {
  // gReady evita encenderlo en una placa donde el codec no arranco: ahi el
  // amplificador solo aportaria siseo, porque no va a sonar nada.
  if (gAmpHold && (int32_t)(gAmpHoldUntil - millis()) <= 0) gAmpHold = false;
  bool idleOff = gIdle && !gAmpHold;
  static int last = -1;  // solo se escribe el pin si cambia
  int want = (gReady && gOn && !gSleeping && !idleOff) ? HIGH : LOW;
  if (want != last) {
    last = want;
    if (want == HIGH) gAmpOnAt = millis();
    digitalWrite(BOARD_AUDIO_PA, want);
  }
}

// ---- I2C del códec ----
static bool esW(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}
static uint8_t esR(uint8_t reg) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(ES8311_ADDR, 1);
  return Wire.available() ? Wire.read() : 0;
}

// Secuencia de init VERIFICADA EN ESTA PLACA (proyecto PlaneRadar2.0, misma
// Waveshare 1.75). Clave: reloj DERIVADO DEL BCLK (reg01=0xBF, sin MCLK externo)
// y referencia interna que alimenta el DAC (reg44=0x58); sin esos dos el codec
// respondia por I2C pero no salia audio. 16kHz, 16-bit, esclavo I2S.
static bool es8311Init() {
  Wire.beginTransmission(ES8311_ADDR);
  if (Wire.endTransmission() != 0) return false;

  // open()
  esW(0x0D, 0xFA); esW(0x44, 0x08); esW(0x44, 0x08);  // power up + quirk de 1a escritura
  esW(0x01, 0x30); esW(0x02, 0x00); esW(0x03, 0x10); esW(0x16, 0x24);
  esW(0x04, 0x10); esW(0x05, 0x00); esW(0x0B, 0x00); esW(0x0C, 0x00);
  esW(0x10, 0x1F); esW(0x11, 0x7F);
  esW(0x00, 0x80); esW(0x00, 0x80);                   // reset clock, esclavo
  esW(0x01, 0xBF);                                    // clk src = BCLK (sin MCLK externo)
  { uint8_t r = esR(0x06); r &= ~0x20; esW(0x06, r); }  // SCLK no invertido
  esW(0x13, 0x10); esW(0x1B, 0x0A); esW(0x1C, 0x6A);
  esW(0x44, 0x58);                                    // referencia interna -> alimenta el DAC

  // config_sample(): BCLK*8 = DIG_MCLK
  esW(0x02, 0x18); esW(0x05, 0x00); esW(0x03, 0x10); esW(0x04, 0x20);
  { uint8_t r = esR(0x07); r &= 0xC0; esW(0x07, r); }
  esW(0x08, 0xFF);
  { uint8_t r = esR(0x06); r &= 0xE0; r |= 0x03; esW(0x06, r); }  // bclk_div=4

  // formato I2S 16-bit
  esW(0x09, 0x0C); esW(0x0A, 0x0C);

  // start() DAC esclavo
  esW(0x00, 0x80); esW(0x01, 0xBF); esW(0x09, 0x0C); esW(0x0A, 0x0C);
  esW(0x17, 0xBF); esW(0x0E, 0x02); esW(0x12, 0x00); esW(0x14, 0x1A);
  esW(0x0D, 0x01); esW(0x15, 0x40); esW(0x37, 0x08); esW(0x45, 0x00);

  // volumen + unmute
  // volumen DAC: pasos de 0.5 dB, 0xBF = 0 dB. 0xAB = -10 dB (la mitad de fuerte
  // al oido); para ajustarlo, cada 2 unidades es 1 dB
  esW(0x32, 0xAB);
  { uint8_t r = esR(0x31); r &= 0x9F; esW(0x31, r); }  // unmute
  return true;
}

// ---- sintetizador de tono cuadrado ----
struct Note { uint16_t f, ms; };

static const Note N_TAP[]    = {{880, 35}};
static const Note N_EAT[]    = {{660, 45}, {0, 12}, {660, 45}};
static const Note N_PLAY[]   = {{784, 45}, {988, 60}};
static const Note N_HEART[]  = {{1047, 55}, {1319, 90}};
static const Note N_HATCH[]  = {{523, 80}, {659, 80}, {784, 110}, {1047, 170}};
static const Note N_EVOLVE[] = {{523, 80}, {659, 80}, {784, 80}, {1047, 90}, {1319, 230}};
static const Note N_MEDAL[]  = {{784, 70}, {0, 25}, {784, 70}, {0, 25}, {1047, 200}};
static const Note N_DENY[]   = {{300, 110}, {200, 170}};
static const Note N_BYE[]    = {{784, 150}, {659, 150}, {523, 280}};
static const Note N_LEVEL[]  = {{784, 70}, {1047, 130}};
// combate (v1.24). La tarea de audio no puede parar ni repetir un sonido, asi
// que no hay musica de fondo: un jingle de entrada y efectos cortos.
static const Note N_BATTLE[] = {
  {392, 90}, {0, 30}, {392, 90}, {0, 30}, {466, 90}, {0, 30}, {523, 180},
  {392, 90}, {0, 30}, {392, 90}, {0, 30}, {698, 90}, {0, 30}, {659, 180},
  {587, 120}, {523, 120}, {466, 120}, {523, 240},
  {784, 150}, {0, 50}, {784, 150}, {0, 50}, {1047, 420},
};
static const Note N_HIT[]     = {{220, 40}, {150, 70}};
static const Note N_CRIT[]    = {{1245, 30}, {311, 40}, {165, 90}};
static const Note N_SPECIAL[] = {{523, 40}, {784, 40}, {1047, 40}, {1568, 90}, {392, 50}, {196, 90}};
static const Note N_WIN[]     = {{523, 100}, {659, 100}, {784, 100}, {1047, 240}, {0, 60}, {988, 120}, {1047, 360}};
static const Note N_LOSE[]    = {{392, 200}, {370, 200}, {349, 200}, {330, 450}};
// efectos de respuesta (v1.30, fx.h). La subida de nivel empieza en 784 Hz: eclosion y evolucion
// empiezan en 523, asi que no se confunden; el descuido cae suave, sin el zumbido grave de DENY
static const Note N_LEVELUP[] = {{784, 60}, {988, 60}, {1175, 60}, {1568, 240}};
static const Note N_OOPS[]    = {{392, 90}, {330, 160}};

struct SfxDef { const Note *n; uint8_t len; };
#define SFX_ENTRY(n) { n, (uint8_t)(sizeof(n) / sizeof(n[0])) }
// en el orden del enum Sfx (audio.h). Sin tamano fijo a proposito: con
// SFX[SFX_COUNT], un efecto anadido al enum y olvidado aqui compilaba y sonaba
// a silencio; asi lo para el static_assert.
static const SfxDef SFX[] = {
  SFX_ENTRY(N_TAP), SFX_ENTRY(N_EAT), SFX_ENTRY(N_PLAY), SFX_ENTRY(N_HEART), SFX_ENTRY(N_HATCH),
  SFX_ENTRY(N_EVOLVE), SFX_ENTRY(N_MEDAL), SFX_ENTRY(N_DENY), SFX_ENTRY(N_BYE), SFX_ENTRY(N_LEVEL),
  SFX_ENTRY(N_BATTLE), SFX_ENTRY(N_HIT), SFX_ENTRY(N_CRIT), SFX_ENTRY(N_SPECIAL), SFX_ENTRY(N_WIN),
  SFX_ENTRY(N_LOSE), SFX_ENTRY(N_LEVELUP), SFX_ENTRY(N_OOPS),
};
static_assert(sizeof(SFX) / sizeof(SFX[0]) == SFX_COUNT, "SFX[] tiene que tener un efecto por cada Sfx");

static int16_t buf[256 * 2];  // estéreo intercalado (L=R)

// reproduce un tono (o silencio si f==0) con rampa de ataque/caida anti-click
static void playTone(uint16_t f, uint16_t ms) {
  int total = SAMPLE_RATE * ms / 1000;
  int half = f ? (SAMPLE_RATE / (2 * f)) : 0;  // medio periodo en muestras
  const int16_t amp = 5000;
  int phase = 0, done = 0;
  bool high = true;
  while (done < total) {
    int n = total - done; if (n > 256) n = 256;
    for (int i = 0; i < n; i++) {
      int16_t s = 0;
      if (f) {
        s = high ? amp : -amp;
        int idx = done + i;
        if (idx < 64) s = (int16_t)(s * idx / 64);                 // ataque
        else if (idx > total - 96) s = (int16_t)(s * (total - idx) / 96);  // caida
        if (++phase >= half) { phase = 0; high = !high; }
      }
      buf[i * 2] = s; buf[i * 2 + 1] = s;
    }
    i2s.write((uint8_t *)buf, n * 4);
    done += n;
  }
}

static void audioTask(void *) {
  uint8_t id;
  for (;;) {
    if (xQueueReceive(gQ, &id, portMAX_DELAY) && gOn && !gSleeping && gReady &&
        id < SFX_COUNT) {
      const SfxDef &d = SFX[id];
      // el NS4150B tarda mas de 8 ms en estabilizarse: si se acaba de encender, el efecto espera
      int32_t since = (int32_t)(millis() - gAmpOnAt);
      if (since >= 0 && since < 50) vTaskDelay(pdMS_TO_TICKS(50 - since));
      for (uint8_t i = 0; i < d.len; i++) playTone(d.n[i].f, d.n[i].ms);
    }
  }
}

void audioBegin() {
  // I2S primero: arranca el MCLK que necesita el códec para engancharse
  pinMode(BOARD_AUDIO_PA, OUTPUT);
  digitalWrite(BOARD_AUDIO_PA, LOW);   // hasta saber si el sonido esta activado

  i2s.setPins(BOARD_I2S_BCLK, BOARD_I2S_LRCK, BOARD_I2S_DOUT, BOARD_I2S_DIN, BOARD_I2S_MCLK);
  if (!i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT,
                 I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) {
    Serial.println("I2S begin fallo");
    return;
  }
  if (!es8311Init()) { Serial.println("ES8311 no responde (audio off)"); return; }

  Preferences p;
  p.begin("tamapoke", true);
  gOn = p.getBool("snd", true);
  p.end();

  gReady = true;
  updateAmplifierPower();
  gQ = xQueueCreate(8, sizeof(uint8_t));
  xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, 1, nullptr, 0);
  sfxPlay(SFX_HATCH);  // jingle de arranque (confirma que suena)
}

bool audioBusy() { return gAmpHold && (int32_t)(gAmpHoldUntil - millis()) > 0; }

void sfxPlay(uint8_t id) {
  if (gReady && gOn && gQ) {
    if (gIdle) {  // un aviso en reposo (p. ej. "listo para evolucionar") tiene que oirse
      gAmpHoldUntil = millis() + 4000;
      gAmpHold = true;
      updateAmplifierPower();
    }
    xQueueSend(gQ, &id, 0);  // descarta si la cola esta llena
  }
}

void audioSetEnabled(bool on) {
  gOn = on;
  updateAmplifierPower();
  Preferences p;
  p.begin("tamapoke", false);
  p.putBool("snd", on);
  p.end();
}
bool audioEnabled() { return gOn; }

void audioSetSleeping(bool sleeping) {
  if (gSleeping == sleeping) return;
  gSleeping = sleeping;
  updateAmplifierPower();  // durmiendo no hay efectos: amp apagado, sin consumo
}

void audioSetIdle(bool idle) {
  gIdle = idle;
  updateAmplifierPower();  // tambien cuando vence el mantenimiento de un aviso
}
