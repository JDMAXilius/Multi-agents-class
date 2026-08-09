// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "ZoransResistance/Game/Data/StaticGameData.h"
#include "ImpaleActorBase.generated.h"

class UCapsuleComponent;

DECLARE_DELEGATE(FImpaleTraceDelegate)

UCLASS(Abstract)
class ZORANSRESISTANCE_API AImpaleActorBase : public AActor
{
	GENERATED_BODY()

public:
	AImpaleActorBase();

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(BlueprintReadOnly, Meta = (ExposeOnSpawn = true), Category = "Impale Actor")
	USkinnedMeshComponent* ImpaledMesh = nullptr;

	UPROPERTY(BlueprintReadWrite, Meta = (ExposeOnSpawn = true), Category = "Impale Actor")
	FName HitBone = NAME_None;

	UPROPERTY(BlueprintReadWrite, Meta = (ExposeOnSpawn = true), Category = "Impale Actor")
	float ImpactForce = 2000.f;

	UPROPERTY(BlueprintReadWrite, Meta = (ExposeOnSpawn = true), Category = "Impale Actor")
	FVector ImpactDirection = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Impale Actor")
	float TraceTime = 0.067f;
	
	void PerformTrace();

	void AttachImpaleActor(FHitResult InHit);

protected:
	virtual void BeginPlay() override;
	
	FCollisionQueryParams QueryParams;
	FCollisionObjectQueryParams ObjQueryParams;

	FVector PreviousLocation = FVector::ZeroVector;

	UPROPERTY()
	USceneComponent* ImpaleActorRoot = nullptr;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Impale Actor")
	UCapsuleComponent* ImpaleActorCollision = nullptr;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Impale Actor")
	UProjectileMovementComponent* ProjectileMovementComponent = nullptr;

	UPROPERTY()
	UPhysicsHandleComponent* PhysicsHandle = nullptr;

	FTimerHandle TraceTimerHandle;
	FImpaleTraceDelegate TraceDelegate;

	static inline TArray<FName> CharacterPointNames{Zorans_StaticGameData::BodyPoints::Point_Head, Zorans_StaticGameData::BodyPoints::Point_Torso,
		Zorans_StaticGameData::BodyPoints::Point_Arm_Left, Zorans_StaticGameData::BodyPoints::Point_Arm_Right,
		Zorans_StaticGameData::BodyPoints::Point_Leg_Left, Zorans_StaticGameData::BodyPoints::Point_Leg_Right};
};
