"""BN42 step 5 — FE_MainMenu.umap: the front-end stage. FALLBACK ONLY (R46).

The PRIMARY path is direct unreal-mcp calls from the editor session — TICKET_BN42 names
them (level toolset for new-level/save, ObjectTools for the WorldSettings game-mode
override, read back). Run this raw-python builder only if the MCP surface genuinely lacks
a level create/save operation, and record the toolset listing that forced it in the Log.

    UnrealEditor-Cmd ... -ExecutePythonScript="Tools/bn/bn41_fe_map.py"
    (or paste into the live editor's Python console — see the BN21 lesson below)

A map that is a STAGE, not a level: WorldSettings' GameModeOverride = ABNFrontEndGameMode,
one directional light + sky so the backdrop is not the void, nothing else. The menu is UI;
BN43 may add a Spillway vista later. Idempotent: re-running rebuilds the same map.

PROVEN APIs (transcribed from Tools/blockout/build_aquarius_blockout.py, which the terminal
has run): LevelEditorSubsystem new_level/load_level/save_current_level,
EditorActorSubsystem spawn_actor_from_class, unreal.EditorAssetLibrary.does_asset_exist.
BN21's recorded lesson applies: -run=pythonscript CANNOT spawn actors (no initialised
editor world) — run this via -ExecutePythonScript or the live editor, like the rig.

WATCH-LIST (one line, honest): `world.get_world_settings()` + setting `GameModeOverride`
(or `DefaultGameMode`) on it is NOT proven anywhere in this repo. Candidates, in the order
to try: (a) unreal.UnrealEditorSubsystem().get_editor_world().get_world_settings() then
set_editor_property('DefaultGameMode', mode_class); (b) if the property name refuses,
list_properties on the WorldSettings actor and use what it names (engine header calls it
DefaultGameMode on AWorldSettings; the per-map override the UI shows edits that same
property). If BOTH refuse: set the override by hand in the editor UI once, note it in the
BN42 Log, and keep the rest of this script as the map's builder — a one-click manual step
recorded honestly beats an invented API.
"""
import unreal

MAP_PATH = "/Game/Maps/FE_MainMenu"


def main():
    level_ss = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_ss = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        level_ss.load_level(MAP_PATH)
        # clean rebuild of OUR actors only (same tag idiom as every generator)
        for a in actor_ss.get_all_level_actors():
            if a.actor_has_tag(unreal.Name("BN41_FrontEndStage")):
                actor_ss.destroy_actor(a)
    else:
        level_ss.new_level(MAP_PATH)

    def tag(a):
        t = list(a.get_editor_property("tags"))
        t.append(unreal.Name("BN41_FrontEndStage"))
        a.set_editor_property("tags", t)

    sun = actor_ss.spawn_actor_from_class(unreal.DirectionalLight,
                                          unreal.Vector(0, 0, 500), unreal.Rotator(0, -35, 30))
    sun.set_actor_label("FE_Sun"); tag(sun)
    sky = actor_ss.spawn_actor_from_class(unreal.SkyAtmosphere,
                                          unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    sky.set_actor_label("FE_Sky"); tag(sky)

    # THE GAME MODE OVERRIDE — the watch-listed write, attempted then verified out loud.
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    ws = world.get_world_settings()
    mode = unreal.load_class(None, "/Script/BreachpointNext.BNFrontEndGameMode")
    try:
        ws.set_editor_property("DefaultGameMode", mode)
        readback = ws.get_editor_property("DefaultGameMode")
        print("FE map: DefaultGameMode = %s" % readback)
        if readback != mode:
            print("WARNING: readback mismatch - set the override by hand and log it in BN42.")
    except Exception as e:  # the honest fallback the docstring promises
        print("WATCH-LIST FIRED: could not set DefaultGameMode via python (%s)." % e)
        print("Set WorldSettings > Game Mode Override to BNFrontEndGameMode by hand, ONCE,")
        print("and record that in TICKET_BN42's Log.")

    level_ss.save_current_level()
    print("saved %s" % MAP_PATH)


if __name__ == "__main__":
    main()
