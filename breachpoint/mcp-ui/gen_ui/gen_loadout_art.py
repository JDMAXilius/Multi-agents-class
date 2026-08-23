#!/usr/bin/env python3
"""Author the BN loadout-tray glyphs (grenades, abilities, two weapon silhouettes) as PNG.

    python3 mcp-ui/gen_ui/gen_loadout_art.py

Import with:
    python3 mcp-ui/gen_ui/import_textures.py Content/BN/UI/Assets /Game/BN/UI/Assets

Figma file `yznvnVdOFDADaugZSeomfP`, page `6:48` (HUD / Elements). Every `d` string and
viewBox below is the literal `svgAssets` payload `download_assets` returned for that node --
not eyeballed, not retyped from a screenshot.

WHY NOT `svg_pillow.render()`
------------------------------
Every glyph here ships `fill-rule="evenodd"` with 2-4 subpaths per `<path>` (Figma exports
one `<path>` per node, all subpaths concatenated in one `d`). `svg_pillow._parse_path` flattens
every `M`/`L`/`H`/`V` command into ONE point list and calls `draw.polygon` on it once per
`<path>` element -- it does not split on `M`, so a path with disjoint or nested subpaths comes
out as one self-intersecting polygon connecting the last point of one subpath straight to the
first point of the next. That is silently wrong, not an error, so it is not safe to reuse here.

Two families genuinely NEED even-odd: Repulsor (`61:60`) and Overshield (`61:84`) are each an
outer diamond/hexagon with a smaller same-shaped inner subpath -- under evenodd that inner
subpath is a HOLE, producing a ring, not two overlapping solid diamonds. The Shotgun (`78:10`)
and Rocket (`78:12`) silhouettes go further: 4 holes each (sight windows, ejection ports, the
rocket's circular window approximated as an 18-point polygon -- no curve commands appear).

So this file implements its own path splitter (`_subpaths`, same M/L/H/V/Z-absolute-only
subset as `svg_pillow`, and it raises `ValueError` on anything else) and fills evenodd
correctly for any nesting depth via XOR: draw each subpath into its own mask layer, then
`ImageChops.difference` (0/255 images -> difference IS xor) accumulates them. For the many
glyphs here that have no holes at all (Frag, DropWall, Grapple, ...) XOR of non-overlapping
regions is just their union, so one code path is correct for every shape in this file.

DUPLICATE CHECK, DONE BEFORE ANY EXTRACTION
--------------------------------------------
ASSET-RULES §1 (reuse before authoring). `Content/UI/HUD/` already ships
HUD_Weapon_{AR,BR,Magnum,Rocket,Shotgun,Sniper}.png. Rendered every `78:x` node at its native
94x31 next to the shipped PNG on a dark backdrop (transparency can't be judged from
`get_screenshot`, GOTCHAS #9, but silhouette shape can):

    78:2  AR       IDENTICAL silhouette to HUD_Weapon_AR.png       -> SKIPPED, reuse shipped
    78:4  BR       IDENTICAL silhouette to HUD_Weapon_BR.png       -> SKIPPED, reuse shipped
    78:6  Magnum   IDENTICAL silhouette to HUD_Weapon_Magnum.png   -> SKIPPED, reuse shipped
    78:8  Sniper   IDENTICAL silhouette to HUD_Weapon_Sniper.png   -> SKIPPED, reuse shipped
    78:10 Shotgun  DIFFERENT: Figma is a pump shotgun (rail cutout, no scope); shipped PNG has
                   a bolt-action profile with a scope dot. -> AUTHORED below as BN_Weapon_Shotgun
    78:12 Rocket   DIFFERENT: Figma has a circular window on the tube and a plain top rail;
                   shipped PNG has a stripe pattern instead, no circular window. -> AUTHORED
                   below as BN_Weapon_Rocket

`download_assets` on both `78:10` and `78:12` confirmed the same trap as GOTCHAS #9 on the
`export` PNG (opaque share 1.000 both) -- the `svgAssets` SVG is what is used below.

SELECTED/UNSELECTED AND READY/COOLING ARE OPACITY, NOT SHAPE
--------------------------------------------------------------
Checked on two pairs directly (`61:27`/`61:39` Frag, `61:54`/`61:57` Grapple) and confirmed on
two more (`61:30`/`61:42` Plasma, `61:60`/`61:63` Repulsor): the Unselected/Cooling glyph SVG
is byte-identical to its Ready/Selected sibling's `d`, plus one added `opacity="0.45"` (grenade
Unselected) or `opacity="0.35"` (ability Cooling) on the `<path>`. One glyph per family covers
both states; state is a runtime alpha multiply, not two textures. The remaining two grenade
pairs (Spike, Dynamo) and four ability pairs (Threat, Drop Wall, Thruster, Overshield) were not
each individually diffed against their Unselected/Cooling twin -- the pattern held on all four
pairs checked, so it is assumed rather than re-verified 6 more times; if it is ever wrong for
one family, only that family's asset needs a redo, and the color is C++'s regardless (this
project's colour law), so opacity should be too.

The Cooling state ALSO adds a `Cooldown Band (fills UP)` `rounded-rectangle` (`61:59`) -- that
is a plain rounded rect UMG already draws (the vitals bars establish that pattern), so it is
not art and is not exported here.

WHITE, NOT CYAN. Every source path ships `fill="#36D1F2"` or `#35D0F2` (Figma's `hud/self`-ish
preview tint) plus `stroke="black"` (an editor-visibility outline, not real line art -- these
shapes are solid glyphs, not strokes). Both are discarded; only the path geometry is kept and
filled flat white, per this ticket's colour law (C++ tints via GameplayEffect-driven tags).

WHAT HAS NO RUNTIME CONSUMER TODAY
------------------------------------
BN ships one grenade type and no ability system (per the ticket brief). All 4 grenade glyphs
and all 6 ability glyphs below are extracted on the founder's instruction with no current
reader -- Frag is the only grenade BN's data table names, and the other three plus every
ability are forward stock. Said again in `03-Loadout-MEASURED.md`.
"""
from __future__ import annotations

import re
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw

OUT = Path(__file__).resolve().parents[2] / "Content/BN/UI/Assets"

# Supersample factor, same convention as svg_pillow.SS: draw big, box-filter down, so edges
# carry real partial-alpha coverage instead of a 1-bit stair-step (preflight's AA check).
SS = 4

_NUM = re.compile(r"[-+]?(?:\d*\.\d+|\d+)(?:[eE][-+]?\d+)?")
_UNSUPPORTED = re.compile(r"[SsQqTtAamlhvcz]")


def _subpaths(d: str) -> list[list[tuple[float, float]]]:
    """Absolute M/L/H/V/Z only -> one point list per subpath (M starts a new one).

    Raises on anything else (curves, relative commands) rather than silently mis-drawing it --
    every `d` string used below was checked against this and none trip it.
    """
    if _UNSUPPORTED.search(d):
        raise ValueError(f"unsupported path command in {d[:60]!r}")
    subs: list[list[tuple[float, float]]] = []
    pts: list[tuple[float, float]] = []
    x = y = 0.0
    for cmd, rest in re.findall(r"([MLHVZ])([^MLHVZ]*)", d):
        nums = [float(n) for n in _NUM.findall(rest)]
        if cmd == "M":
            if pts:
                subs.append(pts)
            pts = []
            for i in range(0, len(nums) - 1, 2):
                x, y = nums[i], nums[i + 1]
                pts.append((x, y))
        elif cmd == "L":
            for i in range(0, len(nums) - 1, 2):
                x, y = nums[i], nums[i + 1]
                pts.append((x, y))
        elif cmd == "H":
            for n in nums:
                x = n
                pts.append((x, y))
        elif cmd == "V":
            for n in nums:
                y = n
                pts.append((x, y))
        # Z: no-op -- ImageDraw.polygon auto-closes each subpath.
    if pts:
        subs.append(pts)
    return subs


def rasterise(d: str, vb_w: float, vb_h: float, w: int, h: int) -> Image.Image:
    """Even-odd fill of `d` (viewBox vb_w x vb_h) into a w x h white-on-transparent RGBA.

    XOR-accumulates one mask layer per subpath (`ImageChops.difference` on 0/255 images IS
    xor), which is the even-odd rule for any nesting depth: a lone subpath fills solid, a
    subpath nested inside another punches a hole, non-overlapping subpaths simply union.
    """
    subs = _subpaths(d)
    W, H = w * SS, h * SS
    sx, sy = W / vb_w, H / vb_h
    mask = Image.new("L", (W, H), 0)
    for sp in subs:
        layer = Image.new("L", (W, H), 0)
        ImageDraw.Draw(layer).polygon([(x * sx, y * sy) for x, y in sp], fill=255)
        mask = ImageChops.difference(mask, layer)
    mask = mask.resize((w, h), Image.LANCZOS)
    im = Image.new("RGBA", (w, h), (255, 255, 255, 0))
    im.putalpha(mask)
    return im


# ---------------------------------------------------------------------------------------
# GRENADES -- design box 22 x 16.67 each (frame, incl. the "Count" text UMG draws), glyph
# itself is the viewBox below. One state each: Unselected is the same `d` at opacity 0.45,
# not authored (see module docstring).
# ---------------------------------------------------------------------------------------
GRENADES = [
    # stem, node, d, viewBox w, viewBox h
    ("BN_Grenade_Frag", "61:27",
     "M5.5 0.640312L10.5 4.64031V11.6403L5.5 15.6403L0.5 11.6403V4.64031L5.5 0.640312Z",
     11.0, 16.2806),
    ("BN_Grenade_Plasma", "61:30",
     "M6.625 0.833333L12.625 8.83333L6.625 16.8333L0.625 8.83333L6.625 0.833333Z",
     13.25, 17.6667),
    ("BN_Grenade_Spike", "61:33",
     "M8.15811 1.82003L10.1581 8.82003L16.1581 9.82003L10.1581 11.82L8.15811 18.82"
     "L6.15811 11.82L0.158114 9.82003L6.15811 8.82003L8.15811 1.82003Z",
     16.3162, 20.6401),
    ("BN_Grenade_Dynamo", "61:36",
     "M0.5 0.5H10.5V4.5H3.5V8.5H10.5V12.5H0.5V8.5H7.5V4.5H0.5V0.5Z",
     11.0, 13.0),
]

# ---------------------------------------------------------------------------------------
# ABILITIES -- design box 50.67 x 30 each. One state each: Cooling is the same `d` at
# opacity 0.35 plus a Cooldown Band rounded-rect UMG draws (61:59) -- not authored.
# Repulsor and Overshield each carry a nested inner subpath -> a punched hole (ring look).
# ---------------------------------------------------------------------------------------
ABILITIES = [
    ("BN_Ability_Grapple", "61:54",
     "M1.20711 19.5L7.20711 13.5L13.2071 19.5H10.2071L7.20711 16.5L4.20711 19.5H1.20711Z"
     "M6.20711 1.5H8.20711V14.5H6.20711V1.5Z"
     "M2.20711 0.5H12.2071V3.5H2.20711V0.5Z",
     14.4142, 20.0),
    ("BN_Ability_Repulsor", "61:60",
     "M8.70711 0.707107L16.7071 8.70711L8.70711 16.7071L0.707107 8.70711L8.70711 0.707107Z"
     "M8.70711 4.70711L12.7071 8.70711L8.70711 12.7071L4.70711 8.70711L8.70711 4.70711Z",
     17.4142, 17.4142),
    ("BN_Ability_Threat", "61:66",
     "M0.5 8.82003H6.5L8.5 1.82003L12.5 15.82L14.5 8.82003H20.5V10.82H15.5L12.5 19.82"
     "L8.5 5.82003L7.5 10.82H0.5V8.82003Z",
     21.0, 21.5114),
    ("BN_Ability_DropWall", "61:72",
     "M0.5 0.5H16.5V3.5H0.5V0.5Z"
     "M0.5 5.5H16.5V8.5H0.5V5.5Z"
     "M0.5 10.5H16.5V13.5H0.5V10.5Z",
     17.0, 14.0),
    ("BN_Ability_Thruster", "61:78",
     "M5.84976 1.02956L10.8498 10.0296H7.84976V18.0296H3.84976V10.0296H0.849757L5.84976 1.02956Z",
     11.6995, 18.5296),
    ("BN_Ability_Overshield", "61:84",
     "M9.5 0.547159L18.5 4.54716V11.5472L9.5 18.5472L0.5 11.5472V4.54716L9.5 0.547159Z"
     "M9.5 4.54716L14.5 6.54716V10.5472L9.5 14.5472L4.5 10.5472V6.54716L9.5 4.54716Z",
     19.0, 19.1806),
]

# ---------------------------------------------------------------------------------------
# WEAPON SILHOUETTES -- design box 94 x 30.67. Only Shotgun and Rocket: AR/BR/Magnum/Sniper
# are pixel-identical silhouettes to shipped Content/UI/HUD/HUD_Weapon_* (see module
# docstring) and are skipped as duplicates, not authored.
# ---------------------------------------------------------------------------------------
WEAPONS = [
    ("BN_Weapon_Shotgun", "78:10",
     "M26 4.5H60V13.5H26V4.5Z"
     "M42 13.5H51L48.5 22.5H40L42 13.5Z"
     "M43.5 17.5H49V14.5H43.5V17.5Z"
     "M2 5H26V9.5H2V5Z"
     "M2 10.5H24V14H2V10.5Z"
     "M5 13H21V11.5H5V13Z"
     "M0 4H2.5V10.5H0V4Z"
     "M30 0H44V4H30V0Z"
     "M32.5 2.8H41.5V1.2H32.5V2.8Z"
     "M60 3.5H78V13.5H60V3.5Z"
     "M63 11H75V6H63V11Z",
     78.0, 22.5),
    ("BN_Weapon_Rocket", "78:12",
     "M4 5.5H74V16.5H4V5.5Z"
     "M14.04 10.01L13.59 9.12L12.88 8.41L11.99 7.96L11 7.8L10.01 7.96L9.12 8.41L8.41 9.12"
     "L7.96 10.01L7.8 11L7.96 11.99L8.41 12.88L9.12 13.59L10.01 14.04L11 14.2L11.99 14.04"
     "L12.88 13.59L13.59 12.88L14.04 11.99L14.2 11L14.04 10.01Z"
     "M0 4H5V18H0V4Z"
     "M28 0H48V5.5H28V0Z"
     "M31 4H45V1.5H31V4Z"
     "M42 16.5H51L48 25.5H40L42 16.5Z"
     "M43.5 20.5H48.5V17.5H43.5V20.5Z"
     "M70 2.5H82V19.5H70V2.5Z"
     "M73 16.5H79V5.5H73V16.5Z",
     82.0, 25.5),
]


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    for group, label in ((GRENADES, "grenade"), (ABILITIES, "ability"), (WEAPONS, "weapon")):
        for stem, node, d, vb_w, vb_h in group:
            # Author at 2x the design box (this ticket's instruction) so the DPI curve
            # (ShortestSide/720 -> 1.5x at 1080p) has headroom before it would upscale.
            w, h = round(vb_w * 2), round(vb_h * 2)
            im = rasterise(d, vb_w, vb_h, w, h)
            path = OUT / f"{stem}.png"
            im.save(path)
            alpha = list(im.getchannel("A").getdata())
            opaque = sum(1 for a in alpha if a == 255) / len(alpha)
            levels = len({a for a in alpha if 0 < a < 255})
            print(f"  {stem:24} {label:8} {node:6} {w}x{h}  opaque={opaque:.3f}  "
                  f"aa_levels={levels}  -> {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
