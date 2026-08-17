#include "Telemetry/BRSpotterSubsystem.h"

#include "Data/BRDataRows.h"
#include "Match/BRGameState.h"
#include "Telemetry/BRTelemetrySubsystem.h"

#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"

DEFINE_LOG_CATEGORY_STATIC(LogBRSpotter, Log, All);

namespace BRSpotter
{
	static const FName Trigger_MatchEndWin(TEXT("Match.End.Win"));
	static const FName Trigger_MatchEndLoss(TEXT("Match.End.Loss"));
}

bool UBRSpotterSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	const UWorld* const World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UBRSpotterSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (!HasSpotterAuthority())
	{
		return;
	}

	TryBindMatchSources();
}

void UBRSpotterSubsystem::Deinitialize()
{
	if (const UWorld* const World = GetWorld())
	{
		if (ABRGameState* const GameState = World->GetGameState<ABRGameState>())
		{
			if (KillFeedHandle.IsValid())
			{
				GameState->OnKillFeedEntryAdded.Remove(KillFeedHandle);
			}
		}
	}

	if (UBRTelemetrySubsystem* const Telemetry = BoundTelemetry.Get())
	{
		if (TelemetryFinalizedHandle.IsValid())
		{
			Telemetry->OnMatchTelemetryFinalized.Remove(TelemetryFinalizedHandle);
		}
	}
	BoundTelemetry = nullptr;

	LoadedLinesTable = nullptr;

	Super::Deinitialize();
}

void UBRSpotterSubsystem::RequestEventLine(FName TriggerId, APlayerState* Subject)
{
	if (!HasSpotterAuthority() || TriggerId.IsNone())
	{
		return;
	}

	ResolveAndDispatch(TriggerId, Subject);
}

bool UBRSpotterSubsystem::HasSpotterAuthority() const
{
	const UWorld* const World = GetWorld();
	return World && World->GetNetMode() != NM_Client;
}

void UBRSpotterSubsystem::TryBindMatchSources()
{
	if (bMatchSourcesBound || !HasSpotterAuthority())
	{
		return;
	}

	UWorld* const World = GetWorld();
	ABRGameState* const GameState = World ? World->GetGameState<ABRGameState>() : nullptr;
	UBRTelemetrySubsystem* const Telemetry = World ? World->GetSubsystem<UBRTelemetrySubsystem>() : nullptr;
	if (!GameState || !Telemetry)
	{
		return;
	}

	KillFeedHandle = GameState->OnKillFeedEntryAdded.AddUObject(this, &UBRSpotterSubsystem::HandleKillFeedEntryAdded);
	TelemetryFinalizedHandle = Telemetry->OnMatchTelemetryFinalized.AddUObject(this, &UBRSpotterSubsystem::HandleMatchTelemetryFinalized);
	BoundTelemetry = Telemetry;

	bMatchSourcesBound = true;
}

void UBRSpotterSubsystem::HandleKillFeedEntryAdded(const FBRKillFeedEntry& Entry)
{
	if (!HasSpotterAuthority())
	{
		return;
	}

	// TODO(spotter-notable-events): classifying a kill as "notable" -- a killing-spree streak, a
	// multi-kill from one rocket, a grapple finish -- needs streak/grouping state that belongs to
	// the medal system (see the TriggerIds in DT_Medals.csv: Kill.Spree, Kill.Rocket.Multi,
	// Kill.Grapple), which is not built in C++ yet (FBRMedalRow in Data/BRDataRows.h has no
	// evaluator). A single `FBRKillFeedEntry` -- one kill, one cause tag, no history -- is not
	// enough to derive that classification without inventing a mapping this codebase has not
	// authored. Once the medal system exists, have it call `RequestEventLine` with the TriggerId
	// it already computed; this handler stays only as the documented wiring point.
	(void)Entry;
}

void UBRSpotterSubsystem::HandleMatchTelemetryFinalized(const FBRMatchTelemetryRecord& Record)
{
	if (!HasSpotterAuthority())
	{
		return;
	}

	UBRTelemetrySubsystem* const Telemetry = BoundTelemetry.Get();
	const AGameStateBase* const GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!Telemetry || !GameState)
	{
		return;
	}

	for (APlayerState* const PlayerState : GameState->PlayerArray)
	{
		if (!PlayerState || PlayerState->IsABot())
		{
			continue;
		}

		// Same object identity Telemetry already keyed during the match -> same key back out.
		const int32 PlayerKey = Telemetry->GetPlayerKey(PlayerState);
		const FBRPlayerMatchTelemetry* const Stats = Record.Players.FindByPredicate(
			[PlayerKey](const FBRPlayerMatchTelemetry& Row) { return Row.PlayerKey == PlayerKey; });
		if (!Stats)
		{
			continue;
		}

		EmitCoachLine(PlayerState, *Stats, Record.WinningTeamId);
	}
}

void UBRSpotterSubsystem::EmitCoachLine(APlayerState* Subject, const FBRPlayerMatchTelemetry& Stats, uint8 WinningTeamId)
{
	const bool bWon = (WinningTeamId != BRMatch::InvalidTeamId) && (Stats.TeamId == WinningTeamId);

	// The real coach line is the personalized, stat-specific sentence the GDD calls for ("You
	// lost 6 fights below 40% shields..."); that needs the model path below or an authored
	// stat-template table this project has not built. DT_SpotterLines only carries generic
	// win/loss flavor, so the fallback below is deliberately generic -- exactly the "canned coach
	// lines only" §5.3's cut order already accepts.
	ResolveAndDispatch(bWon ? BRSpotter::Trigger_MatchEndWin : BRSpotter::Trigger_MatchEndLoss, Subject);
}

void UBRSpotterSubsystem::ResolveAndDispatch(FName TriggerId, APlayerState* Subject)
{
	if (bEnableModelSpotter && ModelCallsUsedThisMatch < FMath::Max(MaxModelCallsPerMatch, 0))
	{
		++ModelCallsUsedThisMatch;

		FText ModelLine;
		if (TryRequestModelLine(TriggerId, Subject, ModelLine))
		{
			BroadcastLine(Subject, EBRSpotterAudience::Self, ModelLine);
			return;
		}
	}

	FName RowName;
	FBRSpotterLineRow Row;
	if (TrySelectCannedLine(TriggerId, RowName, Row))
	{
		LineLastUsedServerTime.Add(RowName, GetServerTime());
		BroadcastLine(Subject, Row.Audience, Row.Text);
	}

	// No candidate (unauthored trigger, or every line on cooldown): legal silence, per the GDD's
	// `spotter_line | null` contract.
}

bool UBRSpotterSubsystem::TryRequestModelLine(FName TriggerId, const APlayerState* Subject, FText& OutLine)
{
	(void)TriggerId;
	(void)Subject;
	(void)OutLine;
	return false;
}

bool UBRSpotterSubsystem::TrySelectCannedLine(FName TriggerId, FName& OutRowName, FBRSpotterLineRow& OutRow)
{
	UDataTable* const Table = GetOrLoadLinesTable();
	if (!Table)
	{
		return false;
	}

	const float Now = GetServerTime();

	TArray<TPair<FName, const FBRSpotterLineRow*>> Candidates;
	float TotalWeight = 0.f;

	for (const TPair<FName, uint8*>& RowPair : Table->GetRowMap())
	{
		const FBRSpotterLineRow* const Row = reinterpret_cast<const FBRSpotterLineRow*>(RowPair.Value);
		if (!Row || Row->TriggerId != TriggerId || Row->Weight <= 0.f)
		{
			continue;
		}

		if (const float* const LastUsed = LineLastUsedServerTime.Find(RowPair.Key))
		{
			if ((Now - *LastUsed) < Row->RepeatCooldown_s)
			{
				continue;
			}
		}

		Candidates.Add(TPair<FName, const FBRSpotterLineRow*>(RowPair.Key, Row));
		TotalWeight += Row->Weight;
	}

	if (Candidates.Num() == 0 || TotalWeight <= 0.f)
	{
		return false;
	}

	float Roll = FMath::FRandRange(0.f, TotalWeight);
	for (const TPair<FName, const FBRSpotterLineRow*>& Candidate : Candidates)
	{
		Roll -= Candidate.Value->Weight;
		if (Roll <= 0.f)
		{
			OutRowName = Candidate.Key;
			OutRow = *Candidate.Value;
			return true;
		}
	}

	// Floating-point rounding left a remainder: the last candidate is still a legal pick.
	OutRowName = Candidates.Last().Key;
	OutRow = *Candidates.Last().Value;
	return true;
}

void UBRSpotterSubsystem::BroadcastLine(const APlayerState* Subject, EBRSpotterAudience Audience, const FText& Line)
{
	if (Line.IsEmpty())
	{
		return;
	}

	OnSpotterLineReady.Broadcast(Subject, Audience, Line);
}

UDataTable* UBRSpotterSubsystem::GetOrLoadLinesTable()
{
	if (LoadedLinesTable)
	{
		return LoadedLinesTable;
	}

	if (SpotterLinesTable.IsNull())
	{
		return nullptr;
	}

	// Synchronous: this loads once, off the hot path (killfeed events and match-end), and the
	// alternative is threading an async load through every call site for an asset this small.
	LoadedLinesTable = SpotterLinesTable.LoadSynchronous();
	if (!LoadedLinesTable)
	{
		UE_LOG(LogBRSpotter, Warning,
			TEXT("SpotterLinesTable '%s' failed to load. Spotter lines are silently disabled -- the game plays identically minus flavor."),
			*SpotterLinesTable.ToString());
	}

	return LoadedLinesTable;
}

float UBRSpotterSubsystem::GetServerTime() const
{
	const UWorld* const World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.f;
}
