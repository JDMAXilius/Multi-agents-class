#include "Telemetry/BRTelemetrySubsystem.h"

#include "Core/BRCore.h"
#include "Match/BRGameMode.h"
#include "Match/BRGameState.h"
#include "Online/BRSessionsSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GenericTeamAgentInterface.h"

namespace BRTelemetry
{
	static const FName Event_Kill(TEXT("Kill"));
	static const FName Event_Join(TEXT("Join"));
	static const FName Event_Leave(TEXT("Leave"));
	static const FName Event_PhaseChanged(TEXT("PhaseChanged"));
	static const FName Event_HostingEnding(TEXT("HostingEnding"));

	static const FName Detail_SelfInflicted(TEXT("SelfInflicted"));
	static const FName Detail_FriendlyFire(TEXT("FriendlyFire"));
	static const FName Detail_JoinInProgress(TEXT("JoinInProgress"));
}

bool UBRTelemetrySubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	const UWorld* const World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UBRTelemetrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Record.MatchId = FGuid::NewGuid();

	PostLoginHandle = FGameModeEvents::OnGameModePostLoginEvent().AddWeakLambda(this,
		[this](AGameModeBase* GameMode, APlayerController* NewPlayer)
		{
			HandlePostLogin(GameMode, NewPlayer);
		});

	LogoutHandle = FGameModeEvents::OnGameModeLogoutEvent().AddWeakLambda(this,
		[this](AGameModeBase* GameMode, AController* Exiting)
		{
			HandleLogout(GameMode, Exiting);
		});
}

void UBRTelemetrySubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (!HasTelemetryAuthority())
	{
		return;
	}

	Record.MapName = FName(*InWorld.GetMapName());

	TryBindMatchSources();
	TryBindServerLifecycle();
}

void UBRTelemetrySubsystem::Deinitialize()
{
	if (!bFinalized && HasTelemetryAuthority())
	{
		FinalizeRecord(bMatchIsLive ? EBRMatchTelemetryOutcome::Fault : Record.Outcome);
	}

	if (PostLoginHandle.IsValid())	{ FGameModeEvents::OnGameModePostLoginEvent().Remove(PostLoginHandle); }
	if (LogoutHandle.IsValid())		{ FGameModeEvents::OnGameModeLogoutEvent().Remove(LogoutHandle); }

	if (const UWorld* const World = GetWorld())
	{
		if (ABRGameMode* const GameMode = World->GetAuthGameMode<ABRGameMode>())
		{
			if (PlayerKilledHandle.IsValid())
			{
				GameMode->OnPlayerKilled.Remove(PlayerKilledHandle);
			}
		}

		if (ABRGameState* const GameState = World->GetGameState<ABRGameState>())
		{
			if (PhaseChangedHandle.IsValid())	{ GameState->OnMatchPhaseChanged.Remove(PhaseChangedHandle); }
			if (KillFeedHandle.IsValid())		{ GameState->OnKillFeedEntryAdded.Remove(KillFeedHandle); }
		}
	}

	if (BoundLifecycle != nullptr && HostingEndingHandle.IsValid())
	{
		BoundLifecycle->OnHostingEnding().Remove(HostingEndingHandle);
	}
	BoundLifecycle = nullptr;

	Super::Deinitialize();
}

void UBRTelemetrySubsystem::TryBindMatchSources()
{
	if (bMatchSourcesBound || !HasTelemetryAuthority())
	{
		return;
	}

	UWorld* const World = GetWorld();
	if (!World)
	{
		return;
	}

	ABRGameMode* const GameMode = World->GetAuthGameMode<ABRGameMode>();
	ABRGameState* const GameState = World->GetGameState<ABRGameState>();
	if (!GameMode || !GameState)
	{
		return;
	}

	PlayerKilledHandle = GameMode->OnPlayerKilled.AddUObject(this, &UBRTelemetrySubsystem::HandlePlayerKilled);
	PhaseChangedHandle = GameState->OnMatchPhaseChanged.AddUObject(this, &UBRTelemetrySubsystem::HandleMatchPhaseChanged);
	KillFeedHandle = GameState->OnKillFeedEntryAdded.AddUObject(this, &UBRTelemetrySubsystem::HandleKillFeedEntryAdded);

	bMatchSourcesBound = true;
}

void UBRTelemetrySubsystem::TryBindServerLifecycle()
{
	if (bLifecycleBound || !HasTelemetryAuthority())
	{
		return;
	}

	const UWorld* const World = GetWorld();
	UGameInstance* const GameInstance = World ? World->GetGameInstance() : nullptr;
	UBRSessionsSubsystem* const Sessions = GameInstance ? GameInstance->GetSubsystem<UBRSessionsSubsystem>() : nullptr;
	if (!Sessions)
	{
		return;
	}

	TScriptInterface<IBRServerLifecycle> Lifecycle = Sessions->GetServerLifecycle();
	if (Lifecycle == nullptr)
	{
		return;
	}

	BoundLifecycle = Lifecycle;
	HostingEndingHandle = BoundLifecycle->OnHostingEnding().AddUObject(this, &UBRTelemetrySubsystem::HandleHostingEnding);
	bLifecycleBound = true;
}

void UBRTelemetrySubsystem::RecordEvent(const FBRTelemetryEvent& Event)
{
	if (!bTelemetryEnabled || bFinalized || !HasTelemetryAuthority())
	{
		return;
	}

	const int32 Capacity = FMath::Clamp(MaxRetainedEvents, 64, 65536);
	if (Record.Events.Num() >= Capacity)
	{
		Record.Events.RemoveAt(0);
		++Record.DroppedEventCount;
	}

	Record.Events.Add(Event);

	OnTelemetryEventRecorded.Broadcast(Event);
}

void UBRTelemetrySubsystem::RecordPlayerEvent(FName EventId, const APlayerState* Subject,
	const APlayerState* Object, float Value, FName Detail)
{
	if (!bTelemetryEnabled || bFinalized || !HasTelemetryAuthority())
	{
		return;
	}

	FBRTelemetryEvent Event;
	Event.EventId = EventId;
	Event.ServerTime = GetServerTime();
	Event.SubjectKey = GetPlayerKey(Subject);
	Event.ObjectKey = GetPlayerKey(Object);
	Event.Value = Value;
	Event.Detail = Detail;

	RecordEvent(Event);
}

int32 UBRTelemetrySubsystem::GetPlayerKey(const APlayerState* Player)
{
	if (!Player)
	{
		return 0;
	}

	const FObjectKey Key(Player);
	if (const int32* const Existing = PlayerKeys.Find(Key))
	{
		return *Existing;
	}

	const int32 NewKey = NextPlayerKey++;
	PlayerKeys.Add(Key, NewKey);
	return NewKey;
}

void UBRTelemetrySubsystem::HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer)
{
	if (!GameMode || !NewPlayer || GameMode->GetWorld() != GetWorld() || !HasTelemetryAuthority())
	{
		return;
	}

	APlayerState* const PlayerState = NewPlayer->PlayerState;
	if (!PlayerState)
	{
		return;
	}

	FBRPlayerMatchTelemetry* const Row = FindOrAddPlayerRow(PlayerState);
	if (Row && bMatchIsLive)
	{
		Row->bJoinedInProgress = true;
		++Record.JoinInProgressCount;
	}

	RecordPlayerEvent(BRTelemetry::Event_Join, PlayerState, nullptr, 1.f,
		bMatchIsLive ? BRTelemetry::Detail_JoinInProgress : NAME_None);
}

void UBRTelemetrySubsystem::HandleLogout(AGameModeBase* GameMode, AController* Exiting)
{
	if (!GameMode || GameMode->GetWorld() != GetWorld() || !HasTelemetryAuthority())
	{
		return;
	}

	const APlayerController* const PC = Cast<APlayerController>(Exiting);
	APlayerState* const PlayerState = PC ? PC->PlayerState : nullptr;
	if (!PlayerState)
	{
		return;
	}

	if (FBRPlayerMatchTelemetry* const Row = FindOrAddPlayerRow(PlayerState))
	{
		if (bMatchIsLive && !Row->bLeftEarly)
		{
			Row->bLeftEarly = true;
			++Record.EarlyDepartureCount;
		}
	}

	RecordPlayerEvent(BRTelemetry::Event_Leave, PlayerState, nullptr, 1.f, NAME_None);
}

void UBRTelemetrySubsystem::HandlePlayerKilled(APlayerState* Killer, APlayerState* Victim,
	const TArray<APlayerState*>& Assists)
{
	if (!HasTelemetryAuthority() || !Victim)
	{
		return;
	}

	if (FBRPlayerMatchTelemetry* const VictimRow = FindOrAddPlayerRow(Victim))
	{
		++VictimRow->Deaths;
		if (!Killer)
		{
			++VictimRow->SelfInflictedDeaths;
		}
	}

	if (Killer)
	{
		if (FBRPlayerMatchTelemetry* const KillerRow = FindOrAddPlayerRow(Killer))
		{
			++KillerRow->Kills;
		}
	}

	for (APlayerState* const Assist : Assists)
	{
		if (FBRPlayerMatchTelemetry* const AssistRow = FindOrAddPlayerRow(Assist))
		{
			++AssistRow->Assists;
		}
	}

	RecordPlayerEvent(BRTelemetry::Event_Kill, Killer, Victim, 1.f,
		Killer ? NAME_None : BRTelemetry::Detail_SelfInflicted);
}

void UBRTelemetrySubsystem::HandleKillFeedEntryAdded(const FBRKillFeedEntry& Entry)
{
	if (!HasTelemetryAuthority())
	{
		return;
	}

	if (Entry.bFriendlyFire && Entry.Killer)
	{
		if (FBRPlayerMatchTelemetry* const KillerRow = FindOrAddPlayerRow(Entry.Killer))
		{
			++KillerRow->FriendlyFireKills;
		}
		RecordPlayerEvent(BRTelemetry::Event_Kill, Entry.Killer, Entry.Victim, 1.f,
			BRTelemetry::Detail_FriendlyFire);
	}
}

void UBRTelemetrySubsystem::HandleMatchPhaseChanged(EBRMatchPhase OldPhase, EBRMatchPhase NewPhase)
{
	if (!HasTelemetryAuthority())
	{
		return;
	}

	const float Now = GetServerTime();

	if (NewPhase == EBRMatchPhase::WarmUp)
	{
		WarmupStartServerTime = Now;
	}
	else if (NewPhase == EBRMatchPhase::Live && LiveStartServerTime <= 0.f)
	{
		LiveStartServerTime = Now;
		if (WarmupStartServerTime > 0.f)
		{
			Record.WarmupDurationSeconds = Now - WarmupStartServerTime;
		}
	}

	bMatchIsLive = (NewPhase == EBRMatchPhase::Live || NewPhase == EBRMatchPhase::SuddenDeath);

	if (NewPhase == EBRMatchPhase::PostMatch)
	{
		if (LiveStartServerTime > 0.f)
		{
			Record.MatchDurationSeconds = Now - LiveStartServerTime;
		}

		if (const ABRGameState* const GameState = GetWorld() ? GetWorld()->GetGameState<ABRGameState>() : nullptr)
		{
			Record.WinningTeamId = GameState->GetWinningTeamId();
		}

		Record.Outcome = EBRMatchTelemetryOutcome::Completed;
	}

	FBRTelemetryEvent Event;
	Event.EventId = BRTelemetry::Event_PhaseChanged;
	Event.ServerTime = Now;
	Event.Value = static_cast<float>(static_cast<uint8>(NewPhase));
	Event.Detail = FName(*FString::FromInt(static_cast<int32>(OldPhase)));
	RecordEvent(Event);
}

void UBRTelemetrySubsystem::HandleHostingEnding(const FBRHostingEndNotice& Notice)
{
	if (!HasTelemetryAuthority())
	{
		return;
	}

	EBRMatchTelemetryOutcome Outcome = Record.Outcome;
	switch (Notice.Reason)
	{
	case EBRHostingEndReason::MatchComplete:
		Outcome = EBRMatchTelemetryOutcome::Completed;
		break;
	case EBRHostingEndReason::HostQuit:
		Outcome = bMatchIsLive ? EBRMatchTelemetryOutcome::AbandonedByHost : EBRMatchTelemetryOutcome::Completed;
		break;
	case EBRHostingEndReason::HostConnectionLost:
		Outcome = bMatchIsLive ? EBRMatchTelemetryOutcome::HostLost : EBRMatchTelemetryOutcome::Completed;
		break;
	case EBRHostingEndReason::PlatformRequested:
		Outcome = EBRMatchTelemetryOutcome::PlatformEnded;
		break;
	case EBRHostingEndReason::Fault:
	default:
		Outcome = EBRMatchTelemetryOutcome::Fault;
		break;
	}

	FBRTelemetryEvent Event;
	Event.EventId = BRTelemetry::Event_HostingEnding;
	Event.ServerTime = GetServerTime();
	Event.Detail = FName(*Notice.DiagnosticCode);
	RecordEvent(Event);

	FinalizeRecord(Outcome);
}

FBRPlayerMatchTelemetry* UBRTelemetrySubsystem::FindOrAddPlayerRow(const APlayerState* Player)
{
	if (!Player)
	{
		return nullptr;
	}

	const int32 Key = GetPlayerKey(Player);
	if (FBRPlayerMatchTelemetry* const Existing = FindPlayerRowByKey(Key))
	{
		return Existing;
	}

	FBRPlayerMatchTelemetry Row;
	Row.PlayerKey = Key;
	Row.bIsBot = Player->IsABot();

	APlayerState* const MutablePlayer = const_cast<APlayerState*>(Player);
	if (const IGenericTeamAgentInterface* const TeamAgent = Cast<IGenericTeamAgentInterface>(MutablePlayer))
	{
		Row.TeamId = TeamAgent->GetGenericTeamId().GetId();
	}
	else if (AController* const OwningController = MutablePlayer ? MutablePlayer->GetOwningController() : nullptr)
	{
		if (const IGenericTeamAgentInterface* const ControllerAgent = Cast<IGenericTeamAgentInterface>(OwningController))
		{
			Row.TeamId = ControllerAgent->GetGenericTeamId().GetId();
		}
	}

	if (Row.bIsBot)
	{
		++Record.BotPlayerCount;
	}
	else
	{
		++Record.HumanPlayerCount;
	}

	const int32 Index = Record.Players.Add(Row);
	return &Record.Players[Index];
}

FBRPlayerMatchTelemetry* UBRTelemetrySubsystem::FindPlayerRowByKey(int32 PlayerKey)
{
	return Record.Players.FindByPredicate(
		[PlayerKey](const FBRPlayerMatchTelemetry& Row) { return Row.PlayerKey == PlayerKey; });
}

void UBRTelemetrySubsystem::FinalizeRecord(EBRMatchTelemetryOutcome Outcome)
{
	if (bFinalized)
	{
		return;
	}

	bFinalized = true;
	Record.Outcome = Outcome;

	if (Record.MatchDurationSeconds <= 0.f && LiveStartServerTime > 0.f)
	{
		Record.MatchDurationSeconds = GetServerTime() - LiveStartServerTime;
	}

	OnMatchTelemetryFinalized.Broadcast(Record);
}

bool UBRTelemetrySubsystem::HasTelemetryAuthority() const
{
	if (!bTelemetryEnabled)
	{
		return false;
	}

	const UWorld* const World = GetWorld();
	if (!World)
	{
		return false;
	}

	return World->GetNetMode() != NM_Client;
}

float UBRTelemetrySubsystem::GetServerTime() const
{
	const UWorld* const World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.f;
}
