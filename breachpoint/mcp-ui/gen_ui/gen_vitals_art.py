#!/usr/bin/env python3
"""Author the two BN vitals shapes UMG cannot draw, as PNG.

    python3 mcp-ui/gen_ui/gen_vitals_art.py

Import with:
    python3 mcp-ui/gen_ui/import_textures.py Content/BN/UI/Assets /Game/BN/UI/Assets

WHY PILLOW DIRECTLY AND NOT `svg_pillow`
----------------------------------------
`svg_pillow` fills solid only ("stroke (solid or linearGradient), fill (solid)"), and the
shield bar's whole character is a horizontal gradient. Rather than fake it with 273 hairline
strokes, these two shapes are rasterised straight — they are a rectangle and a trapezoid, so
there is no path grammar to lose.

EVERY NUMBER IS MEASURED, AND FROM TWO DIFFERENT PLACES ON PURPOSE
------------------------------------------------------------------
SHAPE is measured off the reference RENDER of `61:5` (SET Vitals / Shield + Health), because
`get_screenshot` composites the page ground into the PNG and so cannot be trusted for alpha or
colour — but it is exact about where ink stops (mcp-ui/GOTCHAS.md #9). Row scan of that render,
at its native 277x35:

    shield   y1..y19   x1..x275   full width every row  -> a RECTANGLE, no taper
    black    y20       x0..x276   a 1px separator, wider than the band
    health   y21..y25  x1..274, 3..273, 5..271, 6..270, 8..268  -> a TRAPEZOID, ~7px chamfer
    tick     y26..y30  x137..138  -> UBRRule already draws this; no art needed

The ticket calls the shield "an ARC (constant thickness 16, sag 2.7)". At 273 wide a 2.7 sag is
a 1% deviation and does NOT resolve in the reference render — every shield row spans the full
width. Recorded rather than invented: the shape here is what the render shows.

PREFLIGHT SAYS "ink is coloured", AND IT IS OVERRULED HERE — DELIBERATELY
------------------------------------------------------------------------
`preflight_textures.py` rejects both of these with "icons must ship neutral and be tinted in
UMG", plus "background is baked in" and "filled plate, not a glyph". Per GOTCHAS #10 those are
judged, not obeyed: that rule is calibrated for PADDED GLYPHS, and these are full-bleed bars —
an opaque corner and a 100%-opaque body ARE the shape, not a baked page ground.

The colour objection is the real one and it is overruled for one reason: the shield's whole
character is a TWO-TOKEN gradient (`hud/shield-low` at the ends, `hud/self` at the centre), and
a flat `SetColorAndOpacity` tint cannot express two stops. Baking it is what makes this 1:1
with the reference. THE COST, STATED: a future team-colour or damage-state recolour of the
shield cannot be done with a tint — it needs this file re-run, or a material with two colour
parameters. The health bar is single-token and could ship neutral; it carries its colour only
so the two files stay one story.

The aliasing objection was NOT overruled — it was a real defect and is fixed above.

COLOUR comes from the token table (`6:20`), never from the render, for the same compositing
reason. The render's own health pixels read exactly #F5C542, which is `hud/health` — that
agreement is the check that the two sources are talking about the same thing.

The shield's ends-dark / centre-bright ramp is measured as linear and symmetric (luminance
122 at both ends rising to 206 at x141, the horizontal centre). The token table names both
stops outright: `hud/self #35D0F2` is "YOU: shields" and `hud/shield-low #0E7E9B` is
"Shield bar gradient floor".
"""
from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw

OUT = Path(__file__).resolve().parents[2] / "Content/BN/UI/Assets"

# The design box, from Figma `42:2` / `42:3` / `42:4`.
W, SHIELD_H, HEALTH_H = 273.33, 20.0, 5.0
# The health trapezoid's horizontal inset at its BOTTOM edge, measured off the render
# (left 1->8, right 274->268 across five rows) and averaged to one symmetric number.
HEALTH_CHAMFER = 7.0
# 2x: the project's DPI curve is ShortestSide/720, so a 1080p window draws this at 1.5x.
# Authoring at 1x would upscale and blur; 2x has headroom to 1440p before it does.
SS = 2

SHIELD_HI = (0x35, 0xD0, 0xF2)   # hud/self
SHIELD_LO = (0x0E, 0x7E, 0x9B)   # hud/shield-low
HEALTH = (0xF5, 0xC5, 0x42)      # hud/health


def _lerp(a, b, t):
    return tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(3))


def shield_bar() -> Image.Image:
    """A full-width rectangle carrying the symmetric ends-dark ramp, drawn column by column."""
    w, h = round(W * SS), round(SHIELD_H * SS)
    im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    px = im.load()
    mid = (w - 1) / 2.0
    for x in range(w):
        # 0 at either end, 1 at the centre — the profile the render measures as linear.
        t = 1.0 - abs(x - mid) / mid
        c = _lerp(SHIELD_LO, SHIELD_HI, t)
        for y in range(h):
            px[x, y] = (c[0], c[1], c[2], 255)
    return im


def health_bar() -> Image.Image:
    """The downward-tapering trapezoid. Flat `hud/health`; the render confirms no gradient.

    Drawn at AA x and box-filtered down: the chamfer is the only diagonal in either shape, and
    `ImageDraw.polygon` is 1-bit. `preflight_textures` rejected the first cut of this file for
    exactly that ("only 0 partial-alpha levels — export is 1-bit, will look jagged") and it was
    right — a 7px slope with hard pixel steps is visible at HUD scale.
    """
    AA = 8
    w, h = round(W * SS), round(HEALTH_H * SS)
    inset = HEALTH_CHAMFER * SS
    big = Image.new("RGBA", (w * AA, h * AA), (0, 0, 0, 0))
    ImageDraw.Draw(big).polygon(
        [(0, 0), (w * AA - 1, 0), ((w - inset) * AA - 1, h * AA - 1), (inset * AA, h * AA - 1)],
        fill=HEALTH + (255,))
    return big.resize((w, h), Image.LANCZOS)


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    for stem, im in (("BN_Vitals_ShieldBar", shield_bar()),
                     ("BN_Vitals_HealthBar", health_bar())):
        path = OUT / f"{stem}.png"
        im.save(path)
        print(f"  {stem:24} {im.size[0]}x{im.size[1]}  -> {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
