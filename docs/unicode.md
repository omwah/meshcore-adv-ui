# Unicode coverage

Text is drawn from Spleen first. A code point Spleen lacks falls through a
chain of fallback layers (`mc_font_t::next`, `src/ui/gfx/font.h`), and one that
no layer has shows a replacement box. A layer's glyph advance is its own cell
width, so layers of a different width lay out correctly. Colour emoji are
matched as whole sequences before any of that (below).

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

## Colour emoji

Emoji are drawn in their own colours, from Noto Color Emoji. The framebuffer
holds only theme palette indices (all 16 are theme roles), so an emoji is a
*sprite*: `Canvas::emoji()` records it and `Canvas::flush()` composites its
pixels over the row it belongs to. The rules that keep that correct:

- A sprite remembers the clip it was drawn under. Anything drawn over it later
  (`fillRect`, bitmaps) cuts it back to the largest part still showing, or
  removes it; `clear()` drops all of them. A frame is redrawn from `clear()`,
  so sprites never outlive it.
- A sprite is part of its rows' hash, so a row is pushed again when an emoji
  appears or goes even though the framebuffer behind it did not change.
- At most `MC_MAX_EMOJI` (48; 96 on the T-Deck) are placed per frame; the rest
  show the replacement glyph. That costs 20 bytes of RAM each.

Sequences are found by longest match over a sorted table shared by all sizes
(`emojiMatch`). U+FE0F and U+FE0E are ignored wherever they appear. A bare
character the text fonts draw (©, ™) stays text; it is the emoji only with a
selector. What does not match degrades without boxes:

| Text | Shown |
|---|---|
| Variation selectors, ZWJ, zero-width marks, tag characters, skin tone modifiers | nothing (they take no room) |
| A ZWJ sequence we lack | its parts, each as an emoji if we have it |
| A skin tone we lack | the base emoji |
| A flag we lack | its two letters (🇩🇪 → `DE`) |
| A keycap we lack | its digit |
| An emoji we lack | the replacement box |
| Any emoji at 5x8 (no art that small) | one replacement box per sequence |

Each emoji is two cells wide: 12x12 art at 6x12, 16x16 at 8x16, and 24x24 at
12x24 (on the Cardputer profile the same 12x12 art at 2x, to save flash). Art
is 4 bits a pixel with one shared 15-colour palette per size (index 0 is
transparent), chosen by k-means weighted so greens and blues are not lost to
the yellows that fill the faces.

Which emoji a build carries is `scripts/emoji_set.py`: `core` (everyday
reactions, signs, and what a mesh radio user talks about; the Cardputer) and
`common` (more of everything; the T-Deck takes it in order until its 150 KB of
bitmaps is spent). `MC_UNICODE=min,+emoji-common` adds a tier and `-emoji` drops
them all.

## Sources and licences

| Layer | Source | Licence |
|---|---|---|
| Spleen | `fonts/spleen-*.bdf` | BSD 2-Clause |
| Text fallback | Noto Sans Mono 2.014 (Regular; Bold at 8x16) | SIL OFL 1.1 |
| Colour emoji | Noto Color Emoji 2.057 (CBDT bitmaps) | SIL OFL 1.1 |

Upstream files are pinned by SHA-256 in `scripts/assets.lock`, fetched into the
git-ignored `.cache/assets/` by `scripts/fetch_assets.py`, and not shipped.
The compiled bitmaps are derived from them: see `NOTICE` and `licenses/`.
Noto Sans Mono's glyphs are drawn hinted and thresholded to one bit at the
Spleen cell size and baseline; Bold is used at 8x16 so strokes match Spleen's
two-pixel stems.

## Limits

No text shaping, no right-to-left layout, and combining marks are not
composed. Only the groups and emoji in a profile are drawn; everything else
shows the replacement box. Text glyphs are one-bit in the theme's colours;
only emoji carry their own. A chat screen with more emoji than `MC_MAX_EMOJI`
shows boxes for the extras. The line editor counts a character per code point,
so an emoji typed or pasted there is not laid out as two cells.
