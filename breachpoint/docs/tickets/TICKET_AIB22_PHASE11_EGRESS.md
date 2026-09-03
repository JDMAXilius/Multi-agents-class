# TICKET — AIB22: Phase 11 EGRESS — no bot stands on a platform sweeping; roam the whole level

> STATUS: claimed — Claude (session 014esNfHwPnkiAJkRKBMwR7b), lead, 2026-09-02. Founder rulings
> and law F9 (motion is the default) in `docs/AIBOT-ROADMAP-2.md` §5. Waves per `docs/AIBOT-WAVES.md`.

Founder: a bot on a raised platform with no path off it turns in place "looking for a target".
Wanted: roam the whole level through random points until a target is seen, then go for it; step
off platforms (drop/jump), climb back up when needed; NEVER stand still outside a named tactic.

**Ordering law:** metrics land FIRST (a baseline before any behaviour changes); nav links land
serial (editor is global); the path-following jump hook is a serial header before the Egress
tactic; EQS roam last (it depends on the visit-heat grid, a Phase 12 Team-Mind member that this
packet introduces in its minimal form).

## Kickoff (machine-checkable)
- requires: engine-installed (C++, headless runs) · editor-live for the nav-link and tree steps
- `docs/AIBOT-ROADMAP-2.md` exists with §5 rulings (it does)
- `Tools/aib/80_aib_metrics.py` runs against a current `LogAIBot` (verifier confirms in W-AUDIT)
- owner_path (per writer, disjoint):
  - aib-builder: `Plugins/AIBot/Source/AIBot/`
  - aib-editor: `Content/AIBot/`, `Tools/aib/`, `Tools/blockout/` (nav-link placement scripts)
  - lead: `Config/DefaultEngine.ini` (RecastNavMesh block only), `docs/tickets/`, `docs/AIBOT-*.md`
  - NOT touched: `Source/Breachpoint/`, `Source/BreachpointNext/` (adapter changes = their own packet)

## Waves
- W-AUDIT ×3 (2 Sep): aib-critic "tree on an island + minimal fair Egress" · arena-architect
  "which platform edges lack links, coordinates" · aib-verifier "what to log, how to run, baseline".
  Merged findings: see Log.
- Serial: metrics lines (aib-builder) → baseline runs (aib-verifier).
- Serial (editor): nav-link placement scripts + regen (aib-editor), ini envelope (lead).
- Serial header: `UAIBPathFollowingComponent` jump hook (aib-builder).
- W-BUILD ×2 (disjoint files): Egress tactic + island fact ∥ sweep budget + sweep-while-moving.
- W-REVIEW ×4: containment · fairness · utility pathologies · server-only.
- W-VERIFY ×2: `AIBot.Sim.*` specs ∥ headless seeded 4v4 metrics vs baseline; PIE watch of a
  top-platform spawn leaving within 5 s.

## Steps (in order) — refined at the audit merge
1. Metrics: `stuck_seconds`, `no_path_requests`, `sweep_seconds`, `idle_seconds`,
   `island_egress_count` as structured `LogAIBot` lines + parser + gate; five-run baseline.
2. Nav: `NavLinkProxy` per uncovered platform edge from the blockout scripts; `BN_Drop`
   envelope to the measured gaps; regenerate; count generated + authored links.
3. Path-following jump hook (`SetMoveSegment` on a jump area → JUMP verb) — the only place a
   traversal verb fires from a path.
4. Island fact + Egress tactic under Roam; SweepLook gets a budget and moves while it sweeps.
5. Roam over the whole level: EQS pathing grid scored by visit recency (team visit-heat grid).
6. Review wave, verify wave, log, push.

## Done when
- [ ] Baseline report (5 runs, medians + spread) committed under Tools/aib/ before step 2
- [ ] Every platform on Spillway and Arena01 has a way down; the floor has a way up (link count)
- [ ] `idle_seconds == 0` outside named tactics; `sweep_seconds == 0`; `stuck_seconds` per bot
      under the gate the verifier proposes; kills/min not worse than baseline
- [ ] PIE: a bot placed on the top platform leaves it within 5 s (captured)
- [ ] W-REVIEW: no `high`

## Log
