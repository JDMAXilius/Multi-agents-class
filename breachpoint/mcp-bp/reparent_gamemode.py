"""
BP82 — reparent BP_FPST_GameMode onto ABPGameMode so PIE runs the ported pawn.

THE PROBLEM
    BR_Arena01's World Settings pin DefaultGameMode = BP_FPST_GameMode_C. A map's World
    Settings override BEATS GlobalDefaultGameMode in DefaultEngine.ini, so ABPGameMode never
    runs, DefaultPawnClassOverride is never read, and PIE spawns whatever the template GameMode
    says — which is not AMyCharacter. Standalone runs got past it with

        ?game=/Script/Breachpoint.BPGameMode

    on the URL, but PIE has no such URL, so PIE has never exercised any of the ported C++.

WHY THIS ASSET AND NOT THE MAP
    Content/Maps/ is OUTSIDE this ticket's owner_path; law 5 says a blocked write is a
    contract_gap, not a widened claim. Content/FPSTemplate/ IS inside it, and BP_FPST_GameMode
    is the very class the map already names. Reparenting it onto ABPGameMode makes the map's
    EXISTING override resolve to our C++ GameMode — no map edit, no claim change, and no
    contract_gap needed. Same shape as the BP_FPST_BaseWeapon reparent in reparent_weapon_base.py.

WHAT IT BUYS
    ABPGameMode::GetDefaultPawnClassForController returns DefaultPawnClassOverride (pinned to
    BP_FPSCharacter_C in DefaultGame.ini) BEFORE consulting DefaultPawnClass, so it wins whatever
    the Blueprint's own DefaultPawnClass happens to be. PlayerControllerClass and PlayerStateClass
    come from ABPGameMode's constructor too.

    Note the consequence, because it is not cosmetic: PIE will now get ABPPlayerController, which
    adds its own two input mapping contexts at priority 0. That is exactly the collision that made
    the pawn input-dead, and it is why AMyCharacter now adds its context at MappingContextPriority
    (default 1). If input dies in PIE after this, that is the first place to look.

TYPE COMPATIBILITY (checked, not assumed)
    ABPGameMode : AGameModeBase (BPGameMode.h:14), and BP_FPST_GameMode's NativeParentClass is
    /Script/Engine.GameModeBase. The reparent is within the same base.

RUN IT (editor MUST be closed — a running editor holds this asset and will clobber the write):
    "<UE>/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" <project>.uproject \
        -run=pythonscript -script="mcp-bp/reparent_gamemode.py" -unattended -nosplash

    Then read the GM| lines out of Saved/Logs/Breachpoint.log.

Exit code 0 = every assertion below held. Non-zero = nothing is claimed.

NOTE ON sys.exit: do NOT call it. Raising SystemExit out of a -run=pythonscript commandlet
crashes UE inside FPythonScriptPlugin::ShutdownPython AFTER the commandlet has already reported
success — a fatal error that means nothing. Set FAILURES instead.
"""

import unreal

TARGET = '/Game/FPSTemplate/Blueprints/BP_FPST_GameMode'
NEW_PARENT = '/Script/Breachpoint.BPGameMode'

FAILURES = []


def log(msg):
    unreal.log("GM| " + str(msg))


def fail(msg):
    FAILURES.append(str(msg))
    unreal.log_error("GM| FAIL: " + str(msg))


def main():
    blueprint = unreal.load_asset(TARGET)
    if not blueprint:
        fail("%s did not load" % TARGET)
        return

    new_parent = unreal.load_object(None, NEW_PARENT)
    if not new_parent:
        fail("%s did not load — is the module built?" % NEW_PARENT)
        return

    before = unreal.get_default_object(blueprint.generated_class())
    log("CDO before = %s" % (before.get_class().get_name() if before else None))
    if isinstance(before, unreal.BPGameMode):
        log("already an ABPGameMode child — nothing to do (idempotent)")
    else:
        try:
            unreal.BlueprintEditorLibrary.reparent_blueprint(blueprint, new_parent)
        except Exception as exc:
            fail("reparent raised (%s)" % exc)
            return
        log("reparented onto %s" % NEW_PARENT)

    try:
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        log("compiled")
    except Exception as exc:
        fail("compile failed (%s)" % exc)

    if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint):
        fail("save_loaded_asset returned False")
    else:
        log("saved %s" % TARGET)

    # Assert by re-reading, not by trusting the call above.
    after = unreal.get_default_object(blueprint.generated_class())
    if not isinstance(after, unreal.BPGameMode):
        fail("after reparent the CDO is still %s, not an ABPGameMode child"
             % (after.get_class().get_name() if after else None))
        return
    log("VERIFIED: CDO is an ABPGameMode child")

    # The pawn the map will actually spawn now. DefaultPawnClassOverride is Config-driven and
    # read at runtime by GetDefaultPawnClassForController, so print it as evidence.
    try:
        override = after.get_editor_property('default_pawn_class_override')
        log("DefaultPawnClassOverride = %s" % override)
        if not str(override):
            fail("DefaultPawnClassOverride is empty — pin it under "
                 "[/Script/Breachpoint.BPGameMode] in DefaultGame.ini")
    except Exception as exc:
        log("could not read DefaultPawnClassOverride (%s) — it is Config, check DefaultGame.ini"
            % exc)


main()

if FAILURES:
    log("RESULT: FAILED, %d problem(s) — nothing above is claimed" % len(FAILURES))
    for failure in FAILURES:
        log("  " + failure)
    raise RuntimeError("reparent_gamemode: %d failure(s)" % len(FAILURES))

log("RESULT: OK")
