// Copyright Zollpa LLC.


#include "ZoransWeaponActor.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"
#include "ZoransResistance/AbilitySystem/Data/ZoransGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"
#include "ZoransResistance/ZoransResistance.h"
#include "ZoransResistance/Characters/ZoransCharacterBase.h"
#include "ZoransResistance/Characters/Components/ZoransWeaponComponent.h"

AZoransWeaponActor::AZoransWeaponActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	NetUpdateFrequency = 15.f;
	
	// WeaponData.DataTable = Cast<UDataTable>(StaticLoadObject(UDataTable::StaticClass(), nullptr, TEXT("/Game/Zorans/DataTables/DT_Weapons.DT_Weapons")));
	// WeaponData.RowName = "Unarmed";
	//@todo replace above with static

	WeaponRoot = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponRoot"));
	RootComponent = WeaponRoot;

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(WeaponRoot);
}

void AZoransWeaponActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (CurrentRecoil > 0.f)
	{
		CurrentRecoil = FMath::FInterpConstantTo(CurrentRecoil, 0.f, DeltaSeconds, WeaponDefaults.RecoilRecoverySpeed);
	}

	const float CurrentSpeed = GetOwningCharacter() ? GetOwningCharacter()->GetVelocity().Length() : GetVelocity().Length();
	const float VelocitySpread = CurrentSpeed > 0.f ? FMath::Clamp<float>(CurrentSpeed * 0.0025f * WeaponDefaults.SpreadFromMovement, 0.f, WeaponDefaults.SpreadFromMovement) : 0.f;
	AimingModifier = FMath::FInterpTo(AimingModifier, TargetAimingModifier * (bIsCrouching ? WeaponDefaults.CrouchingSpreadModifier : 1.f), DeltaSeconds, WeaponDefaults.SpreadRecoverySpeed);
	SpreadFromMovement = FMath::FInterpTo(SpreadFromMovement, VelocitySpread, DeltaSeconds, WeaponDefaults.SpreadRecoverySpeed);
	CurrentSpread = FMath::FInterpConstantTo(CurrentSpread, 0.f, DeltaSeconds, WeaponDefaults.SpreadRecoverySpeed);

	if (GetReticleWidget())
	{
		GetReticleWidget()->SetReticleSpread(GetCurrentSpread());
	}
}

USkeletalMeshComponent* AZoransWeaponActor::GetWeaponMesh() const
{
	return WeaponMesh;
}

void AZoransWeaponActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(AZoransWeaponActor, bCueActive, COND_SkipOwner, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(AZoransWeaponActor, LoadedAmmo, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION(AZoransWeaponActor, OwningCharacter, COND_InitialOnly);
}

void AZoransWeaponActor::ApplyRecoil(const float Modifier, const bool bCheckAmmo)
{
	CurrentRecoil = FMath::Clamp<float>(CurrentRecoil + WeaponDefaults.RecoilPerShot * Modifier * AimingModifier, 0.f, WeaponDefaults.RecoilMaximum);
	CurrentSpread = FMath::Clamp<float>(CurrentSpread + WeaponDefaults.SpreadPerShot * Modifier * AimingModifier, 0.f, WeaponDefaults.SpreadMaximum);

	if (WeaponDefaults.RecoilEffect && IsValid(GetOwningCharacter()))
	{
		if (APlayerController* PC = GetOwningCharacter()->GetController<APlayerController>())
		{
			const FRotator RecoilRotation = WeaponDefaults.RecoilDirection.Rotation();
			PC->ClientStartCameraShake(WeaponDefaults.RecoilEffect, CurrentRecoil, ECameraShakePlaySpace::CameraLocal);
			if (WeaponDefaults.RecoilAimAdjustment > 0.f)
			{
				PC->SetControlRotation(PC->GetControlRotation() + RecoilRotation * CurrentRecoil * AimingModifier * (WeaponDefaults.RecoilAimAdjustment * 0.001f));
			}
		}

		if (bCheckAmmo && LoadedAmmo <= 0)
		{
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetOwningCharacter(), GameplayTag_Reload, FGameplayEventData());
		}
	}
}

float AZoransWeaponActor::GetCurrentRecoil() const
{
	const float RecoilAlpha = UKismetMathLibrary::SafeDivide(CurrentRecoil, WeaponDefaults.RecoilMaximum);
	return WeaponDefaults.RecoilCurve.EditorCurveData.Eval(RecoilAlpha) * WeaponDefaults.RecoilMaximum;
}

float AZoransWeaponActor::GetCurrentSpread() const
{
	const float SpreadAlpha = UKismetMathLibrary::SafeDivide(CurrentSpread, WeaponDefaults.SpreadMaximum);
	return (WeaponDefaults.SpreadCurve.EditorCurveData.Eval(SpreadAlpha) * WeaponDefaults.SpreadMaximum + SpreadFromMovement) * AimingModifier;
}

void AZoransWeaponActor::ToggleWeaponAiming(const FGameplayTag InTag, int32 TagCount)
{
	bIsAiming = TagCount > 0;

	TargetAimingModifier = bIsAiming ? WeaponDefaults.ScopedSpreadModifier : 1.f;
	
	if (GetReticleWidget())
	{
		GetReticleWidget()->AimingModeChanged(bIsAiming);
	}
}

void AZoransWeaponActor::ToggleCrouching(const FGameplayTag InTag, int32 TagCount)
{
	bIsCrouching = TagCount > 0 ? true : false;
}

void AZoransWeaponActor::InternalReadyWeapon()
{
	if (!bWeaponReady)
	{
		bWeaponReady = true;
		
		SetActorTickEnabled(true);
		if (GetOwningCharacter() && IsValid(WeaponDefaults.CharacterAnimationClass))
		{
			if (UAnimInstance* CharAnim = GetOwningCharacter()->GetMesh()->GetAnimInstance())
			{
				CharAnim->LinkAnimClassLayers(WeaponDefaults.CharacterAnimationClass);
			}
		}

		if (GetReticleWidget())
		{
			GetReticleWidget()->ReticleOpened(true);
			//GetReticleWidget()->SetVisibility(ESlateVisibility::HitTestInvisible);
		}

		ReadyWeapon();
	}
}

void AZoransWeaponActor::InternalStowWeapon(const bool bForceStow)
{
	if (bWeaponReady || bForceStow)
	{
		bWeaponReady = false;
		
		SetActorTickEnabled(false);
		if (GetReticleWidget())
		{
			GetReticleWidget()->ReticleOpened(false);
		}
		StowWeapon();
	}
}

void AZoransWeaponActor::SetLoadedAmmo(const int32 InAmmo)
{
	LoadedAmmo = InAmmo;
	LocalAmmo = InAmmo;
	
	if (OnLoadedAmmoChangeDelegate.IsBound())
	{
		OnLoadedAmmoChangeDelegate.Broadcast(LoadedAmmo);
	}

	if (OnLocalAmmoChangeDelegate.IsBound())
	{
		OnLocalAmmoChangeDelegate.Broadcast(LocalAmmo);
	}
}

void AZoransWeaponActor::SetReticleIndex(const int32 InIndex)
{
	if (GetReticleWidget())
	{
		GetReticleWidget()->SetReticleIndex(InIndex);
	}
}

void AZoransWeaponActor::ModifyReticleIndex(const bool bIncrease)
{
	if (GetReticleWidget() && GetReticleWidget()->GetReticleMaxIndex() > 0)
	{
		GetReticleWidget()->ModifyReticleIndex(bIncrease);
	}
}

void AZoransWeaponActor::BeginPlay()
{
	if (IsValid(WeaponData.DataTable) && WeaponData.DataTable->GetRowNames().Contains(WeaponData.RowName))
	{
		WeaponDefaults = *WeaponData.DataTable->FindRow<FZoransWeaponData>(WeaponData.RowName, "");
	}

	if (GetOwningCharacter() && GetOwningCharacter()->IsLocallyControlled())
	{
		if (UAbilitySystemComponent* ASC = GetOwningCharacter()->GetAbilitySystemComponent())
		{
			ToggleCrouching(GameplayTag_Crouching.GetTag(), ASC->HasMatchingGameplayTag(GameplayTag_Crouching.GetTag()));
			CrouchTagChangedDelegate = ASC->RegisterGameplayTagEvent(GameplayTag_Crouching.GetTag()).AddUObject(this, &AZoransWeaponActor::ToggleCrouching);

			ToggleWeaponAiming(GameplayTag_Aiming.GetTag(), ASC->HasMatchingGameplayTag(GameplayTag_Aiming.GetTag()));
			AimingTagChangedDelegate = ASC->RegisterGameplayTagEvent(GameplayTag_Aiming.GetTag()).AddUObject(this, &AZoransWeaponActor::ToggleWeaponAiming);
		}
		
		if (IsValid(WeaponDefaults.ReticleWidgetClass))
		{
			if (APlayerController* PC = GetOwningCharacter()->GetController<APlayerController>())
			{
				ReticleWidget = CreateWidget<UReticleWidgetBase>(PC, WeaponDefaults.ReticleWidgetClass);
				if (GetReticleWidget())
				{
					GetReticleWidget()->SetOwningWeapon(this);
					GetReticleWidget()->SetVisibility(ESlateVisibility::Collapsed);
					GetReticleWidget()->AddToViewport(-1);
				}
			}
		}
	}
	
	//bool bCurrentWeapon = false;
	// if (GetOwningCharacter())
	// {
	// 	if (UZoransWeaponComponent* WeaponComponent = GetOwningCharacter()->GetWeaponComponent())
	// 	{
	// 		if (WeaponComponent->EquippedWeapons.IsValidIndex(0) && WeaponComponent->EquippedWeapons[0].WeaponName == WeaponData.RowName)
	// 		{
	// 			UE_LOG(ZoransLog, Warning, TEXT("WeaponActor -> IsCurrentWeapon (%s == %s)"), *WeaponComponent->EquippedWeapons[0].WeaponName.ToString(), *WeaponData.RowName.ToString());
	// 			//bCurrentWeapon = true;
	// 			InternalReadyWeapon();
	// 		}
	// 	}
	// }
	
	// if (!bCurrentWeapon)
	// {
	// 	InternalStowWeapon();
	// }
	
	Super::BeginPlay();
}

void AZoransWeaponActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetOwningCharacter())
	{
		if (UAbilitySystemComponent* ASC = GetOwningCharacter()->GetAbilitySystemComponent())
		{
			ASC->UnregisterGameplayTagEvent(CrouchTagChangedDelegate, GameplayTag_Crouching.GetTag());
			ASC->UnregisterGameplayTagEvent(AimingTagChangedDelegate, GameplayTag_Aiming.GetTag());
		}
	}

	if (GetReticleWidget())
	{
		GetReticleWidget()->RemoveFromParent();
	}

	Super::EndPlay(EndPlayReason);
}

FTransform AZoransWeaponActor::GetWeaponMuzzleTransform_Implementation() const
{
	return GetActorTransform();
}

FTransform AZoransWeaponActor::GetWeaponHandPlacementTransform_Implementation() const
{
	return GetActorTransform();
}

FVector AZoransWeaponActor::GetWeaponTracePoint_Implementation() const
{
	return GetActorLocation();
}

void AZoransWeaponActor::TriggerWeaponCue_WithHitscanData_Implementation(const TArray<FHitscanData>& Hits)
{
	InternalWeaponCue_WithHitscanData(Hits);
}

void AZoransWeaponActor::InternalWeaponCue_WithHitscanData(const TArray<FHitscanData>& Hits)
{
	if (const AZoransCharacterBase* WeaponOwner = GetOwningCharacter())
	{
		if (WeaponOwner->IsLocallyControlled())
		{
			if (WeaponOwner->IsPlayerControlled())
			{
				if (WeaponOwner->GetNetMode() == NM_ListenServer)
				{
					WeaponCue_WithHitscanData(Hits);
				}
				else if (bReceivesServerHitEffect)
				{
					WeaponCue_WithHitscanData(Hits);
				}
			}
			else
			{
				WeaponCue_WithHitscanData(Hits);
			}
		}
		else
		{
			WeaponCue_WithHitscanData(Hits);
		}
	}
}

void AZoransWeaponActor::WeaponCue_WithHitscanData_Implementation(const TArray<FHitscanData>& Hits)
{
	UE_LOG(LogTemp, Error, TEXT("ZoransWeaponActor->WeaponCue_WithHitscanData IS BEING CALLED BUT IS NOT IMPLEMENTED IN THIS WEAPON"));
}

void AZoransWeaponActor::WeaponEffect_Local_Implementation()
{
	UE_LOG(LogTemp, Error, TEXT("ZoransWeaponActor->WeaponEffect_Local IS BEING CALLED BUT IS NOT IMPLEMENTED IN THIS WEAPON"));
}

void AZoransWeaponActor::ToggleActiveWeaponCue(const bool bActive)
{
	bCueActive = bActive;
	if (GetNetMode() < NM_Client || (GetOwningCharacter() && GetOwningCharacter()->IsLocallyControlled()))
	{
		if (bActive)
		{
			WeaponCue_Activated();
		}
		else
		{
			WeaponCue_Deactivated();
		}
	}
}

void AZoransWeaponActor::SetOwningCharacter(AZoransCharacterBase* InOwningCharacter)
{
	OwningCharacter = InOwningCharacter;
}

AZoransCharacterBase* AZoransWeaponActor::GetOwningCharacter()
{
	if (!OwningCharacter.Get())
	{
		if (GetOwner())
		{
			OwningCharacter = Cast<AZoransCharacterBase>(GetOwner());
		}
	}

	return OwningCharacter.Get();
}

void AZoransWeaponActor::OnRep_bCueActive()
{
	if (bCueActive)
	{
		WeaponCue_Activated();
	}
	else
	{
		WeaponCue_Deactivated();
	}
}

void AZoransWeaponActor::WeaponCue_Activated_Implementation()
{
	//UE_LOG(LogTemp, Error, TEXT("ZoransWeaponActor->WeaponCue_Activated IS BEING CALLED BUT IS NOT IMPLEMENTED IN THIS WEAPON"));
}

void AZoransWeaponActor::WeaponCue_Deactivated_Implementation()
{
	//UE_LOG(LogTemp, Error, TEXT("ZoransWeaponActor->WeaponCue_Deactivated IS BEING CALLED BUT IS NOT IMPLEMENTED IN THIS WEAPON"));
}

void AZoransWeaponActor::OnRep_LoadedAmmo()
{
	LoadedAmmoChanged(LoadedAmmo);
	LocalAmmo = LoadedAmmo;

	if (OnLoadedAmmoChangeDelegate.IsBound())
	{
		OnLoadedAmmoChangeDelegate.Broadcast(LoadedAmmo);
	}

	if (OnLocalAmmoChangeDelegate.IsBound())
	{
		OnLocalAmmoChangeDelegate.Broadcast(LocalAmmo);
	}

	if (LoadedAmmo <= 0 && OnLoadedAmmoEmpty.IsBound())
	{
		OnLoadedAmmoEmpty.Broadcast();
	}
}

bool AZoransWeaponActor::TryUseLoadedAmmo(const int32 Amount)
{
	if (LoadedAmmo < Amount)
	{
		UE_LOG(LogTemp, Warning, TEXT("TryUseLoadedAmmo :: Insufficient Ammo"));
		return false;
	}

	LoadedAmmo -= Amount;
	OnRep_LoadedAmmo();
		
	return true;
}

void AZoransWeaponActor::ConsumeLocalAmmo(const int32 Amount)
{
	LocalAmmo = FMath::Clamp<int32>(LocalAmmo, 0, LocalAmmo - Amount);

	if (OnLocalAmmoChangeDelegate.IsBound())
	{
		OnLocalAmmoChangeDelegate.Broadcast(LocalAmmo);
	}
}