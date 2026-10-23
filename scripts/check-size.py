#!/usr/bin/env python3
"""
Check the firmware images against size budgets (flash = app image, RAM =
static .data + .bss). Fails when a build grows past its budget, so growth is
always a deliberate decision. Run after `pixi run build`.
"""

import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOL = os.path.expanduser("~/.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-size")

# env: (max app bytes, max static RAM bytes)
BUDGETS = {
    # Bluetooth over NimBLE (2026-10-11): 1296 -> 915 KB and 127 -> 113 KB
    # static RAM against stock's Bluedroid. Room left for BLE client mode.
    "cardputer_adv": (1_000_000, 120_000),       # stock BLE companion is ~1.25 MB
    "cardputer_adv_usb": (740_000, 108_000),     # MeshCore 1.18 adds dynamic prefs/CLI (~7 KB over old cap)
    # T-Deck: a 6.25 MB app partition, so flash is no concern; internal RAM is
    # (the node and the UI are made in PSRAM at boot).
    "tdeck": (1_200_000, 80_000),
    "tdeck_usb": (900_000, 70_000),
}


def sizes(env):
    elf = os.path.join(ROOT, ".pio", "build", env, "firmware.elf")
    out = subprocess.check_output([TOOL, "-A", elf], text=True)
    sec = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 2 and parts[0].startswith("."):
            sec[parts[0]] = int(parts[1])
    ram = sum(v for k, v in sec.items() if k in (".dram0.data", ".dram0.bss", ".noinit"))
    flash = os.path.getsize(os.path.join(ROOT, ".pio", "build", env, "firmware.bin"))
    return flash, ram


def main():
    bad = 0
    for env, (max_flash, max_ram) in BUDGETS.items():
        try:
            flash, ram = sizes(env)
        except (OSError, subprocess.CalledProcessError) as e:
            print(f"{env}: not built ({e})")
            bad += 1
            continue
        ok = flash <= max_flash and ram <= max_ram
        bad += not ok
        print(f"{'ok  ' if ok else 'OVER'} {env:20} flash {flash/1024:7.1f} KB / {max_flash/1024:.0f}"
              f"   static RAM {ram/1024:6.1f} KB / {max_ram/1024:.0f}")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
