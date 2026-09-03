"""AIB22 step 3(c) — do the live nav bounds cover the geometry?  In-editor only.

    py <repo>/Tools/aib/aib22_nav_bounds.py        # on whatever level is open

Reports every NavMeshBoundsVolume AABB, the union AABB of all StaticMeshActors, the pieces whose
top face lies OUTSIDE every volume (no navmesh can exist there), and project_point_to_navigation
at each piece's top centre (the volume covers it but did recast actually tile it?).
"""
import json
import unreal

MARGIN = 35.0  # agent radius: a top that ends within one radius of the volume edge is "covered"


def aabb(actor):
    o, e = actor.get_actor_bounds(False)
    return [o.x - e.x, o.y - e.y, o.z - e.z], [o.x + e.x, o.y + e.y, o.z + e.z]


def inside(p, box):
    lo, hi = box
    return all(lo[i] - MARGIN <= p[i] <= hi[i] + MARGIN for i in range(3))


def main():
    w = unreal.EditorLevelLibrary.get_editor_world()
    vols = [(a.get_actor_label(), aabb(a))
            for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.NavMeshBoundsVolume)]
    meshes = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.StaticMeshActor)
    lo = [1e9] * 3
    hi = [-1e9] * 3
    uncovered, untiled = [], []
    nav = unreal.NavigationSystemV1.get_navigation_system(w)
    for m in meshes:
        b = aabb(m)
        lo = [min(lo[i], b[0][i]) for i in range(3)]
        hi = [max(hi[i], b[1][i]) for i in range(3)]
        top = [(b[0][0] + b[1][0]) / 2, (b[0][1] + b[1][1]) / 2, b[1][2]]
        if not any(inside(top, v[1]) for v in vols):
            uncovered.append((m.get_actor_label(), [round(x) for x in top]))
            continue
        r = nav.project_point_to_navigation(w, unreal.Vector(*top), None, None, unreal.Vector(50, 50, 200))
        if r is None:  # the wrapper folds bool+out into 'the point, or None when false'
            untiled.append((m.get_actor_label(), [round(x) for x in top]))
    lines = ["AIB22 nav-bounds %s: %d volumes, %d mesh actors" % (w.get_name(), len(vols), len(meshes))]
    for n, b in vols:
        lines.append("  volume %-24s min %s max %s" % (n, [round(x) for x in b[0]], [round(x) for x in b[1]]))
    lines.append("  geometry union         min %s max %s" % ([round(x) for x in lo], [round(x) for x in hi]))
    lines.append("  tops OUTSIDE every volume: %d %s" % (len(uncovered), json.dumps(uncovered[:12])))
    lines.append("  tops covered but NOT on nav: %d %s" % (len(untiled), json.dumps(untiled[:20])))
    lines.append("  nav building now: %s" % nav.is_navigation_being_built(w))
    for l in lines:
        unreal.log(l)


main()
