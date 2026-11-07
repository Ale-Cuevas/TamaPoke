#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Actualizaciones por Wi-Fi (docs/NEXT_FEATURES.md, Tarea OTA, v1.42). Logica pura, sin
// hardware, probada en el PC (test/test_ota.cpp): comparar versiones, leer el manifiesto,
// decidir si se puede instalar, el SHA-256 y la descarga por pasos. El Wi-Fi, HTTPS y la
// escritura en flash viven en ota.cpp; la pantalla, en TamaPoke.ino.
//
// La descarga nunca llama a end() (que cambia la particion de arranque) sin haber
// comprobado antes el tamano y el SHA-256 de lo recibido: cualquier fallo aborta y deja
// el firmware actual intacto.

#define OTA_BOARD "amoled216"            // el de la 1.75 tendra el suyo (FEATURES.md 3e)
#define OTA_SLOT_BYTES 0x300000UL        // app3M_fat9M_16MB: dos ranuras de 3 MB
#define OTA_MIN_BAT 50                   // % con bateria y sin USB (decidido 2026-11-05)
#define OTA_NODATA_MS 20000UL            // sin datos tanto tiempo: error
#define OTA_CHECK_MS 10000UL             // conectar + leer el manifiesto, como mucho
#define OTA_WIFI_MS 8000UL               // parte de ese tiempo para unirse a la red
#define OTA_MANIFEST_MAX 2048            // bytes de manifiesto que se aceptan
#define OTA_SPRITE_PACK 1                // version del paquete de sprites que espera este firmware

// ---------------------------------------------------------------- errores (FR-OTA.11)
// Cada uno es un mensaje en pantalla, nunca una pantalla colgada.
enum : uint8_t {
  OTAE_NONE = 0,
  OTAE_NOWIFI,      // no hay red configurada
  OTAE_NOTFOUND,    // la red no aparece
  OTAE_PASSWORD,    // contrasena incorrecta
  OTAE_NOINTERNET,  // hay red, pero no se llega al servidor (DNS, conexion)
  OTAE_CERT,        // el certificado no es valido
  OTAE_SERVER,      // respuesta HTTP mala o manifiesto que no se entiende
  OTAE_TIMEOUT,     // 20 s sin datos (o comprobar tardo mas de 10 s)
  OTAE_CHECKSUM,    // el SHA-256 no coincide
  OTAE_WRITE,       // fallo al escribir la flash
  OTAE_BATTERY,     // bateria baja
  OTAE_MEMORY,      // no queda RAM interna para HTTPS
  OTAE_BOARD,       // el manifiesto es de otra placa
  OTAE_LOST,        // la conexion se corto a media descarga
  OTAE_COUNT
};

// ---------------------------------------------------------------- versiones
// "1.42" -> 1 y 42. Mayor y menor como enteros, asi 1.9 < 1.10. Nada mas: ni espacios,
// ni "v" delante, ni un tercer numero.
inline bool otaParseVer(const char *s, int &maj, int &mnr) {
  if (!s) return false;
  int part[2] = { 0, 0 };
  for (int i = 0; i < 2; i++) {
    int digits = 0;
    while (*s >= '0' && *s <= '9') {
      if (++digits > 4) return false;
      part[i] = part[i] * 10 + (*s++ - '0');
    }
    if (!digits) return false;
    if (i == 0 && *s++ != '.') return false;
  }
  if (*s) return false;
  maj = part[0];
  mnr = part[1];
  return true;
}
inline bool otaVerValid(const char *s) {
  int a, b;
  return otaParseVer(s, a, b);
}
// <0 si a es anterior a b, 0 si son iguales, >0 si a es posterior. Un texto mal formado
// cuenta como anterior a todo (nunca parece una version nueva).
inline int otaVerCmp(const char *a, const char *b) {
  int am, an, bm, bn;
  bool va = otaParseVer(a, am, an), vb = otaParseVer(b, bm, bn);
  if (!va || !vb) return (int)va - (int)vb;
  if (am != bm) return am < bm ? -1 : 1;
  if (an != bn) return an < bn ? -1 : 1;
  return 0;
}

// ---------------------------------------------------------------- manifiesto (FR-OTA.18)
// {"board":"amoled216","version":"1.43","size":1468000,"sha256":"...","url":"https://...",
//  "min_sprites":1,"notes":"..."}. Un JSON plano: un lector pequeno en vez de una libreria.
// Las claves desconocidas (y objetos o listas anidados) se saltan, para que un manifiesto
// futuro con mas campos no rompa a las placas viejas.
struct OtaManifest {
  char board[16];
  char version[12];
  uint32_t size;
  char sha256[65];      // 64 hex en minusculas
  char url[200];
  uint8_t minSprites;
  char notes[96];       // ASCII: lo que no lo es se cambia por '?' (las fuentes son CP437)
};

enum : uint8_t {
  OTAM_OK = 0,
  OTAM_JSON,     // no es un JSON plano valido
  OTAM_FIELD,    // falta un campo obligatorio
  OTAM_BOARD,    // es de otra placa
  OTAM_VERSION,  // version mal formada
  OTAM_SIZE,     // 0 o mas grande que la ranura
  OTAM_SHA,      // no son 64 hex
  OTAM_URL,      // no es http(s)://
};

namespace otajson {
struct Rd {
  const char *p, *end;
  void ws() { while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++; }
  bool eat(char c) {
    ws();
    if (p < end && *p == c) { p++; return true; }
    return false;
  }
  static int hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  }
  // Lee una cadena en out (cap incluye el '\0'; lo que no cabe se corta). out puede ser
  // nullptr para saltarla. Los escapes \uXXXX fuera del ASCII imprimible dan '?'.
  bool str(char *out, size_t cap) {
    ws();
    if (p >= end || *p != '"') return false;
    p++;
    size_t n = 0;
    while (p < end && *p != '"') {
      char c = *p++;
      if ((unsigned char)c < 0x20) return false;
      if (c == '\\') {
        if (p >= end) return false;
        char e = *p++;
        switch (e) {
          case '"': case '\\': case '/': c = e; break;
          case 'n': case 't': case 'r': case 'b': case 'f': c = ' '; break;
          case 'u': {
            if (end - p < 4) return false;
            int v = 0;
            for (int i = 0; i < 4; i++) {
              int h = hex(*p++);
              if (h < 0) return false;
              v = v * 16 + h;
            }
            c = (v >= 0x20 && v < 0x7f) ? (char)v : '?';
            break;
          }
          default: return false;
        }
      } else if ((unsigned char)c >= 0x80) {
        // UTF-8 crudo: un '?' por caracter (los bytes de continuacion no cuentan)
        if (((unsigned char)c & 0xC0) == 0x80) continue;
        c = '?';
      }
      if (out && n + 1 < cap) out[n++] = c;
    }
    if (p >= end) return false;
    p++;  // la comilla de cierre
    if (out && cap) out[n] = 0;
    return true;
  }
  bool num(uint32_t &v) {
    ws();
    if (p >= end || *p < '0' || *p > '9') return false;
    uint64_t x = 0;
    while (p < end && *p >= '0' && *p <= '9') {
      x = x * 10 + (uint64_t)(*p++ - '0');
      if (x > 0xFFFFFFFFull) return false;
    }
    if (p < end && (*p == '.' || *p == 'e' || *p == 'E')) return false;  // solo enteros
    v = (uint32_t)x;
    return true;
  }
  bool lit(const char *w) {
    size_t n = strlen(w);
    if ((size_t)(end - p) < n || memcmp(p, w, n) != 0) return false;
    p += n;
    return true;
  }
  // Salta un valor cualquiera (tambien anidado), para las claves que no se usan.
  bool skip(int depth = 0) {
    ws();
    if (p >= end || depth > 8) return false;
    char c = *p;
    if (c == '"') return str(nullptr, 0);
    if (c == '{' || c == '[') {
      char close = c == '{' ? '}' : ']';
      p++;
      if (eat(close)) return true;
      for (;;) {
        if (c == '{') {
          if (!str(nullptr, 0) || !eat(':')) return false;
        }
        if (!skip(depth + 1)) return false;
        if (eat(close)) return true;
        if (!eat(',')) return false;
      }
    }
    if (c == '-' || (c >= '0' && c <= '9')) {
      p++;
      while (p < end && ((*p >= '0' && *p <= '9') || *p == '.' || *p == 'e' || *p == 'E' ||
                         *p == '+' || *p == '-')) p++;
      return true;
    }
    return lit("true") || lit("false") || lit("null");
  }
};
}  // namespace otajson

inline bool otaIsHex64(const char *s) {
  for (int i = 0; i < 64; i++)
    if (otajson::Rd::hex(s[i]) < 0) return false;
  return s[64] == 0;
}

// Lee y valida el manifiesto. board es el de esta placa (OTA_BOARD).
inline uint8_t otaParseManifest(const char *json, size_t len, OtaManifest &m,
                                const char *board = OTA_BOARD) {
  memset(&m, 0, sizeof(m));
  otajson::Rd r{ json, json + len };
  enum { F_BOARD = 1, F_VER = 2, F_SIZE = 4, F_SHA = 8, F_URL = 16 };
  int have = 0;
  if (!json || !r.eat('{')) return OTAM_JSON;
  if (!r.eat('}')) {
    for (;;) {
      char key[16];
      if (!r.str(key, sizeof(key)) || !r.eat(':')) return OTAM_JSON;
      bool ok;
      if (!strcmp(key, "board")) { ok = r.str(m.board, sizeof(m.board)); have |= F_BOARD; }
      else if (!strcmp(key, "version")) { ok = r.str(m.version, sizeof(m.version)); have |= F_VER; }
      else if (!strcmp(key, "size")) { ok = r.num(m.size); have |= F_SIZE; }
      else if (!strcmp(key, "sha256")) { ok = r.str(m.sha256, sizeof(m.sha256)); have |= F_SHA; }
      else if (!strcmp(key, "url")) { ok = r.str(m.url, sizeof(m.url)); have |= F_URL; }
      else if (!strcmp(key, "min_sprites")) {
        uint32_t v = 0;
        ok = r.num(v);
        m.minSprites = v > 255 ? 255 : (uint8_t)v;
      }
      else if (!strcmp(key, "notes")) ok = r.str(m.notes, sizeof(m.notes));
      else ok = r.skip();
      if (!ok) return OTAM_JSON;
      if (r.eat('}')) break;
      if (!r.eat(',')) return OTAM_JSON;
    }
  }
  r.ws();
  if (r.p != r.end) return OTAM_JSON;  // basura detras del objeto
  if ((have & (F_BOARD | F_VER | F_SIZE | F_SHA | F_URL)) != (F_BOARD | F_VER | F_SIZE | F_SHA | F_URL))
    return OTAM_FIELD;
  if (strcmp(m.board, board) != 0) return OTAM_BOARD;
  if (!otaVerValid(m.version)) return OTAM_VERSION;
  if (m.size == 0 || m.size > OTA_SLOT_BYTES) return OTAM_SIZE;
  for (char *c = m.sha256; *c; c++)
    if (*c >= 'A' && *c <= 'F') *c = (char)(*c - 'A' + 'a');
  if (strlen(m.sha256) != 64 || !otaIsHex64(m.sha256)) return OTAM_SHA;
  if (strncmp(m.url, "https://", 8) != 0 && strncmp(m.url, "http://", 7) != 0) return OTAM_URL;
  if (strlen(m.url) + 1 >= sizeof(m.url)) return OTAM_URL;  // cortada: no sirve
  return OTAM_OK;
}

// Que hacer con un manifiesto valido.
enum : uint8_t { OTAV_UPTODATE = 0, OTAV_NEWER, OTAV_SPRITES };
// OTAV_SPRITES: hay version nueva, pero pide un paquete de sprites mas nuevo que el de
// este firmware. Se puede instalar igual; la pantalla avisa de que hay que cargar los
// sprites nuevos con el instalador web (FR-OTA.15). force: OTA FORCE, acepta la misma
// version o una anterior.
inline uint8_t otaEvaluate(const OtaManifest &m, const char *current, bool force,
                           uint8_t spritePack = OTA_SPRITE_PACK) {
  if (!force && otaVerCmp(m.version, current) <= 0) return OTAV_UPTODATE;
  return m.minSprites > spritePack ? OTAV_SPRITES : OTAV_NEWER;
}

// ---------------------------------------------------------------- condiciones (FR-OTA.6)
enum : uint8_t { OTAB_OK = 0, OTAB_BATTERY, OTAB_BUSY, OTAB_GAME };
// usb: alimentacion externa. batPct: 0-100, -1 si no hay bateria. busy: evolucion o
// ceremonia. game: minijuego, saco o combate abiertos.
inline uint8_t otaCanInstall(bool usb, int batPct, bool busy, bool game) {
  if (!usb && batPct < OTA_MIN_BAT) return OTAB_BATTERY;
  if (busy) return OTAB_BUSY;
  if (game) return OTAB_GAME;
  return OTAB_OK;
}

// ---------------------------------------------------------------- SHA-256
// Portable (FIPS 180-4), para que el PC y la placa comprueben igual. En la ESP32-S3 a
// 240 MHz pasa de varios MB/s: la red es mucho mas lenta.
struct OtaSha256 {
  uint32_t h[8];
  uint64_t bits;
  uint8_t buf[64];
  uint8_t n;

  OtaSha256() { reset(); }
  void reset() {
    static const uint32_t H0[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
    memcpy(h, H0, sizeof(h));
    bits = 0;
    n = 0;
  }
  static uint32_t ror(uint32_t x, int r) { return (x >> r) | (x << (32 - r)); }
  void block(const uint8_t *p) {
    static const uint32_t K[64] = {
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
      0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
      0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
      0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
      0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
      0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
      0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
      0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
      w[i] = (uint32_t)p[i * 4] << 24 | (uint32_t)p[i * 4 + 1] << 16 | (uint32_t)p[i * 4 + 2] << 8 | p[i * 4 + 3];
    for (int i = 16; i < 64; i++) {
      uint32_t s0 = ror(w[i - 15], 7) ^ ror(w[i - 15], 18) ^ (w[i - 15] >> 3);
      uint32_t s1 = ror(w[i - 2], 17) ^ ror(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; i++) {
      uint32_t t1 = hh + (ror(e, 6) ^ ror(e, 11) ^ ror(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
      uint32_t t2 = (ror(a, 2) ^ ror(a, 13) ^ ror(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
      hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
  }
  void update(const uint8_t *p, size_t len) {
    bits += (uint64_t)len * 8;
    while (len) {
      if (n == 0 && len >= 64) {
        block(p);
        p += 64;
        len -= 64;
        continue;
      }
      size_t room = 64u - n;
      size_t take = room < len ? room : len;
      memcpy(buf + n, p, take);
      n += (uint8_t)take;
      p += take;
      len -= take;
      if (n == 64) {
        block(buf);
        n = 0;
      }
    }
  }
  // Escribe los 64 hex (minusculas) + '\0' en out[65]. Deja el objeto consumido.
  void finishHex(char *out) {
    uint64_t total = bits;
    uint8_t pad = 0x80;
    update(&pad, 1);
    uint8_t zero = 0;
    while (n != 56) update(&zero, 1);
    uint8_t len[8];
    for (int i = 0; i < 8; i++) len[i] = (uint8_t)(total >> (56 - 8 * i));
    update(len, 8);
    static const char *HX = "0123456789abcdef";
    for (int i = 0; i < 8; i++)
      for (int j = 0; j < 4; j++) {
        uint8_t b = (uint8_t)(h[i] >> (24 - 8 * j));
        out[i * 8 + j * 2] = HX[b >> 4];
        out[i * 8 + j * 2 + 1] = HX[b & 15];
      }
    out[64] = 0;
  }
};

// ---------------------------------------------------------------- descarga por pasos
// La flash (Update en la placa) y el flujo de bytes (HTTP) llegan como interfaces, para
// que los tests los sustituyan.
struct OtaSink {
  virtual bool begin(uint32_t size) = 0;
  virtual size_t write(const uint8_t *p, size_t n) = 0;
  virtual bool end() = 0;    // cierra la imagen y cambia la particion de arranque
  virtual void abort() = 0;  // descarta lo escrito: el firmware actual sigue
  virtual ~OtaSink() {}
};
struct OtaSource {
  // >0 bytes leidos, 0 si ahora no hay nada, <0 si la conexion se cerro
  virtual int read(uint8_t *buf, size_t cap) = 0;
  virtual ~OtaSource() {}
};

enum : uint8_t { OTD_IDLE = 0, OTD_RUN, OTD_DONE, OTD_ERROR, OTD_CANCEL };

struct OtaDownload {
  uint8_t state = OTD_IDLE;
  uint8_t err = OTAE_NONE;
  uint32_t total = 0, got = 0;
  uint32_t lastData = 0;
  char want[65] = { 0 };
  OtaSha256 sha;

  bool active() const { return state == OTD_RUN; }
  uint8_t percent() const { return total ? (uint8_t)((uint64_t)got * 100 / total) : 0; }

  bool start(OtaSink &sink, uint32_t size, const char *sha256hex, uint32_t now) {
    got = 0;
    total = size;
    lastData = now;
    sha.reset();
    strncpy(want, sha256hex ? sha256hex : "", 64);
    want[64] = 0;
    if (!size || size > OTA_SLOT_BYTES || !otaIsHex64(want)) return fail(nullptr, OTAE_SERVER);
    if (!sink.begin(size)) return fail(nullptr, OTAE_WRITE);
    state = OTD_RUN;
    err = OTAE_NONE;
    return true;
  }

  // Un paso: lee lo que haya (hasta maxBytes) y lo escribe. Devuelve el estado.
  uint8_t poll(OtaSource &src, OtaSink &sink, uint32_t now, uint8_t *buf, size_t cap,
               size_t maxBytes) {
    if (state != OTD_RUN) return state;
    size_t done = 0;
    while (done < maxBytes && state == OTD_RUN) {
      size_t want_n = cap;
      if (want_n > total - got) want_n = total - got;
      int n = src.read(buf, want_n);
      if (n < 0) {
        fail(&sink, OTAE_LOST);
        break;
      }
      if (n == 0) {
        if (now - lastData > OTA_NODATA_MS) fail(&sink, OTAE_TIMEOUT);
        break;
      }
      if ((size_t)n > total - got) {  // el servidor manda mas de lo anunciado
        fail(&sink, OTAE_SERVER);
        break;
      }
      sha.update(buf, (size_t)n);
      if (sink.write(buf, (size_t)n) != (size_t)n) {
        fail(&sink, OTAE_WRITE);
        break;
      }
      got += (uint32_t)n;
      done += (size_t)n;
      lastData = now;
      if (got == total) finish(sink);
    }
    return state;
  }

  // La tecla USER, hasta el cambio final (FR-OTA.8).
  void cancel(OtaSink &sink) {
    if (state != OTD_RUN) return;
    sink.abort();
    state = OTD_CANCEL;
  }

 private:
  void finish(OtaSink &sink) {
    char hex[65];
    sha.finishHex(hex);
    if (strcmp(hex, want) != 0) {
      fail(&sink, OTAE_CHECKSUM);
      return;
    }
    if (!sink.end()) {
      fail(&sink, OTAE_WRITE);
      return;
    }
    state = OTD_DONE;
  }
  bool fail(OtaSink *sink, uint8_t e) {
    if (sink) sink->abort();
    err = e;
    state = OTD_ERROR;
    return false;
  }
};
