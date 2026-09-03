"""AIB22 "Done when" box 2 — every platform has a way DOWN, the floor has a way UP. In-editor.

    py <repo>/Tools/aib/aib22_platform_census.py                      # editor console, level open

EDITOR-LIVE ONLY. Under `-run=pythonscript` the nav system is not up: Spillway projects nothing
onto nav and Arena01 fails every path both ways, ramps included (2026-09-02), which the live
editor and the headless matches both contradict. Type the line into the status-bar console (the
Unreal MCP Slate inspector can do it: TICKET_AIB22 Log, step 3), one map at a time.

A "platform" is any StaticMeshActor whose top centre projects onto nav and sits more than
STOREY above the lowest nav point in the level. Way down = find_path_to_location_synchronously
from its top to the floor anchor is valid and NOT partial (generated BN_Drop links are in the
tiles, so a valid path IS the link census — no python API exposes the links themselves).
Way up = the reverse query (BN_Climb / ramps). Partial counts as NO: islands do not refuse,
they partial (ticket Log, CONTRADICTION 1).
"""
import sys
import unreal

STOREY = 150.0
EXTENT = unreal.Vector(50, 50, 200)
NS = unreal.NavigationSystemV1


def project(w, v):
    r = NS.project_point_to_navigation(w, v, None, None, EXTENT)
    if isinstance(r, tuple):
        r = r[1] if r[0] else None
    return r


def path_ok(w, a, b):
    p = NS.find_path_to_location_synchronously(w, a, b, None, None)
    return bool(p is not None and p.is_valid() and not p.is_partial())


def census(level=None):
    if level:
        unreal.EditorLoadingAndSavingUtils.load_map("/Game/Maps/" + level)
    w = unreal.EditorLevelLibrary.get_editor_world()
    tops = []
    for m in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.StaticMeshActor):
        o, e = m.get_actor_bounds(False)
        if e.x > 100000:  # sky sphere
            continue
        p = project(w, unreal.Vector(o.x, o.y, o.z + e.z))
        if p is not None:
            tops.append((m.get_actor_label(), p))
    if not tops:
        unreal.log("AIB22 census %s: nothing on nav (is the navmesh built?)" % w.get_name())
        return
    floor_z = min(p.z for _, p in tops)
    floor = min(tops, key=lambda t: t[1].z)[1]
    plats = [(n, p) for n, p in tops if p.z > floor_z + STOREY]
    no_down = [n for n, p in plats if not path_ok(w, p, floor)]
    no_up = [n for n, p in plats if not path_ok(w, floor, p)]
    unreal.log("AIB22 census %s: %d tops on nav, floor z %.0f, %d platforms above %g" % (
        w.get_name(), len(tops), floor_z, len(plats), STOREY))
    unreal.log("  no way DOWN: %d %s" % (len(no_down), no_down))
    unreal.log("  no way UP:   %d %s" % (len(no_up), no_up))
    unreal.log("  box 2 %s" % ("PASS" if not no_down and not no_up else "FAIL"))


if __name__ == "__main__":
    census(sys.argv[1] if len(sys.argv) > 1 else None)
