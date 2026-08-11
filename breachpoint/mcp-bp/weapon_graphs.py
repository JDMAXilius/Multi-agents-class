"""
BP82 â€” read BP_FPST_BaseWeapon's six function graphs, exec order first.

Step 1 of the blueprint-to-cpp method, second half. `bp_extract.py` gave the CDO variables;
this gives the LOGIC. Nothing is written from a function's NAME - that is inference wearing
the costume of a read, and it is the mistake this whole method exists to stop.

THE RULE THIS SCRIPT ENFORCES (blueprint-to-cpp Sec 2): a K2 pin can carry BOTH a default
literal and an incoming wire. THE WIRE WINS; the literal is dead data. A walker that prints
the literal reports every Branch as Condition=true and every wired scalar as 0.0. On the
character port that produced ~30 inferences dressed as reads and four shipped bugs.

Run with the EDITOR OPEN (this reads a live session; it writes nothing):
    python mcp-bp/weapon_graphs.py
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "mcp-ui" / "gen_ui"))
from mcp import MCP, EXIT_PASS, EXIT_BLOCKED  # noqa: E402

BP = "/Game/FPSTemplate/Blueprints/Weapons/BP_FPST_BaseWeapon"

# The server wants a full OBJECT path, not a package path: "/Game/X/BP_Y" is rejected with
# "is not a valid object path", "/Game/X/BP_Y.BP_Y" is accepted. A function graph is a
# subobject of the Blueprint, addressed with a colon.
BPOBJ = BP + "." + BP.rsplit("/", 1)[-1]


def graph_ref(fn):
    return {"refPath": "%s:%s" % (BPOBJ, fn)}
BPT = "editor_toolset.toolsets.blueprint.BlueprintTools"


def main() -> int:
    m = MCP()
    try:
        m.init()
    except Exception as exc:
        print("BLOCKED - no MCP server (is the editor open?): %s" % exc)
        return EXIT_BLOCKED

    out = {"blueprint": BP, "functions": {}, "errors": []}

    def call(tool, args, label):
        """
        A transport-level OK is NOT a success. This server returns tool failures INSIDE the
        payload as a "**FAILED**" string while the HTTP call itself succeeds, so the first
        version of this script printed "OK" seven times over seven errors. Same shape as
        reading an empty log as a passing run: check the result, not the absence of an
        exception.
        """
        try:
            r = m.call(BPT, tool, args)
        except Exception as exc:
            print("  FAIL %-22s transport: %s" % (tool, exc))
            out["errors"].append("%s(%s) transport %s" % (tool, label, exc))
            return None
        text = json.dumps(r)
        if "**FAILED**" in text or "is required by the function input schema" in text:
            snippet = text[text.find("**FAILED**"):][:160] if "**FAILED**" in text else text[:160]
            print("  FAIL %-22s %s" % (tool, snippet))
            out["errors"].append("%s(%s) -> %s" % (tool, label, snippet))
            return None
        print("  OK   %-22s %s" % (tool, label))
        return r

    print("=== list_functions ===")
    fns = call("list_functions", {"blueprint": {"refPath": BPOBJ}}, BPOBJ)
    if fns is not None:
        out["list_functions"] = fns
        print(json.dumps(fns, indent=1)[:1500])

    targets = ["GetCurrentFireMode", "NextFireMode", "GetFireAnimMontage",
               "GetScopeType", "GetReloadBoltAnimMontage", "PlayAnim", "ResetAttachments",
               "UserConstructionScript"]

    for fn in targets:
        print("\n=== %s ===" % fn)
        entry = {}
        # DSL first: it is the readable form. It DIES on a collapsed graph (it casts the
        # graph's outer to a Blueprint, and a collapsed graph's outer is the K2Node_Composite),
        # so a failure here is expected for some graphs and is not fatal.
        dsl = call("read_graph_dsl", {"graph": graph_ref(fn)}, fn)
        if dsl is not None:
            entry["dsl"] = dsl
        # Node-level fallback, which is what actually survives collapsed graphs.
        nodes = call("find_nodes", {"graph": graph_ref(fn)}, fn)
        if nodes is not None:
            entry["nodes"] = nodes
        if entry:
            out["functions"][fn] = entry

    dest = REPO / "mcp-bp" / "weapon_graphs.json"
    dest.write_text(json.dumps(out, indent=1), encoding="utf-8")
    print("\nwrote %s" % dest)
    print("errors: %d" % len(out["errors"]))
    return EXIT_PASS


sys.exit(main())

