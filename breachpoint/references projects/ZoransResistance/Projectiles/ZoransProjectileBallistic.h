// Copyright 2022 Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "ZoransProjectileBase.h"
#include "ZoransProjectileBallistic.generated.h"

DECLARE_DELEGATE_OneParam(FProjectileLifetimeUpdated, float)

USTRUCT(BlueprintType)
struct FProjectileLifetimeBasedEvent
{
	GENERATED_USTRUCT_BODY()

	// The normalized point in the projectiles lifespan it will trigger this event (0-1) *Should not use a value of 1, instead use ServerProjectileExpired
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Projectile Lifetime Based Event")
	float PointInLife = 9999.f;

	// The event index that is sent when triggered. This is used to set up multiple events inside the blueprint. Not required if there is only 1 event type. *Use `SwitchOnInt` when implementing in blueprint
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Projectile Lifetime Based Event")
	int32 EventIndex = -1;

	UPROPERTY()
	bool bTriggered = false;
};

UCLASS()
class ZORANSRESISTANCE_API AZoransProjectileBallistic : public AZoransProjectileBase
{
	GENERATED_BODY()
public:
	AZoransProjectileBallistic();

	void BeginPlay() override;

protected:
	// This is the custom Tick on the server.
	void UpdateProjectile_Server() override;
	// This is the custom Tick on the client.
	void UpdateProjectile_Client() override;

	bool PerformProjectileTrace(FHitResult& InHit, const FVector& NewLocation) override;
	
	// This is how long the projectile should exist for. We use this instead of the built in lifespan system to prevent the projectile from being destroyed during an impact before the clients receive the event.
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, meta = (EditCondition = "bCanModify"), Category = "Projectile Attributes|Ballistic")
	FRuntimeFloatCurve ProjectileForwardModifier = FRuntimeFloatCurve();

	// This value is used to modify the effect of gravity on the projectiles. A value of 1.f is for the most part physically accurate without taking drag into consideration.
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, meta = (EditCondition = "bCanModify"), Category = "Projectile Attributes|Ballistic")
	float GravityModifier = 1.f;

	// This is the amount of time before gravity starts being applied to the projectile. (Normalized by lifetime, 0-1 value)
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, meta = (EditCondition = "bCanModify"), Category = "Projectile Attributes|Ballistic")
	float GravityDelay = 0.f;

	// This function will calculate the location of the projectile at any given point in its lifespan, based on the spawn parameters.
	FVector GetProjectileLocationAtTime(const float InTime, const bool bServer) override;

	// This function calculates the -Z effect of gravity based on the lifetime of the projectile.
	float GetGravityEffectAtTime(const float InTime) const;

	void Internal_ProjectileLifetimeEventTriggered();
	
	UFUNCTION(BlueprintNativeEvent, Category = "Projectile Hit Effects")
	void ProjectileLifetimeEventTriggered(FProjectileLifetimeBasedEvent TriggeredEvent, const FVector EventLocation);

	bool ShouldTriggerLifetimeEvent(const float InTime) const;

	// These are the list of events that happen at a point in the projectile's lifetime.
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, meta = (EditCondition = "bCanModify"), Category = "Projectile Attributes|Ballistic")
	TArray<FProjectileLifetimeBasedEvent> ProjectileLifetimeBasedEvents;
	
	void SetNextLifetimeEvent();

	FProjectileLifetimeUpdated ProjectileLifetimeUpdated;

	bool bHasLifetimeEvent = false;

	void InitializeProjectileTraceParameters() override;

	UPROPERTY()
	FProjectileLifetimeBasedEvent NextLifetimeEvent;
};

