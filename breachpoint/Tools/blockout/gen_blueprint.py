#!/usr/bin/env python3
"""BN28 — engineering blueprints from an arena manifest.

    python3 Tools/blockout/gen_blueprint.py                      # Arena01, SVG sheets
    python3 Tools/blockout/gen_blueprint.py --manifest <path>    # any manifest
    python3 Tools/blockout/gen_blueprint.py --png                # + rasterize via chromium

The level-design pipeline's DRAWING half. The manifest is the source of truth
(ue-editor doctrine); the editor's .umap is one projection of it and these sheets
are another — top-view PLAN cut at 1.5 m, SOUTH and WEST ELEVATIONS, a SECTION
through the arena's fight corridor, and a 3/4 AXONOMETRIC, drawn the way a civil
engineer draws them: white paper, black ink, dimension strings, level datums,
grid, north arrow, scale bar, title block. Iterating the level = edit the
manifest, rerun this, look again. Nothing here is hand-placed.

Stdlib only. The optional --png step shells out to headless chromium when one
exists (the cloud container ships one); the SVGs are the artifact of record.
"""

from __future__ import annotations

import argparse
import json
import math
import shutil
import subprocess
import sys
from datetime import date
from pathlib import Path

HERE = Path(__file__).resolve().parent
GAME = HERE.parents[1]

# ---------------------------------------------------------------------------
# geometry gathering: everything becomes a labeled AABB in metres
# ---------------------------------------------------------------------------

class Box:
    def __init__(self, label, x0, x1, y0, y1, z0, z1, kind):
        self.label = label
        self.x0, self.x1, self.y0, self.y1, self.z0, self.z1 = x0, x1, y0, y1, z0, z1
        self.kind = kind  # solid | deck | stair | cover | wall

    @property
    def cx(self): return (self.x0 + self.x1) / 2
    @property
    def cy(self): return (self.y0 + self.y1) / 2


def gather(manifest, profile):
    boxes = []
    for lm in manifest.get("landmarks", []):
        for fp in lm.get("footprints", []):
            # The role's LEADING tag (before the first ';') is the authoritative
            # word — substring-sniffing the whole prose classified a pier as a deck
            # because it "carries the deck", and a canopy as a stair because it is
            # "not stair-served" (iteration 1 of Arena 02, both on the plan sheet).
            head = fp.get("role", "").lower().split(";")[0]
            label = fp.get("label", "").lower()
            if "stair" in head or "stair" in label:
                kind = "stair"
            elif "deck" in head or "slab" in head:
                kind = "deck"
            else:
                kind = "solid"
            boxes.append(Box(fp.get("label", lm["name"]),
                             fp["x"][0], fp["x"][1], fp["y"][0], fp["y"][1],
                             fp["z"][0], fp["z"][1], kind))
    dims = profile.get("cover_dims_m", {})
    for i, c in enumerate(manifest.get("cover", [])):
        d = dims.get(c.get("height_class", "chest"), {"x": 2, "y": 2, "z": 1.2})
        loc = c["location"]
        boxes.append(Box("cov%d" % i,
                         loc["x"] - d["x"] / 2, loc["x"] + d["x"] / 2,
                         loc["y"] - d["y"] / 2, loc["y"] + d["y"] / 2,
                         loc.get("z", 0), loc.get("z", 0) + d["z"], "cover"))
    b = manifest["bounds"]
    t = profile.get("perimeter_wall_thickness_m", 1.0)
    wz = b.get("z", 12)
    boxes += [Box("wall_s", 0, b["x"], -t, 0, 0, wz, "wall"),
              Box("wall_n", 0, b["x"], b["y"], b["y"] + t, 0, wz, "wall"),
              Box("wall_w", -t, 0, 0, b["y"], 0, wz, "wall"),
              Box("wall_e", b["x"], b["x"] + t, 0, b["y"], 0, wz, "wall")]
    return boxes


# ---------------------------------------------------------------------------
# SVG scaffolding
# ---------------------------------------------------------------------------

STYLE = """
  .paper{fill:#ffffff}
  .frame{fill:none;stroke:#000;stroke-width:2.5}
  .grid{stroke:#000;stroke-width:0.3;opacity:0.13}
  .poche{fill:#111111;stroke:none}
  .floor{fill:#e8e8e8;stroke:#000;stroke-width:0.6}
  .below{fill:none;stroke:#000;stroke-width:0.5;opacity:0.35}
  .over{fill:none;stroke:#000;stroke-width:1.1;stroke-dasharray:6 3.5}
  .cover{fill:#ffffff;stroke:#000;stroke-width:1.1}
  .tread{stroke:#000;stroke-width:0.7}
  .dim{stroke:#000;stroke-width:0.8}
  .dimtext{font:10px monospace;fill:#000}
  .label{font:bold 10.5px monospace;fill:#000;letter-spacing:0.5px}
  .small{font:9px monospace;fill:#000}
  .big{font:bold 16px monospace;fill:#000;letter-spacing:1px}
  .panel{font:bold 13px monospace;fill:#000;letter-spacing:1px}
  .datum{stroke:#000;stroke-width:0.8;stroke-dasharray:12 4 2 4}
  .iso{fill:#ffffff;stroke:#000;stroke-width:1.1;stroke-linejoin:round}
  .isotop{fill:#ededed;stroke:#000;stroke-width:1.1;stroke-linejoin:round}
  .isodark{fill:#d4d4d4;stroke:#000;stroke-width:1.1;stroke-linejoin:round}
  .isoplate{fill:#f7f7f7;stroke:#000;stroke-width:1.6;stroke-linejoin:round}
"""

LEVELS = [(0.0, "L0", "GROUND  ±0.00"), (4.0, "L1", "DECKS  +4.00"), (8.0, "L2", "CROSSING  +8.00")]


def svg_open(w, h, title):
    return ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" '
            'viewBox="0 0 %d %d">' % (w, h, w, h),
            "<style>%s</style>" % STYLE,
            '<rect class="paper" width="%d" height="%d"/>' % (w, h),
            '<title>%s</title>' % title]


def title_block(w, h, sheet_no, sheet_name, arena, scale_txt):
    x0, y0 = w - 352, h - 100
    rows = [("PROJECT", "BREACHPOINT — 4v4 ARENA FPS"),
            ("DRAWING", sheet_name),
            ("ARENA / SHEET", "%s   ·   %s" % (arena, sheet_no)),
            ("SCALE / DATE", "%s   ·   %s   ·   REV B" % (scale_txt, date.today().isoformat()))]
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
            '<text class="dimtext" x="%g" y="%g" transform="rotate(-90 %g %g)" text-anchor="middle">%s</text>'
            % (x - 6, (y0 + y1) / 2, x - 6, (y0 + y1) / 2, txt)]


class LabelBed:
    def __init__(self):
        self.placed = []

    def place(self, x, y, txt, size=11):
        w, h = len(txt) * size * 0.62, size + 3
        while any(abs(x - px) * 2 < w + pw and abs(y - py) * 2 < h + ph
                  for px, py, pw, ph in self.placed):
            y += h + 2
        self.placed.append((x, y, w, h))
        return y


# ---------------------------------------------------------------------------
# SHEET 1 — FLOOR PLANS, one panel per level (the architect's convention:
# CUT solids in black poche, this level's walkable plates in light grey,
# structure above dashed, structure below a faint context line)
# ---------------------------------------------------------------------------

def _level_panel(o, manifest, boxes, lvl_z, lvl_id, lvl_name, ox, oy, S):
    b = manifest["bounds"]
    W, H = b["x"], b["y"]
    X = lambda x: ox + x * S
    Y = lambda y: oy + (H - y) * S
    cut = lvl_z + 1.5

    o.append('<text class="panel" x="%g" y="%g">%s — %s</text>' % (X(0), oy - 14, lvl_id, lvl_name))

    # ground field of the panel
    o.append('<rect class="floor" x="%g" y="%g" width="%g" height="%g" opacity="%s"/>'
             % (X(0), Y(H), W * S, H * S, "0.55" if lvl_z == 0 else "0.0"))
    for i in range(0, int(W) + 1, 5):
        o.append('<line class="grid" x1="%g" y1="%g" x2="%g" y2="%g"/>' % (X(i), Y(0), X(i), Y(H)))
    for j in range(0, int(H) + 1, 5):
        o.append('<line class="grid" x1="%g" y1="%g" x2="%g" y2="%g"/>' % (X(0), Y(j), X(W), Y(j)))

    def rect(bx, cls):
        o.append('<rect class="%s" x="%g" y="%g" width="%g" height="%g"/>'
                 % (cls, X(bx.x0), Y(bx.y1), (bx.x1 - bx.x0) * S, (bx.y1 - bx.y0) * S))

    # 1. below-context first (thin), 2. walkable plates, 3. overhead dashed,
    # 4. cut poche LAST so the black always wins.
    for bx in boxes:
        if bx.kind == "wall":
            continue
        if bx.z1 <= lvl_z - 0.45 and bx.z0 < lvl_z - 0.45:
            rect(bx, "below")
    for bx in sorted((k for k in boxes if k.kind != "wall"), key=lambda k: k.z1):
        if abs(bx.z1 - lvl_z) <= 0.45 and bx.kind != "stair":
            rect(bx, "floor")
    for bx in boxes:
        if bx.kind != "wall" and bx.z0 >= cut:
            rect(bx, "over")
    for bx in boxes:
        if bx.kind == "wall" or bx.kind == "stair":
            continue
        if bx.z0 <= cut - 0.1 and bx.z1 > cut:
            rect(bx, "poche")
    # perimeter poche band — the enclosure IS the drawing's frame
    tw = 0.9 * S
    o.append('<rect class="poche" x="%g" y="%g" width="%g" height="%g"/>' % (X(0) - tw, Y(H) - tw, W * S + 2 * tw, tw))
    o.append('<rect class="poche" x="%g" y="%g" width="%g" height="%g"/>' % (X(0) - tw, Y(0), W * S + 2 * tw, tw))
    o.append('<rect class="poche" x="%g" y="%g" width="%g" height="%g"/>' % (X(0) - tw, Y(H), tw, H * S))
    o.append('<rect class="poche" x="%g" y="%g" width="%g" height="%g"/>' % (X(W), Y(H), tw, H * S))

    # stairs, drawn as stairs: treads + direction arrow (only where they serve this level)
    for bx in boxes:
        if bx.kind != "stair" or not (bx.z0 <= lvl_z <= bx.z1 + 0.5):
            continue
        horiz = (bx.x1 - bx.x0) >= (bx.y1 - bx.y0)
        n = 8
        for i in range(1, n):
            if horiz:
                xx = X(bx.x0 + (bx.x1 - bx.x0) * i / n)
                o.append('<line class="tread" x1="%g" y1="%g" x2="%g" y2="%g"/>' % (xx, Y(bx.y1), xx, Y(bx.y0)))
            else:
                yy = Y(bx.y0 + (bx.y1 - bx.y0) * i / n)
                o.append('<line class="tread" x1="%g" y1="%g" x2="%g" y2="%g"/>' % (X(bx.x0), yy, X(bx.x1), yy))
        o.append('<rect class="cover" fill="none" x="%g" y="%g" width="%g" height="%g"/>'
                 % (X(bx.x0), Y(bx.y1), (bx.x1 - bx.x0) * S, (bx.y1 - bx.y0) * S))
        cxp, cyp = X(bx.cx), Y(bx.cy)
        o.append('<text class="small" x="%g" y="%g" text-anchor="middle">UP</text>' % (cxp, cyp + 3))

    # markers, on their own level only
    if lvl_z == 0:
        for sp in manifest.get("spawn_points", []):
            x, y = X(sp["location"]["x"]), Y(sp["location"]["y"])
            a = math.radians(sp.get("facing", 0))
            o.append('<circle class="cover" cx="%g" cy="%g" r="7.5"/>' % (x, y))
            o.append('<line class="dim" x1="%g" y1="%g" x2="%g" y2="%g"/>'
                     % (x, y, x + 12 * math.cos(a), y - 12 * math.sin(a)))
            o.append('<text class="small" text-anchor="middle" x="%g" y="%g">%s</text>'
                     % (x, y + 3, sp["id"].replace("SP", "")))
    rn = manifest.get("rocket_node")
    if rn and abs(rn["z"] - lvl_z) <= 0.5:
        x, y = X(rn["x"]), Y(rn["y"])
        pts = " ".join("%g,%g" % (x + 10 * math.cos(math.radians(90 + i * 144)),
                                  y - 10 * math.sin(math.radians(90 + i * 144))) for i in range(5))
        o.append('<polygon class="cover" points="%s"/>' % pts)
        o.append('<text class="small" x="%g" y="%g">ROCKET</text>' % (x + 13, y + 3))
    for gp in manifest.get("grapple_points", []):
        if abs(gp["location"]["z"] - lvl_z) > 0.5:
            continue
        x, y = X(gp["location"]["x"]), Y(gp["location"]["y"])
        o.append('<circle class="dim" fill="none" cx="%g" cy="%g" r="5"/>' % (x, y))
        o.append('<line class="dim" x1="%g" y1="%g" x2="%g" y2="%g"/>' % (x - 7, y, x + 7, y))
        o.append('<line class="dim" x1="%g" y1="%g" x2="%g" y2="%g"/>' % (x, y - 7, x, y + 7))
        o.append('<text class="small" x="%g" y="%g">%s</text>' % (x + 8, y - 7, gp["id"]))

    # landmark labels whose pieces live at this level
    bed = LabelBed()
    for lm in manifest.get("landmarks", []):
        fps = [f for f in lm.get("footprints", [])
               if f["z"][0] < cut and f["z"][1] > lvl_z - 0.5]
        if not fps:
            continue
        cx = sum((f["x"][0] + f["x"][1]) / 2 for f in fps) / len(fps)
        top = max(f["y"][1] for f in fps)
        yy = bed.place(X(cx), max(Y(top) - 8, oy + 16), lm["name"].upper())
        o.append('<text class="label" text-anchor="middle" x="%g" y="%g">%s</text>'
                 % (X(cx), yy, lm["name"].upper()))

    o += dim_h(X(0), X(W), Y(H) - tw - 12, "%.1f m" % W)
    o += dim_v(Y(H), Y(0), X(0) - tw - 14, "%.1f m" % H)
    return X, Y


def sheet_plan(manifest, boxes, out_dir):
    b = manifest["bounds"]
    S = 13.5
    pw = b["x"] * S
    gap, M = 120, 100
    w = int(3 * pw + 2 * gap + 2 * M)
    h = int(b["y"] * S + 320)
    o = svg_open(w, h, "FLOOR PLANS")
    o.append('<rect class="frame" x="10" y="10" width="%d" height="%d"/>' % (w - 20, h - 20))
    o.append('<text class="big" x="%d" y="46">FLOOR PLANS — ONE PER LEVEL · CUT 1.5 m ABOVE EACH · '
             'BLACK = CUT STRUCTURE · GREY = WALKABLE · DASHED = OVERHEAD</text>' % M)
    for i, (lz, lid, lname) in enumerate(LEVELS):
        _level_panel(o, manifest, boxes, lz, lid, lname, M + i * (pw + gap), 100, S)

    # legend + north arrow + scale bar, bottom-left
    ly = h - 150
    o.append('<rect class="frame" x="%d" y="%d" width="560" height="118"/>' % (M, ly))
    o.append('<text class="label" x="%d" y="%d">LEGEND</text>' % (M + 10, ly + 18))
    items = [
        ('<rect class="poche" x="%d" y="%d" width="16" height="10"/>', "cut structure (poche)"),
        ('<rect class="floor" x="%d" y="%d" width="16" height="10"/>', "walkable plate at level"),
        ('<rect class="over" x="%d" y="%d" width="16" height="10"/>', "structure overhead (dashed)"),
        ('<rect class="below" x="%d" y="%d" width="16" height="10"/>', "structure below (faint)"),
    ]
    for i, (shape, txt) in enumerate(items):
        yy = ly + 34 + i * 20
        o.append(shape % (M + 12, yy - 9))
        o.append('<text class="small" x="%d" y="%d">%s</text>' % (M + 36, yy, txt))
    sym = [("spawn + facing", "circle"), ("rocket node", "star"), ("grapple anchor", "cross"), ("stair, treads UP", "stair")]
    for i, (txt, kind) in enumerate(sym):
        yy = ly + 34 + i * 20
        sx = M + 300
        if kind == "circle":
            o.append('<circle class="cover" cx="%d" cy="%d" r="6"/>' % (sx + 8, yy - 4))
            o.append('<line class="dim" x1="%d" y1="%d" x2="%d" y2="%d"/>' % (sx + 8, yy - 4, sx + 18, yy - 4))
        elif kind == "star":
            o.append('<polygon class="cover" points="%s"/>' % " ".join(
                "%g,%g" % (sx + 8 + 7 * math.cos(math.radians(90 + k * 144)),
                           yy - 4 - 7 * math.sin(math.radians(90 + k * 144))) for k in range(5)))
        elif kind == "cross":
            o.append('<circle class="dim" fill="none" cx="%d" cy="%d" r="5"/>' % (sx + 8, yy - 4))
            o.append('<line class="dim" x1="%d" y1="%d" x2="%d" y2="%d"/>' % (sx + 1, yy - 4, sx + 15, yy - 4))
            o.append('<line class="dim" x1="%d" y1="%d" x2="%d" y2="%d"/>' % (sx + 8, yy - 11, sx + 8, yy + 3))
        else:
            for k in range(4):
                o.append('<line class="tread" x1="%d" y1="%d" x2="%d" y2="%d"/>' % (sx + k * 5, yy - 10, sx + k * 5, yy + 1))
        o.append('<text class="small" x="%d" y="%d">%s</text>' % (sx + 28, yy, txt))
    nx = w - 80
    o.append('<line class="dim" x1="%d" y1="120" x2="%d" y2="84"/>' % (nx, nx))
    o.append('<polygon points="%d,%d %d,%d %d,%d"/>' % (nx - 6, 96, nx + 6, 96, nx, 80))
    o.append('<text class="label" x="%d" y="136" text-anchor="middle">N</text>' % nx)
    sb, sy = M + 620, h - 60
    o.append('<line class="dim" x1="%g" y1="%d" x2="%g" y2="%d"/>' % (sb, sy, sb + 10 * S, sy))
    for k in (0, 5, 10):
        o.append('<line class="dim" x1="%g" y1="%d" x2="%g" y2="%d"/>' % (sb + k * S, sy - 5, sb + k * S, sy + 5))
        o.append('<text class="small" x="%g" y="%d" text-anchor="middle">%d</text>' % (sb + k * S, sy + 18, k))
    o.append('<text class="small" x="%g" y="%d">m</text>' % (sb + 10 * S + 10, sy + 18))

    o += title_block(w, h, "S1", "FLOOR PLANS L0 / L1 / L2", manifest["arena_id"], "%.1f px/m" % S)
    o.append("</svg>")
    (out_dir / "S1_plan.svg").write_text("\n".join(o), encoding="utf-8")


# ---------------------------------------------------------------------------
# SHEET 2 — ELEVATIONS + SECTION (poche cut, heavy ground line, clean datums)
# ---------------------------------------------------------------------------

def _elev_panel(o, boxes, manifest, axis, y_top, S, M, label, section_at=None):
    b = manifest["bounds"]
    span = b["x"] if axis == "x" else b["y"]
    zmax = 12
    X = lambda v: M + v * S
    Z = lambda z: y_top + (zmax - z) * S
    o.append('<text class="panel" x="%d" y="%d">%s</text>' % (M, y_top - 12, label))
    lbl_x = X(span) + 0.9 * S + 20
    for lvl, name in ((4, "+4.00  L1"), (8, "+8.00  L2")):
        o.append('<line class="datum" x1="%g" y1="%g" x2="%g" y2="%g"/>' % (X(0), Z(lvl), X(span), Z(lvl)))
        o.append('<text class="small" x="%g" y="%g">%s</text>' % (lbl_x, Z(lvl) + 3, name))
    o.append('<text class="small" x="%g" y="%g">±0.00  GROUND</text>' % (lbl_x, Z(0) + 3))
    ordered = sorted(boxes, key=lambda k: (k.y0 if axis == "x" else k.x0), reverse=True)
    for bx in ordered:
        if bx.kind == "wall":
            continue
        u0, u1 = (bx.x0, bx.x1) if axis == "x" else (bx.y0, bx.y1)
        px, pw = X(u0), (u1 - u0) * S
        py, ph = Z(bx.z1), (bx.z1 - bx.z0) * S
        cut = section_at is not None and axis == "x" and bx.y0 <= section_at <= bx.y1
        # looking NORTH: the section shows the cut and what lies BEYOND it —
        # anything wholly on the viewer's side (south, y1 < cut) is out of frame.
        # (Iteration: rev B drew the front and hid the beyond, exactly backwards.)
        if section_at is not None and not cut and axis == "x" and bx.y1 < section_at:
            continue
        if cut and bx.kind not in ("deck",):
            o.append('<rect class="poche" x="%g" y="%g" width="%g" height="%g"/>' % (px, py, pw, ph))
        elif cut:
            o.append('<rect class="isodark" x="%g" y="%g" width="%g" height="%g"/>' % (px, py, pw, ph))
        else:
            o.append('<rect class="cover" x="%g" y="%g" width="%g" height="%g"/>' % (px, py, pw, ph))
    # perimeter wall ends + heavy ground line
    o.append('<rect class="poche" x="%g" y="%g" width="%g" height="%g"/>' % (X(0) - 0.9 * S, Z(zmax), 0.9 * S, zmax * S))
    o.append('<rect class="poche" x="%g" y="%g" width="%g" height="%g"/>' % (X(span), Z(zmax), 0.9 * S, zmax * S))
    o.append('<line x1="%g" y1="%g" x2="%g" y2="%g" stroke="#000" stroke-width="3.5"/>'
             % (X(0) - 0.9 * S - 14, Z(0), X(span) + 0.9 * S + 14, Z(0)))
    o += dim_v(Z(8), Z(0), X(0) - 0.9 * S - 20, "8.00 m")
    o += dim_h(X(0), X(span), Z(0) + 26, "%.1f m" % span)


def sheet_elevations(manifest, boxes, out_dir):
    b = manifest["bounds"]
    S, M = 17, 130
    span = max(b["x"], b["y"])
    panel_h = 13 * 17 + 78
    w, h = int(span * S + 2 * M + 160), int(3 * panel_h + 210)
    o = svg_open(w, h, "ELEVATIONS")
    o.append('<rect class="frame" x="10" y="10" width="%d" height="%d"/>' % (w - 20, h - 20))
    o.append('<text class="big" x="%d" y="48">ELEVATIONS + SECTION — BLACK = CUT / END WALLS · '
             'WHITE = BEYOND</text>' % M)
    _elev_panel(o, boxes, manifest, "x", 110, S, M, "SOUTH ELEVATION — LOOKING NORTH")
    _elev_panel(o, boxes, manifest, "y", 110 + panel_h, S, M, "WEST ELEVATION — LOOKING EAST")
    _elev_panel(o, boxes, manifest, "x", 110 + 2 * panel_h, S, M,
                "SECTION A–A — CUT AT y = %.0f m, LOOKING NORTH" % (b["y"] / 2), section_at=b["y"] / 2)
    o += title_block(w, h, "S2", "ELEVATIONS + SECTION A-A", manifest["arena_id"], "%d px/m" % S)
    o.append("</svg>")
    (out_dir / "S2_elevations.svg").write_text("\n".join(o), encoding="utf-8")


# ---------------------------------------------------------------------------
# SHEET 3 — 3/4 AXONOMETRIC on a ground plate with a cut-down parapet
# ---------------------------------------------------------------------------

def sheet_axon(manifest, boxes, out_dir):
    """Iso 30, viewer NE (verified projection: z up-screen, x+y toward viewer;
    east+north+top faces). v2: the arena sits ON a plate with the 5 m grid, the
    perimeter wall is drawn CUT DOWN to 2 m (the architect's trick: enclosure
    without hiding the interior), and faces get three tones so massing reads."""
    b = manifest["bounds"]
    S = 16
    cos30 = math.cos(math.radians(30))

    def P(x, y, z):
        return ((x - y) * cos30 * S, (x + y) * 0.5 * S - z * S)

    every = list(boxes) + [Box("plate", -1.2, b["x"] + 1.2, -1.2, b["y"] + 1.2, -0.7, 0, "plate")]
    pts = [P(x, y, z) for bx in every for x in (bx.x0, bx.x1)
           for y in (bx.y0, bx.y1) for z in (bx.z0, bx.z1)]
    minx, maxx = min(p[0] for p in pts), max(p[0] for p in pts)
    miny, maxy = min(p[1] for p in pts), max(p[1] for p in pts)
    M = 110
    w, h = int(maxx - minx + 2 * M), int(maxy - miny + 2 * M + 80)

    def Q(x, y, z):
        px, py = P(x, y, z)
        return (px - minx + M, py - miny + M + 50)

    o = svg_open(w, h, "AXONOMETRIC")
    o.append('<rect class="frame" x="10" y="10" width="%d" height="%d"/>' % (w - 20, h - 20))
    o.append('<text class="big" x="%d" y="52">3/4 AXONOMETRIC — ISO 30° FROM THE NORTH-EAST · '
             'PERIMETER WALL CUT TO 2 m</text>' % M)

    def face(ps, cls):
        return '<polygon class="%s" points="%s"/>' % (cls, " ".join("%g,%g" % p for p in ps))

    def draw_box(bx, top_cls="isotop"):
        o.append(face([Q(bx.x1, bx.y0, bx.z0), Q(bx.x1, bx.y1, bx.z0),
                       Q(bx.x1, bx.y1, bx.z1), Q(bx.x1, bx.y0, bx.z1)], "isodark"))
        o.append(face([Q(bx.x0, bx.y1, bx.z0), Q(bx.x1, bx.y1, bx.z0),
                       Q(bx.x1, bx.y1, bx.z1), Q(bx.x0, bx.y1, bx.z1)], "iso"))
        o.append(face([Q(bx.x0, bx.y0, bx.z1), Q(bx.x1, bx.y0, bx.z1),
                       Q(bx.x1, bx.y1, bx.z1), Q(bx.x0, bx.y1, bx.z1)], top_cls))

    # the plate, then its grid, then walls cut to 2 m, then the massing
    draw_box(every[-1], top_cls="isoplate")
    for i in range(0, int(b["x"]) + 1, 5):
        o.append('<polyline fill="none" class="grid" points="%g,%g %g,%g"/>'
                 % (*Q(i, 0, 0), *Q(i, b["y"], 0)))
    for j in range(0, int(b["y"]) + 1, 5):
        o.append('<polyline fill="none" class="grid" points="%g,%g %g,%g"/>'
                 % (*Q(0, j, 0), *Q(b["x"], j, 0)))
    for bx in sorted((k for k in boxes if k.kind == "wall"), key=lambda k: (k.x0 + k.y0)):
        cutw = Box(bx.label, bx.x0, bx.x1, bx.y0, bx.y1, 0, 2.0, "wall")
        draw_box(cutw, top_cls="isodark")
    for bx in sorted((k for k in boxes if k.kind != "wall"), key=lambda k: (k.x0 + k.y0, k.z0)):
        draw_box(bx)

    bed = LabelBed()
    for lm in manifest.get("landmarks", []):
        fps = lm.get("footprints", [])
        if not fps:
            continue
        top = max(fps, key=lambda f: f["z"][1])
        x, y = (top["x"][0] + top["x"][1]) / 2, (top["y"][0] + top["y"][1]) / 2
        px, py = Q(x, y, top["z"][1])
        ly = bed.place(px, py - 40, lm["name"].upper(), 10)
        o.append('<circle cx="%g" cy="%g" r="2.2"/>' % (px, py))
        o.append('<line class="dim" x1="%g" y1="%g" x2="%g" y2="%g"/>' % (px, py, px, ly + 4))
        o.append('<text class="label" x="%g" y="%g" text-anchor="middle">%s</text>'
                 % (px, ly, lm["name"].upper()))
    rn = manifest.get("rocket_node")
    if rn:
        px, py = Q(rn["x"], rn["y"], rn["z"])
        o.append('<polygon class="cover" points="%s"/>' % " ".join(
            "%g,%g" % (px + 9 * math.cos(math.radians(90 + k * 144)),
                       py - 9 * math.sin(math.radians(90 + k * 144))) for k in range(5)))
        o.append('<text class="small" x="%g" y="%g">ROCKET</text>' % (px + 12, py + 3))
    o += title_block(w, h, "S3", "3/4 AXONOMETRIC", manifest["arena_id"], "iso 30 deg")
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
        # chromium's headless viewport runs a few px short of --window-size;
        # over-provision and let the white paper fill the slack (clipped title
        # blocks on iteration 2 are how this line got here).
        win = "%d,%d" % (int(m.group(1)) + 24, int(m.group(2)) + 70) if m else "2400,1600"
        subprocess.run([chromium, "--headless", "--disable-gpu", "--no-sandbox",
                        "--force-device-scale-factor=2", "--default-background-color=FFFFFFFF",
                        "--screenshot=%s" % png, "--hide-scrollbars",
                        "--window-size=%s" % win, svg.as_uri()],
                       check=True, capture_output=True, timeout=120)
        print("  rasterized %s" % png.name)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--manifest", default=str(GAME / "Content" / "Data" / "arena_manifest.json"))
    ap.add_argument("--profile", default=str(HERE / "blockout_profile.json"))
    ap.add_argument("--out", default=None)
    ap.add_argument("--png", action="store_true")
    args = ap.parse_args()

    manifest = json.loads(Path(args.manifest).read_text(encoding="utf-8"))
    profile = json.loads(Path(args.profile).read_text(encoding="utf-8"))
    out_dir = Path(args.out) if args.out else \
        GAME / "docs" / "design" / "blueprints" / manifest["arena_id"]
    out_dir.mkdir(parents=True, exist_ok=True)

    boxes = gather(manifest, profile)
    sheet_plan(manifest, boxes, out_dir)
    sheet_elevations(manifest, boxes, out_dir)
    sheet_axon(manifest, boxes, out_dir)
    print("wrote 3 sheets (%d boxes) into %s" % (len(boxes), out_dir))
    if args.png:
        rasterize(out_dir)


if __name__ == "__main__":
    main()
