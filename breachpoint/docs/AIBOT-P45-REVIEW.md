# W-REVIEW — Phases 4+5 (skill policies, integration, confidence) — barrier record

> 26 Aug 2026. Two reviewers, four surfaces, judged HEAD `ef821df` (the Phase-4/5
> landing). Verdicts: CONTAINMENT **PASS** · FAIRNESS **FAIL** (4 high) ·
> UTILITY/EXECUTION **FAIL** (4 high) · SERVER-ONLY **PASS** · LIFECYCLE **FAIL**
> (1 high). All four mechanical greps clean on both reviews. After dedup: **7 distinct
> blocking findings, all closed in the barrier commit** (one by honest withdrawal, not
> code). The reviewers' one-line class diagnosis held: StateTree instance data
> re-initialising on branch re-entry, and identities/sequences outliving what they
> identified — the grenade-cooldown lesson, found unapplied in four more places.

## The highs, and what closed them

1. **Settled aim error was exactly ZERO for most of every fight** (Expert 79.5% duty
   cycle — the perfect tracker F4 bans, arrived at slowly; pinned as intended by its own
   spec). → `ResidualErrorDegrees` ladder (2.0/1.1/0.65/0.40°), each below its rung's
   outer-half draw; the settle decays TO the floor; the spec pin now asserts the
   residual, plus a ladder-wide structural pin.
2. **The anti-flick law was bypassed three ways** (same-target re-peek free; settle paid
   in dead time; `GetUniqueID` is a recycled index, so a respawn could inherit a settled
   solution). → THE GAP RULE: the policy steps every engaged tick, so a step gap over
   `ReacquireGapSeconds` (0.30) IS a tracking loss — the next step pays the full
   outer-half switch draw. New spec pins the corner re-peek.
3. **The damage seam delivered the attacker's EXACT position, unbounded** — a sniper at
   3000uu through walls became a walkable memory point (the wallhack seed the blast gate
   exists to refuse; the FAIRPLAY M6 amendment's own text says DIRECTION). → the
   remembered point is now the attacker's bearing from the bot, capped at
   `AIB::EngageFadeEndUU`: inside the envelope the point is real, beyond it the bot
   searches that WAY, not THERE. Code now matches the written amendment — no new ruling
   needed.
4. **Expert Area Denial was provably inert** (its only caller sits in Engage, which
   needs a visible target; denial needs the opposite — mutually exclusive). → WITHDRAWN
   as a claim, not as code: the branch is marked DORMANT in the file, the specs stay
   (they prove the ladder's shape), and the Search-side caller is a REGISTERED DEBT —
   it must face the memory point before pressing, which is why it is not a two-line fix.
   No packet may claim the Expert grenade rung landed until the caller exists.
5. **The strafe leg-stamp was instance data** — reset on every Engage re-entry, so one
   authored 220uu step became up to ~1100uu per blink chain. → moved to
   `FAIBMovementState::LastActuatedLegStamp`, beside the leg clock it compares to.
6. **The melee cooldown was instance data** — a belief blink bought a swing 0.3s after
   the last, 5× the authored rate. → `FAIBMeleeState::NextSwingAtSeconds`, an ABSOLUTE
   deadline on controller state (nothing to decay, nothing to reset); and the
   recognition continuity clock (`InRangeSinceSeconds`) now clears on fire-state exit,
   so a branch gap cannot splice two approaches into one paid delay.
7. **Every life replayed a byte-identical draw sequence** (all three streams re-seeded
   from the constant controller id each possession) — a learnable tell that reset on
   death; and raw consecutive indices fed the LCG an evenly-spaced first-draw
   progression across the lobby. → `LifeIndex` + `HashCombine(GetTypeHash(id),
   GetTypeHash(life))`; still deterministic given (bot, life).

Also closed with the highs (mediums taken because the fix was one screen):
- **Burst-before-muzzle** (a fresh acquisition behind the bot held the trigger through a
  175° swing): a burst now STARTS only within 10° of the aim line and BREAKS beyond 30°.
- **Strafe escape ping-pong** (a perpendicular step from the outer 40% of the disc left
  the station-keeping radius; the mover aborted the leg — reads as glitching): the step
  destination is pulled back onto the engaged circle.
- **Self-damage credited the DEALT book** (own-grenade momentum read neutral): filtered
  at the game seam; Taken stands.
- **Confidence assembled entirely from unknowns published as KNOWN** (broken door →
  Assess 0.30, "confident"): `bConfidenceKnown` now requires vitals or damage history.
- **Crowd selectors read an unwritten 0 as "certainly alone"**: `NearbyAllies`/
  `NearbyEnemies`/`Outnumbered` now honour `bCrowdKnown` (the flag guarded only the
  confidence model).
- **Two silent failure paths got voices** (dropped damage notes at Warning;
  recognised-but-throttled grenade at Verbose).

## Registered to the risk register (law 8 — not blocking, not forgotten)

- Melee/grenade LADDER CONSTANTS calibrated to host numbers the module never receives
  (reach ~150uu, blast/arc assumptions in the 450 floor and band ceilings). Real fix is
  Phase 8's tier table carrying host-tuned rows; until then a host that retunes reach
  or arc past the comments' stated assumptions inverts parts of the ladder.
- The confidence spec's knife-edge row (H=0.4/Taken=0.5/d=1400) injects a
  `ConfidenceNorm` no competence level can produce at those facts (model floor 0.125 vs
  injected 0.05). The AXIS is real and spec'd; the reachability claim is not — re-pin on
  H=0.30/Taken=0.50/d=1200 (flips at Trained, P≈0.29/redraw) when the confidence specs
  are next open. The oscillation question itself PASSED: Retreat's 3s commit is a hard
  floor under any flip cycle.
- `CanEvadeBlast` has no caller (the promised blast-dodge consult does not exist yet) —
  every level's blast response is identical today. Same class as the denial debt.
- Novice `StrafeChance` 0.05 ≈ one step per 32s of engagement — effectively inert,
  documented as "never-ever reads as a statue" but buying nothing observable.
- `StepAimPoint` takes absolute world positions — lawful per the conventions block, but
  the worldless GREP cannot see world-reasoning on smuggled coordinates; the law's text
  reads stronger than its enforcement.
- Facts builder publishes `NearbyAllies` from a 10000uu query radius (the objective
  radius, 6.6× sight) — acceptable ONLY while `bCrowdKnown` stays false in BN (it does:
  the query returns 0 and the flag is never raised); revisit when a real ally feed lands.
- Nested-Think ordering assumption (possess-inside-PostGameplayEffectExecute) — one PIE
  breakpoint's worth of live proof, listed on AIB9's watch list.
- `EndPlay` cleans less than `OnUnPossess` (ledger/policy states not reset) — benign
  while controllers die with their match; a pooled-controller future inherits it.

## Explicit passes worth keeping (silence is not a verdict)

Confidence scales and never vetoes (the packet's claim, confirmed in the curves); the
misjudge stream is per-bot with no shared-stream path; no Assess input exceeds a
human's; the aim point derives only from the belief; the strafe rhythm does not leak
through control rotation; sprint-while-strafing is refused twice over (radii disjoint +
the host's own forward-intent gate); the damage seam cannot run client-side (three
independent gates); Think has no reentrancy; `GetLastDamage()` on a client is
unreachable behind the authority gate.

Spec totals after this barrier: **97** (AimPolicy 7 → 9; module was 95 after Phase 6).
