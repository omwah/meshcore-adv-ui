// The UI's MeshModel on top of stock MeshCore's companion MyMesh.
//
// It is also MyMesh's UI task: MeshCore calls the event hooks (patch 0001)
// as messages, ACKs and responses arrive, and the adapter keeps what the UI
// needs that MeshCore does not: message history (MessageStore), delivery
// tracking with retries, signal history per contact, heard adverts, a
// packet log, and the UI's own preferences.
#pragma once
#include <stdint.h>
#include "MyMesh.h"
#include "model/model.h"
#include "app/msg_store.h"
#include "app/platform.h"
#include "app/backup.h"

namespace app {

// Settings that belong to the UI rather than to MeshCore's NodePrefs.
struct UiPrefs {
  uint32_t magic;
  uint8_t version;
  uint8_t theme, timeZone, brightness;
  uint16_t screenTimeoutS, inputLockoutMs;
  uint8_t sound, textSize, gpsTimeSyncAtBoot, autoBackup, backupIdentity, backupMessages, bleEnabled;
  uint8_t presetIdx;
  uint16_t mutedChannels;        // bit per channel slot
  uint8_t favourites[16][4];     // public key prefixes (MeshCore has no favourite flag)
  uint8_t nFavourites;
  // Fields added later go here: older, shorter files load them as 0.
  uint8_t blePinRandom;
};

class MeshAdapter : public AbstractUITask, public model::MeshModel {
public:
  MeshAdapter(mesh::MainBoard* board, MultiSerialInterface* ifaces, Platform& platform)
      : AbstractUITask(board, ifaces), _plat(platform) {}

  // Call after MyMesh::begin(): `fs` is the flash filesystem MeshCore uses.
  // `freshInstall`: there was no identity on flash before MyMesh::begin().
  void begin(MyMesh& mesh, DataStore& store, mesh::RTCClock& rtc, fs::FS& fs, bool bleBuilt, bool freshInstall);

  // ---- AbstractUITask (called by MyMesh) ----
  void msgRead(int) override {}
  void newMsg(uint8_t, const char*, const char*, int) override {}
  void notify(UIEventType) override { _rev++; }
  void loop() override;   // retries, request timeouts
  void onDirectMessage(const ContactInfo& from, uint8_t txt_type, uint8_t path_len, float snr,
                       uint32_t sender_ts, const uint8_t* extra, int extra_len, const char* text) override;
  void onChannelMessage(uint8_t channel_idx, uint8_t path_len, float snr, uint32_t ts, const char* text) override;
  void onAckReceived(const uint8_t* ack) override;
  void onContactResponse(const ContactInfo& from, const uint8_t* data, uint8_t len) override;
  void onTraceData(uint32_t tag, uint8_t flags, const uint8_t* hashes, const uint8_t* snrs, uint8_t path_len,
                   float final_snr) override;
  void onRawRx(float snr, float rssi, const uint8_t* raw, int len) override;
  void onControlData(float snr, float rssi, uint8_t path_len, const uint8_t* payload, int len) override;

  // ---- model::MeshModel ----
  uint32_t revision() const override { return _rev; }
  uint32_t now() const override;
  uint32_t uptimeMs() const override;
  void selfKey(uint8_t out[6]) const override;

  int contactCount() const override;
  bool contact(int i, model::Contact& out) const override;
  void setFavourite(int i, bool fav) override;
  bool removeContact(int i) override;
  bool resetPath(int i) override;

  int channelCount() const override;
  bool channel(int i, model::Channel& out) const override;
  bool joinHashtag(const char* name) override;
  bool joinPrivate(const char* name, const uint8_t psk[16]) override;
  bool createPrivate(const char* name) override;
  bool channelKey(int i, uint8_t psk[16]) const override;
  bool removeChannel(int i) override;
  void setMuted(int i, bool muted) override;

  int threadCount() const override;
  bool thread(int i, model::Thread& out) const override;
  int messageCount(model::ThreadId t) const override;
  bool message(model::ThreadId t, int i, model::Message& out) const override;
  void markRead(model::ThreadId t) override;
  uint16_t unreadTotal() const override;
  bool send(model::ThreadId t, const char* text) override;

  bool advert(bool flood) override;
  int heardCount() const override { return _nHeard; }
  bool heard(int i, model::Contact& out) const override;
  bool addHeard(int i) override;
  bool discoverRepeaters() override;
  model::Req discoverState() const override { return _discoverState; }

  bool login(int contact, const char* password) override;
  bool requestStats(int contact) override;
  bool requestNeighbours(int contact) override;
  bool requestTelemetry(int contact) override;
  bool sendCommand(int contact, const char* cmd) override;
  bool tracePath(int contact) override;
  bool ping(int contact) override;
  model::Req requestState() const override { return _req.state; }
  const char* requestText() const override { return _req.text; }
  const model::RemoteStats& remoteStats() const override { return _remote; }
  int neighbourCount() const override { return _nNeighbours; }
  bool neighbour(int i, model::Neighbour& out) const override;
  uint32_t lastRttMs() const override { return _req.rttMs; }

  model::Radio radio() const override;
  bool setRadio(const model::Radio& r) override;
  int presetCount() const override;
  const model::Preset& preset(int i) const override;

  model::Diag diag() const override;
  int rxLogCount() const override { return _nLog; }
  bool rxLog(int i, model::RxLogEntry& out) const override;
  model::Gps gps() const override { return _plat.gps(); }
  bool gpsSyncTime() override;

  const model::Prefs& prefs() const override { return _prefs; }
  void setPrefs(const model::Prefs& p) override;
  bool bleConnected() const override { return hasConnection(); }
  bool bleAvailable() const override { return _bleBuilt; }
  uint32_t bleActivePin() const override { return _activePin; }
  bool blePairing() const override { return _bleBuilt && _plat.blePairing(); }
  // The PIN for this boot (fixed, or new random if so set); call once,
  // before starting the Bluetooth interface.
  uint32_t choosePinForBoot();

  model::BackupStatus backupStatus() const override;
  bool restoreOffered() const override { return _fresh && _bk.cur.valid; }
  bool backupNow() override;
  bool restore(bool previous) override;
  void factoryReset() override;
  void reboot() override { _plat.reboot(); }

  MessageStore& store() { return _msgs; }

private:
  struct Pending {                 // outgoing DM waiting for its ACK
    uint32_t seq, ts;                // ts: retries resend with the original timestamp
    uint32_t ack, prevAck;           // an ACK for the previous attempt still counts
    uint32_t sentAt, timeout;
    uint8_t key[6];
    uint8_t attempt;
    bool used;
  };
  struct Signal {                  // last direct reception per contact
    uint8_t key[4];
    int8_t snr_x4;
    int16_t rssi;
    uint32_t at;
  };
  struct Heard {
    uint8_t pubkey[PUB_KEY_SIZE];
    char name[model::NAME_LEN];
    uint8_t type;
    int8_t hops, snr_x4;
    int16_t rssi;
    uint32_t at;
    bool hasPos;
    int32_t lat, lon;
  };
  enum class ReqKind : uint8_t { None, Login, Status, Telemetry, Neighbours, Cli, Trace, Ping };
  struct Request {
    ReqKind kind = ReqKind::None;
    model::Req state = model::Req::Idle;
    uint8_t key[PUB_KEY_SIZE];
    uint32_t tag = 0, startedAt = 0, deadline = 0, rttMs = 0;
    char text[200] = {};
  };

  // MeshCore reserves the first MAX_ANON_CONTACTS slots; UI index 0 is the
  // first real contact.
  bool getContact(int i, ContactInfo& c) const { return i >= 0 && _mesh->getContactByIdx(MAX_ANON_CONTACTS + i, c); }
  ContactInfo* contactPtr(int i) const;
  int contactIndex(const uint8_t* pubkey) const;
  bool startRequest(ReqKind k, int contact, uint32_t estTimeout);
  void finishRequest(model::Req st, const char* fmt, ...);
  void noteSignal(const uint8_t* key, float snr, float rssi);
  const Signal* signalFor(const uint8_t* key) const;
  bool isFavourite(const uint8_t* key) const;
  void heardAdvert(const mesh::Packet& pkt, float snr, float rssi);
  void putHeard(const Heard& h);
  void decodeTelemetry(const uint8_t* lpp, int len);
  bool sendDirect(Pending& p, const ContactInfo& c, const char* text);
  void loadUiPrefs();
  void saveUiPrefs();
  void syncPrefsFromMesh();
  void configChanged();          // something a backup holds changed
  void refreshBackupInfo();

  Platform& _plat;
  MyMesh* _mesh = nullptr;
  DataStore* _store = nullptr;
  mesh::RTCClock* _rtc = nullptr;
  fs::FS* _fs = nullptr;
  bool _bleBuilt = false;
  uint32_t _rev = 1;
  mutable MessageStore _msgs;
  mutable ThreadKey _openKey{};
  mutable bool _hasOpen = false;
  UiPrefs _ui{};
  model::Prefs _prefs{};

  static constexpr int MAX_PENDING = 6, MAX_SIGNALS = 48, MAX_HEARD = 24, MAX_LOG = 40, MAX_NEIGH = 16;
  Pending _pending[MAX_PENDING] = {};
  Signal _signals[MAX_SIGNALS] = {};
  Heard _heard[MAX_HEARD];
  int _nHeard = 0;
  model::RxLogEntry _log[MAX_LOG];
  uint8_t _logHash[MAX_LOG][4];
  int _logHead = 0, _nLog = 0;
  Request _req;
  model::RemoteStats _remote{};
  model::Neighbour _neigh[MAX_NEIGH];
  int _nNeighbours = 0;
  model::Req _discoverState = model::Req::Idle;
  uint32_t _discoverTag = 0, _discoverUntil = 0;
  int16_t _lastRssi = 0;
  int8_t _lastSnr = 0;
  uint32_t _dupsSeen = 0;

  // Backup state, cached so screens never trigger SD reads.
  struct {
    bool card;
    Backup::Info cur, prev;
    char error[32];
  } _bk{};
  bool _fresh = false;
  uint32_t _cfgChangedAt = 0;      // 0: nothing waiting for an automatic backup
  uint32_t _lastBackupAt = 0;
  int _seenContacts = -1;
  uint32_t _activePin = 0;
  uint32_t _prefsCheckAt = 0;
  uint32_t _bootGpsUntil = 0;     // boot-time GPS clock sync in progress until
};

}  // namespace app
