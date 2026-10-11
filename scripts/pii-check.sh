#!/usr/bin/env bash
#
# Refuse to commit device- or person-identifying data: MAC addresses, long hex
# strings that look like MeshCore public/private keys, backup images, local
# PlatformIO overrides and serial port by-id paths. Patterns can be allowed
# per line with a trailing "pii-ok" comment (for example, fake test vectors).
#
#   scripts/pii-check.sh          # check staged files (pre-commit hook)
#   scripts/pii-check.sh --all    # check every tracked file

# No pipefail: `grep -q` stops reading early, and the writer's SIGPIPE must
# not count as a failure.
set -eu
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

if [ "${1:-}" = "--all" ]; then
    mapfile -t files < <(git ls-files)
    show() { cat -- "$1"; }
else
    mapfile -t files < <(git diff --cached --name-only --diff-filter=ACM)
    show() { git show ":$1"; }
fi

fail=0
bad() { printf 'pii-check: %s: %s\n' "$1" "$2" >&2; fail=1; }

for f in "${files[@]}"; do
    case "$f" in
        external/*|fonts/*.bdf|licenses/*|pixi.lock) continue ;;   # vendored / tool-generated
        scripts/pii-check.sh) continue ;;                           # its own patterns
        platformio.local.ini|*.backup.bin|sim-data/*|shots/*) bad "$f" "file type must never be committed"; continue ;;
    esac
    # Binary files: only allow known asset types.
    if ! show "$f" | grep -qI . 2>/dev/null; then
        case "$f" in *.png|*.gif|*.jpg) ;; *) bad "$f" "unexpected binary file" ;; esac
        continue
    fi
    hits="$(show "$f" | grep -nE \
        -e '([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}' \
        -e '\b[0-9A-Fa-f]{64}\b' \
        -e '/dev/serial/by-id/' \
        -e '/home/[a-z]' \
        | grep -v 'pii-ok' || true)"
    [ -z "$hits" ] || bad "$f" "possible PII:
$hits"
done

exit $fail
