#!/usr/bin/env python3
"""BN49 — the button-prompt glyphs, drawn from `21:43024`'s own path data.

    python3 Tools/bn/bn49_prompt_glyphs.py     # writes Saved/FigmaExport/T_BN_Prompt*.png

WHERE THE NUMBERS COME FROM. `download_assets` (Figma MCP) on `Button Prompts` `21:43024`
returns five 20x20 SVGs and they are all the same construction: a filled white disc with a glyph
SUBTRACTED out of it (`path id="Subtract"`, `fill="white"`, even-odd). Only the subtracted part
differs. The Menu one, verbatim from that path:

    disc   M10 0 C15.5228 0 20 4.47715 20 10 ... 0 4.47715 4.47715 0 10 0 Z    (centre 10,10 r10)
    bar    M5.5 12.5 C5.22386 12.5 5 12.7239 5 13 ... H14.5 ... H5.5 Z
    bar    M5.5  9.5 ...
    bar    M5.5  6.5 ...

so three bars, x 5..15, 1 tall, rounded caps of r 0.5, at y centres 7 / 10 / 13 — pitch 3.

DRAWN, NOT RASTERISED. There is no local SVG rasteriser on this machine (see
`bn46_roster_icons.py` for the three that were tried and how each failed — one of them silently
returns a plausible-looking scribble, which is worse than an error). These are a circle and
three rounded rectangles, so drawing them from the path data is exact rather than approximate.

BACK IS ADAPTED, AND SAYS SO. The reference's Back glyph is a gamepad face button — the letter
"B" subtracted from the same disc. This build is keyboard-driven, and there is no measured
keyboard variant in the file, so it uses the same disc-minus-glyph construction with a LEFT
CHEVRON instead of a letter. A letter was tried first and rejected on evidence: "ESC" at a size
that fits a 20px disc is 7pt, which does not read at all — the disc came out blank. A chevron
reads at 20px and means the same thing. Swapping to a real `CommonActionWidget` later is a
widget change, not a layout one.
"""
from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw

REPO = Path(__file__).resolve().parents[2]
OUT = REPO.parent / "Saved" / "FigmaExport"

BOX = 20      # the SVG viewBox, in design pixels
SS = 8        # supersample; 20 * 8 = 160 px before the reduction
OUT_PX = 40   # 2x the measured 20x20, a multiple of 4 (the importer rejects anything else),
              # and 160 -> 40 is an EXACT 4:1 box reduction.

# `Subtract`, verbatim: bars span x 5..15 with r-0.5 caps, 1 tall, at y centres 7 / 10 / 13.
BAR_X0, BAR_X1 = 5.0, 15.0
BAR_HALF = 0.5
BAR_YS = (7.0, 10.0, 13.0)


def _disc() -> Image.Image:
    n = BOX * SS
    img = Image.new("RGBA", (n, n), (255, 255, 255, 0))
    ImageDraw.Draw(img).ellipse([0, 0, n - 1, n - 1], fill=(255, 255, 255, 255))
    return img


def _menu() -> Image.Image:
    img = _disc()
    d = ImageDraw.Draw(img)
    for y in BAR_YS:
        # Knock the bar OUT of the disc — the reference subtracts, it does not overlay a dark
        # bar. Punching alpha to 0 is what makes the glyph read on any background.
        d.rounded_rectangle(
            [BAR_X0 * SS, (y - BAR_HALF) * SS, BAR_X1 * SS, (y + BAR_HALF) * SS],
            radius=BAR_HALF * SS, fill=(0, 0, 0, 0))
    return img


def _back() -> Image.Image:
    """The disc with a left chevron knocked out, on Menu's construction and stroke weight."""
    img = _disc()
    d = ImageDraw.Draw(img)
    # Matched to the bars: 1 unit thick with round caps, centred in the same 5..15 field.
    w = int(BAR_HALF * 2 * SS)
    tip, top, bot, back = 7.5 * SS, 6.5 * SS, 13.5 * SS, 12.5 * SS
    mid = 10.0 * SS
    d.line([(back, top), (tip, mid), (back, bot)], fill=(0, 0, 0, 0), width=w, joint="curve")
    # PIL's `line` leaves square ends; round them so the chevron matches the bars' caps.
    for x, y in ((back, top), (tip, mid), (back, bot)):
        d.ellipse([x - w / 2, y - w / 2, x + w / 2, y + w / 2], fill=(0, 0, 0, 0))
    return img


def _emit(name: str, img: Image.Image) -> None:
    img = img.resize((OUT_PX, OUT_PX), Image.LANCZOS)
    a = [p[3] for p in img.getdata()]
    assert min(a) == 0, f"{name}: no transparent pixels — this is a square, not a disc"
    assert max(a) == 255, f"{name}: nothing is opaque — nothing drew"
    assert len(set(a)) >= 8, f"{name}: {len(set(a))} alpha levels — the edge is 1-bit"
    path = OUT / f"{name}.png"
    img.save(path)
    print(f"{path}  {img.size}  alpha {min(a)}..{max(a)}  levels {len(set(a))}")


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    _emit("T_BN_PromptMenu", _menu())
    _emit("T_BN_PromptBack", _back())
    _emit("T_BN_PromptDisc", _disc())


if __name__ == "__main__":
    main()
