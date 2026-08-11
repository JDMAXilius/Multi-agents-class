"""
Generic Blueprint graph reader. Give it an asset path and function names; it prints the DSL.

Generalised out of weapon_graphs.py once a second asset needed the same read. Same three
traps apply and are handled here:
  - the server reports tool failures INSIDE the payload as "**FAILED**" while the HTTP call
    succeeds, so a transport-level OK is not a success;
  - `blueprint` must be a full OBJECT path (package.asset), not a package path;
  - `read_graph_dsl` takes a `graph` ref (package.asset:FunctionName), not blueprint+name.

Run with the EDITOR OPEN. Reads only; writes nothing.
    python mcp-bp/read_graphs.py /Game/Path/BP_Thing [FuncA FuncB ...]
    python mcp-bp/read_graphs.py /Game/Path/BP_Thing          # lists functions, reads all
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "mcp-ui" / "gen_ui"))
from mcp import MCP, EXIT_PASS, EXIT_BLOCKED, EXIT_FAIL  # noqa: E402

BPT = "editor_toolset.toolsets.blueprint.BlueprintTools"


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return EXIT_FAIL

    bp = sys.argv[1].rstrip("/")
    bpobj = bp + "." + bp.rsplit("/", 1)[-1]
    wanted = sys.argv[2:]

    m = MCP()
    try:
        m.init()
    except Exception as exc:
        print("BLOCKED - no MCP server (is the editor open?): %s" % exc)
        return EXIT_BLOCKED

    def call(tool, args):
        try:
            r = m.call(BPT, tool, args)
        except Exception as exc:
            return None, "transport: %s" % exc
        text = json.dumps(r)
        if "**FAILED**" in text:
            return None, text[text.find("**FAILED**"):][:200]
        return r, None

    if not wanted:
        fns, err = call("list_functions", {"blueprint": {"refPath": bpobj}})
        if err:
            print("list_functions FAILED: %s" % err)
            return EXIT_FAIL
        # The payload nests: [[{name:...}, ...], "<schema blurb>"]. Walk to the first list
        # of dicts rather than assuming a depth - assuming one raised TypeError here.
        rows = fns
        while isinstance(rows, list) and rows and isinstance(rows[0], list):
            rows = rows[0]
        wanted = [f["name"] for f in rows if isinstance(f, dict) and "name" in f]
        print("functions: %s\n" % ", ".join(wanted))

    for fn in wanted:
        dsl, err = call("read_graph_dsl", {"graph": {"refPath": "%s:%s" % (bpobj, fn)}})
        print("=" * 78)
        print("### %s" % fn)
        print("=" * 78)
        if err:
            print("FAILED: %s" % err)
            continue
        text = dsl
        if isinstance(dsl, list) and dsl and isinstance(dsl[0], str):
            text = dsl[0]
        elif isinstance(dsl, dict):
            text = dsl.get("returnValue", json.dumps(dsl))
        print(text)
        print()

    return EXIT_PASS


sys.exit(main())
