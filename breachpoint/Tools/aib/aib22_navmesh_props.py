"""AIB22 step 3(d): print the level's RecastNavMesh link-generation settings. In-editor.

    py <repo>/Tools/aib/aib22_navmesh_props.py
"""
import unreal

w = unreal.EditorLevelLibrary.get_editor_world()
found = 0
for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.RecastNavMesh):
    found += 1
    out = {"label": a.get_actor_label()}
    for prop in ("generate_nav_links", "agent_radius", "agent_max_step_height", "agent_max_slope",
                 "cell_height", "tile_size_uu", "runtime_generation", "fixed_tile_pool_size", "tile_pool_size"):
        try:
            out[prop] = str(a.get_editor_property(prop))
        except Exception as exc:  # noqa: BLE001
            out[prop] = "?(%s)" % type(exc).__name__
    try:
        cfgs = a.get_editor_property("nav_link_jump_configs")
        out["nav_link_jump_configs"] = [
            (str(c.get_editor_property("name")), c.get_editor_property("enabled"),
             c.get_editor_property("jump_length"), c.get_editor_property("jump_max_depth"),
             c.get_editor_property("jump_height")) for c in cfgs]
    except Exception as exc:  # noqa: BLE001
        out["nav_link_jump_configs"] = "?(%s: %s)" % (type(exc).__name__, exc)
    unreal.log("AIB22 navmesh %s: %s" % (w.get_name(), out))
if not found:
    unreal.log("AIB22 navmesh %s: NO RecastNavMesh actor in the level" % w.get_name())
