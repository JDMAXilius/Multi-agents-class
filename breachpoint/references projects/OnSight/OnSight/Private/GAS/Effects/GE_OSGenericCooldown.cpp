#include "GAS/Effects/GE_OSGenericCooldown.h"

#include "Data/OSGameplayTags.h"

UGE_OSGenericCooldown::UGE_OSGenericCooldown(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	FSetByCallerFloat SBC;
	SBC.DataTag = FOSGameplayTags::Get().Data_Cooldown_Duration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(SBC);
}
