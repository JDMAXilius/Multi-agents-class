"""BN36 - a 1:1 blockout of Halo 3 ODST 'Rally Point', from its OBJ.

    python3 Tools/blockout/gen_rallypoint_kit.py [--grid 2.0] [--core]

WHY THIS EXISTS. We had two ways to use Rally Point and both were bad:
import the rip (extracted 343 content - cannot ship, and it arrives mirrored by
Interchange so it fights the navmesh), or trace it by eye off screenshots the
way Aquarius was traced (approximate, and the fidelity is self-graded).

This is the third way: read the SOURCE GEOMETRY and re-express it in OUR kit.
The output is our own boxes at measured positions, so nothing of theirs ships,
and "1:1" is a number we can compute rather than a claim.

AXES. The OBJ is Y-up right-handed (Blender export). We convert ONCE, here:

    ue_x = obj_x        ue_y = -obj_z        ue_z = obj_y

That is a real handedness-preserving conversion, NOT the mirror Interchange
applies on import (x, -y, z with no axis swap), which is why the imported mesh
came in upside down and the navmesh found no up-facing surfaces. A blockout
built from this generator cannot inherit that bug.

OUTPUT is the same schema as aquarius_blockout_kit.json, so the committed
builder lands it unchanged:
    Content/Data/rallypoint_blockout_kit.json
"""

from __future__ import annotations

import argparse
import json
import math
import os
from collections import defaultdict

OBJ = os.path.expanduser(
    "~/Downloads/locationshalo-3firefightodstrally-point/source/RallyPoint/"
    "Rally Point.obj")
OUT = "Content/Data/rallypoint_blockout_kit.json"

HORIZ_DOT = 0.70          # |nz| above this = walkable surface
VERT_DOT = 0.50           # |nz| below this = wall
FLOOR_T = 0.20            # blockout floor plate thickness, m
WALL_T = 0.30             # blockout wall thickness, m
LEVEL_BIN = 0.75          # cluster floor heights into levels this tall, m
MIN_LEVEL_CELLS = 12      # ignore levels smaller than this (clutter, props)
MIN_WALL_CELLS = 2


def load_obj(path):
    """Return (verts_in_ue_metres, triangles)."""
    V, F = [], []
    with open(path, "r", errors="replace") as f:
        for line in f:
            if line.startswith("v "):
                p = line.split()
                x, y, z = float(p[1]), float(p[2]), float(p[3])
                V.append((x, -z, y))                  # Y-up -> UE Z-up
            elif line.startswith("f "):
                idx = [int(t.split("/")[0]) - 1 for t in line.split()[1:]]
                for k in range(1, len(idx) - 1):
                    F.append((idx[0], idx[k], idx[k + 1]))
    return V, F


def tri_normal(a, b, c):
    ux, uy, uz = b[0]-a[0], b[1]-a[1], b[2]-a[2]
    vx, vy, vz = c[0]-a[0], c[1]-a[1], c[2]-a[2]
    nx, ny, nz = uy*vz-uz*vy, uz*vx-ux*vz, ux*vy-uy*vx
    L = math.sqrt(nx*nx+ny*ny+nz*nz)
    return (0.0, 0.0, 0.0) if L < 1e-12 else (nx/L, ny/L, nz/L)


def rasterise_tri(a, b, c, g):
    """Yield (gx, gy) cells the triangle covers, by area-proportional sampling."""
    minx = min(a[0], b[0], c[0]); maxx = max(a[0], b[0], c[0])
    miny = min(a[1], b[1], c[1]); maxy = max(a[1], b[1], c[1])
    nx = max(1, int((maxx-minx)/g) + 1)
    ny = max(1, int((maxy-miny)/g) + 1)
    if nx * ny > 20000:                     # skybox-sized junk triangle
        return
    for ix in range(nx):
        for iy in range(ny):
            px = minx + (ix + 0.5) * g
            py = miny + (iy + 0.5) * g
            # barycentric point-in-triangle in XY
            d = ((b[1]-c[1])*(a[0]-c[0]) + (c[0]-b[0])*(a[1]-c[1]))
            if abs(d) < 1e-12:
                continue
            l1 = ((b[1]-c[1])*(px-c[0]) + (c[0]-b[0])*(py-c[1])) / d
            l2 = ((c[1]-a[1])*(px-c[0]) + (a[0]-c[0])*(py-c[1])) / d
            l3 = 1.0 - l1 - l2
            if l1 < -0.02 or l2 < -0.02 or l3 < -0.02:
                continue
            z = l1*a[2] + l2*b[2] + l3*c[2]
            yield (int(math.floor(px/g)), int(math.floor(py/g)), z)


def maximal_rects(cells):
    """Greedy decomposition of a cell set into axis-aligned rectangles."""
    remaining = set(cells)
    rects = []
    while remaining:
        x0, y0 = min(remaining, key=lambda c: (c[1], c[0]))
        w = 0
        while (x0 + w, y0) in remaining:
            w += 1
        h = 1
        while all((x0 + i, y0 + h) in remaining for i in range(w)):
            h += 1
        for i in range(w):
            for j in range(h):
                remaining.discard((x0 + i, y0 + j))
        rects.append((x0, y0, w, h))
    return rects


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--grid", type=float, default=2.0)
    ap.add_argument("--core", action="store_true",
                    help="restrict to the 5-95%% play-space core")
    ap.add_argument("--levels", type=int, default=6,
                    help="keep only the N largest floor levels (a 62 m tall "
                         "city rip otherwise yields 40+ levels of clutter)")
    ap.add_argument("--merge", type=float, default=1.5,
                    help="merge floor levels closer together than this, m")
    ap.add_argument("--min-cells", type=int, default=40)
    args = ap.parse_args()
    g = args.grid

    V, F = load_obj(OBJ)
    print("obj: %d verts, %d tris" % (len(V), len(F)))

    if args.core:
        xs = sorted(v[0] for v in V); ys = sorted(v[1] for v in V)
        lo = lambda a: a[int(len(a)*0.05)]
        hi = lambda a: a[int(len(a)*0.95)]
        BX = (lo(xs), hi(xs)); BY = (lo(ys), hi(ys))
    else:
        BX = (min(v[0] for v in V), max(v[0] for v in V))
        BY = (min(v[1] for v in V), max(v[1] for v in V))
    print("footprint %.1f x %.1f m" % (BX[1]-BX[0], BY[1]-BY[0]))

    floors = defaultdict(list)      # (gx,gy) -> [z, ...]
    walls = defaultdict(lambda: [1e18, -1e18])
    for (ia, ib, ic) in F:
        a, b, c = V[ia], V[ib], V[ic]
        n = tri_normal(a, b, c)
        # UP-FACING ONLY. abs(nz) also accepts ceilings and roof undersides,
        # and treating those as floors fills every interior solid - the first
        # run produced a featureless slab instead of a street plan.
        if n[2] >= HORIZ_DOT:
            for gx, gy, z in rasterise_tri(a, b, c, g):
                if BX[0] <= gx*g <= BX[1] and BY[0] <= gy*g <= BY[1]:
                    floors[(gx, gy)].append(z)
        elif abs(n[2]) <= VERT_DOT:
            zlo = min(a[2], b[2], c[2]); zhi = max(a[2], b[2], c[2])
            if zhi - zlo < 0.4:                     # kerb, not a wall
                continue
            for gx, gy, _z in rasterise_tri(a, b, c, g):
                if BX[0] <= gx*g <= BX[1] and BY[0] <= gy*g <= BY[1]:
                    w = walls[(gx, gy)]
                    w[0] = min(w[0], zlo); w[1] = max(w[1], zhi)

    # cluster floor heights into discrete LEVELS
    levels = defaultdict(set)
    for (gx, gy), zs in floors.items():
        for z in set(round(zz / LEVEL_BIN) * LEVEL_BIN for zz in zs):
            levels[z].add((gx, gy))
    levels = {z: cs for z, cs in levels.items() if len(cs) >= args.min_cells}

    # MERGE near-coincident levels. A 0.75 m bin over 62 m of city produced 43
    # "levels", most of them the same storey split by kerbs and thresholds.
    for _ in range(8):
        zs = sorted(levels)
        merged = False
        for i in range(len(zs) - 1):
            if zs[i+1] - zs[i] < args.merge and zs[i] in levels and zs[i+1] in levels:
                keep = zs[i] if len(levels[zs[i]]) >= len(levels[zs[i+1]]) else zs[i+1]
                drop = zs[i+1] if keep == zs[i] else zs[i]
                levels[keep] |= levels[drop]
                del levels[drop]
                merged = True
                break
        if not merged:
            break

    # keep the N largest storeys; the rest is set dressing, not a floor
    if len(levels) > args.levels:
        keep = sorted(levels, key=lambda z: -len(levels[z]))[:args.levels]
        levels = {z: levels[z] for z in keep}

    # a cell already covered by a LOWER kept level within one merge distance is
    # the same floor seen twice - drop it so storeys do not stack on themselves
    for zi in sorted(levels):
        for zj in sorted(levels):
            if zj > zi and zj - zi < args.merge:
                levels[zj] -= levels[zi]

    ordered = sorted(levels, key=lambda z: -len(levels[z]))
    print("floor levels kept: %d (largest at z=%.2f m, %d cells)"
          % (len(levels), ordered[0], len(levels[ordered[0]])) if ordered
          else "no floor levels")

    placements, variants = [], {}
    def emit(fam, folder, cx, cy, cz, sx, sy, sz, idx):
        key = "%s_%03dx%03dx%03d" % (fam, round(sx*10), round(sy*10), round(sz*10))
        v = variants.setdefault(key, {"id": key, "family": fam,
                                      "size_m": [round(sx, 2), round(sy, 2),
                                                 round(sz, 2)], "count": 0})
        v["count"] += 1
        placements.append({
            "variant": key,
            "label": "BLK_%s_%03d" % (fam, idx),
            "folder": folder,
            "location_cm": [round(cx*100, 1), round(cy*100, 1), round(cz*100, 1)],
            "rotation_deg": {"roll": 0.0, "pitch": 0.0, "yaw": 0.0},
            "scale": [round(sx, 2), round(sy, 2), round(sz, 2)],
        })

    n = 0
    for z in sorted(levels, key=lambda t: t):
        fam = "Floor" if z <= min(levels) + 0.01 else "Deck"
        folder = "Blockout/Floors" if fam == "Floor" else "Blockout/Decks"
        for (x0, y0, w, h) in maximal_rects(levels[z]):
            n += 1
            emit(fam, folder,
                 (x0 + w/2.0)*g, (y0 + h/2.0)*g, z - FLOOR_T/2.0,
                 w*g, h*g, FLOOR_T, n)
    nfloor = n

    wallcells = {k: v for k, v in walls.items() if v[1] - v[0] >= 0.5}
    m = 0
    bands = defaultdict(set)
    for (gx, gy), (zlo, zhi) in wallcells.items():
        bands[(round(zlo/LEVEL_BIN)*LEVEL_BIN, round(zhi/LEVEL_BIN)*LEVEL_BIN)].add((gx, gy))
    for (zlo, zhi), cells in bands.items():
        if len(cells) < MIN_WALL_CELLS:
            continue
        for (x0, y0, w, h) in maximal_rects(cells):
            m += 1
            emit("Wall", "Blockout/Walls",
                 (x0 + w/2.0)*g, (y0 + h/2.0)*g, (zlo + zhi)/2.0,
                 w*g, h*g, max(WALL_T, zhi - zlo), nfloor + m)

    kit = {
        "kit_id": "rallypoint_blockout",
        "version": 1,
        "pass": "greybox",
        "source": "Halo 3 ODST 'Rally Point' OBJ, measured - no Halo geometry "
                  "is reproduced, only our kit boxes at measured positions",
        "units": "centimetres in location_cm; metres in size_m/scale",
        "axes": "UE: X east, Y south, Z up. OBJ Y-up converted ONCE as "
                "(x, -z, y) - handedness preserving, unlike Interchange's mirror",
        "grid_m": g,
        "asset_catalog": "Content/Data/blockout_kit_assets.json",
        "asset_map": {"Floor": "BLK_Floor_400", "Deck": "BLK_Floor_400",
                      "Wall": "BLK_Wall_400"},
        "level_schedule_m": {"floor_thickness": FLOOR_T,
                             "wall_thickness": WALL_T,
                             "level_bin": LEVEL_BIN},
        "footprint_m": [round(BX[1]-BX[0], 2), round(BY[1]-BY[0], 2)],
        "floor_levels_m": sorted(round(z, 2) for z in levels),
        "spawn_points": [],
        "variants": list(variants.values()),
        "placements": placements,
    }
    with open(OUT, "w", encoding="utf-8") as f:
        json.dump(kit, f, indent=1)
    print("wrote %s" % OUT)
    print("  %d placements (%d floor/deck + %d wall), %d variants, grid %.1f m"
          % (len(placements), nfloor, m, len(variants), g))


if __name__ == "__main__":
    main()
