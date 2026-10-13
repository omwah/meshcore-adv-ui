// Ping bot: what counts as a ping, and the reply's text.
#include <unity.h>
#include <string.h>
#include <stdio.h>
#include <string>
#include "app/ping_bot.h"

using namespace app;

// Knows one relay: a repeater whose key starts 7f 01.
static bool namer(void*, const uint8_t* hash, int size, char* out, int n) {
  static const uint8_t key[] = {0x7f, 0x01};
  if (memcmp(hash, key, size)) return false;
  snprintf(out, n, "%s", "Hilltop Repeater");
  return true;
}

static std::string pong(uint8_t pathLen, const uint8_t* path, bool stored = false, float snr = 6.5f) {
  char out[160];
  formatPong(out, sizeof out, pathLen, path, stored, snr, -82, namer, nullptr);
  return out;
}

void test_is_ping() {
  TEST_ASSERT_TRUE(isPing("ping"));
  TEST_ASSERT_TRUE(isPing("Ping!"));
  TEST_ASSERT_TRUE(isPing(" TEST "));
  TEST_ASSERT_TRUE(isPing("test."));
  TEST_ASSERT_TRUE(isPing("PING?!"));
  TEST_ASSERT_FALSE(isPing("pinging"));
  TEST_ASSERT_FALSE(isPing("ping me"));
  TEST_ASSERT_FALSE(isPing("pong"));
  TEST_ASSERT_FALSE(isPing("pong: 1 hop 7f, SNR 6.5"));
  TEST_ASSERT_FALSE(isPing(""));
  TEST_ASSERT_FALSE(isPing("testing"));
}

void test_neighbour() {
  TEST_ASSERT_EQUAL_STRING("pong: direct, SNR 6.5, RSSI -82", pong(0, nullptr).c_str());
  TEST_ASSERT_EQUAL_STRING("pong: direct, SNR -3.3, RSSI -82", pong(0, nullptr, false, -3.25f).c_str());
}

void test_hops_named_and_hex() {
  const uint8_t path[] = {0xa1, 0x7f, 0xc3};
  TEST_ASSERT_EQUAL_STRING("pong: 3 hops a1>Hilltop Re>c3, SNR 6.5", pong(3, path).c_str());
  TEST_ASSERT_EQUAL_STRING("pong: 1 hop Hilltop Re, SNR 6.5", pong(1, path + 1).c_str());
}

void test_two_byte_hashes() {
  const uint8_t path[] = {0x7f, 0x01, 0x7f, 0x02};
  TEST_ASSERT_EQUAL_STRING("pong: 2 hops Hilltop Re>7f02, SNR 6.5", pong(0x40 | 2, path).c_str());
}

void test_path_unknown() {
  TEST_ASSERT_EQUAL_STRING("pong: 2 hops, SNR 6.5", pong(2, nullptr).c_str());
  TEST_ASSERT_EQUAL_STRING("pong: by stored route, SNR 6.5", pong(0xFF, nullptr, true).c_str());
}

void test_stored_route() {
  const uint8_t path[] = {0x7f, 0x22};
  TEST_ASSERT_EQUAL_STRING("pong: 2 hops Hilltop Re>22 (stored route), SNR 6.5", pong(2, path, true).c_str());
}

void test_fits_the_buffer() {
  uint8_t path[63];
  memset(path, 0xab, sizeof path);
  char out[40];
  int n = formatPong(out, sizeof out, 63, path, false, 6.5f, -82, namer, nullptr);
  TEST_ASSERT_EQUAL(39, n);
  TEST_ASSERT_EQUAL(39, (int)strlen(out));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_is_ping);
  RUN_TEST(test_neighbour);
  RUN_TEST(test_hops_named_and_hex);
  RUN_TEST(test_two_byte_hashes);
  RUN_TEST(test_path_unknown);
  RUN_TEST(test_stored_route);
  RUN_TEST(test_fits_the_buffer);
  return UNITY_END();
}
