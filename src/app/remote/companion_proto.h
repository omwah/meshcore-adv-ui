// MeshCore's companion app protocol, as stock companion_radio firmware
// speaks it (examples/companion_radio/MyMesh.cpp, FIRMWARE_VER_CODE 13):
// command codes, reply and push codes, and the layouts of the frames we use.
// Plain C++ (no Arduino, no MeshCore headers) so it builds and is tested on a
// host. Multi-byte fields are little endian.
#pragma once
#include <stdint.h>
#include <string.h>

namespace app {
namespace cp {

constexpr int MAX_FRAME = 176;          // MeshCore's MAX_FRAME_SIZE
constexpr int KEY = 32;                 // PUB_KEY_SIZE
constexpr int MAX_PATH = 64;            // MAX_PATH_SIZE
constexpr uint8_t APP_PROTOCOL = 13;    // the protocol version we understand

enum Cmd : uint8_t {
  APP_START = 1, SEND_TXT_MSG = 2, SEND_CHANNEL_TXT_MSG = 3, GET_CONTACTS = 4, GET_DEVICE_TIME = 5,
  SET_DEVICE_TIME = 6, SEND_SELF_ADVERT = 7, SET_ADVERT_NAME = 8, ADD_UPDATE_CONTACT = 9, SYNC_NEXT_MESSAGE = 10,
  SET_RADIO_PARAMS = 11, SET_RADIO_TX_POWER = 12, RESET_PATH = 13, SET_ADVERT_LATLON = 14, REMOVE_CONTACT = 15,
  EXPORT_CONTACT = 17, IMPORT_CONTACT = 18, REBOOT = 19, GET_BATT_AND_STORAGE = 20, SET_TUNING_PARAMS = 21,
  DEVICE_QUERY = 22, EXPORT_PRIVATE_KEY = 23, IMPORT_PRIVATE_KEY = 24, SEND_LOGIN = 26, SEND_STATUS_REQ = 27,
  LOGOUT = 29, GET_CONTACT_BY_KEY = 30, GET_CHANNEL = 31, SET_CHANNEL = 32, SEND_TRACE_PATH = 36,
  SET_DEVICE_PIN = 37, SET_OTHER_PARAMS = 38, SEND_TELEMETRY_REQ = 39, GET_TUNING_PARAMS = 43,
  SEND_BINARY_REQ = 50, FACTORY_RESET = 51, SEND_PATH_DISCOVERY_REQ = 52, GET_STATS = 56,
  SEND_CONTROL_DATA = 55, SET_PATH_HASH_MODE = 61,
};

enum Resp : uint8_t {
  OK = 0, ERR = 1, CONTACTS_START = 2, CONTACT = 3, END_OF_CONTACTS = 4, SELF_INFO = 5, SENT = 6,
  CURR_TIME = 9, NO_MORE_MESSAGES = 10, EXPORT_CONTACT_R = 11, BATT_AND_STORAGE = 12, DEVICE_INFO = 13,
  PRIVATE_KEY = 14, DISABLED = 15, CONTACT_MSG_RECV_V3 = 16, CHANNEL_MSG_RECV_V3 = 17, CHANNEL_INFO = 18,
  TUNING_PARAMS = 23, STATS = 24,
};

enum Push : uint8_t {
  P_ADVERT = 0x80, P_PATH_UPDATED = 0x81, P_SEND_CONFIRMED = 0x82, P_MSG_WAITING = 0x83, P_LOGIN_SUCCESS = 0x85,
  P_LOGIN_FAIL = 0x86, P_STATUS_RESPONSE = 0x87, P_LOG_RX_DATA = 0x88, P_TRACE_DATA = 0x89, P_NEW_ADVERT = 0x8A,
  P_TELEMETRY_RESPONSE = 0x8B, P_BINARY_RESPONSE = 0x8C, P_PATH_DISCOVERY_RESPONSE = 0x8D, P_CONTROL_DATA = 0x8E,
  P_CONTACT_DELETED = 0x8F, P_CONTACTS_FULL = 0x90,
};

enum Err : uint8_t { E_UNSUPPORTED = 1, E_NOT_FOUND = 2, E_TABLE_FULL = 3, E_BAD_STATE = 4, E_FILE_IO = 5, E_ILLEGAL_ARG = 6 };
enum StatsType : uint8_t { STATS_CORE = 0, STATS_RADIO = 1, STATS_PACKETS = 2 };
enum TxtType : uint8_t { TXT_PLAIN = 0, TXT_CLI = 1, TXT_SIGNED = 2 };

inline bool isPush(uint8_t code) { return code >= 0x80; }

inline uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
inline uint32_t rd32(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
inline void wr16(uint8_t* p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
inline void wr32(uint8_t* p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i)); }

// A contact as the RESP_CODE_CONTACT / PUSH_CODE_NEW_ADVERT frame carries it
// (and CMD_ADD_UPDATE_CONTACT takes it back).
struct Contact {
  uint8_t key[KEY];
  uint8_t type, flags, outPathLen;
  uint8_t outPath[MAX_PATH];
  char name[32];
  uint32_t lastAdvert;
  int32_t lat, lon;
  uint32_t lastmod;
};
constexpr int CONTACT_FRAME = 1 + KEY + 3 + MAX_PATH + 32 + 4 + 4 + 4 + 4;   // 148

inline bool parseContact(const uint8_t* f, int len, Contact& c) {
  if (len < 1 + KEY + 3 + MAX_PATH + 32 + 4) return false;
  int i = 1;
  memcpy(c.key, f + i, KEY); i += KEY;
  c.type = f[i++];
  c.flags = f[i++];
  c.outPathLen = f[i++];
  memcpy(c.outPath, f + i, MAX_PATH); i += MAX_PATH;
  memcpy(c.name, f + i, 32); c.name[31] = 0; i += 32;
  c.lastAdvert = rd32(f + i); i += 4;
  c.lat = c.lon = 0;
  c.lastmod = 0;
  if (len >= i + 8) { c.lat = (int32_t)rd32(f + i); c.lon = (int32_t)rd32(f + i + 4); i += 8; }
  if (len >= i + 4) c.lastmod = rd32(f + i);
  return true;
}

// CMD_ADD_UPDATE_CONTACT: the contact frame layout with the command code.
inline int encodeContact(uint8_t* f, const Contact& c) {
  int i = 0;
  f[i++] = ADD_UPDATE_CONTACT;
  memcpy(f + i, c.key, KEY); i += KEY;
  f[i++] = c.type;
  f[i++] = c.flags;
  f[i++] = c.outPathLen;
  memcpy(f + i, c.outPath, MAX_PATH); i += MAX_PATH;
  memset(f + i, 0, 32);
  strncpy((char*)f + i, c.name, 31); i += 32;
  wr32(f + i, c.lastAdvert); i += 4;
  wr32(f + i, (uint32_t)c.lat); i += 4;
  wr32(f + i, (uint32_t)c.lon); i += 4;
  wr32(f + i, c.lastmod); i += 4;
  return i;
}

// RESP_CODE_SELF_INFO (reply to CMD_APP_START).
struct SelfInfo {
  uint8_t advType;
  int8_t txDbm, txMaxDbm;
  uint8_t key[KEY];
  int32_t lat, lon;
  uint8_t multiAcks, advertLocPolicy, telemModes, manualAdd;
  uint32_t freqKHz, bwHz;   // freq in kHz (MHz * 1000), bw in Hz (kHz * 1000)
  uint8_t sf, cr;
  char name[32];
};

inline bool parseSelfInfo(const uint8_t* f, int len, SelfInfo& s) {
  if (len < 58) return false;
  int i = 1;
  s.advType = f[i++];
  s.txDbm = (int8_t)f[i++];
  s.txMaxDbm = (int8_t)f[i++];
  memcpy(s.key, f + i, KEY); i += KEY;
  s.lat = (int32_t)rd32(f + i); i += 4;
  s.lon = (int32_t)rd32(f + i); i += 4;
  s.multiAcks = f[i++];
  s.advertLocPolicy = f[i++];
  s.telemModes = f[i++];
  s.manualAdd = f[i++];
  s.freqKHz = rd32(f + i); i += 4;
  s.bwHz = rd32(f + i); i += 4;
  s.sf = f[i++];
  s.cr = f[i++];
  int n = len - i;
  if (n > 31) n = 31;
  memcpy(s.name, f + i, n);
  s.name[n] = 0;
  return true;
}

// RESP_CODE_DEVICE_INFO (reply to CMD_DEVICE_QUERY).
struct DeviceInfo {
  uint8_t fwCode;
  uint16_t maxContacts;
  uint8_t maxChannels;
  uint32_t blePin;
  char build[13], model[41], version[21];
  bool repeat;
  uint8_t pathHashMode;
};

inline bool parseDeviceInfo(const uint8_t* f, int len, DeviceInfo& d) {
  memset(&d, 0, sizeof d);
  if (len < 2) return false;
  d.fwCode = f[1];
  if (len < 80) { d.maxContacts = 100; d.maxChannels = 8; return true; }   // before v3
  d.maxContacts = (uint16_t)(f[2] * 2);
  d.maxChannels = f[3];
  d.blePin = rd32(f + 4);
  memcpy(d.build, f + 8, 12);
  memcpy(d.model, f + 20, 40);
  memcpy(d.version, f + 60, 20);
  if (len >= 81) d.repeat = f[80];
  if (len >= 82) d.pathHashMode = f[81];
  return true;
}

// RESP_CODE_CONTACT_MSG_RECV_V3 / CHANNEL_MSG_RECV_V3.
struct Msg {
  bool channel;
  int8_t snr_x4;
  uint8_t keyPrefix[6];     // direct: sender
  uint8_t channelIdx;       // channel
  uint8_t pathLen;          // 0xFF: came direct
  uint8_t txtType;
  uint32_t ts;
  uint8_t extra[4];         // signed (room) posts: the author's key prefix
  bool hasExtra;
  char text[MAX_FRAME];
};

inline bool parseMsg(const uint8_t* f, int len, Msg& m) {
  memset(&m, 0, sizeof m);
  int i = 1;
  if (f[0] == CONTACT_MSG_RECV_V3) {
    if (len < 4 + 6 + 2 + 4) return false;
    m.snr_x4 = (int8_t)f[i];
    i += 3;
    memcpy(m.keyPrefix, f + i, 6); i += 6;
  } else if (f[0] == CHANNEL_MSG_RECV_V3) {
    if (len < 4 + 1 + 2 + 4) return false;
    m.channel = true;
    m.snr_x4 = (int8_t)f[i];
    i += 3;
    m.channelIdx = f[i++];
  } else {
    return false;
  }
  m.pathLen = f[i++];
  m.txtType = f[i++];
  m.ts = rd32(f + i); i += 4;
  if (!m.channel && m.txtType == TXT_SIGNED && len >= i + 4) {
    memcpy(m.extra, f + i, 4);
    m.hasExtra = true;
    i += 4;
  }
  int n = len - i;
  if (n < 0) n = 0;
  if (n > (int)sizeof m.text - 1) n = sizeof m.text - 1;
  memcpy(m.text, f + i, n);
  m.text[n] = 0;
  return true;
}

// RESP_CODE_SENT: the ACK (or request tag) to expect, and how long to wait.
struct Sent { bool flood; uint32_t tag; uint32_t timeoutMs; };
inline bool parseSent(const uint8_t* f, int len, Sent& s) {
  if (len < 10 || f[0] != SENT) return false;
  s.flood = f[1];
  s.tag = rd32(f + 2);
  s.timeoutMs = rd32(f + 6);
  return true;
}

// ---- command encoders: each writes a frame into f and returns its length.

inline int encDeviceQuery(uint8_t* f) { f[0] = DEVICE_QUERY; f[1] = APP_PROTOCOL; return 2; }

inline int encAppStart(uint8_t* f, const char* appName) {
  int i = 0;
  f[i++] = APP_START;
  f[i++] = 1;                  // app version (reserved bytes 1..7)
  memset(f + i, 0, 6); i += 6;
  int n = (int)strlen(appName);
  if (n > 20) n = 20;
  memcpy(f + i, appName, n);
  return i + n;
}

inline int encGetContacts(uint8_t* f, uint32_t since) {
  f[0] = GET_CONTACTS;
  if (!since) return 1;
  wr32(f + 1, since);
  return 5;
}

inline int encKeyCmd(uint8_t* f, uint8_t cmd, const uint8_t* key) {
  f[0] = cmd;
  memcpy(f + 1, key, KEY);
  return 1 + KEY;
}

inline int encSendText(uint8_t* f, uint8_t txtType, uint8_t attempt, uint32_t ts, const uint8_t* keyPrefix,
                       const char* text) {
  int i = 0;
  f[i++] = SEND_TXT_MSG;
  f[i++] = txtType;
  f[i++] = attempt;
  wr32(f + i, ts); i += 4;
  memcpy(f + i, keyPrefix, 6); i += 6;
  int n = (int)strlen(text);
  if (n > MAX_FRAME - i) n = MAX_FRAME - i;
  memcpy(f + i, text, n);
  return i + n;
}

inline int encSendChannel(uint8_t* f, uint8_t idx, uint32_t ts, const char* text) {
  int i = 0;
  f[i++] = SEND_CHANNEL_TXT_MSG;
  f[i++] = TXT_PLAIN;
  f[i++] = idx;
  wr32(f + i, ts); i += 4;
  int n = (int)strlen(text);
  if (n > MAX_FRAME - i) n = MAX_FRAME - i;
  memcpy(f + i, text, n);
  return i + n;
}

inline int encGetChannel(uint8_t* f, uint8_t idx) { f[0] = GET_CHANNEL; f[1] = idx; return 2; }

inline int encSetChannel(uint8_t* f, uint8_t idx, const char* name, const uint8_t secret[16]) {
  f[0] = SET_CHANNEL;
  f[1] = idx;
  memset(f + 2, 0, 32);
  strncpy((char*)f + 2, name, 31);
  memcpy(f + 34, secret, 16);
  return 50;
}

inline int encLogin(uint8_t* f, const uint8_t* key, const char* password) {
  int i = encKeyCmd(f, SEND_LOGIN, key);
  int n = (int)strlen(password);
  if (n > 15) n = 15;
  memcpy(f + i, password, n);
  return i + n;
}

inline int encBinaryReq(uint8_t* f, const uint8_t* key, const uint8_t* data, int n) {
  int i = encKeyCmd(f, SEND_BINARY_REQ, key);
  memcpy(f + i, data, n);
  return i + n;
}

inline int encTelemetryReq(uint8_t* f, const uint8_t* key) {
  f[0] = SEND_TELEMETRY_REQ;
  f[1] = f[2] = f[3] = 0;   // reserved
  memcpy(f + 4, key, KEY);
  return 4 + KEY;
}

inline int encPathDiscovery(uint8_t* f, const uint8_t* key) {
  f[0] = SEND_PATH_DISCOVERY_REQ;
  f[1] = 0;
  memcpy(f + 2, key, KEY);
  return 2 + KEY;
}

inline int encTrace(uint8_t* f, uint32_t tag, uint32_t auth, uint8_t flags, const uint8_t* path, int n) {
  f[0] = SEND_TRACE_PATH;
  wr32(f + 1, tag);
  wr32(f + 5, auth);
  f[9] = flags;
  memcpy(f + 10, path, n);
  return 10 + n;
}

inline int encRadio(uint8_t* f, uint32_t freqKHz, uint32_t bwHz, uint8_t sf, uint8_t cr, bool repeat) {
  f[0] = SET_RADIO_PARAMS;
  wr32(f + 1, freqKHz);
  wr32(f + 5, bwHz);
  f[9] = sf;
  f[10] = cr;
  f[11] = repeat;
  return 12;
}

inline int encOtherParams(uint8_t* f, uint8_t manualAdd, uint8_t telemModes, uint8_t advertLocPolicy, uint8_t multiAcks) {
  f[0] = SET_OTHER_PARAMS;
  f[1] = manualAdd;
  f[2] = telemModes;
  f[3] = advertLocPolicy;
  f[4] = multiAcks;
  return 5;
}

inline int encU8(uint8_t* f, uint8_t cmd, uint8_t v) { f[0] = cmd; f[1] = v; return 2; }
inline int encU32(uint8_t* f, uint8_t cmd, uint32_t v) { f[0] = cmd; wr32(f + 1, v); return 5; }

inline int encLatLon(uint8_t* f, int32_t lat, int32_t lon) {
  f[0] = SET_ADVERT_LATLON;
  wr32(f + 1, (uint32_t)lat);
  wr32(f + 5, (uint32_t)lon);
  return 9;
}

inline int encName(uint8_t* f, const char* name) {
  f[0] = SET_ADVERT_NAME;
  int n = (int)strlen(name);
  if (n > 31) n = 31;
  memcpy(f + 1, name, n);
  return 1 + n;
}

inline int encText(uint8_t* f, uint8_t cmd, const char* word) {   // CMD_REBOOT "reboot", CMD_FACTORY_RESET "reset"
  f[0] = cmd;
  int n = (int)strlen(word);
  memcpy(f + 1, word, n);
  return 1 + n;
}

inline int encBytes(uint8_t* f, uint8_t cmd, const uint8_t* data, int n) {
  f[0] = cmd;
  memcpy(f + 1, data, n);
  return 1 + n;
}

}  // namespace cp
}  // namespace app
