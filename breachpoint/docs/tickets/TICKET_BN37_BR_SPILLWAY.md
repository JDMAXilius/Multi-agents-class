# TICKET — BN37: BR_Spillway, the shooter template remade as a 4v4 competitive arena

> STATUS: REV D BUILT AND SAVED — terminal, 31 Aug 2026. Live in the editor, not yet playtested.

> OWNER: **terminal**. Founder directive, 31 Aug 2026: *"use the
> `/Game/Variant_Shooter/Lvl_Shooter` level assets … duplicate this and re-make the level
> and reinvent it for our own project … a new level, competitive, fun to play, that we can
> showcase everything on — our project skills, actions, gameplay, AI … at least double the
> size, with some parts height and width differences."*

## What landed (REV D)

`/Game/Maps/BR_Spillway` — **80 × 60 m footprint, −4 m to +16 m tall, 204 generated
elements, 8 PlayerStarts, 1 power-weapon node, a resized nav volume, `BP_BNGameMode` in
World Settings.** Nothing hand-placed; the map is the projection of a committed generator.

| | Lvl_Shooter (measured, live editor) | rev A | BR_Spillway rev B |
| --- | --- | --- | --- |
| playable footprint | 30.0 × 36.0 m — 1,080 m² | 2,880 m² | 80.0 × 60.0 m — **4,800 m² (4.44×)** |
| height range | 8.0 m | 16.0 m | **20.0 m** (−4 m to +16 m) |
| walkable tiers | ~2, ad hoc | 4 | **5** — z −400 / 0 / 400 / 800 / 1200 uu |
| main ramp width | — | 4 m | **6 m** (all 16 ramps) |
| geometry actors | 97 | 137 | 204 |
| spawns | 6 | 8 | 8 (4 west, 4 east) |

## REV D — bigger, squarer, and no dead mass

> Founder, 31 Aug 2026: *"a bit more space on tight places and less big square with no
> path on it. make the entire map a bit bigger in overall size, square."*

| | rev C | **rev D** |
| --- | --- | --- |
| footprint | 80 × 60 m — 4,800 m² | **88 × 80 m — 7,040 m² (6.5× the template)** |
| aspect | 1.33 : 1 | **1.10 : 1** |
| culvert headroom | 3 m | **4 m** (floor dropped to −5 m) |
| tower | 24 × 24 m, solid core | **28 × 28 m, core is a ROOM** |
| ground open area | 81% | **87%** |
| worst-case detour | 1.71× | 1.97× |
| elements | 167 | 179 |

**No big square with no path on it.** Every mass over ~4 m now either carries a route on
top or is enterable:

- **The tower core** was a 12 × 12 m solid cube — the worst offender. It is now a 14 × 14 m
  **room** with 5 m doorways north and south, and the shaft down to the culvert is in its
  floor. Its east and west faces stay solid, which is what kills the cross-map sightline;
  the doorways face the axis that was never the problem.
- **The lane's centre mass** (10 × 8 × 8 m of nothing) is now **the blockhouse**: walls with
  two doorways at ground level, a walkable roof at tier 1, and a ramp onto it. Same job —
  it breaks the long shot — but it is a position instead of an obstacle.
- **The two lane side masses** are **plinths whose tops join the shelf**, so they carry the
  flank route rather than interrupting it.
- **The yard masses** are **platforms**: solid to tier 1 with their own ramp on. They have
  to be tall enough to stand across the ground spawns' line, and anything that tall has to
  be worth climbing.
- The tower's two remaining solid quadrants are not dead either — the tier-1 deck is their
  top surface.

**Spawns are staggered instead of walled apart.** Rev C put both teams at matching Y, which
makes a straight east-west line that only a wall can block — and walls are what made the
map a maze in the first place. No cross-team pair shares a Y now, so every spawn line runs
diagonally through the tower, and the geometry that stops it is the hub you were going to
fight over anyway.

**Loose props place themselves.** Crates were hand-positioned for three revisions and every
layout change started a round of whack-a-mole with the overlap gate. `crate_if_clear()`
tests the spot first and skips it otherwise, reporting how many it dropped. Cover density
is a nice-to-have; a crate fused into a ramp is a defect.

## REV C — the flow pass, and why rev B was wrong

> Founder, 31 Aug 2026: *"you make it worse! think about flow, empty space so it can flow
> the fight, and have different paths."*

Correct, and the diagnosis is specific. Rev B kept **adding** — a gated cross-wall at
y = −14 m and another at y = +14 m, a walled spawn room per team, and a gallery of four
sealed cells. That is three bands joined by three doorways each: connected on paper,
a queue in practice. Rev C removes mass rather than adding it.

| | rev B | **rev C** |
| --- | --- | --- |
| elements | 204 | **167** |
| ground open area | 78% | **81%** |
| ground regions | 1 | 1 |
| **worst-case detour** | **2.55×** | **1.71×** |
| p90 detour | 1.20× | **1.15×** |

What came out: both cross-walls (8 actors), both bases' front and side walls (10), the
gallery's spine and cell walls (16), two of the tower's four ground-floor corner blocks,
and roughly half the loose cover. What went in: pillars. The bases are canopies on six
columns with the whole front open; the gallery is a hypostyle hall you can cross in any
direction; the tower's ground floor is a colonnade around a solid core, so circulation
runs *through* the middle of the map and only the core stops the shot.

Sightlines are now broken by **mass standing in open ground** — islands you fight around —
instead of walls you file through. The spawn-sightline gate still passes with every wall
gone, which is the point: the walls were never what was making it fair.

**Rev B answers the founder's 31 Aug note** — *"keep in mind the space you may need to make
it a bit bigger and enough space on areas. like ramps, stairs, underground, etc."*
Envelope 60 × 48 → 80 × 60 m; every main ramp 4 → 6 m; gallery corridors 4 → 6 m; tower
20 → 24 m; and a fifth tier **below ground**.

## THE CULVERT — the underground tier

Floor at −4 m, ceiling at −1 m: **3 m of headroom, deliberately the tightest space in the
arena** and the contrast the 16 m open lane needs.

- **Chamber** 24 × 24 m directly under the tower, on four piers
- **Tunnel W / Tunnel E** 16 × 10 m each, running out under the courts
- **Tunnel S** 8 × 12 m, running out under the south lane
- **Four shafts back to daylight**, each a hole in the ground slab with a 6 m ramp in it:
  west court, east court, south lane, and one inside the tower's own east gate

That last one is why the tower is the map: five tiers pass through one structure, so you
can fight it from −4 m to +12 m without ever leaving it.

The ground slab is no longer one box — `slab_with_holes()` decomposes it around the shafts
and merges the remainder into runs, so a 80 × 60 m floor with four voids is 23 actors.

## Lineage — what "duplicate it" actually kept

`Lvl_Shooter` was duplicated to `BR_Spillway` via `AssetTools.duplicate`. **The original is
untouched** (verified `is_dirty` = false after the whole session).

KEPT from the template: DirectionalLight, SkyLight, SkyAtmosphere, VolumetricCloud,
ExponentialHeightFog, PostProcessVolume, SM_SkySphere, the World Partition setup, the
NavMeshBoundsVolume + RecastNavMesh, and — the point of the exercise — **its entire
prototyping kit**: `SM_Cube`, `SM_Ramp`, `SM_Cylinder`, `SM_ChamferCube` from
`/Game/LevelPrototyping/Meshes`, wearing `MI_PrototypeGrid_TopDark`,
`MI_PrototypeGrid_Gray` and `MI_FirstPersonColorway`.

REMOVED (116 actors): 97 template StaticMeshActors, 6 PlayerStarts, 5 `BP_JumpPad`,
5 `BP_WobbleTarget`, 1 `BP_DoorFrame`. The three Blueprint classes are template gameplay
and would breach R18 if kept; traversal here is ramps + our own dash/grapple instead.

The BLK kit (`/Game/Blockout/Meshes`, BN31) is deliberately NOT used — the founder asked
for the template's assets.

## The layout

    Y+30 ┌──────────────────────────────────────────────────┐
         │ THE GALLERY — roofed, 6 m corridors, 4 cells      │  tight interior
         │ roof deck at 8 m, reached from each base roof     │  z 0, roof z 800
    Y+14 ├───────────── 3 gates ────────────────────────────┤
         │  WEST BASE │  court │  THE TOWER  │ court │ EAST  │  mid band, 28 m deep
         │  roof 4 m  │        │ 24 × 24 m   │       │ BASE  │  z 0 / 400
         │  4 spawns  │  shaft │ -4/0/4/8/12 │ shaft │ 4 spw │  crown 12 m, mast 16 m
    Y-14 ├───────────── 3 gates ────────────────────────────┤
         │ THE SPILLWAY — 16 m open lane, 80 m long          │  wide exterior
         │ 5 m shelf along the south wall, 36 m of it        │  z 0, shelf z 400
    Y-30 └──────────────────────────────────────────────────┘
         X-40                shaft                       X+40

    and underneath all of it, THE CULVERT at z = -4 m.

Width contrast is the design: a 16 m open lane on one side, 6 m corridors on the other,
and a 24 m tower between them. Height contrast is five tiers, each exactly 4 m apart.

**The power weapon** (`ABRPowerWeaponSpawner`) stands on the tower crown at 12 m, behind a
chest-high parapet, beside a 16 m mast that is the only hard cover up there.

## Why every tier is 4 m — this is not a style choice

From `Config/DefaultEngine.ini`:
- `JumpZVelocity 420` → apex **90 uu**. A player cannot jump onto anything.
- NavLink `BN_Climb` `JumpHeight 90` → bots climb 0.9 m. **Up is ramps only.**
- NavLink `BN_Drop` `JumpMaxDepth 1000` → bots drop at most 10 m per link.

So tiers sit at −400 / 0 / 400 / 800 / 1200: every inter-tier drop is 400 uu, well inside the drop
cap, and every climb has a ramp. All 16 ramps are 2:1 — 400 rise over 800 run, 26.6° —
under the 44.765° walkable limit. **The template's own 45° ramps were deliberately not
copied**; they sit on the wrong side of that limit.

## The toolchain (law 7: generated by committed scripts, never hand-placed)

| file | what it is | needs the editor? |
| --- | --- | --- |
| `Tools/blockout/gen_spillway.py` | **the level.** Design + validator → two JSONs | no |
| `Tools/blockout/make_spillway_meshes.py` | the six material variants | yes |
| `Tools/blockout/land_spillway.py` | JSON → actors, over the editor's MCP server | yes |
| `Content/Data/spillway_manifest.json` | design summary, spawns, power node, bounds | — |
| `Content/Data/spillway_placements.json` | the 204 exact transforms | — |

    python3 Tools/blockout/gen_spillway.py --summary   # validate, write nothing
    python3 Tools/blockout/gen_spillway.py             # write both JSONs
    python3 Tools/blockout/make_spillway_meshes.py     # once
    python3 Tools/blockout/land_spillway.py --strip    # first run
    python3 Tools/blockout/land_spillway.py            # every run after

Idempotent: every spawned actor carries `SpillwayGenerated`, and the lander destroys that
tag before it builds. A re-run is a rebuild, never an append.

`gen_spillway.py` refuses to emit a build it considers illegal. Gates: `SPAWN_COUNT`,
`SPAWN_SPACING` (≥ 8 m), `SPAWN_OUT_OF_BOUNDS`, `SIGHTLINE_OPEN_PAIR` (no cross-team spawn
pair may see another past 35 m), `POWER_NODE_UNSUPPORTED`, `RAMP_OFF_TIER`, `RAMP_NARROW`
(< 4 m), `SHAFT_NO_RAMP`, `SHAFT_SEALED`, `BELOW_GROUND_ORPHAN`, and the rev C flow gates
`FLOW_CLUTTERED`, `FLOW_FRAGMENTED`, `FLOW_NO_LAP`, `FLOW_ISLAND_UNREACHABLE`,
`FLOW_DETOUR`, `FLOW_DETOUR_WORST`. Current state: **0 errors, 0 warnings.**

### The flow gates, and the two that were useless

Flow is now measured, not asserted. A 2 m grid is sampled per tier: a cell counts if it
has floor at that tier and 1.8 m of clear air above it.

- `FLOW_CLUTTERED` — ground must be ≥ 55% open. *Now 81%.*
- `FLOW_FRAGMENTED` — ground's largest region must be ≥ 92% of its walkable area.
- `FLOW_NO_LAP` — the cells hugging the tower's four sides must all be in one region, so
  you can run a full lap around the hub. Without that, flanking does not exist.
- `FLOW_ISLAND_UNREACHABLE` — tier 1 is *supposed* to be islands (base roofs, tower ring,
  shelf); the defect there is an island with no ramp touching it, not fragmentation.
- `FLOW_DETOUR` / `FLOW_DETOUR_WORST` — BFS from eight seeds; path length over
  straight-line distance, capped at 1.35 (p90) and 2.00 (worst).

**The first two gates I wrote did not work and are worth recording as such.** Openness and
connectivity both *passed* on rev B — one region, lap closes — because a gated cross-wall
still connects. Only the detour ratio separates them: rev B's worst corner was 2.55× its
straight-line distance against rev C's 1.71×. That number is the queue-at-the-doorway
feeling, and it is the only one of the three that would have caught the mistake.

A third thing went wrong in the gate itself: at a 2 m grid every sample centre landed
exactly on a slab seam, and the strict `x0 < cx < x1` test then reported solid floor as a
hole — the gate's first run "discovered" the ground was split in half. The sampler is
jittered off round coordinates now, and says so in a comment, because the failure looked
exactly like a real defect.

## Log

**Five defects the build found that the drawing did not.** Recorded because each one is a
class of mistake, not a typo.

1. **The base ramps ran backwards.** The base template is authored in west coordinates and
   mirrored by a sign; when the sign convention was corrected the `up` direction was not,
   so both bases' ramps rose *away* from the roof they were meant to reach. Caught by
   `trace_world` along the ramp: 150 uu where 250 was expected. Traces, not screenshots.

2. **The gallery stairwell was unusable.** A two-flight switchback in a 4 m strip: the
   upper flight passes over its own landing with clearance falling from 2.0 m to zero, so
   the bottom of flight two cannot be reached. A 4 m strip cannot hold a switchback.
   Replaced with one straight flight from the base roof (4 m) to the gallery roof (8 m).

3. **Both teams' deck spawns saw each other across 47 m.** A straight line at y = ±950 and
   eye height cleared the tower ground floor (top 4 m) and passed *under* the tier-2 deck
   (underside 7 m). Fixed with two end masses on the tier-1 deck and by carrying the core
   up through tier 1. The validator found this, not a playtest.

4. **The nav volume was built 94 km across.** Its scale was computed against an assumed
   200 uu builder-brush base; the template's brush is actually 2937 × 3349 × 683 uu. The
   lander now measures the current bounds, divides out the current scale to recover the
   base, and solves for the scale it wants. Now 6400 × 5200 × 2200.

5. **`--markers` deleted the whole map.** It skipped placing geometry but still ran the
   clear-by-tag step, so a marker-only pass wiped 130 geometry actors. Markers now carry a
   second tag `SpillwayMarkers` and `--markers` clears only that.

**Two things the screenshots refuted after the geometry was correct.**

- A 4 m parapet on the crown means that standing on the power weapon you cannot see the
  arena — which is the entire point of the perch. Parapets and deck cover are now 1.1 m.
- A 16 m perimeter wall seals the arena into a box and leaves the ground floor in shadow.
  The wall is now 10 m: 2 m above the tallest roof (so it cannot be stepped onto — the
  jump apex is 0.9 m) and 2 m below the crown (so the crown genuinely overlooks). The
  arena's 16 m high point moved onto the tower mast, where it is played.

**A tool finding worth keeping.** `ObjectTools.set_properties` **cannot write the
components of World Partition external actors** through the editor's MCP server. It
returns `false` and changes nothing — verified against `overrideMaterials`,
`overlayMaterial`, `minLOD` and `bOverrideMinLOD` alike, so it is the writer and not the
property name. Per-actor material overrides are therefore unavailable; the colour rides on
the mesh instead, via six material variants of the template meshes in
`/Game/Spillway/Meshes`. `ActorTools` writes (`set_label`, `add_tag`, `set_actor_folder`,
`set_actor_transform`) all work fine on the same actors, as does `set_properties` on
`WorldSettings` — which is how `BP_BNGameMode` got set.

`StaticMeshTools`, `SceneTools` and `ObjectTools` all require the **full** object path
(`/Game/Foo/Bar.Bar`); the package path alone is rejected as "not a valid object path".

### Rev B defects — three more classes the build caught

6. **A shaft can be sealed without anything overlapping it.** `Lane_Mass_C` stood on top of
   `Shaft_S`: the mass sits at z ≥ 0 and the shaft ramp at z < 0, so they never overlap in
   3D and no gate saw it — but the hole was roofed and the ramp unreachable. The new
   `SHAFT_SEALED` gate tests plan overlap for solids whose *underside* is on the ground,
   which is the distinction that matters: a bridge overhead is headroom, a wall is a lid.
   First run of the gate immediately found a second instance — the west base's front wall
   standing on `Shaft_W`.

7. **The overlap scan skipped props.** It only considered `box` and `ramp`, so a culvert
   crate punched through the `Shaft_S` ramp and showed up only as a wrong trace height.
   It now scans every element, at a 2 m³ threshold instead of 8 m³.

8. **Three boxes were built with their arguments in the wrong order.** `box()` takes
   `(x0, y0, z0, x1, y1, z1)` and the crown plinth, crown cover and both ring covers were
   written `(x0, y0, x1, y1, z0, z1)`. Two tripped the degenerate-box assert immediately;
   the mast did not — it silently became a 20 m tall slab from the culvert to the sky, and
   only the overlap scan (34 warnings against everything it touched) exposed it.

## Verified

Read back out of the running editor after the final land:

- 213 actors tagged `SpillwayGenerated` (204 geometry + 9 markers), 233 actors total, in
  11 named outliner folders.
- 8 `APlayerStart`, tagged `West` / `East` 4 and 4.
- NavMeshBoundsVolume 8400 × 6400 × 3000 uu, centred on the arena.
- **All four culvert shafts traced end to end**: each descends 0 → −400 at exactly 2:1
  (−20 / −100 / −250 / −380 at the quarter points), and the culvert floor reads −400
  across the chamber and all three tunnels.
- Recast **has** generated navmesh, and it covers the culvert: the viewport's navigation
  overlay is green across the chamber floor and both tunnels in the underground capture.
  Coverage per tier is still eyeballed from that overlay, not measured tile by tile.
- World Settings `DefaultGameMode` = `/Game/BN/Core/BP_BNGameMode.BP_BNGameMode_C`.
- Every tier height and every one of the 12 ramps confirmed by `trace_world`: base ramps
  10 → 390, gallery stairs 410 → 790, tower T1→T2 410 → 780, tower T2→T3 810 → 1190.
- `/Game/Variant_Shooter/Lvl_Shooter` still clean.

## NOT verified — the honesty ladder (law 6)

This is at rung **"it builds and the geometry measures correct"**. Specifically NOT claimed:

- **Never played.** No PIE session has been run on this map. Nothing about how it *feels*
  is known — pacing, sightline balance, whether the tower is too strong.
- **Navmesh coverage is eyeballed, not measured.** Recast clearly ran and the overlay is
  green on the surface and in the culvert, but nothing here counts tiles per tier or proves
  the five tiers are linked to each other. Whether the ramp-link defect open in BN34 bites
  on 16 ramps is unmeasured. Bot reachability on the crown is the specific risk.
- **No bot run.** 8 AIB bots have never spawned here. The BN34 ramp-link issue is open and
  this map is built entirely out of ramps.
- **Balance is asserted, not measured.** The 180° base symmetry is exact, but the north
  gallery and south lane are *not* rotations of each other, so the two bases sit against
  different neighbourhoods. Each base has one lane-side and one gallery-side ramp, which is
  the argument that it is fair. It is an argument, not a measurement.
- Lighting is the template's, unbuilt, and tuned for a map a third this size.

## Next

1. PIE with 8 bots; watch for "cannot reach the objective" and off-mesh refusals.
2. Read the navmesh tile coverage per tier; the crown at 12 m is the one to check first.
3. If it plays, decide whether `EditorStartupMap` / `GameDefaultMap` should move off
   `BR_Arena01`. Not changed here — that is a founder call.
