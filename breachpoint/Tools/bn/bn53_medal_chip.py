#!/usr/bin/env python3
"""BN45 — the medal chip, drawn from Figma `SET Feedback / Medal Chip` (62:106) path data.

    python3 Tools/bn/bn53_medal_chip.py            # writes Saved/FigmaExport/T_BN_MedalChip.png
    python3 Tools/bn/bn53_medal_chip.py --import   # ...and imports it through the live editor

WHY DRAWN. The symbol's PNG export is fully opaque (alpha 255 everywhere — the known Figma symbol
trap), and its two SVG layers are two hexagons whose points are given outright:

    plate  (45 x 55.2, #8C949E, 1px black stroke)
        M22.5 0.57 L44.5 12.57 V36.57 L22.5 54.57 L0.5 36.57 V12.57 Z
    inlay  (27 x 35.2, #F2C752)
        M13.5 0.57 L26.5 7.57 V23.57 L13.5 34.57 L0.5 23.57 V7.57 Z

Two polygons are exact to draw. CENTRED: HUD-AUDIT.md §Medal Chip records the imported asset with
"plate and inlay both at (0,0); the inlay hangs off the corner" — that is the defect this replaces.
The chip is 64 x 64 in the set; both hexagons sit on its centre. Rendered at 2x (128) with an 8x
supersample, an exact 4:1 reduction.
"""
import sys
from pathlib import Path
from PIL import Image, ImageDraw

REPO = Path(__file__).resolve().parents[2]
OUT = REPO.parent / "Saved" / "FigmaExport"
CHIP, SS, OUT_PX = 64.0, 8, 128
PLATE = [(22.5, 0.57), (44.5, 12.57), (44.5, 36.57), (22.5, 54.57), (0.5, 36.57), (0.5, 12.57)]
INLAY = [(13.5, 0.57), (26.5, 7.57), (26.5, 23.57), (13.5, 34.57), (0.5, 23.57), (0.5, 7.57)]
PLATE_SIZE, INLAY_SIZE = (45.0, 55.2156), (27.0, 35.2229)


def centred(points, size):
    ox, oy = (CHIP - size[0]) / 2, (CHIP - size[1]) / 2
    return [((x + ox) * SS, (y + oy) * SS) for x, y in points]


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    n = int(CHIP * SS)
    img = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.polygon(centred(PLATE, PLATE_SIZE), fill=(0x8C, 0x94, 0x9E, 255), outline=(0, 0, 0, 255), width=SS)
    d.polygon(centred(INLAY, INLAY_SIZE), fill=(0xF2, 0xC7, 0x52, 255), outline=(0, 0, 0, 255), width=SS)
    img = img.resize((OUT_PX, OUT_PX), Image.LANCZOS)
    a = [p[3] for p in img.getdata()]
    assert min(a) == 0 and max(a) == 255, "chip must have transparent corners and an opaque body"
    # centred: the alpha bounding box is symmetric about the middle
    bbox = img.getbbox()
    assert abs((bbox[0] + bbox[2]) / 2 - OUT_PX / 2) <= 1 and abs((bbox[1] + bbox[3]) / 2 - OUT_PX / 2) <= 1, bbox
    path = OUT / "T_BN_MedalChip.png"
    img.save(path)
    print(path, img.size, "bbox", bbox)
    if "--import" in sys.argv:
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        import bn11_lib as B
        r = B.mcp.call("editor_toolset.toolsets.texture.TextureTools", "import_file",
                       source_file=str(path), folder_path="/Game/BN/UI/Art", asset_name="T_BN_MedalChip")
        print("imported", r)


if __name__ == "__main__":
    main()
