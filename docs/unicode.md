# Unicode coverage

Text is drawn from Spleen first. A code point Spleen lacks falls through a
chain of fallback layers (`mc_font_t::next`, `src/ui/gfx/font.h`), and one that
no layer has shows a replacement box. A layer's glyph advance is its own cell
width, so layers of a different width (emoji, later) lay out correctly.

Layers are bitmaps generated at build time by `scripts/gen_fonts.py` and
`scripts/gen_unicode.py` into `src/gen/` (never committed). The firmware parses
no fonts and allocates nothing.

## Choosing coverage

A *profile* says which groups of code points each Spleen size gets. The
firmware environments pick one in `platformio.ini`:

| Environment | `custom_unicode` |
|---|---|
| `cardputer_adv*`, `sim`, `simnet*` | `min` |
| `tdeck*`, `sim_tdeck` | `std` |

Override it for one build with `MC_UNICODE`:

    MC_UNICODE=none pixi run build        # Spleen only, as before the layers
    MC_UNICODE=std,+math pixi run sim     # a profile plus a group
    MC_UNICODE=min,-greek pixi run sim    # ... or minus one
    MC_UNICODE=min,+0x2600-0x26ff pixi run sim   # ... or a raw range

or in `platformio.local.ini` (git-ignored):

    [env:cardputer_adv]
    custom_unicode = min,+arrows

Groups and profiles live in `scripts/unicode_profiles.py`; add to them there
rather than touching the renderer. `pixi run fonts` regenerates with the default
profile and `python -I scripts/gen_fonts.py --report` prints what each layer
costs (also left in `src/gen/unicode_report.txt`). Code points the source font
does not have are skipped and listed in the report.

Policy: emoji come first, and rarely used writing systems and symbols are
left out, most of all on the Cardputer, whose flash is tight.

## Sources and licences

| Layer | Source | Licence |
|---|---|---|
| Spleen | `fonts/spleen-*.bdf` | BSD 2-Clause |
| Text fallback | Noto Sans Mono 2.014 (Regular; Bold at 8x16) | SIL OFL 1.1 |

Upstream files are pinned by SHA-256 in `scripts/assets.lock`, fetched into the
git-ignored `.cache/assets/` by `scripts/fetch_assets.py`, and not shipped.
The compiled bitmaps are derived from them: see `NOTICE` and `licenses/`.
Noto Sans Mono's glyphs are drawn hinted and thresholded to one bit at the
Spleen cell size and baseline; Bold is used at 8x16 so strokes match Spleen's
two-pixel stems.

## Limits

No text shaping, no right-to-left layout, and combining marks are not
composed. Only the groups in a profile are drawn; everything else shows the
replacement box. Colour is the theme's: glyphs are one-bit.
