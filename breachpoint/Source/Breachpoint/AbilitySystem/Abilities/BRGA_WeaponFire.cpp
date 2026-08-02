#include "AbilitySystem/Abilities/BRGA_WeaponFire.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Core/BRCore.h"
#include "Core/BRGameplayTags.h"
#include "AbilitySystem/BRAbilitySystemComponent.h"
#include "AbilitySystem/Effects/BRGameplayEffects.h"
#include "Data/BRDataRows.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Weapons/BREquipmentComponent.h"
#include "Weapons/BRWeaponInstance.h"

namespace
{
	const FName HeadBoneName(TEXT("head"));

	constexpr float RangeToleranceUU = 50.f;
}

UBRGA_WeaponFire::UBRGA_WeaponFire(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	ActivationPolicy = EBRAbilityActivationPolicy::OnInputPressed;

	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(BRGameplayTags::Ability_Weapon_Fire);
	SetAssetTags(AssetTags);

	CancelAbilitiesWithTag.AddTag(BRGameplayTags::Ability_Sprint);

	CooldownTag = BRGameplayTags::Ability_Weapon_Fire;

	bCommitOnActivate = true;
}

bool UBRGA_WeaponFire::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const UBRWeaponInstance* Weapon = GetActiveWeapon();
	if (!Weapon)
	{
		return false;
	}

	if (Weapon->GetAmmoInMag() <= 0)
	{
		return false;
	}

	const FBRWeaponRow* Row = Weapon->GetRow();
	if (!Row)
	{
		return false;
	}

	if (Row->DamageDelivery != EBRDamageDelivery::Hitscan)
	{
		return false;
	}

	if (Row->Range_m <= 0.f)
	{
		return false;
	}

	return true;
}

float UBRGA_WeaponFire::GetCooldownDurationSeconds() const
{
	const FBRWeaponRow* Row = GetWeaponRow();
	if (!Row || Row->RPM <= 0.f)
	{
		return 0.f;
	}
	return 60.f / Row->RPM;
}

void UBRGA_WeaponFire::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive())
	{
		return;
	}

	bShotResolved = false;

	UBRAbilitySystemComponent* ASC = GetBRAbilitySystemComponent();
	if (!ASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (IsLocallyControlled())
	{
		FireLocally();
		return;
	}

	TargetDataDelegateHandle = ASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey())
		.AddUObject(this, &UBRGA_WeaponFire::OnTargetDataReady);
	ASC->CallReplicatedTargetDataDelegatesIfSet(Handle, ActivationInfo.GetActivationPredictionKey());
}

void UBRGA_WeaponFire::FireLocally()
{
	const FBRWeaponRow* Row = GetWeaponRow();
	UBRAbilitySystemComponent* ASC = GetBRAbilitySystemComponent();
	if (!Row || !ASC)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	FVector ViewLocation, ViewDirection;
	if (!GetViewPoint(ViewLocation, ViewDirection))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	const FHitResult Hit = TraceShot(*Row, ViewLocation, ViewDirection);

	FGameplayAbilityTargetData_SingleTargetHit* HitData = new FGameplayAbilityTargetData_SingleTargetHit(Hit);
	FGameplayAbilityTargetDataHandle DataHandle;
	DataHandle.Add(HitData);

	{
		FScopedPredictionWindow ScopedPrediction(ASC, CurrentActivationInfo.GetActivationPredictionKey());

		ASC->ServerSetReplicatedTargetData(
			CurrentSpecHandle,
			CurrentActivationInfo.GetActivationPredictionKey(),
			DataHandle,
			FGameplayTag(),
			ASC->ScopedPredictionKey);
	}

	OnTargetDataReady(DataHandle, FGameplayTag());
}

void UBRGA_WeaponFire::OnTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetData, FGameplayTag ApplicationTag)
{
	if (bShotResolved)
	{
		return;
	}
	bShotResolved = true;

	UBRAbilitySystemComponent* ASC = GetBRAbilitySystemComponent();
	const FBRWeaponRow* Row = GetWeaponRow();
	if (!ASC || !Row)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	if (ASC->GetOwnerRole() == ROLE_Authority)
	{
		ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
	}

	const FGameplayAbilityTargetData* Data = TargetData.Get(0);
	const FHitResult* Claim = Data ? Data->GetHitResult() : nullptr;

	if (Row->FireCueTag.IsValid())
	{
		FGameplayCueParameters CueParams;
		CueParams.Location = Claim ? Claim->TraceStart : FVector::ZeroVector;
		CueParams.Instigator = GetAvatarActorFromActorInfo();

		if (Claim)
		{
			FGameplayEffectContextHandle CueContext = ASC->MakeEffectContext();
			CueContext.AddInstigator(GetAvatarActorFromActorInfo(), GetAvatarActorFromActorInfo());
			CueContext.AddHitResult(*Claim);
			CueParams.EffectContext = CueContext;
		}

		ASC->ExecuteGameplayCue(Row->FireCueTag, CueParams);
	}

	if (HasAuthority(&CurrentActivationInfo))
	{
		bool bAccepted = false;
		FString Reason;

		if (!Claim)
		{
			Reason = TEXT("no hit result in the client's target data");
		}
		else if (ValidateClaim(*Claim, *Row, Reason))
		{
			UBRWeaponInstance* Weapon = GetActiveWeapon();

			if (Weapon && Weapon->ConsumeAmmoForShot())
			{
				bAccepted = true;
				ApplyDamage(*Claim, *Row);
			}
			else
			{
				Reason = TEXT("server had no ammo for this shot");
			}
		}

	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

bool UBRGA_WeaponFire::ValidateClaim(const FHitResult& Claim, const FBRWeaponRow& Row, FString& OutReason) const
{
	FVector ServerLocation, ServerDirection;
	if (!GetViewPoint(ServerLocation, ServerDirection))
	{
		OutReason = TEXT("server could not resolve the shooter's view point");
		return false;
	}

	const float MaxRangeUU = Row.Range_m * BRUnits::MetresToUU + RangeToleranceUU;
	const float ClaimedDistance = FVector::Dist(ServerLocation, Claim.ImpactPoint);
	if (ClaimedDistance > MaxRangeUU)
	{
		OutReason = FString::Printf(
			TEXT("claimed impact at %.0f uu exceeds Range_m %.1f m (+tolerance) = %.0f uu"),
			ClaimedDistance, Row.Range_m, MaxRangeUU);
		return false;
	}

	const FVector ToImpact = (Claim.ImpactPoint - ServerLocation).GetSafeNormal();
	if (!ToImpact.IsNearlyZero())
	{
		const float CosAngle = FVector::DotProduct(ServerDirection, ToImpact);
		const float AcceptHalfAngleDeg = Row.Spread_deg > 0.f ? Row.Spread_deg : 5.f;
		const float MinCos = FMath::Cos(FMath::DegreesToRadians(FMath::Min(AcceptHalfAngleDeg + 2.f, 89.f)));
		if (CosAngle < MinCos)
		{
			OutReason = FString::Printf(
				TEXT("claimed direction is %.1f deg off the server's view; Spread_deg is %.2f"),
				FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(CosAngle, -1.f, 1.f))), Row.Spread_deg);
			return false;
		}
	}

	return true;
}

void UBRGA_WeaponFire::ApplyDamage(const FHitResult& Hit, const FBRWeaponRow& Row) const
{
	UBRAbilitySystemComponent* SourceASC = GetBRAbilitySystemComponent();
	AActor* HitActor = Hit.GetActor();
	if (!SourceASC || !HitActor)
	{
		return;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
	if (!TargetASC || TargetASC == SourceASC)
	{
		return;
	}

	FGameplayTagContainer DamageTags;
	DamageTags.AddTag(BRGameplayTags::Damage_Kinetic);
	if (Hit.BoneName == HeadBoneName)
	{
		DamageTags.AddTag(BRGameplayTags::Damage_Headshot);
	}

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddInstigator(GetAvatarActorFromActorInfo(), GetAvatarActorFromActorInfo());
	Context.AddHitResult(Hit);

	const FGameplayEffectSpecHandle Spec = UBRGE_Damage::MakeSpec(SourceASC, Row.DamagePerShot, DamageTags, Context);
	UBRGE_Damage::ApplyToTarget(Spec, SourceASC, TargetASC);
}

void UBRGA_WeaponFire::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (TargetDataDelegateHandle.IsValid())
	{
		if (UBRAbilitySystemComponent* ASC = GetBRAbilitySystemComponent())
		{
			ASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey())
				.Remove(TargetDataDelegateHandle);
		}
		TargetDataDelegateHandle.Reset();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

UBRWeaponInstance* UBRGA_WeaponFire::GetActiveWeapon() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return nullptr;
	}
	const UBREquipmentComponent* Equipment = Avatar->FindComponentByClass<UBREquipmentComponent>();
	return Equipment ? Equipment->GetActiveWeapon() : nullptr;
}

const FBRWeaponRow* UBRGA_WeaponFire::GetWeaponRow() const
{
	const UBRWeaponInstance* Weapon = GetActiveWeapon();
	return Weapon ? Weapon->GetRow() : nullptr;
}

bool UBRGA_WeaponFire::GetViewPoint(FVector& OutLocation, FVector& OutDirection) const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return false;
	}

	FRotator ViewRotation;
	Avatar->GetActorEyesViewPoint(OutLocation, ViewRotation);
	OutDirection = ViewRotation.Vector();
	return true;
}

FHitResult UBRGA_WeaponFire::TraceShot(const FBRWeaponRow& Row, const FVector& From, const FVector& Direction) const
{
	FHitResult Hit;
	const UWorld* World = GetWorld();
	if (!World)
	{
		return Hit;
	}

	const FVector To = From + Direction * (Row.Range_m * BRUnits::MetresToUU);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BRWeaponFire), true);
	Params.AddIgnoredActor(GetAvatarActorFromActorInfo());
	Params.bReturnPhysicalMaterial = true;

	World->LineTraceSingleByChannel(Hit, From, To, BRCollision::WeaponTrace, Params);

	if (!Hit.bBlockingHit)
	{
		Hit.TraceStart = From;
		Hit.TraceEnd = To;
		Hit.ImpactPoint = To;
	}
	return Hit;
}
