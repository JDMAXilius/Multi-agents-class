#include "Characters/BNHealthComponent.h"

#include "AbilitySystem/Attributes/BNAttributeSet.h"
#include "AbilitySystemComponent.h"

UBNHealthComponent::UBNHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBNHealthComponent::InitializeWithAbilitySystem(UAbilitySystemComponent* InASC)
{
	if (!InASC || HealthChangedHandle.IsValid())
	{
		return;
	}

	CachedAbilitySystem = InASC;

	// CHANGES only — the value at registration is deliberately not read. A respawned pawn
	// registers while the persistent ASC still holds the zero that killed the last one, and the
	// init GE lands after; a value check here would kill the new body on the frame it spawned.
	HealthChangedHandle = InASC->GetGameplayAttributeValueChangeDelegate(UBNAttributeSet::GetHealthAttribute())
		.AddUObject(this, &UBNHealthComponent::HandleHealthChanged);
}

void UBNHealthComponent::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	if (Data.NewValue > 0.f)
	{
		bDeathReported = false;
		return;
	}

	if (bDeathReported)
	{
		return;
	}
	bDeathReported = true;

	OnDeath.Broadcast(this);
}

void UBNHealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HealthChangedHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = CachedAbilitySystem.Get())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UBNAttributeSet::GetHealthAttribute())
				.Remove(HealthChangedHandle);
		}
		HealthChangedHandle.Reset();
	}
	CachedAbilitySystem.Reset();

	Super::EndPlay(EndPlayReason);
}
