"""BREACHPOINT NEXT - BP_BNCharacter defaults-only child + read-back audit (Roadmap 1, Goal 7.1/6.5).

Run inside the UE 5.8 editor (Python Editor Script Plugin):

    py "Tools/bn/20_bp_character.py"

Packet run order: 20_bp_character.py -> 30_reparent_abp.py -> 40_unarmed_layer.py.

Creates /Game/BN/BP_BNCharacter as a Blueprint child of the C++ ABNCharacter if
absent (R26: defaults only - zero graphs, zero new logic). On the CDO: the
mannequin mesh on the inherited character mesh with owner_no_see FALSE (true
first person - the founder's animation set is full-body, the owner sees their
own mannequin), and the anim class pointed at the ABP that 30_reparent_abp.py
will migrate (same CONFIG, read out of that file so the two scripts cannot
disagree). The former 1P-arms component branch was replaced by true-FP
full-body. Compiles, saves, then reads every value back and prints an
intent-vs-actual diff table. The audit table is the proof, not "the script
ran". Idempotent: a second run finds everything set and re-audits.

Exits non-zero if the audit diffs, so a caller can gate on it.
"""
import ast
import os
import sys

import unreal

# /Game/BN/<Domain> mirrors Source/BreachpointNext/<Domain> - docs/BREACHPOINT-NEXT-CONTENT-LAYOUT.md
BP_PACKAGE_PATH = "/Game/BN/Characters"
BP_NAME = "BP_BNCharacter"
BP_PATH = BP_PACKAGE_PATH + "/" + BP_NAME
PARENT_CLASS_PATH = "/Script/BreachpointNext.BNCharacter"
# SKM_Manny, not SK_Mannequin: the latter is at the same path but is the SKELETON, and
# assigning it to SkeletalMeshAsset is refused ("not valid SkeletalMesh"). Confirmed live
# via AssetTools.get_asset_class -> "Skeleton".
MESH_3P_PATH = "/Game/FPSTemplate/Demo/Characters/Heroes/Mannequin/Meshes/SKM_Manny"
# True-FP full-body replaced the former 1P-arms component branch (founder ruling).
REPARENT_SCRIPT = "30_reparent_abp.py"

AUDIT_FMT = "%-32s | %-52s | %-52s | %s"


def _read_shared_config():
    """CONFIG is single-source in 30_reparent_abp.py; parsed with ast rather than
    imported, because that script's top level runs the ABP migration."""
    try:
        here = os.path.dirname(os.path.abspath(__file__))
    except NameError:
        here = os.path.join(
            unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()),
            "Tools", "bn")
    path = os.path.join(here, REPARENT_SCRIPT)
    with open(path, "r", encoding="utf-8") as f:
        tree = ast.parse(f.read())
    for node in tree.body:
        if isinstance(node, ast.Assign):
            for tgt in node.targets:
                if isinstance(tgt, ast.Name) and tgt.id == "CONFIG":
                    return ast.literal_eval(node.value)
    raise RuntimeError("CONFIG dict not found in " + path)


def _find_assets(class_name, roots):
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    try:
        filt = unreal.ARFilter(
            class_paths=[unreal.TopLevelAssetPath("/Script/Engine", class_name)],
            package_paths=list(roots), recursive_paths=True)
        assets = ar.get_assets(filt)
    except Exception:
        # class_names is the pre-5.1 spelling; kept as the fallback only.
        filt = unreal.ARFilter(class_names=[class_name],
                               package_paths=list(roots), recursive_paths=True)
        assets = ar.get_assets(filt)
    return sorted(str(a.package_name) for a in assets)


def choose_mannequin_abp(config):
    """Deterministic ABP choice - duplicated verbatim from 30_reparent_abp.py and
    driven by that file's CONFIG (read via ast above), so both scripts always
    agree on the target asset."""
    # 30 now works on a BN-owned DUPLICATE and leaves the source to BP82. Once that
    # duplicate exists it IS the anim class - pointing the character at the FPSTemplate
    # original would hand it the un-reparented brain.
    bn = config.get("bn_duplicate")
    if bn and unreal.EditorAssetLibrary.does_asset_exist(bn):
        return bn, "bn_duplicate"
    for key in ("abp_preferred", "abp_fallback"):
        path = config[key]
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            return path, key
    found = _find_assets("AnimBlueprint", [config["abp_search_root"]])
    rejects = ("animlayers", "posecreator", "copypose", "retarget", "topdown",
               "postprocess", "target", "ali_")
    found = [p for p in found
             if not any(r in p.rsplit("/", 1)[-1].lower() for r in rejects)]
    if found:
        return found[0], "abp_search_root"
    return None, None


def _set_skeletal_mesh(component, mesh):
    try:
        component.set_editor_property("skeletal_mesh_asset", mesh)  # 5.1+ name
    except Exception:
        component.set_editor_property("skeletal_mesh", mesh)


def _get_skeletal_mesh(component):
    for prop in ("skeletal_mesh_asset", "skeletal_mesh"):
        try:
            return component.get_editor_property(prop)
        except Exception:
            continue
    return None


def main():
    config = _read_shared_config()

    parent_class = unreal.load_class(None, PARENT_CLASS_PATH)
    if parent_class is None:
        unreal.log_error(
            "CLASS LOAD FAILED: %s - the BreachpointNext C++ module is not "
            "compiled (or BNCharacter does not exist in it). Compile the module "
            "and re-run. Nothing was created or saved." % PARENT_CLASS_PATH)
        return 1

    mesh_3p = unreal.EditorAssetLibrary.load_asset(MESH_3P_PATH)
    if mesh_3p is None:
        unreal.log_error("MESH LOAD FAILED: %s. Nothing was created or saved."
                         % MESH_3P_PATH)
        return 1

    abp_path, abp_rule = choose_mannequin_abp(config)
    abp_class = None
    if abp_path is not None:
        abp_asset = unreal.EditorAssetLibrary.load_asset(abp_path)
        if abp_asset is not None:
            abp_class = abp_asset.get_editor_property("generated_class")

    if not unreal.EditorAssetLibrary.does_asset_exist(BP_PATH):
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent_class)
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            BP_NAME, BP_PACKAGE_PATH, None, factory)
        if bp is None:
            unreal.log_error("CREATE FAILED: %s" % BP_PATH)
            return 1
    else:
        bp = unreal.EditorAssetLibrary.load_asset(BP_PATH)

    # Converge a wrong parent instead of failing - idempotence over precondition.
    current_parent = bp.get_editor_property("parent_class")
    if current_parent is None \
            or current_parent.get_path_name() != parent_class.get_path_name():
        unreal.BlueprintEditorLibrary.reparent_blueprint(bp, parent_class)

    cdo = unreal.get_default_object(bp.get_editor_property("generated_class"))
    mesh_comp = cdo.get_editor_property("mesh")  # ACharacter's CharacterMesh0
    _set_skeletal_mesh(mesh_comp, mesh_3p)
    # True first person: full-body, the owner sees their own mannequin. Converges the
    # old True the earlier runs saved on the CDO - the C++ default alone cannot clear it.
    mesh_comp.set_editor_property("owner_no_see", False)
    if abp_class is not None:
        mesh_comp.set_editor_property("animation_mode",
                                      unreal.AnimationMode.ANIMATION_BLUEPRINT)
        try:
            mesh_comp.set_editor_property("anim_class", abp_class)
        except Exception:
            mesh_comp.set_anim_instance_class(abp_class)

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    if not unreal.EditorAssetLibrary.save_asset(BP_PATH):
        unreal.log_error("SAVE FAILED: %s" % BP_PATH)
        return 1

    # --- read-back audit ---
    bp = unreal.EditorAssetLibrary.load_asset(BP_PATH)
    actual_parent = bp.get_editor_property("parent_class")
    cdo = unreal.get_default_object(bp.get_editor_property("generated_class"))
    mesh_comp = cdo.get_editor_property("mesh")
    actual_mesh = _get_skeletal_mesh(mesh_comp)
    actual_anim = mesh_comp.get_editor_property("anim_class")
    status = bp.get_editor_property("status")

    if abp_path is not None:
        abp_intent = "%s.%s_C" % (abp_path, abp_path.rsplit("/", 1)[-1])
    else:
        abp_intent = "none - no candidate ABP on disk"

    rows = [
        ("BP exists on disk", "True",
         str(unreal.EditorAssetLibrary.does_asset_exist(BP_PATH))),
        ("parent class", PARENT_CLASS_PATH,
         actual_parent.get_path_name() if actual_parent else "None"),
        ("3P mesh asset", "%s.%s" % (MESH_3P_PATH, MESH_3P_PATH.rsplit("/", 1)[-1]),
         actual_mesh.get_path_name() if actual_mesh else "None"),
        ("mesh owner_no_see (true FP, full-body)", "False",
         str(mesh_comp.get_editor_property("owner_no_see"))),
        ("anim class (per %s)" % REPARENT_SCRIPT, abp_intent,
         actual_anim.get_path_name() if actual_anim else
         ("none - no candidate ABP on disk" if abp_path is None else "None")),
    ]

    rows.append(("compiles clean", "True",
                 str(status == unreal.BlueprintStatus.BS_UP_TO_DATE)))

    unreal.log("== AUDIT: %s ==" % BP_PATH)
    unreal.log("ABP choice rule: %s" % (abp_rule if abp_rule else "none matched"))
    unreal.log(AUDIT_FMT % ("CHECK", "INTENT", "ACTUAL", "STATUS"))
    diffs = 0
    for name, intent, actual in rows:
        ok = intent == actual
        diffs += 0 if ok else 1
        unreal.log(AUDIT_FMT % (name, intent, actual, "OK" if ok else "DIFF"))
    unreal.log("== AUDIT %s: %d/%d checks =="
               % ("PASSED" if diffs == 0 else "FAILED", len(rows) - diffs, len(rows)))
    return 0 if diffs == 0 else 1


sys.exit(main())
