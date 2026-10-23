// Companion protocol: frame layouts as stock MyMesh writes and reads them,
// and the client's one-command-at-a-time queue.
#include <unity.h>
#include <deque>
#include <vector>
#include <string.h>
#include "app/remote/companion_client.h"

using namespace app;

void setUp() {}
void tearDown() {}

// ---- frames, built byte by byte the way MyMesh.cpp builds them

static void test_contact_round_trip() {
  cp::Contact c{};
  for (int i = 0; i < cp::KEY; i++) c.key[i] = (uint8_t)(i + 1);
  c.type = 2;
  c.flags = 0x03;
  c.outPathLen = 0x42;   // 2 hops of 2 bytes
  c.outPath[0] = 0xAA;
  c.outPath[3] = 0xDD;
  strcpy(c.name, "Hilltop");
  c.lastAdvert = 1791727000;
  c.lat = -33123456;
  c.lon = 151654321;
  c.lastmod = 1791727100;
  uint8_t f[cp::MAX_FRAME];
  int n = cp::encodeContact(f, c);
  TEST_ASSERT_EQUAL(cp::CONTACT_FRAME, n);
  f[0] = cp::RC_CONTACT;   // the reply frame has the same layout
  cp::Contact d;
  TEST_ASSERT_TRUE(cp::parseContact(f, n, d));
  TEST_ASSERT_EQUAL_MEMORY(c.key, d.key, cp::KEY);
  TEST_ASSERT_EQUAL(2, d.type);
  TEST_ASSERT_EQUAL(0x42, d.outPathLen);
  TEST_ASSERT_EQUAL(0xDD, d.outPath[3]);
  TEST_ASSERT_EQUAL_STRING("Hilltop", d.name);
  TEST_ASSERT_EQUAL(-33123456, d.lat);
  TEST_ASSERT_EQUAL(151654321, d.lon);
  TEST_ASSERT_EQUAL_UINT32(1791727100, d.lastmod);
  TEST_ASSERT_FALSE(cp::parseContact(f, 60, d));   // too short
}

static void test_self_info() {
  uint8_t f[cp::MAX_FRAME] = {};
  int i = 0;
  f[i++] = cp::RC_SELF_INFO;
  f[i++] = 1;          // ADV_TYPE_CHAT
  f[i++] = 20;         // tx power
  f[i++] = 22;         // max
  memset(f + i, 0x5A, 32); i += 32;
  cp::wr32(f + i, 37000000); i += 4;
  cp::wr32(f + i, (uint32_t)-122000000); i += 4;
  f[i++] = 0;          // multi acks
  f[i++] = 1;          // advert location policy
  f[i++] = (2 << 4) | (1 << 2) | 2;
  f[i++] = 1;          // manual add
  cp::wr32(f + i, 910525); i += 4;
  cp::wr32(f + i, 62500); i += 4;
  f[i++] = 7;
  f[i++] = 5;
  memcpy(f + i, "Base node", 9); i += 9;
  cp::SelfInfo s;
  TEST_ASSERT_TRUE(cp::parseSelfInfo(f, i, s));
  TEST_ASSERT_EQUAL(20, s.txDbm);
  TEST_ASSERT_EQUAL(-122000000, s.lon);
  TEST_ASSERT_EQUAL(1, s.advertLocPolicy);
  TEST_ASSERT_EQUAL(1, s.manualAdd);
  TEST_ASSERT_EQUAL_UINT32(910525, s.freqKHz);
  TEST_ASSERT_EQUAL_UINT32(62500, s.bwHz);
  TEST_ASSERT_EQUAL(7, s.sf);
  TEST_ASSERT_EQUAL_STRING("Base node", s.name);
}

static void test_device_info() {
  uint8_t f[82] = {};
  f[0] = cp::RC_DEVICE_INFO;
  f[1] = 13;
  f[2] = 175;            // MAX_CONTACTS / 2
  f[3] = 40;
  cp::wr32(f + 4, 654321);
  memcpy(f + 20, "Heltec V3", 9);
  memcpy(f + 60, "v1.17.1", 7);
  f[80] = 1;
  f[81] = 2;
  cp::DeviceInfo d;
  TEST_ASSERT_TRUE(cp::parseDeviceInfo(f, sizeof f, d));
  TEST_ASSERT_EQUAL(350, d.maxContacts);
  TEST_ASSERT_EQUAL(40, d.maxChannels);
  TEST_ASSERT_EQUAL_UINT32(654321, d.blePin);
  TEST_ASSERT_EQUAL_STRING("Heltec V3", d.model);
  TEST_ASSERT_EQUAL_STRING("v1.17.1", d.version);
  TEST_ASSERT_TRUE(d.repeat);
  TEST_ASSERT_EQUAL(2, d.pathHashMode);
}

static void test_messages() {
  // Direct, signed (a room post): snr, reserved x2, key prefix, path, type, ts, author prefix, text.
  uint8_t f[cp::MAX_FRAME] = {};
  int i = 0;
  f[i++] = cp::RC_CONTACT_MSG_RECV_V3;
  f[i++] = (uint8_t)(int8_t)-22;
  i += 2;
  memcpy(f + i, "\x01\x02\x03\x04\x05\x06", 6); i += 6;
  f[i++] = 2;
  f[i++] = cp::TXT_SIGNED;
  cp::wr32(f + i, 1791727500); i += 4;
  memcpy(f + i, "\xAA\xBB\xCC\xDD", 4); i += 4;
  memcpy(f + i, "posted", 6); i += 6;
  cp::Msg m;
  TEST_ASSERT_TRUE(cp::parseMsg(f, i, m));
  TEST_ASSERT_FALSE(m.channel);
  TEST_ASSERT_EQUAL(-22, m.snr_x4);
  TEST_ASSERT_EQUAL(0x06, m.keyPrefix[5]);
  TEST_ASSERT_EQUAL(2, m.pathLen);
  TEST_ASSERT_TRUE(m.hasExtra);
  TEST_ASSERT_EQUAL(0xDD, m.extra[3]);
  TEST_ASSERT_EQUAL_STRING("posted", m.text);

  // Channel: snr, reserved x2, channel index, path, type, ts, "name: text".
  i = 0;
  f[i++] = cp::RC_CHANNEL_MSG_RECV_V3;
  f[i++] = 12;
  i += 2;
  f[i++] = 3;
  f[i++] = 0xFF;
  f[i++] = cp::TXT_PLAIN;
  cp::wr32(f + i, 1791727600); i += 4;
  memcpy(f + i, "Kim: hi", 7); i += 7;
  TEST_ASSERT_TRUE(cp::parseMsg(f, i, m));
  TEST_ASSERT_TRUE(m.channel);
  TEST_ASSERT_EQUAL(3, m.channelIdx);
  TEST_ASSERT_EQUAL(0xFF, m.pathLen);
  TEST_ASSERT_EQUAL_UINT32(1791727600, m.ts);
  TEST_ASSERT_EQUAL_STRING("Kim: hi", m.text);
}

static void test_encoders() {
  uint8_t f[cp::MAX_FRAME];
  uint8_t key[6] = {9, 8, 7, 6, 5, 4};
  int n = cp::encSendText(f, cp::TXT_PLAIN, 2, 0x11223344, key, "yo");
  TEST_ASSERT_EQUAL(1 + 1 + 1 + 4 + 6 + 2, n);   // MyMesh needs len >= 14 for a 1-char text
  TEST_ASSERT_EQUAL(cp::SEND_TXT_MSG, f[0]);
  TEST_ASSERT_EQUAL(2, f[2]);
  TEST_ASSERT_EQUAL_UINT32(0x11223344, cp::rd32(f + 3));
  TEST_ASSERT_EQUAL(4, f[12]);
  TEST_ASSERT_EQUAL('y', f[13]);

  n = cp::encAppStart(f, "meshmash");
  TEST_ASSERT_TRUE(n >= 8);
  TEST_ASSERT_EQUAL('m', f[8]);

  uint8_t secret[16] = {1};
  n = cp::encSetChannel(f, 2, "#test", secret);
  TEST_ASSERT_EQUAL(2 + 32 + 16, n);   // MyMesh: 128-bit secrets only
  TEST_ASSERT_EQUAL_STRING("#test", (const char*)f + 2);
  TEST_ASSERT_EQUAL(1, f[34]);

  n = cp::encRadio(f, 869525, 250000, 11, 5, false);
  TEST_ASSERT_EQUAL(12, n);
  TEST_ASSERT_EQUAL_UINT32(869525, cp::rd32(f + 1));
}

// ---- the client

struct FakeLink : FrameLink {
  bool up = true;
  std::vector<std::vector<uint8_t>> sent;
  std::deque<std::vector<uint8_t>> in;
  State state() const override { return up ? State::Ready : State::Lost; }
  bool send(const uint8_t* f, int n) override { sent.emplace_back(f, f + n); return true; }
  int recv(uint8_t* buf) override {
    if (in.empty()) return 0;
    auto f = in.front();
    in.pop_front();
    memcpy(buf, f.data(), f.size());
    return (int)f.size();
  }
  void reply(std::vector<uint8_t> f) { in.push_back(f); }
};

struct Log : CompanionClient::Handler {
  struct R { uint8_t cmd; uint32_t cookie; int code; };
  std::vector<R> replies;
  std::vector<uint8_t> pushes;
  void onReply(uint8_t cmd, uint32_t cookie, const uint8_t* f, int) override {
    replies.push_back({cmd, cookie, f ? f[0] : -1});
  }
  void onPush(const uint8_t* f, int) override { pushes.push_back(f[0]); }
};

static void test_one_command_in_flight() {
  FakeLink link;
  Log log;
  CompanionClient c(link, log);
  uint8_t f[4] = {cp::GET_DEVICE_TIME};
  c.queue(f, 1, 7);
  f[0] = cp::SYNC_NEXT_MESSAGE;
  c.queue(f, 1, 8);
  c.loop(0);
  TEST_ASSERT_EQUAL(1, (int)link.sent.size());   // the second waits
  link.reply({cp::P_MSG_WAITING});               // a push in between is not a reply
  link.reply({cp::RC_CURR_TIME, 1, 2, 3, 4});
  c.loop(10);
  TEST_ASSERT_EQUAL(1, (int)log.pushes.size());
  TEST_ASSERT_EQUAL(1, (int)log.replies.size());
  TEST_ASSERT_EQUAL(7, log.replies[0].cookie);
  TEST_ASSERT_EQUAL(cp::RC_CURR_TIME, log.replies[0].code);
  TEST_ASSERT_EQUAL(2, (int)link.sent.size());   // now the second
  TEST_ASSERT_EQUAL(cp::SYNC_NEXT_MESSAGE, link.sent[1][0]);
}

static void test_contact_list_streams() {
  FakeLink link;
  Log log;
  CompanionClient c(link, log);
  uint8_t f[1] = {cp::GET_CONTACTS};
  c.queue(f, 1);
  c.loop(0);
  link.reply({cp::RC_CONTACTS_START, 2, 0, 0, 0});
  link.reply({cp::RC_CONTACT});
  c.loop(4000);
  link.reply({cp::RC_CONTACT});
  c.loop(8000);   // more than the timeout since sending, but contacts kept coming
  link.reply({cp::RC_END_OF_CONTACTS, 0, 0, 0, 0});
  c.loop(9000);
  TEST_ASSERT_EQUAL(4, (int)log.replies.size());
  TEST_ASSERT_EQUAL(cp::RC_END_OF_CONTACTS, log.replies[3].code);
  TEST_ASSERT_TRUE(c.idle());
}

static void test_timeout_and_late_reply() {
  FakeLink link;
  Log log;
  CompanionClient c(link, log);
  uint8_t f[1] = {cp::GET_DEVICE_TIME};
  c.queue(f, 1, 1);
  c.loop(0);
  c.loop(CompanionClient::REPLY_TIMEOUT_MS);
  TEST_ASSERT_EQUAL(1, (int)log.replies.size());
  TEST_ASSERT_EQUAL(-1, log.replies[0].code);    // no reply
  link.reply({cp::RC_CURR_TIME, 0, 0, 0, 0});       // too late: ignored
  c.loop(CompanionClient::REPLY_TIMEOUT_MS + 10);
  TEST_ASSERT_EQUAL(1, (int)log.replies.size());
}

static void test_link_loss_fails_everything() {
  FakeLink link;
  Log log;
  CompanionClient c(link, log);
  uint8_t f[1] = {cp::GET_DEVICE_TIME};
  c.queue(f, 1);
  c.queue(f, 1);
  c.loop(0);
  link.up = false;
  c.loop(10);
  TEST_ASSERT_EQUAL(2, (int)log.replies.size());
  TEST_ASSERT_TRUE(c.idle());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_contact_round_trip);
  RUN_TEST(test_self_info);
  RUN_TEST(test_device_info);
  RUN_TEST(test_messages);
  RUN_TEST(test_encoders);
  RUN_TEST(test_one_command_in_flight);
  RUN_TEST(test_contact_list_streams);
  RUN_TEST(test_timeout_and_late_reply);
  RUN_TEST(test_link_loss_fails_everything);
  return UNITY_END();
}
