// The companion protocol client against a stock companion MyMesh, over the
// simulated BLE pipe: handshake, contact list, a DM out (with its ACK) and a
// DM in (synced after the node's "message waiting" push).
// Run: simnet --remote-smoke
#include <stdio.h>
#include <string.h>
#include "sim_node.h"
#include "pipe_link.h"
#include "app/remote/companion_client.h"

using namespace app;

namespace {

struct Recorder : CompanionClient::Handler {
  cp::DeviceInfo dev{};
  cp::SelfInfo self{};
  int contacts = 0, ends = 0, timeouts = 0, waiting = 0, confirmed = 0, messages = 0;
  uint32_t expectAck = 0;
  char lastText[64] = "";
  cp::Contact bob{};
  bool haveBob = false;
  CompanionClient* client = nullptr;

  void onReply(uint8_t cmd, uint32_t, const uint8_t* f, int len) override {
    if (!f) { timeouts++; printf("  no reply to command %u\n", cmd); return; }
    switch (f[0]) {
      case cp::RC_DEVICE_INFO: cp::parseDeviceInfo(f, len, dev); break;
      case cp::RC_SELF_INFO: cp::parseSelfInfo(f, len, self); break;
      case cp::RC_CONTACT: {
        cp::Contact c;
        if (cp::parseContact(f, len, c)) {
          contacts++;
          if (!strcmp(c.name, "Bob")) { bob = c; haveBob = true; }
        }
        break;
      }
      case cp::RC_END_OF_CONTACTS: ends++; break;
      case cp::RC_SENT: {
        cp::Sent s;
        if (cp::parseSent(f, len, s)) expectAck = s.tag;
        break;
      }
      case cp::RC_CONTACT_MSG_RECV_V3: case cp::RC_CHANNEL_MSG_RECV_V3: {
        cp::Msg m;
        if (cp::parseMsg(f, len, m)) { messages++; snprintf(lastText, sizeof lastText, "%s", m.text); }
        uint8_t q[1] = {cp::SYNC_NEXT_MESSAGE};   // until NO_MORE_MESSAGES
        client->queue(q, 1);
        break;
      }
      default: break;
    }
  }
  void onPush(const uint8_t* f, int len) override {
    if (f[0] == cp::P_MSG_WAITING) {
      waiting++;
      uint8_t q[1] = {cp::SYNC_NEXT_MESSAGE};
      client->queue(q, 1);
    } else if (f[0] == cp::P_SEND_CONFIRMED && len >= 5 && cp::rd32(f + 1) == expectAck) {
      confirmed++;
    }
  }
};

}  // namespace

int simnetRemoteSmoke() {
  SimMedium medium;
  PipeLink pipe;
  SimNode base(medium, 0, "Base", "sim-data/remote-smoke/base", 1791727500, nullptr, true, &pipe);
  SimNode bob(medium, 1, "Bob", "sim-data/remote-smoke/bob", 1791727500);
  medium.link(0, 1, 7.5f);
  base.begin();
  bob.begin();

  Recorder rec;
  CompanionClient client(pipe, rec);
  rec.client = &client;
  unsigned long t = 0;
  auto run = [&](unsigned long ms) {
    for (unsigned long end = t + ms; t < end; t += 5) {
      sim_setMillis(t);
      base.loop();
      bob.loop();
      client.loop(t);
    }
  };
  base.select(); base.mesh().advert();
  bob.select(); bob.mesh().advert();
  run(5000);

  app::Peer p{};
  pipe.connect(p, 123456);
  uint8_t f[cp::MAX_FRAME];
  client.queue(f, cp::encDeviceQuery(f));
  client.queue(f, cp::encAppStart(f, "smoke"));
  client.queue(f, cp::encGetContacts(f, 0));
  run(3000);
  printf("device: fw %u, contacts max %u, channels %u; self '%s' at %.3f MHz; contacts %d (end %d)\n",
         rec.dev.fwCode, rec.dev.maxContacts, rec.dev.maxChannels, rec.self.name, rec.self.freqKHz / 1000.0,
         rec.contacts, rec.ends);
  int fail = 0;
  if (strcmp(rec.self.name, "Base") || !rec.haveBob || rec.ends != 1) { printf("FAIL: handshake/contacts\n"); fail = 1; }

  if (rec.haveBob) {
    client.queue(f, cp::encSendText(f, cp::TXT_PLAIN, 0, 1791727600, rec.bob.key, "hello bob"));
    run(10000);
    printf("DM out: expected ack %08x, confirmed %d\n", rec.expectAck, rec.confirmed);
    if (!rec.expectAck || !rec.confirmed) { printf("FAIL: DM out not confirmed\n"); fail = 1; }
  }

  bob.select();
  ContactInfo* to = bob.mesh().searchContactsByPrefix("Base");
  if (to) {
    uint32_t ack, est;
    bob.mesh().sendMessage(*to, 1791727700, 0, "hi base", ack, est);
  }
  run(10000);
  printf("DM in: waiting pushes %d, messages %d, last '%s'; node frames dropped %u, timeouts %d\n", rec.waiting,
         rec.messages, rec.lastText, pipe.dropped, rec.timeouts);
  if (rec.messages != 1 || strcmp(rec.lastText, "hi base")) { printf("FAIL: DM in not synced\n"); fail = 1; }
  printf(fail ? "remote smoke FAILED\n" : "remote smoke ok\n");
  return fail;
}
