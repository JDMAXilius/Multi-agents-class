// Copyright Zollpa LLC.


#include "ZoransGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "EnhancedInputComponent.h"
#include "ZoransAbilitySystemComponent.h"
#include "ZoransGameplayEffect.h"
#include "Data/AbilitySystemData.h"
#include "Data/ZoransGameplayTags.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/KismetMathLibrary.h"
#include "ZoransResistance/Characters/ZoransCharacterBase.h"
#include "ZoransResistance/Interfaces/LocationMagnitudeInterface.h"
#include "ZoransResistance/Teams/TeamStateFunctionLibrary.h"
#include "ZoransResistance/Teams/Data/TeamData.h"
#include "ZoransResistance/Widgets/ZoransAbilityCooldownWidget.h"


UZoransGameplayAbility::UZoransGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo;
	
	ActivationBlockedTags.AddTag(GameplayTag_Dead.GetTag());
	ActivationBlockedTags.AddTag(GameplayTag_Stunned.GetTag());
}

void UZoransGameplayAbility::PreActivate(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate, const FGameplayEventData* TriggerEventData)
{
	Super::PreActivate(Handle, ActorInfo, ActivationInfo, OnGameplayAbilityEndedDelegate, TriggerEventData);

	if (bFirstActivation)
	{
		PreFirstActivation();
	}
}

void UZoransGameplayAbility::PreFirstActivation()
{
	bFirstActivation = false;
	
	GetOwningWeapon();
}

AZoransCharacterBase* UZoransGameplayAbility::GetOwningCharacter()
{
	if (IsValid(OwningCharacter.Get()))
	{
		return OwningCharacter.Get();
	}

	OwningCharacter = Cast<AZoransCharacterBase>(GetAvatarActorFromActorInfo());

	return OwningCharacter.Get();
}

AZoransWeaponActor* UZoransGameplayAbility::GetOwningWeapon()
{
	if (IsValid(ZoransWeaponActor.Get()))
	{
		return ZoransWeaponActor.Get();
	}

	if (GetCurrentSourceObject())
	{
		if (AZoransWeaponActor* SourceWeapon = Cast<AZoransWeaponActor>(GetCurrentSourceObject()))
		{
			ZoransWeaponActor = SourceWeapon;
			return ZoransWeaponActor.Get();
		}
	}

	return nullptr;
}

UZoransAbilitySystemComponent* UZoransGameplayAbility::GetZoransAbilitySystemComponent()
{
	if (ZoransAbilitySystemComponent.Get())
	{
		return ZoransAbilitySystemComponent.Get();
	}

	if (CurrentActorInfo)
	{
		if (IsValid(CurrentActorInfo->AbilitySystemComponent.Get()))
		{
			if (UAbilitySystemComponent* ActorAbilitySystemComponent = CurrentActorInfo->AbilitySystemComponent.Get())
			{
				if (UZoransAbilitySystemComponent* AbilitySystemComponent = Cast<UZoransAbilitySystemComponent>(ActorAbilitySystemComponent))
				{
					ZoransAbilitySystemComponent = AbilitySystemComponent;
				}
			}
		}
	}
	
	return ZoransAbilitySystemComponent.Get();
}

void UZoransGameplayAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);
	
	if (UZoransAbilitySystemComponent* AbilitySystemComponent = Cast<UZoransAbilitySystemComponent>(ActorInfo->AbilitySystemComponent))
	{
		ZoransAbilitySystemComponent = AbilitySystemComponent;
	}

	if (GetZoransAbilitySystemComponent() && IsLocallyControlled())
	{
		if (GetZoransAbilitySystemComponent()->AbilityGrantedDelegate.IsBound())
		{
			if (IsValid(Spec.Ability.Get()))
			{
				if (UZoransGameplayAbility* ZGA = Cast<UZoransGameplayAbility>(Spec.Ability.Get()))
				{
					if (IsValid(GetZoransAbilitySystemComponent()) && GetZoransAbilitySystemComponent()->AbilityGrantedDelegate.IsBound())
					{
						if (IsValid(ZGA->CooldownMaterial) || IsValid(ZGA->ReticleWidgetOverride))
						{
							GetZoransAbilitySystemComponent()->AbilityGrantedDelegate.Broadcast(ZGA);
						}
					}
				}
			}
		}
	}
}

void UZoransGameplayAbility::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfo, Spec);

	if (AZoransCharacterBase* ZoransCharacter = Cast<AZoransCharacterBase>(ActorInfo->AvatarActor.Get()))
	{
		OwningCharacter = ZoransCharacter;
	}
	
	SetupEnhancedInputBindings(ActorInfo, Spec);
	
	if (bActivateWhenGranted)
	{
		ActorInfo->AbilitySystemComponent->TryActivateAbility(Spec.Handle, false);
	}
}

void UZoransGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const bool bReplicateEndAbility, const bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	if (bRemoveAbilityOnEnd)
	{
		GetAbilitySystemComponentFromActorInfo_Checked()->ClearAbility(Handle);
	}
}

void UZoransGameplayAbility::SetupEnhancedInputBindings(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	if (IsValid(ActorInfo->AvatarActor.Get()))
	{
		if (const APawn* AvatarPawn = Cast<APawn>(ActorInfo->AvatarActor.Get()))
		{
			if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(AvatarPawn->InputComponent.Get()))
			{
				if (UZoransGameplayAbility* GameplayAbility = Cast<UZoransGameplayAbility>(Spec.Ability.Get()))
				{
					if (GameplayAbility->ActivationInputAction)
					{
						GameplayAbility->ActivationInputHandle = EnhancedInputComponent->BindAction(GameplayAbility->ActivationInputAction, GameplayAbility->ActivationTriggerType, GameplayAbility, &ThisClass::CallActivationEvent, ActorInfo, Spec).GetHandle();
					}
					
					if (GameplayAbility->CancelInputAction)
					{
						GameplayAbility->CancelInputHandle = EnhancedInputComponent->BindAction(GameplayAbility->CancelInputAction, GameplayAbility->CancelTriggerType, GameplayAbility, &UZoransGameplayAbility::CallCancelledEvent, ActorInfo, Spec).GetHandle();
					}
				}
			}
		}
	}
}

void UZoransGameplayAbility::CallActivationEvent(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec Spec)
{
	if (InputPressedDelegate.IsBound())
	{
		InputPressedDelegate.Broadcast();
	}
	
	ActorInfo->AbilitySystemComponent->TryActivateAbility(Spec.Handle);
}

void UZoransGameplayAbility::CallCancelledEvent(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec Spec)
{
	ActorInfo->AbilitySystemComponent->CancelAbilityHandle(Spec.Handle);
}

void UZoransGameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle,	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (const UGameplayEffect* CooldownGE = GetCooldownGameplayEffect())
	{
		const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(CooldownGE->GetClass(), GetAbilityLevel());
		SpecHandle.Data.Get()->DynamicGrantedTags.AddTag(AbilityCooldownTag);
		FActiveGameplayEffectHandle CooldownEffectHandle = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
	}
}

bool UZoransGameplayAbility::CheckCooldown(const FGameplayAbilitySpecHandle Handle,	const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (AbilityCooldownTag.IsValid() && ActorInfo->AbilitySystemComponent.IsValid())
	{
		if (ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(AbilityCooldownTag))
		{
			const FGameplayEffectQuery TagQuery = FGameplayEffectQuery::MakeQuery_MatchAllOwningTags(FGameplayTagContainer(AbilityCooldownTag));
			const TArray<TPair<float, float>> CooldownValues = ActorInfo->AbilitySystemComponent->GetActiveEffectsTimeRemainingAndDuration(TagQuery);
			if (CooldownValues.IsValidIndex(0))
			{
				if (ActorInfo->IsNetAuthority() && !ActorInfo->IsLocallyControlled()) // if server but not the listen server host (client sent activation, giving some tolerance here)
				{
					const float RemainingCooldown = CooldownValues[0].Key;
					return RemainingCooldown < 0.1f;
				}
				// we are either client or listen server host and the cooldown tag is present (should fail local)
				return false;
			}
		}
	}

	// cooldown tag is not activate at all (not on cooldown)
	return true;
}

bool UZoransGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle,	const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (CostIgnoreTag.IsValid())
	{
		if (ActorInfo->AbilitySystemComponent->GetGameplayTagCount(CostIgnoreTag) > 0)
		{
			return true;
		}
	}
	
	return Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags);
}

float UZoransGameplayAbility::GetWeaponCooldown() const
{
	float Cooldown = 0.2f;

	if (ZoransWeaponActor.Get())
	{
		if (ZoransWeaponActor.Get()->GetWeaponDefaults().RateOfFire > 0.f)
		{
			Cooldown = 60.f / ZoransWeaponActor.Get()->GetWeaponDefaults().RateOfFire;
		}
		else
		{
			Cooldown = 0.2f;
		}
	}
	return Cooldown;
}

void UZoransGameplayAbility::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	if (IsValid(ActorInfo->AvatarActor.Get()))
	{
		if (const APawn* AvatarPawn = Cast<APawn>(ActorInfo->AvatarActor.Get()))
		{
			if (AvatarPawn->IsPlayerControlled() && AvatarPawn->IsLocallyControlled())
			{
				if (IsValid(Spec.Ability))
				{
					if (UZoransGameplayAbility* ZGA = Cast<UZoransGameplayAbility>(Spec.Ability.Get()))
					{
						if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(AvatarPawn->InputComponent.Get()))
						{
							if (IsValid(Spec.Ability))
							{
								if (ZGA->ActivationInputHandle > 0)
								{
									EnhancedInputComponent->RemoveBindingByHandle(ZGA->ActivationInputHandle);
								}
				
								if (ZGA->CancelInputHandle > 0)
								{
									EnhancedInputComponent->RemoveBindingByHandle(ZGA->CancelInputHandle);
								}
							}
						}

						if (IsValid(GetZoransAbilitySystemComponent()) && GetZoransAbilitySystemComponent()->AbilityRemovedDelegate.IsBound())
						{
							if (IsValid(ZGA->CooldownMaterial) || IsValid(ZGA->ReticleWidgetOverride))
							{
								GetZoransAbilitySystemComponent()->AbilityRemovedDelegate.Broadcast(ZGA);
							}
						}
					}
				}
			}
		}
	}

	Super::OnRemoveAbility(ActorInfo, Spec);
}

#if WITH_EDITOR
void UZoransGameplayAbility::PreEditChange(FProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	if (PropertyAboutToChange->NamePrivate == FName("AbilityCooldownTag"))
	{
		OldAbilityCooldownTag = AbilityCooldownTag;
	}
}

void UZoransGameplayAbility::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	if (PropertyChangedEvent.GetPropertyName() == FName("ActivationInputAction") || PropertyChangedEvent.GetPropertyName() == FName("CancelInputAction") || PropertyChangedEvent.GetPropertyName() == FName("AbilityTags"))
	{
		if (IsValid(ActivationInputAction) || IsValid(CancelInputAction))
		{
			AbilityTags.AddTag(GameplayTag_HasInput.GetTag());
		}
		else
		{
			AbilityTags.RemoveTag(GameplayTag_HasInput.GetTag());
		}
	}

	if (PropertyChangedEvent.GetPropertyName() == FName("AbilityCooldownTag"))
	{
		if (OldAbilityCooldownTag.IsValid() && OldAbilityCooldownTag != AbilityCooldownTag)
		{
			AbilityTags.RemoveTag(OldAbilityCooldownTag);
			ActivationBlockedTags.RemoveTag(OldAbilityCooldownTag);
		}
		
		if (AbilityCooldownTag.IsValid())
		{
			AbilityTags.AddTag(AbilityCooldownTag);
			ActivationBlockedTags.AddTag(AbilityCooldownTag);
		}
	}
}
#endif

FDamageMetricValue UZoransGameplayAbility::DealDamageToTargetActor(AActor* TargetActor, AActor* InstigatorActor, AActor* OptionalEffectSource, const FHitResult& HitResult, const TSubclassOf<UGameplayEffect> DamageEffectClass, const float DamageAmount, const FGameplayTag DamageTag, bool& bValidHit, const float ImpactForce, const bool bWeakPoint, const FZoransWeaponData OptionalWeaponData)
{
	bValidHit = false;
	FDamageMetricValue DamageMetric;
	if (TargetActor && InstigatorActor)
	{
		FHitResult ModifiedHit = HitResult;
		if (TargetActor->GetClass()->ImplementsInterface(ULocationMagnitudeInterface::StaticClass()))
		{
			FLocationMagnitudeEvent LocationMagnitudeEvent;
			LocationMagnitudeEvent.SourceActor = InstigatorActor;
			LocationMagnitudeEvent.HitComponent = ModifiedHit.GetComponent();
			LocationMagnitudeEvent.RelativeHitLocation = ModifiedHit.ImpactPoint;
			LocationMagnitudeEvent.WorldHitLocation = ModifiedHit.ImpactPoint;
			LocationMagnitudeEvent.EventMagnitude = DamageAmount;

			ILocationMagnitudeInterface::Execute_LocationMagnitudeEvent(TargetActor, LocationMagnitudeEvent);
		}
		else
		{
			if (UAbilitySystemComponent* InstigatorAbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InstigatorActor))
			{
				if (UAbilitySystemComponent* TargetAbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor))
				{
					FGameplayEffectContextHandle EffectContextHandle = MakeEffectContext(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo());
					EffectContextHandle.AddInstigator(InstigatorActor, OptionalEffectSource);
					EffectContextHandle.AddSourceObject(this);

					if (bWeakPoint)
					{
						ModifiedHit.ElementIndex = 1;
					}
					
					EffectContextHandle.AddHitResult(ModifiedHit);

					const FGameplayEffectSpecHandle EffectSpecHandle = InstigatorAbilitySystemComponent->MakeOutgoingSpec(DamageEffectClass, ImpactForce, EffectContextHandle);
					EffectSpecHandle.Data->SetSetByCallerMagnitude(GameplayTag_Damage.GetTag(), DamageAmount);
					EffectSpecHandle.Data->AddDynamicAssetTag(DamageTag);
					
					if (bWeakPoint)
					{
						EffectSpecHandle.Data->AddDynamicAssetTag(GameplayTag_WeakPoint);
						DamageMetric.bWeakPoint = true;
					}
					
					if (!OptionalWeaponData.WeaponStatusEffects.IsEmpty())
					{
						for (auto &x : OptionalWeaponData.WeaponStatusEffects)
						{
							if (IsValid(x.StatusEffect))
							{
								if (x.ApplicationChance >= FMath::FRand())
								{
									FGameplayEffectSpecHandle StatusEffectHandle = InstigatorAbilitySystemComponent->MakeOutgoingSpec(x.StatusEffect, 1.f, InstigatorAbilitySystemComponent->MakeEffectContext());
									if (StatusEffectHandle.Data->Def->DurationPolicy == EGameplayEffectDurationType::HasDuration)
									{
										StatusEffectHandle.Data->SetDuration(x.EffectDuration, true);
									}
									bValidHit = true;
									DamageMetric.Damage += DamageAmount;
									InstigatorAbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*StatusEffectHandle.Data.Get(), TargetAbilitySystemComponent);
								}
							}
						}
					}

					if (!OptionalWeaponData.OwnerAppliedEffects.IsEmpty())
					{
						for (auto &y : OptionalWeaponData.OwnerAppliedEffects)
						{
							if (IsValid(y.StatusEffect))
							{
								if (y.ApplicationChance >= 1.f)
								{
									FGameplayEffectSpecHandle StatusEffectHandle = InstigatorAbilitySystemComponent->MakeOutgoingSpec(y.StatusEffect, 1.f, InstigatorAbilitySystemComponent->MakeEffectContext());
									if (StatusEffectHandle.Data->Def->DurationPolicy == EGameplayEffectDurationType::HasDuration)
									{
										StatusEffectHandle.Data->SetDuration(y.EffectDuration, true);
									}
									InstigatorAbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*StatusEffectHandle.Data.Get());
								}
								else if (y.ApplicationChance >= FMath::Rand())
								{
									FGameplayEffectSpecHandle StatusEffectHandle = InstigatorAbilitySystemComponent->MakeOutgoingSpec(y.StatusEffect, 1.f, InstigatorAbilitySystemComponent->MakeEffectContext());
									if (StatusEffectHandle.Data->Def->DurationPolicy == EGameplayEffectDurationType::HasDuration)
									{
										StatusEffectHandle.Data->SetDuration(y.EffectDuration, true);
									}
									InstigatorAbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*StatusEffectHandle.Data.Get());
								}
							}
						}
					}
					InstigatorAbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*EffectSpecHandle.Data.Get(), TargetAbilitySystemComponent);
				}
			}
		}
	}

	return DamageMetric;
}

void UZoransGameplayAbility::DealDamageToActorUsingHitResult(const FHitResult& HitResult, AActor* InstigatorActor, AActor* OptionalEffectSource, const TSubclassOf<UGameplayEffect> DamageEffectClass, const float DamageAmount, const FGameplayTag DamageTag)
{
	if (UAbilitySystemComponent* InstigatorAbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InstigatorActor))
	{
		if (UAbilitySystemComponent* TargetAbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(HitResult.GetActor()))
		{
			FGameplayEffectContextHandle EffectContextHandle = MakeEffectContext(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo());
			EffectContextHandle.AddInstigator(InstigatorActor, OptionalEffectSource);
			EffectContextHandle.AddSourceObject(this);
			EffectContextHandle.AddHitResult(HitResult);

			const FGameplayEffectSpecHandle EffectSpecHandle = InstigatorAbilitySystemComponent->MakeOutgoingSpec(DamageEffectClass, 1, EffectContextHandle);
			
			EffectSpecHandle.Data->SetSetByCallerMagnitude(GameplayTag_Damage.GetTag(), DamageAmount);
			EffectSpecHandle.Data->AddDynamicAssetTag(DamageTag);

			InstigatorAbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*EffectSpecHandle.Data.Get(), TargetAbilitySystemComponent);
		}
	}
}

FDamageMetricValue UZoransGameplayAbility::DealDamageToActorUsingHitscanData(const FHitscanData& HitscanData, const FZoransWeaponData& WeaponData, const TSubclassOf<UGameplayEffect> DamageEffectClass, bool& bValidHit, const float DamageModifier)
{
	bValidHit = false;
	FDamageMetricValue DamageMap;
	if (HitscanData.TargetHitType == ETargetHitType::Pawn)
	{
		if (!IsValid(HitscanData.SourceActor) || !IsValid(HitscanData.HitActor) || !IsValid(HitscanData.HitComponent)) return DamageMap;

		if (IsValidTarget(HitscanData.HitActor))
		{
			bool bWeakPointHit = false;
			const FVector SourceLocation = HitscanData.SourceActor->GetActorLocation();
			const FVector TargetLocation = HitscanData.HitComponent->GetComponentLocation();
			const float HitDistance = FVector::Distance(SourceLocation, TargetLocation);
			const float DistanceRatio = UKismetMathLibrary::SafeDivide(HitDistance, WeaponData.WeaponRange);
			const float FalloffModifier = WeaponData.DamageFalloff.EditorCurveData.Eval(DistanceRatio);
			float Damage = WeaponData.DamagePerShot * FalloffModifier * DamageModifier;
			FName HitBoneSection = NAME_None;
			
			if (HitscanData.BoneName != NAME_None)
			{
				if (AZoransCharacterBase* HitCharacter = Cast<AZoransCharacterBase>(HitscanData.HitActor))
				{
					const FBoneDamageMods BoneDamageMods = HitCharacter->GetDamageModifierFromBone(HitscanData.BoneName, HitBoneSection);
					//UE_LOG(LogTemp, Warning, TEXT("Bone Damage Modifier %f"), BoneDamageMods.DamageModifier);
					float DamageModifierFromBone = BoneDamageMods.DamageModifier;
					Damage *= DamageModifierFromBone;

					if (!WeaponData.BodySectionModifiers.IsEmpty() && WeaponData.BodySectionModifiers.Contains(HitBoneSection))
					{
						const float BodySectionMod = *WeaponData.BodySectionModifiers.Find(HitBoneSection);
						DamageModifierFromBone *= BodySectionMod;
						Damage *= BodySectionMod;
					}
					
					bWeakPointHit = DamageModifierFromBone > 1.f;
					if (bWeakPointHit && !FMath::IsNearlyZero(WeaponData.WeakPointBonus))
					{
						const float BonusDamage = Damage * WeaponData.WeakPointBonus;
						Damage += BonusDamage;
					}

					bValidHit = true;
					DamageMap.Damage += Damage;
					if (bWeakPointHit)
					{
						DamageMap.bWeakPoint = true;
					}
				}
			}
			
			if (UAbilitySystemComponent* InstigatorAbilitySystemComponent = GetAbilitySystemComponentFromActorInfo())
			{
				if (UAbilitySystemComponent* TargetAbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(HitscanData.HitActor))
				{
					FGameplayEffectContextHandle EffectContextHandle = MakeEffectContext(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo());
					EffectContextHandle.AddInstigator(HitscanData.SourceActor->GetOwner(), HitscanData.SourceActor);
					EffectContextHandle.AddSourceObject(this);

					FHitResult Hit;
					Hit.BoneName = HitscanData.BoneName;
					Hit.Location = SourceLocation;

					if (bWeakPointHit)
					{
						Hit.ElementIndex = 1;
					}

					EffectContextHandle.AddHitResult(Hit);
				
					const FGameplayEffectSpecHandle EffectSpecHandle = InstigatorAbilitySystemComponent->MakeOutgoingSpec(DamageEffectClass, WeaponData.ImpactForce * FalloffModifier, EffectContextHandle);
		
					EffectSpecHandle.Data->SetSetByCallerMagnitude(GameplayTag_Damage.GetTag(), Damage);
				
					FGameplayTag DamageTypeTag;
					switch (WeaponData.DamageType)
					{
					case EDamageType::Light:
						DamageTypeTag = DamageType::DamageType_Light;
						break;
					case EDamageType::Medium:
						DamageTypeTag = DamageType::DamageType_Medium;
						break;
					case EDamageType::Heavy:
						DamageTypeTag = DamageType::DamageType_Heavy;
						break;
					case EDamageType::Explosive:
						DamageTypeTag = DamageType::DamageType_Explosive;
						break;
					case EDamageType::Impale:
						DamageTypeTag = DamageType::DamageType_Impale;
						break;
					default:
						DamageTypeTag = DamageType::DamageType_Default;
						break;
					}

					if (bWeakPointHit)
					{
						EffectSpecHandle.Data->AddDynamicAssetTag(GameplayTag_WeakPoint);
					}
					
					EffectSpecHandle.Data->AddDynamicAssetTag(DamageTypeTag);
					
					if (!WeaponData.WeaponStatusEffects.IsEmpty())
					{
						for (auto &x : WeaponData.WeaponStatusEffects)
						{
							if (IsValid(x.StatusEffect))
							{
								if (x.ApplicationChance >= FMath::FRand())
								{
									bool bApply = true;
									if (!x.BodySectionFilter.IsEmpty() && HitBoneSection != NAME_None)
									{
										bApply = x.BodySectionFilter.Contains(HitBoneSection);
									}

									if (bApply)
									{
										FGameplayEffectSpecHandle StatusEffectHandle = InstigatorAbilitySystemComponent->MakeOutgoingSpec(x.StatusEffect, 1.f, InstigatorAbilitySystemComponent->MakeEffectContext());
										if (StatusEffectHandle.Data->Def->DurationPolicy == EGameplayEffectDurationType::HasDuration)
										{
											StatusEffectHandle.Data->SetDuration(x.EffectDuration, true);
										}
										InstigatorAbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*StatusEffectHandle.Data.Get(), TargetAbilitySystemComponent);
									}
								}
							}
						}
					}

					if (!WeaponData.OwnerAppliedEffects.IsEmpty())
					{
						for (auto &y : WeaponData.OwnerAppliedEffects)
						{
							if (IsValid(y.StatusEffect))
							{
								if (y.ApplicationChance >= 1.f)
								{
									FGameplayEffectSpecHandle StatusEffectHandle = InstigatorAbilitySystemComponent->MakeOutgoingSpec(y.StatusEffect, 1.f, InstigatorAbilitySystemComponent->MakeEffectContext());
									if (StatusEffectHandle.Data->Def->DurationPolicy == EGameplayEffectDurationType::HasDuration)
									{
										StatusEffectHandle.Data->SetDuration(y.EffectDuration, true);
									}
									InstigatorAbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*StatusEffectHandle.Data.Get());
								}
								else if (y.ApplicationChance >= FMath::Rand())
								{
									FGameplayEffectSpecHandle StatusEffectHandle = InstigatorAbilitySystemComponent->MakeOutgoingSpec(y.StatusEffect, 1.f, InstigatorAbilitySystemComponent->MakeEffectContext());
									if (StatusEffectHandle.Data->Def->DurationPolicy == EGameplayEffectDurationType::HasDuration)
									{
										StatusEffectHandle.Data->SetDuration(y.EffectDuration, true);
									}
									InstigatorAbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*StatusEffectHandle.Data.Get());
								}
							}
						}
					}
					InstigatorAbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*EffectSpecHandle.Data.Get(), TargetAbilitySystemComponent);
				}
			}
		}
	}
	else if (HitscanData.TargetHitType == ETargetHitType::Dynamic)
	{
		if (AActor* DynamicActor = HitscanData.HitActor)
		{
			if (DynamicActor->GetClass()->ImplementsInterface(ULocationMagnitudeInterface::StaticClass()))
			{
				FLocationMagnitudeEvent LocationMagnitudeEvent;
				LocationMagnitudeEvent.SourceActor = HitscanData.SourceActor;
				LocationMagnitudeEvent.HitComponent = HitscanData.HitComponent;
				LocationMagnitudeEvent.RelativeHitLocation = HitscanData.RelativeHitLocation;
				LocationMagnitudeEvent.WorldHitLocation = HitscanData.RelativeHitLocation;

				const FVector SourceLocation = HitscanData.SourceActor->GetActorLocation();
				const FVector TargetLocation = HitscanData.HitComponent->GetComponentLocation();
				const float HitDistance = FVector::Distance(SourceLocation, TargetLocation);
				const float DistanceRatio = UKismetMathLibrary::SafeDivide(HitDistance, WeaponData.WeaponRange);
				const float FalloffModifier = WeaponData.DamageFalloff.EditorCurveData.Eval(DistanceRatio);
				float Damage = WeaponData.DamagePerShot * FalloffModifier * DamageModifier;
				LocationMagnitudeEvent.EventMagnitude = Damage;

				bValidHit = true;
				DamageMap.Damage += Damage;
				
				ILocationMagnitudeInterface::Execute_LocationMagnitudeEvent(DynamicActor, LocationMagnitudeEvent);
			}
		}
	}

	return DamageMap;
}

TArray<FHitscanData> UZoransGameplayAbility::ConvertHitResultsToHitscanData(const TArray<FHitResult>& InHits, AActor* InSourceActor, const bool bUsedForTargetData)
{
	TArray<FHitscanData> OutHits;
	
	for (auto &x : InHits)
	{
		FHitscanData NewHit;
		NewHit.HitActor = x.GetActor();
		NewHit.SourceActor = InSourceActor;
		NewHit.HitComponent = x.GetComponent();
		NewHit.BoneName = x.BoneName;
		NewHit.SurfaceNormal = x.ImpactNormal;

		if (!bUsedForTargetData)
		{
			NewHit.RelativeHitLocation = IsValid(NewHit.HitComponent) ? x.ImpactPoint : x.TraceEnd;
		}
		else
		{
			NewHit.RelativeHitLocation = IsValid(NewHit.HitComponent) ? UKismetMathLibrary::InverseTransformLocation(NewHit.HitComponent->GetComponentTransform(), FVector(x.ImpactPoint.X, x.ImpactPoint.Y,  x.ImpactPoint.Z)) : FVector(x.TraceEnd.X, x.TraceEnd.Y,  x.TraceEnd.Z);
		}
		
		if (IsValid(NewHit.HitComponent))
		{
			switch (NewHit.HitComponent->GetCollisionObjectType())
			{
			case ECC_WorldStatic:
				NewHit.TargetHitType = ETargetHitType::Static;
				break;
			case ECC_WorldDynamic:
				NewHit.TargetHitType = ETargetHitType::Dynamic;
				break;
			case ECC_Pawn:
				NewHit.TargetHitType = ETargetHitType::Pawn;
				break;
			default:
				break;
			}
		}

		OutHits.Add(NewHit);
	}

	return OutHits;
}

void UZoransGameplayAbility::InternalHandleHitscanData(FGameplayAbilityTargetDataHandle InTargetData)
{
	TArray<FHitscanData> OutHits;
	
	const int32 DataCount = InTargetData.Num();
	if (DataCount > 0)
	{
		for (int32 x = 0; x < DataCount; x++)
		{
			if (InTargetData.Get(x))
			{
				if (const FHitscanTargetData* HitData = static_cast<FHitscanTargetData*>(InTargetData.Get(x)))
				{
					FHitscanData NewHit;
					NewHit.HitActor = HitData->HitActor;
					NewHit.SourceActor = HitData->SourceActor;
					NewHit.HitComponent = HitData->HitComponent;
					NewHit.BoneName = HitData->HitBone;
					NewHit.TargetHitType = HitData->TargetHitType;
					NewHit.RelativeHitLocation = IsValid(NewHit.HitComponent) ? UKismetMathLibrary::TransformLocation(NewHit.HitComponent->GetComponentTransform(), HitData->RelativeHitLocation) : HitData->RelativeHitLocation;
					NewHit.SurfaceNormal = HitData->SurfaceNormal;
					
					OutHits.Add(NewHit);
				}
			}
		}
	}

	if (!OutHits.IsEmpty())
	{
		HandleHitscanData(OutHits);
	}
}

void UZoransGameplayAbility::InternalHandleBallisticProjectileSpawnParams(FGameplayAbilityTargetDataHandle InTargetData)
{
	FBaseProjectileSpawnParams SpawnParams;

	if(FGameplayAbilityTargetData* RawData = InTargetData.Get(0); RawData != nullptr)
	{
		const FBaseProjectileSpawnTargetData* RawSpawnData = static_cast<FBaseProjectileSpawnTargetData*>(RawData);
		SpawnParams.SpawnPoint = RawSpawnData->SpawnLocation;
		SpawnParams.LaunchDirection = RawSpawnData->SpawnDirection;
		SpawnParams.SpawnTime = RawSpawnData->LocalSpawnTime;
		SpawnParams.LocalIndex = RawSpawnData->ProjectileIndex;
		SpawnParams.SpawnSocketOverride = RawSpawnData->SpawnSocketOverride;
	}

	HandleBallisticProjectileSpawnParams(SpawnParams);
}

void UZoransGameplayAbility::InternalHandleBouncingProjectileSpawnParams(FGameplayAbilityTargetDataHandle InTargetData)
{
	FBouncingProjectileSpawnParams SpawnParams;

	if(FGameplayAbilityTargetData* RawData = InTargetData.Get(0); RawData != nullptr)
	{
		const FBouncingProjectileSpawnTargetData* RawSpawnData = static_cast<FBouncingProjectileSpawnTargetData*>(RawData);
		
		SpawnParams.SpawnPoint = RawSpawnData->SpawnLocation;
		SpawnParams.LaunchDirection = RawSpawnData->SpawnDirection;
		SpawnParams.SpawnTime = RawSpawnData->LocalSpawnTime;
		SpawnParams.LocalIndex = RawSpawnData->ProjectileIndex;
		SpawnParams.BouncePath = RawSpawnData->ProjectileBouncePath;
		SpawnParams.SpawnSocketOverride = RawSpawnData->SpawnSocketOverride;
	}
	
	HandleBouncingProjectileSpawnParams(SpawnParams);
}

bool UZoransGameplayAbility::IsValidTarget(AActor* InActor)
{
	if (AActor* OwningActor = GetAvatarActorFromActorInfo())
	{
		if (IsValid(InActor))
		{
			ETeamComparisonResult TeamComparisonResult = ETeamComparisonResult::Invalid;
			UTeamStateFunctionLibrary::GetTeamDispositionFromActors(OwningActor, InActor, OwningActor, TeamComparisonResult);
			switch (TeamComparisonResult)
			{
			case ETeamComparisonResult::Invalid:
				if (bTraceEnvironment) return true;
				break;
			case ETeamComparisonResult::Hostile:
				if (bTraceEnemy) return true;
				break;
			case ETeamComparisonResult::Friendly:
				if (bTraceFriendly) return true;
				break;
			}
		}
	}
	
	return false;
}

ETargetHitType UZoransGameplayAbility::GetHitTypeFromComponent(const UPrimitiveComponent* InComponent)
{
	ETargetHitType HitType = ETargetHitType::None;

	if (InComponent)
	{
		switch (InComponent->GetCollisionObjectType())
		{
		case ECC_WorldStatic:
			HitType = ETargetHitType::Static;
			break;
		case ECC_WorldDynamic:
			HitType = ETargetHitType::Dynamic;
			break;
		case ECC_Pawn:
			HitType = ETargetHitType::Pawn;
			break;
		default:
			break;
		}
	}
	
	return HitType;
}

void UZoransGameplayAbility::BeginTrackingTimeMetric()
{
	if (GetWorld() && GetWorld()->GetGameState())
	{
		MetricTimeStart = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();
	}
}

float UZoransGameplayAbility::EndTrackingTimeMetric()
{
	float MetricTime = 0.f;
	
	if (GetWorld() && GetWorld()->GetGameState())
	{
		const float CurrentTime = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();
		MetricTime = CurrentTime - MetricTimeStart;
	}

	return MetricTime;
}

FDamageMetricValue UZoransGameplayAbility::MergeDamageMetrics(const TArray<FDamageMetricValue>& InDamageMetrics)
{
	FDamageMetricValue OutDamageMetrics;
	
	if (!InDamageMetrics.IsEmpty())
	{
		for (const FDamageMetricValue& Metric : InDamageMetrics)
		{
			OutDamageMetrics.Damage += Metric.Damage;
			if (Metric.bWeakPoint)
			{
				OutDamageMetrics.bWeakPoint = true;
			}
		}
	}

	return OutDamageMetrics;
}