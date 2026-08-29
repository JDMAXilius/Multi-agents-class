#!/usr/bin/env python3
"""BN28 — Arena 02 "THE LOCKS": the manifest, emitted by code so the symmetry is
auditable instead of hand-copied.

    python3 Tools/blockout/gen_arena02_manifest.py   # writes Content/Data/arena02_manifest.json

The design is the Halo Infinite research synthesis (ticket BN28's digest,
docs/design/HALO-INFINITE-RESEARCH.md), translated into BREACHPOINT's own laws:

- AQUARIUS plan: a mirror-symmetric rectangle — two team bays, two open
  courtyards, THREE lanes between them (one center, two flanks), each flank a
  different character (research rule 5; the flanks differ, the mirror is across
  the short axis only, which is exactly Aquarius' shape).
- RECHARGE section: THREE floors with a circulation ring that changes level as
  it goes — stairs carry L0->L1, nothing walkable reaches L2 (grapple-primary,
  GDD 2.6), and the center is a three-story sandwich: culvert UNDER the rocket
  deck UNDER the crossing (research: Aquarius' Top Mid / Bottom Mid sandwich,
  upgraded one floor by Recharge's pattern).
- STREETS sightline discipline: ONE deliberate long lane (the Gallery, 26 m),
  everything else broken under the GDD's 35 m cap by construction (rule 7).
- Power position pays in exposure (rule 8): the Crossing sees both courtyards
  and has no cover. The rocket sits at map center, south lip of the deck, so
  all three levels see the contest (GDD 2.6; Arena01's own precedent).

Mirror law: every element is either emitted through mirror() (x -> 56 - x) or
self-symmetric. Team fairness is enforced by construction, not by review.
"""

import json
from pathlib import Path

GAME = Path(__file__).resolve().parents[2]
OUT = GAME / "Content" / "Data" / "arena02_manifest.json"

W = 56.0   # x span (long axis, base to base)
H = 40.0   # y span


def mx(x):
    return round(W - x, 3)


def fp(label, x0, x1, y0, y1, z0, z1, role):
    return {"label": label, "x": [x0, x1], "y": [y0, y1], "z": [z0, z1], "role": role}


def mirror_fp(f, label):
    return {"label": label, "x": sorted([mx(f["x"][0]), mx(f["x"][1])]),
            "y": f["y"], "z": f["z"], "role": f["role"] + " (mirror of %s)" % f["label"]}


landmarks = []

# ---- Home West / Home East — the team bays (Aquarius bases: sheltered, multi-exit)
hw = [
    fp("canopy_w", 2, 10, 15, 25, 3.6, 4, "deck slab; spawn-bay roof, cover from the Crossing; not stair-served"),
    fp("pier_bay_sw", 3, 5, 16, 18, 0, 3.6, "solid; canopy pier, shapes the bay's south exit"),
    fp("pier_bay_nw", 3, 5, 22, 24, 0, 3.6, "solid; canopy pier, shapes the bay's north exit"),
]
landmarks.append({"name": "Home West", "purpose":
    "Team A bay: canopy_w x[2,10] y[15,25] z[3.6,4] on two piers, open to the east "
    "courtyard and around both piers - three ways out, no single-exit spawn room "
    "(research rule 11).", "footprints": hw})
landmarks.append({"name": "Home East", "purpose":
    "Team B bay, the exact x-mirror of Home West: canopy_e x[46,54] y[15,25] "
    "z[3.6,4] on piers x[51,53] y[16,18] z[0,3.6] and x[51,53] y[22,24] z[0,3.6] - "
    "three ways out, same as the west bay.", "footprints":
    [mirror_fp(f, f["label"].replace("_w", "_e").replace("_sw", "_se").replace("_nw", "_ne")) for f in hw]})

# ---- The Locks — the center three-story sandwich (self-symmetric in x)
landmarks.append({"name": "The Locks", "purpose":
    "Center block, the three-story sandwich: the Culvert (open passage y[17,23] at "
    "ground between the piers, baffled mid-length so no line threads it), the rocket "
    "deck at z4 spanning both piers, stairs carrying ground to deck on both sides. "
    "pier_south x[22,34] y[14,17] z[0,3.6]; pier_north x[22,34] y[23,26] z[0,3.6]; "
    "deck x[22,34] y[14,26] z[3.6,4]; baffle x[27,29] y[18.5,21.5] z[0,3.6]; "
    "stair_west x[18,22] y[18,22] z[0,4] climbs east; stair_east x[34,38] y[18,22] "
    "z[0,4] climbs west. Rocket node stands on the deck's south lip, map center in x.",
    "footprints": [
        fp("pier_south", 22, 34, 14, 17, 0, 3.6, "solid; carries the deck; walls the culvert's south side"),
        fp("pier_north", 22, 34, 23, 26, 0, 3.6, "solid; carries the deck; walls the culvert's north side"),
        fp("culvert_baffle", 27, 29, 18.5, 21.5, 0, 3.6, "solid; mid-span pier - kills the base-to-base line through the culvert and splits it into two S-lanes"),
        fp("rocket_deck", 22, 34, 14, 26, 3.6, 4, "deck slab; the rocket terrace (L1), the contested band"),
        fp("stair_west", 18, 22, 18, 22, 0, 4, "ground-to-mid stair volume; abuts the deck at x=22"),
        fp("stair_east", 34, 38, 18, 22, 0, 4, "ground-to-mid stair volume; abuts the deck at x=34"),
    ]})

# ---- The Gallery — the south lane: the ONE deliberate long sightline (26 m clear)
landmarks.append({"name": "The Gallery", "purpose":
    "South lane, the Magnum duel: colonnade_gw x[12,26] y[7,8] z[0,4] and colonnade_ge "
    "x[30,44] y[7,8] z[0,4] wall it from the courtyards, one 4 m door at map center. "
    "Two full-height breakers x[13,15] y[2,5] z[0,3] and x[41,43] y[2,5] z[0,3] cap "
    "the clear line at 26 m - the arena's one deliberate long lane, under the GDD's "
    "35 m by design, chest cover mid-lane for the duel.",
    "footprints": [
        fp("colonnade_gw", 12, 26, 7, 8, 0, 4, "solid; gallery inner wall, west half"),
        fp("colonnade_ge", 30, 44, 7, 8, 0, 4, "solid; gallery inner wall, east half (mirror of colonnade_gw)"),
        fp("breaker_gw", 13, 15, 2, 5, 0, 3, "solid; sightline breaker - the lane's west end cap"),
        fp("breaker_ge", 41, 43, 2, 5, 0, 3, "solid; sightline breaker - the lane's east end cap (mirror)"),
    ]})

# ---- The Works — the north lane: broken CQB under a roof (the flanks DIFFER on purpose)
landmarks.append({"name": "The Works", "purpose":
    "North lane, the AR/melee flank - deliberately NOT the Gallery's shape (Aquarius: "
    "flanks differ, mirror is across the short axis only). colonnade_ww x[12,18] "
    "y[32,33] z[0,4], colonnade_wm x[22,34] y[32,33] z[0,4], colonnade_we x[38,44] "
    "y[32,33] z[0,4] leave two offset 4 m doors; the Rack x[24,32] y[33,40] z[3.6,4] "
    "roofs the lane's middle - a low dark pocket (Aquarius' Planters-under-Pump).",
    "footprints": [
        fp("colonnade_ww", 12, 18, 32, 33, 0, 4, "solid; works inner wall, west segment"),
        fp("colonnade_wm", 22, 34, 32, 33, 0, 4, "solid; works inner wall, center segment"),
        fp("colonnade_we", 38, 44, 32, 33, 0, 4, "solid; works inner wall, east segment (mirror of west)"),
        fp("rack_roof", 24, 32, 33, 40, 3.6, 4, "deck slab; roofs the works' middle pocket; reachable only by grapple"),
    ]})

# ---- The Crossing — L2, grapple-only (Recharge's bridges + Aquarius' Top Mid)
landmarks.append({"name": "The Crossing", "purpose":
    "Upper level (z8), grapple-only by design (GDD 2.6): catwalk_south x[20,36] "
    "y[10,13] z[7.6,8], catwalk_north x[20,36] y[27,30] z[7.6,8], and the top-mid "
    "bridge x[26,30] y[13,27] z[7.6,8] joining them over the rocket deck - an H in "
    "plan. Sees both courtyards and the rocket; carries NO cover; open on every edge "
    "(research rule 8: height buys information, not safety). No walkable structure "
    "reaches z8; every span carries two grapple anchors on distinct approaches.",
    "footprints": [
        fp("catwalk_south", 20, 36, 10, 13, 7.6, 8, "deck slab; south span; intentionally unconnected to walkable structure"),
        fp("catwalk_north", 20, 36, 27, 30, 7.6, 8, "deck slab; north span; intentionally unconnected to walkable structure"),
        fp("bridge_topmid", 26, 30, 13, 27, 7.6, 8, "deck slab; the H's crossbar over the rocket deck - the power position"),
    ]})

# ---- spawns: 2 team bays + 4 scored neutrals, mirrored, all pairs >= 8 m apart
spawns = [
    {"id": "SP1", "location": {"x": 5, "y": 15.5, "z": 0}, "facing": 0, "pool": "team_a"},
    {"id": "SP2", "location": {"x": 5, "y": 24.5, "z": 0}, "facing": 0, "pool": "team_a"},
    {"id": "SP3", "location": {"x": mx(5), "y": 24.5, "z": 0}, "facing": 180, "pool": "team_b"},
    {"id": "SP4", "location": {"x": mx(5), "y": 15.5, "z": 0}, "facing": 180, "pool": "team_b"},
    {"id": "SP5", "location": {"x": 13, "y": 10, "z": 0}, "facing": 45, "pool": "neutral"},
    {"id": "SP6", "location": {"x": 13, "y": 30, "z": 0}, "facing": -45, "pool": "neutral"},
    {"id": "SP7", "location": {"x": mx(13), "y": 30, "z": 0}, "facing": -135, "pool": "neutral"},
    {"id": "SP8", "location": {"x": mx(13), "y": 10, "z": 0}, "facing": 135, "pool": "neutral"},
]
for s in spawns:
    s["scoring_hints"] = {"min_dist_to_combat_m": 15, "last_used_cooldown_s": 8}

# ---- grapple points: >= 2 per upper span, distinct approaches; + deck fast-attack lips
gps = [
    {"id": "GP1", "location": {"x": 22, "y": 11.5, "z": 8}, "serves": "The Crossing (south span)", "approach": "south courtyard-west ground", "notes": "west lip of catwalk_south"},
    {"id": "GP2", "location": {"x": 34, "y": 11.5, "z": 8}, "serves": "The Crossing (south span)", "approach": "south courtyard-east ground", "notes": "east lip of catwalk_south"},
    {"id": "GP3", "location": {"x": 22, "y": 28.5, "z": 8}, "serves": "The Crossing (north span)", "approach": "north courtyard-west ground", "notes": "west lip of catwalk_north"},
    {"id": "GP4", "location": {"x": 34, "y": 28.5, "z": 8}, "serves": "The Crossing (north span)", "approach": "north courtyard-east ground", "notes": "east lip of catwalk_north"},
    {"id": "GP5", "location": {"x": 28, "y": 14, "z": 8}, "serves": "The Crossing (top-mid bridge)", "approach": "south deck / rocket terrace", "notes": "south lip of bridge_topmid - the deck-to-power-position tax (rise 4 m, grapple-only)"},
    {"id": "GP6", "location": {"x": 28, "y": 26, "z": 8}, "serves": "The Crossing (top-mid bridge)", "approach": "north deck / rocket terrace", "notes": "north lip of bridge_topmid"},
    {"id": "GP7", "location": {"x": 24, "y": 14, "z": 4}, "serves": "rocket deck (south lip)", "approach": "south ground", "notes": "fast attack onto the rocket, west lip"},
    {"id": "GP8", "location": {"x": 32, "y": 14, "z": 4}, "serves": "rocket deck (south lip)", "approach": "south ground", "notes": "fast attack onto the rocket, east lip"},
    {"id": "GP9", "location": {"x": 24, "y": 26, "z": 4}, "serves": "rocket deck (north lip)", "approach": "north ground", "notes": "mirror of GP7"},
    {"id": "GP10", "location": {"x": 32, "y": 26, "z": 4}, "serves": "rocket deck (north lip)", "approach": "north ground", "notes": "mirror of GP8"},
]

# ---- cover: courtyard street-breakers (full) + duel/pocket crates (chest), mirrored
cover = []
for x, y, hc in [(16, 11, "full"), (16, 29, "full"), (13, 20, "chest"),
                 (24, 3.5, "chest"), (26, 36, "chest"), (18, 20.0, None)]:
    pass
cover = [
    {"location": {"x": 16, "y": 11, "z": 0}, "height_class": "full"},
    {"location": {"x": mx(16), "y": 11, "z": 0}, "height_class": "full"},
    {"location": {"x": 16, "y": 29, "z": 0}, "height_class": "full"},
    {"location": {"x": mx(16), "y": 29, "z": 0}, "height_class": "full"},
    {"location": {"x": 13, "y": 20, "z": 0}, "height_class": "chest"},
    {"location": {"x": mx(13), "y": 20, "z": 0}, "height_class": "chest"},
    {"location": {"x": 24, "y": 3.5, "z": 0}, "height_class": "chest"},
    {"location": {"x": mx(24), "y": 3.5, "z": 0}, "height_class": "chest"},
    {"location": {"x": 26, "y": 36, "z": 0}, "height_class": "chest"},
    {"location": {"x": mx(26), "y": 36, "z": 0}, "height_class": "chest"},
]

manifest = {
    "arena_id": "breachpoint_vs02",
    "manifest_version": 1,
    "units": "meters",
    "schema_note": "Same schema as arena_manifest.json (BR_Arena01): footprints[] "
                   "(structured min/max extents) are AUTHORITATIVE for landmark geometry; "
                   "the x[..] y[..] z[..] tokens in purpose prose are a readable projection. "
                   "EMITTED by Tools/blockout/gen_arena02_manifest.py - edit that, not this.",
    "bounds": {"x": W, "y": H, "z": 12},
    "rocket_node": {"x": 28, "y": 15.5, "z": 4, "elevation": "mid", "landmark": "The Locks",
                    "notes": "z=4 is the rocket deck's walk surface, on its SOUTH lip at map "
                             "center in x - equidistant from both bays, visible from the south "
                             "courtyard floor (L0), the deck itself (L1) and every Crossing span "
                             "(L2), per GDD 2.6. Respawn timing stays BP09's DT_Weapons row."},
    "spawn_points": spawns,
    "spawn_pools_note": "team_a/team_b bays under the canopies; SP5-SP8 are the four scored "
                        "neutral spawns (GDD 2.6), courtyard corners facing mid, outside all "
                        "three lanes' long lines.",
    "landmarks": landmarks,
    "grapple_points": gps,
    "grapple_note": "Grappleshot range is 20 m (GDD 2.5 / Appendix A). Crossing anchors rise "
                    "8 m from their ground approaches at 4-6 m horizontal stand-off (about "
                    "9-10 m 3D); bridge anchors rise 4 m from the deck. Every span carries two "
                    "anchors on distinct approaches - no single-point choke (GDD 2.6). The "
                    "Halo-metric ratio informing the heights: jump 8 : clamber 12 : grapple 80 "
                    "units (343's own Forge guidance) - our 4 m floors sit above BN's jump, "
                    "inside one grapple.",
    "cover": cover,
    "cover_note": "Full-height pairs break the two east-west courtyard streets under 35 m; "
                  "chest crates arm the Gallery duel and the Works pocket. All mirrored in x.",
    "sightlines": {
        "max_length_m": 35,
        "notes": "Static slab test semantics as Arena01 (both endpoints +1.7 m eye). Longest "
                 "designed clear line: the Gallery between its breakers, 26 m. The culvert is "
                 "baffled mid-span (no base-to-base thread); the courtyard streets are broken "
                 "by the full-height cover pairs (13 m / 22 m / 13 m segments); the north-south "
                 "center line dies on pier_south at y=14. L2-to-ground diagonals approach but "
                 "stay under the cap (longest computed 34.1 m, Crossing end to far courtyard "
                 "corner) - the slab test rules."
    },
    "hazards": [
        "The Crossing has no railing on any edge - an 8 m fall to ground or a 4 m drop to the deck is possible everywhere on it.",
        "The Culvert is blind at both baffle corners - melee territory by design.",
        "The rocket deck is overlooked by all three Crossing spans; holding the pad after the grab is the risk the contest sells."
    ],
    "doubts": [
        "The 34.1 m Crossing-to-far-corner diagonal is 0.9 m under the cap on paper; the "
        "slab test must confirm no clear line exceeds it once real geometry stands.",
        "The Works' Rack pocket may read too dark against the courtyard without a light "
        "pass - flag for the lighting profile, not for geometry.",
        "Canopy roofs (z4) are reachable by grapple though not stair-served - intended "
        "(a home high-spot costs the hook), but watch for spawn-camping from atop them."
    ],
    "landing_note": "Derived arena: THE LOCKS. Aquarius plan x Recharge section, Streets "
                    "sightline discipline, BREACHPOINT laws (35 m cap, 3 levels, grapple-only "
                    "upper, rocket at mid visible from all three). Design digest: "
                    "docs/design/HALO-INFINITE-RESEARCH.md; ticket BN28."
}

OUT.write_text(json.dumps(manifest, indent=1), encoding="utf-8")
print("wrote %s (%d landmarks, %d spawns, %d grapple points, %d cover)" %
      (OUT.relative_to(GAME), len(landmarks), len(spawns), len(gps), len(cover)))
