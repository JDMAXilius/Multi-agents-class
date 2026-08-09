// Copyright Zollpa LLC.


#include "HealthAttributes.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "ZoransResistance/Characters/Components/ComponentInterface.h"
#include "ZoransResistance/Characters/Components/ZoransHealthComponent.h"

void UHealthAttributes::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UHealthAttributes, CurrentHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UHealthAttributes, MaximumHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UHealthAttributes, HealthRegenRate, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UHealthAttributes, CurrentShield, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UHealthAttributes, MaximumShield, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UHealthAttributes, ShieldRegenRate, COND_OwnerOnly, REPNOTIFY_Always);
}

void UHealthAttributes::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	if (Attribute == GetCurrentHealthAttribute())
	{
		NewValue = FMath::Clamp<float>(NewValue, 0.f, GetMaximumHealth());
	}

	if (Attribute == GetMaximumHealthAttribute())
	{
		AdjustAttributeForMaxChange(CurrentHealth, MaximumHealth, NewValue, GetCurrentHealthAttribute());
	}

	if (Attribute == GetCurrentShieldAttribute())
	{
		NewValue = FMath::Clamp<float>(NewValue, 0.f, GetMaximumShield());
	}

	if (Attribute == GetMaximumShieldAttribute())
	{
		SetCurrentShield(FMath::Clamp<float>(GetCurrentShield(), 0.f, NewValue));
	}

	Super::PreAttributeChange(Attribute, NewValue);
}

void UHealthAttributes::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		// Store a local copy of the amount of Damage done and clear the Damage attribute.
		const float LocalDamageDone = GetDamage();

		SetDamage(0.f);
		
		if (LocalDamageDone > 0.f)
		{
			// Apply the Health change and then clamp it.
			UZoransHealthComponent* HealthComponent = nullptr;
			if (Data.Target.GetAvatarActor()->GetClass()->ImplementsInterface(UComponentInterface::StaticClass()))
			{
				if (const IComponentInterface* ComponentInterface = Cast<IComponentInterface>(Data.Target.GetAvatarActor()))
				{
					HealthComponent = ComponentInterface->GetHealthComponent();
				}
			}

			const float StartingShieldValue = GetCurrentShield();
			const float RemainingShield = StartingShieldValue - LocalDamageDone;
			const float DamageRemaining = LocalDamageDone - StartingShieldValue;

			SetCurrentShield(FMath::Clamp(RemainingShield, 0.f, GetMaximumShield()));

			if (DamageRemaining > 0.f)
			{
				const float NewHealth = GetCurrentHealth() - DamageRemaining;
			
				SetCurrentHealth(FMath::Clamp(NewHealth, 0.f, GetMaximumHealth()));
				
				const float UpdatedHealth = GetCurrentHealth();
				if (IsValid(HealthComponent))
				{
					HealthComponent->On_DamageReceived(Data.EffectSpec, Data.Target.GetAvatarActor(), DamageRemaining, UpdatedHealth <= 0.f, false, StartingShieldValue > 0.f);
					if (UpdatedHealth <= 0.f)
					{
						HealthComponent->HandleDeathEvent(Data.EffectSpec);
					}
				}
			}

			if (StartingShieldValue > 0.f && IsValid(HealthComponent))
			{
				HealthComponent->On_DamageReceived(Data.EffectSpec, Data.Target.GetAvatarActor(), FMath::Clamp<float>(LocalDamageDone, 0.f, StartingShieldValue), false, true, false);
			}
		}
	}

	if (Data.EvaluatedData.Attribute == GetCurrentShieldAttribute())
	{
		SetCurrentShield(FMath::Clamp(GetCurrentShield(), 0.f, GetMaximumShield()));
	}

	else if (Data.EvaluatedData.Attribute == GetHealingAttribute())
	{
		// Store a local copy of the amount of Healing done and clear the Healing attribute.
		const float LocalHealingDone = GetHealing();

		SetHealing(0.f);
	
		if (LocalHealingDone > 0.0f)
		{
			const float NewHealth = GetCurrentHealth() + LocalHealingDone;

			SetCurrentHealth(FMath::Clamp(NewHealth, 0.0f, GetMaximumHealth()));
		}
	}
	
	else if (Data.EvaluatedData.Attribute == GetCurrentHealthAttribute())
	{
		SetCurrentHealth(FMath::Clamp(GetCurrentHealth(), 0.0f, GetMaximumHealth()));
	}

	Super::PostGameplayEffectExecute(Data);
}

void UHealthAttributes::OnRep_CurrentHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHealthAttributes, CurrentHealth, OldValue);
}

void UHealthAttributes::OnRep_MaximumHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHealthAttributes, MaximumHealth, OldValue);
}

void UHealthAttributes::OnRep_HealthRegenRate(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHealthAttributes, HealthRegenRate, OldValue);
}

void UHealthAttributes::OnRep_CurrentShield(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHealthAttributes, CurrentShield, OldValue);
}

void UHealthAttributes::OnRep_MaximumShield(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHealthAttributes, MaximumShield, OldValue);
}

void UHealthAttributes::OnRep_ShieldRegenRate(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UHealthAttributes, ShieldRegenRate, OldValue);
}
