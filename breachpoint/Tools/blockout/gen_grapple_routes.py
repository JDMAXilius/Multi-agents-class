#!/usr/bin/env python3
"""AIB19 — generate the bot grapple-route table from the arena manifest.

    python3 Tools/blockout/gen_grapple_routes.py          # rewrite the ini block
    python3 Tools/blockout/gen_grapple_routes.py --check  # derive + print, write nothing

Reads `Content/Data/arena_manifest.json` `grapple_points[]` (the single source of
truth — the blockout profile's own invariant is that the metre->centimetre
conversion happens at exactly ONE committed boundary, and for these routes this
script is that boundary) and writes `[/Script/BreachpointNext.BNAIBWorldQuery]`
GrappleRoutes entries into `Config/DefaultGame.ini`, between markers it owns.
Rerun whenever the manifest's grapple points change. Nothing reads JSON at
runtime; nothing here is hand-typed tuning.

Derivation, per point (constants below are the LIVE-TUNE surface — the ticket's
watch-list: if bots stall at a lip, retune HERE and rerun, the C++ needs nothing):

- outward direction: the `approach` field's leading token — "south" backs off -Y,
  "north" +Y. Anything else refuses loudly; this script invents no geometry.
- approach height: "ground" -> z 0; "terrace"/"deck"/"roof" -> z 4 (the mid
  level). Same refusal rule.
- stand-off: ground shots back off min(rise, STANDOFF_CAP_M) for a shallow line
  to the lip corner; deck shots get the fixed DECK_STANDOFF_M the decks have
  room for.
- the aim point is the manifest's anchor VERBATIM — the authored lip corner.
"""

import argparse
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
GAME = HERE.parents[1]
MANIFEST = GAME / "Content" / "Data" / "arena_manifest.json"
INI = GAME / "Config" / "DefaultGame.ini"

M_TO_CM = 100.0
SECTION = "[/Script/BreachpointNext.BNAIBWorldQuery]"
BEGIN = "; BEGIN gen_grapple_routes (generated - do not hand-edit; rerun Tools/blockout/gen_grapple_routes.py)"
END = "; END gen_grapple_routes"

# The live-tune surface (see module docstring).
STANDOFF_CAP_M = 8.0      # ground shots: how far back the bot stands, at most
DECK_STANDOFF_M = 3.5     # deck shots: the fixed back-off the mid decks have room for
MID_LEVEL_Z_M = 4.0       # the arena's mid level (manifest: decks/terrace/roof at 4)

DIRS = {"south": -1.0, "north": +1.0}
GROUND_WORDS = ("ground",)
DECK_WORDS = ("terrace", "deck", "roof")


def derive(gp: dict) -> tuple[dict, str]:
    gid, loc, approach = gp["id"], gp["location"], (gp.get("approach") or "").lower()
    lead = re.match(r"\s*(\w+)", approach)
    if not lead or lead.group(1) not in DIRS:
        sys.exit(f"error: {gid}: approach {approach!r} does not lead with south/north — "
                 f"this script invents no geometry; fix the manifest or teach the rule")
    dir_y = DIRS[lead.group(1)]

    if any(w in approach for w in GROUND_WORDS):
        az = 0.0
    elif any(w in approach for w in DECK_WORDS):
        az = MID_LEVEL_Z_M
    else:
        sys.exit(f"error: {gid}: approach {approach!r} names no known surface "
                 f"(ground/terrace/deck/roof) — refusing to guess a height")

    rise = float(loc["z"]) - az
    if rise <= 0:
        sys.exit(f"error: {gid}: anchor z {loc['z']} is not above its approach z {az}")
    standoff = DECK_STANDOFF_M if az > 0 else min(rise, STANDOFF_CAP_M)

    route = {
        "approach": (float(loc["x"]), float(loc["y"]) + dir_y * standoff, az),
        "anchor": (float(loc["x"]), float(loc["y"]), float(loc["z"])),
    }
    why = (f"{gid}: {lead.group(1)} stand-off {standoff:g} m at z {az:g}, "
           f"rise {rise:g} m -> anchor ({loc['x']}, {loc['y']}, {loc['z']})")
    return route, why


def vec(m):
    return "(X=%.1f,Y=%.1f,Z=%.1f)" % (m[0] * M_TO_CM, m[1] * M_TO_CM, m[2] * M_TO_CM)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="derive and print, write nothing")
    args = ap.parse_args()

    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    points = manifest.get("grapple_points") or []
    if not points:
        sys.exit("error: the manifest has no grapple_points[] — nothing to generate")

    lines = [BEGIN,
             f"; {len(points)} routes from arena_manifest.json grapple_points[] "
             f"(standoff cap {STANDOFF_CAP_M:g} m ground / {DECK_STANDOFF_M:g} m deck)",
             "!GrappleRoutes=ClearArray"]
    for gp in points:
        route, why = derive(gp)
        print("  " + why)
        lines.append(f"; {why}")
        lines.append(f"+GrappleRoutes=(Approach={vec(route['approach'])},Anchor={vec(route['anchor'])})")
    lines.append(END)
    block = "\n".join(lines)

    if args.check:
        print("\n" + block)
        return

    ini = INI.read_text(encoding="utf-8")
    if BEGIN in ini:
        ini = re.sub(re.escape(BEGIN) + r".*?" + re.escape(END), block, ini, flags=re.S)
    elif SECTION in ini:
        ini = ini.replace(SECTION, SECTION + "\n" + block, 1)
    else:
        ini = ini.rstrip() + "\n\n" + SECTION + "\n" + block + "\n"
    INI.write_text(ini, encoding="utf-8")
    print(f"wrote {len(points)} routes into {INI.relative_to(GAME)} under {SECTION}")


if __name__ == "__main__":
    main()
