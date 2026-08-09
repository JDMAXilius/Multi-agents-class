// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Components/OSAudioComponent.h"
#include "Actors/OSControlPoint.h"

class AOSPlayerState;
#include "OSAnnouncerSubsystem.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAnnouncerSimpleEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAnnouncerControlPointEvent, EAudioTeamRelationship, TeamRelationship, FName, PointName);


UCLASS()
class ONSIGHT_API UOSAnnouncerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
	
public:
	virtual void OnWorldBeginPlay(UWorld& InWorld)  override;
	virtual void Deinitialize() override;
	
	// --- Delegates Blueprints bind to --//
	UPROPERTY(BlueprintAssignable, Category = "Announcer")
	FOnAnnouncerSimpleEvent OnOneMinuteWarning;
	
	UPROPERTY(BlueprintAssignable, Category = "Announcer")
	FOnAnnouncerControlPointEvent OnControlPointCaptured;
	
	UPROPERTY(BlueprintAssignable, Category = "Announcer")
	FOnAnnouncerControlPointEvent OnControlPointNeutralized;
	
	UPROPERTY(BlueprintAssignable, Category = "Announcer")
	FOnAnnouncerControlPointEvent OnControlPointContested;
	
	UPROPERTY(BlueprintAssignable, Category = "Announcer")
	FOnAnnouncerControlPointEvent OnControlPointCapturing;

	UPROPERTY(BlueprintAssignable, Category = "Announcer")
	FOnAnnouncerControlPointEvent OnControlPointNeutralizing;

	//-- Blueprint Utility ---//
	UFUNCTION(BlueprintPure, Category = "Announcer")
	EAudioTeamRelationship GetTeamIndexOccupant(AOSPlayerState* Occupant) const;
	
private:
	TSet<TWeakObjectPtr<AOSControlPoint>> BoundControlPoints;
	
	void BindToGameState();
	void BindToControlPoint(AOSControlPoint* Point);
	
	UFUNCTION()
	void OnOneMinuteWarningReceived();
	
	UFUNCTION()
	void OnControlPointCapturedReceived(AOSControlPoint* Point, int32 CapturingTeamIndex, int32 PreviousOwnerTeamIndex);
	
	UFUNCTION()
	void OnControlPointNeutralizedReceived(AOSControlPoint* Point, int32 PreviousOwnerTeamIndex, int32 NeutralizingTeamIndex);
	
	UFUNCTION()
	void OnControlPointStateChanged(AOSControlPoint* Point, EControlPointState PreviousState, EControlPointState CurrentState,int32 OwningTeam, int32 CapturingTeam);
	
	UFUNCTION()
	void OnControlPointListChanged();
};
