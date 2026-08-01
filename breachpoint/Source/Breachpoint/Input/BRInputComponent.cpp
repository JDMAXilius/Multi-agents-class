// Breachpoint. The Enhanced Input -> InputTag routing component.

#include "Input/BRInputComponent.h"

UBRInputComponent::UBRInputComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Deliberately empty. There is no Tick (law 4) and no state: this component owns bindings,
	// and a binding is created once at setup and fires by delegate thereafter.
}

void UBRInputComponent::RemoveBinds(TArray<uint32>& BindHandles)
{
	for (const uint32 Handle : BindHandles)
	{
		RemoveBindingByHandle(Handle);
	}

	BindHandles.Reset();
}
