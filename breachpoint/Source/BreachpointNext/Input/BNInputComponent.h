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
	template <class UserClass, typename FuncType>
	void BindActionByTag(const UBNInputConfig* Config, FGameplayTag InputTag, ETriggerEvent TriggerEvent, UserClass* Object, FuncType Func)
	{
		if (const UInputAction* Action = Config ? Config->FindInputActionByTag(InputTag) : nullptr)
		{
			BindAction(Action, TriggerEvent, Object, Func);
		}
	}
};
