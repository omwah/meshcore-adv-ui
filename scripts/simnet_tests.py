#!/usr/bin/env python3
"""
Run every test/simnet/*.sim scenario against real MeshCore nodes (env:simnet)
and report pass/fail. Each run gets a new data directory under
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
    failed = 0
    for f in files:
        name = os.path.splitext(os.path.basename(f))[0]
        out = os.path.join(run_dir, name)
        os.makedirs(out, exist_ok=True)
        env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0")
        r = subprocess.run([PROG, "-s", f, "-o", out, "--data", os.path.join(out, "flash")],
                           capture_output=True, text=True, env=env, cwd=ROOT)
        errs = [l for l in r.stderr.splitlines() if not l.startswith("DEBUG")]
        ok = r.returncode == 0
        failed += not ok
        print(f"{'PASS' if ok else 'FAIL'}  {name}")
        if not ok:
            print("      " + "\n      ".join(errs[-8:]))
    print(f"{len(files) - failed}/{len(files)} passed; output in {os.path.relpath(run_dir, ROOT)}")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
