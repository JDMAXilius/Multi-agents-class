# TICKET — AIB23: Phase 12 TEAM MIND + TARGET CLAIMS

> STATUS: done — lead (Mac) 2026-09-03: all boxes met on W-VERIFY v6; residual = none blocking (corpse re-grants fixed in fix #8, verified by the v6 breakdown when it lands).
> `docs/AIBOT-ROADMAP-2.md` (approved; rulings in §5; law F9 motion is the default). Claimed
> when its W-AUDIT merge lands here.

2v1 with team awareness: shared sightings, capped claims (2) with hysteresis and death/timed expiry, AlliesOnTarget scoring term, seeded ring-spread approach goals; the third bot never idles (F9).

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
- [x] Merge logged; steps refined — 2 Sep
- [x] Builds PASS; specs PASS; no `high` — rung 1 Editor+Game PASS, AIBot 247/0; the W-REVIEW HIGH (callout aim point) closed in the review-fix packet (3 Sep)
- [x] The phase's metric gate PASSES vs the previous baseline; kills/min not worse — pile-up 0 (instant-level) in 10/10 v5 + 10/10 v6; thrash 0; kills/min 11.0/13.6 vs 1.0/1.8 (v6)

## Log
### W-AUDIT (aib-critic) — merged by the lead, 2 Sep
Buildable on existing seams, no new subsystem. Two deviations ADOPTED by the lead under the
founder's "do what you think is best" (recorded for the founder):
- No `UAIBTeamMind` yet: `UAIBTeamCoordinator` already IS the server-only per-alliance
  `UWorldSubsystem` (NM_Client guard, injected ally predicate, release belts wired). Add
  `FAIBSightingLedger` + `FAIBTargetClaims` as headless members; rename to Team Mind in Phase 15
  when the heat grids arrive.
- Ring spread by claim ORDINAL (`angle = Hash(target) + Ordinal*PI`), not `hash(bot,target)`:
  two attackers land on opposite sides by construction. Radius = FightRangeUU (900), not the 350
  acceptance. Samples base/±40°/±80° through the existing projection; no TestPathSync per sample.
Seams: publish sightings at `AIBBotController.cpp:698` after `Sensorium.Pump` — ONLY candidates
with `bSightCurrent`, carrying THEIR `LastSeenAtSeconds` (never Now); consume before the pump via
a new `Sensorium.NoteTeamReport` modelled on `NoteSound` (enters the reaction clock, lands as a
lead). `IsEligible` is NOT widened (a report never becomes a held target = the wallhack line).
Claims: route pawns at `AIBTeamCoordinator.cpp:46-53` to `FAIBTargetClaims` (cap 2 per target per
alliance); grant in the Think commit block (`:775-800`) when Engage wins AND `HasVisibleTarget`,
never in MoveNearBelief::EnterState (re-enters on belief blinks). Renew per think; release by
non-renewal at TTL (reuse 5.0 s), on unpossess/EndPlay, and on target death via the injected
liveness (`AreEnemies` folds "a corpse is nobody's enemy"; `IsAliveTarget`) — no GAS door.
Score: `AlliesOnTarget` (n EXCLUDES self) → ×1/(1+n); the THREAT term is added after the
multiplier, unscaled (never turn a bot away from the man shooting it). Fed from claims only.
Numbers: ClaimCap 2 = module constant (never csv — §5.2); ClaimTtl 5.0 (existing);
ClaimMinHoldSeconds 2.0 (new csv column, DT_AIBTiers reimport by aib-editor script);
IncumbentBonus 0.35 / SwitchMargin 0.15 unchanged. TARGET CLAIMS ARE UNGATED BY TEAMWORK (the
Phase 7 slot-claim gate must not be copied — a non-claiming bot would reopen the pile).
Third bot (F9): denial affects SCORE only, never eligibility. A second enemy → automatic; cap
full with one enemy → `Facts.bTargetClaimSaturated` (negative-only, at `AIBFactsBuilder.cpp:129`)
lets Roam/Objective outscore Engage — Roam moves, so F9 holds; NO "HoldBack" ambition. Lone
enemy, nothing else → engage at 0.33× (correct). Watch the 5 s release-to-reclaim dither at
W-REVIEW; the ambition commit window must exceed it — measure, do not assume.
FAIRPLAY amendment draft text is in the agent report (callout = current sight only, original
stamp, hearing-grade, memory only; claims = negative-only intent, never enumerable, never carry
position/health; death release accepted on the 25-Aug terms). Lead adopts it verbatim for W-BUILD.
W-BUILD ×2 file lists (disjoint): A = SightingLedger/TargetClaims (new) + Coordinator +
Sensorium + BotController.cpp + AIBTypes.h + Claims/Sensorium specs (A commits its constants
FIRST); B = TargetPolicy + FactsBuilder + StateTreeTasks + TreeAuthoring + TargetPolicy spec +
DT_AIBTiers (aib-editor) + AIBDataRows.h. Prerequisite: metrics parsers for
claim_grant/claim_deny/target_pileup_count BEFORE the fix. Phase 12 W-BUILD waits for Phase 11's
builds (shared files).
- 2026-09-03 W-BUILD (aib-builder, worktree 985d708a, merge pending behind the Phase 13 build):
  no `UAIBTeamMind` — three headless members on `AIBTeamCoordinator` (`Team/AIBTargetClaims`,
  `Team/AIBSightingLedger`, `Team/AIBVisitHeat`, NM_Client guards on every mutator). Claims
  granted in the Think commit block when Engage wins with a held target (cap `AIB::TargetClaimCap`
  2 per target per alliance), renewed per think, released by ttl / exit past
  `ClaimMinHoldSeconds` (2) / death via injected liveness / unpossess; the third bot is score-only
  (`bTargetClaimSaturated` -> Engage consideration 0.20 ≈ 0.33x); ring spread by claim ordinal at
  radius FightRange − Acceptance (550, DEVIATION from 900: a 900 ring + 350 acceptance parks a bot
  1250 out with tactic=none). Sightings published post-pump (`bSightCurrent` only, the
  publisher's own stamp), consumed pre-pump as a lead never a sight (`TeamReport` reaction kind,
  `TeamReportIntervalSeconds` 1, stale 0.5). Team-only visit heat grid (`VisitHeatCellUU` 500,
  decay 30 s) stamped every Think, Wander draws best-of-3 coldest (Phase 11 step 6 debt paid).
  Lines exactly the parser's Phase 12 shapes (GRANTED/DENIED -> x/RELEASED reason=, team report).
  Specs `AIBot.Sim.TargetClaims` 8 + `AIBot.Sim.TeamMind` 8. Tree unchanged. Off-list but
  required: `Brain/AIBConsideration.*` selector + one Engage consideration (Phase 15 touches the
  same block — merge by hand). The Wander draw edit overlaps AIB22 fix #4 R6/R7.
- 2026-09-03 merged into main (054af0b7; one additive row-field conflict). Building with Phase 15.
- 2026-09-03 rung 1 on main with EVERYTHING merged (AIB22 fix #4, Phases 12/13/14/15, the BN
  retirement, the game-side crowd/seed hooks): BreachpointEditor PASS, Breachpoint PASS (server
  target unbuildable on this engine). Merge compile fixes by the lead: duplicate `BotIndex`
  member/accessor (Phase 14 owns the seed triple), `MatchSeed` shadow, Flank task on the
  controller-held locomotion signature, spec literal/lambda fixes, `FAIBOverlapEpisode` closing
  braces. Specs running; W-REVIEW x4 dispatched on the merged commits.
- 2026-09-03 W-REVIEW (aib-critic on 985d708a/054af0b7): ONE HIGH, four MEDIUM, three LOW.
  H1 a team report writes a teammate's LIVE read of the enemy into the LastKnownLocation of a
     candidate that is already damage-eligible (shot me within 12 s) and `bMayFire` has no
     sight-current/LOS test — the bot fires through the wall at the callout (FAIRPLAY 2 Sep
     condition 3). RULING: a report never overwrites a candidate the bot has sensed by ANY door
     (damage included) and never feeds an aim point: reports create/refresh LEADS only on
     never-sensed candidates; and `bMayFire` requires the bot's OWN current sight (or LOS to the
     believed point) — a callout can move the feet, never the trigger.
  M2 claims leak across a target switch (ghost holders fill the cap). RULING: switching targets
     releases the previous claim (reason=switch).
  M3 the MinHold hysteresis is inverted (a one-think blink at t>2 s releases; the fighter is
     then DENIED). RULING: exit releases only when the target is DEAD or the bot has committed
     to a non-Engage ambition for ≥1 s; a blink never releases (TTL still lapses it).
  M4 saturation (0.328 vs Roam 0.2) does not free the third bot in the clean case and starves
     a bot being shot at. RULING: `bTargetClaimSaturated` is FALSE whenever the bot took damage
     from that target within 3 s (the victim always engages); the saturated Engage consideration
     drops to 0.05 so Roam/Objective win when nobody shoots at you.
  M5 every non-holder takes the same +90° ring slot (two denied bots stack). RULING: non-holder
     slots spread by a seeded per-bot phase; L3 the ring angle seeds off LifeSeed, not UniqueID.
  L1 published pair (live Where, gain-time stamp) conservative; L2 null-pawn claimant can free a
     third claim — risk register. Specs cannot see H1/M2–M4 — add the report-onto-eligible and
     target-switch cases. PASS: F1 floor, F5 relay laundering, claims as information, death
     release via injected liveness, heat team-only, server-only, no Tick.
- 2026-09-03 review-fix packet (aib-builder on main, 21 files, commit "review fixes"): every HIGH
  and MEDIUM ruling in this ticket's W-REVIEW entry is closed on disk (see the packet's per-ruling
  lines in the commit); AIB26 H1 CHANGED THE TREE (tactic children complete to Engage, gun tasks
  `bShouldStateChangeOnReselect=false`) — ST_AIBBot rebuild queued. New/changed shapes: `decide …
  tcommit= treason=` (Verbose unless `-AIBReplay`), `target claim RELEASED … reason=switch`,
  `idle over … tactic=Crowd`, `crowd simulation DISABLED — …`. Building (editor closed).
- 2026-09-03 W-VERIFY v5 (5 x 2 maps): cap-2 holds — instant-level concurrent holders never
  exceeded 2 in 10/10 matches; the judge's 8/9 pile-up buckets were a parser artefact (hand-off
  inside one second) — fixed (91e3085d), `target pile-up buckets` now PASS on both maps. Real
  defect found: corpse re-grants (`GRANTED`/`RELEASED reason=death` alternating 7x in 0.6 s,
  30–50 % of grants) — fix #8 F8-2 refuses claims on dead targets. Team reports and DENIED ->
  roam|rally|retreat present; no engage-anyway. kills/min PASS both maps.
