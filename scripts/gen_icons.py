#!/usr/bin/env python3
"""Rasterize the navigation icons from Material Symbols to 1bpp C arrays.

Reads the pinned SVGs (scripts/assets.lock, via fetch_assets.py), fills their
paths with a small built-in rasterizer (nonzero winding, 8x8 supersampling),
thresholds the coverage to one bit per pixel and writes src/ui/icons_nav.h.
The output is committed, so firmware builds never need the network:

  pixi run icons

Material Symbols are Apache-2.0, (c) Google; the SVGs are converted here to
bitmaps and otherwise unmodified (see NOTICE).
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fetch_assets  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "src", "ui", "icons_nav.h")

# Shortcut order (screens::SC_*): Chats, Contacts, Nodes, Channels, Menu.
# Fields: name, SVG, threshold nudge for icons that clog at small sizes, and a
# window override for icons whose outline leaves Material's 20-unit live area.
# (Nodes is `lan`, not `hub`: hub's three small rings fill in to lopsided
# blobs at 16-20 px, however they are positioned.)
ICONS = [
    ("chats", "ms_chat_bubble.svg", 0.0, None),
    ("contacts", "ms_person.svg", 0.0, None),
    ("nodes", "ms_lan.svg", 0.0, None),
    ("channels", "ms_tag.svg", 0.0, None),
    ("menu", "ms_tune.svg", 0.0, None),
]
# Splash screen: a link and a keyboard after the name, at the title's height
# (24 px, beside the 24-row title font).
SPLASH = [
    ("link", "ms_sync_alt.svg", 0.0, None),
    ("keyboard", "ms_keyboard.svg", 0.0, None),
]
SIZES = (16, 20)
SS = 8  # supersampling per axis
# Material's live area is 20 of the 24 units, centred; scale that to the cell.
VIEW = (80.0, -880.0, 800.0)  # x0, y0, extent (in the 960-unit grid)
# Coverage needed to set a pixel. Below 0.5 thickens thin strokes so a 1.7 px
# stroke does not flicker between one and two pixels along a curve.
THRESHOLD = {16: 0.42, 20: 0.45}


def parse_path(d):
    """Flatten an SVG path (M L H V Q T Z, absolute or relative) to polygons."""
    toks = re.findall(r"[A-Za-z]|-?\d*\.?\d+(?:e-?\d+)?", d)
    polys, cur = [], []
    x = y = sx = sy = 0.0
    qx = qy = None
    cmd = None
    i = 0

    def num():
        nonlocal i
        v = float(toks[i])
        i += 1
        return v

    while i < len(toks):
        if re.match(r"[A-Za-z]", toks[i]):
            cmd = toks[i]
            i += 1
            if cmd in "Zz":
                if cur:
                    polys.append(cur)
                cur, x, y = [], sx, sy
                qx = qy = None
                continue
        rel = cmd.islower()
        c = cmd.upper()
        ox, oy = (x, y) if rel else (0.0, 0.0)
        if c == "M":
            if cur:
                polys.append(cur)
            x, y = ox + num(), oy + num()
            sx, sy = x, y
            cur = [(x, y)]
            cmd = "l" if rel else "L"  # extra pairs are implicit lineto
            qx = qy = None
        elif c == "L":
            x, y = ox + num(), oy + num()
            cur.append((x, y))
            qx = qy = None
        elif c == "H":
            x = (x if rel else 0.0) + num()
            cur.append((x, y))
            qx = qy = None
        elif c == "V":
            y = (y if rel else 0.0) + num()
            cur.append((x, y))
            qx = qy = None
        elif c in "QT":
            if c == "Q":
                cx, cy = ox + num(), oy + num()
            else:  # reflect the previous control point
                cx, cy = (2 * x - qx, 2 * y - qy) if qx is not None else (x, y)
            ex, ey = ox + num(), oy + num()
            for k in range(1, 13):
                t = k / 12
                a, b, e = (1 - t) ** 2, 2 * t * (1 - t), t * t
                cur.append((a * x + b * cx + e * ex, a * y + b * cy + e * ey))
            x, y, qx, qy = ex, ey, cx, cy
        else:
            raise ValueError(f"unsupported path command {cmd!r}")
    if cur:
        polys.append(cur)
    return polys


def coverage(polys, size, view=VIEW):
    """size x size coverage in 0..1, nonzero winding."""
    x0, y0, ext = view
    n = size * SS
    k = n / ext
    edges = []
    for p in polys:
        for (ax, ay), (bx, by) in zip(p, p[1:] + p[:1]):
            ax, ay, bx, by = (ax - x0) * k, (ay - y0) * k, (bx - x0) * k, (by - y0) * k
            if ay != by:
                edges.append((ax, ay, bx, by))
    hits = [[0] * n for _ in range(n)]
    for sy in range(n):
        yc = sy + 0.5
        xs = []
        for ax, ay, bx, by in edges:
            if (ay <= yc < by) or (by <= yc < ay):
                xs.append((ax + (yc - ay) * (bx - ax) / (by - ay), 1 if by > ay else -1))
        xs.sort()
        wind = 0
        for (xa, w), (xb, _) in zip(xs, xs[1:]):
            wind += w
            if wind:
                lo, hi = max(0, int(xa + 0.5)), min(n, int(xb + 0.5))
                for sx in range(lo, hi):
                    hits[sy][sx] = 1
    cov = [[0.0] * size for _ in range(size)]
    for py in range(size):
        for px in range(size):
            s = sum(hits[py * SS + j][px * SS + i] for j in range(SS) for i in range(SS))
            cov[py][px] = s / (SS * SS)
    return cov


def rows(cov, size, thr):
    stride = (size + 7) // 8
    out = []
    for r in cov:
        bits = 0
        for px in range(size):
            bits = (bits << 1) | (1 if r[px] >= thr else 0)
        bits <<= stride * 8 - size
        out.append(bits.to_bytes(stride, "big"))
    return out


def main():
    lines = [
        "// Generated by scripts/gen_icons.py from Material Symbols Rounded (outlined,",
        "// weight 400). Apache-2.0, (c) Google; converted to 1bpp bitmaps. See NOTICE.",
        "// 1bpp, MSB left, ceil(w/8) bytes per row. nav order is screens::SC_*.",
        "#pragma once",
        "#include <stdint.h>",
        "",
        "namespace ui { namespace icon {",
        "",
    ]
    preview = []
    for size in SIZES:
        stride = (size + 7) // 8
        lines.append(f"// {size}x{size}, {stride} byte(s) per row")
        names = []
        for name, svg, nudge, view in ICONS:
            with open(fetch_assets.get(svg)) as f:
                d = re.search(r'\sd="([^"]+)"', f.read()).group(1)
            cov = coverage(parse_path(d), size, view or VIEW)
            thr = THRESHOLD[size] + nudge
            data = rows(cov, size, thr)
            body = ",".join(f"0x{b:02X}" for row in data for b in row)
            lines.append(f"static const uint8_t nav{size}_{name}[] = {{{body}}};")
            names.append(f"nav{size}_{name}")
            preview.append((f"{name} {size}", cov, thr))
        lines.append(f"static const uint8_t* const nav{size}[] = {{{', '.join(names)}}};")
        lines.append("")
    lines.append("// Splash, 24x24")
    for name, svg, nudge, view in SPLASH:
        with open(fetch_assets.get(svg)) as f:
            d = re.search(r'\sd="([^"]+)"', f.read()).group(1)
        cov = coverage(parse_path(d), 24, view or VIEW)
        thr = 0.45 + nudge
        body = ",".join(f"0x{b:02X}" for row in rows(cov, 24, thr) for b in row)
        lines.append(f"static const uint8_t splash24_{name}[] = {{{body}}};")
        preview.append((f"{name} splash24", cov, thr))
    lines.append("")
    lines.append("}}  // namespace ui::icon")
    with open(OUT, "w") as f:
        f.write("\n".join(lines) + "\n")
    print(f"wrote {os.path.relpath(OUT, ROOT)}")
    if "--ascii" in sys.argv:
        for label, cov, thr in preview:
            print(label)
            for r in cov:
                print("".join("#" if v >= thr else "." for v in r))


if __name__ == "__main__":
    main()
