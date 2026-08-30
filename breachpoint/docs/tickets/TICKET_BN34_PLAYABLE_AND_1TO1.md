# TICKET — BN34: make it PLAY, and make it 1:1 (TERMINAL, iterative)

> STATUS: in-progress — mac terminal 30 Aug 2026 (256ed547). Gated on BN33.

> OWNER: **terminal**, with the cloud regenerating data between rounds.
> DEPENDS ON BN33. This is the ticket the founder actually cares about:
> "he needs to make the level playable… we want to stick with the
> references of the Aquarius Halo Infinite level, and we want to be as
> one to one as possible."
>
> **Standing warning from the founder, treat as fact:** what the cloud has
> produced may NOT be correct or faithful. It is a traced approximation
> validated only on geometry. Your eyes in the engine, against the
> reference images, are the higher authority.

## Part A — PLAYABLE (the walk rung)

The cloud proved geometry: one walkable ground region, 100% of decks
reachable, 8.9 s longest rotation, 2.3% team skew, parallel lanes, ramps
≤ 36.3°. **None of that proves the pawn walks.** Prove it:

- [ ] Walk every ramp UP and DOWN with the real pawn. Any ramp that fights
      the capsule, list it here with its label (`BLK_Ramp_00n`).
- [ ] Walk the perimeter: no gaps, no shoot-through seams, no place the
      pawn escapes the arena.
- [ ] Walk each of Hallway 1 / 2 / 3 end to end. The side hallways are
      TWO-LEVEL (founder reference note 19) — confirm both levels exist and
      connect.
- [ ] Stand on every deck: is it reachable by ramp, and is the drop back
      down survivable/intended?
- [ ] Jump/mantle audit: is any deck reachable that should NOT be (Halo's
      rule: no accidental clambers)? Decks are +4.00 m — ramps and the
      Grappleshot only, **and BN35 research says that may be wrong**: 343's
      published comfortable-clamber height is 12 ft = **3.66 m**, so a real
      Aquarius deck a Spartan can mantle would be 3.66 m, not 4.00 m. That
      0.34 m is a gameplay difference, not a cosmetic one. Settle it in
      Part C and report which it is.
- [ ] Head-bump audit under every deck soffit (+3.60 m) and in every doorway.
- [ ] Stopwatch the routes with the real pawn and compare to the cloud's
      model: longest rotation 8.9 s, spawn-to-centre ~3.6 s. Report the
      real numbers — if they diverge, the cloud's walk model is wrong and
      needs correcting.
- [ ] Bots: run a match. Do they path both levels? (AIB19's grapple work is
      live; this map is where it gets exercised.)

## Part B — 1:1 (the fidelity rung)

The cloud's fidelity is measured against its own TRACE, not against the
game. Close that loop:

- [ ] Open the founder's reference set (`docs/design/reference/` — 25
      images: two per-floor overheads, the labelled skeletons, and 18
      in-game shots) beside the editor.
- [ ] Fly the same camera angles as the reference shots and capture
      matching screenshots. Put them side by side and list EVERY deviation:
      shape, proportion, height, missing feature, wrong opening.
- [ ] Check specifically the things the cloud KNOWS are unresolved:
      * `unresolved_capsules` in the kit JSON — 10 traced shapes that could
        not be anchored as ramps and were left OPEN. What are they really?
        Ramps? Stairs? Cover? Nothing?
      * the sunken centre trench (reference shots 1/6) — drawn at grade,
        below-grade geometry never modelled.
      * pass-2 kit pieces not yet placed anywhere: columns, the battery
        octagons at the base mouths, lane crates, deck railings, the 45°
        corner walls. The reference has them; the level does not yet.
      * the derived scale: long axis 52 m is DERIVED, never measured, and
        BN35 confirmed **no published Aquarius dimension exists anywhere** —
        it cannot be corrected by citation, only by measurement. Use Part C.
- [ ] Write the deviation list into this ticket as a numbered table with a
      severity (blocks play / breaks fidelity / cosmetic). The cloud
      regenerates the manifest from it — deviations go back into
      `Tools/blockout/gen_aquarius_kit.py`, never hand-fixed in the map
      (law 7: hand edits are lost on the next rebuild).

## Part C — the METRIC STANDARD (new, from BN35 research)

Read `docs/design/HALO-SOURCING-AND-METRICS.md` first. Two things it settles:

**Halo is authored in FEET.** 1 Forge unit = 1 foot = 0.3048 m; 1 world unit
= 10 ft = 3.048 m (343's own Forge documentation, plus the `ekur` importer's
`FEET_TO_METER = 0.3048 * 10.0`, read first-hand). Our schedule is in round
metres, which lands off Halo's grid at every level: 52.0 m = 170.6 ft,
8.00 m walls = 26.2 ft, +4.00 m decks = 13.1 ft. Nobody designs to those.

**Published movement metrics — treat these as the acceptance criteria:**

| Metric | Forge units | Metres |
|---|---|---|
| Jump, no clamber | 8 | 2.44 |
| Comfortable clamber | 12 | 3.66 |
| Overhead clearance, jumping | 18 | 5.49 |
| Grappleshot range | 80 | 24.38 |
| Sprint speed | — | 8.5 m/s |
| Motion tracker radius | — | 25 m |

- [ ] **Measure the real Aquarius, don't estimate it.** Ranked methods, best
      first: (1) clamber/jump binary search on the actual ledges — a ledge you
      can mantle but not jump onto is between 2.44 m and 3.66 m; (2) the
      Grappleshot as a tape measure — it attaches only within 24.38 m, so back
      up until the reticle refuses; (3) the motion tracker as an on-HUD ruler
      (25 m); (4) timed sprint at 8.5 m/s (over-reads — upper bound only).
      Screenshot-vs-Spartan-height, which is what the cloud did, is the
      weakest rung. Report at least the long axis and one deck height.
- [ ] **Report every measurement in FEET as well as metres**, snapped to whole
      feet. The cloud will re-cut the schedule onto the foot grid once there
      are real numbers — that is a generator change, so do not hand-fix it.
**Reconcile with BN32's MEASURED pawn — the two standards are not the same.**
BN32 read the shipped CDO: capsule **1.92 m**, MaxStepHeight **0.45 m**, jump
apex **0.90 m**, and **no clamber verb at all**. So:

- Halo's 2.44/3.66 m jump-and-clamber band does not exist for our pawn. Any
  deck above 0.90 m is ramp-or-grapple for us whether it is 3.66 m or 4.00 m.
  The 0.34 m therefore matters for **fidelity to the reference**, not for
  traversal — do not report it as a play blocker, report it as a deviation.
- Halo's 5.49 m jump headroom is *their* pawn's. Ours needs 1.92 + 0.90 =
  **2.82 m** plus margin, so the +3.60 m soffit clears us. Check it against
  the real pawn anyway; the number above is arithmetic, not observation.
- Where our pawn and Halo's diverge, that is a **tuning decision for the
  founder**, recorded here — never a silent retune of either.

- [ ] Compare our schedule against BOTH standards: does any lane run longer
      than 24.38 m of ungrappleable surface (Halo's grapple reach)? Does any
      overhead land in the 1.44-1.92 m band BN32 flagged as the navmesh's
      lie? Report both.

## The loop

```
terminal: build -> walk -> screenshot -> deviation list (here)
cloud:    fix the generator -> regenerate manifest -> validator PASS -> push
terminal: pull -> re-run builder (idempotent) -> re-check
```

Repeat until the founder says the level reads as Aquarius and plays.

## Done when

- [ ] Walk rung complete, every box in Part A answered with observations
- [ ] Side-by-side reference comparison delivered to the founder
- [ ] Deviation table written, severities assigned, handed to the cloud
- [ ] At least one full loop closed (cloud fix -> rebuild -> re-check)
- [ ] Founder verdict recorded

## Log

### 30 Aug — the founder was right: it is NOT playable. Two causes fixed, one open.

Founder, on seeing the first build: *"make sure is playble right now is not
looking playble at all."* Correct. The geometry validator says PASS on ten
checks and has said so throughout — **it is measuring its own 2D model, not the
built level.** Everything below is measured against the REAL navmesh in the
editor, which is the first time anything has been.

#### DEFECT 1 (FIXED) — eight ramps hanging in mid-air

`BLK_Ramp_800` is authored as a **wedge** that already climbs its own rise over
its own run. The manifest describes a ramp as a **pitched slab**
(`size_m = [slope_length, width, 0.30]`, slope in `rotation_deg.pitch`). The
builder applied the manifest pitch to the wedge, so the slope was counted
TWICE, and it pinned Z scale to 1.0 so the rise could never be corrected.

Measured on the first build — ramp tops against decks at 3.60-4.00 m:

```
BLK_Ramp_001  z:  25 -> 723 cm      BLK_Ramp_005  z:  25 -> 756 cm
BLK_Ramp_002  z:  25 -> 756 cm      BLK_Ramp_006  z:  25 -> 723 cm
BLK_Ramp_003  z:  25 -> 756 cm      BLK_Ramp_007  z:  25 -> 756 cm
BLK_Ramp_004  z:  25 -> 756 cm      BLK_Ramp_008  z:  25 -> 756 cm
```

Every ramp overshot its deck by ~3.5 m and hung in the air. That is what "not
playable" looked like. Fixed in `build_aquarius_blockout.py`: recover the true
run/rise from the slab (`run = L·cos(pitch)`, `rise = L·sin(pitch)`), scale the
wedge to them, drop the pitch, and seat the wedge base at the FOOT
(`z - rise/2`). All eight now read **z: 25 -> 400 cm** — foot a quarter-step
above the floor, top exactly at deck level.

#### DEFECT 2 (FIXED) — ramps were 0.75 m wide

The traced capsules gave ramp widths of **0.75 m** against a **0.68 m** capsule:
3 cm of clearance per side. Unplayable regardless of anything else. Added
`RAMP_MIN_W = 2.0` to `gen_aquarius_kit.py` (clears the 1.40 m comfort width and
lets two players pass).

#### DEFECT 3 (STILL OPEN) — only 2 of 8 ramps link ground to deck

**This is the blocker. The level still does not play.** Measured by pathing on
the real navmesh from 0.4 m beyond each ramp foot to 0.4 m beyond its top:

```
BLK_Ramp_001  PARTIAL - does NOT link      BLK_Ramp_005  PARTIAL
BLK_Ramp_002  PARTIAL                      BLK_Ramp_006  PARTIAL
BLK_Ramp_003  LINKS ok (10.2 m)            BLK_Ramp_007  PARTIAL
BLK_Ramp_004  LINKS ok (10.1 m)            BLK_Ramp_008  no deck nav to test against
```

Consequence, all-pairs between the 8 spawns on the real navmesh (metres = full
path, `~~` = partial, i.e. blocked):

```
        SP1 SP2 SP3 SP4 SP5 SP6 SP7 SP8
  SP1     .  ~~  ~~  ~~  ~~  ~~  ~~  ~~
  SP2    ~~   .  ~~  ~~  ~~  ~~  ~~  ~~
  SP3    ~~  ~~   .  ~~  27  25  10  16
  SP4    ~~  ~~  ~~   .  ~~  ~~  ~~  ~~
  SP5    ~~  ~~  27  ~~   .  13  17  15
  SP6    ~~  ~~  25  ~~  13   .  15  18
  SP7    ~~  ~~  10  ~~  17  15   .   8
  SP8    ~~  ~~  16  ~~  15  18   8   .
```

The four GROUND spawns (SP3, SP5-SP8) interconnect fine. **SP1, SP2 and SP4 are
deck spawns (raw z = 410) and are cut off from everything.** Only the central
bridge is reachable, via Ramp_003/004.

Navmesh coverage is NOT the problem — it exists on both levels and runs up the
ramp surfaces continuously:
- Floors: **53/53** pieces carry navmesh within 60 cm of their top face (z 10-30)
- Decks: **65/97** pieces carry navmesh (z 410)
- Along a ramp: `0%:34  25%:129  50%:227  75%:300  100%:410` — continuous

So the surfaces are navigable; the ramp-to-deck (or ramp-to-floor) **junction**
is where the islands fail to merge.

#### A hypothesis I tested and DISPROVED — do not repeat it

The two linking ramps had exactly **0.00 m2** of Wall/Tower footprint overlap
while all six failing ones had some (0.13 / 0.39 / 0.56 / 0.13 / 1.79 /
2.26 m2). That correlation looked causal, so `clear_ramps()` was added to
`gen_aquarius_kit.py` to slide each ramp into a clear lane (narrowing to 1.2 m
only where no 2.0 m lane exists). It worked as designed — regenerated, rebuilt,
and the live actors match the manifest exactly (positions, widths 1.2/2.0,
foot z=25) — and **the link results did not change at all.** Wall overlap was a
correlation, not the cause. `clear_ramps()` is kept because ramps embedded in
walls are wrong anyway, but it is NOT the fix.

#### Where the next session should look

1. **The recast tile config, first.** Every nav build logs
   `BorderForLinks (23 vx) exceeds tileSize (0 vx)`. A tile size of **0 voxels**
   is degenerate. `Config/DefaultEngine.ini` already flags this as AIB8's
   unresolved lead ("the next thing to investigate is the recast TILE
   configuration (TileSizeUU / CellSize are unset project-wide)") and it
   reproduces on a clean map. A degenerate tile size is exactly the kind of
   thing that generates per-surface navmesh that never merges across tiles.
   **This is the strongest remaining candidate and it is a project-wide config
   bug, not an Aquarius bug.**
2. The nav build reports `agent radius 35.0` while the measured pawn capsule is
   34 (BN32). Small, but the nav agent is not literally the pawn.
3. 32 of 97 deck pieces carry no navmesh — worth locating before assuming the
   ramps are at fault.
4. Only 4 of BN31's 15 kit assets are used. Stairs (`BLK_Stair_200`) are placed
   nowhere, and a stair is the obvious second route up if ramps stay difficult.

#### Honest rung

Ramps are fixed and provably seated. Ground play is connected. **The upper floor
is not reachable except at the centre, so the level does not yet play as an
arena.** Nobody has walked it in PIE - every statement above is a navmesh
measurement, which is a rung below the pawn actually moving.

### 30 Aug (later) — the recast tile-config lead is a DEAD END, and why

Founder asked for the tile config to be fixed. **There is nothing to fix.** The
`BorderForLinks (23 vx) exceeds tileSize (0 vx)` warning that has been treated as
evidence since AIB8 is an **engine log-ordering bug**:

- `RecastNavMeshGenerator.cpp:5263` calls `ComputeConfigBorderSizes()`, which
  prints the warning;
- `RecastNavMeshGenerator.cpp:5270` assigns `OutConfig.tileSize` — *seven lines
  later*. (Same pattern at 5358 / 5390.)

So the warning reads `tileSize` before it exists. The `0` is an uninitialised
field, not our configuration. Three further confirmations:

1. `:5271` logs an **Error** whenever the real tileSize computes to 1 ("highly
   discouraged... indicates an issue with RecastNavMesh's generation
   properties"). That error has never been logged here.
2. The warning is about MEMORY, not correctness — its own text says "This will
   increase memory usage during nav build", and the source comments "At this
   point we do not clamp -- a large JumpLength is a valid (if expensive)
   configuration."
3. Live `RecastNavMesh-Default` reads `TileSizeUU = 1000`, so the real tile size
   is `1000 / CellSize` voxels, comfortably above the 23 vx border.

`Config/DefaultEngine.ini` has been corrected in place, because it recorded this
lead as the next thing to investigate and would have cost the next session too.

### Hypotheses tested and DISPROVED for the ramp-link failure

Recorded so nobody re-runs them. The failure is stable and reproducible:
**2 of 8 ramps link, the same two, across every rebuild.**

| # | Hypothesis | Test | Result |
|---|---|---|---|
| 1 | Ramps overlap walls, so Recast cannot carve a corridor | perfect correlation (0.00 m2 on both working ramps, 0.13-2.26 m2 on all six failures); added `clear_ramps()`, regenerated, rebuilt, verified live actors match manifest | **DISPROVED** — link results byte-identical |
| 2 | Recast tile config is degenerate (AIB8's lead) | read the engine source | **DISPROVED** — log-ordering bug, see above |
| 3 | The navmesh is stale from the build when ramps still floated | `ProcessTileTasksAndGetUpdatedTiles` is async dispatch, so waited and re-probed after tiles settled | **DISPROVED** — identical results |
| 4 | Ramp heads do not reach their decks | measured head-to-nearest-deck gap from the manifest | **DISPROVED** — all 8 gaps are 0.00 m (one 0.02 m) |
| 5 | The target decks carry no navmesh | probed each deck a failing ramp serves | **DISPROVED, and it inverts** — the failing ramps' decks (007/020/015/006) all have nav at z410, while `Deck_001`, the deck the two WORKING ramps serve, probes NONE at its centre |

Finding 5 is the most interesting and is where the next session should start: the
correlation runs OPPOSITE to the obvious reading, which means the failure is
probably not about the deck surface at all but about which navmesh ISLAND the
ramp's own surface belongs to. Worth dumping the actual navmesh polygons/tiles
per ramp rather than probing points, which is as far as point-projection can go.

Also still unexplained and possibly related: **32 of 97 deck pieces carry no
navmesh**, and `Deck_001` is one of them despite being demonstrably walkable.

### 30 Aug (cloud) — one untested hypothesis for the ramp-link failure, and the
### experiment that settles it in ten minutes

Not a claim, a candidate. Offered because finding 5's inversion fits it and
none of the five disproved hypotheses do.

**Hypothesis: coincident coplanar abutment.** Every junction in this level is
built as an EXACT abutment — the ramp head measures a 0.00 m gap to its deck,
and deck slabs meet edge-to-edge with zero overlap. Recast does not voxelize
two separately-authored surfaces that share a plane the way a human reads them:
`rcFilterLedgeSpans` marks a span unwalkable when a neighbour span's height
differs by more than `walkableClimb`, and coincident faces from two meshes land
in the same or adjacent voxel rows depending on float epsilon. The result is
exactly what is being seen — navmesh that EXISTS on both surfaces but belongs
to two islands that never merge, and a scattering of slabs (32 of 97 decks)
that carve no navmesh at all while being demonstrably walkable. It also
explains why `Deck_001` — served by the two ramps that DO link — is itself one
of the 32: if the merge is decided at the junction rather than on the surface,
surface coverage and linking are independent, which is finding 5's inversion.

**The decisive experiment — do this before anything else, it is one map:**

Build a scratch map with exactly three pieces from the real kit: one
`BLK_Floor_400`, one `BLK_Floor_400` raised to deck height as the target, and
one `BLK_Ramp_800` seated by the same code path as BR_Aquarius. Nav bounds over
it, build, then path foot→top.

- **Links in isolation** → the geometry is fine and the fault is neighbourhood
  (tiles, overlapping pieces, or the pieces adjacent to the junction). Add the
  surrounding walls one at a time until it breaks.
- **Fails in isolation** → the junction itself is the fault, and the fix is
  cheap: give the ramp head a real **overlap** into the deck (start at 0.5 m,
  i.e. more than one nav cell) and sink the ramp top **3-5 cm BELOW** the deck
  top, so the transition is an unambiguous sub-`walkableClimb` step instead of
  a coincident plane. Re-run the isolation map to confirm before regenerating
  the whole level.

**Two numbers worth reading out while you are in there**, because the
hypothesis lives or dies on them: `RecastNavMesh-Default`'s `CellSize` and
`CellHeight`. At the default `CellHeight = 10 uu` a 400.00 vs 400.00 junction
is inside a single voxel, which is precisely where this failure mode lives.

If the overlap fix is the answer, it is a GENERATOR change (law 7) — say so
here and the cloud puts the head-overlap and the sink into
`gen_aquarius_kit.py`'s `ramp_instances()` rather than anyone hand-seating it.

**Second route up, independent of all this:** `BLK_Stair_200` is placed
nowhere, and BN31 authored it. A stair is a stack of sub-`walkableClimb` steps
with no coplanar junction at either end — if the hypothesis is right, stairs
would link where ramps do not, which makes placing one a *second* test as well
as a fallback route. Reference note 19's side hallways are two-level; stairs
belong there regardless.
