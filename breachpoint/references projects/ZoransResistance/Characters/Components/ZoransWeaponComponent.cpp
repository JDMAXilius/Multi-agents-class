// Copyright Zollpa LLC.


#include "ZoransWeaponComponent.h"
#include "AbilitySystemGlobals.h"
#include "ZoransResistance/InventorySystem/Weapons/ZoransWeaponActor.h"
#include "ZoransResistance/InventorySystem/Weapons/Data/ZoransWeaponData.h"
#include "Net/UnrealNetwork.h"
#include "ZoransResistance/AbilitySystem/ZoransAbilitySystemComponent.h"
#include "ZoransResistance/Game/Data/StaticGameData.h"
#include "ZoransResistance/AbilitySystem/ZoransGameplayAbility.h"
#include "GameplayAbilitySpec.h"
#include "Engine/AssetManager.h"
#include "ZoransResistance/AbilitySystem/ZoransGameplayEffect.h"
#include "ZoransResistance/AbilitySystem/Data/ZoransGameplayTags.h"
#include "ZoransResistance/Characters/ZoransCharacterBase.h"
#include "ZoransResistance/Economy/DA_WeaponSkin.h"
#include "ZoransResistance/Game/ZoransGameMode.h"
#include "ZoransResistance/Game/ZoransGameModeLogic.h"
#include "ZoransResistance/Game/ZoransGameState.h"
#include "ZoransResistance/Player/ZoransPlayerState.h"

using namespace Zorans_StaticGameData;

UZoransWeaponComponent::UZoransWeaponComponent()
{
	SetIsReplicatedByDefault(true);

	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	
	EquippedWeapons.AddDefaulted(InventorySize);
}

void UZoransWeaponComponent::InitPlayerLoadout()
{
	if (AZoransPlayerState* ZPS = Cast<AZoransPlayerState>(GetOwner()))
	{
		ZoransPlayerState = ZPS;
	}
	
	if (const AGameModeBase* GameModeBase = GetWorld()->GetAuthGameMode())
	{
		if (const AZoransGameMode* ZoransGameMode = Cast<AZoransGameMode>(GameModeBase))
		{
			// check here for GameMode Overrides (Mode and/or DataLayer Weapon Type Overrides)
			// return after override settings to prevent double equipping
			if(const AZoransGameState* ZoransGameState = Cast<AZoransGameState>(ZoransGameMode->GameState))
			{
				if(ZoransGameState->GetCurrentPhase() == EGamePhase::InProgress)
				{
					if(ZoransGameState->GetGameModeSettings().bHasCustomLoadout && IsValid(GetOwningPlayerState()))
					{
						FName FirstWeapon, SecondWeapon;
						ZoransGameState->GameModeLogic->InitLoadout(GetOwningPlayerState(), FirstWeapon, SecondWeapon);
						EquipNewWeaponInSlot(FirstWeapon, 0);
						EquipNewWeaponInSlot(SecondWeapon, 1);
						EquipNewWeaponInSlot("Unarmed", ObjectiveWeaponSlot);
						SetWeaponSlot(0);
						return;
					}
				}
			}
		}
	}
	
	if (IsValid(GetOwningPlayerState()))
	{
		const FPlayerLoadout SelectedLoadout = GetOwningPlayerState()->GetCurrentLoadout();
		
		TryPickupWeapon(SelectedLoadout.PrimaryWeapon, true);
		TryPickupWeapon(SelectedLoadout.SecondaryWeapon, true);
		EquipNewWeaponInSlot("Unarmed", ObjectiveWeaponSlot);

		SetWeaponSlot(0);
	}
}

void UZoransWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME_CONDITION_NOTIFY(UZoransWeaponComponent, EquippedWeapons, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UZoransWeaponComponent, CurrentWeaponSlot, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UZoransWeaponComponent, LightAmmo, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UZoransWeaponComponent, MediumAmmo, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UZoransWeaponComponent, HeavyAmmo, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UZoransWeaponComponent, ShotgunAmmo, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UZoransWeaponComponent, SniperAmmo, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UZoransWeaponComponent, ExplosiveSmallAmmo, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UZoransWeaponComponent, ExplosiveLargeAmmo, COND_OwnerOnly, REPNOTIFY_Always);
}

void UZoransWeaponComponent::GiveAmmo(const EAmmoType AmmoType, const int32 Amount)
{
	switch (AmmoType)
	{
	case EAmmoType::None:
		break;
	case EAmmoType::Light:
		LightAmmo = FMath::Clamp<int32>(LightAmmo + Amount, 0, LightAmmoCapacity);
		OnRep_LightAmmo();
		break;
	case EAmmoType::Medium:
		MediumAmmo = FMath::Clamp<int32>(MediumAmmo + Amount, 0, MediumAmmoCapacity);
		OnRep_MediumAmmo();
		break;
	case EAmmoType::Heavy:
		HeavyAmmo = FMath::Clamp<int32>(HeavyAmmo + Amount, 0, HeavyAmmoCapacity);
		OnRep_HeavyAmmo();
		break;
	case EAmmoType::Shotgun:
		ShotgunAmmo = FMath::Clamp<int32>(ShotgunAmmo + Amount, 0, ShotgunAmmoCapacity);
		OnRep_ShotgunAmmo();
		break;
	case EAmmoType::Sniper:
		SniperAmmo = FMath::Clamp<int32>(SniperAmmo + Amount, 0, SniperAmmoCapacity);
		OnRep_SniperAmmo();
		break;
	case EAmmoType::ExplosiveSmall:
		ExplosiveSmallAmmo = FMath::Clamp<int32>(ExplosiveSmallAmmo + Amount, 0, ExplosiveSmallAmmoCapacity);
		OnRep_ExplosiveSmallAmmo();
		break;
	case EAmmoType::ExplosiveLarge:
		ExplosiveLargeAmmo = FMath::Clamp<int32>(ExplosiveLargeAmmo + Amount, 0, ExplosiveLargeAmmoCapacity);
		OnRep_ExplosiveLargeAmmo();
		break;
	}
}

bool UZoransWeaponComponent::GiveAmmoForCurrentWeapon(const float Ratio)
{
	int32 AmmoToResupply = 0;

	EAmmoType CurrentAmmoType = EAmmoType::None;
	const AZoransWeaponActor* CurrentWeapon = GetCurrentEquippedWeapon();
	if (IsValid(CurrentWeapon))
	{
		CurrentAmmoType = CurrentWeapon->GetWeaponDefaults().AmmoType;
		switch (CurrentAmmoType)
		{
		case EAmmoType::None:
			break;
		case EAmmoType::Light:
			if (LightAmmo < LightAmmoCapacity)
			{
				AmmoToResupply = FMath::RoundFromZero(LightAmmoCapacity * Ratio);
			}
			break;
		case EAmmoType::Medium:
			if (MediumAmmo < MediumAmmoCapacity)
			{
				AmmoToResupply = FMath::RoundFromZero(MediumAmmoCapacity * Ratio);
			}
			break;
		case EAmmoType::Heavy:
			if (HeavyAmmo < HeavyAmmoCapacity)
			{
				AmmoToResupply = FMath::RoundFromZero(HeavyAmmoCapacity * Ratio);
			}
			break;
		case EAmmoType::Shotgun:
			if (ShotgunAmmo < ShotgunAmmoCapacity)
			{
				AmmoToResupply = FMath::RoundFromZero(ShotgunAmmoCapacity * Ratio);
			}
			break;
		case EAmmoType::Sniper:
			if (SniperAmmo < SniperAmmoCapacity)
			{
				AmmoToResupply = FMath::RoundFromZero(SniperAmmoCapacity * Ratio);
			}
			break;
		case EAmmoType::ExplosiveSmall:
			if (ExplosiveSmallAmmo < ExplosiveSmallAmmoCapacity)
			{
				AmmoToResupply = FMath::RoundFromZero(ExplosiveSmallAmmoCapacity * Ratio);
			}
			break;
		case EAmmoType::ExplosiveLarge:
			if (ExplosiveLargeAmmo < ExplosiveLargeAmmoCapacity)
			{
				AmmoToResupply = FMath::RoundFromZero(ExplosiveLargeAmmoCapacity * Ratio);
			}
			break;
		}
	}

	if (AmmoToResupply > 0)
	{
		GiveAmmo(CurrentAmmoType, AmmoToResupply);
	}

	return AmmoToResupply > 0;
}

void UZoransWeaponComponent::Internal_SetWeaponVisibility(FGameplayTag GameplayTag, const int TagCount) const
{
	if (IsValid(GetCurrentEquippedWeapon()))
	{
		if (TagCount > 0)
		{
			GetCurrentEquippedWeapon()->InternalStowWeapon();
		}
		else
		{
			GetCurrentEquippedWeapon()->InternalReadyWeapon();
		}
	}
}

void UZoransWeaponComponent::InitWeaponComponent(AActor* InOwningActor)
{
	check(InOwningActor);
	
	OwningCharacter = Cast<AZoransCharacterBase>(InOwningActor);
	AbilitySystemComponent = UAbilitySystemGlobals::Get().GetAbilitySystemComponentFromActor(InOwningActor);

	if (GetNetMode() < NM_Client && AbilitySystemComponent.Get())
	{
		AbilitySystemComponent->RegisterGameplayTagEvent(GameplayTag_HideWeapon.GetTag(), EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UZoransWeaponComponent::Internal_SetWeaponVisibility);
	}
	
	RemoveAllEquippedWeapons();

	bIsInitialized = true;

	if (IComponentInterface* ComponentInterface = Cast<IComponentInterface>(InOwningActor))
	{
		ComponentInterface->CheckComponentRequirements();
	}
}

void UZoransWeaponComponent::InitNewWeaponData(const int32 InSlot)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	if (!EquippedWeapons.IsValidIndex(InSlot))
	{
		return;
	}

	ensure(WeaponDataTable);
	
	check(AbilitySystemComponent.Get())
	
	// Clear the old Weapon Data

	RemoveCurrentWeaponData();
	
	// Add the new Weapon Data

	const FName NewWeapon = EquippedWeapons[InSlot].WeaponName;
	
	FZoransWeaponData* NewWeaponData = WeaponDataTable->FindRow<FZoransWeaponData>(NewWeapon, "");

	AZoransWeaponActor* CurrentWeapon = GetWeaponActor(InSlot);

	if (NewWeaponData->PrimaryGameplayAbility)
	{
		const FGameplayAbilitySpecHandle PrimarySpecHandle = AbilitySystemComponent->GiveAbility(AbilitySystemComponent->BuildAbilitySpecFromClass(NewWeaponData->PrimaryGameplayAbility));
		FGameplayAbilitySpec* PrimarySpec = AbilitySystemComponent->FindAbilitySpecFromHandle(PrimarySpecHandle);
		PrimarySpec->SourceObject = CurrentWeapon;
		PrimaryWeaponAbilitySpec = PrimarySpecHandle;
	}

	if (NewWeaponData->SecondaryGameplayAbility)
	{
		const FGameplayAbilitySpecHandle SecondarySpecHandle = AbilitySystemComponent->GiveAbility(AbilitySystemComponent->BuildAbilitySpecFromClass(NewWeaponData->SecondaryGameplayAbility));
		FGameplayAbilitySpec* SecondarySpec = AbilitySystemComponent->FindAbilitySpecFromHandle(SecondarySpecHandle);
		SecondarySpec->SourceObject = CurrentWeapon;
		SecondaryWeaponAbilitySpec = SecondarySpecHandle;
	}

	for (auto AddedAbility : NewWeaponData->OptionalAddedGameplayAbilities)
	{
		const FGameplayAbilitySpecHandle AddedSpecHandle = AbilitySystemComponent->GiveAbility(AbilitySystemComponent->BuildAbilitySpecFromClass(AddedAbility));
		FGameplayAbilitySpec* AddedSpecSpec = AbilitySystemComponent->FindAbilitySpecFromHandle(AddedSpecHandle);
		AddedSpecSpec->SourceObject = CurrentWeapon;
		AddedAbilitySpecHandles.Add(AddedSpecHandle);
	}
	
	for (const TSubclassOf<UGameplayEffect> EquipEffect : NewWeaponData->EquippedGameplayEffects)
	{
		FGameplayEffectContextHandle EffectContextHandle = AbilitySystemComponent->MakeEffectContext();

		EffectContextHandle.AddSourceObject(this);

		FGameplayEffectSpecHandle GameplayEffectSpecHandle = AbilitySystemComponent->MakeOutgoingSpec(EquipEffect, 1.f, EffectContextHandle);
		
		FActiveGameplayEffectHandle EffectHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*GameplayEffectSpecHandle.Data.Get());

		CurrentWeaponEffects.Add(EffectHandle);
	}
}

void UZoransWeaponComponent::SetWeaponSlot(const int32 InSlot)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}
	
	if (CurrentWeaponSlot != InSlot)
	{
		CurrentWeaponSlot = InSlot;

		OnRep_CurrentWeaponSlot();
	}
}

bool UZoransWeaponComponent::TryPickupWeapon(const FName WeaponName, bool bInitialSpawn)
{
	bool bWasSuccessful = false;
	
	if (GetOwnerRole() != ROLE_Authority)
	{
		return bWasSuccessful;
	}

	ensure(WeaponDataTable);

	if (WeaponDataTable->GetRowNames().Contains(WeaponName))
	{
		if (const auto WeaponData = WeaponDataTable->FindRow<FZoransWeaponData>(WeaponName, "", true))
		{
			// Check to see if the weapon is currently in inventory
			// If so, add the weapon's starting ammo amount to current ammo 
			for (FWeaponInfo Weapon : EquippedWeapons)
			{
				if (Weapon.WeaponName == WeaponName)
				{
					switch (WeaponData->AmmoType)
					{
					case EAmmoType::None:
						{
							return bWasSuccessful;
						}
					case EAmmoType::Light:
						{
							if (LightAmmo < LightAmmoCapacity)
							{
								bWasSuccessful = true;
								const int NewAmmo = LightAmmo += WeaponData->AmmoPickupAmount;
								LightAmmo = FMath::Clamp(NewAmmo, 0, LightAmmoCapacity);
								OnRep_LightAmmo();
							
								return bWasSuccessful;
							}

							return bWasSuccessful;
						}
					case EAmmoType::Medium:
						{
							if (MediumAmmo < MediumAmmoCapacity)
							{
								bWasSuccessful = true;
								const int NewAmmo = MediumAmmo += WeaponData->AmmoPickupAmount;
								MediumAmmo = FMath::Clamp(NewAmmo, 0, MediumAmmoCapacity);
								OnRep_MediumAmmo();
							
								return bWasSuccessful;
							}

							return bWasSuccessful;
						}	
					case EAmmoType::Heavy:
						{
							if (HeavyAmmo < HeavyAmmoCapacity)
							{
								bWasSuccessful = true;
								const int NewAmmo = HeavyAmmo += WeaponData->AmmoPickupAmount;
								HeavyAmmo = FMath::Clamp(NewAmmo, 0, HeavyAmmoCapacity);
								OnRep_HeavyAmmo();
							
								return bWasSuccessful;
							}
					
							return bWasSuccessful;
						}
					case EAmmoType::Shotgun:
						{
							if (ShotgunAmmo < ShotgunAmmoCapacity)
							{
								bWasSuccessful = true;
								const int NewAmmo = ShotgunAmmo += WeaponData->AmmoPickupAmount;
								ShotgunAmmo = FMath::Clamp(NewAmmo, 0, ShotgunAmmoCapacity);
								OnRep_ShotgunAmmo();
							
								return bWasSuccessful;
							}
					
							return bWasSuccessful;
						}
					case EAmmoType::Sniper:
						{
							if (SniperAmmo < SniperAmmoCapacity)
							{
								bWasSuccessful = true;
								const int NewAmmo = SniperAmmo += WeaponData->AmmoPickupAmount;
								SniperAmmo = FMath::Clamp(NewAmmo, 0, SniperAmmoCapacity);
								OnRep_SniperAmmo();
							
								return bWasSuccessful;
							}
					
							return bWasSuccessful;
						}
					case EAmmoType::ExplosiveSmall:
						{
							if (ExplosiveSmallAmmo < ExplosiveSmallAmmoCapacity)
							{
								bWasSuccessful = true;
								const int NewAmmo = ExplosiveSmallAmmo += WeaponData->AmmoPickupAmount;
								ExplosiveSmallAmmo = FMath::Clamp(NewAmmo, 0, ExplosiveSmallAmmoCapacity);
								OnRep_ExplosiveSmallAmmo();
							
								return bWasSuccessful;
							}
					
							return bWasSuccessful;
						}
					case EAmmoType::ExplosiveLarge:
						{
							if (ExplosiveLargeAmmo < ExplosiveLargeAmmoCapacity)
							{
								bWasSuccessful = true;
								const int NewAmmo = ExplosiveLargeAmmo += WeaponData->AmmoPickupAmount;
								ExplosiveLargeAmmo = FMath::Clamp(NewAmmo, 0, ExplosiveLargeAmmoCapacity);
								OnRep_ExplosiveLargeAmmo();
							
								return bWasSuccessful;
							}
					
							return bWasSuccessful;
						}
					}
				}
			}
		
			for (int32 Index = 0; Index < EquippedWeapons.Num(); Index++)
			{
				if (Index != ObjectiveWeaponSlot && EquippedWeapons[Index].WeaponName == FName("Unarmed"))
				{
					bWasSuccessful = true;
					EquipNewWeaponInSlot(WeaponName, Index);

					if (SwapWeaponOnPickup && !bInitialSpawn)
					{
						SetWeaponSlot(Index);
					}
			
					return bWasSuccessful;
				}
			}

			// Last resort: Swap out the currently equipped weapon for the new one
			EquipNewWeaponInSlot(WeaponName, CurrentWeaponSlot);
		}
	}

	return bWasSuccessful;
}

void UZoransWeaponComponent::TryPickupDroppedWeapon(const FWeaponInfo WeaponInfo, bool& bWasSuccessful)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}
	
	ensure(WeaponDataTable);

	bWasSuccessful = false;
	
	if (const auto WeaponData = WeaponDataTable->FindRow<FZoransWeaponData>(WeaponInfo.WeaponName, "", true))
	{
		// Check to see if the weapon is currently in inventory
		// If so, add the weapon's starting ammo amount to current ammo 
		for (FWeaponInfo Weapon : EquippedWeapons)
		{
			if (Weapon.WeaponName == WeaponInfo.WeaponName)
			{
				switch (WeaponData->AmmoType)
				{
				case EAmmoType::None:
					{
						return;
					}
				case EAmmoType::Light:
					{
						if (LightAmmo != LightAmmoCapacity)
						{
							bWasSuccessful = true;
							const int NewAmmo = LightAmmo += WeaponInfo.LoadedAmmo;
							LightAmmo = FMath::Clamp(NewAmmo, 0, LightAmmoCapacity);
							OnRep_LightAmmo();
							
							return;
						}
						return;
					}
				case EAmmoType::Medium:
					{
						if (MediumAmmo != MediumAmmoCapacity)
						{
							bWasSuccessful = true;
							const int NewAmmo = MediumAmmo += WeaponInfo.LoadedAmmo;
							MediumAmmo = FMath::Clamp(NewAmmo, 0, MediumAmmoCapacity);
							OnRep_MediumAmmo();
							
							return;
						}	
						return;
					}
				case EAmmoType::Heavy:
					{
						if (HeavyAmmo != HeavyAmmoCapacity)
						{
							bWasSuccessful = true;
							const int NewAmmo = HeavyAmmo += WeaponInfo.LoadedAmmo;
							HeavyAmmo = FMath::Clamp(NewAmmo, 0, HeavyAmmoCapacity);
							OnRep_HeavyAmmo();
							
							return;
						}	
						return;
					}
				case EAmmoType::Shotgun:
					{
						if (ShotgunAmmo != ShotgunAmmoCapacity)
						{
							bWasSuccessful = true;
							const int NewAmmo = ShotgunAmmo += WeaponInfo.LoadedAmmo;
							ShotgunAmmo = FMath::Clamp(NewAmmo, 0, ShotgunAmmoCapacity);
							OnRep_ShotgunAmmo();
							
							return;
						}	
						return;
					}
				case EAmmoType::Sniper:
					{
						if (SniperAmmo != SniperAmmoCapacity)
						{
							bWasSuccessful = true;
							const int NewAmmo = SniperAmmo += WeaponInfo.LoadedAmmo;
							SniperAmmo = FMath::Clamp(NewAmmo, 0, SniperAmmoCapacity);
							OnRep_SniperAmmo();
							
							return;
						}
						return;
					}
				case EAmmoType::ExplosiveSmall:
					{
						if (ExplosiveSmallAmmo != ExplosiveSmallAmmoCapacity)
						{
							bWasSuccessful = true;
							const int NewAmmo = ExplosiveSmallAmmo += WeaponInfo.LoadedAmmo;
							ExplosiveSmallAmmo = FMath::Clamp(NewAmmo, 0, ExplosiveSmallAmmoCapacity);
							OnRep_ExplosiveSmallAmmo();
							
							return;
						}
						return;
					}
				case EAmmoType::ExplosiveLarge:
					{
						if (ExplosiveLargeAmmo != ExplosiveLargeAmmoCapacity)
						{
							bWasSuccessful = true;
							const int NewAmmo = ExplosiveLargeAmmo += WeaponInfo.LoadedAmmo;
							ExplosiveLargeAmmo = FMath::Clamp(NewAmmo, 0, ExplosiveLargeAmmoCapacity);
							OnRep_ExplosiveLargeAmmo();
							
							return;
						}
						return;
					}
				}
			}
		}
		
		for (int32 Index = 0; Index < EquippedWeapons.Num(); Index++)
		{
			if (EquippedWeapons[Index].WeaponName == FName("Unarmed"))
			{
				bWasSuccessful = true;
				EquipDroppedWeaponInSlot(WeaponInfo, Index);

				if (SwapWeaponOnPickup)
				{
					SetWeaponSlot(Index);
				}
			
				return;
			}
		}

		// Last resort: Swap out the currently equipped weapon for the new one
		EquipDroppedWeaponInSlot(WeaponInfo, CurrentWeaponSlot);
	}
}

void UZoransWeaponComponent::EquipNewWeaponInSlot(const FName WeaponName, const int32 InSlot)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	ensure(WeaponDataTable);
	
	if (const auto WeaponData = WeaponDataTable->FindRow<FZoransWeaponData>(WeaponName, "", true))
	{
		if (AZoransWeaponActor* OldWeapon = EquippedWeapons[InSlot].SpawnedWeapon)
		{
			OldWeapon->Destroy();
		}
		
		if (IsValid(WeaponData->WeaponClass) && IsValid(GetOwningCharacter()))
		{
			const FTransform OwnerTransform = GetOwningCharacter()->GetActorTransform();

			TSubclassOf<AZoransWeaponActor> WeaponToSpawn = WeaponData->WeaponClass;
			// if (IsValid(GetOwningPlayerState()) && IsValid(WeaponSkinDataTable))
			// {
			// 	FPlayerLoadout CurrentLoadout = GetOwningPlayerState()->GetCurrentLoadout();
			// 	for (auto const &WeaponSkin : CurrentLoadout.WeaponSkinSets)
			// 	{
			// 		if (WeaponSkin.WeaponName == WeaponName)
			// 		{
			// 			if (const auto WeaponSkinData = WeaponSkinDataTable->FindRow<FWeaponSkinDataAssetRow>(WeaponSkin.WeaponSkin, "", true))
			// 			{
			// 				if (UAssetManager* AssetManager = UAssetManager::GetIfValid())
			// 				{
			// 					auto Path = AssetManager->GetPrimaryAssetPath(WeaponSkinData->WeaponSkinDataAsset);
			// 					TSoftObjectPtr Ptr(Path);
			// 					if (UObject *Obj = Ptr.LoadSynchronous())
			// 					{
			// 						if (UDA_WeaponSkin* WeaponSkinDataAsset = Cast<UDA_WeaponSkin>(Obj))
			// 						{
			// 							TArray<FName> SkinKeys;
			// 							WeaponSkinDataAsset->WeaponSkinOverrides.GetKeys(SkinKeys);
			// 							if (SkinKeys.Contains(WeaponName))
			// 							{
			// 								TSoftClassPtr<AZoransWeaponActor> WeaponOverrideClass = WeaponSkinDataAsset->WeaponSkinOverrides.Find(WeaponName)->WeaponOverrideClass;
			// 								if (WeaponOverrideClass.IsValid())
			// 								{
			// 									WeaponToSpawn = WeaponOverrideClass.Get();
			// 								}
			// 								else if (!WeaponOverrideClass->GetPathName().IsEmpty())
			// 								{
			// 									WeaponToSpawn = WeaponOverrideClass.LoadSynchronous();
			// 								}
			// 							}
			// 						}
			// 					}
			// 				}
			// 			}
			// 			break;
			// 		}
			// 	}
			// }

			if (AZoransWeaponActor* WeaponActor = GetWorld()->SpawnActorDeferred<AZoransWeaponActor>(WeaponToSpawn, OwnerTransform))
			{
				WeaponActor->SetOwningCharacter(GetOwningCharacter());
				WeaponActor->SetOwner(GetOwningCharacter());
				WeaponActor->SetInstigator(GetOwningCharacter());
				EquippedWeapons[InSlot].SpawnedWeapon = WeaponActor;
				EquippedWeapons[InSlot].WeaponName = WeaponName;
				EquippedWeapons[InSlot].LoadedAmmo = WeaponData->AmmoCapacity;
				EquippedWeapons[InSlot].SpawnedWeapon->SetLoadedAmmo(WeaponData->AmmoCapacity);
				GiveAmmo(WeaponData->AmmoType, WeaponData->AmmoPickupAmount);
			
				if (InSlot == CurrentWeaponSlot)
				{
					InitNewWeaponData(CurrentWeaponSlot);
				}

				FActorSpawnParameters WeaponSpawnParams;
				WeaponSpawnParams.Owner = GetOwningCharacter();
				WeaponSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

				WeaponActor->FinishSpawning(OwnerTransform);
				
				OnRep_WeaponInventory();
			}
		}
	}
}

void UZoransWeaponComponent::EquipDroppedWeaponInSlot(const FWeaponInfo WeaponInfo, const int32 InSlot)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}
	
	EquippedWeapons[InSlot].WeaponName = WeaponInfo.WeaponName;
	EquippedWeapons[InSlot].LoadedAmmo = WeaponInfo.LoadedAmmo;

	if (InSlot == CurrentWeaponSlot)
	{
		InitNewWeaponData(CurrentWeaponSlot);
	}
	
	OnRep_WeaponInventory();
}

AZoransWeaponActor* UZoransWeaponComponent::GetCurrentEquippedWeapon() const
{
	if (EquippedWeapons.IsValidIndex(CurrentWeaponSlot))
	{
		return EquippedWeapons[CurrentWeaponSlot].SpawnedWeapon;
	}

	return nullptr;
}

bool UZoransWeaponComponent::HasObjectiveWeaponEquipped() const
{
	return EquippedWeapons.IsValidIndex(ObjectiveWeaponSlot) && IsValid(EquippedWeapons[ObjectiveWeaponSlot].SpawnedWeapon);
}

void UZoransWeaponComponent::CheckCanReloadCurrentWeapon(bool& bCanReload)
{
	bCanReload = false;

	if (!IsValid(GetCurrentEquippedWeapon()))
	{
		return;
	}
	
	const FZoransWeaponData WeaponData = GetCurrentEquippedWeapon()->GetWeaponDefaults();
	
	const int32 AmmoCapacity = WeaponData.AmmoCapacity;
	const int32 LoadedAmmo = GetCurrentEquippedWeapon()->GetLoadedAmmo();

	if (LoadedAmmo >= AmmoCapacity)
	{
		return;
	}
	
	switch (WeaponData.AmmoType)
	{
	case EAmmoType::None :
		{
			return;
		}
	case EAmmoType::Light :
		{
			bCanReload = LightAmmo > 0;
			return;
		}
	case EAmmoType::Medium :
		{
			bCanReload = MediumAmmo > 0;
			return;
		}
	case EAmmoType::Heavy :
		{
			bCanReload = HeavyAmmo > 0;
			return;
		}
	case EAmmoType::Shotgun :
		{
			bCanReload = ShotgunAmmo > 0;
			return;
		}
	case EAmmoType::Sniper :
		{
			bCanReload = SniperAmmo > 0;
			return;
		}
	case EAmmoType::ExplosiveSmall :
		{
			bCanReload = ExplosiveSmallAmmo > 0;
			return;
		}
	case EAmmoType::ExplosiveLarge :
		{
			bCanReload = ExplosiveLargeAmmo > 0;
			return;
		}
	}
}

void UZoransWeaponComponent::TryReloadCurrentWeapon(bool& bWasSuccessful)
{
	bWasSuccessful = false;

	AZoransWeaponActor* CurrentWeapon = GetCurrentEquippedWeapon();
	if (!IsValid(CurrentWeapon))
	{
		return;
	}
	
	const FZoransWeaponData WeaponData = CurrentWeapon->GetWeaponDefaults();
	const int32 AmmoCapacity = WeaponData.AmmoCapacity;
	const int32 LoadedAmmo = CurrentWeapon->GetLoadedAmmo();
	const int32 RequestedAmmo = AmmoCapacity - LoadedAmmo;

	if (RequestedAmmo == 0)
	{
		return;
	}
	
	switch (WeaponData.AmmoType)
	{
	case EAmmoType::None :
		{
			return;
		}
	case EAmmoType::Light :
		{
			if (LightAmmo == 0) { return; }

			bWasSuccessful = true;
			
			if (LightAmmo >= RequestedAmmo)
			{
				CurrentWeapon->SetLoadedAmmo(AmmoCapacity);
				LightAmmo = LightAmmo - RequestedAmmo;
				OnRep_LightAmmo();

				return;
			}
			
			CurrentWeapon->SetLoadedAmmo(LoadedAmmo + LightAmmo);
			LightAmmo = 0;
			OnRep_LightAmmo();
			
			return;
		}
	case EAmmoType::Medium :
		{
			if (MediumAmmo == 0) { return; }

			bWasSuccessful = true;
			
			if (MediumAmmo >= RequestedAmmo)
			{
				CurrentWeapon->SetLoadedAmmo(AmmoCapacity);
				MediumAmmo = MediumAmmo - RequestedAmmo;
				OnRep_MediumAmmo();

				return;
			}
			
			CurrentWeapon->SetLoadedAmmo(LoadedAmmo + MediumAmmo);
			MediumAmmo = 0;
			
			OnRep_MediumAmmo();
		}
	case EAmmoType::Heavy :
		{
			if (HeavyAmmo == 0) { return; }

			bWasSuccessful = true;
			
			if (HeavyAmmo >= RequestedAmmo)
			{
				CurrentWeapon->SetLoadedAmmo(AmmoCapacity);
				HeavyAmmo = HeavyAmmo - RequestedAmmo;
				OnRep_HeavyAmmo();
				
				return;
			}
			
			CurrentWeapon->SetLoadedAmmo(LoadedAmmo + HeavyAmmo);
			HeavyAmmo = 0;
			OnRep_HeavyAmmo();
		}
	case EAmmoType::Shotgun :
		{
			if (ShotgunAmmo == 0) { return; }

			bWasSuccessful = true;
			
			if (ShotgunAmmo >= RequestedAmmo)
			{
				CurrentWeapon->SetLoadedAmmo(AmmoCapacity);
				ShotgunAmmo = ShotgunAmmo - RequestedAmmo;
				OnRep_ShotgunAmmo();
				
				return;
			}
			
			CurrentWeapon->SetLoadedAmmo(LoadedAmmo + ShotgunAmmo);
			ShotgunAmmo = 0;
			OnRep_ShotgunAmmo();
		}
	case EAmmoType::Sniper :
		{
			if (SniperAmmo == 0) { return; }

			bWasSuccessful = true;
			
			if (SniperAmmo >= RequestedAmmo)
			{
				CurrentWeapon->SetLoadedAmmo(AmmoCapacity);
				SniperAmmo = SniperAmmo - RequestedAmmo;
				OnRep_SniperAmmo();
				
				return;
			}
			
			CurrentWeapon->SetLoadedAmmo(LoadedAmmo + SniperAmmo);
			SniperAmmo = 0;
			OnRep_SniperAmmo();
		}
	case EAmmoType::ExplosiveSmall :
		{
			if (ExplosiveSmallAmmo == 0) { return; }

			bWasSuccessful = true;
			
			if (ExplosiveSmallAmmo >= RequestedAmmo)
			{
				CurrentWeapon->SetLoadedAmmo(AmmoCapacity);
				ExplosiveSmallAmmo = ExplosiveSmallAmmo - RequestedAmmo;
				OnRep_ExplosiveSmallAmmo();

				return;
			}
			
			CurrentWeapon->SetLoadedAmmo(LoadedAmmo + ExplosiveSmallAmmo);
			ExplosiveSmallAmmo = 0;
			OnRep_ExplosiveSmallAmmo();
		}
	case EAmmoType::ExplosiveLarge :
		{
			if (ExplosiveLargeAmmo == 0) { return; }

			bWasSuccessful = true;
			
			if (ExplosiveLargeAmmo >= RequestedAmmo)
			{
				CurrentWeapon->SetLoadedAmmo(AmmoCapacity);
				ExplosiveLargeAmmo = ExplosiveLargeAmmo - RequestedAmmo;
				OnRep_ExplosiveLargeAmmo();

				return;
			}
			
			CurrentWeapon->SetLoadedAmmo(LoadedAmmo + ExplosiveLargeAmmo);
			ExplosiveLargeAmmo = 0;
			OnRep_ExplosiveLargeAmmo();
		}
	}
}

void UZoransWeaponComponent::TryUseLoadedAmmo(const int32 Amount, bool& bWasSuccessful)
{
	const int32 LoadedAmmo = EquippedWeapons[CurrentWeaponSlot].LoadedAmmo;
	
	if (LoadedAmmo < Amount)
	{
		bWasSuccessful = false;
		
		return;
	}

	bWasSuccessful = true;
	
	EquippedWeapons[CurrentWeaponSlot].LoadedAmmo = LoadedAmmo - Amount;
}

AZoransWeaponActor* UZoransWeaponComponent::GetWeaponActor(const int32 InSlot) const
{
	return EquippedWeapons[InSlot].SpawnedWeapon;
}

void UZoransWeaponComponent::RemoveWeaponInSlot(const int32 InSlot)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}
	
	const FWeaponInfo BlankWeaponInfo;
	
	if (AZoransWeaponActor* WeaponInSlot = GetWeaponActor(InSlot))
	{
		WeaponInSlot->Destroy();
	}
	EquippedWeapons[InSlot] = BlankWeaponInfo;

	OnRep_WeaponInventory();
}

void UZoransWeaponComponent::RemoveAllEquippedWeapons()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	for (auto WeaponInfo : EquippedWeapons)
	{
		if (IsValid(WeaponInfo.SpawnedWeapon))
		{
			WeaponInfo.SpawnedWeapon->Destroy();
		}
	}

	EquippedWeapons.Empty();
	EquippedWeapons.AddDefaulted(InventorySize);

	CurrentWeaponSlot = -1;
	LightAmmo = 0;
	MediumAmmo = 0;
	HeavyAmmo = 0;
	ShotgunAmmo = 0;
	SniperAmmo = 0;
	ExplosiveSmallAmmo = 0;
	ExplosiveLargeAmmo = 0;

	OnRep_LightAmmo();
	OnRep_MediumAmmo();
	OnRep_HeavyAmmo();
	OnRep_ShotgunAmmo();
	OnRep_SniperAmmo();
	OnRep_ExplosiveSmallAmmo();
	OnRep_ExplosiveLargeAmmo();

	OnRep_WeaponInventory();
	OnRep_CurrentWeaponSlot();
}

void UZoransWeaponComponent::RemoveCurrentWeaponData()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}
	
	AbilitySystemComponent->ClearAbility(PrimaryWeaponAbilitySpec);
	AbilitySystemComponent->ClearAbility(SecondaryWeaponAbilitySpec);

	for (const FGameplayAbilitySpecHandle AddedSpec : AddedAbilitySpecHandles)
	{
		AbilitySystemComponent->ClearAbility(AddedSpec);
	}

	for (const FActiveGameplayEffectHandle ActiveEffect : CurrentWeaponEffects)
	{
		AbilitySystemComponent->RemoveActiveGameplayEffect(ActiveEffect);
	}

	PrimaryWeaponAbilitySpec = FGameplayAbilitySpecHandle();
	SecondaryWeaponAbilitySpec = FGameplayAbilitySpecHandle();

	AddedAbilitySpecHandles.Empty();
	CurrentWeaponEffects.Empty();
}

void UZoransWeaponComponent::OnRep_WeaponInventory() const
{
	for (int32 x = 0; x < EquippedWeapons.Num(); x++)
	{
		if (CurrentWeaponSlot != x)
		{
			if (EquippedWeapons.IsValidIndex(x) && IsValid(EquippedWeapons[x].SpawnedWeapon))
			{
				EquippedWeapons[x].SpawnedWeapon->InternalStowWeapon(true);
			}
		}
		else
		{
			if (EquippedWeapons.IsValidIndex(x) && IsValid(EquippedWeapons[x].SpawnedWeapon))
			{
				EquippedWeapons[x].SpawnedWeapon->InternalReadyWeapon();
			}
		}
	}
	
	if (WeaponInventoryChanged.IsBound())
	{
		WeaponInventoryChanged.Broadcast();
	}
	
	if (CurrentWeaponSlotChanged.IsBound())
	{
		CurrentWeaponSlotChanged.Broadcast();
	}
}

void UZoransWeaponComponent::OnRep_CurrentWeaponSlot()
{
	for (int32 x = 0; x < EquippedWeapons.Num(); x++)
	{
		if (CurrentWeaponSlot != x)
		{
			if (EquippedWeapons.IsValidIndex(x) && IsValid(EquippedWeapons[x].SpawnedWeapon))
			{
				EquippedWeapons[x].SpawnedWeapon->InternalStowWeapon();
			}
		}
		else
		{
			if (EquippedWeapons.IsValidIndex(x) && IsValid(EquippedWeapons[x].SpawnedWeapon))
			{
				EquippedWeapons[x].SpawnedWeapon->InternalReadyWeapon();
			}
		}
	}
	
	InitNewWeaponData(CurrentWeaponSlot);

	if (CurrentWeaponSlotChanged.IsBound())
	{
		CurrentWeaponSlotChanged.Broadcast();
	}
}

void UZoransWeaponComponent::OnRep_LightAmmo() const
{
	if (LightAmmoChanged.IsBound())
	{
		LightAmmoChanged.Broadcast();
	}
}

void UZoransWeaponComponent::OnRep_MediumAmmo() const
{
	if (MediumAmmoChanged.IsBound())
	{
		MediumAmmoChanged.Broadcast();
	}
}

void UZoransWeaponComponent::OnRep_HeavyAmmo() const
{
	if (HeavyAmmoChanged.IsBound())
	{
		HeavyAmmoChanged.Broadcast();
	}
}

void UZoransWeaponComponent::OnRep_ShotgunAmmo() const
{
	if (ShotgunAmmoChanged.IsBound())
	{
		ShotgunAmmoChanged.Broadcast();
	}
}

void UZoransWeaponComponent::OnRep_SniperAmmo() const
{
	if (SniperAmmoChanged.IsBound())
	{
		SniperAmmoChanged.Broadcast();
	}
}

void UZoransWeaponComponent::OnRep_ExplosiveSmallAmmo() const
{
	if (ExplosiveSmallAmmoChanged.IsBound())
	{
		ExplosiveSmallAmmoChanged.Broadcast();
	}
}

void UZoransWeaponComponent::OnRep_ExplosiveLargeAmmo() const
{
	if (ExplosiveLargeAmmoChanged.IsBound())
	{
		ExplosiveLargeAmmoChanged.Broadcast();
	}
}

void UZoransWeaponComponent::DestroyComponent(bool bPromoteChildren)
{
	WeaponInventoryChanged.Clear();
	CurrentWeaponSlotChanged.Clear();
	LightAmmoChanged.Clear();
	MediumAmmoChanged.Clear();
	HeavyAmmoChanged.Clear();
	ShotgunAmmoChanged.Clear();
	SniperAmmoChanged.Clear();
	ExplosiveSmallAmmoChanged.Clear();
	ExplosiveLargeAmmoChanged.Clear();
	
	Super::DestroyComponent(bPromoteChildren);
}

void UZoransWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	for (auto &WeaponInfo : EquippedWeapons)
	{
		if (IsValid(WeaponInfo.SpawnedWeapon))
		{
			WeaponInfo.SpawnedWeapon->Destroy();
		}
	}
}

bool UZoransWeaponComponent::TrySetWeaponSlot(const int32 InSlot)
{
	if (InSlot != CurrentWeaponSlot)
	{
		InternalSetWeaponSlot(InSlot);

		return true;
	}

	return false;
}

int32 UZoransWeaponComponent::GetNextWeaponSlot(const bool bIncrease) const
{
	if(HasObjectiveWeaponEquipped())
	{
		return ObjectiveWeaponSlot;	
	}
	
	if (bIncrease)
	{
		for (int32 Index = CurrentWeaponSlot + 1; Index < EquippedWeapons.Num(); ++Index)
		{
			if (Index != ObjectiveWeaponSlot && EquippedWeapons.IsValidIndex(Index) && EquippedWeapons[Index].WeaponName != WeaponNames::Unarmed)
			{
				return Index;
			}
		}

		for (int32 Index = 0; Index < CurrentWeaponSlot; ++Index)
		{
			if (Index != ObjectiveWeaponSlot && EquippedWeapons.IsValidIndex(Index) && EquippedWeapons[Index].WeaponName != WeaponNames::Unarmed)
			{
				return Index;
			}
		}
	}
	else
	{
		for (int32 Index = CurrentWeaponSlot - 1; Index > -1; --Index)
		{
			if (Index != ObjectiveWeaponSlot && EquippedWeapons.IsValidIndex(Index) && EquippedWeapons[Index].WeaponName != WeaponNames::Unarmed)
			{
				return Index;
			}
		}

		for (int32 Index = EquippedWeapons.Num() - 1; Index > CurrentWeaponSlot; --Index)
		{
			if (Index != ObjectiveWeaponSlot && EquippedWeapons.IsValidIndex(Index) && EquippedWeapons[Index].WeaponName != WeaponNames::Unarmed)
			{
				return Index;
			}
		}
	}

	return CurrentWeaponSlot;
}

void UZoransWeaponComponent::InternalSetWeaponSlot_Implementation(const int32 InWeaponSlot)
{
	if (CurrentWeaponSlot != InWeaponSlot && EquippedWeapons.IsValidIndex(InWeaponSlot))
	{
		CurrentWeaponSlot = InWeaponSlot;
		OnRep_CurrentWeaponSlot();
	}
}