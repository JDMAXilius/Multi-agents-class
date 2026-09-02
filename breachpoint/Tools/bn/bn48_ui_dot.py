#!/usr/bin/env python3
"""BN48 — the filled UI dot, for the profile bar's button affordances.

    python3 Tools/bn/bn48_ui_dot.py        # writes Saved/FigmaExport/T_BN_UIDot.png

WHY THIS EXISTS. `Profile Bar` `21:43023` puts a filled 12 x 12 ellipse under each of the three
button glyphs (`Hold Menu Button`/`Fill`, `View Button`/`Fill`, `Menu Button`/`Fill`). Slate has
no circle primitive, and a `UImage` with no brush is a white RECTANGLE — which is exactly what
shipped: three white squares where the reference has three dots.

`T_CarouselDot_Active` is the wrong asset for it: that is a 7px RING with a small filled centre,
authored for the carousel. This is a plain solid disc.

Rendered rather than downloaded because there is nothing to download — the reference draws an
`<ellipse>`, and the project has no local SVG rasteriser (see `bn46_roster_icons.py` for the
three tools that were tried and why each failed).
"""
from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw

REPO = Path(__file__).resolve().parents[2]
OUT = REPO.parent / "Saved" / "FigmaExport"

SS = 8        # supersample; 24 * 8 = 192 px before the reduction
OUT_PX = 24   # 2x the measured 12 x 12 dot, and 192 -> 24 is an EXACT 8:1 box reduction.
              # Also a multiple of 4, which the Unreal importer requires — it rejects any
              # other edge outright with "produced no assets" (learned on the rank crest).


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    n = OUT_PX * SS
    img = Image.new("RGBA", (n, n), (255, 255, 255, 0))
    # Inset by one supersampled pixel so the disc's antialiased edge is not clipped by the
    # canvas bound — a circle drawn flush to the edge loses its top and bottom row of coverage.
    ImageDraw.Draw(img).ellipse([SS, SS, n - SS, n - SS], fill=(255, 255, 255, 255))
    img = img.resize((OUT_PX, OUT_PX), Image.LANCZOS)

    alphas = [p[3] for p in img.getdata()]
    # The failure this guards is the one being fixed: a full-coverage alpha channel is a
    # SQUARE, and a square is what was on screen.
    assert min(alphas) == 0, "dot: no transparent pixels — this is a square, not a circle"
    assert max(alphas) == 255, "dot: nothing is opaque — the disc did not draw"
    assert len(set(alphas)) >= 8, f"dot: {len(set(alphas))} alpha levels — the edge is 1-bit"

    path = OUT / "T_BN_UIDot.png"
    img.save(path)
    print(f"{path}  {img.size}  alpha {min(alphas)}..{max(alphas)}  levels {len(set(alphas))}")


if __name__ == "__main__":
    main()
