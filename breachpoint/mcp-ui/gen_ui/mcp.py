#!/usr/bin/env python3
"""Shared transport for every generator that drives the live UE editor over MCP.

WHY THIS FILE EXISTS. `build_materials.py` used to open with

    from build_wbp import MCP, Receipt as _WbpReceipt      # transport, verbatim

which is inverted layering: the MATERIALS driver imported the WIDGET driver to obtain an HTTP
client. Nothing was duplicated -- that part was always right -- but `build_materials` could not
run if `build_wbp` broke, and a third driver would have had to import a widget file for the same
reason. Shared infrastructure does not live inside one of its consumers.

WHAT MOVED, AND WHAT DELIBERATELY DID NOT. The transport, the receipt, the verified write and the
comparison are here. `build_one`, `verify_one`, `select` and every toolset-specific call stay in
their own driver -- this module knows nothing about widgets, materials, trees or plans, and the
day it does is the day it has stopped being transport.

Three things learned the expensive way, encoded here rather than in a comment nobody reads:

1. `CreateWidgetBlueprint`'s clobber guard is `FindPackage` -- MEMORY, not disk. In a fresh
   editor where the asset is unloaded it takes the create path against an existing on-disk
   asset. Regeneration therefore DELETES first, explicitly, and asserts the delete.
2. Object refs need the full `/Game/X/Y.Y` form. The short form makes the server drop the
   whole argument object and report "input params Json is empty", which names the wrong cause.
3. Property names are camelCase (`layoutData`, `font`, `brush`), not C++ PascalCase, and
   `set_properties` takes `values` as a JSON *string*. A wrong name fails silently -- which is
   why every write goes through `write_verified` and is read back and compared.
"""
from __future__ import annotations

import json, urllib.request
from datetime import datetime, timezone
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
URL = "http://127.0.0.1:8000/mcp"

UMG = "UMGToolSet.UMGToolSet"
OBJ = "editor_toolset.toolsets.object.ObjectTools"
ASSET = "editor_toolset.toolsets.asset.AssetTools"

EXIT_PASS, EXIT_FAIL, EXIT_BLOCKED = 0, 1, 3


MAT = "editor_toolset.toolsets.material.MaterialTools"       # UNVERIFIED toolset path


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
        self.w(f"**Plan:** `mcp-ui/gen_ui/wbp_plan.py` sha256 `{plan_digest}`")
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



def close(a, b, *, loose_enums: bool = False) -> bool:
    """Did the read-back match what was written? The one comparison every driver shares.

    `loose_enums` IS NOT A STYLE KNOB -- it is the reason this function was two functions.
    `build_wbp` and `build_materials` each grew their own `_close` and the two DIVERGED, which
    is exactly what a copied helper does:

      * widgets need OBJECT-REF tolerance. UE reads a ref back in any of three shapes --
        `{"refPath": ...}`, a bare `/Game/...` path, or the export form
        `/Script/Engine.Font'/Game/....F_X'` -- so the test is "does the asset path appear in
        what came back". Without it a CORRECT font write is reported as a failure because the
        server spelled the same asset a different way.
      * materials need ENUM tolerance. A material enum reads back namespaced: `MD_UI` against
        `EMaterialDomain::MD_UI` is agreement, not drift, and failing it turns every run red.

    Merging them by picking one would have broken the other, and merging by taking the union
    would have loosened WIDGET string comparison to substring matching -- under which writing
    `"Fill"` and reading back `"HAlign_Fill"` PASSES. So the union is opt-in, off by default,
    and the strict path is what widgets keep.
    """
    if isinstance(b, dict) and set(b) == {"refPath"}:
        got = a.get("refPath") if isinstance(a, dict) else a
        return isinstance(got, str) and b["refPath"].split(".")[0] in got
    if isinstance(b, dict):
        return isinstance(a, dict) and all(close(a.get(k), v, loose_enums=loose_enums)
                                           for k, v in b.items())
    if isinstance(b, float):
        return isinstance(a, (int, float)) and abs(a - b) < 1e-4
    if loose_enums and isinstance(b, str) and isinstance(a, str):
        return a == b or b in a or a in b
    return a == b

def write_verified(m: MCP, rc: Receipt, label: str, instance, values: dict,
                   *, loose_enums: bool = False) -> bool:
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
    ok = readback is not None and all(close(readback.get(k), v, loose_enums=loose_enums)
                                      for k, v in values.items())
    rc.call(label, ok, f"wrote {json.dumps(values)}; read {json.dumps(readback)[:180]}")
    return ok


