#!/usr/bin/env bash
#
# Export a clean copy of the MeshCore submodule into .deps/meshcore and apply
# patches/meshcore/*.patch on top. The submodule itself is never modified, so
# updating MeshCore is: bump the submodule, re-run this, fix any patch that no
# longer applies (scripts/mkpatch.sh).
#
# Skips the work when neither the submodule commit nor any patch has changed
# since the last run (stamp in .deps/meshcore.stamp). Pass --force to redo.
#
# The apply/verify logic follows esp-vim's scripts/prepare-deps.sh (Apache-2.0):
# `patch` instead of `git apply` (which silently no-ops on paths outside the
# repo root), and a before/after hash so a no-op patch can't pass.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SUB="$ROOT/external/MeshCore"
DEST="$ROOT/.deps/meshcore"
PATCHES="$ROOT/patches/meshcore"
STAMP="$ROOT/.deps/meshcore.stamp"

die() { printf 'prepare-deps: %s\n' "$*" >&2; exit 1; }

[ -e "$SUB/.git" ] || die "submodule missing: run 'git submodule update --init'"
git -C "$ROOT" check-ignore -q .deps/ || die ".deps/ is not git-ignored; refusing"

shopt -s nullglob
patch_files=("$PATCHES"/*.patch)
want="$(git -C "$SUB" rev-parse HEAD) $(cat /dev/null "${patch_files[@]}" | sha256sum | cut -c1-16)"
if [ "${1:-}" != "--force" ] && [ -f "$STAMP" ] && [ "$(cat "$STAMP")" = "$want" ] && [ -d "$DEST" ]; then
    exit 0
fi

if [ -n "$(git -C "$SUB" status --porcelain)" ]; then
    die "external/MeshCore has local edits; patches must live in patches/meshcore"
fi

rm -rf "$DEST" "$STAMP"
mkdir -p "$DEST"
git -C "$SUB" archive --format=tar HEAD | tar -x -C "$DEST"
printf 'meshcore %s -> .deps/meshcore\n' "$(git -C "$SUB" describe --tags --always)"

patch_target_hash() {
    awk '/^\+\+\+ /{ p=$2; if (p == "/dev/null") next; sub(/^[^\/]*\//, "", p); print p }' "$2" \
        | sort -u | while IFS= read -r f; do
            if [ -f "$1/$f" ]; then sha256sum "$1/$f"; else printf 'absent %s\n' "$f"; fi
        done | sha256sum
}

for p in "${patch_files[@]}"; do
    before="$(patch_target_hash "$DEST" "$p")"
    patch -d "$DEST" -p1 --forward --silent --no-backup-if-mismatch < "$p" \
        || die "FAILED to apply $(basename "$p"); refresh it with scripts/mkpatch.sh"
    [ "$before" != "$(patch_target_hash "$DEST" "$p")" ] \
        || die "$(basename "$p") changed nothing (wrong paths or already applied)"
    printf '  applied %s\n' "$(basename "$p")"
done

printf '%s' "$want" > "$STAMP"
