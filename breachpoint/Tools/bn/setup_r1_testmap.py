"""BREACHPOINT NEXT - R1 test map setup + read-back audit (Roadmap 1, Goal 7.3/7.4).

Run inside the UE 5.8 editor (Python Editor Script Plugin):

    py "Tools/bn/setup_r1_testmap.py"

Creates /Game/BN/L_BNTest (Basic template when available), ensures floor /
PlayerStart / lights, points WorldSettings GameModeOverride at the C++
BNGameMode, saves, then reads everything back from the live editor and prints
an intent-vs-actual diff table. The audit table is the proof, not "the script
ran". Idempotent: a second run converges to the same state and re-audits.

Exits non-zero if the audit diffs, so a caller can gate on it.
"""
import sys

import unreal

# Maps/ is content-only - no Source/BreachpointNext counterpart.
# docs/BREACHPOINT-NEXT-CONTENT-LAYOUT.md
LEVEL_PATH = "/Game/BN/Maps/L_BNTest"
TEMPLATE_PATH = "/Engine/Maps/Templates/Template_Default"  # UE5's Basic template
GAME_MODE_CLASS_PATH = "/Script/BreachpointNext.BNGameMode"
CUBE_PATH = "/Engine/BasicShapes/Cube"
# Mesh names that count as an existing floor (Template_Default ships SM_Template_Map_Floor).
FLOOR_MESH_NAMES = ("SM_Template_Map_Floor", "Floor", "Cube")
FLOOR_LABEL = "BN_Floor"


# 5.8 prefers subsystems; EditorLevelLibrary still exists but is deprecated.
# Every editor call goes subsystem-first with the library as fallback.
def _subsystem(cls):
    try:
        return unreal.get_editor_subsystem(cls)
    except Exception:
        return None


def _level_op(op_name, *args):
    les = _subsystem(unreal.LevelEditorSubsystem)
    if les is not None and hasattr(les, op_name):
        return getattr(les, op_name)(*args)
    return getattr(unreal.EditorLevelLibrary, op_name)(*args)


def _all_actors():
    eas = _subsystem(unreal.EditorActorSubsystem)
    if eas is not None:
        return eas.get_all_level_actors()
    return unreal.EditorLevelLibrary.get_all_level_actors()


def _spawn(actor_class, location):
    eas = _subsystem(unreal.EditorActorSubsystem)
    if eas is not None:
        return eas.spawn_actor_from_class(actor_class, location, unreal.Rotator())
    return unreal.EditorLevelLibrary.spawn_actor_from_class(
        actor_class, location, unreal.Rotator())


def _editor_world():
    ues = _subsystem(unreal.UnrealEditorSubsystem)
    if ues is not None:
        return ues.get_editor_world()
    return unreal.EditorLevelLibrary.get_editor_world()


def _actors_of(actor_class):
    return [a for a in _all_actors() if isinstance(a, actor_class)]


def _find_floor():
    for actor in _actors_of(unreal.StaticMeshActor):
        if actor.get_actor_label() == FLOOR_LABEL:
            return actor
        mesh = actor.static_mesh_component.static_mesh
        if mesh is not None and mesh.get_name() in FLOOR_MESH_NAMES:
            return actor
    return None


def _world_settings():
    # AWorldSettings has no direct Python accessor on World; GameplayStatics finds it.
    return unreal.GameplayStatics.get_actor_of_class(_editor_world(), unreal.WorldSettings)


def setup(game_mode_class):
    if not unreal.EditorAssetLibrary.does_asset_exist(LEVEL_PATH):
        if unreal.EditorAssetLibrary.does_asset_exist(TEMPLATE_PATH):
            created = _level_op("new_level_from_template", LEVEL_PATH, TEMPLATE_PATH)
        else:
            created = _level_op("new_level", LEVEL_PATH)
        if not created:
            unreal.log_error("LEVEL CREATE FAILED: %s" % LEVEL_PATH)
            return False
    else:
        if not _level_op("load_level", LEVEL_PATH):
            unreal.log_error("LEVEL LOAD FAILED: %s" % LEVEL_PATH)
            return False

    if _find_floor() is None:
        floor = _spawn(unreal.StaticMeshActor, unreal.Vector(0.0, 0.0, 0.0))
        floor.static_mesh_component.set_static_mesh(
            unreal.EditorAssetLibrary.load_asset(CUBE_PATH))
        floor.set_actor_scale3d(unreal.Vector(50.0, 50.0, 1.0))
        floor.set_actor_label(FLOOR_LABEL)

    if not _actors_of(unreal.PlayerStart):
        _spawn(unreal.PlayerStart, unreal.Vector(0.0, 0.0, 200.0))

    if not _actors_of(unreal.DirectionalLight):
        _spawn(unreal.DirectionalLight, unreal.Vector(0.0, 0.0, 500.0))
    if not _actors_of(unreal.SkyLight):
        _spawn(unreal.SkyLight, unreal.Vector(0.0, 0.0, 500.0))

    ws = _world_settings()
    if ws is None:
        unreal.log_error("WORLDSETTINGS NOT FOUND: cannot set GameModeOverride.")
        return False
    # DefaultGameMode is the property behind the "GameMode Override" editor field.
    ws.set_editor_property("default_game_mode", game_mode_class)

    if not _level_op("save_current_level"):
        unreal.log_error("SAVE FAILED: %s" % LEVEL_PATH)
        return False
    return True


def audit():
    ws = _world_settings()
    override = ws.get_editor_property("default_game_mode") if ws is not None else None
    rows = [
        ("level exists on disk", "True",
         str(unreal.EditorAssetLibrary.does_asset_exist(LEVEL_PATH))),
        ("GameModeOverride", GAME_MODE_CLASS_PATH,
         override.get_path_name() if override is not None else "None"),
        ("PlayerStart count", "1", str(len(_actors_of(unreal.PlayerStart)))),
        ("floor actor present", "True", str(_find_floor() is not None)),
        ("DirectionalLight count >= 1", "True",
         str(len(_actors_of(unreal.DirectionalLight)) >= 1)),
        ("SkyLight count >= 1", "True", str(len(_actors_of(unreal.SkyLight)) >= 1)),
    ]

    unreal.log("== AUDIT: %s ==" % LEVEL_PATH)
    unreal.log("%-28s | %-40s | %-40s | %s" % ("CHECK", "INTENT", "ACTUAL", "STATUS"))
    diffs = 0
    for name, intent, actual in rows:
        ok = intent == actual
        diffs += 0 if ok else 1
        unreal.log("%-28s | %-40s | %-40s | %s"
                   % (name, intent, actual, "OK" if ok else "DIFF"))
    unreal.log("== AUDIT %s: %d/%d checks =="
               % ("PASSED" if diffs == 0 else "FAILED", len(rows) - diffs, len(rows)))
    return diffs == 0


def main():
    game_mode_class = unreal.load_class(None, GAME_MODE_CLASS_PATH)
    if game_mode_class is None:
        unreal.log_error(
            "CLASS LOAD FAILED: %s - the BreachpointNext C++ module is not compiled "
            "(or BNGameMode does not exist in it). Compile the module and re-run. "
            "Nothing was created or saved." % GAME_MODE_CLASS_PATH)
        return 1

    if not setup(game_mode_class):
        return 1
    return 0 if audit() else 1


sys.exit(main())
