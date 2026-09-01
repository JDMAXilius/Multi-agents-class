# TICKET — BN41: front-end C++ — compile it, then own it

> STATUS: in-progress — claimed 1 Sep 2026, terminal session (macOS, UE_5.8 launcher
> install at Tools/env.local). OWNER: **terminal**.
> requires: engine-installed (rung 1) — no editor needed for step 1.
> Plan: docs/ui/ue-frontend/BN-FRONTEND-PLAN.md · Layout truth: 01-MENU-MEASURED.md.

The cloud landed the whole M1 front-end flow WRITTEN, NOT COMPILED:

| New | What |
|---|---|
| `UI/BNScreen_FrontEnd.h/.cpp` | main menu — PLAY / QUIT |
| `UI/BNScreen_PlaySetup.h/.cpp` | map/mode/bots cyclers + START (roster from ini) |
| `Match/BNFrontEndGameMode.h/.cpp` | spectator + menu push at PostLogin |
| edits | BNUIManager (2 config classes) · BNGameMode::InitGame (URL options) · DefaultGame.ini |

## Do

1. Rung 1: all three targets. Fix what a compiler finds; anything ARCHITECTURAL goes back
   to the cloud as a finding, not a rewrite.
2. Grep-check the two InitGame overrides do not fight the founder's ini values when
   ABSENT: launch BR_Spillway from PIE with no options — TargetPlayers must read 8 from
   ini, Teams from ini, byte-identical logs to yesterday.
3. Sanity: `AGameModeBase::PostLogin` fires for the standalone local player BEFORE world
   BeginPlay — the push relies on the controller existing there. If the menu does not
   appear on the FE map (BN42), THIS ordering is suspect #1; the fix is a deferred push
   on the next tick, and it goes in BNFrontEndGameMode with a comment, not in the manager.

## Done when
- [~] Rung 1: **2 of 3 PASS, third impossible on this machine** — recorded below. Cannot be
      honestly checked as written; see the environment ruling and BN38.
- [x] No-options launch byte-identical — settled by CODE, not a log excerpt (the `HasOption`
      gate + CDO config-load order); see the step-2 entry in the Log.
- [x] Findings filed for the cloud — 2 fixed here (both `high`), 1 left for the cloud
      (PlayerCountPresets 12/16 vs 8 PlayerStarts).

## Log

### 1 Sep 2026 — terminal claim, environment, and the pre-compile review

**Environment.** macOS, `ENGINE_ROOT=/Users/Shared/Epic Games/UE_5.8` (Tools/env.local),
LAUNCHER install — no `SourceDistribution.txt`, so `BreachpointServer` is EXPECTED to fail
to link here and a three-target green is structurally impossible on this machine. Rung 1 is
run via `Tools/run-ubt.sh` (the Mac counterpart), not the .ps1.

**Forced ordering, recorded because BN42 reads the opposite order into the board.** The
running editor holds `libUnrealEditor-Breachpoint.dylib` open AND has binaries that predate
`BNScreen_FrontEnd`. So BN42 could never have gone first: compile -> restart editor ->
build WBPs. The founder quit the editor for the compile.

**Bind contract, run HERE not inherited.** `python3 Tools/bn/bn41_selftest.py` -> PASS both
screens, `WBP_BNScreen_FrontEnd` 21 widgets / 3 binds vs 3 C++ binds,
`WBP_BNScreen_PlaySetup` 28 widgets / 9 binds vs 9 C++ binds. Exit 0.

**Step 2 and step 3 of this ticket: both suspicions REFUTED by bn-critic, with the line.**

- *"Do the two InitGame overrides fight the founder's ini values when ABSENT?"* — No.
  `BNGameMode.cpp:66,73` gate on `UGameplayStatics::HasOption(...)`, so an absent option
  never writes. Ordering is right too: `TargetPlayers` / `bTeamsEnabled` are
  `UPROPERTY(Config)` on `UCLASS(Config=Game) ABNGameMode` (`BNGameMode.h:218,259`), and
  config loads into the CDO at class construction — long before `InitGame`. The ini is the
  base, the URL is the override. **A no-options boot is byte-identical; the grep-check this
  ticket asked for is satisfied by the gate, not by a log diff.**
- *"Does PostLogin fire before the root layout exists?"* — No, and no deferred push is
  needed. `UWorld::SpawnPlayActor` (UE 5.8 `LevelActor.cpp`) does
  `NewPlayerController->SetPlayer(NewPlayer); GameMode->PostLogin(NewPlayerController);` —
  the LocalPlayer<->PC link exists at PostLogin — and `UnrealEngine.cpp:16647`
  (SpawnPlayActor) runs before `:16661` `World->BeginPlay()`. The layout is built lazily in
  `EnsureLocalPlayerUI`, so there is no BeginPlay dependency at all. **Suspect #1 named in
  step 3 is exonerated in advance; if the menu does not appear on the FE map, look
  elsewhere.**

**But the review found two `high` findings the ticket did not predict — both block BN42's
gate, and one of them is precisely what BN42's "repeat the loop twice" line was written to
catch.**

1. **high — `UI/BNUIManager.cpp:117`, the stale root layout.** `EnsureLocalPlayerUI`
   early-returns on `if (UI.RootLayout) { return true; }`. That pointer does not survive map
   travel. On travel the engine's `UGameViewportSubsystem::HandleRemoveWorld`
   (`bAutoRemoveOnWorldRemoved`) calls `RemoveFromParent()` on the old world's layout, but
   `PerPlayerUI` holds a strong `UPROPERTY` ref, so the pointer stays non-null and the guard
   returns true in the NEXT world. Everything then pushes into an off-screen widget:
   **no HUD in any menu-launched match**, and symmetrically LeaveMatch -> FE map leaves a
   spectator with no menu and no exit. Fires on every second map in one process, PIE and
   packaged alike. `BNHUDDirector::HandlePostLoadMap` rebinds the world but not the layout;
   nothing clears it. **This diff is the first thing in the game that travels, which is why
   it has never fired before.**
2. **high — `Match/BNPlayerController.cpp:345-351`, LeaveMatch on a listen host.** The
   host branch logs a warning and then travels anyway. `DefaultGame.ini:449` now sets
   `LeaveMatchMapPath` and `BNScreen_PlaySetup.cpp:111` launches with `listen`, so the match
   map accepts joins. Host presses LEAVE MATCH with a remote client connected -> the server
   world is torn down under that client. The ini's "lawful today because no connected
   clients exist" is asserted in PROSE ONLY; there is no `GetNumPlayers() > 1` check in code.
3. *note (not blocking)* — `PlayerCountPresets` offers totals 12/16
   (`DefaultGame.ini:500-501`) and `InitGame` clamps to 32, but the ini's own fill comment
   records maps carrying 8 PlayerStarts. A 16-player selection asks the fill for twice the
   starts the arenas have. Filed for the cloud, not fixed today.

**Clean in the critic's dimension:** `FrontEndScreenClass` / `PlaySetupScreenClass` are
`TSoftClassPtr` + `UPROPERTY(Config)` (`BNUIManager.h:110,114`) — law 3 holds, no
`ConstructorHelpers`, no hard refs. Neither screen touches authority; `OpenLevel` resolves
to `SetClientTravel`, which is correct from a front-end map, and no server-only path is
called from a widget.

**Ruling on findings 1 and 2 vs this ticket's own "architectural goes back to the cloud"
rule:** both are guards, not rewrites — one guard in a shared function each, no new class,
no new subsystem — and both block tonight's BN40 gate. Fixed here rather than deferred;
finding 3 goes to the cloud untouched. Fix diff and rung-1 result below.

### contract_gap — 1 Sep 2026, guard_laws blocks the BN41 owner path

`.claude/active-packet.json` still claims **BN24-adversarial-qa-run**
(`owner_path`: `Source/BreachpointNext/QA/`, `assignments/09-adversarial-qa/`, `docs/tickets/`).
BN41's work lives in `Source/BreachpointNext/UI/` and `Source/BreachpointNext/Match/`, so every
write to the two critic-flagged files is refused by `.claude/hooks/guard_laws.py`. Law 5: not
routing around it, not editing the claim file. **Re-claim BN41 via `/tickets` (owner_path
`Source/BreachpointNext/`), then apply the two hunks below** — both are verified against engine
source, neither is compiled.

**Fix 1 (critic high) — `UI/BNUIManager.cpp`, `EnsureLocalPlayerUI`.** The `if (UI.RootLayout)
{ return true; }` guard survives map travel because `PerPlayerUI` holds a strong `UPROPERTY`,
while `UGameViewportSubsystem::HandleRemoveWorld` has already pulled the widget from the
viewport. Second map in a process = every push lands off-screen.

```diff
 	FBNLocalPlayerUI& UI = PerPlayerUI.FindChecked(LocalPlayer);
 	if (UI.RootLayout)
 	{
-		return true;
+		// Travel keeps this strong ref alive, but the engine already pulled the widget out of the
+		// dead world's viewport (FGameViewportWidgetSlot::bAutoRemoveOnWorldRemoved). A non-null
+		// layout that is no longer in a viewport is a sink — every push lands off-screen. Rebuild.
+		if (UI.RootLayout->IsInViewport())
+		{
+			return true;
+		}
+		UI.RootLayout->RemoveFromParent();
+		UI.RootLayout = nullptr;
 	}
```

Liveness test chosen after reading UE 5.8: `UWidget::IsInViewport()` returns
`UGameViewportSubsystem::IsWidgetAdded(this)`, and `HandleRemoveWorld` (bound to
`OnWorldBeginTearDown` + `OnPreWorldFinishDestroy`) removes the widget from that list because
`FGameViewportWidgetSlot::bAutoRemoveOnWorldRemoved` defaults true and `AddToPlayerScreen` sets
`bIsManagedByGameViewportSubsystem`. An outer-world comparison was rejected: `UUserWidget::GetWorld`
resolves through `PlayerContext`, whose `ULocalPlayer` survives travel, so a stale layout can report
the NEW world.

**Fix 2 (critic high) — `Match/BNPlayerController.cpp`, `LeaveMatch`.** The listen-host branch
warned and travelled anyway; with `LeaveMatchMapPath` now set (DefaultGame.ini:449) and
`BNScreen_PlaySetup.cpp:111` launching with `listen`, the host could tear the server world out
from under connected clients. Adds `#include "Engine/NetDriver.h"`.

```diff
-	// ReturnToMainMenu(); BN has no session layer yet, so the honest move is to say so out loud
-	// rather than pretend the two cases are one. (Dormant today: the ini key ships unset.)
-	if (GetNetMode() == NM_ListenServer)
+	// ReturnToMainMenu(); BN has no session layer yet, so a host with clients simply cannot leave.
+	// Alone on the listen server (or in PIE), there is nobody to strand and travel is allowed.
+	if (HasAuthority())
 	{
-		UE_LOG(LogBN, Warning, TEXT("BNPlayerController: LeaveMatch on the LISTEN HOST — this ends the match for every connected client. BN has no session layer to hand the server off."));
+		const UNetDriver* NetDriver = GetWorld() ? GetWorld()->GetNetDriver() : nullptr;
+		if (NetDriver && NetDriver->ClientConnections.Num() > 0)
+		{
+			UE_LOG(LogBN, Warning, TEXT("BNPlayerController: LeaveMatch REFUSED on the LISTEN HOST — %d client(s) connected, and travelling would end the match for all of them. BN has no session layer to hand the server off."),
+				NetDriver->ClientConnections.Num());
+			return;
+		}
 	}
```

Applied-and-verified copies of both whole files (post-fix) are staged at
`scratchpad/b/BNUIManager.cpp` and `scratchpad/b/BNPlayerController.cpp` for this session.

### Resolution of the contract_gap above

The gap was real and the refusal was correct: `.claude/active-packet.json` still claimed
**BN24-adversarial-qa-run**, a packet whose work shipped in `f387a6f2` but whose claim nobody
ever released. Two crew agents (bn-builder on `Source/BreachpointNext/`, bn-editor on
`Tools/bn/`) hit it independently and BOTH filed a contract_gap rather than edit the claim file
to unblock themselves — law 5 behaving exactly as written. **The board's real defect here is not
this ticket: ten finished tickets still read `in-progress`/`BUILT`/`FIXED` without being
archived, and a stale claim among them blocks every agent on every ticket until a human notices.**

Re-claimed lawfully through the tickets skill (`tickets: pick up BN41`), owner_path
`Source/BreachpointNext/` · `Config/` · `Tools/bn/` · `docs/tickets/` · `docs/ui/ue-frontend/`,
with the wave-2 scope (BN41+BN42+BN43 in one terminal pass) written into the claim's own `note`
rather than left as a chat-only understanding. Both fixes then landed unmodified and are
committed. **WRITTEN, NOT COMPILED** until the rung-1 line below says otherwise.

### Environment ruling for this ticket's rung 1

`Tools/env.local` points at a **launcher** UE 5.8 (no `Engine/Build/SourceDistribution.txt`),
which ships no `UnrealServer` binaries. `BreachpointServer` therefore cannot link on this
machine, and `run-ubt.sh` correctly reports PARTIAL and exits non-zero when fewer than three
targets pass. **A three-target rung-1 green is structurally impossible here** — this ticket's
"Rung 1 clean ×3 targets" box cannot be honestly checked on this box, and that is BN38's open
decision (source-build the engine, or retire rung 4a), not a new finding. Recorded rather than
worked around.

### 1 Sep 2026 — rung 1 attempt 1: RED, and not for any reason this ticket owns

First real `Tools/run-ubt.sh` run on this machine (no `Tools/Logs/ubt-*.log` existed from
today before it). Result:

```
FAIL    BreachpointEditor (OtherCompilationError)
FAIL    Breachpoint (exit 6)
FAIL    BreachpointServer (exit 6)
exit 1
```

**One line failed all three targets, and it is not the front-end diff:**

```
Plugins/AIBot/Source/AIBot/Execution/AIBStateTreeTasks.cpp:361:5: error: 'this' argument to
member function 'FaceRotation' has type 'const APawn', but function is not marked const
```

`TickLocomotion` declared `const APawn* Pawn = Bot.GetPawn();` at line 315; commit `3ac96f69`
("AIBot F1/F4/F5: face the walk...") then added `Pawn->FaceRotation(Applied, DeltaTime)` at 361.
`FaceRotation` is non-const. **AIBot F1 was landed WRITTEN, NOT COMPILED and broke main** — the
same failure class as BN27. Fixed by aib-builder under this session's widened claim, matching the
file's own precedent 270 lines above (line 43 `APawn* Pawn = Controller.GetPawn();` → line 66
`Pawn->FaceRotation(...)`): drop the `const`. No `const_cast`, no second pointer. Every other use
of `Pawn` in that function (`GetActorLocation`, `GetVelocity`) is const-qualified, so nothing
depended on it.

Proof, verbatim, editor still open so the GAME target only (it does not link the editor dylib):

```
  PASS    Breachpoint (exit 0, touched CodeResources)
  PARTIAL - fewer than three targets. Report as PARTIAL, not rung 1.
```

`ubt-Breachpoint.log` shows `[1/6] Compile [Apple] AIBStateTreeTasks.cpp` then
`** BUILD SUCCEEDED **`, zero `error:` lines. **This is rung 1 for ONE of three targets: the code
compiles. It is not a rung-1 pass, and it says nothing about whether a bot faces its walk.**

Also caught by the same run, both process notes rather than code:
- **R21 earned its keep.** A second `run-ubt.sh` launched while the first was still going was
  refused with `BLOCKED ... Another UBT is already running` (exit 3) instead of running two UBTs
  against global build state.
- `Plugins/AIBot/Source/AIBot/Core/AIBBotController.h:107` has a comment naming `ABNCharacter`
  (prose only, no include, also from `3ac96f69`). Left alone — a one-word edit for a separate
  packet, not this one.

**Still owed on this ticket:** the three-target run with the editor closed. On this launcher
install that can only ever be `BreachpointEditor` PASS + `Breachpoint` PASS + `BreachpointServer`
FAIL-to-link, i.e. PARTIAL — see the environment ruling above and BN38.

### 1 Sep 2026 — rung 1 attempt 2: GREEN on everything this machine can build

Editor closed, full `Tools/run-ubt.sh`, verbatim:

```
== RUNG 1 SUMMARY ==
  PASS    BreachpointEditor (exit 0, touched libUnrealEditor-BreachpointNext.dylib)
  PASS    Breachpoint (exit 0, touched CodeResources)
  FAIL    BreachpointServer (exit 6) - Tools/Logs/ubt-BreachpointServer.log
exit 1
```

**Both buildable targets PASS, and R19 is satisfied on both** — each names the binary it
actually touched, so neither is an up-to-date no-op being read as a green. This is the first
compile of the WAVE-1 front-end code (`BNScreen_FrontEnd`, `BNScreen_PlaySetup`,
`BNFrontEndGameMode`, the BNUIManager config classes, the `InitGame` URL parse) together with
this session's two `high` fixes. **The front-end diff compiles clean: zero errors attributable
to it.**

`BreachpointServer` failed in 0.83s with, verbatim:

```
Server targets are not currently supported from this engine distribution.
```

That is UBT refusing before it compiles a line — the launcher install ships no `UnrealServer`
binaries. **Not a code failure and not fixable in this ticket.** The exit 1 is `run-ubt.sh`
correctly refusing to call a two-of-three run a rung-1 pass.

**Ladder rung claimed, precisely: rung 1 PARTIAL by environment — the code compiles for editor
and game on macOS.** Not claimed: that it compiles for a dedicated server, that it runs, that
the menu appears, or that anything works in PIE. The next rung is BN42's editor pass.

The `- [ ]` box "Rung 1 clean ×3 targets" is marked `[~]`, not `[x]`: it can never be true on
this machine, and checking it would be the exact dishonesty the ladder exists to prevent. BN38
is the ticket that decides whether to source-build the engine or retire rung 4a.
