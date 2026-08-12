#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "Input/BNInputConfig.h"
#include "BNInputComponent.generated.h"

UCLASS()
class BREACHPOINTNEXT_API UBNInputComponent : public UEnhancedInputComponent
{
	GENERATED_BODY()

public:
	// False = the tag has no InputAction in the config, i.e. that control is dead.
	// Callers log it; a silent miss is how input "works on my machine" and ships broken.
	template <class UserClass, typename FuncType>
	bool BindActionByTag(const UBNInputConfig* Config, FGameplayTag InputTag, ETriggerEvent TriggerEvent, UserClass* Object, FuncType Func)
	{
		const UInputAction* Action = Config ? Config->FindInputActionByTag(InputTag) : nullptr;
		if (!Action)
		{
			return false;
		}
		BindAction(Action, TriggerEvent, Object, Func);
		return true;
	}
};
