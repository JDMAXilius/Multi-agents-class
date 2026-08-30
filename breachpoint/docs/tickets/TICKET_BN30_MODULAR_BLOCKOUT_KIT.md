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
