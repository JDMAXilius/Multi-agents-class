#!/usr/bin/env python3
"""Executor half of the WBP generator. Requires a LIVE editor with the MCP server.

    python3 Tools/gen_ui/build_wbp.py [--dry-run] [--asset WBP_RootLayout]

Reads `wbp_plan.py`, rebuilds each WBP from scratch, and writes an R37 receipt to
`docs/ui/receipts/` flushed per line — a run that dies leaves a record of where.

Three things learned the expensive way, encoded here rather than in a comment nobody reads:

1. `CreateWidgetBlueprint`'s clobber guard is `FindPackage` — MEMORY, not disk. In a fresh
   editor where the asset is unloaded it takes the create path against an existing on-disk
   asset. Regeneration therefore DELETES first, explicitly, and asserts the delete.
2. Object refs need the full `/Game/X/Y.Y` form. The short form makes the server drop the
   whole argument object and report "input params Json is empty", which names the wrong cause.
3. Property names are camelCase (`layoutData`, `brushColor`), not C++ PascalCase, and
   `set_properties` takes `values` as a JSON *string*. A wrong name fails silently — which is
   why every write here is read back and compared.

`BindToEventProperty` is deliberately NOT wired up: it adds a Blueprint event graph NODE,
which is the artifact R18/R26 forbid and `audit_wbp.py` exists to catch.
"""
from __future__ import annotations

import argparse, json, sys, urllib.request
from datetime import datetime, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import wbp_plan  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
URL = "http://127.0.0.1:8000/mcp"

UMG = "UMGToolSet.UMGToolSet"
OBJ = "editor_toolset.toolsets.object.ObjectTools"
ASSET = "editor_toolset.toolsets.asset.AssetTools"

EXIT_PASS, EXIT_FAIL, EXIT_BLOCKED = 0, 1, 3


# --------------------------------------------------------------------------- transport
class MCP:
    def __init__(self):
        self.sid = None
        self.n = 0

    def _post(self, payload):
        req = urllib.request.Request(URL, data=json.dumps(payload).encode(), method="POST")
        req.add_header("Content-Type", "application/json")
        req.add_header("Accept", "application/json, text/event-stream")
        if self.sid:
            req.add_header("Mcp-Session-Id", self.sid)
        with urllib.request.urlopen(req, timeout=120) as r:
            self.sid = r.headers.get("Mcp-Session-Id") or self.sid
            body = r.read().decode()
        if body.lstrip().startswith(("event:", "data:")):
            body = "\n".join(l[5:].strip() for l in body.splitlines() if l.startswith("data:"))
        return json.loads(body) if body.strip() else None

    def init(self):
        self._post({"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {
            "protocolVersion": "2025-06-18", "capabilities": {},
            "clientInfo": {"name": "breachpoint-gen-ui", "version": "1"}}})
        try:
            self._post({"jsonrpc": "2.0", "method": "notifications/initialized"})
        except Exception:
            pass

    def call(self, toolset, tool, args):
        self.n += 1
        r = self._post({"jsonrpc": "2.0", "id": 100 + self.n, "method": "tools/call",
                        "params": {"name": "call_tool", "arguments": {
                            "toolset_name": toolset, "tool_name": tool, "arguments": args}}})
        res = r.get("result", {}) if r else {}
        txt = "\n".join(c["text"] for c in res.get("content", []) if c.get("type") == "text")
        if "error" in (r or {}):
            return None, "**ERROR** " + json.dumps(r["error"])
        if res.get("isError"):
            return None, "**FAILED** " + txt
        try:
            return json.loads(txt).get("returnValue"), txt
        except Exception:
            return txt, txt


# --------------------------------------------------------------------------- receipt
class Receipt:
    def __init__(self, path: Path, argv: str, plan_digest: str):
        path.parent.mkdir(parents=True, exist_ok=True)
        self.f = path.open("w", buffering=1)          # line buffered, per R37
        self.path = path
        self.findings: list[tuple[str, str]] = []
        self.w(f"# RECEIPT — WBP generator · {datetime.now(timezone.utc).isoformat()}")
        self.w("")
        self.w(f"**Command:** `{argv}`")
        self.w(f"**Plan:** `Tools/gen_ui/wbp_plan.py` sha256 `{plan_digest}`")
        self.w("**Transport:** raw HTTP `http://127.0.0.1:8000/mcp` (the MCP server runs inside "
               "the editor; under `bEnableToolSearch=true` the toolsets are reached via "
               "`call_tool` and never appear by name in a session tool list).")
        self.w("")

    def w(self, s=""):
        print(s)
        self.f.write(s + "\n")

    def call(self, label, ok, detail):
        self.w(f"- {'PASS' if ok else '**FAILED**'} `{label}` — {detail}")
        if not ok:
            self.findings.append(("high", f"{label}: {detail}"))

    def close(self, verdict, rung_note):
        self.w("")
        self.w("## Findings")
        if self.findings:
            for sev, f in self.findings:
                self.w(f"- **{sev}** — {f}")
        else:
            self.w("- none")
        self.w("")
        self.w("## Verdict")
        self.w(verdict)
        self.w("")
        self.w("## Rung honesty — what this PASS does not mean")
        self.w(rung_note)
        self.f.close()


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
            m.call(OBJ, "set_properties",
                   {"instance": info["slot"], "values": json.dumps(slot_spec)})
            got, txt = m.call(OBJ, "get_properties",
                              {"instance": info["slot"], "properties": list(slot_spec)})
            try:
                readback = json.loads(got) if isinstance(got, str) else got
            except Exception:
                readback = None
            ok = readback is not None and all(
                _close(readback.get(k), v) for k, v in slot_spec.items())
            rc.call(f"slot {node['name']}", ok,
                    f"wrote {json.dumps(slot_spec)}; read {json.dumps(readback)[:180]}")
        elif slot_spec:
            rc.call(f"slot {node['name']}", False, "AddWidget returned no slot handle")

    # (5) compile — the BindWidget contract is enforced HERE by the engine
    ok, txt = m.call(UMG, "CompileWidgetBlueprint", {"widgetBlueprint": wbp})
    rc.call("CompileWidgetBlueprint", ok is True, str(txt)[:200])
    if ok is not True:
        return False

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

    final, _ = m.call(UMG, "GetWidgets", {"widgetBlueprint": wbp})
    got_names = {w["widgetName"] for w in (final or {}).get("widgets", [])}
    want_names = {n["name"] for n in spec["tree"]}
    rc.call("tree matches plan", got_names == want_names,
            f"{len(got_names)} widgets" if got_names == want_names
            else f"missing {sorted(want_names - got_names)}, extra {sorted(got_names - want_names)}")
    return got_names == want_names


def _close(a, b):
    if isinstance(b, dict):
        return isinstance(a, dict) and all(_close(a.get(k), v) for k, v in b.items())
    if isinstance(b, float):
        return isinstance(a, (int, float)) and abs(a - b) < 1e-4
    return a == b


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dry-run", action="store_true", help="validate the plan, touch no editor")
    ap.add_argument("--asset", help="build one asset instead of all")
    args = ap.parse_args()

    problems = wbp_plan.validate_all()
    if problems:
        print("PLAN INVALID — nothing was executed:")
        for p in problems:
            print("  ERROR:", p)
        return EXIT_BLOCKED
    print(f"plan OK: {len(wbp_plan.PLAN)} asset(s)")

    todo = {args.asset: wbp_plan.PLAN[args.asset]} if args.asset else wbp_plan.PLAN
    if args.dry_run:
        for a, s in todo.items():
            print(f"  {a}: {[n['name'] for n in s['tree']]}")
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

    results = {a: build_one(m, rc, a, s) for a, s in todo.items()}
    # A verdict that ignores its own findings is the false-PASS this project keeps a ladder
    # for. Any failed call sinks the run, whether or not the final tree happened to match.
    ok = all(results.values()) and not rc.findings

    rc.close(
        ("PASS — every asset rebuilt from the plan, every slot write verified by read-back, "
         "every tree compiled." if ok else
         "FAIL — see the failed calls above. " +
         f"{sum(1 for v in results.values() if not v)} of {len(results)} asset(s) did not build."),
        "- **Not a rung.** A compiled WBP is not a rendered one. This proves the asset matches "
        "the plan; it does not prove the layout is correct on screen, and `ui-presentation` §11 "
        "still requires the screen be rendered and looked at.\n"
        "- **Not a multiplayer claim.** Law 6: PIE is not multiplayer, and a UI claim from PIE "
        "alone is not a UI claim.\n"
        "- **Does not prove R26 compliance.** Zero graph nodes is asserted by construction here "
        "(this generator never calls `BindToEventProperty`), not measured. "
        "`Tools/audit_ui/audit_wbp.py` is what measures it, and it does not exist yet.")
    print(f"\nreceipt: {rc.path.relative_to(REPO)}")
    return EXIT_PASS if ok else EXIT_FAIL


if __name__ == "__main__":
    sys.exit(main())
