#!/usr/bin/env python3
"""
Run every test/simnet/*.sim scenario against real MeshCore nodes (env:simnet)
and report pass/fail. A scenario with a "# both-modes" line runs twice: as
the local node, and as a remote node's screen over simulated Bluetooth
(simnet --remote; reported as <name>@remote). One with "# remote-only" runs
only that way. Each run gets a new data directory under
sim-data/runs/<timestamp>/ (git-ignored), so nothing is deleted between runs.

    pixi run simnet-test            # all scenarios
    python -I scripts/simnet_tests.py 01-dm-roundtrip
"""

import glob
import os
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROG = os.path.join(ROOT, ".pio", "build", "simnet", "program")


def main():
    want = sys.argv[1:]
    files = sorted(glob.glob(os.path.join(ROOT, "test", "simnet", "*.sim")))
    if want:
        files = [f for f in files if os.path.splitext(os.path.basename(f))[0] in want]
    run_dir = os.path.join(ROOT, "sim-data", "runs", time.strftime("%Y%m%d-%H%M%S"))
    runs = []
    for f in files:
        with open(f) as fh:
            text = fh.read()
        name = os.path.splitext(os.path.basename(f))[0]
        if "# remote-only" not in text:
            runs.append((f, name, []))
        if "# both-modes" in text or "# remote-only" in text:
            runs.append((f, name + "@remote", ["--remote"]))
    failed = 0
    for f, name, extra in runs:
        out = os.path.join(run_dir, name)
        os.makedirs(out, exist_ok=True)
        env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0")
        r = subprocess.run([PROG, "-s", f, "-o", out, "--data", os.path.join(out, "flash")] + extra,
                           capture_output=True, text=True, env=env, cwd=ROOT)
        errs = [l for l in r.stderr.splitlines() if not l.startswith("DEBUG")]
        ok = r.returncode == 0
        failed += not ok
        print(f"{'PASS' if ok else 'FAIL'}  {name}")
        if not ok:
            print("      " + "\n      ".join(errs[-8:]))
    print(f"{len(runs) - failed}/{len(runs)} passed; output in {os.path.relpath(run_dir, ROOT)}")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
