#include "Match/BNGameState.h"
#include "Match/BNGameMode.h"
#include "Match/BNPlayerState.h"
#include "Match/BNTeams.h"
#include "BreachpointNext.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

void ABNGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	// MatchState itself is the PARENT's property now — AGameState replicates it. Only BN's own
	// ride here.
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABNGameState, MatchEndServerTime);
	DOREPLIFETIME(ABNGameState, ScoreLimit);
	DOREPLIFETIME(ABNGameState, Winner);
	DOREPLIFETIME(ABNGameState, Killfeed);
	// COND_None on both is lawful: team totals and the decided team are scoreboard-grade,
	// shown to everyone (the packet's security audit). Empty in FFA, so the array costs
	// nothing until the first authoritative write sizes it.
	DOREPLIFETIME(ABNGameState, TeamScores);
	DOREPLIFETIME(ABNGameState, WinningTeamId);
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
	// SCORE, not kills, since the Hill landed: kills + objective points. Identical in
	// Slayer, where objective points are structurally zero.
	int32 TopScore = TNumericLimits<int32>::Lowest();
	for (APlayerState* PS : PlayerArray)
	{
		ABNPlayerState* BNPS = Cast<ABNPlayerState>(PS);
		if (!BNPS)
		{
			continue;
		}

		const int32 Score = BNPS->GetScore();
		if (Score > TopScore)
		{
			TopScore = Score;
			OutLeaders.Reset();
		}
		if (Score == TopScore)
		{
			OutLeaders.Add(BNPS);
		}
	}
}

void ABNGameState::PushKillfeedEntry(ABNPlayerState* Victim, ABNPlayerState* Killer, FName SourceName)
{
	if (!HasAuthority() || !Victim)
	{
		return;
	}

	FBNKillfeedRingEntry& Entry = Killfeed.AddDefaulted_GetRef();
	Entry.Victim = Victim;
	Entry.Killer = Killer;
	// Names captured NOW, while both PlayerStates are alive to ask — the winner-leaves lesson:
	// a ref can null under the feed mid-window, and a line that loses its name rewrites history.
	Entry.VictimName = Victim->GetPlayerName();
	Entry.KillerName = Killer ? Killer->GetPlayerName() : FString();
	Entry.SourceName = SourceName;
	Entry.Sequence = ++KillfeedSequence;
	Entry.ServerTime = GetServerWorldTimeSeconds();

	// The ring, kept by trimming the FRONT: the array replicates in order and readers dedupe by
	// Sequence, so a shifted tail costs nothing.
	while (Killfeed.Num() > KillfeedCapacity)
	{
		Killfeed.RemoveAt(0);
	}

	// The authority runs no OnRep — same discipline as the match-state announce.
	OnKillfeedChanged.Broadcast();
}

void ABNGameState::ResetKillfeed()
{
	if (HasAuthority() && Killfeed.Num() > 0)
	{
		Killfeed.Reset();
		OnKillfeedChanged.Broadcast();
	}
}

void ABNGameState::OnRep_Killfeed()
{
	OnKillfeedChanged.Broadcast();
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

int32 ABNGameState::GetTeamScore(uint8 Team) const
{
	// 0 for out-of-range OR before the first write sizes the array — honest-unknown, never a
	// crash: an FFA HUD or a late joiner asking early reads "no points yet", which is true.
	return TeamScores.IsValidIndex(Team) ? TeamScores[Team] : 0;
}

void ABNGameState::AddTeamScore(uint8 Team, int32 Points)
{
	if (!HasAuthority())
	{
		return;
	}

	if (Team >= BNTeams::NumTeams)
	{
		// Loud, not silent: the only callers are server-side scoring paths, so an out-of-range
		// team here is a bug upstream (a NoTeam player scoring for "team 255"), not input.
		UE_LOG(LogBN, Warning, TEXT("BNGameState: AddTeamScore refused — team %d is outside the %d-team record."),
			Team, BNTeams::NumTeams);
		return;
	}

	// Sized on the FIRST authoritative write, never in FFA: an empty array replicates nothing,
	// so the surface is free while bTeamsEnabled is false.
	if (TeamScores.Num() < BNTeams::NumTeams)
	{
		TeamScores.SetNumZeroed(BNTeams::NumTeams);
	}

	TeamScores[Team] += Points;

	// The authority runs no OnRep — same discipline as the killfeed push. Readers take the
	// team index and read GetTeamScore back; the delegate carries no number to keep honest.
	OnTeamScoreChanged.Broadcast(Team);
}

void ABNGameState::SetWinningTeamId(uint8 Team)
{
	// The SetWinner idiom: a gated write, no announce of its own — the match state machine's
	// broadcast is the one readers subscribe to, and they read GetWinningTeamId back there.
	if (HasAuthority())
	{
		WinningTeamId = Team;
	}
}

void ABNGameState::ResetTeamScores()
{
	// Never-sized stays never-sized: an FFA restart must not conjure a team record.
	if (!HasAuthority() || TeamScores.Num() == 0)
	{
		return;
	}

	for (int32& Score : TeamScores)
	{
		Score = 0;
	}

	// One broadcast PER TEAM, not per write and not zero: the delegate's payload is a team
	// index, so a single "everything changed" signal does not exist in its vocabulary, and at
	// NumTeams == 2 two calls is the whole storm. Zeroed first, THEN announced, so a subscriber
	// reading GetTeamScore back mid-loop never sees a half-reset record.
	for (int32 Team = 0; Team < TeamScores.Num(); ++Team)
	{
		OnTeamScoreChanged.Broadcast(static_cast<uint8>(Team));
	}
}

void ABNGameState::OnRep_TeamScores()
{
	// The array replicates WHOLE, so a client cannot know which entry moved — announce every
	// index and let readers read values back. Cheap at two; the killfeed ring's lesson about
	// batched updates, applied to the smaller array.
	for (int32 Team = 0; Team < TeamScores.Num(); ++Team)
	{
		OnTeamScoreChanged.Broadcast(static_cast<uint8>(Team));
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
