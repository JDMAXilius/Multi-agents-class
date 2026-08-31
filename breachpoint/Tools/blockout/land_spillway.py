"""BR_Spillway - the landing mechanism. Projects the generator's output into the editor.

    python3 Tools/blockout/gen_spillway.py     # design  -> Content/Data/spillway_*.json
    python3 Tools/blockout/land_spillway.py    # JSON    -> /Game/Maps/BR_Spillway

Talks to the Unreal MCP server that UE 5.8 runs inside the editor process
(Engine/Plugins/Experimental/ModelContextProtocol, HTTP on 127.0.0.1:8000/mcp), so the
editor must be OPEN with BR_Spillway loaded. Nothing here decides geometry - every
transform comes from Content/Data/spillway_placements.json.

IDEMPOTENT. Every actor it spawns carries the tag `SpillwayGenerated`, and step 1 destroys
everything already carrying that tag. A re-run is a clean rebuild, never an append, so
deleting an element from the generator actually deletes it from the map.

    --strip     also remove the inherited Lvl_Shooter geometry and template gameplay
                actors (first run only; they carry no tag of ours so step 1 misses them)
    --dry-run   print the plan, touch nothing
    --markers   spawns / power weapon / nav volume / game mode only, no geometry
"""

import json
import os
import sys
import urllib.request

URL = os.environ.get("UE_MCP_URL", "http://127.0.0.1:8000/mcp")

SCENE = "editor_toolset.toolsets.scene.SceneTools"
ACTOR = "editor_toolset.toolsets.actor.ActorTools"
OBJECT = "editor_toolset.toolsets.object.ObjectTools"
ASSET = "editor_toolset.toolsets.asset.AssetTools"
PROG = "editor_toolset.toolsets.programmatic.ProgrammaticToolset"

MAP_PATH = "/Game/Maps/BR_Spillway"
GAME_MODE = "/Game/BN/Core/BP_BNGameMode.BP_BNGameMode_C"
PLAYER_START = "/Script/Engine.PlayerStart"
POWER_SPAWNER = "/Script/Breachpoint.BRPowerWeaponSpawner"
NAV_BOUNDS_NAME = "NavMeshBoundsVolume"

# Template actors the remake replaces. Matched by label prefix or by class.
STRIP_CLASSES = ["/Script/Engine.StaticMeshActor", "/Script/Engine.PlayerStart"]
STRIP_LABEL_PREFIXES = ["BP_JumpPad", "BP_WobbleTarget", "BP_DoorFrame"]
STRIP_KEEP_LABELS = ["SM_SkySphere"]        # the sky is inherited, not template geometry

CHUNK = 12                                   # elements per execute_tool_script call

_sid = {"v": None}


def _post(method, params=None, rid=1):
    body = {"jsonrpc": "2.0", "method": method}
    if rid is not None:
        body["id"] = rid
    if params is not None:
        body["params"] = params
    headers = {"Content-Type": "application/json",
               "Accept": "application/json, text/event-stream"}
    if _sid["v"]:
        headers["Mcp-Session-Id"] = _sid["v"]
    req = urllib.request.Request(URL, data=json.dumps(body).encode(),
                                 headers=headers, method="POST")
    with urllib.request.urlopen(req, timeout=600) as resp:
        if resp.headers.get("Mcp-Session-Id"):
            _sid["v"] = resp.headers["Mcp-Session-Id"]
        raw = resp.read().decode("utf-8", "replace")
    if not raw.strip():
        return None
    if raw.lstrip().startswith(("event:", "data:")):
        for line in raw.splitlines():
            if line.startswith("data:"):
                raw = line[5:].strip()
                break
    return json.loads(raw)


def connect():
    _post("initialize", {"protocolVersion": "2024-11-05", "capabilities": {},
                         "clientInfo": {"name": "land_spillway", "version": "1"}})
    _post("notifications/initialized", {}, rid=None)


def _tool(name, args):
    r = _post("tools/call", {"name": name, "arguments": args})
    if "error" in r:
        raise RuntimeError(r["error"])
    out = []
    for c in r.get("result", {}).get("content", []):
        t = c.get("text")
        try:
            out.append(json.loads(t))
        except Exception:
            out.append(t)
    if r.get("result", {}).get("isError"):
        raise RuntimeError(out)
    return out[0] if len(out) == 1 else out


def call(toolset, tool, args):
    return _tool("call_tool", {"toolset_name": toolset, "tool_name": tool,
                               "arguments": args})


def script(body):
    r = call(PROG, "execute_tool_script", {"script": body})
    rv = r["returnValue"] if isinstance(r, dict) and "returnValue" in r else r
    return json.loads(rv) if isinstance(rv, str) else rv


# ---------------------------------------------------------------------------
HELPERS = '''
import json
def T(n, a): return execute_tool(n, json.dumps(a))
def find(name="", cls=None, tag=""):
    return T("%s.find_actors", {"root": None, "name": name,
        "actor_type": ({"refPath": cls} if cls else None),
        "tag": tag, "bounds": None, "collision_channels": None})["returnValue"]
''' % SCENE


def step_clear(tag="SpillwayGenerated"):
    """Destroy everything carrying `tag`. This is what makes a re-run a rebuild.

    NOTE the tag argument: markers also carry SpillwayMarkers, and --markers clears only
    THAT. An earlier version cleared SpillwayGenerated unconditionally, so a marker-only
    pass deleted all 130 geometry actors and replaced them with 8 spawns.
    """
    body = HELPERS + '''
def run():
    n = 0
    for a in find(tag="%s"):
        T("%s.remove_from_scene", {"actor": a}); n += 1
    return {"removed": n}
''' % (tag, SCENE)
    return script(body)


def step_strip():
    body = HELPERS + '''
def run():
    removed, kept = 0, []
    for cls in %(classes)s:
        for a in find(cls=cls):
            lb = T("%(actor)s.get_label", {"actor": a})["returnValue"]
            if lb in %(keep)s:
                kept.append(lb); continue
            T("%(scene)s.remove_from_scene", {"actor": a}); removed += 1
    for a in find():
        lb = T("%(actor)s.get_label", {"actor": a})["returnValue"]
        if any(lb.startswith(p) for p in %(prefixes)s):
            T("%(scene)s.remove_from_scene", {"actor": a}); removed += 1
    return {"removed": removed, "kept": kept}
''' % {"classes": json.dumps(STRIP_CLASSES), "keep": json.dumps(STRIP_KEEP_LABELS),
       "prefixes": json.dumps(STRIP_LABEL_PREFIXES), "scene": SCENE, "actor": ACTOR}
    return script(body)


def step_geometry(elements):
    total = 0
    for i in range(0, len(elements), CHUNK):
        batch = elements[i:i + CHUNK]
        body = HELPERS + '''
BATCH = %(batch)s
def run():
    made = 0
    for e in BATCH:
        a = T("%(scene)s.add_to_scene_from_asset", {
            "asset_path": e["mesh"], "name": e["label"],
            "xform": {"location": {"x": e["loc"][0], "y": e["loc"][1], "z": e["loc"][2]},
                      "rotation": {"pitch": e["rot"][0], "yaw": e["rot"][1], "roll": e["rot"][2]},
                      "scale": {"x": e["scale"][0], "y": e["scale"][1], "z": e["scale"][2]}},
            "parent": None, "snap_to_ground": False})["returnValue"]
        T("%(actor)s.set_label", {"actor": a, "label": e["label"]})
        T("%(actor)s.add_tag", {"actor": a, "tag": "SpillwayGenerated"})
        T("%(scene)s.set_actor_folder", {"actor": a, "folder_path": e["folder"]})
        # No material override here, deliberately: the MESH carries the material. See
        # Tools/blockout/make_spillway_meshes.py - ObjectTools.set_properties cannot write
        # the components of World Partition external actors, so per-actor overrides are
        # not available through this server at all.
        made += 1
    return {"made": made}
''' % {"batch": json.dumps(batch), "scene": SCENE, "actor": ACTOR, "object": OBJECT}
        r = script(body)
        total += r["made"]
        print("    %3d/%d" % (total, len(elements)), flush=True)
    return total


def step_markers(man):
    spawns = man["spawn_points"]
    pw = man["power_weapon_node"]
    nav = man["nav_bounds"]
    body = HELPERS + '''
SPAWNS = %(spawns)s
PW = %(pw)s
NAV = %(nav)s
def run():
    out = {"spawns": 0, "power": None, "nav": None}
    for s in SPAWNS:
        a = T("%(scene)s.add_to_scene_from_class", {
            "actor_type": {"refPath": "%(ps)s"}, "name": s["id"],
            "xform": {"location": {"x": s["loc"][0], "y": s["loc"][1], "z": s["loc"][2]},
                      "rotation": {"pitch": 0, "yaw": s["yaw"], "roll": 0},
                      "scale": {"x": 1, "y": 1, "z": 1}},
            "parent": None, "snap_to_ground": False})["returnValue"]
        T("%(actor)s.set_label", {"actor": a, "label": "SPW_" + s["id"]})
        T("%(actor)s.add_tag", {"actor": a, "tag": "SpillwayGenerated"})
        T("%(actor)s.add_tag", {"actor": a, "tag": "SpillwayMarkers"})
        T("%(actor)s.add_tag", {"actor": a, "tag": s["team"]})
        T("%(scene)s.set_actor_folder", {"actor": a, "folder_path": "Spillway/60_Spawns"})
        out["spawns"] += 1
    try:
        a = T("%(scene)s.add_to_scene_from_class", {
            "actor_type": {"refPath": "%(pws)s"}, "name": "SPW_PowerWeapon",
            "xform": {"location": {"x": PW[0], "y": PW[1], "z": PW[2]},
                      "rotation": {"pitch": 0, "yaw": 0, "roll": 0},
                      "scale": {"x": 1, "y": 1, "z": 1}},
            "parent": None, "snap_to_ground": False})["returnValue"]
        T("%(actor)s.set_label", {"actor": a, "label": "SPW_PowerWeapon"})
        T("%(actor)s.add_tag", {"actor": a, "tag": "SpillwayGenerated"})
        T("%(actor)s.add_tag", {"actor": a, "tag": "SpillwayMarkers"})
        T("%(scene)s.set_actor_folder", {"actor": a, "folder_path": "Spillway/60_Spawns"})
        out["power"] = "placed"
    except Exception as ex:
        out["power"] = "FAILED: " + str(ex)[:120]
    # The nav volume is a Brush, and its unscaled size is whatever the template author
    # drew - NOT the 200 uu builder-brush default. Assuming 200 produced a 94 km volume.
    # So: measure the current bounds, divide out the current scale to recover the base
    # size, and only then solve for the scale that gives the size we actually want.
    for a in find(name="%(nav_name)s"):
        xf = T("%(actor)s.get_actor_transform", {"actor": a})["returnValue"]
        bb = T("%(actor)s.get_actor_bounds", {"actor": a})["returnValue"]
        cur = [bb["max"]["x"] - bb["min"]["x"],
               bb["max"]["y"] - bb["min"]["y"],
               bb["max"]["z"] - bb["min"]["z"]]
        sc = [xf["scale"]["x"], xf["scale"]["y"], xf["scale"]["z"]]
        base = [cur[i] / sc[i] for i in range(3)]
        want = [NAV["size"][i] / base[i] for i in range(3)]
        T("%(actor)s.set_actor_transform", {"actor": a, "worldspace": True, "xform": {
            "location": {"x": NAV["centre"][0], "y": NAV["centre"][1], "z": NAV["centre"][2]},
            "rotation": {"pitch": 0, "yaw": 0, "roll": 0},
            "scale": {"x": want[0], "y": want[1], "z": want[2]}}})
        got = T("%(actor)s.get_actor_bounds", {"actor": a})["returnValue"]
        out["nav"] = {"base_uu": [round(v) for v in base],
                      "size_uu": [round(got["max"]["x"] - got["min"]["x"]),
                                  round(got["max"]["y"] - got["min"]["y"]),
                                  round(got["max"]["z"] - got["min"]["z"])]}
    return out
''' % {"spawns": json.dumps(spawns), "pw": json.dumps(pw), "nav": json.dumps(nav),
       "scene": SCENE, "actor": ACTOR, "ps": PLAYER_START, "pws": POWER_SPAWNER,
       "nav_name": NAV_BOUNDS_NAME}
    return script(body)


def step_gamemode():
    body = HELPERS + '''
def run():
    ws = find(cls="/Script/Engine.WorldSettings")
    if not ws:
        return {"world_settings": "not found"}
    T("%(object)s.set_properties", {"instance": ws[0],
        "values": {"DefaultGameMode": {"refPath": "%(gm)s"}}})
    got = T("%(object)s.get_properties", {"instance": ws[0],
        "properties": ["DefaultGameMode"]})["returnValue"]
    return {"world_settings": got}
''' % {"object": OBJECT, "gm": GAME_MODE}
    return script(body)


def main():
    root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    with open(os.path.join(root, "Content", "Data", "spillway_placements.json"),
              encoding="utf-8") as f:
        placements = json.load(f)
    with open(os.path.join(root, "Content", "Data", "spillway_manifest.json"),
              encoding="utf-8") as f:
        man = json.load(f)
    els = placements["elements"]

    if "--dry-run" in sys.argv:
        print("would land %d elements, %d spawns, 1 power node into %s"
              % (len(els), len(man["spawn_points"]), MAP_PATH))
        return 0

    connect()
    cur = call(SCENE, "get_current_level", {})["returnValue"]
    if cur != MAP_PATH:
        raise SystemExit("editor is on %s - open %s first" % (cur, MAP_PATH))
    print("editor on %s" % cur)

    if "--strip" in sys.argv:
        print("strip inherited template actors ...")
        print("   ", step_strip())

    marker_only = "--markers" in sys.argv
    print("clear previous %s ..." % ("SpillwayMarkers" if marker_only else "SpillwayGenerated"))
    print("   ", step_clear("SpillwayMarkers" if marker_only else "SpillwayGenerated"))

    if not marker_only:
        print("place %d elements ..." % len(els))
        step_geometry(els)

    print("place markers ...")
    print("   ", step_markers(man))
    print("world settings ...")
    print("   ", step_gamemode())
    print("\nNOT SAVED. Save with AssetTools.save_assets(['%s']) when you are happy."
          % MAP_PATH)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
