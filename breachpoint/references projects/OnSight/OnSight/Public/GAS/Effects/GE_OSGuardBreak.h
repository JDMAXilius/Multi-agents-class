// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GE_OSAugmentation.h"
#include "OSGameplayEffect.h"
#include "GE_OSGuardBreak.generated.h"

/** GE applied when guard breaks (e.g. from AttributeSet/GuardBreak event). Grant/use for guard-break state or cue. */
UCLASS()
class ONSIGHT_API UGE_OSGuardBreak : public UOSGameplayEffect
{
	GENERATED_BODY()

};
