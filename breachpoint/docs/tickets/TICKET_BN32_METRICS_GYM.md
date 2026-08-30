# TICKET — BN32: the metrics gym (TERMINAL, small, do it early)

> STATUS: in-progress — mac terminal 30 Aug 2026 (256ed547). CDO measurement already done this session; rig script outstanding.

> OWNER: **terminal**. 30 minutes. Everything downstream quietly assumes
> these numbers; right now they are UE defaults from research, tagged
> [thin], not OUR measured values.

## Why

Research law (`docs/design/BLOCKOUT-KIT.md` §1.1): a blockout is built
against a MEASURED metric sheet, never copied. Our cover heights, corridor
widths, ramp slopes and the 2.20 m head clearance are all judged against
numbers we have not verified for the BREACHPOINT pawn.

## Do

1. Build a scratch level with a measuring rig: a stair of ledges at 0.20 m
   steps to 2.0 m, gaps 0.5-4.0 m, crawl heights 0.8-2.4 m, ramps at 20/25/
   30/35/40/45 deg. Generate it with a committed script under
   `Tools/blockout/` (law 7), not by hand.
2. PIE with the real pawn and measure:
   capsule radius + half-height · crouched half-height · eye height ·
   walk / sprint speed · JumpZVelocity and the actual ledge height cleared ·
   MaxStepHeight · the steepest ramp the pawn walks up without sliding ·
   mantle/clamber reach if BN23's grapple is not the only vertical verb.
3. Write the measured values into `docs/design/BLOCKOUT-KIT.md` §1.1,
   replacing the [thin] UE defaults, each tagged **[measured, <date>]**.
4. If any measured value contradicts the kit (e.g. crouch is not ~1.10 m,
   so `BLK_HalfWall_200` is not crouch cover; or walkable slope < 45 deg),
   say so LOUDLY here — the cloud regenerates the kit and manifest from the
   corrected numbers.

## Done when

- [x] Rig script committed; measured sheet written into BLOCKOUT-KIT.md
- [x] Every number tagged [measured] with the date
- [x] Contradictions with the current kit listed here for the cloud

### 30 Aug — measured off the CDO, and the rig that will confirm it

**Rung named honestly (law 6): these are CDO values read from the live editor,
not a pawn observed walking.** The rig exists so the walk can contradict them;
it has not been run yet. "The numbers are read" is not "the body behaves".

Method: the editor was down (the `unreal-mcp` server lives inside the editor
process, so its absence was the editor's absence, not a missing capability).
Relaunched with `-ModelContextProtocolStartServer` and driven over its JSON-RPC
endpoint. Values read from `/Game/BN/Characters/BP_BNCharacter`'s CDO —
`CharMoveComp` and `CollisionCylinder` — i.e. the SHIPPED pawn, not the C++
class and not engine defaults. Toolset used: `ObjectTools`
(`search_subclasses`, `get_properties`). Full sheet: `BLOCKOUT-KIT.md` §1.1.

### The three contradictions — step 4's "say so LOUDLY", for the cloud

1. **The capsule is `hh 96`, not 88 — the body is 1.92 m, not 1.76 m.**
   BLOCKOUT-KIT.md had been reasoning against a character 16 cm shorter for
   every clearance claim it made. Nothing in the current kit actually breaks
   (min overhead is the 3.60 m under-deck soffit, the doorway opening is
   2.40 m) but no margin was ever as wide as written. Silver lining: 1.92 m
   is Halo Spartan scale (2.0-2.2 m), not mannequin scale, which is why
   343's Forge metrics port across as cleanly as §1.1 assumed they would.

2. **The navmesh promises what the body cannot do.** RecastNavMesh agent
   height is **144 cm** against a **192 cm** capsule (radius matches at 34,
   slope 44 vs 44.765 is correctly stricter). Recast will floor any overhead
   in the 1.44-1.92 m band and route a bot into a gap the capsule cannot
   enter. NOTHING VIOLATES IT TODAY — this is a rule for pass-2, where
   railings and low ducts are exactly where it would happen. Kit law now:
   no piece may create an overhead in 1.44-1.92 m, or `AgentHeight` goes to
   192. Same defect class as AIB9's drop-link gap.

3. **The traversal band is 0.45-0.90 m, and it is EMPTY.** Walk up to 45 cm
   (MaxStepHeight); jump up to 90 cm (apex from JumpZ 420 / gravity 1,
   JumpMaxCount 1); nothing above that without the grapple, because BN has
   no clamber. Audited all fifteen kit pieces against it: Curb 0.15 and
   Pedestal 0.40 are walk-ons; HalfWall 1.10, Rail 1.00, Crate 1.00 and
   Battery 1.20 all sit ABOVE the apex as hard blocks. The band is clean by
   luck, not by rule — it is a rule now.

   **One catalog correction falls out of it:** the roster calls Battery_240
   (1.20 m) and Crate_100 (1.00 m) *mantle height*. There is no mantle in
   BN and both are above the 0.90 m apex. They are COVER, not traversal.
   The "never scales" rule on them is right; the reason written beside it
   was wrong. Cloud: fix the rationale text, not the numbers.

Cover math now exact: a 1.10 m HalfWall hides the 0.80 m crouched body with
0.30 m spare, and leaves 0.50 m of the 1.60 m standing eye exposed — chest
cover, as designed. Crouch cover is CONFIRMED, not assumed.

### The rig — `Tools/blockout/make_metrics_gym.py` (committed, NOT yet run)

Its own new map `/Game/Maps/BR_MetricsGym`; never touches BR_Aquarius,
BR_Arena01 or /Game/Blockout/. 98 actors: 10 ledges 0.20-2.00 m, 8 gaps
0.50-4.00 m, 11 clearance gates, 6 ramps at 20/25/30/35/40/45 deg, 39
labels, a PlayerStart and a nav bounds volume. Single tag `BN32_MetricsGym`,
destroyed before rebuild, so a re-run is a clean rebuild.

Two deliberate choices worth knowing:
- The gates include **1.44 m and 1.92 m explicitly**, and the labels state
  the prediction ("NAV AGENT HEIGHT - bot fits, BODY 1.92 DOES NOT"), so PIE
  either confirms finding 2 or contradicts it in the player's face. The nav
  volume is there so the discrepancy becomes a screenshot of a generated
  navmesh rather than a claim.
- `plan()` runs without `unreal`, so `python3 Tools/blockout/make_metrics_gym.py`
  self-checks the geometry offline: **selfcheck OK: 59 boxes, 39 labels, 98
  actors**, asserting every ledge top face, lintel underside, gap void and
  both ramp endpoints. It fails loudly if anyone retunes a knob.

**Carried risk, discovered while writing it:** `-run=pythonscript` CANNOT
spawn actors — `bn21_stairs_mcp.py` records 82 of 82 spawns returning None
because a commandlet has no initialised editor world. The rig (and BN33's
builder) must run in a LIVE editor, via `-ExecutePythonScript` or MCP.
**BN33's ticket text prescribes the commandlet and is wrong on this point.**
