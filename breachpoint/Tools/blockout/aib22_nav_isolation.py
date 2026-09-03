#!/usr/bin/env python3
"""AIB22 step 3(a) - the 3-piece NAV ISOLATION map for the coplanar-merge hypothesis.

    python3 Tools/blockout/aib22_nav_isolation.py --selftest   # geometry, no editor
    python3 Tools/blockout/aib22_nav_isolation.py --dry-run    # print the plan, touch nothing
    python3 Tools/blockout/aib22_nav_isolation.py              # build /Game/Maps/BR_NavIsolation
    python3 Tools/blockout/aib22_nav_isolation.py --into-gym   # fallback: same actors inside
                                                               # BR_MetricsGym at ORIGIN_GYM

THE QUESTION (BN34 defect 3, AIB22 W-AUDIT member 2): every Spillway ramp head (16) abuts its
deck at an EXACT 0.00 coplanar top; on Aquarius 6/8 such ramps failed to merge nav islands.
Recast's rcFilterLedgeSpans decides the junction inside one voxel row (CellHeight 10 uu by
default), so two separately-authored coincident faces can carve two islands that never merge.
This map holds the control and the proposed fix side by side, nothing else, so a nav build
answers the question in isolation:

    PAIR A  "Abut"  - the generator's CURRENT pattern (gen_spillway.ramp + deck):
                      ramp head x == deck x0 (0.00 gap), ramp top z == deck top (0.00 delta)
    PAIR B  "Fix"   - the architect's proposal: ramp head OVERLAPS 50 uu into the deck body,
                      ramp top SUNK 4 uu below the deck top. The visible ramp therefore
                      enters the deck's side face 29 uu below the deck top
                      (overlap * 1/2 + sink) - a step, not a coincident plane, and under the
                      35 uu AgentMaxStepHeight the nav agent walks.

Both ramps are 2:1 (400 rise over 800 run = 26.57 deg) and use the SAME seating code as
BR_Spillway - gen_spillway.ramp()/deck() are imported, not re-derived - and the same baked
meshes (SM_SPW_Ramp, SM_SPW_Deck). If pair A splits and pair B merges, the fix is a generator
change (law 7) and goes into gen_spillway.ramp(); nothing here is ever hand-seated.

COORDINATES (uu, +X east, +Y north, +Z up; every number below is the whole design)
    Floor       x -600..2000   y -600..2600   top z 0      (slab 100 thick, z -100..0)
    A ramp      x    0..800    y    0..600    base 0    rise 400  up +X  head x 800  z 400
    A deck      x  800..1800   y -200..800    top 400   (z 300..400)          gap 0  delta 0
    B ramp      x   50..850    y 1400..2000   base -4   rise 400  up +X  head x 850  z 396
    B deck      x  800..1800   y 1200..2200   top 400   (z 300..400)          overlap 50 sink 4
    Nav bounds  centre (700, 1000, 200)  size (3000, 3600, 1400)  -> z -500..900
    PlayerStart (-300, 300, 100)  facing +X (the foot of ramp A)
    Lights      DirectionalLight + SkyLight (a new_level/duplicated map ships with none - BN32)
The pairs are 600 uu apart in Y across open floor so a merge on one cannot leak to the other
through anything but the shared floor, which is exactly the leg the probe measures.

TRANSPORT: the editor's MCP server (127.0.0.1:8000/mcp, mcp-ui/gen_ui/mcp.py - the same
transport as Tools/bn/bn11_lib.py and land_spillway.py). MAP CREATION: no toolset exposes
new_level, and execute_tool_script forbids `import unreal`, so the map is made by
AssetTools.duplicate of BR_MetricsGym (the one script-made, non-World-Partition map in
Content/Maps; Spillway and Arena01 carry __ExternalActors__) with every BN32_MetricsGym-tagged
actor stripped. If duplicate refuses, --into-gym places the same actors inside BR_MetricsGym
at ORIGIN_GYM under this script's own tag; the probe script honours the same flag.

IDEMPOTENT: every actor is tagged AIB22_NavIsolation and a run destroys that tag first. The
map is saved by the script, then re-loaded, and the actor bounds are read back from that fresh
load and compared to the plan - a mismatch is a finding, not a warning.

Never opened by the metrics gym, the Spillway generator or anything else: one .uasset owner.
"""

import argparse
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(REPO, "mcp-ui", "gen_ui"))

import gen_spillway as G  # noqa: E402  the seating code under test, verbatim

MAP_PATH = "/Game/Maps/BR_NavIsolation"
TEMPLATE_MAP = "/Game/Maps/BR_MetricsGym"
TEMPLATE_TAG = "BN32_MetricsGym"          # what to strip out of the duplicate
TAG = "AIB22_NavIsolation"
FOLDER = "NavIsolation"
ORIGIN_GYM = (0.0, -6000.0, 0.0)          # --into-gym: clear of the gym's y -6..30 m lanes

SCENE = "editor_toolset.toolsets.scene.SceneTools"
ACTOR = "editor_toolset.toolsets.actor.ActorTools"
ASSET = "editor_toolset.toolsets.asset.AssetTools"
NAV_VOLUME = "/Script/NavigationSystem.NavMeshBoundsVolume"
PLAYER_START = "/Script/Engine.PlayerStart"
SUN = "/Script/Engine.DirectionalLight"
SKY = "/Script/Engine.SkyLight"

# --- the design ---------------------------------------------------------------
RISE, RUN, W = 400.0, 800.0, 600.0        # Spillway main ramp: T0->T1, RAMP_RUN, RAMP_W
DECK_TOP = 400.0
OVERLAP = 50.0                            # fix: head this far INSIDE the deck footprint
SINK = 4.0                                # fix: head top this far BELOW the deck top (3..5)
AGENT_MAX_STEP = 35.0                     # RecastNavMesh AgentMaxStepHeight, engine default
                                          # (Config has no override; BN21/BN34 read 35 live)
FLOOR = (-600.0, -600.0, 2000.0, 2600.0)  # x0 y0 x1 y1, top 0
DECK_X0, DECK_X1 = 800.0, 1800.0
NAV_CENTRE, NAV_SIZE = (700.0, 1000.0, 200.0), (3000.0, 3600.0, 1400.0)
START = (-300.0, 300.0, 100.0)

PAIRS = [
    # name, ramp y0, deck y0, ramp base, ramp x0, ramp x1
    {"name": "A_Abut", "ramp_y0": 0.0, "deck_y0": -200.0,
     "base": 0.0, "x0": DECK_X0 - RUN, "x1": DECK_X0},
    {"name": "B_Fix", "ramp_y0": 1400.0, "deck_y0": 1200.0,
     "base": -SINK, "x0": DECK_X0 + OVERLAP - RUN, "x1": DECK_X0 + OVERLAP},
]


def plan():
    """Every actor, in uu, through gen_spillway's own primitives. Pure Python."""
    del G._elements[:]
    G.deck(*FLOOR, 0.0, "NI_Floor", FOLDER + "/Floor", thick=G.FLOOR_T)
    for p in PAIRS:
        G.ramp(p["x0"], p["ramp_y0"], p["x1"], p["ramp_y0"] + W, p["base"], RISE, "+X",
               "NI_%s_Ramp" % p["name"], FOLDER + "/" + p["name"])
        G.deck(DECK_X0, p["deck_y0"], DECK_X1, p["deck_y0"] + 1000.0, DECK_TOP,
               "NI_%s_Deck" % p["name"], FOLDER + "/" + p["name"])
    out = []
    for e in G._elements:
        e = dict(e)
        e["mesh"] = G.VARIANT[(e["mesh"], e["material"])]   # the baked Spillway variants
        out.append(e)
    del G._elements[:]
    return out


def ramp_z(p, x):
    """Surface height of pair p's ramp at world x (its 2:1 line)."""
    return p["base"] + RISE * (x - p["x0"]) / RUN


def probe_points(p):
    """Where the nav probe asks, per pair: on the surfaces, mid-width."""
    y = p["ramp_y0"] + W / 2.0
    xs = {"ramp_foot": p["x0"] + 100.0, "ramp_mid": p["x0"] + RUN / 2.0,
          "ramp_head": DECK_X0 - 60.0}
    pts = {"floor": (-300.0, y, 0.0), "deck": ((DECK_X0 + DECK_X1) / 2.0, y, DECK_TOP)}
    for k, x in xs.items():
        pts[k] = (x, y, ramp_z(p, x))
    return pts


def offset(loc, origin):
    return [loc[0] + origin[0], loc[1] + origin[1], loc[2] + origin[2]]


# --- the check ------------------------------------------------------------------
def selftest():
    els = plan()
    by = {e["label"]: e for e in els}
    assert len(by) == len(els) == 5, "5 actors, unique labels"
    floor = by["NI_Floor"]["aabb"]
    assert floor[5] == 0.0 and floor[2] == -G.FLOOR_T
    print("selftest: %d actors" % len(els))
    for p in PAIRS:
        r, d = by["NI_%s_Ramp" % p["name"]], by["NI_%s_Deck" % p["name"]]
        ra, da = r["aabb"], d["aabb"]
        rise, run = ra[5] - ra[2], ra[3] - ra[0]
        assert abs(rise / run - 0.5) < 1e-12, (p["name"], rise, run)
        assert abs(r["slope_deg"] - 26.57) < 0.01, r["slope_deg"]
        head_x, head_z = ra[3], ra[5]                 # up +X: head is the x1 edge, top z
        gap = head_x - da[0]                          # +: head inside the deck footprint
        delta = da[5] - head_z                        # +: head below the deck top
        step = gap * (RISE / RUN) + delta             # where the ramp meets the deck face
        # the seating transform, checked against the mesh facts (high at local y=0, pivot
        # min corner, yaw 90 maps local +X->+Y and local +Y->-X): head sits at loc.x
        assert r["rot"] == [0.0, 90.0, 0.0] and r["loc"][0] == head_x, r
        assert abs(r["scale"][1] * 100.0 - run) < 1e-9 and abs(r["scale"][2] * 100.0 - rise) < 1e-9
        assert r["loc"][2] == ra[2] == p["base"]
        assert r["mesh"].endswith("SM_SPW_Ramp") and d["mesh"].endswith("SM_SPW_Deck")
        # the floor must carry both feet; the deck must sit at the tier height
        assert floor[0] < ra[0] and ra[1] > floor[1] and ra[4] < floor[4]
        assert da[5] == DECK_TOP and da[2] == DECK_TOP - G.DECK_T
        # every probe point is on a surface and inside the nav bounds
        for k, (x, y, z) in probe_points(p).items():
            for i, (c, s) in enumerate(zip(NAV_CENTRE, NAV_SIZE)):
                assert abs((x, y, z)[i] - c) < s / 2.0 - 100.0, (k, i)
            if k.startswith("ramp"):
                assert ra[0] < x < ra[3] and abs(z - ramp_z(p, x)) < 1e-9
        line = "  %-6s slope %.2f deg (%.0f/%.0f)  head->deck gap %+.2f  top delta %+.2f  step %.1f" \
               % (p["name"], r["slope_deg"], rise, run, gap, delta, step)
        if p["name"] == "A_Abut":
            assert gap == 0.0 and delta == 0.0, line     # the coplanar control: 0.00 / 0.00
            print(line + "   <- CONTROL (0.00 coplanar abutment)")
        else:
            assert gap >= 50.0 and 3.0 <= delta <= 5.0, line
            assert step < AGENT_MAX_STEP, (step, AGENT_MAX_STEP)
            print(line + "   <- FIX (overlap >=50, sunk 3..5, step < %.0f)" % AGENT_MAX_STEP)
    # the two pairs never touch each other except through the floor
    a, b = by["NI_A_Abut_Deck"]["aabb"], by["NI_B_Fix_Deck"]["aabb"]
    assert a[4] < b[1], "decks overlap in Y"
    assert by["NI_A_Abut_Ramp"]["aabb"][4] < by["NI_B_Fix_Ramp"]["aabb"][1]
    print("selftest OK")


# --- the build (live editor over MCP) ----------------------------------------------
def build(into_gym):
    from mcp import MCP
    m = MCP()
    m.init()
    target = TEMPLATE_MAP if into_gym else MAP_PATH
    origin = ORIGIN_GYM if into_gym else (0.0, 0.0, 0.0)

    def call(ts, tool, args):
        v, raw = m.call(ts, tool, args)
        if v is None and "**" in str(raw):
            raise SystemExit("%s.%s failed: %s" % (ts, tool, str(raw)[:300]))
        return v

    exists = call(ASSET, "exists", {"path": target})
    if not exists and into_gym:
        raise SystemExit("STOP: %s missing - nothing to build into" % target)
    if not exists:
        print("duplicating %s -> %s (no toolset exposes new_level)" % (TEMPLATE_MAP, target))
        if not call(ASSET, "duplicate", {"path": TEMPLATE_MAP, "new_path": target}):
            raise SystemExit("STOP: AssetTools.duplicate refused. Re-run with --into-gym "
                             "(same actors inside BR_MetricsGym at %s)." % (ORIGIN_GYM,))
        call(ASSET, "save_assets", {"asset_paths": [target]})   # load_level refuses dirty maps
    cur = call(SCENE, "get_current_level", {})
    if cur != target:
        call(SCENE, "load_level", {"level_path": target})
    print("editor on %s" % call(SCENE, "get_current_level", {}))

    def find(tag):
        return call(SCENE, "find_actors", {"root": None, "name": "", "actor_type": None,
                                           "tag": tag, "bounds": None,
                                           "collision_channels": []}) or []

    tags = [TAG] if into_gym else [TAG, TEMPLATE_TAG]
    for t in tags:
        n = 0
        for a in find(t):
            call(SCENE, "remove_from_scene", {"actor": a})
            n += 1
        print("cleared %d actors tagged %s" % (n, t))

    def finish(a, label, folder):
        call(ACTOR, "set_label", {"actor": a, "label": label})
        call(ACTOR, "add_tag", {"actor": a, "tag": TAG})
        call(SCENE, "set_actor_folder", {"actor": a, "folder_path": folder})
        return a

    def xform(loc, rot=(0, 0, 0), scale=(1, 1, 1)):
        return {"location": dict(zip("xyz", offset(loc, origin))),
                "rotation": {"pitch": rot[0], "yaw": rot[1], "roll": rot[2]},
                "scale": dict(zip("xyz", scale))}

    els = plan()
    for e in els:
        a = call(SCENE, "add_to_scene_from_asset", {
            "asset_path": e["mesh"], "name": e["label"], "parent": None,
            "snap_to_ground": False, "xform": xform(e["loc"], e["rot"], e["scale"])})
        finish(a, e["label"], e["folder"])
    print("placed %d geometry actors" % len(els))

    a = call(SCENE, "add_to_scene_from_class", {
        "actor_type": {"refPath": PLAYER_START}, "name": "NI_Start", "parent": None,
        "snap_to_ground": False, "xform": xform(START)})
    finish(a, "NI_Start", FOLDER + "/Spawns")
    for cls, label, rot in ((SUN, "NI_Sun", (-46.0, 30.0, 0.0)), (SKY, "NI_Sky", (0, 0, 0))):
        a = call(SCENE, "add_to_scene_from_class", {
            "actor_type": {"refPath": cls}, "name": label, "parent": None,
            "snap_to_ground": False, "xform": xform((0, 0, 1000), rot)})
        finish(a, label, FOLDER + "/Lights")

    # The volume is a brush whose unscaled size is whatever the class default draws, NOT a
    # known number (land_spillway: assuming 200 made a 94 km volume). Measure, then solve.
    a = call(SCENE, "add_to_scene_from_class", {
        "actor_type": {"refPath": NAV_VOLUME}, "name": "NI_NavBounds", "parent": None,
        "snap_to_ground": False, "xform": xform(NAV_CENTRE)})
    finish(a, "NI_NavBounds", FOLDER + "/Nav")
    bb = call(ACTOR, "get_actor_bounds", {"actor": a})
    base = [bb["max"][k] - bb["min"][k] for k in "xyz"]
    scale = [NAV_SIZE[i] / base[i] for i in range(3)]
    call(ACTOR, "set_actor_transform", {"actor": a, "worldspace": True,
                                        "xform": xform(NAV_CENTRE, scale=scale)})
    bb = call(ACTOR, "get_actor_bounds", {"actor": a})
    print("nav bounds: base %s -> size %s" % ([round(v) for v in base],
                                              [round(bb["max"][k] - bb["min"][k]) for k in "xyz"]))

    if not call(ASSET, "save_assets", {"asset_paths": [target]}):
        raise SystemExit("STOP: save_assets returned False for %s" % target)
    print("saved %s" % target)

    # Read back from a FRESH load, never from this run's own report.
    call(SCENE, "load_level", {"level_path": target})
    got = {call(ACTOR, "get_label", {"actor": a}): a for a in find(TAG)}
    print("--- fresh-load read-back (%d actors tagged %s) ---" % (len(got), TAG))
    bad = 0
    for e in els:
        a = got.get(e["label"])
        if a is None:
            print("  MISSING %s" % e["label"])
            bad += 1
            continue
        bb = call(ACTOR, "get_actor_bounds", {"actor": a})
        have = [bb["min"]["x"], bb["min"]["y"], bb["min"]["z"],
                bb["max"]["x"], bb["max"]["y"], bb["max"]["z"]]
        want = e["aabb"][:3] + e["aabb"][3:]
        want = offset(want[:3], origin) + offset(want[3:], origin)
        ok = all(abs(h - w) <= 1.0 for h, w in zip(have, want))
        bad += 0 if ok else 1
        print("  %-16s %s aabb %s  want %s" % (e["label"], "OK  " if ok else "DRIFT",
                                               [round(v, 2) for v in have],
                                               [round(v, 2) for v in want]))
    for label in ("NI_Start", "NI_Sun", "NI_Sky", "NI_NavBounds"):
        print("  %-16s %s" % (label, "OK" if label in got else "MISSING"))
        bad += 0 if label in got else 1
    print("read-back: %s" % ("PASS" if not bad else "FAIL (%d)" % bad))
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--into-gym", action="store_true",
                    help="build inside BR_MetricsGym at ORIGIN_GYM instead of a new map")
    args = ap.parse_args()
    if args.selftest:
        selftest()
        return 0
    if args.dry_run:
        for e in plan():
            print("%-16s %-42s loc %s rot %s scale %s" % (e["label"], e["mesh"], e["loc"],
                                                          e["rot"], e["scale"]))
        for p in PAIRS:
            print("%s probes: %s" % (p["name"], json.dumps(probe_points(p))))
        print("nav %s size %s; start %s; map %s" % (NAV_CENTRE, NAV_SIZE, START, MAP_PATH))
        return 0
    return build(args.into_gym)


if __name__ == "__main__":
    raise SystemExit(main())
