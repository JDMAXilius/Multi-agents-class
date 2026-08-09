#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AT_OSTickAbilityTask.generated.h"

/** Per-frame ability task for short windows (reference AT_TickAbilityTask / GA_Weapon_Melee sweep). */
UCLASS()
class ONSIGHT_API UAT_OSTickAbilityTask : public UAbilityTask
{
	GENERATED_BODY()

public:
	UAT_OSTickAbilityTask();
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOSAbilityTickDelegate, float, DeltaTime);

	UPROPERTY(BlueprintAssignable)
	FOSAbilityTickDelegate OnTick;

	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks",
		meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true"))
	static UAT_OSTickAbilityTask* CreateTickTask(UGameplayAbility* OwningAbility, FName TaskInstanceName);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;

private:
	bool bTaskActive = false;
};
