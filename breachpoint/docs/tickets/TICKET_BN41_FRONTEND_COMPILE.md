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
- [ ] Rung 1 clean ×3 targets, recorded here
- [ ] No-options launch byte-identical (log excerpt here)
- [ ] Findings (if any) filed for the cloud

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
