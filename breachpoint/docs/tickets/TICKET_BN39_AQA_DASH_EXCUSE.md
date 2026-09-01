# TICKET — Give the speed detector a launch excuse, and pin it with a test

> STATUS: open — cut by the mac terminal 31 Aug 2026, from BN24's first real run. Small,
> files-only, and the test proves it without an engine.

Founder directive: BN24's 300s run produced two `speed_violation` findings that are **false
positives**, and a false positive in a QA agent is worse than a missed bug — it teaches the
reader to discount the report. `BNAQA::SpeedViolation` convicts a legitimate dash. Fix the
excuse policy, and add the excuse case to the headless suite so it can never regress.

**Ordering law:** none. BN24 may ship its report before this lands — its README names both
rows as false positives already, which is the honest disclosure. This ticket removes the
need for that disclosure.

## The evidence

From `assignments/09-adversarial-qa/report/` (BR_Spillway, 02:25:17Z, sim 177.8 and 178.9 —
1.1s apart, one dash and its decay):

```
speed_violation  ground speed 1800 uu/s vs MaxWalkSpeed 250 (x7.20)  during ability_mash
speed_violation  ground speed  656 uu/s vs MaxWalkSpeed 250 (x2.62)  during ability_mash
```

Why it is not a cheat:

- `Source/BreachpointNext/AbilitySystem/Abilities/BNMovementAbilities.h:181` —
  `float DashSpeedUU = 2000.f;`
- `...BNMovementAbilities.cpp:491` —
  `LaunchCharacter(Direction * DashSpeedUU, /*bXYOverride=*/true, /*bZOverride=*/false);`
  **Z is not overridden**, so a grounded dash never leaves the ground.
- `Source/BreachpointNext/QA/BNAQADetectors.h:95-99` — the rule's only excuse is
  `!bOnGround` ("falling and grapple flight have their own legal envelopes"). A dash is a
  third legal envelope the policy never learned, and it is the one that keeps its feet down.
- The `250` cap is not the base walk speed. It is the ADS slow-GE's `MoveSpeed` arriving via
  `Source/BreachpointNext/Characters/BNCharacter.cpp:551`, so `x7.20` measures a dash against
  a deliberately-reduced cap — the envelope moved, not the pawn.

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- requires: **files-only** (the fix is in an engine-free header; the test is stock g++)
- `Source/BreachpointNext/QA/BNAQADetectors.h` compiles with `g++ -std=c++17 -Wall -Wextra`
  and `assignments/09-adversarial-qa/tests/detector_tests.cpp` passes 44/44 BEFORE any edit
- `BNAQADetectors.h` stays free of every engine type — if a fix needs a UE type it is in the
  wrong file (BN24's step 1 states this and it still binds)
- owner_path: `Source/BreachpointNext/QA/` `assignments/09-adversarial-qa/`

## Steps (in order)

1. **Decide the excuse shape, and write the reason in the Log.** The rule cannot see a dash
   today because it takes only `(bOnGround, Speed2D, MaxWalkSpeed)`. Two candidates:
   - **a · a recency window** — the caller passes seconds-since-last-launch, and the rule
     excuses a body inside it. Honest and cheap; needs the controller to observe launches.
   - **b · an absolute ceiling** — excuse anything at or under `DashSpeedUU`, convict above.
     Simpler, no new plumbing, but it blesses a permanent 2000 uu/s glide, which is exactly
     the exploit shape this detector exists to catch.

   Recommendation: **(a)**. (b) trades a false positive for a false negative, and the false
   negative is the one that lets a real speed hack through.
2. **Change `BNAQA::SpeedViolation`** in `BNAQADetectors.h` — new parameter, excuse policy
   rewritten in the comment the same way every other rule states its own. **Do not** restate
   `DashSpeedUU` there: the number lives in `BNMovementAbilities.h` and `verify.sh` fails the
   build if a threshold is duplicated into the game code. If the window needs a constant, it
   is a `Thresholds::` entry with a one-line justification.
3. **Update the call site**, `BNAdversarialAgent.cpp:486`, to supply the new argument. The
   controller must OBSERVE the launch, not guess it — the probe presses `Input.Dash` itself,
   so it already knows when it dashed; a pawn launched by anything else is a different
   question and belongs in the Log, not in a guess.
4. **Add BOTH cases to `tests/detector_tests.cpp`**: a dash inside the window is excused, and
   a body still over the envelope well after the window is convicted. The suite's whole
   discipline is that every rule is pushed through its firing case AND its excuse case.
5. **Verifier**: `cd assignments/09-adversarial-qa && ./verify.sh` — the detector count rises
   from 44 and every case stays green. Then re-run `bn.aqa.start 300` on `BR_Spillway` per
   BN24 and confirm `ability_mash` no longer reports a dash. Note BN24's open PIE question:
   let the match run ~90s before issuing the command.

## Done when

- [ ] Step 1 decision recorded in this Log with its reasoning
- [ ] `BNAQA::SpeedViolation` excuses a dash and still convicts a sustained overspeed
- [ ] `detector_tests.cpp` covers both, and the suite passes with a count above 44
- [ ] `verify.sh` exits 0, including its no-duplicated-threshold check
- [ ] A fresh 300s run reports no `speed_violation` for a plain dash
- [ ] `assignments/09-adversarial-qa/README.md`'s false-positive disclosure updated to say
      it was found, understood and fixed — not silently deleted
- [ ] Findings + decisions written to this ticket's Log

## Notes

- Crew: builder edits the header, the call site and the tests · verifier re-runs the suite
  and one PIE run · critic REFUTES with one question — *does the new excuse admit any
  sustained speed a cheat could hold?*
- Binary files this ticket OWNS: none
- Out of scope: the `stuck_state` findings from the same run (real, and their own ticket);
  changing `DashSpeedUU`; adding new detector classes — the assignment fixes seven at seven.

## Log

**31 Aug 2026 — mac terminal, cut from BN24's run.** Evidence above gathered from the report
plus the movement ability source before the claim was made; the first reading of these two
rows was "a movement exploit", and reading `LaunchCharacter`'s `bZOverride=false` is what
turned it into a detector bug instead. Recorded because the direction of that correction is
the point: the probe was right that something was odd, and wrong about what.
