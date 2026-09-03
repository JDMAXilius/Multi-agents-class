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
- 2026-09-03 rung 1 on main with EVERYTHING merged (AIB22 fix #4, Phases 12/13/14/15, the BN
  retirement, the game-side crowd/seed hooks): BreachpointEditor PASS, Breachpoint PASS (server
  target unbuildable on this engine). Merge compile fixes by the lead: duplicate `BotIndex`
  member/accessor (Phase 14 owns the seed triple), `MatchSeed` shadow, Flank task on the
  controller-held locomotion signature, spec literal/lambda fixes, `FAIBOverlapEpisode` closing
  braces. Specs running; W-REVIEW x4 dispatched on the merged commits.
- 2026-09-03 spec fix (aib-builder, on main, uncompiled): the failing VETO-bypass case was the
  spec's own facts — `AmmoNorm` defaulted to 0 (an empty magazine), Hold's exact case; the
  spec now states a full magazine. No engine change.
- 2026-09-03 W-REVIEW (aib-critic on c2314cac/4b84c050): ONE HIGH, four MEDIUM, two LOW.
  H1 Hold's end (`Succeeded` + `NoteCurrentTacticFailed`, clock cleared only at the next Think)
     transitions to Root, Root re-selects Engage>Hold, per FRAME for up to 100 ms: Engage
     exits/re-enters each frame, FireWhenAble's burst rest and the melee continuity clock reset
     every frame, 6–12 `hold over` lines per stand, 6–12 suppression strikes. RULING: a child
     tactic's completion NEVER leaves Engage — the Hold task clears its own clock and returns
     Running until the tactic engine re-elects at the next Think (the gun tasks on the parent
     keep their phase); completion transitions of tactic children target the parent's
     re-selection, not Root; one `hold over` line per stand.
  M2 arrival records a Flank FAILURE (re-entry without a latch strikes) → self-suppression to
     20 s. RULING: arrival sets a `bFlankDone` mark on the controller; a Flank entry with the
     mark set Succeeds silently and the engine hands to Push (arrival zeroes the point term).
  M3 the latch is stale only on belief drift, never on the enemy closing (walks away from a
     knife fight for 3.5 s, re-elects while wounded). RULING: the latch clears when the enemy's
     distance drops below FlankRadius/2 and on ANY switch away from Flank.
  M4 the flank goal is hidden by construction so arrival drops to Search — risk register; Push's
     first leg re-acquires via the belief (accepted for Phase 15).
  M5 `facts=` crc is unstable for UU-scale fields. RULING: quantise distances to 10 uu before the
     crc. L6 `decide` at Log by default: RULING Verbose unless `-AIBReplay`. L7 the decide line
     lacks the tactic commit + reason: add `tcommit= treason=`.
  Fix #5 interplay: the all-zero fallback must be a per-engine registered floor (Push for the
     tactic engine, Roam for the ambition engine), never a hardcoded ambition tag. PASS:
     containment, server-only, no Tick, FAIRPLAY F1/F2/F4/F5, header repair well-formed.
- 2026-09-03 review-fix packet (aib-builder on main, 21 files, commit "review fixes"): every HIGH
  and MEDIUM ruling in this ticket's W-REVIEW entry is closed on disk (see the packet's per-ruling
  lines in the commit); AIB26 H1 CHANGED THE TREE (tactic children complete to Engage, gun tasks
  `bShouldStateChangeOnReselect=false`) — ST_AIBBot rebuild queued. New/changed shapes: `decide …
  tcommit= treason=` (Verbose unless `-AIBReplay`), `target claim RELEASED … reason=switch`,
  `idle over … tactic=Crowd`, `crowd simulation DISABLED — …`. Building (editor closed).
- 2026-09-03 W-VERIFY v5: the tactic layer runs — `tactic ->` Push first 36–73 per match,
  Flank first 1–6, Hold first 0–2; `flank starts` 2–8 per match but `flank over` 0 everywhere:
  each flank is cleared by "the fight ended" within 0.5 s (Engage's LOS law) — fix #8 F8-5
  keeps Engage on the belief while a young latch lives. `hold over` 0. `decide` at ~10 Hz per
  bot (21k lines in a 300 s match, Verbose). Switch reasons: veto dominates (Roam<->Rally on a
  stalled Rally goal — the adapter's per-map grapple routes remove the stall). Engage vanished
  for 270 s in the one long match (want 0 with acquisitions) — F8-4.
