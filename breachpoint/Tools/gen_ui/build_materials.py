#!/usr/bin/env python3
"""Executor half of the UI-material generator. Requires a LIVE editor with the MCP server.

    python3 Tools/gen_ui/build_materials.py [--dry-run] [--probe] [--asset M_UI_RadialSweep]

Reads `material_plan.py`, rebuilds each material from scratch, and writes an R37 receipt to
`docs/ui/receipts/` flushed per line — a run that dies leaves a record of where.

    --dry-run   validate the plan and print the graph. Touches no editor.
    --probe     connect, dump every toolset schema into the receipt, build NOTHING.
                RUN THIS FIRST. See "unverified" below.

Transport is `build_wbp.MCP`, IMPORTED, not copied — one HTTP/SSE quirk, one place to fix.
The three things `build_wbp.py` learned the expensive way apply here unchanged and are not
re-derived: delete-before-create (the clobber guard is `FindPackage`, i.e. MEMORY), full
`/Game/X/Y.Y` ref paths (the short form makes the server silently drop the whole argument),
camelCase property names with `values` as a JSON *string*. Plus one this file learned:
an MCP schema wants EVERY property present — `"default": null` does NOT mean optional — so
every call below spells out its full argument set even where the value is the default.


WHICH CALLS ARE UNVERIFIED  (read this before running; it is the whole risk of this file)
========================================================================================
This file was written WITHOUT touching the editor (R29.2: another lane holds the driver
seat), so `describe_toolset` was never called and the MaterialTools schema below is
INFERRED from the naming convention the verified toolsets use. Every such call is tagged
`# UNVERIFIED` at its site and routed through the `T` table at the top of the file, so a
schema mismatch is a one-line edit per call, never a rewrite.

  VERIFIED — exercised by build_wbp.py against this exact server:
    AssetTools.exists / delete / save_assets
    ObjectTools.set_properties / get_properties      (camelCase names, `values` as a string)

  UNVERIFIED — every MaterialTools call, and every property NAME written to a material or
  an expression. In particular:
    MaterialTools.create_material            arg names, and whether it returns a ref
    MaterialTools.create_expression          arg names; whether `nodePos` is required
    MaterialTools.connect_expressions        input-pin naming ("A"/"B"/"Input"/"X"/"Y")
    MaterialTools.connect_property           property enum spelling ("EmissiveColor")
    MaterialTools.recompile_material         name, and whether it is needed at all
    materialDomain / blendMode / shadingModel   the camelCase spellings on UMaterial
  Everything unverified is READ BACK and compared, so an unverified call that silently
  no-ops fails the run rather than producing a green receipt over a broken binary. That is
  the only reason it is safe to write this blind.

MITIGATION: `--probe` dumps the real schemas into a receipt and builds nothing. It costs one
run and converts every line above from a guess into a fact. Do that first.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
from datetime import datetime, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import material_plan                                   # noqa: E402
from build_wbp import MCP, Receipt as _WbpReceipt      # noqa: E402  transport, verbatim

REPO = Path(__file__).resolve().parents[2]

ASSET = "editor_toolset.toolsets.asset.AssetTools"
OBJ = "editor_toolset.toolsets.object.ObjectTools"
MAT = "editor_toolset.toolsets.material.MaterialTools"       # UNVERIFIED toolset path

EXIT_PASS, EXIT_FAIL, EXIT_BLOCKED = 0, 1, 3

# ---------------------------------------------------------------------------------------
# Every guessed tool name, in ONE table. When --probe says a name is wrong, this is the
# only place to edit — that indirection is the entire point of the table and the reason it
# exists for a single-material generator.
# ---------------------------------------------------------------------------------------
T = {
    "create_material":  "create_material",       # UNVERIFIED
    "create_expression": "add_expression",       # VERIFIED by --probe 3 Aug 2026:
                                             # create_expression does not exist.
    "connect_expressions": "connect_expressions",  # UNVERIFIED
    "connect_property": "connect_to_output",     # VERIFIED: material OUTPUTS, not properties.
    "recompile": "recompile",                    # VERIFIED: bare name.
}

# UMaterial property names, camelCase per the ObjectTools convention. UNVERIFIED spellings —
# but written through the VERIFIED set_properties/get_properties pair, so a wrong name is
# caught by the read-back rather than shipped.
MAT_PROPS = {"domain": "materialDomain", "blend_mode": "blendMode",
             "shading_model": "shadingModel"}


class Receipt(_WbpReceipt):
    """`build_wbp.Receipt` with an honest title.

    Deliberately does NOT call super().__init__: the base hardcodes "WBP generator" into its
    heading, and an R37 receipt that misnames its own generator is exactly the kind of small
    lie that makes a receipt worthless. Everything else — `w`, `call`, `close`, the per-line
    flush — is inherited unchanged.
    """

    def __init__(self, path: Path, argv: str, plan_digest: str, mode: str):
        path.parent.mkdir(parents=True, exist_ok=True)
        self.f = path.open("w", buffering=1)
        self.path = path
        self.findings: list[tuple[str, str]] = []
        self.w(f"# RECEIPT — UI material generator ({mode}) · "
               f"{datetime.now(timezone.utc).isoformat()}")
        self.w("")
        self.w(f"**Command:** `{argv}`")
        self.w(f"**Plan:** `Tools/gen_ui/material_plan.py` sha256 `{plan_digest}`")
        self.w("**Transport:** `build_wbp.MCP`, imported. Raw HTTP `http://127.0.0.1:8000/mcp`.")
        self.w("**Caveat:** every `MaterialTools.*` call in this run is UNVERIFIED — written "
               "against an inferred schema without editor access (R29.2). Every unverified "
               "write is read back; an unverified call that no-ops fails the run.")
        self.w("")


# --------------------------------------------------------------------------- probe
def probe(m: MCP, rc: Receipt) -> None:
    """Dump each toolset's real schema into the receipt. Builds nothing, changes nothing.

    `describe_toolset` is a TOP-LEVEL tool, not something reached through `call_tool`, so it
    goes through `m._post` rather than `m.call`. Using the transport's own poster keeps the
    session id and the SSE unwrapping — this is reuse, not a fork.
    """
    for ts in (MAT, OBJ, ASSET):
        rc.w(f"\n## `describe_toolset({ts})`\n")
        try:
            m.n += 1
            r = m._post({"jsonrpc": "2.0", "id": 500 + m.n, "method": "tools/call",
                         "params": {"name": "describe_toolset",
                                    "arguments": {"toolset_name": ts}}})
            txt = "\n".join(c.get("text", "")
                            for c in (r or {}).get("result", {}).get("content", []))
        except Exception as e:                     # a probe never sinks the run
            txt = f"(describe_toolset failed: {e})"
        rc.w("```")
        rc.w(txt.rstrip() or "(empty)")
        rc.w("```")


# --------------------------------------------------------------------------- build
def _readback(m: MCP, instance, wanted: dict) -> tuple[bool, str]:
    """set_properties then get_properties, compared. An unverified write is not a write."""
    m.call(OBJ, "set_properties", {"instance": instance, "values": json.dumps(wanted)})
    got, txt = m.call(OBJ, "get_properties",
                      {"instance": instance, "properties": list(wanted)})
    try:
        back = json.loads(got) if isinstance(got, str) else got
    except Exception:
        back = None
    ok = isinstance(back, dict) and all(_close(back.get(k), v) for k, v in wanted.items())
    return ok, f"wrote {json.dumps(wanted)[:140]}; read {json.dumps(back)[:140]}"


def _close(a, b):
    """Float-tolerant compare. Enum reads may come back as a name or an int, so a string
    property matches if either side contains the other — `MD_UI` vs `EMaterialDomain::MD_UI`
    is agreement, not drift, and failing it would make every run red for no reason."""
    if isinstance(b, dict):
        return isinstance(a, dict) and all(_close(a.get(k), v) for k, v in b.items())
    if isinstance(b, float):
        return isinstance(a, (int, float)) and abs(a - b) < 1e-4
    if isinstance(b, str) and isinstance(a, str):
        return a == b or b in a or a in b
    return a == b


def build_one(m: MCP, rc: Receipt, asset: str, spec: dict) -> bool:
    folder = spec["folder"]
    path = f"{folder}/{asset}.{asset}"        # full form — the short one is silently dropped
    rc.w(f"\n## `{asset}`  →  `{path}`\n")
    rc.w(f"domain {spec['domain']} · blend {spec['blend_mode']} · "
         f"{len(spec['graph'])} nodes · {spec['notes']}\n")

    # (1) delete first. The create path's clobber guard is FindPackage — MEMORY, not disk —
    # so in a fresh editor it happily "creates" over an existing on-disk asset.
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

    # (2) create.  VERIFIED against the live schema, 3 Aug 2026 — and the inferred names
    # were wrong in BOTH directions: camelCase `folderPath`/`assetName` (guessed from
    # UMGToolSet.CreateWidgetBlueprint) are actually snake_case, and `material_function`
    # is REQUIRED even though its only sensible value here is null. This server names one
    # missing required param per attempt, so each wrong guess costs a full round trip.
    res, txt = m.call(MAT, T["create_material"],
                      {"folder_path": folder, "asset_name": asset,
                       "material_function": None})
    rc.call(f"MaterialTools.{T['create_material']} (UNVERIFIED)", res is not None, txt[:200])
    if res is None:
        return False
    mat = {"refPath": path}

    # (3) THE TWO PROPERTIES THAT SILENTLY BREAK EVERY UMG MATERIAL, set before any node.
    #
    # A Surface-domain material in a widget brush renders wrong or not at all and reports
    # nothing — it is the single most common way a UI material fails, and it fails LOOKING
    # like a layout bug. Set first so a run that dies mid-graph still leaves an asset whose
    # domain is at least right. Written through the VERIFIED set/get pair, so the only guess
    # here is the camelCase name, and the read-back catches that.
    ok, detail = _readback(m, mat, {MAT_PROPS["domain"]: spec["domain"],
                                    MAT_PROPS["blend_mode"]: spec["blend_mode"],
                                    MAT_PROPS["shading_model"]: spec["shading_model"]})
    rc.call("material domain/blend/shading (property NAMES unverified)", ok, detail)
    if not ok:
        return False        # a Surface-domain UI material is worse than no material

    # (4) nodes, in plan order — which validate() guarantees is dependency order.
    handles: dict[str, dict] = {}
    for i, (name, node) in enumerate(spec["graph"].items()):
        # UNVERIFIED. `nodePos` is passed because MCP schemas want EVERY property present;
        # the layout is a readability nicety for whoever opens the graph, nothing more.
        # VERIFIED schema: add_expression(material_or_function, expression_class, x, y).
        # x/y are FLAT INTEGERS, not a nodePos struct - the guessed camelCase names were
        # wrong on all three counts.
        info, txt = m.call(MAT, T["create_expression"], {
            "material_or_function": mat,
            "expression_class": {"refPath": f"/Script/Engine.{node['type']}"},
            "x": -1600 + 260 * (i % 6), "y": -400 + 130 * (i // 6),
        })
        ok = info is not None
        rc.call(f"create {name} ({node['type'][len('MaterialExpression'):]}) UNVERIFIED",
                ok, "" if ok else txt[:200])
        if not ok:
            return False
        handles[name] = info if isinstance(info, dict) else {"expression": info}

        if node["props"]:
            ok, detail = _readback(m, handles[name].get("expression", handles[name]),
                                   node["props"])
            rc.call(f"props {name}", ok, detail)
            if not ok:
                return False

    # (5) wires. Source pin is output 0 on every node in this plan (no multi-output nodes),
    # so only the DESTINATION pin name is a guess. UNVERIFIED.
    for name, node in spec["graph"].items():
        for pin, src in node["inputs"].items():
            # An input may name its SOURCE PIN: ("Color", "A") reads the alpha output.
            src, src_out = src if isinstance(src, tuple) else (src, "")
            # VERIFIED: connect_expressions(from_expression, from_output_name,
            # to_expression, to_input_name). It takes NO material - the expressions
            # already know which material they belong to.
            # VERIFIED, and the plan's pin names were wrong for single-input nodes.
            # get_expression_input_names on a ComponentMask returns ['None'] - UE's
            # UNNAMED input, not a pin literally called "Input". Ask the node what its
            # pins are called and fall back to the unnamed one rather than guessing:
            # a wrong pin name is a silent no-wire, and the compile only says
            # "unconnected input" without naming which guess was wrong.
            to_ref = handles[name].get("expression", handles[name])
            names, _ = m.call(MAT, "get_expression_input_names", {"expression": to_ref})
            real = pin if (names and pin in names) else (names[0] if names else "")
            if real == "None":
                real = ""
            _, txt = m.call(MAT, T["connect_expressions"], {
                "from_expression": handles[src].get("expression", handles[src]),
                "from_output_name": src_out,
                "to_expression": to_ref,
                "to_input_name": real,
            })
            # Nothing reliable to read back per-wire; the compile in (7) is what proves the
            # graph, and a missing wire shows up there as an unconnected-input error.
            rc.call(f"wire {src} -> {name}.{pin} (UNVERIFIED)",
                    "**ERROR**" not in txt and "**FAILED**" not in txt, txt[:120])

    # (6) material outputs. UNVERIFIED enum spelling.
    for prop, src in spec["outputs"].items():
        # VERIFIED: connect_to_output(expression, output_name, material_property).
        # Again no material argument, and the property arg is `material_property`.
        _, txt = m.call(MAT, T["connect_property"], {
            "expression": handles[src].get("expression", handles[src]),
            "output_name": "",
            "material_property": prop if prop.startswith("MP_") else "MP_" + prop,
        })
        rc.call(f"output {prop} <- {src} (UNVERIFIED)",
                "**ERROR**" not in txt and "**FAILED**" not in txt, txt[:120])

    # (7) compile. This is where an unconnected input or a bad pin name finally speaks.
    _, txt = m.call(MAT, T["recompile"], {"material_or_function": mat})
    rc.call(f"MaterialTools.{T['recompile']} (UNVERIFIED)",
            "**ERROR**" not in txt and "**FAILED**" not in txt, str(txt)[:200])

    # (8) save — built in memory but never on disk is not a build.
    saved, txt = m.call(ASSET, "save_assets", {"asset_paths": [path]})
    rc.call("save_assets", saved is not None, str(txt)[:150])
    if saved is None:
        return False

    # (9) THE CHECK THE WHOLE PACKET TURNS ON, read back off the saved asset.
    #
    # `SetScalarParameterValue` against a name the material does not carry is a NO-OP: no
    # warning, no log, no crash, the ring simply never sweeps. material_plan.validate()
    # already proved plan-name == C++-constant in text mode; this proves ASSET-name == plan
    # name, and the two together close the loop from the .cpp to the binary.
    for name, node in spec["graph"].items():
        want = node["props"].get("parameterName")
        if not want:
            continue
        got, txt = m.call(OBJ, "get_properties",
                          {"instance": handles[name].get("expression", handles[name]),
                           "properties": ["parameterName"]})
        try:
            back = json.loads(got) if isinstance(got, str) else got
        except Exception:
            back = None
        got_name = (back or {}).get("parameterName")
        rc.call(f"parameter '{want}' present on the saved asset",
                got_name == want, f"read {got_name!r}")
        if got_name != want:
            return False

    return True


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dry-run", action="store_true", help="validate the plan, touch no editor")
    ap.add_argument("--probe", action="store_true", help="dump toolset schemas, build nothing")
    ap.add_argument("--asset", help="build one material instead of all")
    args = ap.parse_args()

    problems = material_plan.validate_all()
    if problems:
        print("PLAN INVALID — nothing was executed:")
        for p in problems:
            print("  ERROR:", p)
        return EXIT_BLOCKED
    print(f"plan OK: {len(material_plan.PLAN)} material(s)")

    todo = ({args.asset: material_plan.PLAN[args.asset]} if args.asset
            else material_plan.PLAN)
    if args.dry_run:
        for a, s in todo.items():
            print(material_plan.describe(a, s))
        return EXIT_PASS

    digest = hashlib.sha256(
        (Path(__file__).parent / "material_plan.py").read_bytes()).hexdigest()[:16]
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    mode = "probe" if args.probe else "build"
    rc = Receipt(REPO / "docs/ui/receipts" / f"gen-materials-{mode}-{stamp}.md",
                 " ".join(sys.argv), digest, mode)

    m = MCP()
    try:
        m.init()
    except Exception as e:
        rc.w(f"\n**BLOCKED** — no MCP server at 127.0.0.1:8000: {e}")
        rc.close("BLOCKED — the editor is not running, or its MCP server is off.",
                 "Nothing was built. No rung.")
        print(f"\nreceipt: {rc.path.relative_to(REPO)}")
        return EXIT_BLOCKED

    if args.probe:
        probe(m, rc)
        rc.close("PROBE — schemas dumped above. NOTHING WAS BUILT.",
                 "- **Not a rung.** No asset was created, modified or saved.\n"
                 "- Its one job is to replace every `UNVERIFIED` in this generator with a "
                 "fact. Diff the dumped schemas against the `T` table and `MAT_PROPS` in "
                 "`build_materials.py` before running a real build.")
        print(f"\nreceipt: {rc.path.relative_to(REPO)}")
        return EXIT_PASS

    results = {a: build_one(m, rc, a, s) for a, s in todo.items()}
    # A verdict that ignores its own findings is the false-PASS this project keeps a ladder
    # for: any failed call sinks the run, whatever the final asset looks like.
    ok = all(results.values()) and not rc.findings

    rc.close(
        ("PASS — material rebuilt from the plan, domain/blend verified by read-back, every "
         "parameter name confirmed present on the saved asset." if ok else
         "FAIL — see the failed calls above. "
         f"{sum(1 for v in results.values() if not v)} of {len(results)} did not build."),
        "- **Not a rung.** A compiled material is not a RENDERED one. Nothing here proves "
        "the sweep goes clockwise, starts at 12, or antialiases — that is a screenshot of "
        "`WBP_Screen_DeathRespawn` with `Sweep` stepped 0 / 0.25 / 0.5 / 1, and this "
        "generator cannot take it.\n"
        "- **Does not prove the ring is the right THICKNESS.** `Thickness` defaults to a "
        "PROVISIONAL 0.0769: `figma_screen_layout.json` measures the 104px box and no "
        "stroke, and no respawn-ring art was ever exported. Filed as a contract_gap.\n"
        "- **Not a multiplayer claim, not a PIE claim.** Law 6.\n"
        "- **Unverified schema.** Every `MaterialTools` call was written without editor "
        "access. A PASS means the read-backs agreed, not that the calls were the right "
        "ones — run `--probe` and reconcile before trusting this receipt twice.")
    print(f"\nreceipt: {rc.path.relative_to(REPO)}")
    return EXIT_PASS if ok else EXIT_FAIL


if __name__ == "__main__":
    sys.exit(main())

# ---------------------------------------------------------------------------
# VERIFIED MaterialTools SCHEMA — probed against the live editor, 3 Aug 2026.
# ---------------------------------------------------------------------------
# The plan was authored without an editor (R29.2) and tagged every call UNVERIFIED.
# The probe was worth running: THREE OF FIVE ASSUMED TOOL NAMES DO NOT EXIST.
#
#   create_expression   -> add_expression
#   connect_property    -> connect_to_output      (materials have OUTPUTS, not properties)
#   recompile_material  -> recompile
#
# Argument names cost four more round trips, and the pattern is worth internalising:
# this server names every required param and REFUSES the whole call until all are
# present, one error at a time. "default": null does not mean optional.
#
#   create_material(folder_path, asset_name, material_function)
#       NOT package_path, NOT name. material_function must be present as null.
#   list_expression_classes(material_or_function, search)
#       NOT filter. Needs an EXISTING material to ask against - there is no
#       standalone class list, so probing means creating a scratch asset first.
#
# Expression classes are refPaths, e.g. /Script/Engine.MaterialExpressionFrac.
# 354 are available and every node this graph needs is among them. Two names are
# substring traps when searching: "TextureCoordinate" also matches
# MeshPaintTextureCoordinateIndex, and "Distance" also matches DistanceCullFade -
# match the FULL class name, never a substring.
