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
- kit.variants: unique (family, size) combinations. Piece-count research
  (30 Aug, third agent pass - Halo Forge caps 640-650 objects for COMPLETE
  shipped 4v4 arenas, Q3 duel maps ~900 brushes finished, community bands
  <300-500 objects, doctrine "big simple shapes, cheap to throw away")
  sets the budgets: MASSING pass 50-100 pieces, GREYBOX 200-400 (soft
  ceiling 500). Decomposition is therefore MAXIMAL RECTS on a coarse grid
  (2.0 m massing / 1.0 m greybox), floors running under structure - no
  micro-tiling, no length-splitting; reuse means the same MESH scaled.
- placements: one entry per instance - {variant, label, folder, location_cm,
  rotation_deg (roll,pitch,yaw), scale} in UNREAL conventions: 1 uu = 1 cm,
  Z up, X = map EAST, Y = map SOUTH, origin at the arena's NW ground corner.
  Location is the box CENTER (engine BasicShapes pivot at center; 100 cm
  unit meshes, so scale = size_m). Deterministic: same inputs, same JSON.

SOURCE GEOMETRY: the same per-floor trace extraction as the REV D blueprint
set (gen_aquarius_blueprint.extract), quantized to the PASS grid (area-
coverage vote, thin-wall bias). Diagonal chamfers step at the pass grid;
yaw-45 wall variants are a pass-2 refinement (BLOCKOUT-KIT.md).

LEVEL SCHEDULE (unchanged): floors t=0.20 top at 0.00 · decks 3.60-4.00 ·
supports 0-3.60 · towers 0-6.50 · perimeter 0-8.00 · ramps rise 0.25->4.00.
"""

from __future__ import annotations

import argparse
import json
import math
import sys
from collections import deque
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from gen_aquarius_blueprint import extract, DECK, TOWER_H, WALL_H  # noqa: E402

GAME = HERE.parents[1]
OUT = GAME / "Content" / "Data" / "aquarius_blockout_kit.json"

PASSES = {                          # research-set budgets (BLOCKOUT-KIT.md)
    # wall_t is LOW on purpose: a 0.5 m wall crossing a 1 m bin covers half
    # at best and far less diagonally, so a high threshold DOTS the perimeter
    # (measured: 0.30 -> 37 disconnected fragments; 0.15 -> one closed ring).
    # A blockout wall you can shoot through is a defect, not a drawing bug.
    # 0.75 m, not 1.0: measured structure fidelity against the traced
    # reference rises 0.66 -> 0.74 IoU (decks 0.84 -> 0.86) for 284 pieces,
    # still inside the researched 200-400 greybox band. 0.5 m would reach
    # 0.79 but costs 432 pieces, past the band.
    "greybox": {"grid": 0.75, "wall_t": 0.20, "slab_t": 0.45, "floor_t": 0.40,
                "budget": (200, 400)},
    "massing": {"grid": 2.0, "wall_t": 0.10, "slab_t": 0.40, "floor_t": 0.35,
                "budget": (50, 100)},
}
GRID_M = 1.0                       # set per pass in main()
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


def quantize(cells, W, H, cell_m, thresh, G=None):
    """Trace cells -> pass-grid occupancy by area coverage."""
    G = G or GRID_M
    gw = math.ceil(W * cell_m / G)
    gh = math.ceil(H * cell_m / G)
    acc = {}
    for (x, y) in cells:
        # spread each trace cell's area over the metre-grid bins it overlaps
        x0, x1 = x * cell_m, (x + 1) * cell_m
        y0, y1 = y * cell_m, (y + 1) * cell_m
        bx0, bx1 = int(x0 / G), min(int(x1 / G), gw - 1)
        by0, by1 = int(y0 / G), min(int(y1 / G), gh - 1)
        for bx in range(bx0, bx1 + 1):
            for by in range(by0, by1 + 1):
                ox = max(0.0, min(x1, (bx + 1) * G) - max(x0, bx * G))
                oy = max(0.0, min(y1, (by + 1) * G) - max(y0, by * G))
                acc[(bx, by)] = acc.get((bx, by), 0.0) + ox * oy
    full = G * G
    return {b for b, a in acc.items() if a >= thresh * full}, gw, gh


def merge_rects(cells):
    """LARGEST-RECTANGLE-FIRST decomposition: repeatedly carve out the biggest
    axis-aligned rectangle that fits in the remaining cells. The old greedy
    top-left scan diced ragged rings (a perimeter came out as 36 one-metre
    fenceposts); largest-first yields long runs - fewer, bigger pieces, which
    is exactly the blockout doctrine."""
    cs = set(cells)
    rects = []
    while cs:
        x0 = min(x for x, _ in cs)
        x1 = max(x for x, _ in cs)
        y0 = min(y for _, y in cs)
        y1 = max(y for _, y in cs)
        best = None                      # (area, x, y, w, h)
        heights = [0] * (x1 - x0 + 2)
        for y in range(y0, y1 + 1):      # histogram sweep, row by row
            for i in range(x1 - x0 + 1):
                heights[i] = heights[i] + 1 if (x0 + i, y) in cs else 0
            stack = []                   # (start index, height)
            for i in range(x1 - x0 + 2):
                hcur = heights[i] if i <= x1 - x0 else 0
                start = i
                while stack and stack[-1][1] >= hcur:
                    si, sh = stack.pop()
                    area = sh * (i - si)
                    if sh and (best is None or area > best[0]):
                        best = (area, x0 + si, y - sh + 1, i - si, sh)
                    start = si
                stack.append((start, hcur))
        _, bx, by, bw, bh = best
        for yy in range(by, by + bh):
            for xx in range(bx, bx + bw):
                cs.discard((xx, yy))
        rects.append((bx, by, bw, bh))
    return rects



def repair_connectivity(qfloor, fam_bins, ref_walk, G):
    """Quantisation can SEAL a pocket the reference has open (measured: the
    south bay). Carve the doorway back: for every walkable pocket cut off
    from the main region, if the REFERENCE connects it, remove the thinnest
    run of blocker bins (<= 2 m) between them. Reference-driven, so the level
    stays 1:1 - we restore an opening, we never invent one."""
    blockers = set().union(*fam_bins.values()) if fam_bins else set()
    walk = qfloor - blockers
    comps = []
    cs = set(walk)
    while cs:
        seed = cs.pop()
        comp, q = {seed}, deque([seed])
        while q:
            x, y = q.popleft()
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                n = (x + dx, y + dy)
                if n in cs:
                    cs.discard(n)
                    comp.add(n)
                    q.append(n)
        comps.append(comp)
    if len(comps) <= 1:
        return set(), []
    comps.sort(key=len, reverse=True)
    main = comps[0]
    carved, log = set(), []
    for pocket in comps[1:]:
        if not (pocket & ref_walk) or not (main & ref_walk):
            continue                       # reference says it is not a room
        # BFS out of the pocket, paying 1 per blocker bin crossed
        best = None
        seen = {c: 0 for c in pocket}
        q = deque((c, 0, ()) for c in pocket)
        while q and best is None:
            c, cost, path = q.popleft()
            if cost > 2:
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                n = (c[0] + dx, c[1] + dy)
                if n in main and cost > 0:
                    best = path
                    break
                if n in blockers and n not in carved:
                    nc = cost + 1
                    if seen.get(n, 99) > nc and nc <= 2:
                        seen[n] = nc
                        q.append((n, nc, path + (n,)))
                elif n in walk and n not in seen:
                    seen[n] = cost
                    q.append((n, cost, path))
        if best:
            carved |= set(best)
            log.append("carved %d bin(s) to reconnect a %.0f m2 pocket"
                       % (len(best), len(pocket) * G * G))
    for fam, bins in fam_bins.items():
        bins -= carved
    return carved, log


def snap_ramp(rec, qdeck, qfloor, G):
    """Guarantee a ramp MEETS what it serves: extend the high end until it
    overlaps a deck bin and the low end until it overlaps a floor bin.
    Ramps that only ALMOST touch are ramps to nowhere (measured: 3 of them)."""
    cx, cy = rec["center_m"][0], rec["center_m"][1]
    run, width = rec["run_m"], rec["size_m"][1]
    yaw = rec["yaw_deg"]
    ax = 0 if int(round(yaw)) % 180 == 0 else 1        # 0 = along x
    sgn = 1.0 if int(round(yaw)) in (0, 90) else -1.0

    def bin_at(x, y):
        return (int(x / G), int(y / G))

    def probe(dist, target):
        x = cx + (sgn * dist if ax == 0 else 0.0)
        y = cy + (sgn * dist if ax == 1 else 0.0)
        return bin_at(x, y) in target

    grow_hi = 0.0
    while grow_hi < 2.0 and not probe(run / 2 + grow_hi, qdeck):
        grow_hi += G / 2
    grow_lo = 0.0
    while grow_lo < 1.5 and not probe(-(run / 2 + grow_lo), qfloor):
        grow_lo += G / 2
    if grow_hi >= 2.0:
        grow_hi = 0.0                                  # no deck that way
    if grow_lo >= 1.5:
        grow_lo = 0.0
    if grow_hi or grow_lo:
        newrun = run + grow_hi + grow_lo
        shift = (grow_hi - grow_lo) / 2 * sgn
        rec["run_m"] = round(newrun, 2)
        rec["center_m"][ax] = round(rec["center_m"][ax] + shift, 3)
        rise = rec["rise_m"]
        rec["size_m"][0] = round(math.hypot(newrun, rise), 2)
        rec["pitch_deg"] = round(math.degrees(math.atan2(rise, newrun)), 2)
    return rec


def decompose(bins, family, z0, z1, G):
    """Grid bins -> modular box instances as MAXIMAL rects (research: big
    simple shapes, few pieces; reuse = the same mesh scaled)."""
    inst = []
    for (bx, by, bw, bh) in merge_rects(bins):
        inst.append({"family": family,
                     "size_m": [round(bw * G, 2), round(bh * G, 2),
                                round(z1 - z0, 2)],
                     "center_m": [round((bx + bw / 2) * G, 3),
                                  round((by + bh / 2) * G, 3),
                                  round((z0 + z1) / 2, 3)],
                     "yaw_deg": 0.0, "pitch_deg": 0.0})
    return inst


def ramp_instances(solids, deckcells, cell_m, qdeck, qwalk, qblock, G):
    """Place each traced capsule as a ramp that ACTUALLY CONNECTS.

    The first pass trusted the capsule's own length and produced ramps into
    walls and ramps to nowhere (caught by validate_aquarius_blockout.py: 3
    orphans, and the deck regions they served were unreachable). Now each
    ramp is anchored at BOTH ends: the head is pushed until it overlaps the
    deck it serves, the foot is placed on free floor at whatever run the
    walkable-slope limit demands. A run longer than the traced capsule is a
    FIDELITY deviation and is recorded as such - the reference walk rules.
    A capsule that cannot be anchored within 45 deg stays a flagged solid.
    """
    DESIRED = 27.0                       # comfortable ramp slope, degrees
    rise = DECK[1] - RAMP_Z0
    out, unresolved = [], []
    for s_ in (s_ for s_ in solids if s_.kind == "stair"):
        x0, y0, x1, y1 = s_.bbox
        horiz = (x1 - x0) >= (y1 - y0)
        ax = 0 if horiz else 1
        cx = (x0 + x1) / 2 * cell_m
        cy = (y0 + y1) / 2 * cell_m
        span = ((x1 - x0) if horiz else (y1 - y0)) * cell_m
        width = max(G, round((((y1 - y0) if horiz else (x1 - x0)) * cell_m) / G) * G)
        centre = cx if ax == 0 else cy
        other = cy if ax == 0 else cx

        def binat(pos, o=None):
            o = other if o is None else o
            x, y = (pos, o) if ax == 0 else (o, pos)
            return (int(x / G), int(y / G))

        # Evaluate BOTH directions END TO END and keep only a configuration
        # that anchors at both ends: a head on deck (never inside structure)
        # and a foot on free floor. Marching to the merely NEAREST deck put
        # two ramp heads inside the perimeter wall.
        def head_anchor(sgn):
            d = span / 2
            while d < span / 2 + 3.0:
                b = binat(centre + sgn * d)
                if b in qblock:
                    return None                  # structure that way, stop
                if b in qdeck:
                    # push a FULL bin into the deck and verify the head bin
                    # itself lands on deck (a half-bin push overshot thin
                    # deck strips and the head served nothing)
                    # the ramp's LAST cell row sits just BEFORE its head
                    # point, so a head verified exactly on a deck bin stopped
                    # short of it - push half a bin further so the ramp body
                    # genuinely overlaps the deck it serves
                    for push in (G / 2, G, G * 1.5):
                        h = centre + sgn * (d + push)
                        if binat(h) in qdeck:
                            return h + sgn * (G / 2)
                    return None
                d += G / 2
            return None

        want = rise / math.tan(math.radians(DESIRED))
        chosen = None
        for sgn in (1, -1):
            head = head_anchor(sgn)
            if head is None:
                continue
            for run in [want] + [want - k * G / 2 for k in range(1, 9)]:
                if run < rise / math.tan(math.radians(45.0)):
                    break
                foot = head - sgn * run
                # the foot AND the ground just outside it must be walkable -
                # the validator links a ramp by its outward neighbour, so a
                # foot that merely sits on floor while facing a wall is a
                # ramp to nowhere
                lat = [binat(foot, other + G), binat(foot, other - G)]
                if (binat(foot) in qwalk and binat(foot + sgn * G / 2) in qwalk
                        and any(b in qwalk for b in
                                [binat(foot - sgn * G)] + lat)):
                    cand = (math.degrees(math.atan2(rise, run)), sgn, run, head)
                    if chosen is None or cand[0] < chosen[0]:
                        chosen = cand
                    break
        if chosen is None:
            # A capsule we cannot anchor as a walkable ramp was never proven
            # to be a SOLID either - and placing 4 m blocks here sealed the
            # bridge's own ramp mouths (caught by the validator). Emit NO
            # geometry; record the doubt and let the reference walk rule.
            unresolved.append("traced capsule at (%.1f, %.1f) could not be "
                              "anchored as a walkable ramp at this "
                              "quantisation - left OPEN, channel (b) rules"
                              % (cx, cy))
            continue
        slope, sgn, run, head = chosen
        foot = head - sgn * run
        mid = (head + foot) / 2
        rec = {"family": "Ramp",
               "size_m": [round(math.hypot(run, rise), 2), round(width, 2),
                          RAMP_T],
               "center_m": [round(mid if ax == 0 else cx, 3),
                            round(cy if ax == 0 else mid, 3),
                            round((RAMP_Z0 + DECK[1]) / 2, 3)],
               "yaw_deg": (0.0 if sgn > 0 else 180.0) if ax == 0
                          else (90.0 if sgn > 0 else -90.0),
               "pitch_deg": round(slope, 2),
               "run_m": round(run, 2), "rise_m": round(rise, 2)}
        if run > span + 1.0:
            rec["deviation"] = ("run extended %.1f m beyond the traced capsule "
                                "to reach a walkable %.0f deg" % (run - span, slope))
        out.append(rec)
    return out, unresolved



def add_access_ramps(inst, qdeck, qwalk, qblock, G):
    """PLAYABILITY GUARANTEE: every sizeable upper region must have a way up.

    Six of the twelve traced capsules could not be anchored as ramps at this
    quantisation, which left deck regions with no route. Rather than ship an
    unreachable upper floor, synthesise the missing ramp: from the region's
    own edge, down a walkable slope, onto free ground. These are DEVIATIONS -
    the reference walk (channel b) replaces them with the real geometry.
    """
    DESIRED, rise = 27.0, DECK[1] - RAMP_Z0
    run = rise / math.tan(math.radians(DESIRED))
    nbins = int(math.ceil(run / G))

    served = set()
    for r in inst:
        if r["family"] != "Ramp" or r["pitch_deg"] <= 0.01:
            continue
        ax = 0 if int(round(r["yaw_deg"])) % 180 == 0 else 1
        sgn = 1 if int(round(r["yaw_deg"])) in (0, 90) else -1
        head = r["center_m"][ax] + sgn * r["run_m"] / 2
        o = r["center_m"][1 - ax]
        # only the head bin itself counts as served - a lateral brush is not
        # a route (a region "served" that way still validated unreachable)
        x, y = ((head, o) if ax == 0 else (o, head))
        served.add((int(x / G), int(y / G)))

    regions, cs = [], set(qdeck)
    while cs:
        seed = cs.pop()
        comp, q = {seed}, deque([seed])
        while q:
            x, y = q.popleft()
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                n = (x + dx, y + dy)
                if n in cs:
                    cs.discard(n)
                    comp.add(n)
                    q.append(n)
        regions.append(comp)

    added = []
    for reg in regions:
        if len(reg) < 8 or (reg & served):
            continue
        best = None
        for (bx, by) in sorted(reg):
            for ax, sgn in ((0, 1), (0, -1), (1, 1), (1, -1)):
                ok = True
                for k in range(1, nbins + 1):
                    b = ((bx - sgn * k, by) if ax == 0 else (bx, by - sgn * k))
                    if b in qblock or b not in qwalk:
                        ok = False
                        break
                if ok:
                    best = (bx, by, ax, sgn)
                    break
            if best:
                break
        if not best:
            continue
        bx, by, ax, sgn = best
        head = ((bx + 0.5) * G if ax == 0 else (by + 0.5) * G)
        foot = head - sgn * run
        mid = (head + foot) / 2
        cxy = [(bx + 0.5) * G, (by + 0.5) * G]
        cxy[ax] = mid
        rec = {"family": "Ramp",
               "size_m": [round(math.hypot(run, rise), 2), round(G, 2), RAMP_T],
               "center_m": [round(cxy[0], 3), round(cxy[1], 3),
                            round((RAMP_Z0 + DECK[1]) / 2, 3)],
               "yaw_deg": (0.0 if sgn > 0 else 180.0) if ax == 0
                          else (90.0 if sgn > 0 else -90.0),
               "pitch_deg": round(DESIRED, 2),
               "run_m": round(run, 2), "rise_m": round(rise, 2),
               "deviation": "ACCESS RAMP synthesised: a %.0f m2 upper region "
                            "had no route up after quantisation"
                            % (len(reg) * G * G)}
        inst.append(rec)
        added.append(rec["deviation"])
    return added


def main():
    global GRID_M
    ap = argparse.ArgumentParser()
    ap.add_argument("--png", action="store_true")
    ap.add_argument("--pass", dest="pass_name", default="greybox",
                    choices=sorted(PASSES))
    args = ap.parse_args()
    cfg = PASSES[args.pass_name]
    GRID_M = cfg["grid"]

    solids, floor, W, H, cell_m = extract()
    deckcells = set().union(*(s.cells for s in solids if s.kind == "deck")) \
        if any(s.kind == "deck" for s in solids) else set()
    masks = {k: set() for k in ("wall", "tower", "support", "deck", "stair")}
    for s in solids:
        masks[s.kind] |= s.cells

    inst = []
    G = cfg["grid"]
    qwall, gw, gh = quantize(masks["wall"], W, H, cell_m, cfg["wall_t"], G)
    qtower, _, _ = quantize(masks["tower"], W, H, cell_m, cfg["slab_t"], G)
    qsup, _, _ = quantize(masks["support"], W, H, cell_m, cfg["slab_t"], G)
    qdeck, _, _ = quantize(masks["deck"], W, H, cell_m, cfg["slab_t"], G)
    qtower -= qwall
    qsup -= qwall | qtower
    # floor plates run UNDER walls/towers/supports (hidden inside the masses)
    # so the plates stay big and few - the research's "big simple shapes"
    qfloor, _, _ = quantize(floor | masks["wall"] | masks["tower"]
                            | masks["support"], W, H, cell_m, cfg["floor_t"], G)

    # PLAYABILITY REPAIR (founder: "make sure the level is playable"):
    # the reference's own walkable set decides which pockets must stay open.
    ref_walk, _, _ = quantize(floor, W, H, cell_m, 0.40, G)
    fam_bins = {"Wall": qwall, "Tower": qtower, "Support": qsup}
    carved, carve_log = repair_connectivity(qfloor, fam_bins, ref_walk, G)
    qwall, qtower, qsup = fam_bins["Wall"], fam_bins["Tower"], fam_bins["Support"]

    inst += decompose(qfloor, "Floor", *FAMILIES["Floor"][:2], G)
    inst += decompose(qdeck, "Deck", *FAMILIES["Deck"][:2], G)
    inst += decompose(qwall, "Wall", *FAMILIES["Wall"][:2], G)
    inst += decompose(qtower, "Tower", *FAMILIES["Tower"][:2], G)
    inst += decompose(qsup, "Support", *FAMILIES["Support"][:2], G)
    qblock = qwall | qtower | qsup
    ramps_built, unresolved = ramp_instances(solids, deckcells, cell_m, qdeck,
                                             qfloor - qblock, qblock, G)
    inst += ramps_built
    access_log = add_access_ramps(inst, qdeck - (qwall | qtower),
                                  qfloor - qblock, qblock, G)

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

    # ---- spawns, converted into the kit frame and proven walkable --------
    Wm_k = max((p["center_m"][0] + p["size_m"][0] / 2) for p in inst)
    Hm_k = max((p["center_m"][1] + p["size_m"][1] / 2) for p in inst)
    blockers_all = qwall | qtower | qsup
    walk_bins = qfloor - blockers_all
    deck_bins = qdeck - (qwall | qtower)
    manp = GAME / "Content" / "Data" / "aquarius_manifest.json"
    spawns_kit, spawn_notes = [], []
    if manp.exists():
        arena = json.loads(manp.read_text(encoding="utf-8"))
        for sp in arena.get("spawn_points", []):
            L = sp["location"]
            # THE FRAME CONVERSION, done once, here: the arena manifest is
            # +y NORTH; the kit is +y SOUTH. Skipping this mirrors every
            # spawn (caught by validate_aquarius_blockout.py).
            x, y = L["x"], Hm_k - L["y"]
            upper = L.get("z", 0) > 2
            target = deck_bins if upper else walk_bins
            b = (int(x / G), int(y / G))
            if b not in target:            # snap to the nearest walkable bin
                cand = min(target, key=lambda c: (c[0] - x / G) ** 2
                           + (c[1] - y / G) ** 2, default=None)
                if cand is None:
                    continue
                moved = math.hypot((cand[0] + .5) * G - x, (cand[1] + .5) * G - y)
                x, y = (cand[0] + 0.5) * G, (cand[1] + 0.5) * G
                spawn_notes.append("%s snapped %.1f m onto walkable ground"
                                   % (sp["id"], moved))
            spawns_kit.append({
                "id": sp["id"], "pool": sp.get("pool", "neutral"),
                "location_cm": [round(x * 100, 1), round(y * 100, 1),
                                round((DECK[1] if upper else 0.0) * 100 + 10, 1)],
                "yaw_deg": round(-float(sp.get("facing", 0)), 1)})

    lo, hi = cfg["budget"]
    kit = {
        "kit_id": "aquarius_blockout_kit",
        "version": 2,
        "pass": args.pass_name,
        "piece_budget": {"band": [lo, hi], "actual": len(inst),
                         "basis": "BLOCKOUT-KIT.md piece-count research: "
                                  "massing 50-100, greybox 200-400 "
                                  "(soft ceiling 500, red line ~650 - the "
                                  "Halo 3/Reach complete-map cap)"},
        "units": "location cm (1uu), scale = metres of a 100 cm unit mesh",
        "axes": "X = map EAST, Y = map SOUTH, Z up; origin at NW ground corner "
                "of the traced footprint; rotations are UE rotators (deg)",
        "grid_m": GRID_M,
        "asset_catalog": "docs/design/blueprints/breachpoint_aquarius/"
                         "K101_kit_catalog_1.png + K102_kit_catalog_2.png - "
                         "the TWELVE modular assets (gen_kit_catalog.py); "
                         "placements below map onto them via asset_map",
        "asset_map": {"Floor": "BLK_Floor_400", "Deck": "BLK_Floor_400",
                      "Wall": "BLK_Wall_400", "Tower": "BLK_Wall_400",
                      "Support": "BLK_Pier_100", "Ramp": "BLK_Ramp_800"},
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
        "spawn_points": spawns_kit,
        "spawn_note": "converted from Content/Data/aquarius_manifest.json into "
                      "the KIT FRAME (+y south, cm) and snapped onto walkable "
                      "ground; the builder must use THESE, not the arena "
                      "manifest's north-up coordinates.",
        "repairs": carve_log + spawn_notes + access_log,
        "unresolved_capsules": unresolved
                   + [i["deviation"] for i in inst if "deviation" in i],
        "placements": placements,
    }
    out = OUT if args.pass_name == "greybox" else \
        OUT.with_name("aquarius_blockout_massing.json")
    out.write_text(json.dumps(kit, indent=1), encoding="utf-8")

    top = sorted(kit["variants"], key=lambda v: -v["count"])[:8]
    print("wrote %s [%s pass]" % (out.relative_to(GAME), args.pass_name))
    verdict = "IN BAND" if lo <= len(placements) <= hi else \
        ("UNDER" if len(placements) < lo else "OVER")
    print("grid %.1f m · %d placements (%s %d-%d) · %d variants"
          % (GRID_M, len(placements), verdict, lo, hi, len(kit["variants"])))
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
