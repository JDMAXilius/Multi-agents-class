#!/usr/bin/env python3
"""BN50 — the carousel's paging chevron, for the news card's dot row.

    python3 Tools/bn/bn50_carousel_arrow.py     # writes Saved/FigmaExport/T_BN_CarouselArrow.png

The reference dot row reads  ‹ ○ ● ○ ○ ›  — a small chevron each side of the dots. One texture,
pointing LEFT; the right-hand one is the same image with RenderTransform scale X = -1, so there is
nothing to keep in sync. Drawn, not downloaded, for the reason `bn46_roster_icons.py` records:
there is no working SVG rasteriser on this machine, and a chevron is two strokes.
"""
from pathlib import Path
from PIL import Image, ImageDraw

REPO = Path(__file__).resolve().parents[2]
OUT = REPO.parent / "Saved" / "FigmaExport"
SS, OUT_PX = 8, 16   # 16 px, a multiple of 4 (importer), drawn at 8x and box-reduced

def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    n = OUT_PX * SS
    img = Image.new("RGBA", (n, n), (255, 255, 255, 0))
    d = ImageDraw.Draw(img)
    w = int(1.6 * SS)
    tip, back, top, bot, mid = 5.5 * SS, 10.5 * SS, 3.5 * SS, 12.5 * SS, 8.0 * SS
    d.line([(back, top), (tip, mid), (back, bot)], fill=(255, 255, 255, 255), width=w, joint="curve")
    for x, y in ((back, top), (tip, mid), (back, bot)):
        d.ellipse([x - w / 2, y - w / 2, x + w / 2, y + w / 2], fill=(255, 255, 255, 255))
    img = img.resize((OUT_PX, OUT_PX), Image.LANCZOS)
    a = [p[3] for p in img.getdata()]
    assert max(a) == 255 and min(a) == 0 and len(set(a)) >= 8, "chevron did not draw cleanly"
    path = OUT / "T_BN_CarouselArrow.png"
    img.save(path)
    print(path, img.size)

if __name__ == "__main__":
    main()
