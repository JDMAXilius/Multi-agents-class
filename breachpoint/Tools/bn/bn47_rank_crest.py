#!/usr/bin/env python3
"""BN47 — the career-rank crest, keyed off the Figma export.

    python3 Tools/bn/bn47_rank_crest.py     # writes Saved/FigmaExport/T_BN_Fig_RankCrest_01.png

WHERE IT COMES FROM. `download_assets` (Figma MCP) on node `21:32826` "Progression Button"
returns two raw images. One is the 512x512 UNSC eagle already imported as
`T_BN_Fig_Watermark_01` — verified byte-identical by md5, so it is not re-imported here. The
other is this crest: `112 x 196`, the bronze-diamond insignia on its dark ribbon.

WHY IT NEEDS KEYING. Figma hands it back as a **JPEG**, so it has no alpha at all, and the
insignia is composited on a WHITE board. Imported as-is it is a white rectangle with a crest
floating in it — the exact "blank white box" failure the roster row's optional binds were held
back for. The board is pure white and the art is dark bronze on near-black, so a straight
luminance key separates them cleanly with no spill.

The crest is `Corporal Grade I` (`I21:32826;7:3726;7:3776`, 52 x 92), not Sergeant: the
`5. Sergeant Grade 1` instance in the same frame is HIDDEN in the source. This ships what the
node actually draws rather than the rank the placeholder text names — if that reads wrong on
screen it is a data mismatch to fix in ini, not art to redraw.
"""
from __future__ import annotations

from pathlib import Path

from PIL import Image

REPO = Path(__file__).resolve().parents[2]
OUT = REPO.parent / "Saved" / "FigmaExport"
SRC = OUT / "rank" / "rank_raw_2.jpeg"

# Measured slot, node `21:32826` content: `Corporal Grade I` is 52 x 92. The export is 112 x 196
# — a hair over 2x — so 104 x 184 is the exact 2x box and both edges are multiples of 4, which
# the Unreal importer requires (it rejects anything else outright: "produced no assets").
OUT_BOX = (104, 184)

# Above this luminance a pixel is board, not art. The board is 255 and the ribbon's lightest
# bronze measures well under 200, so 240 keys the white without eating the highlights.
WHITE_CUT = 240
# Below this it is solidly art. Between the two, alpha ramps — that is the antialiased edge.
ART_CUT = 200


def main() -> None:
    src = Image.open(SRC).convert("RGB").resize(OUT_BOX, Image.LANCZOS)
    px = src.load()
    alpha = Image.new("L", OUT_BOX)
    ap = alpha.load()
    for y in range(OUT_BOX[1]):
        for x in range(OUT_BOX[0]):
            r, g, b = px[x, y]
            lum = (r * 299 + g * 587 + b * 114) // 1000
            if lum >= WHITE_CUT:
                ap[x, y] = 0
            elif lum <= ART_CUT:
                ap[x, y] = 255
            else:
                # Linear ramp across the antialiased boundary, so the silhouette keeps a soft
                # edge instead of the 1-bit stairstep a hard threshold would leave.
                ap[x, y] = int(255 * (WHITE_CUT - lum) / (WHITE_CUT - ART_CUT))

    out = src.convert("RGBA")
    out.putalpha(alpha)

    a = list(alpha.getdata())
    assert max(a) == 255, "crest: nothing is opaque — the key ate the art"
    assert min(a) == 0, "crest: nothing is transparent — the white board survived the key"
    # A crest is mostly ribbon: if the opaque share is tiny the key inverted, if it is nearly
    # everything the key did not fire. Both are silent failures on screen, so assert the band.
    solid = sum(1 for v in a if v > 200) / len(a)
    assert 0.15 < solid < 0.75, f"crest: {solid:.0%} opaque — the key is wrong"

    path = OUT / "T_BN_Fig_RankCrest_01.png"
    out.save(path)
    print(f"{path}  {out.size}  alpha {min(a)}..{max(a)}  opaque {solid:.0%}")


if __name__ == "__main__":
    main()
