// Breachpoint. The replicated match view: phase, clock deadline, team scores, killfeed.
#include "Match/BRGameState.h"

#include "Core/BRCore.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

ABRGameState::ABRGameState()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	TeamScores.Init(0, BRMatch::NumTeams);

	KillFeed.Reserve(BRMatch::KillFeedCapacity);
}

void ABRGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABRGameState, MatchPhase);
	DOREPLIFETIME(ABRGameState, MatchEndServerTime);
	DOREPLIFETIME(ABRGameState, TeamScores);
	DOREPLIFETIME(ABRGameState, ScoreLimit);
	DOREPLIFETIME(ABRGameState, WinningTeamId);
	DOREPLIFETIME(ABRGameState, KillFeed);
}

float ABRGameState::GetSecondsRemaining() const
{
	if (MatchEndServerTime <= 0.f)
	{
		return 0.f;
	}

	return FMath::Max(0.f, MatchEndServerTime - GetServerWorldTimeSeconds());
}

int32 ABRGameState::GetTeamScore(uint8 TeamId) const
{
	return TeamScores.IsValidIndex(TeamId) ? TeamScores[TeamId] : 0;
}

void ABRGameState::ServerSetMatchPhase(EBRMatchPhase NewPhase)
{
	if (!HasAuthority() || NewPhase == MatchPhase)
	{
		return;
	}

	const EBRMatchPhase OldPhase = MatchPhase;
	MatchPhase = NewPhase;

	OnRep_MatchPhase(OldPhase);

	ForceNetUpdate();
}

void ABRGameState::ServerSetMatchEndServerTime(float NewEndServerTime)
{
	if (!HasAuthority())
	{
		return;
	}

	const float Sanitized = FMath::Max(0.f, NewEndServerTime);
	if (FMath::IsNearlyEqual(Sanitized, MatchEndServerTime))
	{
		return;
	}

	MatchEndServerTime = Sanitized;
	OnRep_MatchEndServerTime();
	ForceNetUpdate();
}

int32 ABRGameState::ServerAddTeamScore(uint8 TeamId, int32 Delta)
{
	if (!HasAuthority() || !TeamScores.IsValidIndex(TeamId) || Delta == 0)
	{
		return GetTeamScore(TeamId);
	}

	const TArray<int32> OldScores = TeamScores;

	TeamScores[TeamId] = FMath::Max(0, TeamScores[TeamId] + Delta);

	OnRep_TeamScores(OldScores);
	ForceNetUpdate();

	return TeamScores[TeamId];
}

void ABRGameState::ServerSetScoreLimit(int32 NewScoreLimit)
{
	if (!HasAuthority())
	{
		return;
	}

	if (IsScoringOpen())
	{
		UE_LOG(LogBRNet, Warning, TEXT("[Match] ServerSetScoreLimit(%d) refused: scoring already open."), NewScoreLimit);
		return;
	}

	ScoreLimit = FMath::Max(1, NewScoreLimit);
	ForceNetUpdate();
}

void ABRGameState::ServerSetWinningTeamId(uint8 TeamId)
{
	if (!HasAuthority())
	{
		return;
	}

	WinningTeamId = TeamId;
	ForceNetUpdate();
}

void ABRGameState::ServerPushKillFeedEntry(FBRKillFeedEntry Entry)
{
	if (!HasAuthority())
	{
		return;
	}

	Entry.Sequence = NextKillFeedSequence++;
	Entry.ServerTime = GetServerWorldTimeSeconds();

	if (KillFeed.Num() >= BRMatch::KillFeedCapacity)
	{
		KillFeed.RemoveAt(0, 1, EAllowShrinking::No);
	}
	KillFeed.Add(Entry);

	OnRep_KillFeed();
	ForceNetUpdate();
}

void ABRGameState::OnRep_MatchPhase(EBRMatchPhase OldPhase)
{
	OnMatchPhaseChanged.Broadcast(OldPhase, MatchPhase);
}

void ABRGameState::OnRep_MatchEndServerTime()
{
	OnMatchClockChanged.Broadcast(MatchEndServerTime);
}

void ABRGameState::OnRep_TeamScores(const TArray<int32>& OldScores)
{
	for (int32 TeamId = 0; TeamId < TeamScores.Num(); ++TeamId)
	{
		const int32 OldScore = OldScores.IsValidIndex(TeamId) ? OldScores[TeamId] : 0;
		if (OldScore != TeamScores[TeamId])
		{
			OnTeamScoreChanged.Broadcast(static_cast<uint8>(TeamId), TeamScores[TeamId]);
		}
	}
}

void ABRGameState::OnRep_KillFeed()
{
	for (const FBRKillFeedEntry& Entry : KillFeed)
	{
		if (Entry.Sequence > LastAnnouncedKillFeedSequence)
		{
			LastAnnouncedKillFeedSequence = Entry.Sequence;
			OnKillFeedEntryAdded.Broadcast(Entry);
		}
	}
}
