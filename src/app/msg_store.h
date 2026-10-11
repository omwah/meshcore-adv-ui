// Message history on flash, for the UI (MeshCore keeps none for it).
//
// A fixed-size ring of records in one file: record i lives at slot
// seq % CAPACITY, so writes never grow the file and the oldest messages are
// overwritten. RAM holds only a summary per conversation (built by scanning
// the file at boot) and the messages of the conversation that is open.
#pragma once
#include <stdint.h>
#include <FS.h>
#include "model/model.h"

namespace app {

// Which conversation: a contact (by public key prefix) or a channel slot.
struct ThreadKey {
  uint8_t kind;          // 0 direct, 1 channel
  uint8_t id[6];
  bool operator==(const ThreadKey& o) const;
  static ThreadKey direct(const uint8_t* pubkey);
  static ThreadKey channel(uint8_t slot);
};

struct ThreadInfo {
  ThreadKey key;
  uint32_t lastTs, lastSeq, readSeq;
  uint16_t unread;
  bool lastOut;
  char preview[44];
};

class MessageStore {
public:
  static constexpr int CAPACITY = 512;     // records kept on flash (~108 KB)
  static constexpr int MAX_THREADS = 40;
  static constexpr int CACHE = 24;         // messages of the open conversation in RAM

  bool begin(fs::FS* fs);
  // Append a message; returns its sequence number (0 on failure).
  uint32_t append(const ThreadKey& k, const model::Message& m, uint32_t ack = 0);
  bool setStatus(uint32_t seq, model::Delivery st, uint8_t retries);
  bool setRepeats(uint32_t seq, uint8_t repeats);
  bool text(uint32_t seq, char* out, int n);
  bool ackOf(uint32_t seq, uint32_t& ack);

  int threadCount() const { return _nThreads; }
  const ThreadInfo* thread(int i) const;          // most recent first
  const ThreadInfo* find(const ThreadKey& k) const;
  void markRead(const ThreadKey& k);
  uint16_t unreadTotal() const;
  void forget(const ThreadKey& k);                // drop a thread (contact/channel removed)

  // The open conversation.
  void open(const ThreadKey& k);
  int count() const { return _nCache; }
  const model::Message& at(int i) const { return _cache[i]; }  // 0 = oldest cached

  void wipe();

private:
  struct Rec;
  bool readRec(int slot, Rec& r);
  bool writeRec(const Rec& r);
  ThreadInfo* touch(const ThreadKey& k);
  void sortThreads();
  void saveRead();
  void loadRead();

  fs::FS* _fs = nullptr;
  uint32_t _nextSeq = 1;
  ThreadInfo _threads[MAX_THREADS];
  int _nThreads = 0;
  ThreadKey _open{};
  bool _hasOpen = false;
  model::Message _cache[CACHE];
  uint32_t _cacheSeq[CACHE];
  int _nCache = 0;
};

}  // namespace app
