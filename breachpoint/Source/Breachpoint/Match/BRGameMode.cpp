// Breachpoint. The server-only match spine: phase machine, kill attribution, scored respawn.
#include "Match/BRGameMode.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Core/BRCore.h"
#include "Core/BRGameplayTags.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "GenericTeamAgentInterface.h"
#include "Character/BRCharacter.h"
#include "Match/BRGameState.h"
#include "Match/BRPlayerController.h"
#include "Match/BRPlayerState.h"
#include "TimerManager.h"

namespace
{
	constexpr float MinPhaseSeconds = 0.1f;

	constexpr float SpawnReuseWindowSeconds = 5.f;
}

ABRGameMode::ABRGameMode()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	GameStateClass       = ABRGameState::StaticClass();
	PlayerStateClass     = ABRPlayerState::StaticClass();
	PlayerControllerClass = ABRPlayerController::StaticClass();
	DefaultPawnClass     = ABRCharacter::StaticClass();

	TeamDamageDealt.Init(0.f, BRMatch::NumTeams);
}

void ABRGameMode::InitGameState()
{
	Super::InitGameState();

	if (ABRGameState* BRGS = GetBRGameState())
	{
		BRGS->ServerSetScoreLimit(ScoreLimit);
	}
}

void ABRGameMode::OnPostLogin(AController* NewPlayer)
{
	Super::OnPostLogin(NewPlayer);

	RegisterCombatant(NewPlayer);
}

void ABRGameMode::Logout(AController* Exiting)
{
	UnregisterCombatant(Exiting);

	Super::Logout(Exiting);
}

void ABRGameMode::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();

	EnterPhase(EBRMatchPhase::WarmUp);
}

bool ABRGameMode::PlayerCanRestart_Implementation(APlayerController* Player)
{
	if (GetMatchPhase() == EBRMatchPhase::PostMatch)
	{
		return false;
	}

	return Super::PlayerCanRestart_Implementation(Player);
}

EBRMatchPhase ABRGameMode::GetMatchPhase() const
{
	const ABRGameState* BRGS = GetBRGameState();
	return BRGS ? BRGS->GetMatchPhase() : EBRMatchPhase::None;
}

void ABRGameMode::EnterPhase(EBRMatchPhase NewPhase)
{
	if (!HasAuthority())
	{
		return;
	}

	ABRGameState* BRGS = GetBRGameState();
	if (!BRGS || BRGS->GetMatchPhase() == NewPhase)
	{
		return;
	}

	FTimerManager& TimerManager = GetWorldTimerManager();

	TimerManager.ClearTimer(WarmupTimerHandle);
	TimerManager.ClearTimer(MatchClockTimerHandle);
	TimerManager.ClearTimer(SuddenDeathTimerHandle);
	TimerManager.ClearTimer(PostMatchTimerHandle);

	BRGS->ServerSetMatchPhase(NewPhase);

	const float Now = GetServerTime();

	switch (NewPhase)
	{
	case EBRMatchPhase::WarmUp:
	{
		const float Duration = FMath::Max(MinPhaseSeconds, WarmupSeconds);
		BRGS->ServerSetMatchEndServerTime(Now + Duration);
		TimerManager.SetTimer(WarmupTimerHandle, this, &ABRGameMode::HandleWarmupElapsed, Duration, false);
		break;
	}
	case EBRMatchPhase::Live:
	{
		const float Duration = FMath::Max(MinPhaseSeconds, MatchDurationSeconds);
		BRGS->ServerSetMatchEndServerTime(Now + Duration);
		TimerManager.SetTimer(MatchClockTimerHandle, this, &ABRGameMode::HandleMatchClockExpired, Duration, false);
		break;
	}
	case EBRMatchPhase::SuddenDeath:
	{
		const float Duration = FMath::Max(MinPhaseSeconds, SuddenDeathSeconds);
		BRGS->ServerSetMatchEndServerTime(Now + Duration);
		TimerManager.SetTimer(SuddenDeathTimerHandle, this, &ABRGameMode::HandleSuddenDeathExpired, Duration, false);
		break;
	}
	case EBRMatchPhase::PostMatch:
	{
		const float Duration = FMath::Max(MinPhaseSeconds, PostMatchSeconds);
		BRGS->ServerSetMatchEndServerTime(Now + Duration);
		TimerManager.SetTimer(PostMatchTimerHandle, this, &ABRGameMode::HandlePostMatchElapsed, Duration, false);

		for (TPair<TWeakObjectPtr<AController>, FTimerHandle>& Pair : RespawnTimers)
		{
			TimerManager.ClearTimer(Pair.Value);
		}
		RespawnTimers.Reset();
		break;
	}
	case EBRMatchPhase::None:
	default:
		BRGS->ServerSetMatchEndServerTime(0.f);
		break;
	}
}

void ABRGameMode::HandleWarmupElapsed()
{
	EnterPhase(EBRMatchPhase::Live);
}

void ABRGameMode::HandleMatchClockExpired()
{
	if (bMatchDecided)
	{
		return;
	}

	const ABRGameState* BRGS = GetBRGameState();
	if (!BRGS)
	{
		return;
	}

	int32 BestScore = TNumericLimits<int32>::Min();
	uint8 BestTeam = BRMatch::InvalidTeamId;
	bool bTied = false;

	for (uint8 TeamId = 0; TeamId < BRMatch::NumTeams; ++TeamId)
	{
		const int32 Score = BRGS->GetTeamScore(TeamId);
		if (Score > BestScore)
		{
			BestScore = Score;
			BestTeam = TeamId;
			bTied = false;
		}
		else if (Score == BestScore)
		{
			bTied = true;
		}
	}

	if (bTied)
	{
		EnterPhase(EBRMatchPhase::SuddenDeath);
		return;
	}

	EnterPostMatch(BestTeam);
}

void ABRGameMode::HandleSuddenDeathExpired()
{
	if (bMatchDecided)
	{
		return;
	}

	const ABRGameState* BRGS = GetBRGameState();
	if (!BRGS)
	{
		return;
	}

	uint8 BestTeam = BRMatch::InvalidTeamId;
	int32 BestScore = TNumericLimits<int32>::Min();
	float BestDamage = -1.f;
	bool bAmbiguous = false;

	for (uint8 TeamId = 0; TeamId < BRMatch::NumTeams; ++TeamId)
	{
		const int32 Score = BRGS->GetTeamScore(TeamId);
		const float Damage = TeamDamageDealt.IsValidIndex(TeamId) ? TeamDamageDealt[TeamId] : 0.f;

		if (Score > BestScore || (Score == BestScore && Damage > BestDamage))
		{
			BestScore = Score;
			BestDamage = Damage;
			BestTeam = TeamId;
			bAmbiguous = false;
		}
		else if (Score == BestScore && FMath::IsNearlyEqual(Damage, BestDamage))
		{
			bAmbiguous = true;
		}
	}

	EnterPostMatch(bAmbiguous ? BRMatch::InvalidTeamId : BestTeam);
}

void ABRGameMode::HandlePostMatchElapsed()
{
	RequestMatchTeardown();
}

void ABRGameMode::RequestMatchTeardown()
{
}

void ABRGameMode::ScheduleWinCheck()
{
	if (!HasAuthority() || bWinCheckPending || bMatchDecided)
	{
		return;
	}

	bWinCheckPending = true;

	GetWorldTimerManager().SetTimerForNextTick(this, &ABRGameMode::EvaluateWinConditions);
}

void ABRGameMode::EvaluateWinConditions()
{
	bWinCheckPending = false;

	if (!HasAuthority() || bMatchDecided)
	{
		return;
	}

	const ABRGameState* BRGS = GetBRGameState();
	if (!BRGS || !BRGS->IsScoringOpen())
	{
		return;
	}

	const bool bSuddenDeath = BRGS->GetMatchPhase() == EBRMatchPhase::SuddenDeath;

	uint8 BestTeam = BRMatch::InvalidTeamId;
	int32 BestScore = TNumericLimits<int32>::Min();
	int32 SecondScore = TNumericLimits<int32>::Min();
	float BestDamage = -1.f;

	for (uint8 TeamId = 0; TeamId < BRMatch::NumTeams; ++TeamId)
	{
		const int32 Score = BRGS->GetTeamScore(TeamId);
		const float Damage = TeamDamageDealt.IsValidIndex(TeamId) ? TeamDamageDealt[TeamId] : 0.f;

		if (Score > BestScore || (Score == BestScore && Damage > BestDamage))
		{
			SecondScore = FMath::Max(SecondScore, BestScore);
			BestScore = Score;
			BestDamage = Damage;
			BestTeam = TeamId;
		}
		else
		{
			SecondScore = FMath::Max(SecondScore, Score);
		}
	}

	if (bSuddenDeath)
	{
		if (BestScore > SecondScore)
		{
			EnterPostMatch(BestTeam);
		}
		return;
	}

	if (BestScore >= ScoreLimit)
	{
		EnterPostMatch(BestTeam);
	}
}

void ABRGameMode::EnterPostMatch(uint8 WinningTeamId)
{
	if (!HasAuthority() || bMatchDecided)
	{
		return;
	}

	bMatchDecided = true;

	if (ABRGameState* BRGS = GetBRGameState())
	{
		BRGS->ServerSetWinningTeamId(WinningTeamId);
	}

	EnterPhase(EBRMatchPhase::PostMatch);

	PendingRespawnPlayers.Reset();
	EarliestRespawnServerTime.Reset();

	if (IsMatchInProgress())
	{
		EndMatch();
	}
}

void ABRGameMode::NotifyDamageDealt(APlayerState* Attacker, APlayerState* Victim, float Amount, FGameplayTag DamageTag)
{
	if (!HasAuthority() || !Victim)
	{
		return;
	}

	if (!(Amount > 0.f) || Amount > MaxPlausibleSingleDamage || !FMath::IsFinite(Amount))
	{
		UE_LOG(LogBRNet, Warning, TEXT("[Match] Implausible damage report %.2f dropped (victim %s)."),
			Amount, *GetNameSafe(Victim));
		return;
	}

	PruneDamageLedger(Victim);

	TArray<FBRDamageRecord>& Records = DamageLedger.FindOrAdd(Victim);
	FBRDamageRecord& Record = Records.AddDefaulted_GetRef();
	Record.Attacker = Attacker;
	Record.Amount = Amount;
	Record.ServerTime = GetServerTime();
	Record.DamageTag = DamageTag;

	const ABRGameState* BRGS = GetBRGameState();
	if (!BRGS || !BRGS->IsScoringOpen() || !Attacker || Attacker == Victim)
	{
		return;
	}

	const uint8 AttackerTeam = ResolveTeamId(Attacker);
	const uint8 VictimTeam = ResolveTeamId(Victim);
	if (AttackerTeam != VictimTeam && TeamDamageDealt.IsValidIndex(AttackerTeam))
	{
		TeamDamageDealt[AttackerTeam] += Amount;
	}
}

void ABRGameMode::PruneDamageLedger(APlayerState* Victim)
{
	TArray<FBRDamageRecord>* Records = DamageLedger.Find(Victim);
	if (!Records)
	{
		return;
	}

	const float Cutoff = GetServerTime() - FMath::Max(0.f, KillCreditWindowSeconds);
	Records->RemoveAll([Cutoff](const FBRDamageRecord& Record)
	{
		return Record.ServerTime < Cutoff || !Record.Attacker.IsValid();
	});
}

APlayerState* ABRGameMode::ResolveKiller(APlayerState* Victim, APlayerState* EventInstigator) const
{
	if (!Victim)
	{
		return nullptr;
	}

	const float Cutoff = GetServerTime() - FMath::Max(0.f, KillCreditWindowSeconds);

	if (const TArray<FBRDamageRecord>* Records = DamageLedger.Find(Victim))
	{
		for (int32 Index = Records->Num() - 1; Index >= 0; --Index)
		{
			const FBRDamageRecord& Record = (*Records)[Index];
			if (Record.ServerTime < Cutoff)
			{
				break;
			}

			APlayerState* Attacker = Record.Attacker.Get();
			if (Attacker && Attacker != Victim)
			{
				return Attacker;
			}
		}
	}

	if (EventInstigator && EventInstigator != Victim)
	{
		return EventInstigator;
	}

	return nullptr;
}

void ABRGameMode::CollectAssists(APlayerState* Victim, APlayerState* Killer, TArray<APlayerState*>& OutAssists) const
{
	OutAssists.Reset();

	const TArray<FBRDamageRecord>* Records = DamageLedger.Find(Victim);
	if (!Records)
	{
		return;
	}

	const float Cutoff = GetServerTime() - FMath::Max(0.f, KillCreditWindowSeconds);

	for (const FBRDamageRecord& Record : *Records)
	{
		if (Record.ServerTime < Cutoff)
		{
			continue;
		}

		APlayerState* Attacker = Record.Attacker.Get();
		if (!Attacker || Attacker == Victim || Attacker == Killer)
		{
			continue;
		}

		if (ResolveTeamId(Attacker) == ResolveTeamId(Victim))
		{
			continue;
		}

		OutAssists.AddUnique(Attacker);
	}
}

void ABRGameMode::HandleDeathEvent(APlayerState* Victim, APlayerState* EventInstigator, FGameplayTag CauseTag)
{
	if (!HasAuthority() || !Victim)
	{
		return;
	}

	ABRGameState* BRGS = GetBRGameState();
	if (!BRGS)
	{
		return;
	}

	if (PendingRespawnPlayers.Contains(Victim))
	{
		return;
	}
	PendingRespawnPlayers.Add(Victim);

	APlayerState* Killer = ResolveKiller(Victim, EventInstigator);

	TArray<APlayerState*> Assists;
	CollectAssists(Victim, Killer, Assists);

	DamageLedger.Remove(Victim);

	const uint8 VictimTeam = ResolveTeamId(Victim);
	const uint8 KillerTeam = Killer ? ResolveTeamId(Killer) : BRMatch::InvalidTeamId;
	const bool bSelfInflicted = (Killer == nullptr);
	const bool bFriendlyFire = (!bSelfInflicted && KillerTeam != BRMatch::InvalidTeamId && KillerTeam == VictimTeam);

	const bool bScoringOpen = BRGS->IsScoringOpen() && !bMatchDecided;

	if (bScoringOpen)
	{
		if (bSelfInflicted)
		{
			BRGS->ServerAddTeamScore(VictimTeam, -1);
			SendKillEvent(Victim, Victim, -1.f, CauseTag);
		}
		else if (bFriendlyFire)
		{
			if (bFriendlyFirePenalizesKillersTeam)
			{
				BRGS->ServerAddTeamScore(KillerTeam, -1);
			}
			SendKillEvent(Killer, Victim, -1.f, CauseTag);
		}
		else
		{
			BRGS->ServerAddTeamScore(KillerTeam, 1);
			SendKillEvent(Killer, Victim, 1.f, CauseTag);
		}

		FBRKillFeedEntry Entry;
		Entry.Killer = Killer;
		Entry.Victim = Victim;
		Entry.KillerTeamId = KillerTeam;
		Entry.VictimTeamId = VictimTeam;
		Entry.CauseTag = CauseTag;
		Entry.bSelfInflicted = bSelfInflicted ? 1 : 0;
		Entry.bFriendlyFire = bFriendlyFire ? 1 : 0;
		BRGS->ServerPushKillFeedEntry(Entry);

		OnPlayerKilled.Broadcast(Killer, Victim, Assists);

		ScheduleWinCheck();
	}
	else
	{
	}

	StartRespawnTimer(Victim->GetOwningController());
}

void ABRGameMode::SendKillEvent(APlayerState* Recipient, APlayerState* Victim, float Magnitude, FGameplayTag CauseTag) const
{
	if (!HasAuthority() || !Recipient)
	{
		return;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Recipient);
	if (!ASC)
	{
		return;
	}

	FGameplayEventData Payload;
	Payload.EventTag = BRGameplayTags::Event_Kill;
	Payload.Instigator = Recipient;
	Payload.Target = Victim;
	Payload.EventMagnitude = Magnitude;
	Payload.InstigatorTags.AddTag(CauseTag);

	ASC->HandleGameplayEvent(BRGameplayTags::Event_Kill, &Payload);
}

void ABRGameMode::RegisterCombatant(AController* Combatant)
{
	if (!HasAuthority() || !Combatant || !Combatant->PlayerState)
	{
		return;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Combatant->PlayerState);
	if (!ASC || DeathListenerHandles.Contains(ASC))
	{
		return;
	}

	const FDelegateHandle Handle = ASC->GenericGameplayEventCallbacks
		.FindOrAdd(BRGameplayTags::Event_Death)
		.AddUObject(this, &ABRGameMode::OnDeathGameplayEvent);

	DeathListenerHandles.Add(ASC, Handle);
}

void ABRGameMode::UnregisterCombatant(AController* Combatant)
{
	if (!Combatant)
	{
		return;
	}

	APlayerState* PS = Combatant->PlayerState;
	if (PS)
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS))
		{
			if (const FDelegateHandle* Handle = DeathListenerHandles.Find(ASC))
			{
				if (FGameplayEventMulticastDelegate* Delegate = ASC->GenericGameplayEventCallbacks.Find(BRGameplayTags::Event_Death))
				{
					Delegate->Remove(*Handle);
				}
				DeathListenerHandles.Remove(ASC);
			}
		}

		DamageLedger.Remove(PS);
		PendingRespawnPlayers.Remove(PS);
	}

	if (FTimerHandle* Handle = RespawnTimers.Find(Combatant))
	{
		GetWorldTimerManager().ClearTimer(*Handle);
		RespawnTimers.Remove(Combatant);
	}
	EarliestRespawnServerTime.Remove(Combatant);

	for (auto It = DeathListenerHandles.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
	for (auto It = DamageLedger.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
	for (auto It = SpawnPointLastUsed.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

void ABRGameMode::OnDeathGameplayEvent(const FGameplayEventData* Payload)
{
	if (!HasAuthority() || !Payload)
	{
		return;
	}

	APlayerState* Victim = ResolvePlayerState(Payload->Target);
	if (!Victim)
	{
		return;
	}

	FGameplayTag CauseTag;
	for (const FGameplayTag& Tag : Payload->InstigatorTags)
	{
		if (Tag.MatchesTag(BRGameplayTags::Damage_Kinetic) || Tag.MatchesTag(BRGameplayTags::Damage_Explosive) || Tag.MatchesTag(BRGameplayTags::Damage_Melee))
		{
			CauseTag = Tag;
			break;
		}
	}

	HandleDeathEvent(Victim, ResolvePlayerState(Payload->Instigator), CauseTag);
}

void ABRGameMode::StartRespawnTimer(AController* Victim)
{
	if (!HasAuthority() || !Victim)
	{
		return;
	}

	if (GetMatchPhase() == EBRMatchPhase::PostMatch || bMatchDecided)
	{
		return;
	}

	const float Delay = FMath::Max(0.f, RespawnDelaySeconds);

	EarliestRespawnServerTime.Add(Victim, GetServerTime() + Delay);

	FTimerHandle& Handle = RespawnTimers.FindOrAdd(Victim);
	GetWorldTimerManager().ClearTimer(Handle);
	GetWorldTimerManager().SetTimer(
		Handle,
		FTimerDelegate::CreateUObject(this, &ABRGameMode::HandleRespawnTimerElapsed, TWeakObjectPtr<AController>(Victim)),
		FMath::Max(MinPhaseSeconds, Delay),
		false);
}

void ABRGameMode::HandleRespawnTimerElapsed(TWeakObjectPtr<AController> WeakVictim)
{
	AController* Victim = WeakVictim.Get();
	if (!Victim)
	{
		return;
	}

	RespawnTimers.Remove(Victim);

	if (!CanRespawnNow(Victim))
	{
		return;
	}

	if (APlayerState* PS = Victim->PlayerState)
	{
		PendingRespawnPlayers.Remove(PS);
	}
	EarliestRespawnServerTime.Remove(Victim);

	RestartPlayer(Victim);
}

bool ABRGameMode::CanRespawnNow(AController* Candidate) const
{
	if (!HasAuthority() || !Candidate || !Candidate->PlayerState)
	{
		return false;
	}

	const EBRMatchPhase Phase = GetMatchPhase();

	if (Phase == EBRMatchPhase::PostMatch || Phase == EBRMatchPhase::None || bMatchDecided)
	{
		return false;
	}

	if (Phase == EBRMatchPhase::SuddenDeath && !bAllowRespawnInSuddenDeath)
	{
		return false;
	}

	if (!PendingRespawnPlayers.Contains(Candidate->PlayerState))
	{
		return false;
	}

	if (const float* Earliest = EarliestRespawnServerTime.Find(Candidate))
	{
		if (GetServerTime() + KINDA_SMALL_NUMBER < *Earliest)
		{
			return false;
		}
	}

	return true;
}

bool ABRGameMode::RequestRespawn(AController* Requester)
{
	if (!HasAuthority() || !Requester)
	{
		return false;
	}

	if (!CanRespawnNow(Requester))
	{
		return false;
	}

	if (FTimerHandle* Handle = RespawnTimers.Find(Requester))
	{
		GetWorldTimerManager().ClearTimer(*Handle);
		RespawnTimers.Remove(Requester);
	}

	if (APlayerState* PS = Requester->PlayerState)
	{
		PendingRespawnPlayers.Remove(PS);
	}
	EarliestRespawnServerTime.Remove(Requester);

	RestartPlayer(Requester);
	return true;
}

void ABRGameMode::GatherSpawnCandidates(TArray<AActor*>& OutCandidates) const
{
	OutCandidates.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		if (APlayerStart* Start = *It)
		{
			OutCandidates.Add(Start);
		}
	}
}

float ABRGameMode::ScoreSpawnCandidate(const AActor* Candidate, AController* ForPlayer) const
{
	if (!Candidate)
	{
		return -1.f;
	}

	const UWorld* World = GetWorld();
	const AGameStateBase* GS = GetGameState<AGameStateBase>();
	if (!World || !GS)
	{
		return 0.f;
	}

	const FVector CandidateLocation = Candidate->GetActorLocation();
	const uint8 SpawningTeam = ForPlayer ? ResolveTeamId(ForPlayer->PlayerState) : BRMatch::InvalidTeamId;

	float NearestThreatDistance = TNumericLimits<float>::Max();
	bool bVisibleToThreat = false;

	for (APlayerState* PS : GS->PlayerArray)
	{
		if (!PS || PS == (ForPlayer ? ForPlayer->PlayerState : nullptr))
		{
			continue;
		}

		if (ResolveTeamId(PS) == SpawningTeam || PendingRespawnPlayers.Contains(PS))
		{
			continue;
		}

		const APawn* ThreatPawn = PS->GetPawn();
		if (!ThreatPawn)
		{
			continue;
		}

		const FVector ThreatLocation = ThreatPawn->GetActorLocation();
		const float Distance = FVector::Dist(CandidateLocation, ThreatLocation);
		NearestThreatDistance = FMath::Min(NearestThreatDistance, Distance);

		if (!bVisibleToThreat)
		{
			FCollisionQueryParams Params(SCENE_QUERY_STAT(BRSpawnVisibility), false);
			Params.AddIgnoredActor(Candidate);
			Params.AddIgnoredActor(ThreatPawn);
			bVisibleToThreat = !World->LineTraceTestByChannel(
				CandidateLocation + FVector(0.f, 0.f, 88.f), ThreatLocation, ECC_Visibility, Params);
		}
	}

	if (NearestThreatDistance == TNumericLimits<float>::Max())
	{
		NearestThreatDistance = 100000.f;
	}

	float Score = NearestThreatDistance;

	if (bVisibleToThreat)
	{
		Score *= 0.25f;
	}

	if (const float* LastUsed = SpawnPointLastUsed.Find(const_cast<AActor*>(Candidate)))
	{
		if (GetServerTime() - *LastUsed < SpawnReuseWindowSeconds)
		{
			Score *= 0.5f;
		}
	}

	return Score;
}

AActor* ABRGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	TArray<AActor*> Candidates;
	GatherSpawnCandidates(Candidates);

	if (Candidates.Num() == 0)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	AActor* Best = nullptr;
	float BestScore = -1.f;

	for (AActor* Candidate : Candidates)
	{
		const float Score = ScoreSpawnCandidate(Candidate, Player);
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Candidate;
		}
	}

	if (!Best)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	SpawnPointLastUsed.Add(Best, GetServerTime());
	return Best;
}

void ABRGameMode::ApplyMatchRules(int32 InScoreLimit, float InMatchSeconds, float InSuddenDeathSeconds,
	float InWarmupSeconds, float InRespawnSeconds, float InKillCreditWindowSeconds)
{
	if (!HasAuthority())
	{
		return;
	}

	const ABRGameState* BRGS = GetBRGameState();
	if (BRGS && BRGS->IsScoringOpen())
	{
		UE_LOG(LogBRNet, Warning, TEXT("[Match] ApplyMatchRules refused: scoring already open."));
		return;
	}

	ScoreLimit = FMath::Clamp(InScoreLimit, 1, 999);
	MatchDurationSeconds = FMath::Clamp(InMatchSeconds, 10.f, 3600.f);
	SuddenDeathSeconds = FMath::Clamp(InSuddenDeathSeconds, 5.f, 600.f);
	WarmupSeconds = FMath::Clamp(InWarmupSeconds, MinPhaseSeconds, 300.f);
	RespawnDelaySeconds = FMath::Clamp(InRespawnSeconds, 0.f, 60.f);
	KillCreditWindowSeconds = FMath::Clamp(InKillCreditWindowSeconds, 0.f, 30.f);

	if (ABRGameState* MutableGS = GetBRGameState())
	{
		MutableGS->ServerSetScoreLimit(ScoreLimit);
	}
}

uint8 ABRGameMode::ResolveTeamId(const APlayerState* Player) const
{
	if (!Player)
	{
		return BRMatch::InvalidTeamId;
	}

	APlayerState* MutablePlayer = const_cast<APlayerState*>(Player);

	if (const IGenericTeamAgentInterface* Agent = Cast<IGenericTeamAgentInterface>(MutablePlayer))
	{
		const uint8 TeamId = Agent->GetGenericTeamId().GetId();
		return (TeamId == FGenericTeamId::NoTeam.GetId()) ? BRMatch::InvalidTeamId : TeamId;
	}

	if (const AController* OwningController = MutablePlayer->GetOwningController())
	{
		if (const IGenericTeamAgentInterface* ControllerAgent = Cast<IGenericTeamAgentInterface>(const_cast<AController*>(OwningController)))
		{
			const uint8 TeamId = ControllerAgent->GetGenericTeamId().GetId();
			return (TeamId == FGenericTeamId::NoTeam.GetId()) ? BRMatch::InvalidTeamId : TeamId;
		}
	}

	return BRMatch::InvalidTeamId;
}

APlayerState* ABRGameMode::ResolvePlayerState(const AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}

	if (const APlayerState* AsPlayerState = Cast<APlayerState>(Actor))
	{
		return const_cast<APlayerState*>(AsPlayerState);
	}

	if (const APawn* AsPawn = Cast<APawn>(Actor))
	{
		return AsPawn->GetPlayerState();
	}

	if (const AController* AsController = Cast<AController>(Actor))
	{
		return AsController->PlayerState;
	}

	return nullptr;
}

ABRGameState* ABRGameMode::GetBRGameState() const
{
	return GetGameState<ABRGameState>();
}

float ABRGameMode::GetServerTime() const
{
	if (const AGameStateBase* GS = GetGameState<AGameStateBase>())
	{
		return GS->GetServerWorldTimeSeconds();
	}

	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.f;
}
