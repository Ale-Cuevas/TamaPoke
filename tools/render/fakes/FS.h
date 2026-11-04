#pragma once
// Arduino FS File over stdio, rooted in a host directory (see SD_MMC.h).
#include <dirent.h>
#include <sys/stat.h>
#include <string>
#include "Arduino.h"

#define FILE_READ "r"
#define FILE_WRITE "w"

class File {
 public:
  File() {}
  File(const std::string &host, const std::string &name, const char *mode) : host_(host), name_(name) {
    struct stat st;
    if (stat(host.c_str(), &st) != 0) return;
    if (S_ISDIR(st.st_mode)) {
      dir_ = opendir(host.c_str());
    } else {
      fp_ = fopen(host.c_str(), mode[0] == 'w' ? "wb" : "rb");
      size_ = (size_t)st.st_size;
    }
  }
  File(const File &) = delete;
  File &operator=(const File &) = delete;
  File(File &&o) noexcept { *this = std::move(o); }
  File &operator=(File &&o) noexcept {
    close();
    fp_ = o.fp_; dir_ = o.dir_; size_ = o.size_; host_ = o.host_; name_ = o.name_;
    o.fp_ = nullptr; o.dir_ = nullptr;
    return *this;
  }
  ~File() { close(); }
  explicit operator bool() const { return fp_ || dir_; }
  size_t size() const { return size_; }
  size_t read(uint8_t *buf, size_t n) { return fp_ ? fread(buf, 1, n, fp_) : 0; }
  size_t write(const uint8_t *buf, size_t n) { return fp_ ? fwrite(buf, 1, n, fp_) : 0; }
  bool isDirectory() const { return dir_ != nullptr; }
  const char *name() const { return name_.c_str(); }
  File openNextFile() {
    if (!dir_) return File();
    while (dirent *e = readdir(dir_)) {
      if (e->d_name[0] == '.') continue;
      return File(host_ + "/" + e->d_name, e->d_name, FILE_READ);
    }
    return File();
  }
  void close() {
    if (fp_) fclose(fp_);
    if (dir_) closedir(dir_);
    fp_ = nullptr;
    dir_ = nullptr;
  }

 private:
  FILE *fp_ = nullptr;
  DIR *dir_ = nullptr;
  size_t size_ = 0;
  std::string host_, name_;
};
