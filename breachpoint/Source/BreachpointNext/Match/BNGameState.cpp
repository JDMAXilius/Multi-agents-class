#include "Match/BNGameState.h"
#include "Match/BNGameMode.h"
#include "Match/BNPlayerState.h"
#include "BreachpointNext.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

void ABNGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABNGameState, MatchState);
	DOREPLIFETIME(ABNGameState, MatchEndServerTime);
	DOREPLIFETIME(ABNGameState, ScoreLimit);
	DOREPLIFETIME(ABNGameState, Winner);
}

void ABNGameState::BeginPlay()
{
	Super::BeginPlay();

	// PULLED from the mode rather than pushed by it: the number is the mode's config, but the
	// GameState is the only copy a client can read, and a pull cannot be forgotten.
	if (HasAuthority())
	{
		if (const ABNGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ABNGameMode>() : nullptr)
		{
			ScoreLimit = Mode->GetScoreLimit();
		}
	}
}

float ABNGameState::GetRemainingSeconds() const
{
	if (MatchEndServerTime <= 0.0)
	{
		return 0.f;
	}

	return static_cast<float>(FMath::Max(0.0, MatchEndServerTime - GetServerWorldTimeSeconds()));
}

void ABNGameState::GetLeaders(TArray<ABNPlayerState*>& OutLeaders) const
{
	OutLeaders.Reset();

	// One pass, no sort: the deliverable is the tie SET, not an ordering, and a tie at the top is
	// a real FFA outcome the caller has to be able to see.
	int32 TopKills = TNumericLimits<int32>::Lowest();
	for (APlayerState* PS : PlayerArray)
	{
		ABNPlayerState* BNPS = Cast<ABNPlayerState>(PS);
		if (!BNPS)
		{
			continue;
		}

		const int32 Kills = BNPS->GetKills();
		if (Kills > TopKills)
		{
			TopKills = Kills;
			OutLeaders.Reset();
		}
		if (Kills == TopKills)
		{
			OutLeaders.Add(BNPS);
		}
	}
}

void ABNGameState::SetMatchState(EBNMatchState NewState)
{
	if (!HasAuthority() || MatchState == NewState)
	{
		return;
	}

	MatchState = NewState;

	// The server runs no OnRep, so it announces here — same body, so every machine logs and
	// broadcasts exactly once for the same transition.
	HandleMatchStateChanged();
}

void ABNGameState::SetMatchEndServerTime(double InEndServerTime)
{
	if (HasAuthority())
	{
		MatchEndServerTime = InEndServerTime;
	}
}

void ABNGameState::SetWinner(ABNPlayerState* InWinner)
{
	if (HasAuthority())
	{
		Winner = InWinner;
	}
}

void ABNGameState::OnRep_MatchState()
{
	HandleMatchStateChanged();
}

void ABNGameState::HandleMatchStateChanged()
{
	// Guarded, because this is a LOG: a line added to explain the session must never be the thing
	// that ends it. Same rule the aim probe's axis name learned the hard way.
	const UEnum* StateEnum = StaticEnum<EBNMatchState>();
	const FString StateName = StateEnum
		? StateEnum->GetNameStringByValue(static_cast<int64>(MatchState))
		: FString::Printf(TEXT("%d"), static_cast<int32>(MatchState));
	UE_LOG(LogBN, Log, TEXT("BNGameState: match state -> %s"), *StateName);

	OnMatchStateChanged.Broadcast(MatchState);
}
