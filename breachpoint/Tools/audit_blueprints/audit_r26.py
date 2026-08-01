#!/usr/bin/env python3
# =====================================================================================
# BREACHPOINT - audit_r26.py :: mechanical enforcement of DESIGN RULING R26
#
# R26 permits Blueprint children of BR C++ classes as DEFAULT-VALUE CONTAINERS ONLY, and
# says so in its own words: "Until that script exists this ruling is enforced by goodwill,
# and that is stated here rather than assumed." This is that script.
#
# The five conditions, and how each is asserted here:
#   1. direct child of a BR-prefixed C++ class, no BP in between
#        -> the generated class export's SuperStruct must resolve to an IMPORT of class
#           'Class' whose outer package is /Script/Breachpoint and whose name starts 'BR'.
#           A BlueprintGeneratedClass super means an intermediate BP: FAIL.
#   2. EventGraph and ConstructionScript EMPTY - zero nodes
#        -> every export outered to a graph export is a node. Counted, listed, per graph.
#           See "the ghost-node deviation" below: the number enforced is zero USER nodes.
#   3. no added variables / functions / macros / interfaces / components
#        -> extra EdGraph exports under the Blueprint; SCS component templates; and the
#           presence of the UBlueprint properties NewVariables / ImplementedInterfaces /
#           Timelines / MacroGraphs / DelegateSignatureGraphs in the serialised header
#           (uncooked packages only serialise what differs from the parent default, so
#           the name being absent means the array is empty).
#   4. no gameplay NUMBER set as a default
#        -> PARTIALLY automatable, and this is the one the ruling itself flags. Every
#           property the CDO overrides is extracted and PRINTED with its type. Numeric
#           types and tunable-sounding names are raised as findings; object/class/soft
#           pointers are listed as information for a human. A script cannot know that
#           'Sensitivity' is not gameplay; a human reading the list can.
#   5. named BP_<CppClassWithoutPrefix>
#        -> compared, case-sensitively, against the resolved parent class name.
#
# SCOPE IS BY PARENT, NOT BY FILENAME. Auditing only files called BP_BR* would be evaded
# by naming the asset GM_BR - which is exactly what Content/Core held until Tools/rename_r26
# renamed the five founder Blueprints to their R26 condition-5 names. The scope rule stays
# parent-based anyway: that is what makes the NEXT off-name asset findable. In scope: any
# Blueprint whose ancestry reaches a BR C++ class, PLUS anything named BP_BR* (so a
# mis-parented BP_BR* asset is still caught).
#
# EXIT CODES (Tools/_BRLadderCommon.ps1 vocabulary; only 0 is green):
#   0 PASS   assets were found, every one of them conforms
#   1 FAIL   at least one R26 condition is violated
#   2 INCONCLUSIVE  ran, but some fact could not be established (unparseable package,
#                   unknown parent, editor holding unsaved state)
#   3 BLOCKED  could not run / nothing to audit. ZERO ASSETS IS BLOCKED, NEVER PASS.
#
# THE GHOST-NODE DEVIATION (read this before arguing with the output):
#   R26 says zero nodes. Unreal cannot create a Blueprint with zero nodes: every new BP is
#   born with disabled, auto-placed 'ghost' event nodes (ReceiveBeginPlay, ReceiveTick...)
#   in its ubergraph and a mandatory, undeletable K2Node_FunctionEntry in its construction
#   script. Literal zero is unsatisfiable, so this script enforces zero USER nodes and
#   PRINTS the ghost inventory for every asset - nobody gets to hide behind the exception,
#   because the exception is itemised on screen. A ghost node is recognised by carrying
#   ENodeEnabledState::Disabled; if a human enables one, it stops being a ghost and this
#   audit fails. Ruling owner: please ratify or correct this reading (see README).
#
# Plain CPython 3.8+, stdlib only. Needs no engine, launches no editor, writes no asset.
# =====================================================================================
"""Audit every BR-parented Blueprint against DESIGN RULING R26."""

import argparse
import datetime
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bp_package  # noqa: E402  (same directory; see sys.path above)

EXIT_PASS = 0
EXIT_FAIL = 1
EXIT_INCONCLUSIVE = 2
EXIT_BLOCKED = 3

NATIVE_MODULE = "/Script/Breachpoint"
CLASS_PREFIX = "BR"
ASSET_PREFIX = "BP_BR"

UBERGRAPH_NAME = "EventGraph"
CONSTRUCTION_NAME = "UserConstructionScript"
AUTO_ROOT_TEMPLATE = "DefaultSceneRoot_GEN_VARIABLE"
GHOST_MARKER = "ENodeEnabledState::Disabled"

# Node classes Unreal places itself and that carry no author decision.
MANDATORY_NODE_CLASSES = ("K2Node_FunctionEntry", "K2Node_FunctionResult", "K2Node_Tunnel")

NUMERIC_PROPERTY_TYPES = (
    "FloatProperty", "DoubleProperty", "IntProperty", "Int8Property", "Int16Property",
    "Int64Property", "UInt16Property", "UInt32Property", "UInt64Property", "ByteProperty")
REVIEW_PROPERTY_TYPES = ("StructProperty", "ArrayProperty", "MapProperty", "SetProperty")

# Words that make a property name sound like a tuning number. Law 3: tuning numbers live
# in Content/Data/*.csv. Deliberately broad - a false positive costs one line of human
# reading; a false negative is the silent-drift bug class R19/R20 exist to kill.
TUNABLE_WORDS = (
    "damage", "health", "shield", "armor", "armour", "speed", "velocity", "accel",
    "cooldown", "duration", "radius", "rate", "ammo", "magazine", "clip", "score",
    "limit", "time", "threshold", "magnitude", "multiplier", "scale", "interval",
    "delay", "range", "count", "max", "min", "amount", "power", "force", "impulse",
    "recoil", "spread", "accuracy", "regen", "respawn", "capacity")

# Names that appear in every serialised graph node and say nothing about its content.
GENERIC_NODE_NAMES = frozenset([
    "None", "Guid", "NodeGuid", "NodeComment", "NodePosX", "NodePosY", "NodeWidth",
    "NodeHeight", "EnabledState", "ENodeEnabledState", GHOST_MARKER, "CustomProperties",
    "bCommentBubblePinned", "bCommentBubbleVisible", "bOverrideFunction", "bIsEditable",
    "MemberName", "MemberParent", "MemberReference", "EventReference", "FunctionReference",
    "OutputDelegate", "exec", "then", "delegate", "object", "float", "real", "self",
    "BoolProperty", "ByteProperty", "EnumProperty", "IntProperty", "NameProperty",
    "ObjectProperty", "StrProperty", "StructProperty", "TextProperty", "ArrayProperty",
    "FloatProperty", "DoubleProperty", "SoftObjectPath", "DeltaSeconds",
    "/Script/CoreUObject", "/Script/Engine", "/Script/SlateCore", "/Script/BlueprintGraph",
    # names the lenient scan picks up from adjacent package data, not from the node
    "GeneratedClass", "HideCategories", "ShowCategories", "UberGraphFrame", "BlueprintGuid",
    "CategorySorting", "AllNodes", "Nodes", "Schema", "GraphGuid", "SavedViewOffset",
    "SavedZoomAmount", "EdGraphSchema_K2", "EdGraph", "Blueprint", "Class", "Package"])

MEMBER_FLAG_NAMES = {
    "new_variables": "NewVariables",
    "implemented_interfaces": "ImplementedInterfaces",
    "timelines": "Timelines",
    "macro_graphs": "MacroGraphs",
    "delegate_graphs": "DelegateSignatureGraphs",
    "component_templates": "ComponentTemplates",
}

HIGH, MEDIUM, INFO = "high", "medium", "info"


def finding(asset, code, severity, message):
    return {"asset": asset, "code": code, "severity": severity, "message": message}


# =====================================================================================
# fact extraction (offline: header scan, no engine)
# =====================================================================================

def classify_graph(name):
    if name == UBERGRAPH_NAME:
        return "ubergraph"
    if name == CONSTRUCTION_NAME:
        return "construction"
    return "added"


def extract_facts(file_path, content_dir):
    pkg = bp_package.Package(file_path)
    package_name = bp_package.package_name_from_path(content_dir, file_path)
    asset_name = package_name.rsplit("/", 1)[-1]

    facts = {
        "package": package_name,
        "file": os.path.relpath(file_path, os.path.dirname(content_dir)).replace("\\", "/"),
        "asset_name": asset_name,
        "parent": {"kind": "unknown", "name": None, "package": None},
        "graphs": [],
        "added_components": [],
        "native_components": [],
        "scs_node_count": 0,
        "member_flags": {},
        "overrides": [],
        "unknowns": [],
        "notes": [],
    }

    bp_export = pkg.blueprint_export()
    gen_export = pkg.generated_class_export()
    if bp_export is None and gen_export is None:
        facts["unknowns"].append("package contains no Blueprint export")
        return facts

    # -- condition 1: the parent ------------------------------------------------------
    if gen_export is not None:
        kind, obj = pkg.resolve(gen_export.super_index)
        if kind == "import":
            outer_kind, outer = pkg.resolve(obj.outer_index)
            outer_name = outer.name if outer is not None else None
            if obj.class_name == "BlueprintGeneratedClass":
                facts["parent"] = {"kind": "blueprint", "name": obj.name, "package": outer_name}
            else:
                facts["parent"] = {"kind": "native", "name": obj.name, "package": outer_name}
        elif kind == "export":
            facts["parent"] = {"kind": "blueprint", "name": obj.name, "package": package_name}
        else:
            facts["unknowns"].append("generated class has no resolvable super")
    else:
        facts["unknowns"].append("no BlueprintGeneratedClass export - parent unknown")

    # -- conditions 2 and 3: graphs and their nodes -----------------------------------
    graph_exports = []
    for export in pkg.exports:
        if pkg.class_of(export) == "EdGraph":
            outer_kind, outer = pkg.outer_of(export)
            if outer_kind == "export" and bp_export is not None and outer is bp_export:
                graph_exports.append(export)

    for graph in graph_exports:
        graph_kind = classify_graph(graph.name)
        nodes = []
        for node in pkg.exports_outered_to(graph):
            node_class = pkg.class_of(node)
            refs = pkg.referenced_names(node)
            ghost = GHOST_MARKER in refs
            mandatory = node_class in MANDATORY_NODE_CLASSES
            detail = sorted(refs - GENERIC_NODE_NAMES - set([package_name]))
            nodes.append({
                "class": node_class,
                "ghost": bool(ghost),
                "mandatory": bool(mandatory),
                "user": not (ghost or mandatory),
                "detail": detail[:6],
            })
        facts["graphs"].append({
            "name": graph.name,
            "kind": graph_kind,
            "nodes_total": len(nodes),
            "nodes_user": len([n for n in nodes if n["user"]]),
            "nodes": nodes,
        })

    # -- condition 3: components ------------------------------------------------------
    scs_export = None
    for export in pkg.exports:
        if pkg.class_of(export) == "SimpleConstructionScript":
            scs_export = export
            break
    if scs_export is not None:
        facts["scs_node_count"] = len(pkg.exports_outered_to(scs_export))
    for export in pkg.exports:
        name = export.name
        outer_kind, outer = pkg.outer_of(export)
        if name.endswith("_GEN_VARIABLE"):
            if name != AUTO_ROOT_TEMPLATE:
                facts["added_components"].append("%s (%s)" % (name, pkg.class_of(export)))
        elif outer_kind == "export" and outer is not None and outer.name.startswith("Default__"):
            facts["native_components"].append("%s (%s)" % (name, pkg.class_of(export)))

    # -- condition 3: member arrays ---------------------------------------------------
    if bp_export is not None:
        bp_refs = pkg.referenced_names(bp_export)
        for key, serialised_name in MEMBER_FLAG_NAMES.items():
            facts["member_flags"][key] = serialised_name in bp_refs
    else:
        facts["unknowns"].append("no Blueprint export - member arrays not checked")

    # -- condition 4: what the CDO overrides ------------------------------------------
    cdo = None
    for export in pkg.exports:
        if export.name.startswith("Default__"):
            cdo = export
            break
    if cdo is None:
        facts["unknowns"].append("no CDO export - overridden defaults not readable")
    else:
        facts["overrides"] = [{"name": n, "type": t} for n, t in pkg.tagged_properties(cdo)]
        facts["notes"].append(
            "overrides were read by a lenient tag scan of the CDO serial blob; treat the "
            "LIST as evidence and the ABSENCE of an entry as weak evidence")

    return facts


# =====================================================================================
# discovery
# =====================================================================================

def looks_like_candidate(pkg, asset_name):
    if asset_name.startswith(ASSET_PREFIX):
        return True
    return NATIVE_MODULE in pkg.name_set


def discover(content_dir, verbose=False):
    """Returns (assets, skipped, blockers). Walks Content/ once."""
    assets, skipped, blockers = [], [], []
    if not os.path.isdir(content_dir):
        blockers.append("content directory not found: %s" % content_dir)
        return assets, skipped, blockers

    scanned = 0
    for root, dirs, files in os.walk(content_dir):
        dirs[:] = [d for d in dirs if d not in ("Collections", "Developers")]
        for fname in sorted(files):
            if not fname.lower().endswith(".uasset"):
                continue
            path = os.path.join(root, fname)
            asset_name = fname[:-len(".uasset")]
            scanned += 1
            try:
                pkg = bp_package.Package(path)
            except bp_package.LfsPointerError as exc:
                if asset_name.startswith(ASSET_PREFIX):
                    blockers.append(str(exc))
                else:
                    skipped.append({"file": path, "reason": "git-lfs pointer"})
                continue
            except bp_package.PackageError as exc:
                if asset_name.startswith(ASSET_PREFIX):
                    skipped.append({"file": path, "reason": "UNPARSEABLE: %s" % exc,
                                    "in_scope": True})
                continue
            if not pkg.is_blueprint:
                continue
            if not looks_like_candidate(pkg, asset_name):
                continue
            try:
                facts = extract_facts(path, content_dir)
            except bp_package.PackageError as exc:
                skipped.append({"file": path, "reason": "UNPARSEABLE: %s" % exc,
                                "in_scope": True})
                continue
            # A blueprint that merely REFERENCES the module is not in scope; one that
            # descends from it (or is named BP_BR*) is.
            parent = facts["parent"]
            in_scope = asset_name.startswith(ASSET_PREFIX)
            if parent["kind"] == "native" and parent["package"] == NATIVE_MODULE:
                in_scope = True
            elif parent["kind"] == "blueprint":
                in_scope = in_scope or NATIVE_MODULE in pkg.name_set
            if in_scope:
                assets.append(facts)
            elif verbose:
                skipped.append({"file": path, "reason": "blueprint, but not BR-parented"})

    if verbose:
        skipped.append({"file": content_dir, "reason": "%d .uasset files scanned" % scanned})
    return assets, skipped, blockers


# =====================================================================================
# the rules
# =====================================================================================

def expected_asset_name(parent_name):
    return "BP_" + parent_name


def looks_tunable(prop_name):
    lowered = re.sub(r"[^a-z]", "", prop_name.lower())
    for word in TUNABLE_WORDS:
        if word in lowered:
            return word
    return None


def judge(assets):
    """Returns (findings, unknown_count)."""
    findings = []
    unknowns = 0

    for facts in assets:
        name = facts["asset_name"]
        for note in facts.get("unknowns", []):
            unknowns += 1
            findings.append(finding(name, "R26_UNKNOWN", MEDIUM,
                                    "fact not established: %s" % note))

        # ---- condition 1 -----------------------------------------------------------
        parent = facts["parent"]
        if parent["kind"] == "blueprint":
            findings.append(finding(
                name, "R26_1_INTERMEDIATE_BP", HIGH,
                "parent is the Blueprint class '%s' - R26 condition 1 requires a DIRECT "
                "child of a BR C++ class with no Blueprint in between." % parent["name"]))
        elif parent["kind"] == "native":
            if parent["package"] != NATIVE_MODULE:
                findings.append(finding(
                    name, "R26_1_PARENT_NOT_BR", HIGH,
                    "parent class '%s' comes from %s, not %s. R26 permits BP children of "
                    "BR C++ classes only." % (parent["name"], parent["package"], NATIVE_MODULE)))
            elif not parent["name"].startswith(CLASS_PREFIX):
                findings.append(finding(
                    name, "R26_1_PARENT_NOT_BR", HIGH,
                    "parent class '%s' is not BR-prefixed." % parent["name"]))

        # ---- condition 2 -----------------------------------------------------------
        for graph in facts["graphs"]:
            if graph["kind"] == "added":
                continue
            if graph["nodes_user"] > 0:
                # Class histogram, not per-node detail: this string goes into a ticket Log
                # and 'K2Node_CallFunction x6' is what a reader can act on.
                tally = {}
                for n in graph["nodes"]:
                    if n["user"]:
                        tally[n["class"]] = tally.get(n["class"], 0) + 1
                offenders = ", ".join("%s x%d" % (cls, tally[cls]) for cls in sorted(tally))
                code = "R26_2_EVENTGRAPH_NODES"
                if graph["kind"] == "construction":
                    code = "R26_2_CONSTRUCTION_NODES"
                findings.append(finding(
                    name, code, HIGH,
                    "%s holds %d author-placed node(s): %s. R26 condition 2: the EventGraph "
                    "and the ConstructionScript carry no logic. The moment a BP child "
                    "contains a decision it is a high-severity finding, not a style note."
                    % (graph["name"], graph["nodes_user"], offenders)))

        # ---- condition 3 -----------------------------------------------------------
        for graph in facts["graphs"]:
            if graph["kind"] == "added":
                findings.append(finding(
                    name, "R26_3_ADDED_GRAPH", HIGH,
                    "extra graph '%s' (%d node(s)) - R26 condition 3 forbids new functions "
                    "and macros." % (graph["name"], graph["nodes_total"])))
        if facts["added_components"]:
            findings.append(finding(
                name, "R26_3_ADDED_COMPONENTS", HIGH,
                "Blueprint-added component(s): %s. Components belong on the C++ class."
                % ", ".join(facts["added_components"])))
        flags = facts.get("member_flags", {})
        for key, code, label in (
                ("new_variables", "R26_3_ADDED_VARIABLES", "Blueprint variables"),
                ("implemented_interfaces", "R26_3_ADDED_INTERFACE", "implemented interfaces"),
                ("timelines", "R26_3_TIMELINE", "timelines"),
                ("macro_graphs", "R26_3_ADDED_MACRO", "macro graphs"),
                ("delegate_graphs", "R26_3_ADDED_DELEGATE", "delegate signature graphs")):
            if flags.get(key):
                findings.append(finding(
                    name, code, HIGH,
                    "declares %s - R26 condition 3 allows no new members; it sets values on "
                    "properties the C++ class already declares." % label))

        # ---- condition 4 (partially automatable - see the header) ------------------
        for override in facts.get("overrides", []):
            prop, ptype = override["name"], override["type"]
            word = looks_tunable(prop)
            if ptype in NUMERIC_PROPERTY_TYPES:
                findings.append(finding(
                    name, "R26_4_NUMERIC_DEFAULT", HIGH,
                    "sets numeric default %s (%s). R26 condition 4 / law 3: gameplay "
                    "numbers live in Content/Data/*.csv, never as a Blueprint default. If "
                    "this number is not gameplay, say why in the ticket Log."
                    % (prop, ptype)))
            elif word is not None:
                findings.append(finding(
                    name, "R26_4_TUNABLE_NAME", MEDIUM,
                    "overrides %s (%s) - the name contains '%s', which reads like a tuning "
                    "value. Human check required: a soft asset pointer is fine, a tunable "
                    "is not." % (prop, ptype, word)))
            elif ptype in REVIEW_PROPERTY_TYPES:
                findings.append(finding(
                    name, "R26_4_REVIEW_STRUCT", MEDIUM,
                    "overrides %s (%s) - a struct/container default can hide a number. "
                    "This script cannot read inside it; a human must." % (prop, ptype)))

        # ---- editor cross-check (only present when dump_blueprints.py produced the
        #      facts). A disagreement between the on-disk header and the loaded asset
        #      means one of the two readers is wrong, and neither may be trusted until a
        #      human says which.
        for contradiction in facts.get("crosscheck", {}).get("contradictions", []):
            findings.append(finding(
                name, "R26_CROSSCHECK_DISAGREE", HIGH,
                "the editor and the on-disk package header disagree: %s" % contradiction))

        # ---- condition 5 -----------------------------------------------------------
        # Only meaningful once condition 1 holds: telling somebody to rename BP_BRJumpPad
        # to 'BP_Actor' because it descends from AActor is noise on top of the real
        # finding, which is that it descends from AActor at all.
        parent_is_br = (parent["kind"] == "native"
                        and parent["package"] == NATIVE_MODULE
                        and bool(parent["name"])
                        and parent["name"].startswith(CLASS_PREFIX))
        if parent_is_br:
            expected = expected_asset_name(parent["name"])
            if name != expected:
                findings.append(finding(
                    name, "R26_5_NAME_MISMATCH", MEDIUM,
                    "asset is named '%s'; R26 condition 5 requires '%s' (BP_ + the parent "
                    "C++ class name). Case included - '%s' is not '%s'."
                    % (name, expected, name, expected)))

    return findings, unknowns


# =====================================================================================
# report
# =====================================================================================

def print_report(result, out=sys.stdout):
    w = out.write
    src = result["source"]
    w("\n")
    w("=" * 86 + "\n")
    w("BREACHPOINT R26 BLUEPRINT AUDIT  (docs/DESIGN-RULINGS.md R26)\n")
    w("=" * 86 + "\n")
    w("when        : %s\n" % result["when"])
    w("content dir : %s\n" % result["content_dir"])
    w("fact source : %s\n" % src)
    w("assets found: %d\n" % len(result["assets"]))

    by_asset = {}
    for f in result["findings"]:
        by_asset.setdefault(f["asset"], []).append(f)

    for facts in result["assets"]:
        name = facts["asset_name"]
        parent = facts["parent"]
        w("\n" + "-" * 86 + "\n")
        w("ASSET  %s\n" % facts["package"])
        w("  file            : %s\n" % facts["file"])
        parent_desc = "UNKNOWN"
        if parent["name"]:
            parent_desc = "%s.%s [%s]" % (parent["package"], parent["name"], parent["kind"])
        w("  parent chain    : %s  ->  %s\n" % (name, parent_desc))
        for graph in facts["graphs"]:
            w("  graph %-22s nodes: %d total, %d author-placed, %d engine-placed\n"
              % (graph["name"], graph["nodes_total"], graph["nodes_user"],
                 graph["nodes_total"] - graph["nodes_user"]))
            for node in graph["nodes"]:
                tag = "AUTHOR"
                if node["ghost"]:
                    tag = "ghost "
                elif node["mandatory"]:
                    tag = "engine"
                detail = (" " + ", ".join(node["detail"])) if node["detail"] else ""
                w("      [%s] %s%s\n" % (tag, node["class"], detail))
        if not facts["graphs"]:
            w("  graphs          : none serialised\n")
        w("  added members   : components=%s variables=%s interfaces=%s timelines=%s "
          "macros=%s\n" % (
              facts["added_components"] or "none",
              facts.get("member_flags", {}).get("new_variables"),
              facts.get("member_flags", {}).get("implemented_interfaces"),
              facts.get("member_flags", {}).get("timelines"),
              facts.get("member_flags", {}).get("macro_graphs")))
        w("  native comps    : %s\n" % (", ".join(facts["native_components"]) or "none"))
        w("  overridden props: %d\n" % len(facts.get("overrides", [])))
        for override in facts.get("overrides", []):
            w("      %-34s %s\n" % (override["name"], override["type"]))
        asset_findings = by_asset.get(name, [])
        if asset_findings:
            w("  FINDINGS        : %d\n" % len(asset_findings))
            for f in asset_findings:
                w("      [%-6s] %-26s %s\n" % (f["severity"], f["code"], f["message"]))
        else:
            w("  FINDINGS        : none - conforms to R26 conditions 1-3 and 5; condition 4 "
              "needs the human read above.\n")

    if result["skipped"]:
        w("\n" + "-" * 86 + "\n")
        w("SKIPPED / NOT AUDITED\n")
        for s in result["skipped"]:
            w("  %-60s %s\n" % (s.get("file", "?"), s.get("reason", "")))

    w("\n" + "=" * 86 + "\n")
    w("VERDICT: %s   (exit %d)\n" % (result["verdict"], result["exit_code"]))
    for reason in result["reasons"]:
        w("  %s\n" % reason)
    w("\n")
    counts = result["counts"]
    w("R26AUDIT|source=%s|assets=%d|conforming=%d|high=%d|medium=%d|unknown=%d|verdict=%s\n"
      % (src, counts["assets"], counts["conforming"], counts["high"], counts["medium"],
         counts["unknown"], result["verdict"]))
    w("\n")
    w("Rung: this is a STATIC package-header audit (rung 1/2 evidence). It proves what is\n")
    w("serialised in Content/, not that anything plays. Condition 4 is only partly\n")
    w("automatable - the overridden-property lists above are printed FOR A HUMAN.\n")


# =====================================================================================
# main
# =====================================================================================

def build_result(assets, skipped, blockers, source, content_dir, live_editor=False):
    findings, unknowns = judge(assets)
    high = len([f for f in findings if f["severity"] == HIGH])
    medium = len([f for f in findings if f["severity"] == MEDIUM])
    unparseable = [s for s in skipped if s.get("in_scope")]
    offenders = set(f["asset"] for f in findings
                    if f["severity"] in (HIGH, MEDIUM) and f["code"] != "R26_UNKNOWN")

    reasons = []
    if blockers:
        verdict, code = "BLOCKED", EXIT_BLOCKED
        reasons.extend(blockers)
    elif not assets:
        verdict, code = "BLOCKED", EXIT_BLOCKED
        reasons.append(
            "ZERO Blueprint assets in scope were found under %s. An auditor that finds "
            "nothing and reports green is the most dangerous script in a repo, so this is "
            "BLOCKED, never PASS. Either no BP_BR* / BR-parented Blueprint exists yet, or "
            "the tree was not checked out (git lfs pull), or discovery is broken - and all "
            "three of those are things somebody has to look at." % content_dir)
    elif high > 0:
        verdict, code = "FAIL", EXIT_FAIL
        reasons.append("%d high-severity R26 violation(s) across %d asset(s)."
                       % (high, len(offenders)))
    elif medium > 0:
        verdict, code = "FAIL", EXIT_FAIL
        reasons.append("%d medium-severity R26 violation(s). Non-conforming is "
                       "non-conforming; there is deliberately no flag that mutes a "
                       "finding." % medium)
    elif unparseable:
        verdict, code = "INCONCLUSIVE", EXIT_INCONCLUSIVE
        reasons.append("%d in-scope asset(s) could not be parsed - unreadable is never a "
                       "pass." % len(unparseable))
    elif unknowns:
        verdict, code = "INCONCLUSIVE", EXIT_INCONCLUSIVE
        reasons.append("%d fact(s) could not be established." % unknowns)
    else:
        verdict, code = "PASS", EXIT_PASS
        reasons.append("%d asset(s) audited; every one satisfies R26 conditions 1, 2, 3 and "
                       "5, and no overridden default looks like a gameplay number."
                       % len(assets))

    if live_editor and code == EXIT_PASS and source.startswith("offline"):
        verdict, code = "INCONCLUSIVE", EXIT_INCONCLUSIVE
        reasons.append("An Unreal editor is live. This audit read the bytes ON DISK, which "
                       "lag any unsaved Blueprint edit, so a green claim would be about a "
                       "file rather than about the asset. Save/close the editor and re-run.")

    conforming = len(assets) - len(offenders)
    return {
        "schema": "breachpoint.r26.audit/1",
        "when": datetime.datetime.now().strftime("%Y-%m-%dT%H:%M:%S"),
        "source": source,
        "content_dir": content_dir,
        "assets": assets,
        "skipped": skipped,
        "findings": findings,
        "verdict": verdict,
        "exit_code": code,
        "reasons": reasons,
        "counts": {"assets": len(assets), "conforming": conforming, "high": high,
                   "medium": medium, "unknown": unknowns},
    }


def selftest(content_dir):
    """Prove the node detector actually fires.

    A detector nobody has ever seen trip is not a detector - and this one's whole job is
    to say 'that graph has logic in it'. So: sweep every Blueprint in Content/ that is NOT
    in R26 scope (template/marketplace content, which is full of real graphs) and require
    at least one of them to report author-placed nodes. If the sweep finds none, the
    fixtures are gone and the honest verdict is INCONCLUSIVE, not a green self-test.
    """
    hits, parsed = [], 0
    for root, dirs, files in os.walk(content_dir):
        dirs[:] = [d for d in dirs if d not in ("Collections", "Developers")]
        for fname in sorted(files):
            if not fname.lower().endswith(".uasset"):
                continue
            path = os.path.join(root, fname)
            try:
                pkg = bp_package.Package(path)
                if not pkg.is_blueprint:
                    continue
                facts = extract_facts(path, content_dir)
            except bp_package.PackageError:
                continue
            parsed += 1
            user_nodes = sum(g["nodes_user"] for g in facts["graphs"])
            if user_nodes > 0:
                hits.append((facts["asset_name"], user_nodes,
                             sum(g["nodes_total"] for g in facts["graphs"])))
    print("SELFTEST: parsed %d Blueprint package(s) under %s" % (parsed, content_dir))
    for name, user, total in sorted(hits, key=lambda h: -h[1])[:10]:
        print("  detector fired: %-40s %d author-placed of %d node(s)" % (name, user, total))
    if not hits:
        print("SELFTEST INCONCLUSIVE: no Blueprint anywhere in Content/ reported an "
              "author-placed node. Either this project genuinely has no BP logic left "
              "(possible, and good) or the detector is blind. Verify by hand before "
              "trusting a PASS from this script.")
        return EXIT_INCONCLUSIVE
    print("SELFTEST PASS: the node detector distinguishes author-placed nodes from the "
          "engine's ghost/entry nodes on %d real asset(s)." % len(hits))
    return EXIT_PASS


def default_content_dir():
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.dirname(os.path.dirname(here))
    return os.path.join(repo, "Content")


def main(argv=None):
    ap = argparse.ArgumentParser(prog="audit_r26.py", description=__doc__)
    ap.add_argument("--content", default=None, help="Content/ directory (default: repo Content)")
    ap.add_argument("--facts", default=None,
                    help="score a facts JSON produced by dump_blueprints.py instead of "
                         "scanning packages offline")
    ap.add_argument("--json", dest="json_out", default=None,
                    help="write the full audit (facts + findings) to this path")
    ap.add_argument("--live-editor", action="store_true",
                    help="an editor is running: refuse to turn an on-disk scan into a PASS")
    ap.add_argument("--selftest", action="store_true",
                    help="prove the node detector fires on Blueprints that DO carry logic")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args(argv)

    content_dir = os.path.abspath(args.content or default_content_dir())

    if args.selftest:
        return selftest(content_dir)

    if args.facts:
        try:
            with open(args.facts, "r", encoding="utf-8") as fh:
                payload = json.load(fh)
        except (OSError, ValueError) as exc:
            sys.stderr.write("BLOCKED: cannot read facts file %s: %s\n" % (args.facts, exc))
            return EXIT_BLOCKED
        assets = payload.get("assets", [])
        skipped = payload.get("skipped", [])
        blockers = payload.get("blockers", [])
        source = payload.get("source", "facts-file")
        content_dir = payload.get("content_dir", content_dir)
    else:
        assets, skipped, blockers = discover(content_dir, verbose=args.verbose)
        source = "offline-package-scan"

    result = build_result(assets, skipped, blockers, source, content_dir,
                          live_editor=args.live_editor)
    print_report(result)

    if args.json_out:
        try:
            out_dir = os.path.dirname(os.path.abspath(args.json_out))
            if out_dir and not os.path.isdir(out_dir):
                os.makedirs(out_dir)
            with open(args.json_out, "w", encoding="utf-8") as fh:
                json.dump(result, fh, indent=2, sort_keys=True)
            sys.stdout.write("audit json: %s\n" % args.json_out)
        except OSError as exc:
            sys.stderr.write("warning: could not write %s: %s\n" % (args.json_out, exc))

    return result["exit_code"]


if __name__ == "__main__":
    sys.exit(main())
