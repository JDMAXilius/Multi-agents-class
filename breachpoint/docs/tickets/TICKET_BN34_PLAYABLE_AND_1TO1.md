# TICKET — BN34: make it PLAY, and make it 1:1 (TERMINAL, iterative)

> STATUS: in-progress — mac terminal 30 Aug 2026 (256ed547). Gated on BN33.

> OWNER: **terminal**, with the cloud regenerating data between rounds.
> DEPENDS ON BN33. This is the ticket the founder actually cares about:
> "he needs to make the level playable… we want to stick with the
> references of the Aquarius Halo Infinite level, and we want to be as
> one to one as possible."
>
> **Standing warning from the founder, treat as fact:** what the cloud has
> produced may NOT be correct or faithful. It is a traced approximation
> validated only on geometry. Your eyes in the engine, against the
> reference images, are the higher authority.

## Part A — PLAYABLE (the walk rung)

The cloud proved geometry: one walkable ground region, 100% of decks
reachable, 8.9 s longest rotation, 2.3% team skew, parallel lanes, ramps
≤ 36.3°. **None of that proves the pawn walks.** Prove it:

- [ ] Walk every ramp UP and DOWN with the real pawn. Any ramp that fights
      the capsule, list it here with its label (`BLK_Ramp_00n`).
- [ ] Walk the perimeter: no gaps, no shoot-through seams, no place the
      pawn escapes the arena.
- [ ] Walk each of Hallway 1 / 2 / 3 end to end. The side hallways are
      TWO-LEVEL (founder reference note 19) — confirm both levels exist and
      connect.
- [ ] Stand on every deck: is it reachable by ramp, and is the drop back
      down survivable/intended?
- [ ] Jump/mantle audit: is any deck reachable that should NOT be (Halo's
      rule: no accidental clambers)? Decks are +4.00 m — ramps and the
      Grappleshot only, by design.
- [ ] Head-bump audit under every deck soffit (+3.60 m) and in every doorway.
- [ ] Stopwatch the routes with the real pawn and compare to the cloud's
      model: longest rotation 8.9 s, spawn-to-centre ~3.6 s. Report the
      real numbers — if they diverge, the cloud's walk model is wrong and
      needs correcting.
- [ ] Bots: run a match. Do they path both levels? (AIB19's grapple work is
      live; this map is where it gets exercised.)

## Part B — 1:1 (the fidelity rung)

The cloud's fidelity is measured against its own TRACE, not against the
game. Close that loop:

- [ ] Open the founder's reference set (`docs/design/reference/` — 25
      images: two per-floor overheads, the labelled skeletons, and 18
      in-game shots) beside the editor.
- [ ] Fly the same camera angles as the reference shots and capture
      matching screenshots. Put them side by side and list EVERY deviation:
      shape, proportion, height, missing feature, wrong opening.
- [ ] Check specifically the things the cloud KNOWS are unresolved:
      * `unresolved_capsules` in the kit JSON — 10 traced shapes that could
        not be anchored as ramps and were left OPEN. What are they really?
        Ramps? Stairs? Cover? Nothing?
      * the sunken centre trench (reference shots 1/6) — drawn at grade,
        below-grade geometry never modelled.
      * pass-2 kit pieces not yet placed anywhere: columns, the battery
        octagons at the base mouths, lane crates, deck railings, the 45°
        corner walls. The reference has them; the level does not yet.
      * the derived scale: long axis 52 m is DERIVED, never measured. If
        you can measure the real Aquarius (Forge, or a known-length object
        in a reference shot), that single number corrects everything.
- [ ] Write the deviation list into this ticket as a numbered table with a
      severity (blocks play / breaks fidelity / cosmetic). The cloud
      regenerates the manifest from it — deviations go back into
      `Tools/blockout/gen_aquarius_kit.py`, never hand-fixed in the map
      (law 7: hand edits are lost on the next rebuild).

## The loop

```
terminal: build -> walk -> screenshot -> deviation list (here)
cloud:    fix the generator -> regenerate manifest -> validator PASS -> push
terminal: pull -> re-run builder (idempotent) -> re-check
```

Repeat until the founder says the level reads as Aquarius and plays.

## Done when

- [ ] Walk rung complete, every box in Part A answered with observations
- [ ] Side-by-side reference comparison delivered to the founder
- [ ] Deviation table written, severities assigned, handed to the cloud
- [ ] At least one full loop closed (cloud fix -> rebuild -> re-check)
- [ ] Founder verdict recorded

## Log
