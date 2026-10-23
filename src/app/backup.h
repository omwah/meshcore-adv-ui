// Backup of a node's configuration to removable storage, and restore.
//
// The backup holds MeshCore's own files byte for byte (identity, prefs,
// contacts, channels) plus the UI's files, so MeshCore keeps handling its own
// formats (including migrations) after a restore.
//
// Container (little endian):
//   "MCBK" u16 format u16 nsections u32 created(epoch) char fw[16]
//   per section: u8 pathLen, path, u32 len, u32 crc32, data
// Readers skip sections they don't know; a restore checks every CRC before
// writing anything. Files are written as backup.tmp and renamed into place,
// keeping the previous backup as backup.bak.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <FS.h>

namespace app {

class Backup {
public:
  static constexpr const char* DIR = "/meshmash";
  static constexpr const char* PATH = "/meshmash/backup.bin";
  static constexpr const char* PREV_PATH = "/meshmash/backup.bak";
  static constexpr const char* TMP_PATH = "/meshmash/backup.tmp";

  struct Options {
    bool identity;   // include the private key
    bool messages;   // include message history
  };
  struct Info {
    bool valid;
    uint32_t created;
    uint16_t sections;
    char fw[16];
  };

  static bool write(fs::FS& flash, fs::FS& sd, const Options& o, uint32_t now, const char* fw, char* err, size_t n);
  static bool restore(fs::FS& flash, fs::FS& sd, bool previous, char* err, size_t n);
  static Info info(fs::FS& sd, bool previous);   // header plus a full CRC check

  // The same container holding named blobs rather than files: a remote
  // node's settings, which live on that node (RemoteAdapter). `path` is
  // replaced; the old one is kept as `prevPath`.
  struct Section {
    const char* name;
    const uint8_t* data;
    uint32_t len;
    fs::FS* file = nullptr;   // or: the contents of file `name` on this filesystem
  };
  static bool writeSections(fs::FS& sd, const char* path, const char* prevPath, const Section* s, int n, uint32_t now,
                            const char* fw, char* err, size_t en);
  static Info infoAt(fs::FS& sd, const char* path);
  // Every section of a valid backup, CRC-checked first; `fn` reads `len`
  // bytes of the section from `f`.
  typedef bool (*SectionFn)(void* ctx, const char* name, File& f, uint32_t len);
  static bool readSections(fs::FS& sd, const char* path, SectionFn fn, void* ctx, char* err, size_t en);

  static uint32_t crc32(uint32_t crc, const uint8_t* p, size_t n);
};

}  // namespace app
