#!/usr/bin/env python3
"""Author the BN chrome-and-identity shapes UMG cannot draw, as PNG.

    python3 mcp-ui/gen_ui/gen_chrome_art.py

Import with:
    python3 mcp-ui/gen_ui/import_textures.py Content/BN/UI/Assets /Game/BN/UI/Assets

SCOPE: Figma `yznvnVdOFDADaugZSeomfP`, page `6:48` (HUD / Elements) and page `6:49`
(HUD / Scoreboard) inside the MEASURED frame `43:2`. Chrome-and-identity lane only —
mode icons, team emblems, medals, tracker, world markers. Every number below is read
straight off `download_assets`'s `svgAssets` for the cited node, never eyeballed.

WHY EVERY RING IS DRAWN DIRECTLY, NOT THROUGH `svg_pillow`
------------------------------------------------------------
Every ring/ellipse in this set (`24:2`, `62:54`/`62:65`, `62:79`, `43:5`/`43:6`/`43:7`,
`62:101`) comes back from Figma as a `<circle>` or an `<ellipse>`-shaped bezier `<path>`.
`svg_pillow.render` raises on both by design (absolute M/L/H/V/Z only, GOTCHAS.md #2 in
`gen_reticle_art.py`'s own header) -- so, per that file's precedent, these are drawn
straight from the SVG's own numbers with `ImageDraw.ellipse`.

WHY THE MEDAL CHIP AND NAMEPLATE STAR GO THROUGH `svg_pillow` INSTEAD
-----------------------------------------------------------------------
`62:106` (plate + inlay) and `62:97`'s `star` child are the only nodes here whose SVG is a
`<path d="...">` using ONLY absolute M/L/H/V/Z -- no curves. That is exactly what
`svg_pillow` supports, so it is used instead of hand-rolling a polygon rasteriser twice.

WHITE, NOT THE FIGMA HUES. Every shape below ships neutral; `hud/self`, `hud/team-them`,
medal-tier and other tints are C++'s at runtime (ASSET rule, and GOTCHAS.md's reticle
precedent: "WHITE, NOT CYAN"). Figma bakes `#35D0F2`/`#36D1F2` (`hud/self`) into every ring
in this file and none of it survives into the PNGs -- opacity does, hue does not.

THE TRAP THIS FILE DODGED: `download_assets`'s `export` PNG composites the page background
in (GOTCHAS.md #2/#9, verified on `62:26` elsewhere in this project at opaque share 1.000).
Every shape here comes from `svgAssets` or from raw metadata (`get_metadata`), never from
that `export` field.

WHAT IS DELIBERATELY *NOT* HERE, AND WHY (Phase 1: "export nothing UMG can draw")
-----------------------------------------------------------------------------------
  - Tracker ticks (`24:7..24:10`) and Self Chevron (`24:11`): plain / rotated rectangles.
    `download_assets` returned NO svgAssets for them at all -- Figma itself judged them
    vector-free. A rotated `UImage` via RenderTransform draws these with no art.
  - Minimap spokes (`62:58..62:61`/`62:69..62:72`) and Self marker (`62:62`/`62:73`): same
    story, same reasoning.
  - Minimap blips (`62:74..62:76`, "Enemy Blip 1/2", "Ally Blip 3"): MOCK CONTENT. Those
    are demo positions for a demo frame, not a fixed layout a real minimap would ever want
    baked into its ring texture -- a real widget places blip glyphs dynamically. Baking
    three specific dots at three specific coordinates would be inventing a false invariant.
  - `23:24` HUD / Waypoint and `23:13` HUD / Location Label: see `main()` -- both are
    reported, not rasterised, because neither needs art at all.
  - `43:16`/`43:17` (EAGLE) and `43:23`/`43:24` (COBRA): PLACEHOLDERS, not art. See `main()`.

TWO NODES SHARE ONE ASSET, AND ONE ROW BECOMES THREE
--------------------------------------------------------
`62:54` (Clear) and `62:65` (Contacts) export byte-identical Field/Ring2/Ring3 SVGs -- diffed
directly, not assumed. Contacts adds only the mock blips (excluded above), so one ring asset
(`BN_Minimap_Ring`) serves both states; `62:79` (Disabled) is genuinely different art (a
single dim field, no rings) and gets its own file. Mirrors the ticket's own call on
`43:5`/`43:6`/`43:7`: three concentric circles are one drawable asset, not three.
"""
from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw

from svg_pillow import render as svg_render

OUT = Path(__file__).resolve().parents[2] / "Content/BN/UI/Assets"
SCRATCH = Path(__file__).resolve().parent / "_chrome_svg_scratch"

# 2x the design box throughout: the project's DPI curve is ShortestSide/720, so a 1080p
# window draws these at 1.5x. Authoring at 1x would upscale and blur; 2x has headroom to
# 1440p before it does (same rule `gen_reticle_art.py` and `gen_vitals_art.py` use).
SCALE = 2
# Supersample for the hand-drawn ellipses/rects: `ImageDraw.ellipse`'s outline is 1-bit,
# and `preflight_textures.py` rejects a <3-level alpha edge as jagged (correctly -- every
# shape here is a curve). Draw at SS x, box-filter down; same approach as gen_reticle_art.
SS = 4


def _ring(box_w: float, box_h: float, cx: float, cy: float, rx: float, ry: float,
           stroke: float, opacity: float) -> Image.Image:
    """One stroked ellipse, supersampled, on a `box_w x box_h` (design-unit) canvas."""
    n_w, n_h = round(box_w * SS), round(box_h * SS)
    im = Image.new("RGBA", (n_w, n_h), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    ccx, ccy, crx, cry, sw = cx * SS, cy * SS, rx * SS, ry * SS, max(1, round(stroke * SS))
    d.ellipse([ccx - crx, ccy - cry, ccx + crx, ccy + cry], outline=(255, 255, 255, 255), width=sw)
    im = im.resize((round(box_w * SCALE), round(box_h * SCALE)), Image.LANCZOS)
    if opacity < 1.0:
        a = im.getchannel("A").point(lambda v: round(v * opacity))
        im.putalpha(a)
    return im


def _filled_circle(box: float, cx: float, cy: float, r: float, opacity: float = 1.0) -> Image.Image:
    n = round(box * SS)
    im = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    ccx, ccy, cr = cx * SS, cy * SS, r * SS
    d.ellipse([ccx - cr, ccy - cr, ccx + cr, ccy + cr], fill=(255, 255, 255, 255))
    im = im.resize((round(box * SCALE), round(box * SCALE)), Image.LANCZOS)
    if opacity < 1.0:
        a = im.getchannel("A").point(lambda v: round(v * opacity))
        im.putalpha(a)
    return im


def _paste(canvas: Image.Image, layer: Image.Image, x: float, y: float) -> None:
    """Alpha-composite `layer` onto `canvas` at design-unit `(x, y)` (both already SCALE'd)."""
    canvas.alpha_composite(layer, (round(x * SCALE), round(y * SCALE)))


def _white_path_svg(box_w: float, box_h: float, path_d: str, opacity: float = 1.0) -> Path:
    """Write a scratch SVG: one filled path, forced white, stroke stripped."""
    SCRATCH.mkdir(exist_ok=True)
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{box_w}" height="{box_h}" '
           f'viewBox="0 0 {box_w} {box_h}" fill="none">'
           f'<path d="{path_d}" fill="#ffffff" fill-opacity="{opacity}"/></svg>')
    p = SCRATCH / "tmp.svg"
    p.write_text(svg, encoding="utf-8")
    return p


# ---------------------------------------------------------------------------------------
# 1. Tracker face -- Figma `24:2` HUD / Motion Tracker, 133x133.
#
# Rim/Mid Ring/Inner Ring (`24:4`/`24:5`/`24:6`) are concentric strokes around the SAME
# centre (66.5, 66.5) as `24:3` Field -- confirmed from their own boxes (Mid Ring sits at
# local (22.5,22.5) 88x88 inside the 133x133 parent: 22.5+44 == 66.5, matches). `24:3`
# Field's own radial-gradient fill is `hud/self` at 30%/6% -- a coloured decorative wash,
# not line art, and dropped (colour is C++'s). Numbers straight from the SVGs:
#   Rim        r=65.75  stroke-width=1.5  opacity=0.9
#   Mid Ring   r=43.5   stroke-width=1(default)  opacity=0.55
#   Inner Ring r=21.5   stroke-width=1(default)  opacity=0.75
# ---------------------------------------------------------------------------------------
def tracker_face() -> Image.Image:
    box = 133.0
    canvas = Image.new("RGBA", (round(box * SCALE), round(box * SCALE)), (0, 0, 0, 0))
    for r, sw, op in ((65.75, 1.5, 0.9), (43.5, 1.0, 0.55), (21.5, 1.0, 0.75)):
        ring = _ring(box, box, 66.5, 66.5, r, r, sw, op)
        canvas.alpha_composite(ring, (0, 0))
    return canvas


# ---------------------------------------------------------------------------------------
# 2. Mode icon -- Figma `43:5`/`43:6`/`43:7` (Outer 44 / Inner 24 / Core 14), the ticket's
# own example of "three concentric circles == one asset". All three share centre (22,22) in
# the 44x44 Outer box (Inner's local box is (10,10) 24x24 -> 10+12=22; Core's is (13.5,13.5)
# 14x14 -> 13.5+7=20.5, off by 1.5 from float rounding in the parent frame -- treated as the
# same centre, matching the ticket's read of the shape). Straight from the SVGs:
#   Outer  r=19.5  stroke-width=5  opacity=1
#   Inner  r=9.5   stroke-width=5  opacity=1
#   Core   r=7     FILLED, opacity=1
# ---------------------------------------------------------------------------------------
def mode_icon() -> Image.Image:
    box = 44.0
    canvas = Image.new("RGBA", (round(box * SCALE), round(box * SCALE)), (0, 0, 0, 0))
    canvas.alpha_composite(_ring(box, box, 22, 22, 19.5, 19.5, 5.0, 1.0), (0, 0))
    canvas.alpha_composite(_ring(box, box, 22, 22, 9.5, 9.5, 5.0, 1.0), (0, 0))
    core = _filled_circle(box, 22, 22, 7.0, 1.0)
    canvas.alpha_composite(core, (0, 0))
    return canvas


# ---------------------------------------------------------------------------------------
# 3. Minimap ring -- Figma `62:54` (Clear) / `62:65` (Contacts): byte-identical Field /
# Ring 2 / Ring 3 SVGs (diffed directly). Canvas is the 140x120 ELLIPSE box, not the full
# 140x138 component -- the extra 18px is the "Range"/"Callout" text row, pure UMG text.
# Centre (70,60) for all three (Field's own path spans x0.75-139.25 -> centre 70; Ring 2's
# local box (26.6,22.8) 86.8x74.4 -> 26.6+43.4=70; Ring 3's (49,42) 42x36 -> 49+21=70).
#   Field(boundary)  rx=69.25 ry=59.25  stroke-width=1.5  opacity=1     [fill dropped: colour]
#   Ring 2 (mid)     rx=42.9  ry=36.7   stroke-width=1(default) opacity=0.5
#   Ring 3 (inner)   rx=20.5  ry=17.5   stroke-width=1(default) opacity=0.5
# ---------------------------------------------------------------------------------------
def minimap_ring() -> Image.Image:
    w, h = 140.0, 120.0
    canvas = Image.new("RGBA", (round(w * SCALE), round(h * SCALE)), (0, 0, 0, 0))
    for rx, ry, sw, op in ((69.25, 59.25, 1.5, 1.0), (42.9, 36.7, 1.0, 0.5), (20.5, 17.5, 1.0, 0.5)):
        canvas.alpha_composite(_ring(w, h, 70, 60, rx, ry, sw, op), (0, 0))
    return canvas


# ---------------------------------------------------------------------------------------
# 4. Minimap disabled -- Figma `62:79`, the "jammed/off" field. Genuinely different art
# from `minimap_ring()` (one dim ellipse, no rings), so it gets its own file rather than
# being folded in as a low-opacity variant of the same texture.
#   Field Off  rx=69.5 ry=59.5 (centre 70,60)  stroke #4A5A6B (dropped -> neutral),
#              fill #1A2129 @ 35% (dropped -> neutral fill)
# Figma bakes a specific muted colour into both stroke and fill; both are dropped to
# neutral white per the "colour is C++'s" rule and re-expressed as alpha alone: a dimmer
# stroke (0.4) than the active ring's boundary (1.0 in minimap_ring) and a faint fill
# (0.10) standing in for the dark translucent plate. This is a judgement call, not a
# measurement -- the ORIGINAL alphas (stroke 1.0, fill 0.35 of a DARK colour) do not map
# onto a WHITE neutral 1:1, because a translucent white plate reads as a bright haze, not
# a dim one. What is preserved exactly: this state has a fill (the active ring does not)
# and a visibly duller stroke -- that is the entire visual distinction Figma draws between
# "ring" and "off", and it survives the recolour.
# ---------------------------------------------------------------------------------------
def minimap_disabled() -> Image.Image:
    w, h = 140.0, 120.0
    n_w, n_h = round(w * SS), round(h * SS)
    fill_layer = Image.new("RGBA", (n_w, n_h), (0, 0, 0, 0))
    ImageDraw.Draw(fill_layer).ellipse(
        [(70 - 69.5) * SS, (60 - 59.5) * SS, (70 + 69.5) * SS, (60 + 59.5) * SS],
        fill=(255, 255, 255, round(255 * 0.10)))
    fill_layer = fill_layer.resize((round(w * SCALE), round(h * SCALE)), Image.LANCZOS)
    canvas = Image.new("RGBA", (round(w * SCALE), round(h * SCALE)), (0, 0, 0, 0))
    canvas.alpha_composite(fill_layer, (0, 0))
    canvas.alpha_composite(_ring(w, h, 70, 60, 69.5, 59.5, 1.0, 0.4), (0, 0))
    return canvas


# ---------------------------------------------------------------------------------------
# 5. Medal chip -- Figma `62:106`, plate (`62:107`) + inlay (`62:108`), 64x64 parent.
# Both paths are absolute M/L/V/Z hexagons -- svg_pillow-compatible, no hand-rolled
# polygon needed. Offsets from metadata (plate at local (10,5), inlay at local (19,15));
# sizes from each SVG's OWN viewBox (its export pads ~0.5-1px for the source's 1px stroke,
# which is stripped here along with the fill colour):
#   plate  viewBox 45 x 55.2156   fill #8C949E -> white @ 35% (background badge)
#   inlay  viewBox 27 x 35.2229   fill #F2C752 -> white @ 100% (bright inset the medal
#          glyph itself would sit in -- no separate glyph SVG exists on this node; only
#          the chip backing is here, see the report)
# ---------------------------------------------------------------------------------------
_PLATE_D = "M22.5 0.569544L44.5 12.5695V36.5695L22.5 54.5695L0.5 36.5695V12.5695L22.5 0.569544Z"
_INLAY_D = "M13.5 0.567878L26.5 7.56788V23.5679L13.5 34.5679L0.5 23.5679V7.56788L13.5 0.567878Z"


def medal_chip() -> Image.Image:
    plate_w, plate_h = 45.0, 55.2156
    inlay_w, inlay_h = 27.0, 35.2229
    plate = svg_render(_white_path_svg(plate_w, plate_h, _PLATE_D, 0.35),
                        round(plate_w * SCALE), round(plate_h * SCALE))
    inlay = svg_render(_white_path_svg(inlay_w, inlay_h, _INLAY_D, 1.0),
                        round(inlay_w * SCALE), round(inlay_h * SCALE))
    canvas = Image.new("RGBA", (round(64 * SCALE), round(64 * SCALE)), (0, 0, 0, 0))
    _paste(canvas, plate, 10, 5)
    _paste(canvas, inlay, 19, 15)
    return canvas


# ---------------------------------------------------------------------------------------
# 6. Nameplate star -- Figma `62:97` SET Feedback / Nameplate, 130x30 parent. The ONLY
# extractable art in this node: `Bar` (`62:98`) is a 130x2 rule and `Name` (`62:99`) is
# text -- both plain UMG. `star` (`62:100`) is a filled 5-point star, an absolute
# M/L/H/V/Z path (has H segments -- still inside svg_pillow's supported grammar), fill
# #36D1F2 -> stripped to white. Box is the SVG's OWN viewBox (17 x 15.5387), not the
# metadata's 14x13 box -- the export pads for the source's 1px stroke.
# ---------------------------------------------------------------------------------------
_STAR_D = ("M8.5 1.34629L10.5 6.34629H15.5L11.5 9.34629L13.5 14.3463L8.5 11.3463"
           "L3.5 14.3463L5.5 9.34629L1.5 6.34629H6.5L8.5 1.34629Z")


def nameplate_star() -> Image.Image:
    w, h = 17.0, 15.5387
    return svg_render(_white_path_svg(w, h, _STAR_D, 1.0), round(w * SCALE), round(h * SCALE))


# ---------------------------------------------------------------------------------------
# 7. Interaction prompt key ring -- Figma `62:101` SET Feedback / Interaction Prompt,
# 220x24 parent. The ONLY art: `Key` (`62:102`), an 18x18 circle stroke, r=8.5,
# stroke-width=1(default), opacity=1 (no opacity attr). `K` (`62:103`) and `Verb`
# (`62:104`) are text; `Hold Progress` (`62:105`) is a 96x2 rule -- all plain UMG.
# ---------------------------------------------------------------------------------------
def key_ring() -> Image.Image:
    box = 18.0
    return _ring(box, box, 9, 9, 8.5, 8.5, 1.0, 1.0)


ASSETS = [
    ("BN_Tracker_Face", tracker_face),
    ("BN_Scoreboard_ModeIcon", mode_icon),
    ("BN_Minimap_Ring", minimap_ring),
    ("BN_Minimap_Disabled", minimap_disabled),
    ("BN_Feedback_MedalChip", medal_chip),
    ("BN_Feedback_NameplateStar", nameplate_star),
    ("BN_Feedback_KeyRing", key_ring),
]


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    for stem, fn in ASSETS:
        im = fn()
        path = OUT / f"{stem}.png"
        im.save(path)
        a = im.getchannel("A")
        hist = a.histogram()
        w, h = im.size
        opaque = hist[255] / float(w * h)
        levels = len({v for v in range(1, 255) if hist[v] > 0})
        print(f"  {stem:26} {w}x{h}  opaque={opaque:.3f}  AA-levels={levels}  -> {path}")

    print()
    print("NOT RASTERISED, ON PURPOSE:")
    print("  23:24 HUD / Waypoint      -- both Outer/Inner are plain rotated squares, no")
    print("                               svgAssets returned at all. A rotated UImage draws")
    print("                               this with no art. Nothing to extract.")
    print("  23:13 HUD / Location Label -- pure text node, no svgAssets. Nothing to extract.")
    print("  43:16/43:17 EAGLE Emblem   -- PLACEHOLDER. download_assets returns zero")
    print("                               svgAssets AND zero rawImages; the export PNG is a")
    print("                               single flat colour (71,128,163), 44x44, no glyph.")
    print("                               Despite the Figma label 'ORIGINAL ART', there is")
    print("                               no drawable emblem here -- not extracted, not")
    print("                               invented.")
    print("  43:23/43:24 COBRA Emblem   -- same placeholder story as EAGLE, other tint.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
