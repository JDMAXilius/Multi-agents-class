"""
BP82 — revert BP_FPST_GameMode to /Script/Engine.GameModeBase.

WHY
    reparent_gamemode.py put BP_FPST_GameMode under ABPGameMode so PIE would spawn the ported
    AMyCharacter pawn. It did spawn it — verified by the "MyCharacter READY:" line — but it also
    brought ABPPlayerController into PIE for the first time, and from that point the pawn took
    no keyboard input. Two attempts to fix that (mapping-context priority, then an exclusive
    context) did not restore it, and the founder asked for the previous behaviour back.

    This puts the GameMode back where it was, which restores the default PlayerController and
    the template's own pawn. Pair it with DefaultPawnClassOverride in DefaultGame.ini being set
    back to BP_FPST_Character_C.

WHAT IS *NOT* REVERTED
    None of the C++ port. All of it is inert unless an AMyCharacter is spawned, so it costs
    nothing to leave in place. Re-running reparent_gamemode.py turns it all back on.

RUN IT (editor MUST be closed):
    "<UE>/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" <project>.uproject \
        -run=pythonscript -script="mcp-bp/revert_gamemode.py" -unattended -nosplash

Exit code 0 = verified by re-read. Do NOT call sys.exit (see reparent_gamemode.py).
"""

import unreal

TARGET = '/Game/FPSTemplate/Blueprints/BP_FPST_GameMode'
BASE = '/Script/Engine.GameModeBase'

FAILURES = []


def log(msg):
    unreal.log("GMREV| " + str(msg))


def fail(msg):
    FAILURES.append(str(msg))
    unreal.log_error("GMREV| FAIL: " + str(msg))


def main():
    blueprint = unreal.load_asset(TARGET)
    if not blueprint:
        fail("%s did not load" % TARGET)
        return

    base = unreal.load_object(None, BASE)
    if not base:
        fail("%s did not load" % BASE)
        return

    before = unreal.get_default_object(blueprint.generated_class())
    log("CDO before = %s" % (before.get_class().get_name() if before else None))

    try:
        unreal.BlueprintEditorLibrary.reparent_blueprint(blueprint, base)
        log("reparented onto %s" % BASE)
    except Exception as exc:
        fail("reparent raised (%s)" % exc)
        return

    try:
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        log("compiled")
    except Exception as exc:
        fail("compile failed (%s)" % exc)

    if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint):
        fail("save_loaded_asset returned False")
    else:
        log("saved %s" % TARGET)

    after = unreal.get_default_object(blueprint.generated_class())
    if isinstance(after, unreal.BPGameMode):
        fail("still an ABPGameMode child after revert")
        return
    log("VERIFIED: CDO is %s, no longer an ABPGameMode child"
        % (after.get_class().get_name() if after else None))


main()

if FAILURES:
    log("RESULT: FAILED, %d problem(s)" % len(FAILURES))
    for failure in FAILURES:
        log("  " + failure)
    raise RuntimeError("revert_gamemode: %d failure(s)" % len(FAILURES))

log("RESULT: OK")
