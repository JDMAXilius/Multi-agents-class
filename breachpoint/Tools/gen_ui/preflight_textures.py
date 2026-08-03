#!/usr/bin/env python3
"""Pre-flight every UI texture BEFORE it is imported into UE. Nothing ships unchecked.

    python3 Tools/gen_ui/preflight_textures.py [Content/UI/Icons] [--manifest m.json]

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
  4. Dimensions      — must equal the Figma source size x the export scale, exactly.
                       An off-by-one means the wrong node or a stale export.
  5. Clipping        — ink touching the canvas edge means the glyph is cut off. The
                       source frames all carry deliberate padding, so ink at the edge
                       is a bug, never a design choice.
  6. Neutral ink     — icons ship WHITE and are tinted in UMG from the palette. One
                       texture then serves every state and every colour, which is the
                       single biggest asset-count saving available. A coloured export
                       silently defeats it and cannot be recoloured.
  7. Anti-aliasing   — a real vector export has partial alpha at the edges. An alpha
                       channel that is only 0 and 255 means the export was rasterised
                       1-bit and will look jagged at every size.
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


def preflight(p: Path, expect: dict | None) -> tuple[bool, list[str], str]:
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

    # 4 — dimensions
    if expect:
        want = (int(expect["w"] * expect["scale"]), int(expect["h"] * expect["scale"]))
        if (w, h) != want:
            fails.append(f"size {w}x{h}, expected {want[0]}x{want[1]} "
                         f"({expect['w']}x{expect['h']} @ {expect['scale']}x)")

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

    # 7 — anti-aliasing
    partial = sum(1 for lvl in range(1, 255) if hist[lvl] > 0)
    if partial < MIN_AA_LEVELS:
        fails.append(f"only {partial} partial-alpha levels — export is 1-bit, will look jagged")

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
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    root = REPO / (args[0] if args else "Content/UI/Icons")
    manifest = {}
    if "--manifest" in sys.argv:
        mp = Path(sys.argv[sys.argv.index("--manifest") + 1])
        manifest = json.load(open(mp))

    pngs = sorted(root.rglob("*.png"))
    if not pngs:
        print(f"no PNGs under {root}")
        return 2

    bad = []
    for p in pngs:
        rel = p.relative_to(root).as_posix()
        ok, fails, summary = preflight(p, manifest.get(p.stem))
        if ok:
            print(f"  ok    {rel}  {summary}")
        else:
            print(f"  FAIL  {rel}")
            for f in fails:
                print(f"          - {f}")
            bad.append(rel)

    print(f"\n{len(pngs) - len(bad)}/{len(pngs)} passed pre-flight")
    if bad:
        print("REJECTED — do NOT import:")
        for b in bad:
            print(f"  {b}")
        return 1
    print("All clean. Safe to import into UE.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
