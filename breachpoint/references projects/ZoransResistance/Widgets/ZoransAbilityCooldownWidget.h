// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "Blueprint/UserWidget.h"
#include "ZoransResistance/AbilitySystem/ZoransGameplayAbility.h"
#include "ZoransResistance/AbilitySystem/GameplayTasks/WaitEnhancedInputEvent.h"
#include "ZoransAbilityCooldownWidget.generated.h"

class UZoransGameplayAbility;
class UInputAction;
class UZoransUserInterfaceWidget;

UCLASS()
class ZORANSRESISTANCE_API UZoransAbilityCooldownWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (ExposeOnSpawn = true))
	UZoransGameplayAbility* TrackedAbility = nullptr;

	UFUNCTION(BlueprintCallable, BlueprintCosmetic)
	void UpdateDuration(float& NewDuration);
	
	UFUNCTION(BlueprintCallable, BlueprintPure)
	UAbilitySystemComponent* GetOwningAbilitySystemComponent();

	UFUNCTION()
	void BindAbilityCooldown(const FGameplayAbilitySpec& AbilitySpec);
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FGameplayTag AbilityCooldownTag;

	UFUNCTION(BlueprintImplementableEvent)
	void On_InputPressed();
	
	UFUNCTION(BlueprintImplementableEvent)
	void On_DurationStarted();
	
	UFUNCTION(BlueprintImplementableEvent)
	void On_DurationFinished();

	bool InitTrackedAbilityValues();

	UFUNCTION(BlueprintImplementableEvent)
	void TrackedAbilityUpdated(const UZoransGameplayAbility* NewTrackedAbility);
	
protected:
	
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UMaterialInstance> CooldownMaterial = nullptr;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UInputAction> InputAction = nullptr;
	
	TObjectPtr<UAbilitySystemComponent> OwnerAbilitySystemComponent = nullptr;
	
	FGameplayEffectQuery DurationQueryTags;
	
	FDelegateHandle EffectAddedHandle;
	FDelegateHandle EffectRemovedHandle;
	
	void ListenForGameplayEffectAdded(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayEffectSpec& GameplayEffectSpec, FActiveGameplayEffectHandle ActiveGameplayEffectHandle);

	void ListenForGameplayEffectRemoval(const FActiveGameplayEffect& ActiveGameplayEffect);

	virtual void NativeDestruct() override;
};