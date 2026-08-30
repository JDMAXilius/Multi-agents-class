# TICKET — BN30: the modular blockout kit (Aquarius, then every arena)

> STATUS: BUILT 30 Aug 2026 (cloud lead) — manifest + generator + builder
> committed; **builder WRITTEN, NOT RUN** (no editor in the cloud; the
> terminal owns the projection). Founder directive: "break down each object —
> stairs, wall, ramps, floors, obstacles, columns... modular way, reusing the
> same object throughout the level even at different heights and widths...
> so the Unreal MCP tools can make the level correctly from the get-go.
> Do the research on how to do it correctly."

## What exists

- `docs/design/BLOCKOUT-KIT.md` — the deep-research digest (two agent
  passes: blockout methodology/metrics + modular kit construction, sources
  cited, facts tagged [solid]/[thin]) distilled into kit law, plus the
  Aquarius decomposition record and the validation ladder.
- `Tools/blockout/gen_aquarius_kit.py` — trace -> KIT MANIFEST. Quantizes
  the per-floor extraction (same geometry truth as the REV D drawings) to
  the 0.5 m blockout grid, decomposes each class into rects, splits runs
  into the standard length family [8,4,2,1,0.5] m, emits variants +
  placements. Deterministic; `--png` renders the family-colored contact
  sheet for the founder's eyeball.
- `Content/Data/aquarius_blockout_kit.json` — 878 placements · 90 size
  variants · 6 families (Floor/Deck/Wall/Tower/Support/Ramp) · **2 meshes**
  (BLK_Cube = engine basic cube scaled per instance; BLK_Cylinder reserved
  for pass-2 columns). UE conventions throughout: cm, Z-up, X east /
  Y south, named rotator fields (roll/pitch/yaw — argument-order bug
  research-confirmed), center-pivot locations, per-axis scale.
- `Tools/blockout/build_aquarius_blockout.py` — the LANDING mechanism per
  the ue-editor doctrine: committed, idempotent (tag
  `BN30_BlockoutGenerated`, deletes its own before rebuild), spawns kit
  instances via EditorActorSubsystem + PlayerStarts and the ROCKET marker
  from `aquarius_manifest.json` (metres -> cm at one boundary) + nav bounds,
  saves `/Game/Maps/BR_Aquarius`, queues a top screenshot.
- `docs/design/blueprints/breachpoint_aquarius/kit_contact_sheet.png` —
  top-down instance map colored by family; reads as Aquarius.

## Doubts (carried in the JSON too)

- 8 traced "ramp" capsules cannot climb 4 m under the 45 deg walkable
  limit (68-82 deg as traced) — placed as UNPITCHED solid blocks, flagged
  `suspect`, waiting on channel (b) to say what they really are.
- Diagonal chamfers step at 0.5 m this pass; yaw-45 wall variants pass-2.
- Nav volume spawned from Python may lack brush geometry (known UE quirk,
  noted in the builder) — if nav does not build, scale the default brush
  or place that ONE volume by hand and say so here.

## Done when

- [ ] Terminal runs the builder headless; `/Game/Maps/BR_Aquarius` exists;
      re-run proves idempotency (same actor count, no duplicates)
- [ ] Screenshot set back to the founder (top + one per named region);
      founder verdict on the massing
- [ ] Metrics gym measured in-engine (capsule/crouch/JumpZ/step/slope) and
      written into BLOCKOUT-KIT.md as OUR sheet
- [ ] Walk rung: ramps walkable both ways; decks unreachable except ramps
      + grapple; suspect-capsule verdicts recorded
- [ ] Pass-2 kit: columns (BLK_Cylinder), battery octagons, lane crates,
      railings, yaw-45 walls — AFTER the founder passes the massing

## Log

### 30 Aug — built (cloud lead)

- Research first, per the directive: two parallel passes (methodology +
  kit construction), digests merged into BLOCKOUT-KIT.md with every number
  tagged [solid]/[thin]. Key finds that SHAPED the kit: scaled grey
  primitives are the professional blockout norm (authored variants are an
  art-pass concern); stairs are the one thing scaling may never make;
  Rotator(roll, pitch, yaw) argument order; EditorActorSubsystem over the
  deprecated library; Halo's own 8 u jump / 12 u clamber / 80 u grapple
  bands — which VALIDATE the +4.00 m decks being ramp-and-grapple-only.
- The 45-degree audit caught 8 of 12 traced ramps as unwalkable-steep
  (up to 82 deg): trace slivers, not ramps. Demoted to flagged solid
  blocks rather than pretending — the mass exists, nothing lies about
  being climbable, channel (b) rules.
- Reuse numbers, since reuse was the ask: 878 instances from 2 meshes;
  top variants repeat x53/x52/x51; the whole level is scale-and-position
  of the same cube.

### 30 Aug — founder correction: the kit IS the twelve assets

- "Have the individual assets into a couple screenshots, not the level.
  There should not be more than 12 modular individual assets." The kit's
  identity moved from "2 scaled meshes" to a TWELVE-ASSET roster, each
  drawn alone: K-101/K-102 catalog sheets (gen_kit_catalog.py) - one card
  per asset with canonical dims and its scaling rule (plates/walls/columns
  scale; STAIRS and DOOR OPENINGS never; ramps re-length only).
- The roster: Floor, Wall, HalfWall, Doorway, Ramp, Stair, Column (oct),
  Pier, Crate, Battery (oct), Rail, Curb - the full object breakdown the
  directive named, each traceable to a reference note or research metric.
- aquarius_blockout_kit.json now carries asset_map (families -> assets) and
  points at the catalog; the renderer's face-culling bug (view axis is
  +x+y, so east+SOUTH faces show) found and fixed via a unit-cube probe.

### 30 Aug — founder: WHY the hollow-geometry problems; renderers go double-sided

- Root cause, on the record: the sheet renderers are a hand-rolled SVG
  painter with BACK-FACE CULLING, and the cull test's view-axis sign was
  wrong twice (the projection's camera sits on +X+Y; the test assumed
  +X-Y) - a culled real face IS a hole, hence every "hollow" artifact the
  founder caught (A-301 walls, catalog crate/column faces).
- Fix, per the founder's instinct: DOUBLE-SIDED drawing. Both prism
  painters (gen_kit_catalog, gen_aquarius_blueprint A-301) now draw EVERY
  side face sorted far-to-near, so front faces overpaint back faces and a
  sign mistake can no longer delete geometry; normals only pick the tone.
  Unit-cube and A-301 center crops verified closed.

### 30 Aug — founder: extras + stair fix (kit now FIFTEEN, K-103 added)

- Stair geometry rebuilt as true adjacent step columns DESCENDING toward
  the camera (risers + treads face the viewer); the earlier nested-slab
  construction read as a fan. Verified at zoom.
- Three extras, each demanded by the references, kit at 15 (inside the
  researched under-~15 ceiling): BLK_Wall45_200 (the corner piece - kills
  the stepped-diagonal pass-2 debt; drawn display-rotated because a 45 deg
  chord lies on the camera diagonal), BLK_GlassWall_400 (hydro-tower
  glazing - blocks movement, shows the sightline), BLK_Pedestal_120
  (shot11's power-up / the rocket node read height).

### 30 Aug — assembly blueprints: the level drawn AS the kit (AK-set)

- Founder: "now make the blueprint with those modular assets." New
  gen_aquarius_assembly.py renders the kit JSON itself: AK-101/AK-102
  per-level ASSEMBLY plans (every instance an outlined piece, white module
  seams through the dark masses, grid + dims + module key + BILL OF
  MATERIALS: per-asset quantities and top size variants per level) and
  AK-301, the 3/4 assembly with all 878 placements as individual blocks.
- Rendering calls: floors/decks draw as flat seamed plates (thin slabs as
  3D blocks crumbled the read) and long vertical pieces subdivide to <=2 m
  chunks for the painter (long boxes break depth sorting) - drawing calls
  only; the manifest is untouched.
- The sheets read ONLY aquarius_blockout_kit.json - manifest regenerates,
  sheets follow. BOM headline: L1 = 338 floor + 141 wall + 131 tower +
  11 pier + 12 ramp pieces; L2 = 245 deck plates.

### 30 Aug — founder: "why so many?" — piece-count research + retune

- Third research pass anchors the budget: Halo 3/Reach shipped COMPLETE
  4v4 arenas under a 640-650 object hard cap; H5/Infinite's bigger budgets
  are for art; Q3 duel maps ~900 brushes finished; community bands <300
  flawless / <500 decent; doctrine "big simple shapes". Adopted: MASSING
  50-100, GREYBOX 200-400 (soft 500, red line ~650). Full digest + sources
  in BLOCKOUT-KIT.md.
- Generator retuned: maximal rects (length-family tiling retired to the
  art pass), floor plates run hidden under structure, pass grids 2.0 m /
  1.0 m. Two-pass output: massing 97 pieces (IN BAND) to
  aquarius_blockout_massing.json; greybox 208 pieces (IN BAND) to the
  canonical aquarius_blockout_kit.json. 878 -> 208; 368 of the old
  pieces were 0.5 m trace-edge slivers.
- Budget is IN the JSON (piece_budget.band/actual) so the builder and
  every future regeneration is judged against it. Assembly sheets
  re-rendered from the new manifest.

### 30 Aug — founder: "fix the 3/4" — painter, projection, and a REAL defect

Three faults, one of them geometric (not just a drawing bug):

- **DEFECT, found while fixing the view: the perimeter was DOTTED.** At the
  1 m pass grid a 0.5 m wall covers half a bin at best and far less
  diagonally, so the 0.30 coverage threshold dropped bins and the ring came
  out as **37 disconnected fragments** - a blockout wall players could shoot
  and walk through. wall_t lowered to 0.15 (massing 0.10); the perimeter is
  now ONE connected component, verified by flood fill from the emitted
  placements.
- **Painter:** pieces sorted on their CENTROID, so a 30 x 23 m floor plate
  sorted mid-map and painted over every wall behind mid-map (the grey wash).
  Now sorted on each box's FAR corner, and floors/decks draw as real solids
  at their scheduled thickness instead of flat ghost sheets.
- **Projection:** HZ 0.42 compressed height so far that an 8 m wall drew
  shorter than a 4 m piece is wide - the level read as a curb model. Camera
  retuned to HX/HY/HZ 0.78/0.50/0.62: heights near-true, camera high enough
  to see into the arena over intact 8 m walls (still NO cutaway).
- Decomposition also improved: largest-rectangle-first instead of the greedy
  top-left scan, which had diced the ragged ring into 36 one-metre
  fenceposts (now 16). Counts stay in band: massing 105, greybox 231.
- Sheet finished with an assembly summary (per-family counts + the budget
  band) and a tone key in a left gutter.

### 30 Aug — founder: "playable and 1:1" — a validator, and what it caught

New `Tools/blockout/validate_aquarius_blockout.py`: builds the walkable graph
from the kit manifest and checks connectivity, level-to-level reach, spawn
placement, capsule-passable widths (erosion test), head clearance, ramp
slopes, perimeter closure, plus FIDELITY (per-class IoU vs the traced
reference, dimensions, symmetry). Emits a colour diagnostic. Exit 1 on FAIL.

It failed the level on first run, and every failure was real:

- **P3_SPAWN_AXIS — the frame bug.** The arena manifest is +y NORTH; the kit
  is +y SOUTH. The builder was reading spawns straight from the arena
  manifest, which would have MIRRORED every spawn into the wrong half of the
  map. The kit now converts once and carries `spawn_points` in kit frame,
  snapped onto walkable ground; the builder reads those, and the rocket node
  gets the same conversion.
- **RAMP_ORPHAN x3 — ramps to nowhere.** Two ran their foot into the
  perimeter wall, one ended in mid-air. Ramp placement rewritten: each ramp
  is anchored at BOTH ends (head pushed until it overlaps the deck it serves,
  foot placed on free floor at the run a 27 deg slope needs), both directions
  evaluated end-to-end, and the head overlap fixed for an off-by-one that
  stopped ramps a cell short of their deck.
- **The bridge was unreachable.** Capsules that could not be converted were
  being placed as 4 m SOLID blocks - and they sat exactly in the doorways
  where the bridge's ramps land, sealing the whole central deck (67% deck
  reach). A capsule we cannot anchor was never proven to be a wall either:
  they are now left OPEN and listed in `unresolved_capsules` (10 of them) for
  the reference walk. Deck reach went 67% -> 97%.
- **P1 pocket** sealed by quantisation: the generator now carves the doorway
  back where the REFERENCE has it open (reference-driven, never invented).
- My first width metric was wrong (it flagged every corridor EDGE); replaced
  with the correct test - erode by the capsule radius, then check the ground
  is still one region.

VERDICT now **PASS (0 fail, 2 warn)**: ground one region 723 m2, decks 97%
reachable, 8/8 spawns good, ramps <= 37.7 deg, perimeter closed; fidelity
IoU 0.66 structure / 0.84 decks, dims 0.0%/0.3% off, symmetry 1.000.
Honesty line unchanged: capsule walk, nav mesh and fun are the in-editor rung.

### 30 Aug — "playable and 1:1", second pass: play tests + tighter fidelity

The first pass proved CONNECTIVITY. Connectivity is not playability, and a
self-graded IoU of 0.66 is not 1:1. Both raised:

- **Fidelity.** Measured the cost of the build grid: 1.0 m gives structure
  IoU 0.66, 0.75 m gives 0.74, 0.5 m gives 0.79 but 432 pieces (past the
  band). Greybox pass moved to **0.75 m / wall_t 0.20**: structure IoU
  **0.74**, decks **0.86**, 292 pieces - still in the 200-400 band. Deck
  reach also went 97% -> **100%** and orphan area 13 m2 -> 0.6 m2.
  New `fidelity_overlay.png` shows built vs reference cell by cell: the
  deviation is a one-cell grid fringe everywhere, nothing invented.
- **Playability, three new checks that judge how it PLAYS, not just whether
  it connects:**
  P8 route timing - longest cross-map rotation **8.9 s** (competitive band
  <= 20 s), mean spawn-to-centre 3.6 s;
  P9 team fairness - team_a 4.5 s vs team_b 4.4 s to centre, **2.3% skew**
  against a 5% requirement;
  P10 route redundancy - cut the primary base-to-base route's middle
  (30-70%) plus a 0.5 m skirt and the bases STILL connect: the mid has
  parallel lanes, not one corridor.
  (P10's first version cut the whole route including the base doorway, where
  every lane necessarily coincides - it failed the map for a reason that was
  the test's fault, not the level's. Fixed and recorded.)

VERDICT: **PASS, 0 fail, 2 warn, on ten checks.** The honesty line is
unchanged and unmoved: capsule walk, nav mesh generation and fun are the
in-editor rung, still open.

### 30 Aug — the metrics gym, MEASURED (terminal, Unreal MCP)

Third "done when" box closed. The editor was down (`unreal-mcp` lives inside
the editor process, so the MCP server was simply absent); relaunched with
`-ModelContextProtocolStartServer` and driven over its JSON-RPC endpoint.
Numbers read off the SHIPPED pawn's CDO — `/Game/BN/Characters/BP_BNCharacter`
(`CharMoveComp`, `CollisionCylinder`) — not off the C++ class, not off engine
defaults. Full sheet is now in `docs/design/BLOCKOUT-KIT.md` §1.1.

Three things the measurement changed:

- **The capsule is 96 hh, not 88.** The body is **1.92 m**; the whole
  document had been reasoning about 1.76 m. No current kit piece breaks
  (min overhead 3.60 m, doorway 2.40 m) but every clearance margin quoted so
  far was 16 cm optimistic. It also puts us at Halo's Spartan scale, which
  retroactively justifies importing 343's Forge metrics.
- **The navmesh agent is 1.44 m against a 1.92 m body.** Recast will floor a
  gap the capsule cannot enter and route bots into it. Nothing violates it
  today; it is now kit law that nothing may (or `AgentHeight` goes to 192).
  This is the same class of defect as AIB9's drop-link gap — nav promising
  what the body cannot keep — and worth checking before pass-2 railings.
- **The traversal band is 0.45-0.90 m** (step 45, jump apex 90 from JumpZ 420
  / gravity 1, no clamber, JumpMaxCount 1). Audited all 15 kit pieces: the
  band is EMPTY — Curb 0.15 and Pedestal 0.40 walk-on, everything else
  (HalfWall 1.10, Rail 1.00, Crate 1.00, Battery 1.20) sits above the apex as
  a hard block. Clean by luck; recorded as a rule. Forces one catalog
  correction: Battery_240 and Crate_100 are labelled *mantle height* and
  there is no mantle in BN — they are cover, not traversal.

Also confirmed: `/Game/Maps` holds **only `BR_Arena01`**. `BR_Aquarius` does
not exist — the builder still has not been run, so the first "done when" box
stays open, and with it the walk rung, the nav rung and fun.
