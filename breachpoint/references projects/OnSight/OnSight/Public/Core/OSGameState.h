#pragma once
// Game state: killfeed, match leader, win condition. TDM: team kills/deaths, match over, lowest-population team.

#include "CoreMinimal.h"
#include "OSPlayerState.h"
#include "Data/OSHitDamageContext.h"
#include "GameFramework/GameStateBase.h"
#include "UI/Layers/OSBaseLayerWidget.h"
#include "OSGameState.generated.h"


class UOSWinConditionLayerWidget;
class UOSBaseLayerWidget;
class AOSPlayerState;

USTRUCT(BlueprintType)
struct FOSKillfeedEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FOSDeathEventInfo DeathInfo;
	UPROPERTY(BlueprintReadOnly) int32 ID = 0;
};

UENUM(BlueprintType)
enum class FOSWinConditionType : uint8
{
	NONE UMETA(DisplayName="NONE"),
	KILLS UMETA(DisplayName="KILLS"),
	SCORE UMETA(DisplayName="SCORE")
};

USTRUCT(BlueprintType)
struct FOSMatchResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bGameEnded = false;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<AOSPlayerState> Winner = nullptr;
	
	UPROPERTY(BlueprintReadOnly)
	int32 WinnerValue = -1;
};

USTRUCT(BlueprintType)
struct FOSWinConditionEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FOSWinConditionType StatType = FOSWinConditionType::NONE;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 StatTarget = 0;
	
	FText GetStatLabel() const
	{
		const UEnum* Enum = StaticEnum<FOSWinConditionType>();
		return Enum
			? Enum->GetDisplayNameTextByValue(static_cast<int64>(StatType))
			: FText::FromString("UNKNOWN");
	}
};

USTRUCT(BlueprintType)
struct FOSMatchLeader
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<AOSPlayerState> Leader = nullptr;

	UPROPERTY(BlueprintReadOnly)
	int32 LeaderValue = -1;
	
	FText GetPlayerLabel() const
	{
		if (!Leader) return FText::GetEmpty();
		return FText::FromString(Leader->GetPlayerName());
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchTimeUpdated, int32, NewMatchTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnKillfeedEntryAdded, const FOSKillfeedEntry&, Entry);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWinConditionChanged, const FOSWinConditionEntry&, Entry);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMatchResultChanged, const FOSMatchResult&);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchResultChanged_BP, const FOSMatchResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMatchLeaderChanged, const FOSMatchLeader&, Leader, const int32, Target);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPlayerArrayChanged, AOSPlayerState*, bool);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerArrayChanged_BP, AOSPlayerState*, PlayerState, bool, bAdded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTeamStatsChanged, int32, TeamIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnOneMinuteWarning);




UCLASS()
class ONSIGHT_API AOSGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AOSGameState(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(ReplicatedUsing=OnRep_MatchTime, BlueprintReadOnly, Category = "Match")
	int32 MatchTime = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
	bool bMatchInProgress = false;

	void StartMatchTimer();
	void InitMatchTimer(int32 DurationSeconds);
	void StopMatchTimer();

	UPROPERTY(ReplicatedUsing=OnRep_Killfeed)
	TArray<FOSKillfeedEntry> Killfeed;

	UPROPERTY(BlueprintAssignable)
	FOnMatchTimeUpdated OnMatchTimeUpdated;

	UPROPERTY(BlueprintAssignable)
	FOnKillfeedEntryAdded OnKillfeedEntryAdded;
	UPROPERTY(BlueprintAssignable)
	FOnWinConditionChanged OnWinConditionChanged;
	
	UPROPERTY(EditDefaultsOnly, ReplicatedUsing=OnRep_WinCondition, BlueprintReadOnly, Category = "Match")
	FOSWinConditionEntry WinConditionEntry;
	
	UPROPERTY(BlueprintAssignable, Category = "Match")
	FOnOneMinuteWarning OnOneMinuteWarning;
	
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_OneMinuteWarning();

	UFUNCTION()
	void OnRep_MatchTime();

	UFUNCTION()
	void OnRep_Killfeed();
	
	UFUNCTION()
	void OnRep_WinCondition();

	void AddKillfeed_Server(const FOSDeathEventInfo& Info);
	void UpdateWinCondition_Server(FOSWinConditionType type, const int32 target);
	
	UFUNCTION()
	virtual void UpdateMatchLeader();
	
	UPROPERTY(ReplicatedUsing=OnRep_MatchResult)
	FOSMatchResult MatchResult;
	
	UPROPERTY(ReplicatedUsing=OnRep_MatchLeader, BlueprintReadOnly, Category="Match")
	FOSMatchLeader MatchLeader;
	
	UPROPERTY(BlueprintAssignable)
	FOnMatchLeaderChanged OnMatchLeaderChanged;

	/** Mode UI layer classes (copied from GameMode in InitGameState). Used by HUD to build mode-specific layers. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="UI")
	TArray<TSubclassOf<UOSBaseLayerWidget>> ModeLayerClasses;

	FOnMatchResultChanged OnMatchResultChanged;
	UPROPERTY(BlueprintAssignable, Category="Match")
	FOnMatchResultChanged_BP OnMatchResultChanged_BP;

	FOnPlayerArrayChanged OnPlayerArrayChanged;
	UPROPERTY(BlueprintAssignable, Category="Match")
	FOnPlayerArrayChanged_BP OnPlayerArrayChanged_BP;

	UPROPERTY(BlueprintAssignable, Category="Match")
	FOnTeamStatsChanged OnTeamStatsChanged;
		
	// ------------------------------------------------------------------
	// TDM: team scores, match over, team balance
	// ------------------------------------------------------------------
	/** Number of teams (e.g. 2 for classic TDM). */
	static constexpr int32 MaxTDMTeams = 2;

	UPROPERTY(ReplicatedUsing = OnRep_TeamKills, BlueprintReadOnly, Category = "TDM")
	TArray<int32> TeamKills;

	UPROPERTY(ReplicatedUsing = OnRep_TeamDeaths, BlueprintReadOnly, Category = "TDM")
	TArray<int32> TeamDeaths;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "TDM")
	bool bMatchOver = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "TDM")
	int32 WinningTeamIndex = -1;

	/** Replicated from TDM GameMode MatchConfig so clients/HUD can show kill target and time limit. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "TDM")
	int32 TDMKillLimit = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "TDM")
	int32 TDMTimeLimitSeconds = 0;

	/** Team index with fewest players (for balancing new joins). */
	int32 GetLowestPopulationTeamIndex() const;
	int32 GetTeamKills(int32 TeamIndex) const;
	int32 GetTeamDeaths(int32 TeamIndex) const;
	bool IsMatchOver() const { return bMatchOver; }

	void AddTeamKill(int32 TeamIndex);
	void AddTeamDeath(int32 TeamIndex);
	void NotifyMatchWon(int32 TeamIndex, int32 OverrideWinnerValue = INDEX_NONE);

	UFUNCTION()
	void OnRep_TeamKills();
	UFUNCTION()
	void OnRep_TeamDeaths();
		
	virtual void TryUpdateMatchLeader_Server(AOSPlayerState* Candidate);
	
	UFUNCTION()
	void OnRep_MatchResult();

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_MatchLeader();
	
	virtual void AddPlayerState(APlayerState* PlayerState) override;
	virtual void RemovePlayerState(APlayerState* PlayerState) override;
	
private:
	void UpdateMatchTime();
	void NotifyTimeExpired();
	void EnsureTeamArraysSize();

	bool bCountdownEnabled = false;

	FTimerHandle MatchTimerHandle;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	UPROPERTY(Transient)
	int32 LastSeenID = 0;

	int32 NextID = 1;
};

