# Design Rulings — the ledger the critic judges against

A REFUTER with an attack surface but no statement of intent attacks the design itself,
escalates every round, and never converges — the first data-crew run proved it (three rounds
of "findings" against intended Halo design before rulings were injected). This file is the
fix, made permanent: **rulings already made.** Every critic pass loads it. Attacking a ruling
is out of scope for review; a finding must show a hard-constraint violation or an internal
contradiction. Re-opening a ruling is a *founder/lead* decision, logged here with a date —
never a review outcome.

The lead appends; nobody else writes here. A doubt this ledger closes is closed.

## Combat sandbox (from the 29 Jul 2026 data-crew run)

- **R1. Precision weapons reward aim.** The Magnum's all-headshot TTK beating every other
  path is the intended fantasy, paid for by an 8-round mag and no forgiveness on a miss.
  "Skilled Magnum play is strong" is the design working.
- **R2. The AR is the shield-stripper, not a finisher.** Slower solo TTK is intended; its
  headshot multiplier stays 1.0 — headshot bonuses belong to precision weapons.
- **R3. The Magnum is the finisher, not a self-sufficient primary.** It is NOT required to
  solo a full-200-EHP target from one mag on body shots — the intended line is AR-strip →
  0.4 s swap → Magnum finish.
- **R4. The Rocket is balanced by scarcity, not by damage.** Map pickup, 90 s timer, 2 shots,
  ReserveMags 0 (reload intentionally unreachable). It is SUPPOSED to win the fight it is
  present for. HeadshotMult stays 1.0 (a 240-damage headshot one-shot is a defect — caught
  in the run).
- **R5. Shields-first, no health regen, 2.5 s recharge delay** are pillars, not tunables.
  Proposals touching them go to the founder, not through the pipeline.

## Arena (from the same run)

- **R6. The slice ships one compact map**, not a competitive-ranked layout. Imperfect spawn
  distribution is acceptable when the hard constraints hold (≥ 8 spawns, ≥ 8 m spacing,
  ≤ 35 m sightlines) and the imbalance is documented in the manifest's doubts.
- **R7. Geometry claims are editor-rung.** Mutual visibility, occlusion, and 5 m LOS-breakage
  cannot be settled from coordinates; a manifest that records them in `doubts[]` with that
  caveat has handled them correctly. Only coordinate-provable contradictions are findings.

## Bots & AI (BP08 domain)

- **R8. No LLM in the hot path — structural, not preferential.** Bot decisions in a live
  match are deterministic code reading data. LLMs shape bots offline (tuning rows, ambition
  weights via the curator pipeline) and decorate post-hoc (Spotter strings). There is no
  mid-match model call that a game outcome waits on.
- **R9. One brain: GOAP-style goal layer over a StateTree execution spine** (see
  `BREACHPOINT-AI-BOTS.md`). No second BT asset running in parallel — behavior-tree
  patterns live as selector-shaped tasks *inside* StateTree states. A dual-brain proposal
  is a finding against itself.
- **R10. Plans are short and disposable**: ≤ 3 steps, replanned on event, never per-tick.
  A bot that "thinks" every frame is a perf finding; a plan that survives a world
  contradiction is a correctness finding.
- **R11. The 200 ms reaction floor is a law, not a difficulty knob.** No tier, scalar, or
  ambition weight may produce sub-human reaction; `Breachpoint.Bots.*` pins it.
- **R12. Bots are legible before they are optimal** (the Halo lesson). A bot whose state
  change players can't read (break-off on shield-crack, rocket contest on timer, tier
  fantasy) fails review even if it wins more.

## Crew & pipeline

- **R13. Only `high` severity blocks a landing.** Medium/low land in the risk register with
  the artifact. A reviewer with no ship gate never ships — the run deadlocked to prove it.
- **R14. The orchestrator holds no opinions.** Managers are deterministic (script + lead
  session); intelligence lives inside the boxes. A proposal to add an LLM manager reargues
  a closed ruling.

## Online services & Phase 2 (from the GameLift plan, 29 Jul 2026)

- **R15. Identity is Steam-derived; there is no first-party account creation.** Phase-2 auth
  validates the client's Steam session ticket server-side and issues our own short-lived
  token. Any flow that asks a Steam player to sign up (Cognito login UI included) is a
  defect, not an option. Cognito may serve as token machinery only.
- **R16. Managed fleets (real money) are telemetry-triggered, never date-triggered.**
  GL-3 opens only when demo telemetry shows the listen-server pain: host-quit abandonment,
  NAT/join failure rate, host-advantage complaints. Low numbers = the fleet money stays
  unspent and listen + GL-2 keeps shipping. Billing alarms and fleet caps are acceptance
  criteria on any fleet ticket (denial-of-wallet is an exploit class).
