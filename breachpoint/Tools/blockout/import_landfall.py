"""BN35 - import the Halo 4 'Landfall' rip as a PLAYABLE reference level.

    py <this file>          (in a LIVE editor - the -run=pythonscript commandlet
                             cannot spawn actors; see the BN33 log)

EVERYTHING lands under ONE folder, /Game/Halo4_Landfall/:
    Meshes/     the level BSP static mesh
    Materials/  + Textures/   as Interchange imports them from the .mtl
    Maps/MAP_Halo4_Landfall   the playable map

IP: this is extracted Halo 4 content (343 / Microsoft). It is a reference and
study level - measure it, walk it, steal the LAYOUT into our own kit. It must
not ship in a build. Content/Halo4_Landfall/ is gitignored for that reason.

IDEMPOTENT: every actor is tagged BN35_LandfallRef and the script deletes its
own previous actors before rebuilding. Re-importing reuses the existing mesh.

Source: Blender-exported OBJ, Y-up, 412,761 verts / 707,215 tris, one object
`ca_port_bsp` (the level BSP). Raw OBJ extents X 362.3 / Y(up) 133.9 / Z 256.7;
play-space core (5-95th pct of vertices) 121 x 130 units.
"""

import os
import unreal

SRC = os.path.expanduser(
    "~/Downloads/halo-4multiplayermajesticlandfall/source/Landfall/Landfall.obj")
ROOT = "/Game/Halo4_Landfall"
MESH_PKG = ROOT + "/Meshes"
MAP_PATH = ROOT + "/Maps/MAP_Halo4_Landfall"
TAG = "BN35_LandfallRef"

# 1 OBJ unit = 1 metre is a HYPOTHESIS. The scale-reference box is how you
# check it: it is exactly our measured 0.68 x 1.92 m player capsule (BN32).
UNITS_TO_UU = 100.0

# Interchange maps OBJ (x,y,z) -> UE (x,-y,z) and does NOT rotate Y-up to Z-up
# (InterchangeOBJTranslator.cpp:186, measured in BN31), so the model's up axis
# lands on UE -Y and a -90 roll about X carries -Y onto +Z.
UPRIGHT_ROLL = -90.0


def imported_mesh():
    existing = MESH_PKG + "/Landfall"
    if unreal.EditorAssetLibrary.does_asset_exist(existing):
        unreal.log("reusing already-imported mesh %s" % existing)
        return unreal.EditorAssetLibrary.load_asset(existing)
    if not os.path.exists(SRC):
        raise RuntimeError("source OBJ not found: %s" % SRC)
    task = unreal.AssetImportTask()
    task.filename = SRC
    task.destination_path = MESH_PKG
    task.automated = True
    task.replace_existing = True
    task.save = True
    unreal.log("importing 77 MB / 707k tris with materials + textures - slow")
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = None
    for p in task.imported_object_paths:
        unreal.log("  imported %s" % p)
        obj = unreal.EditorAssetLibrary.load_asset(p.split(".")[0])
        if isinstance(obj, unreal.StaticMesh) and mesh is None:
            mesh = obj
    if mesh is None:
        raise RuntimeError("no StaticMesh produced; got %s"
                           % list(task.imported_object_paths))
    return mesh


def make_walkable(mesh):
    """707k triangles of level BSP: the ONLY sane collision is the triangles.

    An imported OBJ has no simple collision at all, so without this the pawn
    falls through the world. Complex-as-simple gives per-triangle collision,
    which is exactly what a static level wants (and what UE does for BSP).
    """
    try:
        bs = mesh.get_editor_property("body_setup")
        if bs is None:
            unreal.log_warning("no body_setup yet; collision NOT set")
            return False
        bs.set_editor_property(
            "collision_trace_flag",
            unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
        unreal.log("collision: UseComplexAsSimple (per-triangle) set on the mesh")
        return True
    except Exception as e:
        unreal.log_warning("could not set collision: %s" % e)
        return False


def floor_under(world, x, y, top_z, bottom_z):
    """Trace down to find a standable surface, so the PlayerStart is ON something."""
    hit = unreal.SystemLibrary.line_trace_single(
        world, unreal.Vector(x, y, top_z), unreal.Vector(x, y, bottom_z),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], 
        unreal.DrawDebugTrace.NONE, True, unreal.LinearColor.RED,
        unreal.LinearColor.GREEN, 0.0)
    if hit:
        return hit.to_tuple()[4].z          # impact_point.z
    return None


def main():
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    level_ss = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

    if not unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        level_ss.new_level(MAP_PATH)
    else:
        level_ss.load_level(MAP_PATH)

    stale = [a for a in actor_ss.get_all_level_actors()
             if a.actor_has_tag(unreal.Name(TAG))]
    for a in stale:
        actor_ss.destroy_actor(a)
    unreal.log("cleared %d previously generated actors" % len(stale))

    def tag(a, label, folder):
        t = list(a.get_editor_property("tags"))
        t.append(unreal.Name(TAG))
        a.set_editor_property("tags", t)
        a.set_actor_label(label)
        a.set_folder_path(unreal.Name(folder))

    mesh = imported_mesh()
    make_walkable(mesh)

    a = actor_ss.spawn_actor_from_object(
        mesh, unreal.Vector(0, 0, 0),
        unreal.Rotator(UPRIGHT_ROLL, 0.0, 0.0))    # Rotator(roll, pitch, yaw)
    a.set_actor_scale3d(unreal.Vector(UNITS_TO_UU, UNITS_TO_UU, UNITS_TO_UU))
    smc = a.static_mesh_component
    smc.set_mobility(unreal.ComponentMobility.STATIC)
    smc.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
    smc.set_collision_profile_name("BlockAll")
    tag(a, "Landfall_Geometry", "Landfall/Geometry")

    origin, extent = a.get_actor_bounds(False)
    unreal.log("Landfall placed: %.1f x %.1f x %.1f m, z %.1f .. %.1f m"
               % (extent.x * 2 / 100, extent.y * 2 / 100, extent.z * 2 / 100,
                  (origin.z - extent.z) / 100, (origin.z + extent.z) / 100))

    # LIGHTING: sun + sky + atmosphere, or the level renders black (BN33).
    sun = actor_ss.spawn_actor_from_class(
        unreal.DirectionalLight, unreal.Vector(0, 0, origin.z + extent.z + 2000),
        unreal.Rotator(0.0, -48.0, 25.0))
    sun.light_component.set_intensity(3.0)
    tag(sun, "Light_Sun", "Landfall/Lights")
    sky = actor_ss.spawn_actor_from_class(
        unreal.SkyLight, unreal.Vector(0, 0, origin.z + extent.z + 2000),
        unreal.Rotator(0, 0, 0))
    tag(sky, "Light_Sky", "Landfall/Lights")
    try:
        atm = actor_ss.spawn_actor_from_class(
            unreal.SkyAtmosphere, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
        tag(atm, "SkyAtmosphere", "Landfall/Lights")
    except Exception as e:
        unreal.log_warning("no SkyAtmosphere: %s" % e)

    # PLAYER START, dropped onto whatever surface is under the play-space core.
    world = unreal.EditorLevelLibrary.get_editor_world()
    top = origin.z + extent.z + 500
    bottom = origin.z - extent.z - 500
    spot = None
    for (px, py) in ((origin.x, origin.y),
                     (origin.x + 1000, origin.y), (origin.x - 1000, origin.y),
                     (origin.x, origin.y + 1000), (origin.x, origin.y - 1000)):
        z = floor_under(world, px, py, top, bottom)
        if z is not None:
            spot = (px, py, z + 120)
            break
    if spot is None:
        spot = (origin.x, origin.y, origin.z + extent.z + 200)
        unreal.log_warning("no floor found by trace - PlayerStart parked above "
                           "the bounds; move it by hand and say so")
    ps = actor_ss.spawn_actor_from_class(
        unreal.PlayerStart, unreal.Vector(*spot), unreal.Rotator(0, 0, 0))
    tag(ps, "PlayerStart_Landfall", "Landfall/Spawns")
    unreal.log("PlayerStart at %.0f, %.0f, %.0f" % spot)

    # HUMAN SCALE: exactly our measured capsule, 0.68 x 1.92 m (BN32).
    cube = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Cube")
    ref = actor_ss.spawn_actor_from_object(
        cube, unreal.Vector(spot[0] + 150, spot[1], spot[2]),
        unreal.Rotator(0, 0, 0))
    ref.set_actor_scale3d(unreal.Vector(0.68, 0.68, 1.92))
    tag(ref, "SCALE_REF_player_1m92", "Landfall/Scale")

    # EXPOSURE LOCK. Without this the level renders blown-out white: Lumen's
    # auto-exposure adapts to a bright sky and clips everything (the editor
    # says so out loud - "Cached lighting in Lumen and real-time sky capture
    # lighting is going to be clipped"). An unbound PostProcessVolume with min
    # and max brightness pinned to the same value IS the manual-exposure knob,
    # and it is what makes the imported textures actually readable.
    try:
        ppv = actor_ss.spawn_actor_from_class(
            unreal.PostProcessVolume, unreal.Vector(origin.x, origin.y, origin.z),
            unreal.Rotator(0, 0, 0))
        ppv.set_editor_property("unbound", True)
        st = ppv.get_editor_property("settings")
        st.set_editor_property("override_auto_exposure_min_brightness", True)
        st.set_editor_property("override_auto_exposure_max_brightness", True)
        st.set_editor_property("auto_exposure_min_brightness", 1.0)
        st.set_editor_property("auto_exposure_max_brightness", 1.0)
        ppv.set_editor_property("settings", st)
        tag(ppv, "PP_ExposureLock", "Landfall/Lights")
        unreal.log("exposure locked at 1.0 via an unbound PostProcessVolume")
    except Exception as e:
        unreal.log_warning("could not lock exposure: %s" % e)

    nav = actor_ss.spawn_actor_from_class(
        unreal.NavMeshBoundsVolume, unreal.Vector(origin.x, origin.y, origin.z),
        unreal.Rotator(0, 0, 0))
    # a volume's default brush is a 200 uu cube (measured BN33), so halve
    nav.set_actor_scale3d(unreal.Vector(extent.x / 100.0, extent.y / 100.0,
                                        extent.z / 100.0))
    tag(nav, "Nav_Landfall", "Landfall/Nav")

    level_ss.save_current_level()
    unreal.log("saved %s" % MAP_PATH)


if __name__ == "__main__":
    main()
