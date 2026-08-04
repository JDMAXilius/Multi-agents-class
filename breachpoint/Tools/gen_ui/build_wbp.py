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
        # encoding="utf-8" is NOT optional and NOT cosmetic. `Path.open` defaults to the
        # LOCALE encoding, which on a stock Windows box is cp1252 — and this receipt's very
        # first heading contains an em dash, with a `→` on every asset line after it. The run
        # therefore died with UnicodeEncodeError partway through the header on any machine
        # that had not set PYTHONUTF8, AFTER the plan had validated and the editor connection
        # was live. Latent because it depends on the host locale, not on anything in the plan.
        self.f = path.open("w", buffering=1, encoding="utf-8")   # line buffered, per R37
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


def write_verified(m: MCP, rc: Receipt, label: str, instance, values: dict) -> bool:
    """set_properties, then get_properties, then COMPARE. An unverified write is not a write.

    This is the one path every property write in this file takes — slot geometry, fonts and
    brushes alike. It is a function rather than three copies because of HOW this server
    fails: `set_properties` can report a refusal as TEXT in a successful result rather than
    as an error, so the only reliable evidence a property landed is reading it back. That
    read-back is what caught the `save_assets` parameter bug and a silently dropped font
    array; a call site that skips it is a call site that reports a false PASS.

    Known refusal worth naming: `set_properties` rejects a write that changes an ARRAY's size
    and its elements in one call, and drops the whole property while reporting success. No
    payload this file sends contains an array (see `wbp_plan.font_properties`); one that ever
    does must grow the array one entry per call.
    """
    m.call(OBJ, "set_properties", {"instance": instance, "values": json.dumps(values)})
    got, _ = m.call(OBJ, "get_properties",
                    {"instance": instance, "properties": list(values)})
    try:
        readback = json.loads(got) if isinstance(got, str) else got
    except Exception:
        readback = None
    ok = readback is not None and all(_close(readback.get(k), v) for k, v in values.items())
    rc.call(label, ok, f"wrote {json.dumps(values)}; read {json.dumps(readback)[:180]}")
    return ok


def _close(a, b):
    # An object reference. UE reads one back in any of three shapes — `{"refPath": ...}`, a
    # bare `/Game/...` path, or the export form `/Script/Engine.Font'/Game/....F_X'` — so the
    # comparison is "does the asset path appear in what came back". Looser than the rest of
    # this function on purpose: the alternative is a correct font write reported as a failure
    # because the server spelled the same asset a different way.
    if isinstance(b, dict) and set(b) == {"refPath"}:
        got = a.get("refPath") if isinstance(a, dict) else a
        return isinstance(got, str) and b["refPath"].split(".")[0] in got
    if isinstance(b, dict):
        return isinstance(a, dict) and all(_close(a.get(k), v) for k, v in b.items())
    if isinstance(b, float):
        return isinstance(a, (int, float)) and abs(a - b) < 1e-4
    return a == b


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


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dry-run", action="store_true", help="validate the plan, touch no editor")
    ap.add_argument("--asset", help="build one asset instead of all")
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
    print(f"plan OK: {len(wbp_plan.PLAN)} asset(s)")
    for note in wbp_plan.skipped_brushes():
        print("  NOTE (not an error): brush skipped —", note)

    todo = {args.asset: wbp_plan.PLAN[args.asset]} if args.asset else wbp_plan.PLAN
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
        "`Tools/audit_ui/audit_wbp.py` is what measures it, and it does not exist yet.")
    print(f"\nreceipt: {rc.path.relative_to(REPO)}")
    return EXIT_PASS if ok else EXIT_FAIL


if __name__ == "__main__":
    sys.exit(main())
