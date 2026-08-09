// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ReticleWidgetBase.generated.h"

class ANMCharacterBase;
class ANMWeaponActor;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), Blueprintable )
class NEWMOONS_API UReticleWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Reticle")
	ANMWeaponActor* GetOwningWeapon() { return OwningWeapon.Get(); }

	void SetOwningWeapon(ANMWeaponActor* InWeapon);

	TWeakObjectPtr<ANMWeaponActor> OwningWeapon;


protected:

	TWeakObjectPtr<ANMCharacterBase> TargetCharacter;
};
