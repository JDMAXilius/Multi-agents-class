// Copyright Zollpa LLC


#include "MovementAttributes.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

void UMovementAttributes::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UMovementAttributes, CurrentMovementSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMovementAttributes, WeightModifier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMovementAttributes, JumpModifier, COND_None, REPNOTIFY_Always);
}

void UMovementAttributes::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	
	if (Data.EvaluatedData.Attribute == GetCurrentMovementSpeedAttribute())
	{
		SetCurrentMovementSpeed(FMath::Clamp(GetCurrentMovementSpeed(), 0.0f, 1400.0f));
	}
}

void UMovementAttributes::OnRep_CurrentMovementSpeed(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMovementAttributes, CurrentMovementSpeed, OldValue);
}

void UMovementAttributes::OnRep_WeightModifier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMovementAttributes, WeightModifier, OldValue);
}

void UMovementAttributes::OnRep_JumpModifier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMovementAttributes, JumpModifier, OldValue);
}