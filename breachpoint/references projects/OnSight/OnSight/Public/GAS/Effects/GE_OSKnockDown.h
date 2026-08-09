// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OSGameplayEffect.h"
#include "GE_OSKnockDown.generated.h"

/** GE for knockdown state (e.g. grants State.KnockedDown). Apply when damage or event triggers knockdown. */
UCLASS()
class ONSIGHT_API UGE_OSKnockDown : public UOSGameplayEffect
{
	GENERATED_BODY()
};
