#!/usr/bin/env python3
"""BN29 — AQUARIUS, one to one: the manifest, emitted by code.

    python3 Tools/blockout/gen_aquarius_manifest.py

Halo Infinite's Aquarius (343 Industries), recreated at blockout fidelity as a
STUDY level for the BREACHPOINT capstone. Topology is the documented record
(two research passes + the Majmudar analysis digest in TICKET_BN29): three
hallways + two open arenas — the center lane threading base -> courtyard ->
Bottom Mid -> courtyard -> base under the Top Mid bridge, Hydro linking the
Utility corners, Planters-under-Pump linking the Refrigeration corners — two
exits per room, side-hallway sightlines deliberately broken, indoor, small,
close range, exact mirror symmetry.

PROPORTIONS ARE DERIVED (the ticket's channel c): nobody publishes Aquarius'
dimensions and every image source is egress-blocked. Each number below states
its derivation; the manifest upgrades to traced (article figures) or measured
(in-game Forge walk) evidence without this file's structure changing.

Derivation base: an indoor close-range 4v4 map. BR75 optimal TTK holds to
~25-30 m; "close range" centers fights well under that. Courts ~11x14 m (a
court crossing = one sprint burst), the full long axis 44 m so the LONGEST
possible line (a broken-by-design side hall) never exceeds ~22 m clear, and
court-to-court through the center ~22 m — everything inside BREACHPOINT's
35 m cap with margin, which matches "mostly close range engagements".

Mirror law: x -> 44 - x through mirror(); the map is emitted symmetric, not
reviewed symmetric. Yellow = west, Blue = east; Hydro = north lane,
Planters = south lane (Refrigeration LEFT leaving Yellow spawn, per the
documented mirror rule).
"""

import json
from pathlib import Path

GAME = Path(__file__).resolve().parents[2]
OUT = GAME / "Content" / "Data" / "aquarius_manifest.json"

W, H = 44.0, 32.0          # long axis base-to-base; short axis lane-to-lane
LVL = 4.0                  # one storey: Top Mid / Pump / base decks at +4
CEIL = 9.0                 # indoor ceiling


def mx(x):
    return round(W - x, 3)


def fp(label, x0, x1, y0, y1, z0, z1, role):
    return {"label": label, "x": [round(x0, 3), round(x1, 3)], "y": [y0, y1],
            "z": [z0, z1], "role": role}


def mirror(f, label):
    return {"label": label, "x": sorted([mx(f["x"][0]), mx(f["x"][1])]),
            "y": f["y"], "z": f["z"], "role": f["role"] + " (mirror)"}


landmarks = []

# ---- Yellow Base (west) — a two-storey base room, two exits + court front ----
yb = [
    fp("yb_roof", 2, 10, 11, 21, 3.6, 4, "deck slab; the base's upper deck, overlooks the courtyard; tactical rifle sits just before the stairs (documented)"),
    fp("yb_back", 0.9, 2, 11, 21, 0, CEIL, "solid; base back wall against the perimeter"),
    fp("yb_wall_n", 2, 10, 21, 22, 0, 4, "solid; base north wall - the gap OUTSIDE it at x[10,11] is the north exit toward Utility"),
    fp("yb_wall_s", 2, 10, 10, 11, 0, 4, "solid; base south wall - mirror gap is the south exit toward Refrigeration"),
    fp("yb_stair", 3, 5.5, 12, 15.5, 0, 4, "ground-to-deck stair volume inside the base"),
]
landmarks.append({"name": "Yellow Base", "purpose":
    "West spawn room, two storeys: roof deck yb_roof x[2,10] y[11,21] z[3.6,4] on walls "
    "x[0.9,2] y[11,21] z[0,9] (back), x[2,10] y[21,22] z[0,4] and x[2,10] y[10,11] z[0,4] "
    "(sides), stair x[3,5.5] y[12,15.5] z[0,4] inside. Open FRONT to the courtyard plus "
    "north/south exits around the side walls - three ways out (the article's rule: "
    "every room >= 2 exits).", "footprints": yb})
landmarks.append({"name": "Blue Base", "purpose":
    "East spawn, exact x-mirror: roof x[34,42] y[11,21] z[3.6,4], back wall x[42,43.1] "
    "y[11,21] z[0,9], side walls x[34,42] y[21,22] z[0,4] and x[34,42] y[10,11] z[0,4], "
    "stair x[38.5,41] y[12,15.5] z[0,4].", "footprints":
    [mirror(f, f["label"].replace("yb_", "bb_")) for f in yb]})

# ---- The two open arenas (courtyards) — open floor, thruster-slot cover ------
landmarks.append({"name": "Courtyards", "purpose":
    "The two open arenas in front of the bases: yellow court is the open floor "
    "x[11,19] y[8,24] at ground, blue court its mirror x[25,33]. No structure of "
    "their own - the surrounding walls shape them; cover crates carry the "
    "documented thruster pads.", "footprints": [
        fp("court_marker_w", 14.4, 15.6, 15.4, 16.6, 0, 0.1,
           "deck slab; nav marker only - the yellow court's centroid, kept sub-knee so it plays as open floor"),
        fp("court_marker_e", 28.4, 29.6, 15.4, 16.6, 0, 0.1,
           "deck slab; nav marker only - blue court centroid"),
    ]})

# ---- The center: Top Mid over Bottom Mid ------------------------------------
landmarks.append({"name": "Top Mid", "purpose":
    "The elevated center bridge, the map's most important position: deck x[18,26] "
    "y[13.5,18.5] z[3.6,4] carried by tunnel walls x[18,26] y[18.5,19.5] z[0,3.6] and "
    "x[18,26] y[12.5,13.5] z[0,3.6]; BOTTOM MID is the open passage between them "
    "(y[13.5,18.5] at ground) - the one hallway that runs through both arenas. "
    "Court stairs x[14.5,18] y[14,18] z[0,4] (west) and x[26,29.5] y[14,18] z[0,4] "
    "(east) climb onto the bridge ends: the article's split-level junction - from "
    "either court you choose Bottom Mid (ground) or Top Mid (up), and the choice "
    "changes how the next fight is engaged. Power pickup at bridge center "
    "(BREACHPOINT adaptation: the rocket takes Aquarius' camo/OS slot).",
    "footprints": [
        fp("topmid_deck", 18, 26, 13.5, 18.5, 3.6, 4, "deck slab; the bridge - overlooks both courts, no cover on it"),
        fp("topmid_wall_n", 18, 26, 18.5, 19.5, 0, 3.6, "solid; Bottom Mid tunnel north wall"),
        fp("topmid_wall_s", 18, 26, 12.5, 13.5, 0, 3.6, "solid; Bottom Mid tunnel south wall"),
        fp("stair_court_w", 14.5, 18, 14, 18, 0, 4, "ground-to-mid stair volume; abuts the bridge at x=18"),
        fp("stair_court_e", 26, 29.5, 14, 18, 0, 4, "ground-to-mid stair volume; abuts the bridge at x=26"),
    ]})

# ---- North lane: Utility corners + Hydro (broken sightlines) -----------------
landmarks.append({"name": "Hydro", "purpose":
    "North side hallway linking Yellow Utility to Blue Utility along y[26,31]: "
    "inner wall segments x[11,13] y[25,26] z[0,4], x[15,29] y[25,26] z[0,4], "
    "x[31,33] y[25,26] z[0,4] leave a door into each court at x[13,15] and "
    "x[29,31]; columns x[17,19] y[27.5,29.5] z[0,4] and x[25,27] y[27.5,29.5] "
    "z[0,4] BREAK the hall's sightline into ~6 m reads (the article: the "
    "developers broke the side hallways up so players keep moving). Long-range "
    "pickup at the hall's exact center, on the floor.", "footprints": [
        fp("hydro_wall_w", 11, 13, 25, 26, 0, 4, "solid; hydro inner wall, west segment"),
        fp("hydro_wall_m", 15, 29, 25, 26, 0, 4, "solid; hydro inner wall, center segment"),
        fp("hydro_wall_e", 31, 33, 25, 26, 0, 4, "solid; hydro inner wall, east segment"),
        fp("hydro_col_w", 17, 19, 27.5, 29.5, 0, 4, "solid; sightline breaker column"),
        fp("hydro_col_e", 25, 27, 27.5, 29.5, 0, 4, "solid; sightline breaker column (mirror)"),
    ]})
landmarks.append({"name": "Utility Corners", "purpose":
    "The corner rooms on the Hydro side, one per base (sidearm racks): shaped by "
    "the base north walls and corner piers x[8,10] y[27,29] z[0,4] (yellow) and "
    "x[34,36] y[27,29] z[0,4] (blue) - each utility corner keeps two exits: into "
    "its base's north door and into Hydro.", "footprints": [
        fp("util_pier_w", 8, 10, 27, 29, 0, 4, "solid; yellow utility corner pier"),
        fp("util_pier_e", 34, 36, 27, 29, 0, 4, "solid; blue utility corner pier (mirror)"),
    ]})

# ---- South lane: Refrigeration corners + Planters under Pump -----------------
landmarks.append({"name": "Planters and Pump", "purpose":
    "South side hallway linking the Refrigeration corners along y[1,6], roofed at "
    "its middle by PUMP: slab x[18,26] y[1,6] z[3.6,4] on piers x[18,19.5] y[2,5] "
    "z[0,3.6] and x[24.5,26] y[2,5] z[0,3.6] (the piers double as the lane's "
    "sightline breakers); pump stair x[14.5,17.5] y[1.5,4.5] z[0,4] climbs the "
    "west end (its mirror climbs the east). Inner wall segments x[11,13] y[6,7] "
    "z[0,4], x[15,29] y[6,7] z[0,4], x[31,33] y[6,7] z[0,4] leave court doors at "
    "x[13,15] and x[29,31]. Shotgun-tier pickup beneath Pump; grenades on top.",
    "footprints": [
        fp("plant_wall_w", 11, 13, 6, 7, 0, 4, "solid; planters inner wall, west segment"),
        fp("plant_wall_m", 15, 29, 6, 7, 0, 4, "solid; planters inner wall, center segment"),
        fp("plant_wall_e", 31, 33, 6, 7, 0, 4, "solid; planters inner wall, east segment"),
        fp("pump_deck", 18, 26, 1, 6, 3.6, 4, "deck slab; Pump - the raised structure over Planters"),
        fp("pump_pier_w", 18, 19.5, 2, 5, 0, 3.6, "solid; pump pier + lane sightline breaker"),
        fp("pump_pier_e", 24.5, 26, 2, 5, 0, 3.6, "solid; pump pier + lane sightline breaker (mirror)"),
        fp("pump_stair_w", 14.5, 17.5, 1.5, 4.5, 0, 4, "ground-to-mid stair volume; abuts Pump at x=18"),
        fp("pump_stair_e", 26.5, 29.5, 1.5, 4.5, 0, 4, "ground-to-mid stair volume; abuts Pump at x=26 (mirror)"),
        fp("refrig_pier_w", 8, 10, 3, 5, 0, 4, "solid; yellow refrigeration corner pier"),
        fp("refrig_pier_e", 34, 36, 3, 5, 0, 4, "solid; blue refrigeration corner pier (mirror)"),
    ]})

# ---- spawns: 2 per base + 4 court neutrals, mirrored, all >= 8 m apart -------
# Base pairs sit on the room diagonal (the room is 8x10 m - a straight pair
# cannot make the 8 m gate; the diagonal makes 10.4 m); court neutrals sit at
# the courts' mid-facing corners, >= 8 m from every base point.
spawns = [
    {"id": "SP1", "location": {"x": 9, "y": 12, "z": 0}, "facing": 0, "pool": "team_a"},
    {"id": "SP2", "location": {"x": 2.7, "y": 20.3, "z": 0}, "facing": 0, "pool": "team_a"},
    {"id": "SP3", "location": {"x": mx(2.7), "y": 20.3, "z": 0}, "facing": 180, "pool": "team_b"},
    {"id": "SP4", "location": {"x": mx(9), "y": 12, "z": 0}, "facing": 180, "pool": "team_b"},
    {"id": "SP5", "location": {"x": 17, "y": 9.2, "z": 0}, "facing": 45, "pool": "neutral"},
    {"id": "SP6", "location": {"x": 17, "y": 22.8, "z": 0}, "facing": -45, "pool": "neutral"},
    {"id": "SP7", "location": {"x": mx(17), "y": 22.8, "z": 0}, "facing": -135, "pool": "neutral"},
    {"id": "SP8", "location": {"x": mx(17), "y": 9.2, "z": 0}, "facing": 135, "pool": "neutral"},
]
for s in spawns:
    s["scoring_hints"] = {"min_dist_to_combat_m": 12, "last_used_cooldown_s": 8}

# ---- grapple anchors (BREACHPOINT adaptation: hook mobility onto the decks) --
gps = [
    {"id": "GP1", "location": {"x": 18, "y": 16, "z": 4}, "serves": "Top Mid (west lip)", "approach": "south ground of the yellow court", "notes": "west lip of topmid_deck"},
    {"id": "GP2", "location": {"x": 26, "y": 16, "z": 4}, "serves": "Top Mid (east lip)", "approach": "south ground of the blue court", "notes": "east lip of topmid_deck"},
    {"id": "GP3", "location": {"x": 19, "y": 6, "z": 4}, "serves": "Pump (west lip)", "approach": "north ground lane", "notes": "north-west lip of pump_deck"},
    {"id": "GP4", "location": {"x": 25, "y": 6, "z": 4}, "serves": "Pump (east lip)", "approach": "north ground lane", "notes": "north-east lip of pump_deck"},
    {"id": "GP5", "location": {"x": 10, "y": 16, "z": 4}, "serves": "Yellow Base deck", "approach": "east ground (the court)", "notes": "front lip of yb_roof"},
    {"id": "GP6", "location": {"x": 34, "y": 16, "z": 4}, "serves": "Blue Base deck", "approach": "west ground (the court)", "notes": "front lip of bb_roof (mirror)"},
]

# ---- cover: the courts' thruster-pad crates + hall crates, mirrored ----------
cover = [
    {"location": {"x": 13.5, "y": 12, "z": 0}, "height_class": "chest"},
    {"location": {"x": mx(13.5), "y": 12, "z": 0}, "height_class": "chest"},
    {"location": {"x": 13.5, "y": 20, "z": 0}, "height_class": "chest"},
    {"location": {"x": mx(13.5), "y": 20, "z": 0}, "height_class": "chest"},
    {"location": {"x": 22, "y": 29.8, "z": 0}, "height_class": "chest"},
    {"location": {"x": 22, "y": 21.5, "z": 0}, "height_class": "full"},
    {"location": {"x": 22, "y": 10.5, "z": 0}, "height_class": "full"},
]

manifest = {
    "arena_id": "breachpoint_aquarius",
    "manifest_version": 1,
    "units": "meters",
    "schema_note": "Same schema as arena_manifest.json; footprints[] authoritative; "
                   "EMITTED by Tools/blockout/gen_aquarius_manifest.py - edit that. "
                   "STUDY RECREATION of 343 Industries' Aquarius (Halo Infinite) at "
                   "blockout fidelity; proportions DERIVED pending traced/measured "
                   "evidence (TICKET_BN29 channels a/b).",
    "bounds": {"x": W, "y": H, "z": CEIL + 1},
    "rocket_node": {"x": 22, "y": 16, "z": 4, "elevation": "mid", "landmark": "Top Mid",
                    "notes": "z=4 is topmid_deck's walk surface, bridge center - Aquarius' "
                             "camo/OS slot, taken by BREACHPOINT's one power pickup "
                             "(adaptation, TICKET_BN29). Overlooks both courts; timing "
                             "stays BP09's DT_Weapons row."},
    "spawn_points": spawns,
    "spawn_pools_note": "2 per base room + the four scored court neutrals, mirrored; "
                        "base spawns face their open front.",
    "landmarks": landmarks,
    "grapple_points": gps,
    "grapple_note": "BREACHPOINT adaptation: Aquarius serves thruster/clamber mobility; "
                    "our Grappleshot (20 m, GDD 2.5) stands in. Every deck lip carries "
                    "an anchor with a distinct ground approach; rises are one storey "
                    "(4 m) so the stairs remain the primary route - the hook is the "
                    "flank option, exactly the article's split-level junction doctrine.",
    "cover": cover,
    "cover_note": "Chest crates at the courts' documented thruster pads and mid-Hydro; "
                  "full crates north and south of the Top Mid block so the court-to-"
                  "court ground lines break outside the tunnel. Mirrored in x.",
    "sightlines": {
        "max_length_m": 35,
        "notes": "Arena01 slab-test semantics. Longest designed lines: court-to-court "
                 "through Bottom Mid ~=22 m; a side hall read between breakers ~=6-10 m "
                 "(the article: side-hall sightlines deliberately broken); base-to-base "
                 "blocked by the court stair volumes and the Top Mid block. Everything "
                 "sits inside the GDD 35 m cap - 'mostly close range engagements' "
                 "(Majmudar) and our cap agree; the feared 35 m ruling is NOT needed."
    },
    "hazards": [
        "Top Mid carries no cover and is overlooked from nowhere - holding it after the grab is the trade (the documented 'most important position, exposed').",
        "Bottom Mid is a blind 8 m tunnel with door-fights at both mouths.",
        "Pump's top is reachable by two stairs and the hook; its plasma-grenade rack makes it a contested perch."
    ],
    "doubts": [
        "All proportions are DERIVED (channel c) - the article's figures and the real map's Forge measurements upgrade them (TICKET_BN29 a/b).",
        "Aquarius' real base interiors have a richer two-storey layout than one stair volume; blockout fidelity accepts this until channel b measures them.",
        "Utility/Refrigeration corner rooms are shaped here by piers rather than full room shells; if traced figures show enclosed rooms, the generator grows walls."
    ],
    "landing_note": "STUDY RECREATION: Halo Infinite's Aquarius as BREACHPOINT's arena "
                    "(TICKET_BN29, founder directive 29 Aug). Skeleton per Majmudar: "
                    "three hallways, two arenas, split-level junctions, broken side-hall "
                    "sightlines, two exits per room."
}

OUT.write_text(json.dumps(manifest, indent=1), encoding="utf-8")
print("wrote %s (%d landmarks, %d spawns, %d anchors, %d cover)" %
      (OUT.relative_to(GAME), len(landmarks), len(spawns), len(gps), len(cover)))
