// Backup container: round trip, damage detection, and forward compatibility.
#include <unity.h>
#include <string>
#include <sys/stat.h>
#include <FS.h>
#include "app/backup.h"

using app::Backup;

static std::string base() {
  static std::string b;
  if (b.empty()) {
    char tmpl[] = "sim-data/test-backup-XXXXXX";   // git-ignored, one per run
    mkdir("sim-data", 0755);
    b = mkdtemp(tmpl);
  }
  return b;
}

static void writeFile(fs::FS& f, const char* path, const std::string& data) {
  File o = f.open(path, "w", true);
  o.write((const uint8_t*)data.data(), data.size());
  o.close();
}

static std::string readFile(fs::FS& f, const char* path) {
  File i = f.open(path, "r");
  std::string s;
  for (int c; i && (c = i.read()) >= 0;) s += (char)c;
  return s;
}

static int s_case = 0;
struct Fixture {
  fs::FS flash, sd;
  Fixture() {
    std::string d = base() + "/" + std::to_string(++s_case);
    flash.setRoot(d + "/flash");
    sd.setRoot(d + "/sd");
    flash.format();
    sd.format();
    writeFile(flash, "/identity/_main.id", std::string(96, 'K'));
    writeFile(flash, "/prefs.json", "{\"name\":\"node\"}");
    writeFile(flash, "/contacts3", std::string(152, 'C'));
    writeFile(flash, "/ui/prefs.bin", "UIPREFS");
    writeFile(flash, "/ui/msgs.bin", "MESSAGES");
  }
};

void test_round_trip() {
  Fixture f;
  char err[64] = {};
  TEST_ASSERT_TRUE(Backup::write(f.flash, f.sd, {true, false}, 1791727500, "v1.17.1", err, sizeof err));
  Backup::Info i = Backup::info(f.sd, false);
  TEST_ASSERT_TRUE(i.valid);
  TEST_ASSERT_EQUAL_UINT32(1791727500, i.created);
  TEST_ASSERT_EQUAL(4, i.sections);   // identity, prefs, contacts, ui prefs (no messages)

  f.flash.format();
  TEST_ASSERT_TRUE(Backup::restore(f.flash, f.sd, false, err, sizeof err));
  TEST_ASSERT_EQUAL_STRING(std::string(96, 'K').c_str(), readFile(f.flash, "/identity/_main.id").c_str());
  TEST_ASSERT_EQUAL_STRING("UIPREFS", readFile(f.flash, "/ui/prefs.bin").c_str());
  TEST_ASSERT_FALSE(f.flash.exists("/ui/msgs.bin"));
}

void test_identity_can_be_left_out() {
  Fixture f;
  char err[64];
  Backup::write(f.flash, f.sd, {false, true}, 1, "x", err, sizeof err);
  f.flash.format();
  Backup::restore(f.flash, f.sd, false, err, sizeof err);
  TEST_ASSERT_FALSE(f.flash.exists("/identity/_main.id"));
  TEST_ASSERT_EQUAL_STRING("MESSAGES", readFile(f.flash, "/ui/msgs.bin").c_str());
}

void test_corruption_is_detected_and_nothing_written() {
  Fixture f;
  char err[64];
  Backup::write(f.flash, f.sd, {true, false}, 1, "x", err, sizeof err);
  std::string b = readFile(f.sd, Backup::PATH);
  b[b.size() - 3] ^= 0x40;   // flip a bit in the last section
  writeFile(f.sd, Backup::PATH, b);
  TEST_ASSERT_FALSE(Backup::info(f.sd, false).valid);
  f.flash.format();
  TEST_ASSERT_FALSE(Backup::restore(f.flash, f.sd, false, err, sizeof err));
  TEST_ASSERT_FALSE(f.flash.exists("/prefs.json"));   // all-or-nothing
}

void test_truncated_file_is_invalid() {
  Fixture f;
  char err[64];
  Backup::write(f.flash, f.sd, {true, false}, 1, "x", err, sizeof err);
  std::string b = readFile(f.sd, Backup::PATH);
  writeFile(f.sd, Backup::PATH, b.substr(0, b.size() / 2));
  TEST_ASSERT_FALSE(Backup::info(f.sd, false).valid);
}

void test_previous_backup_is_kept() {
  Fixture f;
  char err[64];
  Backup::write(f.flash, f.sd, {true, false}, 100, "x", err, sizeof err);
  writeFile(f.flash, "/prefs.json", "{\"name\":\"changed\"}");
  Backup::write(f.flash, f.sd, {true, false}, 200, "x", err, sizeof err);
  TEST_ASSERT_EQUAL_UINT32(200, Backup::info(f.sd, false).created);
  TEST_ASSERT_EQUAL_UINT32(100, Backup::info(f.sd, true).created);
  Backup::restore(f.flash, f.sd, true, err, sizeof err);
  TEST_ASSERT_EQUAL_STRING("{\"name\":\"node\"}", readFile(f.flash, "/prefs.json").c_str());
}

// A section a newer firmware added (unknown path) is skipped, and a path
// outside the allowed set is never written.
void test_unknown_sections_are_skipped() {
  Fixture f;
  char err[64];
  Backup::write(f.flash, f.sd, {true, false}, 1, "x", err, sizeof err);
  std::string b = readFile(f.sd, Backup::PATH);
  const char* path = "/future/thing";
  std::string data = "FUTURE";
  uint32_t len = data.size(), crc = Backup::crc32(0, (const uint8_t*)data.data(), data.size());
  std::string sec;
  sec += (char)strlen(path);
  sec += path;
  sec.append((const char*)&len, 4);
  sec.append((const char*)&crc, 4);
  sec += data;
  b += sec;
  uint16_t n;
  memcpy(&n, &b[6], 2);
  n++;
  memcpy(&b[6], &n, 2);
  writeFile(f.sd, Backup::PATH, b);
  TEST_ASSERT_TRUE(Backup::info(f.sd, false).valid);
  f.flash.format();
  TEST_ASSERT_TRUE(Backup::restore(f.flash, f.sd, false, err, sizeof err));
  TEST_ASSERT_FALSE(f.flash.exists("/future/thing"));
  TEST_ASSERT_TRUE(f.flash.exists("/contacts3"));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_round_trip);
  RUN_TEST(test_identity_can_be_left_out);
  RUN_TEST(test_corruption_is_detected_and_nothing_written);
  RUN_TEST(test_truncated_file_is_invalid);
  RUN_TEST(test_previous_backup_is_kept);
  RUN_TEST(test_unknown_sections_are_skipped);
  return UNITY_END();
}
