// Copyright Zollpa LLC.


#include "CharacterAttributes.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

void UCharacterAttributes::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UCharacterAttributes, CurrentKineticCharge, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCharacterAttributes, MaximumKineticCharge, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCharacterAttributes, KineticChargeRegen, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UCharacterAttributes, ThrowdownCharge, COND_None, REPNOTIFY_Always);
}

void UCharacterAttributes::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetCurrentKineticChargeAttribute())
	{
		NewValue = FMath::Clamp<float>(NewValue, 0.f, GetMaximumKineticCharge());
	}

	if (Attribute == GetMaximumKineticChargeAttribute())
	{
		AdjustAttributeForMaxChange(CurrentKineticCharge, MaximumKineticCharge, NewValue, GetCurrentKineticChargeAttribute());
	}
}

void UCharacterAttributes::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetCurrentKineticChargeAttribute())
	{
		SetCurrentKineticCharge(FMath::Clamp(GetCurrentKineticCharge(), 0.0f, GetMaximumKineticCharge()));
	}

	else if (Data.EvaluatedData.Attribute == GetKineticChargeRegenAttribute())
	{
		SetKineticChargeRegen(FMath::Clamp(GetKineticChargeRegen(), 0.0f, GetMaximumKineticCharge()));
	}
}

void UCharacterAttributes::OnRep_CurrentKineticCharge(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCharacterAttributes, CurrentKineticCharge, OldValue);
}

void UCharacterAttributes::OnRep_MaximumKineticCharge(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCharacterAttributes, MaximumKineticCharge, OldValue);
}

void UCharacterAttributes::OnRep_KineticChargeRegen(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCharacterAttributes, KineticChargeRegen, OldValue);
}

void UCharacterAttributes::OnRep_ThrowdownCharge(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCharacterAttributes, ThrowdownCharge, OldValue);
}
