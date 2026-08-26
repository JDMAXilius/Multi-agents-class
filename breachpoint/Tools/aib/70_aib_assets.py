"""ST_AIBBot + DT_AIBTiers -- built by C++, driven and read back over Unreal MCP.

    python Tools/aib/70_aib_assets.py probe     # is the running build current? (node vocabulary)
    python Tools/aib/70_aib_assets.py build     # pull the trigger, then read back
    python Tools/aib/70_aib_assets.py audit     # read-back only, from a fresh load

The AIBot twin of Tools/bn/62_bot_assets.py, on the same proven mechanics and the same
R5 finding: a StateTree's GRAPH cannot be authored from Python or any MCP toolset (the
states are bare Instanced UPROPERTYs PropertyAccessUtil refuses; no toolset has a
StateTree factory; CompileStateTree is not a UFUNCTION). The graph is built by
UAIBTreeAuthoring (Source/AIBot/Execution/AIBTreeAuthoring.cpp) compiled into the
editor; this script proves the running build is current, pulls the trigger, and reads
the result back out of the live editor.

THE TRIGGER: no MCP tool calls a function -- setting a property IS reachable, and UE
routes an editor property write through PostEditChangeProperty, so UAIBAssetSettings
carries a Transient bool that behaves as a button.

THE PROBE LIST is derived from Source/AIBot/Execution/AIBStateTreeTasks.h and must be
checked BOTH directions on any vocabulary change (the 14-of-21 lesson from bn/62): every
node struct in the header appears here, and nothing here has left the header. Instance
data structs are deliberately excluded, as in bn/62 -- the stop condition is about node
types the tree places.

Stdlib only; drives the running editor over the MCP server's HTTP JSON-RPC endpoint.
"""

import json
import sys
import urllib.error
import urllib.request

URL = "http://127.0.0.1:8000/mcp"
PROTOCOL_VERSION = "2025-06-18"

ASSET_TOOLSET = "editor_toolset.toolsets.asset.AssetTools"
OBJECT_TOOLSET = "editor_toolset.toolsets.object.ObjectTools"
LOGS_TOOLSET = "EditorToolset.LogsToolset"

SETTINGS_CDO = "/Script/AIBot.Default__AIBAssetSettings"

TREE_OBJECT = "/Game/AIBot/AI/ST_AIBBot.ST_AIBBot"
TIERS_OBJECT = "/Game/AIBot/Data/DT_AIBTiers.DT_AIBTiers"

# Every node struct in AIBStateTreeTasks.h -- 6 conditions (one hidden base + five
# per-ambition gates) and 10 tasks (two hidden bases + the placed eight). The bases are
# probed too: the tree serializes derived types whose layout includes them, and a build
# missing a base is exactly as stale as one missing a leaf.
NODE_STRUCTS = [
    ("FAIBAmbitionGateCondition", "/Script/AIBot.AIBAmbitionGateCondition", "condition"),
    ("FAIBGateEngageCondition", "/Script/AIBot.AIBGateEngageCondition", "condition"),
    ("FAIBGateRetreatCondition", "/Script/AIBot.AIBGateRetreatCondition", "condition"),
    ("FAIBGateSearchCondition", "/Script/AIBot.AIBGateSearchCondition", "condition"),
    ("FAIBGateSeekWeaponCondition", "/Script/AIBot.AIBGateSeekWeaponCondition", "condition"),
    ("FAIBGateRoamCondition", "/Script/AIBot.AIBGateRoamCondition", "condition"),
    ("FAIBAmbitionSentinelTask", "/Script/AIBot.AIBAmbitionSentinelTask", "task"),
    ("FAIBFaceBeliefTask", "/Script/AIBot.AIBFaceBeliefTask", "task"),
    ("FAIBMoveNearBeliefTask", "/Script/AIBot.AIBMoveNearBeliefTask", "task"),
    ("FAIBFireWhenAbleTask", "/Script/AIBot.AIBFireWhenAbleTask", "task"),
    ("FAIBFleeFromBeliefTask", "/Script/AIBot.AIBFleeFromBeliefTask", "task"),
    ("FAIBMoveToLastKnownTask", "/Script/AIBot.AIBMoveToLastKnownTask", "task"),
    ("FAIBSweepLookTask", "/Script/AIBot.AIBSweepLookTask", "task"),
    ("FAIBMoveToPOITask", "/Script/AIBot.AIBMoveToPOITask", "task"),
    ("FAIBMoveToWeaponPOITask", "/Script/AIBot.AIBMoveToWeaponPOITask", "task"),
    ("FAIBWanderTask", "/Script/AIBot.AIBWanderTask", "task"),
]
SCRIPT_STRUCT = "/Script/CoreUObject.ScriptStruct"


# --- MCP transport ---------------------------------------------------------------

def _post(payload, session=None):
    body = json.dumps(payload).encode("utf-8")
    headers = {
        "Content-Type": "application/json",
        "Accept": "application/json, text/event-stream",
    }
    if session:
        headers["Mcp-Session-Id"] = session
    request = urllib.request.Request(URL, data=body, headers=headers, method="POST")
    with urllib.request.urlopen(request, timeout=180) as response:
        return response.headers, response.read().decode("utf-8")


def _new_session():
    """Sessions expire fast, so every call opens its own."""
    headers, _ = _post({
        "jsonrpc": "2.0", "id": 1, "method": "initialize",
        "params": {
            "protocolVersion": PROTOCOL_VERSION,
            "capabilities": {},
            "clientInfo": {"name": "aib-editor", "version": "1"},
        },
    })
    session = headers.get("Mcp-Session-Id") or headers.get("mcp-session-id")
    _post({"jsonrpc": "2.0", "method": "notifications/initialized"}, session)
    return session


def call(toolset, tool, arguments, allow_empty=False):
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
        raise RuntimeError("%s.%s -> %s" % (toolset, tool, envelope["error"]))
    result = envelope.get("result", {})
    content = result.get("content") or []
    text = content[0].get("text", "") if content else ""
    if result.get("isError"):
        raise RuntimeError("%s.%s -> %s" % (toolset, tool, text))
    if not text.strip():
        if allow_empty:
            return None
        raise RuntimeError("%s.%s -> empty response: %s" % (toolset, tool, raw))
    try:
        payload = json.loads(text)
    except json.JSONDecodeError:
        return text
    if isinstance(payload, dict) and "returnValue" in payload:
        return payload["returnValue"]
    return payload


def try_call(toolset, tool, arguments):
    """Same, but hands back the failure text instead of raising -- used by probes."""
    try:
        return True, call(toolset, tool, arguments, allow_empty=True)
    except Exception as error:  # noqa: BLE001 - a probe reports, it does not die
        return False, str(error)


def ref(path):
    """Object/class references cross the wire as {"refPath": ...}, never bare strings."""
    return {"refPath": path}


def require_editor():
    try:
        _new_session()
    except (urllib.error.URLError, OSError) as error:
        raise SystemExit(
            "STOP: no MCP server at %s (%s). The server lives INSIDE the editor -- open the "
            "project first. Autostart is already configured in "
            "Config/DefaultEditorPerProjectUserSettings.ini." % (URL, error))


# --- the stale-build stop condition ----------------------------------------------

def probe():
    print("--- build probe: every AIB node type must exist in the RUNNING editor ---")
    missing = []
    for name, path, kind in NODE_STRUCTS:
        ok, value = try_call(OBJECT_TOOLSET, "get_class", {"instance": ref(path)})
        got = value.get("refPath") if ok and isinstance(value, dict) else value
        good = ok and got == SCRIPT_STRUCT
        print("  %-32s %-10s -> %s  %s" % (name, kind, got, "OK" if good else "MISSING"))
        if not good:
            missing.append(name)

    ok, value = try_call(OBJECT_TOOLSET, "get_class", {"instance": ref(SETTINGS_CDO)})
    settings_ok = ok and isinstance(value, dict)
    print("  %-32s %-10s -> %s  %s" % ("UAIBAssetSettings (the trigger)", "settings",
                                       value.get("refPath") if settings_ok else value,
                                       "OK" if settings_ok else "MISSING"))
    if missing or not settings_ok:
        raise SystemExit(
            "STOP: the running editor is STALE -- missing "
            + ", ".join(missing + ([] if settings_ok else ["UAIBAssetSettings"]))
            + ".\nRebuild (Tools/run-ubt.ps1 -Targets BreachpointEditor) and RESTART the editor. "
              "Building the tree against a stale build writes node references the running "
              "editor cannot resolve.")
    print("  build probe PASS -- the editor is running current code\n")


# --- the trigger ------------------------------------------------------------------

def build():
    probe()
    print("--- pulling the trigger: UAIBAssetSettings.bRebuildBotAssets = true ---")
    ok, value = try_call(OBJECT_TOOLSET, "set_properties", {
        "instance": ref(SETTINGS_CDO),
        # 'values' is a JSON-formatted STRING, not an object -- from describe_toolset.
        "values": json.dumps({"bRebuildBotAssets": True}),
    })
    print("  set_properties -> %s %s" % ("OK" if ok else "FAILED", "" if ok else value))
    if not ok:
        raise SystemExit(
            "STOP: could not pull the trigger. The authoring itself is C++ and is unaffected -- "
            "run it from the editor's Project Settings (AI Bot > Authoring > Rebuild Bot "
            "Assets) and re-run this script with 'audit'.")

    print("\n--- what the editor logged (the C++ read-back) ---")
    ok, entries = try_call(LOGS_TOOLSET, "GetLogEntries", {"pattern": "LogAIBot"})
    if ok and entries:
        for entry in entries if isinstance(entries, list) else [entries]:
            print("  %s" % (entry.get("message") if isinstance(entry, dict) else entry))
    else:
        print("  (log read unavailable: %s) -- falling back to the asset audit below" % entries)

    print()
    audit()


# --- the read-back IS the deliverable ---------------------------------------------

def audit():
    print("--- read-back: the two assets, at the paths DefaultGame.ini names ---")
    for label, object_path in (("ST_AIBBot", TREE_OBJECT), ("DT_AIBTiers", TIERS_OBJECT)):
        ok, value = try_call(ASSET_TOOLSET, "exists", {"path": object_path})
        exists = bool(value) if ok else False
        print("  %-14s %-52s %s" % (label, object_path, "FOUND" if exists else "MISSING"))
        if not exists:
            continue
        ok, cls = try_call(OBJECT_TOOLSET, "get_class", {"instance": ref(object_path)})
        print("  %-14s class -> %s" % ("", cls.get("refPath") if isinstance(cls, dict) else cls))

    print("\n  The tree's STATES cannot be read through MCP (UStateTreeState is not property-"
          "\n  accessible -- see this file's header). UAIBTreeAuthoring::AuditBotAssets prints"
          "\n  them to LogAIBot from inside the editor; that is the graph's read-back of record.")


COMMANDS = {"probe": probe, "build": build, "audit": audit}

if __name__ == "__main__":
    command = sys.argv[1] if len(sys.argv) > 1 else "audit"
    if command not in COMMANDS:
        raise SystemExit("usage: 70_aib_assets.py [probe|build|audit]")
    require_editor()
    COMMANDS[command]()
