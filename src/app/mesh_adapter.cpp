#include "app/mesh_adapter.h"
#include "app/radio_presets.h"
#include <helpers/AdvertDataHelpers.h>
#include <helpers/TxtDataHelpers.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#ifndef UI_DEFAULT_TEXT_SIZE
#define UI_DEFAULT_TEXT_SIZE 1   // medium
#endif

namespace app {

namespace {

constexpr uint32_t UI_PREFS_MAGIC = 0x4955434D;   // "MCUI"
constexpr uint8_t UI_PREFS_VERSION = 2;   // 2: text size levels
const char* const kUiPrefsFile = "/ui/prefs.bin";

constexpr uint8_t REQ_GET_STATUS = 0x01;
constexpr uint8_t REQ_GET_TELEMETRY = 0x03;
constexpr uint8_t REQ_GET_NEIGHBOURS = 0x06;
constexpr uint8_t CTL_DISCOVER_REQ = 0x80;
constexpr uint8_t CTL_DISCOVER_RESP = 0x90;
constexpr uint32_t DISCOVER_LISTEN_MS = 8000;
constexpr int MAX_DM_ATTEMPTS = 3;          // then one last flood if a path was in use

void hex(char* out, size_t n, const uint8_t* b, int len) {
  size_t o = 0;
  for (int i = 0; i < len && o + 2 < n; i++) o += snprintf(out + o, n - o, "%02X", b[i]);
}

int8_t hopsOf(uint8_t path_len) {
  if (path_len == 0xFF) return -1;           // came by a known direct route
  return (int8_t)(path_len & 63);
}

uint32_t rd32(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }
uint16_t rd16(const uint8_t* p) { uint16_t v; memcpy(&v, p, 2); return v; }

}  // namespace

uint8_t batteryPercent(uint16_t mv) {
  // Resting Li-ion curve, mV at 0,10,..,100 %.
  static const uint16_t k[] = {3300, 3600, 3700, 3750, 3790, 3830, 3870, 3920, 3980, 4060, 4150};
  if (mv <= k[0]) return 0;
  for (int i = 1; i < 11; i++)
    if (mv < k[i]) return (uint8_t)((i - 1) * 10 + (mv - k[i - 1]) * 10 / (k[i] - k[i - 1]));
  return 100;
}

// ------------------------------------------------------------------ setup

void MeshAdapter::begin(MyMesh& mesh, DataStore& store, mesh::RTCClock& rtc, fs::FS& fs, bool bleBuilt, bool freshInstall) {
  _mesh = &mesh;
  _store = &store;
  _rtc = &rtc;
  _fs = &fs;
  _bleBuilt = bleBuilt;
  _msgs.begin(&fs);
  loadUiPrefs();
  syncPrefsFromMesh();
  _fresh = freshInstall;
  _seenContacts = _mesh->getNumContacts();
  // "Set clock from GPS at boot": run the receiver until the first fix (at
  // most 10 minutes) even if GPS is otherwise off.
  if (_ui.gpsTimeSyncAtBoot) {
    _bootGpsUntil = millis() + 600000UL;
    if (!_prefs.gpsEnabled) _plat.setGpsPower(true);
  }
  refreshBackupInfo();
  _rev++;
}

void MeshAdapter::loadUiPrefs() {
  memset(&_ui, 0, sizeof _ui);
  File f = _fs->open(kUiPrefsFile, "r");
  // A file from an older build is shorter: the fields it lacks stay 0.
  size_t got = f ? f.read((uint8_t*)&_ui, sizeof _ui) : 0;
  bool ok = got >= offsetof(UiPrefs, blePinRandom) && _ui.magic == UI_PREFS_MAGIC;
  if (f) f.close();
  if (!ok || _ui.version != UI_PREFS_VERSION) {
    memset(&_ui, 0, sizeof _ui);
    _ui.magic = UI_PREFS_MAGIC;
    _ui.version = UI_PREFS_VERSION;
    _ui.brightness = 192;
    _ui.screenTimeoutS = 60;
    _ui.inputLockoutMs = 60;
    _ui.sound = 1;
    _ui.textSize = UI_DEFAULT_TEXT_SIZE;
    _ui.autoBackup = 1;
    _ui.backupIdentity = 1;
    _ui.bleEnabled = 1;
    _ui.presetIdx = 0;
  }
}

void MeshAdapter::saveUiPrefs() {
  File f = _fs->open(kUiPrefsFile, "w", true);
  if (!f) return;
  f.write((const uint8_t*)&_ui, sizeof _ui);
  f.close();
}

void MeshAdapter::syncPrefsFromMesh() {
  NodePrefs* np = _mesh->getNodePrefs();
  model::Prefs& p = _prefs;
  snprintf(p.nodeName, sizeof p.nodeName, "%s", np->node_name);
  p.theme = _ui.theme;
  p.timeZone = _ui.timeZone;
  p.brightness = _ui.brightness;
  p.screenTimeoutS = _ui.screenTimeoutS;
  p.inputLockoutMs = _ui.inputLockoutMs;
  p.sound = _ui.sound;
  p.textSize = _ui.textSize;
  p.gpsEnabled = np->gps_enabled;
  p.gpsShareInAdvert = np->advert_loc_policy != ADVERT_LOC_NONE;
  p.gpsTimeSyncAtBoot = _ui.gpsTimeSyncAtBoot;
  p.autoAddContacts = !np->manual_add_contacts;
  p.autoBackup = _ui.autoBackup;
  p.backupIdentity = _ui.backupIdentity;
  p.backupMessages = _ui.backupMessages;
  p.bleEnabled = _ui.bleEnabled;
  p.blePin = np->ble_pin ? np->ble_pin : _mesh->getBLEPin();
  p.presetIdx = _ui.presetIdx;
  p.blePinRandom = _ui.blePinRandom;
}

void MeshAdapter::setPrefs(const model::Prefs& p) {
  NodePrefs* np = _mesh->getNodePrefs();
  bool advertNeeded = strcmp(np->node_name, p.nodeName) != 0;
  snprintf(np->node_name, sizeof np->node_name, "%s", p.nodeName);
  np->gps_enabled = p.gpsEnabled;
  np->advert_loc_policy = p.gpsShareInAdvert ? ADVERT_LOC_SHARE : ADVERT_LOC_NONE;
  np->manual_add_contacts = !p.autoAddContacts;
  np->ble_pin = p.blePin;
  if (p.presetIdx != _ui.presetIdx && p.presetIdx < kRadioPresetCount && kRadioPresets[p.presetIdx].pathHashSize)
    np->path_hash_mode = kRadioPresets[p.presetIdx].pathHashSize - 1;
  _mesh->savePrefs();

  if (_bleBuilt && (bool)_ui.bleEnabled != p.bleEnabled) {
    if (p.bleEnabled) enableBluetooth();
    else disableBluetooth();
  }
  if ((bool)_prefs.gpsEnabled != p.gpsEnabled) _plat.setGpsPower(p.gpsEnabled);

  _ui.theme = p.theme;
  _ui.timeZone = p.timeZone;
  _ui.brightness = p.brightness;
  _ui.screenTimeoutS = p.screenTimeoutS;
  _ui.inputLockoutMs = p.inputLockoutMs;
  _ui.sound = p.sound;
  _ui.textSize = p.textSize > 2 ? 2 : p.textSize;
  _ui.gpsTimeSyncAtBoot = p.gpsTimeSyncAtBoot;
  _ui.autoBackup = p.autoBackup;
  _ui.backupIdentity = p.backupIdentity;
  _ui.backupMessages = p.backupMessages;
  _ui.bleEnabled = p.bleEnabled;
  _ui.presetIdx = p.presetIdx;
  _ui.blePinRandom = p.blePinRandom;
  saveUiPrefs();
  syncPrefsFromMesh();
  (void)advertNeeded;   // the UI tells the user to advertise; we don't spend airtime unasked
  configChanged();
  _rev++;
}

uint32_t MeshAdapter::choosePinForBoot() {
  if (_ui.blePinRandom) {
    uint32_t r;
    _mesh->getRNG()->random((uint8_t*)&r, sizeof r);
    _activePin = 100000 + r % 900000;
  } else {
    _activePin = _prefs.blePin ? _prefs.blePin : 123456;
  }
  return _activePin;
}

// ------------------------------------------------------------------ time

uint32_t MeshAdapter::now() const { return _rtc->getCurrentTime(); }
uint32_t MeshAdapter::uptimeMs() const { return millis(); }

void MeshAdapter::selfKey(uint8_t out[6]) const { memcpy(out, _mesh->self_id.pub_key, 6); }

// ------------------------------------------------------------------ contacts

ContactInfo* MeshAdapter::contactPtr(int i) const {
  ContactInfo c;
  if (i < 0 || !getContact(i, c)) return nullptr;
  return _mesh->lookupContactByPubKey(c.id.pub_key, PUB_KEY_SIZE);
}

int MeshAdapter::contactIndex(const uint8_t* key) const {
  ContactInfo c;
  for (int i = 0, n = _mesh->getNumContacts(); i < n; i++)
    if (getContact(i, c) && !memcmp(c.id.pub_key, key, 6)) return i;
  return -1;
}

int MeshAdapter::contactCount() const { return _mesh->getNumContacts(); }

bool MeshAdapter::isFavourite(const uint8_t* key) const {
  for (int i = 0; i < _ui.nFavourites; i++)
    if (!memcmp(_ui.favourites[i], key, 4)) return true;
  return false;
}

bool MeshAdapter::contact(int i, model::Contact& out) const {
  ContactInfo c;
  if (i < 0 || !getContact(i, c)) return false;
  memset(&out, 0, sizeof out);
  snprintf(out.name, sizeof out.name, "%s", c.name);
  memcpy(out.key, c.id.pub_key, 6);
  out.type = c.type <= ADV_TYPE_SENSOR ? (model::NodeType)c.type : model::NodeType::None;
  if (c.out_path_len == OUT_PATH_UNKNOWN) {
    out.hops = -1;
  } else {
    out.hops = (int8_t)(c.out_path_len & 63);
    int bytes = out.hops * ((c.out_path_len >> 6) + 1);
    out.pathLen = (uint8_t)(bytes < 16 ? bytes : 16);
    memcpy(out.path, c.out_path, out.pathLen);
  }
  out.favourite = isFavourite(c.id.pub_key);
  out.hasPos = c.gps_lat || c.gps_lon;
  out.lat_e6 = c.gps_lat;
  out.lon_e6 = c.gps_lon;
  out.lastHeard = c.lastmod;
  out.snr_x4 = INT8_MIN;
  if (const Signal* s = signalFor(c.id.pub_key)) {
    out.snr_x4 = s->snr_x4;
    out.rssi = s->rssi;
    if (s->at > out.lastHeard) out.lastHeard = s->at;
  }
  return true;
}

void MeshAdapter::setFavourite(int i, bool fav) {
  ContactInfo c;
  if (!getContact(i, c)) return;
  bool is = isFavourite(c.id.pub_key);
  if (fav == is) return;
  if (fav) {
    if (_ui.nFavourites == 16) return;
    memcpy(_ui.favourites[_ui.nFavourites++], c.id.pub_key, 4);
  } else {
    for (int k = 0; k < _ui.nFavourites; k++)
      if (!memcmp(_ui.favourites[k], c.id.pub_key, 4)) {
        memmove(_ui.favourites[k], _ui.favourites[k + 1], 4 * (_ui.nFavourites - k - 1));
        _ui.nFavourites--;
        break;
      }
  }
  saveUiPrefs();
  configChanged();
  _rev++;
}

bool MeshAdapter::removeContact(int i) {
  ContactInfo* c = contactPtr(i);
  if (!c) return false;
  uint8_t key[PUB_KEY_SIZE];
  memcpy(key, c->id.pub_key, PUB_KEY_SIZE);
  if (!_mesh->removeContact(*c)) return false;
  _mesh->uiDeleteContactBlob(key);
  _mesh->uiMarkContactsDirty();
  _msgs.forget(ThreadKey::direct(key));
  configChanged();
  _rev++;
  return true;
}

bool MeshAdapter::resetPath(int i) {
  ContactInfo* c = contactPtr(i);
  if (!c) return false;
  c->out_path_len = OUT_PATH_UNKNOWN;
  _mesh->uiMarkContactsDirty();
  configChanged();
  _rev++;
  return true;
}

void MeshAdapter::noteSignal(const uint8_t* key, float snr, float rssi) {
  Signal* slot = nullptr;
  for (auto& s : _signals)
    if (!memcmp(s.key, key, 4)) { slot = &s; break; }
  if (!slot) {
    slot = &_signals[0];
    for (auto& s : _signals)
      if (s.at < slot->at) slot = &s;
    memcpy(slot->key, key, 4);
  }
  slot->snr_x4 = (int8_t)(snr * 4);
  slot->rssi = (int16_t)rssi;
  slot->at = now();
}

const MeshAdapter::Signal* MeshAdapter::signalFor(const uint8_t* key) const {
  for (auto& s : _signals)
    if (s.at && !memcmp(s.key, key, 4)) return &s;
  return nullptr;
}

// ------------------------------------------------------------------ channels

int MeshAdapter::channelCount() const {
  int n = 0;
  ChannelDetails d;
  for (int i = 0; i < MAX_GROUP_CHANNELS; i++)
    if (_mesh->getChannel(i, d) && d.name[0]) n++;
  return n;
}

// List index -> channel slot (slots can have gaps after a channel is left).
static int slotOf(MyMesh* m, int i) {
  ChannelDetails d;
  for (int s = 0; s < MAX_GROUP_CHANNELS; s++)
    if (m->getChannel(s, d) && d.name[0] && i-- == 0) return s;
  return -1;
}

bool MeshAdapter::channel(int i, model::Channel& out) const {
  int s = slotOf(_mesh, i);
  ChannelDetails d;
  if (s < 0 || !_mesh->getChannel(s, d)) return false;
  memset(&out, 0, sizeof out);
  out.idx = (uint8_t)s;
  snprintf(out.name, sizeof out.name, "%s", d.name);
  out.hashtag = d.name[0] == '#';
  out.muted = (_ui.mutedChannels >> s) & 1;
  return true;
}

static int freeSlot(MyMesh* m) {
  ChannelDetails d;
  for (int s = 0; s < MAX_GROUP_CHANNELS; s++)
    if (m->getChannel(s, d) && !d.name[0]) return s;
  return -1;
}

bool MeshAdapter::joinPrivate(const char* name, const uint8_t psk[16]) {
  int s = freeSlot(_mesh);
  if (s < 0 || !name[0]) return false;
  ChannelDetails d;
  memset(&d, 0, sizeof d);
  snprintf(d.name, sizeof d.name, "%s", name);
  memcpy(d.channel.secret, psk, 16);
  if (!_mesh->setChannel(s, d)) return false;
  _mesh->uiSaveChannels();
  _ui.mutedChannels &= ~(1u << s);
  saveUiPrefs();
  configChanged();
  _rev++;
  return true;
}

bool MeshAdapter::joinHashtag(const char* name) {
  // Hashtag channels derive their key from the name, as the MeshCore apps
  // do: the first 16 bytes of SHA-256 of "#name" (lower case).
  char n[model::NAME_LEN];
  int o = 0;
  if (name[0] != '#') n[o++] = '#';
  for (int i = 0; name[i] && o < (int)sizeof n - 1; i++) {
    char c = name[i];
    n[o++] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
  }
  n[o] = 0;
  uint8_t key[16];
  mesh::Utils::sha256(key, sizeof key, (const uint8_t*)n, o);
  return joinPrivate(n, key);
}

bool MeshAdapter::createPrivate(const char* name) {
  uint8_t key[16];
  _mesh->getRNG()->random(key, sizeof key);
  return joinPrivate(name, key);
}

bool MeshAdapter::channelKey(int i, uint8_t psk[16]) const {
  int s = slotOf(_mesh, i);
  ChannelDetails d;
  if (s < 0 || !_mesh->getChannel(s, d)) return false;
  memcpy(psk, d.channel.secret, 16);
  return true;
}

bool MeshAdapter::removeChannel(int i) {
  int s = slotOf(_mesh, i);
  if (s <= 0) return false;   // slot 0 (Public) stays
  ChannelDetails d;
  memset(&d, 0, sizeof d);
  if (!_mesh->setChannel(s, d)) return false;
  _mesh->uiSaveChannels();
  _msgs.forget(ThreadKey::channel((uint8_t)s));
  configChanged();
  _rev++;
  return true;
}

void MeshAdapter::setMuted(int i, bool muted) {
  int s = slotOf(_mesh, i);
  if (s < 0) return;
  if (muted) _ui.mutedChannels |= (uint16_t)(1u << s);
  else _ui.mutedChannels &= (uint16_t)~(1u << s);
  saveUiPrefs();
  configChanged();
  _rev++;
}

// ------------------------------------------------------------------ threads

static ThreadKey keyFor(MyMesh* m, model::ThreadId t, bool& ok) {
  ok = true;
  if (t.kind == model::ThreadKind::Channel) return ThreadKey::channel((uint8_t)t.idx);
  ContactInfo c;
  if (!m->getContactByIdx(MAX_ANON_CONTACTS + t.idx, c)) { ok = false; return ThreadKey(); }
  return ThreadKey::direct(c.id.pub_key);
}

int MeshAdapter::threadCount() const {
  // Conversations with contacts that were removed are hidden.
  int n = 0;
  for (int i = 0; i < _msgs.threadCount(); i++) {
    const ThreadInfo* t = _msgs.thread(i);
    if (t->key.kind == 1 || contactIndex(t->key.id) >= 0) n++;
  }
  return n;
}

bool MeshAdapter::thread(int i, model::Thread& out) const {
  for (int k = 0; k < _msgs.threadCount(); k++) {
    const ThreadInfo* t = _msgs.thread(k);
    int ci = -1;
    ChannelDetails d;
    if (t->key.kind == 0) {
      ci = contactIndex(t->key.id);
      if (ci < 0) continue;
    } else if (!_mesh->getChannel(t->key.id[0], d) || !d.name[0]) {
      continue;
    }
    if (i-- != 0) continue;
    memset(&out, 0, sizeof out);
    out.lastTs = t->lastTs;
    out.unread = t->unread;
    snprintf(out.preview, sizeof out.preview, "%s%s", t->lastOut ? "You: " : "", t->preview);
    if (t->key.kind == 1) {
      out.id = {model::ThreadKind::Channel, t->key.id[0]};
      snprintf(out.title, sizeof out.title, "%s", d.name);
      out.muted = (_ui.mutedChannels >> t->key.id[0]) & 1;
    } else {
      ContactInfo c;
      getContact(ci, c);
      out.id = {model::ThreadKind::Direct, (uint16_t)ci};
      snprintf(out.title, sizeof out.title, "%s", c.name);
      out.type = c.type <= ADV_TYPE_SENSOR ? (model::NodeType)c.type : model::NodeType::None;
      out.favourite = isFavourite(c.id.pub_key);
    }
    return true;
  }
  return false;
}

int MeshAdapter::messageCount(model::ThreadId t) const {
  bool ok;
  ThreadKey k = keyFor(_mesh, t, ok);
  if (!ok) return 0;
  if (!_hasOpen || !(_openKey == k)) {
    _msgs.open(k);
    _openKey = k;
    _hasOpen = true;
  }
  return _msgs.count();
}

bool MeshAdapter::message(model::ThreadId t, int i, model::Message& out) const {
  int n = messageCount(t);
  if (i < 0 || i >= n) return false;
  out = _msgs.at(i);
  return true;
}

void MeshAdapter::markRead(model::ThreadId t) {
  bool ok;
  ThreadKey k = keyFor(_mesh, t, ok);
  if (!ok) return;
  const ThreadInfo* ti = _msgs.find(k);
  if (ti && ti->unread) {
    _msgs.markRead(k);
    _rev++;
  }
}

uint16_t MeshAdapter::unreadTotal() const {
  uint32_t n = 0;
  for (int i = 0; i < _msgs.threadCount(); i++) {
    const ThreadInfo* t = _msgs.thread(i);
    if (t->key.kind == 1 && ((_ui.mutedChannels >> t->key.id[0]) & 1)) continue;
    n += t->unread;
  }
  return n > 0xFFFF ? 0xFFFF : (uint16_t)n;
}

// ------------------------------------------------------------------ sending

bool MeshAdapter::sendDirect(Pending& p, const ContactInfo& c, const char* text) {
  uint32_t ack = 0, timeout = 0;
  int r = _mesh->sendMessage(c, p.ts, p.attempt, text, ack, timeout);
  if (r == MSG_SEND_FAILED) return false;
  p.prevAck = p.ack;
  p.ack = ack;
  p.sentAt = millis();
  p.timeout = timeout + 1000;
  return true;
}

bool MeshAdapter::send(model::ThreadId t, const char* text) {
  if (!text[0]) return false;
  uint32_t ts = _rtc->getCurrentTimeUnique();
  model::Message m;
  memset(&m, 0, sizeof m);
  m.ts = ts;
  m.outgoing = true;
  m.snr_x4 = INT8_MIN;
  snprintf(m.text, sizeof m.text, "%s", text);

  if (t.kind == model::ThreadKind::Channel) {
    ChannelDetails d;
    if (!_mesh->getChannel(t.idx, d) || !d.name[0]) return false;
    const char* name = _mesh->getNodePrefs()->node_name;
    bool ok = _mesh->sendGroupMessage(ts, d.channel, name, text, (int)strlen(text));
    m.status = ok ? model::Delivery::Sent : model::Delivery::Failed;
    _msgs.append(ThreadKey::channel((uint8_t)t.idx), m);
    _rev++;
    return ok;
  }

  ContactInfo* c = contactPtr(t.idx);
  if (!c) return false;
  Pending* p = nullptr;
  for (auto& q : _pending)
    if (!q.used) { p = &q; break; }
  if (!p) return false;   // too many messages waiting for ACKs

  m.status = model::Delivery::Pending;
  ThreadKey k = ThreadKey::direct(c->id.pub_key);
  uint32_t seq = _msgs.append(k, m);
  if (!seq) return false;
  memset(p, 0, sizeof *p);
  p->seq = seq;
  p->ts = ts;
  memcpy(p->key, c->id.pub_key, 6);
  p->used = true;
  if (!sendDirect(*p, *c, text)) {
    p->used = false;
    _msgs.setStatus(seq, model::Delivery::Failed, 0);
  }
  _rev++;
  return true;
}

void MeshAdapter::onAckReceived(const uint8_t* ack) {
  uint32_t a = rd32(ack);
  for (auto& p : _pending) {
    if (!p.used || (a != p.ack && a != p.prevAck)) continue;
    _msgs.setStatus(p.seq, model::Delivery::Delivered, p.attempt);
    p.used = false;
    _rev++;
  }
}

void MeshAdapter::loop() {
  uint32_t ms = millis();
  for (auto& p : _pending) {
    if (!p.used || ms - p.sentAt < p.timeout) continue;
    int ci = contactIndex(p.key);
    ContactInfo* c = contactPtr(ci);
    char text[model::TEXT_LEN];
    p.attempt++;
    bool more = c && p.attempt < MAX_DM_ATTEMPTS + (c->out_path_len != OUT_PATH_UNKNOWN ? 1 : 0);
    if (more && p.attempt == MAX_DM_ATTEMPTS) {
      // The stored route seems broken: forget it and flood one last time.
      c->out_path_len = OUT_PATH_UNKNOWN;
      _mesh->uiMarkContactsDirty();
    }
    if (more && _msgs.text(p.seq, text, sizeof text) && sendDirect(p, *c, text)) {
      _msgs.setStatus(p.seq, model::Delivery::Pending, p.attempt);
    } else {
      _msgs.setStatus(p.seq, model::Delivery::Failed, p.attempt);
      p.used = false;
    }
    _rev++;
  }

  if (_req.state == model::Req::Busy && (int32_t)(ms - _req.deadline) >= 0) {
    _req.state = model::Req::Timeout;
    snprintf(_req.text, sizeof _req.text, "No reply");
    _rev++;
  }
  if (_discoverState == model::Req::Busy && (int32_t)(ms - _discoverUntil) >= 0) {
    _discoverState = model::Req::Ok;
    _rev++;
  }

  if (_bootGpsUntil) {
    model::Gps g = _plat.gps();
    bool done = g.fix && g.lastFixS;
    if (done) gpsSyncTime();
    if (done || (int32_t)(ms - _bootGpsUntil) >= 0) {
      _bootGpsUntil = 0;
      if (!_prefs.gpsEnabled) _plat.setGpsPower(false);
    }
  }

  // Contacts MeshCore adds or drops by itself (auto-add) also count.
  int nc = _mesh->getNumContacts();
  if (nc != _seenContacts) {
    _seenContacts = nc;
    configChanged();
  }
  // Card inserted or removed: refresh what the Backup screen shows.
  if ((_plat.sd() != nullptr) != _bk.card) refreshBackupInfo();
  // Automatic backup: 10 s after the last change, at most once a minute.
  if (_ui.autoBackup && _cfgChangedAt && _bk.card && ms - _cfgChangedAt > 10000 &&
      (!_lastBackupAt || ms - _lastBackupAt > 60000))
    backupNow();
}

// ------------------------------------------------------------------ receiving

void MeshAdapter::onDirectMessage(const ContactInfo& from, uint8_t txt_type, uint8_t path_len, float snr,
                                  uint32_t sender_ts, const uint8_t* extra, int extra_len, const char* text) {
  if (path_len == 0 || path_len == 0xFF) noteSignal(from.id.pub_key, snr, _lastRssi);
  if (txt_type == TXT_TYPE_CLI_DATA) {
    if (_req.state == model::Req::Busy && _req.kind == ReqKind::Cli && !memcmp(_req.key, from.id.pub_key, 6))
      finishRequest(model::Req::Ok, "%s", text);
    return;   // command replies are not conversation
  }
  model::Message m;
  memset(&m, 0, sizeof m);
  m.ts = sender_ts;
  m.snr_x4 = (int8_t)(snr * 4);
  m.hops = hopsOf(path_len);
  if (txt_type == TXT_TYPE_SIGNED_PLAIN && extra_len >= 4) {
    // Room server post: extra holds the author's key prefix.
    ContactInfo* author = _mesh->lookupContactByPubKey(extra, 4);
    if (author) snprintf(m.sender, sizeof m.sender, "%s", author->name);
    else hex(m.sender, sizeof m.sender, extra, 4);
  }
  snprintf(m.text, sizeof m.text, "%s", text);
  _msgs.append(ThreadKey::direct(from.id.pub_key), m);
  if (_ui.backupMessages) configChanged();
  _rev++;
}

void MeshAdapter::onChannelMessage(uint8_t idx, uint8_t path_len, float snr, uint32_t ts, const char* text) {
  model::Message m;
  memset(&m, 0, sizeof m);
  m.ts = ts;
  m.snr_x4 = (int8_t)(snr * 4);
  m.hops = hopsOf(path_len);
  // Channel payloads are "sender: text".
  const char* sep = strstr(text, ": ");
  if (sep && sep - text < (int)sizeof m.sender) {
    snprintf(m.sender, sizeof m.sender, "%.*s", (int)(sep - text), text);
    text = sep + 2;
  }
  snprintf(m.text, sizeof m.text, "%s", text);
  ThreadKey k = ThreadKey::channel(idx);
  _msgs.append(k, m);
  if ((_ui.mutedChannels >> idx) & 1) _msgs.markRead(k);
  if (_ui.backupMessages) configChanged();
  _rev++;
}

void MeshAdapter::onRawRx(float snr, float rssi, const uint8_t* raw, int len) {
  _lastSnr = (int8_t)(snr * 4);
  _lastRssi = (int16_t)rssi;
  mesh::Packet pkt;
  if (len <= 0 || len > 255 || !pkt.readFrom(raw, (uint8_t)len)) return;

  uint8_t hash[MAX_HASH_SIZE];
  pkt.calculatePacketHash(hash);
  bool dup = false;
  for (int i = 0; i < _nLog; i++)
    if (!memcmp(_logHash[i], hash, 4)) { dup = true; break; }
  if (dup) _dupsSeen++;

  model::RxLogEntry& e = _log[_logHead];
  memcpy(_logHash[_logHead], hash, 4);
  e.ms = millis();
  e.snr_x4 = _lastSnr;
  e.rssi = _lastRssi;
  e.payloadType = pkt.getPayloadType();
  e.routeType = pkt.getRouteType();
  e.pathLen = pkt.getPathHashCount();
  e.len = (uint8_t)len;
  e.dup = dup;
  uint8_t t = e.payloadType;
  e.srcHash = pkt.payload_len > 1 && (t == PAYLOAD_TYPE_TXT_MSG || t == PAYLOAD_TYPE_REQ || t == PAYLOAD_TYPE_RESPONSE ||
                                      t == PAYLOAD_TYPE_PATH)
                  ? pkt.payload[1]
                  : pkt.payload_len ? pkt.payload[0] : 0;
  _logHead = (_logHead + 1) % MAX_LOG;
  if (_nLog < MAX_LOG) _nLog++;

  if (t == PAYLOAD_TYPE_ADVERT && !dup) heardAdvert(pkt, snr, rssi);
  _rev++;
}

bool MeshAdapter::rxLog(int i, model::RxLogEntry& out) const {
  if (i < 0 || i >= _nLog) return false;
  out = _log[(_logHead - 1 - i + MAX_LOG) % MAX_LOG];
  return true;
}

void MeshAdapter::heardAdvert(const mesh::Packet& pkt, float snr, float rssi) {
  // ADVERT payload: pub_key, timestamp, signature, app data.
  const int hdr = PUB_KEY_SIZE + 4 + SIGNATURE_SIZE;
  if (pkt.payload_len <= hdr) return;
  if (!memcmp(pkt.payload, _mesh->self_id.pub_key, PUB_KEY_SIZE)) return;
  AdvertDataParser parser(&pkt.payload[hdr], (uint8_t)(pkt.payload_len - hdr));
  if (!parser.isValid()) return;
  Heard h;
  memset(&h, 0, sizeof h);
  memcpy(h.pubkey, pkt.payload, PUB_KEY_SIZE);
  snprintf(h.name, sizeof h.name, "%s", parser.hasName() ? parser.getName() : "");
  h.type = parser.getType();
  h.hops = pkt.isRouteFlood() ? (int8_t)pkt.getPathHashCount() : 0;
  h.snr_x4 = (int8_t)(snr * 4);
  h.rssi = (int16_t)rssi;
  h.at = now();
  h.hasPos = parser.hasLatLon();
  h.lat = parser.getIntLat();
  h.lon = parser.getIntLon();
  if (h.hops == 0) noteSignal(h.pubkey, snr, rssi);
  putHeard(h);
}

void MeshAdapter::putHeard(const Heard& h) {
  int slot = -1;
  for (int i = 0; i < _nHeard; i++)
    if (!memcmp(_heard[i].pubkey, h.pubkey, PUB_KEY_SIZE)) { slot = i; break; }
  if (slot < 0) {
    if (_nHeard < MAX_HEARD) slot = _nHeard++;
    else {
      slot = 0;
      for (int i = 1; i < _nHeard; i++)
        if (_heard[i].at < _heard[slot].at) slot = i;
    }
  } else if (!h.name[0]) {
    // A discovery reply carries no name: keep the one from the advert.
    Heard keep = _heard[slot];
    _heard[slot] = h;
    memcpy(_heard[slot].name, keep.name, sizeof keep.name);
    _heard[slot].hasPos = keep.hasPos;
    _heard[slot].lat = keep.lat;
    _heard[slot].lon = keep.lon;
    goto sort;
  }
  _heard[slot] = h;
sort:
  // Most recent first.
  for (int i = 1; i < _nHeard; i++) {
    Heard v = _heard[i];
    int j = i - 1;
    while (j >= 0 && _heard[j].at < v.at) { _heard[j + 1] = _heard[j]; j--; }
    _heard[j + 1] = v;
  }
}

bool MeshAdapter::heard(int i, model::Contact& out) const {
  if (i < 0 || i >= _nHeard) return false;
  const Heard& h = _heard[i];
  memset(&out, 0, sizeof out);
  if (h.name[0]) snprintf(out.name, sizeof out.name, "%s", h.name);
  else {
    ContactInfo* c = _mesh->lookupContactByPubKey(h.pubkey, PUB_KEY_SIZE);
    if (c) snprintf(out.name, sizeof out.name, "%s", c->name);
    else hex(out.name, sizeof out.name, h.pubkey, 4);
  }
  memcpy(out.key, h.pubkey, 6);
  out.type = h.type <= ADV_TYPE_SENSOR ? (model::NodeType)h.type : model::NodeType::None;
  out.hops = h.hops;
  out.snr_x4 = h.snr_x4;
  out.rssi = h.rssi;
  out.lastHeard = h.at;
  out.hasPos = h.hasPos;
  out.lat_e6 = h.lat;
  out.lon_e6 = h.lon;
  return true;
}

bool MeshAdapter::addHeard(int i) {
  if (i < 0 || i >= _nHeard) return false;
  const Heard& h = _heard[i];
  if (_mesh->lookupContactByPubKey(h.pubkey, PUB_KEY_SIZE)) return false;
  ContactInfo c;
  memset(&c, 0, sizeof c);
  c.id = mesh::Identity(h.pubkey);
  if (h.name[0]) snprintf(c.name, sizeof c.name, "%s", h.name);
  else hex(c.name, sizeof c.name, h.pubkey, 4);
  c.type = h.type;
  c.out_path_len = OUT_PATH_UNKNOWN;
  c.lastmod = now();
  c.gps_lat = h.lat;
  c.gps_lon = h.lon;
  if (!_mesh->addContact(c)) return false;
  _mesh->uiMarkContactsDirty();
  configChanged();
  _rev++;
  return true;
}

bool MeshAdapter::advert(bool flood) {
  bool ok = _mesh->uiSendAdvert(flood);
  _rev++;
  return ok;
}

bool MeshAdapter::discoverRepeaters() {
  uint8_t data[10];
  data[0] = CTL_DISCOVER_REQ;
  data[1] = 1 << ADV_TYPE_REPEATER;
  _mesh->getRNG()->random(&data[2], 4);
  memcpy(&_discoverTag, &data[2], 4);
  memset(&data[6], 0, 4);   // since: everything
  mesh::Packet* pkt = _mesh->createControlData(data, sizeof data);
  if (!pkt) return false;
  _mesh->sendZeroHop(pkt);
  _discoverState = model::Req::Busy;
  _discoverUntil = millis() + DISCOVER_LISTEN_MS;
  _rev++;
  return true;
}

void MeshAdapter::onControlData(float snr, float rssi, uint8_t, const uint8_t* p, int len) {
  if (len < 6 + PUB_KEY_SIZE || (p[0] & 0xF0) != CTL_DISCOVER_RESP) return;
  if (_discoverState != model::Req::Busy || rd32(&p[2]) != _discoverTag) return;
  Heard h;
  memset(&h, 0, sizeof h);
  memcpy(h.pubkey, &p[6], PUB_KEY_SIZE);
  h.type = p[0] & 0x0F;
  h.hops = 0;
  h.snr_x4 = (int8_t)(snr * 4);
  h.rssi = (int16_t)rssi;
  h.at = now();
  noteSignal(h.pubkey, snr, rssi);
  putHeard(h);
  _rev++;
}

// ------------------------------------------------------------------ requests

bool MeshAdapter::startRequest(ReqKind k, int contact, uint32_t est) {
  ContactInfo c;
  if (!getContact(contact, c)) return false;
  _req.kind = k;
  _req.state = model::Req::Busy;
  memcpy(_req.key, c.id.pub_key, PUB_KEY_SIZE);
  _req.startedAt = millis();
  _req.deadline = millis() + est + 4000;
  _req.text[0] = 0;
  _rev++;
  return true;
}

void MeshAdapter::finishRequest(model::Req st, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(_req.text, sizeof _req.text, fmt, ap);
  va_end(ap);
  _req.state = st;
  _req.rttMs = millis() - _req.startedAt;
  _rev++;
}

bool MeshAdapter::login(int ci, const char* password) {
  if (_req.state == model::Req::Busy) return false;
  ContactInfo* c = contactPtr(ci);
  uint32_t est = 0;
  if (!c || _mesh->sendLogin(*c, password, est) == MSG_SEND_FAILED) return false;
  return startRequest(ReqKind::Login, ci, est);
}

bool MeshAdapter::requestStats(int ci) {
  if (_req.state == model::Req::Busy) return false;
  ContactInfo* c = contactPtr(ci);
  uint32_t est = 0, tag = 0;
  if (!c || _mesh->sendRequest(*c, REQ_GET_STATUS, tag, est) == MSG_SEND_FAILED) return false;
  _req.tag = tag;
  return startRequest(ReqKind::Status, ci, est);
}

bool MeshAdapter::requestTelemetry(int ci) {
  if (_req.state == model::Req::Busy) return false;
  ContactInfo* c = contactPtr(ci);
  uint32_t est = 0, tag = 0;
  if (!c || _mesh->sendRequest(*c, REQ_GET_TELEMETRY, tag, est) == MSG_SEND_FAILED) return false;
  _req.tag = tag;
  return startRequest(ReqKind::Telemetry, ci, est);
}

bool MeshAdapter::requestNeighbours(int ci) {
  if (_req.state == model::Req::Busy) return false;
  ContactInfo* c = contactPtr(ci);
  // version 0, count, offset (2), order (2 = strongest first), key prefix bytes, 4 random
  uint8_t req[11] = {REQ_GET_NEIGHBOURS, 0, MAX_NEIGH, 0, 0, 2, 4};
  _mesh->getRNG()->random(&req[7], 4);
  uint32_t est = 0, tag = 0;
  if (!c || _mesh->sendRequest(*c, req, sizeof req, tag, est) == MSG_SEND_FAILED) return false;
  _req.tag = tag;
  _nNeighbours = 0;
  return startRequest(ReqKind::Neighbours, ci, est);
}

bool MeshAdapter::sendCommand(int ci, const char* cmd) {
  if (_req.state == model::Req::Busy) return false;
  ContactInfo* c = contactPtr(ci);
  uint32_t est = 0;
  if (!c || _mesh->sendCommandData(*c, _rtc->getCurrentTimeUnique(), 0, cmd, est) == MSG_SEND_FAILED) return false;
  return startRequest(ReqKind::Cli, ci, est);
}

bool MeshAdapter::tracePath(int ci) {
  if (_req.state == model::Req::Busy) return false;
  ContactInfo* c = contactPtr(ci);
  if (!c) return false;
  if (c->out_path_len == OUT_PATH_UNKNOWN) {
    snprintf(_req.text, sizeof _req.text, "No route known yet: send a message first");
    _req.state = model::Req::Failed;
    _rev++;
    return false;
  }
  // Out along the stored route (plus the node itself if it relays), then
  // back the same way: every relay appends the SNR it heard.
  int sz = (c->out_path_len >> 6) + 1, hops = c->out_path_len & 63;
  uint8_t path[MAX_PATH_SIZE];
  int n = 0;
  for (int i = 0; i < hops * sz && n < MAX_PATH_SIZE; i++) path[n++] = c->out_path[i];
  if (c->type == ADV_TYPE_REPEATER || c->type == ADV_TYPE_ROOM)
    for (int i = 0; i < sz && n < MAX_PATH_SIZE; i++) path[n++] = c->id.pub_key[i];
  for (int h = hops - (c->type == ADV_TYPE_REPEATER || c->type == ADV_TYPE_ROOM ? 1 : 2); h >= 0; h--)
    for (int i = 0; i < sz && n < MAX_PATH_SIZE; i++) path[n++] = c->out_path[h * sz + i];
  if (n == 0) {
    snprintf(_req.text, sizeof _req.text, "Direct neighbour: no relays to trace");
    _req.state = model::Req::Failed;
    _rev++;
    return false;
  }
  uint32_t tag;
  _mesh->getRNG()->random((uint8_t*)&tag, 4);
  uint8_t flags = (uint8_t)(sz == 1 ? 0 : sz == 2 ? 1 : sz == 4 ? 2 : 3);
  mesh::Packet* pkt = _mesh->createTrace(tag, 0, flags);
  if (!pkt) return false;
  _mesh->sendDirect(pkt, path, (uint8_t)n);
  _req.tag = tag;
  uint32_t est = radio_driver.getEstAirtimeFor(20 + n) * (uint32_t)(n + 1) * 2;
  return startRequest(ReqKind::Trace, ci, est);
}

bool MeshAdapter::ping(int ci) {
  ContactInfo c;
  if (!getContact(ci, c)) return false;
  bool ok = c.type == ADV_TYPE_CHAT ? tracePath(ci) : requestStats(ci);
  if (ok) _req.kind = ReqKind::Ping;
  return ok;
}

void MeshAdapter::onTraceData(uint32_t tag, uint8_t flags, const uint8_t* hashes, const uint8_t* snrs, uint8_t path_len,
                              float final_snr) {
  if (_req.state != model::Req::Busy || tag != _req.tag) return;
  if (_req.kind == ReqKind::Ping) { finishRequest(model::Req::Ok, "pong"); return; }
  int sz = 1 << (flags & 3);
  int hops = path_len / sz;
  char out[200];
  int o = snprintf(out, sizeof out, "Round trip %u ms, %d relay%s\n", (unsigned)(millis() - _req.startedAt), hops,
                   hops == 1 ? "" : "s");
  for (int h = 0; h < hops && o < (int)sizeof out - 40; h++) {
    char id[12], name[24] = "";
    hex(id, sizeof id, &hashes[h * sz], sz);
    ContactInfo* c = _mesh->lookupContactByPubKey(&hashes[h * sz], sz);
    if (c) snprintf(name, sizeof name, " %.18s", c->name);
    int s = (int8_t)snrs[h];
    o += snprintf(out + o, sizeof out - o, "> %s%s  snr %d.%d\n", id, name, s / 4, (s < 0 ? -s : s) % 4 * 25 / 10);
  }
  snprintf(out + o, sizeof out - o, "back to us  snr %.1f", final_snr);
  finishRequest(model::Req::Ok, "%s", out);
}

void MeshAdapter::onContactResponse(const ContactInfo& from, const uint8_t* data, uint8_t len) {
  if (_req.state != model::Req::Busy || memcmp(from.id.pub_key, _req.key, PUB_KEY_SIZE) || len < 4) return;
  uint32_t tag = rd32(data);
  switch (_req.kind) {
    case ReqKind::Login:
      if (len >= 6 && !memcmp(&data[4], "OK", 2)) finishRequest(model::Req::Ok, "Logged in");
      else if (len >= 7 && data[4] == RESP_SERVER_LOGIN_OK)
        finishRequest(model::Req::Ok, (data[6] & 1) ? "Logged in as admin" : "Logged in as guest");
      else finishRequest(model::Req::Failed, "Login refused");
      break;
    case ReqKind::Ping:
    case ReqKind::Status: {
      if (_req.kind == ReqKind::Ping) { finishRequest(model::Req::Ok, "pong"); break; }
      if (len < 4 + 56) { finishRequest(model::Req::Failed, "Short status reply"); break; }
      const uint8_t* s = data + 4;   // simple_repeater's RepeaterStats
      _remote.valid = true;
      _remote.battMv = rd16(s + 0);
      _remote.queueLen = rd16(s + 2);
      _remote.noiseFloor = (int16_t)rd16(s + 4);
      _remote.lastRssi = (int16_t)rd16(s + 6);
      _remote.rxPackets = rd32(s + 8);
      _remote.txPackets = rd32(s + 12);
      _remote.airTxS = rd32(s + 16);
      _remote.uptimeS = rd32(s + 20);
      _remote.floodsSent = rd32(s + 24);
      _remote.directSent = rd32(s + 28);
      _remote.lastSnr_x4 = (int8_t)(int16_t)rd16(s + 42);
      _remote.dupsDirect = rd16(s + 44);
      _remote.dupsFlood = rd16(s + 46);
      _remote.airRxS = rd32(s + 48);
      finishRequest(model::Req::Ok, "ok");
      break;
    }
    case ReqKind::Telemetry:
      if (tag != _req.tag) return;
      decodeTelemetry(data + 4, len - 4);
      break;
    case ReqKind::Neighbours: {
      if (tag != _req.tag || len < 8) return;
      int count = rd16(data + 6);
      const uint8_t* e = data + 8;
      _nNeighbours = 0;
      for (int i = 0; i < count && _nNeighbours < MAX_NEIGH && e + 9 <= data + len; i++, e += 9) {
        model::Neighbour& n = _neigh[_nNeighbours++];
        memset(&n, 0, sizeof n);
        memcpy(n.key, e, 4);
        n.heardAgoS = rd32(e + 4);
        n.snr_x4 = (int8_t)e[8];
        ContactInfo* c = _mesh->lookupContactByPubKey(e, 4);
        if (c) snprintf(n.name, sizeof n.name, "%s", c->name);
      }
      finishRequest(model::Req::Ok, "%d neighbours", _nNeighbours);
      break;
    }
    default:
      break;
  }
}

bool MeshAdapter::neighbour(int i, model::Neighbour& out) const {
  if (i < 0 || i >= _nNeighbours) return false;
  out = _neigh[i];
  return true;
}

// Cayenne LPP: channel, type, big-endian value. Only common types are named.
void MeshAdapter::decodeTelemetry(const uint8_t* p, int len) {
  char out[200];
  int o = 0;
  out[0] = 0;
  for (int i = 0; i + 2 <= len && o < (int)sizeof out - 40;) {
    uint8_t ch = p[i], type = p[i + 1];
    i += 2;
    auto be = [&](int n) { int32_t v = 0; for (int k = 0; k < n; k++) v = (v << 8) | p[i + k]; return v; };
    const char* label = nullptr;
    int size = 0;
    double val = 0;
    const char* unit = "";
    switch (type) {
      case 116: label = "Voltage"; size = 2; val = (uint16_t)be(2) / 100.0; unit = " V"; break;
      case 103: label = "Temperature"; size = 2; val = (int16_t)be(2) / 10.0; unit = " C"; break;
      case 104: label = "Humidity"; size = 1; val = be(1) / 2.0; unit = " %"; break;
      case 115: label = "Pressure"; size = 2; val = (uint16_t)be(2) / 10.0; unit = " hPa"; break;
      case 117: label = "Current"; size = 2; val = (uint16_t)be(2) / 1000.0; unit = " A"; break;
      case 128: label = "Power"; size = 2; val = (uint16_t)be(2); unit = " W"; break;
      case 2: label = "Analog"; size = 2; val = (int16_t)be(2) / 100.0; break;
      case 136: {   // GPS: lat, lon (0.0001 deg), alt (0.01 m), 3 bytes each
        if (i + 9 > len) { i = len; break; }
        int32_t la = be(3), lo;
        if (la & 0x800000) la |= ~0xFFFFFF;
        i += 3;
        lo = be(3);
        if (lo & 0x800000) lo |= ~0xFFFFFF;
        i += 6;
        o += snprintf(out + o, sizeof out - o, "Position %.4f, %.4f\n", la / 10000.0, lo / 10000.0);
        continue;
      }
      default: i = len; continue;   // unknown size: stop
    }
    if (i + size > len) break;
    i += size;
    o += snprintf(out + o, sizeof out - o, "%s%s: %.2f%s\n", ch > 1 ? "  " : "", label, val, unit);
  }
  finishRequest(model::Req::Ok, "%s", o ? out : "No telemetry shared");
}

// ------------------------------------------------------------------ radio

model::Radio MeshAdapter::radio() const {
  NodePrefs* p = _mesh->getNodePrefs();
  model::Radio r;
  r.freqMHz = p->freq;
  r.bwKHz = p->bw;
  r.sf = p->sf;
  r.cr = p->cr;
  r.txDbm = p->tx_power_dbm;
  r.txMaxDbm = MAX_LORA_TX_POWER;
  r.repeat = p->isRepeatEn();
  r.rxBoost = p->rx_boosted_gain;
  return r;
}

bool MeshAdapter::setRadio(const model::Radio& r) {
  if (r.freqMHz < 150 || r.freqMHz > 2500 || r.sf < 5 || r.sf > 12 || r.cr < 5 || r.cr > 8) return false;
  NodePrefs* p = _mesh->getNodePrefs();
  p->freq = r.freqMHz;
  p->bw = r.bwKHz;
  p->sf = r.sf;
  p->cr = r.cr;
  p->tx_power_dbm = r.txDbm > MAX_LORA_TX_POWER ? MAX_LORA_TX_POWER : r.txDbm;
  p->setRepeatEn(r.repeat);
  p->rx_boosted_gain = r.rxBoost;
  _mesh->savePrefs();
  radio_driver.setParams(p->freq, p->bw, p->sf, p->cr);
  radio_driver.setTxPower(p->tx_power_dbm);
  radio_driver.setRxBoostedGainMode(p->rx_boosted_gain);
  configChanged();
  _rev++;
  return true;
}

int MeshAdapter::presetCount() const { return kRadioPresetCount; }
const model::Preset& MeshAdapter::preset(int i) const { return kRadioPresets[i]; }

// ------------------------------------------------------------------ diagnostics

model::Diag MeshAdapter::diag() const {
  model::Diag d;
  memset(&d, 0, sizeof d);
  d.uptimeS = millis() / 1000;
  d.noiseFloor = (int16_t)radio_driver.getNoiseFloor();
  d.lastRssi = _lastRssi;
  d.lastSnr_x4 = _lastSnr;
  d.rxPackets = radio_driver.getPacketsRecv();
  d.txPackets = radio_driver.getPacketsSent();
  d.rxFlood = _mesh->getNumRecvFlood();
  d.rxDirect = _mesh->getNumRecvDirect();
  d.txFlood = _mesh->getNumSentFlood();
  d.txDirect = _mesh->getNumSentDirect();
  d.dups = _dupsSeen;
  d.rxErrors = radio_driver.getPacketsRecvErrors();
  d.airTxMs = _mesh->getTotalAirTime();
  d.airRxMs = _mesh->getReceiveAirTime();
  uint32_t up = millis();
  d.dutyPct = up ? (uint8_t)(d.airTxMs * 100ULL / up) : 0;
  d.heapFree = _plat.heapFree();
  d.heapMin = _plat.heapMin();
  d.heapMaxBlock = _plat.heapMaxBlock();
  d.battMv = getBattMilliVolts();
  d.battPct = batteryPercent(d.battMv);
  d.charging = _plat.charging();
  d.contacts = (uint16_t)_mesh->getNumContacts();
  d.contactsMax = MAX_CONTACTS;
  d.channels = (uint8_t)channelCount();
  d.channelsMax = MAX_GROUP_CHANNELS;
  return d;
}

bool MeshAdapter::gpsSyncTime() {
  model::Gps g = _plat.gps();
  if (!g.fix || !g.lastFixS) return false;
  _rtc->setCurrentTime(g.lastFixS);
  _rev++;
  return true;
}

// ------------------------------------------------------------------ backup

void MeshAdapter::refreshBackupInfo() {
  fs::FS* sd = _plat.sd();
  _bk.card = sd != nullptr;
  memset(&_bk.cur, 0, sizeof _bk.cur);
  memset(&_bk.prev, 0, sizeof _bk.prev);
  if (sd) {
    _bk.cur = Backup::info(*sd, false);
    _bk.prev = Backup::info(*sd, true);
  }
  _rev++;
}

model::BackupStatus MeshAdapter::backupStatus() const {
  model::BackupStatus b;
  memset(&b, 0, sizeof b);
  b.cardPresent = _bk.card;
  b.hasBackup = _bk.cur.valid;
  b.hasPrevious = _bk.prev.valid;
  b.lastBackupTs = _bk.cur.valid ? _bk.cur.created : 0;
  snprintf(b.lastError, sizeof b.lastError, "%s", _bk.error);
  return b;
}

void MeshAdapter::configChanged() {
  _cfgChangedAt = millis() | 1;
  _rev++;
}

bool MeshAdapter::backupNow() {
  fs::FS* sd = _plat.sd();
  if (!sd) {
    snprintf(_bk.error, sizeof _bk.error, "No SD card");
    _bk.card = false;
    return false;
  }
  _mesh->uiFlushContacts();   // MeshCore saves contacts lazily
  Backup::Options o{_ui.backupIdentity != 0, _ui.backupMessages != 0};
  bool ok = Backup::write(*_fs, *sd, o, now(), FIRMWARE_VERSION, _bk.error, sizeof _bk.error);
  if (ok) _bk.error[0] = 0;
  _cfgChangedAt = 0;
  _lastBackupAt = millis() | 1;
  refreshBackupInfo();
  return ok;
}

bool MeshAdapter::restore(bool previous) {
  fs::FS* sd = _plat.sd();
  if (!sd) return false;
  if (!Backup::restore(*_fs, *sd, previous, _bk.error, sizeof _bk.error)) {
    _rev++;
    return false;
  }
  _fresh = false;
  _plat.reboot();   // MeshCore reloads everything from flash
  return true;
}

void MeshAdapter::factoryReset() {
  _msgs.wipe();
  _fs->remove(kUiPrefsFile);
  _store->formatFileSystem();
  _plat.reboot();
}

}  // namespace app
