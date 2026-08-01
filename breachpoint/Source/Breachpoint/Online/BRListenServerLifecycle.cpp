// Breachpoint. The slice's IBRServerLifecycle: one process, one host, no migration.

#include "Online/BRListenServerLifecycle.h"

#include "Core/BRCore.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CoreDelegates.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "BRListenServerLifecycle"

// =============================================================================================
// Hosting bring-up
// =============================================================================================

bool UBRListenServerLifecycle::InitializeHosting(UGameInstance* InGameInstance)
{
	if (!InGameInstance)
	{
		UE_LOG(LogBROnline, Error, TEXT("ListenLifecycle: InitializeHosting with no GameInstance; hosting refused."));
		return false;
	}

	// Idempotent: a second call from a re-host attempt is fine, a call after an ending is not.
	if (HostingState == EBRHostingState::Ending || HostingState == EBRHostingState::Ended)
	{
		UE_LOG(LogBROnline, Warning, TEXT("ListenLifecycle: InitializeHosting refused — hosting is already ending."));
		return false;
	}

	GameInstance = InGameInstance;

	if (HostingState == EBRHostingState::Uninitialized)
	{
		// Host-quit path 3. Best effort by construction: at OnEnginePreExit the net driver is on
		// its way out, so remotes usually learn of this as a ConnectionLost rather than a clean
		// kick. It is bound anyway because the ONE thing worse than a best-effort notice is no
		// notice: telemetry still gets its abandonment stamp out of OnHostingEnding.
		EnginePreExitHandle = FCoreDelegates::OnEnginePreExit.AddUObject(this, &UBRListenServerLifecycle::HandleEnginePreExit);

		SetHostingState(EBRHostingState::Initializing);
	}

	return true;
}

void UBRListenServerLifecycle::NotifyServerReadyForPlayers()
{
	if (HostingState == EBRHostingState::Hosting)
	{
		return; // idempotent
	}

	if (HostingState != EBRHostingState::Initializing)
	{
		UE_LOG(LogBROnline, Warning, TEXT("ListenLifecycle: NotifyServerReadyForPlayers in state %d ignored."),
			static_cast<int32>(HostingState));
		return;
	}

	SetHostingState(EBRHostingState::Hosting);

	// Diagnostic only, off by default (see the header). A listen server answers to no external
	// health authority; this exists so consumers of OnHostingHealthPulse are testable pre-GameLift.
	if (DiagnosticHealthPulseIntervalSeconds > 0.f && GameInstance.IsValid())
	{
		const float Interval = FMath::Clamp(DiagnosticHealthPulseIntervalSeconds, 1.f, 60.f);
		GameInstance->GetTimerManager().SetTimer(HealthPulseTimerHandle, this,
			&UBRListenServerLifecycle::HandleDiagnosticHealthPulse, Interval, /*bLoop=*/true);
	}

	UE_LOG(LogBROnline, Log, TEXT("ListenLifecycle: hosting is open to players."));
}

// =============================================================================================
// Seam gap 1 — per-player admission. Called on EVERY join, including join-in-progress.
// =============================================================================================

FBRJoinVerdict UBRListenServerLifecycle::ValidateJoin(const FBRJoinRequest& Request)
{
	// The ONE refusal a listen server makes, and it is not an admission policy — it is the
	// host-quit rule: a join that races the host's exit must be told so, not dropped into a
	// world that is two seconds from gone.
	if (!IsAcceptingPlayers())
	{
		UE_LOG(LogBROnline, Log, TEXT("ListenLifecycle: join rejected — hosting state is %d (ending/not open)."),
			static_cast<int32>(HostingState));

		return FBRJoinVerdict::Reject(
			LOCTEXT("JoinRejectedHostingEnding", "This match is no longer accepting players."),
			TEXT("hosting_not_open"));
	}

	// ACCEPT-ALWAYS is the correct listen-server policy, not a stub. There is no placement
	// service on this topology, so there is no credential to verify and nothing this hook could
	// check that the in-game validation does not already re-check (services law 2: a platform
	// datum grants no gameplay authority, so admission grants none either).
	//
	// The GameLift implementation replaces exactly this block with
	// AcceptPlayerSession(Request.Credential.Token) and returns Reject on a refused/expired/
	// replayed player-session id. No call site moves.
	//
	// Deliberately NOT logged: Request.Options and Request.Credential.Token (the token is a
	// secret) and never the platform id at Log verbosity (it is a player identifier).
	UE_LOG(LogBROnline, Log, TEXT("ListenLifecycle: join admitted (join_in_progress=%s, credential=%s)."),
		Request.bJoinInProgress ? TEXT("yes") : TEXT("no"),
		Request.Credential.IsEmpty() ? TEXT("none") : TEXT("present"));

	return FBRJoinVerdict::Accept();
}

void UBRListenServerLifecycle::NotifyPlayerJoined(const FString& PlatformIdString)
{
	if (!PlatformIdString.IsEmpty())
	{
		AdmittedPlayerIds.Add(PlatformIdString);
	}

	UE_LOG(LogBROnline, Log, TEXT("ListenLifecycle: player joined; admitted=%d."), AdmittedPlayerIds.Num());
}

void UBRListenServerLifecycle::NotifyPlayerLeft(const FString& PlatformIdString)
{
	AdmittedPlayerIds.Remove(PlatformIdString);

	UE_LOG(LogBROnline, Log, TEXT("ListenLifecycle: player left; admitted=%d."), AdmittedPlayerIds.Num());

	// NOTE, deliberately: an empty server does NOT end hosting here. On a listen topology the
	// host IS a player, so "everyone left" and "the host left" are the same event and the host
	// leaving is already path 1/3. GameLift's implementation is where an idle-server scale-down
	// belongs (it arrives as PlatformRequested), not here.
}

// =============================================================================================
// The ending funnel — every reason, one path.
// =============================================================================================

void UBRListenServerLifecycle::NotifyMatchComplete(const FBRMatchResultSummary& Summary)
{
	LastMatchSummary = Summary;
	LastMatchSummary.HumanPlayerCount = FMath::Max(Summary.HumanPlayerCount, AdmittedPlayerIds.Num());

	UE_LOG(LogBROnline, Log, TEXT("ListenLifecycle: match complete (winner=%d, %.1fs, %d humans)."),
		Summary.WinningTeamId, Summary.MatchDurationSeconds, LastMatchSummary.HumanPlayerCount);

	// The match frame says "my match is over"; only the lifecycle decides the server dies.
	RequestHostingEnd(EBRHostingEndReason::MatchComplete);
}

void UBRListenServerLifecycle::RequestHostingEnd(EBRHostingEndReason Reason)
{
	// (a) the latch. First reason wins — a host who quits during the post-match card produces
	// MatchComplete, not HostQuit, and telemetry must not see the match abandoned twice.
	if (bHostingEndLatched)
	{
		UE_LOG(LogBROnline, Verbose, TEXT("ListenLifecycle: RequestHostingEnd(%d) ignored — already ending as %d."),
			static_cast<int32>(Reason), static_cast<int32>(EndReason));
		return;
	}

	bHostingEndLatched = true;
	EndReason = Reason;

	if (GameInstance.IsValid())
	{
		GameInstance->GetTimerManager().ClearTimer(HealthPulseTimerHandle);
	}

	// (b) state first, event second — so an OnHostingEnding handler that asks GetHostingState()
	// always sees Ending, and IsAcceptingPlayers() is already false for a join racing the quit.
	SetHostingState(EBRHostingState::Ending);

	const float Grace = FMath::Clamp(HostingEndGraceSeconds, 0.f, 5.f);

	FBRHostingEndNotice Notice;
	Notice.Reason = Reason;
	Notice.GraceSeconds = Grace;
	Notice.RemoteMessage = MakeRemoteMessage(Reason);
	Notice.DiagnosticCode = MakeDiagnosticCode(Reason);

	UE_LOG(LogBROnline, Log, TEXT("ListenLifecycle: hosting ending (%s), grace %.2fs, %d admitted."),
		*Notice.DiagnosticCode, Grace, AdmittedPlayerIds.Num());

	// (c) synchronous, while the world still exists. Telemetry stamps the match outcome here —
	// this is the R16 abandonment datum, so it may not be deferred to a tick that never comes.
	HostingEndingEvent.Broadcast(Notice);

	// (d) the explained kick.
	ReturnRemotePlayersToMainMenu(Notice.RemoteMessage);

	// (e) the bounded grace window, on the GAME INSTANCE timer manager: a world timer would die
	// with the level we are about to leave.
	if (Grace > 0.f && GameInstance.IsValid())
	{
		GameInstance->GetTimerManager().SetTimer(GraceTimerHandle, this,
			&UBRListenServerLifecycle::CompleteHostingEnd, Grace, /*bLoop=*/false);
	}
	else
	{
		CompleteHostingEnd();
	}
}

void UBRListenServerLifecycle::ReturnRemotePlayersToMainMenu(const FText& ReasonText)
{
	// LISTEN-SERVER-ONLY CODE (marked, per online-services.md). It walks the local world's
	// controllers and distinguishes the host's own controller from remotes — precisely the
	// assumption no other file in the project is allowed to make.
	UWorld* World = GetHostWorld();
	if (!World)
	{
		UE_LOG(LogBROnline, Warning, TEXT("ListenLifecycle: no world at ending time; remotes fall back to ConnectionLost."));
		return;
	}

	int32 KickedCount = 0;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		if (Controller && !Controller->IsLocalPlayerController() && Controller->IsPrimaryPlayer())
		{
			// Reliable RPC. Note the engine's default implementation of this RPC DISCARDS the
			// text (APlayerController::ClientReturnToMainMenuWithTextReason_Implementation just
			// calls GameInstance->ReturnToMainMenu()). The remote therefore learns the CATEGORY
			// of the disconnect from UBRSessionsSubsystem's classifier, not from this text, until
			// ABRPlayerController forwards it — filed as a contract_gap by BP11.
			Controller->ClientReturnToMainMenuWithTextReason(ReasonText);
			++KickedCount;
		}
	}

	UE_LOG(LogBROnline, Log, TEXT("ListenLifecycle: returned %d remote player(s) to the main menu."), KickedCount);
}

void UBRListenServerLifecycle::CompleteHostingEnd()
{
	if (HostingState == EBRHostingState::Ended)
	{
		return;
	}

	AdmittedPlayerIds.Reset();

	// (f) UBRSessionsSubsystem observes this transition and does the platform work: destroy the
	// session, return the host to the front end. This class owns no OSS state (law 6).
	SetHostingState(EBRHostingState::Ended);
}

void UBRListenServerLifecycle::HandleEnginePreExit()
{
	// Path 3. Zero grace: the engine is already tearing down and a timer will never fire.
	// Everything that MUST happen (the telemetry stamp) happens inside the synchronous
	// OnHostingEnding broadcast; the kick RPCs are attempted and usually will not flush.
	HostingEndGraceSeconds = 0.f;
	RequestHostingEnd(EBRHostingEndReason::HostQuit);
}

void UBRListenServerLifecycle::HandleDiagnosticHealthPulse()
{
	const UWorld* World = GetHostWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	HostingHealthPulseEvent.Broadcast(Now);
}

// =============================================================================================
// Internals
// =============================================================================================

void UBRListenServerLifecycle::SetHostingState(EBRHostingState NewState)
{
	if (HostingState == NewState)
	{
		return;
	}

	const EBRHostingState OldState = HostingState;
	HostingState = NewState;

	UE_LOG(LogBROnline, Log, TEXT("ListenLifecycle: hosting state %d -> %d."),
		static_cast<int32>(OldState), static_cast<int32>(NewState));

	HostingStateChangedEvent.Broadcast(OldState, NewState);
}

FText UBRListenServerLifecycle::MakeRemoteMessage(EBRHostingEndReason Reason)
{
	// Every reason has a player-facing sentence. None returns empty: an empty string here is how
	// a remote ends up at a main menu with no idea what happened (services law 7).
	switch (Reason)
	{
	case EBRHostingEndReason::MatchComplete:
		return LOCTEXT("EndMatchComplete", "The match has ended.");
	case EBRHostingEndReason::HostQuit:
		return LOCTEXT("EndHostQuit", "The host left the game. The match has ended.");
	case EBRHostingEndReason::HostConnectionLost:
		return LOCTEXT("EndHostConnectionLost", "Connection to the host was lost. The match has ended.");
	case EBRHostingEndReason::PlatformRequested:
		return LOCTEXT("EndPlatformRequested", "This server is shutting down. The match has ended.");
	case EBRHostingEndReason::Fault:
		return LOCTEXT("EndFault", "The server encountered an error. The match has ended.");
	default:
		return LOCTEXT("EndUnknown", "The match has ended.");
	}
}

FString UBRListenServerLifecycle::MakeDiagnosticCode(EBRHostingEndReason Reason)
{
	switch (Reason)
	{
	case EBRHostingEndReason::MatchComplete:		return TEXT("match_complete");
	case EBRHostingEndReason::HostQuit:				return TEXT("host_quit");
	case EBRHostingEndReason::HostConnectionLost:	return TEXT("host_connection_lost");
	case EBRHostingEndReason::PlatformRequested:	return TEXT("platform_requested");
	case EBRHostingEndReason::Fault:				return TEXT("fault");
	default:										return TEXT("unknown");
	}
}

UWorld* UBRListenServerLifecycle::GetHostWorld() const
{
	return GameInstance.IsValid() ? GameInstance->GetWorld() : nullptr;
}

void UBRListenServerLifecycle::BeginDestroy()
{
	if (EnginePreExitHandle.IsValid())
	{
		FCoreDelegates::OnEnginePreExit.Remove(EnginePreExitHandle);
		EnginePreExitHandle.Reset();
	}

	if (GameInstance.IsValid())
	{
		GameInstance->GetTimerManager().ClearTimer(GraceTimerHandle);
		GameInstance->GetTimerManager().ClearTimer(HealthPulseTimerHandle);
	}

	HostingEndingEvent.Clear();
	HostingStateChangedEvent.Clear();
	HostingHealthPulseEvent.Clear();

	Super::BeginDestroy();
}

#undef LOCTEXT_NAMESPACE
