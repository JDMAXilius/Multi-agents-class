// Copyright Zollpa LLC.


#include "ZoransHealthComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "ZoransResistance/AbilitySystem/Attributes/HealthAttributes.h"
#include "ZoransResistance/AbilitySystem/Data/ZoransGameplayTags.h"
#include "GameplayEffect.h"
#include "GameFramework/PlayerState.h"
#include "ZoransResistance/ZoransResistance.h"
#include "ZoransResistance/Player/ZoransPlayerController.h"
#include "ZoransResistance/Player/ZoransPlayerState.h"

UZoransHealthComponent::UZoransHealthComponent()
{
	SetIsReplicatedByDefault(true);
	
	PrimaryComponentTick.bCanEverTick = false;
}

void UZoransHealthComponent::InitHealthComponent(AActor* InOwningActor)
{
	check(InOwningActor)
	
	AbilitySystemComponent = UAbilitySystemGlobals::Get().GetAbilitySystemComponentFromActor(InOwningActor);

	ensure(AbilitySystemComponent.Get());

	HealthAttributes = Cast<UHealthAttributes>(AbilitySystemComponent->GetAttributeSet(UHealthAttributes::StaticClass()));

	CurrentHealthChangeDelegate = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UHealthAttributes::GetCurrentHealthAttribute()).AddUObject(this, &UZoransHealthComponent::On_CurrentHealthChanged);
	MaximumHealthChangeDelegate = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UHealthAttributes::GetMaximumHealthAttribute()).AddUObject(this, &UZoransHealthComponent::On_MaxHealthChanged);
	CurrentShieldChangeDelegate = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UHealthAttributes::GetCurrentShieldAttribute()).AddUObject(this, &UZoransHealthComponent::On_CurrentShieldChanged);
	MaximumShieldChangeDelegate = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UHealthAttributes::GetMaximumShieldAttribute()).AddUObject(this, &UZoransHealthComponent::On_MaxShieldChanged);

	bIsInitialized = true;

	if (IComponentInterface* ComponentInterface = Cast<IComponentInterface>(InOwningActor))
	{
		ComponentInterface->CheckComponentRequirements();
	}
}

AZoransGameState* UZoransHealthComponent::GetZoransGameState()
{
	if (IsValid(ZoransGameState.Get()))
	{
		return ZoransGameState.Get();
	}

	if (AZoransGameState* GameState = Cast<AZoransGameState>(GetWorld()->GetGameState()))
	{
		ZoransGameState = GameState;
		return ZoransGameState.Get();
	}
	
	return nullptr;
}

void UZoransHealthComponent::HandleDeathEvent(const FGameplayEffectSpec& DeathSpec) const
{
	if (IsValid(GetOwner()) && GetNetMode() < NM_Client)
	{
		FGameplayTagContainer DeathEffectTags;
		DeathSpec.GetAllAssetTags(DeathEffectTags);
		FGameplayTagContainer DamageFilter;
		DamageFilter.AddTagFast(DamageType::DamageType_Default);
		const FGameplayTagContainer FilteredDamageTags = DeathEffectTags.Filter(DamageFilter);
		const FGameplayTag DamageTypeTag = FilteredDamageTags.IsEmpty() ? DamageType::DamageType_Default : FilteredDamageTags.First();

		FGameplayEventData DeathEvent;
		DeathEvent.EventTag = DamageTypeTag;
		DeathEvent.Instigator = DeathSpec.GetContext().GetInstigator();
		DeathEvent.OptionalObject = DeathSpec.GetContext().GetAbility();
		DeathEvent.OptionalObject2 = DeathSpec.GetContext().GetAbility();
		DeathEvent.ContextHandle = DeathSpec.GetEffectContext();
		DeathEvent.EventMagnitude = DeathSpec.GetLevel();
				
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetOwner(), DamageTypeTag, DeathEvent);
	}
}

void UZoransHealthComponent::On_DeathFinished(APlayerState* SourcePlayerState, const APlayerState* TargetPlayerState, const UTexture2D* KillfeedIcon, const bool bWeakPoint)
{
	if (GetOwner() && GetNetMode() < NM_Client)
	{
		if (GetZoransGameState() && IsValid(SourcePlayerState) && IsValid(TargetPlayerState))
		{
			GetZoransGameState()->HandleKillfeedEvent(SourcePlayerState, TargetPlayerState, KillfeedIcon, bWeakPoint);
		}
	}
}

void UZoransHealthComponent::On_DamageReceived(const FGameplayEffectSpec& EffectSpec, AActor* AvatarActor, const float DamageDealt, const bool bFatal, const bool bOverrideHitType, const bool bSkipReaction)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	if (EffectSpec.GetEffectContext().IsValid())
	{
		UAbilitySystemComponent* SourceASC = EffectSpec.GetEffectContext().GetInstigatorAbilitySystemComponent();
		UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
		
		const FHitResult Hit = *EffectSpec.GetEffectContext().GetHitResult();
		FName HitBone = Hit.BoneName;
		EHitSeverity HitSeverity = EHitSeverity::Default;

		FGameplayTagContainer DeathEffectTags;
		EffectSpec.GetAllAssetTags(DeathEffectTags);
		FGameplayTagContainer DamageFilter;
		DamageFilter.AddTagFast(DamageType::DamageType_Default);
		const FGameplayTagContainer FilteredDamageTags = DeathEffectTags.Filter(DamageFilter);
		const FGameplayTag DamageTypeTag = FilteredDamageTags.IsEmpty() ? DamageType::DamageType_Default : FilteredDamageTags.First();

		bool bWeakPointHit = false;
		bool bThrowdown = false;
		
		if (DamageTypeTag == DamageType::DamageType_Throwdown)
		{
			bThrowdown = true;
			bWeakPointHit = true;
			HitBone = BodySections::Section_Head;
			HitSeverity = EHitSeverity::Throwdown;
		}
		else
		{
			bWeakPointHit = EffectSpec.GetDynamicAssetTags().HasTagExact(GameplayTag_WeakPoint);
		
			if (bFatal)
			{
				HitSeverity = bWeakPointHit ? EHitSeverity::FatalWeakPoint : EHitSeverity::Fatal;
			}
			else if (bWeakPointHit)
			{
				HitSeverity = EHitSeverity::WeakPoint;
			}
		}
		
		if (IsValid(SourceASC) && IsValid(TargetASC))
		{
			if (DamageDealt > 0.f && IsValid(KineticChargeEffect) && SourceASC != TargetASC)
			{
				const float KineticChargeGenerated = (DamageDealt * 0.25f) + (bFatal ? 20.f : 0.f);
				const FGameplayEffectSpecHandle KineticChargeSpecHandle = SourceASC->MakeOutgoingSpec(KineticChargeEffect, 1.f, SourceASC->MakeEffectContext());
				KineticChargeSpecHandle.Data->SetSetByCallerMagnitude(GameplayTag_Magnitude, KineticChargeGenerated);
				SourceASC->ApplyGameplayEffectSpecToSelf(*KineticChargeSpecHandle.Data.Get());
			}

			if (bThrowdown && bOverrideHitType)
			{
				// do nothing. we do not want the shield damage to pop up for a throwdown K.O. popup
			}
			else if (SourceASC != TargetASC)
			{
				if (const APlayerState* SourcePlayerState = Cast<APlayerState>(SourceASC->GetOwner()))
				{
					if (AZoransPlayerController* SourcePlayerController = Cast<AZoransPlayerController>(SourcePlayerState->GetPlayerController()))
					{
						SourcePlayerController->CreateDamageNumberWidget(AvatarActor, HitBone, DamageDealt, HitSeverity, bOverrideHitType);
					}
				}
			}
		}

		if (GetOwner())
		{
			FGameplayEventData DamageEvent;
			DamageEvent.Instigator = EffectSpec.GetContext().GetInstigator();
			DamageEvent.ContextHandle = EffectSpec.GetEffectContext();
			DamageEvent.EventMagnitude = EffectSpec.GetLevel();
			
			if (bOverrideHitType)
			{
				DamageEvent.TargetTags.AddTag(GameplayTag_PersonalShield);
			}

			if (!bSkipReaction)
			{
				UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetOwner(), DamageTypeTag, DamageEvent);
			}

			if (SourceASC != TargetASC)
			{
				if (IsValid(SourceASC) && SourceASC->GetOwner())
				{
					if (AZoransPlayerState* SourcePlayerState = Cast<AZoransPlayerState>(SourceASC->GetOwner()))
					{
						if (AZoransPlayerState* OwnerPlayerState = Cast<AZoransPlayerState>(GetOwner()))
						{
							OwnerPlayerState->AddPlayerAssist(SourcePlayerState, DamageDealt);
						}
					}
				}
			}
		}
	}
}

void UZoransHealthComponent::On_CurrentHealthChanged(const FOnAttributeChangeData& Data)
{
	EvaluatedHealth = Data.NewValue;
	
	if (OnHealthChange.IsBound())
	{
		if (AbilitySystemComponent.Get() && HealthAttributes)
		{
			const float MaxValue = HealthAttributes->GetMaximumHealth();
			const float CurrentPercentage = Data.NewValue / MaxValue;
			
			OnHealthChange.Broadcast(Data.NewValue, Data.OldValue, MaxValue, CurrentPercentage);
		}
	}
}

void UZoransHealthComponent::On_MaxHealthChanged(const FOnAttributeChangeData& Data) const
{
	if (OnHealthChange.IsBound())
	{
		if (AbilitySystemComponent.Get() && HealthAttributes)
		{
			const float CurrentValue = HealthAttributes->GetCurrentHealth();
			const float CurrentPercentage =  CurrentValue / Data.NewValue;
			
			OnHealthChange.Broadcast(CurrentValue, CurrentValue, Data.NewValue, CurrentPercentage);
		}
	}
}

void UZoransHealthComponent::On_CurrentShieldChanged(const FOnAttributeChangeData& Data)
{
	EvaluatedShield = Data.NewValue;
	
	if (OnShieldChange.IsBound())
	{
		if (AbilitySystemComponent.Get() && HealthAttributes)
		{
			const float MaxValue = HealthAttributes->GetMaximumShield();
			const float CurrentShield = FMath::Clamp<float>(Data.NewValue, 0.f, MaxValue);
			const float CurrentPercentage = CurrentShield / MaxValue;
			
			OnShieldChange.Broadcast(CurrentShield, Data.OldValue, MaxValue, CurrentPercentage);
		}
	}
}

void UZoransHealthComponent::On_MaxShieldChanged(const FOnAttributeChangeData& Data) const
{
	if (OnShieldChange.IsBound())
	{
		if (AbilitySystemComponent.Get() && HealthAttributes)
		{
			const float CurrentValue = FMath::Clamp<float>(HealthAttributes->GetCurrentShield(), 0.f, Data.NewValue);
			const float CurrentPercentage =  CurrentValue / Data.NewValue;
			
			OnShieldChange.Broadcast(CurrentValue, CurrentValue, Data.NewValue, CurrentPercentage);
		}
	}
}

void UZoransHealthComponent::DestroyComponent(const bool bPromoteChildren)
{
	OnHealthChange.Clear();
	OnDeath.Clear();
		
	Super::DestroyComponent(bPromoteChildren);
}

float UZoransHealthComponent::GetHealthRatio() const
{
	float HealthRatio = 1.f;

	if (IsValid(HealthAttributes))
	{
		const float CurrentHealth = HealthAttributes->GetCurrentHealth();
		const float MaximumHealth = HealthAttributes->GetMaximumHealth();
		HealthRatio = CurrentHealth / MaximumHealth;
	}

	return HealthRatio;
}