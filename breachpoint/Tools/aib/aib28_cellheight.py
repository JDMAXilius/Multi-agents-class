"""AIB28 ruling (1): set every nav resolution's CellHeight to 10 on the open level's RecastNavMesh
and rebuild navigation. Re-run aib28_triage.py afterwards (query A must read FULL).

    py <repo>/Tools/aib/aib28_cellheight.py
"""
import unreal

w = unreal.EditorLevelLibrary.get_editor_world()
for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.RecastNavMesh):
    params = a.get_editor_property("nav_mesh_resolution_params")
    before = [(p.get_editor_property("cell_size"), p.get_editor_property("cell_height"), p.get_editor_property("agent_max_step_height")) for p in params]
    for p in params:
        p.set_editor_property("cell_height", 10.0)
    a.set_editor_property("nav_mesh_resolution_params", params)
    after = [(p.get_editor_property("cell_size"), p.get_editor_property("cell_height"), p.get_editor_property("agent_max_step_height")) for p in a.get_editor_property("nav_mesh_resolution_params")]
    unreal.log("AIB28 cellheight %s: before %s after %s" % (a.get_actor_label(), before, after))
unreal.NavigationSystemV1.get_navigation_system(w).build()
unreal.log("AIB28 cellheight: navigation rebuild requested")
