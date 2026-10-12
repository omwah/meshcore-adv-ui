#include "app/remote/companion_client.h"
#include <string.h>

namespace app {

bool CompanionClient::queue(const uint8_t* frame, int len, uint32_t cookie) {
  if (_n >= QUEUE || len <= 0 || len > cp::MAX_FRAME) return false;
  Cmd& c = _q[_n++];
  memcpy(c.f, frame, len);
  c.len = (uint8_t)len;
  c.cookie = cookie;
  return true;
}

bool CompanionClient::pending(uint8_t cmd) const {
  for (int i = 0; i < _n; i++)
    if (_q[i].f[0] == cmd) return true;
  return false;
}

void CompanionClient::complete(const uint8_t* f, int len) {
  Cmd c = _q[0];
  memmove(_q, _q + 1, sizeof(Cmd) * (_n - 1));
  _n--;
  _inFlight = false;
  _h.onReply(c.f[0], c.cookie, f, len);   // may queue more
}

void CompanionClient::reset() {
  // Only what was queued now: a handler that queues again is reset next time.
  for (int n = _n; n > 0 && _n; n--) complete(nullptr, 0);
}

void CompanionClient::loop(uint32_t ms) {
  _now = ms;
  uint8_t f[cp::MAX_FRAME + 1];
  for (int n; (n = _link.recv(f)) > 0;) {
    if (cp::isPush(f[0])) {
      _h.onPush(f, n);
      continue;
    }
    if (!_inFlight) continue;   // a late reply to a command that timed out
    const Cmd& c = _q[0];
    // A contact list: the start and each contact keep the command open.
    if (c.f[0] == cp::GET_CONTACTS && (f[0] == cp::RC_CONTACTS_START || f[0] == cp::RC_CONTACT)) {
      _sentAt = ms;   // still coming
      _h.onReply(c.f[0], c.cookie, f, n);
      continue;
    }
    complete(f, n);
  }

  if (!_link.ready()) {
    if (_n) reset();
    return;
  }
  if (_inFlight && ms - _sentAt >= REPLY_TIMEOUT_MS) complete(nullptr, 0);
  if (!_inFlight && _n) {
    if (_link.send(_q[0].f, _q[0].len)) {
      _inFlight = true;
      _sentAt = ms;
    }
  }
}

}  // namespace app
