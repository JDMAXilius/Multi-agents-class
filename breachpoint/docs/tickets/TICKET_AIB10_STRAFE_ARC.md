# TICKET — AIB10: strafe on an arc so the step can fill its leg

> STATUS: in-progress — mac terminal 26 Aug 2026.

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

_(terminal: outputs verbatim)_
