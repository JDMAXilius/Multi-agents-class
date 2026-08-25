"""DT_BNBotAmbitions — create the table, mirror UBNBotBrain::DefaultRow, read it back.

    python Tools/bn/60_dt_bot_ambitions.py apply    # create + converge + save
    python Tools/bn/60_dt_bot_ambitions.py audit    # read-back diff, intent vs actual

Ticket: docs/archive/BREACHPOINT-NEXT-TASK-R6-DT-AMBITIONS.md

Unlike the other Tools/bn scripts this one does NOT import `unreal` and is not run with
the editor's `py` command. It drives a RUNNING editor over the Unreal MCP server's raw
HTTP JSON-RPC endpoint (tool-search mode: list_toolsets / describe_toolset / call_tool).
Stdlib only — no jq, no requests, no unreal module.

IDEMPOTENT by construction: `apply` skips create when the asset exists, adds only the
row names that are missing, and always writes the full intent over every row. Re-running
on a fresh pull or a dirty editor converges to the same state.

THE NUMBERS ARE NOT TUNING. Every value below is a verbatim mirror of the C++ fallback in
Source/BreachpointNext/AI/BNBotBrain.cpp::UBNBotBrain::DefaultRow (lines 9-31), so that the
table the founder now tunes starts at exactly the shipped behavior. Fields the C++ leaves
at the struct default (Source/BreachpointNext/Data/BNDataRows.h FBNBotAmbitionRow) are
written explicitly here, because a DataTable row must carry every column.
Row names are the case-exact literals from UBNBotBrain::AmbitionRowName (lines 40-42).
"""

import json
import sys
import urllib.request

URL = "http://127.0.0.1:8000/mcp"
PROTOCOL_VERSION = "2025-06-18"

ASSET_FOLDER = "/Game/BN/Data"
ASSET_NAME = "DT_BNBotAmbitions"
ASSET_PATH = f"{ASSET_FOLDER}/{ASSET_NAME}"
# AssetTools takes the PACKAGE path above; a {"refPath": ...} object reference needs the
# full OBJECT path (package.object) or the server answers "not a valid object path".
OBJECT_PATH = f"{ASSET_PATH}.{ASSET_NAME}"
ROW_STRUCT = "/Script/BreachpointNext.BNBotAmbitionRow"

DT_TOOLSET = "editor_toolset.toolsets.data_table.DataTableTools"
ASSET_TOOLSET = "editor_toolset.toolsets.asset.AssetTools"

# row name -> camelCase property -> value.  Mirrors BNBotBrain.cpp DefaultRow exactly.
INTENT = {
    # BNBotBrain.cpp:10-14
    "Fight": {
        "baseUtility": 1.0,
        "healthWeight": 0.0,
        "targetWeight": 1.0,
        "distanceWeight": 0.0,
        "commitSeconds": 3.0,
        "interruptBelowHealthNorm": 0.0,  # struct default, BNDataRows.h
    },
    # BNBotBrain.cpp:17-22
    "Survive": {
        "baseUtility": 1.2,
        "healthWeight": 1.0,
        "targetWeight": 0.0,
        "distanceWeight": 0.0,
        "commitSeconds": 5.0,
        "interruptBelowHealthNorm": 0.35,
    },
    # BNBotBrain.cpp:26-30
    "Roam": {
        "baseUtility": 0.2,
        "healthWeight": 0.0,
        "targetWeight": 0.0,
        "distanceWeight": 0.0,
        "commitSeconds": 2.0,
        "interruptBelowHealthNorm": 0.0,  # struct default, BNDataRows.h
    },
}
ROW_ORDER = ["Fight", "Survive", "Roam"]
COLUMNS = [
    "baseUtility",
    "healthWeight",
    "targetWeight",
    "distanceWeight",
    "commitSeconds",
    "interruptBelowHealthNorm",
]


# --- MCP transport -------------------------------------------------------------

def _post(payload, session=None):
    body = json.dumps(payload).encode("utf-8")
    headers = {
        "Content-Type": "application/json",
        "Accept": "application/json, text/event-stream",
    }
    if session:
        headers["Mcp-Session-Id"] = session
    request = urllib.request.Request(URL, data=body, headers=headers, method="POST")
    with urllib.request.urlopen(request, timeout=60) as response:
        return response.headers, response.read().decode("utf-8")


def _new_session():
    """Sessions expire fast, so every call opens its own."""
    headers, _ = _post({
        "jsonrpc": "2.0", "id": 1, "method": "initialize",
        "params": {
            "protocolVersion": PROTOCOL_VERSION,
            "capabilities": {},
            "clientInfo": {"name": "bn-editor", "version": "1"},
        },
    })
    session = headers.get("Mcp-Session-Id") or headers.get("mcp-session-id")
    _post({"jsonrpc": "2.0", "method": "notifications/initialized"}, session)
    return session


def call(toolset, tool, arguments):
    """call_tool through the tool-search facade; returns the parsed .returnValue."""
    session = _new_session()
    _, raw = _post({
        "jsonrpc": "2.0", "id": 2, "method": "tools/call",
        "params": {
            "name": "call_tool",
            "arguments": {
                "toolset_name": toolset,
                "tool_name": tool,
                "arguments": arguments,
            },
        },
    }, session)
    envelope = json.loads(raw)
    if "error" in envelope:
        raise RuntimeError(f"{toolset}.{tool} -> {envelope['error']}")
    result = envelope.get("result", {})
    content = result.get("content") or []
    text = content[0].get("text", "") if content else ""
    if result.get("isError") or not text.strip():
        # A tool that fails hands back prose, not JSON. Surface it rather than
        # dying on a decode error three frames away from the cause.
        raise RuntimeError(f"{toolset}.{tool} -> {raw}")
    try:
        payload = json.loads(text)
    except json.JSONDecodeError:
        raise RuntimeError(f"{toolset}.{tool} returned non-JSON: {text!r}")
    if isinstance(payload, dict) and "returnValue" in payload:
        return payload["returnValue"]
    return payload


def ref(path):
    """Object/class references cross the wire as {"refPath": ...}, never bare strings."""
    return {"refPath": path}


# --- steps ---------------------------------------------------------------------

def ensure_asset():
    if call(ASSET_TOOLSET, "exists", {"path": ASSET_PATH}):
        print(f"[exists] {ASSET_PATH} - not re-creating")
        return ref(OBJECT_PATH)
    structs = call(DT_TOOLSET, "search_row_structs", {"struct_name": "*BNBotAmbition*"})
    if not any(s.get("refPath") == ROW_STRUCT for s in structs):
        raise SystemExit(
            f"STOP: {ROW_STRUCT} is not in the editor's row-struct picker. "
            "The running build predates R6 — hand back, do not improvise a substitute."
        )
    created = call(DT_TOOLSET, "create", {
        "folder_path": ASSET_FOLDER,
        "asset_name": ASSET_NAME,
        "schema": ref(ROW_STRUCT),
    })
    print(f"[create] {created}")
    return ref(OBJECT_PATH)


def converge_rows(table):
    present = call(DT_TOOLSET, "list_rows", {"data_table": table})
    missing = [name for name in ROW_ORDER if name not in present]
    if missing:
        call(DT_TOOLSET, "add_rows", {"data_table": table, "row_names": missing})
        print(f"[add_rows] {missing}")
    else:
        print("[add_rows] all three rows already present")
    # Always rewrite the full intent: this is what makes a dirty editor converge.
    call(DT_TOOLSET, "set_rows", {
        "data_table": table,
        "values": json.dumps(INTENT),
    })
    print("[set_rows] full intent written over all three rows")
    extra = [name for name in present if name not in ROW_ORDER]
    if extra:
        print(f"[REPORT] unexpected extra rows present, NOT removed: {extra}")


def save(table):
    call(ASSET_TOOLSET, "save_assets", {"asset_paths": [ASSET_PATH]})
    dirty = call(ASSET_TOOLSET, "is_dirty", {"asset_path": ASSET_PATH})
    print(f"[save] saved; is_dirty now {dirty}")


def audit():
    print(f"asset      : {ASSET_PATH}")
    print(f"exists     : {call(ASSET_TOOLSET, 'exists', {'path': ASSET_PATH})}")
    print(f"asset class: {call(ASSET_TOOLSET, 'get_asset_class', {'asset_path': ASSET_PATH})}")
    print(f"is_dirty   : {call(ASSET_TOOLSET, 'is_dirty', {'asset_path': ASSET_PATH})}")
    table = ref(OBJECT_PATH)
    print(f"row names  : {call(DT_TOOLSET, 'list_rows', {'data_table': table})}")
    print(f"schema     : {call(DT_TOOLSET, 'get_schema', {'data_table': table})}")

    raw = call(DT_TOOLSET, "get_rows", {"data_table": table, "row_names": ROW_ORDER})
    print("\n--- get_rows VERBATIM -------------------------------------------------")
    print(raw)
    print("--- end verbatim ------------------------------------------------------\n")

    actual = json.loads(raw) if isinstance(raw, str) else raw
    header = f"{'row':<9} {'column':<26} {'intent':>8} {'actual':>10}  ok"
    print(header)
    print("-" * len(header))
    failures = 0
    for row in ROW_ORDER:
        got = actual.get(row, {})
        for column in COLUMNS:
            want = INTENT[row][column]
            have = got.get(column, "<MISSING>")
            ok = isinstance(have, (int, float)) and abs(float(have) - want) < 1e-6
            failures += 0 if ok else 1
            print(f"{row:<9} {column:<26} {want:>8} {str(have):>10}  {'OK' if ok else 'MISMATCH'}")
    print("-" * len(header))
    print("AUDIT PASS — table matches UBNBotBrain::DefaultRow" if not failures
          else f"AUDIT FAIL — {failures} mismatch(es)")
    return 0 if not failures else 1


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "apply"
    if mode == "apply":
        table = ensure_asset()
        converge_rows(table)
        save(table)
        return 0
    if mode == "audit":
        return audit()
    raise SystemExit(f"unknown mode {mode!r}; use 'apply' or 'audit'")


if __name__ == "__main__":
    sys.exit(main())
