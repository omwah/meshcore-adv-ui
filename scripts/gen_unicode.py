"""Rasterize fallback layers for the Spleen fonts (build-time only).

For each Spleen size this draws the requested code points from Noto Sans Mono
into a cell of the same width and height, on Spleen's own baseline, as 1bpp
bitmaps (hinted, no anti-aliasing), and writes a C table in the same format as
the Spleen fonts (ui/gfx/font.h). Code points Spleen already has at that size,
or Noto lacks, are skipped. The font file is read here and never shipped; only
the derived bitmaps are (see NOTICE and licenses/OFL-1.1-Noto.txt).

Needs Pillow (in the pixi environment). Driven by gen_fonts.py.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bdf2c  # noqa: E402

# Noto Sans Mono's advance is 0.6 em, so the cell width fixes the pixel size.
ADVANCE_EM = 0.6


def cmap(path):
    """The set of code points a TrueType font maps (cmap formats 4 and 12)."""
    d = open(path, "rb").read()
    n = struct.unpack(">H", d[4:6])[0]
    tabs = {d[12 + 16 * i:16 + 16 * i]: struct.unpack(">II", d[20 + 16 * i:28 + 16 * i]) for i in range(n)}
    off = tabs[b"cmap"][0]
    cps = set()
    for i in range(struct.unpack(">H", d[off + 2:off + 4])[0]):
        pid, eid, sub = struct.unpack(">HHI", d[off + 4 + 8 * i:off + 12 + 8 * i])
        if (pid, eid) not in ((3, 1), (3, 10), (0, 3), (0, 4)):
            continue
        t = off + sub
        fmt = struct.unpack(">H", d[t:t + 2])[0]
        if fmt == 4:
            segx2 = struct.unpack(">H", d[t + 6:t + 8])[0]
            ends = struct.unpack(f">{segx2 // 2}H", d[t + 14:t + 14 + segx2])
            starts = struct.unpack(f">{segx2 // 2}H", d[t + 16 + segx2:t + 16 + 2 * segx2])
            for s, e in zip(starts, ends):
                if s != 0xFFFF:
                    cps.update(range(s, e + 1))
        elif fmt == 12:
            ng = struct.unpack(">I", d[t + 12:t + 16])[0]
            for g in range(ng):
                s, e, _ = struct.unpack(">III", d[t + 16 + 12 * g:t + 28 + 12 * g])
                cps.update(range(s, e + 1))
    return cps


def replacement(w, h, base):
    """U+FFFD: a hollow box on the baseline with a dot in it, as wide as fits."""
    rows = [[0] * w for _ in range(h)]
    x0, x1 = (1, w - 2) if w >= 6 else (0, w - 1)
    height = max(5, round((base) * 0.72))
    y1 = base - 1
    y0 = max(0, y1 - height + 1)
    for x in range(x0, x1 + 1):
        rows[y0][x] = rows[y1][x] = 1
    for y in range(y0, y1 + 1):
        rows[y][x0] = rows[y][x1] = 1
    if x1 - x0 >= 4 and y1 - y0 >= 4:
        rows[(y0 + y1) // 2][(x0 + x1) // 2] = 1
    return rows


def pack(rows, w):
    bpr = (w + 7) // 8
    out = []
    for r in rows:
        v = 0
        for x in range(bpr * 8):
            v = (v << 1) | (r[x] if x < w else 0)
        out.append(v)
    return out


def layer(font_path, bdf_path, ranges, skip, out, symbol, header="ui/gfx/font.h", ctype="mc_font_t"):
    """Write `out` (a C file defining `symbol`) with the code points in `ranges`
    that are not in `skip` (what Spleen already has at this size); returns stats."""
    try:
        from PIL import Image, ImageDraw, ImageFont  # imported late: only layer builds need it
    except ImportError:
        sys.exit("gen_unicode: Pillow is needed to draw the Unicode layers; build through "
                 "`pixi run` (its pio has it), or set MC_UNICODE=none for Spleen only")

    (w, h, fx, fy), _ = bdf2c.parse(bdf_path)
    base = h + fy                          # first pixel row below the baseline
    size = int(w / ADVANCE_EM)
    font = ImageFont.truetype(font_path, size, layout_engine=ImageFont.Layout.BASIC)
    mapped = cmap(font_path)
    table, missing, clipped = [], [], []
    wanted = sorted({cp for lo, hi in ranges for cp in range(lo, hi + 1)} - set(skip))
    for cp in wanted:
        if cp not in mapped:
            missing.append(cp)
            continue
        pad = 2 * w
        im = Image.new("1", (w + 2 * pad, h), 0)
        d = ImageDraw.Draw(im)
        d.fontmode = "1"
        d.text((pad, base), chr(cp), font=font, fill=1, anchor="ls")
        bb = im.getbbox()
        if not bb:                          # blank (a space)
            table.append((cp, [0] * h))
            continue
        shift = pad
        if bb[2] - bb[0] <= w:              # fits: slide a glyph that leans out of the cell back in
            if bb[0] < pad:
                shift = bb[0]
            elif bb[2] > pad + w:
                shift = bb[2] - w
        else:
            clipped.append(cp)
        cell = im.crop((shift, 0, shift + w, h))
        rows = [[1 if cell.getpixel((x, y)) else 0 for x in range(w)] for y in range(h)]
        table.append((cp, pack(rows, w)))
    table.append((0xFFFD, pack(replacement(w, h, base), w)))
    table.sort()
    bpr = (w + 7) // 8
    bdf2c.write_c(out, os.path.basename(font_path), header, ctype, symbol, symbol, w, h, bpr, 0, table)
    runs = len(bdf2c.runs_of([cp for cp, _ in table]))
    return {"glyphs": len(table), "runs": runs, "bytes": len(table) * h * bpr + runs * 8,
            "missing": missing, "clipped": clipped, "pixel_size": size, "cell": (w, h)}
