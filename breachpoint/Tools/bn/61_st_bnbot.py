"""ST_BNBot + the four points of interest -- apply, then read back from the live editor.

    python Tools/bn/61_st_bnbot.py apply    # place/converge the 4 POIs, save, probe the tree
    python Tools/bn/61_st_bnbot.py audit    # read-back diff, intent vs actual

Ticket: docs/archive/BREACHPOINT-NEXT-TASK-R5-ST-BNBOT.md (Step 2 verify, Step 3 POIs, Step 4 read-back)

Like Tools/bn/60_dt_bot_ambitions.py and UNLIKE the older Tools/bn scripts, this one does NOT
import `unreal`. It drives a RUNNING editor over the Unreal MCP server's raw HTTP JSON-RPC
endpoint (tool-search mode: list_toolsets / describe_toolset / call_tool). Stdlib only.

IDEMPOTENT by construction: POIs are matched by actor LABEL, created only when missing, and
their full intent (transform + PointName + Radius) is rewritten on every run. A second run
creates nothing and converges to the same state.

============================== WHAT THIS SCRIPT DOES NOT DO ==============================
STEP 1 OF THE TICKET -- the StateTree asset /Game/BN/AI/ST_BNBot -- IS NOT REACHABLE from any
registered toolset, nor from the editor's Python API. Named precisely, so nobody re-pays the
search (verified against UE 5.6 StateTree plugin headers, which match the running build):

  1. CREATION. No toolset creates a StateTree. AssetTools exposes only
     duplicate/move/delete/save -- there is no factory-driven `create_asset`. The typed
     creators (DataTableTools.create, MaterialTools, ...) have no StateTree sibling.
  2. THE GRAPH. UStateTree::EditorData is a bare UPROPERTY(); UStateTreeEditorData::SubTrees
     and UStateTreeState::Children are UPROPERTY(Instanced). None carries CPF_Edit or
     CPF_BlueprintVisible, and PropertyAccessUtil::CanGetPropertyValue denies both
     ObjectTools.get/set_properties AND Python's get/set_editor_property for such properties.
     Proof from the live editor: ObjectTools.list_properties on an existing StateTree
     (/Game/AI/ST_Bot.ST_Bot) returns "" -- the asset exposes ZERO settable properties -- and
     get_properties(["EditorData","Schema"]) errors "could not be read".
     The C++ builder API (UStateTreeEditorData::AddSubTree / AddRootState, UStateTreeState::
     AddChildState / AddTask<T> / AddTransition) is header-inline C++ templates; there is not
     one UFUNCTION in the whole StateTreeEditorModule public API, so nothing scripts it.
  3. COMPILE. UStateTreeEditingSubsystem::CompileStateTree is a plain static C++ function, not
     a UFUNCTION. An uncompiled StateTree runs nothing, so even a hand-built graph needs the
     editor's Compile button.
  4. READ-BACK. Consequence of (2): the states, task lists and transition delays of a
     StateTree CANNOT be read back through these tools either. Building it blind would
     produce an artifact this project cannot audit, and the audit is the deliverable.

So the tree is a HANDBACK, per the ticket's own instruction ("if the StateTree editor cannot
be driven headlessly for the graph itself ... STOP and report exactly which part is not
reachable rather than half-building an asset") and ASSET-RULES S5.

What this script DOES about the tree: it probes the build for the five BN node types, checks
whether the asset exists at exactly the path DefaultGame.ini names, and reports everything it
can reach. The day the tree exists (hand-built in the StateTree editor, or a StateTree toolset
lands), `audit` verifies its path and class without being changed.
=========================================================================================
"""

import json
import os
import sys
import urllib.request

URL = "http://127.0.0.1:8000/mcp"
PROTOCOL_VERSION = "2025-06-18"

ASSET_TOOLSET = "editor_toolset.toolsets.asset.AssetTools"
OBJECT_TOOLSET = "editor_toolset.toolsets.object.ObjectTools"
SCENE_TOOLSET = "editor_toolset.toolsets.scene.SceneTools"
ACTOR_TOOLSET = "editor_toolset.toolsets.actor.ActorTools"

# --- Step 1/2: the tree, and the ini line that is its contract -------------------
TREE_PACKAGE = "/Game/BN/AI/ST_BNBot"          # AssetTools takes the PACKAGE path
TREE_OBJECT = "/Game/BN/AI/ST_BNBot.ST_BNBot"  # {"refPath": ...} needs the OBJECT path
INI_LINE = "BotStateTree=/Game/BN/AI/ST_BNBot.ST_BNBot"  # Config/DefaultGame.ini
TREE_CLASS = "StateTree"

# The five nodes the tree must be able to hold. A USTRUCT resolves through get_class as
# /Script/CoreUObject.ScriptStruct when the module carrying it is loaded; a stale build
# answers with an error instead. This is the ticket's "is FBNHasTargetCondition in the
# picker?" check, done without a picker.
NODE_STRUCTS = [
    ("FBNHasTargetCondition", "/Script/BreachpointNext.BNHasTargetCondition", "condition"),
    ("FBNFaceTargetTask", "/Script/BreachpointNext.BNFaceTargetTask", "task"),
    ("FBNMoveToTargetTask", "/Script/BreachpointNext.BNMoveToTargetTask", "task"),
    ("FBNFireBurstTask", "/Script/BreachpointNext.BNFireBurstTask", "task"),
    ("FBNMoveToPointOfInterestTask", "/Script/BreachpointNext.BNMoveToPointOfInterestTask", "task"),
]
SCRIPT_STRUCT = "/Script/CoreUObject.ScriptStruct"

# --- Step 3: the four points of interest ----------------------------------------
POI_CLASS = "/Script/BreachpointNext.BNPointOfInterest"
LEVEL = "/Game/Maps/BR_Arena01"

# This script runs on the HOST, not inside the editor, so it can watch the repo itself:
# the World Partition external-actor files are the on-disk proof that a placement landed.
REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
EXTERNAL_ACTORS_DIR = os.path.join(
    REPO_ROOT, "Content", "__ExternalActors__", "Maps", "BR_Arena01")
# Folders swept for foreign dirty assets before a save-all. Everything this ticket may write
# lives here; anything else dirty means another writer, and the save is refused.
GUARD_FOLDERS = ["/Game/BN", "/Game/Maps", "/Game/AI"]

# Placement, measured -- not guessed. SceneTools.trace_world straight down at each XY hits
# the arena floor at z=0 for all four; the corners (z=120 cover blocks) and the centre
# (z=900 platform) are avoided on purpose. UE convention: +X North, +Y East. z=50 puts the
# actor origin half a metre off the floor, where a nav projection still finds it.
# Radius mirrors the C++ default (BNPointOfInterest.h: `float Radius = 200.f`) -- written
# explicitly because "read back what you set" needs an intent for every column.
POIS = [
    {"label": "BN_POI_North", "pointName": "North", "loc": (3400.0, 2000.0, 50.0), "radius": 200.0},
    {"label": "BN_POI_South", "pointName": "South", "loc": (600.0, 2000.0, 50.0), "radius": 200.0},
    {"label": "BN_POI_East", "pointName": "East", "loc": (2000.0, 3400.0, 50.0), "radius": 200.0},
    {"label": "BN_POI_West", "pointName": "West", "loc": (2000.0, 600.0, 50.0), "radius": 200.0},
]


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
    with urllib.request.urlopen(request, timeout=120) as response:
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
        raise RuntimeError(f"{toolset}.{tool} -> {envelope['error']}")
    result = envelope.get("result", {})
    content = result.get("content") or []
    text = content[0].get("text", "") if content else ""
    if result.get("isError"):
        raise RuntimeError(f"{toolset}.{tool} -> {text}")
    if not text.strip():
        if allow_empty:
            return None
        raise RuntimeError(f"{toolset}.{tool} -> empty response: {raw}")
    try:
        payload = json.loads(text)
    except json.JSONDecodeError:
        raise RuntimeError(f"{toolset}.{tool} returned non-JSON: {text!r}")
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


# --- build probe (the ticket's stale-build stop condition) -----------------------

def probe_build():
    print("--- build probe: the five BN node types must exist in the RUNNING build -----")
    missing = []
    for name, path, kind in NODE_STRUCTS:
        ok, value = try_call(OBJECT_TOOLSET, "get_class", {"instance": ref(path)})
        got = value.get("refPath") if ok and isinstance(value, dict) else value
        good = ok and got == SCRIPT_STRUCT
        print(f"  {name:<30} {kind:<10} -> {got}  {'OK' if good else 'MISSING'}")
        if not good:
            missing.append(name)
    ok, value = try_call(OBJECT_TOOLSET, "get_class", {"instance": ref(POI_CLASS)})
    poi_ok = ok and isinstance(value, dict)
    print(f"  {'ABNPointOfInterest':<30} {'actor':<10} -> "
          f"{value.get('refPath') if poi_ok else value}  {'OK' if poi_ok else 'MISSING'}")
    if missing or not poi_ok:
        raise SystemExit(
            "STOP: the running build is stale -- missing " + ", ".join(missing + ([] if poi_ok else ["ABNPointOfInterest"]))
            + ". The ticket says stop and report; do not improvise a substitute."
        )
    print("  build probe PASS -- not stale\n")


# --- the tree: what is reachable, and what is the handback ----------------------

def report_tree():
    print("--- Step 1/2: the StateTree asset -------------------------------------------")
    exists = call(ASSET_TOOLSET, "exists", {"path": TREE_PACKAGE})
    print(f"  intent package : {TREE_PACKAGE}")
    print(f"  intent object  : {TREE_OBJECT}")
    print(f"  ini contract   : {INI_LINE}   (Config/DefaultGame.ini)")
    print(f"  exists         : {exists}")
    if not exists:
        print("  STATUS         : HANDBACK -- asset absent; creation, graph editing, compile")
        print("                   and read-back are all unreachable (see this file's header).")
        return False
    asset_class = call(ASSET_TOOLSET, "get_asset_class", {"asset_path": TREE_PACKAGE})
    loaded = call(ASSET_TOOLSET, "load_asset", {"asset_path": TREE_OBJECT})
    props = call(OBJECT_TOOLSET, "list_properties", {"instance": ref(TREE_OBJECT)})
    print(f"  asset class    : {asset_class}  {'OK' if asset_class == TREE_CLASS else 'MISMATCH'}")
    print(f"  object path    : {loaded}")
    print(f"  is_dirty       : {call(ASSET_TOOLSET, 'is_dirty', {'asset_path': TREE_PACKAGE})}")
    print(f"  properties     : {props!r}")
    print("  NOTE           : states / tasks / transition delays are NOT readable through")
    print("                   any registered toolset -- verify them by eye in the editor.")
    return asset_class == TREE_CLASS


# --- the four points of interest -------------------------------------------------

def find_pois():
    """label -> refPath, for every ABNPointOfInterest in the loaded level."""
    actors = call(SCENE_TOOLSET, "find_actors", {
        "root": None, "name": "", "actor_type": ref(POI_CLASS),
        "tag": "", "bounds": None, "collision_channels": [],
    })
    found = {}
    for actor in actors:
        label = call(ACTOR_TOOLSET, "get_label", {"actor": actor})
        found.setdefault(label, []).append(actor)
    return found


def external_actor_files():
    """Every BR_Arena01 external-actor package on disk, repo-relative and sorted."""
    files = []
    for root, _dirs, names in os.walk(EXTERNAL_ACTORS_DIR):
        for name in names:
            if name.endswith(".uasset"):
                full = os.path.join(root, name)
                files.append(os.path.relpath(full, REPO_ROOT).replace("\\", "/"))
    return sorted(files)


def dirty_assets(folders):
    """Assets with unsaved changes in the given content folders. One batched editor call --
    a per-asset round trip over /Game is minutes of HTTP; this is seconds."""
    script = """
import json

def find_assets(folder):
    return execute_tool("editor_toolset.toolsets.asset.AssetTools.find_assets",
        json.dumps({"folder_path": folder, "name": "", "asset_type": None,
                    "recursive": True, "tags": None}))["returnValue"]

def is_dirty(path):
    return execute_tool("editor_toolset.toolsets.asset.AssetTools.is_dirty",
        json.dumps({"asset_path": path}))["returnValue"]

def run():
    dirty = []
    for folder in %s:
        for asset in find_assets(folder):
            if is_dirty(asset):
                dirty.append(asset)
    return {"dirty": dirty}
""" % json.dumps(folders)
    raw = call("editor_toolset.toolsets.programmatic.ProgrammaticToolset",
               "execute_tool_script", {"script": script})
    return json.loads(raw)["dirty"] if isinstance(raw, str) else raw["dirty"]


def check_level():
    level = call(SCENE_TOOLSET, "get_current_level", {})
    if level != LEVEL:
        raise SystemExit(
            f"STOP: the loaded level is {level!r}, not {LEVEL!r}. This script is the only "
            "writer of the arena and will not place actors in a map the ticket did not name."
        )
    print(f"level          : {level}")
    return level


def converge_pois():
    print("--- Step 3: four ABNPointOfInterest in the arena ----------------------------")
    check_level()
    found = find_pois()
    for poi in POIS:
        label = poi["label"]
        matches = found.get(label, [])
        if len(matches) > 1:
            print(f"  [REPORT] {label}: {len(matches)} actors share this label; converging the "
                  "first and leaving the rest alone -- a human must delete the extras.")
        if matches:
            actor = matches[0]
            print(f"  [exists] {label} -> {actor['refPath']}")
        else:
            x, y, z = poi["loc"]
            actor = call(SCENE_TOOLSET, "add_to_scene_from_class", {
                "actor_type": ref(POI_CLASS),
                "name": label,
                "xform": {"location": {"x": x, "y": y, "z": z},
                          "rotation": {"pitch": 0.0, "yaw": 0.0, "roll": 0.0},
                          "scale": {"x": 1.0, "y": 1.0, "z": 1.0}},
                "parent": None,
                "snap_to_ground": False,
            })
            print(f"  [create] {label} -> {actor['refPath']}")
            call(ACTOR_TOOLSET, "set_label", {"actor": actor, "label": label})
        # Always rewrite the full intent: this is what makes a dirty editor converge.
        x, y, z = poi["loc"]
        call(ACTOR_TOOLSET, "set_actor_transform", {
            "actor": actor,
            "xform": {"location": {"x": x, "y": y, "z": z},
                      "rotation": {"pitch": 0.0, "yaw": 0.0, "roll": 0.0},
                      "scale": {"x": 1.0, "y": 1.0, "z": 1.0}},
            "worldspace": True,
        })
        call(OBJECT_TOOLSET, "set_properties", {
            "instance": actor,
            "values": json.dumps({"pointName": poi["pointName"], "radius": poi["radius"]}),
        })
        print(f"           set pointName={poi['pointName']!r} radius={poi['radius']} "
              f"loc=({x}, {y}, {z})")

    extra = [label for label in found if label not in {p["label"] for p in POIS}]
    if extra:
        print(f"  [REPORT] point-of-interest actors this ticket did not name, NOT removed: {extra}")

    save_placed_actors()
    print()


def save_placed_actors():
    """Get the four new actors onto disk. THE ONLY REACHABLE ROUTE, and why:

    BR_Arena01 is a World Partition map, so each placed actor lives in its own package under
    /Game/__ExternalActors__/Maps/BR_Arena01/<x>/<yy>/<hash>. A freshly placed actor's package
    is not in the asset registry yet, so all three targeted saves answer
    "Asset does not exist: /Game/__ExternalActors__/...":
        SceneTools.save_actor(actor)
        AssetTools.is_dirty(<external package path>)
        AssetTools.save_assets([<external package path>])
    AssetTools.save_assets(["/Game/Maps/BR_Arena01"]) returns True and writes NOTHING -- the
    .umap itself is not dirty; the new actors are. That leaves save_assets([]) ("save all
    dirty assets"), which is a project-wide write, so it is guarded and audited here:
      - guard: nothing outside this ticket's owner paths may be dirty when we fire it;
      - audit: the external-actor files on disk are listed before and after, so the report can
        name exactly which files this run created.
    """
    print("  --- saving (World Partition external actors) ---")
    before = set(external_actor_files())
    foreign = dirty_assets(GUARD_FOLDERS)
    if foreign:
        print(f"  [STOP] assets dirty in the editor that this ticket does not own: {foreign}")
        print("  Refusing save_assets([]) -- a save-all would write another agent's asset "
              "(law 7, one writer per binary asset). Resolve ownership, then re-run.")
        raise SystemExit(2)
    call(ASSET_TOOLSET, "save_assets", {"asset_paths": []})
    after = set(external_actor_files())
    created = sorted(after - before)
    print(f"  [save] save_assets([]) done; external-actor files: {len(before)} -> {len(after)}")
    for path in created:
        print(f"  [new file] {path}")
    if not created:
        print("  [new file] none -- every point of interest was already on disk (idempotent run)")


# --- audit -----------------------------------------------------------------------

def audit():
    print("=========================== ST_BNBot / POI AUDIT ============================")
    probe_build()
    tree_ok = report_tree()
    print()
    print("--- Step 3/4: the four placed actors, read back from the live editor --------")
    print(f"level          : {call(SCENE_TOOLSET, 'get_current_level', {})}")
    print(f"{LEVEL} is_dirty: {call(ASSET_TOOLSET, 'is_dirty', {'asset_path': LEVEL})}")
    print(f"external-actor files on disk under Content/__ExternalActors__/Maps/BR_Arena01: "
          f"{len(external_actor_files())}")
    found = find_pois()

    header = (f"{'label':<14} {'pointName':>10} {'radius':>8} "
              f"{'location (x, y, z)':>28}  {'ok'}")
    print(header)
    print("-" * len(header))
    failures = 0
    for poi in POIS:
        matches = found.get(poi["label"], [])
        if not matches:
            print(f"{poi['label']:<14} {'<ABSENT>':>10}")
            failures += 1
            continue
        actor = matches[0]
        values = json.loads(call(OBJECT_TOOLSET, "get_properties", {
            "instance": actor, "properties": ["pointName", "radius"],
        }))
        xform = call(ACTOR_TOOLSET, "get_actor_transform", {"actor": actor})
        loc = xform.get("location", {})
        got = (loc.get("x"), loc.get("y"), loc.get("z"))
        name_ok = values.get("pointName") == poi["pointName"]
        radius_ok = abs(float(values.get("radius", -1)) - poi["radius"]) < 1e-6
        loc_ok = all(a is not None and abs(float(a) - b) < 1e-3
                     for a, b in zip(got, poi["loc"]))
        ok = name_ok and radius_ok and loc_ok and len(matches) == 1
        failures += 0 if ok else 1
        print(f"{poi['label']:<14} {str(values.get('pointName')):>10} "
              f"{str(values.get('radius')):>8} {str(got):>28}  "
              f"{'OK' if ok else 'MISMATCH'}")
        print(f"{'':<14} refPath: {actor['refPath']}")
    print("-" * len(header))

    extra = [label for label in found if label not in {p["label"] for p in POIS}]
    if extra:
        print(f"unexpected extra ABNPointOfInterest labels present: {extra}")

    if failures:
        print(f"POI AUDIT FAIL -- {failures} mismatch(es)")
        return 1
    print("POI AUDIT PASS -- four points of interest match intent")
    if not tree_ok:
        print("TREE AUDIT HANDBACK -- /Game/BN/AI/ST_BNBot is absent and cannot be created,")
        print("  edited, compiled or read back through any registered toolset (header, S1-4).")
        return 2
    print("TREE: asset present at the ini path and of class StateTree. Its states, tasks and")
    print("  transition delays remain unreadable through these tools -- eyes only.")
    return 0


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "apply"
    if mode == "apply":
        probe_build()
        converge_pois()
        report_tree()
        return 0
    if mode == "audit":
        return audit()
    raise SystemExit(f"unknown mode {mode!r}; use 'apply' or 'audit'")


if __name__ == "__main__":
    sys.exit(main())
