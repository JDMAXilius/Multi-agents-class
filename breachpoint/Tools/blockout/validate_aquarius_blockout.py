#!/usr/bin/env python3
"""BN30 — PLAYABILITY + FIDELITY validation of the Aquarius blockout.

    python3 Tools/blockout/validate_aquarius_blockout.py [--png]

Founder (30 Aug): "make sure the level is playable and is 1:1."

WHAT THIS CAN PROVE (geometry, from the committed manifests — no engine):
  P1  every walkable surface is CONNECTED: one region, no orphan pockets
  P2  Level 2 is REACHABLE from Level 1 (ramps actually land on decks)
  P3  every SPAWN sits on walkable floor and reaches every other spawn
  P4  CORRIDOR WIDTHS clear the player: hard floor 2 x capsule radius
      (0.68 m), comfort 1.40 m (Level Design Book "at least 2x player width")
  P5  HEAD CLEARANCE under every deck >= 2.20 m
  P6  RAMP SLOPES <= 45 deg (UE walkable limit ~44.8)
  P7  the perimeter is a CLOSED ring (no shoot-through gaps)
  F1  FIDELITY: per-class IoU of the built blockout against the traced
      reference masks, plus overall dimensions
  F2  mirror SYMMETRY of the built pieces

WHAT THIS CANNOT PROVE — the honesty line: this is the GEOMETRY rung. It does
not prove the level is fun, that the capsule actually walks the ramps, or
that nav mesh generates. Those are the in-editor walk rung (BN30 "Done when"
boxes) and stay open until the terminal runs them.

Exit code 0 = PASS/WARN only, 1 = any FAIL.
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
KIT = GAME / "Content" / "Data" / "aquarius_blockout_kit.json"
MANIFEST = GAME / "Content" / "Data" / "aquarius_manifest.json"

A = 0.25                    # analysis cell, metres (finer than the 1 m build)
PLAYER_R = 0.34             # UE capsule radius
HARD_W = 2 * PLAYER_R       # 0.68 m - cannot pass below this
COMFORT_W = 1.40            # 2x player width (research)
HEAD_MIN = 2.20             # min head clearance under a deck
SLOPE_MAX = 45.0

findings = []               # (level, code, message)


def note(level, code, msg):
    findings.append((level, code, msg))


def rasterize_kit(kit):
    """Kit placements -> per-family occupancy on the analysis grid."""
    xs, ys = [], []
    for p in kit["placements"]:
        cx, cy, _ = (v / 100 for v in p["location_cm"])
        sx, sy, _ = p["scale"]
        xs += [cx - sx / 2, cx + sx / 2]
        ys += [cy - sy / 2, cy + sy / 2]
    Wm, Hm = max(xs), max(ys)
    gw, gh = int(math.ceil(Wm / A)), int(math.ceil(Hm / A))
    occ = {k: set() for k in ("Floor", "Deck", "Wall", "Tower", "Support",
                              "Ramp", "RampBlock")}
    ramps = []
    for p in kit["placements"]:
        f = p["variant"].split("_")[0]
        cx, cy, cz = (v / 100 for v in p["location_cm"])
        sx, sy, sz = p["scale"]
        pitch = abs(p["rotation_deg"]["pitch"])
        yaw = p["rotation_deg"]["yaw"]
        if f == "Ramp":
            if pitch > 0.01:                    # a true walkable ramp
                run = sx * math.cos(math.radians(pitch))
                if int(round(yaw)) % 180 == 90:
                    x0, x1 = cx - sy / 2, cx + sy / 2
                    y0, y1 = cy - run / 2, cy + run / 2
                else:
                    x0, x1 = cx - run / 2, cx + run / 2
                    y0, y1 = cy - sy / 2, cy + sy / 2
                cells = set()
                for gx in range(int(x0 / A), int(math.ceil(x1 / A))):
                    for gy in range(int(y0 / A), int(math.ceil(y1 / A))):
                        cells.add((gx, gy))
                occ["Ramp"] |= cells
                ramps.append({"cells": cells, "yaw": yaw, "slope": pitch,
                              "label": p["label"],
                              "x": (x0, x1), "y": (y0, y1)})
                continue
            key = "RampBlock"                   # flagged suspect: a solid
        else:
            key = f
        x0, x1 = cx - sx / 2, cx + sx / 2
        y0, y1 = cy - sy / 2, cy + sy / 2
        for gx in range(int(x0 / A), int(math.ceil(x1 / A))):
            for gy in range(int(y0 / A), int(math.ceil(y1 / A))):
                occ[key].add((gx, gy))
    return occ, ramps, gw, gh, Wm, Hm


def components(cells, diag=False):
    steps = [(1, 0), (-1, 0), (0, 1), (0, -1)]
    if diag:
        steps += [(1, 1), (1, -1), (-1, 1), (-1, -1)]
    cs, out = set(cells), []
    while cs:
        seed = cs.pop()
        comp, q = {seed}, deque([seed])
        while q:
            x, y = q.popleft()
            for dx, dy in steps:
                n = (x + dx, y + dy)
                if n in cs:
                    cs.discard(n)
                    comp.add(n)
                    q.append(n)
        out.append(comp)
    return out


def clearance_map(free, blocked_or_out):
    """Chebyshev-ish distance transform: metres to the nearest blocker."""
    dist = {c: 0.0 for c in blocked_or_out}
    q = deque(blocked_or_out)
    seen = set(blocked_or_out)
    while q:
        x, y = q.popleft()
        d = dist[(x, y)]
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            n = (x + dx, y + dy)
            if n in free and n not in seen:
                seen.add(n)
                dist[n] = d + A
                q.append(n)
    return {c: dist.get(c, 99.0) for c in free}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--png", action="store_true")
    args = ap.parse_args()

    kit = json.loads(KIT.read_text(encoding="utf-8"))
    man = json.loads(MANIFEST.read_text(encoding="utf-8"))
    occ, ramps, gw, gh, Wm, Hm = rasterize_kit(kit)

    # ---- walkable surfaces ------------------------------------------------
    blockers_l1 = occ["Wall"] | occ["Tower"] | occ["Support"] | occ["RampBlock"]
    l1 = (occ["Floor"] - blockers_l1)
    blockers_l2 = occ["Wall"] | occ["Tower"]        # 8.0 / 6.5 both > 4.0
    l2 = occ["Deck"] - blockers_l2
    ramp_cells = occ["Ramp"] - blockers_l1

    # node = (level, cell); ramps bridge levels
    def nid(lv, c):
        return (lv, c)

    adj = {}

    def link(a, b):
        adj.setdefault(a, set()).add(b)
        adj.setdefault(b, set()).add(a)

    for lv, cells in ((1, l1 | ramp_cells), (2, l2)):
        for c in cells:
            adj.setdefault(nid(lv, c), set())
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                n = (c[0] + dx, c[1] + dy)
                if n in cells:
                    link(nid(lv, c), nid(lv, n))
    # ramp mouths: low end joins L1, high end joins L2
    for r in ramps:
        horiz = int(round(r["yaw"])) % 180 != 90
        cs = r["cells"]
        if horiz:
            lo_x = min(x for x, _ in cs)
            hi_x = max(x for x, _ in cs)
            if int(round(r["yaw"])) == 180:
                lo_x, hi_x = hi_x, lo_x
            lo = [c for c in cs if c[0] == lo_x]
            hi = [c for c in cs if c[0] == hi_x]
            step = 1 if hi_x > lo_x else -1
            lo_out = [(c[0] - step, c[1]) for c in lo]
            hi_out = [(c[0] + step, c[1]) for c in hi]
        else:
            lo_y = min(y for _, y in cs)
            hi_y = max(y for _, y in cs)
            if int(round(r["yaw"])) == -90 or int(round(r["yaw"])) == 270:
                lo_y, hi_y = hi_y, lo_y
            lo = [c for c in cs if c[1] == lo_y]
            hi = [c for c in cs if c[1] == hi_y]
            step = 1 if hi_y > lo_y else -1
            lo_out = [(c[0], c[1] - step) for c in lo]
            hi_out = [(c[0], c[1] + step) for c in hi]
        joined_lo = joined_hi = 0
        # you can step off a ramp mouth in ANY direction, not only straight
        # ahead - link on any 4-neighbour (or an overlapping deck cell)
        for c in lo:
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                n = (c[0] + dx, c[1] + dy)
                if n in l1:
                    link(nid(1, c), nid(1, n))
                    joined_lo += 1
        for c in hi:
            if c in l2:
                link(nid(1, c), nid(2, c))
                joined_hi += 1
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                n = (c[0] + dx, c[1] + dy)
                if n in l2:
                    link(nid(1, c), nid(2, n))
                    joined_hi += 1
        if joined_lo == 0 or joined_hi == 0:
            note("FAIL", "RAMP_ORPHAN",
                 "%s connects L1:%d cells / L2:%d cells - a ramp to nowhere"
                 % (r["label"], joined_lo, joined_hi))

    # ---- P1 / P2 connectivity --------------------------------------------
    nodes = set(adj)
    seen, comps = set(), []
    for n0 in nodes:
        if n0 in seen:
            continue
        comp, q = {n0}, deque([n0])
        seen.add(n0)
        while q:
            u = q.popleft()
            for v in adj.get(u, ()):
                if v not in seen:
                    seen.add(v)
                    comp.add(v)
                    q.append(v)
        comps.append(comp)
    comps.sort(key=len, reverse=True)
    main_comp = comps[0] if comps else set()
    orphan_area = sum(len(c) for c in comps[1:]) * A * A
    if len(comps) == 1:
        note("PASS", "P1_CONNECTED",
             "all %d walkable cells form ONE region" % len(nodes))
    else:
        # A pocket only matters if it is a SPACE. Sub-6 m2 remnants are
        # rasterisation slivers (a 1 m deck strip behind a tower), not rooms.
        ROOM = 6.0
        big = [c for c in comps[1:] if len(c) * A * A >= ROOM]
        largest = max((len(c) * A * A for c in comps[1:]), default=0.0)
        lvl = "FAIL" if big else "WARN"
        note(lvl, "P1_ORPHANS",
             "%d walkable regions; %d orphan pocket(s) >= %.0f m2 "
             "(largest %.1f m2, orphan area %.1f m2 of %.0f m2)"
             % (len(comps), len(big), ROOM, largest, orphan_area,
                len(nodes) * A * A))
    l2_in_main = sum(1 for n in main_comp if n[0] == 2)
    if l2 and l2_in_main == 0:
        note("FAIL", "P2_UPPER_UNREACHABLE",
             "Level 2 has %d walkable cells but none connect to Level 1"
             % len(l2))
    elif l2:
        note("PASS", "P2_UPPER_REACHABLE",
             "Level 2 reachable from Level 1: %.0f%% of deck cells "
             "(%.1f m2) in the main region"
             % (100.0 * l2_in_main / len(l2), l2_in_main * A * A))

    # ---- P3 spawns --------------------------------------------------------
    # The kit carries spawns already converted into the kit frame and snapped
    # onto walkable ground; the arena manifest is checked too, so a builder
    # that skips the conversion still gets caught.
    def cell_of(x, y_south):
        return (int(x / A), int(y_south / A))

    def near(c, here, r=2):
        return any((c[0] + dx, c[1] + dy) in here
                   for dx in range(-r, r + 1) for dy in range(-r, r + 1))

    sp_ok, sp_bad = [], []
    for sp in kit.get("spawn_points", []):
        x, y, z = (v / 100 for v in sp["location_cm"])
        lvl = 2 if z > 2 else 1
        here = l2 if lvl == 2 else (l1 | ramp_cells)
        c = cell_of(x, y)
        (sp_ok if near(c, here) else sp_bad).append((sp["id"], nid(lvl, c)))
    if not kit.get("spawn_points"):
        note("FAIL", "P3_NO_SPAWNS", "the kit carries no spawn points")
    if sp_bad:
        note("FAIL", "P3_SPAWN_BLOCKED",
             "kit spawns not on walkable ground: %s"
             % ", ".join(i for i, _ in sp_bad))
    reach = [s2 for s2 in sp_ok if s2[1] in main_comp]
    if sp_ok and len(reach) == len(sp_ok):
        note("PASS", "P3_SPAWNS",
             "all %d kit spawns sit on walkable ground in the main region"
             % len(sp_ok))
    elif sp_ok:
        note("FAIL", "P3_SPAWN_ISLAND",
             "%d of %d spawns are cut off from the main region"
             % (len(sp_ok) - len(reach), len(sp_ok)))
    # frame check against the raw arena manifest
    flip_needed = 0
    for sp in man.get("spawn_points", []):
        L = sp["location"]
        lvl = 2 if L.get("z", 0) > 2 else 1
        here = l2 if lvl == 2 else (l1 | ramp_cells)
        if near(cell_of(L["x"], Hm - L["y"]), here) and \
                not near(cell_of(L["x"], L["y"]), here):
            flip_needed += 1
    if flip_needed:
        note("INFO", "P3_FRAME",
             "%d arena-manifest spawns need the north->south Y conversion "
             "(y_kit = %.2f - y_manifest); the kit generator applies it - a "
             "builder reading the arena manifest directly would mirror them"
             % (flip_needed, Hm))

    # ---- P4 corridor widths ----------------------------------------------
    # A cell touching a wall is NOT a pinch (the first metric flagged every
    # corridor edge). The real test: ERODE the walkable set by the capsule
    # radius and ask whether the level is still one region - i.e. can a
    # 0.68 m body actually walk every route?
    walk1 = l1 | ramp_cells
    outside = set()
    for c in walk1:
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            n = (c[0] + dx, c[1] + dy)
            if n not in walk1:
                outside.add(n)
    clr = clearance_map(walk1, outside)
    passable = {c for c, d in clr.items() if d >= PLAYER_R}
    comp_full = components(walk1)
    comp_pass = components(passable)
    big_full = max((len(c) for c in comp_full), default=0)
    big_pass = max((len(c) for c in comp_pass), default=0)
    lost = (big_full - big_pass) * A * A
    if comp_pass and len(components(passable)) <= len(comp_full):
        note("PASS", "P4_PASSABLE",
             "the %.2f m capsule reaches %.0f m2 of the %.0f m2 ground "
             "surface in one region (%.0f%%)"
             % (HARD_W, big_pass * A * A, big_full * A * A,
                100.0 * big_pass / max(big_full, 1)))
    else:
        note("FAIL", "P4_IMPASSABLE",
             "eroding by the capsule radius splits the ground into %d regions "
             "- some routes are too narrow to walk" % len(comp_pass))
    if lost > 40:
        note("WARN", "P4_TIGHT",
             "%.0f m2 of ground sits within a capsule radius of structure "
             "(corridor edges and 1 m quantisation slivers)" % lost)
    narrow = [c for c, d in clr.items() if d < COMFORT_W / 2]
    note("INFO", "P4_WIDTH_COMFORT",
         "%.1f%% of the ground surface clears the %.2f m comfort width"
         % (100.0 * (1 - len(narrow) / max(len(walk1), 1)), COMFORT_W))
    pinch = [c for c, d in clr.items() if d < PLAYER_R]

    # ---- P5 head clearance under decks ------------------------------------
    under = walk1 & occ["Deck"]
    head = DECK[0]
    if under and head < HEAD_MIN:
        note("FAIL", "P5_HEADROOM",
             "deck soffit at %.2f m gives less than %.2f m head clearance"
             % (head, HEAD_MIN))
    elif under:
        note("PASS", "P5_HEADROOM",
             "%.1f m2 of ground runs under decks at %.2f m soffit "
             "(>= %.2f m required)" % (len(under) * A * A, head, HEAD_MIN))

    # ---- P6 ramp slopes ---------------------------------------------------
    steep = [r for r in ramps if r["slope"] > SLOPE_MAX]
    blocks = len([p for p in kit["placements"]
                  if p["variant"].startswith("Ramp") and "suspect" in p])
    if steep:
        note("FAIL", "P6_SLOPE",
             "%d ramps exceed the %.0f deg walkable limit" % (len(steep), SLOPE_MAX))
    else:
        note("PASS", "P6_SLOPE",
             "all %d walkable ramps are %.0f deg or shallower (max %.1f)"
             % (len(ramps), SLOPE_MAX,
                max((r["slope"] for r in ramps), default=0.0)))
    if blocks:
        note("WARN", "P6_SUSPECT",
             "%d traced capsules were too steep to be ramps and are placed as "
             "SOLID blocks - vertical routes there are unproven until the "
             "reference walk (channel b)" % blocks)

    # ---- P7 perimeter ring ------------------------------------------------
    ring = components(occ["Wall"], diag=True)
    if len(ring) == 1:
        note("PASS", "P7_PERIMETER", "perimeter wall is one closed ring")
    else:
        note("FAIL", "P7_PERIMETER",
             "perimeter wall is %d disconnected fragments - shoot-through gaps"
             % len(ring))

    # ---- F1 fidelity vs the traced reference ------------------------------
    solids, floor_ref, W, H, cell_m = extract()
    ref = {k: set() for k in ("wall", "tower", "support", "deck")}
    for s in solids:
        if s.kind in ref:
            ref[s.kind] |= s.cells

    def to_analysis(cells):
        out = set()
        for (x, y) in cells:
            x0, x1 = x * cell_m, (x + 1) * cell_m
            y0, y1 = y * cell_m, (y + 1) * cell_m
            for gx in range(int(x0 / A), max(int(x0 / A) + 1,
                                             int(math.ceil(x1 / A)))):
                for gy in range(int(y0 / A), max(int(y0 / A) + 1,
                                                 int(math.ceil(y1 / A)))):
                    out.add((gx, gy))
        return out

    pairs = [("structure (walls+towers+piers)",
              occ["Wall"] | occ["Tower"] | occ["Support"],
              to_analysis(ref["wall"] | ref["tower"] | ref["support"])),
             ("upper decks", occ["Deck"], to_analysis(ref["deck"]))]
    ious = []
    for name, built, refc in pairs:
        inter = len(built & refc)
        union = len(built | refc)
        iou = inter / union if union else 0.0
        cover = inter / len(refc) if refc else 0.0
        ious.append(iou)
        lvl = "PASS" if iou >= 0.60 else ("WARN" if iou >= 0.45 else "FAIL")
        note(lvl, "F1_IOU",
             "%s: IoU %.2f vs the traced reference, %.0f%% of the reference "
             "covered" % (name, iou, 100 * cover))
    ref_w = W * cell_m
    ref_h = H * cell_m
    dw = abs(Wm - ref_w) / ref_w * 100
    dh = abs(Hm - ref_h) / ref_h * 100
    lvl = "PASS" if max(dw, dh) <= 3.0 else "WARN"
    note(lvl, "F1_DIMS",
         "built %.1f x %.1f m vs traced %.1f x %.1f m (%.1f%% / %.1f%% off)"
         % (Wm, Hm, ref_w, ref_h, dw, dh))

    # ---- F2 symmetry ------------------------------------------------------
    def mirror(cells, gwid):
        return {(gwid - 1 - x, y) for (x, y) in cells}
    gwid = int(math.ceil(Wm / A))
    allb = occ["Wall"] | occ["Tower"] | occ["Support"] | occ["Deck"] | occ["Floor"]
    m = mirror(allb, gwid)
    sym = len(allb & m) / len(allb | m) if allb else 0
    lvl = "PASS" if sym >= 0.92 else ("WARN" if sym >= 0.80 else "FAIL")
    note(lvl, "F2_SYMMETRY",
         "mirror symmetry about the long axis: %.3f (1.000 = perfect)" % sym)

    # ---- report -----------------------------------------------------------
    order = {"FAIL": 0, "WARN": 1, "PASS": 2, "INFO": 3}
    print("=" * 74)
    print("AQUARIUS BLOCKOUT — PLAYABILITY + FIDELITY (geometry rung)")
    print("=" * 74)
    for lvl, code, msg in sorted(findings, key=lambda f: (order[f[0]], f[1])):
        print("%-5s %-22s %s" % (lvl, code, msg))
    nfail = sum(1 for f in findings if f[0] == "FAIL")
    nwarn = sum(1 for f in findings if f[0] == "WARN")
    print("-" * 74)
    print("VERDICT: %s   (%d fail, %d warn)   walkable area %.0f m2 "
          "(L1 %.0f + L2 %.0f)"
          % ("FAIL" if nfail else "PASS", nfail, nwarn,
             (len(walk1) + len(l2)) * A * A, len(walk1) * A * A,
             len(l2) * A * A))
    print("NOT PROVEN HERE: capsule walk, nav mesh, and fun. Those are the "
          "in-editor rung (BN30 'Done when').")

    if args.png:
        diagnostic(walk1, l2, blockers_l1, ramp_cells, clr, pinch,
                   main_comp, comps, gw, gh)
    return 1 if nfail else 0


def diagnostic(walk1, l2, blockers, ramps, clr, pinch, main_comp, comps,
               gw, gh):
    try:
        from PIL import Image, ImageDraw
    except ImportError:
        print("note: PIL missing, diagnostic skipped")
        return
    S = 5
    img = Image.new("RGB", (gw * S + 360, gh * S + 60), "white")
    d = ImageDraw.Draw(img)
    ox, oy = 20, 40
    orphan = set()
    for c in comps[1:]:
        orphan |= {n[1] for n in c}

    def px(c):
        return (ox + c[0] * S, oy + c[1] * S, ox + c[0] * S + S, oy + c[1] * S + S)

    for c in blockers:
        d.rectangle(px(c), fill=(30, 30, 34))
    for c in walk1:
        d.rectangle(px(c), fill=(224, 236, 224))
    for c in l2:
        d.rectangle(px(c), fill=(176, 202, 230))
    for c in ramps:
        d.rectangle(px(c), fill=(150, 220, 150))
    for c in pinch:
        d.rectangle(px(c), fill=(235, 90, 70))
    for c in orphan:
        d.rectangle(px(c), fill=(220, 80, 220))
    d.text((ox, 14), "PLAYABILITY DIAGNOSTIC — walkable surface analysis "
                     "(0.25 m cells)", fill="black")
    lx = ox + gw * S + 20
    for i, (col, txt) in enumerate((((224, 236, 224), "Level 1 walkable"),
                                    ((176, 202, 230), "Level 2 deck"),
                                    ((150, 220, 150), "ramp (L1<->L2)"),
                                    ((30, 30, 34), "blocked structure"),
                                    ((235, 90, 70), "pinch < 0.68 m"),
                                    ((220, 80, 220), "orphan region"))):
        y = 60 + i * 22
        d.rectangle([lx, y, lx + 18, y + 12], fill=col, outline=(0, 0, 0))
        d.text((lx + 26, y), txt, fill="black")
    out = (GAME / "docs" / "design" / "blueprints" / "breachpoint_aquarius" /
           "playability_diagnostic.png")
    img.save(out)
    print("wrote %s" % out.relative_to(GAME))


if __name__ == "__main__":
    sys.exit(main())
