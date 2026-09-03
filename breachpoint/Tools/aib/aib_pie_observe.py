"""PIE observation — the founder's "is the AI actually doing it" check. In-editor, PIE running.

    py <repo>/Tools/aib/aib_pie_observe.py sample <tag>     # one line per bot: pos, speed, state
    py <repo>/Tools/aib/aib_pie_observe.py summary <tag>    # per-bot distance, longest still spell

`sample` appends to Saved/AIBObserve/<tag>.jsonl (the lead calls it every ~5 s from the MCP,
capturing the viewport between calls); `summary` reads the file back and logs per-bot totals:
metres moved, longest spell under 20 uu/s, states visited, min/max z. Numbers, not adjectives.
"""
import json
import os
import sys
import time
import unreal

OUT = os.path.join(unreal.Paths.project_saved_dir(), "AIBObserve")


def game_world():
    return unreal.UnrealEditorSubsystem().get_game_world()


def bots(w):
    out = []
    for c in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.AIController):
        p = c.get_editor_property("pawn")
        if p:
            out.append((c.get_name(), c, p))
    return out


def state_of(c):
    for attr in ("get_active_state_name", "get_current_ambition_name"):
        f = getattr(c, attr, None)
        if f:
            try:
                return str(f())
            except Exception:  # noqa: BLE001
                pass
    return "?"


def sample(tag):
    os.makedirs(OUT, exist_ok=True)
    w = game_world()
    t = w.get_time_seconds() if hasattr(w, "get_time_seconds") else time.time()
    rows = []
    for name, c, p in bots(w):
        loc = p.get_actor_location()
        vel = p.get_velocity()
        rows.append({"t": round(float(t), 2), "bot": name, "x": round(loc.x), "y": round(loc.y), "z": round(loc.z),
                     "speed": round(float(vel.length())), "state": state_of(c)})
    with open(os.path.join(OUT, tag + ".jsonl"), "a") as fh:
        for r in rows:
            fh.write(json.dumps(r) + "\n")
    for r in rows:
        unreal.log("AIBOBS %s %s at (%d,%d,%d) speed %d state %s" % (tag, r["bot"], r["x"], r["y"], r["z"], r["speed"], r["state"]))
    unreal.log("AIBOBS %s sample: %d bots at t=%.1f" % (tag, len(rows), t))


def summary(tag):
    path = os.path.join(OUT, tag + ".jsonl")
    rows = [json.loads(l) for l in open(path)]
    by = {}
    for r in rows:
        by.setdefault(r["bot"], []).append(r)
    for bot, rs in sorted(by.items()):
        rs.sort(key=lambda r: r["t"])
        dist = 0.0
        still, longest, prev = 0.0, 0.0, None
        for r in rs:
            if prev:
                dx, dy = r["x"] - prev["x"], r["y"] - prev["y"]
                dist += (dx * dx + dy * dy) ** 0.5
                dt = r["t"] - prev["t"]
                if r["speed"] < 20 and prev["speed"] < 20:
                    still += dt
                    longest = max(longest, still)
                else:
                    still = 0.0
            prev = r
        states = sorted(set(r["state"] for r in rs))
        unreal.log("AIBOBS %s SUMMARY %-24s moved %5.0fuu over %2d samples (%.0fs), longest still spell %4.1fs, z %d..%d, states %s" % (
            tag, bot, dist, len(rs), rs[-1]["t"] - rs[0]["t"], longest, min(r["z"] for r in rs), max(r["z"] for r in rs), "/".join(states)))
    unreal.log("AIBOBS %s summary done" % tag)


if __name__ == "__main__":
    mode, tag = sys.argv[1], sys.argv[2]
    sample(tag) if mode == "sample" else summary(tag)
