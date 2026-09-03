"""AIB28: test the tread hypothesis live — the Default nav resolution's 38 uu cell eats a 42 uu
tread under erosion. Sets resolution 0 cell_size to 19 on the open level's RecastNavMesh and
rebuilds navigation (console). Re-run aib28_triage.py afterwards; query A must read FULL.

    py <repo>/Tools/aib/aib28_cellsize.py [cell]      # default 19
"""
import sys
import unreal

cell = float(sys.argv[1]) if len(sys.argv) > 1 else 19.0
w = unreal.EditorLevelLibrary.get_editor_world()
for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.RecastNavMesh):
    params = a.get_editor_property("nav_mesh_resolution_params")
    before = [(p.get_editor_property("cell_size"), p.get_editor_property("cell_height")) for p in params]
    # element copies do not write back: rebuild the array with a fresh struct at index 0
    fresh = unreal.NavMeshResolutionParam()
    fresh.set_editor_property("cell_size", cell)
    fresh.set_editor_property("cell_height", params[0].get_editor_property("cell_height"))
    fresh.set_editor_property("agent_max_step_height", params[0].get_editor_property("agent_max_step_height"))
    a.set_editor_property("nav_mesh_resolution_params", [fresh] + list(params)[1:])
    after = [(p.get_editor_property("cell_size"), p.get_editor_property("cell_height")) for p in a.get_editor_property("nav_mesh_resolution_params")]
    unreal.log("AIB28 cellsize %s: before %s after %s" % (a.get_actor_label(), before, after))
unreal.SystemLibrary.execute_console_command(w, "RebuildNavigation")
unreal.log("AIB28 cellsize: RebuildNavigation issued")
