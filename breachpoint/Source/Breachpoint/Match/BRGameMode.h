#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "GameplayTagContainer.h"
#include "Match/BRGameState.h"

#include "BRGameMode.generated.h"

class AActor;
class AController;
class APlayerState;
class UAbilitySystemComponent;
struct FGameplayEventData;

UCLASS()
class BREACHPOINT_API ABRGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ABRGameMode();

	virtual void InitGameState() override;
	virtual void OnPostLogin(AController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void HandleMatchHasStarted() override;
	virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	void NotifyDamageDealt(APlayerState* Attacker, APlayerState* Victim, float Amount, FGameplayTag DamageTag);

	void HandleDeathEvent(APlayerState* Victim, APlayerState* EventInstigator, FGameplayTag CauseTag);

	void RegisterCombatant(AController* Combatant);

	void UnregisterCombatant(AController* Combatant);

	bool RequestRespawn(AController* Requester);

	void ApplyMatchRules(int32 InScoreLimit, float InMatchSeconds, float InSuddenDeathSeconds,
		float InWarmupSeconds, float InRespawnSeconds, float InKillCreditWindowSeconds);

	DECLARE_MULTICAST_DELEGATE_ThreeParams(FBRPlayerKilledSignature, APlayerState*, APlayerState*, const TArray<APlayerState*>&);
	FBRPlayerKilledSignature OnPlayerKilled;

	EBRMatchPhase GetMatchPhase() const;

protected:

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Match")
	int32 ScoreLimit = 25;

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Match")
	float MatchDurationSeconds = 480.f;

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Match")
	float SuddenDeathSeconds = 60.f;

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Match")
	float WarmupSeconds = 15.f;

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Match")
	float PostMatchSeconds = 15.f;

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Match")
	float RespawnDelaySeconds = 5.f;

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Match")
	float KillCreditWindowSeconds = 5.f;

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Match")
	bool bAllowRespawnInSuddenDeath = true;

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Match")
	bool bFriendlyFirePenalizesKillersTeam = true;

	UPROPERTY(EditDefaultsOnly, Category = "Breachpoint|Match")
	float MaxPlausibleSingleDamage = 10000.f;

	void EnterPhase(EBRMatchPhase NewPhase);

	void HandleWarmupElapsed();
	void HandleMatchClockExpired();
	void HandleSuddenDeathExpired();
	void HandlePostMatchElapsed();

	void ScheduleWinCheck();
	void EvaluateWinConditions();

	void EnterPostMatch(uint8 WinningTeamId);

	virtual void RequestMatchTeardown();

	APlayerState* ResolveKiller(APlayerState* Victim, APlayerState* EventInstigator) const;

	void CollectAssists(APlayerState* Victim, APlayerState* Killer, TArray<APlayerState*>& OutAssists) const;

	void SendKillEvent(APlayerState* Recipient, APlayerState* Victim, float Magnitude, FGameplayTag CauseTag) const;

	virtual uint8 ResolveTeamId(const APlayerState* Player) const;

	void StartRespawnTimer(AController* Victim);
	void HandleRespawnTimerElapsed(TWeakObjectPtr<AController> WeakVictim);
	bool CanRespawnNow(AController* Candidate) const;

	virtual void GatherSpawnCandidates(TArray<AActor*>& OutCandidates) const;

	float ScoreSpawnCandidate(const AActor* Candidate, AController* ForPlayer) const;

	struct FBRDamageRecord
	{
		TWeakObjectPtr<APlayerState> Attacker;
		float Amount = 0.f;
		float ServerTime = 0.f;
		FGameplayTag DamageTag;
	};

	void OnDeathGameplayEvent(const FGameplayEventData* Payload);

	static APlayerState* ResolvePlayerState(const AActor* Actor);

	void PruneDamageLedger(APlayerState* Victim);

	ABRGameState* GetBRGameState() const;

	float GetServerTime() const;

	TMap<TWeakObjectPtr<APlayerState>, TArray<FBRDamageRecord>> DamageLedger;

	TArray<float> TeamDamageDealt;

	TSet<TWeakObjectPtr<APlayerState>> PendingRespawnPlayers;

	TMap<TWeakObjectPtr<AController>, float> EarliestRespawnServerTime;

	TMap<TWeakObjectPtr<AController>, FTimerHandle> RespawnTimers;

	TMap<TWeakObjectPtr<UAbilitySystemComponent>, FDelegateHandle> DeathListenerHandles;

	TMap<TWeakObjectPtr<AActor>, float> SpawnPointLastUsed;

	FTimerHandle WarmupTimerHandle;
	FTimerHandle MatchClockTimerHandle;
	FTimerHandle SuddenDeathTimerHandle;
	FTimerHandle PostMatchTimerHandle;

	bool bWinCheckPending = false;

	bool bMatchDecided = false;
};
