# TICKET — AIB2: the executor goes live (tree built, bots move, BotSystem=AIB in PIE)

> STATUS: open — cut 26 Aug 2026 by the cloud lead. Phase 3's module half is landed
> **WRITTEN, NOT COMPILED** (the cloud container has no engine). Needs the ENGINE ON DISK
> for step 1 and a **LIVE EDITOR** from step 2 on (the MCP trigger lives inside it).

Phase 3 of `docs/AIBOT-ROADMAP.md`, second half. What landed since AIB1 closed:

- `Execution/AIBExecutor.h` — the seam (`IAIBExecutor::Start/Stop`); `AIBStateTreeExecutor.h/.cpp`
  — the StateTree impl over the controller's `UStateTreeAIComponent`.
- `Execution/AIBStateTreeTasks.h/.cpp` — the node vocabulary: **6 conditions + 10 tasks**
  (five per-ambition gates and two POI movers derive hidden bases; per-branch knobs are
  C++ **virtuals**, not node parameters, because the compiled authoring surface sets
  nothing on a node it adds — every proven call is a zero-arg `AddTask`/`AddEnterCondition`).
- `Execution/AIBTreeAuthoring.h/.cpp` — builds/compiles/saves `/Game/AIBot/AI/ST_AIBBot`
  (flat: Root > one gated branch per ambition, an ambition SENTINEL task in each) and
  mints `/Game/AIBot/Data/DT_AIBTiers` (one Default row mirroring FAIBTierRow). Whole TU
  `#if WITH_EDITOR`, honest stubs otherwise.
- `Data/AIBAssetSettings.h/.cpp` — the Transient-bool MCP trigger (the host's pattern).
- Controller wiring — `UStateTreeAIComponent` subobject (auto-start OFF), `BotStateTree`
  Config soft path, `LastFacts` cache + `GetLastFacts()`, executor Start/Stop at
  possession edges.
- `Config/DefaultGame.ini` — `[/Script/AIBot.AIBBotController] BotStateTree=...`.
- `Tools/aib/70_aib_assets.py` — probe / build / audit over MCP (bn/62's proven mechanics).

## Kickoff (machine-checkable)

- requires: engine-installed, editor-open (from step 2)
- `git pull` shows `Source/AIBot/Execution/` with four .cpp/.h pairs and `Tools/aib/70_aib_assets.py`
- owner_path: `docs/tickets/TICKET_AIB2_EXECUTOR_LIVE.md`
  <!-- Log only. A compile error's FIX is aib-builder's (cloud) — paste it verbatim and
       STOP, unless it is a missing include or an obvious typo: then fix, mark, commit
       separately. -->

## Steps (in order)

1. **Rung 1** — `./Tools/run-ubt.sh BreachpointEditor Breachpoint` (Server stays
   environmental per AIB1). This is the first compile of everything listed above.
2. **Open the editor**, then `python3 Tools/aib/70_aib_assets.py probe` — all 16 node
   structs + the trigger CDO must resolve. STALE editor = STOP, rebuild, restart.
3. `python3 Tools/aib/70_aib_assets.py build` — pulls the trigger; paste the LogAIBot
   authoring report (asset / schema / states / compile / save lines) and the read-back.
   `compile : OK` and `IsReadyToRun YES` are the gates — a saved-but-uncompiled tree runs
   NOTHING and every bot stands still.
4. **Rung 2 regression** — `./Tools/run-specs.sh AIBot`: still **41/41/0** (no spec
   changed; this catches an executor include breaking a worldless suite).
5. **Flip LOCALLY, do not commit the flip:** `BotSystem=AIB` in DefaultGame.ini, PIE with
   bots. Run `docs/AIBOT-PROTOCOLS.md` **P-1** (first possession — the five log lines),
   **P-2** (fairness latency sample), **P-3** (arbitration walk). Paste the log excerpts
   each protocol names.
6. Revert the local flip. Re-run the four mechanical checks from AIB1 (same four greps,
   quoted globs — zsh eats unquoted `--include=*.h`), paste all four empty.

## Watch-list — written-not-compiled spots flagged for honest scrutiny

- `UStateTreeAIComponent` as a controller `CreateDefaultSubobject` + `SetStartLogicAutomatically(false)`
  — transcribed from the host controller; first use in THIS module. Include is
  `Components/StateTreeAIComponent.h`.
- `SetStateTree` / `StartLogic` / `StopLogic(reason)` on the component — host-transcribed.
- The whole authoring TU: `UStateTreeFactory::SetSchemaClass`, `UStateTreeEditorData`
  (`SubTrees.Reset`, `AddRootState`, `AddChildState`, `AddEnterCondition<T>`, `AddTask<T>`,
  `AddTransition` + `bDelayTransition`/`DelayDuration`), `UStateTreeEditingSubsystem::
  ValidateStateTree/CompileStateTree`, `FStateTreeCompilerLog::DumpToLog`, `UPackage::Save`
  — every call transcribed from the host's compiled `BNBotAuthoring.cpp`, re-typed here.
- **USTRUCT node inheritance with virtual knob overrides** (`FAIBGateEngageCondition` :
  `FAIBAmbitionGateCondition` etc.) — the one shape the host did NOT use. The runtime
  dispatches node methods virtually (that is how Tick reaches any node), and the baked
  values live in code, not serialized state — but this is the ticket's highest-scrutiny
  item. If the StateTree compiler rejects derived node structs, the fallback is flat
  per-branch structs with duplicated bodies; paste the compiler message first.
- `FStateTreeTransition&` returned by `AddTransition` being mutable (delay fields) — used
  by the host, same pattern, re-typed.
- `Context.GetInstanceData(*this)` from a DERIVED node type resolving the base's
  `FInstanceDataType` — watch for a type-mismatch assert at tree start.
- The `meta = (Hidden)` on the two base nodes — cosmetic if wrong, not blocking.
- `GetRandomReachablePointInRadius` + `FNavigationSystem::GetCurrent<UNavigationSystemV1>`
  in the wander task — standard engine API, first use in this module.
- `UAIBAssetSettings` CDO path for MCP: `/Script/AIBot.Default__AIBAssetSettings`.

## Done when

- [ ] Rung 1 PASS (Editor + Game; Server recorded environmental)
- [ ] probe: 16/16 node structs + trigger OK against the RUNNING editor
- [ ] build: authoring report pasted; `compile : OK`, read-back `IsReadyToRun YES`,
      states listed = Engage/Retreat/Search/Seek/Roam under Root
- [ ] Rung 2 still 41/41/0, reconciled
- [ ] P-1, P-2, P-3 excerpts pasted from a `BotSystem=AIB` PIE (flip local-only, reverted)
- [ ] Four mechanical checks pasted, empty
- [ ] Deviations recorded

## Log

_(terminal: outputs verbatim)_
