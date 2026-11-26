#pragma once
// Include this INSTEAD of <Arduino_GFX_Library.h>, before anything else pulls
// the library in (TamaPoke.ino does).
//
// Faster panel transfer (docs/SMOOTH_GAMES.md, step 1). The library's
// Arduino_ESP32QSPI::writePixels byte-swaps one chunk into the DMA buffer and
// then waits for it to be sent, so the CPU and the SPI never work at once.
// TpQspi sends the same bytes with the same framing, but swaps chunk N+1 into
// the second DMA buffer while chunk N is on the wire (the
// spi_device_polling_start / polling_end split the library already uses for
// its YCbCr path), and reads the framebuffer two pixels at a time.
//
// The library keeps the handle and its buffers private, and we don't vendor
// it: the header is included with `private` turned into `public` just here.
// The layout doesn't change, only the access check.
//
// Chunks are TP_QSPI_CHUNK_PX pixels (the library's are 1024). Measured on the
// board: 4096 sends the frame in 25.5 ms, 1024 in 27.5, 8192 no better. The two
// DMA buffers are our own, in internal RAM (2 x 8 KB), so no build flag is
// needed and the CI / web-installer build gets the same speed. The bus the
// library creates takes transfers of up to 16 KB, enough for these chunks.
//
//   -D TP_FAST_QSPI=0    the library's own writePixels (to compare)
#include <Arduino.h>
#ifndef TP_FAST_QSPI
#define TP_FAST_QSPI 1
#endif

#if defined(ESP_PLATFORM) && TP_FAST_QSPI
#include <driver/spi_master.h>
#include <esp_heap_caps.h>
#include <Arduino_DataBus.h>
#define private public
#include <databus/Arduino_ESP32QSPI.h>
#undef private
#include <Arduino_GFX_Library.h>

#ifndef TP_QSPI_CHUNK_PX
#define TP_QSPI_CHUNK_PX 4096
#endif
static_assert(TP_QSPI_CHUNK_PX * 2 <= ESP32QSPI_MAX_PIXELS_AT_ONCE * 16 + 8, "chunk bigger than the bus' max_transfer_sz");

class TpQspi : public Arduino_ESP32QSPI {
 public:
  using Arduino_ESP32QSPI::Arduino_ESP32QSPI;

  bool begin(int32_t speed = GFX_NOT_DEFINED, int8_t dataMode = GFX_NOT_DEFINED) override {
    if (!Arduino_ESP32QSPI::begin(speed, dataMode)) return false;
    for (uint8_t i = 0; i < 2; i++)
      pp_[i] = (uint32_t *)heap_caps_aligned_alloc(16, TP_QSPI_CHUNK_PX * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (pp_[0] && pp_[1]) {
      chunk_ = TP_QSPI_CHUNK_PX;
    } else {  // no room: the library's own two (smaller) buffers do the same job
      for (uint8_t i = 0; i < 2; i++) {
        if (pp_[i]) heap_caps_free(pp_[i]);
      }
      pp_[0] = _buffer32;
      pp_[1] = _2nd_buffer32;
      chunk_ = ESP32QSPI_MAX_PIXELS_AT_ONCE;
    }
    return true;
  }

  void writePixels(uint16_t *data, uint32_t len) override {
    uint32_t *buf[2] = {pp_[0], pp_[1]};
    uint8_t cur = 0;
    bool first = true, inflight = false;
    const bool aligned = (((uintptr_t)data) & 3) == 0;
    *_csPortClr = _csPinMask;
    while (len) {
      const uint32_t l = len > chunk_ ? chunk_ : len;
      uint32_t *out = buf[cur];
      const uint32_t pairs = l >> 1;
      uint32_t i = 0;
      if (aligned) {
        const uint32_t *src = (const uint32_t *)data;
        for (; i + 2 <= pairs; i += 2) {  // two words (four pixels) per pass
          uint32_t a = src[i], b = src[i + 1];
          out[i] = ((a & 0x00FF00FFu) << 8) | ((a >> 8) & 0x00FF00FFu);
          out[i + 1] = ((b & 0x00FF00FFu) << 8) | ((b >> 8) & 0x00FF00FFu);
        }
        for (; i < pairs; i++) {
          uint32_t a = src[i];
          out[i] = ((a & 0x00FF00FFu) << 8) | ((a >> 8) & 0x00FF00FFu);
        }
      } else {
        const uint16_t *s = data;
        for (; i < pairs; i++) {
          uint16_t p1 = *s++, p2 = *s++;
          MSB_32_16_16_SET(out[i], p1, p2);
        }
      }
      if (l & 1) {
        uint16_t p = data[l - 1];
        ((uint16_t *)out)[l - 1] = (uint16_t)((p >> 8) | (p << 8));
      }
      data += l;

      // the previous chunk is out: its buffer is free again, and the bus too
      if (inflight) spi_device_polling_end(_handle, portMAX_DELAY);
      if (first) {
        _spi_tran_ext.base.flags = SPI_TRANS_MODE_QIO;
        _spi_tran_ext.base.cmd = 0x32;
        _spi_tran_ext.base.addr = 0x003C00;
        first = false;
      } else {
        _spi_tran_ext.base.flags = SPI_TRANS_MODE_QIO | SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR |
                                   SPI_TRANS_VARIABLE_DUMMY;
      }
      _spi_tran_ext.base.tx_buffer = out;
      _spi_tran_ext.base.length = l << 4;
      spi_device_polling_start(_handle, _spi_tran, portMAX_DELAY);
      inflight = true;
      cur ^= 1;
      len -= l;
    }
    if (inflight) spi_device_polling_end(_handle, portMAX_DELAY);
    *_csPortSet = _csPinMask;
  }

 private:
  uint32_t *pp_[2] = {nullptr, nullptr};
  uint32_t chunk_ = ESP32QSPI_MAX_PIXELS_AT_ONCE;
};
#else
#include <Arduino_GFX_Library.h>
typedef Arduino_ESP32QSPI TpQspi;  // the library's own bus (TP_FAST_QSPI=0) or the PC fakes
#endif
