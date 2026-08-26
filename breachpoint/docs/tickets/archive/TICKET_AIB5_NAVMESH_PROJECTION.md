# TICKET — AIB5: every move goal is projected onto the navmesh before it is requested

> STATUS: done — mac terminal 26 Aug 2026 (1696297). Flee collapsed 267 -> 0.8 refusals
> per Retreat ambition. Residual refusals of a DIFFERENT cause recorded below, unfixed.

Cut from a live finding during AIB4's step-3 match. Retreat and Search are DECIDED
correctly and then never EXECUTE: the ambition engine picks them, the task asks the mover
for a path, and the mover refuses — every frame, for the whole match.

Measured in the AIB4 proof match (92,762 log lines, ~4 minutes, 7 bots):

```
33,095  flee path REFUSED — failing loudly, not standing (F7)     (vs 124 Retreat ambitions)
20,650  cannot path to the last-known spot — search fails loudly (F7)
```

That is ~54k of 92k lines — more than half the log is one defect repeating.

## Root cause

`Execution/AIBStateTreeTasks.cpp` hands the mover RAW world points. The flee goal is pure
arithmetic:

```cpp
Goal = Pawn->GetActorLocation() + Away * InstanceData.FleeDistanceUU;   // :640
```

Nothing guarantees that point is on the navmesh — off a ledge, inside geometry, past the
arena edge — and `MoveToLocation` refuses an off-navmesh goal. The SAME function's other
branch is correct: with no threat point it draws from `GetRandomReachablePointInRadius`,
which is navmesh-valid by construction, and that branch does not spam. The two branches
disagreeing inside one function is the tell.

Search has the same shape at `:727` — a remembered sighting is a world position that need
not be standable.

The decisive comparison, and the reason the older bot framework does not have this bug:

| module | `ProjectPointToNavigation` calls |
|---|---|
| the game module's AI | 5 |
| `Source/AIBot/` | **0** |

The older framework projects every goal before asking for a path. This module never
projects once.

## The fix (one place, not five)

There are FIVE `MoveToLocation` sites in the file (`:310`, `:341`, `:654`, `:727`,
`:871`), and four of them pass a remembered or derived point. Fixing only the two that
log loudly would leave the other two silently refusing. One file-scope helper projects
then moves; every site routes through it.

Projection failure must NOT be swallowed: fall through to the original point and let the
mover refuse, so a genuinely unreachable goal still fails loudly (F7 kept).

## Kickoff (machine-checkable)

- requires: engine-installed (editor needed only for step 4)
- owner_path: `Source/AIBot/Execution/`, `docs/tickets/TICKET_AIB5_NAVMESH_PROJECTION.md`

## Steps (in order)

1. Add the projecting helper; route all five move sites through it.
2. **Rung 1** — `./Tools/run-ubt.sh BreachpointEditor Breachpoint`.
3. **Rung 2** — `./Tools/run-specs.sh AIBot`: 91 expected, 0 failures, reconciled.
4. **Live re-measure** — a `BotSystem=AIB` match of comparable length. The two refusal
   counts must COLLAPSE, and Retreat must be observed actually moving. Paste before/after.
5. Four mechanical checks, pasted empty — check 1 (boundary) matters here: the helper's
   comment must not name the game module or the grep fails.

## Done when

- [x] All five move sites route through one projecting helper
- [x] Rung 1 PASS (Editor + Game; Server environmental)
- [x] Rung 2: 91/91/0, reconciled
- [x] Live before/after refusal counts pasted (three matches); Retreat no longer refuses
- [x] Four mechanical checks pasted, empty

## Log

_(terminal: outputs verbatim)_

### 2026-08-26 — three matches, and the first fix was only half of one

Two commits, because the first measurement proved the first fix insufficient.

`94926ef` projected every goal at the single door to the mover (all five sites).
`1696297` made the flee task fall back to a reachable point when its DIRECTED goal will
not project.

**The measurement, normalized.** Raw counts are misleading here: match 2 had roughly a
third the activity of match 1 (25 eliminations vs 79), so a raw drop flatters the fix.
Per-ambition rate is the honest unit.

```
                                    BEFORE   PROJECT ONLY   + FLEE FALLBACK
flee path REFUSED  (raw)             33095           6745                25
  per Retreat ambition               266.9          168.6               0.8
                                                   -36.8%            -99.7%

cannot path to last-known (raw)      20650             70               415
  per Search ambition                 67.5            0.6               5.7
                                                   -99.0%            -91.6%

eliminations                            79             25                29
```

**Flee is fixed.** 266.9 → 0.8 refusals per Retreat ambition. Projection alone got only
−37%, because "straight away from the threat" is a DIRECTION, not a promise that anything
is standable there; when the directed point lies further off-mesh than the extent
reaches, projection fails, the helper hands the raw point through as designed, and the
mover refuses. Reusing the function's existing reposition draw closed it.

**Search is much improved but I will not put a number on it.** The two post-fix matches
disagree by nearly 10x (0.6 vs 5.7 per ambition). These are single uncontrolled matches
with different spawns, deaths and geometry — not repeated trials — so the honest claim is
"down roughly an order of magnitude from 67.5", not a specific figure. Anyone re-running
this should expect spread.

**Four mechanical checks: all four EMPTY.** Check 1 constrained the fix's own comments —
the helper says "the sibling framework" rather than naming the game module, because
naming it would trip the boundary grep this module is defined by.

#### Residual, unfixed, and a DIFFERENT cause — owed a follow-up

With flee's spam gone, two refusals that were buried underneath are now the top lines:

```
415  cannot path to the last-known spot — search fails loudly (F7)
390  could not path to the belief — closing refused (F7)     <- Engage, Verbose, was hidden
```

Both pass a REMEMBERED point — a place a body actually stood — which is exactly the kind
of point that projects successfully. So projection is very likely not what is failing
here; the likelier cause is genuine unreachability (the goal is on a navmesh island the
bot cannot reach, i.e. a navlink question) rather than an off-mesh goal. That is a
HYPOTHESIS, explicitly not measured, and it is a different defect from the one this
ticket fixed. It wants its own ticket and its own instrumentation — the current logs
cannot distinguish "off-mesh" from "unreachable", which is itself the first thing to fix
about them.

Locomotion is live in the fixed run (49 `jumped to clear whatever it is wedged on`, plus
grenades, melee swings and weapon cycling), so the bodies are moving; the refusals above
are specific goals being declined, not a frozen brain.
