#!/usr/bin/env python3
"""BN29 — AQUARIUS engineering blueprint set (REV D), drawn from POLYGONS.

    python3 Tools/blockout/gen_aquarius_blueprint.py --png

The founder's rulings, in order:
- 30 Aug: "as accurate 1:1 as possible - front, side, top, 3/4, level floors,
  height, width, stairs, platforms, columns, corners, floors, walls" -> REV C
  moved from manifest boxes to full-resolution trace polygons.
- 30 Aug: "an actual blockout... walls not hollow" -> the 3/4 went solid.
- 30 Aug: "remake it from scratch... floor one, floor two, and the
  in-betweens... different heights and widths in the different rooms...
  an actual civil-engineer layout blueprint" -> THIS REVISION. One sheet per
  level plus an intermediate-level sheet, structural grid with bubbles,
  dimension chains, ROOM TAGS carrying each room's measured clear width x
  length and clear height, a ROOM SCHEDULE, section markers on plan,
  hatched poche by material class, and a proper title block per sheet.

SHEET SET
  A-101  LEVEL 1 FLOOR PLAN (FFL 0.00) + room schedule L1
  A-102  LEVEL 2 FLOOR PLAN (FFL +4.00) + room schedule L2
  A-103  INTERMEDIATE LEVEL (between FFL 0.00 and +4.00): ramps R1..Rn with
         rise, deck soffit, supports - the in-betweens
  A-201  SECTIONS A-A / B-B / C-C with grid bubbles, level tags, clearances
  A-301  3/4 BLOCKOUT - solid massing, high camera (unchanged doctrine)

LEVEL SCHEDULE (heights authored from the founder's 25 references; the plan
scale is the one derived number - 52 m long axis until a Forge walk):
  FFL 0.00 ground · U.S. DECK +3.60 · T.O. DECK +4.00 · T.O. TOWER +6.50
  T.O. WALL +8.00 · ramps rise 0.00 -> +4.00

Geometry: per-floor traces at full resolution (~0.27 m cells), v3 semantics,
boundaries polygonized by edge-chaining + closed-loop Douglas-Peucker.
Stdlib only; --png rasterizes via headless chromium.
"""

from __future__ import annotations

import argparse
import json
import math
import shutil
import subprocess
from collections import deque
from datetime import date
from pathlib import Path

HERE = Path(__file__).resolve().parent
GAME = HERE.parents[1]
REFD = GAME / "docs" / "design" / "reference"

LONG_AXIS_M = 52.0
DECK = (3.6, 4.0)
TOWER_H = 6.5
WALL_H = 8.0
DP_EPS_CELLS = 1.25
REV = "D"

STYLE = """
  .paper{fill:#ffffff}
  .frame{fill:none;stroke:#000;stroke-width:1.4}
  .border{fill:none;stroke:#000;stroke-width:2.6}
  .border2{fill:none;stroke:#000;stroke-width:1.0}
  .poche{fill:#111111;stroke:#000;stroke-width:0.9}
  .hatch{fill:url(#h45);stroke:#000;stroke-width:1.1}
  .hatch2{fill:url(#h135);stroke:#000;stroke-width:0.9}
  .plate{fill:#e8e8e8;stroke:#000;stroke-width:1.1}
  .floorp{fill:#f1f1f1;stroke:#333;stroke-width:0.9}
  .stairp{fill:#fafafa;stroke:#000;stroke-width:1.0}
  .over{fill:none;stroke:#000;stroke-width:0.9;stroke-dasharray:6 4}
  .below{fill:none;stroke:#9a9a9a;stroke-width:0.7}
  .tread{stroke:#000;stroke-width:0.6;fill:none}
  .gridl{stroke:#777;stroke-width:0.7;stroke-dasharray:16 7}
  .bubble{fill:#fff;stroke:#000;stroke-width:1.2}
  .secline{stroke:#000;stroke-width:1.7;stroke-dasharray:18 5 4 5}
  .ground{stroke:#000;stroke-width:2.6}
  .beyond{fill:#f4f4f4;stroke:#8a8a8a;stroke-width:0.8}
  .beyond2{fill:#d9d9d9;stroke:#6e6e6e;stroke-width:0.8}
  .dim{stroke:#000;stroke-width:0.8;fill:none}
  .dimtext{font:10px monospace;fill:#000}
  .label{font:bold 10.5px monospace;fill:#000;letter-spacing:0.5px}
  .rooml{font:bold 12.5px monospace;fill:#000;letter-spacing:1px}
  .roomd{font:10px monospace;fill:#000}
  .small{font:9px monospace;fill:#000}
  .note{font:10.5px monospace;fill:#000}
  .big{font:bold 17px monospace;fill:#000;letter-spacing:1px}
  .panel{font:bold 13px monospace;fill:#000;letter-spacing:1px}
  .sheetno{font:bold 30px monospace;fill:#000;letter-spacing:2px}
  .datum{stroke:#000;stroke-width:0.7;stroke-dasharray:12 4 2 4}
  .isotop{fill:#ededed;stroke:#000;stroke-width:1.0;stroke-linejoin:round}
  .isoeast{fill:#ffffff;stroke:#000;stroke-width:1.0;stroke-linejoin:round}
  .isonorth{fill:#d4d4d4;stroke:#000;stroke-width:1.0;stroke-linejoin:round}
  .isoplate{fill:#f7f7f7;stroke:#000;stroke-width:1.6;stroke-linejoin:round}
  .isofloor{fill:#e2e2e2;stroke:#555;stroke-width:0.8;stroke-linejoin:round}
"""

DEFS = """<defs>
<pattern id="h45" width="7" height="7" patternUnits="userSpaceOnUse" patternTransform="rotate(45)">
  <rect width="7" height="7" fill="#ffffff"/><line x1="0" y1="0" x2="0" y2="7" stroke="#000" stroke-width="1.2"/>
</pattern>
<pattern id="h135" width="5" height="5" patternUnits="userSpaceOnUse" patternTransform="rotate(-45)">
  <rect width="5" height="5" fill="#ffffff"/><line x1="0" y1="0" x2="0" y2="5" stroke="#555" stroke-width="1.0"/>
</pattern>
</defs>"""


# ---------------------------------------------------------------------------
# geometry extraction — full-res grids -> classified cell sets -> polygons

def load_grid(tag):
    t = json.loads((REFD / ("aquarius_trace_%s.json" % tag)).read_text())
    x0, y0, x1, y1 = t["bbox_cells"]
    rows = [r[x0:x1 + 1] for r in t["rows"][y0:y1 + 1]]
    W = x1 - x0 + 1
    rows = ["".join(r[x] if x < W // 2 + W % 2 else r[W - 1 - x]
                    for x in range(W)) for r in rows]      # symmetrize, left wins
    return rows, W, y1 - y0 + 1


def components(cells):
    cs, out = set(cells), []
    while cs:
        seed = min(cs)
        comp, q = set(), deque([seed])
        cs.discard(seed)
        while q:
            x, y = q.popleft()
            comp.add((x, y))
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                n = (x + dx, y + dy)
                if n in cs:
                    cs.discard(n)
                    q.append(n)
        out.append(comp)
    return out


def mask_loops(comp):
    """Boundary loops of a cell set (cell-corner vertices), interior on the
    left; outer loops and holes both return, evenodd sorts them out."""
    edges = {}
    for (x, y) in comp:
        for (nb, a, b) in (((x, y - 1), (x, y), (x + 1, y)),
                           ((x + 1, y), (x + 1, y), (x + 1, y + 1)),
                           ((x, y + 1), (x + 1, y + 1), (x, y + 1)),
                           ((x - 1, y), (x, y + 1), (x, y))):
            if nb not in comp:
                edges.setdefault(a, []).append(b)
    loops = []
    while edges:
        start = min(edges)
        loop, cur = [start], edges[start].pop()
        if not edges[start]:
            del edges[start]
        prev = start
        while cur != start:
            loop.append(cur)
            outs = edges[cur]
            if len(outs) == 1:
                nxt = outs.pop()
            else:
                din = (cur[0] - prev[0], cur[1] - prev[1])
                left = (din[1], -din[0])
                nxt = max(outs, key=lambda p: (p[0] - cur[0]) * left[0]
                          + (p[1] - cur[1]) * left[1])
                outs.remove(nxt)
            if not edges[cur]:
                del edges[cur]
            prev, cur = cur, nxt
        loops.append(loop)
    return loops


def _dp(pts, eps):
    if len(pts) < 3:
        return pts
    ax, ay = pts[0]
    bx, by = pts[-1]
    dx, dy = bx - ax, by - ay
    L = math.hypot(dx, dy) or 1e-9
    imax, dmax = 0, -1.0
    for i in range(1, len(pts) - 1):
        d = abs((pts[i][0] - ax) * dy - (pts[i][1] - ay) * dx) / L
        if d > dmax:
            imax, dmax = i, d
    if dmax <= eps:
        return [pts[0], pts[-1]]
    return _dp(pts[:imax + 1], eps)[:-1] + _dp(pts[imax:], eps)


def simplify(loop, eps=DP_EPS_CELLS):
    keep = [p for i, p in enumerate(loop)
            if (p[0] - loop[i - 1][0], p[1] - loop[i - 1][1])
            != (loop[(i + 1) % len(loop)][0] - p[0],
                loop[(i + 1) % len(loop)][1] - p[1])]
    if len(keep) < 4:
        return keep
    if len(keep) < 24:                 # small features: keep their corners
        eps = min(eps, 0.55)
    i1 = max(range(len(keep)), key=lambda i: math.hypot(
        keep[i][0] - keep[0][0], keep[i][1] - keep[0][1]))
    a, b = keep[:i1 + 1], keep[i1:] + [keep[0]]
    return _dp(a, eps)[:-1] + _dp(b, eps)[:-1]


class Solid:
    """One traced component: polygon loops (cell coords), z-range, class."""

    def __init__(self, kind, comp, loops, z0, z1):
        self.kind, self.cells, self.loops, self.z0, self.z1 = kind, comp, loops, z0, z1
        xs = [p[0] for lp in loops for p in lp]
        ys = [p[1] for lp in loops for p in lp]
        self.bbox = (min(xs), min(ys), max(xs), max(ys))


def extract():
    G, W, H = load_grid("ground")
    U, _, _ = load_grid("upper")
    cell_m = LONG_AXIS_M / W

    def mask(rows, ch):
        return {(x, y) for y in range(H) for x in range(W) if rows[y][x] in ch}

    bg = mask(G, ".")
    outside = set()
    q = deque([(x, y) for x in range(W) for y in (0, H - 1) if (x, y) in bg]
              + [(x, y) for y in range(H) for x in (0, W - 1) if (x, y) in bg])
    outside.update(q)
    while q:
        x, y = q.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            n = (x + dx, y + dy)
            if n in bg and n not in outside:
                outside.add(n)
                q.append(n)

    upper = mask(U, "L")
    stairs = set()
    for comp in components(mask(G, "L")):
        cx = sum(x for x, _ in comp) / len(comp)
        if len(comp) < 280 and W * 0.18 < cx < W * 0.82:
            stairs |= comp

    solids = []
    for comp in components(mask(G, "#")):
        touches = any((x + dx, y + dy) in outside or not
                      (0 <= x + dx < W and 0 <= y + dy < H)
                      for (x, y) in comp
                      for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
        if touches:
            kind, z0, z1 = "wall", 0.0, WALL_H
        elif len(comp & upper) >= len(comp) * 0.5:
            kind, z0, z1 = "support", 0.0, DECK[0]
        else:
            kind, z0, z1 = "tower", 0.0, TOWER_H
        solids.append(Solid(kind, comp, [simplify(l) for l in mask_loops(comp)], z0, z1))
    for comp in components(upper):
        solids.append(Solid("deck", comp, [simplify(l) for l in mask_loops(comp)],
                            DECK[0], DECK[1]))
    for comp in components(stairs):
        solids.append(Solid("stair", comp, [simplify(l) for l in mask_loops(comp)],
                            0.0, DECK[1]))
    floor = {(x, y) for y in range(H) for x in range(W)
             if (x, y) not in outside and G[y][x] != "#"}
    return solids, floor, W, H, cell_m


# ---------------------------------------------------------------------------
# generic svg helpers

def svg_open(w, h, title):
    return ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" '
            'viewBox="0 0 %d %d">' % (w, h, w, h),
            "<style>%s</style>" % STYLE, DEFS,
            '<rect class="paper" width="%d" height="%d"/>' % (w, h),
            '<title>%s</title>' % title]


def sheet_chrome(o, w, h, sheet_no, sheet_name, scale_txt):
    """Civil-drawing chrome: double border, title block with a big sheet
    number, and the one note every real drawing carries."""
    o.append('<rect class="border" x="8" y="8" width="%d" height="%d"/>' % (w - 16, h - 16))
    o.append('<rect class="border2" x="20" y="20" width="%d" height="%d"/>' % (w - 40, h - 40))
    bw, bh = 470, 152
    x0, y0 = w - 20 - bw, h - 20 - bh
    o.append('<rect class="frame" x="%d" y="%d" width="%d" height="%d" fill="#fff"/>'
             % (x0, y0, bw, bh))
    rows = [("PROJECT", "BREACHPOINT — 4v4 ARENA FPS"),
            ("ARENA", "AQUARIUS — STUDY RECREATION (BN29)"),
            ("DRAWING", sheet_name),
            ("SCALE/DATE/REV", "%s · %s · REV %s" % (scale_txt, date.today().isoformat(), REV))]
    for i, (k, v) in enumerate(rows):
        yy = y0 + 22 + i * 22
        o.append('<text class="small" x="%d" y="%d">%s</text>' % (x0 + 10, yy, k))
        o.append('<text class="dimtext" x="%d" y="%d">%s</text>' % (x0 + 130, yy, v))
        if i:
            o.append('<line class="dim" x1="%d" y1="%d" x2="%d" y2="%d"/>'
                     % (x0, y0 + 6 + i * 22, x0 + bw - 118, y0 + 6 + i * 22))
    o.append('<line class="frame" x1="%d" y1="%d" x2="%d" y2="%d"/>'
             % (x0 + bw - 118, y0, x0 + bw - 118, y0 + bh))
    o.append('<text class="small" x="%d" y="%d">SHEET</text>' % (x0 + bw - 106, y0 + 26))
    o.append('<text class="sheetno" x="%d" y="%d">%s</text>' % (x0 + bw - 108, y0 + 70, sheet_no))
    o.append('<line class="frame" x1="%d" y1="%d" x2="%d" y2="%d"/>'
             % (x0, y0 + 96, x0 + bw - 118, y0 + 96))
    o.append('<text class="small" x="%d" y="%d">DO NOT SCALE — USE FIGURED DIMENSIONS. '
             'UNITS: METERS.</text>' % (x0 + 10, y0 + 116))
    o.append('<text class="small" x="%d" y="%d">FFL 0.00 / T.O. DECK +4.00 / T.O. WALL +8.00. '
             'PLAN SCALE DERIVED.</text>' % (x0 + 10, y0 + 134))


def dim_h(x0, x1, y, txt):
    return ['<line class="dim" x1="%g" y1="%g" x2="%g" y2="%g"/>' % (x0, y, x1, y),
            '<line class="dim" x1="%g" y1="%g" x2="%g" y2="%g"/>' % (x0, y - 4, x0, y + 4),
            '<line class="dim" x1="%g" y1="%g" x2="%g" y2="%g"/>' % (x1, y - 4, x1, y + 4),
            '<text class="dimtext" text-anchor="middle" x="%g" y="%g">%s</text>'
            % ((x0 + x1) / 2, y - 5, txt)]


def dim_v(y0, y1, x, txt):
    return ['<line class="dim" x1="%g" y1="%g" x2="%g" y2="%g"/>' % (x, y0, x, y1),
            '<line class="dim" x1="%g" y1="%g" x2="%g" y2="%g"/>' % (x - 4, y0, x + 4, y0),
            '<line class="dim" x1="%g" y1="%g" x2="%g" y2="%g"/>' % (x - 4, y1, x + 4, y1),
            '<text class="dimtext" x="%g" y="%g" transform="rotate(-90 %g %g)" '
            'text-anchor="middle">%s</text>'
            % (x - 6, (y0 + y1) / 2, x - 6, (y0 + y1) / 2, txt)]


def path_of(loops, X, Y, cls):
    d = []
    for lp in loops:
        d.append("M " + " L ".join("%.1f %.1f" % (X(px), Y(py)) for px, py in lp) + " Z")
    return '<path class="%s" fill-rule="evenodd" d="%s"/>' % (cls, " ".join(d))


def row_intervals(cells, y):
    xs = sorted(x for (x, yy) in cells if yy == y)
    out = []
    for x in xs:
        if out and x == out[-1][1]:
            out[-1][1] = x + 1
        else:
            out.append([x, x + 1])
    return out


def col_intervals(cells, x):
    ys = sorted(y for (xx, y) in cells if xx == x)
    out = []
    for y in ys:
        if out and y == out[-1][1]:
            out[-1][1] = y + 1
        else:
            out.append([y, y + 1])
    return out


def scale_bar(o, x, y, S_per_m):
    o.append('<text class="small" x="%g" y="%g">GRAPHIC SCALE</text>' % (x, y - 8))
    for i in range(5):
        fill = "#000" if i % 2 == 0 else "#fff"
        o.append('<rect x="%g" y="%g" width="%g" height="7" fill="%s" stroke="#000" '
                 'stroke-width="0.8"/>' % (x + i * 2 * S_per_m, y, 2 * S_per_m, fill))
    for m in (0, 2, 4, 6, 8, 10):
        o.append('<text class="small" x="%g" y="%g" text-anchor="middle">%d</text>'
                 % (x + m * S_per_m, y + 20, m))
    o.append('<text class="small" x="%g" y="%g">m</text>' % (x + 10 * S_per_m + 12, y + 20))


def north_arrow(o, x, y):
    o.append('<g transform="translate(%g,%g)">' % (x, y))
    o.append('<circle class="bubble" cx="0" cy="0" r="17"/>')
    o.append('<path d="M 0 10 L 0 -12 M -5 -4 L 0 -12 L 5 -4" stroke="#000" '
             'stroke-width="1.6" fill="none"/>')
    o.append('<text class="label" x="-4" y="32">N</text></g>')


# ---------------------------------------------------------------------------
# structural grid + rooms

def grid_lines(W, H, cell_m):
    """Uniform structural grid: columns A.. every 6.5 m, rows 1.. in five
    equal bays. Returns ([(label, xcell)...], [(label, ycell)...])."""
    Wm, Hm = W * cell_m, H * cell_m
    nx = round(Wm / 6.5)
    cols = [(chr(ord("A") + i), i * (W / nx)) for i in range(nx + 1)]
    rows = [(str(i + 1), i * (H / 5)) for i in range(6)]
    return cols, rows


def nearest_in(cells, px, py, r=14):
    if (px, py) in cells:
        return px, py
    best, bd = None, 1e9
    for (x, y) in cells:
        d = (x - px) ** 2 + (y - py) ** 2
        if d < bd:
            best, bd = (x, y), d
    return best if best and bd <= r * r else None


def room_metrics(cells, fx, fy, W, H, cell_m, win=None):
    if win:
        x0, x1, y0, y1 = (win[0] * W, win[1] * W, win[2] * H, win[3] * H)
        cells = {(x, y) for (x, y) in cells if x0 <= x < x1 and y0 <= y < y1}
    p = nearest_in(cells, int(fx * W), int(fy * H))
    if not p:
        return None
    px, py = p
    wm = lm = 0
    for a, b in row_intervals(cells, py):
        if a <= px < b:
            wm = (b - a) * cell_m
    for a, b in col_intervals(cells, px):
        if a <= py < b:
            lm = (b - a) * cell_m
    return px, py, wm, lm


ROOMS_L1 = [("101", "BASE 1", 0.115, 0.50, (0.0, 0.24, 0.2, 0.8)),
            ("102", "ARENA 1", 0.335, 0.32, (0.22, 0.50, 0.08, 0.92)),
            ("103", "ARENA 2", 0.665, 0.32, (0.50, 0.78, 0.08, 0.92)),
            ("104", "BASE 2", 0.885, 0.50, (0.76, 1.0, 0.2, 0.8)),
            ("105", "HALLWAY 1", 0.42, 0.53, (0.28, 0.72, 0.40, 0.66)),
            ("106", "HALLWAY 2", 0.50, 0.145, (0.2, 0.8, 0.0, 0.26)),
            ("107", "HALLWAY 3", 0.50, 0.815, (0.2, 0.8, 0.72, 1.0))]
ROOMS_L2 = [("201", "BASE 1 UPPER", 0.115, 0.50, (0.0, 0.24, 0.2, 0.8)),
            ("202", "BASE 2 UPPER", 0.885, 0.50, (0.76, 1.0, 0.2, 0.8)),
            ("203", "BRIDGE", 0.50, 0.34, (0.40, 0.60, 0.10, 0.90)),
            ("204", "NORTH WALK", 0.42, 0.175, (0.15, 0.85, 0.0, 0.28)),
            ("205", "SOUTH WALK", 0.42, 0.80, (0.15, 0.85, 0.70, 1.0))]


def build_rooms(floor, deckcells, W, H, cell_m):
    """Measured clear width x length through each room's tag point, plus the
    clear height: L1 under a deck = 3.60, open = 8.00; L2 = 4.00 to datum."""
    out = {"L1": [], "L2": []}
    for num, name, fx, fy, win in ROOMS_L1:
        m = room_metrics(floor, fx, fy, W, H, cell_m, win)
        if m:
            px, py, wm, lm = m
            clr = DECK[0] if (px, py) in deckcells else WALL_H
            out["L1"].append((num, name, px, py, wm, lm, clr))
    for num, name, fx, fy, win in ROOMS_L2:
        m = room_metrics(deckcells, fx, fy, W, H, cell_m, win)
        if m:
            px, py, wm, lm = m
            out["L2"].append((num, name, px, py, wm, lm, WALL_H - DECK[1]))
    return out


# ---------------------------------------------------------------------------
# plan sheets (A-101 / A-102 / A-103)

PLAN_S = 13.0          # px per cell
MXL, MYT = 160, 150    # plan origin on the sheet
SIDE_W = 470           # right-hand notes/schedule column

GENERAL_NOTES = [
    "1. STUDY RECREATION of 343 Industries' Halo Infinite",
    "   map AQUARIUS. Source: founder reference set (25",
    "   images) traced per floor at ~0.27 m cells.",
    "2. All plan geometry mirror-symmetric about the map's",
    "   long axis center; left half governs.",
    "3. Long axis 52.0 m DERIVED - pending in-game walk",
    "   (channel b). Heights authored from reference shots.",
    "4. Side hallways (106/107) are TWO-LEVEL: lane at FFL",
    "   0.00, walk deck over at +4.00 (rooms 204/205).",
    "5. Decks are pillar-borne; hatched supports carry",
    "   them. Editor pass places the octagonal pillars.",
    "6. Sunken center trench drawn at grade this revision;",
    "   below-grade geometry owed to channel (b).",
    "7. Sightline governance: R45 (DESIGN-RULINGS.md).",
]


def draw_grid_refs(o, X, Y, cols, rows, ph):
    for lab, xc in cols:
        o.append('<line class="gridl" x1="%g" y1="%g" x2="%g" y2="%g"/>'
                 % (X(xc), Y(0) - 14, X(xc), Y(0) + ph + 14))
        for yy in (Y(0) - 26, Y(0) + ph + 30):
            o.append('<circle class="bubble" cx="%g" cy="%g" r="12"/>' % (X(xc), yy))
            o.append('<text class="label" x="%g" y="%g" text-anchor="middle">%s</text>'
                     % (X(xc), yy + 4, lab))
    for lab, yc in rows:
        o.append('<line class="gridl" x1="%g" y1="%g" x2="%g" y2="%g"/>'
                 % (X(0) - 14, Y(yc), X(cols[-1][1]) + 14, Y(yc)))
        o.append('<circle class="bubble" cx="%g" cy="%g" r="12"/>' % (X(0) - 30, Y(yc)))
        o.append('<text class="label" x="%g" y="%g" text-anchor="middle">%s</text>'
                 % (X(0) - 30, Y(yc) + 4, lab))


def room_tag(o, x, y, num, name, wm, lm, clr):
    o.append('<text class="rooml" x="%g" y="%g" text-anchor="middle">%s</text>'
             % (x, y - 10, name))
    o.append('<rect class="bubble" x="%g" y="%g" width="46" height="16" rx="8"/>'
             % (x - 23, y - 4))
    o.append('<text class="label" x="%g" y="%g" text-anchor="middle">%s</text>'
             % (x, y + 8, num))
    o.append('<text class="roomd" x="%g" y="%g" text-anchor="middle">%.1f x %.1f m</text>'
             % (x, y + 26, wm, lm))
    o.append('<text class="roomd" x="%g" y="%g" text-anchor="middle">CLR %.2f m</text>'
             % (x, y + 40, clr))


def schedule_table(o, x, y, title, rooms):
    o.append('<text class="panel" x="%d" y="%d">%s</text>' % (x, y, title))
    heads = ("NO.", "ROOM", "W m", "L m", "CLR m")
    colx = (0, 44, 190, 240, 290)
    yy = y + 20
    for i, htxt in enumerate(heads):
        o.append('<text class="label" x="%d" y="%d">%s</text>' % (x + colx[i], yy, htxt))
    o.append('<line class="dim" x1="%d" y1="%d" x2="%d" y2="%d"/>' % (x, yy + 6, x + 350, yy + 6))
    for num, name, _, _, wm, lm, clr in rooms:
        yy += 19
        for i, val in enumerate((num, name, "%.1f" % wm, "%.1f" % lm, "%.2f" % clr)):
            o.append('<text class="roomd" x="%d" y="%d">%s</text>' % (x + colx[i], yy, val))
    return yy


def notes_block(o, x, y):
    o.append('<text class="panel" x="%d" y="%d">GENERAL NOTES</text>' % (x, y))
    for i, line in enumerate(GENERAL_NOTES):
        o.append('<text class="note" x="%d" y="%d">%s</text>' % (x, y + 20 + i * 17, line))


def sec_marker(o, X, Y, letter, x0c, y0c, x1c, y1c, arrow_deg):
    p0, p1 = (X(x0c), Y(y0c)), (X(x1c), Y(y1c))
    o.append('<line class="secline" x1="%g" y1="%g" x2="%g" y2="%g"/>'
             % (p0[0], p0[1], p1[0], p1[1]))
    for (px, py) in (p0, p1):
        o.append('<g transform="translate(%g,%g)">' % (px, py))
        o.append('<circle class="bubble" cx="0" cy="0" r="13"/>')
        o.append('<text class="label" x="0" y="4" text-anchor="middle">%s</text>' % letter)
        o.append('<path transform="rotate(%g)" d="M 0 -13 L -6 -24 L 6 -24 Z" fill="#000"/>'
                 % arrow_deg)
        o.append('</g>')


def plan_sheet(solids, floor, deckcells, rooms, W, H, cell_m, out_dir,
               level):  # level: "L1" | "L2" | "MEZZ"
    S = PLAN_S
    pw, ph = W * S, H * S
    w = int(MXL + pw + 70 + SIDE_W + 40)
    h = int(MYT + ph + 240)
    X = lambda c: MXL + c * S
    Y = lambda c: MYT + c * S
    cols, rowsg = grid_lines(W, H, cell_m)
    Wm, Hm = W * cell_m, H * cell_m

    names = {"L1": ("A-101", "LEVEL 1 FLOOR PLAN — FFL 0.00",
                    "LEVEL 1 FLOOR PLAN · FFL 0.00 · CUT PLANE +1.50"),
             "L2": ("A-102", "LEVEL 2 FLOOR PLAN — FFL +4.00",
                    "LEVEL 2 FLOOR PLAN · FFL +4.00 · CUT PLANE +5.50"),
             "MEZZ": ("A-103", "INTERMEDIATE LEVEL — BETWEEN FFL 0.00 AND +4.00",
                      "INTERMEDIATE LEVEL · RAMPS, DECK SOFFIT +3.60, SUPPORTS")}
    no, sname, header = names[level]
    o = svg_open(w, h, "AQUARIUS — %s" % sname)
    o.append('<text class="big" x="%d" y="44">%s</text>' % (MXL - 80, header))

    draw_grid_refs(o, X, Y, cols, rowsg, ph)

    if level == "L1":
        for s in solids:
            if s.kind != "wall":            # interior floor first
                continue
        # walkable floor
        for comp in components(floor):
            if len(comp) >= 30:
                o.append(path_of([simplify(l) for l in mask_loops(comp)], X, Y, "floorp"))
        for s in solids:
            if s.kind == "stair":
                o.append(path_of(s.loops, X, Y, "stairp"))
        for s in solids:                    # cut at +1.50, by material
            if s.kind == "wall":
                o.append(path_of(s.loops, X, Y, "poche"))
            elif s.kind == "tower":
                o.append(path_of(s.loops, X, Y, "hatch"))
            elif s.kind == "support":
                o.append(path_of(s.loops, X, Y, "hatch2"))
        for s in solids:                    # decks overhead, dashed
            if s.kind == "deck":
                o.append(path_of(s.loops, X, Y, "over"))
        draw_stair_treads(o, solids, X, Y, cell_m, up_label=True)
        sec_marker(o, X, Y, "A", -3, H / 2, W + 3, H / 2, 0)
        sec_marker(o, X, Y, "B", W / 2, -3, W / 2, H + 3, -90)
        sec_marker(o, X, Y, "C", -3, H * 0.145, W + 3, H * 0.145, 0)
        for fx, fy, txt in ((0.27, 0.50, "FFL 0.00"), (0.73, 0.50, "FFL 0.00")):
            spot_elev(o, X(fx * W), Y(fy * H), txt)

    elif level == "L2":
        for s in solids:                    # ground context, faint
            if s.kind in ("tower", "support", "stair"):
                o.append(path_of(s.loops, X, Y, "below"))
        for s in solids:
            if s.kind == "deck":
                o.append(path_of(s.loops, X, Y, "plate"))
        for s in solids:                    # cut at +5.50
            if s.kind == "wall":
                o.append(path_of(s.loops, X, Y, "poche"))
            elif s.kind == "tower":
                o.append(path_of(s.loops, X, Y, "hatch"))
        for s in solids:                    # ramps arriving from below
            if s.kind == "stair":
                o.append(path_of(s.loops, X, Y, "over"))
        for fx, fy, txt in ((0.115, 0.36, "FFL +4.00"), (0.5, 0.42, "FFL +4.00")):
            spot_elev(o, X(fx * W), Y(fy * H), txt)

    else:  # MEZZ — the in-betweens
        for comp in components(floor):
            if len(comp) >= 30:
                o.append(path_of([simplify(l) for l in mask_loops(comp)], X, Y, "floorp"))
        for s in solids:
            if s.kind == "wall":
                o.append(path_of(s.loops, X, Y, "poche"))
            elif s.kind == "tower":
                o.append(path_of(s.loops, X, Y, "hatch"))
            elif s.kind == "support":
                o.append(path_of(s.loops, X, Y, "hatch2"))
        for s in solids:                    # the deck soffit above, dashed
            if s.kind == "deck":
                o.append(path_of(s.loops, X, Y, "over"))
        for s in solids:
            if s.kind == "stair":
                o.append(path_of(s.loops, X, Y, "stairp"))
        draw_stair_treads(o, solids, X, Y, cell_m, up_label=False)
        ramps = sorted((s for s in solids if s.kind == "stair"),
                       key=lambda s: (s.bbox[1], s.bbox[0]))
        for i, s in enumerate(ramps, 1):
            x0, y0, x1, y1 = s.bbox
            cx, cy = X((x0 + x1) / 2), Y((y0 + y1) / 2)
            o.append('<rect class="bubble" x="%g" y="%g" width="34" height="15" rx="7"/>'
                     % (cx - 17, cy - 8))
            o.append('<text class="small" x="%g" y="%g" text-anchor="middle">R%d</text>'
                     % (cx, cy + 3, i))
            o.append('<text class="small" x="%g" y="%g" text-anchor="middle">RISE 4.00</text>'
                     % (cx, cy + 20))
        o.append('<text class="note" x="%d" y="%d">DASHED = DECK SOFFIT AT +3.60 '
                 '(U.S. DECK) · HATCH ///  TOWERS (RISE TO +6.50) · '
                 'HATCH \\\\\\ SUPPORTS (0.00 TO +3.60)</text>'
                 % (MXL - 60, int(MYT + ph + 76)))
        o.append('<text class="note" x="%d" y="%d">RAMPS R1-R%d: 0.00 TO +4.00. '
                 'THE ONLY VERTICAL CIRCULATION; NO STAIR CORES, NO LIFTS.</text>'
                 % (MXL - 60, int(MYT + ph + 94), len(ramps)))

    # room tags + dims
    if level in ("L1", "L2"):
        for num, name, px, py, wm, lm, clr in rooms[level]:
            room_tag(o, X(px), Y(py), num, name, wm, lm, clr)

    # dimension chains: bays along the top, overall below; rows left, overall
    yy = MYT - 64
    for i in range(len(cols) - 1):
        o += dim_h(X(cols[i][1]), X(cols[i + 1][1]), yy,
                   "%.2f" % ((cols[i + 1][1] - cols[i][1]) * cell_m))
    o += dim_h(X(0), X(W), MYT - 92, "%.1f m OVERALL" % Wm)
    xx = MXL - 58
    for i in range(len(rowsg) - 1):
        o += dim_v(Y(rowsg[i][1]), Y(rowsg[i + 1][1]), xx,
                   "%.2f" % ((rowsg[i + 1][1] - rowsg[i][1]) * cell_m))
    o += dim_v(Y(0), Y(H), MXL - 96, "%.1f m OVERALL" % Hm)

    north_arrow(o, MXL + pw + 40, MYT + 8)
    scale_bar(o, MXL, MYT + ph + 130, S / cell_m)

    # right column: schedule + legend + notes
    sx = int(MXL + pw + 90)
    if level in ("L1", "L2"):
        yend = schedule_table(o, sx, MYT - 40,
                              "ROOM SCHEDULE — %s" % ("LEVEL 1" if level == "L1" else "LEVEL 2"),
                              rooms[level])
        o.append('<text class="small" x="%d" y="%d">W x L measured as clear runs '
                 'through the room tag point.</text>' % (sx, yend + 18))
        ly = yend + 46
    else:
        ly = MYT - 40
    o.append('<text class="panel" x="%d" y="%d">LEGEND</text>' % (sx, ly))
    legend = [("poche", "perimeter wall, cut (solid poche)"),
              ("hatch", "tower / interior mass, cut (hatch)"),
              ("hatch2", "deck support 0.00-3.60, cut"),
              ("plate", "walkable deck plate at level"),
              ("floorp", "walkable floor at FFL 0.00"),
              ("stairp", "ramp volume, treads + UP"),
              ("over", "structure overhead (dashed)"),
              ("below", "structure below (faint)")]
    for i, (cls, txt) in enumerate(legend):
        yy2 = ly + 22 + i * 19
        o.append('<rect class="%s" x="%d" y="%d" width="26" height="11"/>' % (cls, sx, yy2 - 9))
        o.append('<text class="small" x="%d" y="%d">%s</text>' % (sx + 36, yy2, txt))
    lvl = ly + 22 + len(legend) * 19 + 22
    o.append('<text class="panel" x="%d" y="%d">LEVEL SCHEDULE</text>' % (sx, lvl))
    for i, line in enumerate(("FFL 0.00   LEVEL 1 (GROUND)",
                              "U.S. DECK  +3.60 (SOFFIT)",
                              "T.O. DECK  +4.00 (LEVEL 2)",
                              "T.O. TOWER +6.50",
                              "T.O. WALL  +8.00 (DATUM)")):
        o.append('<text class="note" x="%d" y="%d">%s</text>' % (sx, lvl + 20 + i * 17, line))
    notes_block(o, sx, lvl + 20 + 5 * 17 + 30)

    sheet_chrome(o, w, h, no, sname, "%.1f px/m nominal" % (S / cell_m))
    o.append("</svg>")
    fname = {"L1": "A101_level1_plan", "L2": "A102_level2_plan",
             "MEZZ": "A103_intermediate_plan"}[level]
    (out_dir / ("%s.svg" % fname)).write_text("\n".join(o), encoding="utf-8")


def spot_elev(o, x, y, txt):
    o.append('<g transform="translate(%g,%g)">' % (x, y))
    o.append('<path d="M -6 0 L 0 -6 L 6 0 L 0 6 Z" fill="none" stroke="#000" '
             'stroke-width="1.1"/>')
    o.append('<line x1="-6" y1="0" x2="6" y2="0" stroke="#000" stroke-width="0.8"/>')
    o.append('<text class="small" x="10" y="4">%s</text>' % txt)
    o.append('</g>')


def draw_stair_treads(o, solids, X, Y, cell_m, up_label):
    for s in solids:
        if s.kind != "stair":
            continue
        x0, y0, x1, y1 = s.bbox
        horiz = (x1 - x0) >= (y1 - y0)
        n = max(3, int((max(x1 - x0, y1 - y0) * cell_m) / 0.45))
        for i in range(1, n):
            t = i / n
            if horiz:
                xx = x0 + (x1 - x0) * t
                for a, b in col_intervals(s.cells, min(int(xx), x1 - 1)):
                    o.append('<line class="tread" x1="%g" y1="%g" x2="%g" y2="%g"/>'
                             % (X(xx), Y(a), X(xx), Y(b)))
            else:
                yy = y0 + (y1 - y0) * t
                for a, b in row_intervals(s.cells, min(int(yy), y1 - 1)):
                    o.append('<line class="tread" x1="%g" y1="%g" x2="%g" y2="%g"/>'
                             % (X(a), Y(yy), X(b), Y(yy)))
        if up_label:
            o.append('<text class="small" text-anchor="middle" x="%g" y="%g">UP</text>'
                     % (X((x0 + x1) / 2), Y((y0 + y1) / 2) + 3))


# ---------------------------------------------------------------------------
# A-201 — sections with grid bubbles, level tags, clearances

LEVEL_TAGS = ((0, "FFL 0.00"), (DECK[0], "U.S. DECK +3.60"), (DECK[1], "T.O. DECK +4.00"),
              (TOWER_H, "T.O. TOWER +6.50"), (WALL_H, "T.O. WALL +8.00"))


def sheet_sections(solids, W, H, cell_m, out_dir):
    S = 9.8
    zS = S / cell_m
    mx, my = 150, 130
    panel_h = int(WALL_H * zS + 120)
    w = int(W * S + mx + 330)
    h = int(3 * panel_h + my + 220)
    o = svg_open(w, h, "AQUARIUS — SECTIONS")
    o.append('<text class="big" x="%d" y="58">SECTIONS A-A · B-B · C-C — '
             'LEVELS FFL 0.00 / +3.60 / +4.00 / +6.50 / +8.00</text>' % (mx - 60))
    cols, rowsg = grid_lines(W, H, cell_m)
    INNER = ("tower", "support", "deck", "stair")

    def sky(cells_pred, axis, alen, kinds=None):
        top = [0.0] * alen
        for s in solids:
            if kinds and s.kind not in kinds:
                continue
            for (x, y) in s.cells:
                if cells_pred(x, y):
                    i = x if axis == "x" else y
                    if s.z1 > top[i]:
                        top[i] = s.z1
        out, i = [], 0
        while i < alen:
            if top[i] <= 0:
                i += 1
                continue
            j = i
            while j < alen and abs(top[j] - top[i]) < 1e-6:
                j += 1
            out.append((i, j, top[i]))
            i = j
        return out

    def draw_panel(oy, name, alen, cut_iv, layers, axis_label, refs, room_marks):
        nonlocal o
        gy = oy + panel_h - 66
        Z = lambda zm: gy - zm * zS
        Xp = lambda c: mx + c * S
        o.append('<text class="panel" x="%d" y="%d">%s</text>' % (mx - 40, oy + 6, name))
        for lab, cc in refs:                    # grid bubbles over the section
            o.append('<line class="gridl" x1="%g" y1="%g" x2="%g" y2="%g"/>'
                     % (Xp(cc), oy + 22, Xp(cc), Z(0) + 8))
            o.append('<circle class="bubble" cx="%g" cy="%g" r="11"/>' % (Xp(cc), oy + 26))
            o.append('<text class="label" x="%g" y="%g" text-anchor="middle">%s</text>'
                     % (Xp(cc), oy + 30, lab))
        for zm, lab in LEVEL_TAGS:
            o.append('<line class="datum" x1="%g" y1="%g" x2="%g" y2="%g"/>'
                     % (mx - 34, Z(zm), mx + alen * S + 30, Z(zm)))
            o.append('<text class="dimtext" x="%g" y="%g">%s</text>'
                     % (mx + alen * S + 36, Z(zm) + 3, lab))
        for ivs, cls in layers:
            for (a, b, z1) in ivs:
                o.append('<rect class="%s" x="%g" y="%g" width="%g" height="%g"/>'
                         % (cls, Xp(a), Z(z1), (b - a) * S, z1 * zS))
        for (a, b, z0, z1) in cut_iv:
            o.append('<rect class="poche" x="%g" y="%g" width="%g" height="%g"/>'
                     % (Xp(a), Z(z1), (b - a) * S, (z1 - z0) * zS))
        o.append('<line class="ground" x1="%g" y1="%g" x2="%g" y2="%g"/>'
                 % (mx - 34, Z(0), mx + alen * S + 30, Z(0)))
        # vertical level chain at left
        for (za, zb) in ((0, DECK[0]), (DECK[0], DECK[1]), (DECK[1], TOWER_H),
                         (TOWER_H, WALL_H)):
            o += dim_v(Z(zb), Z(za), mx - 46, "%.2f" % (zb - za))
        o += dim_v(Z(WALL_H), Z(0), mx - 86, "%.1f m" % WALL_H)
        for (cc, zt, txt) in room_marks:
            o.append('<text class="label" x="%g" y="%g" text-anchor="middle">%s</text>'
                     % (Xp(cc), Z(zt), txt))
        o.append('<text class="small" x="%d" y="%g">%s</text>' % (mx - 40, gy + 30, axis_label))

    # A-A — front, cut y=H/2, looking north
    yc = H // 2
    cut = []
    for s in solids:
        for a, b in row_intervals(s.cells, yc):
            cut.append((a, b, s.z0, s.z1))
    layers = [(sky(lambda x, y: y < yc, "x", W, ("wall",)), "beyond"),
              (sky(lambda x, y: y < yc, "x", W, INNER), "beyond2")]
    marks = [(W * 0.115, 1.6, "101"), (W * 0.335, 1.6, "102"), (W * 0.5, 5.6, "BRIDGE 203"),
             (W * 0.665, 1.6, "103"), (W * 0.885, 1.6, "104")]
    draw_panel(my, "SECTION A-A — FRONT, AT GRID 4 LOOKING NORTH", W, cut, layers,
               "west - east · cut through Hallway 1, both arenas, both bases · "
               "101/104 CLR 3.60 UNDER DECK, 102/103 CLR 8.00", cols, marks)

    # B-B — side, cut x=W/2, looking west; axis south -> north
    xc = W // 2
    cut = []
    for s in solids:
        for a, b in col_intervals(s.cells, xc):
            cut.append((H - b, H - a, s.z0, s.z1))
    flip = lambda ivs: [(H - b, H - a, z) for (a, b, z) in ivs]
    layers = [(flip(sky(lambda x, y: x < xc, "y", H, ("wall",))), "beyond"),
              (flip(sky(lambda x, y: x < xc, "y", H, INNER)), "beyond2")]
    refs = [(lab, H - cc) for lab, cc in rowsg][::-1]
    marks = [(H * (1 - 0.815), 1.6, "107"), (H * 0.5, 5.6, "BRIDGE 203"),
             (H * (1 - 0.145), 1.6, "106")]
    draw_panel(my + panel_h, "SECTION B-B — SIDE, AT GRID E LOOKING WEST", H, cut, layers,
               "south - north · cut through the BRIDGE and both long hallways · "
               "bridge deck T.O. +4.00 over Hallway 1", refs, marks)

    # C-C — through Hallway 2, looking north: the two-level side lane
    yc2 = int(H * 0.145)
    cut = []
    for s in solids:
        for a, b in row_intervals(s.cells, yc2):
            cut.append((a, b, s.z0, s.z1))
    layers = [(sky(lambda x, y: y < yc2, "x", W, ("wall",)), "beyond"),
              (sky(lambda x, y: y < yc2, "x", W, INNER), "beyond2")]
    marks = [(W * 0.5, 1.6, "106 LOWER · CLR 3.60"), (W * 0.5, 5.6, "204/205 WALK +4.00")]
    draw_panel(my + 2 * panel_h, "SECTION C-C — THROUGH HALLWAY 2 LOOKING NORTH "
               "(THE TWO-LEVEL SIDE LANE)", W, cut, layers,
               "west - east · lane floor FFL 0.00 with the walk deck over at +4.00 "
               "(founder-annotated upper level)", cols, marks)

    sheet_chrome(o, w, h, "A-201", "SECTIONS A-A / B-B / C-C", "%.1f px/m nominal" % zS)
    o.append("</svg>")
    (out_dir / "A201_sections.svg").write_text("\n".join(o), encoding="utf-8")


# ---------------------------------------------------------------------------
# A-301 — 3/4 blockout, solid massing (doctrine unchanged from the ruling)

def sheet_blockout(solids, floor, W, H, cell_m, out_dir):
    S = 11.0
    HX, HY, HZ = 0.74, 0.62, 0.42

    def P(x, y, z):
        Xw, Yw = x * cell_m, (H - y) * cell_m
        return ((Xw - Yw) * HX * S, (Xw + Yw) * HY * S - z * HZ * S)

    pts = [P(0, 0, 0), P(W, 0, 0), P(0, H, 0), P(W, H, 0),
           P(0, 0, WALL_H), P(W, 0, WALL_H), P(0, H, WALL_H), P(W, H, WALL_H)]
    minx = min(p[0] for p in pts)
    maxx = max(p[0] for p in pts)
    miny = min(p[1] for p in pts)
    maxy = max(p[1] for p in pts)
    ox, oy = 100 - minx, 140 - miny
    w = int(maxx - minx + 340)
    h = int(maxy - miny + 320)

    def Q(x, y, z):
        px, py = P(x, y, z)
        return (px + ox, py + oy)

    o = svg_open(w, h, "AQUARIUS — 3/4 BLOCKOUT")
    o.append('<text class="big" x="70" y="58">3/4 BLOCKOUT — SOLID MASSING · '
             'HIGH CAMERA FROM THE NE</text>')
    o.append('<text class="small" x="70" y="82">Solid extrusions of the traced polygons · '
             'full 8 m walls, no cutaway · ramps as wedges · interior floor grey</text>')

    plate = [Q(-2, -2, 0), Q(W + 2, -2, 0), Q(W + 2, H + 2, 0), Q(-2, H + 2, 0)]
    o.append('<path class="isoplate" d="M %s Z"/>'
             % " L ".join("%.1f %.1f" % p for p in plate))
    for comp in components(floor):
        if len(comp) < 30:
            continue
        d = []
        for lp in (simplify(l) for l in mask_loops(comp)):
            if len(lp) >= 3:
                d.append("M " + " L ".join("%.1f %.1f" % Q(px, py, 0)
                                           for px, py in lp) + " Z")
        if d:
            o.append('<path class="isofloor" fill-rule="evenodd" d="%s"/>' % " ".join(d))

    def interior_side(lp, i, cells):
        (ax, ay), (bx, by) = lp[i], lp[(i + 1) % len(lp)]
        mx_, my_ = (ax + bx) / 2, (ay + by) / 2
        dx, dy = bx - ax, by - ay
        L = math.hypot(dx, dy) or 1e-9
        nx, ny = dy / L, -dx / L
        for sgn in (1, -1):
            cx = int(mx_ + sgn * nx * 0.55)
            cy = int(my_ + sgn * ny * 0.55)
            if (cx, cy) in cells:
                return (-sgn * nx, -sgn * ny)
        return (nx, ny)

    def draw_prism(s, cls_top="isotop"):
        # DOUBLE-SIDED (founder, 30 Aug): every side face draws, far to
        # near, so no culling-sign mistake can hollow a mass; the sampled
        # normal only picks the tone.
        z0, z1 = s.z0, s.z1
        faces = []
        for lp in s.loops:
            for i in range(len(lp)):
                nx, ny = interior_side(lp, i, s.cells)
                a, b = lp[i], lp[(i + 1) % len(lp)]
                mx = (a[0] + b[0]) / 2
                my = (a[1] + b[1]) / 2
                faces.append((mx + (H - my), a, b, nx, -ny))
        for _, a, b, ne, nn in sorted(faces, key=lambda f: f[0]):
            quad = [Q(a[0], a[1], z0), Q(b[0], b[1], z0),
                    Q(b[0], b[1], z1), Q(a[0], a[1], z1)]
            cls = "isoeast" if ne >= nn else "isonorth"
            o.append('<path class="%s" d="M %s Z"/>'
                     % (cls, " L ".join("%.1f %.1f" % p for p in quad)))
        d = []
        for lp in s.loops:
            d.append("M " + " L ".join("%.1f %.1f" % Q(px, py, z1) for px, py in lp) + " Z")
        o.append('<path class="%s" fill-rule="evenodd" d="%s"/>' % (cls_top, " ".join(d)))

    upper_cells = set().union(*(s.cells for s in solids if s.kind == "deck")) \
        if any(s.kind == "deck" for s in solids) else set()

    def draw_ramp_wedge(s):
        x0, y0, x1, y1 = s.bbox
        horiz = (x1 - x0) >= (y1 - y0)

        def deck_touch(edge_cells):
            return sum(1 for c in edge_cells if c in upper_cells)

        if horiz:
            lo_e = [(x0 - 1, y) for y in range(y0, y1)]
            hi_e = [(x1, y) for y in range(y0, y1)]
            corners_lo = [(x0, y0), (x0, y1)]
            corners_hi = [(x1, y0), (x1, y1)]
        else:
            lo_e = [(x, y0 - 1) for x in range(x0, x1)]
            hi_e = [(x, y1) for x in range(x0, x1)]
            corners_lo = [(x0, y0), (x1, y0)]
            corners_hi = [(x0, y1), (x1, y1)]
        if deck_touch(lo_e) > deck_touch(hi_e):
            corners_lo, corners_hi = corners_hi, corners_lo
        zlo, zhi = 0.25, s.z1
        (a, b), (c, d) = corners_lo, corners_hi
        top = [Q(a[0], a[1], zlo), Q(b[0], b[1], zlo),
               Q(d[0], d[1], zhi), Q(c[0], c[1], zhi)]
        zz = {a: zlo, b: zlo, c: zhi, d: zhi}
        for e0, e1 in ((a, b), (b, d), (d, c), (c, a)):
            nx, ny = interior_side([e0, e1, (2 * e1[0] - e0[0], 2 * e1[1] - e0[1])],
                                   0, s.cells)
            ne, nn = nx, -ny
            if ne + nn <= 0.02:
                continue
            quad = [Q(e0[0], e0[1], 0), Q(e1[0], e1[1], 0),
                    Q(e1[0], e1[1], zz[e1]), Q(e0[0], e0[1], zz[e0])]
            cls = "isoeast" if ne >= nn else "isonorth"
            o.append('<path class="%s" d="M %s Z"/>'
                     % (cls, " L ".join("%.1f %.1f" % p for p in quad)))
        o.append('<path class="isotop" d="M %s Z"/>'
                 % " L ".join("%.1f %.1f" % p for p in top))
        n = max(3, int(math.hypot(c[0] - a[0], c[1] - a[1]) * cell_m / 0.9))
        for i in range(1, n):
            t = i / n
            p0 = Q(a[0] + (c[0] - a[0]) * t, a[1] + (c[1] - a[1]) * t,
                   zlo + (zhi - zlo) * t)
            p1 = Q(b[0] + (d[0] - b[0]) * t, b[1] + (d[1] - b[1]) * t,
                   zlo + (zhi - zlo) * t)
            o.append('<line class="tread" x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f"/>'
                     % (p0[0], p0[1], p1[0], p1[1]))

    def depth(s):
        x0, y0, x1, y1 = s.bbox
        return (x0 + x1) / 2 + (H - (y0 + y1) / 2)

    for s in sorted(solids, key=lambda s: (depth(s), s.z0)):
        if s.kind == "stair":
            draw_ramp_wedge(s)
        elif s.kind == "deck":
            draw_prism(s, cls_top="isoplate")
        else:
            draw_prism(s)

    def biggest(kind):
        cands = [s for s in solids if s.kind == kind]
        return max(cands, key=lambda s: len(s.cells)) if cands else None

    def centroid(s):
        return (sum(x for x, _ in s.cells) / len(s.cells) + 0.5,
                sum(y for _, y in s.cells) / len(s.cells) + 0.5)

    anchors = []
    far_wall = min((s for s in solids if s.kind == "wall"), key=depth, default=None)
    if far_wall:
        anchors.append(("PERIMETER 8.0 m", far_wall, WALL_H, -30, -40))
    for kind, txt, dz, tx, ty in (("tower", "HYDRO TOWER 6.5 m", TOWER_H, -220, -60),
                                  ("deck", "DECK +4.0 m (PILLAR-BORNE)", DECK[1], 230, -110),
                                  ("stair", "RAMP 0.00 to +4.00", DECK[1], 150, 130)):
        s = biggest(kind)
        if s:
            anchors.append((txt, s, dz, tx, ty))
    for txt, s, dz, tx, ty in anchors:
        cx, cy = centroid(s)
        px, py = Q(cx, cy, dz)
        o.append('<circle cx="%g" cy="%g" r="2.4" fill="#000"/>' % (px, py))
        o.append('<line class="dim" x1="%g" y1="%g" x2="%g" y2="%g"/>'
                 % (px, py, px + tx, py + ty))
        o.append('<text class="label" x="%g" y="%g" text-anchor="%s">%s</text>'
                 % (px + tx + (6 if tx >= 0 else -6), py + ty - 4,
                    "start" if tx >= 0 else "end", txt))

    sheet_chrome(o, w, h, "A-301", "3/4 BLOCKOUT (SOLID MASSING)", "steep 3/4 from NE")
    o.append("</svg>")
    (out_dir / "A301_blockout.svg").write_text("\n".join(o), encoding="utf-8")


# ---------------------------------------------------------------------------

def rasterize(out_dir):
    chromium = shutil.which("chromium") or "/opt/pw-browsers/chromium"
    if not Path(chromium).exists():
        print("note: no chromium found — SVGs stand alone; --png skipped")
        return
    import re as _re
    for svg in sorted(out_dir.glob("A*.svg")):
        png = svg.with_suffix(".png")
        head = svg.read_text(encoding="utf-8")[:200]
        m = _re.search(r'width="(\d+)" height="(\d+)"', head)
        win = "%d,%d" % (int(m.group(1)) + 24, int(m.group(2)) + 70) if m else "2400,1600"
        subprocess.run([chromium, "--headless", "--disable-gpu", "--no-sandbox",
                        "--force-device-scale-factor=2", "--default-background-color=FFFFFFFF",
                        "--screenshot=%s" % png, "--hide-scrollbars",
                        "--window-size=%s" % win, svg.as_uri()],
                       check=True, capture_output=True, timeout=120)
        print("  rasterized %s" % png.name)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=None)
    ap.add_argument("--png", action="store_true")
    args = ap.parse_args()
    out_dir = Path(args.out) if args.out else \
        GAME / "docs" / "design" / "blueprints" / "breachpoint_aquarius"
    out_dir.mkdir(parents=True, exist_ok=True)

    solids, floor, W, H, cell_m = extract()
    deckcells = set().union(*(s.cells for s in solids if s.kind == "deck")) \
        if any(s.kind == "deck" for s in solids) else set()
    rooms = build_rooms(floor, deckcells, W, H, cell_m)
    n = {k: sum(1 for s in solids if s.kind == k)
         for k in ("wall", "tower", "support", "deck", "stair")}
    print("extracted %d solids %s · grid %dx%d · cell %.3f m · rooms L1 %d / L2 %d"
          % (len(solids), n, W, H, cell_m, len(rooms["L1"]), len(rooms["L2"])))
    for lvl in ("L1", "L2", "MEZZ"):
        plan_sheet(solids, floor, deckcells, rooms, W, H, cell_m, out_dir, lvl)
    sheet_sections(solids, W, H, cell_m, out_dir)
    sheet_blockout(solids, floor, W, H, cell_m, out_dir)
    print("wrote 5 sheets (A-101/102/103/201/301) into %s" % out_dir)
    if args.png:
        rasterize(out_dir)


if __name__ == "__main__":
    main()
