#pragma once
#include <stdint.h>
#include "ota_policy.h"

// Actualizaciones por Wi-Fi (docs/NEXT_FEATURES.md, Tarea OTA, v1.42): el pegamento con el
// hardware. El sketch solo incluye este .h, asi WiFi.h y HTTPS se quedan en ota.cpp (regla
// de FEATURES.md) y el renderizador del PC no los necesita (tools/render/stubs.cpp).
//
// El radio solo esta encendido mientras la pantalla de actualizar esta abierta, o un
// momento al probar una red (Improv o WIFI). Mientras esta encendido el sketch no escribe
// el guardado en flash (FR-OTA.7).

// Fases de la pantalla de actualizar
enum : uint8_t {
  OTP_IDLE = 0,   // radio apagado
  OTP_CONNECT,    // uniendose a la red
  OTP_FETCH,      // leyendo el manifiesto (la proxima pasada bloquea unos segundos)
  OTP_UPTODATE,   // ya tiene la ultima
  OTP_AVAILABLE,  // hay version nueva (otaInfo())
  OTP_DOWNLOAD,   // descargando y escribiendo (otaPercent())
  OTP_DONE,       // imagen nueva comprobada y elegida para el proximo arranque
  OTP_ERROR,      // otaErr()
};

// Marca delante de la version en el binario: el sketch define OTA_TAG FW_VERSION y se lo
// pasa a otaBoot(), y OTA STATE lo busca en la otra ranura para decir que version tiene.
#define OTA_TAG "@TPV@"

// ---- arranque y vuelta atras (FR-OTA.13)
// En setup(), pronto. tag es OTA_TAG FW_VERSION. Si esta imagen acaba de llegar por OTA
// (pendiente de verificar), activa el perro guardian del loop(): un cuelgue reinicia y
// el cargador vuelve a la anterior.
void otaBoot(const char *tag);
bool otaPendingVerify();
// La imagen nueva funciona: se queda (y se apaga el perro guardian).
void otaMarkValid();

// ---- red guardada (espacio NVS "net", FR-OTA.3)
bool otaHasWifi();
const char *otaSsid();  // "" si no hay
bool otaSaveWifi(const char *ssid, const char *pass);
void otaForgetWifi();
// Prueba una red sin guardarla, bloqueando hasta ms (Improv, orden WIFI). Enciende y
// apaga el radio. Devuelve OTAE_NONE si conecta, o el error (red no encontrada, clave).
uint8_t otaTestWifi(const char *ssid, const char *pass, uint32_t ms);

// ---- comprobar e instalar (FR-OTA.4-12)
void otaCheck(bool force);  // radio + red + manifiesto; force: acepta igual o anterior
void otaInstall();          // desde OTP_AVAILABLE
void otaCancel();           // tecla USER: corta la descarga (vuelve a OTP_AVAILABLE)
void otaClose();            // al cerrar la pantalla: radio apagado, todo a cero
void otaPoll(uint32_t now); // cada pasada del loop()
bool otaRadioOn();
uint8_t otaPhase();
uint8_t otaErr();
uint8_t otaVerdict();       // OTAV_* del manifiesto leido
uint8_t otaPercent();
const OtaManifest &otaInfo();

// ---- para probar (FR-OTA.19)
void otaSetUrl(const char *url);  // otro manifiesto hasta reiniciar ("" vuelve al de siempre)
const char *otaUrl();
uint32_t otaMinHeap();            // RAM interna minima con el radio encendido (0 si no se uso)
void otaPrintState();             // OTA STATE: ranura, estado, version de la otra
