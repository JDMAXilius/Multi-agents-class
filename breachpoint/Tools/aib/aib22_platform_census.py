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
    # The floor anchor is a PlayerStart (reachable ground by definition, the same anchor the
    # island gate uses), not the lowest nav point - Spillway's lowest nav is the culvert pit.
    starts = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerStart)
    floor = project(w, starts[0].get_actor_location()) if starts else None
    if floor is None:
        floor = min(tops, key=lambda t: t[1].z)[1]
        unreal.log("  (no PlayerStart on nav - anchoring on the lowest nav point instead)")
    floor_z = floor.z
    plats = [(n, p) for n, p in tops if p.z > floor_z + STOREY]

    def detail(n, p, a, b):
        q = NS.find_path_to_location_synchronously(w, a, b, None, None)
        if q is None or not q.is_valid():
            return "%s z%+.0f NO PATH" % (n, p.z - floor_z)
        pts = q.path_points
        end = pts[-1] if pts else a
        return "%s z%+.0f partial, ends %.0fuu short" % (n, p.z - floor_z, (end - b).length())

    no_down = [detail(n, p, p, floor) for n, p in plats if not path_ok(w, p, floor)]
    no_up = [detail(n, p, floor, p) for n, p in plats if not path_ok(w, floor, p)]
    unreal.log("AIB22 census %s: %d tops on nav, anchor z %.0f (%s), %d platforms above %g" % (
        w.get_name(), len(tops), floor_z, "PlayerStart" if starts else "lowest", len(plats), STOREY))
    unreal.log("  no way DOWN: %d %s" % (len(no_down), no_down))
    unreal.log("  no way UP:   %d %s" % (len(no_up), no_up))
    unreal.log("  box 2 %s" % ("PASS" if not no_down and not no_up else "FAIL"))


if __name__ == "__main__":
    if len(sys.argv) > 2 and sys.argv[1] == "--grid":
        detail_grid(sys.argv[2])
    else:
        census(sys.argv[1] if len(sys.argv) > 1 else None)


def detail_grid(label, n=5):
    """Grid an actor's top (n x n) - on nav? path to the PlayerStart anchor? Per point.
    Separates 'the deck is one island' from 'the top-centre landed on a sliver'."""
    w = unreal.EditorLevelLibrary.get_editor_world()
    starts = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerStart)
    anchor = project(w, starts[0].get_actor_location())
    for m in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.StaticMeshActor):
        if m.get_actor_label() != label:
            continue
        o, e = m.get_actor_bounds(False)
        rows = []
        for i in range(n):
            row = ""
            for j in range(n):
                x = o.x - e.x + (2 * e.x) * (i + 0.5) / n
                y = o.y - e.y + (2 * e.y) * (j + 0.5) / n
                p = project(w, unreal.Vector(x, y, o.z + e.z))
                row += "." if p is None else ("U" if path_ok(w, anchor, p) else ("D" if path_ok(w, p, anchor) else "x"))
            rows.append(row)
        unreal.log("AIB22 grid %s top z %.0f extent %.0fx%.0f: '.'=no nav 'x'=nav,no path 'U'=up ok 'D'=down only" % (
            label, o.z + e.z, 2 * e.x, 2 * e.y))
        for r in rows:
            unreal.log("    " + r)
        return
    unreal.log("AIB22 grid: no actor labelled %s" % label)
