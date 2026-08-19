#include "Match/BNGameState.h"
#include "Match/BNGameMode.h"
#include "Match/BNPlayerState.h"
#include "BreachpointNext.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

void ABNGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	// MatchState itself is the PARENT's property now — AGameState replicates it. Only BN's own
	// three ride here.
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

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
	Super::OnRep_MatchState();

	// An FName prints itself — no StaticEnum guard needed, which is the smaller reason this went
	// native. The bigger one: this body runs on every machine for every transition, server
	// included, because AGameState::SetMatchState calls the OnRep by hand on authority.
	UE_LOG(LogBN, Log, TEXT("BNGameState: match state -> %s"), *GetMatchState().ToString());

	OnMatchStateChanged.Broadcast(GetMatchState());
}

// Only once the match is actually over: a winner resolving while play continues is the reference
// arriving late, not an outcome, and re-announcing the ended state is what a reader that rendered
// a tie needs in order to correct itself.
void ABNGameState::OnRep_Winner()
{
	if (!HasMatchEnded())
	{
		return;
	}

	if (Winner)
	{
		UE_LOG(LogBN, Log, TEXT("BNGameState: winner resolved -> %s"), *Winner->GetPlayerName());
		OnMatchStateChanged.Broadcast(GetMatchState());
		return;
	}

	// Null DURING the post-match is not a tie — it is the winner LEAVING and their PlayerState
	// dying under this reference (the critic's find: re-broadcasting here rewrote a decided
	// match into "none (tie)" on every client). The result stands; say what happened and stay
	// out of the delegate. A real tie never passes through this notify at all — its winner was
	// null before the state flipped, so the property never changes.
	UE_LOG(LogBN, Log, TEXT("BNGameState: the winner left during the post-match — the result stands."));
}
