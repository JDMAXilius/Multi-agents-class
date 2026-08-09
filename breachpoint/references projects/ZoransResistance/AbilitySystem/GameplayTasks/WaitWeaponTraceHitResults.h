// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "WaitWeaponTraceHitResults.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWeaponTraceResultDelegate, bool, bHasValidHitResult, const TArray<FHitResult>&, OutHitResults);

UCLASS()
class ZORANSRESISTANCE_API UWaitWeaponTraceHitResults : public UAbilityTask
{
	GENERATED_BODY()

public:
	
	virtual void Activate() override;
	
	UPROPERTY(BlueprintAssignable)
	FWeaponTraceResultDelegate HitResultEventReceived;

	UFUNCTION(BlueprintCallable, meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"), Category = "Ability Tasks")
	static UWaitWeaponTraceHitResults* WaitWeaponTraceHitResults(UGameplayAbility* OwningAbility, FName TaskInstanceName, int32 TraceCount, ECollisionChannel CollisionChannel, FVector TraceStart, FVector TraceDirection, float TraceDistance, float Accuracy, float Spread, bool bShouldOnlyTriggerOnce = true, bool bShowDebugLines = false);
	
private:

	ECollisionChannel CollisionChannel;

	int32 TraceCount;
	
	FVector TraceStartLocation;

	FVector TraceDirection;

	float TraceDistance;

	float Accuracy;

	float Spread;

	bool bShouldOnlyTriggerOnce;

	bool bHasBeenTriggered = false;

	bool bShowDebug = false;
	
	void EventReceived(bool bHasValidHitResult, TArray<FHitResult> OutHitResults);
};