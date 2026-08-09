#pragma once
// Chooser-to-ability mapping: FAbilityClassMapping (CppClass + BlueprintClass). Used when resolving Chooser picks to ability classes.

#include "CoreMinimal.h"
#include "GAS/Abilities/OSGameplayAbility.h"

struct FAbilityClassMapping
{
	TSubclassOf<UOSGameplayAbility> CppClass;
	TSubclassOf<UOSGameplayAbility> BlueprintClass;
};
