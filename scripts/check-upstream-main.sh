#!/usr/bin/env bash
# boards/*/main.cpp mirror MeshCore's examples/companion_radio/main.cpp. When
# the submodule moves and that file changes, review our copies and update the
# recorded hash below.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
KNOWN="7f42ef11d148c48b"   # companion-v1.17.1
got="$(sha256sum "$ROOT/external/MeshCore/examples/companion_radio/main.cpp" | cut -c1-16)"
if [ "$got" != "$KNOWN" ]; then
  echo "upstream companion main.cpp changed ($got): review boards/*/main.cpp, then update KNOWN" >&2
  exit 1
fi
