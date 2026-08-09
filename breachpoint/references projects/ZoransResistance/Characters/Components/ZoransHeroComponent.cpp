// Copyright Zollpa LLC.


#include "ZoransHeroComponent.h"
#include "AbilitySystemGlobals.h"
#include "ComponentInterface.h"
#include "ZoransResistance/AbilitySystem/Attributes/CharacterAttributes.h"

UZoransHeroComponent::UZoransHeroComponent()
{
	SetIsReplicatedByDefault(true);
	
	PrimaryComponentTick.bCanEverTick = false;
}

void UZoransHeroComponent::InitHeroComponent(AActor* InOwningActor)
{
	check(InOwningActor)
	
	AbilitySystemComponent = UAbilitySystemGlobals::Get().GetAbilitySystemComponentFromActor(InOwningActor);

	ensure(AbilitySystemComponent.Get());

	CharacterAttributes = Cast<UCharacterAttributes>(AbilitySystemComponent->GetAttributeSet(UCharacterAttributes::StaticClass()));

	CurrentChargeChangeDelegate = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UCharacterAttributes::GetCurrentKineticChargeAttribute()).AddUObject(this, &UZoransHeroComponent::On_CurrentChargeChanged);
	MaximumChargeChangeDelegate = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UCharacterAttributes::GetMaximumKineticChargeAttribute()).AddUObject(this, &UZoransHeroComponent::On_MaxChargeChanged);
	KineticChargeRegenChangeDelegate = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UCharacterAttributes::GetKineticChargeRegenAttribute()).AddUObject(this, &UZoransHeroComponent::On_KineticChargeRegenChanged);

	bIsInitialized = true;
	
	if (IComponentInterface* ComponentInterface = Cast<IComponentInterface>(InOwningActor))
	{
		ComponentInterface->CheckComponentRequirements();
	}
}

void UZoransHeroComponent::On_CurrentChargeChanged(const FOnAttributeChangeData& Data) const
{
	if (OnKineticChargeChange.IsBound())
	{
		if (AbilitySystemComponent.Get() && CharacterAttributes)
		{
			const float MaxValue = CharacterAttributes->GetMaximumKineticCharge();
			const float CurrentPercentage = Data.NewValue / MaxValue;
			
			OnKineticChargeChange.Broadcast(Data.NewValue, Data.OldValue, MaxValue, CurrentPercentage);
		}
	}
}

void UZoransHeroComponent::On_MaxChargeChanged(const FOnAttributeChangeData& Data) const
{
	if (OnKineticChargeChange.IsBound())
	{
		if (AbilitySystemComponent.Get() && CharacterAttributes)
		{
			const float CurrentValue = CharacterAttributes->GetCurrentKineticCharge();
			const float CurrentPercentage =  CurrentValue / Data.NewValue;
			
			OnKineticChargeChange.Broadcast(CurrentValue, CurrentValue, Data.NewValue, CurrentPercentage);
		}
	}
}

void UZoransHeroComponent::On_KineticChargeRegenChanged(const FOnAttributeChangeData& Data) const
{
	if (OnKineticChargeRegenChange.IsBound())
	{
		if (AbilitySystemComponent.Get() && CharacterAttributes)
		{
			OnKineticChargeRegenChange.Broadcast(Data.NewValue, Data.OldValue);
		}
	}
}

void UZoransHeroComponent::DestroyComponent(const bool bPromoteChildren)
{
	OnKineticChargeChange.Clear();
	OnKineticChargeRegenChange.Clear();
	
	Super::DestroyComponent(bPromoteChildren);
}