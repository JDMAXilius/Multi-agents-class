#pragma once

#include "CoreMinimal.h"
#include "Core/OSGameState.h"
#include "OSGameState_Domination.generated.h"

class AOSControlPoint;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTeamObjectiveScoreChanged, int32, TeamIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnControlPointsChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDominationMatchResult, int32, WinningTeamIndex);

UCLASS()
class ONSIGHT_API AOSGameState_Domination : public AOSGameState
{
	GENERATED_BODY()

public:
	// --- Control Point Registry ---

	UPROPERTY()
	TMap<FName, TObjectPtr<AOSControlPoint>> ControlPointByName;

	UPROPERTY(ReplicatedUsing=OnRep_ControlPoints, BlueprintReadOnly, Category="Domination")
	TArray<TObjectPtr<AOSControlPoint>> ControlPoints;

	void RegisterPoint(AOSControlPoint* Point, FName Name);
	void UnregisterPoint(FName Name);

	UPROPERTY(BlueprintAssignable, Category="Domination")
	FOnControlPointsChanged OnControlPointsChanged;

	// --- Team Objective Scores ---

	UPROPERTY(ReplicatedUsing=OnRep_TeamObjectiveScores, BlueprintReadOnly, Category="Domination")
	TArray<int32> TeamObjectiveScores;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Domination")
	int32 DomScoreLimit = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Domination")
	int32 DomTimeLimitSeconds = 0;

	UPROPERTY(BlueprintAssignable, Category="Domination")
	FOnTeamObjectiveScoreChanged OnTeamObjectiveScoreChanged;

	int32 AddTeamScore(int32 TeamIndex, int32 Amount);

	UFUNCTION(BlueprintPure, Category="Domination")
	int32 GetTeamScore(int32 TeamIndex) const;

	// --- Occupancy queries (aggregate across all points) ---

	/** Total players on any control point (all teams combined). */
	UFUNCTION(BlueprintPure, Category="Domination|Occupancy")
	int32 GetTotalOccupantsOnObjectives() const;

	/** Players from a specific team on any control point. */
	UFUNCTION(BlueprintPure, Category="Domination|Occupancy")
	int32 GetTeamOccupantsOnObjectives(int32 TeamIndex) const;

	/** Number of control points that have at least one player from a given team. */
	UFUNCTION(BlueprintPure, Category="Domination|Occupancy")
	int32 GetOccupiedPointCount(int32 TeamIndex) const;

	// --- Match Result ---
	// WinningTeamIndex lives on parent AOSGameState (replicated but no OnRep).

	/** Fires on ALL machines when the domination match ends. Bind for victory/defeat UI. */
	UPROPERTY(BlueprintAssignable, Category="Domination")
	FOnDominationMatchResult OnDomMatchResult;

	/** Broadcasts OnDomMatchResult to all clients. Called by GameMode at match end. */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_DomMatchResult(int32 InWinningTeamIndex);

private:
	void EnsureScoreArraySize();

	UFUNCTION()
	void OnRep_ControlPoints();

	UFUNCTION()
	void OnRep_TeamObjectiveScores();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
