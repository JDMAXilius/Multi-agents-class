# TICKET — AIB5: every move goal is projected onto the navmesh before it is requested

> STATUS: in-progress — mac terminal 26 Aug 2026.

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

- [ ] All five move sites route through one projecting helper
- [ ] Rung 1 PASS (Editor + Game; Server environmental)
- [ ] Rung 2: 91/91/0, reconciled
- [ ] Live before/after refusal counts pasted; Retreat observed moving
- [ ] Four mechanical checks pasted, empty

## Log

_(terminal: outputs verbatim)_
