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
import sys
import unreal

# One script, one level per invocation:  py import_landfall.py rallypoint
LEVELS = {
    "landfall": {
        "src": "~/Downloads/halo-4multiplayermajesticlandfall/source/"
               "Landfall/Landfall.obj",
        "root": "/Game/Halo4_Landfall",
        "mesh": "Landfall",
        "map": "MAP_Halo4_Landfall",
        # exposure + sun are PER LEVEL because these rips carry very different
        # albedo. Landfall is bright concrete and blew out; Rally Point is a
        # night-ish street and came in almost black at the same settings.
        "exposure": 1.0,
        "sun": 3.0,
    },
    "derelict": {
        # Halo 1 (Xbox) multiplayer "Derelict" - internal Halo name `carousel`,
        # which is the single object in the OBJ and how you know the rip is the
        # right map. Tiny next to the others: 1,412 verts / 2,068 tris, so the
        # import is seconds, not minutes. Raw OBJ extents X 97.6 / Y(up) 20.4 /
        # Z 97.6 - a square, symmetrical footprint, which matches the map.
        "src": "~/Downloads/halo-1multiplayerxboxderelict/source/"
               "Derelict/Derelict.obj",
        "root": "/Game/Halo1_Derelict",
        "mesh": "Derelict",
        "map": "MAP_Halo1_Derelict",
        # This rip's .mtl declares 10 materials and NOT ONE map_Kd - every one is
        # `Kd 1 1 1`, flat white, with the nine .jpeg files sitting unreferenced
        # beside it. So the mesh renders white by design of the source, not by
        # any import fault, and the only readability lever is lighting: keep the
        # sun low enough that shape reads through shading instead of clipping.
        # Wire the textures by hand (see the ticket) if the layout needs reading
        # by material rather than by form.
        "exposure": 0.18,
        "sun": 1.5,
    },
    "rallypoint": {
        "src": "~/Downloads/locationshalo-3firefightodstrally-point/source/"
               "RallyPoint/Rally Point.obj",
        "root": "/Game/Halo3ODST_RallyPoint",
        "mesh": "Rally_Point",
        "map": "MAP_Halo3ODST_RallyPoint",
        "exposure": 0.25,
        "sun": 8.0,
    },
}
WHICH = (sys.argv[1].lower() if len(sys.argv) > 1 else "landfall")
if WHICH not in LEVELS:
    raise SystemExit("unknown level %r; choose one of %s"
                     % (WHICH, sorted(LEVELS)))
CFG = LEVELS[WHICH]

SRC = os.path.expanduser(CFG["src"])
ROOT = CFG["root"]
MESH_PKG = ROOT + "/Meshes"
MAP_PATH = ROOT + "/Maps/" + CFG["map"]
TAG = "BN35_HaloRef_" + WHICH

# 1 OBJ unit = 1 metre is a HYPOTHESIS. The scale-reference box is how you
# check it: it is exactly our measured 0.68 x 1.92 m player capsule (BN32).
UNITS_TO_UU = 100.0

# Interchange maps OBJ (x,y,z) -> UE (x,-y,z) and does NOT rotate Y-up to Z-up
# (InterchangeOBJTranslator.cpp:186, measured in BN31), so the model needs a
# roll about X to stand up.
#
# +90, NOT -90. Founder caught this by eye, 30 Aug: at -90 the entire map is
# UPSIDE DOWN. It is a nasty failure because it looks plausible in a screenshot
# and traces still hit geometry - you are standing under ceilings. What gives it
# away is the navmesh: an inverted level has no UP-FACING surfaces, so Recast
# finds nothing walkable. Measured at -90: 2,086 collision hits, 0% navmesh,
# while a plain engine cube dropped into the same volume navmeshed fine.
UPRIGHT_ROLL = 90.0


def imported_mesh():
    existing = MESH_PKG + "/" + CFG["mesh"]
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
    unreal.log("importing %s - materials + textures, can be slow for big rips"
               % os.path.basename(SRC))
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
    tag(a, WHICH + "_Geometry", "HaloRef/Geometry")

    origin, extent = a.get_actor_bounds(False)
    unreal.log(WHICH + " placed: %.1f x %.1f x %.1f m, z %.1f .. %.1f m"
               % (extent.x * 2 / 100, extent.y * 2 / 100, extent.z * 2 / 100,
                  (origin.z - extent.z) / 100, (origin.z + extent.z) / 100))

    # LIGHTING: sun + sky + atmosphere, or the level renders black (BN33).
    sun = actor_ss.spawn_actor_from_class(
        unreal.DirectionalLight, unreal.Vector(0, 0, origin.z + extent.z + 2000),
        unreal.Rotator(0.0, -48.0, 25.0))
    sun.light_component.set_intensity(CFG.get("sun", 3.0))
    tag(sun, "Light_Sun", "HaloRef/Lights")
    sky = actor_ss.spawn_actor_from_class(
        unreal.SkyLight, unreal.Vector(0, 0, origin.z + extent.z + 2000),
        unreal.Rotator(0, 0, 0))
    tag(sky, "Light_Sky", "HaloRef/Lights")
    try:
        atm = actor_ss.spawn_actor_from_class(
            unreal.SkyAtmosphere, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
        tag(atm, "SkyAtmosphere", "HaloRef/Lights")
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
    tag(ps, "PlayerStart_" + WHICH, "HaloRef/Spawns")
    unreal.log("PlayerStart at %.0f, %.0f, %.0f" % spot)

    # HUMAN SCALE: exactly our measured capsule, 0.68 x 1.92 m (BN32).
    cube = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Cube")
    ref = actor_ss.spawn_actor_from_object(
        cube, unreal.Vector(spot[0] + 150, spot[1], spot[2]),
        unreal.Rotator(0, 0, 0))
    ref.set_actor_scale3d(unreal.Vector(0.68, 0.68, 1.92))
    tag(ref, "SCALE_REF_player_1m92", "HaloRef/Scale")

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
        ev = CFG.get("exposure", 1.0)
        st.set_editor_property("auto_exposure_min_brightness", ev)
        st.set_editor_property("auto_exposure_max_brightness", ev)
        ppv.set_editor_property("settings", st)
        tag(ppv, "PP_ExposureLock", "HaloRef/Lights")
        unreal.log("exposure locked at %.2f via an unbound PostProcessVolume" % ev)
    except Exception as e:
        unreal.log_warning("could not lock exposure: %s" % e)

    # NAV VOLUME Z COMES FROM THE MESH, NOT FROM get_actor_bounds().
    #
    # Measured 30 Aug: on this actor get_actor_bounds() reports centre z -1986
    # while the true world range is -1120..5092 cm - the Z is SIGN-FLIPPED, and
    # re-reading it does not help. Using it put the nav volume almost entirely
    # BELOW the level, so Recast voxelised empty air: 2,086 collision hits and
    # 0% navmesh coverage.
    #
    # The mesh's own local box is reliable. The model is Y-up, Interchange
    # negates Y, and the -90 roll then carries that onto +Z, so world Z is
    # simply the local Y times the scale.
    # World Z follows the roll. With UPRIGHT_ROLL = +90 a rotation about X
    # carries local +Y onto world +Z, so world z = local_y * scale directly.
    # (get_actor_bounds() cannot be trusted for this - it reported centre
    # z -1986 against a true range of -1120..5092 - so the mesh's own local
    # box is the source of truth.)
    # The sign is MEASURED, not derived. With UPRIGHT_ROLL = +90 the actor
    # bounds read z -1120..5092 cm while the mesh's local Y runs -50.92..11.20,
    # so world z = -local_y * scale. Interchange's Y mirror on import is why the
    # obvious sign is the wrong one; trust the measurement.
    lb = mesh.get_bounding_box()
    z_min = -lb.max.y * UNITS_TO_UU
    z_max = -lb.min.y * UNITS_TO_UU
    z_mid = (z_min + z_max) / 2.0
    z_half = (z_max - z_min) / 2.0 + 500.0          # 5 m of headroom
    unreal.log("nav volume Z from mesh box: %.0f .. %.0f cm (centre %.0f)"
               % (z_min, z_max, z_mid))
    nav = actor_ss.spawn_actor_from_class(
        unreal.NavMeshBoundsVolume, unreal.Vector(origin.x, origin.y, z_mid),
        unreal.Rotator(0, 0, 0))
    # a volume's default brush is a 200 uu cube (measured BN33), so halve
    nav.set_actor_scale3d(unreal.Vector(extent.x / 100.0, extent.y / 100.0,
                                        z_half / 100.0))
    tag(nav, "Nav_" + WHICH, "HaloRef/Nav")

    level_ss.save_current_level()
    unreal.log("saved %s" % MAP_PATH)


if __name__ == "__main__":
    main()
