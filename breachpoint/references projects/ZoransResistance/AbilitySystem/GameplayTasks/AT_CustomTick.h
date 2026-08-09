// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AT_CustomTick.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCustomTickDelegate, float, DeltaTime);
/**
 * 
 */
UCLASS()
class ZORANSRESISTANCE_API UAT_CustomTick : public UAbilityTask
{
	GENERATED_UCLASS_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FCustomTickDelegate	OnTick;
	
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
		static UAT_CustomTick* CustomTickTask(UGameplayAbility* OwningAbility, FName TaskInstanceName);

	virtual void Activate() override; 
	
	virtual void TickTask(float DeltaTime) override;
	
	void CustomTick(float DeltaTime);

	virtual void OnDestroy(bool AbilityIsEnding) override;
};
