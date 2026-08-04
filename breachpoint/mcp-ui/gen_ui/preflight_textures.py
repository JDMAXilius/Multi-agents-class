#!/usr/bin/env python3
"""Pre-flight every UI texture BEFORE it is imported into UE. Nothing ships unchecked.

    python3 mcp-ui/gen_ui/preflight_textures.py [Content/UI/Icons] [--manifest m.json]

An icon that is wrong in the engine is expensive to find: it looks perfect in every
image viewer, and the defect only appears composited over a panel at runtime. Every
check below exists because that class of bug is invisible until late.

CHECKS

  1. RGBA            — no alpha channel means no transparency, full stop.
  2. Corner alpha    — all four corners must be 0. A non-zero corner is a baked-in
                       background. This is what the first 41 exports all failed on:
                       `download_assets` composites against the page backdrop, while
                       the Plugin API `exportAsync` renders in isolation.
  3. Opaque share    — >90% fully opaque is a filled plate, not a glyph.
  4. Dimensions      — must equal the size the FILENAME promises x the export scale.
                       The expectation must come from outside the file. Deriving it from
                       the source's own header made this check tautological: rasterize_svg
                       reads sw,sh from the SVG header, renders at sw*scale, and preflight
                       then asserted the output equalled sw*scale — it compared the render
                       against the header it rendered from and could never fail. It missed
                       the two real defects in the set: Assault_16 and Extraction_16 ship
                       viewBox="0 0 17 16", the only non-square _16 sources in a family of
                       14, so they land as 68x64 where every sibling is 64x64 and UMG
                       squashes them 6.25% horizontally in a square 16x16 brush.
                       EXPECTED_SIZE below is the on-disk naming convention, verified
                       against every viewBox in Export/UI. An unrecognised name WARNS.
  5. Clipping        — ink touching the canvas edge means the glyph is cut off. The
                       source frames all carry deliberate padding, so ink at the edge
                       is a bug, never a design choice.
  6. Neutral ink     — icons ship WHITE and are tinted in UMG from the palette. One
                       texture then serves every state and every colour, which is the
                       single biggest asset-count saving available. A coloured export
                       silently defeats it and cannot be recoloured.
  7. Anti-aliasing   — a real vector export has partial alpha at the edges. An alpha
                       channel that is only 0 and 255 means the export was rasterised
                       1-bit and will look jagged at every size — UNLESS the geometry
                       cannot produce partial alpha at all, in which case zero AA is the
                       forced correct output and failing it sends perfect art back to be
                       redrawn. Elimination_16 was quarantined exactly that way: its path
                       is {M,H,V,Z} only and all 18 coordinates are multiples of 0.25, so
                       at 4x every edge lands on an integer device pixel. The caller that
                       holds the SVG (rasterize_svg.aa_possible) decides; this end only
                       honours `aa_possible=False`. Standalone runs have no SVG, so they
                       default to checking — a false alarm is cheaper than a silent skip.
  8. Padding report  — informational. ASSET-PIPELINE §3 says trim to the ink, but
                       grid-designed icons (Lucide is authored on a 24 grid) rely on
                       consistent optical padding for alignment. Reported, not failed:
                       it is a design call, not a defect.
"""
from __future__ import annotations

import json, sys
from pathlib import Path

from PIL import Image

REPO = Path(__file__).resolve().parents[2]
OPAQUE_SHARE_LIMIT = 0.90
CHROMA_TOLERANCE = 12       # max RGB spread before ink counts as coloured
GREY_INK_FLOOR  = 240       # dimmest opaque ink allowed: UMG tinting multiplies
MIN_AA_LEVELS = 3           # distinct partial-alpha values expected from a vector

# Expected SOURCE size per naming convention, in source units — multiplied by the export
# scale to get the pixel size. Every entry below was checked against the actual viewBox of
# all 137 SVGs under Export/UI: 0 unrecognised, and the only two disagreements are the
# genuine defects (Assault_16 / Extraction_16 at 17x16). Prefix rules are tried first
# because T_UI_Rank_16_Vanguard must NOT be read as a "_16" icon.
EXPECTED_BY_PREFIX = [
    ("T_UI_Rank_",             (76, 76)),
    ("T_UI_Icon_Glyph_",       (23, 23)),   # ModeGlyphs, all 14
    ("T_UI_Icon_Currency_",    (24, 40)),   # the only non-square family
    ("T_UI_Icon_Container_",   (40, 40)),
    ("T_UI_Icon_GametypeV2_",  (40, 40)),
]
EXPECTED_BY_SUFFIX = [
    ("_16",  (16, 16)),
    ("_24",  (24, 24)),
    ("_40",  (40, 40)),
    ("_120", (120, 120)),
]

# The HUD family carries NO size convention, so it gets an explicit per-file baseline.
#
# Read the weaker claim this makes, and do not oversell it. The icon rules above are a
# real convention: a name ENDS in _16, so 16x16 is what the name PROMISES, and a 17x16
# source is caught as a defect the first time it is seen. Nothing like that exists here.
# Reticles are deliberately five different sizes because the size encodes the weapon's
# spread; grenades are three different heights; the two Vitals states differ by 1px.
# There is no rule to derive - only observation.
#
# So these numbers were transcribed ONCE, by hand, from the 29 viewBoxes as exported on
# 3 Aug 2026, and committed. That makes this a DRIFT DETECTOR, not a validator: it will
# catch a re-export that silently changes a size, a node id pointed at the wrong frame,
# or a stale file - and it will NOT catch a size that was wrong on the day it was first
# exported, because that wrongness is what got recorded. Regenerating this table from the
# files would make it tautological again, which is the exact bug just fixed in check 4.
#
# Vitals: ShieldHealth is 277 and ShieldBroken 276. One pixel apart for what should be
# two states of one widget. Recorded as measured rather than reconciled - the 1px belongs
# to whoever owns the Figma frame, and averaging it here would hide it forever.
EXPECTED_EXACT = {
    "HUD_Ability_Grapple_Ready":   (52, 31),
    "HUD_Ammo_Readout":            (190, 40),
    "HUD_Ammo_Low":                (190, 40),
    "HUD_Ammo_Battery":            (190, 40),
    "HUD_Feedback_DamageDir":      (40, 42),
    "HUD_Feedback_ShieldBreak":    (40, 42),
    "HUD_Feedback_Medal":          (120, 120),
    "HUD_Grenade_Frag_Sel":        (23, 17),
    "HUD_Grenade_Dynamo_Sel":      (23, 17),
    "HUD_Grenade_Plasma_Sel":      (23, 18),   # not 17 - the four are not a uniform set
    "HUD_Grenade_Spike_Sel":       (23, 21),   # nor is this
    "HUD_Minimap_Clear":           (140, 138),
    "HUD_Minimap_Contacts":        (140, 138),
    "HUD_Minimap_Disabled":        (140, 138),
    "HUD_MotionTracker":           (136, 152),
    "HUD_Reticle_AR":              (43, 43),
    "HUD_Reticle_BR":              (43, 43),
    "HUD_Reticle_EnemyState":      (43, 43),   # renamed: it is the ENEMY-STATE reticle,
                                           # not a hit-marker set. Six paths, all #FF4A3D
                                           # (visr/enemy), id "SET Reticle / Enemy State".
                                           # I reported "4 sub-assets = 4 states"; those
                                           # are the 4 arcs of ONE state. No shield-vs-flesh
                                           # hit-marker art exists anywhere in the file.
    "HUD_Reticle_Magnum":          (36, 36),   # tightest - the most accurate weapon
    "HUD_Reticle_Shotgun":         (52, 52),
    "HUD_Reticle_Sniper":          (58, 58),   # widest, and it is 5 rects, not paths
    "HUD_Vitals_ShieldHealth":     (277, 35),
    "HUD_Vitals_ShieldBroken":     (276, 35),  # see note above: 1px, deliberately not hidden
    "HUD_Weapon_AR":               (94, 31),
    "HUD_Weapon_BR":               (94, 31),
    "HUD_Weapon_Magnum":           (94, 31),
    "HUD_Weapon_Sniper":           (94, 31),
    "HUD_Weapon_Rocket":           (94, 31),
    "HUD_Weapon_Shotgun":          (94, 31),
}


def expected_size(stem: str) -> tuple[int, int] | None:
    """Source size the NAME promises, or None if the convention does not cover it."""
    if stem in EXPECTED_EXACT:          # exact entries win: they are a recorded baseline
        return EXPECTED_EXACT[stem]
    for prefix, wh in EXPECTED_BY_PREFIX:
        if stem.startswith(prefix):
            return wh
    for suffix, wh in EXPECTED_BY_SUFFIX:
        if stem.endswith(suffix):
            return wh
    return None


def preflight(p: Path, expect: dict | None = None, *,
              aa_possible: bool = True) -> tuple[bool, list[str], str]:
    fails: list[str] = []
    notes: list[str] = []
    im = Image.open(p)

    if im.mode != "RGBA":
        return False, [f"mode={im.mode}, no alpha channel"], ""
    im = im.convert("RGBA")
    w, h = im.size
    a = im.getchannel("A")
    px = a.load()

    # 2 — corners
    corners = [px[0, 0], px[w - 1, 0], px[0, h - 1], px[w - 1, h - 1]]
    if any(c != 0 for c in corners):
        fails.append(f"corner alpha {corners} — background is baked in")

    hist = a.histogram()
    opaque = hist[255] / float(w * h)
    transparent = hist[0] / float(w * h)

    # 3 — plate
    if opaque > OPAQUE_SHARE_LIMIT:
        fails.append(f"{opaque:.0%} fully opaque — filled plate, not a glyph")

    # 4 — dimensions, against the NAME, never against the source's own header. `expect`
    # supplies only the scale: its w/h come from the very node/header that produced the
    # render, so they cannot detect a source authored at the wrong size.
    scale = float((expect or {}).get("scale", 4))
    src = expected_size(p.stem)
    if src is None:
        notes.append(f"WARN: '{p.stem}' matches no naming rule — size UNCHECKED")
    else:
        want = (int(src[0] * scale), int(src[1] * scale))
        if (w, h) != want:
            fails.append(f"size {w}x{h}, expected {want[0]}x{want[1]} "
                         f"({src[0]}x{src[1]} @ {scale:g}x, from the name)")

    # 5 — clipping: any ink on the border row/column
    edge = ([px[x, 0] for x in range(w)] + [px[x, h - 1] for x in range(w)]
            + [px[0, y] for y in range(h)] + [px[w - 1, y] for y in range(h)])
    # >128, not >8. At 4x, alpha>128 on the border means geometry within 0.125
    # source units of the frame — a real clip. The old >8 flagged ANTI-ALIAS
    # FEATHER: LandGrab_16 is inset on all four sides (ink y 0.17-15.83 in a 16
    # viewBox) yet tripped 66 'clipped' pixels whose max alpha was 64. Measured
    # safety: max edge alpha across all 128 shipped icons is 0; the two genuine
    # clips sit at 255. Sub-threshold edge ink is reported as a note, not a fail.
    ink_on_edge = sum(1 for v in edge if v > 128)
    feathered = sum(1 for v in edge if 8 < v <= 128)
    if ink_on_edge:
        fails.append(f"{ink_on_edge} edge pixels at alpha>128 — glyph is clipped")
    elif feathered:
        notes.append(f"{feathered} edge px of AA feather (tight padding, not a clip)")

    # 6 — neutral ink, sampled only where the pixel is essentially opaque
    rgb = im.convert("RGB").load()
    spread_max, sampled, brightest = 0, 0, 0
    # No stride. At 1-in-22 a 2x2 coloured accent was missed 19 times in 20 and a
    # 3x3 ten times in 20 — an icon that cannot be tinted, passing silently.
    for y in range(h):
        for x in range(w):
            if px[x, y] < 200:
                continue
            r, g, b = rgb[x, y]
            spread_max = max(spread_max, max(r, g, b) - min(r, g, b))
            brightest = max(brightest, max(r, g, b))
            sampled += 1
    if sampled and spread_max > CHROMA_TOLERANCE:
        fails.append(f"ink is coloured (RGB spread {spread_max}) — "
                     "icons must ship neutral and be tinted in UMG")
    # Spread alone passes a solid (128,128,128) glyph. UMG tinting MULTIPLIES, so
    # that renders at half brightness against every palette colour, with no signal.
    if sampled and brightest < GREY_INK_FLOOR:
        fails.append(f"ink is grey (brightest opaque value {brightest}) — tinting "
                     "multiplies, so this renders dim against every palette colour")

    # 7 — anti-aliasing, only where anti-aliasing is POSSIBLE
    partial = sum(1 for lvl in range(1, 255) if hist[lvl] > 0)
    if partial < MIN_AA_LEVELS:
        if aa_possible:
            fails.append(f"only {partial} partial-alpha levels — export is 1-bit, "
                         "will look jagged")
        else:
            notes.append(f"{partial} AA levels, exempt — geometry is axis-aligned on "
                         "integer device pixels, so zero AA is correct")

    # 8 — padding, informational
    bbox = im.getbbox()
    if bbox:
        bw, bh = bbox[2] - bbox[0], bbox[3] - bbox[1]
        pad = 1 - (bw * bh) / float(w * h)
        notes.append(f"ink {bw}x{bh} in {w}x{h} ({pad:.0%} padding)")

    summary = f"{w}x{h} · {transparent:.0%} clear · {partial} AA levels"
    if notes:
        summary += " · " + "; ".join(notes)
    return (not fails), fails, summary


def main() -> int:
    argv = sys.argv[1:]
    if "--scale" in argv:
        i = argv.index("--scale")
        scale = float(argv[i + 1])
        argv = argv[:i] + argv[i + 2:]
    else:
        scale = 4.0
    args = [a for a in argv if not a.startswith("--")]
    root = REPO / (args[0] if args else "Content/UI/Icons")
    manifest = {}
    if "--manifest" in sys.argv:
        # Only its `scale` is read now; see check 4 on why its w/h cannot be trusted.
        mp = Path(sys.argv[sys.argv.index("--manifest") + 1])
        manifest = json.load(open(mp))

    pngs = sorted(root.rglob("*.png"))
    if not pngs:
        print(f"no PNGs under {root}")
        return 2

    bad, warned = [], []
    for p in pngs:
        rel = p.relative_to(root).as_posix()
        ok, fails, summary = preflight(
            p, {"scale": manifest.get(p.stem, {}).get("scale", scale)})
        if "WARN:" in summary:
            warned.append(rel)
        if ok:
            print(f"  ok    {rel}  {summary}")
        else:
            print(f"  FAIL  {rel}")
            for f in fails:
                print(f"          - {f}")
            bad.append(rel)

    print(f"\n{len(pngs) - len(bad)}/{len(pngs)} passed pre-flight "
          f"(sizes checked at {scale:g}x from the naming convention)")
    if warned:
        print(f"WARNING — {len(warned)} name(s) match no size rule, dimensions UNCHECKED:")
        for wname in warned:
            print(f"  {wname}")
    if bad:
        print("REJECTED — do NOT import:")
        for b in bad:
            print(f"  {b}")
        return 1
    print("All clean. Safe to import into UE.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
