#!/usr/bin/env python3
"""A tiny SVG rasteriser for the Figma stroke/polygon exports, built on Pillow alone.

    from svg_pillow import render          # render(src: Path, w: int, h: int) -> Image (RGBA)

WHY THIS EXISTS, WHEN `rasterize_svg.py` ALREADY HAS FIVE BACKENDS
------------------------------------------------------------------
On Windows, all five are unavailable and the failure is not recoverable by configuration:

  cairosvg       installs from pip, then cannot load `libcairo-2.dll` (no native cairo)
  rsvg-convert   Unix only
  qlmanage/sips  macOS only
  chrome/edge    Edge IS present and IS Chromium, but its headless `--screenshot` writes a
                 byte-identical blank frame regardless of page content OR css background --
                 verified directly, two different backgrounds, same MD5. `svglib` is no help
                 either: its renderer is reportlab's renderPM, which also wants cairo.

So the project had zero working rasterisers on the machine that owns the engine. This closes
that with no native dependency at all.

WHAT IT DELIBERATELY DOES *NOT* SUPPORT
---------------------------------------
This is not an SVG renderer. It handles exactly the subset Figma emits for the button/border
line art, and it RAISES on anything else rather than silently dropping it -- a rasteriser that
quietly skips a path it does not understand is how you ship an asset with a missing edge.

  supported : <path d="..."> with absolute M / L / H / V / Z
              stroke (solid or linearGradient), stroke-width, stroke-opacity, stroke-linecap
              fill (solid), fill-opacity
  raises on : curves (C/S/Q/T/A), relative commands, transforms, clips, masks, patterns,
              radial gradients, text, embedded images

ANTI-ALIASING IS BY SUPERSAMPLE, and that is load-bearing: `preflight_textures` rejects an
export with fewer than `MIN_AA_LEVELS` distinct partial-alpha values, because a 1-bit edge
looks jagged in engine. Drawing at SS x and box-filtering down produces real coverage values.
"""
from __future__ import annotations

import re
import xml.etree.ElementTree as ET
from pathlib import Path

from PIL import Image, ImageDraw

SS = 4  # supersample factor; 4 gives ~17 alpha levels on a diagonal, well over the preflight floor

_NS = "{http://www.w3.org/2000/svg}"
_NUM = re.compile(r"[-+]?(?:\d*\.\d+|\d+)(?:[eE][-+]?\d+)?")

# Relative commands and the curve types we do not flatten. `C` is absent on purpose -- it IS
# supported, by flattening (see `_parse_path`). Figma emits absolute commands only, and text
# converted to outlines arrives as M/H/C/Z, which is how the two `Sticker=Bonus` variants
# reached this file and were correctly REFUSED before cubic support existed.
_UNSUPPORTED_CMD = re.compile(r"[SsQqTtAamlhvcz]")

# Segments per cubic. 16 is well past the point where the LANCZOS downsample can tell, at the
# 12px-tall glyph sizes this file deals with.
_CUBIC_STEPS = 16


class UnsupportedSVG(RuntimeError):
    """Raised for any construct outside the documented subset. Never swallowed."""


def _parse_color(value: str | None, default=None):
    """'white' / '#RGB' / '#RRGGBB' -> (r,g,b). `none` and url(...) return None."""
    if value is None or value in ("none", ""):
        return default
    v = value.strip()
    if v.startswith("url("):
        return None
    if v == "white":
        return (255, 255, 255)
    if v == "black":
        return (0, 0, 0)
    if v.startswith("#"):
        h = v[1:]
        if len(h) == 3:
            h = "".join(c * 2 for c in h)
        if len(h) == 6:
            return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))
    raise UnsupportedSVG(f"unsupported colour {value!r}")


def _parse_path(d: str) -> list[tuple[float, float]]:
    """Absolute M/L/H/V/Z only -> a point list. Figma emits one subpath per line-art node."""
    if _UNSUPPORTED_CMD.search(d):
        raise UnsupportedSVG(f"path uses an unsupported command: {d[:60]!r}")

    pts: list[tuple[float, float]] = []
    x = y = 0.0
    for m in re.finditer(r"([MLHVC])([^MLHVCZ]*)", d):
        cmd, rest = m.group(1), m.group(2)
        nums = [float(n) for n in _NUM.findall(rest)]
        if cmd in ("M", "L"):
            for i in range(0, len(nums) - 1, 2):
                x, y = nums[i], nums[i + 1]
                pts.append((x, y))
        elif cmd == "H":
            for n in nums:
                x = n
                pts.append((x, y))
        elif cmd == "V":
            for n in nums:
                y = n
                pts.append((x, y))
        elif cmd == "C":
            # Flatten each cubic to line segments. Text converted to outlines (the BONUS/FREE
            # sticker labels) is the only thing in this asset set that uses curves.
            for i in range(0, len(nums) - 5, 6):
                x1, y1, x2, y2, x3, y3 = nums[i:i + 6]
                for s in range(1, _CUBIC_STEPS + 1):
                    t = s / _CUBIC_STEPS
                    u = 1.0 - t
                    bx = u*u*u*x + 3*u*u*t*x1 + 3*u*t*t*x2 + t*t*t*x3
                    by = u*u*u*y + 3*u*u*t*y1 + 3*u*t*t*y2 + t*t*t*y3
                    pts.append((bx, by))
                x, y = x3, y3
    if not pts:
        raise UnsupportedSVG(f"path produced no points: {d[:60]!r}")
    return pts


def _gradient_stops(root: ET.Element, url: str) -> list[tuple[float, tuple[int, int, int], float]]:
    """(offset, rgb, alpha) for a linearGradient referenced as url(#id)."""
    gid = url[url.find("#") + 1:url.rfind(")")]
    for grad in root.iter(f"{_NS}linearGradient"):
        if grad.get("id") != gid:
            continue
        stops = []
        for s in grad.iter(f"{_NS}stop"):
            off = float(s.get("offset", 0.0))
            col = _parse_color(s.get("stop-color"), (255, 255, 255))
            a = float(s.get("stop-opacity", 1.0))
            stops.append((off, col, a))
        if not stops:
            break
        return sorted(stops, key=lambda t: t[0])
    raise UnsupportedSVG(f"gradient {url!r} not found or empty")


def _sample(stops, t: float):
    """Linear interpolation between gradient stops at position t in 0..1."""
    t = max(0.0, min(1.0, t))
    prev = stops[0]
    for s in stops:
        if s[0] >= t:
            if s[0] == prev[0]:
                return s[1], s[2]
            f = (t - prev[0]) / (s[0] - prev[0])
            rgb = tuple(round(prev[1][i] + (s[1][i] - prev[1][i]) * f) for i in range(3))
            return rgb, prev[2] + (s[2] - prev[2]) * f
        prev = s
    return stops[-1][1], stops[-1][2]


def render(src: Path, w: int, h: int) -> Image.Image:
    """Rasterise `src` to an RGBA image of exactly w x h, transparent where there is no ink."""
    tree = ET.parse(src)
    root = tree.getroot()

    vb = root.get("viewBox")
    if vb:
        vx, vy, vw, vh = (float(n) for n in _NUM.findall(vb)[:4])
    else:
        vx, vy = 0.0, 0.0
        vw, vh = float(root.get("width", w)), float(root.get("height", h))

    W, H = w * SS, h * SS
    sx, sy = W / vw, H / vh
    canvas = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    draw = ImageDraw.Draw(canvas)

    def to_px(p):
        return ((p[0] - vx) * sx, (p[1] - vy) * sy)

    for node in root.iter(f"{_NS}path"):
        d = node.get("d")
        if not d:
            continue
        pts = [to_px(p) for p in _parse_path(d)]

        fill = _parse_color(node.get("fill"), None)
        if fill is not None and len(pts) >= 3:
            fa = float(node.get("fill-opacity", 1.0))
            draw.polygon(pts, fill=(*fill, round(255 * fa)))

        stroke_raw = node.get("stroke")
        if not stroke_raw or stroke_raw == "none":
            continue

        sw = max(1.0, float(node.get("stroke-width", 1.0)) * ((sx + sy) / 2.0))
        so = float(node.get("stroke-opacity", 1.0))

        if stroke_raw.startswith("url("):
            # Gradient stroke: walk the polyline and emit short segments, colouring each by its
            # position along the TOTAL length. Figma's button borders use only axis-aligned
            # gradients, so arc-length position is the correct parameter.
            stops = _gradient_stops(root, stroke_raw)
            seglens = [((pts[i + 1][0] - pts[i][0]) ** 2 + (pts[i + 1][1] - pts[i][1]) ** 2) ** 0.5
                       for i in range(len(pts) - 1)]
            total = sum(seglens) or 1.0
            walked = 0.0
            for i, L in enumerate(seglens):
                steps = max(1, int(L / 2))
                for s in range(steps):
                    t0, t1 = s / steps, (s + 1) / steps
                    a = (pts[i][0] + (pts[i + 1][0] - pts[i][0]) * t0,
                         pts[i][1] + (pts[i + 1][1] - pts[i][1]) * t0)
                    b = (pts[i][0] + (pts[i + 1][0] - pts[i][0]) * t1,
                         pts[i][1] + (pts[i + 1][1] - pts[i][1]) * t1)
                    rgb, ga = _sample(stops, (walked + L * (t0 + t1) / 2) / total)
                    draw.line([a, b], fill=(*rgb, round(255 * ga * so)), width=round(sw))
                walked += L
        else:
            col = _parse_color(stroke_raw, (255, 255, 255))
            draw.line(pts, fill=(*col, round(255 * so)), width=round(sw), joint="curve")

    # Box-filter down. This is where the anti-aliasing comes from.
    return canvas.resize((w, h), Image.LANCZOS)


if __name__ == "__main__":
    import sys
    p = Path(sys.argv[1])
    scale = float(sys.argv[2]) if len(sys.argv) > 2 else 4.0
    tree = ET.parse(p).getroot()
    w = round(float(tree.get("width", 100)) * scale)
    h = round(float(tree.get("height", 100)) * scale)
    out = p.with_suffix(".png")
    render(p, w, h).save(out)
    print(f"{p.name} -> {out.name}  {w}x{h}")
