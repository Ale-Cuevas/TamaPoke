#pragma once
// The "microSD" is a host directory: TAMAPOKE_SD_ROOT, or tools/sdcard (the
// sprites the repo ships). Read-only use only.
#include "FS.h"

class SDMMCFS {
 public:
  bool setPins(int, int, int) { return true; }
  bool begin(const char *, bool, bool) {
    const char *r = getenv("TAMAPOKE_SD_ROOT");
    root_ = r ? r : TAMAPOKE_DEFAULT_SD_ROOT;
    struct stat st;
    return stat(root_.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
  }
  File open(const char *path, const char *mode = FILE_READ) {
    std::string p(path);
    return File(root_ + p, p.substr(p.rfind('/') + 1), mode);
  }
  File open(const String &path, const char *mode = FILE_READ) { return open(path.c_str(), mode); }
  bool exists(const String &path) {
    struct stat st;
    return stat((root_ + path.c_str()).c_str(), &st) == 0;
  }
  bool remove(const String &) { return false; }
  bool mkdir(const char *) { return true; }
  uint64_t cardSize() { return 32ULL << 30; }
  uint64_t totalBytes() { return 32ULL << 30; }
  uint64_t usedBytes() { return 64ULL << 20; }

 private:
  std::string root_;
};
extern SDMMCFS SD_MMC;
