// Breachpoint. Match telemetry collection: events, not opinions. Authority-only, no PII, no tick.

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
	// Event ids. FNames, not GameplayTags: a telemetry metric is not gameplay state, and routing
	// these through the tag registry would make "add a metric" a Core/ edit in someone else's file.
	static const FName Event_Kill(TEXT("Kill"));
	static const FName Event_Join(TEXT("Join"));
	static const FName Event_Leave(TEXT("Leave"));
	static const FName Event_PhaseChanged(TEXT("PhaseChanged"));
	static const FName Event_HostingEnding(TEXT("HostingEnding"));

	static const FName Detail_SelfInflicted(TEXT("SelfInflicted"));
	static const FName Detail_FriendlyFire(TEXT("FriendlyFire"));
	static const FName Detail_JoinInProgress(TEXT("JoinInProgress"));
}

// =============================================================================================
// Subsystem lifetime
// =============================================================================================

bool UBRTelemetrySubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	// Game worlds only. An editor-preview or inactive world has no match to measure, and creating
	// one there would fold thumbnail scenes into a match record.
	const UWorld* const World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UBRTelemetrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Record.MatchId = FGuid::NewGuid();	// random per MATCH, so events correlate and players do not.

	// Join/leave are bound here (not at BeginPlay) because a player can be admitted before the
	// world finishes starting, and a missed join makes every later row for that player anonymous.
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
		// A client's subsystem exists but stays inert for its whole life. Constraint 1.
		return;
	}

	Record.MapName = FName(*InWorld.GetMapName());

	TryBindMatchSources();
	TryBindServerLifecycle();
}

void UBRTelemetrySubsystem::Deinitialize()
{
	// The world is going away. If nothing closed the record — a PIE stop, a travel, a crash-free
	// but lifecycle-less session — close it now rather than losing the match entirely. Idempotent.
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

// =============================================================================================
// Binding — every source is a delegate the game already fires. Nothing here polls.
// =============================================================================================

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
		// Not an error: a non-Breachpoint map (the front end) has neither, and telemetry simply
		// never starts there. Retried from the next safe point rather than asserted.
		return;
	}

	// The GameMode advertises OnPlayerKilled as "server-side telemetry/spec hook" (BP04). This is
	// the consumer it was cut for — attribution stays entirely in the match frame's hands.
	PlayerKilledHandle = GameMode->OnPlayerKilled.AddUObject(this, &UBRTelemetrySubsystem::HandlePlayerKilled);
	PhaseChangedHandle = GameState->OnMatchPhaseChanged.AddUObject(this, &UBRTelemetrySubsystem::HandleMatchPhaseChanged);
	KillFeedHandle = GameState->OnKillFeedEntryAdded.AddUObject(this, &UBRTelemetrySubsystem::HandleKillFeedEntryAdded);

	bMatchSourcesBound = true;

	UE_LOG(LogBROnline, Log, TEXT("Telemetry: match sources bound (match %s)."), *Record.MatchId.ToString(EGuidFormats::DigitsWithHyphens));
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

	// NOTE the direction of the dependency: telemetry consumes the SEAM (`IBRServerLifecycle`),
	// never the listen implementation. When GameLift replaces the implementation, `PlatformEnded`
	// starts appearing in Outcome and not one line of this file changes.
	TScriptInterface<IBRServerLifecycle> Lifecycle = Sessions->GetServerLifecycle();
	if (Lifecycle == nullptr)
	{
		// The lifecycle is created when hosting starts. A remote client never has one — and never
		// needs one, because a remote has no authority and folds nothing.
		return;
	}

	BoundLifecycle = Lifecycle;
	HostingEndingHandle = BoundLifecycle->OnHostingEnding().AddUObject(this, &UBRTelemetrySubsystem::HandleHostingEnding);
	bLifecycleBound = true;

	UE_LOG(LogBROnline, Log, TEXT("Telemetry: bound to the server lifecycle's ending event."));
}

// =============================================================================================
// Ingestion
// =============================================================================================

void UBRTelemetrySubsystem::RecordEvent(const FBRTelemetryEvent& Event)
{
	if (!bTelemetryEnabled || bFinalized || !HasTelemetryAuthority())
	{
		return;
	}

	const int32 Capacity = FMath::Clamp(MaxRetainedEvents, 64, 65536);
	if (Record.Events.Num() >= Capacity)
	{
		// Bounded by construction (constraint 3). Dropping the OLDEST keeps the end of the match —
		// the part every consumer cares about — and `DroppedEventCount` makes the loss visible
		// instead of pretending the record is complete.
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

	// THE ONE PLACE A PLAYER BECOMES AN IDENTITY HERE, and it is a counter. No hash of a Steam id,
	// no persona name, nothing that survives the match. Constraint 2, made structural.
	const int32 NewKey = NextPlayerKey++;
	PlayerKeys.Add(Key, NewKey);
	return NewKey;
}

// =============================================================================================
// Sources
// =============================================================================================

void UBRTelemetrySubsystem::HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer)
{
	if (!GameMode || !NewPlayer || GameMode->GetWorld() != GetWorld() || !HasTelemetryAuthority())
	{
		return;
	}

	APlayerState* const PlayerState = NewPlayer->PlayerState;
	if (!PlayerState)
	{
		// LAW 3 / join-in-progress honesty: PlayerState is legitimately null for the first frames
		// after login. We do not spin, poll, or assume — this player is folded when their first
		// event arrives, and their row is created then.
		UE_LOG(LogBROnline, Verbose, TEXT("Telemetry: PostLogin with no PlayerState yet; row deferred."));
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
			// BP04's decided edge case: no instigator => the victim is charged. Telemetry records
			// the ruling, it does not re-derive it.
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

	// The killfeed is the only source that distinguishes friendly fire, so the counter is folded
	// here rather than duplicating BP04's team logic in this file.
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

	// The abandonment window: a host quit HERE is the R16 datum; a host quit on the post-match
	// card is not, and conflating the two would inflate the number that unlocks fleet spending.
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

		// Decided, but not yet closed: the record stays open through the post-match window so a
		// host who quits on the end card is still recorded as Completed, not Abandoned.
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

	// SYNCHRONOUS AND LOAD-BEARING. This runs inside the lifecycle's ending broadcast, while the
	// world still exists — the whole reason `OnHostingEnding` is an event with a grace window
	// rather than "the level just went away". Ruling R16 reads the Outcome this line writes.
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
	Event.Detail = FName(*Notice.DiagnosticCode);	// a stable code, never a player-facing sentence
	RecordEvent(Event);

	FinalizeRecord(Outcome);
}

// =============================================================================================
// Internals
// =============================================================================================

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

	// Team through IGenericTeamAgentInterface — the same seam ABRGameMode::ResolveTeamId uses,
	// spelled the same way. Telemetry never invents a second notion of "which side is this on".
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
		return;	// first close wins — a host quit during teardown must not overwrite Completed
	}

	bFinalized = true;
	Record.Outcome = Outcome;

	if (Record.MatchDurationSeconds <= 0.f && LiveStartServerTime > 0.f)
	{
		Record.MatchDurationSeconds = GetServerTime() - LiveStartServerTime;
	}

	UE_LOG(LogBROnline, Log,
		TEXT("Telemetry: match %s closed — outcome=%d, %.1fs, %d humans, %d bots, %d JIP, %d early leaves, %d events (%d dropped)."),
		*Record.MatchId.ToString(EGuidFormats::DigitsWithHyphens), static_cast<int32>(Record.Outcome),
		Record.MatchDurationSeconds, Record.HumanPlayerCount, Record.BotPlayerCount,
		Record.JoinInProgressCount, Record.EarlyDepartureCount, Record.Events.Num(), Record.DroppedEventCount);

	// The ONLY output. No file, no socket, no blocking work — a consumer that wants to ship this
	// somewhere subscribes and does it on its own terms (Spotter today, GL-5's writer later).
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

	// Constraint 1. A listen server passes here; its remote clients do not, and neither does a
	// dedicated server's client. There is deliberately no "unless testing" branch.
	return World->GetNetMode() != NM_Client;
}

float UBRTelemetrySubsystem::GetServerTime() const
{
	const UWorld* const World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.f;
}
