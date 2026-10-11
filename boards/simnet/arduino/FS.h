// Host filesystem rooted at a directory, with the Arduino fs::File API.
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <memory>
#include <string>
#include "Stream.h"

namespace fs {

struct FSInfo {
  size_t totalBytes = 0, usedBytes = 0;
};

class File : public Stream {
public:
  File() {}
  File(FILE* f, const std::string& name) : _f(f, [](FILE* p) { if (p) fclose(p); }), _name(name) {}
  operator bool() const { return (bool)_f; }
  size_t write(uint8_t c) override { return _f && fputc(c, _f.get()) != EOF ? 1 : 0; }
  size_t write(const uint8_t* b, size_t n) override { return _f ? fwrite(b, 1, n, _f.get()) : 0; }
  using Print::write;
  int read() override { return _f ? fgetc(_f.get()) : -1; }
  size_t read(uint8_t* b, size_t n) { return _f ? fread(b, 1, n, _f.get()) : 0; }
  int peek() override {
    if (!_f) return -1;
    int c = fgetc(_f.get());
    if (c != EOF) ungetc(c, _f.get());
    return c;
  }
  int available() override {
    if (!_f) return 0;
    long p = ftell(_f.get());
    fseek(_f.get(), 0, SEEK_END);
    long e = ftell(_f.get());
    fseek(_f.get(), p, SEEK_SET);
    return (int)(e - p);
  }
  bool seek(uint32_t pos) { return _f && fseek(_f.get(), pos, SEEK_SET) == 0; }
  size_t position() const { return _f ? (size_t)ftell(_f.get()) : 0; }
  size_t size() const {
    if (!_f) return 0;
    long p = ftell(_f.get());
    fseek(_f.get(), 0, SEEK_END);
    long e = ftell(_f.get());
    fseek(_f.get(), p, SEEK_SET);
    return (size_t)e;
  }
  void flush() override { if (_f) fflush(_f.get()); }
  void close() { _f.reset(); }
  const char* name() const { return _name.c_str(); }
  bool isDirectory() const { return false; }
  File openNextFile() { return File(); }   // directory listing: unsupported on the host

private:
  std::shared_ptr<FILE> _f;
  std::string _name;
};

class FS {
public:
  explicit FS(const char* root = "") : _root(root) {}
  void setRoot(const std::string& root) { _root = root; }
  const std::string& root() const { return _root; }
  File open(const char* path, const char* mode = "r", bool create = false);
  bool exists(const char* path);
  bool remove(const char* path);
  bool rename(const char* from, const char* to);
  bool mkdir(const char* path);
  bool rmdir(const char* path) { return remove(path); }
  bool format();
  bool info(FSInfo& out);
  size_t usedBytes();

protected:
  std::string full(const char* path) const;
  std::string _root;
};

}  // namespace fs

using fs::File;
using fs::FSInfo;

#define FILE_READ "r"
#define FILE_WRITE "w"
