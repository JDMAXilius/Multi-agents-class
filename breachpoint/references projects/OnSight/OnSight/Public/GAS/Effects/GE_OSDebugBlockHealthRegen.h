#pragma once
// Debug effect: infinite duration, grants Gameplay.Debug.BlockHealthRegen tag.
// GE_OSHealthRegen has this tag in its IgnoreTags, so regen is inhibited while active.

#include "CoreMinimal.h"
#include "GAS/Effects/OSGameplayEffect.h"
#include "GE_OSDebugBlockHealthRegen.generated.h"

UCLASS()
class ONSIGHT_API UGE_OSDebugBlockHealthRegen : public UOSGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_OSDebugBlockHealthRegen(const FObjectInitializer& ObjectInitializer);
};
