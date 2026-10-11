#include "app/msg_store.h"
#include <string.h>
#include <stdio.h>

namespace app {

static const char* const kFile = "/ui/msgs.bin";
static const char* const kReadFile = "/ui/read.bin";

struct __attribute__((packed)) MessageStore::Rec {
  uint32_t seq;          // 0 = empty slot
  ThreadKey key;
  uint8_t flags;         // bit0 outgoing
  uint8_t status, retries, repeats;
  int8_t snr_x4, hops;
  uint32_t ts;
  uint32_t ack;
  char sender[24];
  char text[model::TEXT_LEN];
};

bool ThreadKey::operator==(const ThreadKey& o) const { return kind == o.kind && !memcmp(id, o.id, 6); }

ThreadKey ThreadKey::direct(const uint8_t* pk) {
  ThreadKey k{};
  k.kind = 0;
  memcpy(k.id, pk, 6);
  return k;
}

ThreadKey ThreadKey::channel(uint8_t slot) {
  ThreadKey k{};
  k.kind = 1;
  k.id[0] = slot;
  return k;
}

bool MessageStore::readRec(int slot, Rec& r) {
  File f = _fs->open(kFile, "r");
  if (!f || !f.seek((uint32_t)slot * sizeof(Rec))) return false;
  bool ok = f.read((uint8_t*)&r, sizeof r) == sizeof r;
  f.close();
  return ok;
}

bool MessageStore::writeRec(const Rec& r) {
  // Open read/write so the rest of the ring is kept; create on first use.
  File f = _fs->exists(kFile) ? _fs->open(kFile, "r+") : _fs->open(kFile, "w", true);
  if (!f) return false;
  uint32_t at = (uint32_t)(r.seq % CAPACITY) * sizeof(Rec);
  // SPIFFS can't seek past the end of a file: grow it with empty records
  // up to this slot first.
  size_t size = f.size();
  bool ok = true;
  if (at > size) {
    static const uint8_t zeros[64] = {};
    ok = f.seek(size);
    for (size_t left = at - size; ok && left;) {
      size_t n = left < sizeof zeros ? left : sizeof zeros;
      ok = f.write(zeros, n) == n;
      left -= n;
    }
  }
  ok = ok && f.seek(at) && f.write((const uint8_t*)&r, sizeof r) == sizeof r;
  f.close();
  return ok;
}

bool MessageStore::begin(fs::FS* fs) {
  _fs = fs;
  _nThreads = 0;
  _nextSeq = 1;
  _fs->mkdir("/ui");
  loadRead();
  File f = _fs->open(kFile, "r");
  if (f) {
    Rec r;
    while (f.read((uint8_t*)&r, sizeof r) == sizeof r) {
      if (!r.seq) continue;
      if (r.seq >= _nextSeq) _nextSeq = r.seq + 1;
      ThreadInfo* t = touch(r.key);
      if (!t) continue;
      if (!(r.flags & 1) && r.seq > t->readSeq) t->unread++;
      if (r.seq > t->lastSeq) {
        t->lastSeq = r.seq;
        t->lastTs = r.ts;
        t->lastOut = r.flags & 1;
        snprintf(t->preview, sizeof t->preview, "%s%s%s", r.sender, r.sender[0] ? ": " : "", r.text);
      }
    }
    f.close();
  }
  // Drop read markers of conversations whose messages were all overwritten.
  for (int i = 0; i < _nThreads;)
    if (!_threads[i].lastSeq) _threads[i] = _threads[--_nThreads];
    else i++;
  sortThreads();
  return true;
}

ThreadInfo* MessageStore::touch(const ThreadKey& k) {
  for (int i = 0; i < _nThreads; i++)
    if (_threads[i].key == k) return &_threads[i];
  if (_nThreads == MAX_THREADS) {
    // Evict the oldest conversation summary (its records stay on flash).
    int old = 0;
    for (int i = 1; i < _nThreads; i++)
      if (_threads[i].lastSeq < _threads[old].lastSeq) old = i;
    _threads[old] = _threads[--_nThreads];
  }
  ThreadInfo* t = &_threads[_nThreads++];
  memset(t, 0, sizeof *t);
  t->key = k;
  return t;
}

void MessageStore::sortThreads() {
  for (int i = 1; i < _nThreads; i++) {
    ThreadInfo v = _threads[i];
    int j = i - 1;
    while (j >= 0 && _threads[j].lastSeq < v.lastSeq) { _threads[j + 1] = _threads[j]; j--; }
    _threads[j + 1] = v;
  }
}

const ThreadInfo* MessageStore::thread(int i) const { return i >= 0 && i < _nThreads ? &_threads[i] : nullptr; }

const ThreadInfo* MessageStore::find(const ThreadKey& k) const {
  for (int i = 0; i < _nThreads; i++)
    if (_threads[i].key == k) return &_threads[i];
  return nullptr;
}

uint16_t MessageStore::unreadTotal() const {
  uint32_t n = 0;
  for (int i = 0; i < _nThreads; i++) n += _threads[i].unread;
  return n > 0xFFFF ? 0xFFFF : (uint16_t)n;
}

uint32_t MessageStore::append(const ThreadKey& k, const model::Message& m, uint32_t ack) {
  Rec r;
  memset(&r, 0, sizeof r);
  r.seq = _nextSeq++;
  r.key = k;
  r.flags = m.outgoing ? 1 : 0;
  r.status = (uint8_t)m.status;
  r.retries = m.retries;
  r.repeats = m.repeats;
  r.snr_x4 = m.snr_x4;
  r.hops = m.hops;
  r.ts = m.ts;
  r.ack = ack;
  snprintf(r.sender, sizeof r.sender, "%s", m.sender);
  snprintf(r.text, sizeof r.text, "%s", m.text);
  if (!writeRec(r)) return 0;

  ThreadInfo* t = touch(k);
  t->lastSeq = r.seq;
  t->lastTs = m.ts;
  t->lastOut = m.outgoing;
  if (m.outgoing) {
    t->readSeq = r.seq;   // replying marks the conversation read
    t->unread = 0;
  } else {
    t->unread++;
  }
  snprintf(t->preview, sizeof t->preview, "%s%s%s", m.sender, m.sender[0] ? ": " : "", m.text);
  sortThreads();

  if (_hasOpen && _open == k) {
    if (_nCache == CACHE) {
      memmove(_cache, _cache + 1, sizeof(_cache[0]) * (CACHE - 1));
      memmove(_cacheSeq, _cacheSeq + 1, sizeof(_cacheSeq[0]) * (CACHE - 1));
      _nCache--;
    }
    _cache[_nCache] = m;
    _cacheSeq[_nCache++] = r.seq;
  }
  return r.seq;
}

bool MessageStore::setStatus(uint32_t seq, model::Delivery st, uint8_t retries) {
  Rec r;
  if (!readRec(seq % CAPACITY, r) || r.seq != seq) return false;
  r.status = (uint8_t)st;
  r.retries = retries;
  for (int i = 0; i < _nCache; i++)
    if (_cacheSeq[i] == seq) { _cache[i].status = st; _cache[i].retries = retries; }
  return writeRec(r);
}

bool MessageStore::setRepeats(uint32_t seq, uint8_t repeats) {
  Rec r;
  if (!readRec(seq % CAPACITY, r) || r.seq != seq) return false;
  r.repeats = repeats;
  for (int i = 0; i < _nCache; i++)
    if (_cacheSeq[i] == seq) _cache[i].repeats = repeats;
  return writeRec(r);
}

bool MessageStore::text(uint32_t seq, char* out, int n) {
  Rec r;
  if (!readRec(seq % CAPACITY, r) || r.seq != seq) return false;
  snprintf(out, n, "%s", r.text);
  return true;
}

bool MessageStore::ackOf(uint32_t seq, uint32_t& ack) {
  Rec r;
  if (!readRec(seq % CAPACITY, r) || r.seq != seq) return false;
  ack = r.ack;
  return true;
}

void MessageStore::open(const ThreadKey& k) {
  _open = k;
  _hasOpen = true;
  _nCache = 0;
  File f = _fs->open(kFile, "r");
  if (!f) return;
  // Keep the newest CACHE records of this thread, in sequence order.
  Rec r;
  while (f.read((uint8_t*)&r, sizeof r) == sizeof r) {
    if (!r.seq || !(r.key == k)) continue;
    int pos = _nCache;
    if (_nCache == CACHE) {
      if (r.seq < _cacheSeq[0]) continue;
      memmove(_cache, _cache + 1, sizeof(_cache[0]) * (CACHE - 1));
      memmove(_cacheSeq, _cacheSeq + 1, sizeof(_cacheSeq[0]) * (CACHE - 1));
      pos = --_nCache;
    }
    while (pos > 0 && _cacheSeq[pos - 1] > r.seq) {
      _cache[pos] = _cache[pos - 1];
      _cacheSeq[pos] = _cacheSeq[pos - 1];
      pos--;
    }
    model::Message& m = _cache[pos];
    memset(&m, 0, sizeof m);
    m.ts = r.ts;
    m.outgoing = r.flags & 1;
    m.status = (model::Delivery)r.status;
    m.retries = r.retries;
    m.repeats = r.repeats;
    m.snr_x4 = r.snr_x4;
    m.hops = r.hops;
    snprintf(m.sender, sizeof m.sender, "%s", r.sender);
    snprintf(m.text, sizeof m.text, "%s", r.text);
    _cacheSeq[pos] = r.seq;
    _nCache++;
  }
  f.close();
}

void MessageStore::markRead(const ThreadKey& k) {
  for (int i = 0; i < _nThreads; i++) {
    ThreadInfo& t = _threads[i];
    if (!(t.key == k) || (!t.unread && t.readSeq == t.lastSeq)) continue;
    t.unread = 0;
    t.readSeq = t.lastSeq;
    saveRead();
  }
}

void MessageStore::forget(const ThreadKey& k) {
  for (int i = 0; i < _nThreads; i++)
    if (_threads[i].key == k) {
      memmove(&_threads[i], &_threads[i + 1], sizeof(ThreadInfo) * (_nThreads - i - 1));
      _nThreads--;
      saveRead();
      return;
    }
}

// Read positions: one (key, seq) pair per thread.
void MessageStore::saveRead() {
  File f = _fs->open(kReadFile, "w", true);
  if (!f) return;
  for (int i = 0; i < _nThreads; i++) {
    f.write((const uint8_t*)&_threads[i].key, sizeof(ThreadKey));
    f.write((const uint8_t*)&_threads[i].readSeq, 4);
  }
  f.close();
}

void MessageStore::loadRead() {
  File f = _fs->open(kReadFile, "r");
  if (!f) return;
  ThreadKey k;
  uint32_t seq;
  while (f.read((uint8_t*)&k, sizeof k) == sizeof k && f.read((uint8_t*)&seq, 4) == 4) {
    ThreadInfo* t = touch(k);
    if (t) t->readSeq = seq;
  }
  f.close();
}

void MessageStore::wipe() {
  _fs->remove(kFile);
  _fs->remove(kReadFile);
  _nThreads = 0;
  _nCache = 0;
  _nextSeq = 1;
}

}  // namespace app
