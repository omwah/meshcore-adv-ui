#!/usr/bin/env python3
"""Package firmware images for release (run after `pixi run build`).

For each device env this writes to dist/<version>/:
  meshcore-adv-ui-<version>-<env>.bin         application only: for M5Launcher
                                              (copy to the SD card) or OTA
  meshcore-adv-ui-<version>-<env>-full.bin    bootloader + partitions + app,
                                              flashed at 0x0: M5Burner, web
                                              flashers, esptool
  SHA256SUMS

    pixi run package                  # both envs
    python -I scripts/package.py cardputer_adv
"""
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ENVS = ["cardputer_adv", "cardputer_adv_usb", "tdeck", "tdeck_usb"]


def version():
    with open(os.path.join(ROOT, "platformio.ini")) as f:
        m = re.search(r'MC_UI_VERSION=\\"([^"\\]+)\\"', f.read())
    v = m.group(1) if m else "0.0.0"
    try:
        rev = subprocess.run(["git", "rev-parse", "--short", "HEAD"], cwd=ROOT, capture_output=True,
                             text=True, check=True).stdout.strip()
        dirty = subprocess.run(["git", "status", "--porcelain", "--untracked-files=no"], cwd=ROOT,
                               capture_output=True, text=True).stdout.strip()
        v += f"+{rev}{'.dirty' if dirty else ''}" if v.endswith("-dev") or dirty else ""
    except (OSError, subprocess.CalledProcessError):
        pass
    return v


def metadata(env):
    r = subprocess.run(["pio", "project", "metadata", "-e", env, "--json-output"], cwd=ROOT,
                       capture_output=True, text=True, check=True)
    # Output can start with tool-install chatter: the JSON is the last line.
    return json.loads(r.stdout.strip().splitlines()[-1])[env]


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 16), b""):
            h.update(block)
    return h.hexdigest()


def main():
    envs = sys.argv[1:] or ENVS
    ver = version()
    out = os.path.join(ROOT, "dist", ver)
    os.makedirs(out, exist_ok=True)
    made = []
    for env in envs:
        md = metadata(env)
        app = os.path.join(os.path.dirname(md["prog_path"]), "firmware.bin")
        if not os.path.exists(app):
            sys.exit(f"{env}: {os.path.relpath(app, ROOT)} missing; run `pixi run build` first")
        base = f"meshcore-adv-ui-{ver}-{env}"
        app_out = os.path.join(out, base + ".bin")
        shutil.copyfile(app, app_out)
        full_out = os.path.join(out, base + "-full.bin")
        extra = md["extra"]
        parts = []
        for img in extra["flash_images"]:
            parts += [img["offset"], img["path"]]
        parts += [extra["application_offset"], app]
        subprocess.run(["pio", "pkg", "exec", "-p", "tool-esptoolpy", "--", "esptool.py", "--chip", "esp32s3",
                        "merge_bin", "-o", full_out, "--flash_mode", "keep", "--flash_freq", "keep",
                        "--flash_size", "keep"] + parts,
                       cwd=ROOT, check=True, capture_output=True)
        made += [app_out, full_out]
    with open(os.path.join(out, "SHA256SUMS"), "w") as f:
        for p in sorted(made):
            f.write(f"{sha256(p)}  {os.path.basename(p)}\n")
    for p in made:
        print(f"{os.path.getsize(p) / 1024:8.1f} KB  {os.path.relpath(p, ROOT)}")


if __name__ == "__main__":
    main()
