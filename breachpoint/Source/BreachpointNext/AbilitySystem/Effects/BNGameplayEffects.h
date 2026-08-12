#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "BNGameplayEffects.generated.h"

UCLASS()
class BREACHPOINTNEXT_API UBNGE_InitAttributes : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UBNGE_InitAttributes();
};

UCLASS()
class BREACHPOINTNEXT_API UBNGE_State : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UBNGE_State();
};
