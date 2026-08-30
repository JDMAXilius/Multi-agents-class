#!/usr/bin/env python3
"""BN30 — the modular kit CATALOG: the 12 individual blockout assets.

    python3 Tools/blockout/gen_kit_catalog.py --png

Founder correction (30 Aug): "have the individual assets into a couple
screenshots, not the level. There should not be more than 12 modular
individual assets." This renders exactly that: TWO catalog sheets (K-101,
K-102), six asset cards each — every card one asset drawn alone in 3/4,
with its name, canonical dimensions and its scaling rule.

THE TWELVE (dims in metres, from the research sheet + the level schedule):
  1  BLK_Floor_400    4.0 x 4.0 x 0.2   plate - floors AND decks (t scales)
  2  BLK_Wall_400     4.0 x 0.5 x 4.0   story wall / mass segment
  3  BLK_HalfWall_200 2.0 x 0.5 x 1.1   crouch-cover wall (H fixed)
  4  BLK_Doorway_400  4.0 x 0.5 x 4.0   wall with 1.4 x 2.4 opening (opening fixed)
  5  BLK_Ramp_800     8.0 x 4.0 rise 4  27 deg slab ramp (slope <= 30)
  6  BLK_Stair_200    3.0 x 2.0 rise 2  10 steps 0.20/0.30 (NEVER scales)
  7  BLK_Column_100   0.9 dia x 4.0     octagonal free-standing column
  8  BLK_Pier_100     1.0 x 1.0 x 3.6   deck support pier
  9  BLK_Crate_100    1.0 x 1.0 x 1.0   cover crate (the lane crates)
  10 BLK_Battery_240  2.4 dia x 1.2     mantle-height octagon (base mouths)
  11 BLK_Rail_400     4.0 x 0.1 x 1.0   deck-edge railing
  12 BLK_Curb_400     4.0 x 0.3 x 0.15  trim / edge lip

Scaling law (research-backed): plates, walls, columns, piers, rails, curbs
scale; STAIRS and DOOR OPENINGS never do (rise:run and clearances are
gameplay). Ramps re-length, never steepen past 45 deg.
Stdlib only; --png rasterizes via headless chromium.
"""

from __future__ import annotations

import argparse
import math
import shutil
import subprocess
from datetime import date
from pathlib import Path

HERE = Path(__file__).resolve().parent
GAME = HERE.parents[1]
OUTD = GAME / "docs" / "design" / "blueprints" / "breachpoint_aquarius"

COS30 = math.cos(math.radians(30))

STYLE = """
  .paper{fill:#ffffff}
  .frame{fill:none;stroke:#000;stroke-width:1.4}
  .border{fill:none;stroke:#000;stroke-width:2.6}
  .top{fill:#ededed;stroke:#000;stroke-width:1.1;stroke-linejoin:round}
  .east{fill:#ffffff;stroke:#000;stroke-width:1.1;stroke-linejoin:round}
  .north{fill:#d2d2d2;stroke:#000;stroke-width:1.1;stroke-linejoin:round}
  .shadow{fill:#f0f0f0;stroke:none}
  .name{font:bold 15px monospace;fill:#000;letter-spacing:1px}
  .dims{font:12px monospace;fill:#000}
  .role{font:11px monospace;fill:#333}
  .rule{font:bold 11px monospace;fill:#000}
  .big{font:bold 17px monospace;fill:#000;letter-spacing:1px}
  .small{font:9px monospace;fill:#000}
  .tread{stroke:#000;stroke-width:0.7;fill:none}
"""


def P(x, y, z, S):
    return ((x - y) * COS30 * S, (x + y) * 0.5 * S - z * S)


def octagon(cx, cy, r):
    pts = []
    for i in range(8):
        a = math.pi / 8 + i * math.pi / 4
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def prism(o, poly, z0, z1, S, ox, oy, treads=None):
    """One extruded polygon, DOUBLE-SIDED: every side face draws, sorted far
    to near by midpoint depth, so front faces paint over back faces and no
    culling-sign mistake can ever hollow a solid again (founder, 30 Aug).
    The outward normal is used only to pick the face TONE."""
    n = len(poly)
    cx = sum(p[0] for p in poly) / n
    cy = sum(p[1] for p in poly) / n
    faces = []
    for i in range(n):
        a, b = poly[i], poly[(i + 1) % n]
        mx, my = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
        dx, dy = b[0] - a[0], b[1] - a[1]
        onx, ony = dy, -dx                 # right normal
        if onx * (mx - cx) + ony * (my - cy) < 0:
            onx, ony = -onx, -ony
        depth = mx + my                    # view axis +x+y: small = far
        faces.append((depth, a, b, onx, ony))
    for _, a, b, ne, ns in sorted(faces, key=lambda f: f[0]):
        quad = [P(a[0], a[1], z0, S), P(b[0], b[1], z0, S),
                P(b[0], b[1], z1, S), P(a[0], a[1], z1, S)]
        cls = "east" if ne >= ns else "north"
        o.append('<path class="%s" d="M %s Z"/>' % (
            cls, " L ".join("%.1f %.1f" % (px + ox, py + oy) for px, py in quad)))
    top = [P(px, py, z1, S) for px, py in poly]
    o.append('<path class="top" d="M %s Z"/>' % " L ".join(
        "%.1f %.1f" % (px + ox, py + oy) for px, py in top))
    if treads:
        for (a, b) in treads:
            pa, pb = P(a[0], a[1], a[2], S), P(b[0], b[1], b[2], S)
            o.append('<line class="tread" x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f"/>'
                     % (pa[0] + ox, pa[1] + oy, pb[0] + ox, pb[1] + oy))


def box(x, y, w, d, z0, z1):
    return ([(x, y), (x + w, y), (x + w, y + d), (x, y + d)], z0, z1)


def wedge_faces(o, x, y, run, width, rise, S, ox, oy):
    """Ramp wedge rising toward +x; visible: east end, south side, slope."""
    a, b = (x, y), (x + run, y)                    # far edge (low->high)
    c, d = (x + run, y + width), (x, y + width)    # near (south) edge
    east = [P(*b, 0, S), P(*c, 0, S), P(*c, rise, S), P(*b, rise, S)]
    o.append('<path class="east" d="M %s Z"/>' % " L ".join(
        "%.1f %.1f" % (px + ox, py + oy) for px, py in east))
    south = [P(*d, 0, S), P(*c, 0, S), P(*c, rise, S)]
    o.append('<path class="north" d="M %s Z"/>' % " L ".join(
        "%.1f %.1f" % (px + ox, py + oy) for px, py in south))
    slope = [P(*a, 0, S), P(*b, rise, S), P(*c, rise, S), P(*d, 0, S)]
    o.append('<path class="top" d="M %s Z"/>' % " L ".join(
        "%.1f %.1f" % (px + ox, py + oy) for px, py in slope))
    for i in range(1, 8):
        t = i / 8
        pa = P(x + run * t, y, rise * t, S)
        pb = P(x + run * t, y + width, rise * t, S)
        o.append('<line class="tread" x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f"/>'
                 % (pa[0] + ox, pa[1] + oy, pb[0] + ox, pb[1] + oy))


ASSETS = [
    ("BLK_Floor_400", "4.00 x 4.00 x 0.20 m",
     "floor AND deck plate; top face is the walking surface",
     "SCALES: L / W / T (deck t 0.40)", 30,
     [box(0, 0, 4, 4, 0, 0.2)]),
    ("BLK_Wall_400", "4.00 x 0.50 x 4.00 m",
     "story wall and solid mass segment (towers stack two)",
     "SCALES: L / H / T", 26,
     [box(0, 0, 4, 0.5, 0, 4)]),
    ("BLK_HalfWall_200", "2.00 x 0.50 x 1.10 m",
     "crouch-cover wall: hides a crouched player exactly",
     "SCALES: L only - H 1.10 FIXED (cover band)", 34,
     [box(0, 0, 2, 0.5, 0, 1.1)]),
    ("BLK_Doorway_400", "4.00 x 0.50 x 4.00 m / opening 1.40 x 2.40",
     "wall piece with the door punched through",
     "JAMBS stretch - OPENING NEVER SCALES", 26,
     "DOORWAY"),
    ("BLK_Ramp_800", "8.00 run x 4.00 w, rise 4.00 (27 deg)",
     "the vertical circulation; slope stays under 30 deg",
     "RE-LENGTH ONLY - never steepen past 45 deg", 15,
     "WEDGE"),
    ("BLK_Stair_200", "3.00 run x 2.00 w, rise 2.00 (10 steps 0.20/0.30)",
     "steps for short hops; treads are gameplay metric",
     "NEVER SCALES - new rise:run = new asset", 28,
     [box(0, 0, 3 - i * 0.3, 2, 0, (i + 1) * 0.2) for i in range(10)]),
    ("BLK_Column_100", "0.90 dia x 4.00 m octagon",
     "free-standing, never wall-touching (orbitable - notes 21-25)",
     "SCALES: H only", 30,
     [(octagon(0.45, 0.45, 0.45), 0, 4)]),
    ("BLK_Pier_100", "1.00 x 1.00 x 3.60 m",
     "deck support pier; carries BLK_Floor decks at +3.60",
     "SCALES: footprint only - H matches deck soffit", 30,
     [box(0, 0, 1, 1, 0, 3.6)]),
    ("BLK_Crate_100", "1.00 x 1.00 x 1.00 m",
     "lane cover crate (shot12); jump-over at 1.0",
     "SCALES: uniform, max +/-25%", 40,
     [box(0, 0, 1, 1, 0, 1)]),
    ("BLK_Battery_240", "2.40 dia x 1.20 m octagon",
     "the AQUARIUS battery: mantle cover at each base mouth (shot13)",
     "DOES NOT SCALE - mantle height is gameplay", 34,
     [(octagon(1.2, 1.2, 1.2), 0, 1.2)]),
    ("BLK_Rail_400", "4.00 x 0.10 x 1.00 m",
     "deck-edge railing: stops falls, not shots",
     "SCALES: L only", 30,
     [box(0, 0, 0.1, 0.1, 0, 0.92), box(1.95, 0, 0.1, 0.1, 0, 0.92),
      box(3.9, 0, 0.1, 0.1, 0, 0.92), box(0, 0, 4, 0.1, 0.92, 1.0)]),
    ("BLK_Curb_400", "4.00 x 0.30 x 0.15 m",
     "trim / edge lip: reads edges, channels movement",
     "SCALES: L only", 34,
     [box(0, 0, 4, 0.3, 0, 0.15)]),
]


def card(o, ox, oy, cw, ch, asset, idx):
    name, dims, role, rule, _S, geo = asset
    o.append('<rect class="frame" x="%d" y="%d" width="%d" height="%d"/>'
             % (ox, oy, cw, ch))
    o.append('<text class="small" x="%d" y="%d">%02d</text>' % (ox + 10, oy + 20, idx))
    o.append('<text class="name" x="%d" y="%d">%s</text>' % (ox + 30, oy + 22, name))

    # collect world-space corners, auto-fit and center in the drawing area
    if geo == "WEDGE":
        corners = [(0, 0, 0), (8, 0, 0), (8, 4, 0), (0, 4, 0),
                   (8, 0, 4), (8, 4, 4)]
        fx = [0, 8]
        fy = [0, 4]
    elif geo == "DOORWAY":
        corners = [(x, y, z) for x in (0, 4) for y in (0, 0.5) for z in (0, 4)]
        fx = [0, 4]
        fy = [0, 0.5]
    else:
        corners = [(px, py, z) for poly, z0, z1 in geo
                   for z in (z0, z1) for px, py in poly]
        fx = [px for poly, _, _ in geo for px, _ in poly]
        fy = [py for poly, _, _ in geo for _, py in poly]
    sh_w = [(min(fx) - 0.4, min(fy) - 0.4), (max(fx) + 0.4, min(fy) - 0.4),
            (max(fx) + 0.4, max(fy) + 0.4), (min(fx) - 0.4, max(fy) + 0.4)]
    pts1 = [P(x, y, z, 1.0) for (x, y, z) in corners] \
        + [P(x, y, 0, 1.0) for (x, y) in sh_w]
    bx0 = min(p[0] for p in pts1)
    bx1 = max(p[0] for p in pts1)
    by0 = min(p[1] for p in pts1)
    by1 = max(p[1] for p in pts1)
    aw, ah = cw - 70, ch - 190
    S = min(aw / max(bx1 - bx0, 1e-6), ah / max(by1 - by0, 1e-6), 46)
    dx = ox + 35 + (aw - (bx1 - bx0) * S) / 2 - bx0 * S
    dy = oy + 46 + (ah - (by1 - by0) * S) / 2 - by0 * S

    o.append('<path class="shadow" d="M %s Z"/>' % " L ".join(
        "%.1f %.1f" % (P(x, y, 0, S)[0] + dx, P(x, y, 0, S)[1] + dy)
        for (x, y) in sh_w))
    if geo == "WEDGE":
        wedge_faces(o, 0, 0, 8, 4, 4, S, dx, dy)
        lo = P(0.6, 2.0, 0.62, S)
        hi = P(6.6, 2.0, 3.62, S)
        o.append('<line class="tread" style="stroke-width:1.6" '
                 'x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f"/>'
                 % (lo[0] + dx, lo[1] + dy, hi[0] + dx, hi[1] + dy))
        o.append('<text class="rule" x="%.1f" y="%.1f">UP</text>'
                 % (hi[0] + dx + 6, hi[1] + dy - 4))
    elif geo == "DOORWAY":
        poly, z0, z1 = box(0, 0, 4, 0.5, 0, 4)
        prism(o, poly, z0, z1, S, dx, dy)
        void = [P(1.3, 0.5, 0, S), P(2.7, 0.5, 0, S), P(2.7, 0.5, 2.4, S),
                P(1.3, 0.5, 2.4, S)]
        o.append('<path d="M %s Z" fill="#5a5a5a" stroke="#000" '
                 'stroke-width="1.1"/>' % " L ".join(
                     "%.1f %.1f" % (px + dx, py + dy) for px, py in void))
    else:
        solids = sorted(geo, key=lambda g: (min(x + y for x, y in g[0]), g[1]))
        for poly, z0, z1 in solids:
            prism(o, poly, z0, z1, S, dx, dy)
    o.append('<text class="dims" x="%d" y="%d">%s</text>' % (ox + 14, oy + ch - 58, dims))
    o.append('<text class="role" x="%d" y="%d">%s</text>' % (ox + 14, oy + ch - 38, role))
    o.append('<text class="rule" x="%d" y="%d">%s</text>' % (ox + 14, oy + ch - 16, rule))


def sheet(fname, no, title, assets, start_idx):
    cw, ch = 470, 430
    cols, rows = 3, 2
    w = cols * cw + 4 * 24
    h = rows * ch + 24 * 3 + 150
    o = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" '
         'viewBox="0 0 %d %d">' % (w, h, w, h),
         "<style>%s</style>" % STYLE,
         '<rect class="paper" width="%d" height="%d"/>' % (w, h),
         '<rect class="border" x="8" y="8" width="%d" height="%d"/>' % (w - 16, h - 16)]
    o.append('<text class="big" x="28" y="46">%s</text>' % title)
    o.append('<text class="small" x="28" y="66">BREACHPOINT BN30 · the modular blockout '
             'set · 12 assets total · grey = form only, grid 0.5 m · %s · sheet %s</text>'
             % (date.today().isoformat(), no))
    for i, asset in enumerate(assets):
        r, c = divmod(i, cols)
        card(o, 24 + c * (cw + 24), 84 + r * (ch + 24), cw, ch, asset, start_idx + i)
    o.append('<text class="small" x="28" y="%d">SCALING LAW: plates, walls, columns, '
             'piers, rails, curbs scale · STAIRS and DOOR OPENINGS never scale '
             '(rise:run and clearances are gameplay) · ramps re-length, never steepen '
             'past 45 deg · everything snaps to the 0.5 m grid</text>' % (h - 28))
    o.append("</svg>")
    (OUTD / ("%s.svg" % fname)).write_text("\n".join(o), encoding="utf-8")


def rasterize():
    chromium = shutil.which("chromium") or "/opt/pw-browsers/chromium"
    if not Path(chromium).exists():
        print("no chromium; SVGs only")
        return
    import re
    for svg in sorted(OUTD.glob("K1*.svg")):
        png = svg.with_suffix(".png")
        head = svg.read_text(encoding="utf-8")[:200]
        m = re.search(r'width="(\d+)" height="(\d+)"', head)
        win = "%d,%d" % (int(m.group(1)) + 24, int(m.group(2)) + 70)
        subprocess.run([chromium, "--headless", "--disable-gpu", "--no-sandbox",
                        "--force-device-scale-factor=2",
                        "--default-background-color=FFFFFFFF",
                        "--screenshot=%s" % png, "--hide-scrollbars",
                        "--window-size=%s" % win, svg.as_uri()],
                       check=True, capture_output=True, timeout=120)
        print("  rasterized %s" % png.name)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--png", action="store_true")
    args = ap.parse_args()
    OUTD.mkdir(parents=True, exist_ok=True)
    sheet("K101_kit_catalog_1", "K-101", "MODULAR BLOCKOUT KIT — ASSETS 01-06",
          ASSETS[:6], 1)
    sheet("K102_kit_catalog_2", "K-102", "MODULAR BLOCKOUT KIT — ASSETS 07-12",
          ASSETS[6:], 7)
    print("wrote K-101 / K-102 (12 assets)")
    if args.png:
        rasterize()


if __name__ == "__main__":
    main()
