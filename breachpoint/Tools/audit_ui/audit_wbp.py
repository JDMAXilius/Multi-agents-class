#!/usr/bin/env python3
"""The mechanical R26 gate for Widget Blueprints. Requires a LIVE editor's MCP server.

    python3 Tools/audit_ui/audit_wbp.py               # audit every WBP under Content/
    python3 Tools/audit_ui/audit_wbp.py --asset WBP_RootLayout
    python3 Tools/audit_ui/audit_wbp.py --selftest    # prove the logic, no engine
    python3 Tools/audit_ui/audit_wbp.py --receipt <path>

R26 says it plainly: *"Enforcement is owed, not assumed. An audit must assert conditions
1-3 mechanically (node count, added-member count, parent chain) over every asset and run in
rung 2, or condition 2 erodes to 'only a little logic' within a month."* `audit_r26.py`
covers `BP_BR*` assets; a `.uasset` with a WBP in front of it is the same hazard with a
different extension, and the WBP count is about to go up ~20x.

**Every call is READ-ONLY.** `GetWidgets`, `get_parent`, `list_variables`, `list_graphs`,
`read_graph_dsl`, `search_subclasses` — nothing here mutates, saves, compiles, or opens an
asset, so it is safe to run against an editor somebody else is working in.

Exit: 0 clean · 1 `high` findings (law 8: only `high` blocks a landing) · 3 blocked.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import wbp_expect as E  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
URL = "http://127.0.0.1:8000/mcp"

UMG = "UMGToolSet.UMGToolSet"
BP = "editor_toolset.toolsets.blueprint.BlueprintTools"
OBJ = "editor_toolset.toolsets.object.ObjectTools"

EXIT_PASS, EXIT_FAIL, EXIT_BLOCKED = 0, 1, 3


# --------------------------------------------------------------------------- transport
class MCP:
    """Copied from Tools/gen_ui/import_textures.py — same server, same session dance."""

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
            "clientInfo": {"name": "breachpoint-audit-wbp", "version": "1"}}})
        try:
            self._post({"jsonrpc": "2.0", "method": "notifications/initialized"})
        except Exception:
            pass

    def call(self, toolset, tool, args):
        """-> (returnValue | None, raw text). None means the call FAILED."""
        self.n += 1
        try:
            r = self._post({"jsonrpc": "2.0", "id": 300 + self.n, "method": "tools/call",
                            "params": {"name": "call_tool", "arguments": {
                                "toolset_name": toolset, "tool_name": tool,
                                "arguments": args}}})
        except Exception as e:
            return None, f"transport error: {e}"
        res = (r or {}).get("result", {})
        txt = "\n".join(c["text"] for c in res.get("content", []) if c.get("type") == "text")
        if "error" in (r or {}):
            return None, json.dumps((r or {})["error"])
        if res.get("isError"):
            return None, txt
        try:
            parsed = json.loads(txt)
        except Exception:
            return None, txt or "(empty response)"
        if "returnValue" not in parsed:
            return None, txt
        return parsed["returnValue"], txt


# --------------------------------------------------------------------------- receipt
class Receipt:
    """R37: UTC header, verbatim command, plan digest, one line PER CALL, failures marked."""

    def __init__(self, path: Path, argv: str, digest: str, n_assets: int):
        path.parent.mkdir(parents=True, exist_ok=True)
        self.f = path.open("w", buffering=1)      # line buffered — a run that dies leaves a trail
        self.path = path
        self.findings: list[tuple[str, str]] = []
        self.calls = self.failed = 0
        self.w(f"# RECEIPT — WBP R26 audit · {datetime.now(timezone.utc).isoformat()}")
        self.w("")
        self.w(f"**Command:** `{argv}`")
        self.w(f"**Expectation:** `Tools/audit_ui/wbp_expect.py` sha256 `{digest}` "
               f"— derived from `Content/**/WBP_*.uasset` + the headers, not hardcoded")
        self.w(f"**Assets in scope:** {n_assets}")
        self.w("**Transport:** raw HTTP JSON-RPC `http://127.0.0.1:8000/mcp`, `call_tool` "
               "dispatcher. **Every call below is read-only** — nothing was mutated or saved.")
        self.w("")

    def w(self, s=""):
        print(s)
        self.f.write(s + "\n")

    def call(self, label, ok, detail):
        self.calls += 1
        if not ok:
            self.failed += 1
        self.w(f"- {'PASS' if ok else '**FAILED**'} `{label}` — {detail}")

    def close(self, verdict, rung_note):
        order = {"high": 0, "medium": 1, "low": 2, "info": 3}
        self.w("")
        self.w(f"## Findings ({self.failed} of {self.calls} calls failed)")
        if self.findings:
            for sev, msg in sorted(self.findings, key=lambda x: order.get(x[0], 9)):
                self.w(f"- **{sev}** — {msg}")
        else:
            self.w("- none")
        self.w("")
        self.w("## Verdict")
        self.w(verdict)
        self.w("")
        self.w("## Rung honesty — what this PASS does not mean")
        self.w(rung_note)
        self.f.close()


# --------------------------------------------------------------------------- observation
class ClassResolver:
    """Class compatibility answered by the ENGINE's reflection, not a hardcoded base table.

    A hand-maintained `{class: [bases]}` map (which `Tools/gen_ui/wbp_plan.py` must keep,
    because it runs with no engine) falls back to exact match on any class nobody listed —
    turning "UBorder satisfies UPanelWidget" into a confident, wrong finding the first time
    somebody uses a widget class the table never heard of. This runs inside a live editor,
    so it asks the editor. Answers are cached; a repeat costs nothing.
    """

    def __init__(self, m: MCP, rc: Receipt):
        self.m, self.rc = m, rc
        self._resolved: dict[str, str | None] = {}
        self._compat: dict[tuple[str, str], bool | None] = {}
        self._ancestor: dict[str, str | None] = {}

    def _resolve(self, leaf: str) -> str | None:
        """`UPanelWidget` -> `/Script/UMG.PanelWidget`. Every UMG widget derives from UWidget."""
        if leaf in self._resolved:
            return self._resolved[leaf]
        bare = leaf[1:] if leaf.startswith("U") else leaf
        hits, txt = self.m.call(OBJ, "search_subclasses", {
            "base_class": {"refPath": "/Script/UMG.Widget"}, "class_name": bare})
        # search_subclasses matches on SUBSTRING ("TextBlock" returns six classes), so the
        # exact leaf is picked out here rather than trusting the first hit.
        out = None
        for h in hits or []:
            rp = h.get("refPath", "") if isinstance(h, dict) else str(h)
            if rp.rsplit(".", 1)[-1] == bare:
                out = rp
                break
        self.rc.call(f"search_subclasses resolve {leaf}", out is not None,
                     out or f"unresolved: {txt[:120]}")
        self._resolved[leaf] = out
        return out

    def _cpp_ancestor(self, refpath: str) -> str | None:
        """A Blueprint ASSET path -> the `/Script/` class it derives from, per the editor.

        THE FALSE-`high` BUG LIVED HERE. A widget tree reports a hosted child by its ASSET
        path (`/Game/UI/Components/WBP_ProgressBar.WBP_ProgressBar`) while `search_subclasses`
        answers in CLASS paths (`....WBP_ProgressBar_C`). Comparing the two is a guaranteed
        miss, and it produced three `high` findings against assets the editor itself compiles
        happily — exactly the false red that teaches people to switch a gate off (law 8: only
        `high` blocks a landing, so a lying `high` is worse than no gate).

        Pasting `_C` on would silence it and prove nothing. `_C` is the naming convention for
        a generated class; it is not evidence about what the asset DERIVES FROM, so a string
        rewrite would make `WBP_Killfeed` "satisfy" a `UBRProgressBar` bind the moment the
        subclass query's substring filter got lucky. So this RESOLVES rather than rewrites:
        `BlueprintTools.get_parent` — the same read-only call the audit already makes for R26
        condition 1 — is asked what this Blueprint's parent actually is, and the walk repeats
        while the answer is still a Blueprint, so a WBP parented to another WBP still lands on
        the C++ class at the top of the chain. What comes back is a real class path, and it
        goes into the same `search_subclasses` membership test as any native widget class —
        the engine's reflection stays the judge of compatibility, which was the whole point of
        this class (see the docstring above).
        """
        if refpath in self._ancestor:
            return self._ancestor[refpath]
        cur, seen = refpath, set()
        while cur and not cur.startswith("/Script/"):
            if cur in seen:          # UE cannot make a parent cycle; cheaper to check than trust
                cur = None
                break
            seen.add(cur)
            par, txt = self.m.call(BP, "get_parent", {"blueprint": {"refPath": cur}})
            nxt = _ref(par)
            self.rc.call(f"get_parent {cur.rsplit('/', 1)[-1]}", nxt is not None,
                         nxt or f"unresolved: {txt[:120]} — compatibility left UNVERIFIED")
            cur = nxt
        self._ancestor[refpath] = cur
        return cur

    def compatible(self, declared_leaf: str, actual_refpath: str) -> bool | None:
        key = (declared_leaf, actual_refpath)
        if key in self._compat:
            return self._compat[key]
        base = self._resolve(declared_leaf)
        # A `/Game/` path in the tree is an ASSET; resolve it to the class it derives from
        # before asking a question about classes. See `_cpp_ancestor`.
        actual = actual_refpath or ""
        if not actual.startswith("/Script/"):
            actual = self._cpp_ancestor(actual)
        if base is None or not actual:
            self._compat[key] = None
            return None
        via = "" if actual == actual_refpath else f" (`{actual_refpath}` derives from it)"
        actual_leaf = actual.rsplit(".", 1)[-1]
        hits, txt = self.m.call(OBJ, "search_subclasses", {
            "base_class": {"refPath": base}, "class_name": actual_leaf})
        if hits is None:
            self.rc.call(f"search_subclasses {actual_leaf} under {base}", False, txt[:150])
            self._compat[key] = None
            return None
        got = {h.get("refPath") if isinstance(h, dict) else h for h in hits}
        # `actual == base` is the exact-type case. The server does return the base among its
        # own subclasses, so the membership test alone would cover it — but a class being
        # compatible with itself should not depend on that courtesy.
        ok = actual == base or actual in got
        self.rc.call(f"search_subclasses {actual_leaf} under {base}", True,
                     f"{'is' if ok else 'is NOT'} a subclass{via}")
        self._compat[key] = ok
        return ok


def _ref(x) -> str | None:
    if isinstance(x, dict):
        return x.get("refPath")
    return None if x in (None, "None") else str(x)


def observe(m: MCP, rc: Receipt, exp: dict) -> dict | None:
    """Every read the judgement needs, one asset. None = the asset could not be read at all."""
    ref = {"refPath": exp["game_path"]}
    obs: dict = {"parent": None, "widgets": [], "variables": [], "graphs": {}}

    got, txt = m.call(UMG, "GetWidgets", {"widgetBlueprint": ref})
    if not isinstance(got, dict):
        rc.call("UMGToolSet.GetWidgets", False, f"{exp['game_path']} — {txt[:200]}")
        rc.findings.append(("high", f"{exp['asset']}: could not be read over the MCP "
                                    f"({txt[:120]}) — UNAUDITED, not clean"))
        return None
    info = got.get("info", {})
    obs["parent"] = _ref(info.get("parentClass"))
    obs["widgets"] = [{"name": w.get("widgetName"),
                       "class": _ref(w.get("widgetClassPath")),
                       "isVariable": bool(w.get("bIsVariable")),
                       "inherited": bool(w.get("bInherited"))}
                      for w in got.get("widgets", [])]
    rc.call("UMGToolSet.GetWidgets", True,
            f"parent `{obs['parent']}` · {info.get('widgetCount')} widget(s) "
            f"({info.get('inheritedWidgetCount')} inherited) · "
            f"tree {[w['name'] for w in obs['widgets']]}")

    # get_parent is a SECOND, independent read of condition 1 — GetWidgets' info block and
    # BlueprintTools disagreeing would itself be worth knowing.
    par, txt = m.call(BP, "get_parent", {"blueprint": ref})
    par_ref = _ref(par)
    rc.call("BlueprintTools.get_parent", par_ref is not None, par_ref or txt[:150])
    if par_ref and par_ref != obs["parent"]:
        rc.findings.append(("medium", f"{exp['asset']}: GetWidgets says parent `{obs['parent']}`, "
                                      f"get_parent says `{par_ref}` — the two reads disagree"))

    vs, txt = m.call(BP, "list_variables", {"blueprint": ref})
    if vs is None:
        rc.call("BlueprintTools.list_variables", False, txt[:150])
        rc.findings.append(("high", f"{exp['asset']}: list_variables failed — R26 cond. 3 "
                                    f"is UNVERIFIED for this asset"))
    else:
        names = [v.get("name", str(v)) if isinstance(v, dict) else str(v) for v in vs]
        obs["variables"] = names
        rc.call("BlueprintTools.list_variables", True,
                "empty (R26 cond. 3 satisfied)" if not names else f"**{len(names)}**: {names}")

    gs, txt = m.call(BP, "list_graphs", {"blueprint": ref})
    if gs is None:
        rc.call("BlueprintTools.list_graphs", False, txt[:150])
        rc.findings.append(("high", f"{exp['asset']}: list_graphs failed — R26 cond. 2 "
                                    f"is UNVERIFIED for this asset"))
        return obs
    graph_refs = [_ref(g) for g in gs]
    rc.call("BlueprintTools.list_graphs", True,
            f"{len(graph_refs)} graph(s): {[str(g).rsplit(':', 1)[-1] for g in graph_refs]}")

    for g in graph_refs:
        name = str(g).rsplit(":", 1)[-1]
        dsl, txt = m.call(BP, "read_graph_dsl", {"graph": {"refPath": g}})
        if not isinstance(dsl, str):
            rc.call(f"BlueprintTools.read_graph_dsl {name}", False, txt[:150])
            rc.findings.append(("high", f"{exp['asset']}: {name} could not be read as DSL — "
                                        f"R26 cond. 2 UNVERIFIED"))
            continue
        obs["graphs"][name] = dsl
        r = E.graph_nodes(dsl)
        rc.call(f"BlueprintTools.read_graph_dsl {name}", True,
                f"{r['nodes']} node(s) beyond the engine's default event stubs · "
                f"DSL {json.dumps(dsl)[:200]}")
    return obs


# --------------------------------------------------------------------------- main
def run_selftest() -> int:
    st = Path(__file__).resolve().parent / "selftest_no_editor.py"
    return subprocess.run([sys.executable, str(st)]).returncode


RUNG_NOTE = (
    "- **Not a rung on its own.** This is a static read of three asset properties. It proves "
    "no graph node, no Blueprint variable and the right parent — it proves NOTHING about "
    "whether the UI works, renders, or is laid out correctly.\n"
    "- **A clean audit is not a compiled WBP.** It does not open, compile or cook the asset. "
    "A `BindWidget` mismatch normally surfaces at asset LOAD; that check is reproduced here "
    "from the header, but a passing audit is not a substitute for the engine compiling it.\n"
    "- **Not multiplayer, not PIE** (law 6). No process was launched.\n"
    "- **Node count is DSL forms, not `UEdGraphNode`s.** A node with no DSL representation — "
    "a comment box, a reroute — is invisible to `read_graph_dsl`. Zero-vs-nonzero is exact; "
    "the absolute number is a proxy.\n"
    "- **The three default event stubs are allowed while EMPTY.** UMG stamps "
    "`EventPreConstruct` / `EventConstruct` / `EventTick` into every new WBP and the compiler "
    "drops them unconnected. One statement inside any of them is a `high` finding.\n"
    "- **R26 conditions 4 and 5 are not mechanised here.** \"No gameplay NUMBER\" needs a "
    "human reading defaults against `Content/Data/*.csv`; the naming law is only checked "
    "insofar as a WBP whose `UBR` class does not exist is reported.\n"
    "- **Scope is what is on disk.** An asset nobody committed is an asset this never saw.")


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--asset", help="audit one WBP by name (e.g. WBP_RootLayout)")
    ap.add_argument("--selftest", action="store_true", help="prove the logic, touch no editor")
    ap.add_argument("--receipt", help="receipt path (default docs/ui/receipts/audit-wbp-*.md)")
    args = ap.parse_args(argv)

    if args.selftest:
        return run_selftest()

    assets = E.discover()
    if args.asset:
        assets = [a for a in assets if a["asset"] == args.asset]
        if not assets:
            print(f"no such WBP on disk: {args.asset}")
            return EXIT_BLOCKED
    if not assets:
        print("no WBP_*.uasset found under Content/ — nothing to audit")
        return EXIT_BLOCKED

    digest = hashlib.sha256((Path(__file__).parent / "wbp_expect.py").read_bytes()).hexdigest()[:16]
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    rpath = Path(args.receipt) if args.receipt else \
        REPO / "docs/ui/receipts" / f"audit-wbp-{stamp}.md"
    rc = Receipt(rpath, " ".join(sys.argv), digest, len(assets))

    m = MCP()
    try:
        m.init()
    except Exception as e:
        rc.w(f"**BLOCKED** — no MCP server at {URL}: {e}")
        rc.close("BLOCKED — the editor is not running, or its MCP server is off. "
                 "**Nothing was audited. This is not a PASS.**",
                 "- Nothing ran. No rung, no claim.")
        return EXIT_BLOCKED

    resolver = ClassResolver(m, rc)
    for exp in assets:
        rc.w("")
        rc.w(f"## `{exp['asset']}` → `{exp['game_path']}`")
        rc.w("")
        rc.w(f"expects parent `{exp['expected_parent']}` · header "
             f"`{exp['header'] or '**none found**'}` · binds "
             f"{ {k: v['class'] + ('?' if v['optional'] else '') for k, v in exp['binds'].items()} }")
        rc.w("")
        obs = observe(m, rc, exp)
        if obs is None:
            continue
        found = E.judge(exp, obs, resolver.compatible)
        rc.findings.extend(found)
        rc.w("")
        rc.w("verdict: " + ("**CLEAN**" if not [f for f in found if f[0] != "info"]
                            else ", ".join(f"{s}×{sum(1 for x in found if x[0] == s)}"
                                           for s in dict.fromkeys(s for s, _ in found))))

    highs = [f for f in rc.findings if f[0] == "high"]
    ok = not highs and rc.failed == 0
    rc.close(
        (f"**PASS** — {len(assets)} Widget Blueprint(s) audited, {rc.calls} read-only calls, "
         f"zero failures, zero `high` findings. R26 conditions 1-3 hold for every asset on "
         f"disk." if ok else
         f"**FAIL** — {len(highs)} `high` finding(s) and {rc.failed} failed call(s) across "
         f"{len(assets)} asset(s). Law 8: only `high` blocks a landing; the rest belong in "
         f"the risk register."),
        RUNG_NOTE)
    print(f"\nreceipt: {rpath}")
    return EXIT_PASS if ok else EXIT_FAIL


if __name__ == "__main__":
    raise SystemExit(main())
