#!/usr/bin/env python3
"""BN46 — the Roster Row icons, rendered from the Figma SVG exports.

    python3 Tools/bn/bn46_roster_icons.py       # writes Saved/FigmaExport/T_BN_Roster*.png

WHERE THE ART COMES FROM. `download_assets` on `12:39621` (Roster Row instance, working file
`yznvnVdOFDADaugZSeomfP`) returns 20 SVGs — the whole nameplate, truncated. Three of them are
the icons the row actually binds, identified by viewBox and path id, and committed beside this
script in `Tools/bn/figma/`:

    roster_mic_body.svg      17 x 14.0037   path id="Subtract"  the speaker
    roster_mic_strike.svg    18 x 16        path id="Strike"    the mute slash
    roster_rank_diamond.svg  19.8418 sq     path id="Vector"    the rank diamond RING

The individual icon components are NOT addressable on their own: `download_assets` takes only
`^\\d+[:-]\\d+$`, and the row's children are instance paths (`I12:39621;260:2395`). Asking Figma
for `260:2395` bare returns "node not found". So the route is: pull the row, keep the three.

ONLY THE DIAMOND SHIPS FROM HERE, AND THE MIC IS THE REASON — two rasteriser failures, one
loud and one quiet, both worth knowing about before you trust either tool:

  `mcp-ui/gen_ui/svg_pillow.py` on the gradient-FILLED diamond returns a FULLY EMPTY image
  (alpha 0..0, ONE level). It supports gradient STROKES and solid fills only, and it does not
  raise — a hole in its own stated contract ("raises on anything else rather than silently
  dropping it"). Ship that and the texture is blank; you find out on screen.

  The same rasteriser on `roster_mic_body.svg` returns 88 alpha levels — it looks like it
  worked — and the glyph is GARBAGE. The path is `fill-rule="evenodd"` over several cubic
  subpaths, and the module flattens every subpath into one polygon, so the speaker comes out as
  a scribble. That is the more dangerous failure of the two: blank announces itself, plausible
  does not. Rendered and eyeballed before believing the level count.

  macOS `qlmanage -t` is no fallback: it emits a fully opaque 256x256 with 57 dark pixels in it,
  i.e. essentially nothing, and bakes the white board into the alpha the way Figma's own PNG
  export of a symbol node does (the `bn43_carousel_dots.py` lesson, again).

So the diamond is drawn DIRECTLY from its path data — two nested polygons under the measured
vertical ramp, which is exact — and the three mic states keep the project's existing
`/Game/UI/Icons/Glyphs/T_UI_Glyph_{Mic,Speaking,Muted}_24`, which are the same icon family,
already correct, and already wired into `WBP_RosterRow`'s `MicSwitcher`. A mangled glyph pulled
from Figma is worse than a right one already in the repo.

The diamond keeps its measured gradient — `#AB6C32` at the top to `#E5BF76` at the bottom,
userSpaceOnUse, x constant — because that bronze-to-gold ramp IS the rank read, not a tint.
"""
from __future__ import annotations

import sys
from pathlib import Path

from PIL import Image, ImageDraw

REPO = Path(__file__).resolve().parents[2]
SVG = Path(__file__).resolve().parent / "figma"
OUT = REPO.parent / "Saved" / "FigmaExport"

sys.path.insert(0, str(REPO / "mcp-ui" / "gen_ui"))
from svg_pillow import render as svg_render  # noqa: E402

SS = 8       # supersample factor: the 26 design box is drawn at 208 px.
OUT_PX = 52  # 2x the 26x26 design slot, and 208 -> 52 is an EXACT 4:1 box reduction.
             # 26 itself was tried first and UE REFUSED it outright - "Import produced no
             # assets. The file format may not be supported or the file may be invalid." A
             # 26px edge is not a multiple of 4, and the importer rejects that rather than
             # padding. 52 is, and it also keeps the texture near its display size, which is
             # the bn43_carousel_dots lesson (a 1-design-px ring does not survive a big
             # non-integer reduction on screen).

# The row's measured slots, node `12:39621`: Microphone 16 x 18, Rank insignia 26 x 26.
MIC_BOX = (16, 18)
RANK_BOX = (26, 26)

# roster_rank_diamond.svg, path id="Vector" — outer diamond then the inner hole, verbatim.
D = 19.8418
OUTER = [(D, D / 2), (D / 2, D), (0.0, D / 2), (D / 2, 0.0)]
INNER = [(4.10547, 9.9209), (9.9209, 15.7363), (15.7363, 9.9209), (9.9209, 4.10547)]
GRAD_TOP = (0xAB, 0x6C, 0x32)
GRAD_BOTTOM = (0xE5, 0xBF, 0x76)


def _fit(art: Image.Image, box: tuple[int, int], scale: int) -> Image.Image:
    """Centre `art` on a transparent canvas of `box` * `scale`, preserving its own size."""
    canvas = Image.new("RGBA", (box[0] * scale, box[1] * scale), (0, 0, 0, 0))
    canvas.alpha_composite(art, ((canvas.width - art.width) // 2, (canvas.height - art.height) // 2))
    return canvas


def _whiten(img: Image.Image) -> Image.Image:
    """Keep coverage, throw away colour. See the module note on why white is the neutral."""
    _, _, _, alpha = img.split()
    return Image.merge("RGBA", (Image.new("L", img.size, 255),) * 3 + (alpha,))


def _mic(strike: bool) -> Image.Image:
    body = _whiten(svg_render(SVG / "roster_mic_body.svg", 17 * SS, round(14.0037 * SS)))
    art = _fit(body, MIC_BOX, SS)
    if strike:
        # The strike is authored on its own 18x16 board and overlaps the speaker; composite it
        # at the same centre so the slash crosses the cone exactly as the reference does.
        art.alpha_composite(_fit(_whiten(svg_render(SVG / "roster_mic_strike.svg", 18 * SS, 16 * SS)),
                                 MIC_BOX, SS))
    return art


def _diamond() -> Image.Image:
    """The rank ring: outer diamond minus inner diamond, under the measured vertical ramp."""
    n = round(D * SS)
    # Coverage first, as a mask — a polygon-minus-polygon is a mask operation, and doing it in
    # the mask keeps the gradient from being punched through by the hole's own antialiasing.
    mask = Image.new("L", (n, n), 0)
    md = ImageDraw.Draw(mask)
    md.polygon([(x * SS, y * SS) for x, y in OUTER], fill=255)
    md.polygon([(x * SS, y * SS) for x, y in INNER], fill=0)

    ramp = Image.new("RGBA", (n, n))
    rd = ImageDraw.Draw(ramp)
    for y in range(n):
        t = y / max(n - 1, 1)
        rd.line([(0, y), (n, y)], fill=tuple(
            round(a + (b - a) * t) for a, b in zip(GRAD_TOP, GRAD_BOTTOM)) + (255,))
    ramp.putalpha(mask)
    return _fit(ramp, RANK_BOX, SS)


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    work = [("T_BN_RosterRank_Diamond", _diamond, RANK_BOX)]

    for name, make, box in work:
        art = make().resize((OUT_PX, OUT_PX), Image.LANCZOS)
        alphas = [p[3] for p in art.getdata()]
        # The failure this guards is svg_pillow's silent gradient-fill drop: an all-zero alpha
        # channel is a blank texture, and a blank texture only announces itself on screen.
        assert max(alphas) > 0, f"{name}: rendered EMPTY - nothing was drawn"
        assert min(alphas) == 0, f"{name}: no transparent pixels - this is a filled rectangle"
        assert len(set(alphas)) >= 8, f"{name}: {len(set(alphas))} alpha levels - edge is 1-bit"
        path = OUT / f"{name}.png"
        art.save(path)
        print(f"{path}  {art.size}  alpha {min(alphas)}..{max(alphas)}  levels {len(set(alphas))}")


if __name__ == "__main__":
    main()
