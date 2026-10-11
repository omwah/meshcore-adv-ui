#!/usr/bin/env python3
"""
Run simulator scenarios and turn their frames into PNGs and a gallery.

    pixi run shots                       # every test/scenarios/*.sim, theme 0
    python -I scripts/screenshots.py --themes all tour

Writes shots/<scenario>/<theme>/<name>.png (3x, nearest neighbour), plus
shots/index.html showing them all. shots/ is git-ignored.
"""

import argparse
import glob
import html
import os
import subprocess
import sys
import tempfile

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SIM = os.path.join(ROOT, ".pio", "build", "sim", "program")
THEMES = ["Midnight", "Amber", "Phosphor", "Contrast", "Daylight", "Ocean"]


def run(scenario, theme, outdir):
    os.makedirs(outdir, exist_ok=True)
    with open(scenario) as f:
        body = f.read()
    # Fresh/empty must precede everything; theme goes right after them.
    lines = body.splitlines()
    head = [l for l in lines if l.strip() in ("fresh", "empty")]
    rest = [l for l in lines if l.strip() not in ("fresh", "empty")]
    script = "\n".join(head + [f"theme {theme}"] + rest) + "\n"
    with tempfile.TemporaryDirectory() as tmp:
        env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0")
        r = subprocess.run([SIM, "-o", tmp], input=script, text=True, capture_output=True, env=env)
        if r.returncode:
            sys.exit(f"sim failed on {scenario} (theme {theme}):\n{r.stderr}")
        names = []
        for ppm in sorted(glob.glob(os.path.join(tmp, "*.ppm"))):
            name = os.path.splitext(os.path.basename(ppm))[0]
            im = Image.open(ppm)
            im.resize((im.width * 3, im.height * 3), Image.NEAREST).save(os.path.join(outdir, name + ".png"))
            names.append(name)
    want = sum(1 for l in rest if l.strip().startswith("shot "))
    if len(names) != want:
        sys.exit(f"{scenario}: expected {want} shots, got {len(names)} (is {SIM} the simulator?)")
    return names


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("scenarios", nargs="*", help="scenario names (default: all)")
    ap.add_argument("--themes", default="0", help="comma list of theme numbers, or 'all'")
    ap.add_argument("--out", default=os.path.join(ROOT, "shots"))
    a = ap.parse_args()

    files = sorted(glob.glob(os.path.join(ROOT, "test", "scenarios", "*.sim")))
    if a.scenarios:
        files = [f for f in files if os.path.splitext(os.path.basename(f))[0] in a.scenarios]
    themes = range(len(THEMES)) if a.themes == "all" else [int(t) for t in a.themes.split(",")]

    sections = []
    for f in files:
        sc = os.path.splitext(os.path.basename(f))[0]
        for t in themes:
            d = os.path.join(a.out, sc, THEMES[t])
            names = run(f, t, d)
            sections.append((sc, THEMES[t], [os.path.relpath(os.path.join(d, n + ".png"), a.out) for n in names]))
            print(f"{sc} / {THEMES[t]}: {len(names)} shots")

    with open(os.path.join(a.out, "index.html"), "w") as o:
        o.write("<!doctype html><meta charset=utf-8><title>UI shots</title>"
                "<style>body{background:#222;color:#ddd;font:14px sans-serif}"
                "figure{display:inline-block;margin:8px}img{width:360px;image-rendering:pixelated}</style>")
        for sc, th, imgs in sections:
            o.write(f"<h2>{html.escape(sc)} &middot; {html.escape(th)}</h2>")
            for p in imgs:
                o.write(f"<figure><img src='{html.escape(p)}'><figcaption>{html.escape(os.path.basename(p))}</figcaption></figure>")


if __name__ == "__main__":
    main()
