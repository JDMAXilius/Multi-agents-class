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
      Grappleshot only, by design.
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
      * the derived scale: long axis 52 m is DERIVED, never measured. If
        you can measure the real Aquarius (Forge, or a known-length object
        in a reference shot), that single number corrects everything.
- [ ] Write the deviation list into this ticket as a numbered table with a
      severity (blocks play / breaks fidelity / cosmetic). The cloud
      regenerates the manifest from it — deviations go back into
      `Tools/blockout/gen_aquarius_kit.py`, never hand-fixed in the map
      (law 7: hand edits are lost on the next rebuild).

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
