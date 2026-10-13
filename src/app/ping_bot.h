// The ping bot (local node mode): a direct message that is just "ping" or
// "test" gets a reply saying how it got here. Text only; the adapter
// decides when to answer and does the sending.
#pragma once
#include <stdint.h>

namespace app {

// The whole message is "ping" or "test", in any case, with any trailing
// punctuation or spaces ("Ping!", " test. ").
bool isPing(const char* text);

// Names a relay from its path hash (`size` bytes); false: none known for sure.
typedef bool (*RelayNamer)(void* ctx, const uint8_t* hash, int size, char* out, int n);

// The reply. `pathLen` is a MeshCore path_len (hash size and count; 0xFF:
// the route isn't known), `path` the relays' hashes from the sender's end,
// or null when only their number is known. `stored`: the message came by a
// direct route, so the path is our stored one, not one it was heard on.
// Returns the text's length.
int formatPong(char* out, int n, uint8_t pathLen, const uint8_t* path, bool stored, float snr, int rssi,
               RelayNamer namer, void* ctx);

}  // namespace app
