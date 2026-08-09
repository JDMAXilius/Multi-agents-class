// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "TagListener.generated.h"

// This class does not need to be modified.
UINTERFACE()
class UTagListener : public UInterface
{
	GENERATED_BODY()
};


class NEWMOONS_API ITagListener
{
	GENERATED_BODY()

public:
	virtual void OnTagAdded(const FGameplayTag& Tag) = 0;
	virtual void OnTagRemoved(const FGameplayTag& Tag) = 0;
};
