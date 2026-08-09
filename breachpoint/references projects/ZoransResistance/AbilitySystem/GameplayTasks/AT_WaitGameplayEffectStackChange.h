// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "AbilitySystemComponent.h"
#include "AT_WaitGameplayEffectStackChange.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnGameplayEffectStackChanged, FActiveGameplayEffectHandle, Handle, int32, NewStackCount, int32, OldStackCount);

UCLASS(BlueprintType, meta = (ExposedAsyncProxy = AsyncTask))
class ZORANSRESISTANCE_API UWaitGameplayEffectStackChange : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintAssignable)
	FOnGameplayEffectStackChanged OnGameplayEffectStackChange;

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"))
	static UWaitGameplayEffectStackChange* WaitGameplayEffectStackChange(UAbilitySystemComponent* AbilitySystemComponent, TSubclassOf<UGameplayEffect> InEffectToTrack);
	
	UFUNCTION(BlueprintCallable)
	void EndTask();

protected:
	UPROPERTY()
	UAbilitySystemComponent* ASC;

	UPROPERTY()
	TSubclassOf<UGameplayEffect> EffectToTrack;

	int32 OldStackCount = 0;
	
	virtual void OnActiveGameplayEffectAddedCallback(UAbilitySystemComponent* Target, const FGameplayEffectSpec& SpecApplied, FActiveGameplayEffectHandle ActiveHandle);
	virtual void OnRemoveGameplayEffectCallback(const FActiveGameplayEffect& EffectRemoved);

	virtual void GameplayEffectStackChanged(FActiveGameplayEffectHandle EffectHandle, int32 NewStackCount, int32 PreviousStackCount);
};