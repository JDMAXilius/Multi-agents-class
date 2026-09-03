# TICKET — AIB24: Phase 13 SEPARATION

> STATUS: open — cut 2 Sep 2026 by the lead (session 014esNfHwPnkiAJkRKBMwR7b) from
> `docs/AIBOT-ROADMAP-2.md` (approved; rulings in §5; law F9 motion is the default). Claimed
> when its W-AUDIT merge lands here.

Detour Crowd with separation on every bot (no RVO), players as crowd obstacles, the jump hook on the crowd component, the wedge watchdog yields to teammates instead of jumping, no bot-vs-bot shoving; listen-server rung for the replication claim.

**Ordering law:** waves per `docs/AIBOT-WAVES.md`: W-AUDIT (read-only, one question each) →
merge → serial headers → W-BUILD with disjoint files → W-REVIEW ×4 → W-VERIFY vs the previous
phase's baseline. Metrics for this phase land BEFORE its behaviour (§4 of the roadmap).

## Kickoff (machine-checkable)
- requires: engine-installed; editor-live only for the steps that name it
- the previous phase's W-VERIFY verdict is logged in its ticket
- owner_path: aib-builder `Plugins/AIBot/Source/AIBot/` · aib-editor `Content/AIBot/`,
  `Tools/aib/`, `Tools/blockout/` · lead `Config/`, `docs/tickets/`, `docs/AIBOT-*.md`.
  Anything under `Source/Breachpoint*/` is a contract_gap raised to the founder, never edited here.

## Steps (in order) — refined at the audit merge
1. W-AUDIT (dispatched 2 Sep) → merge below.
2. Metrics + baseline for this phase.
3. Serial headers, then W-BUILD ×2 on disjoint files.
4. W-REVIEW ×4 (containment · fairness · utility pathologies · server-only); a `high` blocks.
5. W-VERIFY ×2 (specs ∥ headless seeded 4v4 vs baseline; PIE/listen rung as the phase names).

## Done when
- [ ] Merge logged; steps refined
- [ ] Builds PASS; specs PASS; no `high`
- [ ] The phase's metric gate PASSES vs the previous baseline; kills/min not worse

## Log
