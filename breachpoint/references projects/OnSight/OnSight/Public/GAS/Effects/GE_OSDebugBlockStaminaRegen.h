#pragma once
// Debug effect: infinite duration, grants Gameplay.Debug.BlockStaminaRegen tag.
// GE_OSStaminaRegen has this tag in its IgnoreTags, so regen is inhibited while active.

#include "CoreMinimal.h"
#include "GAS/Effects/OSGameplayEffect.h"
#include "GE_OSDebugBlockStaminaRegen.generated.h"

UCLASS()
class ONSIGHT_API UGE_OSDebugBlockStaminaRegen : public UOSGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_OSDebugBlockStaminaRegen(const FObjectInitializer& ObjectInitializer);
};
