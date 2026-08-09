// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "Data/ActorData.h"
#include "ZoransControlPointBase.generated.h"

class AZoransCharacterBase;
class USphereComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOwningTeamIndexChangedDelagate, int32, NewOwningTeamIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnControlPointIndexChangedDelegate, int32, NewIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnControlPointControlStateChangedDelegate, EActorControlState, NewControlState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCaptureProgressChangedDelegate, float, NewProgress);

UCLASS()
class ZORANSRESISTANCE_API AZoransControlPointBase : public AActor
{
	GENERATED_BODY()

public:

	AZoransControlPointBase();

	UFUNCTION(BlueprintCallable, BlueprintPure)
	FName GetControlPointName() const { return  ControlPointName; }

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float CaptureTime = 5.0f;
	
	UPROPERTY(BlueprintAssignable)
	FOnOwningTeamIndexChangedDelagate OnOwningIndexTeamChanged;

	UPROPERTY(BlueprintAssignable)
	FOnControlPointIndexChangedDelegate OnControlPointIndexChanged;

	UPROPERTY(BlueprintAssignable)
	FOnControlPointControlStateChangedDelegate OnControlPointControlStateChanged;

	UPROPERTY(BlueprintAssignable)
	FOnCaptureProgressChangedDelegate OnCaptureProgressChanged;
	
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void SetOwningTeamIndex(int32 InIndex);

	UFUNCTION(BlueprintCallable, BlueprintPure)
	int32 GetOwningTeamIndex() const { return OwningTeamIndex; }

	UFUNCTION(BlueprintCallable, BlueprintPure)
	int32 GetDominantTeamIndex() const { return DominantTeamIndex; }
	
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void SetControlPointIndex(int32 InIndex);

	UFUNCTION(BlueprintCallable, BlueprintPure)
	int32 GetControlPointIndex() const { return ControlPointIndex; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void SetControlPointControlState(EActorControlState NewState);

	UFUNCTION(BlueprintCallable, BlueprintPure)
	EActorControlState GetControlPointControlState() const { return ControlState; }
	
	UFUNCTION(BlueprintCallable, BlueprintPure)
	float GetCaptureProgress() const { return CaptureProgress; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void SetCaptureProgress(const float InProgress = 0.5f);
	
	UFUNCTION(BlueprintCallable, BlueprintPure)
	TArray<AZoransCharacterBase*> GetOverlappedCharacters(bool bCheckIfAlive) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void SetDominantTeam(const int32 InTeamIndex);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void AdjustCaptureProgress(const bool bGaining);

protected:

	float IntervalRate = 0.33f;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FName ControlPointName = "Control Point";

	UPROPERTY(BlueprintReadOnly, EditAnywhere, ReplicatedUsing = "OnRep_ControlPointIndex")
	int32 ControlPointIndex = 0;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = "OnRep_CaptureProgress")
	float CaptureProgress = 1.f;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void StartOverlapTimer();

	UFUNCTION(BlueprintImplementableEvent)
	void OnCheckOverlap();
	
	UFUNCTION()
	void OnRep_ControlPointIndex();

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	USceneComponent* SceneRoot = nullptr;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	UMeshComponent* Mesh = nullptr;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	USphereComponent* CollisionShape = nullptr;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = "OnRep_OwningTeamIndex")
	int32 OwningTeamIndex = -1;

	UPROPERTY()
	int32 DominantTeamIndex = -1;

	UFUNCTION()
	void OnRep_OwningTeamIndex();

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = "OnRep_ControlState")
	EActorControlState ControlState = EActorControlState::Uncontested;

	UFUNCTION()
	void OnRep_ControlState();

	UFUNCTION()
	void OnRep_CaptureProgress();

	FTimerHandle OverlapCheckTimer;
};
