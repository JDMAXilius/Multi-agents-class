"""AIB25 Phase 14 - place the AAIBLaneVolume lane painters on a blockout map. In-editor.

    py <repo>/Tools/aib/aib25_lane_volumes.py place      # clear + spawn + RebuildNavigation
    py <repo>/Tools/aib/aib25_lane_volumes.py save       # saves once the navmesh is idle
    py <repo>/Tools/aib/aib25_lane_volumes.py readback   # list what the loaded map carries

Driven by Tools/aib/ue_console.py (the editor console route). Lanes are the W-AUDIT's
folder ordinals on Spillway (05_Culvert=1, 10_Tower=2, 30_SouthLane=3, 40_Gallery=4,
50_Yard=5) and the four west-east corridors on Arena01; boxes never overlap each other
(the yard is two boxes with one id: it wraps the tower). Link areas (bridges, the gallery
tower link, the gantry) are left NavArea_Default on purpose. Idempotent: every
AIBLaneVolume actor is destroyed before the spawn.
"""
import sys
import unreal

TAG = "AIB25"
# map name -> [(label, lane id, centre, half extent)]   (uu; z ranges chosen so the
# sub-floor culvert / ground / mezzanine tiers never share a box)
LANES = {
    "BR_Spillway": [
        ("Lane1_Culvert",   1, (0, 0, -350),     (3300, 1500, 200)),   # z -550..-150
        ("Lane2_Tower",     2, (0, 0, 650),      (1400, 1400, 700)),   # z -50..1350
        ("Lane3_SouthLane", 3, (0, -3000, 300),  (4300, 1000, 400)),   # y -4000..-2000
        ("Lane4_Gallery",   4, (0, 3000, 400),   (4300, 1000, 500)),   # y 2000..4000
        ("Lane5_Yard_W",    5, (-2200, 0, 75),   (800, 2000, 175)),    # x -3000..-1400, z -100..250
        ("Lane5_Yard_E",    5, (2200, 0, 75),    (800, 2000, 175)),    # x 1400..3000
    ],
    "BR_Arena01": [
        ("Lane1_South",     1, (2000, 600, 100),  (2000, 600, 200)),   # y 0..1200 ground
        ("Lane2_MidGround", 2, (2000, 2000, 100), (2000, 800, 200)),   # y 1200..2800 ground
        ("Lane3_North",     3, (2000, 3400, 100), (2000, 600, 200)),   # y 2800..4000 ground
        ("Lane4_Mezzanine", 4, (2000, 2000, 475), (2000, 800, 125)),   # z 350..600 decks + roof
    ],
}


def world():
    return unreal.EditorLevelLibrary.get_editor_world()


def volumes(w):
    return list(unreal.GameplayStatics.get_all_actors_of_class(w, unreal.AIBLaneVolume))


def describe(a):
    o, e = a.get_actor_bounds(False)
    try:
        nloc = a.get_editor_property("net_load_on_client")
    except Exception as ex:
        nloc = "n/a (%s)" % ex
    area = a.get_editor_property("modifier").get_editor_property("area_class")
    return "%s lane_id=%d area=%s bounds x[%.0f,%.0f] y[%.0f,%.0f] z[%.0f,%.0f] tags=%s net_load_on_client=%s" % (
        a.get_actor_label(), a.get_editor_property("lane_id"), area.get_name() if area else None,
        o.x - e.x, o.x + e.x, o.y - e.y, o.y + e.y, o.z - e.z, o.z + e.z, [str(t) for t in a.tags], nloc)


def place():
    w = world()
    name = w.get_name()
    if not hasattr(unreal, "AIBLaneVolume"):
        unreal.log_error("%s STOP: unreal.AIBLaneVolume missing - stale editor build" % TAG)
        return
    if name not in LANES:
        unreal.log_error("%s STOP: no lane table for %s" % (TAG, name))
        return
    old = volumes(w)
    for a in old:
        unreal.EditorLevelLibrary.destroy_actor(a)
    unreal.log("%s %s: cleared %d existing AIBLaneVolume" % (TAG, name, len(old)))
    for label, lane, c, h in LANES[name]:
        a = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.AIBLaneVolume, unreal.Vector(*c))
        a.set_actor_label(label)
        a.set_editor_property("lane_id", lane)               # OnConstruction -> area class
        a.get_editor_property("bounds").set_box_extent(unreal.Vector(*h))
        tags = ["AIBLaneVolume"] + (["SpillwayGenerated"] if name == "BR_Spillway" else [])
        a.set_editor_property("tags", [unreal.Name(t) for t in tags])
        try:
            a.set_editor_property("net_load_on_client", False)
        except Exception as ex:
            unreal.log_warning("%s net_load_on_client not settable: %s" % (TAG, ex))
    for a in volumes(w):
        unreal.log("%s %s: %s" % (TAG, name, describe(a)))
    unreal.SystemLibrary.execute_console_command(w, "RebuildNavigation")
    unreal.log("%s %s: %d volumes placed, RebuildNavigation issued - done" % (TAG, name, len(volumes(w))))


def save():
    w = world()
    if unreal.NavigationSystemV1.is_navigation_being_built(w):
        unreal.log("%s %s: nav still building, NOT saved - done" % (TAG, w.get_name()))
        return
    ok = unreal.EditorLevelLibrary.save_current_level()
    unreal.log("%s %s: save_current_level -> %s - done" % (TAG, w.get_name(), ok))


def readback():
    w = world()
    vs = volumes(w)
    for a in vs:
        unreal.log("%s readback %s: %s" % (TAG, w.get_name(), describe(a)))
    unreal.log("%s readback %s: %d AIBLaneVolume actors - done" % (TAG, w.get_name(), len(vs)))


if __name__ == "__main__":
    {"place": place, "save": save, "readback": readback}[sys.argv[1] if len(sys.argv) > 1 else "readback"]()
