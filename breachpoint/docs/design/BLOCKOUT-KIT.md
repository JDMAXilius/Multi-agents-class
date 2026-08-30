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

### THE SHEET — MEASURED, 30 Aug 2026 [solid]

No longer "UE defaults, verify later". Read straight off the shipped pawn's
CDO (`/Game/BN/Characters/BP_BNCharacter`, its `CharMoveComp` and
`CollisionCylinder`) through the in-editor Unreal MCP server. This is OUR
sheet; the [thin] tags below it are retired.

| Metric | MEASURED | Source |
|---|---|---|
| Capsule standing | r 34 x hh **96** cm = **1.92 m** | CollisionCylinder |
| Capsule crouched | r 34 x hh 40 cm = **0.80 m** | CrouchedHalfHeight 40 |
| Eye height | **1.60 m** standing / **0.72 m** crouched | BaseEyeHeight 64, Crouched 32 |
| Walk speed | 600 cm/s (crouched **300**) | MaxWalkSpeed / Crouched |
| Accel / braking | 2048 / 2048 cm/s2 | MaxAcceleration |
| JumpZ | 420, GravityScale 1, JumpMaxCount **1** | -> apex **0.90 m**, air 0.857 s |
| Flat jump reach | **5.14 m** at 600 cm/s | 600 x 0.857 |
| Max step | **45 cm** | MaxStepHeight |
| Walkable slope | **44.765 deg** | WalkableFloorAngle (Z 0.71) |
| Air control | 0.5 | AirControl |
| Clamber / fall damage | **NONE** | BN has neither |
| Grapple | **22 m**, 4 s cooldown | `BNGA_Grapple.h` MaxRangeUU 2200 |
| NAV agent | r 34, height **144 cm**, slope 44 deg | RecastNavMesh CDO |

**Three findings the measurement produced, all of them kit law now:**

1. **The body is 1.92 m, not the 1.76 m this document assumed.** Every
   clearance number above was written against a character 16 cm shorter.
   Nothing in the current kit breaks (min overhead is the 3.60 m under-deck,
   the doorway is 2.40 m) but the margin was never as big as claimed.
   Our Spartan-ish 1.92 m is also why Halo's own metrics port cleanly — we
   are at Halo scale (2.0-2.2 m), not mannequin scale.
2. **NAV PROMISES WHAT THE BODY CANNOT DO.** The navmesh agent is 1.44 m
   tall against a 1.92 m capsule, so Recast will happily floor any gap
   1.44-1.92 m high and route a bot into a beam it cannot fit under.
   **No kit piece may ever create an overhead in the 1.44-1.92 m band**, or
   `AgentHeight` gets raised to 192 to match the body. Nothing violates it
   today; pass-2 (railings, low ducts) is exactly where it would happen.
3. **The traversal band is 0.45-0.90 m, and it is EMPTY — keep it that way.**
   Walked up to 45 cm (step); jumped up to 90 cm (apex); nothing above that
   without the grapple, because there is no clamber. So a top face in
   0.45-0.90 m is an unintended climb. Audit of the 15-piece kit: Curb 0.15
   and Pedestal 0.40 are walk-ons (below the step); HalfWall 1.10, Rail 1.00,
   Crate 1.00 and Battery 1.20 are all hard blocks (above the apex). The band
   is clean by luck, not by rule. It is a rule now.

   Correction it forces: the catalog calls Battery_240 (1.20 m) and
   Crate_100 (1.00 m) *mantle height*. There is no mantle in BN and both sit
   above the 0.90 m apex — they are COVER, not traversal. The "never scales"
   rule on them is right; the reason written beside it was wrong.

Cover math, now exact: a 1.10 m HalfWall hides the 0.80 m crouched body with
0.30 m to spare, and leaves 0.50 m of the 1.60 m standing eye line exposed —
chest-high cover, as intended.

For reference, the UE 5 baselines this replaces:

| Metric | UE default | Note |
|---|---|---|
| Capsule | r 34 x hh 88 cm (176 cm) | mannequin 180 cm [solid] |
| Crouch | hh 40 cm (80 cm) | [solid] - ours matches |
| Walk speed | 600 cm/s | [solid] |
| JumpZ | 420 (templates often 700) | [solid] - ours is 420 |
| Max step | 45 cm | [solid] |
| Walkable slope | 44.765 deg | [solid] - the hard ramp limit |

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
twelve — extended to FIFTEEN on request (30 Aug: 'any extra individual
modular assets?'). `Tools/blockout/gen_kit_catalog.py` renders the catalog — three sheets,
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
| 13 | BLK_Wall45_200 | 2.0 chord x 0.5 x 4.0 at 45 deg | H only - chord fixed (connection) |
| 14 | BLK_GlassWall_400 | 4.0 x 0.3 x 4.0 | L/H - stays see-through (sightline) |
| 15 | BLK_Pedestal_120 | 1.2 dia x 0.4 octagon | never (pickup read height) |

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

### The ASSEMBLY sheets — the level drawn as the kit

Founder (30 Aug): "now make the blueprint with those modular assets."
`Tools/blockout/gen_aquarius_assembly.py` draws the kit JSON directly:
AK-101 / AK-102 (per-level assembly plans - every placed instance its own
outlined piece, module seams true, BILL OF MATERIALS per level with
per-asset quantities and top size variants) and AK-301 (the 3/4 assembly -
all placements as individual blocks; floors/decks as flat seamed plates,
long pieces subdivided for correct occlusion). These sheets read ONLY the
manifest: regenerate it and they re-render to match.

### Piece-count budgets — the third research pass (founder: "why so many?")

The 878-piece first decomposition was machine granularity, not level-design
practice. Researched anchors: complete SHIPPED 4v4 Forge arenas lived under
the Halo 3 / Reach hard cap of 640-650 objects for two console generations
[solid]; Halo 5's 1,600 and Infinite's ~7,000 budgets exist for the ART pass
[solid]; finished Quake 3 duel maps run ~900-1,000 brushes WITH art [solid];
community performance bands: <300 objects flawless / <500 decent for a whole
map [thin]; doctrine: "big simple shapes, cheap to throw away" [solid].

**Budgets adopted (enforced by the generator, recorded in the JSON):**
- MASSING pass: 50-100 pieces (2.0 m grid) - ours lands at 105 (5 over;
  the ring's connectivity floor, see the wall-threshold note)
  (`aquarius_blockout_massing.json`).
- GREYBOX: 200-400 pieces, soft ceiling 500, red line ~650 (0.75 m grid) -
  ours lands at 292 (`aquarius_blockout_kit.json`, the canonical manifest:
  37 floor plates + 75 deck plates + 58 walls + 44 towers + 5 piers +
  12 ramps).
- Wall coverage thresholds are LOW by necessity: a thin wall crossing a
  1 m bin fails a high threshold and the perimeter comes out DOTTED
  (measured 0.30 -> 37 fragments; 0.15 -> one closed ring).
- How the count fell 878 -> ~230: maximal rects instead of length-family
  tiling (the family returns at the ART pass as authored variants), floor
  plates running hidden under structure, and the 1.0 m pass grid absorbing
  the 0.5 m trace-edge slivers (368 of the old 878 were slivers).

### Playability + fidelity, PROVEN at the geometry rung

`Tools/blockout/validate_aquarius_blockout.py` (founder: "make sure the level
is playable and is 1:1") builds the walkable graph from the kit manifest and
reports PASS/WARN/FAIL. Current verdict **PASS (0 fail, 2 warn)** on TEN checks:

- ground floor is ONE walkable region, 742 m2; **100%** of the deck area
  (both side rings AND the bridge) reachable from it
- the 0.68 m capsule reaches 86% of the ground in one eroded region
- HOW IT PLAYS: longest cross-map rotation **8.9 s** (competitive band
  <= 20 s), mean spawn-to-centre 3.6 s, team_a 4.5 s vs team_b 4.4 s to
  centre (**2.3% skew**, requirement <= 5%), and cutting the primary route's
  middle still leaves the bases connected - **parallel lanes, not one
  corridor**
- all 8 spawns on walkable ground in the main region; all 6 ramps <= 37.7 deg;
  head clearance 3.60 m under decks; perimeter one closed ring
- FIDELITY: structure IoU **0.74** / decks IoU **0.86** against the traced
  reference (86-88% of the reference covered); 52.5 x 30.0 m vs traced
  52.0 x 29.9 m (1.0% / 0.3%); mirror symmetry 0.956. See
  `fidelity_overlay.png` - every deviation is a one-cell grid fringe; no
  shape is invented. Build grid tightened 1.0 -> 0.75 m for this (measured
  IoU 0.66 -> 0.74) at 292 pieces, still inside the researched band.
- WARN carried: 0.6 m2 of deck sliver, and
  100 m2 of ground lies within a capsule radius of structure (corridor edges)

Defects this validator FOUND and the generator now fixes: spawns were being
read in the wrong Y frame (arena manifest is +y north, the kit +y south -
they would have mirrored); three ramps ran into walls or ended in mid-air;
and unconvertible capsules, placed as 4 m solids, were sealing the bridge's
own ramp mouths - they are now left OPEN and recorded in
`unresolved_capsules`, because a capsule we could not anchor was never
proven to be a wall either.

**NOT proven here:** the capsule actually walking, nav mesh generation, and
whether it is fun. Those are the in-editor rung and stay open.

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
