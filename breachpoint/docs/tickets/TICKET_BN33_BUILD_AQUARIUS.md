# TICKET — BN33: build BR_Aquarius from the kit (TERMINAL)

> STATUS: in-progress — mac terminal 30 Aug 2026 (256ed547). Gated on BN31.

> OWNER: **terminal**. DEPENDS ON BN31 (the 15 assets).
> This is the first time the level exists in the engine.

## What lands

`/Game/Maps/BR_Aquarius` — 292 kit instances, 8 spawns, the rocket marker,
a nav volume. Everything from committed data; nothing hand-placed.

## Inputs (all committed, all validated)

- `Content/Data/aquarius_blockout_kit.json` — **the source of truth.**
  292 placements · 94 size variants · 6 families · 0.75 m grid.
  Carries `spawn_points` ALREADY converted into the kit frame,
  `piece_budget`, `repairs`, `unresolved_capsules`, `asset_map`.
- `Content/Data/blockout_kit_assets.json` — the 15 asset specs.
- `Content/Data/aquarius_manifest.json` — rocket node + bounds (metres,
  **+y NORTH** — see the frame trap below).
- Drawings for eyeball comparison: `docs/design/blueprints/
  breachpoint_aquarius/` — A-101/A-102/A-103 (plans), A-201 (sections),
  AK-101/AK-102/AK-301 (assembly), `fidelity_overlay.png`.

## Run it

```
python3 Tools/blockout/gen_aquarius_kit.py                 # (only if data changed)
python3 Tools/blockout/validate_aquarius_blockout.py --png # must exit 0
UnrealEditor-Cmd Breachpoint.uproject -run=pythonscript \
  -script="Tools/blockout/build_aquarius_blockout.py" -stdout -unattended -nosplash
```

`build_aquarius_blockout.py` is idempotent: everything it spawns is tagged
`BN30_BlockoutGenerated` and it deletes its own previous actors first. A
re-run is a clean rebuild — prove that (same actor count twice).

## Traps already found (do not re-discover them the hard way)

- **THE FRAME TRAP.** The arena manifest is **+y NORTH**; the kit is **+y
  SOUTH**. Spawns must come from the KIT (`kit["spawn_points"]`, already
  converted and snapped onto walkable ground); the rocket node gets
  `y_kit = bounds.y - y_manifest`. The builder does this — do not "simplify"
  it. `validate_aquarius_blockout.py` catches a regression as `P3_FRAME`.
- **Rotator argument order** is `unreal.Rotator(roll, pitch, yaw)`. The
  manifest stores named fields so this cannot be transposed silently.
- **Nav volume from Python may spawn without brush geometry** (known UE
  quirk). If nav does not build, scale the default brush or place that ONE
  volume by hand — and record it here.
- Ramps carry a real `pitch`; they are the only rotated-in-pitch pieces.

## Done when

- [x] Map exists; actor count matches the manifest (292 + 8 + 1 + 1)
- [x] Re-run proves idempotency (identical count, no duplicates)
- [x] Every actor is tagged and foldered (`Blockout/Floors`, `/Walls`, …)
- [x] Nav mesh BUILDS and covers the playable floor (screenshot the nav
      overlay — bots need this from day one)
- [ ] Screenshot set to the founder via `EditorAppToolset.CaptureViewport`:
      top-down · each named region (Base 1, Arena 1, Hallway 1/2/3, Bridge,
      Base 2) · one from a player-eye height in each arena
- [x] Log records actor counts, the nav result, and whether kit assets or
      the cube fallback were used

## Log

## Log

### 30 Aug — BR_Aquarius EXISTS, and it is genuinely kit-built

**Rung: built, counted, foldered, nav-covered, screenshotted. NOT walked.**
No pawn has moved in it; BN34 owns that. The one unticked box is the full
per-named-region screenshot set, carried to BN34 which needs reference-matched
angles anyway.

**The run instructions in this ticket are WRONG and were not used.** They
prescribe `UnrealEditor-Cmd -run=pythonscript`, which cannot spawn actors —
`bn21_stairs_mcp.py` recorded 82 of 82 spawns returning None because a
commandlet has no initialised editor world. The working channel is the live
editor's `PY` console command (`PythonScriptPlugin.cpp:1060`), driven through
`SlateInspectorToolset` into the status-bar console box. There is no
`-ExecutePythonScript=` CLI flag in UE 5.8 — that name is a Blueprint K2Node.

#### Counts — exact against the manifest

```
cleared 0 previously generated actors
placed 292 kit instances (94 variants)
placed 8 spawn points from the kit
placed 2 lights (a new_level map ships with none)
saved /Game/Maps/BR_Aquarius
```

Read back from the live level: **304 tagged** `BN30_BlockoutGenerated`, 312
total (the 8 extras are engine defaults: WorldSettings, Brush, physics/debug
managers, nav data). Folders match the manifest family for family —
Decks 97, Floors 53, Ramps 8, Supports 4, Towers 54, Walls 76 = 292, plus
Spawns 8, Markers 1, Nav 1, Lights 2.

**Idempotency proven, not asserted:** the second run printed
`cleared 304 previously generated actors` then placed the same 304; tagged and
total both unchanged at 304 / 312. No duplicates, no `_1` suffixes.

#### KIT-BUILT, not the cube fallback

The builder prints a NOTE when it falls back. **It did not print one.** Sampled
the actual `StaticMesh` reference on four actors per family via
`ActorTools.get_components` + `ObjectTools.get_properties`:

| folder | mesh referenced |
|---|---|
| Blockout/Floors, Blockout/Decks | `SM_BLK_Floor_400` |
| Blockout/Walls, Blockout/Towers | `SM_BLK_Wall_400` |
| Blockout/Supports | `SM_BLK_Pier_100` |
| Blockout/Ramps | `SM_BLK_Ramp_800` |

Only 4 of BN31's 15 assets are consumed by this level; the other 11 are the
pass-2 pieces BN34 lists as not yet placed. Expected, not a defect.

#### Nav — it builds, and it actually covers the floor

`RebuildNavigation` completed in 0.01 s, which looked like an empty build, so it
was tested rather than trusted. A read-only probe
(`NavigationSystemV1.project_point_to_navigation` per spawn, plus
`get_random_reachable_point_in_radius`):

- **8 / 8 spawns project onto the navmesh**, every one landing at z=10 (floor);
- **10 / 10 random reachable points** inside a 40 m radius of map centre.

The fast build was fast, not empty. The nav-overlay screenshot is answered by
query instead — a stronger proof than a picture, but recorded as a substitution.

**Two nav warnings carried, neither fixed here:**
- `BorderForLinks (23 vx) exceeds tileSize (0 vx)` — the SAME warning
  `Config/DefaultEngine.ini` already flags as AIB8's unresolved lead ("the next
  thing to investigate is the recast TILE configuration"). It reproduces on a
  clean map, so it is project-wide config, not this level's fault.
- The build reports `agent radius 35.0` while the pawn capsule measured 34
  (BN32). One unit, but the nav agent is not literally the pawn.

#### Two defects fixed in the GENERATOR (law 7), not in the map

1. **The map had no lights.** `new_level()` ships a level with zero
   DirectionalLight and zero SkyLight — every founder screenshot would have been
   black. Caught on BR_MetricsGym first. Both builders now place a tagged
   sun + sky light (`Blockout/Lights`).
2. **The nav volume was 2x oversized.** A volume's default brush is a **200 uu**
   cube, not 100, so `scale = bounds_m + 4` produced a 112 x 68 m volume over a
   52 x 30 m map — 4x the area to voxelise, for nothing. Now halved; measured
   after the fix at **56.3 x 34.2 x 14.0 m**, i.e. bounds + 2 m margin as
   intended. (The "nav volume may spawn without brush geometry" trap this ticket
   warns about did NOT occur — bounds came back valid.)

#### Screenshots

In `docs/design/blueprints/breachpoint_aquarius/`: `aquarius_built_top.png`
(top-down, whole footprint), `aquarius_built_iso.png` (3/4), plus two
eye-height shots. **Framing note for whoever repeats this:** the editor viewport
is 2862x794 — extremely wide — so a top-down needs `yaw 90` to put the 52 m long
axis across the wide screen axis, and `z >= 9000` to fit 30 m vertically. The
first attempt at z=6200 / yaw=0 cropped the map at both ends.
