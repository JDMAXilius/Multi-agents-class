#!/usr/bin/env python3
"""BN43 — the Carousel Dot textures, rendered from the Figma SVG spec.

    python3 Tools/bn/bn43_carousel_dots.py            # writes Saved/FigmaExport/*.png

WHERE THE NUMBERS COME FROM. Pulled through the Figma MCP off `12:38169` "Carousel Dot"
(page `6:26` Navigation), whose two symbols export as these SVGs verbatim:

    Active   <circle cx=3.5 cy=3.5 r=3 stroke=white/>          on a 7x7 box
             <circle cx=2   cy=2   r=2 fill=white/>            on a 4x4 box
    Inactive <circle cx=3.5 cy=3.5 r=3 stroke=white opacity=0.5/>

So a dot is a 7x7 one-pixel white RING, and Active adds a filled r=2 centre. That is the
whole asset — there is no bitmap in Figma to download.

WHY THIS SCRIPT RATHER THAN THE PNG EXPORT. `download_assets` on those symbol nodes returns
a FULLY OPAQUE 28x28 png: Figma bakes the symbol's white artboard into the export, so the
alpha channel comes back 255 everywhere and the "dot" is a white square in UE. Verified —
min and max alpha were both 255 on both states. The SVG is the only faithful source.

WHY NOT `mcp-ui/gen_ui/svg_pillow.py`. That rasteriser is deliberately path-only and RAISES
on anything else rather than dropping it silently; `<circle>` is not in its grammar. These
are two circles, so they are drawn as circles here at SS x supersampling and downsampled,
which is exact rather than approximated.

The other committed route, `mcp-ui/gen_ui/figma_export.py` (Figma REST), needs FIGMA_TOKEN in
Tools/env.local and none is set on this machine — so it could not be used for this pass.
"""
from pathlib import Path

from PIL import Image, ImageDraw

BOX = 7          # the SVG viewBox, in design pixels
SS = 32          # supersample factor; 7*32 = 224 px before the downsample
OUT_PX = 112     # final texture edge (16x the design box, power-of-two friendly enough)

REPO = Path(__file__).resolve().parents[2]
OUT = REPO.parent / "Saved" / "FigmaExport"


def _dot(filled: bool, opacity: float) -> Image.Image:
    """One dot at OUT_PX square, drawn at SS and downsampled for a clean edge."""
    n = BOX * SS
    img = Image.new("RGBA", (n, n), (255, 255, 255, 0))
    d = ImageDraw.Draw(img)

    a = int(round(255 * opacity))
    # stroke: r=3 centred at 3.5 with the SVG default stroke-width of 1, so the ring band
    # runs r 2.5..3.5 — drawn as an outlined ellipse of width 1 design px.
    r, cx = 3.0 * SS, 3.5 * SS
    d.ellipse([cx - r, cx - r, cx + r, cx + r], outline=(255, 255, 255, a), width=SS)

    if filled:
        # the Active centre: a SEPARATE 4x4 symbol, r=2 fill, so it is centred in the 7x7.
        r2 = 2.0 * SS
        d.ellipse([cx - r2, cx - r2, cx + r2, cx + r2], fill=(255, 255, 255, a))

    img = img.resize((OUT_PX, OUT_PX), Image.LANCZOS)
    # LANCZOS overshoots on a hard edge — it pushed the half-opacity ring's peak alpha to 143
    # where the SVG says 128. Ringing above the source is a resampling artifact, never the
    # design, so clamp the channel back to the state's own ceiling.
    ceil = int(round(255 * opacity))
    r, g, b, al = img.split()
    al = al.point(lambda v: min(v, ceil))
    return Image.merge("RGBA", (r, g, b, al))


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    for name, filled, op in (("T_CarouselDot_Active", True, 1.0),
                             ("T_CarouselDot_Inactive", False, 0.5)):
        img = _dot(filled, op)
        alphas = [p[3] for p in img.getdata()]
        # The failure this guards is the exact one the PNG export hit: an image whose alpha
        # never drops to 0 is a filled square, not a dot, and it will render as one.
        assert min(alphas) == 0, f"{name}: no transparent pixels — this is not a dot"
        # The ceiling is the STATE'S OWN opacity, not 255: Inactive is opacity=0.5 in the SVG,
        # so a peak near 127 is correct there and demanding 255 would fail a good render.
        want = int(round(255 * op))
        assert abs(max(alphas) - want) <= 2, (
            f"{name}: peak alpha {max(alphas)}, expected ~{want} for opacity {op}")
        path = OUT / f"{name}.png"
        img.save(path)
        print(f"{path}  {img.size}  alpha {min(alphas)}..{max(alphas)}")


if __name__ == "__main__":
    main()
