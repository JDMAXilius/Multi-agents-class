// Copyright Zollpa LLC

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CosmeticWeapon.generated.h"

struct FGameplayEventData;
class UZoransAbilitySystemComponent;
class AZoransCharacterBase;

UCLASS()
class ZORANSRESISTANCE_API ACosmeticWeapon : public AActor
{
	GENERATED_BODY()

public:
	ACosmeticWeapon();

	void Internal_CosmeticWeaponEvent(const FGameplayEventData* InEventData);

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Cosmetic Weapon")
	void CosmeticWeaponEvent(const int32 EventIndex = -1);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Cosmetic Weapon")
	AZoransCharacterBase* GetOwningCharacter() { return OwningCharacter.Get(); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Cosmetic Weapon")
	UZoransAbilitySystemComponent* GetOwningAbilitySystemComponent() { return OwningAbilitySystemComponent.Get() ; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Cosmetic Weapon")
	AZoransCharacterBase* GetTargetCharacter() { return TargetCharacter.Get(); }

	UFUNCTION(BlueprintCallable, Category = "Cosmetic Weapon")
	void RemoveCosmeticWeapon();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Cosmetic Weapon")
	void OnCosmeticWeaponRemoved();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	TWeakObjectPtr<AZoransCharacterBase> OwningCharacter;
	TWeakObjectPtr<UZoransAbilitySystemComponent> OwningAbilitySystemComponent;
	TWeakObjectPtr<AZoransCharacterBase> TargetCharacter;
};