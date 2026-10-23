#!/usr/bin/env bash
#
# Time the text path on the host for a Unicode profile (default: each of none,
# min, std), optimised and without sanitizers:
#
#   scripts/bench-text.sh [none|min|std|SPEC ...]
#
# Output is in nanoseconds per call and per element. It regenerates src/gen/
# for the profile (the next pio build will regenerate it again), and needs the
# pixi environment's Python for the generator.
set -eu
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
profiles=("$@")
[ ${#profiles[@]} -gt 0 ] || profiles=(none min std)
for p in "${profiles[@]}"; do
    MC_UNICODE="$p" pixi run python -I scripts/gen_fonts.py
    rm -f "$OUT"/*.o
    for c in src/gen/*.c; do gcc -std=gnu11 -O2 -I src -c "$c" -o "$OUT/$(basename "$c").o"; done
    g++ -std=gnu++17 -O2 -I src -o "$OUT/bench" \
        scripts/bench_text.cpp src/ui/gfx/canvas.cpp src/ui/gfx/emoji.cpp src/ui/gfx/utf8.cpp src/ui/wrap.cpp \
        "$OUT"/*.o
    "$OUT/bench"
done
