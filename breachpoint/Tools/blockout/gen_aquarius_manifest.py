#!/usr/bin/env python3
"""BN29 v2 — AQUARIUS one-to-one, generated FROM THE TRACED REFERENCE.

    python3 Tools/blockout/trace_aquarius.py       # first: image -> class grid
    python3 Tools/blockout/gen_aquarius_manifest.py

v1 built Aquarius from documented callout topology at derived proportions; the
founder rejected it against the real layout and supplied the map's actual
top-down (docs/design/reference/aquarius_thegamescabin.jpg — the same figure
source the Majmudar article used). v2 is built FROM that image: every box in
this manifest comes from the trace grid, so the silhouette, the four floor
tones, the tank columns, the ramp capsules and the base wings are the
reference's own shapes, not my invention. THE LAYOUT IS THE LAYOUT.

Pipeline: trace grid (0.25 m cells at the chosen scale) -> re-grid to 0.5 m ->
symmetrize about the map's own mirror line (the art is hand-drawn and a cell
or two off; the LEVEL must be exactly fair - left half wins, right half is its
mirror; logged adaptation) -> per class:
  BLACK  -> solid mass z[0,5] (the fish-tank columns, interior blocks, and the
            perimeter ring, which the image draws as its black outline)
  LIGHT  -> raised deck z[0,4] (bases, wings, protrusion interiors, Top Mid)
            ... EXCEPT small isolated light islands inside open floor, which
            are the image's RAMP capsules -> stair volumes z[0,4]
            ... and the center sliver between the tanks -> Top Mid bridge
            deck z[3.6,4] (walk-under below: the real bridge)
  MID/DARK -> open floor at ground (the image's two grays are a subtle grade
            the blockout flattens; recorded in the trace for later refinement)
Greedy rectangle merge turns each class mask into AABB footprints.

SCALE (still the one derived number): long axis = 52 m, from the close-range
brief; channel (b) (an in-game Forge walk) replaces it with a measurement
without changing anything else here.
"""

from __future__ import annotations

import json
from collections import deque
from pathlib import Path

GAME = Path(__file__).resolve().parents[2]
TRACE = GAME / "docs" / "design" / "reference" / "aquarius_trace.json"
OUT = GAME / "Content" / "Data" / "aquarius_manifest.json"

LONG_AXIS_M = 52.0


def main():
    t = json.loads(TRACE.read_text(encoding="utf-8"))
    rows = t["rows"]
    x0, y0, x1, y1 = t["bbox_cells"]

    # crop to bbox, re-grid 2x2 (0.5 m cells), majority-ish (any non-bg wins,
    # deepest class wins ties so thin walls survive)
    RANK = {"#": 4, "L": 3, "m": 2, "d": 2, ".": 0}
    W4 = x1 - x0 + 1
    m_per_c4 = LONG_AXIS_M / W4
    gw, gh = (W4 + 1) // 2, (y1 - y0 + 2) // 2
    cell_m = m_per_c4 * 2
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
            best = max(votes, key=lambda c: (votes[c], RANK[c]))
            nz = {c: n for c, n in votes.items() if c != "."}
            if best == "." and nz:
                best = max(nz, key=lambda c: (nz[c], RANK[c]))
                if sum(nz.values()) < 2:
                    best = "."
            g[gy][gx] = best

    # symmetrize: left half wins (mirror col from the map's own width)
    for gy in range(gh):
        for gx in range(gw // 2):
            g[gy][gw - 1 - gx] = g[gy][gx]

    # flip y so +y is north (image rows go down); classify light components
    g = g[::-1]

    def components(match):
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

    cx_mid = gw / 2
    light_comps = components(lambda c: c == "L")
    ramps, bridge, decks = [], [], []
    for comp in light_comps:
        xs = [x for _, x in comp]
        ys = [y for y, _ in comp]
        cx = sum(xs) / len(xs)
        cy = sum(ys) / len(ys)
        if len(comp) < 90 and abs(cx - cx_mid) < 6 / cell_m and abs(cy - gh / 2) < 6 / cell_m:
            bridge.append(comp)          # the sliver between the tanks: Top Mid
        elif len(comp) < 70:
            ramps.append(comp)           # capsule ramps and small steps
        else:
            decks.append(comp)           # bases, wings, protrusion interiors

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
        out = []
        for i, (x, y, w, h) in enumerate(merge_rects(cells)):
            out.append({"label": "%s_%02d" % (label, i),
                        "x": [round(x * cell_m, 2), round((x + w) * cell_m, 2)],
                        "y": [round(y * cell_m, 2), round((y + h) * cell_m, 2)],
                        "z": [z0, z1], "role": role})
        return out

    black = [(y, x) for y in range(gh) for x in range(gw) if g[y][x] == "#"]
    lm = []
    lm.append({"name": "Masses and Perimeter", "footprints":
               fps(black, 0, 7, "solid; traced BLACK: fish-tank columns, interior blocks, perimeter ring - FULL room height (a 5 m cap let the deck-to-deck eye line sail over the tanks; the real columns reach the ceiling)", "blk"),
               "purpose": "Every BLACK region of the traced reference as solid mass "
                          "x[0,%g] y[0,%g] overall; the outline ring is the arena wall." % (round(gw*cell_m,1), round(gh*cell_m,1))})
    deck_cells = [c for comp in decks for c in comp]
    lm.append({"name": "Upper Decks", "footprints":
               fps(deck_cells, 0, 4, "deck slab; traced LIGHT: bases, wings, protrusion interiors - raised one storey, walkable top", "dek"),
               "purpose": "Every large LIGHT region as a raised deck x[0,%g] y[0,%g] overall; "
                          "the two bases and their wings live here." % (round(gw*cell_m,1), round(gh*cell_m,1))})
    ramp_cells = [c for comp in ramps for c in comp]
    lm.append({"name": "Ramps", "footprints":
               fps(ramp_cells, 0, 4, "ground-to-mid stair volume; traced capsule - the image's ramp pills", "rmp"),
               "purpose": "Small isolated LIGHT capsules as stair volumes x[0,%g] y[0,%g] overall - "
                          "the reference draws its ramps as light pills inside the open floor." % (round(gw*cell_m,1), round(gh*cell_m,1))})
    bridge_cells = [c for comp in bridge for c in comp]
    if bridge_cells:
        lm.append({"name": "Top Mid", "footprints":
                   fps(bridge_cells, 3.6, 4, "deck slab; traced center sliver - the bridge between the tanks, walk-under below", "tmd"),
                   "purpose": "The LIGHT sliver between the center tank columns as the Top Mid "
                              "bridge deck x[0,%g] y[0,%g] overall; Aquarius' power position - "
                              "the ROCKET stands at bridge center (our slice's one power "
                              "pickup, in the camo/OS slot)." % (round(gw*cell_m,1), round(gh*cell_m,1))})

    Wm, Hm = round(gw * cell_m, 2), round(gh * cell_m, 2)
    mxx = lambda x: round(Wm - x, 2)

    # anchors from the trace's own features: bridge center + capsule tops + base fronts
    if bridge_cells:
        bx = sum(x for _, x in bridge_cells) / len(bridge_cells) * cell_m
        by = sum(y for y, _ in bridge_cells) / len(bridge_cells) * cell_m
    else:
        bx, by = Wm / 2, Hm / 2
    rocket = {"x": round(bx, 2), "y": round(by, 2), "z": 4, "elevation": "mid",
              "landmark": "Top Mid",
              "notes": "Bridge-deck center from the trace - Aquarius' camo/OS slot, taken by "
                       "BREACHPOINT's one power pickup (BN29 adaptation)."}

    # Spawns tuck into the WEST (covered) half of the base deck, the way real
    # Aquarius spawns sit inside the base rather than on its exposed front -
    # which is also what keeps every spawn PAIR line under the 35 m kickoff
    # gate while the map keeps its real long lines between non-spawn positions.
    west_decks = sorted(decks, key=lambda c: min(x for _, x in c))[:1]
    if west_decks:
        comp = west_decks[0]
        xmid = (min(x for _, x in comp) + max(x for _, x in comp)) / 2
        west = [(y, x) for (y, x) in comp if x <= xmid]
        corners = [min(west, key=lambda c: c[0] * 1000 + c[1]),
                   min(west, key=lambda c: -c[0] * 1000 + c[1]),
                   max(west, key=lambda c: c[0] * 1000 - c[1]),
                   min(west, key=lambda c: c[0] * 1000 - c[1])]
        best, bd = None, -1
        for i in range(len(corners)):
            for j in range(i + 1, len(corners)):
                (y1, x1), (y2, x2) = corners[i], corners[j]
                d = ((x1 - x2) ** 2 + (y1 - y2) ** 2) ** 0.5 * cell_m
                if d > bd:
                    bd, best = d, (corners[i], corners[j])
        (ay, ax), (by_, bx_) = best
        inset = 1.2 / cell_m
        cy_, cx_ = (ay + by_) / 2, (ax + bx_) / 2
        def pull(y, x):
            return (y + (cy_ - y) * 0.15 + (inset if y < cy_ else -inset) * 0,
                    x + (cx_ - x) * 0.15)
        (ay, ax), (by_, bx_) = pull(ay, ax), pull(by_, bx_)
        p1 = (round(ax * cell_m, 1), round(ay * cell_m, 1))
        p2 = (round(bx_ * cell_m, 1), round(by_ * cell_m, 1))
        assert ((p1[0]-p2[0])**2 + (p1[1]-p2[1])**2) ** 0.5 >= 8.0, "base spawn pair under 8 m"
    else:
        p1, p2 = (6, Hm * 0.4), (3, Hm * 0.6)
    spawns = [
        {"id": "SP1", "location": {"x": p1[0], "y": p1[1], "z": 4}, "facing": 0, "pool": "team_a"},
        {"id": "SP2", "location": {"x": p2[0], "y": p2[1], "z": 4}, "facing": 0, "pool": "team_a"},
        {"id": "SP3", "location": {"x": mxx(p2[0]), "y": p2[1], "z": 4}, "facing": 180, "pool": "team_b"},
        {"id": "SP4", "location": {"x": mxx(p1[0]), "y": p1[1], "z": 4}, "facing": 180, "pool": "team_b"},
        {"id": "SP5", "location": {"x": round(Wm*0.36,1), "y": round(Hm*0.26,1), "z": 0}, "facing": 45, "pool": "neutral"},
        {"id": "SP6", "location": {"x": round(Wm*0.36,1), "y": round(Hm*0.74,1), "z": 0}, "facing": -45, "pool": "neutral"},
        {"id": "SP7", "location": {"x": mxx(round(Wm*0.36,1)), "y": round(Hm*0.74,1), "z": 0}, "facing": -135, "pool": "neutral"},
        {"id": "SP8", "location": {"x": mxx(round(Wm*0.36,1)), "y": round(Hm*0.26,1), "z": 0}, "facing": 135, "pool": "neutral"},
    ]
    for s in spawns:
        s["scoring_hints"] = {"min_dist_to_combat_m": 12, "last_used_cooldown_s": 8}

    gps = [
        {"id": "GP1", "location": {"x": round(bx - 2, 1), "y": round(by, 1), "z": 4}, "serves": "Top Mid (west lip)", "approach": "west ground", "notes": "bridge west lip, from the trace"},
        {"id": "GP2", "location": {"x": round(bx + 2, 1), "y": round(by, 1), "z": 4}, "serves": "Top Mid (east lip)", "approach": "east ground", "notes": "bridge east lip"},
        {"id": "GP3", "location": {"x": round(Wm*0.25,1), "y": round(Hm*0.5,1), "z": 4}, "serves": "Yellow base deck lip", "approach": "east ground (the court)", "notes": "base front lip"},
        {"id": "GP4", "location": {"x": mxx(round(Wm*0.25,1)), "y": round(Hm*0.5,1), "z": 4}, "serves": "Blue base deck lip", "approach": "west ground (the court)", "notes": "base front lip (mirror)"},
    ]

    manifest = {
        "arena_id": "breachpoint_aquarius",
        "manifest_version": 2,
        "units": "meters",
        "schema_note": "TRACED from docs/design/reference/aquarius_thegamescabin.jpg "
                       "(founder-supplied, the article's own figure source) by "
                       "trace_aquarius.py; EMITTED by gen_aquarius_manifest.py. "
                       "footprints[] authoritative. STUDY RECREATION of 343's Aquarius.",
        "bounds": {"x": Wm, "y": Hm, "z": 10},
        "rocket_node": rocket,
        "spawn_points": spawns,
        "spawn_pools_note": "Base pairs on the base decks (z4, the traced light regions); "
                            "four scored neutrals on the courts' open floor.",
        "landmarks": lm,
        "grapple_points": gps,
        "grapple_note": "BN29 adaptation: our Grappleshot (20 m) stands in for Infinite's "
                        "thruster/clamber mobility; anchors on the traced deck lips, rises "
                        "one storey, stairs stay primary.",
        "cover": [],
        "cover_note": "None added: the traced solids ARE the cover (the reference's own "
                      "blocks). Cover crates return only if the terminal walk finds the "
                      "real map's loose crates.",
        "sightlines": {
            "max_length_m": 48,
            "cap_ruling": "R45",
            "notes": "R45 (DESIGN-RULINGS.md): one-to-one supersedes the GDD 35 m cap for "
                     "this map only. The traced geometry's own worst spawn-pair line is "
                     "47.65 m - Aquarius' documented end-to-end base sightlines, kept "
                     "because the layout is the layout. 48 m declared as the measured "
                     "ceiling; every line past 35 m surfaces as a SIGHTLINE_PAIR_RULED "
                     "warning naming the ruling."
        },
        "hazards": ["Top Mid bridge has no rails - the traced sliver is exactly as narrow "
                    "as the reference draws it."],
        "doubts": [
            "GDD RULING PENDING: the map's real (non-spawn) long lines run ~38-44 m, "
            "past the GDD 2.6 cap of 35 m - one-to-one supersedes per the BN29 founder "
            "directive, and the GDD amendment awaits the founder's signature.",
            "Scale: long axis assumed 52 m (derived) - a Forge walk (channel b) replaces it.",
            "The image's two gray tones (a subtle floor grade) are flattened to one ground "
            "plane in this blockout; the trace keeps the distinction for a later pass.",
            "Interior door openings under the decks cannot be read from a top-down: deck "
            "undersides are open at their edges in this blockout; terminal refines.",
        ],
        "landing_note": "BN29: THE LAYOUT IS THE LAYOUT - every box traced from the "
                        "founder-supplied reference; symmetrized about the map's own "
                        "mirror line (left half wins; hand-drawn art was a cell or two off)."
    }
    OUT.write_text(json.dumps(manifest, indent=1), encoding="utf-8")
    n = sum(len(l["footprints"]) for l in lm)
    print("wrote %s: bounds %gx%g m, %d landmarks, %d boxes, %d ramps comps, bridge %s" %
          (OUT.relative_to(GAME), Wm, Hm, len(lm), n, len(ramps), bool(bridge_cells)))


if __name__ == "__main__":
    main()
