// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OSGameplayEffect.h"
#include "GE_OSApplyDamage.generated.h"

/** GE that applies damage: uses ExecCalc_OSDamage, SetByCaller Data.Damage. Add to specs when applying damage (e.g. from attack). */
UCLASS()
class ONSIGHT_API UGE_OSApplyDamage : public UOSGameplayEffect
{
	GENERATED_BODY()

public:
	UGE_OSApplyDamage(const FObjectInitializer& ObjectInitializer);
};
