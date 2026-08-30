"""BN30 — build the Aquarius blockout in-editor from the modular kit manifest.

STATUS: WRITTEN, NOT RUN (cloud has no editor; the terminal runs this).

    UnrealEditor-Cmd Breachpoint.uproject -run=pythonscript \
        -script="Tools/blockout/build_aquarius_blockout.py" \
        -stdout -unattended -nosplash

Doctrine (ue-editor skill, law 7): the kit JSON is the SOURCE OF TRUTH and this
committed script is the LANDING MECHANISM. An Unreal MCP session may run reads,
screenshots and spike probes against the result, but geometry lands only by
re-running this script after regenerating the manifest:

    python3 Tools/blockout/gen_aquarius_kit.py     # manifest
    <this script in the editor>                    # projection

IDEMPOTENT: every actor it spawns is tagged BN30_BlockoutGenerated and the
script deletes previously-tagged actors first — a re-run is a clean rebuild.

Reads:  Content/Data/aquarius_blockout_kit.json   (placements, cm / UE rotators)
        Content/Data/aquarius_manifest.json       (spawns, rocket - metres!)
Map:    /Game/Maps/BR_Aquarius (created if absent) — BN30 owns this .umap.
Axes:   kit X = map east, kit Y = map south, Z up — spawned as-is into UE
        world axes (X east, Y south when viewed top-down with N up-screen).
"""

import json

import unreal

KIT_PATH = "Content/Data/aquarius_blockout_kit.json"
MANIFEST_PATH = "Content/Data/aquarius_manifest.json"
MAP_PATH = "/Game/Maps/BR_Aquarius"
TAG = "BN30_BlockoutGenerated"
M_TO_UU = 100.0                      # manifest metres -> UE cm, converted ONCE


def load_json(rel):
    path = unreal.Paths.project_dir() + rel
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)



def ensure_lighting(actor_ss, tag_fn, FOLDER="Blockout/Lights"):
    """A map made by new_level() has NO lights - every screenshot comes out black.

    Found 30 Aug 2026: BR_MetricsGym's first capture was unreadable and an actor
    audit showed no DirectionalLight and no SkyLight in the level at all. Both
    are tagged like everything else, so a re-run replaces them rather than
    stacking a new sun on every build.
    """
    sun = actor_ss.spawn_actor_from_class(
        unreal.DirectionalLight, unreal.Vector(0, 0, 1000),
        unreal.Rotator(0.0, -46.0, 30.0))
    sun.set_actor_label("Light_Sun")
    tag_fn(sun)
    sun.set_folder_path(unreal.Name(FOLDER))
    sky = actor_ss.spawn_actor_from_class(
        unreal.SkyLight, unreal.Vector(0, 0, 1000), unreal.Rotator(0, 0, 0))
    sky.set_actor_label("Light_Sky")
    tag_fn(sky)
    sky.set_folder_path(unreal.Name(FOLDER))
    return 2


def main():
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    level_ss = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

    if not unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        level_ss.new_level(MAP_PATH)
    else:
        level_ss.load_level(MAP_PATH)

    # idempotency: clear our previous projection before rebuilding
    stale = [a for a in actor_ss.get_all_level_actors()
             if a.actor_has_tag(unreal.Name(TAG))]
    for a in stale:
        actor_ss.destroy_actor(a)
    print("cleared %d previously generated actors" % len(stale))

    kit = load_json(KIT_PATH)

    # Place instances of the FIFTEEN AUTHORED KIT ASSETS (BN31). Each family
    # maps to its kit asset; scale = desired size / the asset's own size, so
    # editing an asset repropagates everywhere. If BN31's assets are absent
    # (its watch-list unresolved) fall back to the engine cube and SAY SO -
    # a written-down fallback is fine, a silent one is not.
    try:
        spec = load_json("Content/Data/blockout_kit_assets.json")
        asset_size = {a["id"]: a for a in spec["assets"]}
    except Exception:
        asset_size = {}
    FAMILY_ASSET = kit.get("asset_map", {})
    resolved, fallback = {}, False
    for fam, aid in FAMILY_ASSET.items():
        path = "/Game/Blockout/Meshes/SM_%s" % aid
        obj = (unreal.EditorAssetLibrary.load_asset(path)
               if unreal.EditorAssetLibrary.does_asset_exist(path) else None)
        if obj is None:
            fallback = True
        resolved[fam] = (obj, asset_size.get(aid))
    cube = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Cube")
    if fallback:
        print("NOTE: one or more SM_BLK kit assets missing - falling back to "
              "/Engine/BasicShapes/Cube for those families. RECORD THIS in "
              "TICKET_BN31 before claiming the build is kit-built.")

    def tag(a):
        # UPROPERTY arrays through the Python proxy: read, modify, write back
        t = list(a.get_editor_property("tags"))
        t.append(unreal.Name(TAG))
        a.set_editor_property("tags", t)

    count = 0
    for p in kit["placements"]:
        fam = p["variant"].split("_")[0]
        obj, aspec = resolved.get(fam, (None, None))
        loc = unreal.Vector(*p["location_cm"])
        # research-confirmed rotator order: Rotator(roll, pitch, yaw)
        rot = unreal.Rotator(p["rotation_deg"]["roll"],
                             p["rotation_deg"]["pitch"],
                             p["rotation_deg"]["yaw"])
        want = p["scale"]                       # metres of the placed piece
        if obj is not None and aspec:
            base = aspec["size_m"]
            if aspec["shape"] == "wedge":       # ramp: run, width, rise
                sc = [want[0] / base[0], want[1] / base[1], 1.0]
            else:
                sc = [want[i] / base[i] if base[i] else 1.0 for i in range(3)]
        else:
            obj, sc = cube, want                # unit cube: scale IS metres
        a = actor_ss.spawn_actor_from_object(obj, loc, rot)
        a.set_actor_scale3d(unreal.Vector(*sc))
        a.set_actor_label(p["label"])
        tag(a)
        a.set_folder_path(unreal.Name(p["folder"]))
        # StaticMeshActor defaults to Static; assert rather than set
        try:
            a.static_mesh_component.set_mobility(unreal.ComponentMobility.STATIC)
        except AttributeError:
            pass
        count += 1
    print("placed %d kit instances (%d variants)"
          % (count, len(kit["variants"])))

    # spawns come from the KIT (already converted into the kit frame and
    # snapped onto walkable ground). The arena manifest is +y NORTH while the
    # kit is +y SOUTH - reading spawns straight from the arena manifest
    # mirrors them, which validate_aquarius_blockout.py catches as P3_FRAME.
    man = load_json(MANIFEST_PATH)
    for sp in kit.get("spawn_points", []):
        loc = unreal.Vector(*sp["location_cm"])
        rot = unreal.Rotator(0.0, 0.0, float(sp.get("yaw_deg", 0.0)))
        ps = actor_ss.spawn_actor_from_class(unreal.PlayerStart, loc, rot)
        ps.set_actor_label("Spawn_%s_%s" % (sp.get("pool", "n"), sp["id"]))
        tag(ps)
        ps.set_folder_path(unreal.Name("Blockout/Spawns"))
    print("placed %d spawn points from the kit" % len(kit.get("spawn_points", [])))

    rk = man.get("rocket_node")
    if rk:
        b = man["bounds"]
        loc = unreal.Vector(rk["x"] * M_TO_UU,
                            (b["y"] - rk["y"]) * M_TO_UU,   # north -> south
                            rk["z"] * M_TO_UU + 20.0)
        mk = actor_ss.spawn_actor_from_class(unreal.TargetPoint, loc,
                                             unreal.Rotator(0, 0, 0))
        mk.set_actor_label("ROCKET_Node")
        tag(mk)
        mk.set_folder_path(unreal.Name("Blockout/Markers"))

    # nav bounds over the whole footprint (+2 m margin), bots from day one
    b = man["bounds"]
    nav = actor_ss.spawn_actor_from_class(
        unreal.NavMeshBoundsVolume,
        unreal.Vector(b["x"] * M_TO_UU / 2, b["y"] * M_TO_UU / 2,
                      b["z"] * M_TO_UU / 2),
        unreal.Rotator(0, 0, 0))
    nav.set_actor_scale3d(unreal.Vector(b["x"] + 4, b["y"] + 4, b["z"] + 4))
    nav.set_actor_label("Nav_Aquarius")
    tag(nav)
    nav.set_folder_path(unreal.Name("Blockout/Nav"))

    lights = ensure_lighting(actor_ss, tag)
    print("placed %d lights (a new_level map ships with none)" % lights)

    level_ss.save_current_level()
    print("saved %s" % MAP_PATH)

    # evidence loop: top-down + one per named region for the founder's eyes
    unreal.AutomationLibrary.take_high_res_screenshot(
        1920, 1080, "aquarius_blockout_top.png")
    print("screenshot queued: aquarius_blockout_top.png "
          "(set a top view first when running attended)")


if __name__ == "__main__":
    main()
