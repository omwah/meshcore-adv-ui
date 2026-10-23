#!/usr/bin/env python3
"""Check that every upstream asset the build uses has an allowed licence on file.

For each entry in scripts/assets.lock (and the vendored Spleen fonts) this
requires: a licence in ALLOWED below, the attribution named in NOTICE, and the
licence text in the repository, byte for byte what upstream publishes where the
lock pins it. It also refuses GNU Unifont and other GPL-licensed glyph sources
anywhere in the tracked tree. MeshMash stays MIT; fonts and icons keep their
own licences (see NOTICE).

An asset with no row in ASSETS fails the check, so adding one is a decision
about its licence, not an accident.
"""
import hashlib
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ALLOWED = {"Apache-2.0", "OFL-1.1", "BSD-2-Clause", "MIT"}

# lock entry prefix -> (licence, name that must appear in NOTICE, licence text in the repo,
#                       lock entry that holds that text verbatim, or None)
ASSETS = {
    "ms_": ("Apache-2.0", "Material Symbols", "licenses/Apache-2.0.txt", None),
    "noto_sans_mono": ("OFL-1.1", "Noto Sans Mono", "licenses/OFL-1.1-Noto.txt", "noto_ofl.txt"),
    "noto_ofl.txt": ("OFL-1.1", "Noto Sans Mono", "licenses/OFL-1.1-Noto.txt", "noto_ofl.txt"),
    "noto_color_emoji": ("OFL-1.1", "Noto Color Emoji", "licenses/OFL-1.1-NotoEmoji.txt", "noto_emoji_ofl.txt"),
    "noto_emoji_ofl.txt": ("OFL-1.1", "Noto Color Emoji", "licenses/OFL-1.1-NotoEmoji.txt", "noto_emoji_ofl.txt"),
}
# Vendored in the tree, not fetched.
VENDORED = [("fonts/spleen-*.bdf", "BSD-2-Clause", "Spleen", "fonts/LICENSE.spleen")]
FORBIDDEN = re.compile(r"unifont", re.I)


def read(path):
    with open(os.path.join(ROOT, path), "rb") as f:
        return f.read()


def main():
    bad = []
    notice = read("NOTICE").decode()
    lock = {}
    for line in read("scripts/assets.lock").decode().splitlines():
        line = line.split("#", 1)[0].strip()
        if line:
            name, sha, url = line.split(None, 2)
            lock[name] = (sha, url)

    for name, (sha, url) in lock.items():
        row = next((v for k, v in ASSETS.items() if name.startswith(k)), None)
        if row is None:
            bad.append(f"{name}: no licence recorded in scripts/check-licenses.py")
            continue
        lic, who, text, verbatim = row
        if lic not in ALLOWED:
            bad.append(f"{name}: licence {lic} is not allowed")
        if who not in notice:
            bad.append(f"{name}: NOTICE does not credit {who}")
        if not os.path.exists(os.path.join(ROOT, text)):
            bad.append(f"{name}: licence text {text} is missing")
        elif verbatim:
            if hashlib.sha256(read(text)).hexdigest() != lock[verbatim][0]:
                bad.append(f"{text} is not byte for byte what {verbatim} pins upstream")

    import glob
    for pattern, lic, who, text in VENDORED:
        if glob.glob(os.path.join(ROOT, pattern)):
            if lic not in ALLOWED:
                bad.append(f"{pattern}: licence {lic} is not allowed")
            if who not in notice:
                bad.append(f"{pattern}: NOTICE does not credit {who}")
            if not os.path.exists(os.path.join(ROOT, text)):
                bad.append(f"{pattern}: licence text {text} is missing")

    tracked = subprocess.check_output(["git", "-C", ROOT, "ls-files"], text=True).splitlines()
    for f in tracked:
        if f in ("scripts/check-licenses.py", "NOTICE", "docs/unicode.md"):   # these say what is refused
            continue
        if FORBIDDEN.search(f):
            bad.append(f"{f}: GNU Unifont is not used here")
            continue
        try:
            if FORBIDDEN.search(read(f).decode("utf-8", "ignore")):
                bad.append(f"{f}: mentions Unifont")
        except OSError:
            pass

    for b in dict.fromkeys(bad):                  # one report per problem, not per asset
        print("check-licenses:", b, file=sys.stderr)
    print(f"check-licenses: {len(lock)} pinned assets and {len(VENDORED)} vendored font set(s) "
          f"{'FAILED' if bad else 'ok'}")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
