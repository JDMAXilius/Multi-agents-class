// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "OSFXEventDataAsset.h"
#include "Engine/DataAsset.h"
#include "OSFXLibraryDataAsset.generated.h"

/**
 * 
 */
UCLASS()
class ONSIGHT_API UOSFXLibraryDataAsset : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<FGameplayTag, TObjectPtr<UOSFXEventDataAsset>> Events;

	UFUNCTION(BlueprintCallable)
	const UOSFXEventDataAsset* FindEvent(FGameplayTag Tag) const
	{
		if (const TObjectPtr<UOSFXEventDataAsset>* Found = Events.Find(Tag))
			return Found->Get();
		return nullptr;
	}
};
