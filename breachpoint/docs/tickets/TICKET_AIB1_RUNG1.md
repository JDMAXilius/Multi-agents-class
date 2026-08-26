# TICKET — AIB1: first compile of the AIBot module, and its first spec counts

> STATUS: in-progress — mac terminal 25 Aug 2026 (041fb2d). Engine on disk, no editor open.

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
   (5 tests), `AIBot.Sim.Sensorium` (19) and `AIBot.Sim.AmbitionEngine` (17) —
   **41 started, 0 failures**. Zero ran = INCONCLUSIVE, never pass. Paste the counts. (Note the project's default rung-2
   filter does NOT include `AIBot.Sim.*` — the explicit filter argument is mandatory.)
3. **The four mechanical checks** (post-W-REVIEW forms; paste all four empty):
   - boundary (CASE-INSENSITIVE, word-alone shapes included): `grep -rniE "breachpoint|\bBN([A-Z_[:space:]-]|$)" Source/AIBot/ --include=*.h --include=*.cpp --include=*.cs`
   - replication: `grep -rn "Replicated\|DOREPLIFETIME\|NetSerialize" Source/AIBot/ --include=*.h --include=*.cpp`
   - worldless brain: `grep -rn "UWorld\|AActor\|GetWorld" Source/AIBot/Brain/ Source/AIBot/Skills/`
   - F8 quarantine: `grep -rn "GetPerceptionComponent\|HasActiveStimulus\|GetCurrentlyPerceivedActors" Source/AIBot/ --include=*.h --include=*.cpp | grep -v AIBBotController.cpp`

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
- `EAllowShrinking::No` in `AIBReactionClock.cpp` — the 5.5+ enum form of RemoveAt.
- `SightConfig->SetMaxAge(...)` — proven in the old module's controller, first use here.
- The sensorium spec now builds a real `UWorld` + spawned actors (the host damage spec's
  proven fixture) — first use of that fixture in THIS module.
- `Sensorium.SetRandomSeed(static_cast<int32>(GetUniqueID()))` — per-bot seeding.
- `FRuntimeFloatCurve` + `GetRichCurve()/GetRichCurveConst()->Eval` in Brain/ — the
  inline-curve type, first use in this module; and `TOptional<float>` selector returns.
- `NewObject<UAIBAmbitionEngine>(this)` in OnPossess + the engine spec's
  `GetTransientPackage()` construction (the host killfeed spec's proven pattern).
- The blast perceivability gate's engine calls: `GetActorEyesViewPoint`,
  `LineTraceSingleByChannel(ECC_Visibility)`, `FCollisionQueryParams` with
  `SCENE_QUERY_STAT` — standard engine API, first use in THIS module.
- `FRichCurve::Eval`'s no-keys default-return contract is now PINNED by a spec
  ("treats an unauthored curve as identity") — if that test fails, the engine
  contract differs from the module's reading and the consideration default needs
  rethinking, not just the test.

## Game-side additions (Phase 3's adapter half, same rung-1 run)

The compile now also covers `Source/BreachpointNext/AIBotAdapter/` (the avatar adapter),
the `BotSystem` A/B switch in `BNGameMode` (widened to `AAIController` in four audited
places), the projectile's AIB blast branch, and the `"AIBot"` dependency in
BreachpointNext.Build.cs. **Default is `BotSystem=BN` — behaviour must be UNCHANGED until
the ini flips.** Watch: the cross-module includes (`Core/AIBTags.h` etc. resolve against
AIBot's exposed root while BN has its own `Core/` — filenames disambiguate, first use of
that shape), `IsA<AAIBBotController>`, and `FindComponentByClass` + `RegisterComponent`
at possession time.

## Done when

- [ ] Rung 1 PASS, all three targets, output tail in the Log
- [ ] Rung 2: 41 started / 0 failed, counts pasted
- [ ] All four mechanical checks pasted, empty
- [ ] Any deviation (a fixed typo, a real error handed back) recorded in the Log

## Log

_(terminal: outputs verbatim)_
