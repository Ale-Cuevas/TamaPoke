#pragma once
// Arduino String over std::string: what the sketch and Arduino_GFX call.
#include <cstdlib>
#include <cstring>
#include <string>

class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper *>(s))

class String {
 public:
  String(const char *s = "") : v_(s ? s : "") {}
  String(const std::string &s) : v_(s) {}
  String(char c) : v_(1, c) {}
  String(int n) : v_(std::to_string(n)) {}
  String(unsigned n) : v_(std::to_string(n)) {}
  String(long n) : v_(std::to_string(n)) {}
  String(unsigned long n) : v_(std::to_string(n)) {}
  const char *c_str() const { return v_.c_str(); }
  unsigned length() const { return (unsigned)v_.size(); }
  void trim() {
    size_t a = v_.find_first_not_of(" \t\r\n"), b = v_.find_last_not_of(" \t\r\n");
    v_ = a == std::string::npos ? "" : v_.substr(a, b - a + 1);
  }
  bool startsWith(const String &p) const { return v_.compare(0, p.v_.size(), p.v_) == 0; }
  bool endsWith(const String &s) const {
    return s.v_.size() <= v_.size() && v_.compare(v_.size() - s.v_.size(), s.v_.size(), s.v_) == 0;
  }
  int indexOf(char c, unsigned from = 0) const {
    size_t p = v_.find(c, from);
    return p == std::string::npos ? -1 : (int)p;
  }
  int indexOf(const char *s, unsigned from = 0) const {
    size_t p = v_.find(s, from);
    return p == std::string::npos ? -1 : (int)p;
  }
  int lastIndexOf(char c) const {
    size_t p = v_.rfind(c);
    return p == std::string::npos ? -1 : (int)p;
  }
  String substring(unsigned from) const { return from >= v_.size() ? String("") : String(v_.substr(from)); }
  String substring(unsigned from, unsigned to) const {
    if (to > v_.size()) to = (unsigned)v_.size();
    return from >= to ? String("") : String(v_.substr(from, to - from));
  }
  long toInt() const { return strtol(v_.c_str(), nullptr, 10); }
  char charAt(unsigned i) const { return i < v_.size() ? v_[i] : 0; }
  char operator[](unsigned i) const { return charAt(i); }
  String &operator+=(const String &o) { v_ += o.v_; return *this; }
  String &operator+=(const char *o) { v_ += o; return *this; }
  String &operator+=(char c) { v_ += c; return *this; }
  friend String operator+(String a, const String &b) { return a += b; }
  bool operator==(const String &o) const { return v_ == o.v_; }
  bool operator==(const char *o) const { return v_ == o; }
  bool operator!=(const char *o) const { return v_ != o; }
  operator const char *() const { return v_.c_str(); }

 private:
  std::string v_;
};
