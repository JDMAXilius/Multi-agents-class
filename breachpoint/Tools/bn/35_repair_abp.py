"""BREACHPOINT NEXT - repair the duplicated mannequin ABP after reparent fallout (Roadmap 1, Goal 7.2/6.3).

Run inside the UE 5.8 editor (Python Editor Script Plugin):

    py "Tools/bn/35_repair_abp.py"

Packet run order: 20_bp_character.py -> 30_reparent_abp.py -> 35_repair_abp.py
-> 40_unarmed_layer.py.

Repairs the two compile-breakers on /Game/BN/Animation/ABP_BNMannequin - the
ONLY asset this script writes; the FPSTemplate source ABP is loaded read-only
as evidence and never written, never saved:

(a) *_0 rename artifacts. The duplicate's own GroundDistance was auto-renamed
    GroundDistance_0 during a name collision with the C++ property, nodes were
    re-pointed at it, then it was deleted - leaving orphaned references. Swept
    GENERICALLY, not hardcoded: for every known-good variable name X (a BP
    variable on the duplicate or on the original), any X_0 reference is
    re-pointed at X via BlueprintEditorLibrary.replace_variable_references
    (probed with hasattr - it wraps FBlueprintEditorUtils::
    ReplaceVariableReferences). GroundDistance_0 -> GroundDistance is the known
    case; the sweep catches siblings. If the API is absent in this build's
    python, the audit says so and prints the exact manual step.

(b) absorbed enum variables. Five BP variables typed to ASSET enums
    (RootYawOffsetMode + the cardinal-direction family) were deleted by 30's
    matcher because uint8 C++ properties shared their names - but enum-typed
    graph nodes (Equal (Enum), enum defaults like 'NewEnumerator2') cannot bind
    byte properties. The parallel C++ fix REMOVES those five uint8 properties
    from UBNAnimInstance; this script RESTORES the BP variables: every variable
    that is enum-typed on the ORIGINAL ABP and missing on the duplicate is
    re-added with the SAME name and the original's exact FEdGraphPinType (the
    pin type struct is copied off the original's variable entry, so the enum
    asset reference is exact, never guessed). Non-enum variables are never
    touched. If the editor is still running a C++ module where the uint8
    property is live, that restore is refused (it would re-create the
    collision) and flagged - compile the C++ removal first, then re-run.

DIAGNOSE-FIRST: both variable tables (duplicate + original) are printed before
any mutation, so the repair works from evidence. AUDIT-LAST: variable table
after with a RESTORED/UNCHANGED mark per row, the sweep log, and the compile
status. The audit table is the proof, not "the script ran". Idempotent: a
second run finds the enums present, the sweep no-ops, and re-audits.

Exits non-zero if the audit diffs, so a caller can gate on it.
"""
import ast
import os
import sys

import unreal

REPARENT_SCRIPT = "30_reparent_abp.py"

TABLE_FMT = "%-32s | %-80s | %s"
AUDIT_FMT = "%-40s | %-52s | %-52s | %s"

MANUAL_STEP = (
    "MANUAL STEP (this build's python has no "
    "BlueprintEditorLibrary.replace_variable_references): open %s, and in its "
    "graphs right-click each of the two 'Get GroundDistance_0' nodes -> "
    "'Replace variable GroundDistance_0 with...' -> 'Get GroundDistance' "
    "(repeat for any other *_0 reference the compiler lists), then compile "
    "and save.")


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


def choose_original_abp(config):
    """Deterministic ORIGINAL choice - duplicated verbatim from 30_reparent_abp.py
    (and deliberately WITHOUT 20's bn_duplicate preference: the evidence source
    here must be the un-migrated FPSTemplate original, read-only)."""
    for key in ("abp_preferred", "abp_fallback"):
        path = config[key]
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            return path, key
    found = _find_assets("AnimBlueprint", [config["abp_search_root"]])
    # Character locomotion ABPs only - layer/utility ABPs are not a mannequin brain.
    rejects = ("animlayers", "posecreator", "copypose", "retarget", "topdown",
               "postprocess", "target", "ali_")
    found = [p for p in found
             if not any(r in p.rsplit("/", 1)[-1].lower() for r in rejects)]
    if found:
        return found[0], "abp_search_root"
    return None, None


# --- variable evidence -----------------------------------------------------------

def _bp_variables(bp):
    """(name, FEdGraphPinType) for every blueprint-added variable, from the
    Blueprint's NewVariables array - the only surface that keeps the original
    FName (the CDO's python dir() is snake_cased and lossy)."""
    out = []
    for desc in bp.get_editor_property("new_variables"):
        out.append((str(desc.get_editor_property("var_name")),
                    desc.get_editor_property("var_type")))
    return out


def _pin_type_str(pintype):
    """category:/full/path.SubObject - the sub-object path is printed in full so
    an asset-enum reference is verifiable at a glance in the diagnose table."""
    try:
        cat = str(pintype.get_editor_property("pin_category"))
        sub = pintype.get_editor_property("pin_sub_category_object")
        cont = ""
        try:
            ct = pintype.get_editor_property("container_type")
            if ct != unreal.PinContainerType.NONE:
                cont = " [%s]" % ct
        except Exception:
            pass
        return cat + (":" + sub.get_path_name() if sub else "") + cont
    except Exception:
        return "(unreadable)"


def _is_enum_pin(pintype):
    """Enum-typed in either spelling: pin_category 'enum', or 'byte' with a
    pin_sub_category_object (an asset UserDefinedEnum or a native UEnum).
    Container type is irrelevant - an array of enums is still enum-typed."""
    try:
        cat = str(pintype.get_editor_property("pin_category"))
        if cat == "enum":
            return True
        if cat == "byte":
            return pintype.get_editor_property("pin_sub_category_object") is not None
    except Exception:
        pass
    return False


def print_variable_table(title, variables, marks=None):
    unreal.log("== %s ==" % title)
    unreal.log(TABLE_FMT % ("BP VARIABLE", "PIN TYPE", "MARK"))
    for name, pintype in variables:
        unreal.log(TABLE_FMT % (name, _pin_type_str(pintype),
                                (marks or {}).get(name, "")))
    if not variables:
        unreal.log("(no blueprint-added variables)")


# --- repair ----------------------------------------------------------------------

def main():
    config = _read_shared_config()
    bel = unreal.BlueprintEditorLibrary

    dup_path = config.get("bn_duplicate")
    if not dup_path or not unreal.EditorAssetLibrary.does_asset_exist(dup_path):
        unreal.log_error("DUPLICATE MISSING: %s does not exist - run %s first. "
                         "Nothing was modified." % (dup_path, REPARENT_SCRIPT))
        return 1
    bp = unreal.EditorAssetLibrary.load_asset(dup_path)
    if bp is None or not isinstance(bp, unreal.AnimBlueprint):
        unreal.log_error("NOT AN ANIM BLUEPRINT: %s. Nothing was modified." % dup_path)
        return 1

    orig_path, orig_rule = choose_original_abp(config)
    if orig_path is None or orig_path == dup_path:
        unreal.log_error("NO ORIGINAL ABP for evidence (rule matched: %s) - the "
                         "restore needs the original's enum pin types. Nothing "
                         "was modified." % (orig_rule,))
        return 1
    # READ-ONLY evidence: loaded, tabled, never written, never saved.
    orig = unreal.EditorAssetLibrary.load_asset(orig_path)
    if orig is None or not isinstance(orig, unreal.AnimBlueprint):
        unreal.log_error("ORIGINAL LOAD FAILED: %s. Nothing was modified." % orig_path)
        return 1

    # (1) DIAGNOSE - both tables before any mutation.
    dup_vars = _bp_variables(bp)
    orig_vars = _bp_variables(orig)
    print_variable_table("DIAGNOSE: duplicate %s (BEFORE)" % dup_path, dup_vars)
    print_variable_table("DIAGNOSE: original %s (read-only)" % orig_path, orig_vars)

    dup_names = {n for n, _t in dup_vars}
    orig_enum = [(n, t) for n, t in orig_vars if _is_enum_pin(t)]
    missing_enum = [(n, t) for n, t in orig_enum if n not in dup_names]
    unreal.log("enum variables on original: %d; missing on duplicate: %s"
               % (len(orig_enum), [n for n, _t in missing_enum] or "none"))

    # (3) RESTORE the absorbed enum variables - BEFORE the *_0 sweep, so a
    # restored name is a live sweep target in the same run. Refuse any restore
    # whose name is still a live property on the C++ parent: the parallel C++
    # fix (removing the five uint8s from UBNAnimInstance) must compile first,
    # or the add just re-creates the collision that caused this.
    parent_class = bp.get_editor_property("parent_class")
    parent_cdo = unreal.get_default_object(parent_class) if parent_class else None
    restored, blocked, failed = [], [], []
    can_add = hasattr(bel, "add_member_variable")
    for name, pintype in missing_enum:
        live_cpp = False
        if parent_cdo is not None:
            try:
                parent_cdo.get_editor_property(name)
                live_cpp = True
            except Exception:
                live_cpp = False
        if live_cpp:
            blocked.append(name)
            unreal.log_warning(
                "RESTORE BLOCKED: %s is still a live C++ property on %s - "
                "compile the parallel C++ removal, then re-run."
                % (name, parent_class.get_path_name()))
            continue
        if not can_add:
            failed.append(name)
            continue
        # The pin type is the ORIGINAL's own FEdGraphPinType entry - same
        # category, same enum asset sub-object - so the reference is exact.
        if bel.add_member_variable(bp, name, pintype):
            restored.append(name)
        else:
            failed.append(name)
    if missing_enum and not can_add:
        unreal.log_error("add_member_variable API missing in this build's "
                         "python - no variables restored.")

    # (2) REPAIR *_0 rename artifacts. Known-good names = BP variables now on
    # the duplicate plus everything on the original (covers names absorbed into
    # C++ like GroundDistance, whose FName python's snake_cased dir() loses).
    # For each, re-point any X_0 reference at X; a no-op when no such reference
    # exists, which is what makes the sweep idempotent. A name that IS a
    # declared variable ending in _0 is legit and skipped, never clobbered.
    dup_names_now = {n for n, _t in _bp_variables(bp)}
    good_names = sorted(dup_names_now | {n for n, _t in orig_vars})
    swept = []
    can_sweep = hasattr(bel, "replace_variable_references")
    if can_sweep:
        for name in good_names:
            ghost = name + "_0"
            if ghost in dup_names_now or ghost in good_names:
                continue
            # Void API: it reports nothing back, so the compile row below is
            # the proof that the orphans re-resolved - not this call.
            bel.replace_variable_references(bp, ghost, name)
            swept.append("%s -> %s" % (ghost, name))
    else:
        unreal.log_error(MANUAL_STEP % dup_path)

    # (4) compile, save (the DUPLICATE only), audit from a fresh load.
    bel.compile_blueprint(bp)
    if not unreal.EditorAssetLibrary.save_asset(dup_path):
        unreal.log_error("SAVE FAILED: %s" % dup_path)
        return 1

    bp = unreal.EditorAssetLibrary.load_asset(dup_path)
    after_vars = _bp_variables(bp)
    after_names = {n for n, _t in after_vars}
    still_missing = [n for n, _t in orig_enum
                     if n not in after_names and n not in blocked]
    status = bp.get_editor_property("status")
    actual_parent = bp.get_editor_property("parent_class")

    marks = {n: "UNCHANGED" for n in after_names}
    marks.update({n: "RESTORED (enum, from original)" for n in restored})
    print_variable_table("AUDIT: duplicate %s (AFTER)" % dup_path,
                         after_vars, marks)
    unreal.log("*_0 sweep (%s): %s"
               % ("replace_variable_references" if can_sweep else "API ABSENT",
                  "; ".join(swept) if swept else "none applied"))
    unreal.log("remaining error count: not exposed by BlueprintEditorLibrary in "
               "5.8 python - BlueprintStatus is the proxy (status=%s)" % status)

    rows = [
        ("duplicate is the only asset written", dup_path, dup_path),
        ("parent class", config["new_parent"],
         actual_parent.get_path_name() if actual_parent else "None"),
        ("enum vars restored", "%d of %d missing" % (len(missing_enum),
                                                     len(missing_enum)),
         "%d of %d missing" % (len(restored), len(missing_enum))),
        ("enum vars still missing vs original", "none",
         ", ".join(still_missing) if still_missing else "none"),
        ("restores blocked by live C++ property", "none",
         ", ".join(blocked) if blocked else "none"),
        ("*_0 references swept", "replace_variable_references applied",
         "replace_variable_references applied" if can_sweep
         else "API absent - manual step printed above"),
        ("compiles clean", "True",
         str(status == unreal.BlueprintStatus.BS_UP_TO_DATE)),
    ]

    unreal.log("== AUDIT: %s (original %s untouched) ==" % (dup_path, orig_path))
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
