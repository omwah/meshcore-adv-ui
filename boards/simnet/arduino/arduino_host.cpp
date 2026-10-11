#include "Arduino.h"
#include "FS.h"
#include "LittleFS.h"
#include "Wire.h"
#include <random>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

static unsigned long s_ms = 0;
static std::mt19937 s_rng(12345);

unsigned long millis() { return s_ms; }
unsigned long micros() { return s_ms * 1000UL; }
void sim_setMillis(unsigned long ms) { s_ms = ms; }
void delay(unsigned long) {}
void yield() {}
long random(long max) { return max > 0 ? (long)(s_rng() % (unsigned long)max) : 0; }
long random(long min, long max) { return max > min ? min + random(max - min) : min; }
void randomSeed(unsigned long seed) { s_rng.seed(seed); }

char* ltoa(long v, char* buf, int base) {
  if (base == 16) sprintf(buf, "%lx", v);
  else sprintf(buf, "%ld", v);
  return buf;
}
char* itoa(int v, char* buf, int base) { return ltoa(v, buf, base); }

HardwareSerial Serial;
size_t HardwareSerial::write(uint8_t c) {
  // MeshCore debug output; keep it off stdout (which may carry results).
  fputc(c, stderr);
  return 1;
}

TwoWire Wire;
fs::FS LittleFS;

namespace fs {

std::string FS::full(const char* path) const {
  std::string p = _root;
  if (path[0] != '/') p += '/';
  return p + path;
}

static void mkdirs(const std::string& path) {
  for (size_t i = 1; i < path.size(); i++)
    if (path[i] == '/') ::mkdir(path.substr(0, i).c_str(), 0755);
}

File FS::open(const char* path, const char* mode, bool create) {
  std::string f = full(path);
  bool write = mode[0] == 'w' || mode[0] == 'a';
  if (write || create) mkdirs(f);
  const char* m = mode[0] == 'w' ? "w+b" : mode[0] == 'a' ? "a+b" : mode[1] == '+' ? "r+b" : "rb";
  FILE* fp = fopen(f.c_str(), m);
  return fp ? File(fp, path) : File();
}

bool FS::exists(const char* path) {
  struct stat st;
  return stat(full(path).c_str(), &st) == 0;
}

bool FS::remove(const char* path) { return ::remove(full(path).c_str()) == 0; }
bool FS::rename(const char* a, const char* b) { return ::rename(full(a).c_str(), full(b).c_str()) == 0; }
bool FS::mkdir(const char* path) { mkdirs(full(path) + "/"); return true; }

static void rmTree(const std::string& p) {
  DIR* d = opendir(p.c_str());
  if (!d) { ::remove(p.c_str()); return; }
  while (dirent* e = readdir(d)) {
    std::string n = e->d_name;
    if (n == "." || n == "..") continue;
    rmTree(p + "/" + n);
  }
  closedir(d);
  ::rmdir(p.c_str());
}

bool FS::format() {
  rmTree(_root);
  mkdirs(_root + "/");
  return true;
}

bool FS::info(FSInfo& out) {
  out.totalBytes = 1408 * 1024;
  out.usedBytes = usedBytes();
  return true;
}

size_t FS::usedBytes() {
  size_t total = 0;
  DIR* d = opendir(_root.c_str());
  if (!d) return 0;
  while (dirent* e = readdir(d)) {
    struct stat st;
    if (stat((_root + "/" + e->d_name).c_str(), &st) == 0 && S_ISREG(st.st_mode)) total += st.st_size;
  }
  closedir(d);
  return total;
}

}  // namespace fs
