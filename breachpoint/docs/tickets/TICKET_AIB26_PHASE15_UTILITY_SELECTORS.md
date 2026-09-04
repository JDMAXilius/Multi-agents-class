# TICKET — AIB26: Phase 15 UTILITY SELECTORS

> STATUS: open, VERIFIED 2026-09-03 — the tactic layer runs on the final build but flanks never complete (`flank over` 2 of 123 starts): the ambition-layer flank clear bypasses the hold, and the empty-hand melee floor needs a held weapon (game-side swap defect). Two named follow-ups in the Log. Earlier: in-progress — lead (Mac, session 014esNfHwPnkiAJkRKBMwR7b) 2026-09-03 (8e324dce), founder ruling: all phases run in parallel with Phase 11, W-BUILD in isolated worktrees, merged serially behind AIB22 fix #4. Was: open — cut 2 Sep 2026 by the lead (session 014esNfHwPnkiAJkRKBMwR7b) from
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
- 2026-09-03 W-VERIFY v6 (final build): tactic layer live (Push first 36–73 per match, Flank
  starts 45 / 78 per 5 matches, latch lifetime median 4.1 s vs 1.3 in v5) but `flank over`
  2 / 0 — `ClearFlankLatch("the fight ended")` (`AIBBotController.cpp:1159`) fires whenever the
  AMBITION leaves Engage (Retreat/Roam veto) and never consults `bFlankHolding`, so F8-5's hold
  is bypassed by the ambition layer. F8-4's melee floor never fires empty-handed:
  `bMeleeAvailable` requires a held weapon (`BNAIBAvatarAdapter.cpp:370-376`) and the
  empty-hand case has none (`SwapNext` never lands a weapon — game-side equipment defect,
  high, bn-builder) — `ammo=0.00` decide lines never want Engage (0 of 109,289). Switch rate
  42 / 45 per bot-min (baseline 16 / 28), veto 60 %, Roam<->Rally triplet 2.1 / 2.6 per bot-min
  = Rally urgency 0 inside RallyNearUU (`BNGameMode.cpp:1340`), a game-side objective shape.
  CLOSE-OUT (lead): builds/specs/no-high met; the metric-gate box is NOT met (flanks never
  complete). Two follow-ups: the ambition-layer flank clear must read `bFlankHolding` (plugin,
  one line) and the empty-hand weapon swap (game, bn-builder).

### 3 Sep (cloud) — both named follow-ups landed, plus the third from the status list

**WRITTEN, NOT COMPILED**, all three; terminal owes rung 1 + a v7 verify pass.

1. **The empty-hand swap — the `high`, and it was a CIRCLE, not an equipment defect.**
   Traced end to end: `BNGA_SwapNext` and `EquipNext()` are sound; every life begins on
   the null Unarmed slot (`EquipIndex(0)` at loadout); the only swap press in the module
   lived in `FireWhenAble`, which the tree runs under Engage and Retreat alone; the v6
   ambitions score Engage from weapon facts. Need Engage to draw, need a weapon to want
   Engage — 0 Engage wants in 109,289 decide lines is that circle, measured. Fix: **the
   draw reflex** in `AAIBBotController::Think` — ambition-blind, empty hand + usable
   pouch + alive → press the cycle verb at the equip's own 0.6 s beat. FAIBWeaponPolicy
   still owns the decision; the task's empty-hand arm now DEFERS (one presser per cause,
   or two throttles interleave into a cycle that skips every other weapon) and keeps only
   its don't-fire-into-the-montage duty.
2. **The flank clear consults `bFlankHolding`** (`ThinkTactic`): a veto's half-second
   excursion out of Engage no longer clears a young latch or resets the tactic engine —
   the latch ages out of the hold via FlankCommitSeconds in the facts builder, and only
   then does a non-Engage ambition clear as "the fight ended". Expect `flank over` to
   finally move off 2/123.
3. **The Rally edge is a ramp, not a cliff** (`BNGameMode::GetObjectiveUrgency` +
   `BNAIB::RallyBlendUU=300`): the 0-or-0.3 step at RallyNearUU snapped across Roam's
   0.2 floor every boundary crossing — the measured Roam<->Rally triplet at 2.1/2.6 per
   bot-min, 60% veto share. The want now rises 0→0.3 across the 300uu shoulder, so the
   ambition engine's own hysteresis decides. BN22 W-REVIEW M1's kilometre-out annulus is
   NOT reopened: the only sub-0.3 zone added is the arrival shoulder, and beyond it M1's
   floor stands untouched.

v7 gates to read: `flank over` > 2; Roam<->Rally triplet and ambition_switches/bot-min
down from 42/45; `ammo=0.00` lines vanish within ~2 s of every spawn; kills/min not worse.


### 3 Sep 2026 — v7 numbers come from the AIB22 run (mac terminal, lead)

Follow-ups (d) the flank clear reads `bFlankHolding` and the Rally urgency ramp landed in
`199941d0`. No separate batch: `Tools/aib/aib22_verify.sh v7` (started 13:41) instruments
both maps for every phase, so this ticket's gates are read from the same logs.

Gates per this ticket: `flank over` > 2 of 123 starts; Roam<->Rally triplet and
`ambition_switches`/bot-min down from 42/45; `ammo=0.00` lines gone within ~2 s of spawn;
kills/min not worse.

The empty-hand melee floor stays open regardless of this run — it needs a held weapon, and
the weapon swap defect is game-side (bn-builder, `high`), not in an AIB owner path.

### 3 Sep 2026 — W-VERIFY v7 RESULT: gate NOT met. Flanks still do not complete

Same headless batch as AIB22 (5 x 300 s per map). Read from the raw logs, because the
metrics parser disagrees with them — see the defect note below.

**`flank over` — the gate was > 2. Measured 1 (Spillway) and 3 (Arena01), on 113 and 245
flank starts.** v6 was 2 of 123. Fix (d) — the flank clear reading `bFlankHolding` —
did NOT move this. Flanks start in quantity and essentially never complete, exactly as
v6 reported. `hold_seconds` is 0.000 on both maps: the hold the fix was built around is
not accumulating any time at all, which is the more useful clue about why.

**PARSER DEFECT, filed here so the next run is not misled:** `80_aib_metrics.py` reports
`flank_count 0.000` and `flank_stalled 0.000` on both maps while the same logs contain
113 / 245 `flank start` lines. The aggregate is not counting what the log plainly says.
Any verdict taken from the JSON summary for flank metrics is currently worthless — read
the logs. `island_egress_count` is likewise 0.000 on Arena01 and deserves the same
suspicion before anyone trusts it.

**`ammo=0.00` — the gate was "gone within ~2 s of every spawn". Measured 7,670 (Spillway)
and 8,757 (Arena01) occurrences.** Unchanged in character. This is the known game-side
empty-hand weapon-swap defect (bn-builder, `high`), outside every AIB owner path, and it
will not clear from inside this ticket.

`ambition_switches` remains high (1,679 in a single sampled match). `route_changes` median
206 on Arena01 shows the route layer is alive.

**Verdict: AIB26 stays OPEN.** Two of its three gates fail on measurement, one of them
blocked behind a game-side defect this ticket cannot touch. Recommend the next pass start
at `hold_seconds == 0` rather than at the clear, and fix the parser first so the run can
be judged from its own summary.

### 3 Sep (cloud) — `hold_seconds == 0` traced, and it is a miss in my own v7 fix

The v7 write-up recommended the next pass start at `hold_seconds == 0` rather than at the
flank clear. Following that: **the two are the same bug, and I fixed one of them and left
the other standing right beside it.**

`ThinkTactic`'s non-Engage branch carries two clears. The comment above them states the
rule the whole block exists for — *"Engage flaps by design (a 0.2 s belief loss lands in
Search and back), so Search keeps the tactic state; anything else is a different life of
the fight."* v7 made the FLANK latch obey that. The line underneath it,
`HoldSinceSeconds = -1.0;`, stayed **unconditional** — so every excursion out of Engage
reset the hold clock, *including the Search flap the comment calls the same fight*.

With `ambition_switches` at 42-45 per bot-minute and veto at 60%, a stand cannot survive
to `HoldMaxSeconds` under that: the clock is wiped several times a second of standing.
Hence `hold over` 0 everywhere and `hold_seconds` 0.000 on both maps — not a Hold that is
never elected (v6 counted `Hold first` 0-2 per match, so it IS elected), but a Hold whose
clock can never mature. And since F8-5's hold is the mechanism a flank was supposed to be
protected by, a hold that never accumulates is a coherent explanation for flanks that
start in quantity and never complete.

**Fixed:** both clears now read one `bSameFight` predicate, in one place. Same rule, same
line, so the next person cannot fix half of it — which is exactly what I did.

**WRITTEN, NOT COMPILED.** v8 gates for this: `hold_seconds` > 0 and `hold over` lines
present at all (the floor is "the clock matures ever", not a target number); `flank over`
above v7's 1/3; and watch that Hold does not now overstay — `HoldMaxSeconds` is the bound,
and if stands get long the tier row is the knob, not this predicate.


### 3 Sep (Windows terminal, aib-verifier) — v8 measured

**RUNG 3, headless seeded batch.** NOT PIE, NOT packaged, NOT listen+client, NOT a human
playing. Same 10 logs as the AIB22 entry of this date (5 x 300 s per map, `-game -windowed
-nullrhi -unattended`, `LogAIBot Verbose`, 7 ODST bots, no human; the `-server` form is dead
on Windows — see AIB22 for the pilot evidence). The cloud's `bSameFight` fix is COMPILED
here for the first time: `UnrealEditor-AIBot.dll` 20:24:01 vs `AIBBotController.cpp`
20:13:13, and `grep bSameFight` finds it at lines 1168/1169/1184 with both clears under one
predicate. Parser self-test PASS (exit 0).

#### Gate 1 — `hold_seconds > 0` and `hold over` present at all: **FAIL**

`grep -c 'hold over'` across all 10 logs = **0**. `hold_seconds` median 0.000, and per-bot
across 70 bot-matches: **max 0, sum 0, nonzero 0**. Unchanged from v7.

**But the v7 diagnosis is REFUTED by measurement, and the fix is not what is blocking it.**
The claim was that `ambition_switches` at 42-45/bot-min wiped the clock before a stand could
reach `HoldMaxSeconds`. Measured directly, from the idle sampler:

- **3,200 Hold stands** across the 10 matches (`idle over — Ns … tactic=Hold`).
- Longest single Hold stand in the whole batch: **3.4 s**. Second longest 3.0 s.
- **Stands reaching `HoldMaxSeconds` (4.0 s, `AIBDataRows.h:219`): 0 of 3,200.**
- The distribution is 1,213 stands of 0.1 s, 429 of 0.2 s, 314 of 0.3 s; only 4 stands in the
  entire batch exceed 2.0 s. Total Hold standing 1,227.8 s = ~17.5 s per bot per match.

With a perfectly-behaved clock, **not one stand in this batch would have logged `hold over`**
— every one of them ends 0.6 s or more short of the bound. Corroborating, independently:
`still=Hold` appears **0 times in 3,421 `stall over` lines** while `still=StrafeHold` appears
151 times, so `FAIBHoldStationTask::EnterState` — the only caller of `NoteHoldEntered` — is
either not entering or entering for far less than a stall segment. (Caveat named: an idle
segment ends when the body moves, so 3.4 s is a LOWER bound on station residency, not the
residency itself; `FAIBHoldStationTask` emits nothing on entry, so no direct count exists.
That missing entry line is what would settle it, and its absence is why this is the second
run that cannot.)

So: the predicate fix is correct and it did not move this gate, because the gate was never
about the predicate. **`HoldMaxSeconds = 4.0` is above the longest stand the bot ever takes.**
The cloud wrote "if stands get long the tier row is the knob, not this predicate" — stands
are SHORT, and the tier row is still the knob, in the other direction. NOT MINE TO FIX.

#### Gate 2 — `flank over` above v7: **PASS on Spillway, FAIL (tie) on Arena01**

| | v6 | v7 | **v8** | starts (v8) | completion rate |
|---|---|---|---|---|---|
| Spillway | — | 1 of 113 | **6** | 150 | 4.0 % |
| Arena01 | — | 3 of 245 | **3** | 258 | 1.2 % |
| both | 2 of 123 | 4 of 358 (1.12 %) | **9** | 408 | **2.21 %** |

Against the task's aggregate framing ("above v7's 2 of 123") 9 of 408 PASSES. Against this
ticket's own per-map wording ("above v7's 1/3") Spillway 6 > 1 PASSES and Arena01 3 = 3 does
not clear it. Outcomes: 8 `arrived` (0.0-2.7 s), 1 `stalled after 6.4s (F7)`. Flanks still
overwhelmingly do not complete.

**The clear-cause distribution DID move, and that is the fix's real signature.** v6/v7:
"every one cleared as 'the fight ended' within 0.5s". v8, all 10 logs:

    147  flank point cleared — the belief drifted
    114  flank point cleared — the fight ended
     24  flank point cleared — the enemy closed
      8  flank point cleared — stalled
      4  flank point cleared — switched away from Flank

"the fight ended" is no longer the dominant clear; belief drift is. The latch now survives the
Engage->Search excursion as designed. It just hands the flank to a different killer.

#### Gate 3 — Hold overstay: **no overstay. The opposite.**

Longest Hold stand 3.4 s vs a 4.0 s bound; `idle_seconds_tactical` median rose to 62.4 / 72.5
(v7 53.9 / 58.3), which is more NAMED standing, spread thin, never long.

#### v7's "PARSER DEFECT" — **REFUTED**

v7 filed `80_aib_metrics.py` as broken for reporting `flank_count 0.000` beside 113/245 flank
starts. The parser is correct: `flank_count` is built from the `flank_over` regex
(`80_aib_metrics.py:149-150, 348-351`) and counts **completions, not starts**. v8 per-bot
`flank_count` sums to exactly 6 (Spillway) and 3 (Arena01), matching `grep -c 'flank over'`
line for line. Median 0 across 35 bot-samples is the truth when only 6 samples are nonzero.
`island_egress_count 0` is likewise real: `grep -c 'island egress'` is 0 in all 10 logs. No
parser change is owed; the v7 note should be struck so no future run distrusts a good tool.

#### Fairness spot-check

7,467 acquisitions: **0 below the 0.20 s module floor, 0 below the ODST row's 0.22 s min**,
fastest 0.233 s. PASS. 37.7 % run above the row's 0.34 s max, out to 0.433 s — slow, not
fast, so no `high` for aib-critic. Detail in the AIB22 entry.

**VERDICT: AIB26 stays OPEN.** Gate 1 FAIL with its cause now measured and relocated from the
predicate to `HoldMaxSeconds`; gate 2 PASS on Spillway, tie on Arena01; gate 3 no overstay.
Evidence: `Tools/Logs/aib-v8-{spillway,arena01}-{1..5}.log`,
`Tools/aib/baselines/aib22-{spillway,arena01}-verify-v8.json`.
