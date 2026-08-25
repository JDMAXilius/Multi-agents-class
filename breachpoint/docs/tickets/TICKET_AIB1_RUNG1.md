# TICKET — AIB1: first compile of the AIBot module, and its first spec counts

> STATUS: open — cut 25 Aug 2026 by the cloud lead. Needs the ENGINE ON DISK; **no live
> editor** (rung 2's gate requires none holding the project). Phases 0 and 1 of
> `docs/AIBOT-ROADMAP.md` are landed WRITTEN, NOT COMPILED — this ticket is the proof.

The module is `Source/AIBot/` — self-contained, engine-deps only, `GameplayAbilities`
deliberately absent from its Build.cs (GAS purity as a linker guarantee). It is registered
in `Breachpoint.uproject` and all three `Target.cs`. Phase 1 (sensorium, reaction clock,
target memory) is worldless C++ with a spec suite.

## Kickoff (machine-checkable)

- requires: engine-installed
- `git pull` shows `Source/AIBot/` with Perception/ implemented and two Tests/ specs
- owner_path: `docs/tickets/TICKET_AIB1_RUNG1.md`
  <!-- Log only. If a compile error needs a FIX, that is aib-builder's (cloud) — paste the
       error verbatim in the Log and STOP unless the fix is a missing include or an obvious
       typo, in which case fix it, mark it clearly in the Log, and commit it separately. -->

## Steps (in order)

1. **Rung 1** — `Tools/run-ubt.ps1` (or the mac equivalent): all three targets. The
   editor target additionally proves the `Target.bBuildEditor` block resolves.
2. **Rung 2** — `Tools/run-specs.sh AIBot` . Expected: suites `AIBot.Sim.Scaffold`
   (4 tests) and `AIBot.Sim.Sensorium` (11 tests) — **15 started, 0 failures**. Zero ran
   = INCONCLUSIVE, never pass. Paste the counts.
3. **Boundary check, mechanically:**
   `grep -rn "Breachpoint" Source/AIBot/ --include=*.h --include=*.cpp --include=*.cs`
   must return NOTHING. Paste the empty result.

## Watch-list — written-not-compiled spots the cloud flags for honest scrutiny

- `Cast<IAIBAvatarInterface>(Component)` in `Core/AIBBotController.cpp` — the interface
  cast on a plain component pointer.
- The `UINTERFACE(MinimalAPI, NotBlueprintable, ...)` blocks in `Interfaces/` — pattern
  taken from the old module's `BRServerLifecycle.h`, first use in THIS module.
- `UAISense::GetSenseID<UAISense_Hearing>()` in the perception handler — transcribed from
  the host controller's compiled usage, re-typed here.
- `UCLASS(Config=Game)` + `UPROPERTY(Config) float ThinkIntervalSeconds` — its ini section
  would be `[/Script/AIBot.AIBBotController]`; nothing writes it yet, existence is enough.
- `AIBOT_API` on a namespace'd `inline constexpr` (`AIB::MinReactionSeconds`) is NOT used —
  the constant is header-inline on purpose; flagging so nobody "fixes" it into an export.

## Done when

- [ ] Rung 1 PASS, all three targets, output tail in the Log
- [ ] Rung 2: 15 started / 0 failed, counts pasted
- [ ] Boundary grep pasted, empty
- [ ] Any deviation (a fixed typo, a real error handed back) recorded in the Log

## Log

_(terminal: outputs verbatim)_
