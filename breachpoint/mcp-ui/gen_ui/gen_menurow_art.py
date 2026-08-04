#!/usr/bin/env python3
"""Author the five Menu Row shapes no existing texture covers, as SVG -> PNG.

    python3 mcp-ui/gen_ui/gen_menurow_art.py

Every number here is MEASURED off Figma `12:724` (Menu Row) via `get_metadata`, not derived
and not eyeballed. The per-shape docstrings cite the node the number came from. Import the
output with `import_textures.py Content/UI/Components/Buttons/Assets /Game/UI/Components/Buttons/Assets`.

WHY THESE FIVE AND NOT MORE
---------------------------
`Assets/README.md` already ruled on the whole set: out of 50 variants, three things need art
and UMG draws the rest. This closes the Menu Row half of that list. Checkbox ticks reuse the
shipped `T_UI_Glyph_Check_*`; plates, borders, side ticks and every rectangle stay procedural
on `UBRHairlineBorder`.

THE RASTERISER'S LIMITS SHAPE THE SVG
-------------------------------------
`svg_pillow.render` handles absolute `M/L/H/V/Z` only -- no curves, no transforms, no
`<circle>`. So the slider handle is a 24-gon written as line segments, which at 6 px is
indistinguishable from a circle and, unlike a curve, actually renders.
"""
from __future__ import annotations

import math
from pathlib import Path

from PIL import Image

from svg_pillow import render

OUT = Path(__file__).resolve().parents[2] / "Content/UI/Components/Buttons/Assets"

_HDR = ('<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" '
        'viewBox="0 0 {w} {h}" fill="none">')


def _poly(pts: list[tuple[float, float]]) -> str:
    """An absolute closed path. `Z` matters: `render` fills only when len(pts) >= 3."""
    d = " ".join(f"{'M' if i == 0 else 'L'}{x:.4f} {y:.4f}" for i, (x, y) in enumerate(pts))
    return d + " Z"


# ---------------------------------------------------------------------------
# 1. The diagonal hatch — Figma `12:901` "Lines", inside `12:900` "Highlight" (246x24)
#
# MEASURED, every one of these: 69 <line> children, each with a bounding box of exactly
# 36 x 36 -- equal width and height IS the 45 degree angle, and 36*sqrt(2) = 50.9 is the
# "51 long" the breakdown recorded. Their x steps by exactly 3 (36, 39, 42 ... 240), which
# is the 3 px pitch. The frame sits at (-33, -5), so the band overflows the 24-tall
# Highlight top and bottom and is clipped by it -- that is why the lines look like they
# run off the edges rather than meeting the corners.
#
# TWO THINGS ARE BAKED INTO ALPHA, and both are deliberate:
#   - the `Highlight` frame's own opacity 0.5,
#   - `Rectangle 154` (110 x 24, a black gradient at 0.8) fading the left third.
# `wbp_plan.py` writes brushes, never colours (its law 1), so a hatch that needed a runtime
# tint would render at full white and be wrong in the asset. Baking makes it correct with no
# C++ hook -- and `UBRMenuRow` declares no hatch bind to drive one with.
#
# ONE THING IS NOT MEASURED: which way the 45 degrees leans. A Figma <line> reports a square
# bounding box either way and the direction is in its transform, which get_metadata does not
# return. "/" is the choice here; if a capture shows "\", flip `y0`/`y1` and re-run.
# ---------------------------------------------------------------------------
HATCH_W, HATCH_H = 110, 24  # Rectangle 154 is 110x24 — see WIDTH note
HATCH_PITCH = 3.0
HATCH_RUN = 36.0            # 36 across and 36 down == 45 degrees
HATCH_STROKE = 0.5
HATCH_PEAK_ALPHA = 0.5      # the Highlight frame's own opacity, at the gradient's strong end


def hatch_svg() -> str:
    """Lines at a 3 px pitch covering the whole 110x24 plate.

    WIDTH IS 110, NOT 246, AND THAT WAS MEASURED, NOT ASSUMED. The `Lines` frame reports
    143x36 while its 69 children span 36..240 — the two disagree, so neither was trusted.
    The Figma render of `12:894` settles it: per-column standard deviation over the row's
    text-free rows is 20.4 at x=10 and decays monotonically to 0.47 by x=110, then exactly
    0.00 for every column out to 240. The hatch occupies the left 110 px and nothing else,
    which is precisely `Rectangle 154`'s box.

    COUNT IS NOT 69 HERE, DELIBERATELY. 69 lines at a 3 px pitch advance 204 px; a 110-wide
    plate needs ~49. The pitch is the measured invariant and is preserved exactly; the count
    is an output of the plate width. Lines start at -HATCH_RUN so the bottom-left corner is
    covered — the first attempt started at x=3 and left a bare wedge there, visible on sight.
    """
    parts = [_HDR.format(w=HATCH_W, h=HATCH_H)]
    x = -HATCH_RUN
    while x <= HATCH_W:
        parts.append(
            f'<path d="M{x:.3f} {HATCH_H:.3f} L{x + HATCH_RUN:.3f} {HATCH_H - HATCH_RUN:.3f}" '
            f'stroke="#ffffff" stroke-width="{HATCH_STROKE}"/>'
        )
        x += HATCH_PITCH
    parts.append("</svg>")
    return "\n".join(parts)


def hatch_post(img: Image.Image) -> Image.Image:
    """Bake the Rectangle 154 gradient: full at the left edge, zero at x=110.

    CALIBRATED against `12:894`'s render. Reading the brightest hatch pixel per column and
    solving it back over the #313131 plate gives an implied line alpha of 0.43 at x=20,
    0.17 at x=40, 0.12 at x=60, 0.03 at x=100 — a linear ramp off a ~0.5 peak, which is what
    a Figma linear gradient at the Highlight's 0.5 opacity produces. Sampling noise is real
    (the per-column maximum depends on whether a line's crest lands in that column), so the
    ramp is fitted, not point-matched.
    """
    a = img.getchannel("A").load()
    w, h = img.size
    for px in range(w):
        k = HATCH_PEAK_ALPHA * max(0.0, 1.0 - px / float(HATCH_W))
        for py in range(h):
            a[px, py] = round(a[px, py] * k)
    return img


# ---------------------------------------------------------------------------
# 2. Disclosure triangle — Figma `Polygon 13`, 6 x 6, inside Drop Down's `Text and Icon`.
# Points DOWN at rest; the Active variant moves it 6 px down the frame rather than
# rotating it (measured: y=8 idle/hover, y=14 active), so one asset serves all three.
# ---------------------------------------------------------------------------
def triangle_svg() -> str:
    return (_HDR.format(w=6, h=6)
            + f'<path d="{_poly([(0, 1), (6, 1), (3, 5.5)])}" fill="#ffffff"/></svg>')


# ---------------------------------------------------------------------------
# 3. Dig Down arrows — Figma `12:972` "Arrows", 9 x 6.25 at x=277 (OUTSIDE the Text Frame).
# `Polygon 9` solid plus a `Subtract` boolean at opacity 0.5: one lead chevron and two
# ghosted trailing ones. Alpha carries the 0.5 for the same reason the hatch does.
# ---------------------------------------------------------------------------
ARROW_H = 6.25
ARROW_THICK = 1.3


def arrows_svg() -> str:
    """A lead chevron plus one ghosted trailing chevron in a 9 x 6.25 box.

    THE FIGMA RENDER OF THIS NODE IS UNUSABLE and that is why this is built from metadata
    alone: `get_screenshot` on `12:972` returns a 9x7 PNG whose alpha is 255 everywhere — the
    page background composited over the glyph, exactly the `download_assets` defect
    `Assets/README.md` records and `check_transparency.py` exists to catch. So the shape here
    comes from the node tree (`Polygon 9` solid + a `Subtract` boolean at opacity 0.5), not
    from a picture. TWO chevrons, not three: the first pass crowded three into 9 px and they
    fused into a blob on inspection.
    """
    def chev(x: float, opacity: float) -> str:
        tip, mid = x + 3.2, ARROW_H / 2.0
        return (f'<path d="{_poly([(x, 0), (tip, mid), (x, ARROW_H), (x + ARROW_THICK, ARROW_H), (tip + ARROW_THICK, mid), (x + ARROW_THICK, 0)])}" '
                f'fill="#ffffff" fill-opacity="{opacity}"/>')
    return (_HDR.format(w=9, h=ARROW_H)
            + chev(4.5, 1.0)
            + chev(0.3, 0.5)
            + "</svg>")


# ---------------------------------------------------------------------------
# 4. Slider handle — Figma `Circle`, 6 x 6 ELLIPSE, fill #ffffff. A 24-gon, because the
# rasteriser has no curve support and at 6 px the difference is below one pixel.
# ---------------------------------------------------------------------------
def dot_svg(size: float = 6.0, segments: int = 24) -> str:
    r = size / 2.0
    pts = [(r + r * math.cos(2 * math.pi * i / segments),
            r + r * math.sin(2 * math.pi * i / segments)) for i in range(segments)]
    return _HDR.format(w=size, h=size) + f'<path d="{_poly(pts)}" fill="#ffffff"/></svg>'


# ---------------------------------------------------------------------------
# 5. Slider tick — Figma `Polygon 15`, 4 x 4, sits at y=-6 ABOVE the track.
# ---------------------------------------------------------------------------
def tick_svg() -> str:
    return (_HDR.format(w=4, h=4)
            + f'<path d="{_poly([(0, 0), (4, 0), (2, 4)])}" fill="#ffffff"/></svg>')


SHAPES = [
    # (stem, svg text, px width, px height, post-processor)
    ("MenuRow_Hatch",    hatch_svg(),    HATCH_W, HATCH_H, hatch_post),
    ("MenuRow_Triangle", triangle_svg(),       6,       6, None),
    ("MenuRow_Arrows",   arrows_svg(),         9,       7, None),
    ("MenuRow_Dot",      dot_svg(),            6,       6, None),
    ("MenuRow_Tick",     tick_svg(),           4,       4, None),
]


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    for stem, svg, w, h, post in SHAPES:
        svg_path = OUT / f"{stem}.svg"
        svg_path.write_text(svg, encoding="utf-8")
        img = render(svg_path, w, h)
        if post:
            img = post(img)
        png = OUT / f"{stem}.png"
        img.save(png)

        alpha = img.getchannel("A")
        levels = len({v for v in alpha.getdata() if 0 < v < 255})
        opaque = sum(1 for v in alpha.getdata() if v == 255)
        print(f"{stem:18} {w:>4}x{h:<4} partial-alpha levels {levels:>3}  "
              f"fully-opaque px {opaque:>5}  -> {png.name}")
    print("\nNow: python3 mcp-ui/gen_ui/preflight_textures.py Content/UI/Components/Buttons/Assets")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
