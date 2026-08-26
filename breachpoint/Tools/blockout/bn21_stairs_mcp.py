#!/usr/bin/env python3
"""BN21 — replace the two solid stair volumes with walkable flights, over Unreal MCP.

WHY THIS EXISTS AND build_arena.py DOES NOT DO IT
-------------------------------------------------
build_arena.py is the blockout's builder and stays so. It cannot perform THIS change:

1. Run as `-run=pythonscript`, every spawn fails — `SpawnActorFromObject. No actor was
   spawned`, 82 of 82, then `'NoneType' object has no attribute 'set_actor_scale3d'`. A
   commandlet has no properly initialised editor world to spawn into. MCP runs INSIDE a
   live editor that does.
2. Its clear-and-rebuild is inert on this level: 49 actors carry no generated-tag, so
   `clear_generated` removes 0 and a full rebuild would DOUBLE the geometry rather than
   replace it. The two stair volumes are among those 49.

So this is surgical and idempotent: delete the two stair solids by name, add the treads.
Still a COMMITTED SCRIPT, never hand-placement (law 7) — the medium changed, the law did
not. The map must be lfs-locked (law 7, one owner per binary).

    python3 Tools/blockout/bn21_stairs_mcp.py --dry-run   # print the plan, touch nothing
    python3 Tools/blockout/bn21_stairs_mcp.py             # delete, place, save

Geometry comes from arena_plan.stair_steps() — the same function the plan validates — so
this script and the plan can never disagree about where a tread goes.
"""
from __future__ import annotations

import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(REPO, "mcp-ui", "gen_ui"))

import arena_plan  # noqa: E402
from mcp import MCP  # noqa: E402

SCENE = "editor_toolset.toolsets.scene.SceneTools"
ACTOR = "editor_toolset.toolsets.actor.ActorTools"
ASSET = "editor_toolset.toolsets.asset.AssetTools"

M_TO_CM = 100.0
CUBE = "/Engine/BasicShapes/Cube.Cube"
CUBE_BASE_UU = 100.0          # the engine cube is 100 uu on a side
LEVEL = "/Game/Maps/BR_Arena01.BR_Arena01"

# The two stair volumes as the manifest states them, and the actors that carry them today.
STAIRS = [
    {"actor": "BR_LM_Mezzanine_Catwalks_03", "box": {"x": [13, 16], "y": [19, 21], "z": [0, 4]}},
    {"actor": "BR_LM_Mezzanine_Catwalks_04", "box": {"x": [24, 27], "y": [19, 21], "z": [0, 4]}},
]


def treads():
    """Every tread to place, as (label, location_cm, size_cm)."""
    out = []
    for st in STAIRS:
        steps = arena_plan.stair_steps(st["box"])
        if not steps:
            raise SystemExit("refusing: %s produced no steps — check STAIR_MAX_RISE_M" % st["actor"])
        for i, b in enumerate(steps, start=1):
            cx = (b["x"][0] + b["x"][1]) / 2.0
            cy = (b["y"][0] + b["y"][1]) / 2.0
            cz = (b["z"][0] + b["z"][1]) / 2.0
            out.append((
                "%s_step%02d" % (st["actor"], i),
                [cx * M_TO_CM, cy * M_TO_CM, cz * M_TO_CM],
                [(b["x"][1] - b["x"][0]) * M_TO_CM,
                 (b["y"][1] - b["y"][0]) * M_TO_CM,
                 (b["z"][1] - b["z"][0]) * M_TO_CM],
            ))
    return out


def check_lock():
    """law 7: the binary must be locked by someone before we rewrite it."""
    import subprocess
    rel = "breachpoint/Content/Maps/BR_Arena01.umap"
    try:
        raw = subprocess.check_output(["git", "lfs", "locks", "--json"], cwd=REPO,
                                      stderr=subprocess.STDOUT).decode("utf-8")
        for lk in json.loads(raw or "[]") or []:
            if str(lk.get("path", "")).replace("\\", "/").endswith("Content/Maps/BR_Arena01.umap"):
                return True, "locked by %s" % (lk.get("owner") or {}).get("name", "?")
    except Exception as exc:                                    # noqa: BLE001
        return False, "could not query lfs locks: %s" % exc
    return False, "NOT lfs-locked — run: git lfs lock %s" % rel


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    plan = treads()
    print("BN21: %d treads across %d flights" % (len(plan), len(STAIRS)))
    rises = []
    for st in STAIRS:
        prev = st["box"]["z"][0]
        for b in arena_plan.stair_steps(st["box"]):
            rises.append((b["z"][1] - prev) * M_TO_CM)
            prev = b["z"][1]
    print("  max riser %.1f uu (MaxStepHeight is 45) — the character WALKS this" % max(rises))
    if args.dry_run:
        for label, loc, size in plan:
            print("  %-42s @ %-28s size %s" % (label, [round(v) for v in loc], [round(v) for v in size]))
        return 0

    ok, detail = check_lock()
    print("lfs lock: %s" % detail)
    if not ok:
        return 1

    m = MCP(); m.init()
    v, raw = m.call(SCENE, "get_current_level", {})
    print("current level: %r" % (v if v is not None else raw[:80]))

    # 1. remove the two solid volumes, by name
    for st in STAIRS:
        found, raw = m.call(SCENE, "find_actors", {"root": None, "name": st["actor"],
                                                   "actor_type": None, "tag": "",
                                                   "bounds": None, "collision_channels": None})
        if not found:
            print("  %s: not present (already replaced?) — %s" % (st["actor"], str(raw)[:70]))
            continue
        for a in found:
            v, raw = m.call(SCENE, "remove_from_scene", {"actor": a})
            print("  removed %s -> %s" % (st["actor"], str(raw)[:60]))

    # 2. place the treads
    placed = 0
    for label, loc, size in plan:
        # asset_path is a PLAIN STRING here, not a {"refPath": ...} object — one of the
        # few tools that differs, and the schema is the only place that says so.
        # snap_to_ground MUST be False: these treads are placed at computed heights and
        # dropping them to the floor would rebuild the solid wall one cube at a time.
        v, raw = m.call(SCENE, "add_to_scene_from_asset", {
            "asset_path": CUBE,
            "name": label,
            "parent": None,
            "snap_to_ground": False,
            "xform": {"location": {"x": loc[0], "y": loc[1], "z": loc[2]},
                      "rotation": None,
                      "scale": {"x": size[0] / CUBE_BASE_UU,
                                "y": size[1] / CUBE_BASE_UU,
                                "z": size[2] / CUBE_BASE_UU}},
        })
        if v is None:
            print("  FAILED %s: %s" % (label, str(raw)[:100]))
        else:
            placed += 1
    print("placed %d/%d treads" % (placed, len(plan)))
    if placed != len(plan):
        print("REFUSING to save: not every tread landed. The level is dirty but unsaved;")
        print("close the editor without saving, or fix and re-run.")
        return 1

    # asset_paths, plural, plain strings — same lesson as asset_path above.
    v, raw = m.call(ASSET, "save_assets", {"asset_paths": [LEVEL], "only_if_dirty": False})
    print("save: %s" % str(raw)[:120])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
