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
    meshes = {m["id"]: unreal.EditorAssetLibrary.load_asset(m["mesh"])
              for m in kit["modules"]}
    cube = meshes["BLK_Cube"]

    def tag(a):
        # UPROPERTY arrays through the Python proxy: read, modify, write back
        t = list(a.get_editor_property("tags"))
        t.append(unreal.Name(TAG))
        a.set_editor_property("tags", t)

    count = 0
    for p in kit["placements"]:
        loc = unreal.Vector(*p["location_cm"])
        # research-confirmed rotator order: Rotator(roll, pitch, yaw)
        rot = unreal.Rotator(p["rotation_deg"]["roll"],
                             p["rotation_deg"]["pitch"],
                             p["rotation_deg"]["yaw"])
        a = actor_ss.spawn_actor_from_object(cube, loc, rot)
        a.set_actor_scale3d(unreal.Vector(*p["scale"]))
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

    # spawns + rocket from the arena manifest (METRES -> cm at this boundary)
    man = load_json(MANIFEST_PATH)
    for sp in man.get("spawn_points", []):
        L = sp["location"]
        loc = unreal.Vector(L["x"] * M_TO_UU, L["y"] * M_TO_UU,
                            L["z"] * M_TO_UU + 10.0)
        rot = unreal.Rotator(0.0, 0.0, float(sp.get("facing", 0)))
        ps = actor_ss.spawn_actor_from_class(unreal.PlayerStart, loc, rot)
        ps.set_actor_label("Spawn_%s_%s" % (sp.get("pool", "n"), sp["id"]))
        ps.tags.append(unreal.Name(TAG))
        ps.set_folder_path(unreal.Name("Blockout/Spawns"))
    rk = man.get("rocket_node")
    if rk:
        loc = unreal.Vector(rk["x"] * M_TO_UU, rk["y"] * M_TO_UU,
                            rk["z"] * M_TO_UU + 20.0)
        mk = actor_ss.spawn_actor_from_class(unreal.TargetPoint, loc,
                                             unreal.Rotator(0, 0, 0))
        mk.set_actor_label("ROCKET_Node")
        mk.tags.append(unreal.Name(TAG))
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
    nav.tags.append(unreal.Name(TAG))
    nav.set_folder_path(unreal.Name("Blockout/Nav"))

    level_ss.save_current_level()
    print("saved %s" % MAP_PATH)

    # evidence loop: top-down + one per named region for the founder's eyes
    unreal.AutomationLibrary.take_high_res_screenshot(
        1920, 1080, "aquarius_blockout_top.png")
    print("screenshot queued: aquarius_blockout_top.png "
          "(set a top view first when running attended)")


if __name__ == "__main__":
    main()
