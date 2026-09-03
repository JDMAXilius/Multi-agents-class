# TICKET — AIB27: Phase 16 HYGIENE

> STATUS: done — lead (Mac) 2026-09-03: BN legacy bot system retired end to end; the four contract gaps that need the founder are listed in docs/tickets/HANDOFF.md ("Decisions the founder must make").
> `docs/AIBOT-ROADMAP-2.md` (approved; rulings in §5; law F9 motion is the default). Claimed
> when its W-AUDIT merge lands here.

Retire the dead BN and BR bot systems, rewrite ai-builder.md, STATUS lines on every AIB ticket, stand up Breachpoint.Bots.*, unblock Gauntlet or record why.

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
- [x] Builds PASS; specs PASS; no `high` — BN retirement (files + the three uassets + row structs) compiles on Editor+Game, BreachpointNext 36/0 (3 Sep)
- [x] The phase's metric gate PASSES vs the previous baseline; kills/min not worse — hygiene phase: no metric gate; the retired system is gone and the AIBot batch is the acceptance instrument

## Log
### W-AUDIT (Explore) — merged by the lead, 2 Sep; small safe edits applied
DONE NOW (lead, docs/config only): `aib-builder.md` / `aib-editor.md` owner path `Source/AIBot/` →
`Plugins/AIBot/Source/AIBot/` (the plugin moved in Phase 10; guard-hook relevant). STATUS
vocabulary normalised (AIB17/18/19 BUILT/landed → in-progress; AIB22 claimed → in-progress;
AIB21 → in-progress, M1 is written-not-compiled; AIB2's duplicate 26 Aug STATUS block struck).
Correction: every AIB ticket already HAD a STATUS line — the roadmap bullet was stale; the real
defect was state accuracy. `run-gauntlet.ps1`: the BR_Arena01 blocker text corrected where found.
SCHEDULED (serial packets, after Phase 11's builds because BNGameMode.cpp is shared):
- Retire the BN legacy bot system (lead, `Source/BreachpointNext/` is in this session's owner
  path): delete `Source/BreachpointNext/AI/` + `Tests/BNBotBrainSpec.cpp`; BNGameMode loses the
  A/B switch (`BotSystem` gone, AIB class only); BNProjectile drops the `ABNBotController`
  branch; BNGameData drops the two Find*Row + tables; BNDataRows drops the two structs AFTER the
  two DT_BN* uassets and ST_BNBot are deleted (editor pass, redirectors); ini keys :341-344,
  :352-356, :465-473, :616-622, :639 removed; Tools/bn/60,61,62 deleted; Build.cs UnrealEd block
  loses its only consumer. Rung 1 on all targets.
CONTRACT_GAP raised to the founder (outside every AIB owner path):
- The BR-era bot system (`Source/Breachpoint/AI/`, `Content/AI/ST_Bot` (an empty 1 KB stub),
  `Content/Data/DT_BotAmbitions|DT_BotTuning` + csv, `BRDataRows.h` row structs) is fully dead and
  unreferenced but sits in `ai-builder`'s owner path under a closed ruling (DESIGN-RULINGS BP103)
  and the `BREACHPOINT-AI-BOTS.md` design. Retiring it needs a new dated ruling + a named owner.
- `ai-builder.md` describes code that does not exist (ST_Bot spine, `UBRSpotterSubsystem`,
  `Breachpoint.Bots.Brain`); retire it and fix the 20 documents/agents that route to it by name.
- The test contract names `Breachpoint.Bots.*` (testing.md:108, QUALITY-BARS:60) but the bot
  suite is `AIBot.Sim.*` (14 specs). Options: rename three specs (Ambition/Plan/Action) — breaks
  every citation of `AIBot.Sim.X` incl. AIB21's closing gate — or amend the contract to name
  `AIBot.Sim.*`. Lead recommends amending the contract (one edit, law needs the founder's sign-off).
- Gauntlet stays BLOCKED (exit 3): node `BRGauntlet.SmokeTS2C` never authored (BP00 step 3),
  the wrapper is Win64/RunUAT.bat-only with no Mac counterpart, and the scenario is a dedicated-
  server test whose survival hangs on BN38's unwritten BreachpointServer ruling. PIE is not a
  substitute.
- 2026-09-03 BN legacy retirement, files-only half (bn-builder, worktree, merged e3a7eca8):
  `Source/BreachpointNext/AI/` + `Tests/BNBotBrainSpec.cpp` + `Tools/bn/60-62` deleted; BNGameMode
  loses `BotSystem`/`BotControllerClass` (AIB class only, same spawn path); BNProjectile drops
  the BN branch; BNGameData drops both Find*Row + tables (now `UCLASS()`); BNAssetSettings loses
  the `bRebuildBotAssets` button (a third consumer the ticket missed); Build.cs drops the
  UnrealEd block AND `StateTreeModule`/`GameplayStateTreeModule` (BN-bot-only); ini keys gone
  (`AIBBotControllerClass` kept). 34 files, +11/−6402. NOT yet compiled — rung 1 on all
  buildable targets runs with AIB22 fix #4 once the v4 batch frees the machine. Left for the
  editor pass: `FBNBotTuningRow`/`FBNBotAmbitionRow` after DT_BNBot*/ST_BNBot are deleted.
  Stale prose to scrub later: aib-verifier.md:24 (A/B match), run-specs.sh:13 usage example,
  70_aib_assets.py:7 docstring.
- 2026-09-03 rung 1 on main with EVERYTHING merged (AIB22 fix #4, Phases 12/13/14/15, the BN
  retirement, the game-side crowd/seed hooks): BreachpointEditor PASS, Breachpoint PASS (server
  target unbuildable on this engine). Merge compile fixes by the lead: duplicate `BotIndex`
  member/accessor (Phase 14 owns the seed triple), `MatchSeed` shadow, Flank task on the
  controller-held locomotion signature, spec literal/lambda fixes, `FAIBOverlapEpisode` closing
  braces. Specs running; W-REVIEW x4 dispatched on the merged commits.
