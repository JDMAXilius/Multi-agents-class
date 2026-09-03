# TICKET — AIB26: Phase 15 UTILITY SELECTORS

> STATUS: in-progress — lead (Mac, session 014esNfHwPnkiAJkRKBMwR7b) 2026-09-03 (8e324dce), founder ruling: all phases run in parallel with Phase 11, W-BUILD in isolated worktrees, merged serially behind AIB22 fix #4. Was: open — cut 2 Sep 2026 by the lead (session 014esNfHwPnkiAJkRKBMwR7b) from
> `docs/AIBOT-ROADMAP-2.md` (approved; rulings in §5; law F9 motion is the default). Claimed
> when its W-AUDIT merge lands here.

Ambition and tactic selection on StateTree utility selectors with C++ considerations reading the Team Mind and csv response curves, MinDwell per ambition, seeded weighted selection, deterministic replay diff; influence map only if roaming still reads aimless.

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
### W-AUDIT (aib-critic) — merged by the lead, 2 Sep; the roadmap's Phase 15 text is REPLACED
VERDICT ADOPTED: keep the worldless `UAIBAmbitionEngine` (utility + hysteresis + commit + VETO,
835 lines of headless spec) and bind the tree to it; do NOT move selection onto StateTree's
native utility selectors. Evidence: the native random stream is seeded from wall-clock
(`StateTreeExecutionContext.cpp:1513`; `UStateTreeComponent::StartTree` never sets RandomSeed and
the seam is module-private) → seeded weighted selection is unreachable; highest-utility picks the
FIRST child on an all-zero board (the exact bug `AIBAmbitionEngine.cpp:239` fixed); the weighted
variant filters score-0 children so Roam's floor and the ungated Fallback vanish → TreeRunStatus
Failed ("seven bots, seven errors"); native utility is stateless at selection (no hysteresis /
commit / VETO — consideration memory is alloca'd per evaluation, so MinDwell cannot live there);
combination semantics differ (Min/Max/Multiply, no make-up); testability collapses into a
binary asset; F8's type-level quarantine (`FAIBConsideration::Evaluate(const FAIBFacts&)` cannot
touch the world) becomes a review convention. Native considerations could also score Engage
before `DrawReactionSeconds` matures (R11/F1 `high`).
Design: tactic layer = a SECOND engine instance on the controller (per AIB25) with Push/Flank/Hold
specs; one nested level under Engage in `AIBTreeAuthoring.cpp:176-189` gated by
`FAIBGateTacticCondition` (copy of the ambition gate at `AIBStateTreeTasks.cpp:535-557`); root
stays TrySelectChildrenInOrder. "MinDwell" already exists as `CommitSeconds` + `SwitchCostFactor`
— no new knob; Flank CommitSeconds 3–4 s and MEASURE the VETO bypass (Flank's gate must return 0
only on a LATCHED failure or it dithers through its own commit). Weighted non-argmax selection:
~15 lines in Rescore over a controller-owned FRandomStream seeded from MatchSeed+BotIndex — skip
until argmax reads robotic in the log.
Replay: one `AIBot: decide bot=<BotIndex> seq= want= s= over= rs= tac= ts= commit=<ticks
remaining> rng=<stream call count> facts=<crc32 of quantised facts>` per decision, keyed on the
stable BotIndex (never GetName/UniqueID); diff excludes wall-clock, absolutes, object ids; scores
3 dp. Verifier: two `-AIBSeed=N` runs, `grep " decide "`, sort (bot,seq), diff; `--replay-diff`
in the metrics script. DEPENDS ON AIB25's match seed; `-FixedSeed -BENCHMARK` alone is not enough.
Influence-map deferral criterion: build only if after Phase 11's recency roam, over 5 runs × 2
maps, `roam_coverage_300 < 0.60` OR `roam_revisit_ratio > 0.35` (one extra `roam goal — cell=
age=` line at the wander draw), with idle==0 and sweep==0 already passing.
Containment note: the roadmap said considerations read "GAS attributes read-only" — the plugin
has NO GAS dep; facts arrive through `IAIBAvatarInterface`/`FAIBFacts` only (a direct ASC read =
`high`). Brain/ and Skills/ contain no UWorld/AActor today and must stay so.
W-BUILD (after Phase 12 A and Phase 14 B, which own AIBBotController.cpp / AIBTypes.h /
TreeAuthoring): A Brain (AIBTactic.h tags, AmbitionEngine, its spec) · B Execution (tactic gate,
TreeAuthoring, new TacticGate spec) · C Core (second engine, tactic clocks, decide line,
DecisionRandom) · D aib-editor (decide regex + --replay-diff, AIB_Tactics.csv).
- 2026-09-03 W-BUILD (aib-builder, worktree c2314cac, merge pending): the tactic layer is a
  SECOND `UAIBAmbitionEngine` instance on the controller (`TacticEngine`), tactics
  `AIBot.Tactic.Push/Flank/Hold` as one nested level under Engage — `Engage > [Flank(gated),
  Hold(gated), Push(ungated floor, last)]`, gun tasks stay on the Engage parent. No native utility
  selectors, no weighted non-argmax (skipped per verdict; `DecisionRandom` seeded
  Hash(MatchSeed,BotIndex,LifeIndex) exists, one draw = flank ring phase). Flank's gate returns 0
  only on a LATCHED failure (`FAIBFlankLatch`, cleared on arrive/refused/stalled/drift/fight-over);
  commit = CommitSeconds + SwitchCostFactor, no new knob; rows FlankCommitSeconds 3.5,
  FlankRadiusUU 700, FlankMaxDetourFactor 1.5, HoldMaxSeconds 4. `EAIBSwitchReason`
  first|merit|veto|interrupt on every switch line; `AIBot: decide bot= seq= want= s= over= rs=
  tac= ts= commit= rng= facts=<crc32>` every Think (replay diff = sort (bot,seq) and diff).
  Specs AmbitionEngineSpec +8, `AIBot.Sim.TacticGate` 3. **Tree CHANGED** (Engage +3 children,
  3 new node types) — ST_AIBBot rebuild editor-live before any PIE. Parser regexes for decide /
  tactic -> / flank / hold landed (lead). Gaps: `SetMatchSeed`/`SetBotIndex` callers = AIB25
  lane A's manager (merged) + BNGameMode seam (bn-builder in flight); AIB25 lane B's list also
  names `Brain/AIBTactic.h` and the Engage tasks — built HERE, lane B must not rebuild them.
- 2026-09-03 merged into main (4b84c050; three additive conflicts kept both: row fields, the two
  controller structs, includes). Building. ST_AIBBot rebuild queued for the next editor session.
