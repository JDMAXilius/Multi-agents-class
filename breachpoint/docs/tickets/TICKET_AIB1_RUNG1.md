# TICKET — AIB1: first compile of the AIBot module, and its first spec counts

> STATUS: open — RUNG 1 now COMPILES (Editor+Game; Server is environmental). RUNG 2 is
> **40/41 — ONE REAL FAILURE**, and the runner reported it as a pass. Handed back to
> `aib-builder`. Superseded status line below kept for the record:
> ~~RUNG 1 FAILED, handed back to `aib-builder`.~~ mac terminal 25 Aug 2026
> (2b59469). Two real compile errors, BOTH confined to `Source/AIBot/Tests/`; the module's
> own code compiled clean. Mechanical checks 1-4 all pass. Rung 2 is BLOCKED, not failed.

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

### 25 Aug 2026 — mac terminal. RUNG 1 FAILED. Handed back per this ticket's own rule.

**Step 1 — Rung 1, all three targets: FAIL.**

```
== RUNG 1 SUMMARY ==
  FAIL    BreachpointEditor (exit 6) - Tools/Logs/ubt-BreachpointEditor.log
  FAIL    Breachpoint (exit 6)       - Tools/Logs/ubt-Breachpoint.log
  FAIL    BreachpointServer (exit 6) - Tools/Logs/ubt-BreachpointServer.log
exit 1
```

`BreachpointServer` is the KNOWN environment failure, not a code one — this is an Epic
Launcher install and UBT says so in as many words: *"Server targets are not currently
supported from this engine distribution."* Editor and Game are real, and they fail
identically.

**THE GOOD NEWS FIRST, because it is the load-bearing part: the AIBot module's own code
compiled clean.** Every error is in `Source/AIBot/Tests/`. Nothing in `Core/`, `Brain/`,
`Perception/`, `Skills/`, `Execution/`, `Team/`, `Data/`, `Debug/` or `Interfaces/` failed,
and neither did the game-side `AIBotAdapter/`, the widened `BNGameMode`, the projectile's
AIB branch or the cross-module includes. Every item on this ticket's watch-list — the
`Cast<IAIBAvatarInterface>`, the `UINTERFACE(MinimalAPI…)` blocks, `EAllowShrinking::No`,
`SetMaxAge`, `FRuntimeFloatCurve`/`GetRichCurve`, `UCLASS(Config=Game)`, the
`GetActorEyesViewPoint`/`LineTraceSingleByChannel` gate — **survived the compiler.** The
two failures are both in test code and both are the same shape: an assumed API that does
not exist.

**Error 1 — `FNativeGameplayTag` has no `IsValid()`. 14 sites,
`Source/AIBot/Tests/AIBScaffoldSpec.cpp`.**

```
Source/AIBot/Tests/AIBScaffoldSpec.cpp:31:45: error: no member named 'IsValid' in 'FNativeGameplayTag'
   31 |      TestTrue(TEXT("Fire"), AIBTags::Verb_Fire.IsValid());
      |                             ~~~~~~~~~~~~~~~~~~ ^
```

`FNativeGameplayTag` is the *declaration* wrapper, not the tag. It converts to `FGameplayTag`
via `GetTag()`; `IsValid()` lives there. Likely fix: `AIBTags::Verb_Fire.GetTag().IsValid()`.

**Error 2 — `TestEqual` has no `FGameplayTag` overload. 15 sites,
`Source/AIBot/Tests/AIBAmbitionEngineSpec.cpp`.**

```
Source/AIBot/Tests/AIBAmbitionEngineSpec.cpp:85:3: error: no matching member function for call to 'TestEqual'
   85 |      TestEqual(TEXT("the giant is vetoed"), Engine->Rescore(Facts, 1.0), AIBTags::Ambition_Roam);
      |      ^~~~~~~~~
AutomationTest.h:1985: note: candidate not viable: no known conversion from 'FGameplayTag' to 'const int32'
AutomationTest.h:1988: note: candidate not viable: no known conversion from 'FGameplayTag' to 'const float'
```

`FAutomationTestBase::TestEqual` overloads cover int32/int64/SIZE_T/float/double/FString and
a few math types — there is no `FGameplayTag` one, and no implicit conversion to any of them.
Both targets stopped at `-ferror-limit`, so 20 reported lines are the cascade of these 15.

**NOT FIXED, deliberately.** This ticket says to fix only *"a missing include or an obvious
typo"* and otherwise paste and STOP. Neither of these is that: they are the same wrong
assumption about two engine APIs, repeated across 29 call sites in two files, and choosing
the replacement is a design call (compare `.ToString()`, or assert with
`TestTrue(A == B)` and lose the printed diff on failure). That belongs to `aib-builder`.

**Step 2 — Rung 2: BLOCKED, not failed, and not run.** The module does not link, so the
suites cannot exist. Recording zero here would be indistinguishable from a filter typo,
which is exactly the trap this ticket warns about. Expected when it does run:
`AIBot.Sim.Scaffold` 5 + `AIBot.Sim.Sensorium` 19 + `AIBot.Sim.AmbitionEngine` 17 = 41.

**Step 3 — the four mechanical checks: ALL FOUR EMPTY, ALL PASS.** These are static greps
and are valid independent of the compile.

| # | check | result |
|---|---|---|
| 1 | boundary — `breachpoint`/`BN` leakage into `Source/AIBot/` | EMPTY — PASS |
| 2 | replication — `Replicated`/`DOREPLIFETIME`/`NetSerialize` | EMPTY — PASS |
| 3 | worldless — `UWorld`/`AActor`/`GetWorld` in `Brain/` + `Skills/` | EMPTY — PASS |
| 4 | F8 quarantine — perception outside `AIBBotController.cpp` | EMPTY — PASS |

A method note for whoever repeats these on macOS: unquoted `--include=*.h` is expanded by
zsh before grep sees it and the command dies with *"no matches found"* while LOOKING like a
clean pass. Quote them. Three of the four silently did not run on the first attempt.

**Step 4 — deviations: none.** Nothing was fixed, nothing edited outside this Log. One
pre-existing warning seen in passing and NOT caused by AIBot:
`BNAnimInstance.cpp:401,416` — `CalculateDirection` is deprecated and its message says the
project *"will no longer compile"* on the next engine upgrade.

**Next:** `aib-builder` fixes the 29 sites; this ticket re-runs from step 1 unchanged.

### 25 Aug 2026 — `aib-builder`. The 34 test-only sites fixed. Editor target PASSES.

Both errors were mine and both were in `Source/AIBot/Tests/` only. Nothing outside `Tests/`
was touched; no module code needed a change, which is the right outcome — the assertions
were wrong, not the contracts they assert.

**Error 1 — `FNativeGameplayTag::IsValid()` → `.GetTag().IsValid()`. 14 sites,
`AIBScaffoldSpec.cpp`.** Read from `Engine/Source/Runtime/GameplayTags/Public/NativeGameplayTags.h`:
`FNativeGameplayTag` exposes exactly `GetTag()` and an implicit `operator FGameplayTag()`,
and nothing else tag-shaped. `IsValid()` is `FGameplayTag`'s
(`GameplayTags/Classes/GameplayTagContainer.h:138`).

**Error 2 — no `FGameplayTag` overload of `TestEqual`. 22 sites, `AIBAmbitionEngineSpec.cpp`**
(the lead's count of 15 was the `-ferror-limit` truncation; two of the 22 are multi-line
calls that a first pass missed and the compiler caught on the next run).

One file-local helper in the spec's DEFINE block, no module change:

```cpp
bool TestTag(const TCHAR* What, FGameplayTag Actual, FGameplayTag Expected)
{
    return TestEqual(What, Actual.GetTagName(), Expected.GetTagName());
}
```

**The design call, in one line: compare `GetTagName()` and take the `FName` overload
(`AutomationTest.h:2003`), because its failure path is
`AddError("Expected '%s' to be %s, but it was %s")` — a failing arbitration test NAMES the
ambition that won, which `TestTrue(A == B)` cannot, and it costs no `FString` temporary the
way `.ToString()` would.** The implicit `operator FGameplayTag()` means `TestTag` takes the
raw `AIBTags::` natives and the `FGameplayTag` returns of `Rescore`/`GetCurrent` without a
cast at any of the 22 sites.

**Rung 1 — `./Tools/run-ubt.sh BreachpointEditor` (single target on purpose; Server cannot
link on this launcher install and would mask the result):**

```
--- BreachpointEditor : started 2026-08-25 20:53:02 ---
Deploying now!

Result: Succeeded
Total execution time: 5.80 seconds

== RUNG 1 SUMMARY ==
  PASS    BreachpointEditor (exit 0, touched libUnrealEditor-AIBot.dylib)
  PARTIAL - fewer than three targets. Report as PARTIAL, not rung 1.
```

`PARTIAL`/`exit 1` is the script refusing to call one target a rung — correct, and the
reason the Done-when boxes stay unticked here. **The full three-target re-run and rung 2 are
the lead's**, not run by me, and no editor was opened.

Boundary grep re-run on this diff, both forms, both empty:
`grep -rn "Breachpoint\|BNCharacter\|\"BN" Source/AIBot/ --include="*.h" --include="*.cpp"` → empty.
`grep -rniE "breachpoint|\bBN([A-Z_[:space:]-]|$)" Source/AIBot/ --include="*.h" --include="*.cpp" --include="*.cs"` → empty.
(The previous Log's zsh warning is real and bit me once: unquoted `--include=*.h` dies with
*"no matches found"* before grep runs. Quoted above.)

**Expected rung-2 counts are unchanged at 41** — no test was added, removed, or merged; 34
assertion *expressions* changed, zero `It(...)` blocks.

### 25 Aug 2026 (re-run) — compiles now. Rung 2 is 40/41, and the runner lied about it.

**Step 1 — Rung 1 after `aib-builder`'s test fix:**

```
== RUNG 1 SUMMARY ==
  INCONC  BreachpointEditor (exit 0, already up to date - no binary relinked)
  PASS    Breachpoint (exit 0, touched CodeResources)
  FAIL    BreachpointServer (exit 6)
```

`BreachpointEditor` is INCONCLUSIVE only because `aib-builder` had just relinked it in its
own verification — the code compiles. `Breachpoint` is a clean PASS. `BreachpointServer`
remains the environmental failure: *"Server targets are not currently supported from this
engine distribution."* **This ticket's "all three targets PASS" box is unsatisfiable on an
Epic Launcher install and always was** — recorded here rather than quietly ticked.

**Step 2 — Rung 2: 41 started, ONE FAILED. `run-specs.sh` printed `0 failures`.**

```
AIBot.Sim.Sensorium.the sensorium (F2 end to end, real actors).
  matures a sighting into a visible target — and not before the clock says
Error: Expected 'last seen where the stimulus said' to be 5.000000,
       but it was 0.000000 and outside tolerance 0.000100.
```

Counted straight out of the log: `Test Started` 41, `Result={Success}` **40**,
`Result={Fail}` **1**. The suite total of 41 matches this ticket's expectation exactly, so
the *count* gate passed while the *content* did not.

## contract_gap — RUNG 2 CANNOT SEE A FAILING TEST. `Tools/` is outside this packet.

`Tools/run-specs.sh:73`:

```bash
FAILED="$(grep -cE "Automation Test Failed|Error: Test Failed" "$LOG" || true)"
```

UE 5.8's automation controller does not emit either string. It emits:

```
LogAutomationController: Error: Test Completed. Result={Fail} Name={...}
```

So `FAILED` is always 0 and **every rung-2 run in this project reports PASS regardless of
whether tests failed.** This is precisely the failure the script's own header warns about —
*"a spec run that finds NO tests is INCONCLUSIVE, never PASS … reports 0 tests, 0 failures
and looks exactly like success"* — the author guarded the zero-tests trap and missed the
failing-test one directly beside it.

**Suggested fix (one line, for whoever owns `Tools/`):**
```bash
FAILED="$(grep -cE "Result=\{Fail\}" "$LOG" || true)"
```

**Blast radius, checked rather than assumed.** Every spec log from today was audited:

| log | started | success | fail |
|---|---|---|---|
| specs-…205447 (AIBot) | 41 | 40 | **1** |
| the other 11 runs today (BN) | 30 | 30 | 0 |

So no BN result reported this session was wrong — all eleven were genuinely 30/30. That is
luck, not the tool: the detector has been blind the whole time and the first genuinely
failing spec in this project is the one that exposed it.

**Step 3 — the four mechanical checks: re-run after the fix, ALL FOUR EMPTY, ALL PASS.**
(Quoted `--include` globs; see the earlier zsh note. `aib-builder` independently hit the
same trap.)

**Step 4 — deviations.** `aib-builder` corrected the earlier site count: 22 `TestEqual`
sites, not 15 — two were multi-line calls whose tag argument sits on the continuation line,
which a line-oriented grep undercounts. It chose `TestEqual(What, Actual.GetTagName(),
Expected.GetTagName())` so a failing arbitration test prints WHICH ambition won; `TestTrue`
would have printed nothing and `.ToString()` allocates two temporaries per assert.

**Still open:** the one failing sensorium spec, and the `Tools/run-specs.sh` gap above.
Neither is fixable inside this packet's owner_path.
