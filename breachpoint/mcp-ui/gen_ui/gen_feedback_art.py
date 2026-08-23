#!/usr/bin/env python3
"""Author the BN centre-screen feedback art the shipped set does not cover, as PNG.

    python3 mcp-ui/gen_ui/gen_feedback_art.py

Source: file `yznvnVdOFDADaugZSeomfP`, page `6:48` (HUD / Elements), read via the Figma
MCP 22 Aug 2026. Node numbers below all cite the child that carries them, not the parent
SET — re-read with `get_metadata(fileKey, nodeId)` to check any of them.

THREE OF SEVEN ASSIGNED NODES ARE DUPLICATES — NOT AUTHORED HERE (ASSET-RULES §1)
-----------------------------------------------------------------------------------
`get_metadata` + `download_assets` on all four "SET Reticle / *" nodes, then a pixel
diff against `Content/UI/HUD/HUD_Reticle_*.png` (composited on a dark ground, since raw
alpha can't be trusted per GOTCHAS #9), confirmed three are already shipped:

    62:12 "SET Reticle / Battle Rifle" (3 dots x{7,19.3,31.7} y19.3 r2 + tick x20.67 y8
          1.33x6)               == HUD_Reticle_BR.png, pixel-identical composite.
    62:17 "SET Reticle / Magnum"  (chev path + dot at 17,17)
                                  == HUD_Reticle_Magnum.png, pixel-identical composite.
    62:29 "SET Reticle / Enemy State" (4 arcs + centre cross)
                                  == HUD_Reticle_EnemyState.png, which is itself already
                                     byte-for-byte identical to HUD_Reticle_AR.png.

Re-authoring any of the three would be a redundant asset, which ASSET-RULES §1 calls a
finding, not initiative. Only `62:20` (Sniper) has no shipped counterpart.

A FOURTH NODE ISN'T AUTHORED TWICE, FOR THE SAME REASON
---------------------------------------------------------
`62:85` "SET Feedback / Hitmarker" and `62:90` "SET Feedback / Hitmarker Kill" have
IDENTICAL child geometry (four `rect x y 10x1.6 rotate(+-45 x y)` marks at the same four
pivots) — the only difference in the source is fill colour, white vs `#FF4A3D`
(`hud/threat`). Per this file's own output rule (white line art; colour is applied by
C++), baking two colours would make two BYTE-IDENTICAL white PNGs. One asset,
`BN_Feedback_Hitmarker.png`, backs BOTH states; C++ tints `hud/threat` for the kill case
via GameplayCue, same as every other reticle colour.

WHY THESE ARE DRAWN DIRECTLY AND NOT VIA `svg_pillow`
-------------------------------------------------------
Sniper (`62:20`) is 5 axis-aligned rects — no path grammar to lose, so it is built with
`ImageDraw.rectangle` straight from the SVG's own numbers, same posture as
`gen_vitals_art.py`'s bars.

Hitmarker (`62:85`) is 4 rects under `transform="rotate(deg cx cy)"`. `svg_pillow`
RAISES on transforms by design (GOTCHAS: a rasteriser that silently drops a transform
ships an asset with a missing edge). The four corners are rotated by hand with a
straight rotation matrix around each rect's own `(x,y)` pivot — the same "read the SVG's
own numbers, draw them with Pillow" posture `gen_reticle_art.py` uses for the circle
`svg_pillow` can't parse either.

Damage Direction (`62:95`) is a single CLOSED cubic path (ends `Z`) exported twice in the
source SVG — once as a `<mask>` fill, once as a masked stroke. That's Figma's "inside
stroke" export trick, and the two are pixel-redundant: the path already traces the full
band outline (outer curve out, a short `L` connector, inner curve back, `L` closes), so
FILLING that one closed path with nonzero winding reproduces the visible crescent exactly
— confirmed against `get_screenshot(62:95)`, which shows precisely this band in the
upper-left of the 120x120 frame. `svg_pillow` DOES flatten cubics (its docstring header
undersells this — see its own `_parse_path`, case `"C"`) but it also unconditionally
fills whatever it finds inside a `<mask>`, which would double-render the mask's copy as
an opaque white blob. So this one path is flattened and filled by hand instead, skipping
the second (masked, stroke) copy entirely.

`get_metadata` reports the Arc's own bounding box as the full 120x120 node, at x=0,y=0.
That is NOT the crop's placement offset — it is the underlying full ellipse's bbox,
which is confirmed by fitting a circle to the flattened points: both the outer curve
(60 points) and the inner curve (48 points) fit circles centred within 0.03 units of the
SAME point, radius 60.00 and 51.60 respectively (an 8.4-thick ring, exactly the earlier
eyeballed estimate) -- but that common centre sits at LOCAL (48.07, 59.97), not (60, 60).
Requiring it to land on (60, 60), the canvas centre implied by the 120x120 bbox, fixes
the crop's placement offset at (+11.93, +0.03) -- confirmed independently against
`get_screenshot(62:95)`, whose ink measures x:12-58, y:0-28 in the 120x120 render, a
match. `DD_OFFSET` below applies this before scaling. A rotation pivot for the runtime
"which side did the damage come from" spin is (60, 60), the canvas centre.

TWO EDGE-TOUCHING FINDINGS, BOTH JUDGED FALSE POSITIVES (GOTCHAS #10)
-------------------------------------------------------------------------
Sniper's four bars start/end at exactly 0 and 57.33 - the design box's own edges - by
construction (a "the crosshair reaches the frame" reticle, the same posture as
`BN_Reticle_Shotgun`'s full-bleed ring). A prior session already hit this: see
`mcp-ui/gen_ui/quarantine/HUD_Reticle_Sniper.txt` ("14 edge pixels at alpha>128 -- glyph
is clipped"). Recorded here as the same false positive, not fixed by insetting the art.

Hitmarker's rotated corners land at y=-0.00003 (M0/M1's outer tip) - floating-point zero,
i.e. the tip is drawn to touch the top edge deliberately. A prior session hit this too,
under the WRONG name: `mcp-ui/gen_ui/quarantine/HUD_Feedback_DamageDir.txt`
("2 edge pixels at alpha>128 -- glyph is clipped") holds an SVG whose own `id` reads
`"SET Feedback / Hitmarker"` and whose geometry is this exact shape - a mislabelled
extraction of what is really the hitmarker, not the damage-direction arc, quarantined
under the wrong asset's name.

WHITE, NOT RED. Every mark above ships as neutral line art; `hud/threat` is a C++ tint.
"""
from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageDraw

OUT = Path(__file__).resolve().parents[2] / "Content/BN/UI/Assets"
SS = 4  # supersample factor -- curves and rotated edges need real coverage values,
        # not a 1-bit rasterisation (preflight_textures MIN_AA_LEVELS).


def _canvas(box_w: float, box_h: float) -> tuple[Image.Image, ImageDraw.ImageDraw, float]:
    scale = SS
    im = Image.new("RGBA", (round(box_w * scale), round(box_h * scale)), (0, 0, 0, 0))
    return im, ImageDraw.Draw(im), scale


def _finish(im: Image.Image, final_w: int, final_h: int) -> Image.Image:
    return im.resize((final_w, final_h), Image.LANCZOS)


# ---------------------------------------------------------------------------------------
# 62:20 "SET Reticle / Sniper" -- 57.33 x 57.33, 5 axis-aligned rects, numbers from the SVG
# (`download_assets` svgAssets on 62:20): Bar1 y27.865 w14 h1.6 (x0) / Bar2 x43.33 (same
# y/w/h, ends at 57.33) / Bar3 x27.865 w1.6 h14 (y0) / Bar4 y43.33 (same x/w/h, ends
# 57.33) / Centre x27.5 y27.5 2.3x2.3. Bars deliberately touch both box edges (0 and
# 57.33) -- see the false-positive note above.
SNIPER_BOX = 57.33
SNIPER_RECTS = [
    (0.0, 27.865, 14.0, 1.6),      # Bar 1
    (43.33, 27.865, 14.0, 1.6),    # Bar 2
    (27.865, 0.0, 1.6, 14.0),      # Bar 3
    (27.865, 43.33, 1.6, 14.0),    # Bar 4
    (27.5, 27.5, 2.3, 2.3),        # Centre
]


def reticle_sniper() -> Image.Image:
    im, d, scale = _canvas(SNIPER_BOX, SNIPER_BOX)
    for x, y, w, h in SNIPER_RECTS:
        d.rectangle([x * scale, y * scale, (x + w) * scale, (y + h) * scale],
                    fill=(255, 255, 255, 255))
    final = round(SNIPER_BOX * 2)  # author at 2x the design box (DPI curve headroom)
    return _finish(im, final, final)


# ---------------------------------------------------------------------------------------
# 62:85 "SET Feedback / Hitmarker" (== 62:90 "Hitmarker Kill" geometry, see docstring).
# Box is 40 x 42 -- get_metadata reports 40x40, but the SVG's own viewBox is 40x42 and
# the flattened ink only reaches y=35.07, matching the 40x42 convention already recorded
# for this exact feedback family in preflight_textures.EXPECTED_EXACT (HUD_Feedback_
# DamageDir / ShieldBreak, both (40, 42)). Trusted over the metadata's rounder number.
HITMARKER_BOX = (40.0, 42.0)
# (x, y, w, h, deg, pivot_x, pivot_y) -- pivot is always the rect's own (x, y) corner,
# straight from the SVG's `rotate(deg cx cy)`.
HITMARKER_MARKS = [
    (6.0, 7.07104, 10.0, 1.6, -45.0, 6.0, 7.07104),                # M0
    (25.7976, 7.07104, 10.0, 1.6, -45.0, 25.7976, 7.07104),        # M1
    (6.0, 26.8687, 10.0, 1.6, 45.0, 6.0, 26.8687),                 # M2
    (25.7976, 26.8687, 10.0, 1.6, 45.0, 25.7976, 26.8687),         # M3
]


def _rotated_rect_corners(x, y, w, h, deg, cx, cy):
    a = math.radians(deg)
    ca, sa = math.cos(a), math.sin(a)
    corners = [(x, y), (x + w, y), (x + w, y + h), (x, y + h)]
    out = []
    for px, py in corners:
        dx, dy = px - cx, py - cy
        out.append((cx + dx * ca - dy * sa, cy + dx * sa + dy * ca))
    return out


def feedback_hitmarker() -> Image.Image:
    box_w, box_h = HITMARKER_BOX
    im, d, scale = _canvas(box_w, box_h)
    for x, y, w, h, deg, cx, cy in HITMARKER_MARKS:
        pts = [(px * scale, py * scale) for px, py in _rotated_rect_corners(x, y, w, h, deg, cx, cy)]
        d.polygon(pts, fill=(255, 255, 255, 255))
    final_w, final_h = round(box_w * 2), round(box_h * 2)
    return _finish(im, final_w, final_h)


# ---------------------------------------------------------------------------------------
# 62:95 "SET Feedback / Damage Direction" -- 120 x 120 anchor box, one closed cubic path.
# Control points transcribed verbatim from the SVG (`download_assets` svgAssets on
# 62:95); see the docstring for why this is filled directly instead of routed through
# svg_pillow's mask handling.
DAMAGE_DIR_BOX = 120.0
# The SVG crop's placement inside the 120x120 frame -- see the docstring's circle-fit.
DD_OFFSET = (11.93, 0.03)
_DD_START = (-6.57506e-08, 24.0661)
_DD_CUBICS = [
    (5.40909, 16.8252, 12.3841, 10.8999, 20.4043, 6.73267),
    (28.4244, 2.56542, 37.2823, 0.26391, 46.3166, -2.53113e-10),
]
_DD_LINE_1 = (46.5619, 8.39642)
_DD_CUBICS_BACK = [
    (38.7924, 8.62338, 31.1746, 10.6027, 24.2773, 14.1865),
    (17.38, 17.7704, 11.3814, 22.8661, 6.7296, 29.0933),
]
_DD_CLOSE = _DD_START


def _flatten_cubic(p0, c1, c2, p3, steps=24):
    x0, y0 = p0
    x1, y1 = c1
    x2, y2 = c2
    x3, y3 = p3
    pts = []
    for s in range(1, steps + 1):
        t = s / steps
        u = 1.0 - t
        bx = u**3 * x0 + 3 * u * u * t * x1 + 3 * u * t * t * x2 + t**3 * x3
        by = u**3 * y0 + 3 * u * u * t * y1 + 3 * u * t * t * y2 + t**3 * y3
        pts.append((bx, by))
    return pts


def _damage_direction_polygon():
    pts = [_DD_START]
    cur = _DD_START
    for c1x, c1y, c2x, c2y, ex, ey in _DD_CUBICS:
        seg = _flatten_cubic(cur, (c1x, c1y), (c2x, c2y), (ex, ey))
        pts.extend(seg)
        cur = (ex, ey)
    pts.append(_DD_LINE_1)
    cur = _DD_LINE_1
    for c1x, c1y, c2x, c2y, ex, ey in _DD_CUBICS_BACK:
        seg = _flatten_cubic(cur, (c1x, c1y), (c2x, c2y), (ex, ey))
        pts.extend(seg)
        cur = (ex, ey)
    pts.append(_DD_CLOSE)
    ox, oy = DD_OFFSET
    return [(px + ox, py + oy) for px, py in pts]


def feedback_damage_direction() -> Image.Image:
    im, d, scale = _canvas(DAMAGE_DIR_BOX, DAMAGE_DIR_BOX)
    pts = [(px * scale, py * scale) for px, py in _damage_direction_polygon()]
    d.polygon(pts, fill=(255, 255, 255, 255))
    final = round(DAMAGE_DIR_BOX * 2)
    return _finish(im, final, final)


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    assets = [
        ("BN_Reticle_Sniper", reticle_sniper()),
        ("BN_Feedback_Hitmarker", feedback_hitmarker()),
        ("BN_Feedback_DamageDirection", feedback_damage_direction()),
    ]
    for stem, im in assets:
        path = OUT / f"{stem}.png"
        im.save(path)
        a = im.getchannel("A")
        hist = a.histogram()
        w, h = im.size
        opaque = hist[255] / float(w * h)
        levels = len({p for p in a.getdata() if 0 < p < 255})
        print(f"  {stem:28} {w}x{h}  opaque={opaque:.3f}  partial-alpha levels={levels}  -> {path}")
    print("  BN_Feedback_HitmarkerKill  -- reuses BN_Feedback_Hitmarker.png (62:90 is the "
          "same geometry as 62:85, colour only; not authored separately)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
