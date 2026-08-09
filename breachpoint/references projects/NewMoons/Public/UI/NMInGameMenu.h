// Fill out your copyright notice in the Description page of Project Settings. 

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NMInGameMenu.generated.h"

class UImage;
class UTextBlock;
class UProgressBar;
class ANMWeaponActor;
class UNMHealthComponent;
class UNMWeaponComponent;

UCLASS()
class NEWMOONS_API UNMInGameMenu : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> WeaponIcon;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> WeaponName;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> LoadedAmmo;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MaxAmmo;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> AmmoIcon;

protected:
	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Called when the health changes on the owners health component */
	UFUNCTION()
	void OnHealthChanged(UNMHealthComponent* HealthComponent, float OldValue, float NewValue, float ScalarValue, AActor* DamageInstigator, AActor* DamageCauser);
	/** Called when current weapon changes on the owners weapon component */
	UFUNCTION()
	void OnWeaponChanged(ANMWeaponActor* NewWeapon);
	UFUNCTION()
	void OnWeaponAmmoChanged(int32 Value);
};
