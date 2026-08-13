"""BREACHPOINT NEXT - reparent the mannequin ABP to UBNAnimInstance (Roadmap 1, Goal 7.2/6.3).

Run inside the UE 5.8 editor (Python Editor Script Plugin):

    py "Tools/bn/30_reparent_abp.py"

Packet run order: 20_bp_character.py -> 30_reparent_abp.py -> 40_unarmed_layer.py.

DUPLICATE-FIRST, REPORT-FIRST migration. It picks the mannequin ABP (prefer
ABP_BRMannequin3P, else the FPSTemplate character ABP) and **duplicates it** to
/Game/BN/Animation/ABP_BNMannequin. Everything after that happens on the duplicate:
the source is read and never written, which is what leaves ABP_BRMannequin3P and all
of FPSTemplate/ to BP82.

On the duplicate it prints a full matched/unmatched variable table (every BP variable
vs every property the new C++ parent exposes), reparents to
/Script/BreachpointNext.BNAnimInstance, and deletes ONLY the BP variables whose name
AND type provably match a C++ property, so those graph nodes re-resolve to the
inherited C++ property. The rest are listed as NEEDS DECISION.

THE EVENT GRAPH IS PRESERVED. See ALLOW_GRAPH_CLEAR below - it is off, and stays off
until the ubergraph has been ported to C++ 1:1 and verified by a human. Deleting
matched variables does not lose logic; deleting the graph does, and there is no other
copy of it.

Compiles, saves, then reads everything back and prints an intent-vs-actual audit
table. The audit table is the proof, not "the script ran". Idempotent: a second run
finds the duplicate present, the parent already correct, and re-audits.

Exits non-zero if the audit diffs, so a caller can gate on it.
"""
import sys

import unreal

# Shared with 20_bp_character.py, which parses this dict straight out of this file
# (ast, not import - this script's top level runs a migration). Single source of
# truth: both scripts always name the same ABP.
CONFIG = {
    "abp_preferred": "/Game/Characters/ABP_BRMannequin3P",
    "abp_fallback": "/Game/FPSTemplate/Demo/Characters/Heroes/Mannequin/Animations/ABP_Mannequin_Base",
    "abp_search_root": "/Game/FPSTemplate",
    "new_parent": "/Script/BreachpointNext.BNAnimInstance",
    # The DUPLICATE is what BN owns and mutates. The source above is read only, which
    # is what keeps ABP_BRMannequin3P and everything under FPSTemplate/ BP82's.
    "bn_duplicate": "/Game/BN/Animation/ABP_BNMannequin",
}

# THE FOUNDER'S RULE, 12 Aug 2026: the event graph is not cleared until its logic has been
# moved to C++ 1:1 AND verified. A matched variable list is NOT that bar - it says nothing
# about whether the ubergraph maths (cardinal direction, root-yaw offset, spring interp)
# has been ported. Flipping this to True before that port is done silently destroys the
# only copy of that logic. It stays False until a human has done the move.
ALLOW_GRAPH_CLEAR = False

AUDIT_FMT = "%-32s | %-52s | %-52s | %s"


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
    """Deterministic ABP choice - duplicated verbatim in 20_bp_character.py and
    driven by the CONFIG above (which 20 reads out of this file), so both scripts
    always agree on the target asset."""
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


# --- variable report -----------------------------------------------------------

# BP pin categories whose CDO default value on the C++ side proves the type.
_PIN_TO_VALUE_TYPES = {
    "bool": ("bool",),
    "float": ("float",), "real": ("float",), "double": ("float",),
    "int": ("int",), "int64": ("int",),
    "name": ("Name",), "string": ("str", "String"), "text": ("Text",),
}


def _pin_type_str(pintype):
    try:
        cat = str(pintype.get_editor_property("pin_category"))
        sub = pintype.get_editor_property("pin_sub_category_object")
        return cat + (":" + sub.get_name() if sub else "")
    except Exception:
        return "(unreadable)"


def _type_proof(pintype, cpp_value):
    """True only when the BP pin type PROVABLY equals the C++ property's type,
    judged from the CDO value - anything unprovable stays unmatched (defensive:
    we delete only what we can prove)."""
    try:
        if pintype.get_editor_property("container_type") != unreal.PinContainerType.NONE:
            return False
    except Exception:
        pass  # older reflection without container_type: fall through to category
    try:
        cat = str(pintype.get_editor_property("pin_category"))
        vtype = type(cpp_value).__name__
        if cat in _PIN_TO_VALUE_TYPES:
            return vtype in _PIN_TO_VALUE_TYPES[cat]
        sub = None
        try:
            sub = pintype.get_editor_property("pin_sub_category_object")
        except Exception:
            pass
        # THE FOUNDER'S RULING, 13 Aug 2026: enum variables stay BP-owned until
        # graph-clear day (the UENUM port and the graph retype land together).
        # A uint8 C++ property can hold the value, but enum-typed graph nodes
        # (Equal (Enum), enum defaults like 'NewEnumerator2') cannot bind a byte
        # property - byte-vs-enum is NEVER a match. Any enum pin (asset enum or
        # otherwise) is permanently excluded from absorption/deletion here; the
        # 35_repair_abp.py restore exists because this exclusion once didn't.
        if cat == "enum" or (cat == "byte" and sub is not None):
            return False
        if cat == "struct" and sub is not None:
            return sub.get_name() == vtype
    except Exception:
        return False
    return False  # object/class refs: a None default proves nothing - needs decision


def _bp_variables(bp):
    """(name, FEdGraphPinType) for every blueprint-added variable, from the
    Blueprint's NewVariables array - the only surface that keeps the original
    FName (the CDO's python dir() is snake_cased and lossy)."""
    out = []
    for desc in bp.get_editor_property("new_variables"):
        out.append((str(desc.get_editor_property("var_name")),
                    desc.get_editor_property("var_type")))
    return out


def _cpp_exposed(new_parent_class):
    """Properties BNAnimInstance introduces beyond UAnimInstance, via dir() diff
    of the two CDOs filtered to real reflected properties."""
    cdo = unreal.get_default_object(new_parent_class)
    base = unreal.get_default_object(unreal.AnimInstance)
    out = []
    for n in sorted(set(dir(cdo)) - set(dir(base))):
        if n.startswith("_"):
            continue
        try:
            out.append((n, type(cdo.get_editor_property(n)).__name__))
        except Exception:
            continue
    return out


def classify(bp, new_parent_class):
    cdo = unreal.get_default_object(new_parent_class)
    matched, unmatched = [], []
    for name, pintype in _bp_variables(bp):
        try:
            cpp_value = cdo.get_editor_property(name)
        except Exception:
            unmatched.append((name, pintype, "no C++ property with this name"))
            continue
        if _type_proof(pintype, cpp_value):
            matched.append((name, pintype))
        else:
            unmatched.append((name, pintype,
                              "name exists on C++ parent but type not provable"))
    return matched, unmatched


def print_variable_report(abp_path, matched, unmatched, cpp_props):
    unreal.log("== VARIABLE REPORT: %s vs %s ==" % (abp_path, CONFIG["new_parent"]))
    unreal.log("%-32s | %-28s | %s" % ("BP VARIABLE", "PIN TYPE", "VERDICT"))
    for name, pintype in matched:
        unreal.log("%-32s | %-28s | MATCHED (delete -> re-resolves to C++)"
                   % (name, _pin_type_str(pintype)))
    for name, pintype, why in unmatched:
        unreal.log("%-32s | %-28s | NEEDS DECISION - %s"
                   % (name, _pin_type_str(pintype), why))
    if not matched and not unmatched:
        unreal.log("(ABP declares no blueprint variables)")
    unreal.log("-- C++ parent exposes beyond UAnimInstance:")
    for n, t in cpp_props:
        unreal.log("   %s (%s)" % (n, t))
    if not cpp_props:
        unreal.log("   (none)")


# --- migration -----------------------------------------------------------------

def _try_clear_event_graph(bp):
    """BlueprintEditorLibrary has no documented node-clearing API in 5.8; probe
    for graph-removal entry points and report honestly when none exist."""
    bel = unreal.BlueprintEditorLibrary
    if not hasattr(bel, "find_event_graph"):
        return False, "no find_event_graph API - graph left in place, FLAGGED"
    graph = bel.find_event_graph(bp)
    if graph is None:
        return True, "no event graph present"
    for fn in ("remove_graph",):
        if hasattr(bel, fn):
            try:
                getattr(bel, fn)(bp, graph)
                if bel.find_event_graph(bp) is None:
                    return True, "event graph removed via %s" % fn
            except Exception as err:
                return False, "%s failed (%s) - graph left, FLAGGED" % (fn, err)
    return False, "no graph-clearing API in this build - graph left, FLAGGED"


def main():
    abp_path, rule = choose_mannequin_abp(CONFIG)
    if abp_path is None:
        unreal.log_error("NO CANDIDATE ABP: neither %s nor %s exists and the "
                         "search under %s found nothing. Nothing was modified."
                         % (CONFIG["abp_preferred"], CONFIG["abp_fallback"],
                            CONFIG["abp_search_root"]))
        return 1

    new_parent = unreal.load_class(None, CONFIG["new_parent"])
    if new_parent is None:
        unreal.log_error(
            "CLASS LOAD FAILED: %s - the BreachpointNext C++ module is not "
            "compiled, or UBNAnimInstance is not written yet (bn-builder Goal "
            "6.1). Compile the module and re-run. Nothing was modified."
            % CONFIG["new_parent"])
        return 1

    source = unreal.EditorAssetLibrary.load_asset(abp_path)
    if source is None or not isinstance(source, unreal.AnimBlueprint):
        unreal.log_error("NOT AN ANIM BLUEPRINT: %s. Nothing was modified." % abp_path)
        return 1

    # Work on a BN-owned duplicate. The source ABP is never written - it belongs to BP82.
    target_path = CONFIG["bn_duplicate"]
    if unreal.EditorAssetLibrary.does_asset_exist(target_path):
        bp = unreal.EditorAssetLibrary.load_asset(target_path)
        unreal.log("DUPLICATE: %s already exists, converging it." % target_path)
    else:
        bp = unreal.EditorAssetLibrary.duplicate_asset(abp_path, target_path)
        if bp is None:
            unreal.log_error("DUPLICATE FAILED: %s -> %s. Nothing was modified."
                             % (abp_path, target_path))
            return 1
        unreal.log("DUPLICATE: %s -> %s (source untouched)" % (abp_path, target_path))
    abp_path = target_path

    # (b) report FIRST - the founder sees the full picture before any mutation.
    matched, unmatched = classify(bp, new_parent)
    print_variable_report(abp_path, matched, unmatched, _cpp_exposed(new_parent))

    # (c) reparent, idempotent.
    old_parent = bp.get_editor_property("parent_class")
    if old_parent is None or old_parent.get_path_name() != new_parent.get_path_name():
        unreal.BlueprintEditorLibrary.reparent_blueprint(bp, new_parent)

    # (d) delete only the proven matches.
    removed = []
    if matched and not hasattr(unreal.BlueprintEditorLibrary, "remove_member_variable"):
        unreal.log_error("remove_member_variable API missing - no variables "
                         "deleted; all %d matches left as NEEDS DECISION." % len(matched))
        unmatched = unmatched + [(n, t, "API missing") for n, t in matched]
        matched = []
    for name, _pintype in matched:
        unreal.BlueprintEditorLibrary.remove_member_variable(bp, name)
        removed.append(name)

    # (e) event graph. Preserved by default - see ALLOW_GRAPH_CLEAR at the top. The old
    # "clear it once every variable matched" behaviour is exactly the trapdoor that rule
    # closes: matched variables are not a ported ubergraph.
    if not ALLOW_GRAPH_CLEAR:
        graph_ok, graph_msg = True, ("PRESERVED - ALLOW_GRAPH_CLEAR is off until the "
                                     "ubergraph is ported to C++ 1:1 and verified")
    elif unmatched:
        graph_ok, graph_msg = True, ("left in place - %d unmatched variable(s) may "
                                     "be fed by it, FLAGGED" % len(unmatched))
    else:
        graph_ok, graph_msg = _try_clear_event_graph(bp)

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    if not unreal.EditorAssetLibrary.save_asset(abp_path):
        unreal.log_error("SAVE FAILED: %s" % abp_path)
        return 1

    # --- read-back audit ---
    bp = unreal.EditorAssetLibrary.load_asset(abp_path)
    actual_parent = bp.get_editor_property("parent_class")
    remaining = [n for n, _t in _bp_variables(bp)]
    still_present = [n for n in removed if n in remaining]
    status = bp.get_editor_property("status")

    rows = [
        ("ABP chosen (rule)", "%s (%s)" % (abp_path, rule),
         "%s (%s)" % (abp_path, rule)),
        ("parent class", CONFIG["new_parent"],
         actual_parent.get_path_name() if actual_parent else "None"),
        ("matched vars removed", "%d removed, 0 remaining" % len(removed),
         "%d removed, %d remaining"
         % (len(removed) - len(still_present), len(still_present))),
        ("unmatched vars (needs decision)", str(len(unmatched)),
         str(len(remaining) - len(still_present))),
        ("event graph", "cleared or accounted for",
         "cleared or accounted for" if graph_ok else graph_msg),
        ("compiles clean", "True",
         str(status == unreal.BlueprintStatus.BS_UP_TO_DATE)),
    ]

    unreal.log("== AUDIT: %s ==" % abp_path)
    unreal.log("event graph detail: %s" % graph_msg)
    for name, _pintype, why in [(n, t, w) for n, t, w in unmatched]:
        unreal.log("NEEDS DECISION: %s - %s" % (name, why))
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
