// The UI's only view of the mesh. Implemented by the MeshCore adapter on
// devices (src/app) and by a scripted mock in the simulator. No Arduino or
// MeshCore types cross this boundary, so the UI builds and tests on a host.
//
// Lists are read by index into caller-provided structs: nothing here
// allocates, and the UI holds no copies beyond what is on screen.
#pragma once
#include <stdint.h>
#include <stddef.h>

namespace model {

constexpr int NAME_LEN = 32;
constexpr int TEXT_LEN = 160;     // MeshCore's max text payload, plus NUL

enum class NodeType : uint8_t { None = 0, Chat = 1, Repeater = 2, Room = 3, Sensor = 4 };

struct Contact {
  char name[NAME_LEN];
  uint8_t key[6];                 // public key prefix (display only)
  NodeType type;
  int8_t hops;                    // -1: no known path (flood)
  bool favourite;
  bool hasPos;
  int32_t lat_e6, lon_e6;
  uint32_t lastHeard;             // epoch seconds of last advert/packet
  int8_t snr_x4;                  // last SNR heard directly, dB*4 (INT8_MIN: unknown)
  int16_t rssi;                   // last RSSI heard directly, dBm (0: unknown)
  uint8_t pathLen;                // bytes of stored path
  uint8_t path[16];               // first hops of the stored path (hash bytes)
};

struct Channel {
  uint8_t idx;
  char name[NAME_LEN];
  bool hashtag;                   // "#name" (key derived from name) vs private PSK
  bool muted;
};

enum class ThreadKind : uint8_t { Direct, Channel };

struct ThreadId {
  ThreadKind kind;
  uint16_t idx;                   // contact index, or channel index
  bool operator==(const ThreadId& o) const { return kind == o.kind && idx == o.idx; }
};

struct Thread {
  ThreadId id;
  char title[NAME_LEN];
  char preview[48];
  NodeType type;                  // for DMs: the contact's type
  uint32_t lastTs;
  uint16_t unread;
  bool favourite;
  bool muted;
};

enum class Delivery : uint8_t { None, Pending, Sent, Delivered, Failed };

struct Message {
  uint32_t ts;                    // epoch seconds (sender's time for incoming)
  bool outgoing;
  Delivery status;                // outgoing only
  uint8_t retries;
  int8_t snr_x4;                  // incoming only, INT8_MIN unknown
  int8_t hops;                    // incoming: path length, -1 direct-unknown
  uint8_t repeats;                // outgoing channel msgs: times heard repeated
  char sender[NAME_LEN];          // channel messages: who sent it
  char text[TEXT_LEN];
};

struct Radio {
  float freqMHz;
  float bwKHz;
  uint8_t sf, cr;
  int8_t txDbm;
  int8_t txMaxDbm;
  bool repeat;                    // client repeat
  bool rxBoost;
};

struct Preset {
  const char* name;
  float freqMHz, bwKHz;
  uint8_t sf, cr;
  uint8_t pathHashSize;           // bytes per path hop to use, 0 = leave as is
};

struct Diag {
  uint32_t uptimeS;
  int16_t noiseFloor;             // dBm
  int16_t lastRssi;               // dBm
  int8_t lastSnr_x4;
  uint32_t rxPackets, txPackets, rxFlood, rxDirect, txFlood, txDirect, dups, rxErrors;
  uint32_t airTxMs, airRxMs;      // cumulative airtime
  uint8_t dutyPct;                // tx airtime over the last hour
  uint16_t queueLen;              // outbound queue
  uint16_t offlineQueue;          // frames waiting for the phone app
  uint32_t heapFree, heapMin, heapMaxBlock;
  uint16_t battMv;
  uint8_t battPct;
  bool charging;
  uint16_t contacts, contactsMax;
  uint8_t channels, channelsMax;
};

struct RxLogEntry {
  uint32_t ms;                    // device uptime ms
  int8_t snr_x4;
  int16_t rssi;
  uint8_t payloadType;            // MeshCore PAYLOAD_TYPE_*
  uint8_t routeType;              // ROUTE_TYPE_*
  uint8_t pathLen;
  uint8_t len;
  uint8_t srcHash;                // first byte of sender hash when known
  bool dup;
};

struct Gps {
  bool enabled, fix;
  uint8_t sats;
  int32_t lat_e6, lon_e6;
  int32_t altCm;
  uint32_t lastFixS;              // epoch of last fix
  uint8_t hdop_x10;
};

struct Prefs {
  char nodeName[NAME_LEN];
  uint8_t theme;
  uint8_t timeZone;               // index into ui::kZones
  uint8_t brightness;             // 0..255
  uint16_t screenTimeoutS;        // 0 = never
  uint16_t inputLockoutMs;
  bool sound;
  bool largeText;
  bool gpsEnabled;
  bool gpsShareInAdvert;
  bool gpsTimeSyncAtBoot;
  bool autoAddContacts;
  bool autoBackup;
  bool backupIdentity;
  bool backupMessages;
  bool bleEnabled;
  uint32_t blePin;
  uint8_t presetIdx;              // index into presets(), 0xFF custom
};

struct BackupStatus {
  bool cardPresent;
  bool hasBackup, hasPrevious;
  uint32_t lastBackupTs;          // epoch, 0 never
  char lastError[32];
};

// Results of a remote (repeater / room) request.
struct RemoteStats {
  bool valid;
  uint16_t battMv;
  uint32_t uptimeS;
  int16_t noiseFloor, lastRssi;
  int8_t lastSnr_x4;
  uint32_t rxPackets, txPackets, airTxS, airRxS;
  uint16_t queueLen;
  uint32_t floodsSent, directSent, dupsFlood, dupsDirect;
};

struct Neighbour {
  uint8_t key[4];
  char name[NAME_LEN];            // resolved from contacts, or empty
  int8_t snr_x4;
  uint32_t heardAgoS;
};

enum class Req : uint8_t { Idle, Busy, Ok, Failed, Timeout };

class MeshModel {
public:
  virtual ~MeshModel() {}

  // Bumped whenever anything the UI shows may have changed.
  virtual uint32_t revision() const = 0;
  virtual uint32_t now() const = 0;               // epoch seconds (RTC)
  virtual uint32_t uptimeMs() const = 0;

  // Identity.
  virtual void selfKey(uint8_t out[6]) const = 0;

  // Contacts.
  virtual int contactCount() const = 0;
  virtual bool contact(int i, Contact& out) const = 0;
  virtual void setFavourite(int i, bool fav) = 0;
  virtual bool removeContact(int i) = 0;
  virtual bool resetPath(int i) = 0;

  // Channels.
  virtual int channelCount() const = 0;
  virtual bool channel(int i, Channel& out) const = 0;
  virtual bool joinHashtag(const char* name) = 0;        // "#foo"
  virtual bool joinPrivate(const char* name, const uint8_t psk[16]) = 0;
  virtual bool createPrivate(const char* name) = 0;      // random PSK
  virtual bool channelKey(int i, uint8_t psk[16]) const = 0;
  virtual bool removeChannel(int i) = 0;
  virtual void setMuted(int i, bool muted) = 0;

  // Conversations, most recent first.
  virtual int threadCount() const = 0;
  virtual bool thread(int i, Thread& out) const = 0;
  virtual int messageCount(ThreadId t) const = 0;
  virtual bool message(ThreadId t, int i, Message& out) const = 0;  // 0 = oldest kept
  virtual void markRead(ThreadId t) = 0;
  virtual uint16_t unreadTotal() const = 0;
  virtual bool send(ThreadId t, const char* text) = 0;

  // Adverts and discovery.
  virtual bool advert(bool flood) = 0;
  virtual int heardCount() const = 0;                     // recently heard adverts
  virtual bool heard(int i, Contact& out) const = 0;
  virtual bool addHeard(int i) = 0;
  virtual bool discoverRepeaters() = 0;                   // zero-hop
  virtual Req discoverState() const = 0;

  // Repeater / room admin, one request in flight at a time.
  virtual bool login(int contact, const char* password) = 0;
  virtual bool requestStats(int contact) = 0;
  virtual bool requestNeighbours(int contact) = 0;
  virtual bool requestTelemetry(int contact) = 0;
  virtual bool sendCommand(int contact, const char* cmd) = 0;   // CLI text
  virtual bool tracePath(int contact) = 0;
  virtual bool ping(int contact) = 0;
  virtual Req requestState() const = 0;
  virtual const char* requestText() const = 0;            // last reply / error, for display
  virtual const RemoteStats& remoteStats() const = 0;
  virtual int neighbourCount() const = 0;
  virtual bool neighbour(int i, Neighbour& out) const = 0;
  virtual uint32_t lastRttMs() const = 0;

  // Radio.
  virtual Radio radio() const = 0;
  virtual bool setRadio(const Radio& r) = 0;
  virtual int presetCount() const = 0;
  virtual const Preset& preset(int i) const = 0;

  // Diagnostics.
  virtual Diag diag() const = 0;
  virtual int rxLogCount() const = 0;
  virtual bool rxLog(int i, RxLogEntry& out) const = 0;  // 0 = newest
  virtual Gps gps() const = 0;
  virtual bool gpsSyncTime() = 0;

  // Settings.
  virtual const Prefs& prefs() const = 0;
  virtual void setPrefs(const Prefs& p) = 0;
  virtual bool bleConnected() const = 0;
  virtual bool bleAvailable() const = 0;                  // compiled in

  // Backup.
  virtual BackupStatus backupStatus() const = 0;
  // True on a fresh install (no identity yet) when the SD card holds a backup.
  virtual bool restoreOffered() const = 0;
  virtual bool backupNow() = 0;
  virtual bool restore(bool previous) = 0;                // reboots on success
  virtual void factoryReset() = 0;
  virtual void reboot() = 0;
};

}  // namespace model
