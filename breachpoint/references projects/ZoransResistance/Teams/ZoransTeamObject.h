// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "Data/TeamData.h"
#include "UObject/Object.h"
#include "ZoransTeamObject.generated.h"

class AZoransGameState;
class AZoransPlayerState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTeamStatsChangeDelegate, UZoransTeamObject*, TeamObject, EScoreStat, ScoreStat);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAssignedPlayersChangeDelegate);

UCLASS(NotBlueprintable)
class ZORANSRESISTANCE_API UZoransTeamObject final : public UObject
{
	GENERATED_BODY()

public:
	
	UPROPERTY(BlueprintReadOnly, Replicated)
	int32 TeamIndex = -1;
	
	UPROPERTY(BlueprintReadOnly, Replicated)
	TArray<int32> FriendlyTeams;

	UPROPERTY(BlueprintReadOnly, Replicated)
	TArray<AZoransPlayerState*> AssignedPlayers;

	UPROPERTY(BlueprintAssignable)
	FOnAssignedPlayersChangeDelegate OnAssignedPlayersChanged;
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = "OnRep_TeamKills")
	int32 TeamKills = 0;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = "OnRep_TeamDeaths")
	int32 TeamDeaths = 0;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = "OnRep_TeamAssists")
	int32 TeamAssists = 0;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = "OnRep_TeamObjectives")
	int32 TeamObjectives = 0;

	UPROPERTY(BlueprintAssignable)
	FTeamStatsChangeDelegate OnTeamStatsChange;

	void AssignPlayerToTeam(AZoransPlayerState* InPlayerState);

	void RemoveAssignedPlayerFromTeam(AZoransPlayerState* InPlayerState);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void UpdateKillScore(int32 NewScore);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void UpdateDeathScore(int32 NewScore);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void UpdateAssistScore(int32 NewScore);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void UpdateObjectiveScore(int32 NewScore);
	
	virtual UWorld* GetWorld() const override;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	AActor* GetOwningActor() const;

	virtual bool CallRemoteFunction(UFunction* Function, void* Parms, FOutParmRec* OutParms, FFrame* Stack) override;

protected:
	
	virtual bool IsSupportedForNetworking() const override { return true; }

	UFUNCTION()
	void Internal_TeamAssignmentsUpdated();
	
	UFUNCTION()
	void OnRep_TeamKills();

	UFUNCTION()
	void OnRep_TeamDeaths();

	UFUNCTION()
	void OnRep_TeamAssists();

	UFUNCTION()
	void OnRep_TeamObjectives();
};