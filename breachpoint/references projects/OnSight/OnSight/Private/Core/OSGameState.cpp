#include "Core/OSGameState.h"
#include "Core/OSGameMode.h"
#include "OSLogCategories.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Core/OSPlayerState.h"

AOSGameState::AOSGameState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bCountdownEnabled(false)
{
	bReplicates = true;
	bAlwaysRelevant = true;
	TeamKills.SetNumZeroed(MaxTDMTeams);
	TeamDeaths.SetNumZeroed(MaxTDMTeams);
}

void AOSGameState::BeginPlay()
{
	Super::BeginPlay();
}

void AOSGameState::StartMatchTimer()
{
	if (!HasAuthority() || bMatchInProgress) return;

	bMatchInProgress = true;
	GetWorldTimerManager().ClearTimer(MatchTimerHandle);
	GetWorldTimerManager().SetTimer(MatchTimerHandle, this, &AOSGameState::UpdateMatchTime, 1.0f, true);
}

void AOSGameState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(MatchTimerHandle);
	Super::EndPlay(EndPlayReason);
}

void AOSGameState::UpdateMatchTime()
{
	if (!bMatchInProgress)
	{
		return;
	}

	if (bCountdownEnabled)
	{
		--MatchTime;
		
		if (MatchTime == 60)
		{
			Multicast_OneMinuteWarning();
		}
		
		if (MatchTime <= 0)
		{
			MatchTime = 0;
			NotifyTimeExpired();
		}
	}
	else
	{
		++MatchTime;
	}

	OnRep_MatchTime();
}

void AOSGameState::InitMatchTimer(int32 DurationSeconds)
{
	if (DurationSeconds > 0)
	{
		MatchTime = DurationSeconds;
		bCountdownEnabled = true;
	}
}

void AOSGameState::StopMatchTimer()
{
	bMatchInProgress = false;
	GetWorldTimerManager().ClearTimer(MatchTimerHandle);
	ForceNetUpdate();
}

void AOSGameState::NotifyTimeExpired()
{
	StopMatchTimer();

	if (UWorld* World = GetWorld())
	{
		if (AOSGameMode* GM = Cast<AOSGameMode>(World->GetAuthGameMode()))
		{
			GM->OnMatchTimeExpired();
		}
	}
}


void AOSGameState::OnRep_MatchTime()
{
	UE_LOG(LogTemp, Verbose, TEXT("OnRep_MatchTime: %d"), MatchTime);
	OnMatchTimeUpdated.Broadcast(MatchTime);
}

void AOSGameState::OnRep_Killfeed()
{
	for (const FOSKillfeedEntry& E : Killfeed)
	{
		if (E.ID > LastSeenID)
		{
			LastSeenID = E.ID;
			OnKillfeedEntryAdded.Broadcast(E);
		}
	}
}

void AOSGameState::OnRep_WinCondition()
{
	OnWinConditionChanged.Broadcast(WinConditionEntry);
}


void AOSGameState::AddKillfeed_Server( const FOSDeathEventInfo& Info )
{
	if (!HasAuthority()) return;
	UE_LOG(LogTemp, Warning, TEXT("[Killfeed][Server] Inst=%s Vict=%s UidInst=%d UidVict=%d"),
		*GetNameSafe(Info.InstigatorPS),
		*GetNameSafe(Info.VictimPS),
		Info.InstigatorPlayerStateUniqueId.IsValid(),
		Info.VictimPlayerStateUniqueId.IsValid());

	FOSKillfeedEntry E;
	E.DeathInfo = Info;
	E.ID = NextID++;

	Killfeed.Add(E);

	// keepin it small
	const int32 MaxEntries = 16;
	if (Killfeed.Num() > MaxEntries)
		Killfeed.RemoveAt(0, Killfeed.Num() - MaxEntries);

	ForceNetUpdate();
	OnRep_Killfeed();
}

void AOSGameState::UpdateWinCondition_Server( FOSWinConditionType type, const int32 target )
{
	if (type == FOSWinConditionType::NONE) return;
	
	WinConditionEntry.StatType = type;
	WinConditionEntry.StatTarget = target;
	ForceNetUpdate();
	OnRep_WinCondition();
}

void AOSGameState::UpdateMatchLeader()
{
	if (!HasAuthority()) return;
	
	
}

void AOSGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(AOSGameState, MatchTime, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME(AOSGameState, bMatchInProgress);
	DOREPLIFETIME(AOSGameState, Killfeed);
	DOREPLIFETIME(AOSGameState, WinConditionEntry);
	DOREPLIFETIME(AOSGameState, MatchResult);
	DOREPLIFETIME(AOSGameState, MatchLeader);
	DOREPLIFETIME(AOSGameState, TeamKills);
	DOREPLIFETIME(AOSGameState, TeamDeaths);
	DOREPLIFETIME(AOSGameState, bMatchOver);
	DOREPLIFETIME(AOSGameState, WinningTeamIndex);
	DOREPLIFETIME(AOSGameState, TDMKillLimit);
	DOREPLIFETIME(AOSGameState, TDMTimeLimitSeconds);
	DOREPLIFETIME(AOSGameState, ModeLayerClasses);
}

void AOSGameState::EnsureTeamArraysSize()
{
	if (TeamKills.Num() < MaxTDMTeams)
	{
		TeamKills.SetNumZeroed(MaxTDMTeams);
	}
	if (TeamDeaths.Num() < MaxTDMTeams)
	{
		TeamDeaths.SetNumZeroed(MaxTDMTeams);
	}
}

int32 AOSGameState::GetLowestPopulationTeamIndex() const
{
	TArray<int32> Counts;
	Counts.SetNumZeroed(MaxTDMTeams);
	for (int32 i = 0; i < PlayerArray.Num(); ++i)
	{
		if (const AOSPlayerState* PS = Cast<AOSPlayerState>(PlayerArray[i]))
		{
			const int32 Team = PS->GetTeamIndex();
			if (Team >= 0 && Team < MaxTDMTeams)
			{
				Counts[Team]++;
			}
		}
	}
	int32 BestTeam = 0;
	for (int32 t = 1; t < MaxTDMTeams; ++t)
	{
		if (Counts[t] < Counts[BestTeam])
		{
			BestTeam = t;
		}
	}
	return BestTeam;
}

int32 AOSGameState::GetTeamKills(int32 TeamIndex) const
{
	return TeamKills.IsValidIndex(TeamIndex) ? TeamKills[TeamIndex] : 0;
}

int32 AOSGameState::GetTeamDeaths(int32 TeamIndex) const
{
	return TeamDeaths.IsValidIndex(TeamIndex) ? TeamDeaths[TeamIndex] : 0;
}

void AOSGameState::AddTeamKill(int32 TeamIndex)
{
	if (!HasAuthority() || TeamIndex < 0 || TeamIndex >= MaxTDMTeams) return;
	EnsureTeamArraysSize();
	TeamKills[TeamIndex]++;
	OnRep_TeamKills();
}

void AOSGameState::AddTeamDeath(int32 TeamIndex)
{
	if (!HasAuthority() || TeamIndex < 0 || TeamIndex >= MaxTDMTeams) return;
	EnsureTeamArraysSize();
	TeamDeaths[TeamIndex]++;
	OnRep_TeamDeaths();
}

void AOSGameState::NotifyMatchWon(int32 TeamIndex, int32 OverrideWinnerValue)
{
	if (!HasAuthority()) return;
	bMatchOver = true;
	WinningTeamIndex = TeamIndex;

	MatchResult.bGameEnded = true;
	MatchResult.Winner = nullptr;
	MatchResult.WinnerValue = (OverrideWinnerValue != INDEX_NONE) ? OverrideWinnerValue : TeamIndex;

	UE_LOG(LogOSDomination, Log, TEXT("[MATCH] Over. WinningTeam=%d WinnerValue=%d"), TeamIndex, MatchResult.WinnerValue);

	ForceNetUpdate();
	OnRep_MatchResult();
}

void AOSGameState::OnRep_TeamKills()
{
	// Replication can briefly truncate arrays on join-in-progress / late relevance.
	// Ensure consistent array sizing before broadcasting per-team deltas (#63).
	EnsureTeamArraysSize();

	for (int32 i = 0; i < TeamKills.Num(); i++)
		OnTeamStatsChanged.Broadcast(i);
}

void AOSGameState::OnRep_TeamDeaths()
{
	// Keep team arrays aligned with TeamKills for any HUD consumers relying on indices (#63).
	EnsureTeamArraysSize();

	// Not necessary for now.
	//for (int32 i = 0; i < TeamDeaths.Num(); i++)
	//	OnTeamStatsChanged.Broadcast(i);
}

void AOSGameState::OnRep_MatchResult()
{
	OnMatchResultChanged.Broadcast(MatchResult);
	OnMatchResultChanged_BP.Broadcast(MatchResult);
}

void AOSGameState::OnRep_MatchLeader()
{
	OnMatchLeaderChanged.Broadcast(MatchLeader, WinConditionEntry.StatTarget);
}

void AOSGameState::AddPlayerState( APlayerState* PlayerState )
{
	Super::AddPlayerState(PlayerState);
	if (PlayerState)
	{
		auto* OSPS = Cast<AOSPlayerState>(PlayerState);
		OnPlayerArrayChanged.Broadcast(OSPS, true);
		OnPlayerArrayChanged_BP.Broadcast(OSPS, true);
	}
}

void AOSGameState::RemovePlayerState( APlayerState* PlayerState )
{
	AOSPlayerState* const OSPS = Cast<AOSPlayerState>(PlayerState);
	Super::RemovePlayerState(PlayerState);
	OnPlayerArrayChanged.Broadcast(OSPS, false);
	OnPlayerArrayChanged_BP.Broadcast(OSPS, false);
}

void AOSGameState::TryUpdateMatchLeader_Server(AOSPlayerState* Candidate)
{
	if (!HasAuthority() || !Candidate) return;

	int32 Value = 0;

	switch (WinConditionEntry.StatType)
	{
	case FOSWinConditionType::KILLS:
		Value = Candidate->Stats.Kills;
		break;

	case FOSWinConditionType::SCORE:
		Value = Candidate->Stats.Score; 
		break;

	default:
		return;
	}

	if (Value <= MatchLeader.LeaderValue) return;

	MatchLeader.Leader = Candidate;
	MatchLeader.LeaderValue = Value;
	ForceNetUpdate();
	OnRep_MatchLeader();
}

void AOSGameState::Multicast_OneMinuteWarning_Implementation()
{
	OnOneMinuteWarning.Broadcast();
}
