// Actualizaciones por Wi-Fi (v1.42): Wi-Fi, HTTPS, escritura de la imagen y vuelta atras.
// La logica que se puede probar en el PC (versiones, manifiesto, descarga por pasos) esta
// en ota_policy.h; aqui solo lo que toca el hardware.
#include "ota.h"
#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_heap_caps.h>
#include <mbedtls/x509.h>

// El paquete de certificados raiz de Mozilla que ya trae el core (libmbedtls, ~69 KB). Con
// el se comprueban los de github.com y de su almacen de descargas sin depender de que
// GitHub no cambie de autoridad (decidido 2026-11-07, en vez de un certs.h propio).
extern const uint8_t crtBundleStart[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t crtBundleEnd[] asm("_binary_x509_crt_bundle_end");

#define OTA_DEFAULT_URL "https://github.com/Ale-Cuevas/TamaPoke/releases/latest/download/ota-" OTA_BOARD ".json"
#define OTA_MIN_FREE 90000UL    // RAM interna libre para encender el radio (Wi-Fi + TLS)
#define OTA_MIN_BLOCK 36000UL   // y un bloque contiguo para los buffers de TLS
#define OTA_STEP_BYTES 8192     // por pasada del loop(): ~2 sectores de flash, la UI sigue viva

// El core marca valida la imagen nueva nada mas arrancar salvo que esto devuelva true: la
// marcamos nosotros cuando ha demostrado que funciona (otaMarkValid, FR-OTA.13).
extern "C" bool verifyRollbackLater() { return true; }

static bool gPending = false;
static const char *gVersion = "0.0";
static uint8_t gPhase = OTP_IDLE, gErr = OTAE_NONE, gVerdict = OTAV_UPTODATE;
static bool gForce = false;
static OtaManifest gMan;
static char gUrl[200] = "";
static char gSsid[33] = "";
static bool gSsidLoaded = false;
static uint32_t gT0 = 0;
static uint32_t gMinHeap = 0;
static volatile uint8_t gDiscReason = 0;
static bool gEventHooked = false;

static NetworkClientSecure *gTls = nullptr;
static NetworkClient *gPlain = nullptr;
static HTTPClient *gHttp = nullptr;
static uint8_t *gBuf = nullptr;
static OtaDownload gDl;

// ---------------------------------------------------------------- arranque
void otaBoot(const char *tag) {
  // tag es OTA_TAG FW_VERSION: usarlo aqui hace que el enlazador lo deje en el binario, y
  // OTA STATE lo busca en la otra ranura
  if (tag && !strncmp(tag, OTA_TAG, strlen(OTA_TAG))) gVersion = tag + strlen(OTA_TAG);
  const esp_partition_t *run = esp_ota_get_running_partition();
  esp_ota_img_states_t st;
  gPending = run && esp_ota_get_state_partition(run, &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY;
  if (gPending) {
    enableLoopWDT();  // un cuelgue reinicia y el cargador vuelve a la imagen anterior
    Serial.printf("OTA: imagen nueva a prueba en %s\n", run->label);
  }
}

bool otaPendingVerify() { return gPending; }

void otaMarkValid() {
  if (!gPending) return;
  esp_ota_mark_app_valid_cancel_rollback();
  disableLoopWDT();
  gPending = false;
  Serial.println("OTA: imagen nueva confirmada");
}

// ---------------------------------------------------------------- red guardada
static void loadSsid() {
  if (gSsidLoaded) return;
  Preferences p;
  gSsid[0] = 0;
  if (p.begin("net", true)) {
    if (p.isKey("ssid")) p.getString("ssid", gSsid, sizeof(gSsid));
    p.end();
  }
  gSsidLoaded = true;
}

bool otaHasWifi() {
  loadSsid();
  return gSsid[0] != 0;
}
const char *otaSsid() {
  loadSsid();
  return gSsid;
}

bool otaSaveWifi(const char *ssid, const char *pass) {
  if (!ssid || !*ssid || strlen(ssid) > 32 || strlen(pass) > 64) return false;
  Preferences p;
  if (!p.begin("net", false)) return false;
  bool ok = p.putString("ssid", ssid) > 0;
  if (*pass) ok = ok && p.putString("pass", pass) > 0;
  else p.remove("pass");  // red abierta
  p.end();
  gSsidLoaded = false;
  return ok;
}

void otaForgetWifi() {
  Preferences p;
  if (p.begin("net", false)) {
    p.clear();
    p.end();
  }
  gSsidLoaded = false;
}

// ---------------------------------------------------------------- radio
static void onDisconnect(arduino_event_id_t, arduino_event_info_t info) {
  gDiscReason = info.wifi_sta_disconnected.reason;
}

static bool heapOk() {
  return heap_caps_get_free_size(MALLOC_CAP_INTERNAL) >= OTA_MIN_FREE &&
         heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) >= OTA_MIN_BLOCK;
}

static void noteHeap() {
  uint32_t f = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  if (!gMinHeap || f < gMinHeap) gMinHeap = f;
}

static void radioStart(const char *ssid, const char *pass) {
  if (!gEventHooked) {
    WiFi.onEvent(onDisconnect, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    gEventHooked = true;
  }
  WiFi.persistent(false);  // sin copia propia del driver en los 20 KB de NVS
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  gDiscReason = 0;
  WiFi.begin(ssid, pass && *pass ? pass : nullptr);
  noteHeap();
}

static void radioStop() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

bool otaRadioOn() { return WiFi.getMode() != WIFI_OFF; }

// Motivo de la ultima desconexion -> error para el jugador
static uint8_t wifiErr() {
  switch (gDiscReason) {
    case WIFI_REASON_NO_AP_FOUND:
    case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY:
    case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD:
    case WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD:
    case WIFI_REASON_BEACON_TIMEOUT:
      return OTAE_NOTFOUND;
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_AUTH_EXPIRE:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_MIC_FAILURE:
    case WIFI_REASON_CONNECTION_FAIL:
      return OTAE_PASSWORD;
    default:
      return OTAE_NOTFOUND;
  }
}

uint8_t otaTestWifi(const char *ssid, const char *pass, uint32_t ms) {
  if (!heapOk()) return OTAE_MEMORY;
  bool wasOn = otaRadioOn();
  radioStart(ssid, pass);
  uint32_t t0 = millis();
  uint8_t err = OTAE_NOTFOUND;
  while (millis() - t0 < ms) {
    if (WiFi.status() == WL_CONNECTED) {
      err = OTAE_NONE;
      break;
    }
    if (gDiscReason && WiFi.status() != WL_IDLE_STATUS && millis() - t0 > 1500) {
      // ya dijo por que no: no hace falta esperar al final
      if (wifiErr() == OTAE_PASSWORD) break;
    }
    feedLoopWDT();
    delay(100);
  }
  if (err != OTAE_NONE) err = wifiErr();
  if (!wasOn) radioStop();
  return err;
}

// ---------------------------------------------------------------- HTTP
const char *otaUrl() { return gUrl[0] ? gUrl : OTA_DEFAULT_URL; }

void otaSetUrl(const char *url) {
  strncpy(gUrl, url ? url : "", sizeof(gUrl) - 1);
  gUrl[sizeof(gUrl) - 1] = 0;
}

static void httpEnd() {
  if (gHttp) {
    gHttp->end();
    delete gHttp;
    gHttp = nullptr;
  }
  if (gTls) {
    gTls->stop();
    delete gTls;
    gTls = nullptr;
  }
  if (gPlain) {
    gPlain->stop();
    delete gPlain;
    gPlain = nullptr;
  }
}

// Abre un GET (siguiendo redirecciones: los enlaces de descarga de GitHub mandan a su
// almacen). Devuelve el codigo HTTP (>0) o el error OTAE_* en negativo.
static int httpGet(const char *url, uint32_t timeoutMs) {
  httpEnd();
  gHttp = new HTTPClient();
  bool tls = strncmp(url, "https://", 8) == 0;
  bool ok;
  if (tls) {
    gTls = new NetworkClientSecure();
    gTls->setCACertBundle(crtBundleStart, crtBundleEnd - crtBundleStart);  // nunca setInsecure()
    gTls->setHandshakeTimeout(timeoutMs / 1000 + 1);
    ok = gHttp->begin(*gTls, url);
  } else {  // solo con OTA URL, para un servidor de pruebas local (FR-OTA.19)
    gPlain = new NetworkClient();
    ok = gHttp->begin(*gPlain, url);
  }
  if (!ok) return -OTAE_SERVER;
  gHttp->setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  gHttp->setConnectTimeout((int32_t)timeoutMs);
  gHttp->setTimeout((uint16_t)min<uint32_t>(timeoutMs, 10000));
  gHttp->setUserAgent("TamaPoke-OTA");
  int code = gHttp->GET();
  noteHeap();
  if (code > 0) return code;
  if (tls) {
    char msg[8];
    int e = gTls->lastError(msg, sizeof(msg));
    if (e == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED || e == MBEDTLS_ERR_X509_CERT_UNKNOWN_FORMAT ||
        e == MBEDTLS_ERR_X509_FATAL_ERROR)
      return -OTAE_CERT;
  }
  return code == HTTPC_ERROR_READ_TIMEOUT ? -OTAE_TIMEOUT : -OTAE_NOINTERNET;
}

// ---------------------------------------------------------------- Update como OtaSink
struct UpdateSink : OtaSink {
  bool begin(uint32_t size) override { return Update.begin(size, U_FLASH); }
  size_t write(const uint8_t *p, size_t n) override { return Update.write(const_cast<uint8_t *>(p), n); }
  bool end() override { return Update.end(false); }
  void abort() override {
    if (Update.isRunning()) Update.abort();
  }
};
static UpdateSink gSink;

struct HttpSource : OtaSource {
  int read(uint8_t *buf, size_t cap) override {
    if (!gHttp) return -1;
    NetworkClient *s = gHttp->getStreamPtr();
    if (!s) return -1;
    int av = s->available();
    if (av > 0) return s->read(buf, (size_t)av < cap ? (size_t)av : cap);
    return s->connected() ? 0 : -1;
  }
};
static HttpSource gSrc;

// ---------------------------------------------------------------- fases
static void fail(uint8_t e) {
  gErr = e;
  gPhase = OTP_ERROR;
  httpEnd();
  Serial.printf("OTA: error %u\n", e);
}

void otaCheck(bool force) {
  otaClose();
  gForce = force;
  gErr = OTAE_NONE;
  memset(&gMan, 0, sizeof(gMan));
  if (!otaHasWifi()) return fail(OTAE_NOWIFI);
  if (!heapOk()) return fail(OTAE_MEMORY);
  char pass[65] = "";
  Preferences p;
  if (p.begin("net", true)) {
    if (p.isKey("pass")) p.getString("pass", pass, sizeof(pass));
    p.end();
  }
  radioStart(gSsid, pass);
  memset(pass, 0, sizeof(pass));
  gT0 = millis();
  gPhase = OTP_CONNECT;
}

static void fetchManifest() {
  uint32_t left = OTA_CHECK_MS - min<uint32_t>(millis() - gT0, OTA_CHECK_MS - 2000);
  int code = httpGet(otaUrl(), left);
  if (code < 0) return fail((uint8_t)-code);
  if (code != 200) {
    Serial.printf("OTA: HTTP %d\n", code);
    return fail(OTAE_SERVER);
  }
  int len = gHttp->getSize();
  if (len > OTA_MANIFEST_MAX) return fail(OTAE_SERVER);
  String body = gHttp->getString();
  httpEnd();
  if (body.length() > OTA_MANIFEST_MAX) return fail(OTAE_SERVER);
  uint8_t r = otaParseManifest(body.c_str(), body.length(), gMan);
  if (r != OTAM_OK) {
    Serial.printf("OTA: manifiesto malo (%u)\n", r);
    return fail(r == OTAM_BOARD ? OTAE_BOARD : OTAE_SERVER);
  }
  gVerdict = otaEvaluate(gMan, gVersion, gForce);
  gPhase = gVerdict == OTAV_UPTODATE ? OTP_UPTODATE : OTP_AVAILABLE;
  Serial.printf("OTA: manifiesto v%s %lu B -> %s\n", gMan.version, (unsigned long)gMan.size,
                gPhase == OTP_UPTODATE ? "al dia" : "nueva");
}

void otaInstall() {
  if (gPhase != OTP_AVAILABLE) return;
  if (!heapOk()) return fail(OTAE_MEMORY);
  int code = httpGet(gMan.url, 10000);
  if (code < 0) return fail((uint8_t)-code);
  if (code != 200) {
    Serial.printf("OTA: HTTP %d\n", code);
    return fail(OTAE_SERVER);
  }
  int len = gHttp->getSize();
  if (len > 0 && (uint32_t)len != gMan.size) {
    Serial.printf("OTA: tamano %d, el manifiesto dice %lu\n", len, (unsigned long)gMan.size);
    return fail(OTAE_SERVER);
  }
  if (!gBuf) gBuf = (uint8_t *)heap_caps_malloc(4096, MALLOC_CAP_8BIT);
  if (!gBuf) return fail(OTAE_MEMORY);
  if (!gDl.start(gSink, gMan.size, gMan.sha256, millis())) return fail(gDl.err);
  gPhase = OTP_DOWNLOAD;
}

void otaCancel() {
  if (gPhase != OTP_DOWNLOAD) return;
  gDl.cancel(gSink);
  httpEnd();
  gPhase = OTP_AVAILABLE;
  Serial.println("OTA: cancelada");
}

void otaClose() {
  if (gPhase == OTP_DOWNLOAD) gDl.cancel(gSink);
  httpEnd();
  if (gBuf) {
    free(gBuf);
    gBuf = nullptr;
  }
  if (otaRadioOn()) radioStop();
  gPhase = OTP_IDLE;
}

void otaPoll(uint32_t now) {
  switch (gPhase) {
    case OTP_CONNECT:
      if (WiFi.status() == WL_CONNECTED) {
        gPhase = OTP_FETCH;  // la pantalla dibuja "Comprobando..." antes del bloqueo de TLS
      } else if (now - gT0 > OTA_WIFI_MS ||
                 (gDiscReason && wifiErr() == OTAE_PASSWORD && now - gT0 > 1500)) {
        fail(wifiErr());
      }
      break;
    case OTP_FETCH:
      fetchManifest();
      break;
    case OTP_DOWNLOAD: {
      uint8_t st = gDl.poll(gSrc, gSink, now, gBuf, 4096, OTA_STEP_BYTES);
      noteHeap();
      if (st == OTD_DONE) {
        httpEnd();
        radioStop();
        gPhase = OTP_DONE;
        Serial.println("OTA: imagen comprobada, arranca en la proxima");
      } else if (st == OTD_ERROR) {
        if (gDl.err == OTAE_WRITE) Serial.printf("OTA: Update: %s\n", Update.errorString());
        fail(gDl.err);
      }
      break;
    }
    default:
      break;
  }
}

uint8_t otaPhase() { return gPhase; }
uint8_t otaErr() { return gErr; }
uint8_t otaVerdict() { return gVerdict; }
uint8_t otaPercent() { return gPhase == OTP_DONE ? 100 : gDl.percent(); }
const OtaManifest &otaInfo() { return gMan; }
uint32_t otaMinHeap() { return gMinHeap; }

// ---------------------------------------------------------------- OTA STATE
static const char *stateName(esp_ota_img_states_t s) {
  switch (s) {
    case ESP_OTA_IMG_NEW: return "new";
    case ESP_OTA_IMG_PENDING_VERIFY: return "pending-verify";
    case ESP_OTA_IMG_VALID: return "valid";
    case ESP_OTA_IMG_INVALID: return "invalid";
    case ESP_OTA_IMG_ABORTED: return "aborted";
    default: return "undefined";
  }
}

// Busca OTA_TAG seguido de una version en una ranura (los firmwares desde la v1.42 lo
// llevan). Lee por trozos de 4 KB solapados.
static bool slotVersion(const esp_partition_t *p, char *out, size_t cap) {
  static const size_t CH = 4096, OV = 16;
  uint8_t *buf = (uint8_t *)malloc(CH + OV);
  if (!buf) return false;
  bool found = false;
  size_t tl = strlen(OTA_TAG);
  for (size_t off = 0; off < p->size && !found; off += CH) {
    size_t n = min(CH + OV, (size_t)(p->size - off));
    if (esp_partition_read(p, off, buf, n) != ESP_OK) break;
    for (size_t i = 0; i + tl + 4 <= n; i++) {
      if (memcmp(buf + i, OTA_TAG, tl) != 0 || buf[i + tl] < '0' || buf[i + tl] > '9') continue;
      size_t k = 0;
      while (k + 1 < cap && i + tl + k < n && buf[i + tl + k] && buf[i + tl + k] < 0x7f) {
        out[k] = (char)buf[i + tl + k];
        k++;
      }
      out[k] = 0;
      found = otaVerValid(out);
      if (found) break;
    }
  }
  free(buf);
  return found;
}

void otaPrintState() {
  const esp_partition_t *run = esp_ota_get_running_partition();
  const esp_partition_t *boot = esp_ota_get_boot_partition();
  const esp_partition_t *other = esp_ota_get_next_update_partition(nullptr);
  esp_ota_img_states_t st;
  Serial.printf("running=%s state=%s boot=%s pending=%d\n", run ? run->label : "?",
                run && esp_ota_get_state_partition(run, &st) == ESP_OK ? stateName(st) : "undefined",
                boot ? boot->label : "?", gPending ? 1 : 0);
  if (other) {
    char v[12] = "";
    const char *sname = esp_ota_get_state_partition(other, &st) == ESP_OK ? stateName(st) : "undefined";
    if (slotVersion(other, v, sizeof(v)))
      Serial.printf("other=%s state=%s version=%s\n", other->label, sname, v);
    else
      Serial.printf("other=%s state=%s version=unknown (empty or before v1.42)\n", other->label, sname);
  }
  Serial.printf("phase=%u err=%u url=%s wifi=%s radio=%d minheap=%lu\n", gPhase, gErr, otaUrl(),
                otaHasWifi() ? otaSsid() : "-", otaRadioOn() ? 1 : 0, (unsigned long)gMinHeap);
}
