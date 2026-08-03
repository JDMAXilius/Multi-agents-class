#!/usr/bin/env python3
"""Reject any UI texture that shipped with a baked-in background.

    python3 Tools/gen_ui/check_transparency.py Content/UI/Icons

ASSET-PIPELINE.md §3: "Transparent background. No baked-in panel colour behind an icon."
That rule exists because a baked background cannot be recoloured from the palette, cannot
sit on a different panel, and quietly defeats the whole tint-in-UMG approach — one texture
is supposed to serve every state and every colour.

The failure is easy to ship and hard to see: the Figma art pages carry `__backdrop`
rectangles behind the components, so an export taken at the wrong scope looks perfect in a
viewer and is wrong in the engine.

Three checks, because "has an alpha channel" proves nothing on its own:
  1. mode is RGBA at all
  2. all four corners are fully transparent
  3. the fully-opaque share of the image is below a threshold — a glyph is ink on nothing;
     an icon whose pixels are ~all opaque is a filled plate
"""
from __future__ import annotations

import sys
from pathlib import Path

from PIL import Image

REPO = Path(__file__).resolve().parents[2]
OPAQUE_SHARE_LIMIT = 0.90   # above this, it is a plate, not a glyph


def check(p: Path) -> tuple[bool, str]:
    im = Image.open(p)
    if im.mode != "RGBA":
        return False, f"mode={im.mode}, no alpha channel"
    a = im.getchannel("A")
    w, h = im.size
    corners = [a.getpixel(xy) for xy in ((0, 0), (w - 1, 0), (0, h - 1), (w - 1, h - 1))]
    if any(c != 0 for c in corners):
        return False, f"corner alpha {corners} — background is baked in"
    hist = a.histogram()
    opaque = hist[255] / float(w * h)
    if opaque > OPAQUE_SHARE_LIMIT:
        return False, f"{opaque:.0%} of pixels fully opaque — looks like a filled plate"
    transparent = hist[0] / float(w * h)
    return True, f"{w}x{h} · {transparent:.0%} transparent · {opaque:.0%} opaque ink"


def main() -> int:
    root = REPO / (sys.argv[1] if len(sys.argv) > 1 else "Content/UI/Icons")
    pngs = sorted(root.rglob("*.png"))
    if not pngs:
        print(f"no PNGs under {root}")
        return 2
    bad = []
    for p in pngs:
        ok, why = check(p)
        rel = p.relative_to(root)
        if ok:
            print(f"  ok    {rel}  {why}")
        else:
            print(f"  FAIL  {rel}  {why}")
            bad.append(rel)
    print(f"\n{len(pngs) - len(bad)}/{len(pngs)} clean")
    if bad:
        print("REJECTED — do not import these:")
        for b in bad:
            print(f"  {b}")
        return 1
    print("All transparent. Safe to import.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

# ---------------------------------------------------------------------------
# HOW TO EXPORT SO THIS CHECK PASSES  (learned the hard way, 2 Aug 2026)
#
#   WRONG: Figma MCP `download_assets`
#          It composites the node against the PAGE BACKDROP. Every one of the
#          first 41 exports came back with corner alpha 255 — the dark
#          `__backdrop` rect (23,23,26) behind the icon pages, and the light
#          `__backdrop / parametric` rect (245,245,245) behind the rank page,
#          which made the rank marks a solid near-white square. All discarded.
#
#   RIGHT: Figma Plugin API `node.exportAsync({format:"PNG", constraint:{type:"SCALE", value:4}})`
#          via the `use_figma` tool. Renders the node in ISOLATION.
#          Verified: corner (0,0,0,0), 87-95% transparent, white ink intact.
#
# The components themselves are clean — `fills: []` on every one — so this was
# never an art problem. It was an export-path problem, and it is invisible in a
# preview: the icon looks perfect and the background only shows up in engine.
# ---------------------------------------------------------------------------
