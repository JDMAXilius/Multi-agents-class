#!/usr/bin/env python3
# =====================================================================================
# BREACHPOINT - dump_blueprints.py :: the `unreal`-importing half of the R26 audit
#
# Serves:   DESIGN RULING R26 enforcement, cross-check pass. Runs INSIDE Unreal.
# Shape:    mirrors Tools/blockout (arena_plan.py = decisions, build_arena.py = executor).
#           Here: audit_r26.py holds every RULE; this file adds facts only the editor can
#           see, and then hands scoring back to audit_r26.py. It judges nothing itself.
#
# WHY IT IS A CROSS-CHECK AND NOT THE PRIMARY READER
#   The Unreal Python API does not expose UEdGraph::Nodes (a bare UPROPERTY, neither
#   editable nor BlueprintVisible), so the editor cannot hand us a node count directly.
#   The package header can, and does, without an engine. So the offline scan stays the
#   primary evidence and this pass adds what only the editor knows - the asset-registry
#   truth about the parent class, the engine's own IsDataOnly verdict, and its component
#   counts - then reports DISAGREEMENTS. A contradiction between the two readers is a
#   high finding: it means one of them is lying and nobody knows which.
#
# HONESTY NOTE, LOUD, ON PURPOSE
#   The packet that wrote this file was forbidden to launch the editor (a human had it
#   open) and forbidden to compile (R21, two builders live). So this half has NEVER BEEN
#   EXECUTED. The offline half has: see the README's evidence section. Every editor call
#   below is wrapped and degrades to 'fact not established' -> INCONCLUSIVE, never to a
#   silent pass. First runner: paste the output into the ticket Log and fix what breaks.
#
# Headless:
#   UnrealEditor-Cmd.exe <repo>\Breachpoint.uproject -run=pythonscript ^
#       -script="<repo>\Tools\audit_blueprints\dump_blueprints.py" ^
#       -stdout -unattended -nosplash -nop4 -NoLiveCoding
#   or: Tools\audit_blueprints\audit-blueprints.ps1 -Editor
#
# Exit codes: whatever audit_r26.py scores (0 PASS / 1 FAIL / 2 INCONCLUSIVE / 3 BLOCKED).
# =====================================================================================
"""Editor-side cross-check pass for the R26 Blueprint audit."""

import json
import os
import shlex
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import audit_r26  # noqa: E402  (same directory; see sys.path above)

try:
    import unreal
except ImportError:  # pragma: no cover - the message is the point
    sys.stderr.write(
        "BLOCKED: dump_blueprints.py must run inside Unreal (it imports `unreal`).\n"
        "  To audit R26 WITHOUT an editor - which is the supported everyday path - run:\n"
        "      python Tools/audit_blueprints/audit_r26.py\n"
        "  That half needs no engine, launches nothing, and is the primary evidence.\n")
    sys.exit(audit_r26.EXIT_BLOCKED)


def log(msg):
    unreal.log("[BR-r26] %s" % msg)


def warn(msg):
    unreal.log_warning("[BR-r26] %s" % msg)


def parse_args():
    """Args reach a -run=pythonscript script inconsistently across launch shapes, so we
    accept them from sys.argv AND from BR_R26_ARGS (what the PS wrapper sets)."""
    argv = [a for a in sys.argv[1:] if not a.startswith("-run=")]
    env_args = os.environ.get("BR_R26_ARGS", "").strip()
    if env_args:
        argv.extend(shlex.split(env_args))
    out = {"json": None, "content": None}
    i = 0
    while i < len(argv):
        if argv[i] == "--json" and i + 1 < len(argv):
            out["json"] = argv[i + 1]; i += 2; continue
        if argv[i] == "--content" and i + 1 < len(argv):
            out["content"] = argv[i + 1]; i += 2; continue
        i += 1
    return out


def project_dir():
    return os.path.normpath(
        unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))


def asset_data_for(package_name):
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    for getter in ("get_asset_by_object_path",):
        try:
            data = getattr(registry, getter)(package_name + "." + package_name.rsplit("/", 1)[-1])
            if data and data.is_valid():
                return data
        except Exception:
            continue
    return None


def tag(asset_data, key):
    try:
        value = asset_data.get_tag_value(key)
    except Exception:
        return None
    if value in (None, ""):
        return None
    return str(value)


def crosscheck(facts):
    """Annotate one asset's offline facts with what the editor says. Returns a dict with
    'editor' (whatever we could read) and 'contradictions' (a list of strings)."""
    package = facts["package"]
    result = {"editor": {}, "contradictions": [], "unreadable": []}

    data = asset_data_for(package)
    if data is None:
        result["unreadable"].append("asset registry has no entry for %s" % package)
        return result

    parent_path = tag(data, "ParentClass") or tag(data, "NativeParentClass")
    is_data_only = tag(data, "IsDataOnly")
    num_bp_components = tag(data, "NumBlueprintComponents")
    num_native_components = tag(data, "NumNativeComponents")
    num_replicated = tag(data, "NumReplicatedProperties")
    result["editor"] = {
        "parent_class_tag": parent_path,
        "is_data_only": is_data_only,
        "num_blueprint_components": num_bp_components,
        "num_native_components": num_native_components,
        "num_replicated_properties": num_replicated,
    }

    # -- parent -------------------------------------------------------------------------
    offline_parent = facts.get("parent", {}).get("name")
    if parent_path and offline_parent:
        if offline_parent not in parent_path:
            result["contradictions"].append(
                "package header says the parent is '%s', the asset registry says '%s'"
                % (offline_parent, parent_path))
    elif not parent_path:
        result["unreadable"].append("no ParentClass/NativeParentClass registry tag")

    # -- the engine's own 'data only' verdict -------------------------------------------
    # UE calls a Blueprint data-only when it adds no variables, no functions, no macros,
    # no interfaces, no timelines, and its ubergraph holds nothing but auto-placed ghost
    # nodes. That is R26 conditions 2 and 3 in the engine's own words - so a false here,
    # with a clean offline scan, means one of the two readers missed something.
    user_nodes = sum(g.get("nodes_user", 0) for g in facts.get("graphs", []))
    added = bool(facts.get("added_components")) or any(
        facts.get("member_flags", {}).values())
    if is_data_only is not None:
        engine_says_clean = str(is_data_only).lower() in ("true", "1")
        offline_says_clean = (user_nodes == 0 and not added)
        if engine_says_clean != offline_says_clean:
            result["contradictions"].append(
                "engine IsDataOnly=%s but the offline scan found user_nodes=%d, "
                "added_members=%s" % (is_data_only, user_nodes, added))
    else:
        result["unreadable"].append("no IsDataOnly registry tag")

    # -- components ---------------------------------------------------------------------
    if num_bp_components is not None:
        try:
            engine_count = int(num_bp_components)
        except ValueError:
            engine_count = None
        if engine_count is not None:
            offline_count = len(facts.get("added_components", []))
            # The engine counts the auto-placed DefaultSceneRoot; the offline scan does
            # not (see audit_r26.AUTO_ROOT_TEMPLATE), so 0 vs 1 is expected, not a lie.
            if engine_count - offline_count not in (0, 1):
                result["contradictions"].append(
                    "registry NumBlueprintComponents=%d, offline scan found %d added "
                    "component(s)" % (engine_count, offline_count))
    return result


def main():
    args = parse_args()
    root = project_dir()
    content_dir = os.path.abspath(args["content"] or os.path.join(root, "Content"))

    log("=" * 78)
    log("R26 Blueprint audit - editor cross-check pass")
    log("project : %s" % root)
    log("content : %s" % content_dir)

    assets, skipped, blockers = audit_r26.discover(content_dir)
    log("offline scan found %d in-scope Blueprint(s)" % len(assets))

    for facts in assets:
        try:
            facts["crosscheck"] = crosscheck(facts)
        except Exception as exc:      # never let one bad asset hide the rest
            facts["crosscheck"] = {"editor": {}, "contradictions": [],
                                   "unreadable": ["cross-check raised %s" % exc]}
        for item in facts["crosscheck"].get("unreadable", []):
            facts.setdefault("unknowns", []).append("editor cross-check: %s" % item)
            warn("%s: %s" % (facts["asset_name"], item))

    result = audit_r26.build_result(assets, skipped, blockers,
                                    "editor-crosscheck+offline-package-scan", content_dir)
    audit_r26.print_report(result, out=sys.stdout)
    for line in ("verdict %s (exit %d)" % (result["verdict"], result["exit_code"]),):
        log(line)

    out_path = args["json"] or os.path.join(root, "Saved", "R26", "r26-audit.json")
    try:
        out_dir = os.path.dirname(out_path)
        if out_dir and not os.path.isdir(out_dir):
            os.makedirs(out_dir)
        with open(out_path, "w", encoding="utf-8") as fh:
            json.dump(result, fh, indent=2, sort_keys=True)
        log("audit json: %s" % out_path)
    except OSError as exc:
        warn("could not write %s: %s" % (out_path, exc))

    return result["exit_code"]


if __name__ == "__main__":
    sys.exit(main())
