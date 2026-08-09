// Copyrigguilty taemin albumht Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "ZoransGameModeLogic.h"
#include "GameFramework/GameState.h"
#include "ZoransResistance/Widgets/Data/WidgetData.h"
#include "ZoransResistance/Teams/Data/TeamData.h"
#include "ZoransResistance/Game/ZoransGameMode.h"
#include "ZoransResistance/Game/Data/ZoransGameModeData.h"
#include "ZoransResistance/Teams/ZoransTeamObject.h"
#include "ZoransGameState.generated.h"

class UZoransGameModeLogic;
class AZoransGameMode;
class UZoransTeamObject;
class UZoransGameState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGamePhaseChangeDelegate, EGamePhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTeamRegistrationFinishedDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTeamSystemInitializedDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRegisteredTeamStatsUpdatedDelegate, UZoransTeamObject*, TeamObject, EScoreStat, ScoreStat);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FOnKillFeedEventDelegate, const APlayerState*, SourcePlayerState, const APlayerState*, TargetPlayerState, const UTexture2D*, KillfeedIcon, const bool, bWeakPoint, const bool, bFinalKill);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerJoinDelegate, APlayerState*, InPlayer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerLeaveDelegate, APlayerState*, InPlayer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInitialTeamAssignmentsCompletedDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTeamAssignmentsUpdatedDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGameEventNotification, const FGameEventAnnouncement&, GameEvent);

UCLASS()
class ZORANSRESISTANCE_API AZoransGameState : public AGameState
{
	GENERATED_BODY()

public:

	AZoransGameState();

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Classes")
	TSubclassOf<APawn> DefaultPlayerClass = nullptr;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Classes", meta = (Displayname = "Default AI Class"))
	TSubclassOf<APawn> DefaultAIClass = nullptr;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameModeOverride DefaultGameModeOverride = EGameModeOverride::Default;
	
	virtual void AddPlayerState(APlayerState* PlayerState) override;

	virtual void RemovePlayerState(APlayerState* PlayerState) override;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	UDataTable* ExperienceDataTable = nullptr;

	UFUNCTION(BlueprintCallable)
	EGameModeOverride GetGameModeOverride() const { return GameModeOverride; }

	UPROPERTY(EditDefaultsOnly, Category = "Game Mode|Logic")
	TSubclassOf<UZoransGameModeLogic> DefaultGameModeLogic;
	
	UPROPERTY(EditDefaultsOnly, Category = "Game Mode|Logic")
	TMap<EGameModeOverride, TSubclassOf<UZoransGameModeLogic>> GameModeLogicMap;
	
	UPROPERTY(BlueprintReadOnly, Category = "Game Mode|Logic")
	UZoransGameModeLogic* GameModeLogic;

	UFUNCTION(BlueprintCallable)
	FGameModeSettings GetGameModeSettings() const { return GameModeSettings; }
	
	UPROPERTY(BlueprintAssignable, Category = "Game Phase")
	FGamePhaseChangeDelegate GamePhaseChangeDelegate;

	UPROPERTY(BlueprintAssignable)
	FOnPlayerJoinDelegate OnPlayerJoinDelegate;
	
	UFUNCTION(NetMulticast, Reliable)
	void Internal_OnPlayerJoin(APlayerState* InPlayerState);

	UPROPERTY(BlueprintAssignable)
	FOnPlayerLeaveDelegate OnPlayerLeaveDelegate;
	
	UFUNCTION(NetMulticast, Reliable)
	void Internal_OnPlayerLeave(APlayerState* InPlayerState);
	
	UFUNCTION(BlueprintImplementableEvent)
	void OnGamePhaseChanged(const EGamePhase NewPhase);

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Game Phase")
	bool bSetPhaseAutomatically = true;
	
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Game Phase")
	void SetGamePhase(const EGamePhase NewPhase, bool UseCustomTime = false, float TimeOverride = 1.0f);
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TArray<FTeamDefinition> TeamDefinitions;
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = "OnRep_RegisteredTeams")
	TArray<UZoransTeamObject*> RegisteredTeams;

	UPROPERTY(BlueprintReadOnly, Replicated)
	TObjectPtr<UZoransTeamObject> RegisteredFreeForAllTeam;

	UFUNCTION(BlueprintCallable)
	UZoransTeamObject* GetTeamObjectByTeamIndex(int32 InIndex) const;

	UPROPERTY(BlueprintAssignable)
	FOnTeamRegistrationFinishedDelegate OnTeamRegistrationFinished;

	UPROPERTY(BlueprintAssignable)
	FOnInitialTeamAssignmentsCompletedDelegate OnInitialTeamAssignmentsCompleted;

	UPROPERTY(BlueprintAssignable)
	FOnTeamAssignmentsUpdatedDelegate OnTeamAssignmentsUpdated;

	UPROPERTY(BlueprintAssignable)
	FGameEventNotification GameEventNotification;
	
	void AssignPlayerStateToTeam(AZoransPlayerState* InPlayerState, int32 TeamIndex);
	
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void UpdatePlayerScore(AZoransPlayerState* InPlayerState, EScoreStat StatToChange, int32 Amount, EStatChangeOperation StatChangeOperation, const bool bContributeToTeamScore = true);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void UpdateTeamScore(int32 InTeamIndex, EScoreStat StatToChange, int32 Amount, EStatChangeOperation StatChangeOperation);
	
	UFUNCTION()
	void Internal_OnRegisteredTeamStatsChanged(UZoransTeamObject* TeamObject, EScoreStat ChangedStat);

	UPROPERTY(BlueprintAssignable)
	FRegisteredTeamStatsUpdatedDelegate OnRegisteredTeamStatsUpdated;

	UFUNCTION(BlueprintAuthorityOnly, Category = "Team")
	void AssignCurrentPlayersToTeams();

	UFUNCTION(NetMulticast, Reliable)
	void Internal_OnInitialTeamAssignmentsFinished();

	UFUNCTION(NetMulticast, Reliable)
	void Internal_OnTeamAssignmentsUpdated();
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Team")
	void TeamStatsUpdated(UZoransTeamObject* TeamObject, EScoreStat ChangedStat);

	UFUNCTION(NetMulticast, Reliable)
	void HandleKillfeedEvent(APlayerState* SourcePlayerState, const APlayerState* TargetPlayerState, const UTexture2D* KillfeedIcon, const bool bWeakPoint = false);

	UPROPERTY(BlueprintAssignable)
	FOnKillFeedEventDelegate OnKillfeedEvent;

	static int32 GetNumberOfBotsOnTeam(const UZoransTeamObject* InTeamObject);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Team")
	bool bUseTeamSpawns = false;
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Classes")
	TSubclassOf<APawn> AISpawnerClass = nullptr;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	float GetPhaseTimeRemaining() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Team")
	void SetSpawnAI(const bool bShouldSpawnAI) { bSpawnAI = bShouldSpawnAI; }

	bool DoesTeamHaveEmptySlot(const UZoransTeamObject* InTeamObject) const;

	UZoransTeamObject* GetLowestPopulationTeam(const bool bIncludeBots = false);

	UZoransTeamObject* GetTeamWithMostBots();
	
	bool ExitWaitingPhase() const;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Team")
	bool bAutoAssignToTeams = false;

	bool bSpawnAI = false;

	bool bBotsInit = false;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	EGamePhase GetCurrentPhase() const { return CurrentGamePhase; }

	UFUNCTION(BlueprintCallable, BlueprintPure)
	AZoransGameMode* GetZoransGameMode() const { return ZoransGameMode.Get(); }

	UPROPERTY(BlueprintReadOnly, Replicated)
	bool bInitialTeamAssignmentsFinished = false;
	
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void CleanupGameState();
	
	UFUNCTION(BlueprintCallable, BlueprintPure)
	FExperienceData GetExperienceData() const { return ExperienceData; }

	UFUNCTION(BlueprintCallable, BlueprintPure)
	FGameModeInfo GetGameModeInfo() const { return GameModeInfo; }

	UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable, Category = "Game Event")
	void BroadcastGameEvent(const FGameEventAnnouncement& InGameEvent);

	UFUNCTION(NetMulticast, Reliable, Category = "Game Event")
	void MTC_BroadcastGameEvent(const FGameEventAnnouncement& InGameEvent);

	UFUNCTION(Exec)
	void DebugPrintTags() const;
	
protected:

	UPROPERTY(Replicated)
	EGameModeOverride GameModeOverride = EGameModeOverride::Default;
	
	UPROPERTY(Replicated)
	FGameModeSettings GameModeSettings;
	
	UPROPERTY(BlueprintReadOnly)
	int32 FinalScoreTarget = 50;

	UPROPERTY(BlueprintReadOnly)
	bool bTeamRegistrationFinished = false;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category="Game Mode")
	TMap<EGameModeOverride, bool> CanRouletteInto;
		
	UPROPERTY(Replicated)
	float PhaseEndTime = 0.0f;
	
	void CheckForPhaseChange();
	
	virtual void Tick(float DeltaSeconds) override;
	
	static int32 CalculateScoreChange(int32 CurrentAmount, int32 ChangeAmount, EStatChangeOperation StatChangeOperation);
	
	void UpdateScoreForTeam(int32 TeamIndex, EScoreStat StatToChange, int32 Amount, EStatChangeOperation StatChangeOperation) const;

	virtual void BeginPlay() override;
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = "OnRep_GamePhaseChanged")
	EGamePhase CurrentGamePhase = EGamePhase::Loading;

	TWeakObjectPtr<AZoransGameMode> ZoransGameMode;

	UPROPERTY()
	FExperienceData ExperienceData;

	UPROPERTY(Replicated)
	FGameModeInfo GameModeInfo;

	UFUNCTION()
	void OnRep_GamePhaseChanged();

	UFUNCTION()
	void OnRep_RegisteredTeams();
};