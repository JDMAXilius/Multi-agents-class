"""AIB28 step 2 triage, in-editor (level open): the architect's three path queries, the
PlayerStart dump, and every StaticMeshActor top between z 100 and 250.

    py <repo>/Tools/aib/aib28_triage.py
"""
import unreal

NS = unreal.NavigationSystemV1
EXTENT = unreal.Vector(50, 50, 200)


def project(w, v):
    r = NS.project_point_to_navigation(w, v, None, None, EXTENT)
    if isinstance(r, tuple):
        r = r[1] if r[0] else None
    return r


def path(w, a, b):
    p = NS.find_path_to_location_synchronously(w, a, b, None, None)
    if p is None or not p.is_valid():
        return "NO PATH"
    pts = p.path_points
    end = pts[-1] if pts else a
    return "%s len %.0f, ends %.0fuu from the goal" % ("partial" if p.is_partial() else "FULL", p.get_path_length(), (end - b).length())


def main():
    w = unreal.EditorLevelLibrary.get_editor_world()
    Q = {
        "A floor -> tread 7": ((1000, 2000, 10), (1327, 2000, 225)),
        "B tread 13 -> drum roof": ((1579, 2000, 410), (1700, 2000, 410)),
        "C roof -> SP5": ((1700, 2000, 410), (2000, 1300, 405)),
        "D floor -> mezz spawn tier": ((1000, 2000, 10), (2000, 1400, 410)),
    }
    for name, (a, b) in Q.items():
        pa, pb = project(w, unreal.Vector(*a)), project(w, unreal.Vector(*b))
        if pa is None or pb is None:
            unreal.log("AIB28 %s: off nav (start %s, goal %s)" % (name, pa is not None, pb is not None))
            continue
        unreal.log("AIB28 %s: %s  [start nav z %.0f, goal nav z %.0f]" % (name, path(w, pa, pb), pa.z, pb.z))
    for s in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerStart):
        l = s.get_actor_location()
        p = project(w, l)
        unreal.log("AIB28 PlayerStart %-24s at (%.0f,%.0f,%.0f) nav %s" % (s.get_actor_label(), l.x, l.y, l.z, "(%.0f,%.0f,%.0f)" % (p.x, p.y, p.z) if p else "NONE"))
    for m in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.StaticMeshActor):
        o, e = m.get_actor_bounds(False)
        top = o.z + e.z
        if 100 <= top <= 250 and e.x < 5000:
            unreal.log("AIB28 top %-28s z %.0f at (%.0f,%.0f) ext %.0fx%.0f" % (m.get_actor_label(), top, o.x, o.y, 2 * e.x, 2 * e.y))


main()
