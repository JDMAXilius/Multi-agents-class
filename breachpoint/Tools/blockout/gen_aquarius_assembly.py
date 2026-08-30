#!/usr/bin/env python3
"""BN30 — ASSEMBLY blueprints: the level drawn AS the modular kit.

    python3 Tools/blockout/gen_aquarius_assembly.py --png

Founder (30 Aug): "now make the blueprint with those modular assets."
These sheets draw Content/Data/aquarius_blockout_kit.json — every placed
INSTANCE individually, seams visible, so the drawing shows the level the way
it is actually built: the same fifteen assets repeated at kit scales.

  AK-101  LEVEL 1 ASSEMBLY — floors, walls, towers, piers, ramps as placed
          modules with seams; decks dashed overhead; BOM (bill of materials)
  AK-102  LEVEL 2 ASSEMBLY — deck plates as placed modules; cut structure;
          ramps arriving; BOM
  AK-301  3/4 ASSEMBLY — all 800+ instances drawn as individual blocks,
          the level as it will stand in the editor

Reuses the REV D chrome (borders, title block, grid bubbles, scale bar) and
the double-sided face law. Reads ONLY the kit JSON — if the manifest
regenerates, these sheets re-render to match; nothing is drawn by hand.
"""

from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from gen_aquarius_blueprint import (  # noqa: E402
    svg_open, sheet_chrome, dim_h, dim_v, scale_bar, north_arrow, rasterize)

GAME = HERE.parents[1]
KIT = GAME / "Content" / "Data" / "aquarius_blockout_kit.json"
OUTD = GAME / "docs" / "design" / "blueprints" / "breachpoint_aquarius"

ASSET_OF = {"Floor": "BLK_Floor_400", "Deck": "BLK_Floor_400",
            "Wall": "BLK_Wall_400", "Tower": "BLK_Wall_400",
            "Support": "BLK_Pier_100", "Ramp": "BLK_Ramp_800"}

EXTRA_STYLE = """
  .mwall{fill:#1a1a1a;stroke:#ffffff;stroke-width:0.8}
  .mwallo{fill:none;stroke:#000;stroke-width:1.2}
  .mtower{fill:#3a3a3a;stroke:#ffffff;stroke-width:0.8}
  .mfloor{fill:#f1f1f1;stroke:#777;stroke-width:0.7}
  .mdeck{fill:#dfe4ea;stroke:#556;stroke-width:0.8}
  .mpier{fill:url(#h135);stroke:#000;stroke-width:0.9}
  .mramp{fill:#fafafa;stroke:#000;stroke-width:1.0}
  .msus{fill:url(#h45);stroke:#000;stroke-width:1.0}
"""


def fam(p):
    return p["variant"].split("_")[0]


def rect_m(p):
    x, y, _ = p["location_cm"]
    sx, sy, _ = p["scale"]
    return x / 100 - sx / 2, y / 100 - sy / 2, sx, sy


def load():
    kit = json.loads(KIT.read_text(encoding="utf-8"))
    xs = [rect_m(p) for p in kit["placements"]]
    Wm = max(r[0] + r[2] for r in xs)
    Hm = max(r[1] + r[3] for r in xs)
    return kit, Wm, Hm


def grid_cols_rows(Wm, Hm):
    nx = max(1, round(Wm / 6.5))
    cols = [(chr(ord("A") + i), i * Wm / nx) for i in range(nx + 1)]
    rows = [(str(i + 1), i * Hm / 5) for i in range(6)]
    return cols, rows


def bom_rows(placements):
    counts = {}
    for p in placements:
        key = (fam(p), p["variant"])
        counts[key] = counts.get(key, 0) + 1
    fam_tot = {}
    for (f, _), n in counts.items():
        fam_tot[f] = fam_tot.get(f, 0) + n
    top = sorted(counts.items(), key=lambda kv: -kv[1])[:14]
    return fam_tot, top, len(counts)


def draw_bom(o, x, y, title, placements):
    fam_tot, top, nvar = bom_rows(placements)
    o.append('<text class="panel" x="%d" y="%d">%s</text>' % (x, y, title))
    yy = y + 22
    o.append('<text class="label" x="%d" y="%d">ASSET (kit card)</text>' % (x, yy))
    o.append('<text class="label" x="%d" y="%d">QTY</text>' % (x + 300, yy))
    o.append('<line class="dim" x1="%d" y1="%d" x2="%d" y2="%d"/>'
             % (x, yy + 6, x + 350, yy + 6))
    for f in ("Floor", "Deck", "Wall", "Tower", "Support", "Ramp"):
        if f not in fam_tot:
            continue
        yy += 19
        o.append('<text class="roomd" x="%d" y="%d">%s  as  %s</text>'
                 % (x, yy, f, ASSET_OF[f]))
        o.append('<text class="roomd" x="%d" y="%d">%d</text>'
                 % (x + 300, yy, fam_tot[f]))
    yy += 30
    o.append('<text class="label" x="%d" y="%d">TOP SIZE VARIANTS (of %d)</text>'
             % (x, yy, nvar))
    o.append('<line class="dim" x1="%d" y1="%d" x2="%d" y2="%d"/>'
             % (x, yy + 6, x + 350, yy + 6))
    for (f, vid), n in top:
        yy += 17
        o.append('<text class="roomd" x="%d" y="%d">%s</text>' % (x, yy, vid))
        o.append('<text class="roomd" x="%d" y="%d">x%d</text>' % (x + 300, yy, n))
    return yy


def draw_ramp_plan(o, p, X, Y, dashed=False):
    x0, y0, w, h = rect_m(p)
    cls = "over" if dashed else "mramp"
    o.append('<rect class="%s" x="%g" y="%g" width="%g" height="%g"/>'
             % (cls, X(x0), Y(y0), w * (X(1) - X(0)), h * (X(1) - X(0))))
    if dashed:
        return
    yaw = p["rotation_deg"]["yaw"] % 360
    S = X(1) - X(0)
    cx, cy = X(x0 + w / 2), Y(y0 + h / 2)
    run = max(w, h) * 0.38 * S
    ang = {0: 0, 90: 90, 180: 180, 270: -90}.get(int(yaw), 0)
    rad = math.radians(ang)
    dx, dy = math.cos(rad) * run, math.sin(rad) * run
    o.append('<line class="dim" x1="%g" y1="%g" x2="%g" y2="%g"/>'
             % (cx - dx, cy - dy, cx + dx, cy + dy))
    o.append('<path transform="translate(%g,%g) rotate(%g)" '
             'd="M 0 0 L -8 -4 L -8 4 Z" fill="#000"/>' % (cx + dx, cy + dy, ang))
    n = max(3, int(max(w, h) / 0.9))
    for i in range(1, n):
        t = i / n
        if int(yaw) in (0, 180):
            xx = X(x0 + w * t)
            o.append('<line class="tread" x1="%g" y1="%g" x2="%g" y2="%g"/>'
                     % (xx, Y(y0), xx, Y(y0 + h)))
        else:
            yyp = Y(y0 + h * t)
            o.append('<line class="tread" x1="%g" y1="%g" x2="%g" y2="%g"/>'
                     % (X(x0), yyp, X(x0 + w), yyp))


def assembly_plan(kit, Wm, Hm, out_dir, level):
    S = 26.0                            # px per metre
    MXL, MYT = 160, 150
    pw, ph = Wm * S, Hm * S
    SIDE = 480
    w = int(MXL + pw + 90 + SIDE)
    h = int(MYT + ph + 250)
    X = lambda m: MXL + m * S
    Y = lambda m: MYT + m * S
    P = kit["placements"]
    by = lambda *fs: [p for p in P if fam(p) in fs]

    no, name = (("AK-101", "LEVEL 1 ASSEMBLY — MODULAR PLACEMENTS") if level == "L1"
                else ("AK-102", "LEVEL 2 ASSEMBLY — MODULAR PLACEMENTS"))
    o = svg_open(w, h, "AQUARIUS — %s" % name)
    o[2] = o[2].replace("</style>", EXTRA_STYLE + "</style>") if "</style>" in o[2] else o[2]
    # svg_open puts style in element 1; patch robustly:
    for i, el in enumerate(o):
        if el.startswith("<style>"):
            o[i] = el.replace("</style>", EXTRA_STYLE + "</style>")
            break
    o.append('<text class="big" x="%d" y="44">%s · EVERY PIECE ONE KIT INSTANCE, '
             'SEAMS TRUE</text>' % (MXL - 80, name))

    cols, rows = grid_cols_rows(Wm, Hm)
    for lab, xm in cols:
        o.append('<line class="gridl" x1="%g" y1="%g" x2="%g" y2="%g"/>'
                 % (X(xm), MYT - 14, X(xm), MYT + ph + 14))
        for yy in (MYT - 26, MYT + ph + 30):
            o.append('<circle class="bubble" cx="%g" cy="%g" r="12"/>' % (X(xm), yy))
            o.append('<text class="label" x="%g" y="%g" text-anchor="middle">%s</text>'
                     % (X(xm), yy + 4, lab))
    for lab, ym in rows:
        o.append('<line class="gridl" x1="%g" y1="%g" x2="%g" y2="%g"/>'
                 % (MXL - 14, Y(ym), X(Wm) + 14, Y(ym)))
        o.append('<circle class="bubble" cx="%g" cy="%g" r="12"/>' % (MXL - 30, Y(ym)))
        o.append('<text class="label" x="%g" y="%g" text-anchor="middle">%s</text>'
                 % (MXL - 30, Y(ym) + 4, lab))

    def rects(ps, cls):
        for p in ps:
            x0, y0, rw, rh = rect_m(p)
            o.append('<rect class="%s" x="%g" y="%g" width="%g" height="%g"/>'
                     % (cls, X(x0), Y(y0), rw * S, rh * S))

    if level == "L1":
        rects(by("Floor"), "mfloor")
        for p in by("Ramp"):
            if "suspect" in p:
                x0, y0, rw, rh = rect_m(p)
                o.append('<rect class="msus" x="%g" y="%g" width="%g" height="%g"/>'
                         % (X(x0), Y(y0), rw * S, rh * S))
            else:
                draw_ramp_plan(o, p, X, Y)
        rects(by("Support"), "mpier")
        rects(by("Wall"), "mwall")
        rects(by("Tower"), "mtower")
        for p in by("Deck"):                       # overhead, dashed
            x0, y0, rw, rh = rect_m(p)
            o.append('<rect class="over" x="%g" y="%g" width="%g" height="%g"/>'
                     % (X(x0), Y(y0), rw * S, rh * S))
        plc = by("Floor", "Wall", "Tower", "Support", "Ramp")
    else:
        rects(by("Floor"), "below")
        rects(by("Deck"), "mdeck")
        rects(by("Wall"), "mwall")
        rects(by("Tower"), "mtower")
        for p in by("Ramp"):
            draw_ramp_plan(o, p, X, Y, dashed=True)
        plc = by("Deck", "Wall", "Tower")

    o += dim_h(X(0), X(Wm), MYT - 64, "%.1f m OVERALL" % Wm)
    o += dim_v(Y(0), Y(Hm), MXL - 64, "%.1f m OVERALL" % Hm)
    north_arrow(o, X(Wm) + 40, MYT + 8)
    scale_bar(o, MXL, MYT + ph + 120, S)

    sx = int(MXL + pw + 70)
    yy = draw_bom(o, sx, MYT - 40, "BILL OF MATERIALS — %s"
                  % ("LEVEL 1" if level == "L1" else "LEVEL 2"), plc)
    yy += 34
    o.append('<text class="panel" x="%d" y="%d">MODULE KEY</text>' % (sx, yy))
    key = [("mwall", "BLK_Wall_400 (perimeter, 8.0 m)"),
           ("mtower", "BLK_Wall_400 (tower mass, 6.5 m)"),
           ("mpier", "BLK_Pier_100 (deck support)"),
           ("mfloor", "BLK_Floor_400 (floor plate)"),
           ("mdeck", "BLK_Floor_400 (deck plate, t 0.40)"),
           ("mramp", "BLK_Ramp_800 (arrow = UP)"),
           ("msus", "suspect capsule (channel b rules)"),
           ("over", "structure overhead (dashed)")]
    for i, (cls, txt) in enumerate(key):
        y2 = yy + 20 + i * 19
        o.append('<rect class="%s" x="%d" y="%d" width="26" height="11"/>' % (cls, sx, y2 - 9))
        o.append('<text class="small" x="%d" y="%d">%s</text>' % (sx + 36, y2, txt))
    y3 = yy + 20 + len(key) * 19 + 24
    o.append('<text class="small" x="%d" y="%d">White joints inside dark pieces are '
             'the MODULE SEAMS between</text>' % (sx, y3))
    o.append('<text class="small" x="%d" y="%d">maximal pieces on the pass grid '
             '(research: big simple shapes).</text>' % (sx, y3 + 16))
    o.append('<text class="small" x="%d" y="%d">Full parts list: '
             'Content/Data/aquarius_blockout_kit.json (deterministic).</text>'
             % (sx, y3 + 34))

    sheet_chrome(o, w, h, no, name, "%.0f px/m" % S)
    o.append("</svg>")
    fn = "AK101_assembly_L1" if level == "L1" else "AK102_assembly_L2"
    (out_dir / ("%s.svg" % fn)).write_text("\n".join(o), encoding="utf-8")


# ---------------------------------------------------------------------------
# AK-301 — every instance as its own block, high 3/4

def assembly_axon(kit, Wm, Hm, out_dir):
    S = 11.5
    # HZ was 0.42 - so an 8 m wall drew SHORTER than a 4 m piece is wide and
    # the level read as a flat curb model. Heights now draw near true.
    HX, HY, HZ = 0.78, 0.50, 0.62

    def P3(x, y, z):
        Xw, Yw = x, Hm - y
        return ((Xw - Yw) * HX * S, (Xw + Yw) * HY * S - z * HZ * S)

    corners_extent = [P3(0, 0, 0), P3(Wm, 0, 0), P3(0, Hm, 0), P3(Wm, Hm, 0),
                      P3(0, 0, 8), P3(Wm, 0, 8), P3(0, Hm, 8), P3(Wm, Hm, 8)]
    minx = min(p[0] for p in corners_extent)
    maxx = max(p[0] for p in corners_extent)
    miny = min(p[1] for p in corners_extent)
    maxy = max(p[1] for p in corners_extent)
    ox, oy = 430 - minx, 150 - miny   # left gutter holds the key
    w = int(maxx - minx + 670)
    h = int(maxy - miny + 330)

    def Q(x, y, z):
        px, py = P3(x, y, z)
        return (px + ox, py + oy)

    o = svg_open(w, h, "AQUARIUS — 3/4 ASSEMBLY")
    o.append('<text class="big" x="70" y="58">3/4 ASSEMBLY — EVERY KIT INSTANCE '
             'AS ITS OWN BLOCK</text>')
    o.append('<text class="small" x="70" y="82">%d placements of the 15-asset kit · '
             'maximal pieces on the 1.0 m pass grid · suspect capsules included</text>'
             % len(kit["placements"]))

    plate = [Q(-2, -2, 0), Q(Wm + 2, -2, 0), Q(Wm + 2, Hm + 2, 0), Q(-2, Hm + 2, 0)]
    o.append('<path class="isoplate" d="M %s Z"/>'
             % " L ".join("%.1f %.1f" % p for p in plate))

    def box_corners(p):
        cx, cy, cz = (v / 100 for v in p["location_cm"])
        sx, sy, sz = p["scale"]
        yaw = math.radians(p["rotation_deg"]["yaw"])
        pit = math.radians(p["rotation_deg"]["pitch"])
        cyw, syw = math.cos(yaw), math.sin(yaw)
        cp, sp = math.cos(pit), math.sin(pit)
        f = (cyw * cp, syw * cp, sp)             # local +X (nose up = +z)
        r = (-syw, cyw, 0.0)                     # local +Y
        u = (r[1] * f[2] - r[2] * f[1],
             r[2] * f[0] - r[0] * f[2],
             r[0] * f[1] - r[1] * f[0])          # r x f
        if u[2] < 0:
            u = (-u[0], -u[1], -u[2])
        out = {}
        for a in (-1, 1):
            for b in (-1, 1):
                for g in (-1, 1):
                    out[(a, b, g)] = (
                        cx + f[0] * sx / 2 * a + r[0] * sy / 2 * b + u[0] * sz / 2 * g,
                        cy + f[1] * sx / 2 * a + r[1] * sy / 2 * b + u[1] * sz / 2 * g,
                        cz + f[2] * sx / 2 * a + r[2] * sy / 2 * b + u[2] * sz / 2 * g)
        return out

    FACES = [((-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1), "isotop"),
             ((-1, -1, -1), (1, -1, -1), (1, 1, -1), (-1, 1, -1), "isotop"),
             ((1, -1, -1), (1, 1, -1), (1, 1, 1), (1, -1, 1), "isoeast"),
             ((-1, -1, -1), (-1, 1, -1), (-1, 1, 1), (-1, -1, 1), "isoeast"),
             ((-1, -1, -1), (1, -1, -1), (1, -1, 1), (-1, -1, 1), "isonorth"),
             ((-1, 1, -1), (1, 1, -1), (1, 1, 1), (-1, 1, 1), "isonorth")]

    def depth_of(p):
        cx, cy, _ = (v / 100 for v in p["location_cm"])
        return cx + (Hm - cy)

    # PAINTER, fixed (founder: "fix the 3/4"). Two rules the old version
    # broke: (a) EVERY piece is a real box with its true thickness - floors
    # and decks were flat ghost sheets before; (b) sort by the piece's FAR
    # corner, not its centroid. A 20 m floor plate has a centroid mid-map,
    # so centroid-sorting painted it over every wall behind mid-map (the
    # grey wash). Its far corner sorts it first, where the ground belongs.
    def far_depth(corners):
        return min(c[0] + (Hm - c[1]) for c in corners.values())

    TONE = {"Floor": ("isofloor", "isonorth", "isoeast"),
            "Deck": ("isotop", "isonorth", "isoeast"),
            "Wall": ("isotop", "isonorth", "isoeast"),
            "Tower": ("isotop", "isonorth", "isoeast"),
            "Support": ("isotop", "isonorth", "isoeast"),
            "Ramp": ("isotop", "isonorth", "isoeast")}

    draw = []
    for p in kit["placements"]:
        cs = box_corners(p)
        draw.append((far_depth(cs), p["location_cm"][2], cs, fam(p)))

    for _, _, cs, family in sorted(draw, key=lambda t: (t[0], t[1])):
        top_cls, n_cls, e_cls = TONE.get(family, TONE["Wall"])
        faces = []
        for f in FACES:
            pts = [cs[f[i]] for i in range(4)]
            fc = [sum(c[i] for c in pts) / 4 for i in range(3)]
            cls = {"isotop": top_cls, "isonorth": n_cls,
                   "isoeast": e_cls}[f[4]]
            faces.append((fc[0] + (Hm - fc[1]) + fc[2] * 0.05, pts, cls))
        for _, pts, fcls in sorted(faces, key=lambda t: t[0]):
            quad = [Q(*c) for c in pts]
            o.append('<path class="%s" style="stroke-width:0.55" d="M %s Z"/>'
                     % (fcls, " L ".join("%.1f %.1f" % q for q in quad)))

    # key + count summary in the sheet's free lower-left corner
    fam_tot = {}
    for pl in kit["placements"]:
        fam_tot[fam(pl)] = fam_tot.get(fam(pl), 0) + 1
    kx, ky = 90, h - 450
    o.append('<text class="panel" x="%d" y="%d">ASSEMBLY SUMMARY</text>' % (kx, ky))
    rows = [("%d pieces total" % len(kit["placements"]), None)]
    for f in ("Floor", "Deck", "Wall", "Tower", "Support", "Ramp"):
        if f in fam_tot:
            rows.append(("%-8s %3d   as %s" % (f, fam_tot[f], ASSET_OF[f]), None))
    bud = kit.get("piece_budget", {})
    if bud:
        rows.append(("budget %d-%d (%s pass)" % (bud["band"][0], bud["band"][1],
                                                 kit.get("pass", "greybox")), None))
    for i, (txt, _) in enumerate(rows):
        o.append('<text class="roomd" x="%d" y="%d">%s</text>'
                 % (kx, ky + 22 + i * 18, txt))
    ly = ky + 22 + len(rows) * 18 + 16
    o.append('<text class="panel" x="%d" y="%d">TONE KEY</text>' % (kx, ly))
    for i, (cls, txt) in enumerate((("isotop", "top face (lit)"),
                                    ("isoeast", "east face"),
                                    ("isonorth", "north face"),
                                    ("isofloor", "floor plate top"),
                                    ("isoplate", "ground plane"))):
        yy = ly + 22 + i * 18
        o.append('<rect class="%s" x="%d" y="%d" width="24" height="11"/>'
                 % (cls, kx, yy - 9))
        o.append('<text class="small" x="%d" y="%d">%s</text>' % (kx + 34, yy, txt))

    o.append('<text class="small" x="70" y="102">Every piece a real solid at its '
             'scheduled thickness · painter sorts on each box far corner · '
             'build_aquarius_blockout.py places exactly these.</text>')
    sheet_chrome(o, w, h, "AK-301", "3/4 ASSEMBLY (MODULAR INSTANCES)", "steep 3/4 from NE")
    o.append("</svg>")
    (out_dir / "AK301_assembly_axon.svg").write_text("\n".join(o), encoding="utf-8")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--png", action="store_true")
    args = ap.parse_args()
    OUTD.mkdir(parents=True, exist_ok=True)
    kit, Wm, Hm = load()
    assembly_plan(kit, Wm, Hm, OUTD, "L1")
    assembly_plan(kit, Wm, Hm, OUTD, "L2")
    assembly_axon(kit, Wm, Hm, OUTD)
    print("wrote AK-101 / AK-102 / AK-301 (%d placements)" % len(kit["placements"]))
    if args.png:
        import re
        import shutil
        import subprocess
        chromium = shutil.which("chromium") or "/opt/pw-browsers/chromium"
        for svg in sorted(OUTD.glob("AK*.svg")):
            png = svg.with_suffix(".png")
            head = svg.read_text(encoding="utf-8")[:200]
            m = re.search(r'width="(\d+)" height="(\d+)"', head)
            win = "%d,%d" % (int(m.group(1)) + 24, int(m.group(2)) + 70)
            subprocess.run([chromium, "--headless", "--disable-gpu", "--no-sandbox",
                            "--force-device-scale-factor=2",
                            "--default-background-color=FFFFFFFF",
                            "--screenshot=%s" % png, "--hide-scrollbars",
                            "--window-size=%s" % win, svg.as_uri()],
                           check=True, capture_output=True, timeout=180)
            print("  rasterized %s" % png.name)


if __name__ == "__main__":
    main()
