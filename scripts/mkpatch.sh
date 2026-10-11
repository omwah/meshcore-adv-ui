#!/usr/bin/env bash
#
# Write the difference between the pristine MeshCore submodule and the edited
# .deps/meshcore tree as ONE patch file:
#
#   scripts/mkpatch.sh 0003-ui-hooks        -> patches/meshcore/0003-ui-hooks.patch
#
# Workflow for a new change: run prepare-deps.sh, edit files under
# .deps/meshcore, then run this with a new number. The patch contains every
# difference not already covered by earlier patches, so make one change at a time.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SUB="$ROOT/external/MeshCore"
EDITED="$ROOT/.deps/meshcore"
PATCHES="$ROOT/patches/meshcore"
name="${1:?usage: mkpatch.sh NNNN-short-name}"
out="$PATCHES/$name.patch"

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/a"
git -C "$SUB" archive --format=tar HEAD | tar -x -C "$tmp/a"
# Baseline = pristine + all existing patches except the one being (re)written.
shopt -s nullglob
for p in "$PATCHES"/*.patch; do
    [ -e "$p" ] && [ "$p" != "$out" ] && patch -d "$tmp/a" -p1 --silent < "$p"
done
cp -a "$EDITED" "$tmp/b"

(cd "$tmp" && diff -ruN a b) > "$out.new" || true
[ -s "$out.new" ] || { rm -f "$out.new"; echo "no differences"; exit 1; }
mv "$out.new" "$out"
echo "wrote $out"
