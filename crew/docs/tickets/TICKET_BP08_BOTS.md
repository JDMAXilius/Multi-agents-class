# TICKET — BP08: Bots — StateTree + EQS opponents

> STATUS: open — cut by lead session, 29 Jul 2026. Gated by BP04 (match frame) + BP07
> (navigable arena). The M4 go/no-go hinges on this ticket.

Founder directive: bots are players the AI drives — same input tags, same ASC path, same
loadout sets, zero privileged state. Deterministic: same seed + tuning row ⇒ identical
action trace. StateTree + EQS; MassAI and Learning Agents stay rejected.

**Ordering law:** 1 → 2 → 3; 4 continuous after 2; 5 gates.

## Steps (in order)

1. `ABRBotController` + perception (sight/hearing config) + seeded decision jitter; ability
   activation by InputTag on its PlayerState ASC; aim error from `DT_BotTuning.accuracy_pct`
   applied BEFORE fire activation. Owner: **ai-builder**.
2. `BRStateTreeTasks` + `ST_Bot` asset (Seek/Engage/Flush/Reposition/Retreat/ContestRocket)
   + `BREnvQueryContexts` cover/threat/rocket scoring + EQS assets. Owner: **ai-builder**.
3. `BRBotManagerComponent`: fill to roster at start, 10 s backfill, difficulty scalar
   application. **tuning-curator** returns the baseline row + 3 scalar sets
   (reaction_ms ≥ 200 schema-enforced); critic refutes samples. Owner: **ai-builder**.
4. **Nightly soaks live from this ticket**: 20 bot-vs-bot matches, seeds logged, Combat QA
   report to the board each morning. Owner: **verifier** (runs), **builder** (harness).
5. Verify + refute: `Breachpoint.Bots.*` determinism suite (seeded trace identity);
   critic REFUTER: bot reads hidden state? superhuman path? stuck-state livelock?
   grapple-point pathing exploits? Owner: **verifier**, **critic**.

## Done when

- [ ] Full 4v4 (1 human + 7 bots) end-to-end — the M4 build
- [ ] Determinism suite green; soak seeds reproduce
- [ ] Three difficulty settings from ONE StateTree via scalars only
- [ ] Nightly soak report posting to the board
- [ ] Critic findings addressed or waived in the Log
- Crew: ai-builder owns · tuning-curator curates · verifier proves+soaks · critic refutes
- Out of scope: distinct per-tier StateTrees (Phase 2), Spotter (BP11)

## Log

(append findings here, dated, newest last)
