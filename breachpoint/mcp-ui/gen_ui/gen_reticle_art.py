#!/usr/bin/env python3
"""Author the one BN reticle the shipped set does not cover.

    python3 mcp-ui/gen_ui/gen_reticle_art.py

`Content/UI/HUD/` ships HUD_Reticle_AR, _BR, _Magnum and _EnemyState. BN's weapon table has
four rows — Rifle, Pistol, Shotgun, Knife — so Rifle -> AR and Pistol -> Magnum reuse shipped
art (ASSET-RULES §1, reuse before authoring) and only Shotgun has nothing. Knife is melee and
falls back to the HUD's default.

WHY THIS IS AUTHORED AND NOT EXPORTED
-------------------------------------
`download_assets` on `62:26` (SET Reticle / Shotgun) returns a PNG whose opaque share is
**1.000** — the page ground is composited into it, exactly the trap in mcp-ui/GOTCHAS.md #9.
Dropping that texture on the HUD would paint a 52px opaque plate over screen centre.

The same call's SVG is clean and fully specifies the shape, so the ring is drawn from the
SVG's own numbers instead:

    <circle id="Ring" opacity="0.85" cx="26" cy="26" r="25.3" stroke="white" stroke-width="1.4"/>

`svg_pillow` raises on `<circle>` by design (absolute M/L/H/V/Z only), and a ring is one
Pillow call, so there is no path grammar to lose by drawing it directly.

WHITE, NOT CYAN. Every shipped HUD_Reticle_* is white line art that the widget tints; the
reticle's rest/enemy/ally colour is `hud/self` vs `hud/threat` at runtime, not baked here.
"""
from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw

OUT = Path(__file__).resolve().parents[2] / "Content/BN/UI/Assets"

BOX, CX, CY, R, STROKE, OPACITY = 52.0, 26.0, 26.0, 25.3, 1.4, 0.85
# 4x, then box-filtered down: the ring is a curve, and Pillow's ellipse outline is 1-bit.
# preflight_textures rejects an export with too few partial-alpha levels, and supersampling
# is what produces real coverage values (the same reason svg_pillow supersamples).
SS = 4
# The shipped reticles are 40-ish; this is authored at 2x the 52 design box so the DPI curve
# (ShortestSide/720 -> 1.5x at 1080p) has headroom.
FINAL = 104


def ring() -> Image.Image:
    n = round(BOX * SS)
    im = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    r, w = R * SS, STROKE * SS
    cx, cy = CX * SS, CY * SS
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=(255, 255, 255, 255), width=round(w))
    im = im.resize((FINAL, FINAL), Image.LANCZOS)
    # the SVG's own layer opacity, applied to alpha rather than baked into RGB
    a = im.getchannel("A").point(lambda v: round(v * OPACITY))
    im.putalpha(a)
    return im


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    im = ring()
    path = OUT / "BN_Reticle_Shotgun.png"
    im.save(path)
    levels = len({p for p in im.getchannel("A").getdata() if 0 < p < 255})
    print(f"  BN_Reticle_Shotgun  {im.size[0]}x{im.size[1]}  partial-alpha levels={levels}  -> {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
