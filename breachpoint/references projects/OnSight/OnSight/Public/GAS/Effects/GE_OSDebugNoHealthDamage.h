#pragma once
// Debug effect: infinite duration, grants Gameplay.Debug.NoHealthDamage tag.
// When present, PostGameplayEffectExecute preserves health on damage. Hit reactions still play.

#include "CoreMinimal.h"
#include "GAS/Effects/OSGameplayEffect.h"
#include "GE_OSDebugNoHealthDamage.generated.h"

UCLASS()
class ONSIGHT_API UGE_OSDebugNoHealthDamage : public UOSGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_OSDebugNoHealthDamage(const FObjectInitializer& ObjectInitializer);
};
