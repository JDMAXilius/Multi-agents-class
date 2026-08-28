# TICKET — AIB7: the skill ladders reach the body (Phase 4 integration)

> STATUS: open — cut 26 Aug 2026 by the cloud lead. The integration is landed and
> ~~**WRITTEN, NOT COMPILED**~~ **COMPILES** (corrected 28 Aug 2026: all targets build
> clean and the module suite reads 119/119/0, which supersedes this ticket's 91/91 —
> Phases 6-10 landed on top of this integration and could not have if it had not
> compiled). Steps 1–2 are answered. What keeps it open is step 3 (LIVE EDITOR, tree
> rebuild — a NEW node struct) and step 4's `BotSystem=AIB` PIE observations; neither
> has been run.

Phase 4's second half: the four proven policies now DRIVE the proven task layer. The
principle throughout: the live-PIE actuators the terminal built stay; the policies add
the COMPETENCE dimension on top.

- **Aim (F4 executes at last)** — `FAIBFaceBeliefTask` steers at the policy's aim point:
  the belief displaced by the level's held, decaying angular error (cone draw → settle →
  redraw; full reset on target switch). The bot aims where it believes MINUS how good
  its hands are. TurnDegreesPerSecond still bounds the swing (no snap path).
- **Melee** — two gates, two owners: the REACH stays the held weapon's through the door
  (`GetMeleeRangeUU` × commit fraction, live distance); the RECOGNITION is the level's —
  `FAIBMeleePolicy::ShouldMelee` stepped every tick, so the continuous-range reset law
  holds. An Expert reads the closing fight early; a Novice realises at point-blank and
  takes a beat more.
- **Grenade** — the fixed band constants are RETIRED; the call is now the policy's
  recognition ladder (Novice never / opener / finisher-on-damage-dealt / denial) on the
  level's consider cadence, read from the FACT snapshot (a throw is a decision — one
  info door; the one named exception to the fire task's live-distance rule). The
  controller's 8s throttle still owns the cadence ceiling.
- **Strafe (NEW node: `FAIBStrafeTask`)** — the movement policy decides the rhythm
  (strafe chance / leg cadence / juke, per-life state on the controller so a branch
  blink cannot reset the dance); the task actuates ONE lateral navmesh-projected step
  per leg, perpendicular to the BELIEF line — the host's compiled strafe geometry,
  transcribed. Active only inside the station-keeping radius (350uu, mirroring
  MoveNearBelief's acceptance) so two tasks never fight over pathfollowing. Authored
  beside the burst in Engage.
- **Controller** — policy states (aim/melee/grenade/movement) live per-life on the
  controller (the grenade-cooldown lesson generalised: StateTree re-initialises instance
  data on every state ENTRY), reset at possession AND unpossession; one execution-side
  `PolicyRandom` per bot (the F-3.7 hazard is cross-BOT lockstep, not cross-skill).

## Kickoff (machine-checkable)

- requires: engine-installed; editor-open from step 3
- owner_path: `docs/tickets/TICKET_AIB7_SKILL_INTEGRATION.md`
  <!-- Log only; compile-error protocol as AIB1–AIB5. -->

## Steps (in order)

1. **Rung 1** — `./Tools/run-ubt.sh BreachpointEditor Breachpoint`.
2. **Rung 2** — `./Tools/run-specs.sh AIBot`: still **91/91/0** (integration adds no
   spec; this catches an include or signature break in the worldless suites).
3. **Tree rebuild (REQUIRED — a new node struct)**: `python3 Tools/aib/70_aib_assets.py
   probe` — now **18** structs — then `build`; the read-back's Engage line must show
   **5 tasks** (sentinel, face, move, fire, strafe).
4. **The live proof** (PIE — `BotSystem=AIB` is the shipped default since the founder's flip):
   - Bots in a firefight STRAFE while holding position (count `Verb_Sprint`-style: no
     counter exists for strafe, so observe + note; the leg cadence should read irregular,
     never a metronome).
   - Aim is no longer perfect: shots MISS early in an exchange and settle — watch a
     first-contact volley land wide then walk in. (All bots are Trained-tier defaults
     until Phase 8; the effect is modest by design: 4° cone, 1.1s settle.)
   - Melee shows the recognition beat at point-blank rather than firing instantly.
   - Grenades still fly (Trained sees the opener) and `threw (call N)` appears in
     LogAIBot with call=1 (Opener); call=2 (Finisher) if a fight runs hot.
5. Four mechanical checks, pasted empty.

## Watch-list — written-not-compiled spots flagged for honest scrutiny

- `FAIBAimPolicy::StepAimPoint`'s geometry (perpendicular re-orthonormalisation, tan
  offset) was compiled in AIB3 but never CONSUMED — this is its first caller. Watch for
  a bot aiming at its own feet on a near-vertical belief line (the degenerate-axis path).
- `AActor::GetUniqueID()` as the aim policy's opaque target id — ubiquitous engine API,
  transcribed per the controller's own seed usage.
- The strafe task's 6-arg `MoveToLocation(..., bStopOnOverlap, bUsePathfinding,
  bProjectDestinationToNavigation, bCanStrafe)` — transcribed from the host's compiled
  strafe body, re-typed here.
- Strafe vs MoveNearBelief hand-off at exactly the 350uu edge: both idle inside/outside
  their own halves by design, but a bot oscillating across the boundary could see
  alternating owners — if PIE shows jitter at mid-range, the fix is hysteresis on
  `EngagedRadiusUU`, noted here first.
- The grenade policy's Expert ceiling (1500uu, envelope-anchored) is TIGHTER than the
  retired band's 2200 — expect somewhat fewer long throws than AIB2's match; that is
  the policy being honest about the sight envelope, not a regression.

## Done when

- [ ] Rung 1 PASS (Editor + Game; Server recorded environmental)
- [ ] Rung 2: 91/91/0, reconciled
- [ ] Probe 18/18; rebuild; read-back shows Engage with 5 tasks
- [ ] Step-4 observations recorded (strafe, aim settle, melee beat, grenade calls)
- [ ] Four mechanical checks pasted, empty
- [ ] Deviations recorded

## Log

_(terminal: outputs verbatim)_

### 2026-08-28 — board-hygiene pass: "WRITTEN, NOT COMPILED" was stale, the live half is not

Corrected, not re-measured. This ticket claimed uncompiled text while five later phases
sat on top of it — a header that would have told the next agent this integration was
unsafe to touch, when the only unsafe thing about it is that **nobody has watched it
run**. Verified this session by the verifier: all targets compile, AIBot 119/119/0.

Boxes deliberately left unchecked:

- *Rung 2: 91/91/0* — the count is dead (119 now). It is re-pinned, not ticked, because
  this ticket's own reconciliation was against 91 and re-reading a later number is not
  the same act.
- *Probe 18/18; rebuild; read-back shows Engage with 5 tasks* — LIVE EDITOR, unrun. The
  strafe node (`FAIBStrafeTask`) is the new struct; AIB10 later moved its geometry
  (BN22 §5), so whoever runs the rebuild should read back against TODAY's task list.
- *Step-4 observations (strafe, aim settle, melee beat, grenade calls)* — eyes-on, unrun.
