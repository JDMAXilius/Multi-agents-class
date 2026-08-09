// Copyright Zollpa LLC.


#include "WaitWeaponTraceHitResults.h"

void UWaitWeaponTraceHitResults::Activate()
{
	if (!AbilitySystemComponent.Get() || !Ability)
	{
		return;
	}
	
	bool bHasValidHits = false;
	
	TArray<FHitResult> OutHits;
	
	for (int Index = 0; Index < TraceCount; Index++)
	{
		FHitResult TempHitResult;

		FVector TraceEnd = TraceStartLocation + TraceDirection * TraceDistance; 

		FCollisionQueryParams Params;
		
		Params.AddIgnoredActor(Ability->GetAvatarActorFromActorInfo());
		Params.bReturnPhysicalMaterial = true;
		
		GetWorld()->LineTraceSingleByChannel(TempHitResult, TraceStartLocation, TraceEnd, CollisionChannel, Params);

		if (bShowDebug)
		{
			DrawDebugLine(GetWorld(), TraceStartLocation, TraceEnd, FColor::Green, true, 1.0f, 0, 2);
		}
		if (TempHitResult.bBlockingHit)
		{
			bHasValidHits = true;
		}
		OutHits.Add(TempHitResult);
	}

	EventReceived(bHasValidHits,OutHits);
}

UWaitWeaponTraceHitResults* UWaitWeaponTraceHitResults::WaitWeaponTraceHitResults(UGameplayAbility* OwningAbility, const FName TaskInstanceName, const int32 TraceCount, const ECollisionChannel CollisionChannel, const FVector TraceStart, const FVector TraceDirection, const float TraceDistance, const float Accuracy, const float Spread, const bool bShouldOnlyTriggerOnce, const bool bShowDebugLines)
{
	UWaitWeaponTraceHitResults* AbilityTask = NewAbilityTask<UWaitWeaponTraceHitResults>(OwningAbility, TaskInstanceName);

	AbilityTask->CollisionChannel = CollisionChannel;
	AbilityTask->TraceCount = TraceCount;
	AbilityTask->TraceStartLocation = TraceStart;
	AbilityTask->TraceDirection = TraceDirection;
	AbilityTask-> TraceDistance = TraceDistance;
	AbilityTask->Accuracy = Accuracy;
	AbilityTask->bShouldOnlyTriggerOnce = bShouldOnlyTriggerOnce;
	AbilityTask->bShowDebug = bShowDebugLines;

	return AbilityTask;
}

void UWaitWeaponTraceHitResults::EventReceived(const bool bHasValidHitResult, TArray<FHitResult> const OutHitResults)
{
	if (bShouldOnlyTriggerOnce && bHasBeenTriggered)
	{
		bHasBeenTriggered = true;
		
		return;
	}

	HitResultEventReceived.Broadcast(bHasValidHitResult, OutHitResults);
}