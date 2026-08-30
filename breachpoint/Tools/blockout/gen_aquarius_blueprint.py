#!/usr/bin/env python3
"""BN29 — AQUARIUS 1:1 blueprint sheets, drawn from POLYGONS, not boxes.

    python3 Tools/blockout/gen_aquarius_blueprint.py --png

The founder's ruling (30 Aug): "re do the blueprints to be as accurate 1:1 as
possible - front, side, top, 3/4, level floors, height, width, stairs,
platforms, columns, corners, floors, walls." The generic gen_blueprint.py
draws the MANIFEST's axis-aligned boxes - honest for a blockout, blocky as a
drawing. This script goes back to the traced per-floor grids at FULL
resolution (~0.27 m cells), classifies them with the same v3 semantics as
gen_aquarius_manifest.py, extracts the region BOUNDARIES as polygons
(edge-chaining + Douglas-Peucker, so the 45-degree chamfers come out as true
diagonals - crisp walls, hex towers, true corners), and draws:

  S1  floor plans L0/L1 - poche cut, walkable plates, stairs, founder labels
  S2  front (Section A-A, looking north), side (Section B-B, looking west),
      Section C-C through Hallway 2's two-level lane - all with level datums
  S3  3/4 blockout view - solid massing, high camera, ramp wedges

LEVEL SCHEDULE (authored from the founder's 25 references; scale itself is
the one derived number - 52 m long axis until a Forge walk replaces it):
  0.00 ground · +3.60 deck soffit · +4.00 deck top · +6.50 hydro-tower top
  +8.00 perimeter/ceiling datum · ramps rise 0.00 -> +4.00

Stdlib only; --png rasterizes via headless chromium like gen_blueprint.py.
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
DP_EPS_CELLS = 1.25          # staircase -> diagonal without eating castellations

STYLE = """
  .paper{fill:#ffffff}
  .frame{fill:none;stroke:#000;stroke-width:1.4}
  .poche{fill:#111111;stroke:#000;stroke-width:0.8}
  .pochelow{fill:#555555;stroke:#000;stroke-width:0.8}
  .plate{fill:#e8e8e8;stroke:#000;stroke-width:1.0}
  .stairp{fill:#f2f2f2;stroke:#000;stroke-width:1.0}
  .over{fill:none;stroke:#000;stroke-width:0.9;stroke-dasharray:6 4}
  .below{fill:none;stroke:#9a9a9a;stroke-width:0.7}
  .tread{stroke:#000;stroke-width:0.6;fill:none}
  .cutline{stroke:#000;stroke-width:1.6}
  .beyond{fill:#f4f4f4;stroke:#8a8a8a;stroke-width:0.8}
  .beyond2{fill:#d9d9d9;stroke:#6e6e6e;stroke-width:0.8}
  .ground{stroke:#000;stroke-width:2.4}
  .dim{stroke:#000;stroke-width:0.8;fill:none}
  .dimtext{font:10px monospace;fill:#000}
  .label{font:bold 10.5px monospace;fill:#000;letter-spacing:0.5px}
  .rooml{font:bold 12px monospace;fill:#000;letter-spacing:1px}
  .small{font:9px monospace;fill:#000}
  .big{font:bold 16px monospace;fill:#000;letter-spacing:1px}
  .panel{font:bold 13px monospace;fill:#000;letter-spacing:1px}
  .datum{stroke:#000;stroke-width:0.7;stroke-dasharray:12 4 2 4}
  .isotop{fill:#ededed;stroke:#000;stroke-width:1.0;stroke-linejoin:round}
  .isoeast{fill:#ffffff;stroke:#000;stroke-width:1.0;stroke-linejoin:round}
  .isonorth{fill:#d4d4d4;stroke:#000;stroke-width:1.0;stroke-linejoin:round}
  .isoplate{fill:#f7f7f7;stroke:#000;stroke-width:1.6;stroke-linejoin:round}
  .isofloor{fill:#e2e2e2;stroke:#555;stroke-width:0.8;stroke-linejoin:round}
"""


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
    """Boundary loops of a cell set as vertex lists (cell corners), interior on
    the left. Outer loops and hole loops both come back; evenodd fill sorts
    them out in the drawing and the interior-sampling normal test in the axon."""
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
            else:                       # checkerboard corner: keep turning left
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
# svg helpers

def svg_open(w, h, title):
    return ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" '
            'viewBox="0 0 %d %d">' % (w, h, w, h),
            "<style>%s</style>" % STYLE,
            '<rect class="paper" width="%d" height="%d"/>' % (w, h),
            '<title>%s</title>' % title]


def title_block(w, h, sheet_no, sheet_name, scale_txt):
    x0, y0 = w - 352, h - 100
    rows = [("PROJECT", "BREACHPOINT — 4v4 ARENA FPS"),
            ("DRAWING", sheet_name),
            ("ARENA / SHEET", "breachpoint_aquarius   ·   %s" % sheet_no),
            ("SCALE / DATE", "%s   ·   %s   ·   REV C" % (scale_txt, date.today().isoformat()))]
    out = ['<g>', '<rect class="frame" x="%d" y="%d" width="336" height="84"/>' % (x0, y0)]
    for i, (k, v) in enumerate(rows):
        yy = y0 + 18 + i * 20
        out.append('<text class="small" x="%d" y="%d">%s</text>' % (x0 + 8, yy, k))
        out.append('<text class="dimtext" x="%d" y="%d">%s</text>' % (x0 + 110, yy, v))
        if i:
            out.append('<line class="dim" x1="%d" y1="%d" x2="%d" y2="%d"/>'
                       % (x0, y0 + 4 + i * 20, x0 + 336, y0 + 4 + i * 20))
    out.append('</g>')
    return out


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


# ---------------------------------------------------------------------------
# S1 — floor plans

def sheet_plans(solids, floor, W, H, cell_m, out_dir):
    S = 9.4                                    # px per cell
    pw, ph = W * S, H * S
    mx, my = 90, 120
    w = int(2 * pw + 3 * mx + 40)
    h = int(ph + my + 260)
    o = svg_open(w, h, "AQUARIUS — FLOOR PLANS")
    o.append('<text class="big" x="%d" y="46">FLOOR PLANS — L0 GROUND ±0.00 · '
             'L1 DECKS +4.00 · CUT 1.5 m ABOVE EACH · POCHE = CUT STRUCTURE</text>' % mx)

    Wm, Hm = W * cell_m, H * cell_m
    labels = [("BASE 1", 0.11, 0.50), ("ARENA 1", 0.33, 0.42),
              ("ARENA 2", 0.67, 0.42), ("BASE 2", 0.89, 0.50),
              ("HALLWAY 2", 0.50, 0.145), ("HALLWAY 3", 0.50, 0.815),
              ("HALLWAY 1", 0.415, 0.53)]
    wall_cells = set().union(*(s.cells for s in solids if s.kind == "wall"))

    for pi, (lid, lname) in enumerate((("L0", "GROUND  ±0.00"), ("L1", "DECKS  +4.00"))):
        ox = mx + pi * (pw + mx)
        X = lambda c, ox=ox: ox + c * S
        Y = lambda c: my + c * S
        o.append('<rect class="frame" x="%g" y="%g" width="%g" height="%g"/>'
                 % (ox - 8, my - 8, pw + 16, ph + 16))
        o.append('<text class="panel" x="%g" y="%g">%s — %s</text>' % (ox, my - 16, lid, lname))
        cut = (1.5 if pi == 0 else DECK[1] + 1.5)

        if pi == 1:      # ground context first, faint
            for s in solids:
                if s.kind in ("tower", "support", "stair"):
                    o.append(path_of(s.loops, X, Y, "below"))
        for s in solids:                                   # walkable plates
            if pi == 0 and s.kind == "stair":
                o.append(path_of(s.loops, X, Y, "stairp"))
            if pi == 1 and s.kind == "deck":
                o.append(path_of(s.loops, X, Y, "plate"))
        for s in solids:                                   # cut structure
            if s.z0 <= cut - 0.1 and s.z1 > cut:
                o.append(path_of(s.loops, X, Y, "poche"))
            elif pi == 0 and s.kind == "support":          # support: cut low
                o.append(path_of(s.loops, X, Y, "poche"))
        if pi == 0:                                        # decks overhead
            for s in solids:
                if s.kind == "deck":
                    o.append(path_of(s.loops, X, Y, "over"))
        if pi == 1:                                        # ramps arriving
            for s in solids:
                if s.kind == "stair":
                    o.append(path_of(s.loops, X, Y, "over"))

        for s in solids:                                   # stair treads + UP
            if s.kind != "stair":
                continue
            x0, y0, x1, y1 = s.bbox
            horiz = (x1 - x0) >= (y1 - y0)
            n = max(3, int((max(x1 - x0, y1 - y0) * cell_m) / 0.45))
            for i in range(1, n):
                t = i / n
                if horiz:
                    xx = x0 + (x1 - x0) * t
                    iv = col_intervals(s.cells, min(int(xx), x1 - 1))
                    for a, b in iv:
                        o.append('<line class="tread" x1="%g" y1="%g" x2="%g" y2="%g"/>'
                                 % (X(xx), Y(a), X(xx), Y(b)))
                else:
                    yy = y0 + (y1 - y0) * t
                    iv = row_intervals(s.cells, min(int(yy), y1 - 1))
                    for a, b in iv:
                        o.append('<line class="tread" x1="%g" y1="%g" x2="%g" y2="%g"/>'
                                 % (X(a), Y(yy), X(b), Y(yy)))
            cxp, cyp = X((x0 + x1) / 2), Y((y0 + y1) / 2)
            if pi == 0:
                o.append('<text class="small" text-anchor="middle" x="%g" y="%g">UP</text>'
                         % (cxp, cyp + 3))

        for txt, fx, fy in labels:                          # founder's names
            if pi == 1 and txt.startswith("HALLWAY"):
                continue
            o.append('<text class="rooml" text-anchor="middle" x="%g" y="%g">%s</text>'
                     % (X(fx * W), Y(fy * H), txt))
        if pi == 1:
            o.append('<text class="rooml" text-anchor="middle" x="%g" y="%g">BRIDGE</text>'
                     % (X(0.5 * W), Y(0.30 * H)))
            o.append('<text class="small" text-anchor="middle" x="%g" y="%g">(N-S · ROCKET AT CENTER)</text>'
                     % (X(0.5 * W), Y(0.30 * H) + 12))

        o += dim_h(X(0), X(W), my - 34, "%.1f m" % Wm)
        o += dim_v(Y(0), Y(H), ox - 26, "%.1f m" % Hm)

        # keyed dims: one hydro tower, Hallway 2 clear width, wall thickness
        if pi == 0:
            tw = [s for s in solids if s.kind == "tower"
                  and (s.bbox[2] - s.bbox[0]) * cell_m >= 3]
            if tw:
                big = max(tw, key=lambda s:
                          (s.bbox[2] - s.bbox[0]) * (s.bbox[3] - s.bbox[1]))
                x0, y0, x1, y1 = big.bbox
                o += dim_h(X(x0), X(x1), Y(y0) - 8, "%.1f" % ((x1 - x0) * cell_m))
                o += dim_v(Y(y0), Y(y1), X(x1) + 22, "%.1f" % ((y1 - y0) * cell_m))
            lane = col_intervals(floor, W // 2)
            if lane:
                a, b = lane[0]                       # north lane clear width
                o += dim_v(Y(a), Y(b), X(W // 2) - 40,
                           "%.1f clear" % ((b - a) * cell_m))
            wint = col_intervals(wall_cells, W // 2)
            if wint:
                a, b = wint[0]                       # north wall thickness
                o += dim_v(Y(a), Y(b), X(W // 2) + 40,
                           "%.1f wall" % ((b - a) * cell_m))
        else:
            decks = [s for s in solids if s.kind == "deck"]
            ivals = []
            for s in decks:
                ivals += row_intervals(s.cells, H // 2)
            mid = [iv for iv in ivals if abs((iv[0] + iv[1]) / 2 - W / 2) < W * 0.12]
            if mid:
                a, b = mid[0]
                o += dim_h(X(a), X(b), Y(H // 2) - 8, "%.1f" % ((b - a) * cell_m))

    # north arrow + legend
    o.append('<g transform="translate(%d,%d)">' % (w - 60, 60))
    o.append('<line class="cutline" x1="0" y1="18" x2="0" y2="-14"/>')
    o.append('<path d="M -6 -8 L 0 -20 L 6 -8 Z" fill="#000"/>')
    o.append('<text class="label" x="-4" y="34">N</text></g>')
    ly = h - 210
    o.append('<rect class="frame" x="%d" y="%d" width="560" height="168"/>' % (mx, ly))
    o.append('<text class="label" x="%d" y="%d">LEGEND · LEVEL SCHEDULE</text>' % (mx + 10, ly + 18))
    items = [("poche", "cut structure (walls · towers · supports)"),
             ("plate", "walkable deck plate at level"),
             ("stairp", "ramp/stair volume, treads + UP"),
             ("over", "structure overhead (dashed)"),
             ("below", "structure below (faint)")]
    for i, (cls, txt) in enumerate(items):
        yy = ly + 36 + i * 18
        o.append('<rect class="%s" x="%d" y="%d" width="22" height="10"/>' % (cls, mx + 12, yy - 9))
        o.append('<text class="small" x="%d" y="%d">%s</text>' % (mx + 42, yy, txt))
    sched = ["0.00 ground", "+3.60 deck soffit", "+4.00 deck top",
             "+6.50 hydro-tower top", "+8.00 perimeter datum",
             "ramps rise 0.00 to +4.00"]
    for i, txt in enumerate(sched):
        o.append('<text class="small" x="%d" y="%d">%s</text>'
                 % (mx + 330, ly + 36 + i * 18, txt))
    o.append('<text class="small" x="%d" y="%d">SCALE: long axis %.0f m DERIVED '
             '(Forge walk owed) · heights authored from reference shots</text>'
             % (mx, h - 24, LONG_AXIS_M))
    o += title_block(w, h, "S1", "FLOOR PLANS L0 / L1 (POLYGON-TRUE)", "%.1f px/m" % (S / cell_m))
    o.append("</svg>")
    (out_dir / "S1_plan.svg").write_text("\n".join(o), encoding="utf-8")


# ---------------------------------------------------------------------------
# S2 — front/side sections + south exterior elevation

def sheet_sections(solids, W, H, cell_m, out_dir):
    S = 9.4
    zS = S / cell_m                       # px per meter, vertical
    pw = W * S
    pws = H * S
    mx, my = 100, 96
    panel_h = int(WALL_H * zS + 70)
    w = int(pw + 2 * mx)
    h = int(3 * panel_h + my + 210)
    o = svg_open(w, h, "AQUARIUS — SECTIONS")
    o.append('<text class="big" x="%d" y="46">FRONT · SIDE · SOUTH ELEVATION — '
             'DATUMS 0.00 / +3.60 / +4.00 / +6.50 / +8.00</text>' % mx)

    def draw_panel(o, oy, name, axis_len_cells, cut_iv, beyond_sky, axis_label):
        gy = oy + panel_h - 40
        Z = lambda zm: gy - zm * zS
        Xp = lambda c: mx + c * S
        o.append('<text class="panel" x="%d" y="%d">%s</text>' % (mx, oy + 4, name))
        for zm, lab in ((0, "0.00"), (DECK[0], "+3.60"), (DECK[1], "+4.00"),
                        (TOWER_H, "+6.50"), (WALL_H, "+8.00")):
            o.append('<line class="datum" x1="%g" y1="%g" x2="%g" y2="%g"/>'
                     % (mx - 30, Z(zm), mx + axis_len_cells * S + 30, Z(zm)))
            o.append('<text class="dimtext" x="%g" y="%g">%s</text>'
                     % (mx + axis_len_cells * S + 34, Z(zm) + 3, lab))
        for ivs, cls in beyond_sky:                  # far to near layers
            for (a, b, z1) in ivs:
                o.append('<rect class="%s" x="%g" y="%g" width="%g" height="%g"/>'
                         % (cls, Xp(a), Z(z1), (b - a) * S, z1 * zS))
        for (a, b, z0, z1) in cut_iv:
            o.append('<rect class="poche" x="%g" y="%g" width="%g" height="%g"/>'
                     % (Xp(a), Z(z1), (b - a) * S, (z1 - z0) * zS))
        o.append('<line class="ground" x1="%g" y1="%g" x2="%g" y2="%g"/>'
                 % (mx - 30, Z(0), mx + axis_len_cells * S + 30, Z(0)))
        o += dim_h(Xp(0), Xp(axis_len_cells), oy + 18,
                   "%.1f m" % (axis_len_cells * cell_m))
        o += dim_v(Z(WALL_H), Z(0), mx - 40, "%.1f m" % WALL_H)
        o.append('<text class="small" x="%d" y="%g">%s</text>' % (mx, gy + 26, axis_label))

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

    INNER = ("tower", "support", "deck", "stair")

    # FRONT — Section A-A, cut y = H/2, looking NORTH (rows < H/2 beyond).
    # Beyond in two tones: the far perimeter light, interior masses darker.
    yc = H // 2
    cut = []
    for s in solids:
        for a, b in row_intervals(s.cells, yc):
            cut.append((a, b, s.z0, s.z1))
    layers = [(sky(lambda x, y: y < yc, "x", W, ("wall",)), "beyond"),
              (sky(lambda x, y: y < yc, "x", W, INNER), "beyond2")]
    draw_panel(o, my, "FRONT — SECTION A-A AT MID (LOOKING NORTH)", W, cut, layers,
               "west  ·  east — cut through Hallway 1 / the arenas / both bases")

    # SIDE — Section B-B, cut x = W/2, looking WEST (cols < W/2 beyond).
    # Horizontal axis: south on the left, north on the right (viewer faces west).
    xc = W // 2
    cut = []
    for s in solids:
        for a, b in col_intervals(s.cells, xc):
            cut.append((H - b, H - a, s.z0, s.z1))
    flip = lambda ivs: [(H - b, H - a, z) for (a, b, z) in ivs]
    layers = [(flip(sky(lambda x, y: x < xc, "y", H, ("wall",))), "beyond"),
              (flip(sky(lambda x, y: x < xc, "y", H, INNER)), "beyond2")]
    draw_panel(o, my + panel_h, "SIDE — SECTION B-B AT MID (LOOKING WEST)", H, cut, layers,
               "south  ·  north — cut through the BRIDGE and both long hallways")

    # SECTION C-C — through HALLWAY 2 (the north long lane), looking north:
    # the founder's batch-4 pin made the side hallways TWO-LEVEL; this cut
    # shows the lane floor, the deck over it, and the perimeter behind.
    yc2 = int(H * 0.145)
    cut = []
    for s in solids:
        for a, b in row_intervals(s.cells, yc2):
            cut.append((a, b, s.z0, s.z1))
    layers = [(sky(lambda x, y: y < yc2, "x", W, ("wall",)), "beyond"),
              (sky(lambda x, y: y < yc2, "x", W, INNER), "beyond2")]
    draw_panel(o, my + 2 * panel_h,
               "SECTION C-C THROUGH HALLWAY 2 (LOOKING NORTH) — THE TWO-LEVEL SIDE LANE",
               W, cut, layers,
               "west  ·  east — lane floor at 0.00, deck over the lane at +4.00 "
               "(the founder-annotated upper level), perimeter beyond")

    o.append('<text class="small" x="%d" y="%d">Cut = black poche · beyond = grey silhouette '
             'by column (union skyline) · ramps read in Section B-B as their full volume</text>'
             % (mx, h - 24))
    o += title_block(w, h, "S2", "SECTIONS A-A / B-B / C-C", "%.1f px/m" % zS)
    o.append("</svg>")
    (out_dir / "S2_elevations.svg").write_text("\n".join(o), encoding="utf-8")


# ---------------------------------------------------------------------------
# S3 — 3/4 axonometric from extruded polygons

COS30 = math.cos(math.radians(30))


def sheet_axon(solids, floor, W, H, cell_m, out_dir):
    """The BLOCKOUT view (founder, 30 Aug): 'like an actual blockout level
    design... make sure the walls are not hollow.' REV C's iso-30 cutaway
    (near wall sliced to 2 m) read as hollow shells - gone. Every mass now
    draws SOLID at full height, and the camera goes UP instead: a steep
    3/4 from the north-east, high enough to see down into the arena over
    the intact 8 m perimeter. Ramps draw as real wedges, low end to high."""
    S = 11.0
    HX, HY, HZ = 0.74, 0.62, 0.42     # steep dimetric: high camera, solid walls

    def P(x, y, z):
        """world: X east = x·cell, Y north = (H−y)·cell; steep 3/4 from NE."""
        Xw, Yw = x * cell_m, (H - y) * cell_m
        return ((Xw - Yw) * HX * S, (Xw + Yw) * HY * S - z * HZ * S)

    pts = [P(0, 0, 0), P(W, 0, 0), P(0, H, 0), P(W, H, 0),
           P(0, 0, WALL_H), P(W, 0, WALL_H), P(0, H, WALL_H), P(W, H, WALL_H)]
    minx = min(p[0] for p in pts)
    maxx = max(p[0] for p in pts)
    miny = min(p[1] for p in pts)
    maxy = max(p[1] for p in pts)
    ox, oy = 80 - minx, 120 - miny
    w = int(maxx - minx + 300)
    h = int(maxy - miny + 300)

    def Q(x, y, z):
        px, py = P(x, y, z)
        return (px + ox, py + oy)

    o = svg_open(w, h, "AQUARIUS — 3/4 BLOCKOUT")
    o.append('<text class="big" x="60" y="46">3/4 BLOCKOUT — SOLID MASSING · '
             'HIGH CAMERA FROM THE NE</text>')

    # ground plate, then the walkable interior floor as its own grey plate
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
        nx, ny = dy / L, -dx / L          # candidate normal
        for sgn in (1, -1):
            cx = int(mx_ + sgn * nx * 0.55)
            cy = int(my_ + sgn * ny * 0.55)
            if (cx, cy) in cells:
                return (-sgn * nx, -sgn * ny)   # outward = away from interior
        return (nx, ny)

    def draw_prism(s, z1_override=None, cls_top="isotop"):
        z0, z1 = s.z0, (z1_override if z1_override is not None else s.z1)
        for lp in s.loops:
            for i in range(len(lp)):
                nx, ny = interior_side(lp, i, s.cells)
                # world normal: east = +x, north = -y(cell)
                ne, nn = nx, -ny
                if ne + nn <= 0.02:
                    continue
                a, b = lp[i], lp[(i + 1) % len(lp)]
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
        """Blockout ramp: the stair bbox as a solid wedge, low end at the
        arena floor, high end against whichever side touches the deck."""
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
        base = {p: 0.0 for p in (a, b, c, d)}
        zz = {a: zlo, b: zlo, c: zhi, d: zhi}
        for e0, e1 in ((a, b), (b, d), (d, c), (c, a)):
            mxx = (e0[0] + e1[0]) / 2
            myy = (e0[1] + e1[1]) / 2
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
        # tread ticks on the slope so it reads as a ramp, not a slab
        n = max(3, int(math.hypot(c[0] - a[0], c[1] - a[1]) * cell_m / 0.9))
        for i in range(1, n):
            t = i / n
            p0 = Q(a[0] + (c[0] - a[0]) * t, a[1] + (c[1] - a[1]) * t,
                   zlo + (zhi - zlo) * t)
            p1 = Q(b[0] + (d[0] - b[0]) * t, b[1] + (d[1] - b[1]) * t,
                   zlo + (zhi - zlo) * t)
            o.append('<line class="tread" x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f"/>'
                     % (p0[0], p0[1], p1[0], p1[1]))

    # ONE painter's list, far to near: NE viewer => near = big (x + (H - y)).
    # EVERYTHING solid, full height - no cutaway (the founder's ruling: an
    # actual blockout, no hollow walls). The high camera does the seeing-in.
    def depth(s):
        x0, y0, x1, y1 = s.bbox
        return (x0 + x1) / 2 + (H - (y0 + y1) / 2)

    order = sorted(solids, key=lambda s: (depth(s), s.z0))
    for s in order:
        if s.kind == "stair":
            draw_ramp_wedge(s)
        elif s.kind == "deck":
            draw_prism(s, cls_top="isoplate")
        else:
            draw_prism(s)

    # labels anchored to real masses, leaders out to clear paper
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

    o.append('<text class="small" x="60" y="76">Solid extrusions of the traced polygons · full 8 m walls, no cutaway · '
             'ramps as wedges · interior floor grey</text>')
    o += title_block(w, h, "S3", "3/4 BLOCKOUT (SOLID MASSING)", "steep 3/4 from NE")
    o.append("</svg>")
    (out_dir / "S3_axonometric.svg").write_text("\n".join(o), encoding="utf-8")


# ---------------------------------------------------------------------------

def rasterize(out_dir):
    chromium = shutil.which("chromium") or "/opt/pw-browsers/chromium"
    if not Path(chromium).exists():
        print("note: no chromium found — SVGs stand alone; --png skipped")
        return
    import re as _re
    for svg in sorted(out_dir.glob("S*.svg")):
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
    n = {k: sum(1 for s in solids if s.kind == k)
         for k in ("wall", "tower", "support", "deck", "stair")}
    print("extracted %d solids %s · grid %dx%d · cell %.3f m"
          % (len(solids), n, W, H, cell_m))
    sheet_plans(solids, floor, W, H, cell_m, out_dir)
    sheet_sections(solids, W, H, cell_m, out_dir)
    sheet_axon(solids, floor, W, H, cell_m, out_dir)
    print("wrote 3 sheets into %s" % out_dir)
    if args.png:
        rasterize(out_dir)


if __name__ == "__main__":
    main()
