# TICKET — BN32: the metrics gym (TERMINAL, small, do it early)

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

- [ ] Rig script committed; measured sheet written into BLOCKOUT-KIT.md
- [ ] Every number tagged [measured] with the date
- [ ] Contradictions with the current kit listed here for the cloud
