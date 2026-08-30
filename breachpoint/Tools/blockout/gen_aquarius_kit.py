#!/usr/bin/env python3
"""BN30 — the MODULAR BLOCKOUT KIT for Aquarius.

    python3 Tools/blockout/gen_aquarius_kit.py [--png]

Founder directive (30 Aug): "break down each object - stairs, wall, ramps,
floors, obstacles, columns... block it out in a modular way, reusing the same
object throughout the level even at different heights and widths... a modular
set of blockout assets... so the Unreal MCP tools can make the level correctly
from the get-go."

WHAT THIS EMITS — Content/Data/aquarius_blockout_kit.json:
- kit.modules: the FEW meshes (engine basic shapes; a blockout scales
  primitives, it does not author meshes): BLK_Cube for floors/walls/masses/
  ramps, BLK_Cylinder reserved for pass-2 columns.
- kit.variants: unique (family, size) combinations - the "modular pieces".
  Sizes snap to the 0.5 m blockout grid and long runs split into the standard
  length family [8, 4, 2, 1, 0.5] m, so the same variant repeats across the
  level instead of every box being unique.
- placements: one entry per instance - {variant, label, folder, location_cm,
  rotation_deg (roll,pitch,yaw), scale} in UNREAL conventions: 1 uu = 1 cm,
  Z up, X = map EAST, Y = map SOUTH, origin at the arena's NW ground corner.
  Location is the box CENTER (engine BasicShapes pivot at center; 100 cm
  unit meshes, so scale = size_m). Deterministic: same inputs, same JSON.

SOURCE GEOMETRY: the same per-floor trace extraction as the REV D blueprint
set (gen_aquarius_blueprint.extract) - classes wall / tower / support / deck /
stair / floor, quantized to the 0.5 m grid (area-coverage vote, thin-wall
bias). Diagonal chamfers step at 0.5 m this pass; yaw-45 wall variants are a
pass-2 refinement and are called out in docs/design/BLOCKOUT-KIT.md.

LEVEL SCHEDULE (unchanged): floors t=0.20 top at 0.00 · decks 3.60-4.00 ·
supports 0-3.60 · towers 0-6.50 · perimeter 0-8.00 · ramps rise 0.25->4.00.
"""

from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from gen_aquarius_blueprint import extract, DECK, TOWER_H, WALL_H  # noqa: E402

GAME = HERE.parents[1]
OUT = GAME / "Content" / "Data" / "aquarius_blockout_kit.json"

GRID_M = 0.5                       # the blockout grid everything snaps to
LENGTHS = [8.0, 4.0, 2.0, 1.0, 0.5]  # standard modular length family
FLOOR_T = 0.2
RAMP_T = 0.3
RAMP_Z0 = 0.25                     # ramps land a quarter-step above the floor

FAMILIES = {                       # family -> (z0, z1, folder)
    "Floor":   (-FLOOR_T, 0.0, "Blockout/Floors"),
    "Deck":    (DECK[0], DECK[1], "Blockout/Decks"),
    "Wall":    (0.0, WALL_H, "Blockout/Walls"),
    "Tower":   (0.0, TOWER_H, "Blockout/Towers"),
    "Support": (0.0, DECK[0], "Blockout/Supports"),
}


def quantize(cells, W, H, cell_m, thresh):
    """Trace cells -> 0.5 m occupancy grid by area coverage."""
    gw = math.ceil(W * cell_m / GRID_M)
    gh = math.ceil(H * cell_m / GRID_M)
    acc = {}
    for (x, y) in cells:
        # spread each trace cell's area over the metre-grid bins it overlaps
        x0, x1 = x * cell_m, (x + 1) * cell_m
        y0, y1 = y * cell_m, (y + 1) * cell_m
        bx0, bx1 = int(x0 / GRID_M), min(int(x1 / GRID_M), gw - 1)
        by0, by1 = int(y0 / GRID_M), min(int(y1 / GRID_M), gh - 1)
        for bx in range(bx0, bx1 + 1):
            for by in range(by0, by1 + 1):
                ox = max(0.0, min(x1, (bx + 1) * GRID_M) - max(x0, bx * GRID_M))
                oy = max(0.0, min(y1, (by + 1) * GRID_M) - max(y0, by * GRID_M))
                acc[(bx, by)] = acc.get((bx, by), 0.0) + ox * oy
    full = GRID_M * GRID_M
    return {b for b, a in acc.items() if a >= thresh * full}, gw, gh


def merge_rects(cells):
    cs = set(cells)
    rects = []
    while cs:
        x, y = min(cs)
        w = 1
        while (x + w, y) in cs:
            w += 1
        h = 1
        while all((xx, y + h) in cs for xx in range(x, x + w)):
            h += 1
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                cs.discard((xx, yy))
        rects.append((x, y, w, h))
    return rects


def split_standard(length_m):
    """Greedy split of a run into the standard length family."""
    out, rest = [], round(length_m / GRID_M) * GRID_M
    for L in LENGTHS:
        while rest >= L - 1e-9:
            out.append(L)
            rest = round((rest - L) / GRID_M) * GRID_M
    return out


def decompose(bins, family, z0, z1):
    """Grid bins -> modular box instances: rects, long axis split into the
    standard family so identical variants repeat."""
    inst = []
    for (bx, by, bw, bh) in merge_rects(bins):
        horiz = bw >= bh
        run_m = (bw if horiz else bh) * GRID_M
        cross_m = (bh if horiz else bw) * GRID_M
        off = 0.0
        for L in split_standard(run_m):
            cx = (bx * GRID_M) + (off + L / 2 if horiz else bw * GRID_M / 2)
            cy = (by * GRID_M) + (bh * GRID_M / 2 if horiz else off + L / 2)
            size = (L, cross_m) if horiz else (cross_m, L)
            inst.append({"family": family,
                         "size_m": [round(size[0], 2), round(size[1], 2),
                                    round(z1 - z0, 2)],
                         "center_m": [round(cx, 3), round(cy, 3),
                                      round((z0 + z1) / 2, 3)],
                         "yaw_deg": 0.0, "pitch_deg": 0.0})
            off += L
    return inst


def ramp_instances(solids, deckcells, cell_m):
    """Each traced ramp -> ONE BLK_Cube instance pitched into a slab wedge.
    Low end at the arena floor, high end against whichever bbox edge touches
    the deck mask (the same orientation test the A-301 blockout view uses)."""
    out = []
    for s in (s for s in solids if s.kind == "stair"):
        x0, y0, x1, y1 = s.bbox
        horiz = (x1 - x0) >= (y1 - y0)

        def touch(edge):
            return sum(1 for c in edge if c in deckcells)

        if horiz:
            lo = touch((x0 - 1, y) for y in range(y0, y1))
            hi = touch((x1, y) for y in range(y0, y1))
            run = (x1 - x0) * cell_m
            width = (y1 - y0) * cell_m
            yaw = 0.0 if hi >= lo else 180.0     # +X toward the high end
        else:
            lo = touch((x, y0 - 1) for x in range(x0, x1))
            hi = touch((x, y1) for x in range(x0, x1))
            run = (y1 - y0) * cell_m
            width = (x1 - x0) * cell_m
            yaw = 90.0 if hi >= lo else -90.0    # +Y(south) / -Y toward high
        run = max(GRID_M, round(run / GRID_M) * GRID_M)
        width = max(GRID_M, round(width / GRID_M) * GRID_M)
        rise = DECK[1] - RAMP_Z0
        slope = math.degrees(math.atan2(rise, run))
        cx = (x0 + x1) / 2 * cell_m
        cy = (y0 + y1) / 2 * cell_m
        if slope > 45.0:
            # NOT a ramp: a capsule too short to climb 4 m within the UE
            # walkable-slope limit (~44.8 deg). The trace shows a thin light
            # sliver here, not a runnable ramp - place an unpitched block so
            # the mass exists, flag it, and let channel (b) say what it is.
            out.append({"family": "Ramp",
                        "size_m": [round(run, 2), round(width, 2),
                                   round(DECK[1], 2)],
                        "center_m": [round(cx, 3), round(cy, 3),
                                     round(DECK[1] / 2, 3)],
                        "yaw_deg": 0.0 if horiz else 90.0, "pitch_deg": 0.0,
                        "run_m": round(run, 2), "rise_m": rise,
                        "suspect": "slope %.0f deg exceeds walkable 45 - "
                                   "placed as solid block pending channel (b)"
                                   % slope})
            continue
        slab = math.hypot(run, rise)
        out.append({"family": "Ramp",
                    "size_m": [round(slab, 2), round(width, 2), RAMP_T],
                    "center_m": [round(cx, 3), round(cy, 3),
                                 round((RAMP_Z0 + DECK[1]) / 2, 3)],
                    "yaw_deg": yaw,
                    # UE rotator: positive pitch = nose up toward local +X
                    "pitch_deg": round(slope, 2),
                    "run_m": round(run, 2), "rise_m": round(rise, 2)})
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--png", action="store_true")
    args = ap.parse_args()

    solids, floor, W, H, cell_m = extract()
    deckcells = set().union(*(s.cells for s in solids if s.kind == "deck")) \
        if any(s.kind == "deck" for s in solids) else set()
    masks = {k: set() for k in ("wall", "tower", "support", "deck", "stair")}
    for s in solids:
        masks[s.kind] |= s.cells

    inst = []
    # thin-wall bias: walls keep bins at 35% coverage; slabs want half
    qwall, gw, gh = quantize(masks["wall"], W, H, cell_m, 0.35)
    qtower, _, _ = quantize(masks["tower"], W, H, cell_m, 0.45)
    qsup, _, _ = quantize(masks["support"], W, H, cell_m, 0.45)
    qdeck, _, _ = quantize(masks["deck"], W, H, cell_m, 0.45)
    qstair, _, _ = quantize(masks["stair"], W, H, cell_m, 0.45)
    qfloor, _, _ = quantize(floor, W, H, cell_m, 0.4)
    qtower -= qwall
    qsup -= qwall | qtower
    qfloor -= qwall | qtower           # floor continues under decks and ramps

    inst += decompose(qfloor, "Floor", *FAMILIES["Floor"][:2])
    inst += decompose(qdeck, "Deck", *FAMILIES["Deck"][:2])
    inst += decompose(qwall, "Wall", *FAMILIES["Wall"][:2])
    inst += decompose(qtower, "Tower", *FAMILIES["Tower"][:2])
    inst += decompose(qsup, "Support", *FAMILIES["Support"][:2])
    inst += ramp_instances(solids, deckcells, cell_m)

    # variants: the modular catalogue - unique (family, size)
    variants, order = {}, []
    for it in inst:
        key = (it["family"], tuple(it["size_m"]))
        if key not in variants:
            vid = "%s_%03dx%03dx%03d" % (it["family"],
                                         round(it["size_m"][0] * 100) // 10,
                                         round(it["size_m"][1] * 100) // 10,
                                         round(it["size_m"][2] * 100) // 10)
            variants[key] = {"id": vid, "family": it["family"],
                             "size_m": it["size_m"], "count": 0}
            order.append(key)
        variants[key]["count"] += 1

    folders = dict((k, v[2]) for k, v in FAMILIES.items())
    folders["Ramp"] = "Blockout/Ramps"
    placements = []
    per_family = {}
    for it in inst:
        key = (it["family"], tuple(it["size_m"]))
        v = variants[key]
        n = per_family[it["family"]] = per_family.get(it["family"], 0) + 1
        entry = {"variant": v["id"],
                 "label": "BLK_%s_%03d" % (it["family"], n),
                 "folder": folders[it["family"]],
                 "location_cm": [round(it["center_m"][0] * 100, 1),
                                 round(it["center_m"][1] * 100, 1),
                                 round(it["center_m"][2] * 100, 1)],
                 "rotation_deg": {"roll": 0.0, "pitch": it["pitch_deg"],
                                  "yaw": it["yaw_deg"]},
                 "scale": [it["size_m"][0], it["size_m"][1], it["size_m"][2]]}
        if "suspect" in it:
            entry["suspect"] = it["suspect"]
        placements.append(entry)

    kit = {
        "kit_id": "aquarius_blockout_kit",
        "version": 1,
        "units": "location cm (1uu), scale = metres of a 100 cm unit mesh",
        "axes": "X = map EAST, Y = map SOUTH, Z up; origin at NW ground corner "
                "of the traced footprint; rotations are UE rotators (deg)",
        "grid_m": GRID_M,
        "length_family_m": LENGTHS,
        "modules": [
            {"id": "BLK_Cube", "mesh": "/Engine/BasicShapes/Cube.Cube",
             "note": "100 cm cube, pivot at CENTER; every Floor/Deck/Wall/"
                     "Tower/Support/Ramp variant is this mesh scaled "
                     "(ramps additionally pitched)."},
            {"id": "BLK_Cylinder", "mesh": "/Engine/BasicShapes/Cylinder.Cylinder",
             "note": "reserved for the pass-2 free-standing columns "
                     "(orbitable, never wall-touching - reference notes 21-25); "
                     "no auto-placements this pass."},
        ],
        "variants": [variants[k] for k in order],
        "level_schedule_m": {"floor_top": 0.0, "deck": list(DECK),
                             "tower_top": TOWER_H, "wall_top": WALL_H,
                             "ramp": [RAMP_Z0, DECK[1]]},
        "doubts": [p["label"] + ": " + p["suspect"]
                   for p in placements if "suspect" in p],
        "pass2_owed": [
            "yaw-45 wall variants where the trace shows true diagonals "
            "(stepped at 0.5 m this pass)",
            "free-standing columns (BLK_Cylinder), battery octagons at the "
            "base mouths, lane crates - reference notes 16-25 anchors",
            "railings on deck edges the reference shows railed",
            "sunken center trench below grade (channel b)",
        ],
        "placements": placements,
    }
    OUT.write_text(json.dumps(kit, indent=1), encoding="utf-8")

    top = sorted(kit["variants"], key=lambda v: -v["count"])[:8]
    print("wrote %s" % OUT.relative_to(GAME))
    print("grid %.1f m · %d placements · %d variants of %d modules"
          % (GRID_M, len(placements), len(kit["variants"]), len(kit["modules"])))
    print("per family: %s" % {k: per_family.get(k, 0) for k in
                              ("Floor", "Deck", "Wall", "Tower", "Support", "Ramp")})
    print("top variants: " + ", ".join("%s x%d" % (v["id"], v["count"]) for v in top))

    if args.png:
        contact_sheet(kit, gw, gh)


def contact_sheet(kit, gw, gh):
    """Top-down instance map colored by family — the founder's eyeball check
    that the modular decomposition still IS the level."""
    try:
        from PIL import Image, ImageDraw
    except ImportError:
        print("note: PIL missing, contact sheet skipped")
        return
    S = 26
    img = Image.new("RGB", (gw * S // 2 + 320, gh * S // 2 + 80), "white")
    d = ImageDraw.Draw(img)
    colors = {"Floor": (235, 235, 235), "Deck": (180, 205, 235),
              "Support": (250, 210, 160), "Tower": (120, 120, 125),
              "Wall": (35, 35, 40), "Ramp": (170, 230, 170)}
    zorder = {"Floor": 0, "Deck": 1, "Support": 2, "Tower": 3, "Wall": 4, "Ramp": 5}
    fam = lambda p: p["variant"].split("_")[0]
    for p in sorted(kit["placements"], key=lambda p: zorder[fam(p)]):
        x, y, _ = p["location_cm"]
        sx, sy, _ = p["scale"]
        f = fam(p)
        x0 = 20 + (x / 100 - sx / 2) * S / 2
        y0 = 40 + (y / 100 - sy / 2) * S / 2
        d.rectangle([x0, y0, x0 + sx * S / 2, y0 + sy * S / 2],
                    fill=colors[f], outline=(90, 90, 90))
    lx = gw * S // 2 + 40
    d.text((lx, 40), "MODULAR INSTANCES", fill="black")
    yy = 70
    for f, c in colors.items():
        n = sum(1 for p in kit["placements"] if fam(p) == f)
        d.rectangle([lx, yy, lx + 18, yy + 12], fill=c, outline=(0, 0, 0))
        d.text((lx + 26, yy), "%s x%d" % (f, n), fill="black")
        yy += 22
    d.text((lx, yy + 10), "%d placements" % len(kit["placements"]), fill="black")
    d.text((lx, yy + 32), "%d variants" % len(kit["variants"]), fill="black")
    out = GAME / "docs" / "design" / "blueprints" / "breachpoint_aquarius" / "kit_contact_sheet.png"
    img.save(out)
    print("wrote %s" % out.relative_to(GAME))


if __name__ == "__main__":
    main()
