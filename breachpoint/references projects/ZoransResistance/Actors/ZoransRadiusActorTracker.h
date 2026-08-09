// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Actor.h"
#include "ZoransRadiusActorTracker.generated.h"

UCLASS()
class ZORANSRESISTANCE_API AZoransRadiusActorTracker : public AActor
{
	GENERATED_BODY()

public:
	AZoransRadiusActorTracker();

	bool IsOwnerWithinRadius() const { return bOwnerWithinRadius; }

	UFUNCTION(BlueprintCallable, Category = "Detection")
	TArray<AActor*> GetFriendlyActors() { return FriendlyActors; }
	
	UFUNCTION(BlueprintCallable, Category = "Detection")
	TArray<AActor*> GetEnemyActors() { return EnemyActors; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(BlueprintReadOnly, Meta = (ExposeOnSpawn = true), Category = "Detection")
	AActor* OwningActorReference = nullptr;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Detection")
	bool bTrackFriendly = false;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Detection")
	bool bTrackEnemy = true;

	UPROPERTY(BlueprintReadWrite, Meta = (ExposeOnSpawn = true), Category = "Detection")
	int32 OwningTeam = -1;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Detection")
	USphereComponent* DetectionSphere = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Detection")
	TArray<AActor*> FriendlyActors;

	UPROPERTY(BlueprintReadOnly, Category = "Detection")
	TArray<AActor*> EnemyActors;

	UPROPERTY()
	TArray<AActor*> IgnoredActors;

	UFUNCTION(BlueprintImplementableEvent, Category = "Detection")
	void FriendlyActorsEmpty();
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Detection")
	void FriendlyActorsNotEmpty();

	UFUNCTION(BlueprintImplementableEvent, Category = "Detection")
	void EnemyActorsEmpty();

	UFUNCTION(BlueprintImplementableEvent, Category = "Detection")
	void EnemyActorsNotEmpty();
	
	void Internal_OwnerEnteredRadius(AActor* OwningActor);

	UFUNCTION(BlueprintImplementableEvent, Category = "Detection")
	void OwnerEnteredRadius(AActor* OwningActor);

	void Internal_OwnerExitedRadius(AActor* OwningActor);

	UFUNCTION(BlueprintImplementableEvent, Category = "Detection")
	void OwnerExitedRadius(AActor* OwningActor);

	UFUNCTION(BlueprintImplementableEvent, Category = "Detection")
	void NewActorDetected(AActor* InActor, const bool bFriendly);

	UFUNCTION(BlueprintImplementableEvent, Category = "Detection")
	void ActorDetectionLost(AActor* InActor, const bool bFriendly);

	UFUNCTION(BlueprintNativeEvent, Category = "Detection")
	void OverrideOwningTeam();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Detection")
	FVector GetTraceStartLocation() const;

	UFUNCTION()
	void DetectionSphereBeginOverlap(AActor* OverlappedActor, AActor* OtherActor);

	UFUNCTION()
	void DetectionSphereEndOverlap(AActor* OverlappedActor, AActor* OtherActor);

	UFUNCTION(BlueprintCallable, Category = "Detection")
	AActor* GetClosestActor(const bool bFriendly = false, const bool bLineOfSight = true, const float InFilterByAngle = -1.f);

	UFUNCTION(BlueprintCallable, Category = "Detection")
	TArray<AActor*> GetFilteredActors(const bool bFriendly = false, const bool bLineOfSight = false, const float InFilterByAngle = -1.f);

	bool HasLineOfSight(const FVector& InTargetVector) const;
	bool FilterByAngle(const FVector& InTargetVector, const float InAngle) const;
	
	bool bOwnerWithinRadius = false;
};
