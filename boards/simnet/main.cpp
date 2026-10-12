// simnet: the real UI on real MeshCore companion nodes over a simulated LoRa
// medium, all in one process on a virtual clock.
//
//   simnet [-s script] [-o outdir] [--data dir]
//   simnet --smoke
//   simnet --remote-smoke
//
// The UI node is "Cardputer" (id 0). Commands besides runner.cpp's:
//   node <name> [snr] [echo] [telem]   add a companion node linked to the UI node
//   repeater <name> [snr] [password]   add a stock MeshCore repeater
//   room <name> [snr] [password]       add a stock MeshCore room server
//   as <name> login <room> <password>  a companion logs in to a room/repeater
//   link <a> <b> <snr> | unlink <a> <b>
//   as <name> advert [flood]
//   as <name> dm <to> <text>      a direct message (to = node name)
//   as <name> chan <slot> <text>  post to channel slot (0 = Public)
//   as <name> join <#hashtag>
//   reboot                        restart the UI node on its existing flash
//   wipe                          erase the UI node's flash and reboot (fresh install)
//   sd on|off                     insert or remove the SD card (sim-data/.../sd)
//   expect identity same|new      UI node key vs. before the last wipe
//   expect screen on|off
//   expect sounds <n>          notification chimes played so far
//   expect request idle|busy|ok|failed|timeout
//   expect repeats <n>         repeats heard of the last message we sent in the open chat
//   expect contacts|unread|heard|threads <n>
//   expect status <pending|sent|delivered|failed>   last message we sent in the open chat
//   dump                          print the UI node's view to stdout
#ifndef PIO_UNIT_TESTING
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <memory>
#include <string>
#include "ui/app.h"
#include "ui/screens/screens.h"
#include "app/mesh_adapter.h"
#include "runner.h"
#include "sim_node.h"
#include "bot.h"
#include "sim_repeater.h"
#include "sim_room.h"

int simnetSmoke();
int simnetRemoteSmoke();

namespace {

constexpr uint32_t kEpoch = 1791727500;   // 2026-10-11 14:05 UTC

class SimSound : public hal::Sound {
public:
  void play(const hal::Tone*, int) override { played++; }
  int played = 0;
};
SimSound g_sound;

class SimPlatform : public app::Platform {
public:
  fs::FS* sd() override { return card ? &sdFs : nullptr; }
  fs::FS sdFs;
  bool card = true;
  uint32_t heapFree() override { return 118 * 1024; }
  uint32_t heapMin() override { return 96 * 1024; }
  uint32_t heapMaxBlock() override { return 64 * 1024; }
  void reboot() override { rebootRequested = true; }
  bool rebootRequested = false;
};

struct Bot {
  std::unique_ptr<BotTask> task;
  std::unique_ptr<SimNode> node;
};

class NetWorld : public sim::World {
public:
  NetWorld(const std::string& dir, app::MeshAdapter& ui, MultiSerialInterface& ifaces, SimPlatform& plat)
      : _dir(dir), _ui(ui), _ifaces(ifaces), _plat(plat) {
    _plat.sdFs.setRoot(dir + "/sd");
    _plat.sdFs.mkdir("/");
  }

  void attach(ui::App& app, ui::Screen* first) override {
    _app = &app;
    _first = first;
  }

  void start() override {
    bootUiNode(_fresh);
    _ids["Cardputer"] = 0;
  }

  // Start (or restart) the UI node the way the firmware's setup() does.
  void bootUiNode(bool format) {
    _uiNode.reset();
    _uiNode.reset(new SimNode(_medium, 0, "Cardputer", _dir + "/ui", kEpoch, &_ui, format));
    bool fresh = !_uiNode->fs().exists("/identity/_main.id");
    _uiNode->begin();
    _ui.begin(_uiNode->mesh(), _uiNode->store(), *rtcOf(*_uiNode), _uiNode->fs(), false, fresh);
    _plat.rebootRequested = false;
  }

  void reboot(bool format) {
    bootUiNode(format);
    if (_app) _app->begin(_first);   // the UI starts again at its boot screen
  }

  bool setup(const char* cmd, const char*) override {
    if (!strcmp(cmd, "keep")) { _fresh = false; return true; }   // reuse flash from a previous run
    return false;
  }

  void step(uint32_t now) override {
    sim_setMillis(now);
    if (_plat.rebootRequested) reboot(false);
    if (_uiNode) {
      _uiNode->loop();
      _ui.loop();
    }
    for (auto& b : _bots) {
      b.second.node->loop();
      b.second.task->loop();
    }
    for (auto& r : _rpts) r.second->loop();
    for (auto& r : _rooms) r.second->loop();
    if (_uiNode) _uiNode->select();   // the UI reads its node's globals
  }

  bool command(const char* cmd, const char* arg, char* err, size_t n) override {
    if (!strcmp(cmd, "node")) {
      char name[32] = {}, flag[8] = {}, flag2[8] = {};
      float snr = 8.0f;
      sscanf(arg, "%31s %f %7s %7s", name, &snr, flag, flag2);
      int id = _nextId++;
      Bot b;
      b.task.reset(new BotTask(&board, &_ifaces));
      b.node.reset(new SimNode(_medium, id, name, _dir + "/" + name, kEpoch, b.task.get()));
      b.node->begin();
      b.task->attach(&b.node->mesh(), rtcOf(*b.node));
      b.task->echo = !strcmp(flag, "echo") || !strcmp(flag2, "echo");
      if (!strcmp(flag, "telem") || !strcmp(flag2, "telem")) {   // shares base telemetry: answers pings
        b.node->select();
        b.node->mesh().getNodePrefs()->telemetry_mode_base = TELEM_MODE_ALLOW_ALL;
      }
      _medium.link(0, id, snr);
      _ids[name] = id;
      _bots[name] = std::move(b);
      return true;
    }
    if (!strcmp(cmd, "room")) {
      char name[32] = {}, pw[32] = "password";
      float snr = 8.0f;
      sscanf(arg, "%31s %f %31s", name, &snr, pw);
      int id = _nextId++;
      _rooms[name] = SimRoom::create(_medium, id, name, _dir + "/" + name, kEpoch, pw);
      _medium.link(0, id, snr);
      _ids[name] = id;
      return true;
    }
    if (!strcmp(cmd, "repeater")) {
      char name[32] = {}, pw[32] = "password";
      float snr = 8.0f;
      sscanf(arg, "%31s %f %31s", name, &snr, pw);
      int id = _nextId++;
      _rpts[name] = SimRepeater::create(_medium, id, name, _dir + "/" + name, kEpoch, pw);
      _medium.link(0, id, snr);
      _ids[name] = id;
      return true;
    }
    if (!strcmp(cmd, "link") || !strcmp(cmd, "unlink")) {
      char a[32], b[32];
      float snr = 8.0f;
      if (sscanf(arg, "%31s %31s %f", a, b, &snr) < 2 || !_ids.count(a) || !_ids.count(b)) {
        snprintf(err, n, "unknown node");
        return true;
      }
      if (cmd[0] == 'l') _medium.link(_ids[a], _ids[b], snr);
      else _medium.unlink(_ids[a], _ids[b]);
      return true;
    }
    if (!strcmp(cmd, "as")) return asNode(arg, err, n);
    if (!strcmp(cmd, "reboot")) { reboot(false); return true; }
    if (!strcmp(cmd, "wipe")) {
      memcpy(_keyBeforeWipe, _uiNode->mesh().self_id.pub_key, sizeof _keyBeforeWipe);
      reboot(true);
      return true;
    }
    if (!strcmp(cmd, "sd")) { _plat.card = !strcmp(arg, "on"); return true; }
    if (!strcmp(cmd, "expect")) return expect(arg, err, n);
    if (!strcmp(cmd, "dump")) { dump(); return true; }
    return false;
  }

private:
  static mesh::RTCClock* rtcOf(SimNode& n) { return n.mesh().getRTCClock(); }

  bool asNode(const char* arg, char* err, size_t n) {
    char name[32] = {}, what[16] = {};
    int used = 0;
    sscanf(arg, "%31s %15s %n", name, what, &used);
    const char* rest = arg + used;
    auto rm = _rooms.find(name);
    if (rm != _rooms.end()) {
      if (strcmp(what, "advert")) { snprintf(err, n, "rooms only advert"); return true; }
      rm->second->advert(strstr(rest, "flood") != nullptr);
      if (_uiNode) _uiNode->select();
      return true;
    }
    auto rp = _rpts.find(name);
    if (rp != _rpts.end()) {
      if (strcmp(what, "advert")) { snprintf(err, n, "repeaters only advert"); return true; }
      rp->second->advert(strstr(rest, "flood") != nullptr);
      if (_uiNode) _uiNode->select();
      return true;
    }
    auto it = _bots.find(name);
    if (it == _bots.end()) { snprintf(err, n, "no node '%s'", name); return true; }
    SimNode& node = *it->second.node;
    node.select();
    MyMesh& m = node.mesh();
    if (!strcmp(what, "advert")) {
      m.uiSendAdvert(strstr(rest, "flood") != nullptr);
    } else if (!strcmp(what, "dm")) {
      char to[32] = {};
      int u = 0;
      sscanf(rest, "%31s %n", to, &u);
      ContactInfo* c = m.searchContactsByPrefix(to);
      if (!c) { snprintf(err, n, "%s has no contact '%s' (advertise first)", name, to); return true; }
      uint32_t ack, est;
      m.sendMessage(*c, rtcOf(node)->getCurrentTimeUnique(), 0, rest + u, ack, est);
    } else if (!strcmp(what, "chan")) {
      int slot = 0, u = 0;
      sscanf(rest, "%d %n", &slot, &u);
      ChannelDetails d;
      if (!m.getChannel(slot, d) || !d.name[0]) { snprintf(err, n, "%s has no channel %d", name, slot); return true; }
      const char* text = rest + u;
      m.sendGroupMessage(rtcOf(node)->getCurrentTimeUnique(), d.channel, m.getNodePrefs()->node_name, text,
                         (int)strlen(text));
    } else if (!strcmp(what, "login")) {
      char to[32] = {}, pw[32] = {};
      sscanf(rest, "%31s %31s", to, pw);
      ContactInfo* c = m.searchContactsByPrefix(to);
      if (!c) { snprintf(err, n, "%s has no contact '%s'", name, to); return true; }
      uint32_t est;
      m.sendLogin(*c, pw, est);
    } else if (!strcmp(what, "join")) {
      ChannelDetails d;
      memset(&d, 0, sizeof d);
      snprintf(d.name, sizeof d.name, "%s", rest);
      mesh::Utils::sha256(d.channel.secret, 16, (const uint8_t*)rest, (int)strlen(rest));
      int slot = 1;
      ChannelDetails e;
      while (m.getChannel(slot, e) && e.name[0]) slot++;
      m.setChannel(slot, d);
      m.uiSaveChannels();
    } else {
      snprintf(err, n, "unknown action '%s'", what);
    }
    if (_uiNode) _uiNode->select();
    return true;
  }

  bool expect(const char* arg, char* err, size_t n) {
    char what[16] = {}, val[16] = {};
    sscanf(arg, "%15s %15s", what, val);
    long want = atol(val);
    long got = -1;
    if (!strcmp(what, "contacts")) got = _ui.contactCount();
    else if (!strcmp(what, "unread")) got = _ui.unreadTotal();
    else if (!strcmp(what, "heard")) got = _ui.heardCount();
    else if (!strcmp(what, "threads")) got = _ui.threadCount();
    else if (!strcmp(what, "channels")) got = _ui.channelCount();
    else if (!strcmp(what, "sounds")) got = g_sound.played;
    else if (!strcmp(what, "request")) {
      static const char* const st[] = {"idle", "busy", "ok", "failed", "timeout"};
      const char* g = st[(int)_ui.requestState()];
      if (strcmp(g, val)) snprintf(err, n, "request is %s (%s)", g, _ui.requestText());
      return true;
    }
    else if (!strcmp(what, "window")) got = _ui.store().count();   // messages of the open chat in RAM
    else if (!strcmp(what, "first") || !strcmp(what, "last")) {      // text in the window's oldest/newest
      app::MessageStore& s = _ui.store();
      const char* g = s.count() ? s.at(what[0] == 'f' ? 0 : s.count() - 1).text : "(none)";
      if (!strstr(g, val)) snprintf(err, n, "%s message is '%s'", what, g);
      return true;
    }
    else if (!strcmp(what, "repeats")) {   // last message we sent in the open chat
      app::MessageStore& s = _ui.store();
      got = -1;
      for (int i = s.count() - 1; i >= 0; i--)
        if (s.at(i).outgoing) { got = s.at(i).repeats; break; }
    }
    else if (!strcmp(what, "screen")) {
      bool on = _app && !_app->asleep();
      if (on != !strcmp(val, "on")) snprintf(err, n, "screen is %s", on ? "on" : "off");
      return true;
    }
    else if (!strcmp(what, "identity")) {
      bool same = !memcmp(_keyBeforeWipe, _uiNode->mesh().self_id.pub_key, sizeof _keyBeforeWipe);
      if (same != !strcmp(val, "same")) snprintf(err, n, "identity is %s", same ? "the same" : "new");
      return true;
    }
    else if (!strcmp(what, "status")) {
      static const char* const names[] = {"none", "pending", "sent", "delivered", "failed"};
      app::MessageStore& s = _ui.store();
      const char* g = "no outgoing message";
      for (int i = s.count() - 1; i >= 0; i--)
        if (s.at(i).outgoing) { g = names[(int)s.at(i).status]; break; }
      if (strcmp(g, val)) snprintf(err, n, "expected status %s, got %s", val, g);
      return true;
    } else {
      snprintf(err, n, "unknown expectation '%s'", what);
      return true;
    }
    if (got != want) snprintf(err, n, "expected %s %ld, got %ld", what, want, got);
    return true;
  }

  void dump() {
    printf("contacts %d, channels %d, threads %d, unread %u, heard %d\n", _ui.contactCount(), _ui.channelCount(),
           _ui.threadCount(), _ui.unreadTotal(), _ui.heardCount());
    for (int i = 0; i < _ui.threadCount(); i++) {
      model::Thread t;
      _ui.thread(i, t);
      printf("  thread %-16s unread %u  %s\n", t.title, t.unread, t.preview);
    }
    model::Diag d = _ui.diag();
    printf("  rx %u tx %u flood %u/%u direct %u/%u dups %u air %lums\n", d.rxPackets, d.txPackets, d.rxFlood,
           d.txFlood, d.rxDirect, d.txDirect, d.dups, (unsigned long)d.airTxMs);
  }

  std::string _dir;
  app::MeshAdapter& _ui;
  MultiSerialInterface& _ifaces;
  SimMedium _medium;
  std::unique_ptr<SimNode> _uiNode;
  std::map<std::string, Bot> _bots;
  std::map<std::string, std::unique_ptr<SimRepeater>> _rpts;
  std::map<std::string, std::unique_ptr<SimRoom>> _rooms;
  std::map<std::string, int> _ids;
  bool _fresh = true;
  SimPlatform& _plat;
  ui::App* _app = nullptr;
  ui::Screen* _first = nullptr;
  uint8_t _keyBeforeWipe[PUB_KEY_SIZE] = {};
  int _nextId = 1;   // 0 is the UI node
};

uint32_t g_now = 0;

}  // namespace

int main(int argc, char** argv) {
  const char* script = nullptr;
  const char* out = ".";
  std::string data = "sim-data/simnet";
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--smoke")) return simnetSmoke();
    if (!strcmp(argv[i], "--remote-smoke")) return simnetRemoteSmoke();
    if (!strcmp(argv[i], "-s") && i + 1 < argc) script = argv[++i];
    else if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
    else if (!strcmp(argv[i], "--data") && i + 1 < argc) data = argv[++i];
  }
  FILE* in = script ? fopen(script, "r") : stdin;
  if (!in) { perror(script); return 1; }

  static MultiSerialInterface ifaces;
  static SimPlatform platform;
  static app::MeshAdapter adapter(&board, &ifaces, platform);
  static sim::SimDisplay display;
  static sim::SimKeyboard kb(&g_now);
  static ui::App ui(adapter, display, kb, &g_sound);
  static NetWorld world(data, adapter, ifaces, platform);
  return sim::runScript(in, out, ui, ui::screens::boot(), kb, display, g_now, world);
}
#endif
