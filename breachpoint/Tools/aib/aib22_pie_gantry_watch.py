"""AIB22 "Done when" box 4 — a bot placed on the top platform leaves it within 5 s. PIE, in-editor.

    py <repo>/Tools/aib/aib22_pie_gantry_watch.py --place BR_LM_The_Gantry_01   # PIE running
    py <repo>/Tools/aib/aib22_pie_gantry_watch.py --read  BR_LM_The_Gantry_01   # 5 s later, and again

--place teleports EVERY possessed AI bot onto the platform's top (spread across it, +100 uu),
records t0. --read prints, per bot, seconds since t0, its z, whether it is still inside the
platform's XY footprint above its top, and the LogAIBot island/egress lines are the verdict
(read them with the MCP LogsToolset, category LogAIBot). Python cannot sleep on the game
thread, so it is two invocations. Type both into the editor console (Cmd) while PIE runs.
"""
import sys
import time
import unreal

STAMP = "/tmp/aib22_gantry_t0"


def game_world():
    return unreal.UnrealEditorSubsystem().get_game_world()


def platform(w, label):
    for m in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.StaticMeshActor):
        if m.get_actor_label() == label:
            o, e = m.get_actor_bounds(False)
            return o, e
    raise SystemExit("no actor labelled %s in the PIE world" % label)


def bots(w):
    out = []
    for c in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.AIController):
        p = c.get_controlled_pawn() if hasattr(c, "get_controlled_pawn") else c.get_editor_property("pawn")
        if p:
            out.append((c.get_name(), p))
    return out


def place(label):
    w = game_world()
    o, e = platform(w, label)
    bs = bots(w)
    n = max(1, len(bs))
    for i, (name, p) in enumerate(bs):
        # spread along the platform's long axis, inset from the edges by 100
        f = (i + 0.5) / n
        x = o.x - (e.x - 100) + 2 * (e.x - 100) * f
        p.set_actor_location(unreal.Vector(x, o.y, o.z + e.z + 100), False, True)
    with open(STAMP, "w") as fh:
        fh.write("%f" % time.time())
    unreal.log("AIB22 gantry: placed %d bots on %s top z %.0f (footprint %.0fx%.0f)" % (
        len(bs), label, o.z + e.z, 2 * e.x, 2 * e.y))


def read(label):
    w = game_world()
    o, e = platform(w, label)
    try:
        t0 = float(open(STAMP).read())
    except Exception:
        t0 = time.time()
    dt = time.time() - t0
    still = 0
    for name, p in bots(w):
        loc = p.get_actor_location()
        on = (abs(loc.x - o.x) <= e.x + 50 and abs(loc.y - o.y) <= e.y + 50 and loc.z > o.z + e.z - 50)
        still += int(on)
        unreal.log("AIB22 gantry t+%.1fs %-28s z %7.0f  %s" % (dt, name, loc.z, "STILL ON" if on else "left"))
    unreal.log("AIB22 gantry t+%.1fs: %d still on %s -> box 4 %s" % (
        dt, still, label, "PASS" if still == 0 and dt >= 5 else ("pending" if dt < 5 else "FAIL")))


if __name__ == "__main__":
    mode, label = sys.argv[1], sys.argv[2]
    place(label) if mode == "--place" else read(label)
