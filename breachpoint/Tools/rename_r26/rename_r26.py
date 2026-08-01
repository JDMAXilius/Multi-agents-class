#!/usr/bin/env python
# =====================================================================================
# BREACHPOINT - Tools/rename_r26/rename_r26.py
#
# Serves:   R26 condition 5 - a conforming BP child is named BP_<CppClassWithoutPrefix>.
#           The five founder Blueprints were authored as GM_BR / GS_BR / PC_BR / PS_BR /
#           BP_BRcharacter; audit_r26.py flags all five as written.
#
# Why a script and not `git mv`: a .uasset stores its own package name INSIDE the file.
#           Renaming the file on disk leaves the package name stale and UE refuses to load
#           it. `EditorAssetLibrary.rename_asset` rewrites the package, fixes up every
#           referencing asset, and leaves a redirector; `fixup_redirectors` then collapses
#           them. That is the only correct rename, and it needs a running editor.
#
# Contract: docs/contracts/data-and-assets.md (one owner per binary, lock before editing).
# Skill:    .claude/skills/ue-editor (headless -run=pythonscript; generated-script doctrine).
#
# ATOMICITY: this script also rewrites Config/DefaultEngine.ini's GlobalDefaultGameMode,
#           and only AFTER every rename succeeded. The repo is therefore never in a state
#           where the ini names an asset that does not exist - either all-old (PIE works)
#           or all-new (PIE works). A partial rename aborts before touching the ini.
#
# Idempotent: a name already at its target is reported as ALREADY and is not an error, so
#           a re-run after a partial failure is safe.
# =====================================================================================
import os
import sys

import unreal

# (current package path, target package path) - R26 condition 5 names.
RENAMES = [
    ("/Game/Core/GM_BR",              "/Game/Core/BP_BRGameMode"),
    ("/Game/Core/GS_BR",              "/Game/Core/BP_BRGameState"),
    ("/Game/Core/PC_BR",              "/Game/Core/BP_BRPlayerController"),
    ("/Game/Core/PS_BR",              "/Game/Core/BP_BRPlayerState"),
    # case-only fix: 'character' -> 'Character'. R26 condition 5 is compared
    # case-sensitively against the parent class name (ABRCharacter).
    ("/Game/Characters/BP_BRcharacter", "/Game/Characters/BP_BRCharacter"),
]

INI_RELATIVE = os.path.join("Config", "DefaultEngine.ini")
INI_OLD = "/Game/Core/GM_BR.GM_BR_C"
INI_NEW = "/Game/Core/BP_BRGameMode.BP_BRGameMode_C"


def log(msg):
    unreal.log("[rename_r26] {0}".format(msg))


def fail(msg):
    unreal.log_error("[rename_r26] {0}".format(msg))


def main():
    asset_lib = unreal.EditorAssetLibrary
    renamed, already, failed = [], [], []

    for src, dst in RENAMES:
        if asset_lib.does_asset_exist(dst) and not asset_lib.does_asset_exist(src):
            already.append(dst)
            log("ALREADY {0}".format(dst))
            continue
        if not asset_lib.does_asset_exist(src):
            failed.append((src, dst, "source asset not found"))
            fail("MISSING {0} (and target absent) - nothing to rename".format(src))
            continue
        # Case-only renames need a two-step on case-insensitive filesystems: UE treats
        # the target as already existing. Go via a temporary distinct name.
        if src.lower() == dst.lower():
            tmp = dst + "__r26tmp"
            ok = asset_lib.rename_asset(src, tmp) and asset_lib.rename_asset(tmp, dst)
        else:
            ok = asset_lib.rename_asset(src, dst)
        if ok:
            renamed.append((src, dst))
            log("RENAMED {0} -> {1}".format(src, dst))
        else:
            failed.append((src, dst, "rename_asset returned False"))
            fail("FAILED  {0} -> {1}".format(src, dst))

    if failed:
        fail("{0} rename(s) failed - Config/DefaultEngine.ini left UNCHANGED so the "
             "project stays self-consistent. Fix and re-run (safe: idempotent)."
             .format(len(failed)))
        for src, dst, why in failed:
            fail("   {0} -> {1}: {2}".format(src, dst, why))
        sys.exit(2)

    # rename_asset already fixed up referencing assets and left a redirector behind.
    # Collapsing the redirector is tidiness, not correctness - the project loads either
    # way - so a failure here is logged and tolerated rather than failing the run.
    try:
        registry = unreal.AssetRegistryHelpers.get_asset_registry()
        redirectors = []
        for folder in ("/Game/Core", "/Game/Characters"):
            for data in registry.get_assets_by_path(folder, recursive=False):
                if str(data.asset_class_path.asset_name) == "ObjectRedirector":
                    redirectors.append(data.get_asset())
        if redirectors:
            unreal.AssetToolsHelpers.get_asset_tools().fixup_referencers(redirectors)
            log("collapsed {0} redirector(s)".format(len(redirectors)))
        else:
            log("no redirectors to collapse")
    except Exception as exc:                                    # noqa: BLE001
        log("redirector cleanup skipped ({0}) - harmless, the renames stand".format(exc))

    # Only now - every rename succeeded - repoint the ini.
    proj = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
    ini_path = os.path.join(proj, INI_RELATIVE)
    if not os.path.isfile(ini_path):
        fail("ini not found at {0}".format(ini_path))
        sys.exit(3)
    with open(ini_path, "r", encoding="utf-8") as f:
        text = f.read()
    if INI_NEW in text:
        log("ini ALREADY points at {0}".format(INI_NEW))
    elif INI_OLD in text:
        with open(ini_path, "w", encoding="utf-8") as f:
            f.write(text.replace(INI_OLD, INI_NEW))
        log("ini repointed: {0} -> {1}".format(INI_OLD, INI_NEW))
    else:
        fail("ini contains neither the old nor the new GlobalDefaultGameMode value - "
             "renames SUCCEEDED but the ini needs a human. Check line "
             "GlobalDefaultGameMode= in Config/DefaultEngine.ini.")
        sys.exit(4)

    unreal.EditorAssetLibrary.save_directory("/Game/Core", False, True)
    unreal.EditorAssetLibrary.save_directory("/Game/Characters", False, True)
    log("DONE - renamed {0}, already-conforming {1}".format(len(renamed), len(already)))
    log("NEXT: re-run Tools/audit_blueprints/audit-blueprints.ps1 - condition 5 should "
        "now be clean for all five.")


if __name__ == "__main__":
    main()
