# THE MODULAR BLOCKOUT KIT — research, rules, and the Aquarius decomposition

> BN30. Founder directive (30 Aug): "break down each object — stairs, wall,
> ramps, floors, obstacles, columns... block it out in a modular way, reusing
> the same object throughout the level even at different heights and widths...
> a modular set of blockout assets... so the Unreal MCP tools can make the
> level correctly from the get-go. Do the research on how to do it correctly."
>
> Two research passes feed this document (methodology/metrics; modular-kit
> construction). Facts below are tagged [solid] (multiple sources agree) or
> [thin] (single source / needs in-engine verification). Sources: The Level
> Design Book, Joel Burgess's GDC modular-kit talks, Epic docs, Halo Support's
> official Forge map requirements, Valve/Quake metric kits, World of Level
> Design, polycount, Allar's ue5-style-guide.

## 1. The research, distilled

### 1.1 Metrics come first [solid]

A blockout is built against a locked METRIC SHEET, measured in-engine (the
"metrics gym": a test level of steps, gaps, ramps and crawl heights), never
copied blind. The numbers that matter, with UE 5 baselines:

| Metric | UE default | Note |
|---|---|---|
| Capsule | r 34 x hh 88 cm (176 cm) | mannequin 180 cm [solid] |
| Crouch | hh 40 cm (80 cm) | [thin] - measure ours |
| Walk speed | 600 cm/s | [solid] |
| JumpZ | 420 (templates often 700) | [thin] - measure ours |
| Max step | 45 cm | [thin] |
| Walkable slope | 44.765 deg | [thin] - the hard ramp limit |

Halo's own numbers (343's official Forge map requirements, [solid]): 1 Forge
unit = 1 ft; jump WITHOUT clamber 8 u (~2.4 m); comfortable clamber 12 u
(~3.7 m); grapple range 80 u (~24 m); ceilings ~18 u (~5.5 m) where players
jump; "no toe clambers"; out-of-bounds roofs visibly above the play space.
Spartans are ~2.0-2.2 m — Halo worlds run ~1.15x mannequin scale [solid].

**What this means for Aquarius:** the +4.00 m decks sit ABOVE Halo's own
comfortable-clamber band (~3.7 m) — vertical access is ramps and the
Grappleshot by design, which matches both the reference map and BN23/AIB19.
Sanity math [solid method]: at 600 cm/s our longest lane (~46 m) is ~7.7 s —
inside the 5-10 s first-contact band the working hypothesis targets.

### 1.2 The workflow [solid]

Macro shell -> circulation loops (no dead ends) -> cover and sightlines ->
micro. Playtest in first person after every pass; judge scale ONLY from the
player camera (scale creep from editor-view judging is the most-cited
blockout failure); validate with a stopwatch (rotation seconds, not metres);
no art pass until the greybox is fun repeatedly. Spawns: orientation cover,
off high-traffic lanes, maximize enemy distance-to-line-of-sight.

### 1.3 The kit rules [solid unless tagged]

- ONE base grid; every footprint a multiple of it; build with snap on. Kits
  live or die on grid discipline — off-grid geometry kills modular swap-out.
- SMALL kits win: a working Bethesda pipe kit was four pieces; a full basic
  roster is 8-12 piece types. Under ~15 modules is professional-normal.
- Scaled primitives ARE the blockout: non-uniform scaling of a grey unit
  cube is standard greybox practice (nothing textured, nothing breaks);
  production later replaces each size with an authored variant because
  scaling breaks texel density. The dichotomy is well-sourced [solid].
- What scaling CANNOT make: stairs (rise:run is gameplay; scaling a stair
  changes the step metric) and connection pieces (corners, doorframes).
  Ramps as pitched slabs are an accepted greybox shortcut [thin].
- Pivots: architecture kits use a corner-on-grid pivot for end-to-end
  snapping; props bottom-center; ONE rule per family, zero exceptions.
- Naming: Epic's `SM_` prefix + kit tag + piece + dimensions
  (`SM_BLK_Wall_400x300x20`); numeric `_01` variants (Allar/Epic [solid]).
- Placement: individual StaticMeshActors while the level iterates (full
  editability); ISM/HISM/Packed Level Actors only when instance counts hurt
  [solid]. Mobility Static, simple box collision.
- Python rebuild: `EditorActorSubsystem` (not the deprecated
  EditorLevelLibrary); `unreal.Rotator(roll, pitch, yaw)` ARGUMENT ORDER is
  a classic bug source [solid] — the manifest stores named fields.

### 1.4 The mistake list the kit must not commit [solid]

Scale creep · over-detailing before fun · off-grid geometry · forgotten
verticality · cover that ignores the real crouch height · slopes near the
45 deg limit · unintentional clambers · spawns facing danger · **hollow /
paper-thin walls** (the founder's own 30 Aug finding, independently on the
canon list) · building multiple passes between playtests.

## 2. OUR kit — TWELVE assets (K-101 / K-102)

Founder correction (30 Aug): the kit is its INDIVIDUAL assets, no more than
twelve. `Tools/blockout/gen_kit_catalog.py` renders the catalog — two sheets,
one card per asset, each with canonical dims and its scaling rule:

| # | Asset | Canonical size (m) | Scaling rule |
|---|---|---|---|
| 01 | BLK_Floor_400 | 4.0 x 4.0 x 0.2 | scales L/W/T (deck t 0.4) |
| 02 | BLK_Wall_400 | 4.0 x 0.5 x 4.0 | scales L/H/T |
| 03 | BLK_HalfWall_200 | 2.0 x 0.5 x 1.1 | L only - H fixed (cover band) |
| 04 | BLK_Doorway_400 | 4.0x0.5x4.0, opening 1.4x2.4 | jambs stretch, opening never |
| 05 | BLK_Ramp_800 | 8.0 run x 4.0, rise 4.0 (27 deg) | re-length only, never past 45 |
| 06 | BLK_Stair_200 | 3.0 run x 2.0, rise 2.0 (0.20/0.30) | NEVER scales |
| 07 | BLK_Column_100 | 0.9 dia x 4.0 octagon | H only; never wall-touching |
| 08 | BLK_Pier_100 | 1.0 x 1.0 x 3.6 | footprint only |
| 09 | BLK_Crate_100 | 1.0 x 1.0 x 1.0 | uniform, max +/-25% |
| 10 | BLK_Battery_240 | 2.4 dia x 1.2 octagon | never (mantle height) |
| 11 | BLK_Rail_400 | 4.0 x 0.1 x 1.0 | L only |
| 12 | BLK_Curb_400 | 4.0 x 0.3 x 0.15 | L only |

The Aquarius placement manifest maps onto these via `asset_map` (Floor/Deck
-> 01, Wall/Tower -> 02, Support -> 08, Ramp -> 05); HalfWall, Doorway,
Stair, Column, Crate, Battery, Rail, Curb are placed in the editor pass
against the reference-note anchors (16-25).

### The generated decomposition — `aquarius_blockout_kit.json`

Generated by `Tools/blockout/gen_aquarius_kit.py` from the same per-floor
trace extraction the REV D blueprint set draws (single geometry truth).

- **Modules (meshes): 2.** `BLK_Cube` (`/Engine/BasicShapes/Cube`, 100 cm,
  center pivot) carries every family — Floor, Deck, Wall, Tower, Support,
  Ramp (pitched slab). `BLK_Cylinder` is reserved for pass-2 columns.
  Center pivot is a deliberate deviation from the corner-pivot convention:
  instances are placed by computed box centers from the manifest, never
  hand-chained, so the corner pivot's snapping advantage does not apply.
- **Grid 0.5 m**; long runs split into the standard length family
  **[8, 4, 2, 1, 0.5] m** so identical variants repeat (the reuse the
  founder asked for): same variant, different position — and the same
  MODULE at different scales across variants.
- **Families and the level schedule:** Floor t 0.20 (top 0.00) · Deck
  3.60-4.00 · Wall 0-8.00 · Tower 0-6.50 · Support 0-3.60 · Ramp slab
  t 0.30 rising 0.25 -> 4.00, pitched, never past 45 deg.
- **Current Aquarius decomposition:** 878 placements · 90 size-variants ·
  6 families · 2 meshes. Top repeats: 1.0x0.5 floor tile x53, 2.0x0.5 x52,
  4.0x0.5 x51, 0.5-thick 8 m wall x46, 1.0x0.5 deck x44.
- **Doubts, in the JSON itself:** 8 traced "ramp" capsules are too short to
  climb 4 m under the 45 deg walkable limit (they whiff at 68-82 deg. They
  are placed as UNPITCHED solid blocks and flagged `suspect` — channel (b)
  says what they really are). Diagonal chamfers step at 0.5 m this pass;
  yaw-45 wall variants are pass-2.
- **Pass-2 owed** (also in the JSON): BLK_Cylinder free-standing columns
  (orbitable, never wall-touching — notes 21-25), battery octagons at base
  mouths, lane crates, deck-edge railings (~1.0 m [thin]), the sunken
  trench.

## 3. The pipeline (doctrine: ue-editor skill, law 7)

```
founder references  ->  trace_aquarius.py        (per-floor class grids)
                    ->  gen_aquarius_blueprint.py (drawings: A-101..A-301)
                    ->  gen_aquarius_kit.py       (THE KIT MANIFEST)
                    ->  build_aquarius_blockout.py (in-editor projection)
```

- The kit JSON is the **source of truth**; the .umap is its projection.
- The committed, idempotent `build_aquarius_blockout.py` is the LANDING
  mechanism (every actor tagged `BN30_BlockoutGenerated`; re-run = clean
  rebuild). Headless: `UnrealEditor-Cmd Breachpoint.uproject
  -run=pythonscript -script="Tools/blockout/build_aquarius_blockout.py"`.
- **Unreal MCP** (first-party UE 5.8 server, terminal side) is for the
  EVIDENCE LOOP: `EditorAppToolset.CaptureViewport` screenshots after each
  build, `SceneTools.find_actors` / world traces to verify placements,
  spike probes. It never lands geometry directly — that keeps every map
  reproducible by the verifier from git alone.
- Iteration: founder feedback -> regenerate manifest -> re-run builder ->
  new screenshots. The editor state never drifts ahead of the manifest.

## 4. Validation targets (before any art pass)

1. Metrics gym in-engine: measure OUR capsule, crouch, JumpZ, step, slope;
   write the sheet into this doc as measured [owed to terminal].
2. Walk rung: every ramp walkable both ways; no unintentional ledge grabs
   onto decks; deck reachability = ramps + grapple only, as designed.
3. Stopwatch rung: spawn-to-spawn and lane rotations in seconds (working
   band: first contact 5-10 s; longest rotation under ~20 s [CS-derived]).
4. Sightline rung: R45-governed lines re-checked in-engine from eye height,
   not from the trace (arena_plan's 36.06 m longest spawn line).
5. Playtest cadence: play after every pass; three fun greybox sessions
   before any art conversation.
