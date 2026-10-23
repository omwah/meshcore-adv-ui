#!/usr/bin/env python3
"""Fetch pinned upstream assets (icons, fonts) into a git-ignored cache.

Everything is listed in scripts/assets.lock as `name  sha256  url` lines. A
file is downloaded only when missing from .cache/assets/ and is always checked
against its SHA-256, so a regenerated asset is reproducible and a changed
upstream is noticed. The cache is never committed (see scripts/pii-check.sh).

  python -I scripts/fetch_assets.py [NAME ...]    # fetch all, or just these
  python -I scripts/fetch_assets.py --update NAME # print a fresh lock line
"""
import hashlib
import os
import sys
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOCK = os.path.join(ROOT, "scripts", "assets.lock")
CACHE = os.path.join(ROOT, ".cache", "assets")


def read_lock():
    out = {}
    with open(LOCK) as f:
        for line in f:
            line = line.split("#", 1)[0].strip()
            if not line:
                continue
            name, sha, url = line.split(None, 2)
            out[name] = (sha, url)
    return out


def download(url):
    with urllib.request.urlopen(url, timeout=60) as r:
        return r.read()


def fetch(name, entry):
    sha, url = entry
    path = os.path.join(CACHE, name)
    if os.path.exists(path):
        with open(path, "rb") as f:
            if hashlib.sha256(f.read()).hexdigest() == sha:
                return path
    data = download(url)
    got = hashlib.sha256(data).hexdigest()
    if got != sha:
        sys.exit(f"{name}: sha256 mismatch\n  want {sha}\n  got  {got}\n  url  {url}")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)
    return path


def get(name):
    """Path of the verified cached asset `name`, downloading it if needed."""
    lock = read_lock()
    if name not in lock:
        sys.exit(f"{name}: not in scripts/assets.lock")
    return fetch(name, lock[name])


def main(argv):
    if argv[:1] == ["--update"]:
        lock = read_lock()
        for name in argv[1:]:
            url = lock[name][1]
            print(f"{name}  {hashlib.sha256(download(url)).hexdigest()}  {url}")
        return
    lock = read_lock()
    for name in argv or lock:
        print(fetch(name, lock[name]))


if __name__ == "__main__":
    main(sys.argv[1:])
