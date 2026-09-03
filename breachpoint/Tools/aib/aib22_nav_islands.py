#!/usr/bin/env python3
"""AIB22 step 3(a) - is each isolation deck PATH-CONNECTED to the floor after a nav build?

    python3 Tools/aib/aib22_nav_islands.py --selftest        # probe points vs the plan, no editor
    python3 Tools/aib/aib22_nav_islands.py                   # MCP read-back + dispatch the probe
    python3 Tools/aib/aib22_nav_islands.py --report          # print the last JSON as deck= lines

  and, INSIDE the editor (Output Log > Cmd, or via Python remote execution if enabled):

    py <repo>/Tools/aib/aib22_nav_islands.py --in-editor --rebuild   # RebuildNavigation, return
    py <repo>/Tools/aib/aib22_nav_islands.py --in-editor             # probe, write the JSON

Output: one line per deck `deck=<name> connected=<yes|no>` and
Tools/aib/baselines/nav-isolation-<YYYY-MM-DD>.json (points projected, legs, agent numbers).

WHAT THE MCP TRANSPORT CAN AND CANNOT DO HERE (read from the 5.8 EditorToolset source):
  - CAN: confirm the loaded level, list the AIB22_NavIsolation actors and their bounds
    (ActorTools.get_actor_bounds), and line-trace the surfaces at every probe point
    (SceneTools.trace_world). That is a GEOMETRY read-back - it proves the pieces are where
    the plan says, not that Recast merged them.
  - CANNOT ask the navigation system. There is no navigation toolset (AIModuleToolset is
    behaviour trees only), no console-command tool, ObjectTools reads/writes properties but
    calls no UFUNCTION, and ProgrammaticToolset.execute_tool_script allow-lists only
    json/math/datetime/copy/re/time - `import unreal` is rejected at AST check and again by
    _safe_import. So the connectivity question is answered by the editor's OWN Python
    (PythonScriptPlugin, full `unreal`), the way BN33 ran `RebuildNavigation` and
    `project_point_to_navigation`: --in-editor below. The outer run dispatches it through the
    engine's remote_execution client when Editor Preferences > Python > Remote Execution is on
    (it is NOT on in Config/), and otherwise prints the exact `py` line to paste.

WHY TWO IN-EDITOR CALLS: Recast tiles are built on workers and APPLIED on the game-thread tick
(BN34: "ProcessTileTasksAndGetUpdatedTiles is async dispatch"). A script that issues
RebuildNavigation and then sleeps on the game thread waits forever. So --rebuild returns at
once and the probe REFUSES while is_navigation_being_built() is true; run it again.

THE MEASURE, per pair (points from aib22_nav_isolation.probe_points, on the surfaces):
  - project_point_to_navigation at floor / ramp_foot / ramp_mid / ramp_head / deck
    (extent 50,50,200): does nav EXIST there?
  - find_path_to_location_synchronously floor->deck: connected = valid AND NOT partial
    (partial is what an island returns - AIB22 W-AUDIT member 1). Legs floor->ramp_mid and
    ramp_mid->deck localise WHICH junction (foot or head) split.
  - the RecastNavMesh numbers the hypothesis lives on: CellSize, CellHeight,
    AgentMaxStepHeight, AgentRadius, AgentHeight, TileSizeUU, read off the live actor.
"""

import argparse
import datetime
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(REPO, "Tools", "blockout"))
sys.path.insert(0, os.path.join(REPO, "mcp-ui", "gen_ui"))

import aib22_nav_isolation as ISO  # noqa: E402

BASELINES = os.path.join(HERE, "baselines")
UE_ROOT = os.environ.get("UE_ROOT", "/Users/Shared/Epic Games/UE_5.8")
REMOTE_EXEC = os.path.join(UE_ROOT, "Engine", "Plugins", "Experimental", "PythonScriptPlugin",
                           "Content", "Python")
EXTENT = (50.0, 50.0, 200.0)
LEGS = (("floor", "deck"), ("floor", "ramp_mid"), ("ramp_mid", "deck"))
RECAST_PROPS = ("nav_mesh_resolution_params", "agent_radius", "agent_height",
                "agent_max_slope", "tile_size_uu")   # 5.8: cell_size/cell_height/step live in
                                                   # nav_mesh_resolution_params (per resolution)
# the same numbers through ObjectTools (its property names are camelCase; verified 2026-09-02)
RECAST_PROPS_MCP = ["navMeshResolutionParams", "agentRadius", "agentHeight", "agentMaxSlope",
                    "tileSizeUU", "mergeRegionSize", "minRegionArea", "ledgeSlopeFilterMode",
                    "bFilterLowSpanSequences", "runtimeGeneration"]
RECAST_CLASS = "/Script/NavigationSystem.RecastNavMesh"


def json_path(date=None):
    return os.path.join(BASELINES, "nav-isolation-%s.json"
                        % (date or datetime.date.today().isoformat()))


def report_lines(data):
    """The one-line-per-deck verdicts, from the JSON the in-editor half wrote."""
    out = []
    for p in data["pairs"]:
        leg = p["legs"]["floor->deck"]
        c = leg["connected"]
        out.append("deck=%s connected=%s" % (p["deck"], "UNMEASURED" if c is None
                                             else "yes" if c else "no"))
    return out


def probes(into_gym):
    origin = ISO.ORIGIN_GYM if into_gym else (0.0, 0.0, 0.0)
    return [(p, {k: tuple(ISO.offset(v, origin)) for k, v in ISO.probe_points(p).items()})
            for p in ISO.PAIRS]


# --- inside the editor: the only place the nav system can be asked --------------------
def in_editor(rebuild, into_gym):
    import unreal
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    NS = unreal.NavigationSystemV1
    if rebuild:
        unreal.SystemLibrary.execute_console_command(world, "RebuildNavigation")
        print("AIB22: RebuildNavigation issued; run again WITHOUT --rebuild once the build "
              "finishes (the probe refuses while tiles are still being applied).")
        return 0
    if NS.is_navigation_being_built(world):
        print("AIB22: STOP - navigation is still being built; re-run in a moment.")
        return 3

    agent = {}
    for nav in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.RecastNavMesh):
        for prop in RECAST_PROPS:
            try:
                agent[prop] = nav.get_editor_property(prop)
            except Exception as exc:  # noqa: BLE001 - deprecated names differ per version
                agent[prop] = "n/a: %s" % str(exc)[:60]
        break

    def vec(t):
        return unreal.Vector(*t)

    def project(pt):
        # UE's Python wrapper folds a bool return + out-param into "the out value, or
        # None when the call returned false" (not a tuple).
        r = NS.project_point_to_navigation(world, vec(pt), None, None, vec(EXTENT))
        if isinstance(r, tuple):                 # defensive: (bool, Vector) shape
            r = r[1] if r[0] else None
        return {"query": list(pt), "on_nav": r is not None,
                "projected": [r.x, r.y, r.z] if r is not None else None}

    def path(a, b):
        p = NS.find_path_to_location_synchronously(world, vec(a), vec(b), None, None)
        if p is None or not p.is_valid():
            return {"valid": False, "partial": None, "length": None, "connected": False}
        partial = bool(p.is_partial())
        return {"valid": True, "partial": partial, "length": round(p.get_path_length(), 1),
                "points": len(p.path_points), "connected": not partial}

    data = {"date": datetime.date.today().isoformat(), "map": ISO.TEMPLATE_MAP if into_gym
            else ISO.MAP_PATH, "level_loaded": world.get_outermost().get_name(),
            "nav_agent": agent, "extent": list(EXTENT), "pairs": []}
    for p, pts in probes(into_gym):
        rec = {"name": p["name"], "deck": "NI_%s_Deck" % p["name"],
               "points": {k: project(v) for k, v in pts.items()},
               "legs": {"%s->%s" % (a, b): path(pts[a], pts[b]) for a, b in LEGS}}
        data["pairs"].append(rec)
    os.makedirs(BASELINES, exist_ok=True)
    with open(json_path(), "w", encoding="utf-8") as f:
        json.dump(data, f, indent=1, default=str)
    for line in report_lines(data):
        print(line)
        unreal.log(line)
    print("wrote %s" % json_path())
    return 0


# --- outside the editor: MCP geometry read-back, then hand the nav question over --------
def mcp_readback(into_gym):
    from mcp import MCP
    SCENE = "editor_toolset.toolsets.scene.SceneTools"
    ACTOR = "editor_toolset.toolsets.actor.ActorTools"
    m = MCP()
    m.init()
    want_level = ISO.TEMPLATE_MAP if into_gym else ISO.MAP_PATH
    cur, _ = m.call(SCENE, "get_current_level", {})
    print("level loaded: %s%s" % (cur, "" if cur == want_level else "  <- NOT %s, STOP" % want_level))
    if cur != want_level:
        return 1
    found, _ = m.call(SCENE, "find_actors", {"root": None, "name": "", "actor_type": None,
                                             "tag": ISO.TAG, "bounds": None,
                                             "collision_channels": []})
    labels = {}
    for a in found or []:
        lb, _ = m.call(ACTOR, "get_label", {"actor": a})
        bb, _ = m.call(ACTOR, "get_actor_bounds", {"actor": a})
        labels[lb] = bb
        print("  %-16s aabb min %s max %s" % (lb, [round(bb["min"][k]) for k in "xyz"],
                                              [round(bb["max"][k]) for k in "xyz"]))
    bad = [e["label"] for e in ISO.plan() if e["label"] not in labels]
    if bad:
        print("MISSING actors: %s - build first (Tools/blockout/aib22_nav_isolation.py)" % bad)
        return 1
    print("--- trace_world at every probe point (geometry present, NOT nav) ---")
    drift = 0
    for p, pts in probes(into_gym):
        for k, (x, y, z) in pts.items():
            d, raw = m.call(SCENE, "trace_world", {"start": {"x": x, "y": y, "z": z + 300.0},
                                                    "end": {"x": x, "y": y, "z": z - 300.0}})
            hit = None if d is None else round(z + 300.0 - float(d), 2)
            ok = hit is not None and abs(hit - z) <= 2.0
            drift += 0 if ok else 1
            print("  %-6s %-9s want z %7.2f  hit z %s  %s%s" % (
                p["name"], k, z, hit, "OK" if ok else "DRIFT",
                "" if d is not None else "  (%s)" % str(raw)[:80]))
    print("geometry read-back: %s" % ("PASS" if not drift else "FAIL (%d)" % drift))
    if drift:
        return 1, None
    OBJ = "editor_toolset.toolsets.object.ObjectTools"
    navs, _ = m.call(SCENE, "find_actors", {"root": None, "name": "", "actor_type": RECAST_CLASS,
                                            "tag": "", "bounds": None, "collision_channels": []})
    agent = {}
    for nav in navs or []:
        bb, _ = m.call(ACTOR, "get_actor_bounds", {"actor": nav})
        got, raw = m.call(OBJ, "get_properties", {"instance": nav, "properties": RECAST_PROPS_MCP})
        agent = json.loads(got) if isinstance(got, str) else (got or {"error": raw[:200]})
        agent["actor"] = nav["refPath"].rsplit(".", 1)[-1]
        agent["tile_bounds"] = [[round(bb["min"][k]) for k in "xyz"], [round(bb["max"][k]) for k in "xyz"]]
        break
    print("RecastNavMesh (live actor): %s" % (json.dumps(agent) if agent else "NONE in level"))
    return 0, agent


def write_unmeasured(into_gym, agent):
    """The transport cannot ask the nav system: save what WAS read (geometry, agent numbers)
    with every leg UNMEASURED, so --report and the ticket Log say so instead of 'no'."""
    data = {"date": datetime.date.today().isoformat(),
            "map": ISO.TEMPLATE_MAP if into_gym else ISO.MAP_PATH, "measured": False,
            "why": "no console-command / python-exec MCP tool and Python remote execution is "
                   "off; the two --in-editor lines were not run", "nav_agent": agent,
            "extent": list(EXTENT), "pairs": []}
    for p, pts in probes(into_gym):
        data["pairs"].append({"name": p["name"], "deck": "NI_%s_Deck" % p["name"],
                              "points": {k: {"query": list(v), "on_nav": None, "projected": None}
                                         for k, v in pts.items()},
                              "legs": {"%s->%s" % (a, b): {"valid": None, "partial": None,
                                                           "length": None, "connected": None}
                                       for a, b in LEGS}})
    os.makedirs(BASELINES, exist_ok=True)
    with open(json_path(), "w", encoding="utf-8") as f:
        json.dump(data, f, indent=1, default=str)
    print("wrote %s (legs UNMEASURED)" % json_path())


def dispatch(into_gym):
    """Run the in-editor half through Python remote execution if the editor answers."""
    me = os.path.abspath(__file__)
    cmd = '"%s" --in-editor%s' % (me, " --into-gym" if into_gym else "")
    try:
        sys.path.insert(0, REMOTE_EXEC)
        import remote_execution as rx
        import time
        r = rx.RemoteExecution()
        r.start()
        time.sleep(1.5)
        nodes = r.remote_nodes
        if not nodes:
            r.stop()
            raise RuntimeError("no editor advertising Python remote execution on 239.0.0.1:6766")
        r.open_command_connection(nodes[0]["node_id"])
        for extra in (" --rebuild", ""):
            res = r.run_command(cmd + extra, exec_mode=rx.MODE_EXEC_FILE)
            print("remote %s -> %s" % (extra or "probe", json.dumps(res)[:400]))
            if extra:
                time.sleep(5.0)             # let the tiles apply before the probe
        r.stop()
        return True
    except Exception as exc:  # noqa: BLE001 - the fallback is the paste line, not a crash
        print("remote execution unavailable (%s)." % str(exc)[:120])
        print("Paste into the editor Output Log (Cmd: Python), one after the other:")
        print("  py %s --rebuild" % cmd)
        print("  py %s" % cmd)
        return False


def selftest():
    for p, pts in probes(False):
        r = ISO.ramp_z
        assert pts["floor"][2] == 0.0 and pts["deck"][2] == ISO.DECK_TOP
        assert ISO.DECK_X0 < pts["deck"][0] < ISO.DECK_X1
        for k in ("ramp_foot", "ramp_mid", "ramp_head"):
            x, y, z = pts[k]
            assert p["x0"] < x < ISO.DECK_X0, (k, x)      # visible ramp, never inside the deck
            assert abs(z - r(p, x)) < 1e-9
            assert y == p["ramp_y0"] + ISO.W / 2.0
        assert pts["ramp_head"][2] < ISO.DECK_TOP - 20.0    # a step is visible at the head
        for a, b in LEGS:
            assert a in pts and b in pts
    # the report formatter against the shape the in-editor half writes
    fake = {"pairs": [{"deck": "NI_A_Abut_Deck", "legs": {"floor->deck": {"connected": False}}},
                      {"deck": "NI_B_Fix_Deck", "legs": {"floor->deck": {"connected": True}}}]}
    assert report_lines(fake) == ["deck=NI_A_Abut_Deck connected=no",
                                  "deck=NI_B_Fix_Deck connected=yes"]
    fake["pairs"][0]["legs"]["floor->deck"]["connected"] = None
    assert report_lines(fake)[0] == "deck=NI_A_Abut_Deck connected=UNMEASURED"
    assert json_path("2026-09-02").endswith("baselines/nav-isolation-2026-09-02.json")
    for p, pts in probes(True):
        assert pts["floor"][1] == ISO.probe_points(p)["floor"][1] + ISO.ORIGIN_GYM[1]
    print("selftest OK: %d pairs x %d points, %d legs" % (len(ISO.PAIRS), len(pts), len(LEGS)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--in-editor", action="store_true", help="run under the editor's python")
    ap.add_argument("--rebuild", action="store_true", help="with --in-editor: RebuildNavigation")
    ap.add_argument("--report", action="store_true", help="print the latest JSON, no editor")
    ap.add_argument("--into-gym", action="store_true", help="the builder's --into-gym layout")
    args = ap.parse_args()
    if args.selftest:
        selftest()
        return 0
    if args.in_editor:
        return in_editor(args.rebuild, args.into_gym)
    if not args.report:
        rc, agent = mcp_readback(args.into_gym)
        if rc:
            return rc
        if not dispatch(args.into_gym):
            write_unmeasured(args.into_gym, agent)
            return 3
    path = json_path()
    if not os.path.exists(path):
        print("no %s yet - run the two in-editor lines above, then --report" % path)
        return 3
    with open(path, encoding="utf-8") as f:
        data = json.load(f)
    print("nav agent: %s" % json.dumps(data.get("nav_agent"), default=str))
    if data.get("measured") is False:
        print("UNMEASURED: %s" % data.get("why"))
    for p in data["pairs"]:
        for k, v in p["legs"].items():
            print("  %-6s %-16s valid=%s partial=%s length=%s" % (p["name"], k, v["valid"],
                                                                  v["partial"], v["length"]))
        print("  %-6s on_nav: %s" % (p["name"], {k: v["on_nav"] for k, v in p["points"].items()}))
    for line in report_lines(data):
        print(line)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
