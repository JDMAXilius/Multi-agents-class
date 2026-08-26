# TICKET — AIB16: an ambition whose branch fails must yield

> STATUS: done — mac terminal 26 Aug 2026. Starvation FIXED and proved live.
> The hill's own reachability is a SEPARATE open question, untouched by this.

Cut from AIB11's HIGH finding. Scoring alone cannot tell a want that is merely LOSING from
one that is IMPOSSIBLE: a failing branch scores exactly what it scored before, so it wins
again, forever, and every other behaviour starves.

## The mechanism

Hysteresis and commit both make an incumbent STICKIER, so neither could be the answer.
This is the one mechanism pushing the other way: the executor reports a branch it could
not run, and that want scores ZERO for a window.

Zero rather than "less", deliberately — the engine already rules that *an incumbent whose
fresh score is zero has declared itself impossible* and releases its commit on the spot
(THE VETO). Suppression reuses that ruling rather than inventing a second way out.

- Applied to the RAW score, so the debugger's scoreboard shows the zero. A suppressed want
  still reading full utility in the instrument would hide the very class of bug this kills.
- Strikes ESCALATE (3s → 6s → …, capped 20s): one failure may be a blocked doorway, ten is
  a want that cannot be served at all.
- Strikes are FORGOTTEN after a 10s clean spell, or a bot failing once a minute would
  accumulate its way into permanent silence.
- Cleared by `ResetArbitration`, so a respawn never inherits a dead life's strikes.

Three call sites, all in `FAIBMoveToObjectiveTask` — including the no-POI path, which was
AIB11 run 1's exact deadlock. Wiring only the two "cannot reach" paths would have left the
original failure intact.

## Proof

Specs **117/117/0** (114 + two new: the starvation case, and escalate/forget/respawn). The
starvation spec also proves the veto releases a COMMITTED incumbent, since the mode want
held the commit when it went to zero.

Live, `BotSystem=AIB`, hill on, same arena and build:

```
                       hill BEFORE   hill AFTER   slayer (control)
ambition switches                7          589               2277
eliminations                     0           32                 76
cannot-reach failures          226           42                  0
suppressions fired               —           42                  —
```

42 suppressions against 42 failures — 1:1, every failure reported. One bot's trace:

```
Mode.Hold (0.72) over Roam (0.20)      wins on merit
cannot reach the objective (F7)        branch fails
Roam (0.20) over Mode.Hold (0.00)      SUPPRESSED — another want runs
Mode.Hold (0.90) over Roam (0.20)      recovers and tries again
cannot reach (F7)
Search (0.69) over Roam (0.20)         goes and does something else
Engage (0.77) over Roam (0.20)         fights
```

## What this does NOT fix — stated plainly

The bots still **never hold the hill**: zero objective points, and 42 `cannot reach`
failures remain. This ticket fixed the STARVATION, not the reachability. The bots now
cycle — try, fail, go fight, try again — which is correct behaviour and a large
improvement over freezing, but it is not the objective loop working.

Whether the hill at (2000,1300,405) is genuinely unreachable is now answerable, because
the failure is no longer masked by a deadlock. That belongs with AIB9 (bots leave the
mesh / goals unreachable) and needs the `DescribeMoveFailure` diagnostic wired into
`FAIBMoveToObjectiveTask`, which currently logs its failures without it.

`bHillEnabled` restored to False and `ScoreLimit` to 7 — slayer is the shipped default,
and the objective still does not pay out.
