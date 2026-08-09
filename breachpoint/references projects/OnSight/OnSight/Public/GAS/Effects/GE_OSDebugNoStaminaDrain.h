#pragma once
// Debug effect: infinite duration, grants Gameplay.Debug.NoStaminaDrain tag.
// Override modifier sets Stamina to 9999 — standard PreAttributeChange clamp brings it to MaxStamina.

#include "CoreMinimal.h"
#include "GAS/Effects/OSGameplayEffect.h"
#include "GE_OSDebugNoStaminaDrain.generated.h"

UCLASS()
class ONSIGHT_API UGE_OSDebugNoStaminaDrain : public UOSGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_OSDebugNoStaminaDrain(const FObjectInitializer& ObjectInitializer);
};
