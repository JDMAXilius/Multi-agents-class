#include "Input/BNInputConfig.h"

const UInputAction* UBNInputConfig::FindInputActionByTag(FGameplayTag InputTag) const
{
	for (const FBNInputBinding& Binding : Bindings)
	{
		if (Binding.InputTag.MatchesTagExact(InputTag))
		{
			return Binding.InputAction;
		}
	}
	return nullptr;
}
