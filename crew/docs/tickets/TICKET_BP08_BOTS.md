# TICKET — BP08: Bots — the three-layer brain (ambitions → StateTree → GAS)

> STATUS: open — recut 29 Jul 2026 for the GOAP-layer architecture
> (`breachpoint/BREACHPOINT-AI-BOTS.md` is the binding design; rulings R8–R12 apply).
> The M4 go/no-go hinges on this ticket.

Founder directive: bots are players the AI drives — same input tags, same ASC path, same
loadout sets, zero privileged state. Three layers, one brain: a GOAP-style ambition layer
decides WHAT (utility-scored from data, rescored on event), one StateTree decides HOW
(BT-shaped selectors live INSIDE states — no second BehaviorTree asset), GAS is the only
hand. Deterministic: same seed + tuning row + event trace ⇒ identical ambition/plan/action
traces. Legibility outranks win-rate. MassAI and Learning Agents stay rejected.

## Kickoff (machine-checkable — the tickets skill verifies these BEFORE a claim)

- Ticket BP04 is DONE (match frame — bots need a match to join)
- Ticket BP07 is DONE and `Content/Data/arena_manifest.json` re-validates (EQS vocabulary)
- `DT_BotTuning` row struct exists in `BRDataRows.h` and rung 1 is green on it
- owner_path: `Source/Breachpoint/AI/`, `Content/AI/`, `Content/Data/DT_BotAmbitions.csv`,
  `Content/Data/DT_BotTuning.csv`

**Ordering law:** 1 → 2 → 3 → 4; 5 continuous after 3; 6 gates.

## Steps (in order)

1. **Layer 1 — `UBRBotBrain` + `BRBotFacts`** (pure, headless): ambition utility scoring
   from `DT_BotAmbitions` × facts × tuning weights; hysteresis + seeded quantized commit
   window; bounded ≤3-step plan chains with ASC-query preconditions; replan on
   contradiction event. Pinned suite `Breachpoint.Bots.Brain` red-then-green: same seed ⇒
   identical ambition/plan trace, `reaction_ms ≥ 200` unviolatable. Owner: **ai-builder**;
   **sim-builder** reviews purity (no world, no wall-clock, seeded stream passed in).
2. **Data — `DT_BotAmbitions.csv` + ambition weights in `DT_BotTuning`**: tuning-curator
   proposes rows (ambitions, base utilities, consideration weights; 3 tier scalar sets
   expressing the fantasies — Recruit over-commits, Veteran times the rocket); critic
   refutes against R11/R12; builder lands. Owner: **tuning-curator** → **builder**.
3. **Layer 2 — `ABRBotController` + `ST_Bot` + `BRStateTreeTasks` + `BREnvQueryContexts`**:
   perception (server gameplay events only) → facts → brain; plan enters StateTree as
   parameters; stance states execute (Engage internals = priority selector task);
   EQS scores manifest landmarks/cover/perches; InputTag ability activation on the bot's
   PlayerState ASC, aim error from `accuracy_pct` BEFORE fire activation.
   Owner: **ai-builder**.
4. **Match glue — `BRBotManagerComponent`**: fill to roster at start, 10 s backfill, tier
   scalar application (GameMode authority domain). Owner: **ai-builder**, **builder**
   consults on GameMode seam.
5. **Nightly soaks live from this step**: 20 bot-vs-bot matches, seeds logged, report to
   the board each morning (stuck navmesh, spawn-kill loops, TTK distribution, fights lost
   below 40% shields → tuning-curator's balance triggers). Owner: **verifier** (runs),
   **builder** (harness: `Tools/run-soak.ps1`).
6. **Verify + refute.** Verifier: `Breachpoint.Bots.*` determinism suites (brain headless;
   spine trace via functional rung; soak seeds reproduce). Critic REFUTER: bot reads hidden
   state? sub-200 ms path under any scalar combination? ambition thrash under rapid
   events (hysteresis hole)? plan surviving a contradiction? stuck-state livelock?
   grapple-perch pathing exploit? illegible tier (R12)? Owner: **verifier**, **critic**.

## Done when

- [ ] Full 4v4 (1 human + 7 bots) end-to-end — the M4 build
- [ ] `Breachpoint.Bots.Brain` green headless: seed ⇒ identical ambition/plan trace
- [ ] Action-trace determinism green; soak seeds reproduce
- [ ] Three difficulty tiers from ONE StateTree + ONE brain via data scalars only
- [ ] Legibility check passes: a playtester can name a bot's ambition from behavior alone
      (break-off on shield-crack, visible rocket contest at T−10 s)
- [ ] Nightly soak report posting to the board
- [ ] Critic findings addressed or waived in the Log

## Notes

- Crew: ai-builder owns · sim-builder purity review · tuning-curator curates ·
  verifier proves+soaks · critic refutes
- Binary files this ticket OWNS (lock before editing): `Content/AI/ST_Bot.uasset`,
  EQS assets under `Content/AI/`
- Out of scope: full GOAP A* planning (rejected, R10) · a second BehaviorTree asset (R9) ·
  per-tier StateTrees (Phase 2) · Spotter subsystem (BP11) · mid-match adaptive difficulty
  (between matches, as data, via curator — R8)

## Log

(append findings here, dated, newest last)
