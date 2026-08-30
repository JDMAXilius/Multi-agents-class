#!/usr/bin/env python3
"""BN29 v3 — AQUARIUS one-to-one from PER-FLOOR traced references.

    python3 Tools/blockout/trace_aquarius.py --image docs/design/reference/aquarius_overhead_GROUND_floor.webp --tag ground
    python3 Tools/blockout/trace_aquarius.py --image docs/design/reference/aquarius_overhead_UPPER_floor.webp --tag upper
    python3 Tools/blockout/gen_aquarius_manifest.py

The founder finished the reference handoff (15 images, docs/design/reference/):
per-floor overheads, in-game shots, and the skeleton labeled in their own hand
- Arena 1/2 (the courts), Hallway 1 (the SHORT east-west center crossing
through both arenas), Hallways 2/3 (the long north/south lanes), Base 1/2, and
the BRIDGE running NORTH-SOUTH over the center (v2 had it east-west; wrong).

v3 builds each floor from its own traced overhead:
- GROUND trace: BLACK -> solids. The perimeter ring (cells touching background)
  rises to the ceiling; the glass hydro TOWERS (interior blacks with no deck
  above) rise ~two storeys; blacks UNDER an upper deck become its supports.
- UPPER trace: LIGHT -> the upper walkable ring, base upper floors, and the
  N-S bridge, as pillar-borne deck slabs z[3.6,4] - open beneath (the shots
  show octagonal pillars; the blockout floats the slabs and says so).
- Small isolated ground-light capsules away from the bases -> stair volumes
  (the long mesh ramps of shots 9/4; also Hallway 1's glass-grate crossing).
Both grids symmetrized about the map's own mirror (left half wins).

Scale stays the one derived number: long axis 52 m (channel b - an in-game
Forge walk - replaces it; nothing else changes).
"""

from __future__ import annotations

import json
from collections import deque
from pathlib import Path

GAME = Path(__file__).resolve().parents[2]
REFD = GAME / "docs" / "design" / "reference"
OUT = GAME / "Content" / "Data" / "aquarius_manifest.json"

LONG_AXIS_M = 52.0
DECK_Z = (3.6, 4.0)
TOWER_H = 6.5
WALL_H = 8.0


def load(tag):
    t = json.loads((REFD / ("aquarius_trace_%s.json" % tag)).read_text())
    return t


def main():
    tg, tu = load("ground"), load("upper")
    assert tg["bbox_cells"] == tu["bbox_cells"], "overheads not registered"
    x0, y0, x1, y1 = tg["bbox_cells"]
    W4 = x1 - x0 + 1
    cell_m = LONG_AXIS_M / W4 * 2

    def regrid(t):
        rows = t["rows"]
        RANK = {"#": 4, "L": 3, "m": 2, "d": 2, ".": 0}
        gw, gh = (W4 + 1) // 2, (y1 - y0 + 2) // 2
        g = [["." for _ in range(gw)] for _ in range(gh)]
        for gy in range(gh):
            for gx in range(gw):
                votes = {}
                for dy in (0, 1):
                    for dx in (0, 1):
                        yy, xx = y0 + gy * 2 + dy, x0 + gx * 2 + dx
                        if yy <= y1 and xx <= x1:
                            c = rows[yy][xx]
                            votes[c] = votes.get(c, 0) + 1
                if votes.get("#", 0) >= 1 and votes.get(".", 0) < 3:
                    g[gy][gx] = "#"
                else:
                    best = max(votes, key=lambda c: (votes[c], RANK[c]))
                    nz = {c: n for c, n in votes.items() if c != "."}
                    if best == "." and nz and sum(nz.values()) >= 2:
                        best = max(nz, key=lambda c: (nz[c], RANK[c]))
                    g[gy][gx] = best
        for gy in range(gh):                       # symmetrize, left wins
            for gx in range(gw // 2):
                g[gy][gw - 1 - gx] = g[gy][gx]
        return [row[:] for row in g[::-1]], gw, gh  # +y = north

    G, gw, gh = regrid(tg)
    Up, _, _ = regrid(tu)

    def components(g, match):
        seen = [[False] * gw for _ in range(gh)]
        out = []
        for sy in range(gh):
            for sx in range(gw):
                if match(g[sy][sx]) and not seen[sy][sx]:
                    comp, q = [], deque([(sy, sx)])
                    seen[sy][sx] = True
                    while q:
                        y, x = q.popleft()
                        comp.append((y, x))
                        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                            yy, xx = y + dy, x + dx
                            if 0 <= yy < gh and 0 <= xx < gw and not seen[yy][xx] \
                                    and match(g[yy][xx]):
                                seen[yy][xx] = True
                                q.append((yy, xx))
                    out.append(comp)
        return out

    def merge_rects(cells):
        cs = set(cells)
        rects = []
        while cs:
            y, x = min(cs)
            w = 1
            while (y, x + w) in cs:
                w += 1
            h = 1
            while all((y + h, xx) in cs for xx in range(x, x + w)):
                h += 1
            for yy in range(y, y + h):
                for xx in range(x, x + w):
                    cs.discard((yy, xx))
            rects.append((x, y, w, h))
        return rects

    def fps(cells, z0, z1, role, label):
        return [{"label": "%s_%02d" % (label, i),
                 "x": [round(x * cell_m, 2), round((x + w) * cell_m, 2)],
                 "y": [round(y * cell_m, 2), round((y + h) * cell_m, 2)],
                 "z": [z0, z1], "role": role}
                for i, (x, y, w, h) in enumerate(merge_rects(cells))]

    # ---- upper floor: LIGHT = walkable deck --------------------------------
    upper_walk = [(y, x) for y in range(gh) for x in range(gw) if Up[y][x] == "L"]
    upper_set = set(upper_walk)

    # ---- ground blacks, three destinies ----------------------------------------
    ring, support, tower = [], [], []
    for y in range(gh):
        for x in range(gw):
            if G[y][x] != "#":
                continue
            edge = any(not (0 <= y + dy < gh and 0 <= x + dx < gw)
                       or G[y + dy][x + dx] == "."
                       for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)))
            if edge:
                ring.append((y, x))
            elif (y, x) in upper_set:
                support.append((y, x))
            else:
                tower.append((y, x))

    # ---- stairs: small ground-light capsules away from the base ends ------
    stairs = []
    for comp in components(G, lambda c: c == "L"):
        xs = [x for _, x in comp]
        cx = sum(xs) / len(xs)
        if len(comp) < 70 and gw * 0.18 < cx < gw * 0.82:
            stairs += comp
    stair_set = set(stairs)
    tower = [c for c in tower if c not in stair_set]

    lm = []
    lm.append({"name": "Perimeter", "footprints":
               fps(ring, 0, WALL_H, "solid; traced silhouette ring - the arena wall", "rng"),
               "purpose": "The traced outline as the enclosing wall, x[0,%g] y[0,%g] overall." % (round(gw*cell_m,1), round(gh*cell_m,1))})
    lm.append({"name": "Hydro Towers and Blocks", "footprints":
               fps(tower, 0, TOWER_H, "solid; traced ground BLACK with no deck above - the glass hydroponic towers and interior blocks (shots 6/11)", "twr"),
               "purpose": "Ground-floor solid masses x[0,%g] y[0,%g] overall - the four glass towers and their kin, two storeys." % (round(gw*cell_m,1), round(gh*cell_m,1))})
    lm.append({"name": "Deck Supports", "footprints":
               fps(support, 0, DECK_Z[0], "solid; traced ground BLACK beneath an upper deck - walls/pillars carrying the floor above", "sup"),
               "purpose": "Ground blacks under upper decks x[0,%g] y[0,%g] overall, capped at the deck soffit." % (round(gw*cell_m,1), round(gh*cell_m,1))})
    lm.append({"name": "Upper Floor", "footprints":
               fps(upper_walk, DECK_Z[0], DECK_Z[1], "deck slab; traced UPPER-overhead LIGHT - the walkway ring, base upper floors and the north-south BRIDGE; pillar-borne, open beneath (blockout floats it and says so)", "upr"),
               "purpose": "The entire upper walkable floor from its own overhead, x[0,%g] y[0,%g] overall. "
                          "The founder-labeled BRIDGE is its center north-south strip; the ROCKET stands at "
                          "the bridge's center (Aquarius' camo/OS slot - BN29 adaptation)." % (round(gw*cell_m,1), round(gh*cell_m,1))})
    if stairs:
        lm.append({"name": "Ramps", "footprints":
                   fps(stairs, 0, DECK_Z[1], "ground-to-mid stair volume; traced ground-light capsule - the long mesh ramps (shots 4/9) and the Hallway 1 crossing", "rmp"),
                   "purpose": "Traced capsules as stair volumes x[0,%g] y[0,%g] overall." % (round(gw*cell_m,1), round(gh*cell_m,1))})

    Wm, Hm = round(gw * cell_m, 2), round(gh * cell_m, 2)
    mxx = lambda v: round(Wm - v, 2)

    # rocket at the bridge: centroid of upper cells in the center N-S band
    band = [(y, x) for (y, x) in upper_walk if abs(x - gw / 2) < 2.5 / cell_m]
    if band:
        bx = sum(x for _, x in band) / len(band) * cell_m
        by = sum(y for y, _ in band) / len(band) * cell_m
    else:
        bx, by = Wm / 2, Hm / 2
    rocket = {"x": round(bx, 2), "y": round(by, 2), "z": DECK_Z[1], "elevation": "mid",
              "landmark": "Upper Floor",
              "notes": "Center of the founder-labeled north-south BRIDGE - the rocket in "
                       "Aquarius' camo/OS slot (BN29 adaptation)."}

    # spawns: on the west base upper floor (tucked), neutrals on the courts
    west_upper = sorted(components(Up, lambda c: c == "L"),
                        key=lambda comp: min(x for _, x in comp))
    big_west = [c for c in west_upper if len(c) > 200][:1]
    if big_west:
        comp = big_west[0]
        xs = [x for _, x in comp]
        xmid = (min(xs) + max(xs)) / 2
        westside = [(y, x) for (y, x) in comp if x <= xmid]
        pts = [min(westside, key=lambda c: c[0] * 1000 + c[1]),
               max(westside, key=lambda c: c[0] * 1000 - c[1])]
        (ay, ax), (by_, bx_) = pts
        cy_, cx_ = (ay + by_) / 2, (ax + bx_) / 2
        ay, ax = ay + (cy_ - ay) * 0.2, ax + (cx_ - ax) * 0.2
        by_, bx_ = by_ + (cy_ - by_) * 0.2, bx_ + (cx_ - bx_) * 0.2
        p1 = (round(ax * cell_m, 1), round(ay * cell_m, 1))
        p2 = (round(bx_ * cell_m, 1), round(by_ * cell_m, 1))
    else:
        p1, p2 = (6, Hm * 0.35), (4, Hm * 0.65)
    spawns = [
        {"id": "SP1", "location": {"x": p1[0], "y": p1[1], "z": DECK_Z[1]}, "facing": 0, "pool": "team_a"},
        {"id": "SP2", "location": {"x": p2[0], "y": p2[1], "z": DECK_Z[1]}, "facing": 0, "pool": "team_a"},
        {"id": "SP3", "location": {"x": mxx(p2[0]), "y": p2[1], "z": DECK_Z[1]}, "facing": 180, "pool": "team_b"},
        {"id": "SP4", "location": {"x": mxx(p1[0]), "y": p1[1], "z": DECK_Z[1]}, "facing": 180, "pool": "team_b"},
        {"id": "SP5", "location": {"x": round(Wm*0.36,1), "y": round(Hm*0.32,1), "z": 0}, "facing": 45, "pool": "neutral"},
        {"id": "SP6", "location": {"x": round(Wm*0.36,1), "y": round(Hm*0.68,1), "z": 0}, "facing": -45, "pool": "neutral"},
        {"id": "SP7", "location": {"x": mxx(round(Wm*0.36,1)), "y": round(Hm*0.68,1), "z": 0}, "facing": -135, "pool": "neutral"},
        {"id": "SP8", "location": {"x": mxx(round(Wm*0.36,1)), "y": round(Hm*0.32,1), "z": 0}, "facing": 135, "pool": "neutral"},
    ]
    for s in spawns:
        s["scoring_hints"] = {"min_dist_to_combat_m": 12, "last_used_cooldown_s": 8}

    gps = [
        {"id": "GP1", "location": {"x": round(bx - 2, 1), "y": round(by, 1), "z": 4}, "serves": "Bridge (west lip)", "approach": "Arena 1 ground", "notes": "bridge west lip"},
        {"id": "GP2", "location": {"x": round(bx + 2, 1), "y": round(by, 1), "z": 4}, "serves": "Bridge (east lip)", "approach": "Arena 2 ground", "notes": "bridge east lip"},
        {"id": "GP3", "location": {"x": round(Wm*0.30,1), "y": round(Hm*0.5,1), "z": 4}, "serves": "Base 1 upper lip", "approach": "Arena 1 ground", "notes": "base front lip"},
        {"id": "GP4", "location": {"x": mxx(round(Wm*0.30,1)), "y": round(Hm*0.5,1), "z": 4}, "serves": "Base 2 upper lip", "approach": "Arena 2 ground", "notes": "base front lip (mirror)"},
    ]

    manifest = {
        "arena_id": "breachpoint_aquarius",
        "manifest_version": 3,
        "units": "meters",
        "schema_note": "PER-FLOOR TRACED from the founder's reference set (15 images, "
                       "docs/design/reference/): ground + upper overheads traced separately "
                       "by trace_aquarius.py, combined by gen_aquarius_manifest.py. "
                       "footprints[] authoritative. STUDY RECREATION of 343's Aquarius.",
        "bounds": {"x": Wm, "y": Hm, "z": 10},
        "rocket_node": rocket,
        "spawn_points": spawns,
        "spawn_pools_note": "Base pairs tucked on each base's upper floor (their own traced "
                            "region); four scored neutrals on the arena floors.",
        "landmarks": lm,
        "grapple_points": gps,
        "grapple_note": "BN29 adaptation: the Grappleshot (20 m) stands in for thruster/"
                        "clamber mobility. Anchors on the bridge and base-front lips; stairs "
                        "and the traced ramps stay the primary routes.",
        "cover": [],
        "cover_note": "The traced solids are the cover; loose crates return via channel (b).",
        "sightlines": {
            "max_length_m": 48,
            "cap_ruling": "R45",
            "notes": "R45 (DESIGN-RULINGS.md): one-to-one supersedes the GDD 35 m cap for "
                     "this map only - Aquarius' documented end-to-end base lines. Every "
                     "line past 35 m surfaces as SIGHTLINE_PAIR_RULED naming the ruling."
        },
        "hazards": [
            "The bridge and the upper ring carry no rails where the reference shows none.",
            "Hallway 1's crossing threads between the deck supports at ground - blind at both mouths."
        ],
        "doubts": [
            "Scale: long axis 52 m DERIVED; a Forge walk (channel b) replaces it.",
            "The sunken power-up trench (shots 1/6) is below-grade geometry a top-down "
            "cannot bound; flattened to floor level in this blockout, owed to channel (b).",
            "Upper decks float (pillar positions unknowable from overheads); the shots "
            "show octagonal pillars - terminal adds supports during the editor pass.",
            "Interior doorways under decks are open at their edges pending the walk."
        ],
        "landing_note": "BN29 v3: per-floor one-to-one. Skeleton per the founder's own "
                        "labels - Arena 1/2, Hallway 1 center E-W crossing, Hallways 2/3 "
                        "north/south, Base 1/2, BRIDGE north-south. THE LAYOUT IS THE LAYOUT."
    }
    OUT.write_text(json.dumps(manifest, indent=1), encoding="utf-8")
    n = sum(len(l["footprints"]) for l in lm)
    print("wrote %s: %gx%g m, %d landmarks, %d boxes (ring %d / towers %d / supports %d / upper %d / stairs %d cells)"
          % (OUT.relative_to(GAME), Wm, Hm, len(lm), n, len(ring), len(tower), len(support), len(upper_walk), len(stairs)))


if __name__ == "__main__":
    main()
