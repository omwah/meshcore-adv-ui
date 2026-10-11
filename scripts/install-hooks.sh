#!/usr/bin/env bash
# Install the repository's git hooks (pre-commit PII check).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
hook="$(git -C "$ROOT" rev-parse --git-path hooks)/pre-commit"
printf '#!/bin/sh\nexec "%s" \n' 'scripts/pii-check.sh' > "$ROOT/$hook" 2>/dev/null || printf '#!/bin/sh\nexec scripts/pii-check.sh\n' > "$hook"
chmod +x "$ROOT/$hook" 2>/dev/null || chmod +x "$hook"
echo "installed $hook"
