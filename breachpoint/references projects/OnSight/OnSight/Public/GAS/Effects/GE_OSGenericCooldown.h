#pragma once

/* Duration GE for the unified cooldown pipeline.
   Duration comes from SetByCaller(Data.Cooldown.Duration) at apply time.
   Granted tags come from each ability's CooldownTags, added to the spec's
   DynamicGrantedTags in UOSGameplayAbility::ApplyCooldown. */

#include "CoreMinimal.h"
#include "GAS/Effects/OSGameplayEffect.h"
#include "GE_OSGenericCooldown.generated.h"

UCLASS()
class ONSIGHT_API UGE_OSGenericCooldown : public UOSGameplayEffect
{
	GENERATED_BODY()

	UGE_OSGenericCooldown(const FObjectInitializer& ObjectInitializer);
};
