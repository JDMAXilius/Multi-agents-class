# TICKET — AIB8: raise the drop-link JumpLength to the sprint reach, and re-run the table

> STATUS: done — mac terminal 26 Aug 2026. Hypothesis REFUTED. 600 reverted to 400.
> The experiment also invalidated AIB7's "six specific positions" reading — see the Log.

AIB7 proved the remaining move refusals are genuine unreachability (30/30 `self=yes
goal=yes`) at six goal positions: 593, 604, 615, 713, 761, 771 uu, each retried 5 times.
This ticket tests one hypothesis about why: the generated jump links are too short.

## The physics, because the number is not free

`DefaultEngine.ini` derives the current value from the character, and says so:

```
JumpZVelocity 420, gravity 980  ->  air time 2*420/980 = 0.857s
MaxWalkSpeed 600                ->  flat-to-flat reach 0.857 * 600 = 514uu
JumpLength 400 "sits under the measured 514 so the generator cannot promise a gap the
bot cannot clear."
```

That reasoning is right and must be preserved. But it was computed at WALK speed only.
`CT_Combat.csv` carries `Movement.Sprint.SpeedMultiplier = 1.2`, so:

```
sprint speed  600 * 1.2 = 720 uu/s
sprint reach  0.857 * 720 = 617uu
```

**617 against an observed median of 615.** Three of the six stuck positions (593, 604,
615) sit just under the sprint reach; three (713, 761, 771) exceed it.

## What this ticket changes, and the cost it accepts

`BN_Drop.JumpLength` 400 -> 600. Flat gaps are drop links (depth 0 is inside
`JumpMaxDepth` 0..800), so this is the config that governs them.

The cost is explicit and is the reason this is a ticket and not a tweak: **600 is above
the 514 WALK reach.** A link generated at 600uu is a promise the body can only keep while
SPRINTING. A walking bot that takes one falls. The old 400 was safe for any gait; 600 is
not. It is chosen anyway because no value that is safe for a walking bot (<514) can reach
ANY of the six observed positions — a "safe" raise would be measurably useless, which is
worse than an honest risk.

## What this ticket does NOT claim

The logged distance is straight-line bot-to-goal, NOT gap width. A goal 615uu away may sit
across a much narrower gap. So this is a TEST of the hypothesis, not a fix known to work.
The three positions beyond sprint reach (713+) should NOT be fixed by this, and if they
are, the hypothesis was wrong and the real cause is still unfound.

## Kickoff (machine-checkable)

- requires: engine-installed
- owner_path: `Config/DefaultEngine.ini`, `docs/tickets/TICKET_AIB8_JUMPLENGTH.md`

## Steps (in order)

1. `BN_Drop.JumpLength` 400 -> 600, with the derivation in the comment.
2. Live match, `BotSystem=AIB`; re-run AIB7's table.
3. Read it honestly against three outcomes:
   - 593/604/615 gone AND 713+ remain  -> hypothesis CONFIRMED, cost accepted.
   - nothing changes                   -> gap width was never the cause; REVERT.
   - everything gone                   -> hypothesis was wrong about the mechanism; keep
     the result, distrust the explanation, and say so.
4. Watch for the predicted cost: bots falling / new "wedged" or stuck lines.

## Done when

- [x] JumpLength raised with its derivation recorded, then REVERTED
- [x] Table re-run and pasted; plus a revert-confirm run
- [x] Outcome judged: case 2 (revert), with a fourth outcome nobody predicted
- [x] Fall/stuck regression checked

## Log

_(terminal: outputs verbatim)_

### 2026-08-26 — refuted, reverted, and the baseline turned out to be a mirage

**The table.** Three matches, same map, same 7 bots, ~4 minutes each.

```
                        400 (run A)      600      400 (run B, revert-confirm)
refusals                         30     8515                          1385
distinct distances                6     1337                           122
min / median / max      593/713/771  364/1112/2651            269/437/2183
eliminations                     20       14                            25
ambition switches               708      456                           828
BorderForLinks                23 vx    34 vx                         23 vx
```

**600 is worse, and that part is solid.** Normalized to refusals per ambition switch —
the only fair unit, since the matches differ in length and activity — 600 scores 18.67
against run B's 1.67 at the same config: **11.2x worse**. Fewer kills and fewer switches
follow from bots that cannot path. Reverted to 400; the config comment now carries the
result so nobody raises it again without reading this first.

**And now the part I got wrong.** The revert did NOT return to 30 refusals. It returned
to 1,385. Two matches at an IDENTICAL config are **39x apart** (0.04 vs 1.67 refusals per
switch).

So AIB7's headline reading — "six specific goal positions, each retried 5 times, in a
tight 593–771uu band" — was an artefact of a single unusually quiet 1m47s match, not a
property of the arena. With more data the same config produces 122 distinct distances
from 269uu to 2183uu. The tight band was a small sample, and I presented it as a
geometric finding. It was not one.

That also dissolves the reasoning that led here: the "median 615 vs sprint reach 617"
coincidence, which looked like the whole answer, was a coincidence of six samples. The
physics in the Kickoff is still correct — sprint reach IS 617uu — but it was never
evidence about these failures.

**What still holds, now across 9,930 refusals in three matches:**

```
9,900  self=yes goal=yes    genuine unreachability   (99.85%)
   15  self=NO              THE BOT is off the mesh  (0.15%, run B only)
    0  goal=NO              no off-mesh goal, anywhere, in any run
```

Zero `goal=NO` across all three runs is a strong repeated result: AIB5's projection fix
is complete. The `self=NO` rows are new — they appear only in run B, so a bot does
occasionally end up off the navmesh itself. Rare, but it is a different defect from
unreachability and it is now visible because AIB7 built the instrument.

**The next suspect, and it is not this knob.** `BorderForLinks` scales with JumpLength
(23 vx at 400, 34 vx at 600) and BOTH exceed the tile size the builder reports:
`BorderForLinks (N vx) exceeds tileSize (0 vx)`. Widening the link border against a
degenerate tile size is what got worse. `TileSizeUU` and `CellSize` are unset
project-wide, so recast runs on engine defaults and the builder reports a 0-voxel tile.
That is where to look — and it should be looked at with REPEATED matches, because this
ticket has just shown that a single match cannot tell 0.04 from 1.67.
