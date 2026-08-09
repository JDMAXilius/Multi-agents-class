#pragma once
// Debug effect: infinite duration, grants Gameplay.State.StunLock tag.
// When present, hit reaction ability can retrigger while already active.

#include "CoreMinimal.h"
#include "GAS/Effects/OSGameplayEffect.h"
#include "GE_OSDebugStunLock.generated.h"

UCLASS()
class ONSIGHT_API UGE_OSDebugStunLock : public UOSGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_OSDebugStunLock(const FObjectInitializer& ObjectInitializer);
};
