// Actualizaciones por Wi-Fi (ota_policy.h, improv.h, v1.42): versiones, manifiesto,
// condiciones para instalar, SHA-256, la descarga por pasos y el Improv por serie.
#include "framework.h"
#include "../ota_policy.h"
#include "../improv.h"
#include <string>
#include <vector>

// ---------------------------------------------------------------- versiones
TEST(ota, version_1_9_es_anterior_a_1_10) {
  CHECK(otaVerCmp("1.9", "1.10") < 0);
  CHECK(otaVerCmp("1.10", "1.9") > 0);
  CHECK(otaVerCmp("1.42", "1.42") == 0);
  CHECK(otaVerCmp("2.0", "1.99") > 0);
  CHECK(otaVerCmp("1.41", "1.42") < 0);
}

TEST(ota, versiones_mal_formadas_nunca_parecen_nuevas) {
  const char *bad[] = { "", "1", "1.", ".1", "v1.42", "1.42.1", "1.42 ", " 1.42", "1,42", "a.b",
                        "12345.1", "1.-2" };
  for (const char *b : bad) {
    CHECK_MSG(!otaVerValid(b), b);
    CHECK_MSG(otaVerCmp(b, "0.0") < 0, b);
  }
  CHECK(!otaVerValid(nullptr));
  CHECK(otaVerValid("0.0"));
  CHECK(otaVerValid("1.42"));
}

// ---------------------------------------------------------------- manifiesto
static const char *SHA_A = "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08";

static std::string manifest(const std::string &extra = "", const std::string &board = "amoled216",
                            const std::string &size = "1468000") {
  return std::string("{\"board\":\"") + board + "\",\"version\":\"1.43\",\"size\":" + size +
         ",\"sha256\":\"" + SHA_A +
         "\",\"url\":\"https://github.com/Ale-Cuevas/TamaPoke/releases/download/v1.43/"
         "tamapoke-amoled216.bin\",\"min_sprites\":1,\"notes\":\"Box fixes\"" + extra + "}";
}

static uint8_t parse(const std::string &s, OtaManifest &m) { return otaParseManifest(s.c_str(), s.size(), m); }

TEST(ota, manifiesto_bueno) {
  OtaManifest m;
  CHECK_EQ((int)parse(manifest(), m), (int)OTAM_OK);
  CHECK_STREQ(m.board, "amoled216");
  CHECK_STREQ(m.version, "1.43");
  CHECK_EQ(m.size, (uint32_t)1468000);
  CHECK_STREQ(m.sha256, SHA_A);
  CHECK_EQ((int)m.minSprites, 1);
  CHECK_STREQ(m.notes, "Box fixes");
  CHECK(strstr(m.url, "/v1.43/tamapoke-amoled216.bin") != nullptr);
}

TEST(ota, manifiesto_con_espacios_y_claves_nuevas) {
  std::string s = "{\n  \"future\": {\"a\":[1,2,{\"b\":null}], \"c\":\"}\"},\n" + manifest().substr(1);
  s.insert(s.size() - 1, ", \"x\": -1.5e3, \"y\": true ");
  OtaManifest m;
  CHECK_EQ((int)parse(s, m), (int)OTAM_OK);
  CHECK_STREQ(m.version, "1.43");
}

TEST(ota, manifiesto_falta_un_campo) {
  const char *keys[] = { "board", "version", "size", "sha256", "url" };
  for (const char *k : keys) {
    std::string s = manifest();
    std::string key = std::string("\"") + k + "\":";
    size_t a = s.find(key);
    size_t b = s.find(",\"", a + 1);
    s.erase(a, b - a + 1);
    OtaManifest m;
    CHECK_MSG(parse(s, m) == OTAM_FIELD, k);
  }
}

TEST(ota, manifiesto_de_otra_placa) {
  OtaManifest m;
  CHECK_EQ((int)parse(manifest("", "amoled175"), m), (int)OTAM_BOARD);
}

TEST(ota, imagen_mas_grande_que_la_ranura) {
  OtaManifest m;
  CHECK_EQ((int)parse(manifest("", "amoled216", "3145729"), m), (int)OTAM_SIZE);
  CHECK_EQ((int)parse(manifest("", "amoled216", "3145728"), m), (int)OTAM_OK);
  CHECK_EQ((int)parse(manifest("", "amoled216", "0"), m), (int)OTAM_SIZE);
  CHECK_EQ((int)parse(manifest("", "amoled216", "99999999999"), m), (int)OTAM_JSON);
  CHECK_EQ((int)parse(manifest("", "amoled216", "1.5"), m), (int)OTAM_JSON);
}

TEST(ota, manifiestos_rotos) {
  OtaManifest m;
  std::string good = manifest();
  CHECK_EQ((int)parse("", m), (int)OTAM_JSON);
  CHECK_EQ((int)parse("[]", m), (int)OTAM_JSON);
  CHECK_EQ((int)parse("{}", m), (int)OTAM_FIELD);
  CHECK_EQ((int)parse(good.substr(0, good.size() - 1), m), (int)OTAM_JSON);  // sin cerrar
  CHECK_EQ((int)parse(good.substr(0, 40), m), (int)OTAM_JSON);               // cortado
  CHECK_EQ((int)parse(good + "x", m), (int)OTAM_JSON);                       // basura detras
  CHECK_EQ((int)parse("<html>404</html>", m), (int)OTAM_JSON);               // pagina de error
  CHECK_EQ((int)otaParseManifest(nullptr, 0, m), (int)OTAM_JSON);
}

TEST(ota, sha_y_url_validados) {
  OtaManifest m;
  std::string s = manifest();
  std::string bad = s;
  bad.replace(bad.find(SHA_A), 64, std::string(63, 'a') + "g");
  CHECK_EQ((int)parse(bad, m), (int)OTAM_SHA);
  bad = s;
  bad.replace(bad.find(SHA_A), 64, std::string(62, 'a'));
  CHECK_EQ((int)parse(bad, m), (int)OTAM_SHA);
  bad = s;  // en mayusculas vale, y se guarda en minusculas
  std::string up = SHA_A;
  for (char &c : up) c = (char)toupper(c);
  bad.replace(bad.find(SHA_A), 64, up);
  CHECK_EQ((int)parse(bad, m), (int)OTAM_OK);
  CHECK_STREQ(m.sha256, SHA_A);
  bad = s;
  bad.replace(bad.find("https://"), 8, "ftp://");
  CHECK_EQ((int)parse(bad, m), (int)OTAM_URL);
  bad = s;  // una url tan larga que no cabe: no se usa cortada
  bad.insert(bad.find("tamapoke-amoled216.bin"), std::string(250, 'x'));
  CHECK_EQ((int)parse(bad, m), (int)OTAM_URL);
}

TEST(ota, notas_fuera_del_ascii_salen_como_interrogacion) {
  OtaManifest m;
  std::string s = manifest();
  s.replace(s.find("Box fixes"), 9, "Caf\\u00e9 y \xc3\xb1 \\\"ok\\\"");
  CHECK_EQ((int)parse(s, m), (int)OTAM_OK);
  CHECK_STREQ(m.notes, "Caf? y ? \"ok\"");
  s = manifest();  // notas larguisimas: se cortan, el resto del manifiesto vale
  s.replace(s.find("Box fixes"), 9, std::string(500, 'n'));
  CHECK_EQ((int)parse(s, m), (int)OTAM_OK);
  CHECK_EQ(strlen(m.notes), sizeof(m.notes) - 1);
}

TEST(ota, evaluar_version_y_sprites) {
  OtaManifest m;
  parse(manifest(), m);  // 1.43, sprites 1
  CHECK_EQ((int)otaEvaluate(m, "1.42", false), (int)OTAV_NEWER);
  CHECK_EQ((int)otaEvaluate(m, "1.43", false), (int)OTAV_UPTODATE);
  CHECK_EQ((int)otaEvaluate(m, "1.50", false), (int)OTAV_UPTODATE);  // sin bajar de version...
  CHECK_EQ((int)otaEvaluate(m, "1.50", true), (int)OTAV_NEWER);      // ...salvo OTA FORCE
  m.minSprites = 2;
  CHECK_EQ((int)otaEvaluate(m, "1.42", false, 1), (int)OTAV_SPRITES);
  CHECK_EQ((int)otaEvaluate(m, "1.42", false, 2), (int)OTAV_NEWER);
}

// ---------------------------------------------------------------- condiciones
TEST(ota, condiciones_para_instalar) {
  CHECK_EQ((int)otaCanInstall(false, 49, false, false), (int)OTAB_BATTERY);
  CHECK_EQ((int)otaCanInstall(false, 50, false, false), (int)OTAB_OK);
  CHECK_EQ((int)otaCanInstall(false, -1, false, false), (int)OTAB_BATTERY);
  CHECK_EQ((int)otaCanInstall(true, 5, false, false), (int)OTAB_OK);   // con USB da igual
  CHECK_EQ((int)otaCanInstall(true, -1, false, false), (int)OTAB_OK);
  CHECK_EQ((int)otaCanInstall(true, 100, true, false), (int)OTAB_BUSY);   // evolucion, ceremonia
  CHECK_EQ((int)otaCanInstall(true, 100, false, true), (int)OTAB_GAME);   // minijuego abierto
  CHECK_EQ((int)otaCanInstall(false, 10, true, true), (int)OTAB_BATTERY); // la bateria primero
}

// ---------------------------------------------------------------- SHA-256
static std::string shaHex(const std::string &s) {
  OtaSha256 h;
  h.update((const uint8_t *)s.data(), s.size());
  char out[65];
  h.finishHex(out);
  return out;
}

TEST(ota, sha256_vectores_conocidos) {
  CHECK_EQ(shaHex(""), std::string("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
  CHECK_EQ(shaHex("abc"), std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
  CHECK_EQ(shaHex("test"), std::string(SHA_A));
  CHECK_EQ(shaHex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
           std::string("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
  CHECK_EQ(shaHex(std::string(1000000, 'a')),
           std::string("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
}

TEST(ota, sha256_igual_en_trozos_que_de_una_vez) {
  std::string data;
  for (int i = 0; i < 5000; i++) data += (char)(i * 31 + 7);
  OtaSha256 h;
  size_t pos = 0, step = 1;
  while (pos < data.size()) {
    size_t n = std::min(step, data.size() - pos);
    h.update((const uint8_t *)data.data() + pos, n);
    pos += n;
    step = step * 3 % 97 + 1;
  }
  char out[65];
  h.finishHex(out);
  CHECK_EQ(std::string(out), shaHex(data));
}

// ---------------------------------------------------------------- descarga por pasos
struct FakeSink : OtaSink {
  std::vector<uint8_t> data;
  bool began = false, ended = false, aborted = false;
  bool failBegin = false, failEnd = false;
  size_t failWriteAt = SIZE_MAX;
  bool begin(uint32_t) override { began = true; return !failBegin; }
  size_t write(const uint8_t *p, size_t n) override {
    if (ended) testFail(__FILE__, __LINE__, "write despues de end");
    if (data.size() + n > failWriteAt) return 0;
    data.insert(data.end(), p, p + n);
    return n;
  }
  bool end() override { ended = true; return !failEnd; }
  void abort() override { aborted = true; }
};

struct FakeSource : OtaSource {
  std::string data;
  size_t pos = 0;
  size_t chunk = 1000;       // bytes por read como mucho
  size_t stallAt = SIZE_MAX; // a partir de aqui, read devuelve 0 (sin datos)
  size_t closeAt = SIZE_MAX; // a partir de aqui, la conexion se cierra
  int read(uint8_t *buf, size_t cap) override {
    if (pos >= closeAt) return -1;
    if (pos >= stallAt || pos >= data.size()) return 0;
    size_t n = cap < chunk ? cap : chunk;
    if (n > data.size() - pos) n = data.size() - pos;
    if (n > stallAt - pos) n = stallAt - pos;
    memcpy(buf, data.data() + pos, n);
    pos += n;
    return (int)n;
  }
};

static std::string image(size_t n) {
  std::string s;
  for (size_t i = 0; i < n; i++) s += (char)(i * 131 + (i >> 8));
  return s;
}

static uint8_t runAll(OtaDownload &d, FakeSource &src, FakeSink &sink, uint32_t &now, int maxSteps = 10000) {
  uint8_t buf[512];
  while (maxSteps-- && d.poll(src, sink, now, buf, sizeof(buf), 4096) == OTD_RUN) now += 25;
  return d.state;
}

TEST(ota, descarga_buena_cierra_la_imagen) {
  FakeSource src;
  src.data = image(50000);
  FakeSink sink;
  OtaDownload d;
  uint32_t now = 1000;
  CHECK(d.start(sink, (uint32_t)src.data.size(), shaHex(src.data).c_str(), now));
  CHECK(sink.began);
  CHECK_EQ((int)runAll(d, src, sink, now), (int)OTD_DONE);
  CHECK(sink.ended);
  CHECK(!sink.aborted);
  CHECK_EQ((int)d.percent(), 100);
  CHECK(std::string(sink.data.begin(), sink.data.end()) == src.data);
}

TEST(ota, un_paso_lee_como_mucho_lo_pedido) {
  FakeSource src;
  src.data = image(50000);
  FakeSink sink;
  OtaDownload d;
  d.start(sink, (uint32_t)src.data.size(), shaHex(src.data).c_str(), 0);
  uint8_t buf[512];
  d.poll(src, sink, 0, buf, sizeof(buf), 4096);
  CHECK_RANGE(d.got, (uint32_t)4096, (uint32_t)4096 + 511);
  CHECK_RANGE((int)d.percent(), 8, 9);
}

TEST(ota, suma_que_no_coincide_aborta_sin_end) {
  FakeSource src;
  src.data = image(20000);
  FakeSink sink;
  OtaDownload d;
  uint32_t now = 0;
  d.start(sink, (uint32_t)src.data.size(), SHA_A, now);
  CHECK_EQ((int)runAll(d, src, sink, now), (int)OTD_ERROR);
  CHECK_EQ((int)d.err, (int)OTAE_CHECKSUM);
  CHECK(sink.aborted);
  CHECK(!sink.ended);
}

TEST(ota, end_nunca_antes_de_comprobar) {
  // Un byte cambiado en medio: el SHA lo caza al final y end() no llega a llamarse.
  FakeSource src;
  src.data = image(30000);
  std::string sum = shaHex(src.data);
  src.data[12345] ^= 1;
  FakeSink sink;
  OtaDownload d;
  uint32_t now = 0;
  d.start(sink, (uint32_t)src.data.size(), sum.c_str(), now);
  runAll(d, src, sink, now);
  CHECK_EQ((int)d.err, (int)OTAE_CHECKSUM);
  CHECK(!sink.ended);
}

TEST(ota, veinte_segundos_sin_datos) {
  FakeSource src;
  src.data = image(30000);
  src.stallAt = 10000;
  FakeSink sink;
  OtaDownload d;
  uint32_t now = 0;
  d.start(sink, (uint32_t)src.data.size(), shaHex(src.data).c_str(), now);
  uint8_t buf[512];
  while (d.got < 10000) d.poll(src, sink, now += 10, buf, sizeof(buf), 4096);
  uint32_t t0 = now;
  d.poll(src, sink, t0 + OTA_NODATA_MS, buf, sizeof(buf), 4096);
  CHECK_EQ((int)d.state, (int)OTD_RUN);  // justo 20 s: aun espera
  d.poll(src, sink, t0 + OTA_NODATA_MS + 1, buf, sizeof(buf), 4096);
  CHECK_EQ((int)d.state, (int)OTD_ERROR);
  CHECK_EQ((int)d.err, (int)OTAE_TIMEOUT);
  CHECK(sink.aborted);
  CHECK(!sink.ended);
}

TEST(ota, el_reloj_que_da_la_vuelta_no_corta_la_descarga) {
  FakeSource src;
  src.data = image(8000);
  FakeSink sink;
  OtaDownload d;
  uint32_t now = 0xFFFFFF00u;
  d.start(sink, (uint32_t)src.data.size(), shaHex(src.data).c_str(), now);
  CHECK_EQ((int)runAll(d, src, sink, now), (int)OTD_DONE);
}

TEST(ota, conexion_cortada_a_media_descarga) {
  FakeSource src;
  src.data = image(30000);
  src.closeAt = 15000;
  FakeSink sink;
  OtaDownload d;
  uint32_t now = 0;
  d.start(sink, (uint32_t)src.data.size(), shaHex(src.data).c_str(), now);
  CHECK_EQ((int)runAll(d, src, sink, now), (int)OTD_ERROR);
  CHECK_EQ((int)d.err, (int)OTAE_LOST);
  CHECK(sink.aborted && !sink.ended);
}

TEST(ota, fallo_al_escribir) {
  FakeSource src;
  src.data = image(30000);
  FakeSink sink;
  sink.failWriteAt = 9000;
  OtaDownload d;
  uint32_t now = 0;
  d.start(sink, (uint32_t)src.data.size(), shaHex(src.data).c_str(), now);
  CHECK_EQ((int)runAll(d, src, sink, now), (int)OTD_ERROR);
  CHECK_EQ((int)d.err, (int)OTAE_WRITE);
  CHECK(sink.aborted && !sink.ended);

  FakeSink s2;  // end() que falla (p. ej. la imagen no es valida para el arranque)
  s2.failEnd = true;
  FakeSource src2;
  src2.data = image(3000);
  OtaDownload d2;
  d2.start(s2, (uint32_t)src2.data.size(), shaHex(src2.data).c_str(), now);
  CHECK_EQ((int)runAll(d2, src2, s2, now), (int)OTD_ERROR);
  CHECK_EQ((int)d2.err, (int)OTAE_WRITE);
  CHECK(s2.aborted);

  FakeSink s3;  // begin() que falla: ni se empieza
  s3.failBegin = true;
  OtaDownload d3;
  CHECK(!d3.start(s3, 3000, SHA_A, now));
  CHECK_EQ((int)d3.err, (int)OTAE_WRITE);
}

TEST(ota, mas_datos_de_los_anunciados) {
  // El servidor manda mas que size: lo que sobra no se escribe y no hay end().
  FakeSource src;
  src.data = image(30000);
  FakeSink sink;
  OtaDownload d;
  uint32_t now = 0;
  // size anunciado menor: llega a total, el SHA (de 30000) no coincide con lo leido
  d.start(sink, 20000, shaHex(src.data).c_str(), now);
  runAll(d, src, sink, now);
  CHECK_EQ((int)d.err, (int)OTAE_CHECKSUM);
  CHECK_EQ(sink.data.size(), (size_t)20000);
  CHECK(!sink.ended);
}

TEST(ota, cancelar_en_cada_momento) {
  for (size_t at : { (size_t)0, (size_t)1, (size_t)4096, (size_t)15000, (size_t)29999 }) {
    FakeSource src;
    src.data = image(30000);
    FakeSink sink;
    OtaDownload d;
    uint32_t now = 0;
    d.start(sink, (uint32_t)src.data.size(), shaHex(src.data).c_str(), now);
    uint8_t buf[1];
    while (d.got < at) d.poll(src, sink, now += 1, buf, sizeof(buf), 1);
    d.cancel(sink);
    CHECK_EQ((int)d.state, (int)OTD_CANCEL);
    CHECK(sink.aborted);
    CHECK(!sink.ended);
    // despues de cancelar no se lee ni se escribe nada mas
    size_t before = sink.data.size();
    d.poll(src, sink, now + 1, buf, sizeof(buf), 4096);
    CHECK_EQ(sink.data.size(), before);
  }
  // cancelar despues del cambio final no hace nada
  FakeSource src;
  src.data = image(1000);
  FakeSink sink;
  OtaDownload d;
  uint32_t now = 0;
  d.start(sink, 1000, shaHex(src.data).c_str(), now);
  runAll(d, src, sink, now);
  d.cancel(sink);
  CHECK_EQ((int)d.state, (int)OTD_DONE);
  CHECK(!sink.aborted);
}

TEST(ota, empezar_con_datos_malos) {
  FakeSink sink;
  OtaDownload d;
  CHECK(!d.start(sink, 0, SHA_A, 0));
  CHECK(!d.start(sink, OTA_SLOT_BYTES + 1, SHA_A, 0));
  CHECK(!d.start(sink, 1000, "abc", 0));
  CHECK(!sink.began);
  CHECK_EQ((int)d.state, (int)OTD_ERROR);
}

// ---------------------------------------------------------------- Improv por serie
static uint8_t feedAll(SerialFramer &f, const std::string &s, uint32_t now = 0) {
  uint8_t last = SF_NONE;
  for (char c : s) {
    uint8_t r = f.feed((uint8_t)c, now);
    if (r != SF_NONE) last = r;
  }
  return last;
}

static std::string rpcWifi(const std::string &ssid, const std::string &pass) {
  std::string d;
  d += (char)IMPROV_C_WIFI;
  d += (char)(2 + ssid.size() + pass.size());
  d += (char)ssid.size();
  d += ssid;
  d += (char)pass.size();
  d += pass;
  uint8_t out[300];
  size_t n = improvPacket(IMPROV_T_RPC, (const uint8_t *)d.data(), (uint8_t)d.size(), out, sizeof(out));
  return std::string((char *)out, n);
}

TEST(ota, improv_las_lineas_de_texto_siguen_igual) {
  SerialFramer f;
  CHECK_EQ((int)feedAll(f, "TAP 10 20\r\n"), (int)SF_LINE);
  CHECK_STREQ(f.line, "TAP 10 20");
  CHECK_EQ((int)feedAll(f, "IMU\n"), (int)SF_LINE);  // empieza por I, pero no es Improv
  CHECK_STREQ(f.line, "IMU");
  CHECK_EQ((int)feedAll(f, "IMPRO\n"), (int)SF_LINE);
  CHECK_STREQ(f.line, "IMPRO");
}

TEST(ota, improv_paquete_wifi) {
  SerialFramer f;
  CHECK_EQ((int)feedAll(f, rpcWifi("MiCasa", "s3cr3t pass")), (int)SF_IMPROV);
  CHECK_EQ((int)f.type(), (int)IMPROV_T_RPC);
  ImprovRpc r;
  CHECK(improvParseRpc(f.data(), f.dataLen(), r));
  CHECK_EQ((int)r.cmd, (int)IMPROV_C_WIFI);
  CHECK_STREQ(r.ssid, "MiCasa");
  CHECK_STREQ(r.pass, "s3cr3t pass");
  // y la siguiente linea de texto sale bien
  CHECK_EQ((int)feedAll(f, "STATS\n"), (int)SF_LINE);
  CHECK_STREQ(f.line, "STATS");
}

TEST(ota, improv_red_abierta_y_datos_con_saltos_de_linea) {
  SerialFramer f;
  CHECK_EQ((int)feedAll(f, rpcWifi("Red\nAbierta", "")), (int)SF_IMPROV);  // '\n' dentro: es binario
  ImprovRpc r;
  CHECK(improvParseRpc(f.data(), f.dataLen(), r));
  CHECK_STREQ(r.ssid, "Red\nAbierta");
  CHECK_STREQ(r.pass, "");
}

TEST(ota, improv_suma_mala_se_descarta) {
  SerialFramer f;
  std::string p = rpcWifi("MiCasa", "x");
  p.back() ^= 0x55;
  CHECK_EQ((int)feedAll(f, p), (int)SF_NONE);
  CHECK(!f.busy());
  CHECK_EQ((int)feedAll(f, "BACK\n"), (int)SF_LINE);
  CHECK_STREQ(f.line, "BACK");
}

TEST(ota, improv_rpc_mal_formados) {
  ImprovRpc r;
  const uint8_t shortPkt[] = { IMPROV_C_WIFI };
  CHECK(!improvParseRpc(shortPkt, 1, r));
  const uint8_t badLen[] = { IMPROV_C_WIFI, 9, 1, 'a' };
  CHECK(!improvParseRpc(badLen, sizeof(badLen), r));
  const uint8_t noSsid[] = { IMPROV_C_WIFI, 2, 0, 0 };
  CHECK(!improvParseRpc(noSsid, sizeof(noSsid), r));
  const uint8_t passOver[] = { IMPROV_C_WIFI, 4, 1, 'a', 5, 'b' };
  CHECK(!improvParseRpc(passOver, sizeof(passOver), r));
  const uint8_t state[] = { IMPROV_C_STATE, 0 };
  CHECK(improvParseRpc(state, sizeof(state), r));
  CHECK_EQ((int)r.cmd, (int)IMPROV_C_STATE);
}

TEST(ota, improv_respuesta_con_cadenas) {
  const char *info[] = { "TamaPoke", "1.42", "ESP32-S3", "Pika" };
  uint8_t out[300];
  size_t n = improvResult(IMPROV_C_INFO, info, 4, out, sizeof(out));
  CHECK(n > 0);
  // la misma maquina la reconoce como paquete valido
  SerialFramer f;
  uint8_t r = SF_NONE;
  for (size_t i = 0; i < n; i++) r = f.feed(out[i], 0);
  CHECK_EQ((int)r, (int)SF_IMPROV);
  CHECK_EQ((int)f.type(), (int)IMPROV_T_RESULT);
  const uint8_t *d = f.data();
  CHECK_EQ((int)d[0], (int)IMPROV_C_INFO);
  CHECK_EQ((int)d[1], (int)f.dataLen() - 2);
  CHECK_EQ((int)d[2], 8);
  CHECK(memcmp(d + 3, "TamaPoke", 8) == 0);
  // sin cadenas (la respuesta a "red configurada")
  n = improvResult(IMPROV_C_WIFI, nullptr, 0, out, sizeof(out));
  CHECK_EQ(n, (size_t)12);
  CHECK_EQ((int)out[9], (int)IMPROV_C_WIFI);
  CHECK_EQ((int)out[10], 0);
}

TEST(ota, improv_linea_sin_fin_tras_un_segundo) {
  SerialFramer f;
  feedAll(f, "STATS", 100);
  CHECK(!f.idleLine(100 + SERIAL_LINE_IDLE_MS));
  CHECK(f.idleLine(101 + SERIAL_LINE_IDLE_MS));
  CHECK_STREQ(f.line, "STATS");
  // un paquete a medias que se queda quieto se descarta, no se toma por texto
  std::string p = rpcWifi("MiCasa", "x");
  feedAll(f, p.substr(0, 12), 5000);
  CHECK(f.busy());
  CHECK(!f.idleLine(5000 + SERIAL_LINE_IDLE_MS + 1));
  CHECK(!f.busy());
}
