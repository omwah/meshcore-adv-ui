// Talks to a companion node over a FrameLink: one command in flight at a
// time (the node answers in order, and its send queue is only a few frames
// deep), replies matched to the command that asked, pushes passed on as
// they come. Plain C++, no Arduino: the caller passes the time in.
#pragma once
#include <stdint.h>
#include "app/remote/companion_proto.h"
#include "app/remote/frame_link.h"

namespace app {

class CompanionClient {
public:
  class Handler {
  public:
    virtual ~Handler() {}
    // A reply to a command: `f` is the reply frame, or null if none came in
    // time (or the link went down). A contact list streams several replies
    // to one CMD_GET_CONTACTS; the last is END_OF_CONTACTS (or ERR).
    virtual void onReply(uint8_t cmd, uint32_t cookie, const uint8_t* f, int len) = 0;
    virtual void onPush(const uint8_t* f, int len) = 0;
  };

  static constexpr int QUEUE = 12;
  static constexpr uint32_t REPLY_TIMEOUT_MS = 5000;

  CompanionClient(FrameLink& link, Handler& handler) : _link(link), _h(handler) {}

  // Queue a command; `cookie` comes back with its reply. False: queue full.
  bool queue(const uint8_t* frame, int len, uint32_t cookie = 0);
  // Whether a command with this code is queued or in flight.
  bool pending(uint8_t cmd) const;
  int queued() const { return _n; }
  bool idle() const { return _n == 0; }
  // Run: read frames, time out, send the next command.
  void loop(uint32_t ms);
  // The link went down: every queued command fails (onReply with null).
  void reset();

private:
  struct Cmd {
    uint8_t f[cp::MAX_FRAME];
    uint8_t len;
    uint32_t cookie;
  };
  void complete(const uint8_t* f, int len);

  FrameLink& _link;
  Handler& _h;
  Cmd _q[QUEUE];
  int _n = 0;                   // _q[0] is in flight when _inFlight
  bool _inFlight = false;
  uint32_t _sentAt = 0, _now = 0;
};

}  // namespace app
