"""AIB28 rulings (2)-(3) - move the crate-top and gantry PlayerStarts onto walkable tiers.
In-editor, BR_Arena01 open:

    py <repo>/Tools/aib/aib28_playerstarts.py         # move + log old/new + nav projections
    py <repo>/Tools/aib/aib28_playerstarts.py dump    # projections only, touch nothing

PlayerStart_8v8_2/5/6/7 stand ON the four cover crates (700,700)/(3300,700)/... : each
moves 250 uu toward the map centre on both axes (clear of the 100 uu crate half-width),
z = floor + 100. BR_Spawn_SP7/SP8 stand on the gantry (z 805, no way down): each walks
from its (x, 2000) toward the centre in 100 uu steps until the point projects onto the
mezzanine tier (nav z 380..440). Idempotent: a start already off its crate / on the tier
is left where it is. Saves the level.
"""
import sys
import unreal

TAG = "AIB28"
NS = unreal.NavigationSystemV1
EXTENT = unreal.Vector(50, 50, 200)
CENTRE = unreal.Vector(2000, 2000, 0)
CRATE_STARTS = ("PlayerStart_8v8_2", "PlayerStart_8v8_5", "PlayerStart_8v8_6", "PlayerStart_8v8_7")
GANTRY_STARTS = ("BR_Spawn_SP7", "BR_Spawn_SP8")
FLOOR_Z, MEZZ = 0.0, (380.0, 440.0)


def project(w, v):
    r = NS.project_point_to_navigation(w, v, None, None, EXTENT)
    if isinstance(r, tuple):
        r = r[1] if r[0] else None
    return r


def fmt(v):
    return "(%.0f,%.0f,%.0f)" % (v.x, v.y, v.z) if v is not None else "NONE"


def sign(x):
    return 1.0 if x > 0 else (-1.0 if x < 0 else 0.0)


def move(a, new):
    old = a.get_actor_location()
    a.modify()
    a.set_actor_location(new, False, False)
    w = a.get_world()
    unreal.log("%s moved %s %s -> %s  nav old %s new %s" % (
        TAG, a.get_actor_label(), fmt(old), fmt(new), fmt(project(w, old)), fmt(project(w, new))))


def main(dump_only):
    w = unreal.EditorLevelLibrary.get_editor_world()
    starts = {s.get_actor_label(): s for s in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerStart)}
    for label, s in sorted(starts.items()):
        l = s.get_actor_location()
        unreal.log("%s before %-20s at %s nav %s" % (TAG, label, fmt(l), fmt(project(w, l))))
    if dump_only:
        unreal.log("%s dump - done" % TAG)
        return
    moved = 0
    for label in CRATE_STARTS:
        s = starts.get(label)
        if s is None:
            unreal.log_warning("%s no actor labelled %s" % (TAG, label))
            continue
        l = s.get_actor_location()
        p = project(w, l)
        if p is not None and p.z < 100:          # already on the floor
            unreal.log("%s %s already on the floor (nav %s), untouched" % (TAG, label, fmt(p)))
            continue
        new = unreal.Vector(l.x + 250 * sign(CENTRE.x - l.x), l.y + 250 * sign(CENTRE.y - l.y), FLOOR_Z + 100)
        move(s, new)
        moved += 1
    for label in GANTRY_STARTS:
        s = starts.get(label)
        if s is None:
            unreal.log_warning("%s no actor labelled %s" % (TAG, label))
            continue
        l = s.get_actor_location()
        p = project(w, l)
        if p is not None and MEZZ[0] <= p.z <= MEZZ[1]:
            unreal.log("%s %s already on the mezzanine tier (nav %s), untouched" % (TAG, label, fmt(p)))
            continue
        x, step = l.x, 100 * sign(CENTRE.x - l.x)
        new = None
        for _ in range(12):
            cand = unreal.Vector(x, 2000, 510)
            q = project(w, cand)
            if q is not None and MEZZ[0] <= q.z <= MEZZ[1]:
                new = unreal.Vector(q.x, q.y, q.z + 100)
                break
            x += step
        if new is None:
            unreal.log_error("%s %s: no mezzanine nav found along y=2000 from x %.0f" % (TAG, label, l.x))
            continue
        move(s, new)
        moved += 1
    ok = unreal.EditorLevelLibrary.save_current_level() if moved else "nothing to save"
    unreal.log("%s %d starts moved, save_current_level -> %s - done" % (TAG, moved, ok))


if __name__ == "__main__":
    main(len(sys.argv) > 1 and sys.argv[1] == "dump")
