#pragma once
// Host stand-in for ESP-IDF's nvs.h: the NVS command reads the partition's statistics. The
// renderer's NVS is a map in memory (Preferences.h), so there is nothing to count.
#include <cstddef>
#include <cstdint>
#ifndef ESP_OK
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#endif
typedef uint32_t nvs_handle_t;
typedef enum { NVS_READONLY, NVS_READWRITE } nvs_open_mode_t;
typedef struct {
  size_t used_entries, free_entries, available_entries, total_entries, namespace_count;
} nvs_stats_t;
inline esp_err_t nvs_get_stats(const char *, nvs_stats_t *) { return ESP_FAIL; }
inline esp_err_t nvs_open(const char *, nvs_open_mode_t, nvs_handle_t *) { return ESP_FAIL; }
inline esp_err_t nvs_get_used_entry_count(nvs_handle_t, size_t *) { return ESP_FAIL; }
inline void nvs_close(nvs_handle_t) {}
