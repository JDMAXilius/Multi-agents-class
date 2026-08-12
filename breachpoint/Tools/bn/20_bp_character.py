"""BREACHPOINT NEXT - BP_BNCharacter defaults-only child + read-back audit (Roadmap 1, Goal 7.1/6.5).

Run inside the UE 5.8 editor (Python Editor Script Plugin):

    py "Tools/bn/20_bp_character.py"

Packet run order: 20_bp_character.py -> 30_reparent_abp.py -> 40_unarmed_layer.py.

Creates /Game/BN/BP_BNCharacter as a Blueprint child of the C++ ABNCharacter if
absent (R26: defaults only - zero graphs, zero new logic). On the CDO: the 3P
mannequin mesh on the inherited character mesh with owner_no_see, the anim class
pointed at the ABP that 30_reparent_abp.py will migrate (same CONFIG, read out of
that file so the two scripts cannot disagree), and a 1P arms component ONLY IF a
first-person arms skeletal mesh actually exists on disk (none does today - the
search runs live so the script converges the day one lands). Compiles, saves,
then reads every value back and prints an intent-vs-actual diff table. The audit
table is the proof, not "the script ran". Idempotent: a second run finds
everything set and re-audits.

Exits non-zero if the audit diffs, so a caller can gate on it.
"""
import ast
import os
import sys

import unreal

BP_PACKAGE_PATH = "/Game/BN"
BP_NAME = "BP_BNCharacter"
BP_PATH = BP_PACKAGE_PATH + "/" + BP_NAME
PARENT_CLASS_PATH = "/Script/BreachpointNext.BNCharacter"
MESH_3P_PATH = "/Game/FPSTemplate/Demo/Characters/Heroes/Mannequin/Meshes/SK_Mannequin"
ARMS_SEARCH_ROOTS = ("/Game/FPSTemplate", "/Game/FirstPerson")
ARMS_NAME_TOKENS = ("arms", "hands")
ARMS_COMPONENT_NAME = "Mesh1P"
# The C++ subobject is named CameraComponent (BNCharacter.cpp); "Camera" kept for
# tolerance if the C++ name ever shortens.
CAMERA_COMPONENT_NAMES = ("Camera", "CameraComponent")
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


def _find_arms_mesh():
    candidates = [p for p in _find_assets("SkeletalMesh", ARMS_SEARCH_ROOTS)
                  if any(t in p.rsplit("/", 1)[-1].lower() for t in ARMS_NAME_TOKENS)]
    return candidates[0] if candidates else None


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


# --- 1P arms component through SubobjectDataSubsystem (the 5.x SCS surface) ------

def _gather(bp):
    sds = unreal.get_editor_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    out = []
    for handle in sds.k2_gather_subobject_data_for_blueprint(bp):
        data = sds.k2_find_subobject_data_from_handle(handle)
        out.append((handle, str(lib.get_variable_name(data)), lib.get_object(data)))
    return sds, out


def _find_named(entries, name):
    for handle, var_name, obj in entries:
        if var_name == name:
            return handle, obj
    return None, None


def _ensure_arms_component(bp, arms_mesh):
    """Returns (component_template or None, note). Creation failure is reported,
    never raised - the audit rows carry the verdict."""
    try:
        sds, entries = _gather(bp)
        _handle, existing = _find_named(entries, ARMS_COMPONENT_NAME)
        if existing is None:
            parent_handle, note = None, "attached under camera"
            for handle, var_name, obj in entries:
                if isinstance(obj, unreal.CameraComponent) \
                        and var_name in CAMERA_COMPONENT_NAMES:
                    parent_handle = handle
                    break
            if parent_handle is None:
                # No discoverable camera: root attach, called out in the audit.
                parent_handle, note = entries[0][0], "attached under root (no camera found)"
            params = unreal.AddNewSubobjectParams(
                parent_handle=parent_handle,
                new_class=unreal.SkeletalMeshComponent,
                blueprint_context=bp)
            result = sds.add_new_subobject(params)
            new_handle = result[0] if isinstance(result, tuple) else result
            sds.rename_subobject(new_handle, unreal.Text(ARMS_COMPONENT_NAME))
            _sds, entries = _gather(bp)
            _handle, existing = _find_named(entries, ARMS_COMPONENT_NAME)
            if existing is None:
                fail = result[1] if isinstance(result, tuple) and len(result) > 1 else "?"
                return None, "add_new_subobject produced no component (%s)" % fail
        else:
            note = "already present"
        _set_skeletal_mesh(existing, arms_mesh)
        existing.set_editor_property("only_owner_see", True)
        return existing, note
    except Exception as err:
        return None, "SubobjectDataSubsystem path failed: %s" % err


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
    mesh_comp.set_editor_property("owner_no_see", True)
    if abp_class is not None:
        mesh_comp.set_editor_property("animation_mode",
                                      unreal.AnimationMode.ANIMATION_BLUEPRINT)
        try:
            mesh_comp.set_editor_property("anim_class", abp_class)
        except Exception:
            mesh_comp.set_anim_instance_class(abp_class)

    arms_mesh_path = _find_arms_mesh()
    arms_comp, arms_note = None, "skipped - no 1P arms skeletal mesh under %s" \
        % (ARMS_SEARCH_ROOTS,)
    if arms_mesh_path is not None:
        arms_mesh = unreal.EditorAssetLibrary.load_asset(arms_mesh_path)
        arms_comp, arms_note = _ensure_arms_component(bp, arms_mesh)

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
        ("3P owner_no_see", "True",
         str(mesh_comp.get_editor_property("owner_no_see"))),
        ("3P anim class (per %s)" % REPARENT_SCRIPT, abp_intent,
         actual_anim.get_path_name() if actual_anim else
         ("none - no candidate ABP on disk" if abp_path is None else "None")),
    ]

    _sds, entries = _gather(bp)
    _handle, arms_actual = _find_named(entries, ARMS_COMPONENT_NAME)
    if arms_mesh_path is None:
        rows.append(("1P arms component", "absent (no arms mesh found by search)",
                     "absent (no arms mesh found by search)" if arms_actual is None
                     else "present unexpectedly"))
    else:
        rows.append(("1P arms component (%s)" % arms_note, "present",
                     "present" if arms_actual is not None else "absent"))
        if arms_actual is not None:
            actual_arms_mesh = _get_skeletal_mesh(arms_actual)
            rows.append(("1P arms mesh asset",
                         "%s.%s" % (arms_mesh_path, arms_mesh_path.rsplit("/", 1)[-1]),
                         actual_arms_mesh.get_path_name() if actual_arms_mesh
                         else "None"))
            rows.append(("1P only_owner_see", "True",
                         str(arms_actual.get_editor_property("only_owner_see"))))

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
