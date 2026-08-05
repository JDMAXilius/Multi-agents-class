#!/usr/bin/env python3
"""Executor half of the WBP generator. Requires a LIVE editor with the MCP server.

    python3 mcp-ui/gen_ui/build_wbp.py [--dry-run] [--asset WBP_RootLayout]

SELECTING WHAT TO TOUCH -- read this before rebuilding anything:

    --parent BRButton          every asset parented to UBRButton (the nine buttons). PREFER
                               THIS. It selects by the C++ contract, so a tenth button type
                               is included the day it is planned, with no flag to update.
    --filter 'WBP_Button*'     name glob. A guess about naming discipline; --parent is a fact.
    --asset WBP_ButtonCheckbox exactly one.
    --list                     print the selection and exit. Touches no editor.
    (no flag)                  ALL FORTY-SIX. Deletes and recreates every asset in the plan.

A selection that matches nothing exits BLOCKED (3) rather than falling through to the
everything path -- a typo'd filter must not become a full rebuild.

Reads `wbp_plan.py`, rebuilds each WBP from scratch, and writes an R37 receipt to
`docs/ui/receipts/` flushed per line — a run that dies leaves a record of where.

Three things learned the expensive way, encoded here rather than in a comment nobody reads:

1. `CreateWidgetBlueprint`'s clobber guard is `FindPackage` — MEMORY, not disk. In a fresh
   editor where the asset is unloaded it takes the create path against an existing on-disk
   asset. Regeneration therefore DELETES first, explicitly, and asserts the delete.
2. Object refs need the full `/Game/X/Y.Y` form. The short form makes the server drop the
   whole argument object and report "input params Json is empty", which names the wrong cause.
3. Property names are camelCase (`layoutData`, `font`, `brush`), not C++ PascalCase, and
   `set_properties` takes `values` as a JSON *string*. A wrong name fails silently — which is
   why every write here goes through `write_verified` and is read back and compared. Note
   `letterSpacing` lives INSIDE the `font` struct (`FSlateFontInfo::LetterSpacing`, 1/1000 em),
   not beside it on the text block.

`BindToEventProperty` is deliberately NOT wired up: it adds a Blueprint event graph NODE,
which is the artifact R18/R26 forbid and `audit_wbp.py` exists to catch.
"""
from __future__ import annotations

import argparse
import re, json, sys, urllib.request
from fnmatch import fnmatch
from datetime import datetime, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import wbp_plan  # noqa: E402
from mcp import (REPO, URL, UMG, OBJ, ASSET,          # noqa: E402  shared transport
                 EXIT_PASS, EXIT_FAIL, EXIT_BLOCKED,
                 MCP, Receipt, write_verified, close)

# --------------------------------------------------------------------------- build
def full_path(folder: str, asset: str) -> str:
    return f"{folder}/{asset}.{asset}"


def build_one(m: MCP, rc: Receipt, asset: str, spec: dict) -> bool:
    folder, path = spec["folder"], full_path(spec["folder"], asset)
    rc.w(f"\n## `{asset}`  →  `{path}`\n")
    rc.w(f"parent `{spec['parent_class']}` · {len(spec['tree'])} widgets · {spec['notes']}\n")

    # (1) delete first — see module docstring note 1. Absent is fine; failure to remove is not.
    exists, _ = m.call(ASSET, "exists", {"path": path})
    if exists:
        _, txt = m.call(ASSET, "delete", {"path": path})
        gone, _ = m.call(ASSET, "exists", {"path": path})
        rc.call("AssetTools.delete", not gone,
                "removed for a clean rebuild" if not gone else f"still present: {txt}")
        if gone:
            return False
    else:
        rc.w("- `AssetTools.exists` → absent; creating fresh")

    # (2) create
    res, txt = m.call(UMG, "CreateWidgetBlueprint", {
        "folderPath": folder, "assetName": asset,
        "parentClass": {"refPath": spec["parent_class"]}})
    rc.call("CreateWidgetBlueprint", res is not None, txt[:200])
    if res is None:
        return False
    wbp = {"refPath": path}

    # UMG gives a new WBP a default root from UMGEditorProjectSettings — strip it so the
    # plan is the only thing that decides the tree.
    cur, _ = m.call(UMG, "GetWidgets", {"widgetBlueprint": wbp})
    for w in (cur or {}).get("widgets", []):
        if w.get("parent") in (None, "None"):
            m.call(UMG, "RemoveWidget", {"widgetBlueprint": wbp, "widget": w["widget"]})

    # (3) the tree, in plan order — which is creation order and, for an Overlay, z-order
    handles: dict[str, dict] = {}
    for node in spec["tree"]:
        parent = handles[node["parent"]]["widget"] if node["parent"] else None
        info, txt = m.call(UMG, "AddWidget", {
            "widgetBlueprint": wbp, "widgetClass": {"refPath": node["class"]},
            "widgetDisplayName": node["name"], "parentWidget": parent, "childIndex": -1})
        ok = isinstance(info, dict) and info.get("widgetName") == node["name"]
        rc.call(f"AddWidget {node['name']}", ok,
                f"{node['class'].rsplit('.', 1)[-1]}"
                + (f" under {node['parent']}" if node["parent"] else " (root)")
                + ("" if ok else f" — {txt[:200]}"))
        if not ok:
            return False
        handles[node["name"]] = info

        if node.get("bind"):
            _, t = m.call(UMG, "ToggleWidgetAsVariable",
                          {"widgetBlueprint": wbp, "widget": info["widget"], "bIsVariable": True})

        # (4) slot properties — written, then READ BACK and compared. A wrong camelCase
        # name fails silently, so an unverified write is not a write.
        slot_spec = node.get("slot")
        if slot_spec and isinstance(info.get("slot"), dict):
            write_verified(m, rc, f"slot {node['name']}", info["slot"], slot_spec)
        elif slot_spec:
            rc.call(f"slot {node['name']}", False, "AddWidget returned no slot handle")

        # (4b) ART — `font` and `brush`, written on the WIDGET rather than its slot.
        #
        # DELIBERATELY NON-FATAL. A font that did not take is a real failure and is recorded
        # as one (which sinks the run's verdict), but it does not abandon the asset: a HUD
        # with correct geometry and a default face is recoverable, a half-built tree is not.
        # A brush whose texture is missing is not even a failure — it is a SKIP with a loud
        # line, because the HUD texture set is landing in another lane right now.
        art, notes = wbp_plan.art_properties(node)
        for why in notes:
            rc.w(f"- SKIPPED `art {node['name']}` — {why}")
        if art:
            write_verified(m, rc, f"art {node['name']}", info["widget"], art)

        # (4c) PLAIN WIDGET PROPERTIES. Added because `TileView` made it non-optional: UMG
        # REFUSES TO COMPILE a UListViewBase with no EntryWidgetClass ("required for any
        # UListViewBase to function"), so WBP_ItemGrid could not be built at all — not
        # "builds and renders empty", which is what it was expected to do. Slot properties,
        # fonts and brushes were the only three things this generator could write, and that
        # was a gap in the generator rather than a fact about widgets.
        #
        # Deliberately the SAME verified write as everything else: set, read back, compare.
        # An unverified property write is not a write, and a silently-wrong camelCase name
        # here would produce a compiling asset that lists nothing.
        props = node.get("properties")
        if props:
            write_verified(m, rc, f"props {node['name']}", info["widget"], props)

    # (5) compile — the BindWidget contract is enforced HERE by the engine
    ok, txt = m.call(UMG, "CompileWidgetBlueprint", {"widgetBlueprint": wbp})
    rc.call("CompileWidgetBlueprint", ok is True, str(txt)[:200])
    if ok is not True:
        return False

    # (5b) THE DESIGNER PREVIEW MODE IS NOT WRITABLE FROM HERE, and that is recorded rather
    # than worked around.
    #
    # `DesignSizeMode` / `DesignTimeSize` are `WITH_EDITORONLY_DATA` on `UUserWidget` declared
    # as bare `UPROPERTY()` with no edit specifier, so `ObjectTools.list_properties` does not
    # see them at all — verified: 107 properties come back on a WidgetBlueprint and neither is
    # among them, while `previewBackground` (which IS `EditDefaultsOnly`) is. A write reads
    # back null, which `write_verified` correctly reports as a `high` finding rather than
    # letting it pass.
    #
    # THE SIZE IS SOLVED IN C++ INSTEAD, which is better anyway: `UBRButton::ApplyButtonType`
    # sets the 250 component-board width when `IsDesignTime()` and clears it at runtime, so
    # the preview is the measured box in every mode without a per-asset editor setting that
    # no script can reproduce.

    # (5c) CLASS DEFAULTS — properties on the generated class's CDO, not on a widget.
    #
    # `RowType` and `Style` live on the parent C++ class, so there is no tree node to hang
    # them off. Without this they stay at their C++ defaults and the asset is wrong in a way
    # nothing catches: every Menu Row Type would report `Default` and `ApplyButtonType` would
    # give Icon Only / Map Voting / Image a 28px height instead of 40 / 60 / 120.
    #
    # AFTER compile, because the CDO does not exist until the class is generated.
    if spec.get("class_defaults"):
        cdo = {"refPath": f"{spec['folder']}/{asset}.Default__{asset}_C"}
        write_verified(m, rc, "class defaults", cdo, spec["class_defaults"])
        ok, txt = m.call(UMG, "CompileWidgetBlueprint", {"widgetBlueprint": wbp})
        rc.call("recompile after class defaults", ok is True, str(txt)[:200])

    # (6) save, then read the tree back from the compiled asset
    saved, txt = m.call(ASSET, "save_assets", {"asset_paths": [path]})
    rc.call("save_assets", saved is not None, str(txt)[:150])
    if saved is None:
        return False          # built in memory but never on disk is not a build

    desc, _ = m.call(UMG, "GetWidgetDescription",
                     {"widgetBlueprint": wbp, "startWidget": None, "maxDepth": -1})
    rc.w("\n```")
    rc.w((desc or {}).get("description", "(no description returned)").rstrip())
    rc.w("```")

    # Ground truth is the TREE, parsed from GetWidgetDescription — NOT GetWidgets.
    #
    # GetWidgets enumerates the parent class's DECLARED BindWidget members, including ones
    # no widget satisfies. Comparing it against the plan compared a declaration list to a
    # creation list, so every optional bind the plan deliberately skipped was reported as
    # "extra" and five CORRECT assets were failed with `high` findings. Verified directly:
    # WBP_MatchBand's GetWidgets returns 6 names, its tree holds 4, and the two phantoms
    # are exactly the optional RocketCountdown* binds that have no measured geometry.
    #
    # Skipping an OPTIONAL bind is legal and often right — that is what BindWidgetOptional
    # means. Skipping a NON-OPTIONAL one is fatal, and that is caught in two other places
    # already: wbp_plan.validate() refuses the plan before an editor is opened, and
    # CompileWidgetBlueprint above fails at (5). This check's job is narrower and its name
    # says it: does the tree match the plan.
    _TREE_NODE = re.compile(r"^\s*\[\d+\]\s+\S+\s+(\S+)", re.M)
    got_names = set(_TREE_NODE.findall((desc or {}).get("description", "")))
    want_names = {n["name"] for n in spec["tree"]}
    rc.call("tree matches plan", got_names == want_names,
            f"{len(got_names)} widgets" if got_names == want_names
            else f"missing {sorted(want_names - got_names)}, extra {sorted(got_names - want_names)}")
    return got_names == want_names



def verify_one(m: MCP, rc: Receipt, asset: str, spec: dict) -> bool:
    """The NO-DELETE gate BP70 D1 demands. `build_one` deletes before it creates, so its own
    `tree matches plan` comparison can only ever see a tree built seconds earlier — a stale
    widget left in an on-disk asset by an EARLIER generation is structurally invisible to a
    build receipt (HUD-CPP-AUDIT §3). This path reads the asset AS IT SITS ON DISK and runs
    the same comparison, which is the only artifact that can prove D1's class of defect
    present or absent. It writes nothing.
    """
    path = full_path(spec["folder"], asset)
    rc.w(f"\n## VERIFY `{asset}`  →  `{path}`  (no delete, no writes)\n")

    exists, _ = m.call(ASSET, "exists", {"path": path})
    if not exists:
        rc.call("asset exists", False, "absent — nothing to verify; build it first")
        return False

    wbp = {"refPath": path}
    desc, _ = m.call(UMG, "GetWidgetDescription",
                     {"widgetBlueprint": wbp, "startWidget": None, "maxDepth": -1})
    rc.w("\n```")
    rc.w((desc or {}).get("description", "(no description returned)").rstrip())
    rc.w("```")

    # Same ground truth as build_one: the TREE, never GetWidgets (which enumerates declared
    # binds including unsatisfied optionals). `extra` here IS the stale-widget finding —
    # a name in the asset the plan does not create, exactly the shape of BP70 D1's duplicate
    # ammo readout and the killfeed double of 04efb2a.
    _TREE_NODE = re.compile(r"^\s*\[\d+\]\s+\S+\s+(\S+)", re.M)
    got_names = set(_TREE_NODE.findall((desc or {}).get("description", "")))
    want_names = {n["name"] for n in spec["tree"]}
    rc.call("on-disk tree matches plan", got_names == want_names,
            f"{len(got_names)} widgets" if got_names == want_names
            else f"missing {sorted(want_names - got_names)}, extra {sorted(got_names - want_names)}")
    return got_names == want_names


def select(args) -> dict:
    """Which assets this run touches. Narrowing is the point, not a convenience.

    A rebuild of the nine buttons used to mean either nine `--asset` invocations (nine receipts,
    nine chances to stop halfway with no record of which) or `build_wbp.py` with no flag, which
    DELETES AND RECREATES ALL FORTY-SIX. The second is how an unrelated asset gets rebuilt at a
    plan digest nobody reviewed, and it is silent — the receipt says PASS because every asset
    matched the plan it was just built from.

    `--parent` beats `--filter` and is the one to reach for. It selects by the C++ contract
    (`parent_class`), so it cannot drift from what the module actually is: add a tenth button
    type to `PLAN` tomorrow and `--parent BRButton` picks it up with no flag to update. A name
    glob is a guess about naming discipline; a parent class is a fact.
    """
    plan = wbp_plan.PLAN
    if args.asset:
        if args.asset not in plan:
            return {}
        return {args.asset: plan[args.asset]}
    if args.parent:
        want = args.parent if args.parent.startswith("/Script/") else f"/Script/Breachpoint.{args.parent}"
        return {a: s for a, s in plan.items() if s.get("parent_class") == want}
    if args.filter:
        return {a: s for a, s in plan.items() if fnmatch(a, args.filter)}
    return plan


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dry-run", action="store_true", help="validate the plan, touch no editor")
    ap.add_argument("--asset", help="build one asset instead of all")
    ap.add_argument("--filter", metavar="GLOB",
                    help="build/verify every asset whose name matches a glob "
                         "(e.g. 'WBP_Button*'). Combines with --verify and --dry-run.")
    ap.add_argument("--parent", metavar="CLASS",
                    help="build/verify every asset parented to this class "
                         "(e.g. BRButton). Selects by CONTRACT, not by name — a new "
                         "button type is included the day it is planned.")
    ap.add_argument("--list", action="store_true",
                    help="print the selected assets and exit. Touches no editor.")
    ap.add_argument("--verify", action="store_true",
                    help="read the ON-DISK assets and diff their trees against the plan; "
                         "no delete, no create, no writes. The stale-widget gate (BP70 D1).")
    args = ap.parse_args()

    problems = wbp_plan.validate_all()
    if problems:
        print("PLAN INVALID — nothing was executed:")
        for p in problems:
            print("  ERROR:", p)
        return EXIT_BLOCKED
    todo = select(args)
    # Notes AFTER selection and scoped TO it. Emitting all forty-six while nine are selected
    # buries the ones that matter under thirty-seven that do not, and a note nobody reads is
    # the same as no note. Each line is prefixed `<Asset>.<Node>: ...`, so the selection filter
    # is a prefix match on the asset name.
    print(f"plan OK: {len(wbp_plan.PLAN)} asset(s), {len(todo)} selected")
    for note in wbp_plan.skipped_brushes():
        if note.split(".", 1)[0] in todo:
            print("  NOTE (not an error): brush skipped —", note)

    if not todo:
        print("NOTHING SELECTED — no asset matches. Nothing was executed.")
        return EXIT_BLOCKED
    if args.list:
        for a, s in todo.items():
            print(f"  {a:26} {len(s['tree']):2} nodes  {s['parent_class'].rsplit('.', 1)[-1]}")
        print(f"{len(todo)} of {len(wbp_plan.PLAN)} selected")
        return EXIT_PASS
    if args.dry_run:
        for a, s in todo.items():
            print(f"  {a}: {[n['name'] for n in s['tree']]}")
            for n in s["tree"]:
                art, notes = wbp_plan.art_properties(n)
                if art:
                    print(f"    art {n['name']}: {json.dumps(art)}")
                for why in notes:
                    print(f"    SKIPPED {n['name']}: {why}")
        return EXIT_PASS

    import hashlib
    digest = hashlib.sha256((Path(__file__).parent / "wbp_plan.py").read_bytes()).hexdigest()[:16]
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    rc = Receipt(REPO / "docs/ui/receipts" / f"gen-ui-{stamp}.md",
                 " ".join(sys.argv), digest)

    m = MCP()
    try:
        m.init()
    except Exception as e:
        rc.w(f"\n**BLOCKED** — no MCP server at {URL}: {e}")
        rc.close("BLOCKED — the editor is not running, or its MCP server is off.",
                 "Nothing was built. No rung.")
        return EXIT_BLOCKED

    if args.verify:
        results = {a: verify_one(m, rc, a, s) for a, s in todo.items()}
        ok = all(results.values()) and not rc.findings
        # A verdict must name the finding it actually has. The old text blamed `extra`
        # names unconditionally, so a run whose only fault was an ABSENT asset reported
        # "diverges from the plan / stale widgets" and sent the reader hunting for a
        # divergence that was not there. A verifier that misreports why it failed is
        # worse than one that fails: it spends the next session's time on the wrong bug.
        if ok:
            verdict = "PASS — every on-disk tree matches the plan; no stale widgets."
        elif rc.findings:
            verdict = "FAIL — " + "; ".join(msg for _sev, msg in rc.findings)
        else:
            verdict = ("FAIL — an on-disk tree diverges from the plan; `extra` names are "
                       "stale widgets (BP70 D1's class); rebuild the asset from the plan.")
        rc.close(
            verdict,
            "- **Read-only.** Nothing was deleted, created or written.\n"
            "- **Not a rung.** A matching tree is not a rendered screen.")
        print(f"\nreceipt: {rc.path.relative_to(REPO)}")
        return EXIT_PASS if ok else EXIT_FAIL

    results = {a: build_one(m, rc, a, s) for a, s in todo.items()}
    # A verdict that ignores its own findings is the false-PASS this project keeps a ladder
    # for. Any failed call sinks the run, whether or not the final tree happened to match.
    ok = all(results.values()) and not rc.findings

    rc.close(
        ("PASS — every asset rebuilt from the plan, every slot AND art write verified by "
         "read-back, every tree compiled." if ok else
         "FAIL — see the failed calls above. " +
         f"{sum(1 for v in results.values() if not v)} of {len(results)} asset(s) did not build."),
        "- **Not a rung.** A compiled WBP is not a rendered one. This proves the asset matches "
        "the plan; it does not prove the layout is correct on screen, and `ui-presentation` §11 "
        "still requires the screen be rendered and looked at.\n"
        "- **A verified font write is not legible text.** Read-back proves the face, size and "
        "letter spacing landed on the property. It does not prove a 20px style fits a 43px "
        "clock box once real glyphs are shaped, and it does not prove the typeface resolved "
        "inside the composite font rather than falling back. Both need eyes on a render.\n"
        "- **Not a multiplayer claim.** Law 6: PIE is not multiplayer, and a UI claim from PIE "
        "alone is not a UI claim.\n"
        "- **Does not prove R26 compliance.** Zero graph nodes is asserted by construction here "
        "(this generator never calls `BindToEventProperty`), not measured. "
        "nothing measures it — the audit that would has not been written.")
    print(f"\nreceipt: {rc.path.relative_to(REPO)}")
    return EXIT_PASS if ok else EXIT_FAIL


if __name__ == "__main__":
    sys.exit(main())
