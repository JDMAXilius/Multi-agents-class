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
            role = (fp.get("role", "") + " " + fp.get("label", "")).lower()
            kind = "stair" if "stair" in role else ("deck" if "deck" in role or "slab" in role else "solid")
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
  .frame{fill:none;stroke:#000;stroke-width:2}
  .grid{stroke:#000;stroke-width:0.25;opacity:0.18}
  .grid5{stroke:#000;stroke-width:0.5;opacity:0.30}
  .solid{fill:#ffffff;stroke:#000;stroke-width:1.8}
  .wall{fill:#ffffff;stroke:#000;stroke-width:2.2}
  .deck{fill:none;stroke:#000;stroke-width:1.2;stroke-dasharray:7 4}
  .stair{fill:none;stroke:#000;stroke-width:1.0;stroke-dasharray:2.5 2.5}
  .cover{fill:#ffffff;stroke:#000;stroke-width:0.9}
  .hatch{stroke:#000;stroke-width:0.45;opacity:0.65}
  .dim{stroke:#000;stroke-width:0.7}
  .dimtext{font:10px monospace;fill:#000}
  .label{font:11px monospace;fill:#000}
  .small{font:9px monospace;fill:#000}
  .big{font:bold 15px monospace;fill:#000;letter-spacing:1px}
  .datum{stroke:#000;stroke-width:0.7;stroke-dasharray:12 4 2 4}
  .iso{fill:#ffffff;stroke:#000;stroke-width:1.1;stroke-linejoin:round}
  .isotop{fill:#f2f2f2;stroke:#000;stroke-width:1.1;stroke-linejoin:round}
"""

def svg_open(w, h, title):
    return ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" '
            'viewBox="0 0 %d %d">' % (w, h, w, h),
            "<style>%s</style>" % STYLE,
            '<rect class="paper" width="%d" height="%d"/>' % (w, h),
            '<title>%s</title>' % title]


def title_block(w, h, sheet_no, sheet_name, arena, scale_txt):
    x0, y0 = w - 342, h - 92
    rows = [("PROJECT", "BREACHPOINT — 4v4 ARENA FPS"),
            ("DRAWING", sheet_name),
            ("ARENA / SHEET", "%s   ·   %s" % (arena, sheet_no)),
            ("SCALE / DATE", "%s   ·   %s   ·   REV A" % (scale_txt, date.today().isoformat()))]
    out = ['<g>', '<rect class="frame" x="%d" y="%d" width="330" height="80"/>' % (x0, y0)]
    for i, (k, v) in enumerate(rows):
        yy = y0 + 17 + i * 19
        out.append('<text class="small" x="%d" y="%d">%s</text>' % (x0 + 8, yy, k))
        out.append('<text class="dimtext" x="%d" y="%d">%s</text>' % (x0 + 106, yy, v))
        if i:
            out.append('<line class="dim" x1="%d" y1="%d" x2="%d" y2="%d"/>'
                       % (x0, y0 + 3 + i * 19, x0 + 330, y0 + 3 + i * 19))
    out.append('</g>')
    return out


def dim_h(x0, x1, y, txt, off=0):
    """Horizontal dimension string: extension ticks + measurement text."""
    return ['<line class="dim" x1="%g" y1="%g" x2="%g" y2="%g"/>' % (x0, y, x1, y),
            '<line class="dim" x1="%g" y1="%g" x2="%g" y2="%g"/>' % (x0, y - 4, x0, y + 4),
            '<line class="dim" x1="%g" y1="%g" x2="%g" y2="%g"/>' % (x1, y - 4, x1, y + 4),
            '<text class="dimtext" text-anchor="middle" x="%g" y="%g">%s</text>'
            % ((x0 + x1) / 2, y - 5 + off, txt)]


def dim_v(y0, y1, x, txt):
    return ['<line class="dim" x1="%g" y1="%g" x2="%g" y2="%g"/>' % (x, y0, x, y1),
            '<line class="dim" x1="%g" y1="%g" x2="%g" y2="%g"/>' % (x - 4, y0, x + 4, y0),
            '<line class="dim" x1="%g" y1="%g" x2="%g" y2="%g"/>' % (x - 4, y1, x + 4, y1),
            '<text class="dimtext" x="%g" y="%g" transform="rotate(-90 %g %g)" text-anchor="middle">%s</text>'
            % (x - 6, (y0 + y1) / 2, x - 6, (y0 + y1) / 2, txt)]


def hatch_rect(px, py, pw, ph, step=7):
    """45-degree section hatching clipped to a rect."""
    out = []
    d = -(ph)
    while d < pw:
        x0, y0 = max(px, px + d), py + max(0, -d)
        x1, y1 = min(px + pw, px + d + ph), py + ph - max(0, d + ph - pw)
        if x1 > x0:
            out.append('<line class="hatch" x1="%g" y1="%g" x2="%g" y2="%g"/>' % (x0, y1, x1, y0))
        d += step
    return out


# ---------------------------------------------------------------------------
# SHEET 1 — PLAN
# ---------------------------------------------------------------------------

CUT_Z = 1.5   # architectural cut plane: below = solid ink, above = dashed overhead


class LabelBed:
    """Nudges labels down until they stop overprinting each other — the fix for
    iteration 1, where THE CORE and MEZZANINE CATWALKS shared a centroid."""

    def __init__(self):
        self.placed = []

    def place(self, x, y, txt, size=11):
        w, h = len(txt) * size * 0.62, size + 3
        while any(abs(x - px) * 2 < w + pw and abs(y - py) * 2 < h + ph
                  for px, py, pw, ph in self.placed):
            y += h + 2
        self.placed.append((x, y, w, h))
        return y

def sheet_plan(manifest, boxes, out_dir):
    b = manifest["bounds"]
    S, M = 26, 90                     # px per metre, margin
    w, h = int(b["x"] * S + 2 * M + 120), int(b["y"] * S + 2 * M + 60)
    X = lambda x: M + x * S
    Y = lambda y: h - M - y * S       # north up
    o = svg_open(w, h, "PLAN")
    o.append('<rect class="frame" x="8" y="8" width="%d" height="%d"/>' % (w - 16, h - 16))

    for i in range(0, int(b["x"]) + 1):
        o.append('<line class="%s" x1="%g" y1="%g" x2="%g" y2="%g"/>'
                 % ("grid5" if i % 5 == 0 else "grid", X(i), Y(0), X(i), Y(b["y"])))
    for j in range(0, int(b["y"]) + 1):
        o.append('<line class="%s" x1="%g" y1="%g" x2="%g" y2="%g"/>'
                 % ("grid5" if j % 5 == 0 else "grid", X(0), Y(j), X(b["x"]), Y(j)))

    order = {"wall": 0, "solid": 1, "stair": 2, "cover": 3, "deck": 4}
    for bx in sorted(boxes, key=lambda k: order[k.kind]):
        cls = bx.kind if bx.kind in ("wall", "deck", "stair", "cover") else \
              ("solid" if bx.z0 < CUT_Z else "deck")
        px, py = X(bx.x0), Y(bx.y1)
        pw, ph = (bx.x1 - bx.x0) * S, (bx.y1 - bx.y0) * S
        o.append('<rect class="%s" x="%g" y="%g" width="%g" height="%g"/>' % (cls, px, py, pw, ph))
        if cls == "solid" and bx.kind != "cover":
            o += hatch_rect(px, py, pw, ph)   # cut solids get section hatch, the convention
        if bx.kind == "stair":                # stair arrow, UP toward the higher abutment
            o.append('<text class="small" x="%g" y="%g" text-anchor="middle">UP</text>'
                     % (X(bx.cx), Y(bx.cy) + 3))

    bed = LabelBed()
    for lm in manifest.get("landmarks", []):
        fps = lm.get("footprints", [])
        if not fps:
            continue
        cx = sum((f["x"][0] + f["x"][1]) / 2 for f in fps) / len(fps)
        # ABOVE the landmark's northmost edge, not inside the hatching
        top = max(f["y"][1] for f in fps)
        yy = bed.place(X(cx), Y(top) - 10, lm["name"].upper())
        o.append('<text class="label" text-anchor="middle" x="%g" y="%g">%s</text>'
                 % (X(cx), yy, lm["name"].upper()))

    for sp in manifest.get("spawn_points", []):
        x, y = X(sp["location"]["x"]), Y(sp["location"]["y"])
        a = math.radians(sp.get("facing", 0))
        o.append('<circle class="cover" cx="%g" cy="%g" r="7"/>' % (x, y))
        o.append('<line class="dim" x1="%g" y1="%g" x2="%g" y2="%g"/>'
                 % (x, y, x + 11 * math.cos(a), y - 11 * math.sin(a)))
        o.append('<text class="small" text-anchor="middle" x="%g" y="%g">%s</text>'
                 % (x, y + 3, sp["id"].replace("SP", "")))

    rn = manifest.get("rocket_node")
    if rn:
        x, y = X(rn["x"]), Y(rn["y"])
        pts = " ".join("%g,%g" % (x + 9 * math.cos(math.radians(90 + i * 144)),
                                  y - 9 * math.sin(math.radians(90 + i * 144))) for i in range(5))
        o.append('<polygon class="solid" points="%s"/>' % pts)
        o.append('<text class="small" x="%g" y="%g">ROCKET z=%g</text>' % (x + 12, y + 3, rn["z"]))

    for gp in manifest.get("grapple_points", []):
        x, y = X(gp["location"]["x"]), Y(gp["location"]["y"])
        o.append('<circle class="dim" fill="none" cx="%g" cy="%g" r="5"/>' % (x, y))
        o.append('<line class="dim" x1="%g" y1="%g" x2="%g" y2="%g"/>' % (x - 5, y, x + 5, y))
        o.append('<line class="dim" x1="%g" y1="%g" x2="%g" y2="%g"/>' % (x, y - 5, x, y + 5))
        o.append('<text class="small" x="%g" y="%g">%s z=%g</text>'
                 % (x + 7, y - 6, gp["id"], gp["location"]["z"]))

    o += dim_h(X(0), X(b["x"]), Y(b["y"]) - 26, "%.1f m" % b["x"])
    o += dim_v(Y(b["y"]), Y(0), X(0) - 26, "%.1f m" % b["y"])
    # section mark A-A through the fight corridor (the arena's mid line)
    ya = Y(b["y"] / 2)
    o.append('<line class="datum" x1="%g" y1="%g" x2="%g" y2="%g"/>' % (X(0) - 12, ya, X(b["x"]) + 12, ya))
    for tx in (X(0) - 12, X(b["x"]) + 12):
        o.append('<text class="big" x="%g" y="%g" text-anchor="middle">A</text>' % (tx, ya - 8))
    # north arrow + scale bar
    nx, ny = w - 60, 74
    o.append('<line class="dim" x1="%d" y1="%d" x2="%d" y2="%d"/>' % (nx, ny, nx, ny - 34))
    o.append('<polygon points="%d,%d %d,%d %d,%d"/>' % (nx - 5, ny - 24, nx + 5, ny - 24, nx, ny - 38))
    o.append('<text class="label" x="%d" y="%d" text-anchor="middle">N</text>' % (nx, ny + 14))
    sb = X(0)
    o.append('<line class="dim" x1="%g" y1="%d" x2="%g" y2="%d"/>' % (sb, h - 30, sb + 10 * S, h - 30))
    for k in range(0, 11, 5):
        o.append('<line class="dim" x1="%g" y1="%d" x2="%g" y2="%d"/>' % (sb + k * S, h - 35, sb + k * S, h - 25))
        o.append('<text class="small" x="%g" y="%d" text-anchor="middle">%d</text>' % (sb + k * S, h - 15, k))
    o.append('<text class="small" x="%g" y="%d">m</text>' % (sb + 10 * S + 8, h - 15))
    o.append('<text class="big" x="%d" y="34">PLAN — CUT AT +%.1f m · OVERHEAD DASHED</text>' % (M, CUT_Z))
    o += title_block(w, h, "S1", "PLAN (TOP VIEW)", manifest["arena_id"], "1:%d px/m %d" % (1, S))
    o.append("</svg>")
    (out_dir / "S1_plan.svg").write_text("\n".join(o), encoding="utf-8")
    return w, h


# ---------------------------------------------------------------------------
# SHEET 2 — ELEVATIONS + SECTION
# ---------------------------------------------------------------------------

def _elev_panel(o, boxes, manifest, axis, y_top, S, M, label, section_at=None):
    """axis 'x': south elevation (x right, z up). axis 'y': west elevation."""
    b = manifest["bounds"]
    span = b["x"] if axis == "x" else b["y"]
    zmax = 12
    X = lambda v: M + v * S
    Z = lambda z: y_top + (zmax - z) * S
    o.append('<text class="big" x="%d" y="%d">%s</text>' % (M, y_top - 12, label))
    for lvl, name in ((0, "LVL 0  GROUND ±0.00"), (4, "LVL 1  MID +4.00"), (8, "LVL 2  UPPER +8.00")):
        o.append('<line class="datum" x1="%g" y1="%g" x2="%g" y2="%g"/>' % (X(0), Z(lvl), X(span), Z(lvl)))
        o.append('<text class="small" x="%g" y="%g">%s</text>' % (X(span) + 8, Z(lvl) + 3, name))
    ordered = sorted(boxes, key=lambda k: (k.y0 if axis == "x" else k.x0), reverse=True)
    for bx in ordered:
        if bx.kind == "wall":
            continue
        u0, u1 = (bx.x0, bx.x1) if axis == "x" else (bx.y0, bx.y1)
        px, pw = X(u0), (u1 - u0) * S
        py, ph = Z(bx.z1), (bx.z1 - bx.z0) * S
        cut = section_at is not None and (
            (axis == "x" and bx.y0 <= section_at <= bx.y1))
        if section_at is not None and not cut and axis == "x" and bx.y0 > section_at:
            continue   # a section shows only the cut and what lies beyond it
        cls = "solid" if bx.kind in ("solid", "cover", "stair") else "deck"
        o.append('<rect class="%s" x="%g" y="%g" width="%g" height="%g"/>' % (cls, px, py, pw, ph))
        if cut and bx.kind != "deck":
            o += hatch_rect(px, py, pw, ph)
    o += dim_v(Z(8), Z(0), X(0) - 22, "8.00 m")
    o += dim_v(Z(12), Z(0), X(span) + 60, "12.00 m clear")
    o += dim_h(X(0), X(span), Z(0) + 24, "%.1f m" % span)


def sheet_elevations(manifest, boxes, out_dir):
    b = manifest["bounds"]
    S, M = 18, 110
    span = max(b["x"], b["y"])
    panel_h = 13 * S + 60
    w, h = int(span * S + 2 * M + 130), int(3 * panel_h + 150)
    o = svg_open(w, h, "ELEVATIONS")
    o.append('<rect class="frame" x="8" y="8" width="%d" height="%d"/>' % (w - 16, h - 16))
    _elev_panel(o, boxes, manifest, "x", 70, S, M, "SOUTH ELEVATION — LOOKING NORTH")
    _elev_panel(o, boxes, manifest, "y", 70 + panel_h, S, M, "WEST ELEVATION — LOOKING EAST")
    _elev_panel(o, boxes, manifest, "x", 70 + 2 * panel_h, S, M,
                "SECTION A–A — CUT AT y=%.0f m, LOOKING NORTH" % (b["y"] / 2), section_at=b["y"] / 2)
    o += title_block(w, h, "S2", "ELEVATIONS + SECTION A-A", manifest["arena_id"], "%d px/m" % S)
    o.append("</svg>")
    (out_dir / "S2_elevations.svg").write_text("\n".join(o), encoding="utf-8")


# ---------------------------------------------------------------------------
# SHEET 3 — 3/4 AXONOMETRIC
# ---------------------------------------------------------------------------

def sheet_axon(manifest, boxes, out_dir):
    """Iso 30-degree, VIEWER AT THE NORTH-EAST, screen y down:
        sx = (x - y) * cos30 * S
        sy = (x + y) * sin30 * S - z * S
    Larger x+y walks DOWN-screen (toward the viewer), larger z walks UP.
    Visible per box: the EAST (+x) face, the NORTH (+y) face, the TOP.
    Painter: ascending (x + y), then ascending z — far and low first.
    (Iteration 1 drew the south/west pair against a double-flipped Y and
    every box rendered inside-out; this projection is the fix, verified on
    a hand-checked unit cube before re-render.)"""
    S = 17
    cos30, sin30 = math.cos(math.radians(30)), 0.5

    def P(x, y, z):
        return ((x - y) * cos30 * S, (x + y) * sin30 * S - z * S)

    pts = [P(x, y, z) for bx in boxes for x in (bx.x0, bx.x1)
           for y in (bx.y0, bx.y1) for z in (bx.z0, bx.z1)]
    minx, maxx = min(p[0] for p in pts), max(p[0] for p in pts)
    miny, maxy = min(p[1] for p in pts), max(p[1] for p in pts)
    M = 100
    w, h = int(maxx - minx + 2 * M), int(maxy - miny + 2 * M + 60)

    def Q(x, y, z):
        px, py = P(x, y, z)
        return (px - minx + M, py - miny + M + 40)

    o = svg_open(w, h, "AXONOMETRIC")
    o.append('<rect class="frame" x="8" y="8" width="%d" height="%d"/>' % (w - 16, h - 16))
    o.append('<text class="big" x="%d" y="40">3/4 AXONOMETRIC — ISO 30°, VIEW FROM THE NORTH-EAST, UP IS +Z</text>' % 24)

    def face(ps, cls):
        return '<polygon class="%s" points="%s"/>' % (cls, " ".join("%g,%g" % p for p in ps))

    for bx in sorted((k for k in boxes if k.kind != "wall"),
                     key=lambda k: (k.x0 + k.y0, k.z0)):
        # east (+x) face, north (+y) face, top — the three the NE viewer sees
        o.append(face([Q(bx.x1, bx.y0, bx.z0), Q(bx.x1, bx.y1, bx.z0),
                       Q(bx.x1, bx.y1, bx.z1), Q(bx.x1, bx.y0, bx.z1)], "iso"))
        o.append(face([Q(bx.x0, bx.y1, bx.z0), Q(bx.x1, bx.y1, bx.z0),
                       Q(bx.x1, bx.y1, bx.z1), Q(bx.x0, bx.y1, bx.z1)], "iso"))
        o.append(face([Q(bx.x0, bx.y0, bx.z1), Q(bx.x1, bx.y0, bx.z1),
                       Q(bx.x1, bx.y1, bx.z1), Q(bx.x0, bx.y1, bx.z1)], "isotop"))

    bed = LabelBed()
    for lm in manifest.get("landmarks", []):
        fps = lm.get("footprints", [])
        if not fps:
            continue
        top = max(fps, key=lambda f: f["z"][1])
        x, y = (top["x"][0] + top["x"][1]) / 2, (top["y"][0] + top["y"][1]) / 2
        px, py = Q(x, y, top["z"][1])
        ly = bed.place(px, py - 34, lm["name"].upper(), 9)
        o.append('<line class="dim" x1="%g" y1="%g" x2="%g" y2="%g"/>' % (px, py, px, ly + 3))
        o.append('<text class="small" x="%g" y="%g" text-anchor="middle">%s</text>'
                 % (px, ly, lm["name"].upper()))
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
