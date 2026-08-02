// Breachpoint. The slice's IBRServerLifecycle: one process, one host, no migration.
#include "Online/BRListenServerLifecycle.h"

#include "Core/BRCore.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CoreDelegates.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "BRListenServerLifecycle"

bool UBRListenServerLifecycle::InitializeHosting(UGameInstance* InGameInstance)
{
	if (!InGameInstance)
	{
		UE_LOG(LogBROnline, Error, TEXT("ListenLifecycle: InitializeHosting with no GameInstance; hosting refused."));
		return false;
	}

	if (HostingState == EBRHostingState::Ending || HostingState == EBRHostingState::Ended)
	{
		UE_LOG(LogBROnline, Warning, TEXT("ListenLifecycle: InitializeHosting refused — hosting is already ending."));
		return false;
	}

	GameInstance = InGameInstance;

	if (HostingState == EBRHostingState::Uninitialized)
	{
		EnginePreExitHandle = FCoreDelegates::OnEnginePreExit.AddUObject(this, &UBRListenServerLifecycle::HandleEnginePreExit);

		SetHostingState(EBRHostingState::Initializing);
	}

	return true;
}

void UBRListenServerLifecycle::NotifyServerReadyForPlayers()
{
	if (HostingState == EBRHostingState::Hosting)
	{
		return;
	}

	if (HostingState != EBRHostingState::Initializing)
	{
		UE_LOG(LogBROnline, Warning, TEXT("ListenLifecycle: NotifyServerReadyForPlayers in state %d ignored."),
			static_cast<int32>(HostingState));
		return;
	}

	SetHostingState(EBRHostingState::Hosting);

	if (DiagnosticHealthPulseIntervalSeconds > 0.f && GameInstance.IsValid())
	{
		const float Interval = FMath::Clamp(DiagnosticHealthPulseIntervalSeconds, 1.f, 60.f);
		GameInstance->GetTimerManager().SetTimer(HealthPulseTimerHandle, this,
			&UBRListenServerLifecycle::HandleDiagnosticHealthPulse, Interval, true);
	}
}

FBRJoinVerdict UBRListenServerLifecycle::ValidateJoin(const FBRJoinRequest& Request)
{
	if (!IsAcceptingPlayers())
	{
		return FBRJoinVerdict::Reject(
			LOCTEXT("JoinRejectedHostingEnding", "This match is no longer accepting players."),
			TEXT("hosting_not_open"));
	}

	return FBRJoinVerdict::Accept();
}

void UBRListenServerLifecycle::NotifyPlayerJoined(const FString& PlatformIdString)
{
	if (!PlatformIdString.IsEmpty())
	{
		AdmittedPlayerIds.Add(PlatformIdString);
	}
}

void UBRListenServerLifecycle::NotifyPlayerLeft(const FString& PlatformIdString)
{
	AdmittedPlayerIds.Remove(PlatformIdString);
}

void UBRListenServerLifecycle::NotifyMatchComplete(const FBRMatchResultSummary& Summary)
{
	LastMatchSummary = Summary;
	LastMatchSummary.HumanPlayerCount = FMath::Max(Summary.HumanPlayerCount, AdmittedPlayerIds.Num());

	RequestHostingEnd(EBRHostingEndReason::MatchComplete);
}

void UBRListenServerLifecycle::RequestHostingEnd(EBRHostingEndReason Reason)
{
	if (bHostingEndLatched)
	{
		return;
	}

	bHostingEndLatched = true;
	EndReason = Reason;

	if (GameInstance.IsValid())
	{
		GameInstance->GetTimerManager().ClearTimer(HealthPulseTimerHandle);
	}

	SetHostingState(EBRHostingState::Ending);

	const float Grace = FMath::Clamp(HostingEndGraceSeconds, 0.f, 5.f);

	FBRHostingEndNotice Notice;
	Notice.Reason = Reason;
	Notice.GraceSeconds = Grace;
	Notice.RemoteMessage = MakeRemoteMessage(Reason);
	Notice.DiagnosticCode = MakeDiagnosticCode(Reason);

	HostingEndingEvent.Broadcast(Notice);

	ReturnRemotePlayersToMainMenu(Notice.RemoteMessage);

	if (Grace > 0.f && GameInstance.IsValid())
	{
		GameInstance->GetTimerManager().SetTimer(GraceTimerHandle, this,
			&UBRListenServerLifecycle::CompleteHostingEnd, Grace, false);
	}
	else
	{
		CompleteHostingEnd();
	}
}

void UBRListenServerLifecycle::ReturnRemotePlayersToMainMenu(const FText& ReasonText)
{
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
			Controller->ClientReturnToMainMenuWithTextReason(ReasonText);
			++KickedCount;
		}
	}
}

void UBRListenServerLifecycle::CompleteHostingEnd()
{
	if (HostingState == EBRHostingState::Ended)
	{
		return;
	}

	AdmittedPlayerIds.Reset();

	SetHostingState(EBRHostingState::Ended);
}

void UBRListenServerLifecycle::HandleEnginePreExit()
{
	HostingEndGraceSeconds = 0.f;
	RequestHostingEnd(EBRHostingEndReason::HostQuit);
}

void UBRListenServerLifecycle::HandleDiagnosticHealthPulse()
{
	const UWorld* World = GetHostWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	HostingHealthPulseEvent.Broadcast(Now);
}

void UBRListenServerLifecycle::SetHostingState(EBRHostingState NewState)
{
	if (HostingState == NewState)
	{
		return;
	}

	const EBRHostingState OldState = HostingState;
	HostingState = NewState;

	HostingStateChangedEvent.Broadcast(OldState, NewState);
}

FText UBRListenServerLifecycle::MakeRemoteMessage(EBRHostingEndReason Reason)
{
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
