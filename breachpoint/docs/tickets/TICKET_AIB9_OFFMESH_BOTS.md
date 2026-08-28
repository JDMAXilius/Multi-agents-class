# TICKET — AIB9: bots go off the navmesh, and the tile-config lead is dead

> STATUS: open — cut 26 Aug 2026 by mac terminal. Two results, one closing a lead and one
> opening a real defect. **28 Aug: the traversal half this ticket became famous for is
> DONE and has left — the DROP is fixed (JumpMaxDepth 800 → 1000; refusals 9.54 → 0.42
> per ambition switch) and the CLIMB was never navlinks' to answer: BN21's treads and
> AIB19's grapple took it. §2's own defect — bots leaving the mesh, `self=NO` — is
> UNMEASURED, and is now the only thing holding this ticket open. The 26 Aug entry's
> "WRITTEN, NOT COMPILED" instrument compiles.**

## 1. The tile-config lead is CLOSED — it was an engine bug, not our config

AIB8 named the recast tile configuration as the next suspect, on the strength of this
warning scaling with `JumpLength` (23 vx at 400, 34 vx at 600):

```
LogNavigationDataBuild: Warning: ComputeConfigBorderSizes: BorderForLinks (N vx)
exceeds tileSize (0 vx). This will increase memory usage during nav build.
```

**That lead is wrong and is now closed.** `FRecastNavMeshGenerator::SetupTileConfig`
(`RecastNavMeshGenerator.cpp`) does this:

```cpp
5263:  UE::NavMesh::Private::ComputeConfigBorderSizes(IsGeneratingLinks(), OutConfig);  // warns
...
5270:  OutConfig.tileSize = FMath::Max(FMath::TruncToInt(DestNavMesh->TileSizeUU / CellSize), 1);
```

The warning compares against `tileSize` **seven lines before that field is assigned**. It
is 0 at every call, so the warning fires unconditionally whenever link generation is on
and carries no information about any project's settings. Engine ordering bug.

Our real values are healthy: `TileSizeUU=1000`, `CellSize=19` (engine defaults,
`BaseEngine.ini:3052-3053`) give a true tileSize of **52 vx**, comfortably above the 23 vx
border. Unset was never a problem, and writing a tile config would have been a fix for a
non-problem.

**Do not chase this warning again.** It is noise by construction.

## 2. The real defect: bots leave the navmesh

AIB7's instrument reports `self=NO` when the BOT's own location will not project. Nothing
it asks for can path in that state — the goal is innocent. Across every measured match:

```
match                       refusals   self=NO   share
diag  (JumpLength 400)            30         0    0.0%
jump  (JumpLength 600)          8515         0    0.0%
revert(JumpLength 400)          1385        15    1.1%
rep-1 (JumpLength 400)           620        75   12.1%
```

75 in one match is not noise. This is a distinct defect from unreachability and has its
own likely causes: spawning off the mesh, a fall that ends outside nav bounds, a
knockback, or geometry the nav bounds volume does not cover.

## Kickoff (machine-checkable)

- requires: engine-installed
- owner_path: `docs/tickets/TICKET_AIB9_OFFMESH_BOTS.md` (investigation; any fix is a
  later packet with its own owner path)

## Steps (in order)

1. **Repeated baseline first.** Five matches minimum at one config. AIB8 proved a single
   match cannot tell 0.04 from 1.67 refusals per switch — two runs at IDENTICAL settings
   came out 39x apart. Report mean AND spread, never one run.
2. Log WHERE a `self=NO` bot is: location, and whether it is inside `BR_NavBounds`.
3. Correlate with the moment: fresh spawn, post-fall, post-knockback, or steady state.
4. Only then propose a fix.

## Done when

- [ ] Five-match baseline with mean and spread, per config
- [ ] `self=NO` incidence measured across those five, not one
- [ ] Location and timing of off-mesh bots characterised
- [ ] Cause named with evidence, or explicitly left open

## Log

### 2026-08-26 — cut

Runs so far at `JumpLength 400`, refusals per ambition switch: **0.04, 1.67, 1.22**. The
0.04 run is the outlier that misled AIB7 into a "six specific positions" reading.

A five-match sweep was launched and only run 1 of 5 completed — the background job died
after the first match. Run 1: 620 refusals / 507 switches = 1.22 per switch, of which 75
(12.1%) were `self=NO`. The sweep needs re-running before anything here is called a
baseline.

### 2026-08-26 — steps 2+3's instrument WRITTEN (cloud lead; "continue with the
roadmap" — this ticket blocks AIB11's hill proof and BN17, so its instrument is the
critical path). WRITTEN, NOT COMPILED; the harness half proven on a synthetic log.

- Every `self=NO` refusal line now carries the WHERE and the MOMENT, appended by
  `DescribeMoveFailure` only on that branch (the off-mesh bot's own position is the
  evidence; the other cases already say what they need):
  `| off-mesh self at (X, Y, Z) age=<s>s falling=yes|no velZ=<n> lastHit=<s>s|never`
  — age from a new `PossessedAtSeconds` stamp on the controller (set beside the
  per-life seed), falling/velZ from the pawn's movement component (the idiom already
  compiled at BNAnimInstance.cpp:237 and twice in this same TU), lastHit from a new
  const accessor on the damage ledger's existing TakenStamp (a stamp, not a norm —
  "how long ago" is the question).
- The harness (`80_aib_metrics.py`) counts `offmesh_self` and pre-buckets
  `offmesh_moments`: fresh_spawn_lt2s / falling / hit_within_1s — the ticket's own
  candidate causes as countable correlates (overlapping on purpose; they are weights,
  not a partition). Proven: 2 events, all three buckets, on synthetic lines in the
  exact C++ format.
- What this buys step 4: after the five-match re-run, the mean/spread question AND
  the cause question come out of the same logs — `age<2s` dominating says spawns,
  `falling` says the projector is being asked mid-air, `hit_within_1s` says
  knockback, and none-of-the-above says nav-bounds geometry, with (X,Y) clustering
  to point at WHERE. The fix packet then names its cause with evidence, per the
  ticket's own bar.

### 2026-08-28 — board-hygiene pass: what left this ticket, and what did not

Corrections only; nothing below was measured by this pass.

- **The instrument COMPILES.** The 26 Aug entry's "WRITTEN, NOT COMPILED" is stale —
  `DescribeMoveFailure`'s off-mesh clause, `PossessedAtSeconds`, and the damage-ledger
  stamp accessor are all in a clean build (all targets, verified this session). The
  harness half was already proven on synthetic lines. So steps 2+3 are ready to run and
  have not been run.
- **The drop half is FIXED** — `JumpMaxDepth` 800 → 1000, move refusals **9.54 → 0.42
  per ambition switch**. That is the number BN22 §3 audits as "the machinery is INTACT".
- **The climb half is CLOSED OUT OF THIS TICKET, as it should have been.** This ticket's
  own reasoning said it: a navlink promising a 400uu climb against a 90uu apex is a
  promise the body cannot keep. The answers were geometry and a grapple, not a link —
  **BN21** (26 walkable treads; mid-flight pawns 1 → 16 across 90 PIE samples, footprint
  hits 5 → 53) and **AIB19** (bots fire the Grappleshot; 5 ACTIVATED / 6 REFUSED in one
  match, roughly half still short of the standoff point — AIB19's open finding, not
  this ticket's).
- **BN17 is therefore UNBLOCKED**, and told so in its own header. Its stated gate was
  "hill reachability"; the reachability was the stairs.

**What still holds this ticket open, unchanged:** the four Done-when boxes are all about
`self=NO`, and none of them has a number yet. The five-match sweep died after run 1 on 26
Aug and has not been re-launched — one run is what this ticket was cut to forbid. Whoever
picks it up starts at step 1, with the instrument already in the build.
