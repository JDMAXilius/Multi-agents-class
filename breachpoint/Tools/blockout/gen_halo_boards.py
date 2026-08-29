#!/usr/bin/env python3
"""BN28 — Halo Infinite level-layout boards, reconstructed from documented callouts.

    python3 Tools/blockout/gen_halo_boards.py [--png]

Draws DIAGRAMMATIC top-view boards of Halo Infinite's own multiplayer arena
maps — the reference genre the founder asked for — from a per-map gazetteer
(docs/design/reference/halo_maps.json) whose every zone position comes from
documented callout guides. The cloud cannot fetch the real HCS board images
(egress proxy), so these are honest reconstructions: adjacency and floors are
sourced, exact proportions are NOT — every sheet says so on its face.

Zone boxes sit on a coarse layout grid. Floors: L0 sunken = dark, L1 main =
white, L2 upper = dashed. Connections: solid = walk, tick-marks = stairs/ramp,
arrow = one-way (lift/cannon). Pickups are lettered badges on their zone.
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
GAME = HERE.parents[1]
DATA = GAME / "docs" / "design" / "reference" / "halo_maps.json"
OUT = GAME / "docs" / "design" / "reference" / "boards"

CELL = 62          # px per grid cell
STYLE = """
  .paper{fill:#ffffff}
  .frame{fill:none;stroke:#000;stroke-width:2.5}
  .z0{fill:#3a3a3a;stroke:#000;stroke-width:1.6;rx:7}
  .z1{fill:#ffffff;stroke:#000;stroke-width:1.8;rx:7}
  .z2{fill:#f0f0f0;stroke:#000;stroke-width:1.6;stroke-dasharray:7 4;rx:7}
  .t0{font:bold 11px monospace;fill:#ffffff}
  .t1{font:bold 11px monospace;fill:#000}
  .sub{font:9px monospace;fill:#555}
  .sub0{font:9px monospace;fill:#cccccc}
  .conn{stroke:#000;stroke-width:2.2;fill:none}
  .stair{stroke:#000;stroke-width:1.2}
  .badge{fill:#000}
  .badgetxt{font:bold 10px monospace;fill:#fff}
  .side{font:bold 13px monospace;fill:#000;letter-spacing:2px}
  .big{font:bold 16px monospace;fill:#000;letter-spacing:1px}
  .small{font:9.5px monospace;fill:#000}
  .note{font:italic 10px monospace;fill:#333}
"""


def draw_map(m):
    gw, gh = m["grid"]
    w = gw * CELL + 160
    h = gh * CELL + 260
    X = lambda c: 80 + c * CELL
    Y = lambda r: 120 + r * CELL
    o = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d">' % (w, h, w, h),
         "<style>%s</style>" % STYLE,
         '<rect class="paper" width="%d" height="%d"/>' % (w, h),
         '<rect class="frame" x="8" y="8" width="%d" height="%d"/>' % (w - 16, h - 16)]
    o.append('<text class="big" x="80" y="52">%s</text>' % m["title"].upper())
    o.append('<text class="small" x="80" y="72">%s</text>' % m["subtitle"])
    o.append('<text class="note" x="80" y="90">RECONSTRUCTION from documented callouts — '
             'adjacency and floors sourced, proportions diagrammatic, NOT measured.</text>')

    zones = {z["name"]: z for z in m["zones"]}

    def center(z):
        c = z["cell"]
        return (X(c[0]) + c[2] * CELL / 2, Y(c[1]) + c[3] * CELL / 2)

    # connections under zones
    for c in m.get("connections", []):
        a, b = zones[c[0]], zones[c[1]]
        (x1, y1), (x2, y2) = center(a), center(b)
        o.append('<line class="conn" x1="%g" y1="%g" x2="%g" y2="%g"/>' % (x1, y1, x2, y2))
        kind = c[2] if len(c) > 2 else "walk"
        mx, my = (x1 + x2) / 2, (y1 + y2) / 2
        if kind == "stairs":
            dx, dy = x2 - x1, y2 - y1
            L = max((dx * dx + dy * dy) ** 0.5, 1)
            px, py = -dy / L * 7, dx / L * 7
            for t in (-0.12, 0.0, 0.12):
                cx2, cy2 = mx + dx * t, my + dy * t
                o.append('<line class="stair" x1="%g" y1="%g" x2="%g" y2="%g"/>'
                         % (cx2 - px, cy2 - py, cx2 + px, cy2 + py))
        elif kind == "lift":
            o.append('<polygon points="%g,%g %g,%g %g,%g"/>' % (mx - 6, my + 6, mx + 6, my + 6, mx, my - 8))

    for z in m["zones"]:
        c = z["cell"]
        fl = z.get("floor", 1)
        o.append('<rect class="z%d" x="%g" y="%g" width="%g" height="%g" rx="8"/>'
                 % (fl, X(c[0]), Y(c[1]), c[2] * CELL, c[3] * CELL))
        cx, cy = center(z)
        if z.get("label_at") == "top":
            cy = Y(c[1]) + 16
        tcls = "t0" if fl == 0 else "t1"
        scls = "sub0" if fl == 0 else "sub"
        words = z["name"].upper().split()
        lines = [" ".join(words[:2])] if len(words) <= 2 else [" ".join(words[:1]), " ".join(words[1:])]
        if len(z["name"]) > 12 and len(words) >= 2:
            lines = [" ".join(words[: len(words) // 2]), " ".join(words[len(words) // 2:])]
        y0 = cy - (len(lines) - 1) * 6
        for i, ln in enumerate(lines):
            o.append('<text class="%s" text-anchor="middle" x="%g" y="%g">%s</text>'
                     % (tcls, cx, y0 + i * 12 + 4, ln))
        if z.get("tag"):
            o.append('<text class="%s" text-anchor="middle" x="%g" y="%g">%s</text>'
                     % (scls, cx, y0 + len(lines) * 12 + 4, z["tag"]))

    for pk in m.get("pickups", []):
        z = zones[pk["zone"]]
        c = z["cell"]
        bx, by = X(c[0]) + c[2] * CELL - 13, Y(c[1]) + 13
        o.append('<circle class="badge" cx="%g" cy="%g" r="10"/>' % (bx, by))
        o.append('<text class="badgetxt" text-anchor="middle" x="%g" y="%g">%s</text>'
                 % (bx, by + 3.5, pk["glyph"]))

    for side, sx, anchor in (("A", 80, "start"), ("B", w - 80, "end")):
        label = m.get("side_%s" % side.lower())
        if label:
            o.append('<text class="side" x="%g" y="%g" text-anchor="%s">%s</text>'
                     % (sx, Y(gh) + 34, anchor, label.upper()))

    # legend
    ly = h - 96
    o.append('<rect class="frame" x="80" y="%d" width="%d" height="66"/>' % (ly, w - 200))
    items = [("z0", "sunken / bottom floor"), ("z1", "main floor"), ("z2", "upper (dashed)")]
    for i, (cls, txt) in enumerate(items):
        o.append('<rect class="%s" x="%d" y="%d" width="22" height="14" rx="4"/>' % (cls, 92 + i * 190, ly + 12))
        o.append('<text class="small" x="%d" y="%d">%s</text>' % (120 + i * 190, ly + 23, txt))
    o.append('<text class="small" x="92" y="%d">conn: line=walk, ticks=stairs/ramp, triangle=lift · badges: %s</text>'
             % (ly + 48, m.get("badge_key", "")))
    o.append('<text class="small" x="92" y="%d">sources: %s</text>' % (ly + 62, m.get("sources", "")))
    o.append("</svg>")
    return "\n".join(o), w, h


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--png", action="store_true")
    args = ap.parse_args()
    maps = json.loads(DATA.read_text(encoding="utf-8"))["maps"]
    OUT.mkdir(parents=True, exist_ok=True)
    for m in maps:
        svg, w, h = draw_map(m)
        f = OUT / ("%s.svg" % m["id"])
        f.write_text(svg, encoding="utf-8")
        print("wrote %s" % f.name)
        if args.png:
            chromium = shutil.which("chromium") or "/opt/pw-browsers/chromium"
            subprocess.run([chromium, "--headless", "--disable-gpu", "--no-sandbox",
                            "--force-device-scale-factor=2", "--hide-scrollbars",
                            "--default-background-color=FFFFFFFF",
                            "--screenshot=%s" % f.with_suffix(".png"),
                            "--window-size=%d,%d" % (w + 24, h + 70), f.as_uri()],
                           check=True, capture_output=True, timeout=120)
            print("  rasterized %s.png" % m["id"])


if __name__ == "__main__":
    main()
