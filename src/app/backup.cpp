#include "app/backup.h"
#include <stdio.h>
#include <string.h>

namespace app {

namespace {

constexpr uint16_t FORMAT = 1;
constexpr int CHUNK = 256;

// What a backup may contain, and what a restore may write: nothing else.
struct Entry {
  const char* path;
  enum { Always, Identity, Messages } when;
};
const Entry kFiles[] = {
    {"/identity/_main.id", Entry::Identity},
    {"/prefs.json", Entry::Always},
    {"/new_prefs", Entry::Always},
    {"/contacts3", Entry::Always},
    {"/channels2", Entry::Always},
    {"/ui/prefs.bin", Entry::Always},
    {"/ui/msgs.bin", Entry::Messages},
    {"/ui/read.bin", Entry::Messages},
};

bool allowed(const char* path) {
  for (const Entry& e : kFiles)
    if (!strcmp(e.path, path)) return true;
  return false;
}

template <typename T>
bool put(File& f, const T& v) { return f.write((const uint8_t*)&v, sizeof v) == sizeof v; }
template <typename T>
bool get(File& f, T& v) { return f.read((uint8_t*)&v, sizeof v) == sizeof v; }

void setErr(char* err, size_t n, const char* msg) {
  if (err && n) snprintf(err, n, "%s", msg);
}

struct Header {
  char magic[4];
  uint16_t format, sections;
  uint32_t created;
  char fw[16];
} __attribute__((packed));

// Walk the sections of an open backup. With `sink`, each verified section
// is handed over (re-read from the file); without, only CRCs are checked.
template <typename Sink>
bool walk(File& f, uint16_t sections, Sink sink) {
  uint8_t buf[CHUNK];
  for (int s = 0; s < sections; s++) {
    uint8_t plen;
    char path[64];
    uint32_t len, crc;
    if (!get(f, plen) || plen >= sizeof path || f.read((uint8_t*)path, plen) != plen) return false;
    path[plen] = 0;
    if (!get(f, len) || !get(f, crc)) return false;
    size_t start = f.position();
    uint32_t c = 0;
    for (uint32_t done = 0; done < len;) {
      int n = len - done > CHUNK ? CHUNK : (int)(len - done);
      if ((int)f.read(buf, n) != n) return false;
      c = Backup::crc32(c, buf, n);
      done += n;
    }
    if (c != crc) return false;
    if (!sink(f, path, start, len)) return false;
    f.seek(start + len);
  }
  return true;
}

void mkParents(fs::FS& fs, const char* path) {
  char dir[64];
  snprintf(dir, sizeof dir, "%s", path);
  char* slash = strrchr(dir, '/');
  if (slash && slash != dir) {
    *slash = 0;
    fs.mkdir(dir);
  }
}

}  // namespace

uint32_t Backup::crc32(uint32_t crc, const uint8_t* p, size_t n) {
  crc = ~crc;
  while (n--) {
    crc ^= *p++;
    for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1)));
  }
  return ~crc;
}

bool Backup::write(fs::FS& flash, fs::FS& sd, const Options& o, uint32_t now, const char* fw, char* err, size_t n) {
  sd.mkdir(DIR);
  File out = sd.open(TMP_PATH, "w", true);
  if (!out) { setErr(err, n, "Cannot write to SD card"); return false; }

  Header h;
  memcpy(h.magic, "MCBK", 4);
  h.format = FORMAT;
  h.sections = 0;
  h.created = now;
  memset(h.fw, 0, sizeof h.fw);
  snprintf(h.fw, sizeof h.fw, "%s", fw);
  bool ok = put(out, h);

  uint8_t buf[CHUNK];
  for (const Entry& e : kFiles) {
    if ((e.when == Entry::Identity && !o.identity) || (e.when == Entry::Messages && !o.messages)) continue;
    if (!flash.exists(e.path)) continue;
    File in = flash.open(e.path, "r");
    if (!in) continue;
    uint32_t len = (uint32_t)in.size(), crc = 0;
    for (int r; (r = (int)in.read(buf, CHUNK)) > 0;) crc = crc32(crc, buf, r);
    in.seek(0);
    uint8_t plen = (uint8_t)strlen(e.path);
    ok = ok && put(out, plen) && out.write((const uint8_t*)e.path, plen) == plen && put(out, len) && put(out, crc);
    for (int r; ok && (r = (int)in.read(buf, CHUNK)) > 0;) ok = out.write(buf, r) == (size_t)r;
    in.close();
    h.sections++;
  }
  ok = ok && out.seek(0) && put(out, h);
  out.close();
  if (!ok) {
    sd.remove(TMP_PATH);
    setErr(err, n, "SD card write failed");
    return false;
  }
  // Keep the previous good backup, then move the new one into place.
  if (sd.exists(PATH)) {
    sd.remove(PREV_PATH);
    sd.rename(PATH, PREV_PATH);
  }
  if (!sd.rename(TMP_PATH, PATH)) { setErr(err, n, "Could not finish the backup file"); return false; }
  return true;
}

Backup::Info Backup::info(fs::FS& sd, bool previous) {
  Info i;
  memset(&i, 0, sizeof i);
  const char* path = previous ? PREV_PATH : PATH;
  if (!sd.exists(path)) return i;
  File f = sd.open(path, "r");
  Header h;
  if (!f || !get(f, h) || memcmp(h.magic, "MCBK", 4) || h.format == 0) return i;
  i.created = h.created;
  i.sections = h.sections;
  memcpy(i.fw, h.fw, sizeof i.fw);
  i.fw[sizeof i.fw - 1] = 0;
  i.valid = walk(f, h.sections, [](File&, const char*, size_t, uint32_t) { return true; });
  f.close();
  return i;
}

bool Backup::restore(fs::FS& flash, fs::FS& sd, bool previous, char* err, size_t n) {
  if (!info(sd, previous).valid) { setErr(err, n, "Backup is missing or damaged"); return false; }
  File f = sd.open(previous ? PREV_PATH : PATH, "r");
  Header h;
  if (!f || !get(f, h)) { setErr(err, n, "Cannot read backup"); return false; }
  uint8_t buf[CHUNK];
  bool ok = walk(f, h.sections, [&](File& in, const char* path, size_t start, uint32_t len) {
    if (!allowed(path)) return true;   // newer firmware's file: skip
    mkParents(flash, path);
    File out = flash.open(path, "w", true);
    if (!out) return false;
    in.seek(start);
    for (uint32_t done = 0; done < len;) {
      int c = len - done > CHUNK ? CHUNK : (int)(len - done);
      if ((int)in.read(buf, c) != c || out.write(buf, c) != (size_t)c) { out.close(); return false; }
      done += c;
    }
    out.close();
    return true;
  });
  f.close();
  if (!ok) setErr(err, n, "Restore failed while writing");
  return ok;
}

}  // namespace app
