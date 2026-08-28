# TICKET — AIB10: strafe on an arc so the step can fill its leg

> STATUS: in-progress — mac terminal 26 Aug 2026. **28 Aug: it COMPILES** (the 26 Aug
> entry's "WRITTEN, NOT COMPILED" is stale) **and the fix landed** — BN22 §5 put
> FightRangeUU (900) on BOTH the strafe and MoveNearBelief and rebanded the spiral clamp
> to [280, 900], which is this ticket's root cause taken out at the geometry. Still open
> on the one thing it exists for: **the live before/after has not been pasted.**

Founder report: the strafe distance is far too short. It is, and the fixed 220uu step is
not the reason — the geometry is.

## Root cause

`FAIBStrafeTask` steps PERPENDICULAR to the belief line, and is gated to run only while
within `EngagedRadiusUU = 350` of the belief — deliberately mirroring
`FAIBMoveNearBeliefTask`'s `AcceptanceRadiusUU = 350`, which is exactly where a bot parks.

A perpendicular step always INCREASES range: from distance `d`, a lateral `L` lands at
`sqrt(d^2 + L^2)`. So the room before the step leaves its own gate is `sqrt(350^2 - d^2)`:

```
d=200uu -> 287uu of room     d=340uu ->  83uu
d=300uu -> 180uu             d=350uu ->   0uu
```

At the distance bots actually fight from, the step is squeezed to nothing. A bot at 340
that steps 220 lands at 405uu, outside the gate — strafe stops and MoveNearBelief walks it
back. That thrash IS the too-short strafe.

Second, smaller mismatch: one step per LEG, but the step is a constant while legs are not.
At 600uu/s a leg covers 210–1200uu depending on rung, against a fixed 220uu — calibrated
for Expert's SHORTEST leg, so every other rung steps once and stands for 60–80% of its leg.

## The fix

Strafe along an ARC around the belief instead of a chord away from it. Rotating the
bot's own bearing about the belief by `StepDistance / d` radians keeps range to the target
CONSTANT by construction, so a step can never leave the gate however long it is. That is
also what strafing physically is — circling an opponent, not backing away sideways.

With range no longer the limiter, the step derives from the leg it must fill:
`remaining leg seconds * pawn max speed`, so footwork covers the whole leg at every rung
instead of a constant tuned for one.

Bounds kept: a per-leg arc cap so a long leg cannot spin the bot around the target, and a
floor so a nearly-expired leg still reads as a step.

## Kickoff (machine-checkable)

- requires: engine-installed
- owner_path: `Source/AIBot/Execution/`, `docs/tickets/TICKET_AIB10_STRAFE_ARC.md`

## Steps (in order)

1. Arc geometry + leg-derived step distance.
2. **Rung 1** — `./Tools/run-ubt.sh BreachpointEditor Breachpoint`.
3. **Rung 2** — `./Tools/run-specs.sh AIBot`: 91 expected, reconciled.
4. Live: strafe-refusal count and lateral travel per leg, before vs after.

## Done when

- [ ] Arc geometry lands; range to belief is invariant under a strafe step
- [ ] Step distance derives from the leg, not a constant
- [ ] Rung 1 PASS; Rung 2 91/91/0 reconciled
- [ ] Live before/after pasted

## Log

**26 Aug (cloud lead, on the founder's "continue with AIB10 strafe opportunity") —
the 182:1 number is not an opportunity measurement, and the instrument is now fixed
so the next run produces one. WRITTEN, NOT COMPILED (the harness half is proven —
synthetic-log asserts green).**

- **Why 182:1 meant nothing:** the "strafe held — outside the engaged radius" line
  fired EVERY TICK while outside the gate; the "strafe leg" line fires once per
  ~0.35–2s leg. Dividing the two divides frames by legs — at 60fps, 182 holds per leg
  is ~3 seconds outside per executed leg, which could be a bot denied 95% of its fight
  or one at 50% under high fps. The ratio cannot distinguish them. (The AIB8 lesson
  again, one layer up: a measurement whose units don't match is an impression with
  digits.)
- **The re-instrument (this change):** the gate hold is now a SPELL with edges, state
  on `FAIBMovementState` (per-life, survives Engage re-entry like the leg stamp):
  one "strafe held" line when a visible-target bot first leaves the gate, one
  "strafe opportunity back — X.Xs outside (reentered|target lost)" line when the
  spell ends. Holds and legs are now commensurate (spells vs legs), and denied time
  is summable. Harness: `strafe_holds` counts spells (NOT comparable with pre-26-Aug
  logs — those were frame counts; the regex comment says so), new
  `strafe_denied_seconds` and `strafe_spell_ends` by reason. Proven:
  2 spells / 2 legs / 10.5s / {reentered:1, target lost:1} on a synthetic log.
- **Re-measure protocol (terminal):** five Marine-FFA logs with LogAIBot Verbose.
  The decision number is `strafe_denied_seconds` vs (legs × mean leg seconds) —
  the true denied:stepping split per fight — plus the spell-end reasons (a fight
  ending by "target lost" while outside means the whole fight happened beyond 350uu).
- **The decision those numbers feed (NOT taken now — measure first):** if denied time
  dominates, the opportunity fix is an architecture question with two honest options:
  (a) widen the strafe gate beyond `EngagedRadiusUU` and arbitrate the two movers
  (MoveNearBelief owns closing, strafe owns lateral once the mover stations — needs
  an explicit hand-off, today's radii ARE the arbitration); or (b) accept that AIB
  strafes only at station range and let the fade/stand-off band pull fights inward.
  Option (a) is the Halo-fidelity answer (Infinite bots strafe at mid-range) but
  touches the mover contract; it gets its own packet if the numbers demand it.
- Collision note: this ticket is terminal-owned; the cloud took the instrument half
  on the founder's word with no terminal pushes in flight (fetch clean at 5af5041).
  The behavior half stays the terminal's after the re-measure.

### 2026-08-28 — board-hygiene pass: compiled, landed, unmeasured

Nothing re-measured here. Three corrections:

1. **"WRITTEN, NOT COMPILED" (26 Aug entry) is false.** All targets build clean this
   session; the AIBot suite reads 119/119/0.
2. **The fix shipped inside BN22 §5**, not under this ticket's own commit — "footwork
   owns the fight range": FightRangeUU (900) on the strafe gate AND MoveNearBelief, the
   spiral clamp rebanded [280, 900] with the chord ratchet kept as closing pressure, and
   the param renamed so the authored tree's stale serialized 350 drops to the new
   default. That is the root cause in this ticket's own Root-cause section — the 350
   gate that squeezed every lateral step to nothing — removed.
3. **BN22's review barrier also fixed a survivor of the same bug** (M3, aib-critic): the
   350-beeline lived on at `EnterState`, re-issued on every belief blink, and now mirrors
   the fight-range yield.

Boxes stay unchecked. The first two are code claims that a reader could verify by
inspection, but this ticket's bar is a **measured** one — its whole cut was that a number
(182:1) meant nothing without the right instrument, and the fixed instrument has not
produced a run yet. BN22's own proof list carries the same row (`strafe leg` lines at
350-900, `strafe_denied_seconds` vs the 26 Aug baseline); one run closes both.
