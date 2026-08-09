// Copyright Zollpa LLC.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CommonActivatableWidget.h"
#include "ReticleWidgetBase.generated.h"

class AZoransCharacterBase;
class AZoransWeaponActor;


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), Blueprintable )
class ZORANSRESISTANCE_API UReticleWidgetBase : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void SetReticleSpread(const float InSpread);

	UFUNCTION(BlueprintImplementableEvent, Category = "Reticle")
	void ReticleTick(const float Delta);

	UFUNCTION(BlueprintImplementableEvent, Category = "Reticle")
	void AimingModeChanged(const bool bAiming);

    UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Reticle")
    void ReticleOpened(const bool bOpened);

	UFUNCTION(BlueprintImplementableEvent, Category = "Reticle")
	void ReticleIndexChanged(const int32 Index);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = Reticle)
	int32 GetReticleIndex() { return ReticleIndex; }

	UFUNCTION(BlueprintCallable, Category = "Reticle")
	void SetReticleIndex(const int32 InIndex);

	UFUNCTION(BlueprintCallable, Category = "Reticle")
	void ModifyReticleIndex(const bool bIncrease);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Reticle")
	int32 GetReticleMaxIndex() { return ReticleMaxIndex; }

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Reticle")
	float SpreadOffset = 0.1f;

	// The highest value the ReticleIndex can be set to. NOTE* ReticleIndex starts at 0, so a ReticleMaxIndex of 2 means it has 3 levels
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Reticle")
	int32 ReticleMaxIndex = 0;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Reticle")
	bool bReticleLoopOnClamp = false;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Reticle")
	AZoransWeaponActor* GetOwningWeapon() { return OwningWeapon.Get(); }

	void SetOwningWeapon(AZoransWeaponActor* InWeapon);

	TWeakObjectPtr<AZoransWeaponActor> OwningWeapon;

	UFUNCTION(BlueprintCallable, Category = "Reticle")
	void SetTargetCharacter(AZoransCharacterBase* InTargetCharacter);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Reticle")
	AZoransCharacterBase* GetTargetCharacter() { return TargetCharacter.Get(); }

	UFUNCTION(BlueprintImplementableEvent, Category = "Reticle")
	void TargetCharacterUpdated(const AZoransCharacterBase* InTargetCharacter, const bool bHasTarget);

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Reticle")
	float WeaponSpread = 0.f;
	
	int32 ReticleIndex = 0;

	c<AZoransCharacterBase> TargetCharacter;
};