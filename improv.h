#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Improv Wi-Fi por serie (https://www.improv-wifi.com/serial/), v1.42, FR-OTA.1: tras
// flashear, el instalador web (ESP Web Tools) ofrece "Connect to Wi-Fi" y manda el nombre
// y la contrasena de la red por el mismo USB. Nadie teclea una contrasena en la placa.
// Logica pura, probada en el PC (test/test_ota.cpp); el Wi-Fi lo pone TamaPoke.ino.
//
// Un paquete: "IMPROV" + version(1) + tipo + longitud + datos + suma (los bytes anteriores
// sumados, mod 256). Llegan mezclados con las ordenes de texto (TAP, PUT, ...), asi que
// SerialFramer separa las dos cosas byte a byte: una linea que empieza por "IMPROV" es un
// paquete; cualquier otra cosa es texto hasta '\n'.

#define IMPROV_VERSION 1
enum : uint8_t {
  IMPROV_T_STATE = 0x01, IMPROV_T_ERROR = 0x02, IMPROV_T_RPC = 0x03, IMPROV_T_RESULT = 0x04
};
enum : uint8_t {
  IMPROV_S_READY = 0x02, IMPROV_S_PROVISIONING = 0x03, IMPROV_S_PROVISIONED = 0x04
};
enum : uint8_t {
  IMPROV_E_NONE = 0x00, IMPROV_E_INVALID = 0x01, IMPROV_E_UNKNOWN = 0x02,
  IMPROV_E_CONNECT = 0x03, IMPROV_E_OTHER = 0xFF
};
enum : uint8_t {
  IMPROV_C_WIFI = 0x01, IMPROV_C_STATE = 0x02, IMPROV_C_INFO = 0x03, IMPROV_C_SCAN = 0x04
};

#define SERIAL_LINE_MAX 200      // una orden de texto mas larga se corta
#define SERIAL_LINE_IDLE_MS 1000 // una linea sin '\n' se acepta tras 1 s quieta (como antes)

enum : uint8_t { SF_NONE = 0, SF_LINE, SF_IMPROV };

struct SerialFramer {
  char line[SERIAL_LINE_MAX + 1];
  uint16_t len = 0;
  uint8_t pkt[9 + 256];  // cabecera(6) + version + tipo + longitud + datos(<=255) + suma
  uint16_t plen = 0;
  bool inPkt = false;
  uint32_t lastByte = 0;

  void reset() { len = 0; plen = 0; inPkt = false; }

  // Un byte. Devuelve SF_LINE (texto en line, sin \r\n) o SF_IMPROV (paquete valido en
  // pkt, plen bytes) cuando hay algo completo. Un paquete con la suma mala se descarta.
  uint8_t feed(uint8_t b, uint32_t now) {
    lastByte = now;
    if (inPkt) {
      pkt[plen++] = b;
      if (plen >= 9 && plen == 9u + pkt[8] + 1u) {
        inPkt = false;
        uint16_t n = plen;
        plen = 0;
        uint8_t sum = 0;
        for (uint16_t i = 0; i + 1 < n; i++) sum += pkt[i];
        if (sum == pkt[n - 1]) {
          plen = n;
          return SF_IMPROV;
        }
      }
      return SF_NONE;
    }
    if (b == '\n') {
      line[len] = 0;
      if (len && line[len - 1] == '\r') line[--len] = 0;
      len = 0;
      return SF_LINE;
    }
    if (len < SERIAL_LINE_MAX) line[len++] = (char)b;
    if (len == 6 && memcmp(line, "IMPROV", 6) == 0) {
      memcpy(pkt, line, 6);
      plen = 6;
      len = 0;
      inPkt = true;
    }
    return SF_NONE;
  }
  // Para el bucle: una linea a medias sin '\n' que lleva 1 s quieta cuenta como entera
  // (el monitor serie de Arduino puede no mandar fin de linea). Un paquete a medias se
  // descarta.
  bool idleLine(uint32_t now) {
    if (inPkt && now - lastByte > SERIAL_LINE_IDLE_MS) { inPkt = false; plen = 0; }
    if (!len || inPkt || now - lastByte <= SERIAL_LINE_IDLE_MS) return false;
    line[len] = 0;
    len = 0;
    return true;
  }
  // Si ahora mismo hay un paquete a medias (para no cortarlo con otra cosa)
  bool busy() const { return inPkt || len; }

  uint8_t type() const { return pkt[7]; }
  const uint8_t *data() const { return pkt + 9; }
  uint8_t dataLen() const { return pkt[8]; }
};

// ---------------------------------------------------------------- RPC
struct ImprovRpc {
  uint8_t cmd = 0;
  char ssid[33] = { 0 };
  char pass[65] = { 0 };
};

// Lee un paquete RPC (tipo 0x03): orden + longitud + datos. Para IMPROV_C_WIFI, los datos
// son longitud+nombre y longitud+contrasena. false si esta mal formado.
inline bool improvParseRpc(const uint8_t *d, uint8_t n, ImprovRpc &r) {
  r = ImprovRpc();
  if (n < 2 || d[1] != n - 2) return false;
  r.cmd = d[0];
  if (r.cmd != IMPROV_C_WIFI) return true;
  const uint8_t *p = d + 2, *end = d + n;
  if (p >= end) return false;
  uint8_t sl = *p++;
  if (sl == 0 || sl > 32 || end - p < sl + 1) return false;
  memcpy(r.ssid, p, sl);
  r.ssid[sl] = 0;
  p += sl;
  uint8_t pl = *p++;
  if (pl > 64 || end - p != pl) return false;
  memcpy(r.pass, p, pl);
  r.pass[pl] = 0;
  return true;
}

// Monta un paquete en out (hasta cap bytes) y devuelve su longitud (0 si no cabe).
inline size_t improvPacket(uint8_t type, const uint8_t *data, uint8_t n, uint8_t *out, size_t cap) {
  if (cap < 10u + n) return 0;
  memcpy(out, "IMPROV", 6);
  out[6] = IMPROV_VERSION;
  out[7] = type;
  out[8] = n;
  if (n) memcpy(out + 9, data, n);
  uint8_t sum = 0;
  for (size_t i = 0; i < 9u + n; i++) sum += out[i];
  out[9 + n] = sum;
  return 10u + n;
}

// Resultado de una orden: orden + longitud total + cadenas con su longitud delante.
inline size_t improvResult(uint8_t cmd, const char *const *strs, uint8_t count, uint8_t *out,
                           size_t cap) {
  uint8_t d[250];
  size_t n = 2;
  for (uint8_t i = 0; i < count; i++) {
    size_t sl = strlen(strs[i]);
    if (sl > 64) sl = 64;
    if (n + 1 + sl > sizeof(d)) return 0;
    d[n++] = (uint8_t)sl;
    memcpy(d + n, strs[i], sl);
    n += sl;
  }
  d[0] = cmd;
  d[1] = (uint8_t)(n - 2);
  return improvPacket(IMPROV_T_RESULT, d, (uint8_t)n, out, cap);
}
