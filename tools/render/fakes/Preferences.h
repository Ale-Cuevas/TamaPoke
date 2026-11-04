#pragma once
// NVS in memory: every boot of the renderer starts from a blank save.
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

class Preferences {
 public:
  bool begin(const char *ns, bool = false) { ns_ = ns; return true; }
  void end() {}
  bool clear() { store()[ns_].clear(); return true; }
  bool isKey(const char *k) { return store()[ns_].count(k) > 0; }
  bool remove(const char *k) { return store()[ns_].erase(k) > 0; }
  size_t putBool(const char *k, bool v) { return put(k, &v, sizeof v); }
  size_t putUChar(const char *k, uint8_t v) { return put(k, &v, sizeof v); }
  size_t putChar(const char *k, int8_t v) { return put(k, &v, sizeof v); }
  size_t putShort(const char *k, int16_t v) { return put(k, &v, sizeof v); }
  size_t putUShort(const char *k, uint16_t v) { return put(k, &v, sizeof v); }
  size_t putInt(const char *k, int32_t v) { return put(k, &v, sizeof v); }
  size_t putUInt(const char *k, uint32_t v) { return put(k, &v, sizeof v); }
  size_t putBytes(const char *k, const void *v, size_t n) { return put(k, v, n); }
  size_t putString(const char *k, const char *v) { return put(k, v, strlen(v) + 1); }
  bool getBool(const char *k, bool d = false) { return get(k, d); }
  uint8_t getUChar(const char *k, uint8_t d = 0) { return get(k, d); }
  int8_t getChar(const char *k, int8_t d = 0) { return get(k, d); }
  int16_t getShort(const char *k, int16_t d = 0) { return get(k, d); }
  uint16_t getUShort(const char *k, uint16_t d = 0) { return get(k, d); }
  int32_t getInt(const char *k, int32_t d = 0) { return get(k, d); }
  uint32_t getUInt(const char *k, uint32_t d = 0) { return get(k, d); }
  size_t getBytesLength(const char *k) { return isKey(k) ? store()[ns_][k].size() : 0; }
  size_t getBytes(const char *k, void *buf, size_t n) {
    if (!isKey(k)) return 0;
    auto &v = store()[ns_][k];
    size_t c = v.size() < n ? v.size() : n;
    memcpy(buf, v.data(), c);
    return c;
  }
  size_t getString(const char *k, char *buf, size_t n) {
    if (!n) return 0;
    size_t c = getBytes(k, buf, n);
    buf[c ? c - 1 : 0] = 0;
    return c;
  }

 private:
  using Ns = std::map<std::string, std::vector<uint8_t>>;
  static std::map<std::string, Ns> &store() {
    static std::map<std::string, Ns> s;
    return s;
  }
  size_t put(const char *k, const void *v, size_t n) {
    auto &d = store()[ns_][k];
    d.assign((const uint8_t *)v, (const uint8_t *)v + n);
    return n;
  }
  template <class T> T get(const char *k, T d) {
    if (!isKey(k)) return d;
    T v;
    return getBytes(k, &v, sizeof v) == sizeof v ? v : d;
  }
  std::string ns_;
};
