"""AIB22: dump the climb geometry of the open level (steps / ramps / mezzanine pieces). In-editor."""
import unreal

w = unreal.EditorLevelLibrary.get_editor_world()
for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.StaticMeshActor):
    n = a.get_actor_label()
    if any(k in n for k in ("step", "Step", "Ramp", "ramp", "Stair", "Mezzanine", "Catwalk")):
        o, e = a.get_actor_bounds(False)
        r = a.get_actor_rotation()
        unreal.log("AIB22 step %-40s top z %6.0f  centre (%.0f,%.0f,%.0f)  ext (%.0f,%.0f,%.0f)  rot p%.0f y%.0f r%.0f" % (
            n, o.z + e.z, o.x, o.y, o.z, e.x, e.y, e.z, r.pitch, r.yaw, r.roll))
