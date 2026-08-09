// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/NMInGameMenu.h"
#include "Characters/Components/NMHealthComponent.h"
#include "Characters/Components/NMWeaponComponent.h"
#include "InventorySystem/Weapons/NMWeaponActor.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Player/NMPlayerState.h"

bool UNMInGameMenu::Initialize()
{
	return Super::Initialize();
}

void UNMInGameMenu::NativeConstruct()
{
	Super::NativeConstruct();

	if (UNMHealthComponent* HealthComponent = UNMHealthComponent::FindHealthComponent(GetOwningPlayerPawn()))
	{
		HealthBar->SetPercent(HealthComponent->GetHealthNormalized());
		HealthComponent->OnHealthChanged.AddUniqueDynamic(this, &ThisClass::OnHealthChanged);
	}

	if (UNMWeaponComponent* WeaponComponent = UNMWeaponComponent::FindWeaponComponent(GetOwningPlayerPawn()))
	{
		WeaponComponent->CurrentWeaponChanged.AddUniqueDynamic(this, &ThisClass::OnWeaponChanged);
	}
}

void UNMInGameMenu::NativeDestruct()
{
	// Health changed callback removed
	if (UNMHealthComponent* HealthComponent = UNMHealthComponent::FindHealthComponent(GetOwningPlayerPawn()))
	{
		HealthComponent->OnHealthChanged.RemoveAll(this);
	}

	// Remove weapon component delegate bindings if it exists
	if (UNMWeaponComponent* WeaponComponent = UNMWeaponComponent::FindWeaponComponent(GetOwningPlayerPawn()))
	{
		WeaponComponent->CurrentWeaponChanged.RemoveAll(this);

		// Ensure that the equipped weapon is valid before removing the delegate binding
		if (ANMWeaponActor* EquippedWeapon = WeaponComponent->GetEquippedWeapon())
		{
			EquippedWeapon->OnLoadedAmmoChangeDelegate.RemoveAll(this);
		}
	}

	Super::NativeDestruct();
}

void UNMInGameMenu::OnHealthChanged(UNMHealthComponent* HealthComponent, float OldValue, float NewValue, float ScalarValue, AActor* DamageInstigator, AActor* DamageCauser)
{
	HealthBar->SetPercent(ScalarValue);
}

void UNMInGameMenu::OnWeaponChanged(ANMWeaponActor* NewWeapon)
{
	if (NewWeapon)
	{
		NewWeapon->OnLoadedAmmoChangeDelegate.AddUniqueDynamic(this, &ThisClass::OnWeaponAmmoChanged);
		WeaponName->SetText(NewWeapon->GetWeaponName());
		WeaponIcon->SetBrushFromTexture(NewWeapon->GetWeaponIcon());
		MaxAmmo->SetText(FText::FromString(FString::FromInt(NewWeapon->GetMaxClipAmmo())));
		LoadedAmmo->SetText(FText::FromString(FString::FromInt(NewWeapon->GetLoadedAmmo())));
	}
}

void UNMInGameMenu::OnWeaponAmmoChanged(int32 Value)
{
	LoadedAmmo->SetText(FText::FromString(FString::FromInt(Value)));
}
