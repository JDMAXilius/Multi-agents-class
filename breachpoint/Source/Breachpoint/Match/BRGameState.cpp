// Breachpoint. The replicated match view: phase, clock deadline, team scores, killfeed.

#include "Match/BRGameState.h"

#include "Core/BRCore.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

ABRGameState::ABRGameState()
{
	// Law 4: no gameplay Tick anywhere in the match frame. Phase, scores and the killfeed
	// are event-driven; the countdown is derived on demand from one replicated float.
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// Fixed team table, allocated once. Never Add/Remove after this: a resized replicated
	// array costs a full array resend, and 4v4 has exactly two sides.
	TeamScores.Init(0, BRMatch::NumTeams);

	KillFeed.Reserve(BRMatch::KillFeedCapacity);
}

void ABRGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Every property below is public match state that all four-plus-four players and any
	// spectator must agree on, so each is COND_None BY DECISION, not by default. See the
	// rationale table in the header. Anything that becomes per-player private later gets
	// DOREPLIFETIME_CONDITION(..., COND_OwnerOnly) and moves off the GameState entirely.
	DOREPLIFETIME(ABRGameState, MatchPhase);
	DOREPLIFETIME(ABRGameState, MatchEndServerTime);
	DOREPLIFETIME(ABRGameState, TeamScores);
	DOREPLIFETIME(ABRGameState, ScoreLimit);
	DOREPLIFETIME(ABRGameState, WinningTeamId);
	DOREPLIFETIME(ABRGameState, KillFeed);
}

// ---------------------------------------------------------------------------
// Read side
// ---------------------------------------------------------------------------

float ABRGameState::GetSecondsRemaining() const
{
	if (MatchEndServerTime <= 0.f)
	{
		return 0.f;
	}

	// GetServerWorldTimeSeconds() is authoritative server time on the server and the
	// client's synced estimate of it on a client. Same subtraction on every machine =>
	// the same number on every machine, with zero per-second traffic.
	return FMath::Max(0.f, MatchEndServerTime - GetServerWorldTimeSeconds());
}

int32 ABRGameState::GetTeamScore(uint8 TeamId) const
{
	return TeamScores.IsValidIndex(TeamId) ? TeamScores[TeamId] : 0;
}

// ---------------------------------------------------------------------------
// Write side — server only, one authority gate per entry point
// ---------------------------------------------------------------------------

void ABRGameState::ServerSetMatchPhase(EBRMatchPhase NewPhase)
{
	if (!HasAuthority() || NewPhase == MatchPhase)
	{
		return;
	}

	const EBRMatchPhase OldPhase = MatchPhase;
	MatchPhase = NewPhase;

	// The listen-server host is not a client of itself: RepNotifies do not fire locally,
	// so mirror the cosmetic notify by hand or the host's HUD silently lags every client's.
	OnRep_MatchPhase(OldPhase);

	// Phase is a rare, latency-sensitive change. Push it now instead of waiting for the
	// next scheduled net update; this is why the class does not need a raised frequency.
	ForceNetUpdate();

	UE_LOG(LogBRNet, Log, TEXT("[Match] Phase %d -> %d (server time %.2f)"),
		static_cast<int32>(OldPhase), static_cast<int32>(NewPhase), GetServerWorldTimeSeconds());
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

	// Clamped at zero: the -1 self rule may never drive a scoreboard negative, and a
	// negative score would break both the win check and the HUD's fill ratio.
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

	// Refused once scoring is open: moving the goalposts mid-match would let a late
	// data reload decide a live game. Rules are read before the first shot or not at all.
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

	// The server stamps identity and time. A caller (or a future RPC path) cannot forge a
	// sequence number to re-order or suppress somebody else's row.
	Entry.Sequence = NextKillFeedSequence++;
	Entry.ServerTime = GetServerWorldTimeSeconds();

	// Ring, not log: bounded memory AND bounded bandwidth. The array resends on change, so
	// the cap is the per-kill cost ceiling.
	if (KillFeed.Num() >= BRMatch::KillFeedCapacity)
	{
		KillFeed.RemoveAt(0, 1, EAllowShrinking::No);
	}
	KillFeed.Add(Entry);

	OnRep_KillFeed();
	ForceNetUpdate();
}

// ---------------------------------------------------------------------------
// RepNotifies — cosmetic only (netcode.md law 3)
// ---------------------------------------------------------------------------

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
	// One announcement per row, in order, exactly once — even though the whole array
	// arrives on every change and even if two kills land inside one net update.
	// Law 7: a late joiner's first RepNotify carries rows it never saw live; the high-water
	// mark starts at 0, so it announces them once and then stays quiet. If that ever looks
	// wrong on screen, the fix is in the widget (drop rows older than N seconds via
	// Entry.ServerTime), not more replication.
	for (const FBRKillFeedEntry& Entry : KillFeed)
	{
		if (Entry.Sequence > LastAnnouncedKillFeedSequence)
		{
			LastAnnouncedKillFeedSequence = Entry.Sequence;
			OnKillFeedEntryAdded.Broadcast(Entry);
		}
	}
}
